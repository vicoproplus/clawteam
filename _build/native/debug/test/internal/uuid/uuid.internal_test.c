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

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4uuid33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760;

struct _M0TPB6Logger;

struct _M0Y4Char;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4uuid33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0BTPB4Show;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS4Char;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
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

struct _M0TPB13StringBuilder {
  int32_t $1;
  moonbit_bytes_t $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4uuid33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0Y4Char {
  int32_t $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
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

struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4uuid33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS4Char {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
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

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4uuid39____test__757569642e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN17error__to__stringS769(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN14handle__resultS760(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testC1771l425(
  struct _M0TWEu*
);

int32_t _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testC1767l426(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal4uuid45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS694(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS689(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S676(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal4uuid28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal4uuid34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4uuid29____test__757569642e6d6274__0(
  
);

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr*,
  struct _M0TPB6Logger
);

int32_t _M0IPC14char4CharPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0MPC14char4Char13is__printable(int32_t);

int32_t _M0MPC14char4Char11is__control(int32_t);

moonbit_string_t _M0MPC14char4Char7to__hex(int32_t);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Hash13hash__combine(int32_t, struct _M0TPB6Hasher*);

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t,
  struct _M0TPB6Hasher*
);

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher*,
  moonbit_string_t
);

int32_t _M0MPC13int3Int13is__surrogate(int32_t);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

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

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1393l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t);

moonbit_string_t _M0FPB16char__to__string(int32_t);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(int32_t);

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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(int32_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t
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

moonbit_string_t _M0IPC14char4CharPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC14char4CharPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    117, 105, 100, 58, 117, 117, 105, 100, 46, 109, 98, 116, 58, 50, 
    49, 50, 58, 49, 49, 45, 50, 49, 50, 58, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
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
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_11 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 97, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[100]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 99), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 117, 
    105, 100, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    117, 105, 100, 58, 117, 117, 105, 100, 46, 109, 98, 116, 58, 50, 
    49, 50, 58, 51, 53, 45, 50, 49, 50, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    117, 105, 100, 58, 117, 117, 105, 100, 46, 109, 98, 116, 58, 50, 
    49, 50, 58, 51, 45, 50, 49, 50, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[98]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 97), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 117, 
    105, 100, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_24 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 117, 105, 100, 
    34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    117, 117, 105, 100, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
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
} const moonbit_string_literal_49 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_26 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_47 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 56, 49, 58, 57, 45, 56, 
    49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_38 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_43 =
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

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN17error__to__stringS769$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN17error__to__stringS769
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal4uuid39____test__757569642e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal4uuid39____test__757569642e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal4uuid35____test__757569642e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal4uuid39____test__757569642e6d6274__0_2edyncall$closure.data;

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
} _M0FP077Char_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC14char4CharPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC14char4CharPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP077Char_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP077Char_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1657 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal4uuid48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4uuid39____test__757569642e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1802
) {
  return _M0FP48clawteam8clawteam8internal4uuid29____test__757569642e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS790,
  moonbit_string_t _M0L8filenameS765,
  int32_t _M0L5indexS768
) {
  struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760* _closure_2074;
  struct _M0TWssbEu* _M0L14handle__resultS760;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS769;
  void* _M0L11_2atry__errS784;
  struct moonbit_result_0 _tmp_2076;
  int32_t _handle__error__result_2077;
  int32_t _M0L6_2atmpS1790;
  void* _M0L3errS785;
  moonbit_string_t _M0L4nameS787;
  struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS788;
  moonbit_string_t _M0L8_2afieldS1803;
  int32_t _M0L6_2acntS2010;
  moonbit_string_t _M0L7_2anameS789;
  #line 524 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS765);
  _closure_2074
  = (struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760*)moonbit_malloc(sizeof(struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760));
  Moonbit_object_header(_closure_2074)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760, $1) >> 2, 1, 0);
  _closure_2074->code
  = &_M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN14handle__resultS760;
  _closure_2074->$0 = _M0L5indexS768;
  _closure_2074->$1 = _M0L8filenameS765;
  _M0L14handle__resultS760 = (struct _M0TWssbEu*)_closure_2074;
  _M0L17error__to__stringS769
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN17error__to__stringS769$closure.data;
  moonbit_incref(_M0L12async__testsS790);
  moonbit_incref(_M0L17error__to__stringS769);
  moonbit_incref(_M0L8filenameS765);
  moonbit_incref(_M0L14handle__resultS760);
  #line 558 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _tmp_2076
  = _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__test(_M0L12async__testsS790, _M0L8filenameS765, _M0L5indexS768, _M0L14handle__resultS760, _M0L17error__to__stringS769);
  if (_tmp_2076.tag) {
    int32_t const _M0L5_2aokS1799 = _tmp_2076.data.ok;
    _handle__error__result_2077 = _M0L5_2aokS1799;
  } else {
    void* const _M0L6_2aerrS1800 = _tmp_2076.data.err;
    moonbit_decref(_M0L12async__testsS790);
    moonbit_decref(_M0L17error__to__stringS769);
    moonbit_decref(_M0L8filenameS765);
    _M0L11_2atry__errS784 = _M0L6_2aerrS1800;
    goto join_783;
  }
  if (_handle__error__result_2077) {
    moonbit_decref(_M0L12async__testsS790);
    moonbit_decref(_M0L17error__to__stringS769);
    moonbit_decref(_M0L8filenameS765);
    _M0L6_2atmpS1790 = 1;
  } else {
    struct moonbit_result_0 _tmp_2078;
    int32_t _handle__error__result_2079;
    moonbit_incref(_M0L12async__testsS790);
    moonbit_incref(_M0L17error__to__stringS769);
    moonbit_incref(_M0L8filenameS765);
    moonbit_incref(_M0L14handle__resultS760);
    #line 561 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    _tmp_2078
    = _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS790, _M0L8filenameS765, _M0L5indexS768, _M0L14handle__resultS760, _M0L17error__to__stringS769);
    if (_tmp_2078.tag) {
      int32_t const _M0L5_2aokS1797 = _tmp_2078.data.ok;
      _handle__error__result_2079 = _M0L5_2aokS1797;
    } else {
      void* const _M0L6_2aerrS1798 = _tmp_2078.data.err;
      moonbit_decref(_M0L12async__testsS790);
      moonbit_decref(_M0L17error__to__stringS769);
      moonbit_decref(_M0L8filenameS765);
      _M0L11_2atry__errS784 = _M0L6_2aerrS1798;
      goto join_783;
    }
    if (_handle__error__result_2079) {
      moonbit_decref(_M0L12async__testsS790);
      moonbit_decref(_M0L17error__to__stringS769);
      moonbit_decref(_M0L8filenameS765);
      _M0L6_2atmpS1790 = 1;
    } else {
      struct moonbit_result_0 _tmp_2080;
      int32_t _handle__error__result_2081;
      moonbit_incref(_M0L12async__testsS790);
      moonbit_incref(_M0L17error__to__stringS769);
      moonbit_incref(_M0L8filenameS765);
      moonbit_incref(_M0L14handle__resultS760);
      #line 564 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _tmp_2080
      = _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS790, _M0L8filenameS765, _M0L5indexS768, _M0L14handle__resultS760, _M0L17error__to__stringS769);
      if (_tmp_2080.tag) {
        int32_t const _M0L5_2aokS1795 = _tmp_2080.data.ok;
        _handle__error__result_2081 = _M0L5_2aokS1795;
      } else {
        void* const _M0L6_2aerrS1796 = _tmp_2080.data.err;
        moonbit_decref(_M0L12async__testsS790);
        moonbit_decref(_M0L17error__to__stringS769);
        moonbit_decref(_M0L8filenameS765);
        _M0L11_2atry__errS784 = _M0L6_2aerrS1796;
        goto join_783;
      }
      if (_handle__error__result_2081) {
        moonbit_decref(_M0L12async__testsS790);
        moonbit_decref(_M0L17error__to__stringS769);
        moonbit_decref(_M0L8filenameS765);
        _M0L6_2atmpS1790 = 1;
      } else {
        struct moonbit_result_0 _tmp_2082;
        int32_t _handle__error__result_2083;
        moonbit_incref(_M0L12async__testsS790);
        moonbit_incref(_M0L17error__to__stringS769);
        moonbit_incref(_M0L8filenameS765);
        moonbit_incref(_M0L14handle__resultS760);
        #line 567 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        _tmp_2082
        = _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS790, _M0L8filenameS765, _M0L5indexS768, _M0L14handle__resultS760, _M0L17error__to__stringS769);
        if (_tmp_2082.tag) {
          int32_t const _M0L5_2aokS1793 = _tmp_2082.data.ok;
          _handle__error__result_2083 = _M0L5_2aokS1793;
        } else {
          void* const _M0L6_2aerrS1794 = _tmp_2082.data.err;
          moonbit_decref(_M0L12async__testsS790);
          moonbit_decref(_M0L17error__to__stringS769);
          moonbit_decref(_M0L8filenameS765);
          _M0L11_2atry__errS784 = _M0L6_2aerrS1794;
          goto join_783;
        }
        if (_handle__error__result_2083) {
          moonbit_decref(_M0L12async__testsS790);
          moonbit_decref(_M0L17error__to__stringS769);
          moonbit_decref(_M0L8filenameS765);
          _M0L6_2atmpS1790 = 1;
        } else {
          struct moonbit_result_0 _tmp_2084;
          moonbit_incref(_M0L14handle__resultS760);
          #line 570 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
          _tmp_2084
          = _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS790, _M0L8filenameS765, _M0L5indexS768, _M0L14handle__resultS760, _M0L17error__to__stringS769);
          if (_tmp_2084.tag) {
            int32_t const _M0L5_2aokS1791 = _tmp_2084.data.ok;
            _M0L6_2atmpS1790 = _M0L5_2aokS1791;
          } else {
            void* const _M0L6_2aerrS1792 = _tmp_2084.data.err;
            _M0L11_2atry__errS784 = _M0L6_2aerrS1792;
            goto join_783;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1790) {
    void* _M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1801 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1801)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1801)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS784
    = _M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1801;
    goto join_783;
  } else {
    moonbit_decref(_M0L14handle__resultS760);
  }
  goto joinlet_2075;
  join_783:;
  _M0L3errS785 = _M0L11_2atry__errS784;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS788
  = (struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS785;
  _M0L8_2afieldS1803 = _M0L36_2aMoonBitTestDriverInternalSkipTestS788->$0;
  _M0L6_2acntS2010
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS788)->rc;
  if (_M0L6_2acntS2010 > 1) {
    int32_t _M0L11_2anew__cntS2011 = _M0L6_2acntS2010 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS788)->rc
    = _M0L11_2anew__cntS2011;
    moonbit_incref(_M0L8_2afieldS1803);
  } else if (_M0L6_2acntS2010 == 1) {
    #line 577 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS788);
  }
  _M0L7_2anameS789 = _M0L8_2afieldS1803;
  _M0L4nameS787 = _M0L7_2anameS789;
  goto join_786;
  goto joinlet_2085;
  join_786:;
  #line 578 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN14handle__resultS760(_M0L14handle__resultS760, _M0L4nameS787, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2085:;
  joinlet_2075:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN17error__to__stringS769(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1789,
  void* _M0L3errS770
) {
  void* _M0L1eS772;
  moonbit_string_t _M0L1eS774;
  #line 547 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS1789);
  switch (Moonbit_object_tag(_M0L3errS770)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS775 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS770;
      moonbit_string_t _M0L8_2afieldS1804 = _M0L10_2aFailureS775->$0;
      int32_t _M0L6_2acntS2012 =
        Moonbit_object_header(_M0L10_2aFailureS775)->rc;
      moonbit_string_t _M0L4_2aeS776;
      if (_M0L6_2acntS2012 > 1) {
        int32_t _M0L11_2anew__cntS2013 = _M0L6_2acntS2012 - 1;
        Moonbit_object_header(_M0L10_2aFailureS775)->rc
        = _M0L11_2anew__cntS2013;
        moonbit_incref(_M0L8_2afieldS1804);
      } else if (_M0L6_2acntS2012 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS775);
      }
      _M0L4_2aeS776 = _M0L8_2afieldS1804;
      _M0L1eS774 = _M0L4_2aeS776;
      goto join_773;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS777 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS770;
      moonbit_string_t _M0L8_2afieldS1805 = _M0L15_2aInspectErrorS777->$0;
      int32_t _M0L6_2acntS2014 =
        Moonbit_object_header(_M0L15_2aInspectErrorS777)->rc;
      moonbit_string_t _M0L4_2aeS778;
      if (_M0L6_2acntS2014 > 1) {
        int32_t _M0L11_2anew__cntS2015 = _M0L6_2acntS2014 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS777)->rc
        = _M0L11_2anew__cntS2015;
        moonbit_incref(_M0L8_2afieldS1805);
      } else if (_M0L6_2acntS2014 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS777);
      }
      _M0L4_2aeS778 = _M0L8_2afieldS1805;
      _M0L1eS774 = _M0L4_2aeS778;
      goto join_773;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS779 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS770;
      moonbit_string_t _M0L8_2afieldS1806 = _M0L16_2aSnapshotErrorS779->$0;
      int32_t _M0L6_2acntS2016 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS779)->rc;
      moonbit_string_t _M0L4_2aeS780;
      if (_M0L6_2acntS2016 > 1) {
        int32_t _M0L11_2anew__cntS2017 = _M0L6_2acntS2016 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS779)->rc
        = _M0L11_2anew__cntS2017;
        moonbit_incref(_M0L8_2afieldS1806);
      } else if (_M0L6_2acntS2016 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS779);
      }
      _M0L4_2aeS780 = _M0L8_2afieldS1806;
      _M0L1eS774 = _M0L4_2aeS780;
      goto join_773;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS781 =
        (struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS770;
      moonbit_string_t _M0L8_2afieldS1807 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS781->$0;
      int32_t _M0L6_2acntS2018 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS781)->rc;
      moonbit_string_t _M0L4_2aeS782;
      if (_M0L6_2acntS2018 > 1) {
        int32_t _M0L11_2anew__cntS2019 = _M0L6_2acntS2018 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS781)->rc
        = _M0L11_2anew__cntS2019;
        moonbit_incref(_M0L8_2afieldS1807);
      } else if (_M0L6_2acntS2018 == 1) {
        #line 548 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS781);
      }
      _M0L4_2aeS782 = _M0L8_2afieldS1807;
      _M0L1eS774 = _M0L4_2aeS782;
      goto join_773;
      break;
    }
    default: {
      _M0L1eS772 = _M0L3errS770;
      goto join_771;
      break;
    }
  }
  join_773:;
  return _M0L1eS774;
  join_771:;
  #line 553 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS772);
}

int32_t _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__executeN14handle__resultS760(
  struct _M0TWssbEu* _M0L6_2aenvS1775,
  moonbit_string_t _M0L8testnameS761,
  moonbit_string_t _M0L7messageS762,
  int32_t _M0L7skippedS763
) {
  struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760* _M0L14_2acasted__envS1776;
  moonbit_string_t _M0L8_2afieldS1817;
  moonbit_string_t _M0L8filenameS765;
  int32_t _M0L8_2afieldS1816;
  int32_t _M0L6_2acntS2020;
  int32_t _M0L5indexS768;
  int32_t _if__result_2088;
  moonbit_string_t _M0L10file__nameS764;
  moonbit_string_t _M0L10test__nameS766;
  moonbit_string_t _M0L7messageS767;
  moonbit_string_t _M0L6_2atmpS1788;
  moonbit_string_t _M0L6_2atmpS1815;
  moonbit_string_t _M0L6_2atmpS1787;
  moonbit_string_t _M0L6_2atmpS1814;
  moonbit_string_t _M0L6_2atmpS1785;
  moonbit_string_t _M0L6_2atmpS1786;
  moonbit_string_t _M0L6_2atmpS1813;
  moonbit_string_t _M0L6_2atmpS1784;
  moonbit_string_t _M0L6_2atmpS1812;
  moonbit_string_t _M0L6_2atmpS1782;
  moonbit_string_t _M0L6_2atmpS1783;
  moonbit_string_t _M0L6_2atmpS1811;
  moonbit_string_t _M0L6_2atmpS1781;
  moonbit_string_t _M0L6_2atmpS1810;
  moonbit_string_t _M0L6_2atmpS1779;
  moonbit_string_t _M0L6_2atmpS1780;
  moonbit_string_t _M0L6_2atmpS1809;
  moonbit_string_t _M0L6_2atmpS1778;
  moonbit_string_t _M0L6_2atmpS1808;
  moonbit_string_t _M0L6_2atmpS1777;
  #line 531 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1776
  = (struct _M0R110_24clawteam_2fclawteam_2finternal_2fuuid_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c760*)_M0L6_2aenvS1775;
  _M0L8_2afieldS1817 = _M0L14_2acasted__envS1776->$1;
  _M0L8filenameS765 = _M0L8_2afieldS1817;
  _M0L8_2afieldS1816 = _M0L14_2acasted__envS1776->$0;
  _M0L6_2acntS2020 = Moonbit_object_header(_M0L14_2acasted__envS1776)->rc;
  if (_M0L6_2acntS2020 > 1) {
    int32_t _M0L11_2anew__cntS2021 = _M0L6_2acntS2020 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1776)->rc
    = _M0L11_2anew__cntS2021;
    moonbit_incref(_M0L8filenameS765);
  } else if (_M0L6_2acntS2020 == 1) {
    #line 531 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1776);
  }
  _M0L5indexS768 = _M0L8_2afieldS1816;
  if (!_M0L7skippedS763) {
    _if__result_2088 = 1;
  } else {
    _if__result_2088 = 0;
  }
  if (_if__result_2088) {
    
  }
  #line 537 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS764 = _M0MPC16string6String6escape(_M0L8filenameS765);
  #line 538 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS766 = _M0MPC16string6String6escape(_M0L8testnameS761);
  #line 539 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS767 = _M0MPC16string6String6escape(_M0L7messageS762);
  #line 540 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 542 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1788
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS764);
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1815
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1788);
  moonbit_decref(_M0L6_2atmpS1788);
  _M0L6_2atmpS1787 = _M0L6_2atmpS1815;
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1814
  = moonbit_add_string(_M0L6_2atmpS1787, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1787);
  _M0L6_2atmpS1785 = _M0L6_2atmpS1814;
  #line 542 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1786
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS768);
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1813 = moonbit_add_string(_M0L6_2atmpS1785, _M0L6_2atmpS1786);
  moonbit_decref(_M0L6_2atmpS1785);
  moonbit_decref(_M0L6_2atmpS1786);
  _M0L6_2atmpS1784 = _M0L6_2atmpS1813;
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1812
  = moonbit_add_string(_M0L6_2atmpS1784, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1784);
  _M0L6_2atmpS1782 = _M0L6_2atmpS1812;
  #line 542 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1783
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS766);
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1811 = moonbit_add_string(_M0L6_2atmpS1782, _M0L6_2atmpS1783);
  moonbit_decref(_M0L6_2atmpS1782);
  moonbit_decref(_M0L6_2atmpS1783);
  _M0L6_2atmpS1781 = _M0L6_2atmpS1811;
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1810
  = moonbit_add_string(_M0L6_2atmpS1781, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1781);
  _M0L6_2atmpS1779 = _M0L6_2atmpS1810;
  #line 542 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1780
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS767);
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1809 = moonbit_add_string(_M0L6_2atmpS1779, _M0L6_2atmpS1780);
  moonbit_decref(_M0L6_2atmpS1779);
  moonbit_decref(_M0L6_2atmpS1780);
  _M0L6_2atmpS1778 = _M0L6_2atmpS1809;
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1808
  = moonbit_add_string(_M0L6_2atmpS1778, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1778);
  _M0L6_2atmpS1777 = _M0L6_2atmpS1808;
  #line 541 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1777);
  #line 544 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S759,
  moonbit_string_t _M0L8filenameS756,
  int32_t _M0L5indexS750,
  struct _M0TWssbEu* _M0L14handle__resultS746,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS748
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS726;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS755;
  struct _M0TWEuQRPC15error5Error* _M0L1fS728;
  moonbit_string_t* _M0L5attrsS729;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS749;
  moonbit_string_t _M0L4nameS732;
  moonbit_string_t _M0L4nameS730;
  int32_t _M0L6_2atmpS1774;
  struct _M0TWEOs* _M0L5_2aitS734;
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__* _closure_2097;
  struct _M0TWEu* _M0L6_2atmpS1765;
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__* _closure_2098;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS1766;
  struct moonbit_result_0 _result_2099;
  #line 405 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S759);
  moonbit_incref(_M0FP48clawteam8clawteam8internal4uuid48moonbit__test__driver__internal__no__args__tests);
  #line 412 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS755
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal4uuid48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS756);
  if (_M0L7_2abindS755 == 0) {
    struct moonbit_result_0 _result_2090;
    if (_M0L7_2abindS755) {
      moonbit_decref(_M0L7_2abindS755);
    }
    moonbit_decref(_M0L17error__to__stringS748);
    moonbit_decref(_M0L14handle__resultS746);
    _result_2090.tag = 1;
    _result_2090.data.ok = 0;
    return _result_2090;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS757 =
      _M0L7_2abindS755;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS758 =
      _M0L7_2aSomeS757;
    _M0L10index__mapS726 = _M0L13_2aindex__mapS758;
    goto join_725;
  }
  join_725:;
  #line 414 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS749
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS726, _M0L5indexS750);
  if (_M0L7_2abindS749 == 0) {
    struct moonbit_result_0 _result_2092;
    if (_M0L7_2abindS749) {
      moonbit_decref(_M0L7_2abindS749);
    }
    moonbit_decref(_M0L17error__to__stringS748);
    moonbit_decref(_M0L14handle__resultS746);
    _result_2092.tag = 1;
    _result_2092.data.ok = 0;
    return _result_2092;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS751 = _M0L7_2abindS749;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS752 = _M0L7_2aSomeS751;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS1821 = _M0L4_2axS752->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS753 = _M0L8_2afieldS1821;
    moonbit_string_t* _M0L8_2afieldS1820 = _M0L4_2axS752->$1;
    int32_t _M0L6_2acntS2022 = Moonbit_object_header(_M0L4_2axS752)->rc;
    moonbit_string_t* _M0L8_2aattrsS754;
    if (_M0L6_2acntS2022 > 1) {
      int32_t _M0L11_2anew__cntS2023 = _M0L6_2acntS2022 - 1;
      Moonbit_object_header(_M0L4_2axS752)->rc = _M0L11_2anew__cntS2023;
      moonbit_incref(_M0L8_2afieldS1820);
      moonbit_incref(_M0L4_2afS753);
    } else if (_M0L6_2acntS2022 == 1) {
      #line 412 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS752);
    }
    _M0L8_2aattrsS754 = _M0L8_2afieldS1820;
    _M0L1fS728 = _M0L4_2afS753;
    _M0L5attrsS729 = _M0L8_2aattrsS754;
    goto join_727;
  }
  join_727:;
  _M0L6_2atmpS1774 = Moonbit_array_length(_M0L5attrsS729);
  if (_M0L6_2atmpS1774 >= 1) {
    moonbit_string_t _M0L6_2atmpS1819 = (moonbit_string_t)_M0L5attrsS729[0];
    moonbit_string_t _M0L7_2anameS733 = _M0L6_2atmpS1819;
    moonbit_incref(_M0L7_2anameS733);
    _M0L4nameS732 = _M0L7_2anameS733;
    goto join_731;
  } else {
    _M0L4nameS730 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_2093;
  join_731:;
  _M0L4nameS730 = _M0L4nameS732;
  joinlet_2093:;
  #line 415 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS734 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS729);
  while (1) {
    moonbit_string_t _M0L4attrS736;
    moonbit_string_t _M0L7_2abindS743;
    int32_t _M0L6_2atmpS1758;
    int64_t _M0L6_2atmpS1757;
    moonbit_incref(_M0L5_2aitS734);
    #line 417 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS743 = _M0MPB4Iter4nextGsE(_M0L5_2aitS734);
    if (_M0L7_2abindS743 == 0) {
      if (_M0L7_2abindS743) {
        moonbit_decref(_M0L7_2abindS743);
      }
      moonbit_decref(_M0L5_2aitS734);
    } else {
      moonbit_string_t _M0L7_2aSomeS744 = _M0L7_2abindS743;
      moonbit_string_t _M0L7_2aattrS745 = _M0L7_2aSomeS744;
      _M0L4attrS736 = _M0L7_2aattrS745;
      goto join_735;
    }
    goto joinlet_2095;
    join_735:;
    _M0L6_2atmpS1758 = Moonbit_array_length(_M0L4attrS736);
    _M0L6_2atmpS1757 = (int64_t)_M0L6_2atmpS1758;
    moonbit_incref(_M0L4attrS736);
    #line 418 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS736, 5, 0, _M0L6_2atmpS1757)
    ) {
      int32_t _M0L6_2atmpS1764 = _M0L4attrS736[0];
      int32_t _M0L4_2axS737 = _M0L6_2atmpS1764;
      if (_M0L4_2axS737 == 112) {
        int32_t _M0L6_2atmpS1763 = _M0L4attrS736[1];
        int32_t _M0L4_2axS738 = _M0L6_2atmpS1763;
        if (_M0L4_2axS738 == 97) {
          int32_t _M0L6_2atmpS1762 = _M0L4attrS736[2];
          int32_t _M0L4_2axS739 = _M0L6_2atmpS1762;
          if (_M0L4_2axS739 == 110) {
            int32_t _M0L6_2atmpS1761 = _M0L4attrS736[3];
            int32_t _M0L4_2axS740 = _M0L6_2atmpS1761;
            if (_M0L4_2axS740 == 105) {
              int32_t _M0L6_2atmpS1818 = _M0L4attrS736[4];
              int32_t _M0L6_2atmpS1760;
              int32_t _M0L4_2axS741;
              moonbit_decref(_M0L4attrS736);
              _M0L6_2atmpS1760 = _M0L6_2atmpS1818;
              _M0L4_2axS741 = _M0L6_2atmpS1760;
              if (_M0L4_2axS741 == 99) {
                void* _M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1759;
                struct moonbit_result_0 _result_2096;
                moonbit_decref(_M0L17error__to__stringS748);
                moonbit_decref(_M0L14handle__resultS746);
                moonbit_decref(_M0L5_2aitS734);
                moonbit_decref(_M0L1fS728);
                _M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1759
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1759)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1759)->$0
                = _M0L4nameS730;
                _result_2096.tag = 0;
                _result_2096.data.err
                = _M0L109clawteam_2fclawteam_2finternal_2fuuid_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1759;
                return _result_2096;
              }
            } else {
              moonbit_decref(_M0L4attrS736);
            }
          } else {
            moonbit_decref(_M0L4attrS736);
          }
        } else {
          moonbit_decref(_M0L4attrS736);
        }
      } else {
        moonbit_decref(_M0L4attrS736);
      }
    } else {
      moonbit_decref(_M0L4attrS736);
    }
    continue;
    joinlet_2095:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS746);
  moonbit_incref(_M0L4nameS730);
  _closure_2097
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__*)moonbit_malloc(sizeof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__));
  Moonbit_object_header(_closure_2097)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__, $0) >> 2, 2, 0);
  _closure_2097->code
  = &_M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testC1771l425;
  _closure_2097->$0 = _M0L14handle__resultS746;
  _closure_2097->$1 = _M0L4nameS730;
  _M0L6_2atmpS1765 = (struct _M0TWEu*)_closure_2097;
  _closure_2098
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__*)moonbit_malloc(sizeof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__));
  Moonbit_object_header(_closure_2098)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__, $0) >> 2, 3, 0);
  _closure_2098->code
  = &_M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testC1767l426;
  _closure_2098->$0 = _M0L17error__to__stringS748;
  _closure_2098->$1 = _M0L14handle__resultS746;
  _closure_2098->$2 = _M0L4nameS730;
  _M0L6_2atmpS1766 = (struct _M0TWRPC15error5ErrorEu*)_closure_2098;
  #line 423 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal4uuid45moonbit__test__driver__internal__catch__error(_M0L1fS728, _M0L6_2atmpS1765, _M0L6_2atmpS1766);
  _result_2099.tag = 1;
  _result_2099.data.ok = 1;
  return _result_2099;
}

int32_t _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testC1771l425(
  struct _M0TWEu* _M0L6_2aenvS1772
) {
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__* _M0L14_2acasted__envS1773;
  moonbit_string_t _M0L8_2afieldS1823;
  moonbit_string_t _M0L4nameS730;
  struct _M0TWssbEu* _M0L8_2afieldS1822;
  int32_t _M0L6_2acntS2024;
  struct _M0TWssbEu* _M0L14handle__resultS746;
  #line 425 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1773
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1771__l425__*)_M0L6_2aenvS1772;
  _M0L8_2afieldS1823 = _M0L14_2acasted__envS1773->$1;
  _M0L4nameS730 = _M0L8_2afieldS1823;
  _M0L8_2afieldS1822 = _M0L14_2acasted__envS1773->$0;
  _M0L6_2acntS2024 = Moonbit_object_header(_M0L14_2acasted__envS1773)->rc;
  if (_M0L6_2acntS2024 > 1) {
    int32_t _M0L11_2anew__cntS2025 = _M0L6_2acntS2024 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1773)->rc
    = _M0L11_2anew__cntS2025;
    moonbit_incref(_M0L4nameS730);
    moonbit_incref(_M0L8_2afieldS1822);
  } else if (_M0L6_2acntS2024 == 1) {
    #line 425 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1773);
  }
  _M0L14handle__resultS746 = _M0L8_2afieldS1822;
  #line 425 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS746->code(_M0L14handle__resultS746, _M0L4nameS730, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal4uuid41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testC1767l426(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS1768,
  void* _M0L3errS747
) {
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__* _M0L14_2acasted__envS1769;
  moonbit_string_t _M0L8_2afieldS1826;
  moonbit_string_t _M0L4nameS730;
  struct _M0TWssbEu* _M0L8_2afieldS1825;
  struct _M0TWssbEu* _M0L14handle__resultS746;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS1824;
  int32_t _M0L6_2acntS2026;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS748;
  moonbit_string_t _M0L6_2atmpS1770;
  #line 426 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1769
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fuuid_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1767__l426__*)_M0L6_2aenvS1768;
  _M0L8_2afieldS1826 = _M0L14_2acasted__envS1769->$2;
  _M0L4nameS730 = _M0L8_2afieldS1826;
  _M0L8_2afieldS1825 = _M0L14_2acasted__envS1769->$1;
  _M0L14handle__resultS746 = _M0L8_2afieldS1825;
  _M0L8_2afieldS1824 = _M0L14_2acasted__envS1769->$0;
  _M0L6_2acntS2026 = Moonbit_object_header(_M0L14_2acasted__envS1769)->rc;
  if (_M0L6_2acntS2026 > 1) {
    int32_t _M0L11_2anew__cntS2027 = _M0L6_2acntS2026 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1769)->rc
    = _M0L11_2anew__cntS2027;
    moonbit_incref(_M0L4nameS730);
    moonbit_incref(_M0L14handle__resultS746);
    moonbit_incref(_M0L8_2afieldS1824);
  } else if (_M0L6_2acntS2026 == 1) {
    #line 426 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1769);
  }
  _M0L17error__to__stringS748 = _M0L8_2afieldS1824;
  #line 426 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1770
  = _M0L17error__to__stringS748->code(_M0L17error__to__stringS748, _M0L3errS747);
  #line 426 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS746->code(_M0L14handle__resultS746, _M0L4nameS730, _M0L6_2atmpS1770, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal4uuid45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS721,
  struct _M0TWEu* _M0L6on__okS722,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS719
) {
  void* _M0L11_2atry__errS717;
  struct moonbit_result_0 _tmp_2101;
  void* _M0L3errS718;
  #line 375 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _tmp_2101 = _M0L1fS721->code(_M0L1fS721);
  if (_tmp_2101.tag) {
    int32_t const _M0L5_2aokS1755 = _tmp_2101.data.ok;
    moonbit_decref(_M0L7on__errS719);
  } else {
    void* const _M0L6_2aerrS1756 = _tmp_2101.data.err;
    moonbit_decref(_M0L6on__okS722);
    _M0L11_2atry__errS717 = _M0L6_2aerrS1756;
    goto join_716;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS722->code(_M0L6on__okS722);
  goto joinlet_2100;
  join_716:;
  _M0L3errS718 = _M0L11_2atry__errS717;
  #line 383 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS719->code(_M0L7on__errS719, _M0L3errS718);
  joinlet_2100:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S676;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS689;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS694;
  struct _M0TUsiE** _M0L6_2atmpS1754;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS701;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS702;
  moonbit_string_t _M0L6_2atmpS1753;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS703;
  int32_t _M0L7_2abindS704;
  int32_t _M0L2__S705;
  #line 193 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S676 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS689
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS694 = 0;
  _M0L6_2atmpS1754 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS701
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS701)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS701->$0 = _M0L6_2atmpS1754;
  _M0L16file__and__indexS701->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS702
  = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS689(_M0L57moonbit__test__driver__internal__get__cli__args__internalS689);
  #line 284 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1753 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS702, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS703
  = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS694(_M0L51moonbit__test__driver__internal__split__mbt__stringS694, _M0L6_2atmpS1753, 47);
  _M0L7_2abindS704 = _M0L10test__argsS703->$1;
  _M0L2__S705 = 0;
  while (1) {
    if (_M0L2__S705 < _M0L7_2abindS704) {
      moonbit_string_t* _M0L8_2afieldS1828 = _M0L10test__argsS703->$0;
      moonbit_string_t* _M0L3bufS1752 = _M0L8_2afieldS1828;
      moonbit_string_t _M0L6_2atmpS1827 =
        (moonbit_string_t)_M0L3bufS1752[_M0L2__S705];
      moonbit_string_t _M0L3argS706 = _M0L6_2atmpS1827;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS707;
      moonbit_string_t _M0L4fileS708;
      moonbit_string_t _M0L5rangeS709;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS710;
      moonbit_string_t _M0L6_2atmpS1750;
      int32_t _M0L5startS711;
      moonbit_string_t _M0L6_2atmpS1749;
      int32_t _M0L3endS712;
      int32_t _M0L1iS713;
      int32_t _M0L6_2atmpS1751;
      moonbit_incref(_M0L3argS706);
      #line 288 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS707
      = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS694(_M0L51moonbit__test__driver__internal__split__mbt__stringS694, _M0L3argS706, 58);
      moonbit_incref(_M0L16file__and__rangeS707);
      #line 289 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS708
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS707, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS709
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS707, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS710
      = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS694(_M0L51moonbit__test__driver__internal__split__mbt__stringS694, _M0L5rangeS709, 45);
      moonbit_incref(_M0L15start__and__endS710);
      #line 294 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1750
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS710, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L5startS711
      = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S676(_M0L45moonbit__test__driver__internal__parse__int__S676, _M0L6_2atmpS1750);
      #line 295 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1749
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS710, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L3endS712
      = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S676(_M0L45moonbit__test__driver__internal__parse__int__S676, _M0L6_2atmpS1749);
      _M0L1iS713 = _M0L5startS711;
      while (1) {
        if (_M0L1iS713 < _M0L3endS712) {
          struct _M0TUsiE* _M0L8_2atupleS1747;
          int32_t _M0L6_2atmpS1748;
          moonbit_incref(_M0L4fileS708);
          _M0L8_2atupleS1747
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1747)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1747->$0 = _M0L4fileS708;
          _M0L8_2atupleS1747->$1 = _M0L1iS713;
          moonbit_incref(_M0L16file__and__indexS701);
          #line 297 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS701, _M0L8_2atupleS1747);
          _M0L6_2atmpS1748 = _M0L1iS713 + 1;
          _M0L1iS713 = _M0L6_2atmpS1748;
          continue;
        } else {
          moonbit_decref(_M0L4fileS708);
        }
        break;
      }
      _M0L6_2atmpS1751 = _M0L2__S705 + 1;
      _M0L2__S705 = _M0L6_2atmpS1751;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS703);
    }
    break;
  }
  return _M0L16file__and__indexS701;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS694(
  int32_t _M0L6_2aenvS1728,
  moonbit_string_t _M0L1sS695,
  int32_t _M0L3sepS696
) {
  moonbit_string_t* _M0L6_2atmpS1746;
  struct _M0TPB5ArrayGsE* _M0L3resS697;
  struct _M0TPC13ref3RefGiE* _M0L1iS698;
  struct _M0TPC13ref3RefGiE* _M0L5startS699;
  int32_t _M0L3valS1741;
  int32_t _M0L6_2atmpS1742;
  #line 261 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1746 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS697
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS697)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS697->$0 = _M0L6_2atmpS1746;
  _M0L3resS697->$1 = 0;
  _M0L1iS698
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS698)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS698->$0 = 0;
  _M0L5startS699
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS699)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS699->$0 = 0;
  while (1) {
    int32_t _M0L3valS1729 = _M0L1iS698->$0;
    int32_t _M0L6_2atmpS1730 = Moonbit_array_length(_M0L1sS695);
    if (_M0L3valS1729 < _M0L6_2atmpS1730) {
      int32_t _M0L3valS1733 = _M0L1iS698->$0;
      int32_t _M0L6_2atmpS1732;
      int32_t _M0L6_2atmpS1731;
      int32_t _M0L3valS1740;
      int32_t _M0L6_2atmpS1739;
      if (
        _M0L3valS1733 < 0
        || _M0L3valS1733 >= Moonbit_array_length(_M0L1sS695)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1732 = _M0L1sS695[_M0L3valS1733];
      _M0L6_2atmpS1731 = _M0L6_2atmpS1732;
      if (_M0L6_2atmpS1731 == _M0L3sepS696) {
        int32_t _M0L3valS1735 = _M0L5startS699->$0;
        int32_t _M0L3valS1736 = _M0L1iS698->$0;
        moonbit_string_t _M0L6_2atmpS1734;
        int32_t _M0L3valS1738;
        int32_t _M0L6_2atmpS1737;
        moonbit_incref(_M0L1sS695);
        #line 270 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS1734
        = _M0MPC16string6String17unsafe__substring(_M0L1sS695, _M0L3valS1735, _M0L3valS1736);
        moonbit_incref(_M0L3resS697);
        #line 270 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS697, _M0L6_2atmpS1734);
        _M0L3valS1738 = _M0L1iS698->$0;
        _M0L6_2atmpS1737 = _M0L3valS1738 + 1;
        _M0L5startS699->$0 = _M0L6_2atmpS1737;
      }
      _M0L3valS1740 = _M0L1iS698->$0;
      _M0L6_2atmpS1739 = _M0L3valS1740 + 1;
      _M0L1iS698->$0 = _M0L6_2atmpS1739;
      continue;
    } else {
      moonbit_decref(_M0L1iS698);
    }
    break;
  }
  _M0L3valS1741 = _M0L5startS699->$0;
  _M0L6_2atmpS1742 = Moonbit_array_length(_M0L1sS695);
  if (_M0L3valS1741 < _M0L6_2atmpS1742) {
    int32_t _M0L8_2afieldS1829 = _M0L5startS699->$0;
    int32_t _M0L3valS1744;
    int32_t _M0L6_2atmpS1745;
    moonbit_string_t _M0L6_2atmpS1743;
    moonbit_decref(_M0L5startS699);
    _M0L3valS1744 = _M0L8_2afieldS1829;
    _M0L6_2atmpS1745 = Moonbit_array_length(_M0L1sS695);
    #line 276 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS1743
    = _M0MPC16string6String17unsafe__substring(_M0L1sS695, _M0L3valS1744, _M0L6_2atmpS1745);
    moonbit_incref(_M0L3resS697);
    #line 276 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS697, _M0L6_2atmpS1743);
  } else {
    moonbit_decref(_M0L5startS699);
    moonbit_decref(_M0L1sS695);
  }
  return _M0L3resS697;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS689(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682
) {
  moonbit_bytes_t* _M0L3tmpS690;
  int32_t _M0L6_2atmpS1727;
  struct _M0TPB5ArrayGsE* _M0L3resS691;
  int32_t _M0L1iS692;
  #line 250 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS690
  = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1727 = Moonbit_array_length(_M0L3tmpS690);
  #line 254 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L3resS691 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1727);
  _M0L1iS692 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1723 = Moonbit_array_length(_M0L3tmpS690);
    if (_M0L1iS692 < _M0L6_2atmpS1723) {
      moonbit_bytes_t _M0L6_2atmpS1830;
      moonbit_bytes_t _M0L6_2atmpS1725;
      moonbit_string_t _M0L6_2atmpS1724;
      int32_t _M0L6_2atmpS1726;
      if (_M0L1iS692 < 0 || _M0L1iS692 >= Moonbit_array_length(_M0L3tmpS690)) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1830 = (moonbit_bytes_t)_M0L3tmpS690[_M0L1iS692];
      _M0L6_2atmpS1725 = _M0L6_2atmpS1830;
      moonbit_incref(_M0L6_2atmpS1725);
      #line 256 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1724
      = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682, _M0L6_2atmpS1725);
      moonbit_incref(_M0L3resS691);
      #line 256 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS691, _M0L6_2atmpS1724);
      _M0L6_2atmpS1726 = _M0L1iS692 + 1;
      _M0L1iS692 = _M0L6_2atmpS1726;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS690);
    }
    break;
  }
  return _M0L3resS691;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS682(
  int32_t _M0L6_2aenvS1637,
  moonbit_bytes_t _M0L5bytesS683
) {
  struct _M0TPB13StringBuilder* _M0L3resS684;
  int32_t _M0L3lenS685;
  struct _M0TPC13ref3RefGiE* _M0L1iS686;
  #line 206 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L3resS684 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS685 = Moonbit_array_length(_M0L5bytesS683);
  _M0L1iS686
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS686)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS686->$0 = 0;
  while (1) {
    int32_t _M0L3valS1638 = _M0L1iS686->$0;
    if (_M0L3valS1638 < _M0L3lenS685) {
      int32_t _M0L3valS1722 = _M0L1iS686->$0;
      int32_t _M0L6_2atmpS1721;
      int32_t _M0L6_2atmpS1720;
      struct _M0TPC13ref3RefGiE* _M0L1cS687;
      int32_t _M0L3valS1639;
      if (
        _M0L3valS1722 < 0
        || _M0L3valS1722 >= Moonbit_array_length(_M0L5bytesS683)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1721 = _M0L5bytesS683[_M0L3valS1722];
      _M0L6_2atmpS1720 = (int32_t)_M0L6_2atmpS1721;
      _M0L1cS687
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS687)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS687->$0 = _M0L6_2atmpS1720;
      _M0L3valS1639 = _M0L1cS687->$0;
      if (_M0L3valS1639 < 128) {
        int32_t _M0L8_2afieldS1831 = _M0L1cS687->$0;
        int32_t _M0L3valS1641;
        int32_t _M0L6_2atmpS1640;
        int32_t _M0L3valS1643;
        int32_t _M0L6_2atmpS1642;
        moonbit_decref(_M0L1cS687);
        _M0L3valS1641 = _M0L8_2afieldS1831;
        _M0L6_2atmpS1640 = _M0L3valS1641;
        moonbit_incref(_M0L3resS684);
        #line 215 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS684, _M0L6_2atmpS1640);
        _M0L3valS1643 = _M0L1iS686->$0;
        _M0L6_2atmpS1642 = _M0L3valS1643 + 1;
        _M0L1iS686->$0 = _M0L6_2atmpS1642;
      } else {
        int32_t _M0L3valS1644 = _M0L1cS687->$0;
        if (_M0L3valS1644 < 224) {
          int32_t _M0L3valS1646 = _M0L1iS686->$0;
          int32_t _M0L6_2atmpS1645 = _M0L3valS1646 + 1;
          int32_t _M0L3valS1655;
          int32_t _M0L6_2atmpS1654;
          int32_t _M0L6_2atmpS1648;
          int32_t _M0L3valS1653;
          int32_t _M0L6_2atmpS1652;
          int32_t _M0L6_2atmpS1651;
          int32_t _M0L6_2atmpS1650;
          int32_t _M0L6_2atmpS1649;
          int32_t _M0L6_2atmpS1647;
          int32_t _M0L8_2afieldS1832;
          int32_t _M0L3valS1657;
          int32_t _M0L6_2atmpS1656;
          int32_t _M0L3valS1659;
          int32_t _M0L6_2atmpS1658;
          if (_M0L6_2atmpS1645 >= _M0L3lenS685) {
            moonbit_decref(_M0L1cS687);
            moonbit_decref(_M0L1iS686);
            moonbit_decref(_M0L5bytesS683);
            break;
          }
          _M0L3valS1655 = _M0L1cS687->$0;
          _M0L6_2atmpS1654 = _M0L3valS1655 & 31;
          _M0L6_2atmpS1648 = _M0L6_2atmpS1654 << 6;
          _M0L3valS1653 = _M0L1iS686->$0;
          _M0L6_2atmpS1652 = _M0L3valS1653 + 1;
          if (
            _M0L6_2atmpS1652 < 0
            || _M0L6_2atmpS1652 >= Moonbit_array_length(_M0L5bytesS683)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1651 = _M0L5bytesS683[_M0L6_2atmpS1652];
          _M0L6_2atmpS1650 = (int32_t)_M0L6_2atmpS1651;
          _M0L6_2atmpS1649 = _M0L6_2atmpS1650 & 63;
          _M0L6_2atmpS1647 = _M0L6_2atmpS1648 | _M0L6_2atmpS1649;
          _M0L1cS687->$0 = _M0L6_2atmpS1647;
          _M0L8_2afieldS1832 = _M0L1cS687->$0;
          moonbit_decref(_M0L1cS687);
          _M0L3valS1657 = _M0L8_2afieldS1832;
          _M0L6_2atmpS1656 = _M0L3valS1657;
          moonbit_incref(_M0L3resS684);
          #line 222 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS684, _M0L6_2atmpS1656);
          _M0L3valS1659 = _M0L1iS686->$0;
          _M0L6_2atmpS1658 = _M0L3valS1659 + 2;
          _M0L1iS686->$0 = _M0L6_2atmpS1658;
        } else {
          int32_t _M0L3valS1660 = _M0L1cS687->$0;
          if (_M0L3valS1660 < 240) {
            int32_t _M0L3valS1662 = _M0L1iS686->$0;
            int32_t _M0L6_2atmpS1661 = _M0L3valS1662 + 2;
            int32_t _M0L3valS1678;
            int32_t _M0L6_2atmpS1677;
            int32_t _M0L6_2atmpS1670;
            int32_t _M0L3valS1676;
            int32_t _M0L6_2atmpS1675;
            int32_t _M0L6_2atmpS1674;
            int32_t _M0L6_2atmpS1673;
            int32_t _M0L6_2atmpS1672;
            int32_t _M0L6_2atmpS1671;
            int32_t _M0L6_2atmpS1664;
            int32_t _M0L3valS1669;
            int32_t _M0L6_2atmpS1668;
            int32_t _M0L6_2atmpS1667;
            int32_t _M0L6_2atmpS1666;
            int32_t _M0L6_2atmpS1665;
            int32_t _M0L6_2atmpS1663;
            int32_t _M0L8_2afieldS1833;
            int32_t _M0L3valS1680;
            int32_t _M0L6_2atmpS1679;
            int32_t _M0L3valS1682;
            int32_t _M0L6_2atmpS1681;
            if (_M0L6_2atmpS1661 >= _M0L3lenS685) {
              moonbit_decref(_M0L1cS687);
              moonbit_decref(_M0L1iS686);
              moonbit_decref(_M0L5bytesS683);
              break;
            }
            _M0L3valS1678 = _M0L1cS687->$0;
            _M0L6_2atmpS1677 = _M0L3valS1678 & 15;
            _M0L6_2atmpS1670 = _M0L6_2atmpS1677 << 12;
            _M0L3valS1676 = _M0L1iS686->$0;
            _M0L6_2atmpS1675 = _M0L3valS1676 + 1;
            if (
              _M0L6_2atmpS1675 < 0
              || _M0L6_2atmpS1675 >= Moonbit_array_length(_M0L5bytesS683)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1674 = _M0L5bytesS683[_M0L6_2atmpS1675];
            _M0L6_2atmpS1673 = (int32_t)_M0L6_2atmpS1674;
            _M0L6_2atmpS1672 = _M0L6_2atmpS1673 & 63;
            _M0L6_2atmpS1671 = _M0L6_2atmpS1672 << 6;
            _M0L6_2atmpS1664 = _M0L6_2atmpS1670 | _M0L6_2atmpS1671;
            _M0L3valS1669 = _M0L1iS686->$0;
            _M0L6_2atmpS1668 = _M0L3valS1669 + 2;
            if (
              _M0L6_2atmpS1668 < 0
              || _M0L6_2atmpS1668 >= Moonbit_array_length(_M0L5bytesS683)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1667 = _M0L5bytesS683[_M0L6_2atmpS1668];
            _M0L6_2atmpS1666 = (int32_t)_M0L6_2atmpS1667;
            _M0L6_2atmpS1665 = _M0L6_2atmpS1666 & 63;
            _M0L6_2atmpS1663 = _M0L6_2atmpS1664 | _M0L6_2atmpS1665;
            _M0L1cS687->$0 = _M0L6_2atmpS1663;
            _M0L8_2afieldS1833 = _M0L1cS687->$0;
            moonbit_decref(_M0L1cS687);
            _M0L3valS1680 = _M0L8_2afieldS1833;
            _M0L6_2atmpS1679 = _M0L3valS1680;
            moonbit_incref(_M0L3resS684);
            #line 231 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS684, _M0L6_2atmpS1679);
            _M0L3valS1682 = _M0L1iS686->$0;
            _M0L6_2atmpS1681 = _M0L3valS1682 + 3;
            _M0L1iS686->$0 = _M0L6_2atmpS1681;
          } else {
            int32_t _M0L3valS1684 = _M0L1iS686->$0;
            int32_t _M0L6_2atmpS1683 = _M0L3valS1684 + 3;
            int32_t _M0L3valS1707;
            int32_t _M0L6_2atmpS1706;
            int32_t _M0L6_2atmpS1699;
            int32_t _M0L3valS1705;
            int32_t _M0L6_2atmpS1704;
            int32_t _M0L6_2atmpS1703;
            int32_t _M0L6_2atmpS1702;
            int32_t _M0L6_2atmpS1701;
            int32_t _M0L6_2atmpS1700;
            int32_t _M0L6_2atmpS1692;
            int32_t _M0L3valS1698;
            int32_t _M0L6_2atmpS1697;
            int32_t _M0L6_2atmpS1696;
            int32_t _M0L6_2atmpS1695;
            int32_t _M0L6_2atmpS1694;
            int32_t _M0L6_2atmpS1693;
            int32_t _M0L6_2atmpS1686;
            int32_t _M0L3valS1691;
            int32_t _M0L6_2atmpS1690;
            int32_t _M0L6_2atmpS1689;
            int32_t _M0L6_2atmpS1688;
            int32_t _M0L6_2atmpS1687;
            int32_t _M0L6_2atmpS1685;
            int32_t _M0L3valS1709;
            int32_t _M0L6_2atmpS1708;
            int32_t _M0L3valS1713;
            int32_t _M0L6_2atmpS1712;
            int32_t _M0L6_2atmpS1711;
            int32_t _M0L6_2atmpS1710;
            int32_t _M0L8_2afieldS1834;
            int32_t _M0L3valS1717;
            int32_t _M0L6_2atmpS1716;
            int32_t _M0L6_2atmpS1715;
            int32_t _M0L6_2atmpS1714;
            int32_t _M0L3valS1719;
            int32_t _M0L6_2atmpS1718;
            if (_M0L6_2atmpS1683 >= _M0L3lenS685) {
              moonbit_decref(_M0L1cS687);
              moonbit_decref(_M0L1iS686);
              moonbit_decref(_M0L5bytesS683);
              break;
            }
            _M0L3valS1707 = _M0L1cS687->$0;
            _M0L6_2atmpS1706 = _M0L3valS1707 & 7;
            _M0L6_2atmpS1699 = _M0L6_2atmpS1706 << 18;
            _M0L3valS1705 = _M0L1iS686->$0;
            _M0L6_2atmpS1704 = _M0L3valS1705 + 1;
            if (
              _M0L6_2atmpS1704 < 0
              || _M0L6_2atmpS1704 >= Moonbit_array_length(_M0L5bytesS683)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1703 = _M0L5bytesS683[_M0L6_2atmpS1704];
            _M0L6_2atmpS1702 = (int32_t)_M0L6_2atmpS1703;
            _M0L6_2atmpS1701 = _M0L6_2atmpS1702 & 63;
            _M0L6_2atmpS1700 = _M0L6_2atmpS1701 << 12;
            _M0L6_2atmpS1692 = _M0L6_2atmpS1699 | _M0L6_2atmpS1700;
            _M0L3valS1698 = _M0L1iS686->$0;
            _M0L6_2atmpS1697 = _M0L3valS1698 + 2;
            if (
              _M0L6_2atmpS1697 < 0
              || _M0L6_2atmpS1697 >= Moonbit_array_length(_M0L5bytesS683)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1696 = _M0L5bytesS683[_M0L6_2atmpS1697];
            _M0L6_2atmpS1695 = (int32_t)_M0L6_2atmpS1696;
            _M0L6_2atmpS1694 = _M0L6_2atmpS1695 & 63;
            _M0L6_2atmpS1693 = _M0L6_2atmpS1694 << 6;
            _M0L6_2atmpS1686 = _M0L6_2atmpS1692 | _M0L6_2atmpS1693;
            _M0L3valS1691 = _M0L1iS686->$0;
            _M0L6_2atmpS1690 = _M0L3valS1691 + 3;
            if (
              _M0L6_2atmpS1690 < 0
              || _M0L6_2atmpS1690 >= Moonbit_array_length(_M0L5bytesS683)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1689 = _M0L5bytesS683[_M0L6_2atmpS1690];
            _M0L6_2atmpS1688 = (int32_t)_M0L6_2atmpS1689;
            _M0L6_2atmpS1687 = _M0L6_2atmpS1688 & 63;
            _M0L6_2atmpS1685 = _M0L6_2atmpS1686 | _M0L6_2atmpS1687;
            _M0L1cS687->$0 = _M0L6_2atmpS1685;
            _M0L3valS1709 = _M0L1cS687->$0;
            _M0L6_2atmpS1708 = _M0L3valS1709 - 65536;
            _M0L1cS687->$0 = _M0L6_2atmpS1708;
            _M0L3valS1713 = _M0L1cS687->$0;
            _M0L6_2atmpS1712 = _M0L3valS1713 >> 10;
            _M0L6_2atmpS1711 = _M0L6_2atmpS1712 + 55296;
            _M0L6_2atmpS1710 = _M0L6_2atmpS1711;
            moonbit_incref(_M0L3resS684);
            #line 242 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS684, _M0L6_2atmpS1710);
            _M0L8_2afieldS1834 = _M0L1cS687->$0;
            moonbit_decref(_M0L1cS687);
            _M0L3valS1717 = _M0L8_2afieldS1834;
            _M0L6_2atmpS1716 = _M0L3valS1717 & 1023;
            _M0L6_2atmpS1715 = _M0L6_2atmpS1716 + 56320;
            _M0L6_2atmpS1714 = _M0L6_2atmpS1715;
            moonbit_incref(_M0L3resS684);
            #line 243 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS684, _M0L6_2atmpS1714);
            _M0L3valS1719 = _M0L1iS686->$0;
            _M0L6_2atmpS1718 = _M0L3valS1719 + 4;
            _M0L1iS686->$0 = _M0L6_2atmpS1718;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS686);
      moonbit_decref(_M0L5bytesS683);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS684);
}

int32_t _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S676(
  int32_t _M0L6_2aenvS1630,
  moonbit_string_t _M0L1sS677
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS678;
  int32_t _M0L3lenS679;
  int32_t _M0L1iS680;
  int32_t _M0L8_2afieldS1835;
  #line 197 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L3resS678
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS678)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS678->$0 = 0;
  _M0L3lenS679 = Moonbit_array_length(_M0L1sS677);
  _M0L1iS680 = 0;
  while (1) {
    if (_M0L1iS680 < _M0L3lenS679) {
      int32_t _M0L3valS1635 = _M0L3resS678->$0;
      int32_t _M0L6_2atmpS1632 = _M0L3valS1635 * 10;
      int32_t _M0L6_2atmpS1634;
      int32_t _M0L6_2atmpS1633;
      int32_t _M0L6_2atmpS1631;
      int32_t _M0L6_2atmpS1636;
      if (_M0L1iS680 < 0 || _M0L1iS680 >= Moonbit_array_length(_M0L1sS677)) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1634 = _M0L1sS677[_M0L1iS680];
      _M0L6_2atmpS1633 = _M0L6_2atmpS1634 - 48;
      _M0L6_2atmpS1631 = _M0L6_2atmpS1632 + _M0L6_2atmpS1633;
      _M0L3resS678->$0 = _M0L6_2atmpS1631;
      _M0L6_2atmpS1636 = _M0L1iS680 + 1;
      _M0L1iS680 = _M0L6_2atmpS1636;
      continue;
    } else {
      moonbit_decref(_M0L1sS677);
    }
    break;
  }
  _M0L8_2afieldS1835 = _M0L3resS678->$0;
  moonbit_decref(_M0L3resS678);
  return _M0L8_2afieldS1835;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S656,
  moonbit_string_t _M0L12_2adiscard__S657,
  int32_t _M0L12_2adiscard__S658,
  struct _M0TWssbEu* _M0L12_2adiscard__S659,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S660
) {
  struct moonbit_result_0 _result_2108;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S660);
  moonbit_decref(_M0L12_2adiscard__S659);
  moonbit_decref(_M0L12_2adiscard__S657);
  moonbit_decref(_M0L12_2adiscard__S656);
  _result_2108.tag = 1;
  _result_2108.data.ok = 0;
  return _result_2108;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S661,
  moonbit_string_t _M0L12_2adiscard__S662,
  int32_t _M0L12_2adiscard__S663,
  struct _M0TWssbEu* _M0L12_2adiscard__S664,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S665
) {
  struct moonbit_result_0 _result_2109;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S665);
  moonbit_decref(_M0L12_2adiscard__S664);
  moonbit_decref(_M0L12_2adiscard__S662);
  moonbit_decref(_M0L12_2adiscard__S661);
  _result_2109.tag = 1;
  _result_2109.data.ok = 0;
  return _result_2109;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S666,
  moonbit_string_t _M0L12_2adiscard__S667,
  int32_t _M0L12_2adiscard__S668,
  struct _M0TWssbEu* _M0L12_2adiscard__S669,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S670
) {
  struct moonbit_result_0 _result_2110;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S670);
  moonbit_decref(_M0L12_2adiscard__S669);
  moonbit_decref(_M0L12_2adiscard__S667);
  moonbit_decref(_M0L12_2adiscard__S666);
  _result_2110.tag = 1;
  _result_2110.data.ok = 0;
  return _result_2110;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4uuid21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4uuid50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S671,
  moonbit_string_t _M0L12_2adiscard__S672,
  int32_t _M0L12_2adiscard__S673,
  struct _M0TWssbEu* _M0L12_2adiscard__S674,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S675
) {
  struct moonbit_result_0 _result_2111;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S675);
  moonbit_decref(_M0L12_2adiscard__S674);
  moonbit_decref(_M0L12_2adiscard__S672);
  moonbit_decref(_M0L12_2adiscard__S671);
  _result_2111.tag = 1;
  _result_2111.data.ok = 0;
  return _result_2111;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal4uuid28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal4uuid34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S655
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S655);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4uuid29____test__757569642e6d6274__0(
  
) {
  int32_t _M0L6_2atmpS1628;
  struct _M0Y4Char* _M0L14_2aboxed__selfS1629;
  struct _M0TPB4Show _M0L6_2atmpS1621;
  moonbit_string_t _M0L6_2atmpS1624;
  moonbit_string_t _M0L6_2atmpS1625;
  moonbit_string_t _M0L6_2atmpS1626;
  moonbit_string_t _M0L6_2atmpS1627;
  moonbit_string_t* _M0L6_2atmpS1623;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1622;
  #line 211 "E:\\moonbit\\clawteam\\internal\\uuid\\uuid.mbt"
  #line 212 "E:\\moonbit\\clawteam\\internal\\uuid\\uuid.mbt"
  _M0L6_2atmpS1628 = _M0MPC14byte4Byte8to__char(97);
  _M0L14_2aboxed__selfS1629
  = (struct _M0Y4Char*)moonbit_malloc(sizeof(struct _M0Y4Char));
  Moonbit_object_header(_M0L14_2aboxed__selfS1629)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Char) >> 2, 0, 0);
  _M0L14_2aboxed__selfS1629->$0 = _M0L6_2atmpS1628;
  _M0L6_2atmpS1621
  = (struct _M0TPB4Show){
    _M0FP077Char_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS1629
  };
  _M0L6_2atmpS1624 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1625 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1626 = 0;
  _M0L6_2atmpS1627 = 0;
  _M0L6_2atmpS1623 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1623[0] = _M0L6_2atmpS1624;
  _M0L6_2atmpS1623[1] = _M0L6_2atmpS1625;
  _M0L6_2atmpS1623[2] = _M0L6_2atmpS1626;
  _M0L6_2atmpS1623[3] = _M0L6_2atmpS1627;
  _M0L6_2atmpS1622
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1622)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1622->$0 = _M0L6_2atmpS1623;
  _M0L6_2atmpS1622->$1 = 4;
  #line 212 "E:\\moonbit\\clawteam\\internal\\uuid\\uuid.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1621, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS1622);
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS653,
  struct _M0TPB6Logger _M0L6loggerS654
) {
  moonbit_string_t _M0L6_2atmpS1620;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1619;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1620 = _M0L4selfS653;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1619 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1620);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1619, _M0L6loggerS654);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS630,
  struct _M0TPB6Logger _M0L6loggerS652
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS1844;
  struct _M0TPC16string10StringView _M0L3pkgS629;
  moonbit_string_t _M0L7_2adataS631;
  int32_t _M0L8_2astartS632;
  int32_t _M0L6_2atmpS1618;
  int32_t _M0L6_2aendS633;
  int32_t _M0Lm9_2acursorS634;
  int32_t _M0Lm13accept__stateS635;
  int32_t _M0Lm10match__endS636;
  int32_t _M0Lm20match__tag__saver__0S637;
  int32_t _M0Lm6tag__0S638;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS639;
  struct _M0TPC16string10StringView _M0L8_2afieldS1843;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS648;
  void* _M0L8_2afieldS1842;
  int32_t _M0L6_2acntS2028;
  void* _M0L16_2apackage__nameS649;
  struct _M0TPC16string10StringView _M0L8_2afieldS1840;
  struct _M0TPC16string10StringView _M0L8filenameS1595;
  struct _M0TPC16string10StringView _M0L8_2afieldS1839;
  struct _M0TPC16string10StringView _M0L11start__lineS1596;
  struct _M0TPC16string10StringView _M0L8_2afieldS1838;
  struct _M0TPC16string10StringView _M0L13start__columnS1597;
  struct _M0TPC16string10StringView _M0L8_2afieldS1837;
  struct _M0TPC16string10StringView _M0L9end__lineS1598;
  struct _M0TPC16string10StringView _M0L8_2afieldS1836;
  int32_t _M0L6_2acntS2032;
  struct _M0TPC16string10StringView _M0L11end__columnS1599;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS1844
  = (struct _M0TPC16string10StringView){
    _M0L4selfS630->$0_1, _M0L4selfS630->$0_2, _M0L4selfS630->$0_0
  };
  _M0L3pkgS629 = _M0L8_2afieldS1844;
  moonbit_incref(_M0L3pkgS629.$0);
  moonbit_incref(_M0L3pkgS629.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS631 = _M0MPC16string10StringView4data(_M0L3pkgS629);
  moonbit_incref(_M0L3pkgS629.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS632 = _M0MPC16string10StringView13start__offset(_M0L3pkgS629);
  moonbit_incref(_M0L3pkgS629.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1618 = _M0MPC16string10StringView6length(_M0L3pkgS629);
  _M0L6_2aendS633 = _M0L8_2astartS632 + _M0L6_2atmpS1618;
  _M0Lm9_2acursorS634 = _M0L8_2astartS632;
  _M0Lm13accept__stateS635 = -1;
  _M0Lm10match__endS636 = -1;
  _M0Lm20match__tag__saver__0S637 = -1;
  _M0Lm6tag__0S638 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1610 = _M0Lm9_2acursorS634;
    if (_M0L6_2atmpS1610 < _M0L6_2aendS633) {
      int32_t _M0L6_2atmpS1617 = _M0Lm9_2acursorS634;
      int32_t _M0L10next__charS643;
      int32_t _M0L6_2atmpS1611;
      moonbit_incref(_M0L7_2adataS631);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS643
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS631, _M0L6_2atmpS1617);
      _M0L6_2atmpS1611 = _M0Lm9_2acursorS634;
      _M0Lm9_2acursorS634 = _M0L6_2atmpS1611 + 1;
      if (_M0L10next__charS643 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1612;
          _M0Lm6tag__0S638 = _M0Lm9_2acursorS634;
          _M0L6_2atmpS1612 = _M0Lm9_2acursorS634;
          if (_M0L6_2atmpS1612 < _M0L6_2aendS633) {
            int32_t _M0L6_2atmpS1616 = _M0Lm9_2acursorS634;
            int32_t _M0L10next__charS644;
            int32_t _M0L6_2atmpS1613;
            moonbit_incref(_M0L7_2adataS631);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS644
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS631, _M0L6_2atmpS1616);
            _M0L6_2atmpS1613 = _M0Lm9_2acursorS634;
            _M0Lm9_2acursorS634 = _M0L6_2atmpS1613 + 1;
            if (_M0L10next__charS644 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1614 = _M0Lm9_2acursorS634;
                if (_M0L6_2atmpS1614 < _M0L6_2aendS633) {
                  int32_t _M0L6_2atmpS1615 = _M0Lm9_2acursorS634;
                  _M0Lm9_2acursorS634 = _M0L6_2atmpS1615 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S637 = _M0Lm6tag__0S638;
                  _M0Lm13accept__stateS635 = 0;
                  _M0Lm10match__endS636 = _M0Lm9_2acursorS634;
                  goto join_640;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_640;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_640;
    }
    break;
  }
  goto joinlet_2112;
  join_640:;
  switch (_M0Lm13accept__stateS635) {
    case 0: {
      int32_t _M0L6_2atmpS1608;
      int32_t _M0L6_2atmpS1607;
      int64_t _M0L6_2atmpS1604;
      int32_t _M0L6_2atmpS1606;
      int64_t _M0L6_2atmpS1605;
      struct _M0TPC16string10StringView _M0L13package__nameS641;
      int64_t _M0L6_2atmpS1601;
      int32_t _M0L6_2atmpS1603;
      int64_t _M0L6_2atmpS1602;
      struct _M0TPC16string10StringView _M0L12module__nameS642;
      void* _M0L4SomeS1600;
      moonbit_decref(_M0L3pkgS629.$0);
      _M0L6_2atmpS1608 = _M0Lm20match__tag__saver__0S637;
      _M0L6_2atmpS1607 = _M0L6_2atmpS1608 + 1;
      _M0L6_2atmpS1604 = (int64_t)_M0L6_2atmpS1607;
      _M0L6_2atmpS1606 = _M0Lm10match__endS636;
      _M0L6_2atmpS1605 = (int64_t)_M0L6_2atmpS1606;
      moonbit_incref(_M0L7_2adataS631);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS641
      = _M0MPC16string6String4view(_M0L7_2adataS631, _M0L6_2atmpS1604, _M0L6_2atmpS1605);
      _M0L6_2atmpS1601 = (int64_t)_M0L8_2astartS632;
      _M0L6_2atmpS1603 = _M0Lm20match__tag__saver__0S637;
      _M0L6_2atmpS1602 = (int64_t)_M0L6_2atmpS1603;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS642
      = _M0MPC16string6String4view(_M0L7_2adataS631, _M0L6_2atmpS1601, _M0L6_2atmpS1602);
      _M0L4SomeS1600
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1600)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1600)->$0_0
      = _M0L13package__nameS641.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1600)->$0_1
      = _M0L13package__nameS641.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1600)->$0_2
      = _M0L13package__nameS641.$2;
      _M0L7_2abindS639
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS639)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS639->$0_0 = _M0L12module__nameS642.$0;
      _M0L7_2abindS639->$0_1 = _M0L12module__nameS642.$1;
      _M0L7_2abindS639->$0_2 = _M0L12module__nameS642.$2;
      _M0L7_2abindS639->$1 = _M0L4SomeS1600;
      break;
    }
    default: {
      void* _M0L4NoneS1609;
      moonbit_decref(_M0L7_2adataS631);
      _M0L4NoneS1609
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS639
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS639)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS639->$0_0 = _M0L3pkgS629.$0;
      _M0L7_2abindS639->$0_1 = _M0L3pkgS629.$1;
      _M0L7_2abindS639->$0_2 = _M0L3pkgS629.$2;
      _M0L7_2abindS639->$1 = _M0L4NoneS1609;
      break;
    }
  }
  joinlet_2112:;
  _M0L8_2afieldS1843
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS639->$0_1, _M0L7_2abindS639->$0_2, _M0L7_2abindS639->$0_0
  };
  _M0L15_2amodule__nameS648 = _M0L8_2afieldS1843;
  _M0L8_2afieldS1842 = _M0L7_2abindS639->$1;
  _M0L6_2acntS2028 = Moonbit_object_header(_M0L7_2abindS639)->rc;
  if (_M0L6_2acntS2028 > 1) {
    int32_t _M0L11_2anew__cntS2029 = _M0L6_2acntS2028 - 1;
    Moonbit_object_header(_M0L7_2abindS639)->rc = _M0L11_2anew__cntS2029;
    moonbit_incref(_M0L8_2afieldS1842);
    moonbit_incref(_M0L15_2amodule__nameS648.$0);
  } else if (_M0L6_2acntS2028 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS639);
  }
  _M0L16_2apackage__nameS649 = _M0L8_2afieldS1842;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS649)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS650 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS649;
      struct _M0TPC16string10StringView _M0L8_2afieldS1841 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS650->$0_1,
                                              _M0L7_2aSomeS650->$0_2,
                                              _M0L7_2aSomeS650->$0_0};
      int32_t _M0L6_2acntS2030 = Moonbit_object_header(_M0L7_2aSomeS650)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS651;
      if (_M0L6_2acntS2030 > 1) {
        int32_t _M0L11_2anew__cntS2031 = _M0L6_2acntS2030 - 1;
        Moonbit_object_header(_M0L7_2aSomeS650)->rc = _M0L11_2anew__cntS2031;
        moonbit_incref(_M0L8_2afieldS1841.$0);
      } else if (_M0L6_2acntS2030 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS650);
      }
      _M0L12_2apkg__nameS651 = _M0L8_2afieldS1841;
      if (_M0L6loggerS652.$1) {
        moonbit_incref(_M0L6loggerS652.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L12_2apkg__nameS651);
      if (_M0L6loggerS652.$1) {
        moonbit_incref(_M0L6loggerS652.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS652.$0->$method_3(_M0L6loggerS652.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS649);
      break;
    }
  }
  _M0L8_2afieldS1840
  = (struct _M0TPC16string10StringView){
    _M0L4selfS630->$1_1, _M0L4selfS630->$1_2, _M0L4selfS630->$1_0
  };
  _M0L8filenameS1595 = _M0L8_2afieldS1840;
  moonbit_incref(_M0L8filenameS1595.$0);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L8filenameS1595);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_3(_M0L6loggerS652.$1, 58);
  _M0L8_2afieldS1839
  = (struct _M0TPC16string10StringView){
    _M0L4selfS630->$2_1, _M0L4selfS630->$2_2, _M0L4selfS630->$2_0
  };
  _M0L11start__lineS1596 = _M0L8_2afieldS1839;
  moonbit_incref(_M0L11start__lineS1596.$0);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L11start__lineS1596);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_3(_M0L6loggerS652.$1, 58);
  _M0L8_2afieldS1838
  = (struct _M0TPC16string10StringView){
    _M0L4selfS630->$3_1, _M0L4selfS630->$3_2, _M0L4selfS630->$3_0
  };
  _M0L13start__columnS1597 = _M0L8_2afieldS1838;
  moonbit_incref(_M0L13start__columnS1597.$0);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L13start__columnS1597);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_3(_M0L6loggerS652.$1, 45);
  _M0L8_2afieldS1837
  = (struct _M0TPC16string10StringView){
    _M0L4selfS630->$4_1, _M0L4selfS630->$4_2, _M0L4selfS630->$4_0
  };
  _M0L9end__lineS1598 = _M0L8_2afieldS1837;
  moonbit_incref(_M0L9end__lineS1598.$0);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L9end__lineS1598);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_3(_M0L6loggerS652.$1, 58);
  _M0L8_2afieldS1836
  = (struct _M0TPC16string10StringView){
    _M0L4selfS630->$5_1, _M0L4selfS630->$5_2, _M0L4selfS630->$5_0
  };
  _M0L6_2acntS2032 = Moonbit_object_header(_M0L4selfS630)->rc;
  if (_M0L6_2acntS2032 > 1) {
    int32_t _M0L11_2anew__cntS2038 = _M0L6_2acntS2032 - 1;
    Moonbit_object_header(_M0L4selfS630)->rc = _M0L11_2anew__cntS2038;
    moonbit_incref(_M0L8_2afieldS1836.$0);
  } else if (_M0L6_2acntS2032 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2037 =
      (struct _M0TPC16string10StringView){_M0L4selfS630->$4_1,
                                            _M0L4selfS630->$4_2,
                                            _M0L4selfS630->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2036;
    struct _M0TPC16string10StringView _M0L8_2afieldS2035;
    struct _M0TPC16string10StringView _M0L8_2afieldS2034;
    struct _M0TPC16string10StringView _M0L8_2afieldS2033;
    moonbit_decref(_M0L8_2afieldS2037.$0);
    _M0L8_2afieldS2036
    = (struct _M0TPC16string10StringView){
      _M0L4selfS630->$3_1, _M0L4selfS630->$3_2, _M0L4selfS630->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2036.$0);
    _M0L8_2afieldS2035
    = (struct _M0TPC16string10StringView){
      _M0L4selfS630->$2_1, _M0L4selfS630->$2_2, _M0L4selfS630->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2035.$0);
    _M0L8_2afieldS2034
    = (struct _M0TPC16string10StringView){
      _M0L4selfS630->$1_1, _M0L4selfS630->$1_2, _M0L4selfS630->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2034.$0);
    _M0L8_2afieldS2033
    = (struct _M0TPC16string10StringView){
      _M0L4selfS630->$0_1, _M0L4selfS630->$0_2, _M0L4selfS630->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2033.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS630);
  }
  _M0L11end__columnS1599 = _M0L8_2afieldS1836;
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L11end__columnS1599);
  if (_M0L6loggerS652.$1) {
    moonbit_incref(_M0L6loggerS652.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_3(_M0L6loggerS652.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS652.$0->$method_2(_M0L6loggerS652.$1, _M0L15_2amodule__nameS648);
  return 0;
}

int32_t _M0IPC14char4CharPB4Show6output(
  int32_t _M0L4selfS628,
  struct _M0TPB6Logger _M0L6loggerS626
) {
  #line 486 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  if (_M0L6loggerS626.$1) {
    moonbit_incref(_M0L6loggerS626.$1);
  }
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, 39);
  if (_M0L4selfS628 == 39) {
    goto join_627;
  } else if (_M0L4selfS628 == 92) {
    goto join_627;
  } else if (_M0L4selfS628 == 10) {
    if (_M0L6loggerS626.$1) {
      moonbit_incref(_M0L6loggerS626.$1);
    }
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6loggerS626.$0->$method_0(_M0L6loggerS626.$1, (moonbit_string_t)moonbit_string_literal_13.data);
  } else if (_M0L4selfS628 == 13) {
    if (_M0L6loggerS626.$1) {
      moonbit_incref(_M0L6loggerS626.$1);
    }
    #line 497 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6loggerS626.$0->$method_0(_M0L6loggerS626.$1, (moonbit_string_t)moonbit_string_literal_14.data);
  } else if (_M0L4selfS628 == 8) {
    if (_M0L6loggerS626.$1) {
      moonbit_incref(_M0L6loggerS626.$1);
    }
    #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6loggerS626.$0->$method_0(_M0L6loggerS626.$1, (moonbit_string_t)moonbit_string_literal_15.data);
  } else if (_M0L4selfS628 == 9) {
    if (_M0L6loggerS626.$1) {
      moonbit_incref(_M0L6loggerS626.$1);
    }
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6loggerS626.$0->$method_0(_M0L6loggerS626.$1, (moonbit_string_t)moonbit_string_literal_16.data);
  } else if (_M0L4selfS628 >= 32 && _M0L4selfS628 <= 126) {
    if (_M0L6loggerS626.$1) {
      moonbit_incref(_M0L6loggerS626.$1);
    }
    #line 502 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, _M0L4selfS628);
  } else {
    int32_t _M0L6_2atmpS1593;
    #line 506 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1593 = _M0MPC14char4Char13is__printable(_M0L4selfS628);
    if (!_M0L6_2atmpS1593) {
      moonbit_string_t _M0L6_2atmpS1594;
      if (_M0L6loggerS626.$1) {
        moonbit_incref(_M0L6loggerS626.$1);
      }
      #line 508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
      _M0L6loggerS626.$0->$method_0(_M0L6loggerS626.$1, (moonbit_string_t)moonbit_string_literal_17.data);
      #line 509 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
      _M0L6_2atmpS1594 = _M0MPC14char4Char7to__hex(_M0L4selfS628);
      if (_M0L6loggerS626.$1) {
        moonbit_incref(_M0L6loggerS626.$1);
      }
      #line 509 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
      _M0L6loggerS626.$0->$method_0(_M0L6loggerS626.$1, _M0L6_2atmpS1594);
      if (_M0L6loggerS626.$1) {
        moonbit_incref(_M0L6loggerS626.$1);
      }
      #line 510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
      _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, 125);
    } else {
      if (_M0L6loggerS626.$1) {
        moonbit_incref(_M0L6loggerS626.$1);
      }
      #line 514 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
      _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, _M0L4selfS628);
    }
  }
  goto joinlet_2116;
  join_627:;
  if (_M0L6loggerS626.$1) {
    moonbit_incref(_M0L6loggerS626.$1);
  }
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, 92);
  if (_M0L6loggerS626.$1) {
    moonbit_incref(_M0L6loggerS626.$1);
  }
  #line 494 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, _M0L4selfS628);
  joinlet_2116:;
  #line 519 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L6loggerS626.$0->$method_3(_M0L6loggerS626.$1, 39);
  return 0;
}

int32_t _M0MPC14char4Char13is__printable(int32_t _M0L4selfS621) {
  int32_t _M0L4selfS622;
  int32_t _if__result_2119;
  #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 341 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  if (_M0MPC14char4Char11is__control(_M0L4selfS621)) {
    return 0;
  }
  _M0L4selfS622 = _M0L4selfS621;
  if (_M0L4selfS622 >= 57344 && _M0L4selfS622 <= 63743) {
    goto join_623;
  } else if (_M0L4selfS622 >= 983040 && _M0L4selfS622 <= 1048573) {
    goto join_623;
  } else if (_M0L4selfS622 >= 1048576 && _M0L4selfS622 <= 1114109) {
    goto join_623;
  }
  goto joinlet_2117;
  join_623:;
  return 0;
  joinlet_2117:;
  if (_M0L4selfS622 == 173) {
    goto join_624;
  } else if (_M0L4selfS622 >= 1536 && _M0L4selfS622 <= 1541) {
    goto join_624;
  } else if (_M0L4selfS622 == 1564) {
    goto join_624;
  } else if (_M0L4selfS622 == 1757) {
    goto join_624;
  } else if (_M0L4selfS622 == 1807) {
    goto join_624;
  } else if (_M0L4selfS622 >= 2192 && _M0L4selfS622 <= 2193) {
    goto join_624;
  } else if (_M0L4selfS622 == 2274) {
    goto join_624;
  } else if (_M0L4selfS622 == 6158) {
    goto join_624;
  } else if (_M0L4selfS622 >= 8203 && _M0L4selfS622 <= 8207) {
    goto join_624;
  } else if (_M0L4selfS622 >= 8234 && _M0L4selfS622 <= 8238) {
    goto join_624;
  } else if (_M0L4selfS622 >= 8288 && _M0L4selfS622 <= 8292) {
    goto join_624;
  } else if (_M0L4selfS622 >= 8294 && _M0L4selfS622 <= 8303) {
    goto join_624;
  } else if (_M0L4selfS622 == 65279) {
    goto join_624;
  } else if (_M0L4selfS622 >= 65529 && _M0L4selfS622 <= 65531) {
    goto join_624;
  } else if (_M0L4selfS622 == 69821) {
    goto join_624;
  } else if (_M0L4selfS622 == 69837) {
    goto join_624;
  } else if (_M0L4selfS622 >= 78896 && _M0L4selfS622 <= 78911) {
    goto join_624;
  } else if (_M0L4selfS622 >= 113824 && _M0L4selfS622 <= 113827) {
    goto join_624;
  } else if (_M0L4selfS622 >= 119155 && _M0L4selfS622 <= 119162) {
    goto join_624;
  } else if (_M0L4selfS622 == 917505) {
    goto join_624;
  } else if (_M0L4selfS622 >= 917536 && _M0L4selfS622 <= 917631) {
    goto join_624;
  }
  goto joinlet_2118;
  join_624:;
  return 0;
  joinlet_2118:;
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  if (_M0MPC13int3Int13is__surrogate(_M0L4selfS622)) {
    return 0;
  }
  if (_M0L4selfS622 == 8232) {
    _if__result_2119 = 1;
  } else {
    _if__result_2119 = _M0L4selfS622 == 8233;
  }
  if (_if__result_2119) {
    return 0;
  }
  if (_M0L4selfS622 >= 64976 && _M0L4selfS622 <= 65007) {
    goto join_625;
  } else if (_M0L4selfS622 >= 65534 && _M0L4selfS622 <= 65535) {
    goto join_625;
  } else if (_M0L4selfS622 >= 131070 && _M0L4selfS622 <= 131071) {
    goto join_625;
  } else if (_M0L4selfS622 >= 196606 && _M0L4selfS622 <= 196607) {
    goto join_625;
  } else if (_M0L4selfS622 >= 262142 && _M0L4selfS622 <= 262143) {
    goto join_625;
  } else if (_M0L4selfS622 >= 327678 && _M0L4selfS622 <= 327679) {
    goto join_625;
  } else if (_M0L4selfS622 >= 393214 && _M0L4selfS622 <= 393215) {
    goto join_625;
  } else if (_M0L4selfS622 >= 458750 && _M0L4selfS622 <= 458751) {
    goto join_625;
  } else if (_M0L4selfS622 >= 524286 && _M0L4selfS622 <= 524287) {
    goto join_625;
  } else if (_M0L4selfS622 >= 589822 && _M0L4selfS622 <= 589823) {
    goto join_625;
  } else if (_M0L4selfS622 >= 655358 && _M0L4selfS622 <= 655359) {
    goto join_625;
  } else if (_M0L4selfS622 >= 720894 && _M0L4selfS622 <= 720895) {
    goto join_625;
  } else if (_M0L4selfS622 >= 786430 && _M0L4selfS622 <= 786431) {
    goto join_625;
  } else if (_M0L4selfS622 >= 851966 && _M0L4selfS622 <= 851967) {
    goto join_625;
  } else if (_M0L4selfS622 >= 917502 && _M0L4selfS622 <= 917503) {
    goto join_625;
  } else if (_M0L4selfS622 >= 983038 && _M0L4selfS622 <= 983039) {
    goto join_625;
  } else if (_M0L4selfS622 >= 1048574 && _M0L4selfS622 <= 1048575) {
    goto join_625;
  } else if (_M0L4selfS622 >= 1114110 && _M0L4selfS622 <= 1114111) {
    goto join_625;
  }
  goto joinlet_2120;
  join_625:;
  return 0;
  joinlet_2120:;
  return 1;
}

int32_t _M0MPC14char4Char11is__control(int32_t _M0L4selfS620) {
  #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0L4selfS620 >= 0 && _M0L4selfS620 <= 31
         || _M0L4selfS620 >= 127 && _M0L4selfS620 <= 159
         || 0;
}

moonbit_string_t _M0MPC14char4Char7to__hex(int32_t _M0L4charS619) {
  int32_t _M0L4codeS618;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L4codeS618 = _M0L4charS619;
  if (_M0L4codeS618 >= 0 && _M0L4codeS618 <= 255) {
    int32_t _M0L6_2atmpS1592 = _M0L4codeS618 & 0xff;
    #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    return _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1592);
  } else if (_M0L4codeS618 <= 65535) {
    int32_t _M0L6_2atmpS1591 = _M0L4codeS618 >> 8;
    int32_t _M0L6_2atmpS1590 = _M0L6_2atmpS1591 & 0xff;
    moonbit_string_t _M0L6_2atmpS1587;
    int32_t _M0L6_2atmpS1589;
    moonbit_string_t _M0L6_2atmpS1588;
    moonbit_string_t _M0L6_2atmpS1845;
    #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1587 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1590);
    _M0L6_2atmpS1589 = _M0L4codeS618 & 0xff;
    #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1588 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1589);
    #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1845 = moonbit_add_string(_M0L6_2atmpS1587, _M0L6_2atmpS1588);
    moonbit_decref(_M0L6_2atmpS1587);
    moonbit_decref(_M0L6_2atmpS1588);
    return _M0L6_2atmpS1845;
  } else {
    int32_t _M0L6_2atmpS1586 = _M0L4codeS618 >> 16;
    int32_t _M0L6_2atmpS1585 = _M0L6_2atmpS1586 & 0xff;
    moonbit_string_t _M0L6_2atmpS1581;
    int32_t _M0L6_2atmpS1584;
    int32_t _M0L6_2atmpS1583;
    moonbit_string_t _M0L6_2atmpS1582;
    moonbit_string_t _M0L6_2atmpS1847;
    moonbit_string_t _M0L6_2atmpS1578;
    int32_t _M0L6_2atmpS1580;
    moonbit_string_t _M0L6_2atmpS1579;
    moonbit_string_t _M0L6_2atmpS1846;
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1581 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1585);
    _M0L6_2atmpS1584 = _M0L4codeS618 >> 8;
    _M0L6_2atmpS1583 = _M0L6_2atmpS1584 & 0xff;
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1582 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1583);
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1847 = moonbit_add_string(_M0L6_2atmpS1581, _M0L6_2atmpS1582);
    moonbit_decref(_M0L6_2atmpS1581);
    moonbit_decref(_M0L6_2atmpS1582);
    _M0L6_2atmpS1578 = _M0L6_2atmpS1847;
    _M0L6_2atmpS1580 = _M0L4codeS618 & 0xff;
    #line 24 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1579 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1580);
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
    _M0L6_2atmpS1846 = moonbit_add_string(_M0L6_2atmpS1578, _M0L6_2atmpS1579);
    moonbit_decref(_M0L6_2atmpS1578);
    moonbit_decref(_M0L6_2atmpS1579);
    return _M0L6_2atmpS1846;
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS617) {
  moonbit_string_t _M0L6_2atmpS1577;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS1577 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS617);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS1577);
  moonbit_decref(_M0L6_2atmpS1577);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS616,
  struct _M0TPB6Hasher* _M0L6hasherS615
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS615, _M0L4selfS616);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS614,
  struct _M0TPB6Hasher* _M0L6hasherS613
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS613, _M0L4selfS614);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS611,
  moonbit_string_t _M0L5valueS609
) {
  int32_t _M0L7_2abindS608;
  int32_t _M0L1iS610;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS608 = Moonbit_array_length(_M0L5valueS609);
  _M0L1iS610 = 0;
  while (1) {
    if (_M0L1iS610 < _M0L7_2abindS608) {
      int32_t _M0L6_2atmpS1575 = _M0L5valueS609[_M0L1iS610];
      int32_t _M0L6_2atmpS1574 = (int32_t)_M0L6_2atmpS1575;
      uint32_t _M0L6_2atmpS1573 = *(uint32_t*)&_M0L6_2atmpS1574;
      int32_t _M0L6_2atmpS1576;
      moonbit_incref(_M0L4selfS611);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS611, _M0L6_2atmpS1573);
      _M0L6_2atmpS1576 = _M0L1iS610 + 1;
      _M0L1iS610 = _M0L6_2atmpS1576;
      continue;
    } else {
      moonbit_decref(_M0L4selfS611);
      moonbit_decref(_M0L5valueS609);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC13int3Int13is__surrogate(int32_t _M0L4selfS607) {
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (55296 <= _M0L4selfS607) {
    return _M0L4selfS607 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS605,
  int32_t _M0L3idxS606
) {
  int32_t _M0L6_2atmpS1848;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1848 = _M0L4selfS605[_M0L3idxS606];
  moonbit_decref(_M0L4selfS605);
  return _M0L6_2atmpS1848;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS592,
  int32_t _M0L3keyS588
) {
  int32_t _M0L4hashS587;
  int32_t _M0L14capacity__maskS1558;
  int32_t _M0L6_2atmpS1557;
  int32_t _M0L1iS589;
  int32_t _M0L3idxS590;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS587 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS588);
  _M0L14capacity__maskS1558 = _M0L4selfS592->$3;
  _M0L6_2atmpS1557 = _M0L4hashS587 & _M0L14capacity__maskS1558;
  _M0L1iS589 = 0;
  _M0L3idxS590 = _M0L6_2atmpS1557;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1852 =
      _M0L4selfS592->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1556 =
      _M0L8_2afieldS1852;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1851;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS591;
    if (
      _M0L3idxS590 < 0
      || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS1556)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1851
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1556[
        _M0L3idxS590
      ];
    _M0L7_2abindS591 = _M0L6_2atmpS1851;
    if (_M0L7_2abindS591 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1545;
      if (_M0L7_2abindS591) {
        moonbit_incref(_M0L7_2abindS591);
      }
      moonbit_decref(_M0L4selfS592);
      if (_M0L7_2abindS591) {
        moonbit_decref(_M0L7_2abindS591);
      }
      _M0L6_2atmpS1545 = 0;
      return _M0L6_2atmpS1545;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS593 =
        _M0L7_2abindS591;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS594 =
        _M0L7_2aSomeS593;
      int32_t _M0L4hashS1547 = _M0L8_2aentryS594->$3;
      int32_t _if__result_2123;
      int32_t _M0L8_2afieldS1849;
      int32_t _M0L3pslS1550;
      int32_t _M0L6_2atmpS1552;
      int32_t _M0L6_2atmpS1554;
      int32_t _M0L14capacity__maskS1555;
      int32_t _M0L6_2atmpS1553;
      if (_M0L4hashS1547 == _M0L4hashS587) {
        int32_t _M0L3keyS1546 = _M0L8_2aentryS594->$4;
        _if__result_2123 = _M0L3keyS1546 == _M0L3keyS588;
      } else {
        _if__result_2123 = 0;
      }
      if (_if__result_2123) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1850;
        int32_t _M0L6_2acntS2039;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1549;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1548;
        moonbit_incref(_M0L8_2aentryS594);
        moonbit_decref(_M0L4selfS592);
        _M0L8_2afieldS1850 = _M0L8_2aentryS594->$5;
        _M0L6_2acntS2039 = Moonbit_object_header(_M0L8_2aentryS594)->rc;
        if (_M0L6_2acntS2039 > 1) {
          int32_t _M0L11_2anew__cntS2041 = _M0L6_2acntS2039 - 1;
          Moonbit_object_header(_M0L8_2aentryS594)->rc
          = _M0L11_2anew__cntS2041;
          moonbit_incref(_M0L8_2afieldS1850);
        } else if (_M0L6_2acntS2039 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2040 =
            _M0L8_2aentryS594->$1;
          if (_M0L8_2afieldS2040) {
            moonbit_decref(_M0L8_2afieldS2040);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS594);
        }
        _M0L5valueS1549 = _M0L8_2afieldS1850;
        _M0L6_2atmpS1548 = _M0L5valueS1549;
        return _M0L6_2atmpS1548;
      } else {
        moonbit_incref(_M0L8_2aentryS594);
      }
      _M0L8_2afieldS1849 = _M0L8_2aentryS594->$2;
      moonbit_decref(_M0L8_2aentryS594);
      _M0L3pslS1550 = _M0L8_2afieldS1849;
      if (_M0L1iS589 > _M0L3pslS1550) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1551;
        moonbit_decref(_M0L4selfS592);
        _M0L6_2atmpS1551 = 0;
        return _M0L6_2atmpS1551;
      }
      _M0L6_2atmpS1552 = _M0L1iS589 + 1;
      _M0L6_2atmpS1554 = _M0L3idxS590 + 1;
      _M0L14capacity__maskS1555 = _M0L4selfS592->$3;
      _M0L6_2atmpS1553 = _M0L6_2atmpS1554 & _M0L14capacity__maskS1555;
      _M0L1iS589 = _M0L6_2atmpS1552;
      _M0L3idxS590 = _M0L6_2atmpS1553;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS601,
  moonbit_string_t _M0L3keyS597
) {
  int32_t _M0L4hashS596;
  int32_t _M0L14capacity__maskS1572;
  int32_t _M0L6_2atmpS1571;
  int32_t _M0L1iS598;
  int32_t _M0L3idxS599;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS597);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS596 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS597);
  _M0L14capacity__maskS1572 = _M0L4selfS601->$3;
  _M0L6_2atmpS1571 = _M0L4hashS596 & _M0L14capacity__maskS1572;
  _M0L1iS598 = 0;
  _M0L3idxS599 = _M0L6_2atmpS1571;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1858 =
      _M0L4selfS601->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1570 =
      _M0L8_2afieldS1858;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1857;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS600;
    if (
      _M0L3idxS599 < 0
      || _M0L3idxS599 >= Moonbit_array_length(_M0L7entriesS1570)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1857
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1570[
        _M0L3idxS599
      ];
    _M0L7_2abindS600 = _M0L6_2atmpS1857;
    if (_M0L7_2abindS600 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1559;
      if (_M0L7_2abindS600) {
        moonbit_incref(_M0L7_2abindS600);
      }
      moonbit_decref(_M0L4selfS601);
      if (_M0L7_2abindS600) {
        moonbit_decref(_M0L7_2abindS600);
      }
      moonbit_decref(_M0L3keyS597);
      _M0L6_2atmpS1559 = 0;
      return _M0L6_2atmpS1559;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS602 =
        _M0L7_2abindS600;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS603 =
        _M0L7_2aSomeS602;
      int32_t _M0L4hashS1561 = _M0L8_2aentryS603->$3;
      int32_t _if__result_2125;
      int32_t _M0L8_2afieldS1853;
      int32_t _M0L3pslS1564;
      int32_t _M0L6_2atmpS1566;
      int32_t _M0L6_2atmpS1568;
      int32_t _M0L14capacity__maskS1569;
      int32_t _M0L6_2atmpS1567;
      if (_M0L4hashS1561 == _M0L4hashS596) {
        moonbit_string_t _M0L8_2afieldS1856 = _M0L8_2aentryS603->$4;
        moonbit_string_t _M0L3keyS1560 = _M0L8_2afieldS1856;
        int32_t _M0L6_2atmpS1855;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS1855
        = moonbit_val_array_equal(_M0L3keyS1560, _M0L3keyS597);
        _if__result_2125 = _M0L6_2atmpS1855;
      } else {
        _if__result_2125 = 0;
      }
      if (_if__result_2125) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1854;
        int32_t _M0L6_2acntS2042;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1563;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1562;
        moonbit_incref(_M0L8_2aentryS603);
        moonbit_decref(_M0L4selfS601);
        moonbit_decref(_M0L3keyS597);
        _M0L8_2afieldS1854 = _M0L8_2aentryS603->$5;
        _M0L6_2acntS2042 = Moonbit_object_header(_M0L8_2aentryS603)->rc;
        if (_M0L6_2acntS2042 > 1) {
          int32_t _M0L11_2anew__cntS2045 = _M0L6_2acntS2042 - 1;
          Moonbit_object_header(_M0L8_2aentryS603)->rc
          = _M0L11_2anew__cntS2045;
          moonbit_incref(_M0L8_2afieldS1854);
        } else if (_M0L6_2acntS2042 == 1) {
          moonbit_string_t _M0L8_2afieldS2044 = _M0L8_2aentryS603->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2043;
          moonbit_decref(_M0L8_2afieldS2044);
          _M0L8_2afieldS2043 = _M0L8_2aentryS603->$1;
          if (_M0L8_2afieldS2043) {
            moonbit_decref(_M0L8_2afieldS2043);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS603);
        }
        _M0L5valueS1563 = _M0L8_2afieldS1854;
        _M0L6_2atmpS1562 = _M0L5valueS1563;
        return _M0L6_2atmpS1562;
      } else {
        moonbit_incref(_M0L8_2aentryS603);
      }
      _M0L8_2afieldS1853 = _M0L8_2aentryS603->$2;
      moonbit_decref(_M0L8_2aentryS603);
      _M0L3pslS1564 = _M0L8_2afieldS1853;
      if (_M0L1iS598 > _M0L3pslS1564) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1565;
        moonbit_decref(_M0L4selfS601);
        moonbit_decref(_M0L3keyS597);
        _M0L6_2atmpS1565 = 0;
        return _M0L6_2atmpS1565;
      }
      _M0L6_2atmpS1566 = _M0L1iS598 + 1;
      _M0L6_2atmpS1568 = _M0L3idxS599 + 1;
      _M0L14capacity__maskS1569 = _M0L4selfS601->$3;
      _M0L6_2atmpS1567 = _M0L6_2atmpS1568 & _M0L14capacity__maskS1569;
      _M0L1iS598 = _M0L6_2atmpS1566;
      _M0L3idxS599 = _M0L6_2atmpS1567;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS572
) {
  int32_t _M0L6lengthS571;
  int32_t _M0Lm8capacityS573;
  int32_t _M0L6_2atmpS1522;
  int32_t _M0L6_2atmpS1521;
  int32_t _M0L6_2atmpS1532;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS574;
  int32_t _M0L3endS1530;
  int32_t _M0L5startS1531;
  int32_t _M0L7_2abindS575;
  int32_t _M0L2__S576;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS572.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS571
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS572);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS573 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS571);
  _M0L6_2atmpS1522 = _M0Lm8capacityS573;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1521 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1522);
  if (_M0L6lengthS571 > _M0L6_2atmpS1521) {
    int32_t _M0L6_2atmpS1523 = _M0Lm8capacityS573;
    _M0Lm8capacityS573 = _M0L6_2atmpS1523 * 2;
  }
  _M0L6_2atmpS1532 = _M0Lm8capacityS573;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS574
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1532);
  _M0L3endS1530 = _M0L3arrS572.$2;
  _M0L5startS1531 = _M0L3arrS572.$1;
  _M0L7_2abindS575 = _M0L3endS1530 - _M0L5startS1531;
  _M0L2__S576 = 0;
  while (1) {
    if (_M0L2__S576 < _M0L7_2abindS575) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1862 =
        _M0L3arrS572.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1527 =
        _M0L8_2afieldS1862;
      int32_t _M0L5startS1529 = _M0L3arrS572.$1;
      int32_t _M0L6_2atmpS1528 = _M0L5startS1529 + _M0L2__S576;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1861 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1527[
          _M0L6_2atmpS1528
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS577 =
        _M0L6_2atmpS1861;
      moonbit_string_t _M0L8_2afieldS1860 = _M0L1eS577->$0;
      moonbit_string_t _M0L6_2atmpS1524 = _M0L8_2afieldS1860;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1859 =
        _M0L1eS577->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1525 =
        _M0L8_2afieldS1859;
      int32_t _M0L6_2atmpS1526;
      moonbit_incref(_M0L6_2atmpS1525);
      moonbit_incref(_M0L6_2atmpS1524);
      moonbit_incref(_M0L1mS574);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS574, _M0L6_2atmpS1524, _M0L6_2atmpS1525);
      _M0L6_2atmpS1526 = _M0L2__S576 + 1;
      _M0L2__S576 = _M0L6_2atmpS1526;
      continue;
    } else {
      moonbit_decref(_M0L3arrS572.$0);
    }
    break;
  }
  return _M0L1mS574;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS580
) {
  int32_t _M0L6lengthS579;
  int32_t _M0Lm8capacityS581;
  int32_t _M0L6_2atmpS1534;
  int32_t _M0L6_2atmpS1533;
  int32_t _M0L6_2atmpS1544;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS582;
  int32_t _M0L3endS1542;
  int32_t _M0L5startS1543;
  int32_t _M0L7_2abindS583;
  int32_t _M0L2__S584;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS580.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS579
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS580);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS581 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS579);
  _M0L6_2atmpS1534 = _M0Lm8capacityS581;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1533 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1534);
  if (_M0L6lengthS579 > _M0L6_2atmpS1533) {
    int32_t _M0L6_2atmpS1535 = _M0Lm8capacityS581;
    _M0Lm8capacityS581 = _M0L6_2atmpS1535 * 2;
  }
  _M0L6_2atmpS1544 = _M0Lm8capacityS581;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS582
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1544);
  _M0L3endS1542 = _M0L3arrS580.$2;
  _M0L5startS1543 = _M0L3arrS580.$1;
  _M0L7_2abindS583 = _M0L3endS1542 - _M0L5startS1543;
  _M0L2__S584 = 0;
  while (1) {
    if (_M0L2__S584 < _M0L7_2abindS583) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1865 =
        _M0L3arrS580.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1539 =
        _M0L8_2afieldS1865;
      int32_t _M0L5startS1541 = _M0L3arrS580.$1;
      int32_t _M0L6_2atmpS1540 = _M0L5startS1541 + _M0L2__S584;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1864 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1539[
          _M0L6_2atmpS1540
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS585 = _M0L6_2atmpS1864;
      int32_t _M0L6_2atmpS1536 = _M0L1eS585->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1863 =
        _M0L1eS585->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1537 =
        _M0L8_2afieldS1863;
      int32_t _M0L6_2atmpS1538;
      moonbit_incref(_M0L6_2atmpS1537);
      moonbit_incref(_M0L1mS582);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS582, _M0L6_2atmpS1536, _M0L6_2atmpS1537);
      _M0L6_2atmpS1538 = _M0L2__S584 + 1;
      _M0L2__S584 = _M0L6_2atmpS1538;
      continue;
    } else {
      moonbit_decref(_M0L3arrS580.$0);
    }
    break;
  }
  return _M0L1mS582;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS565,
  moonbit_string_t _M0L3keyS566,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS567
) {
  int32_t _M0L6_2atmpS1519;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS566);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1519 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS566);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS565, _M0L3keyS566, _M0L5valueS567, _M0L6_2atmpS1519);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS568,
  int32_t _M0L3keyS569,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS570
) {
  int32_t _M0L6_2atmpS1520;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1520 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS569);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS568, _M0L3keyS569, _M0L5valueS570, _M0L6_2atmpS1520);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS544
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1872;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS543;
  int32_t _M0L8capacityS1511;
  int32_t _M0L13new__capacityS545;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1506;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1505;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS1871;
  int32_t _M0L6_2atmpS1507;
  int32_t _M0L8capacityS1509;
  int32_t _M0L6_2atmpS1508;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1510;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1870;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS546;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1872 = _M0L4selfS544->$5;
  _M0L9old__headS543 = _M0L8_2afieldS1872;
  _M0L8capacityS1511 = _M0L4selfS544->$2;
  _M0L13new__capacityS545 = _M0L8capacityS1511 << 1;
  _M0L6_2atmpS1506 = 0;
  _M0L6_2atmpS1505
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS545, _M0L6_2atmpS1506);
  _M0L6_2aoldS1871 = _M0L4selfS544->$0;
  if (_M0L9old__headS543) {
    moonbit_incref(_M0L9old__headS543);
  }
  moonbit_decref(_M0L6_2aoldS1871);
  _M0L4selfS544->$0 = _M0L6_2atmpS1505;
  _M0L4selfS544->$2 = _M0L13new__capacityS545;
  _M0L6_2atmpS1507 = _M0L13new__capacityS545 - 1;
  _M0L4selfS544->$3 = _M0L6_2atmpS1507;
  _M0L8capacityS1509 = _M0L4selfS544->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1508 = _M0FPB21calc__grow__threshold(_M0L8capacityS1509);
  _M0L4selfS544->$4 = _M0L6_2atmpS1508;
  _M0L4selfS544->$1 = 0;
  _M0L6_2atmpS1510 = 0;
  _M0L6_2aoldS1870 = _M0L4selfS544->$5;
  if (_M0L6_2aoldS1870) {
    moonbit_decref(_M0L6_2aoldS1870);
  }
  _M0L4selfS544->$5 = _M0L6_2atmpS1510;
  _M0L4selfS544->$6 = -1;
  _M0L8_2aparamS546 = _M0L9old__headS543;
  while (1) {
    if (_M0L8_2aparamS546 == 0) {
      if (_M0L8_2aparamS546) {
        moonbit_decref(_M0L8_2aparamS546);
      }
      moonbit_decref(_M0L4selfS544);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS547 =
        _M0L8_2aparamS546;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS548 =
        _M0L7_2aSomeS547;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1869 =
        _M0L4_2axS548->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS549 =
        _M0L8_2afieldS1869;
      moonbit_string_t _M0L8_2afieldS1868 = _M0L4_2axS548->$4;
      moonbit_string_t _M0L6_2akeyS550 = _M0L8_2afieldS1868;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1867 =
        _M0L4_2axS548->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS551 =
        _M0L8_2afieldS1867;
      int32_t _M0L8_2afieldS1866 = _M0L4_2axS548->$3;
      int32_t _M0L6_2acntS2046 = Moonbit_object_header(_M0L4_2axS548)->rc;
      int32_t _M0L7_2ahashS552;
      if (_M0L6_2acntS2046 > 1) {
        int32_t _M0L11_2anew__cntS2047 = _M0L6_2acntS2046 - 1;
        Moonbit_object_header(_M0L4_2axS548)->rc = _M0L11_2anew__cntS2047;
        moonbit_incref(_M0L8_2avalueS551);
        moonbit_incref(_M0L6_2akeyS550);
        if (_M0L7_2anextS549) {
          moonbit_incref(_M0L7_2anextS549);
        }
      } else if (_M0L6_2acntS2046 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS548);
      }
      _M0L7_2ahashS552 = _M0L8_2afieldS1866;
      moonbit_incref(_M0L4selfS544);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS544, _M0L6_2akeyS550, _M0L8_2avalueS551, _M0L7_2ahashS552);
      _M0L8_2aparamS546 = _M0L7_2anextS549;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS555
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1878;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS554;
  int32_t _M0L8capacityS1518;
  int32_t _M0L13new__capacityS556;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1513;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1512;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS1877;
  int32_t _M0L6_2atmpS1514;
  int32_t _M0L8capacityS1516;
  int32_t _M0L6_2atmpS1515;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1517;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1876;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS557;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1878 = _M0L4selfS555->$5;
  _M0L9old__headS554 = _M0L8_2afieldS1878;
  _M0L8capacityS1518 = _M0L4selfS555->$2;
  _M0L13new__capacityS556 = _M0L8capacityS1518 << 1;
  _M0L6_2atmpS1513 = 0;
  _M0L6_2atmpS1512
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS556, _M0L6_2atmpS1513);
  _M0L6_2aoldS1877 = _M0L4selfS555->$0;
  if (_M0L9old__headS554) {
    moonbit_incref(_M0L9old__headS554);
  }
  moonbit_decref(_M0L6_2aoldS1877);
  _M0L4selfS555->$0 = _M0L6_2atmpS1512;
  _M0L4selfS555->$2 = _M0L13new__capacityS556;
  _M0L6_2atmpS1514 = _M0L13new__capacityS556 - 1;
  _M0L4selfS555->$3 = _M0L6_2atmpS1514;
  _M0L8capacityS1516 = _M0L4selfS555->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1515 = _M0FPB21calc__grow__threshold(_M0L8capacityS1516);
  _M0L4selfS555->$4 = _M0L6_2atmpS1515;
  _M0L4selfS555->$1 = 0;
  _M0L6_2atmpS1517 = 0;
  _M0L6_2aoldS1876 = _M0L4selfS555->$5;
  if (_M0L6_2aoldS1876) {
    moonbit_decref(_M0L6_2aoldS1876);
  }
  _M0L4selfS555->$5 = _M0L6_2atmpS1517;
  _M0L4selfS555->$6 = -1;
  _M0L8_2aparamS557 = _M0L9old__headS554;
  while (1) {
    if (_M0L8_2aparamS557 == 0) {
      if (_M0L8_2aparamS557) {
        moonbit_decref(_M0L8_2aparamS557);
      }
      moonbit_decref(_M0L4selfS555);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS558 =
        _M0L8_2aparamS557;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS559 =
        _M0L7_2aSomeS558;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1875 =
        _M0L4_2axS559->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS560 =
        _M0L8_2afieldS1875;
      int32_t _M0L6_2akeyS561 = _M0L4_2axS559->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1874 =
        _M0L4_2axS559->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS562 =
        _M0L8_2afieldS1874;
      int32_t _M0L8_2afieldS1873 = _M0L4_2axS559->$3;
      int32_t _M0L6_2acntS2048 = Moonbit_object_header(_M0L4_2axS559)->rc;
      int32_t _M0L7_2ahashS563;
      if (_M0L6_2acntS2048 > 1) {
        int32_t _M0L11_2anew__cntS2049 = _M0L6_2acntS2048 - 1;
        Moonbit_object_header(_M0L4_2axS559)->rc = _M0L11_2anew__cntS2049;
        moonbit_incref(_M0L8_2avalueS562);
        if (_M0L7_2anextS560) {
          moonbit_incref(_M0L7_2anextS560);
        }
      } else if (_M0L6_2acntS2048 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS559);
      }
      _M0L7_2ahashS563 = _M0L8_2afieldS1873;
      moonbit_incref(_M0L4selfS555);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS555, _M0L6_2akeyS561, _M0L8_2avalueS562, _M0L7_2ahashS563);
      _M0L8_2aparamS557 = _M0L7_2anextS560;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS514,
  moonbit_string_t _M0L3keyS520,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS521,
  int32_t _M0L4hashS516
) {
  int32_t _M0L14capacity__maskS1486;
  int32_t _M0L6_2atmpS1485;
  int32_t _M0L3pslS511;
  int32_t _M0L3idxS512;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1486 = _M0L4selfS514->$3;
  _M0L6_2atmpS1485 = _M0L4hashS516 & _M0L14capacity__maskS1486;
  _M0L3pslS511 = 0;
  _M0L3idxS512 = _M0L6_2atmpS1485;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1883 =
      _M0L4selfS514->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1484 =
      _M0L8_2afieldS1883;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1882;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS513;
    if (
      _M0L3idxS512 < 0
      || _M0L3idxS512 >= Moonbit_array_length(_M0L7entriesS1484)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1882
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1484[
        _M0L3idxS512
      ];
    _M0L7_2abindS513 = _M0L6_2atmpS1882;
    if (_M0L7_2abindS513 == 0) {
      int32_t _M0L4sizeS1469 = _M0L4selfS514->$1;
      int32_t _M0L8grow__atS1470 = _M0L4selfS514->$4;
      int32_t _M0L7_2abindS517;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS518;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS519;
      if (_M0L4sizeS1469 >= _M0L8grow__atS1470) {
        int32_t _M0L14capacity__maskS1472;
        int32_t _M0L6_2atmpS1471;
        moonbit_incref(_M0L4selfS514);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS514);
        _M0L14capacity__maskS1472 = _M0L4selfS514->$3;
        _M0L6_2atmpS1471 = _M0L4hashS516 & _M0L14capacity__maskS1472;
        _M0L3pslS511 = 0;
        _M0L3idxS512 = _M0L6_2atmpS1471;
        continue;
      }
      _M0L7_2abindS517 = _M0L4selfS514->$6;
      _M0L7_2abindS518 = 0;
      _M0L5entryS519
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS519)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS519->$0 = _M0L7_2abindS517;
      _M0L5entryS519->$1 = _M0L7_2abindS518;
      _M0L5entryS519->$2 = _M0L3pslS511;
      _M0L5entryS519->$3 = _M0L4hashS516;
      _M0L5entryS519->$4 = _M0L3keyS520;
      _M0L5entryS519->$5 = _M0L5valueS521;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS514, _M0L3idxS512, _M0L5entryS519);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS522 =
        _M0L7_2abindS513;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS523 =
        _M0L7_2aSomeS522;
      int32_t _M0L4hashS1474 = _M0L14_2acurr__entryS523->$3;
      int32_t _if__result_2131;
      int32_t _M0L3pslS1475;
      int32_t _M0L6_2atmpS1480;
      int32_t _M0L6_2atmpS1482;
      int32_t _M0L14capacity__maskS1483;
      int32_t _M0L6_2atmpS1481;
      if (_M0L4hashS1474 == _M0L4hashS516) {
        moonbit_string_t _M0L8_2afieldS1881 = _M0L14_2acurr__entryS523->$4;
        moonbit_string_t _M0L3keyS1473 = _M0L8_2afieldS1881;
        int32_t _M0L6_2atmpS1880;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS1880
        = moonbit_val_array_equal(_M0L3keyS1473, _M0L3keyS520);
        _if__result_2131 = _M0L6_2atmpS1880;
      } else {
        _if__result_2131 = 0;
      }
      if (_if__result_2131) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1879;
        moonbit_incref(_M0L14_2acurr__entryS523);
        moonbit_decref(_M0L3keyS520);
        moonbit_decref(_M0L4selfS514);
        _M0L6_2aoldS1879 = _M0L14_2acurr__entryS523->$5;
        moonbit_decref(_M0L6_2aoldS1879);
        _M0L14_2acurr__entryS523->$5 = _M0L5valueS521;
        moonbit_decref(_M0L14_2acurr__entryS523);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS523);
      }
      _M0L3pslS1475 = _M0L14_2acurr__entryS523->$2;
      if (_M0L3pslS511 > _M0L3pslS1475) {
        int32_t _M0L4sizeS1476 = _M0L4selfS514->$1;
        int32_t _M0L8grow__atS1477 = _M0L4selfS514->$4;
        int32_t _M0L7_2abindS524;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS525;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS526;
        if (_M0L4sizeS1476 >= _M0L8grow__atS1477) {
          int32_t _M0L14capacity__maskS1479;
          int32_t _M0L6_2atmpS1478;
          moonbit_decref(_M0L14_2acurr__entryS523);
          moonbit_incref(_M0L4selfS514);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS514);
          _M0L14capacity__maskS1479 = _M0L4selfS514->$3;
          _M0L6_2atmpS1478 = _M0L4hashS516 & _M0L14capacity__maskS1479;
          _M0L3pslS511 = 0;
          _M0L3idxS512 = _M0L6_2atmpS1478;
          continue;
        }
        moonbit_incref(_M0L4selfS514);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS514, _M0L3idxS512, _M0L14_2acurr__entryS523);
        _M0L7_2abindS524 = _M0L4selfS514->$6;
        _M0L7_2abindS525 = 0;
        _M0L5entryS526
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS526)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS526->$0 = _M0L7_2abindS524;
        _M0L5entryS526->$1 = _M0L7_2abindS525;
        _M0L5entryS526->$2 = _M0L3pslS511;
        _M0L5entryS526->$3 = _M0L4hashS516;
        _M0L5entryS526->$4 = _M0L3keyS520;
        _M0L5entryS526->$5 = _M0L5valueS521;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS514, _M0L3idxS512, _M0L5entryS526);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS523);
      }
      _M0L6_2atmpS1480 = _M0L3pslS511 + 1;
      _M0L6_2atmpS1482 = _M0L3idxS512 + 1;
      _M0L14capacity__maskS1483 = _M0L4selfS514->$3;
      _M0L6_2atmpS1481 = _M0L6_2atmpS1482 & _M0L14capacity__maskS1483;
      _M0L3pslS511 = _M0L6_2atmpS1480;
      _M0L3idxS512 = _M0L6_2atmpS1481;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS530,
  int32_t _M0L3keyS536,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS537,
  int32_t _M0L4hashS532
) {
  int32_t _M0L14capacity__maskS1504;
  int32_t _M0L6_2atmpS1503;
  int32_t _M0L3pslS527;
  int32_t _M0L3idxS528;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1504 = _M0L4selfS530->$3;
  _M0L6_2atmpS1503 = _M0L4hashS532 & _M0L14capacity__maskS1504;
  _M0L3pslS527 = 0;
  _M0L3idxS528 = _M0L6_2atmpS1503;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1886 =
      _M0L4selfS530->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1502 =
      _M0L8_2afieldS1886;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1885;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS529;
    if (
      _M0L3idxS528 < 0
      || _M0L3idxS528 >= Moonbit_array_length(_M0L7entriesS1502)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1885
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1502[
        _M0L3idxS528
      ];
    _M0L7_2abindS529 = _M0L6_2atmpS1885;
    if (_M0L7_2abindS529 == 0) {
      int32_t _M0L4sizeS1487 = _M0L4selfS530->$1;
      int32_t _M0L8grow__atS1488 = _M0L4selfS530->$4;
      int32_t _M0L7_2abindS533;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS534;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS535;
      if (_M0L4sizeS1487 >= _M0L8grow__atS1488) {
        int32_t _M0L14capacity__maskS1490;
        int32_t _M0L6_2atmpS1489;
        moonbit_incref(_M0L4selfS530);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS530);
        _M0L14capacity__maskS1490 = _M0L4selfS530->$3;
        _M0L6_2atmpS1489 = _M0L4hashS532 & _M0L14capacity__maskS1490;
        _M0L3pslS527 = 0;
        _M0L3idxS528 = _M0L6_2atmpS1489;
        continue;
      }
      _M0L7_2abindS533 = _M0L4selfS530->$6;
      _M0L7_2abindS534 = 0;
      _M0L5entryS535
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS535)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS535->$0 = _M0L7_2abindS533;
      _M0L5entryS535->$1 = _M0L7_2abindS534;
      _M0L5entryS535->$2 = _M0L3pslS527;
      _M0L5entryS535->$3 = _M0L4hashS532;
      _M0L5entryS535->$4 = _M0L3keyS536;
      _M0L5entryS535->$5 = _M0L5valueS537;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS530, _M0L3idxS528, _M0L5entryS535);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS538 =
        _M0L7_2abindS529;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS539 =
        _M0L7_2aSomeS538;
      int32_t _M0L4hashS1492 = _M0L14_2acurr__entryS539->$3;
      int32_t _if__result_2133;
      int32_t _M0L3pslS1493;
      int32_t _M0L6_2atmpS1498;
      int32_t _M0L6_2atmpS1500;
      int32_t _M0L14capacity__maskS1501;
      int32_t _M0L6_2atmpS1499;
      if (_M0L4hashS1492 == _M0L4hashS532) {
        int32_t _M0L3keyS1491 = _M0L14_2acurr__entryS539->$4;
        _if__result_2133 = _M0L3keyS1491 == _M0L3keyS536;
      } else {
        _if__result_2133 = 0;
      }
      if (_if__result_2133) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS1884;
        moonbit_incref(_M0L14_2acurr__entryS539);
        moonbit_decref(_M0L4selfS530);
        _M0L6_2aoldS1884 = _M0L14_2acurr__entryS539->$5;
        moonbit_decref(_M0L6_2aoldS1884);
        _M0L14_2acurr__entryS539->$5 = _M0L5valueS537;
        moonbit_decref(_M0L14_2acurr__entryS539);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS539);
      }
      _M0L3pslS1493 = _M0L14_2acurr__entryS539->$2;
      if (_M0L3pslS527 > _M0L3pslS1493) {
        int32_t _M0L4sizeS1494 = _M0L4selfS530->$1;
        int32_t _M0L8grow__atS1495 = _M0L4selfS530->$4;
        int32_t _M0L7_2abindS540;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS541;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS542;
        if (_M0L4sizeS1494 >= _M0L8grow__atS1495) {
          int32_t _M0L14capacity__maskS1497;
          int32_t _M0L6_2atmpS1496;
          moonbit_decref(_M0L14_2acurr__entryS539);
          moonbit_incref(_M0L4selfS530);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS530);
          _M0L14capacity__maskS1497 = _M0L4selfS530->$3;
          _M0L6_2atmpS1496 = _M0L4hashS532 & _M0L14capacity__maskS1497;
          _M0L3pslS527 = 0;
          _M0L3idxS528 = _M0L6_2atmpS1496;
          continue;
        }
        moonbit_incref(_M0L4selfS530);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS530, _M0L3idxS528, _M0L14_2acurr__entryS539);
        _M0L7_2abindS540 = _M0L4selfS530->$6;
        _M0L7_2abindS541 = 0;
        _M0L5entryS542
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS542)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS542->$0 = _M0L7_2abindS540;
        _M0L5entryS542->$1 = _M0L7_2abindS541;
        _M0L5entryS542->$2 = _M0L3pslS527;
        _M0L5entryS542->$3 = _M0L4hashS532;
        _M0L5entryS542->$4 = _M0L3keyS536;
        _M0L5entryS542->$5 = _M0L5valueS537;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS530, _M0L3idxS528, _M0L5entryS542);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS539);
      }
      _M0L6_2atmpS1498 = _M0L3pslS527 + 1;
      _M0L6_2atmpS1500 = _M0L3idxS528 + 1;
      _M0L14capacity__maskS1501 = _M0L4selfS530->$3;
      _M0L6_2atmpS1499 = _M0L6_2atmpS1500 & _M0L14capacity__maskS1501;
      _M0L3pslS527 = _M0L6_2atmpS1498;
      _M0L3idxS528 = _M0L6_2atmpS1499;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS495,
  int32_t _M0L3idxS500,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS499
) {
  int32_t _M0L3pslS1452;
  int32_t _M0L6_2atmpS1448;
  int32_t _M0L6_2atmpS1450;
  int32_t _M0L14capacity__maskS1451;
  int32_t _M0L6_2atmpS1449;
  int32_t _M0L3pslS491;
  int32_t _M0L3idxS492;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS493;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1452 = _M0L5entryS499->$2;
  _M0L6_2atmpS1448 = _M0L3pslS1452 + 1;
  _M0L6_2atmpS1450 = _M0L3idxS500 + 1;
  _M0L14capacity__maskS1451 = _M0L4selfS495->$3;
  _M0L6_2atmpS1449 = _M0L6_2atmpS1450 & _M0L14capacity__maskS1451;
  _M0L3pslS491 = _M0L6_2atmpS1448;
  _M0L3idxS492 = _M0L6_2atmpS1449;
  _M0L5entryS493 = _M0L5entryS499;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1888 =
      _M0L4selfS495->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1447 =
      _M0L8_2afieldS1888;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1887;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS494;
    if (
      _M0L3idxS492 < 0
      || _M0L3idxS492 >= Moonbit_array_length(_M0L7entriesS1447)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1887
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1447[
        _M0L3idxS492
      ];
    _M0L7_2abindS494 = _M0L6_2atmpS1887;
    if (_M0L7_2abindS494 == 0) {
      _M0L5entryS493->$2 = _M0L3pslS491;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS495, _M0L5entryS493, _M0L3idxS492);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS497 =
        _M0L7_2abindS494;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS498 =
        _M0L7_2aSomeS497;
      int32_t _M0L3pslS1437 = _M0L14_2acurr__entryS498->$2;
      if (_M0L3pslS491 > _M0L3pslS1437) {
        int32_t _M0L3pslS1442;
        int32_t _M0L6_2atmpS1438;
        int32_t _M0L6_2atmpS1440;
        int32_t _M0L14capacity__maskS1441;
        int32_t _M0L6_2atmpS1439;
        _M0L5entryS493->$2 = _M0L3pslS491;
        moonbit_incref(_M0L14_2acurr__entryS498);
        moonbit_incref(_M0L4selfS495);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS495, _M0L5entryS493, _M0L3idxS492);
        _M0L3pslS1442 = _M0L14_2acurr__entryS498->$2;
        _M0L6_2atmpS1438 = _M0L3pslS1442 + 1;
        _M0L6_2atmpS1440 = _M0L3idxS492 + 1;
        _M0L14capacity__maskS1441 = _M0L4selfS495->$3;
        _M0L6_2atmpS1439 = _M0L6_2atmpS1440 & _M0L14capacity__maskS1441;
        _M0L3pslS491 = _M0L6_2atmpS1438;
        _M0L3idxS492 = _M0L6_2atmpS1439;
        _M0L5entryS493 = _M0L14_2acurr__entryS498;
        continue;
      } else {
        int32_t _M0L6_2atmpS1443 = _M0L3pslS491 + 1;
        int32_t _M0L6_2atmpS1445 = _M0L3idxS492 + 1;
        int32_t _M0L14capacity__maskS1446 = _M0L4selfS495->$3;
        int32_t _M0L6_2atmpS1444 =
          _M0L6_2atmpS1445 & _M0L14capacity__maskS1446;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_2135 =
          _M0L5entryS493;
        _M0L3pslS491 = _M0L6_2atmpS1443;
        _M0L3idxS492 = _M0L6_2atmpS1444;
        _M0L5entryS493 = _tmp_2135;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS505,
  int32_t _M0L3idxS510,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS509
) {
  int32_t _M0L3pslS1468;
  int32_t _M0L6_2atmpS1464;
  int32_t _M0L6_2atmpS1466;
  int32_t _M0L14capacity__maskS1467;
  int32_t _M0L6_2atmpS1465;
  int32_t _M0L3pslS501;
  int32_t _M0L3idxS502;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS503;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1468 = _M0L5entryS509->$2;
  _M0L6_2atmpS1464 = _M0L3pslS1468 + 1;
  _M0L6_2atmpS1466 = _M0L3idxS510 + 1;
  _M0L14capacity__maskS1467 = _M0L4selfS505->$3;
  _M0L6_2atmpS1465 = _M0L6_2atmpS1466 & _M0L14capacity__maskS1467;
  _M0L3pslS501 = _M0L6_2atmpS1464;
  _M0L3idxS502 = _M0L6_2atmpS1465;
  _M0L5entryS503 = _M0L5entryS509;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1890 =
      _M0L4selfS505->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1463 =
      _M0L8_2afieldS1890;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1889;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS504;
    if (
      _M0L3idxS502 < 0
      || _M0L3idxS502 >= Moonbit_array_length(_M0L7entriesS1463)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1889
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1463[
        _M0L3idxS502
      ];
    _M0L7_2abindS504 = _M0L6_2atmpS1889;
    if (_M0L7_2abindS504 == 0) {
      _M0L5entryS503->$2 = _M0L3pslS501;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS505, _M0L5entryS503, _M0L3idxS502);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS507 =
        _M0L7_2abindS504;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS508 =
        _M0L7_2aSomeS507;
      int32_t _M0L3pslS1453 = _M0L14_2acurr__entryS508->$2;
      if (_M0L3pslS501 > _M0L3pslS1453) {
        int32_t _M0L3pslS1458;
        int32_t _M0L6_2atmpS1454;
        int32_t _M0L6_2atmpS1456;
        int32_t _M0L14capacity__maskS1457;
        int32_t _M0L6_2atmpS1455;
        _M0L5entryS503->$2 = _M0L3pslS501;
        moonbit_incref(_M0L14_2acurr__entryS508);
        moonbit_incref(_M0L4selfS505);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS505, _M0L5entryS503, _M0L3idxS502);
        _M0L3pslS1458 = _M0L14_2acurr__entryS508->$2;
        _M0L6_2atmpS1454 = _M0L3pslS1458 + 1;
        _M0L6_2atmpS1456 = _M0L3idxS502 + 1;
        _M0L14capacity__maskS1457 = _M0L4selfS505->$3;
        _M0L6_2atmpS1455 = _M0L6_2atmpS1456 & _M0L14capacity__maskS1457;
        _M0L3pslS501 = _M0L6_2atmpS1454;
        _M0L3idxS502 = _M0L6_2atmpS1455;
        _M0L5entryS503 = _M0L14_2acurr__entryS508;
        continue;
      } else {
        int32_t _M0L6_2atmpS1459 = _M0L3pslS501 + 1;
        int32_t _M0L6_2atmpS1461 = _M0L3idxS502 + 1;
        int32_t _M0L14capacity__maskS1462 = _M0L4selfS505->$3;
        int32_t _M0L6_2atmpS1460 =
          _M0L6_2atmpS1461 & _M0L14capacity__maskS1462;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_2137 =
          _M0L5entryS503;
        _M0L3pslS501 = _M0L6_2atmpS1459;
        _M0L3idxS502 = _M0L6_2atmpS1460;
        _M0L5entryS503 = _tmp_2137;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS479,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS481,
  int32_t _M0L8new__idxS480
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1893;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1433;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1434;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1892;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1891;
  int32_t _M0L6_2acntS2050;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS482;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1893 = _M0L4selfS479->$0;
  _M0L7entriesS1433 = _M0L8_2afieldS1893;
  moonbit_incref(_M0L5entryS481);
  _M0L6_2atmpS1434 = _M0L5entryS481;
  if (
    _M0L8new__idxS480 < 0
    || _M0L8new__idxS480 >= Moonbit_array_length(_M0L7entriesS1433)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1892
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1433[
      _M0L8new__idxS480
    ];
  if (_M0L6_2aoldS1892) {
    moonbit_decref(_M0L6_2aoldS1892);
  }
  _M0L7entriesS1433[_M0L8new__idxS480] = _M0L6_2atmpS1434;
  _M0L8_2afieldS1891 = _M0L5entryS481->$1;
  _M0L6_2acntS2050 = Moonbit_object_header(_M0L5entryS481)->rc;
  if (_M0L6_2acntS2050 > 1) {
    int32_t _M0L11_2anew__cntS2053 = _M0L6_2acntS2050 - 1;
    Moonbit_object_header(_M0L5entryS481)->rc = _M0L11_2anew__cntS2053;
    if (_M0L8_2afieldS1891) {
      moonbit_incref(_M0L8_2afieldS1891);
    }
  } else if (_M0L6_2acntS2050 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2052 =
      _M0L5entryS481->$5;
    moonbit_string_t _M0L8_2afieldS2051;
    moonbit_decref(_M0L8_2afieldS2052);
    _M0L8_2afieldS2051 = _M0L5entryS481->$4;
    moonbit_decref(_M0L8_2afieldS2051);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS481);
  }
  _M0L7_2abindS482 = _M0L8_2afieldS1891;
  if (_M0L7_2abindS482 == 0) {
    if (_M0L7_2abindS482) {
      moonbit_decref(_M0L7_2abindS482);
    }
    _M0L4selfS479->$6 = _M0L8new__idxS480;
    moonbit_decref(_M0L4selfS479);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS483;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS484;
    moonbit_decref(_M0L4selfS479);
    _M0L7_2aSomeS483 = _M0L7_2abindS482;
    _M0L7_2anextS484 = _M0L7_2aSomeS483;
    _M0L7_2anextS484->$0 = _M0L8new__idxS480;
    moonbit_decref(_M0L7_2anextS484);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS485,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS487,
  int32_t _M0L8new__idxS486
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1896;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1435;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1436;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1895;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1894;
  int32_t _M0L6_2acntS2054;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS488;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1896 = _M0L4selfS485->$0;
  _M0L7entriesS1435 = _M0L8_2afieldS1896;
  moonbit_incref(_M0L5entryS487);
  _M0L6_2atmpS1436 = _M0L5entryS487;
  if (
    _M0L8new__idxS486 < 0
    || _M0L8new__idxS486 >= Moonbit_array_length(_M0L7entriesS1435)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1895
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1435[
      _M0L8new__idxS486
    ];
  if (_M0L6_2aoldS1895) {
    moonbit_decref(_M0L6_2aoldS1895);
  }
  _M0L7entriesS1435[_M0L8new__idxS486] = _M0L6_2atmpS1436;
  _M0L8_2afieldS1894 = _M0L5entryS487->$1;
  _M0L6_2acntS2054 = Moonbit_object_header(_M0L5entryS487)->rc;
  if (_M0L6_2acntS2054 > 1) {
    int32_t _M0L11_2anew__cntS2056 = _M0L6_2acntS2054 - 1;
    Moonbit_object_header(_M0L5entryS487)->rc = _M0L11_2anew__cntS2056;
    if (_M0L8_2afieldS1894) {
      moonbit_incref(_M0L8_2afieldS1894);
    }
  } else if (_M0L6_2acntS2054 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2055 =
      _M0L5entryS487->$5;
    moonbit_decref(_M0L8_2afieldS2055);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS487);
  }
  _M0L7_2abindS488 = _M0L8_2afieldS1894;
  if (_M0L7_2abindS488 == 0) {
    if (_M0L7_2abindS488) {
      moonbit_decref(_M0L7_2abindS488);
    }
    _M0L4selfS485->$6 = _M0L8new__idxS486;
    moonbit_decref(_M0L4selfS485);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS489;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS490;
    moonbit_decref(_M0L4selfS485);
    _M0L7_2aSomeS489 = _M0L7_2abindS488;
    _M0L7_2anextS490 = _M0L7_2aSomeS489;
    _M0L7_2anextS490->$0 = _M0L8new__idxS486;
    moonbit_decref(_M0L7_2anextS490);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS472,
  int32_t _M0L3idxS474,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS473
) {
  int32_t _M0L7_2abindS471;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1898;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1420;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1421;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1897;
  int32_t _M0L4sizeS1423;
  int32_t _M0L6_2atmpS1422;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS471 = _M0L4selfS472->$6;
  switch (_M0L7_2abindS471) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1415;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1899;
      moonbit_incref(_M0L5entryS473);
      _M0L6_2atmpS1415 = _M0L5entryS473;
      _M0L6_2aoldS1899 = _M0L4selfS472->$5;
      if (_M0L6_2aoldS1899) {
        moonbit_decref(_M0L6_2aoldS1899);
      }
      _M0L4selfS472->$5 = _M0L6_2atmpS1415;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1902 =
        _M0L4selfS472->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1419 =
        _M0L8_2afieldS1902;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1901;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1418;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1416;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1417;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1900;
      if (
        _M0L7_2abindS471 < 0
        || _M0L7_2abindS471 >= Moonbit_array_length(_M0L7entriesS1419)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1901
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1419[
          _M0L7_2abindS471
        ];
      _M0L6_2atmpS1418 = _M0L6_2atmpS1901;
      if (_M0L6_2atmpS1418) {
        moonbit_incref(_M0L6_2atmpS1418);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1416
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1418);
      moonbit_incref(_M0L5entryS473);
      _M0L6_2atmpS1417 = _M0L5entryS473;
      _M0L6_2aoldS1900 = _M0L6_2atmpS1416->$1;
      if (_M0L6_2aoldS1900) {
        moonbit_decref(_M0L6_2aoldS1900);
      }
      _M0L6_2atmpS1416->$1 = _M0L6_2atmpS1417;
      moonbit_decref(_M0L6_2atmpS1416);
      break;
    }
  }
  _M0L4selfS472->$6 = _M0L3idxS474;
  _M0L8_2afieldS1898 = _M0L4selfS472->$0;
  _M0L7entriesS1420 = _M0L8_2afieldS1898;
  _M0L6_2atmpS1421 = _M0L5entryS473;
  if (
    _M0L3idxS474 < 0
    || _M0L3idxS474 >= Moonbit_array_length(_M0L7entriesS1420)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1897
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1420[
      _M0L3idxS474
    ];
  if (_M0L6_2aoldS1897) {
    moonbit_decref(_M0L6_2aoldS1897);
  }
  _M0L7entriesS1420[_M0L3idxS474] = _M0L6_2atmpS1421;
  _M0L4sizeS1423 = _M0L4selfS472->$1;
  _M0L6_2atmpS1422 = _M0L4sizeS1423 + 1;
  _M0L4selfS472->$1 = _M0L6_2atmpS1422;
  moonbit_decref(_M0L4selfS472);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS476,
  int32_t _M0L3idxS478,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS477
) {
  int32_t _M0L7_2abindS475;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1904;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1429;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1430;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1903;
  int32_t _M0L4sizeS1432;
  int32_t _M0L6_2atmpS1431;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS475 = _M0L4selfS476->$6;
  switch (_M0L7_2abindS475) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1424;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1905;
      moonbit_incref(_M0L5entryS477);
      _M0L6_2atmpS1424 = _M0L5entryS477;
      _M0L6_2aoldS1905 = _M0L4selfS476->$5;
      if (_M0L6_2aoldS1905) {
        moonbit_decref(_M0L6_2aoldS1905);
      }
      _M0L4selfS476->$5 = _M0L6_2atmpS1424;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1908 =
        _M0L4selfS476->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1428 =
        _M0L8_2afieldS1908;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1907;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1427;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1425;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1426;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1906;
      if (
        _M0L7_2abindS475 < 0
        || _M0L7_2abindS475 >= Moonbit_array_length(_M0L7entriesS1428)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1907
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1428[
          _M0L7_2abindS475
        ];
      _M0L6_2atmpS1427 = _M0L6_2atmpS1907;
      if (_M0L6_2atmpS1427) {
        moonbit_incref(_M0L6_2atmpS1427);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1425
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1427);
      moonbit_incref(_M0L5entryS477);
      _M0L6_2atmpS1426 = _M0L5entryS477;
      _M0L6_2aoldS1906 = _M0L6_2atmpS1425->$1;
      if (_M0L6_2aoldS1906) {
        moonbit_decref(_M0L6_2aoldS1906);
      }
      _M0L6_2atmpS1425->$1 = _M0L6_2atmpS1426;
      moonbit_decref(_M0L6_2atmpS1425);
      break;
    }
  }
  _M0L4selfS476->$6 = _M0L3idxS478;
  _M0L8_2afieldS1904 = _M0L4selfS476->$0;
  _M0L7entriesS1429 = _M0L8_2afieldS1904;
  _M0L6_2atmpS1430 = _M0L5entryS477;
  if (
    _M0L3idxS478 < 0
    || _M0L3idxS478 >= Moonbit_array_length(_M0L7entriesS1429)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1903
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1429[
      _M0L3idxS478
    ];
  if (_M0L6_2aoldS1903) {
    moonbit_decref(_M0L6_2aoldS1903);
  }
  _M0L7entriesS1429[_M0L3idxS478] = _M0L6_2atmpS1430;
  _M0L4sizeS1432 = _M0L4selfS476->$1;
  _M0L6_2atmpS1431 = _M0L4sizeS1432 + 1;
  _M0L4selfS476->$1 = _M0L6_2atmpS1431;
  moonbit_decref(_M0L4selfS476);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS460
) {
  int32_t _M0L8capacityS459;
  int32_t _M0L7_2abindS461;
  int32_t _M0L7_2abindS462;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1413;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS463;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS464;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_2138;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS459
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS460);
  _M0L7_2abindS461 = _M0L8capacityS459 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS462 = _M0FPB21calc__grow__threshold(_M0L8capacityS459);
  _M0L6_2atmpS1413 = 0;
  _M0L7_2abindS463
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS459, _M0L6_2atmpS1413);
  _M0L7_2abindS464 = 0;
  _block_2138
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_2138)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_2138->$0 = _M0L7_2abindS463;
  _block_2138->$1 = 0;
  _block_2138->$2 = _M0L8capacityS459;
  _block_2138->$3 = _M0L7_2abindS461;
  _block_2138->$4 = _M0L7_2abindS462;
  _block_2138->$5 = _M0L7_2abindS464;
  _block_2138->$6 = -1;
  return _block_2138;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS466
) {
  int32_t _M0L8capacityS465;
  int32_t _M0L7_2abindS467;
  int32_t _M0L7_2abindS468;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1414;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS469;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS470;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_2139;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS465
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS466);
  _M0L7_2abindS467 = _M0L8capacityS465 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS468 = _M0FPB21calc__grow__threshold(_M0L8capacityS465);
  _M0L6_2atmpS1414 = 0;
  _M0L7_2abindS469
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS465, _M0L6_2atmpS1414);
  _M0L7_2abindS470 = 0;
  _block_2139
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_2139)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_2139->$0 = _M0L7_2abindS469;
  _block_2139->$1 = 0;
  _block_2139->$2 = _M0L8capacityS465;
  _block_2139->$3 = _M0L7_2abindS467;
  _block_2139->$4 = _M0L7_2abindS468;
  _block_2139->$5 = _M0L7_2abindS470;
  _block_2139->$6 = -1;
  return _block_2139;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS458) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS458 >= 0) {
    int32_t _M0L6_2atmpS1412;
    int32_t _M0L6_2atmpS1411;
    int32_t _M0L6_2atmpS1410;
    int32_t _M0L6_2atmpS1409;
    if (_M0L4selfS458 <= 1) {
      return 1;
    }
    if (_M0L4selfS458 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1412 = _M0L4selfS458 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1411 = moonbit_clz32(_M0L6_2atmpS1412);
    _M0L6_2atmpS1410 = _M0L6_2atmpS1411 - 1;
    _M0L6_2atmpS1409 = 2147483647 >> (_M0L6_2atmpS1410 & 31);
    return _M0L6_2atmpS1409 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS457) {
  int32_t _M0L6_2atmpS1408;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1408 = _M0L8capacityS457 * 13;
  return _M0L6_2atmpS1408 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS453
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS453 == 0) {
    if (_M0L4selfS453) {
      moonbit_decref(_M0L4selfS453);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS454 =
      _M0L4selfS453;
    return _M0L7_2aSomeS454;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS455
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS455 == 0) {
    if (_M0L4selfS455) {
      moonbit_decref(_M0L4selfS455);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS456 =
      _M0L4selfS455;
    return _M0L7_2aSomeS456;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS452
) {
  moonbit_string_t* _M0L6_2atmpS1407;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1407 = _M0L4selfS452;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1407);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS451
) {
  moonbit_string_t* _M0L6_2atmpS1405;
  int32_t _M0L6_2atmpS1909;
  int32_t _M0L6_2atmpS1406;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1404;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS451);
  _M0L6_2atmpS1405 = _M0L4selfS451;
  _M0L6_2atmpS1909 = Moonbit_array_length(_M0L4selfS451);
  moonbit_decref(_M0L4selfS451);
  _M0L6_2atmpS1406 = _M0L6_2atmpS1909;
  _M0L6_2atmpS1404
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1406, _M0L6_2atmpS1405
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1404);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS449
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS448;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__* _closure_2140;
  struct _M0TWEOs* _M0L6_2atmpS1392;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS448
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS448)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS448->$0 = 0;
  _closure_2140
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__));
  Moonbit_object_header(_closure_2140)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__, $0_0) >> 2, 2, 0);
  _closure_2140->code = &_M0MPC15array9ArrayView4iterGsEC1393l570;
  _closure_2140->$0_0 = _M0L4selfS449.$0;
  _closure_2140->$0_1 = _M0L4selfS449.$1;
  _closure_2140->$0_2 = _M0L4selfS449.$2;
  _closure_2140->$1 = _M0L1iS448;
  _M0L6_2atmpS1392 = (struct _M0TWEOs*)_closure_2140;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1392);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1393l570(
  struct _M0TWEOs* _M0L6_2aenvS1394
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__* _M0L14_2acasted__envS1395;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS1914;
  struct _M0TPC13ref3RefGiE* _M0L1iS448;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS1913;
  int32_t _M0L6_2acntS2057;
  struct _M0TPB9ArrayViewGsE _M0L4selfS449;
  int32_t _M0L3valS1396;
  int32_t _M0L6_2atmpS1397;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1395
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1393__l570__*)_M0L6_2aenvS1394;
  _M0L8_2afieldS1914 = _M0L14_2acasted__envS1395->$1;
  _M0L1iS448 = _M0L8_2afieldS1914;
  _M0L8_2afieldS1913
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1395->$0_1,
      _M0L14_2acasted__envS1395->$0_2,
      _M0L14_2acasted__envS1395->$0_0
  };
  _M0L6_2acntS2057 = Moonbit_object_header(_M0L14_2acasted__envS1395)->rc;
  if (_M0L6_2acntS2057 > 1) {
    int32_t _M0L11_2anew__cntS2058 = _M0L6_2acntS2057 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1395)->rc
    = _M0L11_2anew__cntS2058;
    moonbit_incref(_M0L1iS448);
    moonbit_incref(_M0L8_2afieldS1913.$0);
  } else if (_M0L6_2acntS2057 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1395);
  }
  _M0L4selfS449 = _M0L8_2afieldS1913;
  _M0L3valS1396 = _M0L1iS448->$0;
  moonbit_incref(_M0L4selfS449.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1397 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS449);
  if (_M0L3valS1396 < _M0L6_2atmpS1397) {
    moonbit_string_t* _M0L8_2afieldS1912 = _M0L4selfS449.$0;
    moonbit_string_t* _M0L3bufS1400 = _M0L8_2afieldS1912;
    int32_t _M0L8_2afieldS1911 = _M0L4selfS449.$1;
    int32_t _M0L5startS1402 = _M0L8_2afieldS1911;
    int32_t _M0L3valS1403 = _M0L1iS448->$0;
    int32_t _M0L6_2atmpS1401 = _M0L5startS1402 + _M0L3valS1403;
    moonbit_string_t _M0L6_2atmpS1910 =
      (moonbit_string_t)_M0L3bufS1400[_M0L6_2atmpS1401];
    moonbit_string_t _M0L4elemS450;
    int32_t _M0L3valS1399;
    int32_t _M0L6_2atmpS1398;
    moonbit_incref(_M0L6_2atmpS1910);
    moonbit_decref(_M0L3bufS1400);
    _M0L4elemS450 = _M0L6_2atmpS1910;
    _M0L3valS1399 = _M0L1iS448->$0;
    _M0L6_2atmpS1398 = _M0L3valS1399 + 1;
    _M0L1iS448->$0 = _M0L6_2atmpS1398;
    moonbit_decref(_M0L1iS448);
    return _M0L4elemS450;
  } else {
    moonbit_decref(_M0L4selfS449.$0);
    moonbit_decref(_M0L1iS448);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS447
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS447;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS446,
  struct _M0TPB6Logger _M0L6loggerS445
) {
  moonbit_string_t _M0L6_2atmpS1391;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1391 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS446, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS445.$0->$method_0(_M0L6loggerS445.$1, _M0L6_2atmpS1391);
  return 0;
}

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t _M0L4selfS444) {
  #line 435 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0FPB16char__to__string(_M0L4selfS444);
}

moonbit_string_t _M0FPB16char__to__string(int32_t _M0L4charS443) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS442;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1390;
  #line 441 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0L7_2aselfS442 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L7_2aselfS442);
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS442, _M0L4charS443);
  _M0L6_2atmpS1390 = _M0L7_2aselfS442;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\char.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1390);
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS436,
  moonbit_string_t _M0L5valueS438
) {
  int32_t _M0L3lenS1380;
  moonbit_string_t* _M0L6_2atmpS1382;
  int32_t _M0L6_2atmpS1917;
  int32_t _M0L6_2atmpS1381;
  int32_t _M0L6lengthS437;
  moonbit_string_t* _M0L8_2afieldS1916;
  moonbit_string_t* _M0L3bufS1383;
  moonbit_string_t _M0L6_2aoldS1915;
  int32_t _M0L6_2atmpS1384;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1380 = _M0L4selfS436->$1;
  moonbit_incref(_M0L4selfS436);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1382 = _M0MPC15array5Array6bufferGsE(_M0L4selfS436);
  _M0L6_2atmpS1917 = Moonbit_array_length(_M0L6_2atmpS1382);
  moonbit_decref(_M0L6_2atmpS1382);
  _M0L6_2atmpS1381 = _M0L6_2atmpS1917;
  if (_M0L3lenS1380 == _M0L6_2atmpS1381) {
    moonbit_incref(_M0L4selfS436);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS436);
  }
  _M0L6lengthS437 = _M0L4selfS436->$1;
  _M0L8_2afieldS1916 = _M0L4selfS436->$0;
  _M0L3bufS1383 = _M0L8_2afieldS1916;
  _M0L6_2aoldS1915 = (moonbit_string_t)_M0L3bufS1383[_M0L6lengthS437];
  moonbit_decref(_M0L6_2aoldS1915);
  _M0L3bufS1383[_M0L6lengthS437] = _M0L5valueS438;
  _M0L6_2atmpS1384 = _M0L6lengthS437 + 1;
  _M0L4selfS436->$1 = _M0L6_2atmpS1384;
  moonbit_decref(_M0L4selfS436);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS439,
  struct _M0TUsiE* _M0L5valueS441
) {
  int32_t _M0L3lenS1385;
  struct _M0TUsiE** _M0L6_2atmpS1387;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L6_2atmpS1386;
  int32_t _M0L6lengthS440;
  struct _M0TUsiE** _M0L8_2afieldS1919;
  struct _M0TUsiE** _M0L3bufS1388;
  struct _M0TUsiE* _M0L6_2aoldS1918;
  int32_t _M0L6_2atmpS1389;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1385 = _M0L4selfS439->$1;
  moonbit_incref(_M0L4selfS439);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1387 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS439);
  _M0L6_2atmpS1920 = Moonbit_array_length(_M0L6_2atmpS1387);
  moonbit_decref(_M0L6_2atmpS1387);
  _M0L6_2atmpS1386 = _M0L6_2atmpS1920;
  if (_M0L3lenS1385 == _M0L6_2atmpS1386) {
    moonbit_incref(_M0L4selfS439);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS439);
  }
  _M0L6lengthS440 = _M0L4selfS439->$1;
  _M0L8_2afieldS1919 = _M0L4selfS439->$0;
  _M0L3bufS1388 = _M0L8_2afieldS1919;
  _M0L6_2aoldS1918 = (struct _M0TUsiE*)_M0L3bufS1388[_M0L6lengthS440];
  if (_M0L6_2aoldS1918) {
    moonbit_decref(_M0L6_2aoldS1918);
  }
  _M0L3bufS1388[_M0L6lengthS440] = _M0L5valueS441;
  _M0L6_2atmpS1389 = _M0L6lengthS440 + 1;
  _M0L4selfS439->$1 = _M0L6_2atmpS1389;
  moonbit_decref(_M0L4selfS439);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS431) {
  int32_t _M0L8old__capS430;
  int32_t _M0L8new__capS432;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS430 = _M0L4selfS431->$1;
  if (_M0L8old__capS430 == 0) {
    _M0L8new__capS432 = 8;
  } else {
    _M0L8new__capS432 = _M0L8old__capS430 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS431, _M0L8new__capS432);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS434
) {
  int32_t _M0L8old__capS433;
  int32_t _M0L8new__capS435;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS433 = _M0L4selfS434->$1;
  if (_M0L8old__capS433 == 0) {
    _M0L8new__capS435 = 8;
  } else {
    _M0L8new__capS435 = _M0L8old__capS433 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS434, _M0L8new__capS435);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS421,
  int32_t _M0L13new__capacityS419
) {
  moonbit_string_t* _M0L8new__bufS418;
  moonbit_string_t* _M0L8_2afieldS1922;
  moonbit_string_t* _M0L8old__bufS420;
  int32_t _M0L8old__capS422;
  int32_t _M0L9copy__lenS423;
  moonbit_string_t* _M0L6_2aoldS1921;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS418
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS419, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS1922 = _M0L4selfS421->$0;
  _M0L8old__bufS420 = _M0L8_2afieldS1922;
  _M0L8old__capS422 = Moonbit_array_length(_M0L8old__bufS420);
  if (_M0L8old__capS422 < _M0L13new__capacityS419) {
    _M0L9copy__lenS423 = _M0L8old__capS422;
  } else {
    _M0L9copy__lenS423 = _M0L13new__capacityS419;
  }
  moonbit_incref(_M0L8old__bufS420);
  moonbit_incref(_M0L8new__bufS418);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS418, 0, _M0L8old__bufS420, 0, _M0L9copy__lenS423);
  _M0L6_2aoldS1921 = _M0L4selfS421->$0;
  moonbit_decref(_M0L6_2aoldS1921);
  _M0L4selfS421->$0 = _M0L8new__bufS418;
  moonbit_decref(_M0L4selfS421);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS427,
  int32_t _M0L13new__capacityS425
) {
  struct _M0TUsiE** _M0L8new__bufS424;
  struct _M0TUsiE** _M0L8_2afieldS1924;
  struct _M0TUsiE** _M0L8old__bufS426;
  int32_t _M0L8old__capS428;
  int32_t _M0L9copy__lenS429;
  struct _M0TUsiE** _M0L6_2aoldS1923;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS424
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS425, 0);
  _M0L8_2afieldS1924 = _M0L4selfS427->$0;
  _M0L8old__bufS426 = _M0L8_2afieldS1924;
  _M0L8old__capS428 = Moonbit_array_length(_M0L8old__bufS426);
  if (_M0L8old__capS428 < _M0L13new__capacityS425) {
    _M0L9copy__lenS429 = _M0L8old__capS428;
  } else {
    _M0L9copy__lenS429 = _M0L13new__capacityS425;
  }
  moonbit_incref(_M0L8old__bufS426);
  moonbit_incref(_M0L8new__bufS424);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS424, 0, _M0L8old__bufS426, 0, _M0L9copy__lenS429);
  _M0L6_2aoldS1923 = _M0L4selfS427->$0;
  moonbit_decref(_M0L6_2aoldS1923);
  _M0L4selfS427->$0 = _M0L8new__bufS424;
  moonbit_decref(_M0L4selfS427);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS417
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS417 == 0) {
    moonbit_string_t* _M0L6_2atmpS1378 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2141 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2141)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2141->$0 = _M0L6_2atmpS1378;
    _block_2141->$1 = 0;
    return _block_2141;
  } else {
    moonbit_string_t* _M0L6_2atmpS1379 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS417, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2142 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2142)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2142->$0 = _M0L6_2atmpS1379;
    _block_2142->$1 = 0;
    return _block_2142;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS415,
  struct _M0TPC16string10StringView _M0L3strS416
) {
  int32_t _M0L3lenS1366;
  int32_t _M0L6_2atmpS1368;
  int32_t _M0L6_2atmpS1367;
  int32_t _M0L6_2atmpS1365;
  moonbit_bytes_t _M0L8_2afieldS1925;
  moonbit_bytes_t _M0L4dataS1369;
  int32_t _M0L3lenS1370;
  moonbit_string_t _M0L6_2atmpS1371;
  int32_t _M0L6_2atmpS1372;
  int32_t _M0L6_2atmpS1373;
  int32_t _M0L3lenS1375;
  int32_t _M0L6_2atmpS1377;
  int32_t _M0L6_2atmpS1376;
  int32_t _M0L6_2atmpS1374;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1366 = _M0L4selfS415->$1;
  moonbit_incref(_M0L3strS416.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1368 = _M0MPC16string10StringView6length(_M0L3strS416);
  _M0L6_2atmpS1367 = _M0L6_2atmpS1368 * 2;
  _M0L6_2atmpS1365 = _M0L3lenS1366 + _M0L6_2atmpS1367;
  moonbit_incref(_M0L4selfS415);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS415, _M0L6_2atmpS1365);
  _M0L8_2afieldS1925 = _M0L4selfS415->$0;
  _M0L4dataS1369 = _M0L8_2afieldS1925;
  _M0L3lenS1370 = _M0L4selfS415->$1;
  moonbit_incref(_M0L4dataS1369);
  moonbit_incref(_M0L3strS416.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1371 = _M0MPC16string10StringView4data(_M0L3strS416);
  moonbit_incref(_M0L3strS416.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1372 = _M0MPC16string10StringView13start__offset(_M0L3strS416);
  moonbit_incref(_M0L3strS416.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1373 = _M0MPC16string10StringView6length(_M0L3strS416);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1369, _M0L3lenS1370, _M0L6_2atmpS1371, _M0L6_2atmpS1372, _M0L6_2atmpS1373);
  _M0L3lenS1375 = _M0L4selfS415->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1377 = _M0MPC16string10StringView6length(_M0L3strS416);
  _M0L6_2atmpS1376 = _M0L6_2atmpS1377 * 2;
  _M0L6_2atmpS1374 = _M0L3lenS1375 + _M0L6_2atmpS1376;
  _M0L4selfS415->$1 = _M0L6_2atmpS1374;
  moonbit_decref(_M0L4selfS415);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS407,
  int32_t _M0L3lenS410,
  int32_t _M0L13start__offsetS414,
  int64_t _M0L11end__offsetS405
) {
  int32_t _M0L11end__offsetS404;
  int32_t _M0L5indexS408;
  int32_t _M0L5countS409;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS405 == 4294967296ll) {
    _M0L11end__offsetS404 = Moonbit_array_length(_M0L4selfS407);
  } else {
    int64_t _M0L7_2aSomeS406 = _M0L11end__offsetS405;
    _M0L11end__offsetS404 = (int32_t)_M0L7_2aSomeS406;
  }
  _M0L5indexS408 = _M0L13start__offsetS414;
  _M0L5countS409 = 0;
  while (1) {
    int32_t _if__result_2144;
    if (_M0L5indexS408 < _M0L11end__offsetS404) {
      _if__result_2144 = _M0L5countS409 < _M0L3lenS410;
    } else {
      _if__result_2144 = 0;
    }
    if (_if__result_2144) {
      int32_t _M0L2c1S411 = _M0L4selfS407[_M0L5indexS408];
      int32_t _if__result_2145;
      int32_t _M0L6_2atmpS1363;
      int32_t _M0L6_2atmpS1364;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S411)) {
        int32_t _M0L6_2atmpS1359 = _M0L5indexS408 + 1;
        _if__result_2145 = _M0L6_2atmpS1359 < _M0L11end__offsetS404;
      } else {
        _if__result_2145 = 0;
      }
      if (_if__result_2145) {
        int32_t _M0L6_2atmpS1362 = _M0L5indexS408 + 1;
        int32_t _M0L2c2S412 = _M0L4selfS407[_M0L6_2atmpS1362];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S412)) {
          int32_t _M0L6_2atmpS1360 = _M0L5indexS408 + 2;
          int32_t _M0L6_2atmpS1361 = _M0L5countS409 + 1;
          _M0L5indexS408 = _M0L6_2atmpS1360;
          _M0L5countS409 = _M0L6_2atmpS1361;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_19.data);
        }
      }
      _M0L6_2atmpS1363 = _M0L5indexS408 + 1;
      _M0L6_2atmpS1364 = _M0L5countS409 + 1;
      _M0L5indexS408 = _M0L6_2atmpS1363;
      _M0L5countS409 = _M0L6_2atmpS1364;
      continue;
    } else {
      moonbit_decref(_M0L4selfS407);
      return _M0L5countS409 >= _M0L3lenS410;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS401
) {
  int32_t _M0L3endS1353;
  int32_t _M0L8_2afieldS1926;
  int32_t _M0L5startS1354;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1353 = _M0L4selfS401.$2;
  _M0L8_2afieldS1926 = _M0L4selfS401.$1;
  moonbit_decref(_M0L4selfS401.$0);
  _M0L5startS1354 = _M0L8_2afieldS1926;
  return _M0L3endS1353 - _M0L5startS1354;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS402
) {
  int32_t _M0L3endS1355;
  int32_t _M0L8_2afieldS1927;
  int32_t _M0L5startS1356;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1355 = _M0L4selfS402.$2;
  _M0L8_2afieldS1927 = _M0L4selfS402.$1;
  moonbit_decref(_M0L4selfS402.$0);
  _M0L5startS1356 = _M0L8_2afieldS1927;
  return _M0L3endS1355 - _M0L5startS1356;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS403
) {
  int32_t _M0L3endS1357;
  int32_t _M0L8_2afieldS1928;
  int32_t _M0L5startS1358;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1357 = _M0L4selfS403.$2;
  _M0L8_2afieldS1928 = _M0L4selfS403.$1;
  moonbit_decref(_M0L4selfS403.$0);
  _M0L5startS1358 = _M0L8_2afieldS1928;
  return _M0L3endS1357 - _M0L5startS1358;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS399,
  int64_t _M0L19start__offset_2eoptS397,
  int64_t _M0L11end__offsetS400
) {
  int32_t _M0L13start__offsetS396;
  if (_M0L19start__offset_2eoptS397 == 4294967296ll) {
    _M0L13start__offsetS396 = 0;
  } else {
    int64_t _M0L7_2aSomeS398 = _M0L19start__offset_2eoptS397;
    _M0L13start__offsetS396 = (int32_t)_M0L7_2aSomeS398;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS399, _M0L13start__offsetS396, _M0L11end__offsetS400);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS394,
  int32_t _M0L13start__offsetS395,
  int64_t _M0L11end__offsetS392
) {
  int32_t _M0L11end__offsetS391;
  int32_t _if__result_2146;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS392 == 4294967296ll) {
    _M0L11end__offsetS391 = Moonbit_array_length(_M0L4selfS394);
  } else {
    int64_t _M0L7_2aSomeS393 = _M0L11end__offsetS392;
    _M0L11end__offsetS391 = (int32_t)_M0L7_2aSomeS393;
  }
  if (_M0L13start__offsetS395 >= 0) {
    if (_M0L13start__offsetS395 <= _M0L11end__offsetS391) {
      int32_t _M0L6_2atmpS1352 = Moonbit_array_length(_M0L4selfS394);
      _if__result_2146 = _M0L11end__offsetS391 <= _M0L6_2atmpS1352;
    } else {
      _if__result_2146 = 0;
    }
  } else {
    _if__result_2146 = 0;
  }
  if (_if__result_2146) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS395,
                                                 _M0L11end__offsetS391,
                                                 _M0L4selfS394};
  } else {
    moonbit_decref(_M0L4selfS394);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS390
) {
  moonbit_string_t _M0L8_2afieldS1930;
  moonbit_string_t _M0L3strS1349;
  int32_t _M0L5startS1350;
  int32_t _M0L8_2afieldS1929;
  int32_t _M0L3endS1351;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1930 = _M0L4selfS390.$0;
  _M0L3strS1349 = _M0L8_2afieldS1930;
  _M0L5startS1350 = _M0L4selfS390.$1;
  _M0L8_2afieldS1929 = _M0L4selfS390.$2;
  _M0L3endS1351 = _M0L8_2afieldS1929;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1349, _M0L5startS1350, _M0L3endS1351);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS388,
  struct _M0TPB6Logger _M0L6loggerS389
) {
  moonbit_string_t _M0L8_2afieldS1932;
  moonbit_string_t _M0L3strS1346;
  int32_t _M0L5startS1347;
  int32_t _M0L8_2afieldS1931;
  int32_t _M0L3endS1348;
  moonbit_string_t _M0L6substrS387;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1932 = _M0L4selfS388.$0;
  _M0L3strS1346 = _M0L8_2afieldS1932;
  _M0L5startS1347 = _M0L4selfS388.$1;
  _M0L8_2afieldS1931 = _M0L4selfS388.$2;
  _M0L3endS1348 = _M0L8_2afieldS1931;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS387
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1346, _M0L5startS1347, _M0L3endS1348);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS387, _M0L6loggerS389);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS379,
  struct _M0TPB6Logger _M0L6loggerS377
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS378;
  int32_t _M0L3lenS380;
  int32_t _M0L1iS381;
  int32_t _M0L3segS382;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS377.$1) {
    moonbit_incref(_M0L6loggerS377.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 34);
  moonbit_incref(_M0L4selfS379);
  if (_M0L6loggerS377.$1) {
    moonbit_incref(_M0L6loggerS377.$1);
  }
  _M0L6_2aenvS378
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS378)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS378->$0 = _M0L4selfS379;
  _M0L6_2aenvS378->$1_0 = _M0L6loggerS377.$0;
  _M0L6_2aenvS378->$1_1 = _M0L6loggerS377.$1;
  _M0L3lenS380 = Moonbit_array_length(_M0L4selfS379);
  _M0L1iS381 = 0;
  _M0L3segS382 = 0;
  _2afor_383:;
  while (1) {
    int32_t _M0L4codeS384;
    int32_t _M0L1cS386;
    int32_t _M0L6_2atmpS1330;
    int32_t _M0L6_2atmpS1331;
    int32_t _M0L6_2atmpS1332;
    int32_t _tmp_2150;
    int32_t _tmp_2151;
    if (_M0L1iS381 >= _M0L3lenS380) {
      moonbit_decref(_M0L4selfS379);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
      break;
    }
    _M0L4codeS384 = _M0L4selfS379[_M0L1iS381];
    switch (_M0L4codeS384) {
      case 34: {
        _M0L1cS386 = _M0L4codeS384;
        goto join_385;
        break;
      }
      
      case 92: {
        _M0L1cS386 = _M0L4codeS384;
        goto join_385;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1333;
        int32_t _M0L6_2atmpS1334;
        moonbit_incref(_M0L6_2aenvS378);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_13.data);
        _M0L6_2atmpS1333 = _M0L1iS381 + 1;
        _M0L6_2atmpS1334 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1333;
        _M0L3segS382 = _M0L6_2atmpS1334;
        goto _2afor_383;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1335;
        int32_t _M0L6_2atmpS1336;
        moonbit_incref(_M0L6_2aenvS378);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_14.data);
        _M0L6_2atmpS1335 = _M0L1iS381 + 1;
        _M0L6_2atmpS1336 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1335;
        _M0L3segS382 = _M0L6_2atmpS1336;
        goto _2afor_383;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1337;
        int32_t _M0L6_2atmpS1338;
        moonbit_incref(_M0L6_2aenvS378);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_15.data);
        _M0L6_2atmpS1337 = _M0L1iS381 + 1;
        _M0L6_2atmpS1338 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1337;
        _M0L3segS382 = _M0L6_2atmpS1338;
        goto _2afor_383;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1339;
        int32_t _M0L6_2atmpS1340;
        moonbit_incref(_M0L6_2aenvS378);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_16.data);
        _M0L6_2atmpS1339 = _M0L1iS381 + 1;
        _M0L6_2atmpS1340 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1339;
        _M0L3segS382 = _M0L6_2atmpS1340;
        goto _2afor_383;
        break;
      }
      default: {
        if (_M0L4codeS384 < 32) {
          int32_t _M0L6_2atmpS1342;
          moonbit_string_t _M0L6_2atmpS1341;
          int32_t _M0L6_2atmpS1343;
          int32_t _M0L6_2atmpS1344;
          moonbit_incref(_M0L6_2aenvS378);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
          if (_M0L6loggerS377.$1) {
            moonbit_incref(_M0L6loggerS377.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_17.data);
          _M0L6_2atmpS1342 = _M0L4codeS384 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1341 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1342);
          if (_M0L6loggerS377.$1) {
            moonbit_incref(_M0L6loggerS377.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, _M0L6_2atmpS1341);
          if (_M0L6loggerS377.$1) {
            moonbit_incref(_M0L6loggerS377.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 125);
          _M0L6_2atmpS1343 = _M0L1iS381 + 1;
          _M0L6_2atmpS1344 = _M0L1iS381 + 1;
          _M0L1iS381 = _M0L6_2atmpS1343;
          _M0L3segS382 = _M0L6_2atmpS1344;
          goto _2afor_383;
        } else {
          int32_t _M0L6_2atmpS1345 = _M0L1iS381 + 1;
          int32_t _tmp_2149 = _M0L3segS382;
          _M0L1iS381 = _M0L6_2atmpS1345;
          _M0L3segS382 = _tmp_2149;
          goto _2afor_383;
        }
        break;
      }
    }
    goto joinlet_2148;
    join_385:;
    moonbit_incref(_M0L6_2aenvS378);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
    if (_M0L6loggerS377.$1) {
      moonbit_incref(_M0L6loggerS377.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1330 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS386);
    if (_M0L6loggerS377.$1) {
      moonbit_incref(_M0L6loggerS377.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, _M0L6_2atmpS1330);
    _M0L6_2atmpS1331 = _M0L1iS381 + 1;
    _M0L6_2atmpS1332 = _M0L1iS381 + 1;
    _M0L1iS381 = _M0L6_2atmpS1331;
    _M0L3segS382 = _M0L6_2atmpS1332;
    continue;
    joinlet_2148:;
    _tmp_2150 = _M0L1iS381;
    _tmp_2151 = _M0L3segS382;
    _M0L1iS381 = _tmp_2150;
    _M0L3segS382 = _tmp_2151;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS373,
  int32_t _M0L3segS376,
  int32_t _M0L1iS375
) {
  struct _M0TPB6Logger _M0L8_2afieldS1934;
  struct _M0TPB6Logger _M0L6loggerS372;
  moonbit_string_t _M0L8_2afieldS1933;
  int32_t _M0L6_2acntS2059;
  moonbit_string_t _M0L4selfS374;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS1934
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS373->$1_0, _M0L6_2aenvS373->$1_1
  };
  _M0L6loggerS372 = _M0L8_2afieldS1934;
  _M0L8_2afieldS1933 = _M0L6_2aenvS373->$0;
  _M0L6_2acntS2059 = Moonbit_object_header(_M0L6_2aenvS373)->rc;
  if (_M0L6_2acntS2059 > 1) {
    int32_t _M0L11_2anew__cntS2060 = _M0L6_2acntS2059 - 1;
    Moonbit_object_header(_M0L6_2aenvS373)->rc = _M0L11_2anew__cntS2060;
    if (_M0L6loggerS372.$1) {
      moonbit_incref(_M0L6loggerS372.$1);
    }
    moonbit_incref(_M0L8_2afieldS1933);
  } else if (_M0L6_2acntS2059 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS373);
  }
  _M0L4selfS374 = _M0L8_2afieldS1933;
  if (_M0L1iS375 > _M0L3segS376) {
    int32_t _M0L6_2atmpS1329 = _M0L1iS375 - _M0L3segS376;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS372.$0->$method_1(_M0L6loggerS372.$1, _M0L4selfS374, _M0L3segS376, _M0L6_2atmpS1329);
  } else {
    moonbit_decref(_M0L4selfS374);
    if (_M0L6loggerS372.$1) {
      moonbit_decref(_M0L6loggerS372.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS371) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS370;
  int32_t _M0L6_2atmpS1326;
  int32_t _M0L6_2atmpS1325;
  int32_t _M0L6_2atmpS1328;
  int32_t _M0L6_2atmpS1327;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1324;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS370 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1326 = _M0IPC14byte4BytePB3Div3div(_M0L1bS371, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1325
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1326);
  moonbit_incref(_M0L7_2aselfS370);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS370, _M0L6_2atmpS1325);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1328 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS371, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1327
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1328);
  moonbit_incref(_M0L7_2aselfS370);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS370, _M0L6_2atmpS1327);
  _M0L6_2atmpS1324 = _M0L7_2aselfS370;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1324);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS369) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS369 < 10) {
    int32_t _M0L6_2atmpS1321;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1321 = _M0IPC14byte4BytePB3Add3add(_M0L1iS369, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1321);
  } else {
    int32_t _M0L6_2atmpS1323;
    int32_t _M0L6_2atmpS1322;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1323 = _M0IPC14byte4BytePB3Add3add(_M0L1iS369, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1322 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1323, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1322);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS367,
  int32_t _M0L4thatS368
) {
  int32_t _M0L6_2atmpS1319;
  int32_t _M0L6_2atmpS1320;
  int32_t _M0L6_2atmpS1318;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1319 = (int32_t)_M0L4selfS367;
  _M0L6_2atmpS1320 = (int32_t)_M0L4thatS368;
  _M0L6_2atmpS1318 = _M0L6_2atmpS1319 - _M0L6_2atmpS1320;
  return _M0L6_2atmpS1318 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS365,
  int32_t _M0L4thatS366
) {
  int32_t _M0L6_2atmpS1316;
  int32_t _M0L6_2atmpS1317;
  int32_t _M0L6_2atmpS1315;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1316 = (int32_t)_M0L4selfS365;
  _M0L6_2atmpS1317 = (int32_t)_M0L4thatS366;
  _M0L6_2atmpS1315 = _M0L6_2atmpS1316 % _M0L6_2atmpS1317;
  return _M0L6_2atmpS1315 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS363,
  int32_t _M0L4thatS364
) {
  int32_t _M0L6_2atmpS1313;
  int32_t _M0L6_2atmpS1314;
  int32_t _M0L6_2atmpS1312;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1313 = (int32_t)_M0L4selfS363;
  _M0L6_2atmpS1314 = (int32_t)_M0L4thatS364;
  _M0L6_2atmpS1312 = _M0L6_2atmpS1313 / _M0L6_2atmpS1314;
  return _M0L6_2atmpS1312 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS361,
  int32_t _M0L4thatS362
) {
  int32_t _M0L6_2atmpS1310;
  int32_t _M0L6_2atmpS1311;
  int32_t _M0L6_2atmpS1309;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1310 = (int32_t)_M0L4selfS361;
  _M0L6_2atmpS1311 = (int32_t)_M0L4thatS362;
  _M0L6_2atmpS1309 = _M0L6_2atmpS1310 + _M0L6_2atmpS1311;
  return _M0L6_2atmpS1309 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS358,
  int32_t _M0L5startS356,
  int32_t _M0L3endS357
) {
  int32_t _if__result_2152;
  int32_t _M0L3lenS359;
  int32_t _M0L6_2atmpS1307;
  int32_t _M0L6_2atmpS1308;
  moonbit_bytes_t _M0L5bytesS360;
  moonbit_bytes_t _M0L6_2atmpS1306;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS356 == 0) {
    int32_t _M0L6_2atmpS1305 = Moonbit_array_length(_M0L3strS358);
    _if__result_2152 = _M0L3endS357 == _M0L6_2atmpS1305;
  } else {
    _if__result_2152 = 0;
  }
  if (_if__result_2152) {
    return _M0L3strS358;
  }
  _M0L3lenS359 = _M0L3endS357 - _M0L5startS356;
  _M0L6_2atmpS1307 = _M0L3lenS359 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1308 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS360
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1307, _M0L6_2atmpS1308);
  moonbit_incref(_M0L5bytesS360);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS360, 0, _M0L3strS358, _M0L5startS356, _M0L3lenS359);
  _M0L6_2atmpS1306 = _M0L5bytesS360;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1306, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS355) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS355;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS339,
  int32_t _M0L5radixS338
) {
  int32_t _if__result_2153;
  int32_t _M0L12is__negativeS340;
  uint32_t _M0L3numS341;
  uint16_t* _M0L6bufferS342;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS338 < 2) {
    _if__result_2153 = 1;
  } else {
    _if__result_2153 = _M0L5radixS338 > 36;
  }
  if (_if__result_2153) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_22.data, (moonbit_string_t)moonbit_string_literal_23.data);
  }
  if (_M0L4selfS339 == 0) {
    return (moonbit_string_t)moonbit_string_literal_24.data;
  }
  _M0L12is__negativeS340 = _M0L4selfS339 < 0;
  if (_M0L12is__negativeS340) {
    int32_t _M0L6_2atmpS1304 = -_M0L4selfS339;
    _M0L3numS341 = *(uint32_t*)&_M0L6_2atmpS1304;
  } else {
    _M0L3numS341 = *(uint32_t*)&_M0L4selfS339;
  }
  switch (_M0L5radixS338) {
    case 10: {
      int32_t _M0L10digit__lenS343;
      int32_t _M0L6_2atmpS1301;
      int32_t _M0L10total__lenS344;
      uint16_t* _M0L6bufferS345;
      int32_t _M0L12digit__startS346;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS343 = _M0FPB12dec__count32(_M0L3numS341);
      if (_M0L12is__negativeS340) {
        _M0L6_2atmpS1301 = 1;
      } else {
        _M0L6_2atmpS1301 = 0;
      }
      _M0L10total__lenS344 = _M0L10digit__lenS343 + _M0L6_2atmpS1301;
      _M0L6bufferS345
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS344, 0);
      if (_M0L12is__negativeS340) {
        _M0L12digit__startS346 = 1;
      } else {
        _M0L12digit__startS346 = 0;
      }
      moonbit_incref(_M0L6bufferS345);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS345, _M0L3numS341, _M0L12digit__startS346, _M0L10total__lenS344);
      _M0L6bufferS342 = _M0L6bufferS345;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS347;
      int32_t _M0L6_2atmpS1302;
      int32_t _M0L10total__lenS348;
      uint16_t* _M0L6bufferS349;
      int32_t _M0L12digit__startS350;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS347 = _M0FPB12hex__count32(_M0L3numS341);
      if (_M0L12is__negativeS340) {
        _M0L6_2atmpS1302 = 1;
      } else {
        _M0L6_2atmpS1302 = 0;
      }
      _M0L10total__lenS348 = _M0L10digit__lenS347 + _M0L6_2atmpS1302;
      _M0L6bufferS349
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS348, 0);
      if (_M0L12is__negativeS340) {
        _M0L12digit__startS350 = 1;
      } else {
        _M0L12digit__startS350 = 0;
      }
      moonbit_incref(_M0L6bufferS349);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS349, _M0L3numS341, _M0L12digit__startS350, _M0L10total__lenS348);
      _M0L6bufferS342 = _M0L6bufferS349;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS351;
      int32_t _M0L6_2atmpS1303;
      int32_t _M0L10total__lenS352;
      uint16_t* _M0L6bufferS353;
      int32_t _M0L12digit__startS354;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS351
      = _M0FPB14radix__count32(_M0L3numS341, _M0L5radixS338);
      if (_M0L12is__negativeS340) {
        _M0L6_2atmpS1303 = 1;
      } else {
        _M0L6_2atmpS1303 = 0;
      }
      _M0L10total__lenS352 = _M0L10digit__lenS351 + _M0L6_2atmpS1303;
      _M0L6bufferS353
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS352, 0);
      if (_M0L12is__negativeS340) {
        _M0L12digit__startS354 = 1;
      } else {
        _M0L12digit__startS354 = 0;
      }
      moonbit_incref(_M0L6bufferS353);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS353, _M0L3numS341, _M0L12digit__startS354, _M0L10total__lenS352, _M0L5radixS338);
      _M0L6bufferS342 = _M0L6bufferS353;
      break;
    }
  }
  if (_M0L12is__negativeS340) {
    _M0L6bufferS342[0] = 45;
  }
  return _M0L6bufferS342;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS332,
  int32_t _M0L5radixS335
) {
  uint32_t _M0Lm3numS333;
  uint32_t _M0L4baseS334;
  int32_t _M0Lm5countS336;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS332 == 0u) {
    return 1;
  }
  _M0Lm3numS333 = _M0L5valueS332;
  _M0L4baseS334 = *(uint32_t*)&_M0L5radixS335;
  _M0Lm5countS336 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1298 = _M0Lm3numS333;
    if (_M0L6_2atmpS1298 > 0u) {
      int32_t _M0L6_2atmpS1299 = _M0Lm5countS336;
      uint32_t _M0L6_2atmpS1300;
      _M0Lm5countS336 = _M0L6_2atmpS1299 + 1;
      _M0L6_2atmpS1300 = _M0Lm3numS333;
      _M0Lm3numS333 = _M0L6_2atmpS1300 / _M0L4baseS334;
      continue;
    }
    break;
  }
  return _M0Lm5countS336;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS330) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS330 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS331;
    int32_t _M0L6_2atmpS1297;
    int32_t _M0L6_2atmpS1296;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS331 = moonbit_clz32(_M0L5valueS330);
    _M0L6_2atmpS1297 = 31 - _M0L14leading__zerosS331;
    _M0L6_2atmpS1296 = _M0L6_2atmpS1297 / 4;
    return _M0L6_2atmpS1296 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS329) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS329 >= 100000u) {
    if (_M0L5valueS329 >= 10000000u) {
      if (_M0L5valueS329 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS329 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS329 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS329 >= 1000u) {
    if (_M0L5valueS329 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS329 >= 100u) {
    return 3;
  } else if (_M0L5valueS329 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS319,
  uint32_t _M0L3numS307,
  int32_t _M0L12digit__startS310,
  int32_t _M0L10total__lenS309
) {
  uint32_t _M0Lm3numS306;
  int32_t _M0Lm6offsetS308;
  uint32_t _M0L6_2atmpS1295;
  int32_t _M0Lm9remainingS321;
  int32_t _M0L6_2atmpS1276;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS306 = _M0L3numS307;
  _M0Lm6offsetS308 = _M0L10total__lenS309 - _M0L12digit__startS310;
  while (1) {
    uint32_t _M0L6_2atmpS1239 = _M0Lm3numS306;
    if (_M0L6_2atmpS1239 >= 10000u) {
      uint32_t _M0L6_2atmpS1262 = _M0Lm3numS306;
      uint32_t _M0L1tS311 = _M0L6_2atmpS1262 / 10000u;
      uint32_t _M0L6_2atmpS1261 = _M0Lm3numS306;
      uint32_t _M0L6_2atmpS1260 = _M0L6_2atmpS1261 % 10000u;
      int32_t _M0L1rS312 = *(int32_t*)&_M0L6_2atmpS1260;
      int32_t _M0L2d1S313;
      int32_t _M0L2d2S314;
      int32_t _M0L6_2atmpS1240;
      int32_t _M0L6_2atmpS1259;
      int32_t _M0L6_2atmpS1258;
      int32_t _M0L6d1__hiS315;
      int32_t _M0L6_2atmpS1257;
      int32_t _M0L6_2atmpS1256;
      int32_t _M0L6d1__loS316;
      int32_t _M0L6_2atmpS1255;
      int32_t _M0L6_2atmpS1254;
      int32_t _M0L6d2__hiS317;
      int32_t _M0L6_2atmpS1253;
      int32_t _M0L6_2atmpS1252;
      int32_t _M0L6d2__loS318;
      int32_t _M0L6_2atmpS1242;
      int32_t _M0L6_2atmpS1241;
      int32_t _M0L6_2atmpS1245;
      int32_t _M0L6_2atmpS1244;
      int32_t _M0L6_2atmpS1243;
      int32_t _M0L6_2atmpS1248;
      int32_t _M0L6_2atmpS1247;
      int32_t _M0L6_2atmpS1246;
      int32_t _M0L6_2atmpS1251;
      int32_t _M0L6_2atmpS1250;
      int32_t _M0L6_2atmpS1249;
      _M0Lm3numS306 = _M0L1tS311;
      _M0L2d1S313 = _M0L1rS312 / 100;
      _M0L2d2S314 = _M0L1rS312 % 100;
      _M0L6_2atmpS1240 = _M0Lm6offsetS308;
      _M0Lm6offsetS308 = _M0L6_2atmpS1240 - 4;
      _M0L6_2atmpS1259 = _M0L2d1S313 / 10;
      _M0L6_2atmpS1258 = 48 + _M0L6_2atmpS1259;
      _M0L6d1__hiS315 = (uint16_t)_M0L6_2atmpS1258;
      _M0L6_2atmpS1257 = _M0L2d1S313 % 10;
      _M0L6_2atmpS1256 = 48 + _M0L6_2atmpS1257;
      _M0L6d1__loS316 = (uint16_t)_M0L6_2atmpS1256;
      _M0L6_2atmpS1255 = _M0L2d2S314 / 10;
      _M0L6_2atmpS1254 = 48 + _M0L6_2atmpS1255;
      _M0L6d2__hiS317 = (uint16_t)_M0L6_2atmpS1254;
      _M0L6_2atmpS1253 = _M0L2d2S314 % 10;
      _M0L6_2atmpS1252 = 48 + _M0L6_2atmpS1253;
      _M0L6d2__loS318 = (uint16_t)_M0L6_2atmpS1252;
      _M0L6_2atmpS1242 = _M0Lm6offsetS308;
      _M0L6_2atmpS1241 = _M0L12digit__startS310 + _M0L6_2atmpS1242;
      _M0L6bufferS319[_M0L6_2atmpS1241] = _M0L6d1__hiS315;
      _M0L6_2atmpS1245 = _M0Lm6offsetS308;
      _M0L6_2atmpS1244 = _M0L12digit__startS310 + _M0L6_2atmpS1245;
      _M0L6_2atmpS1243 = _M0L6_2atmpS1244 + 1;
      _M0L6bufferS319[_M0L6_2atmpS1243] = _M0L6d1__loS316;
      _M0L6_2atmpS1248 = _M0Lm6offsetS308;
      _M0L6_2atmpS1247 = _M0L12digit__startS310 + _M0L6_2atmpS1248;
      _M0L6_2atmpS1246 = _M0L6_2atmpS1247 + 2;
      _M0L6bufferS319[_M0L6_2atmpS1246] = _M0L6d2__hiS317;
      _M0L6_2atmpS1251 = _M0Lm6offsetS308;
      _M0L6_2atmpS1250 = _M0L12digit__startS310 + _M0L6_2atmpS1251;
      _M0L6_2atmpS1249 = _M0L6_2atmpS1250 + 3;
      _M0L6bufferS319[_M0L6_2atmpS1249] = _M0L6d2__loS318;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1295 = _M0Lm3numS306;
  _M0Lm9remainingS321 = *(int32_t*)&_M0L6_2atmpS1295;
  while (1) {
    int32_t _M0L6_2atmpS1263 = _M0Lm9remainingS321;
    if (_M0L6_2atmpS1263 >= 100) {
      int32_t _M0L6_2atmpS1275 = _M0Lm9remainingS321;
      int32_t _M0L1tS322 = _M0L6_2atmpS1275 / 100;
      int32_t _M0L6_2atmpS1274 = _M0Lm9remainingS321;
      int32_t _M0L1dS323 = _M0L6_2atmpS1274 % 100;
      int32_t _M0L6_2atmpS1264;
      int32_t _M0L6_2atmpS1273;
      int32_t _M0L6_2atmpS1272;
      int32_t _M0L5d__hiS324;
      int32_t _M0L6_2atmpS1271;
      int32_t _M0L6_2atmpS1270;
      int32_t _M0L5d__loS325;
      int32_t _M0L6_2atmpS1266;
      int32_t _M0L6_2atmpS1265;
      int32_t _M0L6_2atmpS1269;
      int32_t _M0L6_2atmpS1268;
      int32_t _M0L6_2atmpS1267;
      _M0Lm9remainingS321 = _M0L1tS322;
      _M0L6_2atmpS1264 = _M0Lm6offsetS308;
      _M0Lm6offsetS308 = _M0L6_2atmpS1264 - 2;
      _M0L6_2atmpS1273 = _M0L1dS323 / 10;
      _M0L6_2atmpS1272 = 48 + _M0L6_2atmpS1273;
      _M0L5d__hiS324 = (uint16_t)_M0L6_2atmpS1272;
      _M0L6_2atmpS1271 = _M0L1dS323 % 10;
      _M0L6_2atmpS1270 = 48 + _M0L6_2atmpS1271;
      _M0L5d__loS325 = (uint16_t)_M0L6_2atmpS1270;
      _M0L6_2atmpS1266 = _M0Lm6offsetS308;
      _M0L6_2atmpS1265 = _M0L12digit__startS310 + _M0L6_2atmpS1266;
      _M0L6bufferS319[_M0L6_2atmpS1265] = _M0L5d__hiS324;
      _M0L6_2atmpS1269 = _M0Lm6offsetS308;
      _M0L6_2atmpS1268 = _M0L12digit__startS310 + _M0L6_2atmpS1269;
      _M0L6_2atmpS1267 = _M0L6_2atmpS1268 + 1;
      _M0L6bufferS319[_M0L6_2atmpS1267] = _M0L5d__loS325;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1276 = _M0Lm9remainingS321;
  if (_M0L6_2atmpS1276 >= 10) {
    int32_t _M0L6_2atmpS1277 = _M0Lm6offsetS308;
    int32_t _M0L6_2atmpS1288;
    int32_t _M0L6_2atmpS1287;
    int32_t _M0L6_2atmpS1286;
    int32_t _M0L5d__hiS327;
    int32_t _M0L6_2atmpS1285;
    int32_t _M0L6_2atmpS1284;
    int32_t _M0L6_2atmpS1283;
    int32_t _M0L5d__loS328;
    int32_t _M0L6_2atmpS1279;
    int32_t _M0L6_2atmpS1278;
    int32_t _M0L6_2atmpS1282;
    int32_t _M0L6_2atmpS1281;
    int32_t _M0L6_2atmpS1280;
    _M0Lm6offsetS308 = _M0L6_2atmpS1277 - 2;
    _M0L6_2atmpS1288 = _M0Lm9remainingS321;
    _M0L6_2atmpS1287 = _M0L6_2atmpS1288 / 10;
    _M0L6_2atmpS1286 = 48 + _M0L6_2atmpS1287;
    _M0L5d__hiS327 = (uint16_t)_M0L6_2atmpS1286;
    _M0L6_2atmpS1285 = _M0Lm9remainingS321;
    _M0L6_2atmpS1284 = _M0L6_2atmpS1285 % 10;
    _M0L6_2atmpS1283 = 48 + _M0L6_2atmpS1284;
    _M0L5d__loS328 = (uint16_t)_M0L6_2atmpS1283;
    _M0L6_2atmpS1279 = _M0Lm6offsetS308;
    _M0L6_2atmpS1278 = _M0L12digit__startS310 + _M0L6_2atmpS1279;
    _M0L6bufferS319[_M0L6_2atmpS1278] = _M0L5d__hiS327;
    _M0L6_2atmpS1282 = _M0Lm6offsetS308;
    _M0L6_2atmpS1281 = _M0L12digit__startS310 + _M0L6_2atmpS1282;
    _M0L6_2atmpS1280 = _M0L6_2atmpS1281 + 1;
    _M0L6bufferS319[_M0L6_2atmpS1280] = _M0L5d__loS328;
    moonbit_decref(_M0L6bufferS319);
  } else {
    int32_t _M0L6_2atmpS1289 = _M0Lm6offsetS308;
    int32_t _M0L6_2atmpS1294;
    int32_t _M0L6_2atmpS1290;
    int32_t _M0L6_2atmpS1293;
    int32_t _M0L6_2atmpS1292;
    int32_t _M0L6_2atmpS1291;
    _M0Lm6offsetS308 = _M0L6_2atmpS1289 - 1;
    _M0L6_2atmpS1294 = _M0Lm6offsetS308;
    _M0L6_2atmpS1290 = _M0L12digit__startS310 + _M0L6_2atmpS1294;
    _M0L6_2atmpS1293 = _M0Lm9remainingS321;
    _M0L6_2atmpS1292 = 48 + _M0L6_2atmpS1293;
    _M0L6_2atmpS1291 = (uint16_t)_M0L6_2atmpS1292;
    _M0L6bufferS319[_M0L6_2atmpS1290] = _M0L6_2atmpS1291;
    moonbit_decref(_M0L6bufferS319);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS301,
  uint32_t _M0L3numS295,
  int32_t _M0L12digit__startS293,
  int32_t _M0L10total__lenS292,
  int32_t _M0L5radixS297
) {
  int32_t _M0Lm6offsetS291;
  uint32_t _M0Lm1nS294;
  uint32_t _M0L4baseS296;
  int32_t _M0L6_2atmpS1221;
  int32_t _M0L6_2atmpS1220;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS291 = _M0L10total__lenS292 - _M0L12digit__startS293;
  _M0Lm1nS294 = _M0L3numS295;
  _M0L4baseS296 = *(uint32_t*)&_M0L5radixS297;
  _M0L6_2atmpS1221 = _M0L5radixS297 - 1;
  _M0L6_2atmpS1220 = _M0L5radixS297 & _M0L6_2atmpS1221;
  if (_M0L6_2atmpS1220 == 0) {
    int32_t _M0L5shiftS298;
    uint32_t _M0L4maskS299;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS298 = moonbit_ctz32(_M0L5radixS297);
    _M0L4maskS299 = _M0L4baseS296 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1222 = _M0Lm1nS294;
      if (_M0L6_2atmpS1222 > 0u) {
        int32_t _M0L6_2atmpS1223 = _M0Lm6offsetS291;
        uint32_t _M0L6_2atmpS1229;
        uint32_t _M0L6_2atmpS1228;
        int32_t _M0L5digitS300;
        int32_t _M0L6_2atmpS1226;
        int32_t _M0L6_2atmpS1224;
        int32_t _M0L6_2atmpS1225;
        uint32_t _M0L6_2atmpS1227;
        _M0Lm6offsetS291 = _M0L6_2atmpS1223 - 1;
        _M0L6_2atmpS1229 = _M0Lm1nS294;
        _M0L6_2atmpS1228 = _M0L6_2atmpS1229 & _M0L4maskS299;
        _M0L5digitS300 = *(int32_t*)&_M0L6_2atmpS1228;
        _M0L6_2atmpS1226 = _M0Lm6offsetS291;
        _M0L6_2atmpS1224 = _M0L12digit__startS293 + _M0L6_2atmpS1226;
        _M0L6_2atmpS1225
        = ((moonbit_string_t)moonbit_string_literal_25.data)[
          _M0L5digitS300
        ];
        _M0L6bufferS301[_M0L6_2atmpS1224] = _M0L6_2atmpS1225;
        _M0L6_2atmpS1227 = _M0Lm1nS294;
        _M0Lm1nS294 = _M0L6_2atmpS1227 >> (_M0L5shiftS298 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS301);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1230 = _M0Lm1nS294;
      if (_M0L6_2atmpS1230 > 0u) {
        int32_t _M0L6_2atmpS1231 = _M0Lm6offsetS291;
        uint32_t _M0L6_2atmpS1238;
        uint32_t _M0L1qS303;
        uint32_t _M0L6_2atmpS1236;
        uint32_t _M0L6_2atmpS1237;
        uint32_t _M0L6_2atmpS1235;
        int32_t _M0L5digitS304;
        int32_t _M0L6_2atmpS1234;
        int32_t _M0L6_2atmpS1232;
        int32_t _M0L6_2atmpS1233;
        _M0Lm6offsetS291 = _M0L6_2atmpS1231 - 1;
        _M0L6_2atmpS1238 = _M0Lm1nS294;
        _M0L1qS303 = _M0L6_2atmpS1238 / _M0L4baseS296;
        _M0L6_2atmpS1236 = _M0Lm1nS294;
        _M0L6_2atmpS1237 = _M0L1qS303 * _M0L4baseS296;
        _M0L6_2atmpS1235 = _M0L6_2atmpS1236 - _M0L6_2atmpS1237;
        _M0L5digitS304 = *(int32_t*)&_M0L6_2atmpS1235;
        _M0L6_2atmpS1234 = _M0Lm6offsetS291;
        _M0L6_2atmpS1232 = _M0L12digit__startS293 + _M0L6_2atmpS1234;
        _M0L6_2atmpS1233
        = ((moonbit_string_t)moonbit_string_literal_25.data)[
          _M0L5digitS304
        ];
        _M0L6bufferS301[_M0L6_2atmpS1232] = _M0L6_2atmpS1233;
        _M0Lm1nS294 = _M0L1qS303;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS301);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS288,
  uint32_t _M0L3numS284,
  int32_t _M0L12digit__startS282,
  int32_t _M0L10total__lenS281
) {
  int32_t _M0Lm6offsetS280;
  uint32_t _M0Lm1nS283;
  int32_t _M0L6_2atmpS1216;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS280 = _M0L10total__lenS281 - _M0L12digit__startS282;
  _M0Lm1nS283 = _M0L3numS284;
  while (1) {
    int32_t _M0L6_2atmpS1204 = _M0Lm6offsetS280;
    if (_M0L6_2atmpS1204 >= 2) {
      int32_t _M0L6_2atmpS1205 = _M0Lm6offsetS280;
      uint32_t _M0L6_2atmpS1215;
      uint32_t _M0L6_2atmpS1214;
      int32_t _M0L9byte__valS285;
      int32_t _M0L2hiS286;
      int32_t _M0L2loS287;
      int32_t _M0L6_2atmpS1208;
      int32_t _M0L6_2atmpS1206;
      int32_t _M0L6_2atmpS1207;
      int32_t _M0L6_2atmpS1212;
      int32_t _M0L6_2atmpS1211;
      int32_t _M0L6_2atmpS1209;
      int32_t _M0L6_2atmpS1210;
      uint32_t _M0L6_2atmpS1213;
      _M0Lm6offsetS280 = _M0L6_2atmpS1205 - 2;
      _M0L6_2atmpS1215 = _M0Lm1nS283;
      _M0L6_2atmpS1214 = _M0L6_2atmpS1215 & 255u;
      _M0L9byte__valS285 = *(int32_t*)&_M0L6_2atmpS1214;
      _M0L2hiS286 = _M0L9byte__valS285 / 16;
      _M0L2loS287 = _M0L9byte__valS285 % 16;
      _M0L6_2atmpS1208 = _M0Lm6offsetS280;
      _M0L6_2atmpS1206 = _M0L12digit__startS282 + _M0L6_2atmpS1208;
      _M0L6_2atmpS1207
      = ((moonbit_string_t)moonbit_string_literal_25.data)[
        _M0L2hiS286
      ];
      _M0L6bufferS288[_M0L6_2atmpS1206] = _M0L6_2atmpS1207;
      _M0L6_2atmpS1212 = _M0Lm6offsetS280;
      _M0L6_2atmpS1211 = _M0L12digit__startS282 + _M0L6_2atmpS1212;
      _M0L6_2atmpS1209 = _M0L6_2atmpS1211 + 1;
      _M0L6_2atmpS1210
      = ((moonbit_string_t)moonbit_string_literal_25.data)[
        _M0L2loS287
      ];
      _M0L6bufferS288[_M0L6_2atmpS1209] = _M0L6_2atmpS1210;
      _M0L6_2atmpS1213 = _M0Lm1nS283;
      _M0Lm1nS283 = _M0L6_2atmpS1213 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1216 = _M0Lm6offsetS280;
  if (_M0L6_2atmpS1216 == 1) {
    uint32_t _M0L6_2atmpS1219 = _M0Lm1nS283;
    uint32_t _M0L6_2atmpS1218 = _M0L6_2atmpS1219 & 15u;
    int32_t _M0L6nibbleS290 = *(int32_t*)&_M0L6_2atmpS1218;
    int32_t _M0L6_2atmpS1217 =
      ((moonbit_string_t)moonbit_string_literal_25.data)[_M0L6nibbleS290];
    _M0L6bufferS288[_M0L12digit__startS282] = _M0L6_2atmpS1217;
    moonbit_decref(_M0L6bufferS288);
  } else {
    moonbit_decref(_M0L6bufferS288);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS279) {
  struct _M0TWEOs* _M0L7_2afuncS278;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS278 = _M0L4selfS279;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS278->code(_M0L7_2afuncS278);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS273
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS272;
  struct _M0TPB6Logger _M0L6_2atmpS1201;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS272 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS272);
  _M0L6_2atmpS1201
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS272
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS273, _M0L6_2atmpS1201);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS272);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS275
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS274;
  struct _M0TPB6Logger _M0L6_2atmpS1202;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS274 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS274);
  _M0L6_2atmpS1202
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS274
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS275, _M0L6_2atmpS1202);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS274);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS277
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS276;
  struct _M0TPB6Logger _M0L6_2atmpS1203;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS276 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS276);
  _M0L6_2atmpS1203
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS276
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS277, _M0L6_2atmpS1203);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS276);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS271
) {
  int32_t _M0L8_2afieldS1935;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1935 = _M0L4selfS271.$1;
  moonbit_decref(_M0L4selfS271.$0);
  return _M0L8_2afieldS1935;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS270
) {
  int32_t _M0L3endS1199;
  int32_t _M0L8_2afieldS1936;
  int32_t _M0L5startS1200;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1199 = _M0L4selfS270.$2;
  _M0L8_2afieldS1936 = _M0L4selfS270.$1;
  moonbit_decref(_M0L4selfS270.$0);
  _M0L5startS1200 = _M0L8_2afieldS1936;
  return _M0L3endS1199 - _M0L5startS1200;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS269
) {
  moonbit_string_t _M0L8_2afieldS1937;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1937 = _M0L4selfS269.$0;
  return _M0L8_2afieldS1937;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS265,
  moonbit_string_t _M0L5valueS266,
  int32_t _M0L5startS267,
  int32_t _M0L3lenS268
) {
  int32_t _M0L6_2atmpS1198;
  int64_t _M0L6_2atmpS1197;
  struct _M0TPC16string10StringView _M0L6_2atmpS1196;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1198 = _M0L5startS267 + _M0L3lenS268;
  _M0L6_2atmpS1197 = (int64_t)_M0L6_2atmpS1198;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1196
  = _M0MPC16string6String11sub_2einner(_M0L5valueS266, _M0L5startS267, _M0L6_2atmpS1197);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS265, _M0L6_2atmpS1196);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS258,
  int32_t _M0L5startS264,
  int64_t _M0L3endS260
) {
  int32_t _M0L3lenS257;
  int32_t _M0L3endS259;
  int32_t _M0L5startS263;
  int32_t _if__result_2160;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS257 = Moonbit_array_length(_M0L4selfS258);
  if (_M0L3endS260 == 4294967296ll) {
    _M0L3endS259 = _M0L3lenS257;
  } else {
    int64_t _M0L7_2aSomeS261 = _M0L3endS260;
    int32_t _M0L6_2aendS262 = (int32_t)_M0L7_2aSomeS261;
    if (_M0L6_2aendS262 < 0) {
      _M0L3endS259 = _M0L3lenS257 + _M0L6_2aendS262;
    } else {
      _M0L3endS259 = _M0L6_2aendS262;
    }
  }
  if (_M0L5startS264 < 0) {
    _M0L5startS263 = _M0L3lenS257 + _M0L5startS264;
  } else {
    _M0L5startS263 = _M0L5startS264;
  }
  if (_M0L5startS263 >= 0) {
    if (_M0L5startS263 <= _M0L3endS259) {
      _if__result_2160 = _M0L3endS259 <= _M0L3lenS257;
    } else {
      _if__result_2160 = 0;
    }
  } else {
    _if__result_2160 = 0;
  }
  if (_if__result_2160) {
    if (_M0L5startS263 < _M0L3lenS257) {
      int32_t _M0L6_2atmpS1193 = _M0L4selfS258[_M0L5startS263];
      int32_t _M0L6_2atmpS1192;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1192
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1193);
      if (!_M0L6_2atmpS1192) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS259 < _M0L3lenS257) {
      int32_t _M0L6_2atmpS1195 = _M0L4selfS258[_M0L3endS259];
      int32_t _M0L6_2atmpS1194;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1194
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1195);
      if (!_M0L6_2atmpS1194) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS263,
                                                 _M0L3endS259,
                                                 _M0L4selfS258};
  } else {
    moonbit_decref(_M0L4selfS258);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS254) {
  struct _M0TPB6Hasher* _M0L1hS253;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS253 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS253);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS253, _M0L4selfS254);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS253);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS256
) {
  struct _M0TPB6Hasher* _M0L1hS255;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS255 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS255);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS255, _M0L4selfS256);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS255);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS251) {
  int32_t _M0L4seedS250;
  if (_M0L10seed_2eoptS251 == 4294967296ll) {
    _M0L4seedS250 = 0;
  } else {
    int64_t _M0L7_2aSomeS252 = _M0L10seed_2eoptS251;
    _M0L4seedS250 = (int32_t)_M0L7_2aSomeS252;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS250);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS249) {
  uint32_t _M0L6_2atmpS1191;
  uint32_t _M0L6_2atmpS1190;
  struct _M0TPB6Hasher* _block_2161;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1191 = *(uint32_t*)&_M0L4seedS249;
  _M0L6_2atmpS1190 = _M0L6_2atmpS1191 + 374761393u;
  _block_2161
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2161)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2161->$0 = _M0L6_2atmpS1190;
  return _block_2161;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS248) {
  uint32_t _M0L6_2atmpS1189;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1189 = _M0MPB6Hasher9avalanche(_M0L4selfS248);
  return *(int32_t*)&_M0L6_2atmpS1189;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS247) {
  uint32_t _M0L8_2afieldS1938;
  uint32_t _M0Lm3accS246;
  uint32_t _M0L6_2atmpS1178;
  uint32_t _M0L6_2atmpS1180;
  uint32_t _M0L6_2atmpS1179;
  uint32_t _M0L6_2atmpS1181;
  uint32_t _M0L6_2atmpS1182;
  uint32_t _M0L6_2atmpS1184;
  uint32_t _M0L6_2atmpS1183;
  uint32_t _M0L6_2atmpS1185;
  uint32_t _M0L6_2atmpS1186;
  uint32_t _M0L6_2atmpS1188;
  uint32_t _M0L6_2atmpS1187;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS1938 = _M0L4selfS247->$0;
  moonbit_decref(_M0L4selfS247);
  _M0Lm3accS246 = _M0L8_2afieldS1938;
  _M0L6_2atmpS1178 = _M0Lm3accS246;
  _M0L6_2atmpS1180 = _M0Lm3accS246;
  _M0L6_2atmpS1179 = _M0L6_2atmpS1180 >> 15;
  _M0Lm3accS246 = _M0L6_2atmpS1178 ^ _M0L6_2atmpS1179;
  _M0L6_2atmpS1181 = _M0Lm3accS246;
  _M0Lm3accS246 = _M0L6_2atmpS1181 * 2246822519u;
  _M0L6_2atmpS1182 = _M0Lm3accS246;
  _M0L6_2atmpS1184 = _M0Lm3accS246;
  _M0L6_2atmpS1183 = _M0L6_2atmpS1184 >> 13;
  _M0Lm3accS246 = _M0L6_2atmpS1182 ^ _M0L6_2atmpS1183;
  _M0L6_2atmpS1185 = _M0Lm3accS246;
  _M0Lm3accS246 = _M0L6_2atmpS1185 * 3266489917u;
  _M0L6_2atmpS1186 = _M0Lm3accS246;
  _M0L6_2atmpS1188 = _M0Lm3accS246;
  _M0L6_2atmpS1187 = _M0L6_2atmpS1188 >> 16;
  _M0Lm3accS246 = _M0L6_2atmpS1186 ^ _M0L6_2atmpS1187;
  return _M0Lm3accS246;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS244,
  moonbit_string_t _M0L1yS245
) {
  int32_t _M0L6_2atmpS1939;
  int32_t _M0L6_2atmpS1177;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1939 = moonbit_val_array_equal(_M0L1xS244, _M0L1yS245);
  moonbit_decref(_M0L1xS244);
  moonbit_decref(_M0L1yS245);
  _M0L6_2atmpS1177 = _M0L6_2atmpS1939;
  return !_M0L6_2atmpS1177;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS241,
  int32_t _M0L5valueS240
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS240, _M0L4selfS241);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS243,
  moonbit_string_t _M0L5valueS242
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS242, _M0L4selfS243);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS238,
  int32_t _M0L5valueS239
) {
  uint32_t _M0L6_2atmpS1176;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1176 = *(uint32_t*)&_M0L5valueS239;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS238, _M0L6_2atmpS1176);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS228,
  moonbit_string_t _M0L7contentS229,
  moonbit_string_t _M0L3locS231,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS233
) {
  moonbit_string_t _M0L6actualS227;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6actualS227 = _M0L3objS228.$0->$method_1(_M0L3objS228.$1);
  moonbit_incref(_M0L7contentS229);
  moonbit_incref(_M0L6actualS227);
  #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS227, _M0L7contentS229)
  ) {
    moonbit_string_t _M0L3locS230;
    moonbit_string_t _M0L9args__locS232;
    moonbit_string_t _M0L15expect__escapedS234;
    moonbit_string_t _M0L15actual__escapedS235;
    moonbit_string_t _M0L6_2atmpS1174;
    moonbit_string_t _M0L6_2atmpS1173;
    moonbit_string_t _M0L6_2atmpS1955;
    moonbit_string_t _M0L6_2atmpS1172;
    moonbit_string_t _M0L6_2atmpS1954;
    moonbit_string_t _M0L14expect__base64S236;
    moonbit_string_t _M0L6_2atmpS1171;
    moonbit_string_t _M0L6_2atmpS1170;
    moonbit_string_t _M0L6_2atmpS1953;
    moonbit_string_t _M0L6_2atmpS1169;
    moonbit_string_t _M0L6_2atmpS1952;
    moonbit_string_t _M0L14actual__base64S237;
    moonbit_string_t _M0L6_2atmpS1168;
    moonbit_string_t _M0L6_2atmpS1951;
    moonbit_string_t _M0L6_2atmpS1167;
    moonbit_string_t _M0L6_2atmpS1950;
    moonbit_string_t _M0L6_2atmpS1165;
    moonbit_string_t _M0L6_2atmpS1166;
    moonbit_string_t _M0L6_2atmpS1949;
    moonbit_string_t _M0L6_2atmpS1164;
    moonbit_string_t _M0L6_2atmpS1948;
    moonbit_string_t _M0L6_2atmpS1162;
    moonbit_string_t _M0L6_2atmpS1163;
    moonbit_string_t _M0L6_2atmpS1947;
    moonbit_string_t _M0L6_2atmpS1161;
    moonbit_string_t _M0L6_2atmpS1946;
    moonbit_string_t _M0L6_2atmpS1159;
    moonbit_string_t _M0L6_2atmpS1160;
    moonbit_string_t _M0L6_2atmpS1945;
    moonbit_string_t _M0L6_2atmpS1158;
    moonbit_string_t _M0L6_2atmpS1944;
    moonbit_string_t _M0L6_2atmpS1156;
    moonbit_string_t _M0L6_2atmpS1157;
    moonbit_string_t _M0L6_2atmpS1943;
    moonbit_string_t _M0L6_2atmpS1155;
    moonbit_string_t _M0L6_2atmpS1942;
    moonbit_string_t _M0L6_2atmpS1153;
    moonbit_string_t _M0L6_2atmpS1154;
    moonbit_string_t _M0L6_2atmpS1941;
    moonbit_string_t _M0L6_2atmpS1152;
    moonbit_string_t _M0L6_2atmpS1940;
    moonbit_string_t _M0L6_2atmpS1151;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1150;
    struct moonbit_result_0 _result_2162;
    #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L3locS230 = _M0MPB9SourceLoc16to__json__string(_M0L3locS231);
    #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L9args__locS232 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS233);
    moonbit_incref(_M0L7contentS229);
    #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15expect__escapedS234
    = _M0MPC16string6String6escape(_M0L7contentS229);
    moonbit_incref(_M0L6actualS227);
    #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15actual__escapedS235 = _M0MPC16string6String6escape(_M0L6actualS227);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1174
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS229);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1173
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1174);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1955
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS1173);
    moonbit_decref(_M0L6_2atmpS1173);
    _M0L6_2atmpS1172 = _M0L6_2atmpS1955;
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1954
    = moonbit_add_string(_M0L6_2atmpS1172, (moonbit_string_t)moonbit_string_literal_26.data);
    moonbit_decref(_M0L6_2atmpS1172);
    _M0L14expect__base64S236 = _M0L6_2atmpS1954;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1171
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS227);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1170
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1171);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1953
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS1170);
    moonbit_decref(_M0L6_2atmpS1170);
    _M0L6_2atmpS1169 = _M0L6_2atmpS1953;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1952
    = moonbit_add_string(_M0L6_2atmpS1169, (moonbit_string_t)moonbit_string_literal_26.data);
    moonbit_decref(_M0L6_2atmpS1169);
    _M0L14actual__base64S237 = _M0L6_2atmpS1952;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1168 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS230);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1951
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS1168);
    moonbit_decref(_M0L6_2atmpS1168);
    _M0L6_2atmpS1167 = _M0L6_2atmpS1951;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1950
    = moonbit_add_string(_M0L6_2atmpS1167, (moonbit_string_t)moonbit_string_literal_28.data);
    moonbit_decref(_M0L6_2atmpS1167);
    _M0L6_2atmpS1165 = _M0L6_2atmpS1950;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1166
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS232);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1949 = moonbit_add_string(_M0L6_2atmpS1165, _M0L6_2atmpS1166);
    moonbit_decref(_M0L6_2atmpS1165);
    moonbit_decref(_M0L6_2atmpS1166);
    _M0L6_2atmpS1164 = _M0L6_2atmpS1949;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1948
    = moonbit_add_string(_M0L6_2atmpS1164, (moonbit_string_t)moonbit_string_literal_29.data);
    moonbit_decref(_M0L6_2atmpS1164);
    _M0L6_2atmpS1162 = _M0L6_2atmpS1948;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1163
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS234);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1947 = moonbit_add_string(_M0L6_2atmpS1162, _M0L6_2atmpS1163);
    moonbit_decref(_M0L6_2atmpS1162);
    moonbit_decref(_M0L6_2atmpS1163);
    _M0L6_2atmpS1161 = _M0L6_2atmpS1947;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1946
    = moonbit_add_string(_M0L6_2atmpS1161, (moonbit_string_t)moonbit_string_literal_30.data);
    moonbit_decref(_M0L6_2atmpS1161);
    _M0L6_2atmpS1159 = _M0L6_2atmpS1946;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1160
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS235);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1945 = moonbit_add_string(_M0L6_2atmpS1159, _M0L6_2atmpS1160);
    moonbit_decref(_M0L6_2atmpS1159);
    moonbit_decref(_M0L6_2atmpS1160);
    _M0L6_2atmpS1158 = _M0L6_2atmpS1945;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1944
    = moonbit_add_string(_M0L6_2atmpS1158, (moonbit_string_t)moonbit_string_literal_31.data);
    moonbit_decref(_M0L6_2atmpS1158);
    _M0L6_2atmpS1156 = _M0L6_2atmpS1944;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1157
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S236);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1943 = moonbit_add_string(_M0L6_2atmpS1156, _M0L6_2atmpS1157);
    moonbit_decref(_M0L6_2atmpS1156);
    moonbit_decref(_M0L6_2atmpS1157);
    _M0L6_2atmpS1155 = _M0L6_2atmpS1943;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1942
    = moonbit_add_string(_M0L6_2atmpS1155, (moonbit_string_t)moonbit_string_literal_32.data);
    moonbit_decref(_M0L6_2atmpS1155);
    _M0L6_2atmpS1153 = _M0L6_2atmpS1942;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1154
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S237);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1941 = moonbit_add_string(_M0L6_2atmpS1153, _M0L6_2atmpS1154);
    moonbit_decref(_M0L6_2atmpS1153);
    moonbit_decref(_M0L6_2atmpS1154);
    _M0L6_2atmpS1152 = _M0L6_2atmpS1941;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1940
    = moonbit_add_string(_M0L6_2atmpS1152, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1152);
    _M0L6_2atmpS1151 = _M0L6_2atmpS1940;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1150
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1150)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1150)->$0
    = _M0L6_2atmpS1151;
    _result_2162.tag = 0;
    _result_2162.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1150;
    return _result_2162;
  } else {
    int32_t _M0L6_2atmpS1175;
    struct moonbit_result_0 _result_2163;
    moonbit_decref(_M0L9args__locS233);
    moonbit_decref(_M0L3locS231);
    moonbit_decref(_M0L7contentS229);
    moonbit_decref(_M0L6actualS227);
    _M0L6_2atmpS1175 = 0;
    _result_2163.tag = 1;
    _result_2163.data.ok = _M0L6_2atmpS1175;
    return _result_2163;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS220
) {
  struct _M0TPB13StringBuilder* _M0L3bufS218;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS219;
  int32_t _M0L7_2abindS221;
  int32_t _M0L1iS222;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS218 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS219 = _M0L4selfS220;
  moonbit_incref(_M0L3bufS218);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS218, 91);
  _M0L7_2abindS221 = _M0L7_2aselfS219->$1;
  _M0L1iS222 = 0;
  while (1) {
    if (_M0L1iS222 < _M0L7_2abindS221) {
      int32_t _if__result_2165;
      moonbit_string_t* _M0L8_2afieldS1957;
      moonbit_string_t* _M0L3bufS1148;
      moonbit_string_t _M0L6_2atmpS1956;
      moonbit_string_t _M0L4itemS223;
      int32_t _M0L6_2atmpS1149;
      if (_M0L1iS222 != 0) {
        moonbit_incref(_M0L3bufS218);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS218, (moonbit_string_t)moonbit_string_literal_33.data);
      }
      if (_M0L1iS222 < 0) {
        _if__result_2165 = 1;
      } else {
        int32_t _M0L3lenS1147 = _M0L7_2aselfS219->$1;
        _if__result_2165 = _M0L1iS222 >= _M0L3lenS1147;
      }
      if (_if__result_2165) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS1957 = _M0L7_2aselfS219->$0;
      _M0L3bufS1148 = _M0L8_2afieldS1957;
      _M0L6_2atmpS1956 = (moonbit_string_t)_M0L3bufS1148[_M0L1iS222];
      _M0L4itemS223 = _M0L6_2atmpS1956;
      if (_M0L4itemS223 == 0) {
        moonbit_incref(_M0L3bufS218);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS218, (moonbit_string_t)moonbit_string_literal_34.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS224 = _M0L4itemS223;
        moonbit_string_t _M0L6_2alocS225 = _M0L7_2aSomeS224;
        moonbit_string_t _M0L6_2atmpS1146;
        moonbit_incref(_M0L6_2alocS225);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1146
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS225);
        moonbit_incref(_M0L3bufS218);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS218, _M0L6_2atmpS1146);
      }
      _M0L6_2atmpS1149 = _M0L1iS222 + 1;
      _M0L1iS222 = _M0L6_2atmpS1149;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS219);
    }
    break;
  }
  moonbit_incref(_M0L3bufS218);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS218, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS218);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS217
) {
  moonbit_string_t _M0L6_2atmpS1145;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1144;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1145 = _M0L4selfS217;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1144 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1145);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1144);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS216
) {
  struct _M0TPB13StringBuilder* _M0L2sbS215;
  struct _M0TPC16string10StringView _M0L8_2afieldS1970;
  struct _M0TPC16string10StringView _M0L3pkgS1129;
  moonbit_string_t _M0L6_2atmpS1128;
  moonbit_string_t _M0L6_2atmpS1969;
  moonbit_string_t _M0L6_2atmpS1127;
  moonbit_string_t _M0L6_2atmpS1968;
  moonbit_string_t _M0L6_2atmpS1126;
  struct _M0TPC16string10StringView _M0L8_2afieldS1967;
  struct _M0TPC16string10StringView _M0L8filenameS1130;
  struct _M0TPC16string10StringView _M0L8_2afieldS1966;
  struct _M0TPC16string10StringView _M0L11start__lineS1133;
  moonbit_string_t _M0L6_2atmpS1132;
  moonbit_string_t _M0L6_2atmpS1965;
  moonbit_string_t _M0L6_2atmpS1131;
  struct _M0TPC16string10StringView _M0L8_2afieldS1964;
  struct _M0TPC16string10StringView _M0L13start__columnS1136;
  moonbit_string_t _M0L6_2atmpS1135;
  moonbit_string_t _M0L6_2atmpS1963;
  moonbit_string_t _M0L6_2atmpS1134;
  struct _M0TPC16string10StringView _M0L8_2afieldS1962;
  struct _M0TPC16string10StringView _M0L9end__lineS1139;
  moonbit_string_t _M0L6_2atmpS1138;
  moonbit_string_t _M0L6_2atmpS1961;
  moonbit_string_t _M0L6_2atmpS1137;
  struct _M0TPC16string10StringView _M0L8_2afieldS1960;
  int32_t _M0L6_2acntS2061;
  struct _M0TPC16string10StringView _M0L11end__columnS1143;
  moonbit_string_t _M0L6_2atmpS1142;
  moonbit_string_t _M0L6_2atmpS1959;
  moonbit_string_t _M0L6_2atmpS1141;
  moonbit_string_t _M0L6_2atmpS1958;
  moonbit_string_t _M0L6_2atmpS1140;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS215 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS1970
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$0_1, _M0L4selfS216->$0_2, _M0L4selfS216->$0_0
  };
  _M0L3pkgS1129 = _M0L8_2afieldS1970;
  moonbit_incref(_M0L3pkgS1129.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1128
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1129);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1969
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS1128);
  moonbit_decref(_M0L6_2atmpS1128);
  _M0L6_2atmpS1127 = _M0L6_2atmpS1969;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1968
  = moonbit_add_string(_M0L6_2atmpS1127, (moonbit_string_t)moonbit_string_literal_26.data);
  moonbit_decref(_M0L6_2atmpS1127);
  _M0L6_2atmpS1126 = _M0L6_2atmpS1968;
  moonbit_incref(_M0L2sbS215);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1126);
  moonbit_incref(_M0L2sbS215);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, (moonbit_string_t)moonbit_string_literal_36.data);
  _M0L8_2afieldS1967
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$1_1, _M0L4selfS216->$1_2, _M0L4selfS216->$1_0
  };
  _M0L8filenameS1130 = _M0L8_2afieldS1967;
  moonbit_incref(_M0L8filenameS1130.$0);
  moonbit_incref(_M0L2sbS215);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS215, _M0L8filenameS1130);
  _M0L8_2afieldS1966
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$2_1, _M0L4selfS216->$2_2, _M0L4selfS216->$2_0
  };
  _M0L11start__lineS1133 = _M0L8_2afieldS1966;
  moonbit_incref(_M0L11start__lineS1133.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1132
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1133);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1965
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS1132);
  moonbit_decref(_M0L6_2atmpS1132);
  _M0L6_2atmpS1131 = _M0L6_2atmpS1965;
  moonbit_incref(_M0L2sbS215);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1131);
  _M0L8_2afieldS1964
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$3_1, _M0L4selfS216->$3_2, _M0L4selfS216->$3_0
  };
  _M0L13start__columnS1136 = _M0L8_2afieldS1964;
  moonbit_incref(_M0L13start__columnS1136.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1135
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1136);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1963
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS1135);
  moonbit_decref(_M0L6_2atmpS1135);
  _M0L6_2atmpS1134 = _M0L6_2atmpS1963;
  moonbit_incref(_M0L2sbS215);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1134);
  _M0L8_2afieldS1962
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$4_1, _M0L4selfS216->$4_2, _M0L4selfS216->$4_0
  };
  _M0L9end__lineS1139 = _M0L8_2afieldS1962;
  moonbit_incref(_M0L9end__lineS1139.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1138
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1139);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1961
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_39.data, _M0L6_2atmpS1138);
  moonbit_decref(_M0L6_2atmpS1138);
  _M0L6_2atmpS1137 = _M0L6_2atmpS1961;
  moonbit_incref(_M0L2sbS215);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1137);
  _M0L8_2afieldS1960
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$5_1, _M0L4selfS216->$5_2, _M0L4selfS216->$5_0
  };
  _M0L6_2acntS2061 = Moonbit_object_header(_M0L4selfS216)->rc;
  if (_M0L6_2acntS2061 > 1) {
    int32_t _M0L11_2anew__cntS2067 = _M0L6_2acntS2061 - 1;
    Moonbit_object_header(_M0L4selfS216)->rc = _M0L11_2anew__cntS2067;
    moonbit_incref(_M0L8_2afieldS1960.$0);
  } else if (_M0L6_2acntS2061 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2066 =
      (struct _M0TPC16string10StringView){_M0L4selfS216->$4_1,
                                            _M0L4selfS216->$4_2,
                                            _M0L4selfS216->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2065;
    struct _M0TPC16string10StringView _M0L8_2afieldS2064;
    struct _M0TPC16string10StringView _M0L8_2afieldS2063;
    struct _M0TPC16string10StringView _M0L8_2afieldS2062;
    moonbit_decref(_M0L8_2afieldS2066.$0);
    _M0L8_2afieldS2065
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$3_1, _M0L4selfS216->$3_2, _M0L4selfS216->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2065.$0);
    _M0L8_2afieldS2064
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$2_1, _M0L4selfS216->$2_2, _M0L4selfS216->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2064.$0);
    _M0L8_2afieldS2063
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$1_1, _M0L4selfS216->$1_2, _M0L4selfS216->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2063.$0);
    _M0L8_2afieldS2062
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$0_1, _M0L4selfS216->$0_2, _M0L4selfS216->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2062.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS216);
  }
  _M0L11end__columnS1143 = _M0L8_2afieldS1960;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1142
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1143);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1959
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS1142);
  moonbit_decref(_M0L6_2atmpS1142);
  _M0L6_2atmpS1141 = _M0L6_2atmpS1959;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1958
  = moonbit_add_string(_M0L6_2atmpS1141, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1141);
  _M0L6_2atmpS1140 = _M0L6_2atmpS1958;
  moonbit_incref(_M0L2sbS215);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1140);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS215);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS213,
  moonbit_string_t _M0L3strS214
) {
  int32_t _M0L3lenS1116;
  int32_t _M0L6_2atmpS1118;
  int32_t _M0L6_2atmpS1117;
  int32_t _M0L6_2atmpS1115;
  moonbit_bytes_t _M0L8_2afieldS1972;
  moonbit_bytes_t _M0L4dataS1119;
  int32_t _M0L3lenS1120;
  int32_t _M0L6_2atmpS1121;
  int32_t _M0L3lenS1123;
  int32_t _M0L6_2atmpS1971;
  int32_t _M0L6_2atmpS1125;
  int32_t _M0L6_2atmpS1124;
  int32_t _M0L6_2atmpS1122;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1116 = _M0L4selfS213->$1;
  _M0L6_2atmpS1118 = Moonbit_array_length(_M0L3strS214);
  _M0L6_2atmpS1117 = _M0L6_2atmpS1118 * 2;
  _M0L6_2atmpS1115 = _M0L3lenS1116 + _M0L6_2atmpS1117;
  moonbit_incref(_M0L4selfS213);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS213, _M0L6_2atmpS1115);
  _M0L8_2afieldS1972 = _M0L4selfS213->$0;
  _M0L4dataS1119 = _M0L8_2afieldS1972;
  _M0L3lenS1120 = _M0L4selfS213->$1;
  _M0L6_2atmpS1121 = Moonbit_array_length(_M0L3strS214);
  moonbit_incref(_M0L4dataS1119);
  moonbit_incref(_M0L3strS214);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1119, _M0L3lenS1120, _M0L3strS214, 0, _M0L6_2atmpS1121);
  _M0L3lenS1123 = _M0L4selfS213->$1;
  _M0L6_2atmpS1971 = Moonbit_array_length(_M0L3strS214);
  moonbit_decref(_M0L3strS214);
  _M0L6_2atmpS1125 = _M0L6_2atmpS1971;
  _M0L6_2atmpS1124 = _M0L6_2atmpS1125 * 2;
  _M0L6_2atmpS1122 = _M0L3lenS1123 + _M0L6_2atmpS1124;
  _M0L4selfS213->$1 = _M0L6_2atmpS1122;
  moonbit_decref(_M0L4selfS213);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS205,
  int32_t _M0L13bytes__offsetS200,
  moonbit_string_t _M0L3strS207,
  int32_t _M0L11str__offsetS203,
  int32_t _M0L6lengthS201
) {
  int32_t _M0L6_2atmpS1114;
  int32_t _M0L6_2atmpS1113;
  int32_t _M0L2e1S199;
  int32_t _M0L6_2atmpS1112;
  int32_t _M0L2e2S202;
  int32_t _M0L4len1S204;
  int32_t _M0L4len2S206;
  int32_t _if__result_2166;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1114 = _M0L6lengthS201 * 2;
  _M0L6_2atmpS1113 = _M0L13bytes__offsetS200 + _M0L6_2atmpS1114;
  _M0L2e1S199 = _M0L6_2atmpS1113 - 1;
  _M0L6_2atmpS1112 = _M0L11str__offsetS203 + _M0L6lengthS201;
  _M0L2e2S202 = _M0L6_2atmpS1112 - 1;
  _M0L4len1S204 = Moonbit_array_length(_M0L4selfS205);
  _M0L4len2S206 = Moonbit_array_length(_M0L3strS207);
  if (_M0L6lengthS201 >= 0) {
    if (_M0L13bytes__offsetS200 >= 0) {
      if (_M0L2e1S199 < _M0L4len1S204) {
        if (_M0L11str__offsetS203 >= 0) {
          _if__result_2166 = _M0L2e2S202 < _M0L4len2S206;
        } else {
          _if__result_2166 = 0;
        }
      } else {
        _if__result_2166 = 0;
      }
    } else {
      _if__result_2166 = 0;
    }
  } else {
    _if__result_2166 = 0;
  }
  if (_if__result_2166) {
    int32_t _M0L16end__str__offsetS208 =
      _M0L11str__offsetS203 + _M0L6lengthS201;
    int32_t _M0L1iS209 = _M0L11str__offsetS203;
    int32_t _M0L1jS210 = _M0L13bytes__offsetS200;
    while (1) {
      if (_M0L1iS209 < _M0L16end__str__offsetS208) {
        int32_t _M0L6_2atmpS1109 = _M0L3strS207[_M0L1iS209];
        int32_t _M0L6_2atmpS1108 = (int32_t)_M0L6_2atmpS1109;
        uint32_t _M0L1cS211 = *(uint32_t*)&_M0L6_2atmpS1108;
        uint32_t _M0L6_2atmpS1104 = _M0L1cS211 & 255u;
        int32_t _M0L6_2atmpS1103;
        int32_t _M0L6_2atmpS1105;
        uint32_t _M0L6_2atmpS1107;
        int32_t _M0L6_2atmpS1106;
        int32_t _M0L6_2atmpS1110;
        int32_t _M0L6_2atmpS1111;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1103 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1104);
        if (
          _M0L1jS210 < 0 || _M0L1jS210 >= Moonbit_array_length(_M0L4selfS205)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS205[_M0L1jS210] = _M0L6_2atmpS1103;
        _M0L6_2atmpS1105 = _M0L1jS210 + 1;
        _M0L6_2atmpS1107 = _M0L1cS211 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1106 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1107);
        if (
          _M0L6_2atmpS1105 < 0
          || _M0L6_2atmpS1105 >= Moonbit_array_length(_M0L4selfS205)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS205[_M0L6_2atmpS1105] = _M0L6_2atmpS1106;
        _M0L6_2atmpS1110 = _M0L1iS209 + 1;
        _M0L6_2atmpS1111 = _M0L1jS210 + 2;
        _M0L1iS209 = _M0L6_2atmpS1110;
        _M0L1jS210 = _M0L6_2atmpS1111;
        continue;
      } else {
        moonbit_decref(_M0L3strS207);
        moonbit_decref(_M0L4selfS205);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS207);
    moonbit_decref(_M0L4selfS205);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS198,
  struct _M0TPC16string10StringView _M0L3objS197
) {
  struct _M0TPB6Logger _M0L6_2atmpS1102;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1102
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS198
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS197, _M0L6_2atmpS1102);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS143
) {
  int32_t _M0L6_2atmpS1101;
  struct _M0TPC16string10StringView _M0L7_2abindS142;
  moonbit_string_t _M0L7_2adataS144;
  int32_t _M0L8_2astartS145;
  int32_t _M0L6_2atmpS1100;
  int32_t _M0L6_2aendS146;
  int32_t _M0Lm9_2acursorS147;
  int32_t _M0Lm13accept__stateS148;
  int32_t _M0Lm10match__endS149;
  int32_t _M0Lm20match__tag__saver__0S150;
  int32_t _M0Lm20match__tag__saver__1S151;
  int32_t _M0Lm20match__tag__saver__2S152;
  int32_t _M0Lm20match__tag__saver__3S153;
  int32_t _M0Lm20match__tag__saver__4S154;
  int32_t _M0Lm6tag__0S155;
  int32_t _M0Lm6tag__1S156;
  int32_t _M0Lm9tag__1__1S157;
  int32_t _M0Lm9tag__1__2S158;
  int32_t _M0Lm6tag__3S159;
  int32_t _M0Lm6tag__2S160;
  int32_t _M0Lm9tag__2__1S161;
  int32_t _M0Lm6tag__4S162;
  int32_t _M0L6_2atmpS1058;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1101 = Moonbit_array_length(_M0L4reprS143);
  _M0L7_2abindS142
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1101, _M0L4reprS143
  };
  moonbit_incref(_M0L7_2abindS142.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS144 = _M0MPC16string10StringView4data(_M0L7_2abindS142);
  moonbit_incref(_M0L7_2abindS142.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS145
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS142);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1100 = _M0MPC16string10StringView6length(_M0L7_2abindS142);
  _M0L6_2aendS146 = _M0L8_2astartS145 + _M0L6_2atmpS1100;
  _M0Lm9_2acursorS147 = _M0L8_2astartS145;
  _M0Lm13accept__stateS148 = -1;
  _M0Lm10match__endS149 = -1;
  _M0Lm20match__tag__saver__0S150 = -1;
  _M0Lm20match__tag__saver__1S151 = -1;
  _M0Lm20match__tag__saver__2S152 = -1;
  _M0Lm20match__tag__saver__3S153 = -1;
  _M0Lm20match__tag__saver__4S154 = -1;
  _M0Lm6tag__0S155 = -1;
  _M0Lm6tag__1S156 = -1;
  _M0Lm9tag__1__1S157 = -1;
  _M0Lm9tag__1__2S158 = -1;
  _M0Lm6tag__3S159 = -1;
  _M0Lm6tag__2S160 = -1;
  _M0Lm9tag__2__1S161 = -1;
  _M0Lm6tag__4S162 = -1;
  _M0L6_2atmpS1058 = _M0Lm9_2acursorS147;
  if (_M0L6_2atmpS1058 < _M0L6_2aendS146) {
    int32_t _M0L6_2atmpS1060 = _M0Lm9_2acursorS147;
    int32_t _M0L6_2atmpS1059;
    moonbit_incref(_M0L7_2adataS144);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1059
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1060);
    if (_M0L6_2atmpS1059 == 64) {
      int32_t _M0L6_2atmpS1061 = _M0Lm9_2acursorS147;
      _M0Lm9_2acursorS147 = _M0L6_2atmpS1061 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1062;
        _M0Lm6tag__0S155 = _M0Lm9_2acursorS147;
        _M0L6_2atmpS1062 = _M0Lm9_2acursorS147;
        if (_M0L6_2atmpS1062 < _M0L6_2aendS146) {
          int32_t _M0L6_2atmpS1099 = _M0Lm9_2acursorS147;
          int32_t _M0L10next__charS170;
          int32_t _M0L6_2atmpS1063;
          moonbit_incref(_M0L7_2adataS144);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS170
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1099);
          _M0L6_2atmpS1063 = _M0Lm9_2acursorS147;
          _M0Lm9_2acursorS147 = _M0L6_2atmpS1063 + 1;
          if (_M0L10next__charS170 == 58) {
            int32_t _M0L6_2atmpS1064 = _M0Lm9_2acursorS147;
            if (_M0L6_2atmpS1064 < _M0L6_2aendS146) {
              int32_t _M0L6_2atmpS1065 = _M0Lm9_2acursorS147;
              int32_t _M0L12dispatch__15S171;
              _M0Lm9_2acursorS147 = _M0L6_2atmpS1065 + 1;
              _M0L12dispatch__15S171 = 0;
              loop__label__15_174:;
              while (1) {
                int32_t _M0L6_2atmpS1066;
                switch (_M0L12dispatch__15S171) {
                  case 3: {
                    int32_t _M0L6_2atmpS1069;
                    _M0Lm9tag__1__2S158 = _M0Lm9tag__1__1S157;
                    _M0Lm9tag__1__1S157 = _M0Lm6tag__1S156;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1069 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1069 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1074 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS178;
                      int32_t _M0L6_2atmpS1070;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS178
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1074);
                      _M0L6_2atmpS1070 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1070 + 1;
                      if (_M0L10next__charS178 < 58) {
                        if (_M0L10next__charS178 < 48) {
                          goto join_177;
                        } else {
                          int32_t _M0L6_2atmpS1071;
                          _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                          _M0Lm9tag__2__1S161 = _M0Lm6tag__2S160;
                          _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                          _M0Lm6tag__3S159 = _M0Lm9_2acursorS147;
                          _M0L6_2atmpS1071 = _M0Lm9_2acursorS147;
                          if (_M0L6_2atmpS1071 < _M0L6_2aendS146) {
                            int32_t _M0L6_2atmpS1073 = _M0Lm9_2acursorS147;
                            int32_t _M0L10next__charS180;
                            int32_t _M0L6_2atmpS1072;
                            moonbit_incref(_M0L7_2adataS144);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS180
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1073);
                            _M0L6_2atmpS1072 = _M0Lm9_2acursorS147;
                            _M0Lm9_2acursorS147 = _M0L6_2atmpS1072 + 1;
                            if (_M0L10next__charS180 < 48) {
                              if (_M0L10next__charS180 == 45) {
                                goto join_172;
                              } else {
                                goto join_179;
                              }
                            } else if (_M0L10next__charS180 > 57) {
                              if (_M0L10next__charS180 < 59) {
                                _M0L12dispatch__15S171 = 3;
                                goto loop__label__15_174;
                              } else {
                                goto join_179;
                              }
                            } else {
                              _M0L12dispatch__15S171 = 6;
                              goto loop__label__15_174;
                            }
                            join_179:;
                            _M0L12dispatch__15S171 = 0;
                            goto loop__label__15_174;
                          } else {
                            goto join_163;
                          }
                        }
                      } else if (_M0L10next__charS178 > 58) {
                        goto join_177;
                      } else {
                        _M0L12dispatch__15S171 = 1;
                        goto loop__label__15_174;
                      }
                      join_177:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1075;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1075 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1075 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1077 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS182;
                      int32_t _M0L6_2atmpS1076;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS182
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1077);
                      _M0L6_2atmpS1076 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1076 + 1;
                      if (_M0L10next__charS182 < 58) {
                        if (_M0L10next__charS182 < 48) {
                          goto join_181;
                        } else {
                          _M0L12dispatch__15S171 = 2;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS182 > 58) {
                        goto join_181;
                      } else {
                        _M0L12dispatch__15S171 = 3;
                        goto loop__label__15_174;
                      }
                      join_181:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1078;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1078 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1078 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1080 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS183;
                      int32_t _M0L6_2atmpS1079;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS183
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1080);
                      _M0L6_2atmpS1079 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1079 + 1;
                      if (_M0L10next__charS183 == 58) {
                        _M0L12dispatch__15S171 = 1;
                        goto loop__label__15_174;
                      } else {
                        _M0L12dispatch__15S171 = 0;
                        goto loop__label__15_174;
                      }
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1081;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__4S162 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1081 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1081 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1089 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS185;
                      int32_t _M0L6_2atmpS1082;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS185
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1089);
                      _M0L6_2atmpS1082 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1082 + 1;
                      if (_M0L10next__charS185 < 58) {
                        if (_M0L10next__charS185 < 48) {
                          goto join_184;
                        } else {
                          _M0L12dispatch__15S171 = 4;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS185 > 58) {
                        goto join_184;
                      } else {
                        int32_t _M0L6_2atmpS1083;
                        _M0Lm9tag__1__2S158 = _M0Lm9tag__1__1S157;
                        _M0Lm9tag__1__1S157 = _M0Lm6tag__1S156;
                        _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                        _M0L6_2atmpS1083 = _M0Lm9_2acursorS147;
                        if (_M0L6_2atmpS1083 < _M0L6_2aendS146) {
                          int32_t _M0L6_2atmpS1088 = _M0Lm9_2acursorS147;
                          int32_t _M0L10next__charS187;
                          int32_t _M0L6_2atmpS1084;
                          moonbit_incref(_M0L7_2adataS144);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS187
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1088);
                          _M0L6_2atmpS1084 = _M0Lm9_2acursorS147;
                          _M0Lm9_2acursorS147 = _M0L6_2atmpS1084 + 1;
                          if (_M0L10next__charS187 < 58) {
                            if (_M0L10next__charS187 < 48) {
                              goto join_186;
                            } else {
                              int32_t _M0L6_2atmpS1085;
                              _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                              _M0Lm9tag__2__1S161 = _M0Lm6tag__2S160;
                              _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                              _M0L6_2atmpS1085 = _M0Lm9_2acursorS147;
                              if (_M0L6_2atmpS1085 < _M0L6_2aendS146) {
                                int32_t _M0L6_2atmpS1087 =
                                  _M0Lm9_2acursorS147;
                                int32_t _M0L10next__charS189;
                                int32_t _M0L6_2atmpS1086;
                                moonbit_incref(_M0L7_2adataS144);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS189
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1087);
                                _M0L6_2atmpS1086 = _M0Lm9_2acursorS147;
                                _M0Lm9_2acursorS147 = _M0L6_2atmpS1086 + 1;
                                if (_M0L10next__charS189 < 58) {
                                  if (_M0L10next__charS189 < 48) {
                                    goto join_188;
                                  } else {
                                    _M0L12dispatch__15S171 = 5;
                                    goto loop__label__15_174;
                                  }
                                } else if (_M0L10next__charS189 > 58) {
                                  goto join_188;
                                } else {
                                  _M0L12dispatch__15S171 = 3;
                                  goto loop__label__15_174;
                                }
                                join_188:;
                                _M0L12dispatch__15S171 = 0;
                                goto loop__label__15_174;
                              } else {
                                goto join_176;
                              }
                            }
                          } else if (_M0L10next__charS187 > 58) {
                            goto join_186;
                          } else {
                            _M0L12dispatch__15S171 = 1;
                            goto loop__label__15_174;
                          }
                          join_186:;
                          _M0L12dispatch__15S171 = 0;
                          goto loop__label__15_174;
                        } else {
                          goto join_163;
                        }
                      }
                      join_184:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1090;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1090 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1090 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1092 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS191;
                      int32_t _M0L6_2atmpS1091;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS191
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1092);
                      _M0L6_2atmpS1091 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1091 + 1;
                      if (_M0L10next__charS191 < 58) {
                        if (_M0L10next__charS191 < 48) {
                          goto join_190;
                        } else {
                          _M0L12dispatch__15S171 = 5;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS191 > 58) {
                        goto join_190;
                      } else {
                        _M0L12dispatch__15S171 = 3;
                        goto loop__label__15_174;
                      }
                      join_190:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_176;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1093;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__3S159 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1093 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1093 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1095 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS193;
                      int32_t _M0L6_2atmpS1094;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS193
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1095);
                      _M0L6_2atmpS1094 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1094 + 1;
                      if (_M0L10next__charS193 < 48) {
                        if (_M0L10next__charS193 == 45) {
                          goto join_172;
                        } else {
                          goto join_192;
                        }
                      } else if (_M0L10next__charS193 > 57) {
                        if (_M0L10next__charS193 < 59) {
                          _M0L12dispatch__15S171 = 3;
                          goto loop__label__15_174;
                        } else {
                          goto join_192;
                        }
                      } else {
                        _M0L12dispatch__15S171 = 6;
                        goto loop__label__15_174;
                      }
                      join_192:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1096;
                    _M0Lm9tag__1__1S157 = _M0Lm6tag__1S156;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1096 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1096 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1098 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS195;
                      int32_t _M0L6_2atmpS1097;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS195
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1098);
                      _M0L6_2atmpS1097 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1097 + 1;
                      if (_M0L10next__charS195 < 58) {
                        if (_M0L10next__charS195 < 48) {
                          goto join_194;
                        } else {
                          _M0L12dispatch__15S171 = 2;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS195 > 58) {
                        goto join_194;
                      } else {
                        _M0L12dispatch__15S171 = 1;
                        goto loop__label__15_174;
                      }
                      join_194:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  default: {
                    goto join_163;
                    break;
                  }
                }
                join_176:;
                _M0Lm6tag__1S156 = _M0Lm9tag__1__2S158;
                _M0Lm6tag__2S160 = _M0Lm9tag__2__1S161;
                _M0Lm20match__tag__saver__0S150 = _M0Lm6tag__0S155;
                _M0Lm20match__tag__saver__1S151 = _M0Lm6tag__1S156;
                _M0Lm20match__tag__saver__2S152 = _M0Lm6tag__2S160;
                _M0Lm20match__tag__saver__3S153 = _M0Lm6tag__3S159;
                _M0Lm20match__tag__saver__4S154 = _M0Lm6tag__4S162;
                _M0Lm13accept__stateS148 = 0;
                _M0Lm10match__endS149 = _M0Lm9_2acursorS147;
                goto join_163;
                join_172:;
                _M0Lm9tag__1__1S157 = _M0Lm9tag__1__2S158;
                _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                _M0Lm6tag__2S160 = _M0Lm9tag__2__1S161;
                _M0L6_2atmpS1066 = _M0Lm9_2acursorS147;
                if (_M0L6_2atmpS1066 < _M0L6_2aendS146) {
                  int32_t _M0L6_2atmpS1068 = _M0Lm9_2acursorS147;
                  int32_t _M0L10next__charS175;
                  int32_t _M0L6_2atmpS1067;
                  moonbit_incref(_M0L7_2adataS144);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS175
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1068);
                  _M0L6_2atmpS1067 = _M0Lm9_2acursorS147;
                  _M0Lm9_2acursorS147 = _M0L6_2atmpS1067 + 1;
                  if (_M0L10next__charS175 < 58) {
                    if (_M0L10next__charS175 < 48) {
                      goto join_173;
                    } else {
                      _M0L12dispatch__15S171 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS175 > 58) {
                    goto join_173;
                  } else {
                    _M0L12dispatch__15S171 = 1;
                    continue;
                  }
                  join_173:;
                  _M0L12dispatch__15S171 = 0;
                  continue;
                } else {
                  goto join_163;
                }
                break;
              }
            } else {
              goto join_163;
            }
          } else {
            continue;
          }
        } else {
          goto join_163;
        }
        break;
      }
    } else {
      goto join_163;
    }
  } else {
    goto join_163;
  }
  join_163:;
  switch (_M0Lm13accept__stateS148) {
    case 0: {
      int32_t _M0L6_2atmpS1057 = _M0Lm20match__tag__saver__1S151;
      int32_t _M0L6_2atmpS1056 = _M0L6_2atmpS1057 + 1;
      int64_t _M0L6_2atmpS1053 = (int64_t)_M0L6_2atmpS1056;
      int32_t _M0L6_2atmpS1055 = _M0Lm20match__tag__saver__2S152;
      int64_t _M0L6_2atmpS1054 = (int64_t)_M0L6_2atmpS1055;
      struct _M0TPC16string10StringView _M0L11start__lineS164;
      int32_t _M0L6_2atmpS1052;
      int32_t _M0L6_2atmpS1051;
      int64_t _M0L6_2atmpS1048;
      int32_t _M0L6_2atmpS1050;
      int64_t _M0L6_2atmpS1049;
      struct _M0TPC16string10StringView _M0L13start__columnS165;
      int32_t _M0L6_2atmpS1047;
      int64_t _M0L6_2atmpS1044;
      int32_t _M0L6_2atmpS1046;
      int64_t _M0L6_2atmpS1045;
      struct _M0TPC16string10StringView _M0L3pkgS166;
      int32_t _M0L6_2atmpS1043;
      int32_t _M0L6_2atmpS1042;
      int64_t _M0L6_2atmpS1039;
      int32_t _M0L6_2atmpS1041;
      int64_t _M0L6_2atmpS1040;
      struct _M0TPC16string10StringView _M0L8filenameS167;
      int32_t _M0L6_2atmpS1038;
      int32_t _M0L6_2atmpS1037;
      int64_t _M0L6_2atmpS1034;
      int32_t _M0L6_2atmpS1036;
      int64_t _M0L6_2atmpS1035;
      struct _M0TPC16string10StringView _M0L9end__lineS168;
      int32_t _M0L6_2atmpS1033;
      int32_t _M0L6_2atmpS1032;
      int64_t _M0L6_2atmpS1029;
      int32_t _M0L6_2atmpS1031;
      int64_t _M0L6_2atmpS1030;
      struct _M0TPC16string10StringView _M0L11end__columnS169;
      struct _M0TPB13SourceLocRepr* _block_2183;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS164
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1053, _M0L6_2atmpS1054);
      _M0L6_2atmpS1052 = _M0Lm20match__tag__saver__2S152;
      _M0L6_2atmpS1051 = _M0L6_2atmpS1052 + 1;
      _M0L6_2atmpS1048 = (int64_t)_M0L6_2atmpS1051;
      _M0L6_2atmpS1050 = _M0Lm20match__tag__saver__3S153;
      _M0L6_2atmpS1049 = (int64_t)_M0L6_2atmpS1050;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS165
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1048, _M0L6_2atmpS1049);
      _M0L6_2atmpS1047 = _M0L8_2astartS145 + 1;
      _M0L6_2atmpS1044 = (int64_t)_M0L6_2atmpS1047;
      _M0L6_2atmpS1046 = _M0Lm20match__tag__saver__0S150;
      _M0L6_2atmpS1045 = (int64_t)_M0L6_2atmpS1046;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS166
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1044, _M0L6_2atmpS1045);
      _M0L6_2atmpS1043 = _M0Lm20match__tag__saver__0S150;
      _M0L6_2atmpS1042 = _M0L6_2atmpS1043 + 1;
      _M0L6_2atmpS1039 = (int64_t)_M0L6_2atmpS1042;
      _M0L6_2atmpS1041 = _M0Lm20match__tag__saver__1S151;
      _M0L6_2atmpS1040 = (int64_t)_M0L6_2atmpS1041;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS167
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1039, _M0L6_2atmpS1040);
      _M0L6_2atmpS1038 = _M0Lm20match__tag__saver__3S153;
      _M0L6_2atmpS1037 = _M0L6_2atmpS1038 + 1;
      _M0L6_2atmpS1034 = (int64_t)_M0L6_2atmpS1037;
      _M0L6_2atmpS1036 = _M0Lm20match__tag__saver__4S154;
      _M0L6_2atmpS1035 = (int64_t)_M0L6_2atmpS1036;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS168
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1034, _M0L6_2atmpS1035);
      _M0L6_2atmpS1033 = _M0Lm20match__tag__saver__4S154;
      _M0L6_2atmpS1032 = _M0L6_2atmpS1033 + 1;
      _M0L6_2atmpS1029 = (int64_t)_M0L6_2atmpS1032;
      _M0L6_2atmpS1031 = _M0Lm10match__endS149;
      _M0L6_2atmpS1030 = (int64_t)_M0L6_2atmpS1031;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS169
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1029, _M0L6_2atmpS1030);
      _block_2183
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2183)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_2183->$0_0 = _M0L3pkgS166.$0;
      _block_2183->$0_1 = _M0L3pkgS166.$1;
      _block_2183->$0_2 = _M0L3pkgS166.$2;
      _block_2183->$1_0 = _M0L8filenameS167.$0;
      _block_2183->$1_1 = _M0L8filenameS167.$1;
      _block_2183->$1_2 = _M0L8filenameS167.$2;
      _block_2183->$2_0 = _M0L11start__lineS164.$0;
      _block_2183->$2_1 = _M0L11start__lineS164.$1;
      _block_2183->$2_2 = _M0L11start__lineS164.$2;
      _block_2183->$3_0 = _M0L13start__columnS165.$0;
      _block_2183->$3_1 = _M0L13start__columnS165.$1;
      _block_2183->$3_2 = _M0L13start__columnS165.$2;
      _block_2183->$4_0 = _M0L9end__lineS168.$0;
      _block_2183->$4_1 = _M0L9end__lineS168.$1;
      _block_2183->$4_2 = _M0L9end__lineS168.$2;
      _block_2183->$5_0 = _M0L11end__columnS169.$0;
      _block_2183->$5_1 = _M0L11end__columnS169.$1;
      _block_2183->$5_2 = _M0L11end__columnS169.$2;
      return _block_2183;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS144);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS140,
  int32_t _M0L5indexS141
) {
  int32_t _M0L3lenS139;
  int32_t _if__result_2184;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS139 = _M0L4selfS140->$1;
  if (_M0L5indexS141 >= 0) {
    _if__result_2184 = _M0L5indexS141 < _M0L3lenS139;
  } else {
    _if__result_2184 = 0;
  }
  if (_if__result_2184) {
    moonbit_string_t* _M0L6_2atmpS1028;
    moonbit_string_t _M0L6_2atmpS1973;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1028 = _M0MPC15array5Array6bufferGsE(_M0L4selfS140);
    if (
      _M0L5indexS141 < 0
      || _M0L5indexS141 >= Moonbit_array_length(_M0L6_2atmpS1028)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1973 = (moonbit_string_t)_M0L6_2atmpS1028[_M0L5indexS141];
    moonbit_incref(_M0L6_2atmpS1973);
    moonbit_decref(_M0L6_2atmpS1028);
    return _M0L6_2atmpS1973;
  } else {
    moonbit_decref(_M0L4selfS140);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS137
) {
  moonbit_string_t* _M0L8_2afieldS1974;
  int32_t _M0L6_2acntS2068;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS1974 = _M0L4selfS137->$0;
  _M0L6_2acntS2068 = Moonbit_object_header(_M0L4selfS137)->rc;
  if (_M0L6_2acntS2068 > 1) {
    int32_t _M0L11_2anew__cntS2069 = _M0L6_2acntS2068 - 1;
    Moonbit_object_header(_M0L4selfS137)->rc = _M0L11_2anew__cntS2069;
    moonbit_incref(_M0L8_2afieldS1974);
  } else if (_M0L6_2acntS2068 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS137);
  }
  return _M0L8_2afieldS1974;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS138
) {
  struct _M0TUsiE** _M0L8_2afieldS1975;
  int32_t _M0L6_2acntS2070;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS1975 = _M0L4selfS138->$0;
  _M0L6_2acntS2070 = Moonbit_object_header(_M0L4selfS138)->rc;
  if (_M0L6_2acntS2070 > 1) {
    int32_t _M0L11_2anew__cntS2071 = _M0L6_2acntS2070 - 1;
    Moonbit_object_header(_M0L4selfS138)->rc = _M0L11_2anew__cntS2071;
    moonbit_incref(_M0L8_2afieldS1975);
  } else if (_M0L6_2acntS2070 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS138);
  }
  return _M0L8_2afieldS1975;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS136) {
  struct _M0TPB13StringBuilder* _M0L3bufS135;
  struct _M0TPB6Logger _M0L6_2atmpS1027;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS135 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS135);
  _M0L6_2atmpS1027
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS135
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS136, _M0L6_2atmpS1027);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS135);
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS129
) {
  int32_t _M0L17codepoint__lengthS128;
  int32_t _M0L6_2atmpS1026;
  moonbit_bytes_t _M0L4dataS130;
  int32_t _M0L1iS131;
  int32_t _M0L12utf16__indexS132;
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_incref(_M0L1sS129);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L17codepoint__lengthS128
  = _M0MPC16string6String20char__length_2einner(_M0L1sS129, 0, 4294967296ll);
  _M0L6_2atmpS1026 = _M0L17codepoint__lengthS128 * 4;
  _M0L4dataS130 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1026, 0);
  _M0L1iS131 = 0;
  _M0L12utf16__indexS132 = 0;
  while (1) {
    if (_M0L1iS131 < _M0L17codepoint__lengthS128) {
      int32_t _M0L6_2atmpS1023;
      int32_t _M0L1cS133;
      int32_t _M0L6_2atmpS1024;
      int32_t _M0L6_2atmpS1025;
      moonbit_incref(_M0L1sS129);
      #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1023
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS129, _M0L12utf16__indexS132);
      _M0L1cS133 = _M0L6_2atmpS1023;
      if (_M0L1cS133 > 65535) {
        int32_t _M0L6_2atmpS991 = _M0L1iS131 * 4;
        int32_t _M0L6_2atmpS993 = _M0L1cS133 & 255;
        int32_t _M0L6_2atmpS992 = _M0L6_2atmpS993 & 0xff;
        int32_t _M0L6_2atmpS998;
        int32_t _M0L6_2atmpS994;
        int32_t _M0L6_2atmpS997;
        int32_t _M0L6_2atmpS996;
        int32_t _M0L6_2atmpS995;
        int32_t _M0L6_2atmpS1003;
        int32_t _M0L6_2atmpS999;
        int32_t _M0L6_2atmpS1002;
        int32_t _M0L6_2atmpS1001;
        int32_t _M0L6_2atmpS1000;
        int32_t _M0L6_2atmpS1008;
        int32_t _M0L6_2atmpS1004;
        int32_t _M0L6_2atmpS1007;
        int32_t _M0L6_2atmpS1006;
        int32_t _M0L6_2atmpS1005;
        int32_t _M0L6_2atmpS1009;
        int32_t _M0L6_2atmpS1010;
        if (
          _M0L6_2atmpS991 < 0
          || _M0L6_2atmpS991 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS991] = _M0L6_2atmpS992;
        _M0L6_2atmpS998 = _M0L1iS131 * 4;
        _M0L6_2atmpS994 = _M0L6_2atmpS998 + 1;
        _M0L6_2atmpS997 = _M0L1cS133 >> 8;
        _M0L6_2atmpS996 = _M0L6_2atmpS997 & 255;
        _M0L6_2atmpS995 = _M0L6_2atmpS996 & 0xff;
        if (
          _M0L6_2atmpS994 < 0
          || _M0L6_2atmpS994 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS994] = _M0L6_2atmpS995;
        _M0L6_2atmpS1003 = _M0L1iS131 * 4;
        _M0L6_2atmpS999 = _M0L6_2atmpS1003 + 2;
        _M0L6_2atmpS1002 = _M0L1cS133 >> 16;
        _M0L6_2atmpS1001 = _M0L6_2atmpS1002 & 255;
        _M0L6_2atmpS1000 = _M0L6_2atmpS1001 & 0xff;
        if (
          _M0L6_2atmpS999 < 0
          || _M0L6_2atmpS999 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS999] = _M0L6_2atmpS1000;
        _M0L6_2atmpS1008 = _M0L1iS131 * 4;
        _M0L6_2atmpS1004 = _M0L6_2atmpS1008 + 3;
        _M0L6_2atmpS1007 = _M0L1cS133 >> 24;
        _M0L6_2atmpS1006 = _M0L6_2atmpS1007 & 255;
        _M0L6_2atmpS1005 = _M0L6_2atmpS1006 & 0xff;
        if (
          _M0L6_2atmpS1004 < 0
          || _M0L6_2atmpS1004 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1004] = _M0L6_2atmpS1005;
        _M0L6_2atmpS1009 = _M0L1iS131 + 1;
        _M0L6_2atmpS1010 = _M0L12utf16__indexS132 + 2;
        _M0L1iS131 = _M0L6_2atmpS1009;
        _M0L12utf16__indexS132 = _M0L6_2atmpS1010;
        continue;
      } else {
        int32_t _M0L6_2atmpS1011 = _M0L1iS131 * 4;
        int32_t _M0L6_2atmpS1013 = _M0L1cS133 & 255;
        int32_t _M0L6_2atmpS1012 = _M0L6_2atmpS1013 & 0xff;
        int32_t _M0L6_2atmpS1018;
        int32_t _M0L6_2atmpS1014;
        int32_t _M0L6_2atmpS1017;
        int32_t _M0L6_2atmpS1016;
        int32_t _M0L6_2atmpS1015;
        int32_t _M0L6_2atmpS1020;
        int32_t _M0L6_2atmpS1019;
        int32_t _M0L6_2atmpS1022;
        int32_t _M0L6_2atmpS1021;
        if (
          _M0L6_2atmpS1011 < 0
          || _M0L6_2atmpS1011 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1011] = _M0L6_2atmpS1012;
        _M0L6_2atmpS1018 = _M0L1iS131 * 4;
        _M0L6_2atmpS1014 = _M0L6_2atmpS1018 + 1;
        _M0L6_2atmpS1017 = _M0L1cS133 >> 8;
        _M0L6_2atmpS1016 = _M0L6_2atmpS1017 & 255;
        _M0L6_2atmpS1015 = _M0L6_2atmpS1016 & 0xff;
        if (
          _M0L6_2atmpS1014 < 0
          || _M0L6_2atmpS1014 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1014] = _M0L6_2atmpS1015;
        _M0L6_2atmpS1020 = _M0L1iS131 * 4;
        _M0L6_2atmpS1019 = _M0L6_2atmpS1020 + 2;
        if (
          _M0L6_2atmpS1019 < 0
          || _M0L6_2atmpS1019 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1019] = 0;
        _M0L6_2atmpS1022 = _M0L1iS131 * 4;
        _M0L6_2atmpS1021 = _M0L6_2atmpS1022 + 3;
        if (
          _M0L6_2atmpS1021 < 0
          || _M0L6_2atmpS1021 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1021] = 0;
      }
      _M0L6_2atmpS1024 = _M0L1iS131 + 1;
      _M0L6_2atmpS1025 = _M0L12utf16__indexS132 + 1;
      _M0L1iS131 = _M0L6_2atmpS1024;
      _M0L12utf16__indexS132 = _M0L6_2atmpS1025;
      continue;
    } else {
      moonbit_decref(_M0L1sS129);
    }
    break;
  }
  #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS130);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS125,
  int32_t _M0L5indexS126
) {
  int32_t _M0L2c1S124;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S124 = _M0L4selfS125[_M0L5indexS126];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S124)) {
    int32_t _M0L6_2atmpS990 = _M0L5indexS126 + 1;
    int32_t _M0L6_2atmpS1976 = _M0L4selfS125[_M0L6_2atmpS990];
    int32_t _M0L2c2S127;
    int32_t _M0L6_2atmpS988;
    int32_t _M0L6_2atmpS989;
    moonbit_decref(_M0L4selfS125);
    _M0L2c2S127 = _M0L6_2atmpS1976;
    _M0L6_2atmpS988 = (int32_t)_M0L2c1S124;
    _M0L6_2atmpS989 = (int32_t)_M0L2c2S127;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS988, _M0L6_2atmpS989);
  } else {
    moonbit_decref(_M0L4selfS125);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S124);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS123) {
  int32_t _M0L6_2atmpS987;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS987 = (int32_t)_M0L4selfS123;
  return _M0L6_2atmpS987;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS121,
  int32_t _M0L8trailingS122
) {
  int32_t _M0L6_2atmpS986;
  int32_t _M0L6_2atmpS985;
  int32_t _M0L6_2atmpS984;
  int32_t _M0L6_2atmpS983;
  int32_t _M0L6_2atmpS982;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS986 = _M0L7leadingS121 - 55296;
  _M0L6_2atmpS985 = _M0L6_2atmpS986 * 1024;
  _M0L6_2atmpS984 = _M0L6_2atmpS985 + _M0L8trailingS122;
  _M0L6_2atmpS983 = _M0L6_2atmpS984 - 56320;
  _M0L6_2atmpS982 = _M0L6_2atmpS983 + 65536;
  return _M0L6_2atmpS982;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS114,
  int32_t _M0L13start__offsetS115,
  int64_t _M0L11end__offsetS112
) {
  int32_t _M0L11end__offsetS111;
  int32_t _if__result_2186;
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS112 == 4294967296ll) {
    _M0L11end__offsetS111 = Moonbit_array_length(_M0L4selfS114);
  } else {
    int64_t _M0L7_2aSomeS113 = _M0L11end__offsetS112;
    _M0L11end__offsetS111 = (int32_t)_M0L7_2aSomeS113;
  }
  if (_M0L13start__offsetS115 >= 0) {
    if (_M0L13start__offsetS115 <= _M0L11end__offsetS111) {
      int32_t _M0L6_2atmpS975 = Moonbit_array_length(_M0L4selfS114);
      _if__result_2186 = _M0L11end__offsetS111 <= _M0L6_2atmpS975;
    } else {
      _if__result_2186 = 0;
    }
  } else {
    _if__result_2186 = 0;
  }
  if (_if__result_2186) {
    int32_t _M0L12utf16__indexS116 = _M0L13start__offsetS115;
    int32_t _M0L11char__countS117 = 0;
    while (1) {
      if (_M0L12utf16__indexS116 < _M0L11end__offsetS111) {
        int32_t _M0L2c1S118 = _M0L4selfS114[_M0L12utf16__indexS116];
        int32_t _if__result_2188;
        int32_t _M0L6_2atmpS980;
        int32_t _M0L6_2atmpS981;
        #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S118)) {
          int32_t _M0L6_2atmpS976 = _M0L12utf16__indexS116 + 1;
          _if__result_2188 = _M0L6_2atmpS976 < _M0L11end__offsetS111;
        } else {
          _if__result_2188 = 0;
        }
        if (_if__result_2188) {
          int32_t _M0L6_2atmpS979 = _M0L12utf16__indexS116 + 1;
          int32_t _M0L2c2S119 = _M0L4selfS114[_M0L6_2atmpS979];
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S119)) {
            int32_t _M0L6_2atmpS977 = _M0L12utf16__indexS116 + 2;
            int32_t _M0L6_2atmpS978 = _M0L11char__countS117 + 1;
            _M0L12utf16__indexS116 = _M0L6_2atmpS977;
            _M0L11char__countS117 = _M0L6_2atmpS978;
            continue;
          } else {
            #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
            _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_41.data);
          }
        }
        _M0L6_2atmpS980 = _M0L12utf16__indexS116 + 1;
        _M0L6_2atmpS981 = _M0L11char__countS117 + 1;
        _M0L12utf16__indexS116 = _M0L6_2atmpS980;
        _M0L11char__countS117 = _M0L6_2atmpS981;
        continue;
      } else {
        moonbit_decref(_M0L4selfS114);
        return _M0L11char__countS117;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS114);
    #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_43.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS110) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS110 >= 56320) {
    return _M0L4selfS110 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS109) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS109 >= 55296) {
    return _M0L4selfS109 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS90) {
  struct _M0TPB13StringBuilder* _M0L3bufS88;
  int32_t _M0L3lenS89;
  int32_t _M0L3remS91;
  int32_t _M0L1iS92;
  #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L3bufS88 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS89 = Moonbit_array_length(_M0L4dataS90);
  _M0L3remS91 = _M0L3lenS89 % 3;
  _M0L1iS92 = 0;
  while (1) {
    int32_t _M0L6_2atmpS927 = _M0L3lenS89 - _M0L3remS91;
    if (_M0L1iS92 < _M0L6_2atmpS927) {
      int32_t _M0L6_2atmpS949;
      int32_t _M0L2b0S93;
      int32_t _M0L6_2atmpS948;
      int32_t _M0L6_2atmpS947;
      int32_t _M0L2b1S94;
      int32_t _M0L6_2atmpS946;
      int32_t _M0L6_2atmpS945;
      int32_t _M0L2b2S95;
      int32_t _M0L6_2atmpS944;
      int32_t _M0L6_2atmpS943;
      int32_t _M0L2x0S96;
      int32_t _M0L6_2atmpS942;
      int32_t _M0L6_2atmpS939;
      int32_t _M0L6_2atmpS941;
      int32_t _M0L6_2atmpS940;
      int32_t _M0L6_2atmpS938;
      int32_t _M0L2x1S97;
      int32_t _M0L6_2atmpS937;
      int32_t _M0L6_2atmpS934;
      int32_t _M0L6_2atmpS936;
      int32_t _M0L6_2atmpS935;
      int32_t _M0L6_2atmpS933;
      int32_t _M0L2x2S98;
      int32_t _M0L6_2atmpS932;
      int32_t _M0L2x3S99;
      int32_t _M0L6_2atmpS928;
      int32_t _M0L6_2atmpS929;
      int32_t _M0L6_2atmpS930;
      int32_t _M0L6_2atmpS931;
      int32_t _M0L6_2atmpS950;
      if (_M0L1iS92 < 0 || _M0L1iS92 >= Moonbit_array_length(_M0L4dataS90)) {
        #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS949 = (int32_t)_M0L4dataS90[_M0L1iS92];
      _M0L2b0S93 = (int32_t)_M0L6_2atmpS949;
      _M0L6_2atmpS948 = _M0L1iS92 + 1;
      if (
        _M0L6_2atmpS948 < 0
        || _M0L6_2atmpS948 >= Moonbit_array_length(_M0L4dataS90)
      ) {
        #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS947 = (int32_t)_M0L4dataS90[_M0L6_2atmpS948];
      _M0L2b1S94 = (int32_t)_M0L6_2atmpS947;
      _M0L6_2atmpS946 = _M0L1iS92 + 2;
      if (
        _M0L6_2atmpS946 < 0
        || _M0L6_2atmpS946 >= Moonbit_array_length(_M0L4dataS90)
      ) {
        #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS945 = (int32_t)_M0L4dataS90[_M0L6_2atmpS946];
      _M0L2b2S95 = (int32_t)_M0L6_2atmpS945;
      _M0L6_2atmpS944 = _M0L2b0S93 & 252;
      _M0L6_2atmpS943 = _M0L6_2atmpS944 >> 2;
      if (
        _M0L6_2atmpS943 < 0
        || _M0L6_2atmpS943
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x0S96 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS943];
      _M0L6_2atmpS942 = _M0L2b0S93 & 3;
      _M0L6_2atmpS939 = _M0L6_2atmpS942 << 4;
      _M0L6_2atmpS941 = _M0L2b1S94 & 240;
      _M0L6_2atmpS940 = _M0L6_2atmpS941 >> 4;
      _M0L6_2atmpS938 = _M0L6_2atmpS939 | _M0L6_2atmpS940;
      if (
        _M0L6_2atmpS938 < 0
        || _M0L6_2atmpS938
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x1S97 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS938];
      _M0L6_2atmpS937 = _M0L2b1S94 & 15;
      _M0L6_2atmpS934 = _M0L6_2atmpS937 << 2;
      _M0L6_2atmpS936 = _M0L2b2S95 & 192;
      _M0L6_2atmpS935 = _M0L6_2atmpS936 >> 6;
      _M0L6_2atmpS933 = _M0L6_2atmpS934 | _M0L6_2atmpS935;
      if (
        _M0L6_2atmpS933 < 0
        || _M0L6_2atmpS933
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x2S98 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS933];
      _M0L6_2atmpS932 = _M0L2b2S95 & 63;
      if (
        _M0L6_2atmpS932 < 0
        || _M0L6_2atmpS932
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x3S99 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS932];
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS928 = _M0MPC14byte4Byte8to__char(_M0L2x0S96);
      moonbit_incref(_M0L3bufS88);
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS928);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS929 = _M0MPC14byte4Byte8to__char(_M0L2x1S97);
      moonbit_incref(_M0L3bufS88);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS929);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS930 = _M0MPC14byte4Byte8to__char(_M0L2x2S98);
      moonbit_incref(_M0L3bufS88);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS930);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS931 = _M0MPC14byte4Byte8to__char(_M0L2x3S99);
      moonbit_incref(_M0L3bufS88);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS931);
      _M0L6_2atmpS950 = _M0L1iS92 + 3;
      _M0L1iS92 = _M0L6_2atmpS950;
      continue;
    }
    break;
  }
  if (_M0L3remS91 == 1) {
    int32_t _M0L6_2atmpS958 = _M0L3lenS89 - 1;
    int32_t _M0L6_2atmpS1977;
    int32_t _M0L6_2atmpS957;
    int32_t _M0L2b0S101;
    int32_t _M0L6_2atmpS956;
    int32_t _M0L6_2atmpS955;
    int32_t _M0L2x0S102;
    int32_t _M0L6_2atmpS954;
    int32_t _M0L6_2atmpS953;
    int32_t _M0L2x1S103;
    int32_t _M0L6_2atmpS951;
    int32_t _M0L6_2atmpS952;
    if (
      _M0L6_2atmpS958 < 0
      || _M0L6_2atmpS958 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1977 = (int32_t)_M0L4dataS90[_M0L6_2atmpS958];
    moonbit_decref(_M0L4dataS90);
    _M0L6_2atmpS957 = _M0L6_2atmpS1977;
    _M0L2b0S101 = (int32_t)_M0L6_2atmpS957;
    _M0L6_2atmpS956 = _M0L2b0S101 & 252;
    _M0L6_2atmpS955 = _M0L6_2atmpS956 >> 2;
    if (
      _M0L6_2atmpS955 < 0
      || _M0L6_2atmpS955
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S102 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS955];
    _M0L6_2atmpS954 = _M0L2b0S101 & 3;
    _M0L6_2atmpS953 = _M0L6_2atmpS954 << 4;
    if (
      _M0L6_2atmpS953 < 0
      || _M0L6_2atmpS953
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S103 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS953];
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS951 = _M0MPC14byte4Byte8to__char(_M0L2x0S102);
    moonbit_incref(_M0L3bufS88);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS951);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS952 = _M0MPC14byte4Byte8to__char(_M0L2x1S103);
    moonbit_incref(_M0L3bufS88);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS952);
    moonbit_incref(_M0L3bufS88);
    #line 85 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
    moonbit_incref(_M0L3bufS88);
    #line 86 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
  } else if (_M0L3remS91 == 2) {
    int32_t _M0L6_2atmpS974 = _M0L3lenS89 - 2;
    int32_t _M0L6_2atmpS973;
    int32_t _M0L2b0S104;
    int32_t _M0L6_2atmpS972;
    int32_t _M0L6_2atmpS1978;
    int32_t _M0L6_2atmpS971;
    int32_t _M0L2b1S105;
    int32_t _M0L6_2atmpS970;
    int32_t _M0L6_2atmpS969;
    int32_t _M0L2x0S106;
    int32_t _M0L6_2atmpS968;
    int32_t _M0L6_2atmpS965;
    int32_t _M0L6_2atmpS967;
    int32_t _M0L6_2atmpS966;
    int32_t _M0L6_2atmpS964;
    int32_t _M0L2x1S107;
    int32_t _M0L6_2atmpS963;
    int32_t _M0L6_2atmpS962;
    int32_t _M0L2x2S108;
    int32_t _M0L6_2atmpS959;
    int32_t _M0L6_2atmpS960;
    int32_t _M0L6_2atmpS961;
    if (
      _M0L6_2atmpS974 < 0
      || _M0L6_2atmpS974 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS973 = (int32_t)_M0L4dataS90[_M0L6_2atmpS974];
    _M0L2b0S104 = (int32_t)_M0L6_2atmpS973;
    _M0L6_2atmpS972 = _M0L3lenS89 - 1;
    if (
      _M0L6_2atmpS972 < 0
      || _M0L6_2atmpS972 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1978 = (int32_t)_M0L4dataS90[_M0L6_2atmpS972];
    moonbit_decref(_M0L4dataS90);
    _M0L6_2atmpS971 = _M0L6_2atmpS1978;
    _M0L2b1S105 = (int32_t)_M0L6_2atmpS971;
    _M0L6_2atmpS970 = _M0L2b0S104 & 252;
    _M0L6_2atmpS969 = _M0L6_2atmpS970 >> 2;
    if (
      _M0L6_2atmpS969 < 0
      || _M0L6_2atmpS969
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S106 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS969];
    _M0L6_2atmpS968 = _M0L2b0S104 & 3;
    _M0L6_2atmpS965 = _M0L6_2atmpS968 << 4;
    _M0L6_2atmpS967 = _M0L2b1S105 & 240;
    _M0L6_2atmpS966 = _M0L6_2atmpS967 >> 4;
    _M0L6_2atmpS964 = _M0L6_2atmpS965 | _M0L6_2atmpS966;
    if (
      _M0L6_2atmpS964 < 0
      || _M0L6_2atmpS964
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S107 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS964];
    _M0L6_2atmpS963 = _M0L2b1S105 & 15;
    _M0L6_2atmpS962 = _M0L6_2atmpS963 << 2;
    if (
      _M0L6_2atmpS962 < 0
      || _M0L6_2atmpS962
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x2S108 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS962];
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS959 = _M0MPC14byte4Byte8to__char(_M0L2x0S106);
    moonbit_incref(_M0L3bufS88);
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS959);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS960 = _M0MPC14byte4Byte8to__char(_M0L2x1S107);
    moonbit_incref(_M0L3bufS88);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS960);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS961 = _M0MPC14byte4Byte8to__char(_M0L2x2S108);
    moonbit_incref(_M0L3bufS88);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS961);
    moonbit_incref(_M0L3bufS88);
    #line 96 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
  } else {
    moonbit_decref(_M0L4dataS90);
  }
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS88);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS85,
  int32_t _M0L2chS87
) {
  int32_t _M0L3lenS922;
  int32_t _M0L6_2atmpS921;
  moonbit_bytes_t _M0L8_2afieldS1979;
  moonbit_bytes_t _M0L4dataS925;
  int32_t _M0L3lenS926;
  int32_t _M0L3incS86;
  int32_t _M0L3lenS924;
  int32_t _M0L6_2atmpS923;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS922 = _M0L4selfS85->$1;
  _M0L6_2atmpS921 = _M0L3lenS922 + 4;
  moonbit_incref(_M0L4selfS85);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS85, _M0L6_2atmpS921);
  _M0L8_2afieldS1979 = _M0L4selfS85->$0;
  _M0L4dataS925 = _M0L8_2afieldS1979;
  _M0L3lenS926 = _M0L4selfS85->$1;
  moonbit_incref(_M0L4dataS925);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS86
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS925, _M0L3lenS926, _M0L2chS87);
  _M0L3lenS924 = _M0L4selfS85->$1;
  _M0L6_2atmpS923 = _M0L3lenS924 + _M0L3incS86;
  _M0L4selfS85->$1 = _M0L6_2atmpS923;
  moonbit_decref(_M0L4selfS85);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS80,
  int32_t _M0L8requiredS81
) {
  moonbit_bytes_t _M0L8_2afieldS1983;
  moonbit_bytes_t _M0L4dataS920;
  int32_t _M0L6_2atmpS1982;
  int32_t _M0L12current__lenS79;
  int32_t _M0Lm13enough__spaceS82;
  int32_t _M0L6_2atmpS918;
  int32_t _M0L6_2atmpS919;
  moonbit_bytes_t _M0L9new__dataS84;
  moonbit_bytes_t _M0L8_2afieldS1981;
  moonbit_bytes_t _M0L4dataS916;
  int32_t _M0L3lenS917;
  moonbit_bytes_t _M0L6_2aoldS1980;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS1983 = _M0L4selfS80->$0;
  _M0L4dataS920 = _M0L8_2afieldS1983;
  _M0L6_2atmpS1982 = Moonbit_array_length(_M0L4dataS920);
  _M0L12current__lenS79 = _M0L6_2atmpS1982;
  if (_M0L8requiredS81 <= _M0L12current__lenS79) {
    moonbit_decref(_M0L4selfS80);
    return 0;
  }
  _M0Lm13enough__spaceS82 = _M0L12current__lenS79;
  while (1) {
    int32_t _M0L6_2atmpS914 = _M0Lm13enough__spaceS82;
    if (_M0L6_2atmpS914 < _M0L8requiredS81) {
      int32_t _M0L6_2atmpS915 = _M0Lm13enough__spaceS82;
      _M0Lm13enough__spaceS82 = _M0L6_2atmpS915 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS918 = _M0Lm13enough__spaceS82;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS919 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS84
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS918, _M0L6_2atmpS919);
  _M0L8_2afieldS1981 = _M0L4selfS80->$0;
  _M0L4dataS916 = _M0L8_2afieldS1981;
  _M0L3lenS917 = _M0L4selfS80->$1;
  moonbit_incref(_M0L4dataS916);
  moonbit_incref(_M0L9new__dataS84);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS84, 0, _M0L4dataS916, 0, _M0L3lenS917);
  _M0L6_2aoldS1980 = _M0L4selfS80->$0;
  moonbit_decref(_M0L6_2aoldS1980);
  _M0L4selfS80->$0 = _M0L9new__dataS84;
  moonbit_decref(_M0L4selfS80);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS74,
  int32_t _M0L6offsetS75,
  int32_t _M0L5valueS73
) {
  uint32_t _M0L4codeS72;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS72 = _M0MPC14char4Char8to__uint(_M0L5valueS73);
  if (_M0L4codeS72 < 65536u) {
    uint32_t _M0L6_2atmpS897 = _M0L4codeS72 & 255u;
    int32_t _M0L6_2atmpS896;
    int32_t _M0L6_2atmpS898;
    uint32_t _M0L6_2atmpS900;
    int32_t _M0L6_2atmpS899;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS896 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS897);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS896;
    _M0L6_2atmpS898 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS900 = _M0L4codeS72 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS899 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS900);
    if (
      _M0L6_2atmpS898 < 0
      || _M0L6_2atmpS898 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS898] = _M0L6_2atmpS899;
    moonbit_decref(_M0L4selfS74);
    return 2;
  } else if (_M0L4codeS72 < 1114112u) {
    uint32_t _M0L2hiS76 = _M0L4codeS72 - 65536u;
    uint32_t _M0L6_2atmpS913 = _M0L2hiS76 >> 10;
    uint32_t _M0L2loS77 = _M0L6_2atmpS913 | 55296u;
    uint32_t _M0L6_2atmpS912 = _M0L2hiS76 & 1023u;
    uint32_t _M0L2hiS78 = _M0L6_2atmpS912 | 56320u;
    uint32_t _M0L6_2atmpS902 = _M0L2loS77 & 255u;
    int32_t _M0L6_2atmpS901;
    int32_t _M0L6_2atmpS903;
    uint32_t _M0L6_2atmpS905;
    int32_t _M0L6_2atmpS904;
    int32_t _M0L6_2atmpS906;
    uint32_t _M0L6_2atmpS908;
    int32_t _M0L6_2atmpS907;
    int32_t _M0L6_2atmpS909;
    uint32_t _M0L6_2atmpS911;
    int32_t _M0L6_2atmpS910;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS901 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS902);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS901;
    _M0L6_2atmpS903 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS905 = _M0L2loS77 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS904 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS905);
    if (
      _M0L6_2atmpS903 < 0
      || _M0L6_2atmpS903 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS903] = _M0L6_2atmpS904;
    _M0L6_2atmpS906 = _M0L6offsetS75 + 2;
    _M0L6_2atmpS908 = _M0L2hiS78 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS907 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS908);
    if (
      _M0L6_2atmpS906 < 0
      || _M0L6_2atmpS906 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS906] = _M0L6_2atmpS907;
    _M0L6_2atmpS909 = _M0L6offsetS75 + 3;
    _M0L6_2atmpS911 = _M0L2hiS78 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS910 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS911);
    if (
      _M0L6_2atmpS909 < 0
      || _M0L6_2atmpS909 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS909] = _M0L6_2atmpS910;
    moonbit_decref(_M0L4selfS74);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS74);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_44.data, (moonbit_string_t)moonbit_string_literal_45.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS71) {
  int32_t _M0L6_2atmpS895;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS895 = *(int32_t*)&_M0L4selfS71;
  return _M0L6_2atmpS895 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS894;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS894 = _M0L4selfS70;
  return *(uint32_t*)&_M0L6_2atmpS894;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS69
) {
  moonbit_bytes_t _M0L8_2afieldS1985;
  moonbit_bytes_t _M0L4dataS893;
  moonbit_bytes_t _M0L6_2atmpS890;
  int32_t _M0L8_2afieldS1984;
  int32_t _M0L3lenS892;
  int64_t _M0L6_2atmpS891;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS1985 = _M0L4selfS69->$0;
  _M0L4dataS893 = _M0L8_2afieldS1985;
  moonbit_incref(_M0L4dataS893);
  _M0L6_2atmpS890 = _M0L4dataS893;
  _M0L8_2afieldS1984 = _M0L4selfS69->$1;
  moonbit_decref(_M0L4selfS69);
  _M0L3lenS892 = _M0L8_2afieldS1984;
  _M0L6_2atmpS891 = (int64_t)_M0L3lenS892;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS890, 0, _M0L6_2atmpS891);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS64,
  int32_t _M0L6offsetS68,
  int64_t _M0L6lengthS66
) {
  int32_t _M0L3lenS63;
  int32_t _M0L6lengthS65;
  int32_t _if__result_2191;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS63 = Moonbit_array_length(_M0L4selfS64);
  if (_M0L6lengthS66 == 4294967296ll) {
    _M0L6lengthS65 = _M0L3lenS63 - _M0L6offsetS68;
  } else {
    int64_t _M0L7_2aSomeS67 = _M0L6lengthS66;
    _M0L6lengthS65 = (int32_t)_M0L7_2aSomeS67;
  }
  if (_M0L6offsetS68 >= 0) {
    if (_M0L6lengthS65 >= 0) {
      int32_t _M0L6_2atmpS889 = _M0L6offsetS68 + _M0L6lengthS65;
      _if__result_2191 = _M0L6_2atmpS889 <= _M0L3lenS63;
    } else {
      _if__result_2191 = 0;
    }
  } else {
    _if__result_2191 = 0;
  }
  if (_if__result_2191) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS64, _M0L6offsetS68, _M0L6lengthS65);
  } else {
    moonbit_decref(_M0L4selfS64);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS61
) {
  int32_t _M0L7initialS60;
  moonbit_bytes_t _M0L4dataS62;
  struct _M0TPB13StringBuilder* _block_2192;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS61 < 1) {
    _M0L7initialS60 = 1;
  } else {
    _M0L7initialS60 = _M0L10size__hintS61;
  }
  _M0L4dataS62 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS60, 0);
  _block_2192
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2192)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2192->$0 = _M0L4dataS62;
  _block_2192->$1 = 0;
  return _block_2192;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS888;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS888 = (int32_t)_M0L4selfS59;
  return _M0L6_2atmpS888;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS49,
  int32_t _M0L11dst__offsetS50,
  moonbit_string_t* _M0L3srcS51,
  int32_t _M0L11src__offsetS52,
  int32_t _M0L3lenS53
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS49, _M0L11dst__offsetS50, _M0L3srcS51, _M0L11src__offsetS52, _M0L3lenS53);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS54,
  int32_t _M0L11dst__offsetS55,
  struct _M0TUsiE** _M0L3srcS56,
  int32_t _M0L11src__offsetS57,
  int32_t _M0L3lenS58
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS54, _M0L11dst__offsetS55, _M0L3srcS56, _M0L11src__offsetS57, _M0L3lenS58);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS22,
  int32_t _M0L11dst__offsetS24,
  moonbit_bytes_t _M0L3srcS23,
  int32_t _M0L11src__offsetS25,
  int32_t _M0L3lenS27
) {
  int32_t _if__result_2193;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS22 == _M0L3srcS23) {
    _if__result_2193 = _M0L11dst__offsetS24 < _M0L11src__offsetS25;
  } else {
    _if__result_2193 = 0;
  }
  if (_if__result_2193) {
    int32_t _M0L1iS26 = 0;
    while (1) {
      if (_M0L1iS26 < _M0L3lenS27) {
        int32_t _M0L6_2atmpS861 = _M0L11dst__offsetS24 + _M0L1iS26;
        int32_t _M0L6_2atmpS863 = _M0L11src__offsetS25 + _M0L1iS26;
        int32_t _M0L6_2atmpS862;
        int32_t _M0L6_2atmpS864;
        if (
          _M0L6_2atmpS863 < 0
          || _M0L6_2atmpS863 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS862 = (int32_t)_M0L3srcS23[_M0L6_2atmpS863];
        if (
          _M0L6_2atmpS861 < 0
          || _M0L6_2atmpS861 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS861] = _M0L6_2atmpS862;
        _M0L6_2atmpS864 = _M0L1iS26 + 1;
        _M0L1iS26 = _M0L6_2atmpS864;
        continue;
      } else {
        moonbit_decref(_M0L3srcS23);
        moonbit_decref(_M0L3dstS22);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS869 = _M0L3lenS27 - 1;
    int32_t _M0L1iS29 = _M0L6_2atmpS869;
    while (1) {
      if (_M0L1iS29 >= 0) {
        int32_t _M0L6_2atmpS865 = _M0L11dst__offsetS24 + _M0L1iS29;
        int32_t _M0L6_2atmpS867 = _M0L11src__offsetS25 + _M0L1iS29;
        int32_t _M0L6_2atmpS866;
        int32_t _M0L6_2atmpS868;
        if (
          _M0L6_2atmpS867 < 0
          || _M0L6_2atmpS867 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS866 = (int32_t)_M0L3srcS23[_M0L6_2atmpS867];
        if (
          _M0L6_2atmpS865 < 0
          || _M0L6_2atmpS865 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS865] = _M0L6_2atmpS866;
        _M0L6_2atmpS868 = _M0L1iS29 - 1;
        _M0L1iS29 = _M0L6_2atmpS868;
        continue;
      } else {
        moonbit_decref(_M0L3srcS23);
        moonbit_decref(_M0L3dstS22);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS31,
  int32_t _M0L11dst__offsetS33,
  moonbit_string_t* _M0L3srcS32,
  int32_t _M0L11src__offsetS34,
  int32_t _M0L3lenS36
) {
  int32_t _if__result_2196;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS31 == _M0L3srcS32) {
    _if__result_2196 = _M0L11dst__offsetS33 < _M0L11src__offsetS34;
  } else {
    _if__result_2196 = 0;
  }
  if (_if__result_2196) {
    int32_t _M0L1iS35 = 0;
    while (1) {
      if (_M0L1iS35 < _M0L3lenS36) {
        int32_t _M0L6_2atmpS870 = _M0L11dst__offsetS33 + _M0L1iS35;
        int32_t _M0L6_2atmpS872 = _M0L11src__offsetS34 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS1987;
        moonbit_string_t _M0L6_2atmpS871;
        moonbit_string_t _M0L6_2aoldS1986;
        int32_t _M0L6_2atmpS873;
        if (
          _M0L6_2atmpS872 < 0
          || _M0L6_2atmpS872 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1987 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS872];
        _M0L6_2atmpS871 = _M0L6_2atmpS1987;
        if (
          _M0L6_2atmpS870 < 0
          || _M0L6_2atmpS870 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1986 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS870];
        moonbit_incref(_M0L6_2atmpS871);
        moonbit_decref(_M0L6_2aoldS1986);
        _M0L3dstS31[_M0L6_2atmpS870] = _M0L6_2atmpS871;
        _M0L6_2atmpS873 = _M0L1iS35 + 1;
        _M0L1iS35 = _M0L6_2atmpS873;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS878 = _M0L3lenS36 - 1;
    int32_t _M0L1iS38 = _M0L6_2atmpS878;
    while (1) {
      if (_M0L1iS38 >= 0) {
        int32_t _M0L6_2atmpS874 = _M0L11dst__offsetS33 + _M0L1iS38;
        int32_t _M0L6_2atmpS876 = _M0L11src__offsetS34 + _M0L1iS38;
        moonbit_string_t _M0L6_2atmpS1989;
        moonbit_string_t _M0L6_2atmpS875;
        moonbit_string_t _M0L6_2aoldS1988;
        int32_t _M0L6_2atmpS877;
        if (
          _M0L6_2atmpS876 < 0
          || _M0L6_2atmpS876 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1989 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS876];
        _M0L6_2atmpS875 = _M0L6_2atmpS1989;
        if (
          _M0L6_2atmpS874 < 0
          || _M0L6_2atmpS874 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1988 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS874];
        moonbit_incref(_M0L6_2atmpS875);
        moonbit_decref(_M0L6_2aoldS1988);
        _M0L3dstS31[_M0L6_2atmpS874] = _M0L6_2atmpS875;
        _M0L6_2atmpS877 = _M0L1iS38 - 1;
        _M0L1iS38 = _M0L6_2atmpS877;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS40,
  int32_t _M0L11dst__offsetS42,
  struct _M0TUsiE** _M0L3srcS41,
  int32_t _M0L11src__offsetS43,
  int32_t _M0L3lenS45
) {
  int32_t _if__result_2199;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS40 == _M0L3srcS41) {
    _if__result_2199 = _M0L11dst__offsetS42 < _M0L11src__offsetS43;
  } else {
    _if__result_2199 = 0;
  }
  if (_if__result_2199) {
    int32_t _M0L1iS44 = 0;
    while (1) {
      if (_M0L1iS44 < _M0L3lenS45) {
        int32_t _M0L6_2atmpS879 = _M0L11dst__offsetS42 + _M0L1iS44;
        int32_t _M0L6_2atmpS881 = _M0L11src__offsetS43 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS1991;
        struct _M0TUsiE* _M0L6_2atmpS880;
        struct _M0TUsiE* _M0L6_2aoldS1990;
        int32_t _M0L6_2atmpS882;
        if (
          _M0L6_2atmpS881 < 0
          || _M0L6_2atmpS881 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1991 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS881];
        _M0L6_2atmpS880 = _M0L6_2atmpS1991;
        if (
          _M0L6_2atmpS879 < 0
          || _M0L6_2atmpS879 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1990 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS879];
        if (_M0L6_2atmpS880) {
          moonbit_incref(_M0L6_2atmpS880);
        }
        if (_M0L6_2aoldS1990) {
          moonbit_decref(_M0L6_2aoldS1990);
        }
        _M0L3dstS40[_M0L6_2atmpS879] = _M0L6_2atmpS880;
        _M0L6_2atmpS882 = _M0L1iS44 + 1;
        _M0L1iS44 = _M0L6_2atmpS882;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS887 = _M0L3lenS45 - 1;
    int32_t _M0L1iS47 = _M0L6_2atmpS887;
    while (1) {
      if (_M0L1iS47 >= 0) {
        int32_t _M0L6_2atmpS883 = _M0L11dst__offsetS42 + _M0L1iS47;
        int32_t _M0L6_2atmpS885 = _M0L11src__offsetS43 + _M0L1iS47;
        struct _M0TUsiE* _M0L6_2atmpS1993;
        struct _M0TUsiE* _M0L6_2atmpS884;
        struct _M0TUsiE* _M0L6_2aoldS1992;
        int32_t _M0L6_2atmpS886;
        if (
          _M0L6_2atmpS885 < 0
          || _M0L6_2atmpS885 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1993 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS885];
        _M0L6_2atmpS884 = _M0L6_2atmpS1993;
        if (
          _M0L6_2atmpS883 < 0
          || _M0L6_2atmpS883 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1992 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS883];
        if (_M0L6_2atmpS884) {
          moonbit_incref(_M0L6_2atmpS884);
        }
        if (_M0L6_2aoldS1992) {
          moonbit_decref(_M0L6_2aoldS1992);
        }
        _M0L3dstS40[_M0L6_2atmpS883] = _M0L6_2atmpS884;
        _M0L6_2atmpS886 = _M0L1iS47 - 1;
        _M0L1iS47 = _M0L6_2atmpS886;
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

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS16,
  moonbit_string_t _M0L3locS17
) {
  moonbit_string_t _M0L6_2atmpS850;
  moonbit_string_t _M0L6_2atmpS1996;
  moonbit_string_t _M0L6_2atmpS848;
  moonbit_string_t _M0L6_2atmpS849;
  moonbit_string_t _M0L6_2atmpS1995;
  moonbit_string_t _M0L6_2atmpS847;
  moonbit_string_t _M0L6_2atmpS1994;
  moonbit_string_t _M0L6_2atmpS846;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS850 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1996
  = moonbit_add_string(_M0L6_2atmpS850, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS850);
  _M0L6_2atmpS848 = _M0L6_2atmpS1996;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS849
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1995 = moonbit_add_string(_M0L6_2atmpS848, _M0L6_2atmpS849);
  moonbit_decref(_M0L6_2atmpS848);
  moonbit_decref(_M0L6_2atmpS849);
  _M0L6_2atmpS847 = _M0L6_2atmpS1995;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1994
  = moonbit_add_string(_M0L6_2atmpS847, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS847);
  _M0L6_2atmpS846 = _M0L6_2atmpS1994;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS846);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS18,
  moonbit_string_t _M0L3locS19
) {
  moonbit_string_t _M0L6_2atmpS855;
  moonbit_string_t _M0L6_2atmpS1999;
  moonbit_string_t _M0L6_2atmpS853;
  moonbit_string_t _M0L6_2atmpS854;
  moonbit_string_t _M0L6_2atmpS1998;
  moonbit_string_t _M0L6_2atmpS852;
  moonbit_string_t _M0L6_2atmpS1997;
  moonbit_string_t _M0L6_2atmpS851;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS855 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1999
  = moonbit_add_string(_M0L6_2atmpS855, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS855);
  _M0L6_2atmpS853 = _M0L6_2atmpS1999;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS854
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1998 = moonbit_add_string(_M0L6_2atmpS853, _M0L6_2atmpS854);
  moonbit_decref(_M0L6_2atmpS853);
  moonbit_decref(_M0L6_2atmpS854);
  _M0L6_2atmpS852 = _M0L6_2atmpS1998;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1997
  = moonbit_add_string(_M0L6_2atmpS852, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS852);
  _M0L6_2atmpS851 = _M0L6_2atmpS1997;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS851);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS860;
  moonbit_string_t _M0L6_2atmpS2002;
  moonbit_string_t _M0L6_2atmpS858;
  moonbit_string_t _M0L6_2atmpS859;
  moonbit_string_t _M0L6_2atmpS2001;
  moonbit_string_t _M0L6_2atmpS857;
  moonbit_string_t _M0L6_2atmpS2000;
  moonbit_string_t _M0L6_2atmpS856;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS860 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2002
  = moonbit_add_string(_M0L6_2atmpS860, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS860);
  _M0L6_2atmpS858 = _M0L6_2atmpS2002;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS859
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2001 = moonbit_add_string(_M0L6_2atmpS858, _M0L6_2atmpS859);
  moonbit_decref(_M0L6_2atmpS858);
  moonbit_decref(_M0L6_2atmpS859);
  _M0L6_2atmpS857 = _M0L6_2atmpS2001;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2000
  = moonbit_add_string(_M0L6_2atmpS857, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS857);
  _M0L6_2atmpS856 = _M0L6_2atmpS2000;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS856);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5valueS15
) {
  uint32_t _M0L3accS845;
  uint32_t _M0L6_2atmpS844;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS845 = _M0L4selfS14->$0;
  _M0L6_2atmpS844 = _M0L3accS845 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS844;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS842;
  uint32_t _M0L6_2atmpS843;
  uint32_t _M0L6_2atmpS841;
  uint32_t _M0L6_2atmpS840;
  uint32_t _M0L6_2atmpS839;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS842 = _M0L4selfS12->$0;
  _M0L6_2atmpS843 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS841 = _M0L3accS842 + _M0L6_2atmpS843;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS840 = _M0FPB4rotl(_M0L6_2atmpS841, 17);
  _M0L6_2atmpS839 = _M0L6_2atmpS840 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS839;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS836;
  int32_t _M0L6_2atmpS838;
  uint32_t _M0L6_2atmpS837;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS836 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS838 = 32 - _M0L1rS11;
  _M0L6_2atmpS837 = _M0L1xS10 >> (_M0L6_2atmpS838 & 31);
  return _M0L6_2atmpS836 | _M0L6_2atmpS837;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S6,
  struct _M0TPB6Logger _M0L10_2ax__4934S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS2003;
  int32_t _M0L6_2acntS2072;
  moonbit_string_t _M0L15_2a_2aarg__4935S8;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S6;
  _M0L8_2afieldS2003 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS2072 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS2072 > 1) {
    int32_t _M0L11_2anew__cntS2073 = _M0L6_2acntS2072 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS2073;
    moonbit_incref(_M0L8_2afieldS2003);
  } else if (_M0L6_2acntS2072 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__4935S8 = _M0L8_2afieldS2003;
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_48.data);
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S9, _M0L15_2a_2aarg__4935S8);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_49.data);
  return 0;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS797) {
  switch (Moonbit_object_tag(_M0L4_2aeS797)) {
    case 2: {
      moonbit_decref(_M0L4_2aeS797);
      return (moonbit_string_t)moonbit_string_literal_50.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS797);
      return (moonbit_string_t)moonbit_string_literal_51.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS797);
      return (moonbit_string_t)moonbit_string_literal_52.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS797);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS797);
      return (moonbit_string_t)moonbit_string_literal_53.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC14char4CharPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS820
) {
  struct _M0Y4Char* _M0L14_2aboxed__selfS821 =
    (struct _M0Y4Char*)_M0L11_2aobj__ptrS820;
  int32_t _M0L8_2afieldS2004 = _M0L14_2aboxed__selfS821->$0;
  int32_t _M0L7_2aselfS819;
  moonbit_decref(_M0L14_2aboxed__selfS821);
  _M0L7_2aselfS819 = _M0L8_2afieldS2004;
  return _M0IPC14char4CharPB4Show10to__string(_M0L7_2aselfS819);
}

int32_t _M0IPC14char4CharPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS817,
  struct _M0TPB6Logger _M0L8_2aparamS816
) {
  struct _M0Y4Char* _M0L14_2aboxed__selfS818 =
    (struct _M0Y4Char*)_M0L11_2aobj__ptrS817;
  int32_t _M0L8_2afieldS2005 = _M0L14_2aboxed__selfS818->$0;
  int32_t _M0L7_2aselfS815;
  moonbit_decref(_M0L14_2aboxed__selfS818);
  _M0L7_2aselfS815 = _M0L8_2afieldS2005;
  _M0IPC14char4CharPB4Show6output(_M0L7_2aselfS815, _M0L8_2aparamS816);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS814,
  int32_t _M0L8_2aparamS813
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS812 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS814;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS812, _M0L8_2aparamS813);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS811,
  struct _M0TPC16string10StringView _M0L8_2aparamS810
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS809 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS811;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS809, _M0L8_2aparamS810);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS808,
  moonbit_string_t _M0L8_2aparamS805,
  int32_t _M0L8_2aparamS806,
  int32_t _M0L8_2aparamS807
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS804 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS808;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS804, _M0L8_2aparamS805, _M0L8_2aparamS806, _M0L8_2aparamS807);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS803,
  moonbit_string_t _M0L8_2aparamS802
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS801 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS803;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS801, _M0L8_2aparamS802);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS835 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS834;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS833;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS724;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS832;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS831;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS830;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS829;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS723;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS828;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS827;
  _M0L6_2atmpS835[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal4uuid35____test__757569642e6d6274__0_2eclo);
  _M0L8_2atupleS834
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS834)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS834->$0
  = _M0FP48clawteam8clawteam8internal4uuid35____test__757569642e6d6274__0_2eclo;
  _M0L8_2atupleS834->$1 = _M0L6_2atmpS835;
  _M0L8_2atupleS833
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS833)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS833->$0 = 0;
  _M0L8_2atupleS833->$1 = _M0L8_2atupleS834;
  _M0L7_2abindS724
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS724[0] = _M0L8_2atupleS833;
  _M0L6_2atmpS832 = _M0L7_2abindS724;
  _M0L6_2atmpS831
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS832
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS830
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS831);
  _M0L8_2atupleS829
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS829)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS829->$0 = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L8_2atupleS829->$1 = _M0L6_2atmpS830;
  _M0L7_2abindS723
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS723[0] = _M0L8_2atupleS829;
  _M0L6_2atmpS828 = _M0L7_2abindS723;
  _M0L6_2atmpS827
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS828
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal4uuid48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS827);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS826;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS791;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS792;
  int32_t _M0L7_2abindS793;
  int32_t _M0L2__S794;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS826
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS791
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS791)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS791->$0 = _M0L6_2atmpS826;
  _M0L12async__testsS791->$1 = 0;
  #line 438 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS792
  = _M0FP48clawteam8clawteam8internal4uuid52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS793 = _M0L7_2abindS792->$1;
  _M0L2__S794 = 0;
  while (1) {
    if (_M0L2__S794 < _M0L7_2abindS793) {
      struct _M0TUsiE** _M0L8_2afieldS2009 = _M0L7_2abindS792->$0;
      struct _M0TUsiE** _M0L3bufS825 = _M0L8_2afieldS2009;
      struct _M0TUsiE* _M0L6_2atmpS2008 =
        (struct _M0TUsiE*)_M0L3bufS825[_M0L2__S794];
      struct _M0TUsiE* _M0L3argS795 = _M0L6_2atmpS2008;
      moonbit_string_t _M0L8_2afieldS2007 = _M0L3argS795->$0;
      moonbit_string_t _M0L6_2atmpS822 = _M0L8_2afieldS2007;
      int32_t _M0L8_2afieldS2006 = _M0L3argS795->$1;
      int32_t _M0L6_2atmpS823 = _M0L8_2afieldS2006;
      int32_t _M0L6_2atmpS824;
      moonbit_incref(_M0L6_2atmpS822);
      moonbit_incref(_M0L12async__testsS791);
      #line 439 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal4uuid44moonbit__test__driver__internal__do__execute(_M0L12async__testsS791, _M0L6_2atmpS822, _M0L6_2atmpS823);
      _M0L6_2atmpS824 = _M0L2__S794 + 1;
      _M0L2__S794 = _M0L6_2atmpS824;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS792);
    }
    break;
  }
  #line 441 "E:\\moonbit\\clawteam\\internal\\uuid\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal4uuid28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal4uuid34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS791);
  return 0;
}