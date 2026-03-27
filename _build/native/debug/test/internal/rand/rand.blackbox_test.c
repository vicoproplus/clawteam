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

struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPB4Json5Array;

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE2Ok;

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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal20rand__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE3Err;

struct _M0TWEOUsRPB4JsonE;

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

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0R38String_3a_3aiter_2eanon__u2003__l247__;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TPC15bytes9BytesView;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal20rand__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__;

struct _M0TPB5ArrayGsE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__;

struct _M0DTPC15error5Error123clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno;

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

struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE2Ok {
  moonbit_bytes_t $0;
  
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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal20rand__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE3Err {
  void* $0;
  
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

struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2003__l247__ {
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

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal20rand__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
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

struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0DTPC15error5Error123clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno {
  int32_t $0;
  
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

struct moonbit_result_1 {
  int tag;
  union { moonbit_bytes_t ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal20rand__blackbox__test49____test__72616e645f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1373(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1364(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testC3533l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testC3529l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1297(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1292(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1279(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal20rand__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal20rand__blackbox__test39____test__72616e645f746573742e6d6274__0(
  
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal4rand5bytes(int32_t);

#define _M0FP48clawteam8clawteam8internal4rand11rand__bytes moonbit_moonclaw_rand_bytes

int32_t _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(
  void*,
  struct _M0TPB6Logger
);

#define _M0FP48clawteam8clawteam8internal5errno15errno__strerror moonbit_moonclaw_errno_strerror

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void*,
  int32_t
);

#define _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte moonbit_moonclaw_c_load_byte

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void*,
  int32_t
);

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(void*);

#define _M0MP48clawteam8clawteam8internal1c7Pointer10__is__null moonbit_moonclaw_c_is_null

#define _M0FP48clawteam8clawteam8internal1c6strlen moonbit_moonclaw_c_strlen

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView,
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

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15bytes9BytesView6length(struct _M0TPC15bytes9BytesView);

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

moonbit_bytes_t _M0MPC15bytes5Bytes4make(int32_t, int32_t);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2206l591(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2022l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2003l247(struct _M0TWEOc*);

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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(
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

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
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

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
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

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

int32_t moonbit_moonclaw_c_load_byte(void*, int32_t);

int32_t moonbit_moonclaw_c_is_null(void*);

int32_t moonbit_moonclaw_rand_bytes(moonbit_bytes_t);

void* moonbit_moonclaw_errno_strerror(int32_t);

uint64_t moonbit_moonclaw_c_strlen(void*);

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 114, 
    97, 110, 100, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 
    115, 116, 58, 114, 97, 110, 100, 95, 116, 101, 115, 116, 46, 109, 
    98, 116, 58, 52, 58, 51, 45, 52, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 114, 
    97, 110, 100, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 
    115, 116, 58, 114, 97, 110, 100, 95, 116, 101, 115, 116, 46, 109, 
    98, 116, 58, 52, 58, 49, 54, 45, 52, 58, 51, 48, 0
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
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[112]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 111), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 114, 97, 
    110, 100, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_44 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 114, 97, 110, 100, 
    34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
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
} const moonbit_string_literal_71 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 56, 
    48, 58, 53, 45, 49, 56, 48, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_32 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_23 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    114, 97, 110, 100, 95, 116, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    98, 121, 116, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_64 =
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
} const moonbit_string_literal_14 =
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
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_48 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    69, 114, 114, 110, 111, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 114, 
    97, 110, 100, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 
    115, 116, 58, 114, 97, 110, 100, 95, 116, 101, 115, 116, 46, 109, 
    98, 116, 58, 52, 58, 52, 48, 45, 52, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[114]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 113), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 114, 97, 
    110, 100, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 
    107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_61 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    114, 97, 110, 100, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[1]; 
} const moonbit_bytes_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 0), 0};

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1373$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1373
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal20rand__blackbox__test49____test__72616e645f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal20rand__blackbox__test49____test__72616e645f746573742e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal20rand__blackbox__test45____test__72616e645f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal20rand__blackbox__test49____test__72616e645f746573742e6d6274__0_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB30ryu__to__string_2erecord_2f903$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f903 =
  &_M0FPB30ryu__to__string_2erecord_2f903$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal20rand__blackbox__test49____test__72616e645f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3564
) {
  return _M0FP48clawteam8clawteam8internal20rand__blackbox__test39____test__72616e645f746573742e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1394,
  moonbit_string_t _M0L8filenameS1369,
  int32_t _M0L5indexS1372
) {
  struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364* _closure_4217;
  struct _M0TWssbEu* _M0L14handle__resultS1364;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1373;
  void* _M0L11_2atry__errS1388;
  struct moonbit_result_0 _tmp_4219;
  int32_t _handle__error__result_4220;
  int32_t _M0L6_2atmpS3552;
  void* _M0L3errS1389;
  moonbit_string_t _M0L4nameS1391;
  struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1392;
  moonbit_string_t _M0L8_2afieldS3565;
  int32_t _M0L6_2acntS4134;
  moonbit_string_t _M0L7_2anameS1393;
  #line 526 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1369);
  _closure_4217
  = (struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364*)moonbit_malloc(sizeof(struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364));
  Moonbit_object_header(_closure_4217)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364, $1) >> 2, 1, 0);
  _closure_4217->code
  = &_M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1364;
  _closure_4217->$0 = _M0L5indexS1372;
  _closure_4217->$1 = _M0L8filenameS1369;
  _M0L14handle__resultS1364 = (struct _M0TWssbEu*)_closure_4217;
  _M0L17error__to__stringS1373
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1373$closure.data;
  moonbit_incref(_M0L12async__testsS1394);
  moonbit_incref(_M0L17error__to__stringS1373);
  moonbit_incref(_M0L8filenameS1369);
  moonbit_incref(_M0L14handle__resultS1364);
  #line 560 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4219
  = _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1394, _M0L8filenameS1369, _M0L5indexS1372, _M0L14handle__resultS1364, _M0L17error__to__stringS1373);
  if (_tmp_4219.tag) {
    int32_t const _M0L5_2aokS3561 = _tmp_4219.data.ok;
    _handle__error__result_4220 = _M0L5_2aokS3561;
  } else {
    void* const _M0L6_2aerrS3562 = _tmp_4219.data.err;
    moonbit_decref(_M0L12async__testsS1394);
    moonbit_decref(_M0L17error__to__stringS1373);
    moonbit_decref(_M0L8filenameS1369);
    _M0L11_2atry__errS1388 = _M0L6_2aerrS3562;
    goto join_1387;
  }
  if (_handle__error__result_4220) {
    moonbit_decref(_M0L12async__testsS1394);
    moonbit_decref(_M0L17error__to__stringS1373);
    moonbit_decref(_M0L8filenameS1369);
    _M0L6_2atmpS3552 = 1;
  } else {
    struct moonbit_result_0 _tmp_4221;
    int32_t _handle__error__result_4222;
    moonbit_incref(_M0L12async__testsS1394);
    moonbit_incref(_M0L17error__to__stringS1373);
    moonbit_incref(_M0L8filenameS1369);
    moonbit_incref(_M0L14handle__resultS1364);
    #line 563 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    _tmp_4221
    = _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1394, _M0L8filenameS1369, _M0L5indexS1372, _M0L14handle__resultS1364, _M0L17error__to__stringS1373);
    if (_tmp_4221.tag) {
      int32_t const _M0L5_2aokS3559 = _tmp_4221.data.ok;
      _handle__error__result_4222 = _M0L5_2aokS3559;
    } else {
      void* const _M0L6_2aerrS3560 = _tmp_4221.data.err;
      moonbit_decref(_M0L12async__testsS1394);
      moonbit_decref(_M0L17error__to__stringS1373);
      moonbit_decref(_M0L8filenameS1369);
      _M0L11_2atry__errS1388 = _M0L6_2aerrS3560;
      goto join_1387;
    }
    if (_handle__error__result_4222) {
      moonbit_decref(_M0L12async__testsS1394);
      moonbit_decref(_M0L17error__to__stringS1373);
      moonbit_decref(_M0L8filenameS1369);
      _M0L6_2atmpS3552 = 1;
    } else {
      struct moonbit_result_0 _tmp_4223;
      int32_t _handle__error__result_4224;
      moonbit_incref(_M0L12async__testsS1394);
      moonbit_incref(_M0L17error__to__stringS1373);
      moonbit_incref(_M0L8filenameS1369);
      moonbit_incref(_M0L14handle__resultS1364);
      #line 566 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _tmp_4223
      = _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1394, _M0L8filenameS1369, _M0L5indexS1372, _M0L14handle__resultS1364, _M0L17error__to__stringS1373);
      if (_tmp_4223.tag) {
        int32_t const _M0L5_2aokS3557 = _tmp_4223.data.ok;
        _handle__error__result_4224 = _M0L5_2aokS3557;
      } else {
        void* const _M0L6_2aerrS3558 = _tmp_4223.data.err;
        moonbit_decref(_M0L12async__testsS1394);
        moonbit_decref(_M0L17error__to__stringS1373);
        moonbit_decref(_M0L8filenameS1369);
        _M0L11_2atry__errS1388 = _M0L6_2aerrS3558;
        goto join_1387;
      }
      if (_handle__error__result_4224) {
        moonbit_decref(_M0L12async__testsS1394);
        moonbit_decref(_M0L17error__to__stringS1373);
        moonbit_decref(_M0L8filenameS1369);
        _M0L6_2atmpS3552 = 1;
      } else {
        struct moonbit_result_0 _tmp_4225;
        int32_t _handle__error__result_4226;
        moonbit_incref(_M0L12async__testsS1394);
        moonbit_incref(_M0L17error__to__stringS1373);
        moonbit_incref(_M0L8filenameS1369);
        moonbit_incref(_M0L14handle__resultS1364);
        #line 569 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        _tmp_4225
        = _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1394, _M0L8filenameS1369, _M0L5indexS1372, _M0L14handle__resultS1364, _M0L17error__to__stringS1373);
        if (_tmp_4225.tag) {
          int32_t const _M0L5_2aokS3555 = _tmp_4225.data.ok;
          _handle__error__result_4226 = _M0L5_2aokS3555;
        } else {
          void* const _M0L6_2aerrS3556 = _tmp_4225.data.err;
          moonbit_decref(_M0L12async__testsS1394);
          moonbit_decref(_M0L17error__to__stringS1373);
          moonbit_decref(_M0L8filenameS1369);
          _M0L11_2atry__errS1388 = _M0L6_2aerrS3556;
          goto join_1387;
        }
        if (_handle__error__result_4226) {
          moonbit_decref(_M0L12async__testsS1394);
          moonbit_decref(_M0L17error__to__stringS1373);
          moonbit_decref(_M0L8filenameS1369);
          _M0L6_2atmpS3552 = 1;
        } else {
          struct moonbit_result_0 _tmp_4227;
          moonbit_incref(_M0L14handle__resultS1364);
          #line 572 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
          _tmp_4227
          = _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1394, _M0L8filenameS1369, _M0L5indexS1372, _M0L14handle__resultS1364, _M0L17error__to__stringS1373);
          if (_tmp_4227.tag) {
            int32_t const _M0L5_2aokS3553 = _tmp_4227.data.ok;
            _M0L6_2atmpS3552 = _M0L5_2aokS3553;
          } else {
            void* const _M0L6_2aerrS3554 = _tmp_4227.data.err;
            _M0L11_2atry__errS1388 = _M0L6_2aerrS3554;
            goto join_1387;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3552) {
    void* _M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3563 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3563)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
    ((struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3563)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1388
    = _M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3563;
    goto join_1387;
  } else {
    moonbit_decref(_M0L14handle__resultS1364);
  }
  goto joinlet_4218;
  join_1387:;
  _M0L3errS1389 = _M0L11_2atry__errS1388;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1392
  = (struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1389;
  _M0L8_2afieldS3565 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1392->$0;
  _M0L6_2acntS4134
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1392)->rc;
  if (_M0L6_2acntS4134 > 1) {
    int32_t _M0L11_2anew__cntS4135 = _M0L6_2acntS4134 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1392)->rc
    = _M0L11_2anew__cntS4135;
    moonbit_incref(_M0L8_2afieldS3565);
  } else if (_M0L6_2acntS4134 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1392);
  }
  _M0L7_2anameS1393 = _M0L8_2afieldS3565;
  _M0L4nameS1391 = _M0L7_2anameS1393;
  goto join_1390;
  goto joinlet_4228;
  join_1390:;
  #line 580 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1364(_M0L14handle__resultS1364, _M0L4nameS1391, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4228:;
  joinlet_4218:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1373(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3551,
  void* _M0L3errS1374
) {
  void* _M0L1eS1376;
  moonbit_string_t _M0L1eS1378;
  #line 549 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3551);
  switch (Moonbit_object_tag(_M0L3errS1374)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1379 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1374;
      moonbit_string_t _M0L8_2afieldS3566 = _M0L10_2aFailureS1379->$0;
      int32_t _M0L6_2acntS4136 =
        Moonbit_object_header(_M0L10_2aFailureS1379)->rc;
      moonbit_string_t _M0L4_2aeS1380;
      if (_M0L6_2acntS4136 > 1) {
        int32_t _M0L11_2anew__cntS4137 = _M0L6_2acntS4136 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1379)->rc
        = _M0L11_2anew__cntS4137;
        moonbit_incref(_M0L8_2afieldS3566);
      } else if (_M0L6_2acntS4136 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1379);
      }
      _M0L4_2aeS1380 = _M0L8_2afieldS3566;
      _M0L1eS1378 = _M0L4_2aeS1380;
      goto join_1377;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1381 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1374;
      moonbit_string_t _M0L8_2afieldS3567 = _M0L15_2aInspectErrorS1381->$0;
      int32_t _M0L6_2acntS4138 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1381)->rc;
      moonbit_string_t _M0L4_2aeS1382;
      if (_M0L6_2acntS4138 > 1) {
        int32_t _M0L11_2anew__cntS4139 = _M0L6_2acntS4138 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1381)->rc
        = _M0L11_2anew__cntS4139;
        moonbit_incref(_M0L8_2afieldS3567);
      } else if (_M0L6_2acntS4138 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1381);
      }
      _M0L4_2aeS1382 = _M0L8_2afieldS3567;
      _M0L1eS1378 = _M0L4_2aeS1382;
      goto join_1377;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1383 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1374;
      moonbit_string_t _M0L8_2afieldS3568 = _M0L16_2aSnapshotErrorS1383->$0;
      int32_t _M0L6_2acntS4140 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1383)->rc;
      moonbit_string_t _M0L4_2aeS1384;
      if (_M0L6_2acntS4140 > 1) {
        int32_t _M0L11_2anew__cntS4141 = _M0L6_2acntS4140 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1383)->rc
        = _M0L11_2anew__cntS4141;
        moonbit_incref(_M0L8_2afieldS3568);
      } else if (_M0L6_2acntS4140 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1383);
      }
      _M0L4_2aeS1384 = _M0L8_2afieldS3568;
      _M0L1eS1378 = _M0L4_2aeS1384;
      goto join_1377;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error123clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1385 =
        (struct _M0DTPC15error5Error123clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1374;
      moonbit_string_t _M0L8_2afieldS3569 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1385->$0;
      int32_t _M0L6_2acntS4142 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1385)->rc;
      moonbit_string_t _M0L4_2aeS1386;
      if (_M0L6_2acntS4142 > 1) {
        int32_t _M0L11_2anew__cntS4143 = _M0L6_2acntS4142 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1385)->rc
        = _M0L11_2anew__cntS4143;
        moonbit_incref(_M0L8_2afieldS3569);
      } else if (_M0L6_2acntS4142 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1385);
      }
      _M0L4_2aeS1386 = _M0L8_2afieldS3569;
      _M0L1eS1378 = _M0L4_2aeS1386;
      goto join_1377;
      break;
    }
    default: {
      _M0L1eS1376 = _M0L3errS1374;
      goto join_1375;
      break;
    }
  }
  join_1377:;
  return _M0L1eS1378;
  join_1375:;
  #line 555 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1376);
}

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1364(
  struct _M0TWssbEu* _M0L6_2aenvS3537,
  moonbit_string_t _M0L8testnameS1365,
  moonbit_string_t _M0L7messageS1366,
  int32_t _M0L7skippedS1367
) {
  struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364* _M0L14_2acasted__envS3538;
  moonbit_string_t _M0L8_2afieldS3579;
  moonbit_string_t _M0L8filenameS1369;
  int32_t _M0L8_2afieldS3578;
  int32_t _M0L6_2acntS4144;
  int32_t _M0L5indexS1372;
  int32_t _if__result_4231;
  moonbit_string_t _M0L10file__nameS1368;
  moonbit_string_t _M0L10test__nameS1370;
  moonbit_string_t _M0L7messageS1371;
  moonbit_string_t _M0L6_2atmpS3550;
  moonbit_string_t _M0L6_2atmpS3577;
  moonbit_string_t _M0L6_2atmpS3549;
  moonbit_string_t _M0L6_2atmpS3576;
  moonbit_string_t _M0L6_2atmpS3547;
  moonbit_string_t _M0L6_2atmpS3548;
  moonbit_string_t _M0L6_2atmpS3575;
  moonbit_string_t _M0L6_2atmpS3546;
  moonbit_string_t _M0L6_2atmpS3574;
  moonbit_string_t _M0L6_2atmpS3544;
  moonbit_string_t _M0L6_2atmpS3545;
  moonbit_string_t _M0L6_2atmpS3573;
  moonbit_string_t _M0L6_2atmpS3543;
  moonbit_string_t _M0L6_2atmpS3572;
  moonbit_string_t _M0L6_2atmpS3541;
  moonbit_string_t _M0L6_2atmpS3542;
  moonbit_string_t _M0L6_2atmpS3571;
  moonbit_string_t _M0L6_2atmpS3540;
  moonbit_string_t _M0L6_2atmpS3570;
  moonbit_string_t _M0L6_2atmpS3539;
  #line 533 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3538
  = (struct _M0R127_24clawteam_2fclawteam_2finternal_2frand__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1364*)_M0L6_2aenvS3537;
  _M0L8_2afieldS3579 = _M0L14_2acasted__envS3538->$1;
  _M0L8filenameS1369 = _M0L8_2afieldS3579;
  _M0L8_2afieldS3578 = _M0L14_2acasted__envS3538->$0;
  _M0L6_2acntS4144 = Moonbit_object_header(_M0L14_2acasted__envS3538)->rc;
  if (_M0L6_2acntS4144 > 1) {
    int32_t _M0L11_2anew__cntS4145 = _M0L6_2acntS4144 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3538)->rc
    = _M0L11_2anew__cntS4145;
    moonbit_incref(_M0L8filenameS1369);
  } else if (_M0L6_2acntS4144 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3538);
  }
  _M0L5indexS1372 = _M0L8_2afieldS3578;
  if (!_M0L7skippedS1367) {
    _if__result_4231 = 1;
  } else {
    _if__result_4231 = 0;
  }
  if (_if__result_4231) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1368 = _M0MPC16string6String6escape(_M0L8filenameS1369);
  #line 540 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1370 = _M0MPC16string6String6escape(_M0L8testnameS1365);
  #line 541 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1371 = _M0MPC16string6String6escape(_M0L7messageS1366);
  #line 542 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3550
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1368);
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3577
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3550);
  moonbit_decref(_M0L6_2atmpS3550);
  _M0L6_2atmpS3549 = _M0L6_2atmpS3577;
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3576
  = moonbit_add_string(_M0L6_2atmpS3549, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3549);
  _M0L6_2atmpS3547 = _M0L6_2atmpS3576;
  #line 544 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3548
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1372);
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3575 = moonbit_add_string(_M0L6_2atmpS3547, _M0L6_2atmpS3548);
  moonbit_decref(_M0L6_2atmpS3547);
  moonbit_decref(_M0L6_2atmpS3548);
  _M0L6_2atmpS3546 = _M0L6_2atmpS3575;
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3574
  = moonbit_add_string(_M0L6_2atmpS3546, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3546);
  _M0L6_2atmpS3544 = _M0L6_2atmpS3574;
  #line 544 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3545
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1370);
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3573 = moonbit_add_string(_M0L6_2atmpS3544, _M0L6_2atmpS3545);
  moonbit_decref(_M0L6_2atmpS3544);
  moonbit_decref(_M0L6_2atmpS3545);
  _M0L6_2atmpS3543 = _M0L6_2atmpS3573;
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3572
  = moonbit_add_string(_M0L6_2atmpS3543, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3543);
  _M0L6_2atmpS3541 = _M0L6_2atmpS3572;
  #line 544 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3542
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1371);
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3571 = moonbit_add_string(_M0L6_2atmpS3541, _M0L6_2atmpS3542);
  moonbit_decref(_M0L6_2atmpS3541);
  moonbit_decref(_M0L6_2atmpS3542);
  _M0L6_2atmpS3540 = _M0L6_2atmpS3571;
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3570
  = moonbit_add_string(_M0L6_2atmpS3540, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3540);
  _M0L6_2atmpS3539 = _M0L6_2atmpS3570;
  #line 543 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3539);
  #line 546 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1363,
  moonbit_string_t _M0L8filenameS1360,
  int32_t _M0L5indexS1354,
  struct _M0TWssbEu* _M0L14handle__resultS1350,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1352
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1330;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1359;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1332;
  moonbit_string_t* _M0L5attrsS1333;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1353;
  moonbit_string_t _M0L4nameS1336;
  moonbit_string_t _M0L4nameS1334;
  int32_t _M0L6_2atmpS3536;
  struct _M0TWEOs* _M0L5_2aitS1338;
  struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__* _closure_4240;
  struct _M0TWEOc* _M0L6_2atmpS3527;
  struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__* _closure_4241;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3528;
  struct moonbit_result_0 _result_4242;
  #line 407 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1363);
  moonbit_incref(_M0FP48clawteam8clawteam8internal20rand__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1359
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal20rand__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1360);
  if (_M0L7_2abindS1359 == 0) {
    struct moonbit_result_0 _result_4233;
    if (_M0L7_2abindS1359) {
      moonbit_decref(_M0L7_2abindS1359);
    }
    moonbit_decref(_M0L17error__to__stringS1352);
    moonbit_decref(_M0L14handle__resultS1350);
    _result_4233.tag = 1;
    _result_4233.data.ok = 0;
    return _result_4233;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1361 =
      _M0L7_2abindS1359;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1362 =
      _M0L7_2aSomeS1361;
    _M0L10index__mapS1330 = _M0L13_2aindex__mapS1362;
    goto join_1329;
  }
  join_1329:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1353
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1330, _M0L5indexS1354);
  if (_M0L7_2abindS1353 == 0) {
    struct moonbit_result_0 _result_4235;
    if (_M0L7_2abindS1353) {
      moonbit_decref(_M0L7_2abindS1353);
    }
    moonbit_decref(_M0L17error__to__stringS1352);
    moonbit_decref(_M0L14handle__resultS1350);
    _result_4235.tag = 1;
    _result_4235.data.ok = 0;
    return _result_4235;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1355 =
      _M0L7_2abindS1353;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1356 = _M0L7_2aSomeS1355;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3583 = _M0L4_2axS1356->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1357 = _M0L8_2afieldS3583;
    moonbit_string_t* _M0L8_2afieldS3582 = _M0L4_2axS1356->$1;
    int32_t _M0L6_2acntS4146 = Moonbit_object_header(_M0L4_2axS1356)->rc;
    moonbit_string_t* _M0L8_2aattrsS1358;
    if (_M0L6_2acntS4146 > 1) {
      int32_t _M0L11_2anew__cntS4147 = _M0L6_2acntS4146 - 1;
      Moonbit_object_header(_M0L4_2axS1356)->rc = _M0L11_2anew__cntS4147;
      moonbit_incref(_M0L8_2afieldS3582);
      moonbit_incref(_M0L4_2afS1357);
    } else if (_M0L6_2acntS4146 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1356);
    }
    _M0L8_2aattrsS1358 = _M0L8_2afieldS3582;
    _M0L1fS1332 = _M0L4_2afS1357;
    _M0L5attrsS1333 = _M0L8_2aattrsS1358;
    goto join_1331;
  }
  join_1331:;
  _M0L6_2atmpS3536 = Moonbit_array_length(_M0L5attrsS1333);
  if (_M0L6_2atmpS3536 >= 1) {
    moonbit_string_t _M0L6_2atmpS3581 = (moonbit_string_t)_M0L5attrsS1333[0];
    moonbit_string_t _M0L7_2anameS1337 = _M0L6_2atmpS3581;
    moonbit_incref(_M0L7_2anameS1337);
    _M0L4nameS1336 = _M0L7_2anameS1337;
    goto join_1335;
  } else {
    _M0L4nameS1334 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4236;
  join_1335:;
  _M0L4nameS1334 = _M0L4nameS1336;
  joinlet_4236:;
  #line 417 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1338 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1333);
  while (1) {
    moonbit_string_t _M0L4attrS1340;
    moonbit_string_t _M0L7_2abindS1347;
    int32_t _M0L6_2atmpS3520;
    int64_t _M0L6_2atmpS3519;
    moonbit_incref(_M0L5_2aitS1338);
    #line 419 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1347 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1338);
    if (_M0L7_2abindS1347 == 0) {
      if (_M0L7_2abindS1347) {
        moonbit_decref(_M0L7_2abindS1347);
      }
      moonbit_decref(_M0L5_2aitS1338);
    } else {
      moonbit_string_t _M0L7_2aSomeS1348 = _M0L7_2abindS1347;
      moonbit_string_t _M0L7_2aattrS1349 = _M0L7_2aSomeS1348;
      _M0L4attrS1340 = _M0L7_2aattrS1349;
      goto join_1339;
    }
    goto joinlet_4238;
    join_1339:;
    _M0L6_2atmpS3520 = Moonbit_array_length(_M0L4attrS1340);
    _M0L6_2atmpS3519 = (int64_t)_M0L6_2atmpS3520;
    moonbit_incref(_M0L4attrS1340);
    #line 420 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1340, 5, 0, _M0L6_2atmpS3519)
    ) {
      int32_t _M0L6_2atmpS3526 = _M0L4attrS1340[0];
      int32_t _M0L4_2axS1341 = _M0L6_2atmpS3526;
      if (_M0L4_2axS1341 == 112) {
        int32_t _M0L6_2atmpS3525 = _M0L4attrS1340[1];
        int32_t _M0L4_2axS1342 = _M0L6_2atmpS3525;
        if (_M0L4_2axS1342 == 97) {
          int32_t _M0L6_2atmpS3524 = _M0L4attrS1340[2];
          int32_t _M0L4_2axS1343 = _M0L6_2atmpS3524;
          if (_M0L4_2axS1343 == 110) {
            int32_t _M0L6_2atmpS3523 = _M0L4attrS1340[3];
            int32_t _M0L4_2axS1344 = _M0L6_2atmpS3523;
            if (_M0L4_2axS1344 == 105) {
              int32_t _M0L6_2atmpS3580 = _M0L4attrS1340[4];
              int32_t _M0L6_2atmpS3522;
              int32_t _M0L4_2axS1345;
              moonbit_decref(_M0L4attrS1340);
              _M0L6_2atmpS3522 = _M0L6_2atmpS3580;
              _M0L4_2axS1345 = _M0L6_2atmpS3522;
              if (_M0L4_2axS1345 == 99) {
                void* _M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3521;
                struct moonbit_result_0 _result_4239;
                moonbit_decref(_M0L17error__to__stringS1352);
                moonbit_decref(_M0L14handle__resultS1350);
                moonbit_decref(_M0L5_2aitS1338);
                moonbit_decref(_M0L1fS1332);
                _M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3521
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3521)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
                ((struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3521)->$0
                = _M0L4nameS1334;
                _result_4239.tag = 0;
                _result_4239.data.err
                = _M0L125clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3521;
                return _result_4239;
              }
            } else {
              moonbit_decref(_M0L4attrS1340);
            }
          } else {
            moonbit_decref(_M0L4attrS1340);
          }
        } else {
          moonbit_decref(_M0L4attrS1340);
        }
      } else {
        moonbit_decref(_M0L4attrS1340);
      }
    } else {
      moonbit_decref(_M0L4attrS1340);
    }
    continue;
    joinlet_4238:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1350);
  moonbit_incref(_M0L4nameS1334);
  _closure_4240
  = (struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__*)moonbit_malloc(sizeof(struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__));
  Moonbit_object_header(_closure_4240)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__, $0) >> 2, 2, 0);
  _closure_4240->code
  = &_M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testC3533l427;
  _closure_4240->$0 = _M0L14handle__resultS1350;
  _closure_4240->$1 = _M0L4nameS1334;
  _M0L6_2atmpS3527 = (struct _M0TWEOc*)_closure_4240;
  _closure_4241
  = (struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__*)moonbit_malloc(sizeof(struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__));
  Moonbit_object_header(_closure_4241)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__, $0) >> 2, 3, 0);
  _closure_4241->code
  = &_M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testC3529l428;
  _closure_4241->$0 = _M0L17error__to__stringS1352;
  _closure_4241->$1 = _M0L14handle__resultS1350;
  _closure_4241->$2 = _M0L4nameS1334;
  _M0L6_2atmpS3528 = (struct _M0TWRPC15error5ErrorEu*)_closure_4241;
  #line 425 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal20rand__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1332, _M0L6_2atmpS3527, _M0L6_2atmpS3528);
  _result_4242.tag = 1;
  _result_4242.data.ok = 1;
  return _result_4242;
}

int32_t _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testC3533l427(
  struct _M0TWEOc* _M0L6_2aenvS3534
) {
  struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__* _M0L14_2acasted__envS3535;
  moonbit_string_t _M0L8_2afieldS3585;
  moonbit_string_t _M0L4nameS1334;
  struct _M0TWssbEu* _M0L8_2afieldS3584;
  int32_t _M0L6_2acntS4148;
  struct _M0TWssbEu* _M0L14handle__resultS1350;
  #line 427 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3535
  = (struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3533__l427__*)_M0L6_2aenvS3534;
  _M0L8_2afieldS3585 = _M0L14_2acasted__envS3535->$1;
  _M0L4nameS1334 = _M0L8_2afieldS3585;
  _M0L8_2afieldS3584 = _M0L14_2acasted__envS3535->$0;
  _M0L6_2acntS4148 = Moonbit_object_header(_M0L14_2acasted__envS3535)->rc;
  if (_M0L6_2acntS4148 > 1) {
    int32_t _M0L11_2anew__cntS4149 = _M0L6_2acntS4148 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3535)->rc
    = _M0L11_2anew__cntS4149;
    moonbit_incref(_M0L4nameS1334);
    moonbit_incref(_M0L8_2afieldS3584);
  } else if (_M0L6_2acntS4148 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3535);
  }
  _M0L14handle__resultS1350 = _M0L8_2afieldS3584;
  #line 427 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1350->code(_M0L14handle__resultS1350, _M0L4nameS1334, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal20rand__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testC3529l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3530,
  void* _M0L3errS1351
) {
  struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__* _M0L14_2acasted__envS3531;
  moonbit_string_t _M0L8_2afieldS3588;
  moonbit_string_t _M0L4nameS1334;
  struct _M0TWssbEu* _M0L8_2afieldS3587;
  struct _M0TWssbEu* _M0L14handle__resultS1350;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3586;
  int32_t _M0L6_2acntS4150;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1352;
  moonbit_string_t _M0L6_2atmpS3532;
  #line 428 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3531
  = (struct _M0R223_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2frand__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3529__l428__*)_M0L6_2aenvS3530;
  _M0L8_2afieldS3588 = _M0L14_2acasted__envS3531->$2;
  _M0L4nameS1334 = _M0L8_2afieldS3588;
  _M0L8_2afieldS3587 = _M0L14_2acasted__envS3531->$1;
  _M0L14handle__resultS1350 = _M0L8_2afieldS3587;
  _M0L8_2afieldS3586 = _M0L14_2acasted__envS3531->$0;
  _M0L6_2acntS4150 = Moonbit_object_header(_M0L14_2acasted__envS3531)->rc;
  if (_M0L6_2acntS4150 > 1) {
    int32_t _M0L11_2anew__cntS4151 = _M0L6_2acntS4150 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3531)->rc
    = _M0L11_2anew__cntS4151;
    moonbit_incref(_M0L4nameS1334);
    moonbit_incref(_M0L14handle__resultS1350);
    moonbit_incref(_M0L8_2afieldS3586);
  } else if (_M0L6_2acntS4150 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3531);
  }
  _M0L17error__to__stringS1352 = _M0L8_2afieldS3586;
  #line 428 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3532
  = _M0L17error__to__stringS1352->code(_M0L17error__to__stringS1352, _M0L3errS1351);
  #line 428 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1350->code(_M0L14handle__resultS1350, _M0L4nameS1334, _M0L6_2atmpS3532, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1324,
  struct _M0TWEOc* _M0L6on__okS1325,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1322
) {
  void* _M0L11_2atry__errS1320;
  struct moonbit_result_0 _tmp_4244;
  void* _M0L3errS1321;
  #line 375 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4244 = _M0L1fS1324->code(_M0L1fS1324);
  if (_tmp_4244.tag) {
    int32_t const _M0L5_2aokS3517 = _tmp_4244.data.ok;
    moonbit_decref(_M0L7on__errS1322);
  } else {
    void* const _M0L6_2aerrS3518 = _tmp_4244.data.err;
    moonbit_decref(_M0L6on__okS1325);
    _M0L11_2atry__errS1320 = _M0L6_2aerrS3518;
    goto join_1319;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1325->code(_M0L6on__okS1325);
  goto joinlet_4243;
  join_1319:;
  _M0L3errS1321 = _M0L11_2atry__errS1320;
  #line 383 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1322->code(_M0L7on__errS1322, _M0L3errS1321);
  joinlet_4243:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1279;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1292;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1297;
  struct _M0TUsiE** _M0L6_2atmpS3516;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1304;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1305;
  moonbit_string_t _M0L6_2atmpS3515;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1306;
  int32_t _M0L7_2abindS1307;
  int32_t _M0L2__S1308;
  #line 193 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1279 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1292
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1297 = 0;
  _M0L6_2atmpS3516 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1304
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1304)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1304->$0 = _M0L6_2atmpS3516;
  _M0L16file__and__indexS1304->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1305
  = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1292(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1292);
  #line 284 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3515 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1305, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1306
  = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1297(_M0L51moonbit__test__driver__internal__split__mbt__stringS1297, _M0L6_2atmpS3515, 47);
  _M0L7_2abindS1307 = _M0L10test__argsS1306->$1;
  _M0L2__S1308 = 0;
  while (1) {
    if (_M0L2__S1308 < _M0L7_2abindS1307) {
      moonbit_string_t* _M0L8_2afieldS3590 = _M0L10test__argsS1306->$0;
      moonbit_string_t* _M0L3bufS3514 = _M0L8_2afieldS3590;
      moonbit_string_t _M0L6_2atmpS3589 =
        (moonbit_string_t)_M0L3bufS3514[_M0L2__S1308];
      moonbit_string_t _M0L3argS1309 = _M0L6_2atmpS3589;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1310;
      moonbit_string_t _M0L4fileS1311;
      moonbit_string_t _M0L5rangeS1312;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1313;
      moonbit_string_t _M0L6_2atmpS3512;
      int32_t _M0L5startS1314;
      moonbit_string_t _M0L6_2atmpS3511;
      int32_t _M0L3endS1315;
      int32_t _M0L1iS1316;
      int32_t _M0L6_2atmpS3513;
      moonbit_incref(_M0L3argS1309);
      #line 288 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1310
      = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1297(_M0L51moonbit__test__driver__internal__split__mbt__stringS1297, _M0L3argS1309, 58);
      moonbit_incref(_M0L16file__and__rangeS1310);
      #line 289 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1311
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1310, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1312
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1310, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1313
      = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1297(_M0L51moonbit__test__driver__internal__split__mbt__stringS1297, _M0L5rangeS1312, 45);
      moonbit_incref(_M0L15start__and__endS1313);
      #line 294 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3512
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1313, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1314
      = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1279(_M0L45moonbit__test__driver__internal__parse__int__S1279, _M0L6_2atmpS3512);
      #line 295 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3511
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1313, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1315
      = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1279(_M0L45moonbit__test__driver__internal__parse__int__S1279, _M0L6_2atmpS3511);
      _M0L1iS1316 = _M0L5startS1314;
      while (1) {
        if (_M0L1iS1316 < _M0L3endS1315) {
          struct _M0TUsiE* _M0L8_2atupleS3509;
          int32_t _M0L6_2atmpS3510;
          moonbit_incref(_M0L4fileS1311);
          _M0L8_2atupleS3509
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3509)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3509->$0 = _M0L4fileS1311;
          _M0L8_2atupleS3509->$1 = _M0L1iS1316;
          moonbit_incref(_M0L16file__and__indexS1304);
          #line 297 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1304, _M0L8_2atupleS3509);
          _M0L6_2atmpS3510 = _M0L1iS1316 + 1;
          _M0L1iS1316 = _M0L6_2atmpS3510;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1311);
        }
        break;
      }
      _M0L6_2atmpS3513 = _M0L2__S1308 + 1;
      _M0L2__S1308 = _M0L6_2atmpS3513;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1306);
    }
    break;
  }
  return _M0L16file__and__indexS1304;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1297(
  int32_t _M0L6_2aenvS3490,
  moonbit_string_t _M0L1sS1298,
  int32_t _M0L3sepS1299
) {
  moonbit_string_t* _M0L6_2atmpS3508;
  struct _M0TPB5ArrayGsE* _M0L3resS1300;
  struct _M0TPC13ref3RefGiE* _M0L1iS1301;
  struct _M0TPC13ref3RefGiE* _M0L5startS1302;
  int32_t _M0L3valS3503;
  int32_t _M0L6_2atmpS3504;
  #line 261 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3508 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1300
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1300)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1300->$0 = _M0L6_2atmpS3508;
  _M0L3resS1300->$1 = 0;
  _M0L1iS1301
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1301)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1301->$0 = 0;
  _M0L5startS1302
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1302)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1302->$0 = 0;
  while (1) {
    int32_t _M0L3valS3491 = _M0L1iS1301->$0;
    int32_t _M0L6_2atmpS3492 = Moonbit_array_length(_M0L1sS1298);
    if (_M0L3valS3491 < _M0L6_2atmpS3492) {
      int32_t _M0L3valS3495 = _M0L1iS1301->$0;
      int32_t _M0L6_2atmpS3494;
      int32_t _M0L6_2atmpS3493;
      int32_t _M0L3valS3502;
      int32_t _M0L6_2atmpS3501;
      if (
        _M0L3valS3495 < 0
        || _M0L3valS3495 >= Moonbit_array_length(_M0L1sS1298)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3494 = _M0L1sS1298[_M0L3valS3495];
      _M0L6_2atmpS3493 = _M0L6_2atmpS3494;
      if (_M0L6_2atmpS3493 == _M0L3sepS1299) {
        int32_t _M0L3valS3497 = _M0L5startS1302->$0;
        int32_t _M0L3valS3498 = _M0L1iS1301->$0;
        moonbit_string_t _M0L6_2atmpS3496;
        int32_t _M0L3valS3500;
        int32_t _M0L6_2atmpS3499;
        moonbit_incref(_M0L1sS1298);
        #line 270 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3496
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1298, _M0L3valS3497, _M0L3valS3498);
        moonbit_incref(_M0L3resS1300);
        #line 270 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1300, _M0L6_2atmpS3496);
        _M0L3valS3500 = _M0L1iS1301->$0;
        _M0L6_2atmpS3499 = _M0L3valS3500 + 1;
        _M0L5startS1302->$0 = _M0L6_2atmpS3499;
      }
      _M0L3valS3502 = _M0L1iS1301->$0;
      _M0L6_2atmpS3501 = _M0L3valS3502 + 1;
      _M0L1iS1301->$0 = _M0L6_2atmpS3501;
      continue;
    } else {
      moonbit_decref(_M0L1iS1301);
    }
    break;
  }
  _M0L3valS3503 = _M0L5startS1302->$0;
  _M0L6_2atmpS3504 = Moonbit_array_length(_M0L1sS1298);
  if (_M0L3valS3503 < _M0L6_2atmpS3504) {
    int32_t _M0L8_2afieldS3591 = _M0L5startS1302->$0;
    int32_t _M0L3valS3506;
    int32_t _M0L6_2atmpS3507;
    moonbit_string_t _M0L6_2atmpS3505;
    moonbit_decref(_M0L5startS1302);
    _M0L3valS3506 = _M0L8_2afieldS3591;
    _M0L6_2atmpS3507 = Moonbit_array_length(_M0L1sS1298);
    #line 276 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3505
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1298, _M0L3valS3506, _M0L6_2atmpS3507);
    moonbit_incref(_M0L3resS1300);
    #line 276 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1300, _M0L6_2atmpS3505);
  } else {
    moonbit_decref(_M0L5startS1302);
    moonbit_decref(_M0L1sS1298);
  }
  return _M0L3resS1300;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1292(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285
) {
  moonbit_bytes_t* _M0L3tmpS1293;
  int32_t _M0L6_2atmpS3489;
  struct _M0TPB5ArrayGsE* _M0L3resS1294;
  int32_t _M0L1iS1295;
  #line 250 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1293
  = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3489 = Moonbit_array_length(_M0L3tmpS1293);
  #line 254 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1294 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3489);
  _M0L1iS1295 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3485 = Moonbit_array_length(_M0L3tmpS1293);
    if (_M0L1iS1295 < _M0L6_2atmpS3485) {
      moonbit_bytes_t _M0L6_2atmpS3592;
      moonbit_bytes_t _M0L6_2atmpS3487;
      moonbit_string_t _M0L6_2atmpS3486;
      int32_t _M0L6_2atmpS3488;
      if (
        _M0L1iS1295 < 0 || _M0L1iS1295 >= Moonbit_array_length(_M0L3tmpS1293)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3592 = (moonbit_bytes_t)_M0L3tmpS1293[_M0L1iS1295];
      _M0L6_2atmpS3487 = _M0L6_2atmpS3592;
      moonbit_incref(_M0L6_2atmpS3487);
      #line 256 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3486
      = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285, _M0L6_2atmpS3487);
      moonbit_incref(_M0L3resS1294);
      #line 256 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1294, _M0L6_2atmpS3486);
      _M0L6_2atmpS3488 = _M0L1iS1295 + 1;
      _M0L1iS1295 = _M0L6_2atmpS3488;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1293);
    }
    break;
  }
  return _M0L3resS1294;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1285(
  int32_t _M0L6_2aenvS3399,
  moonbit_bytes_t _M0L5bytesS1286
) {
  struct _M0TPB13StringBuilder* _M0L3resS1287;
  int32_t _M0L3lenS1288;
  struct _M0TPC13ref3RefGiE* _M0L1iS1289;
  #line 206 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1287 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1288 = Moonbit_array_length(_M0L5bytesS1286);
  _M0L1iS1289
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1289)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1289->$0 = 0;
  while (1) {
    int32_t _M0L3valS3400 = _M0L1iS1289->$0;
    if (_M0L3valS3400 < _M0L3lenS1288) {
      int32_t _M0L3valS3484 = _M0L1iS1289->$0;
      int32_t _M0L6_2atmpS3483;
      int32_t _M0L6_2atmpS3482;
      struct _M0TPC13ref3RefGiE* _M0L1cS1290;
      int32_t _M0L3valS3401;
      if (
        _M0L3valS3484 < 0
        || _M0L3valS3484 >= Moonbit_array_length(_M0L5bytesS1286)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3483 = _M0L5bytesS1286[_M0L3valS3484];
      _M0L6_2atmpS3482 = (int32_t)_M0L6_2atmpS3483;
      _M0L1cS1290
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1290)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1290->$0 = _M0L6_2atmpS3482;
      _M0L3valS3401 = _M0L1cS1290->$0;
      if (_M0L3valS3401 < 128) {
        int32_t _M0L8_2afieldS3593 = _M0L1cS1290->$0;
        int32_t _M0L3valS3403;
        int32_t _M0L6_2atmpS3402;
        int32_t _M0L3valS3405;
        int32_t _M0L6_2atmpS3404;
        moonbit_decref(_M0L1cS1290);
        _M0L3valS3403 = _M0L8_2afieldS3593;
        _M0L6_2atmpS3402 = _M0L3valS3403;
        moonbit_incref(_M0L3resS1287);
        #line 215 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1287, _M0L6_2atmpS3402);
        _M0L3valS3405 = _M0L1iS1289->$0;
        _M0L6_2atmpS3404 = _M0L3valS3405 + 1;
        _M0L1iS1289->$0 = _M0L6_2atmpS3404;
      } else {
        int32_t _M0L3valS3406 = _M0L1cS1290->$0;
        if (_M0L3valS3406 < 224) {
          int32_t _M0L3valS3408 = _M0L1iS1289->$0;
          int32_t _M0L6_2atmpS3407 = _M0L3valS3408 + 1;
          int32_t _M0L3valS3417;
          int32_t _M0L6_2atmpS3416;
          int32_t _M0L6_2atmpS3410;
          int32_t _M0L3valS3415;
          int32_t _M0L6_2atmpS3414;
          int32_t _M0L6_2atmpS3413;
          int32_t _M0L6_2atmpS3412;
          int32_t _M0L6_2atmpS3411;
          int32_t _M0L6_2atmpS3409;
          int32_t _M0L8_2afieldS3594;
          int32_t _M0L3valS3419;
          int32_t _M0L6_2atmpS3418;
          int32_t _M0L3valS3421;
          int32_t _M0L6_2atmpS3420;
          if (_M0L6_2atmpS3407 >= _M0L3lenS1288) {
            moonbit_decref(_M0L1cS1290);
            moonbit_decref(_M0L1iS1289);
            moonbit_decref(_M0L5bytesS1286);
            break;
          }
          _M0L3valS3417 = _M0L1cS1290->$0;
          _M0L6_2atmpS3416 = _M0L3valS3417 & 31;
          _M0L6_2atmpS3410 = _M0L6_2atmpS3416 << 6;
          _M0L3valS3415 = _M0L1iS1289->$0;
          _M0L6_2atmpS3414 = _M0L3valS3415 + 1;
          if (
            _M0L6_2atmpS3414 < 0
            || _M0L6_2atmpS3414 >= Moonbit_array_length(_M0L5bytesS1286)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3413 = _M0L5bytesS1286[_M0L6_2atmpS3414];
          _M0L6_2atmpS3412 = (int32_t)_M0L6_2atmpS3413;
          _M0L6_2atmpS3411 = _M0L6_2atmpS3412 & 63;
          _M0L6_2atmpS3409 = _M0L6_2atmpS3410 | _M0L6_2atmpS3411;
          _M0L1cS1290->$0 = _M0L6_2atmpS3409;
          _M0L8_2afieldS3594 = _M0L1cS1290->$0;
          moonbit_decref(_M0L1cS1290);
          _M0L3valS3419 = _M0L8_2afieldS3594;
          _M0L6_2atmpS3418 = _M0L3valS3419;
          moonbit_incref(_M0L3resS1287);
          #line 222 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1287, _M0L6_2atmpS3418);
          _M0L3valS3421 = _M0L1iS1289->$0;
          _M0L6_2atmpS3420 = _M0L3valS3421 + 2;
          _M0L1iS1289->$0 = _M0L6_2atmpS3420;
        } else {
          int32_t _M0L3valS3422 = _M0L1cS1290->$0;
          if (_M0L3valS3422 < 240) {
            int32_t _M0L3valS3424 = _M0L1iS1289->$0;
            int32_t _M0L6_2atmpS3423 = _M0L3valS3424 + 2;
            int32_t _M0L3valS3440;
            int32_t _M0L6_2atmpS3439;
            int32_t _M0L6_2atmpS3432;
            int32_t _M0L3valS3438;
            int32_t _M0L6_2atmpS3437;
            int32_t _M0L6_2atmpS3436;
            int32_t _M0L6_2atmpS3435;
            int32_t _M0L6_2atmpS3434;
            int32_t _M0L6_2atmpS3433;
            int32_t _M0L6_2atmpS3426;
            int32_t _M0L3valS3431;
            int32_t _M0L6_2atmpS3430;
            int32_t _M0L6_2atmpS3429;
            int32_t _M0L6_2atmpS3428;
            int32_t _M0L6_2atmpS3427;
            int32_t _M0L6_2atmpS3425;
            int32_t _M0L8_2afieldS3595;
            int32_t _M0L3valS3442;
            int32_t _M0L6_2atmpS3441;
            int32_t _M0L3valS3444;
            int32_t _M0L6_2atmpS3443;
            if (_M0L6_2atmpS3423 >= _M0L3lenS1288) {
              moonbit_decref(_M0L1cS1290);
              moonbit_decref(_M0L1iS1289);
              moonbit_decref(_M0L5bytesS1286);
              break;
            }
            _M0L3valS3440 = _M0L1cS1290->$0;
            _M0L6_2atmpS3439 = _M0L3valS3440 & 15;
            _M0L6_2atmpS3432 = _M0L6_2atmpS3439 << 12;
            _M0L3valS3438 = _M0L1iS1289->$0;
            _M0L6_2atmpS3437 = _M0L3valS3438 + 1;
            if (
              _M0L6_2atmpS3437 < 0
              || _M0L6_2atmpS3437 >= Moonbit_array_length(_M0L5bytesS1286)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3436 = _M0L5bytesS1286[_M0L6_2atmpS3437];
            _M0L6_2atmpS3435 = (int32_t)_M0L6_2atmpS3436;
            _M0L6_2atmpS3434 = _M0L6_2atmpS3435 & 63;
            _M0L6_2atmpS3433 = _M0L6_2atmpS3434 << 6;
            _M0L6_2atmpS3426 = _M0L6_2atmpS3432 | _M0L6_2atmpS3433;
            _M0L3valS3431 = _M0L1iS1289->$0;
            _M0L6_2atmpS3430 = _M0L3valS3431 + 2;
            if (
              _M0L6_2atmpS3430 < 0
              || _M0L6_2atmpS3430 >= Moonbit_array_length(_M0L5bytesS1286)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3429 = _M0L5bytesS1286[_M0L6_2atmpS3430];
            _M0L6_2atmpS3428 = (int32_t)_M0L6_2atmpS3429;
            _M0L6_2atmpS3427 = _M0L6_2atmpS3428 & 63;
            _M0L6_2atmpS3425 = _M0L6_2atmpS3426 | _M0L6_2atmpS3427;
            _M0L1cS1290->$0 = _M0L6_2atmpS3425;
            _M0L8_2afieldS3595 = _M0L1cS1290->$0;
            moonbit_decref(_M0L1cS1290);
            _M0L3valS3442 = _M0L8_2afieldS3595;
            _M0L6_2atmpS3441 = _M0L3valS3442;
            moonbit_incref(_M0L3resS1287);
            #line 231 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1287, _M0L6_2atmpS3441);
            _M0L3valS3444 = _M0L1iS1289->$0;
            _M0L6_2atmpS3443 = _M0L3valS3444 + 3;
            _M0L1iS1289->$0 = _M0L6_2atmpS3443;
          } else {
            int32_t _M0L3valS3446 = _M0L1iS1289->$0;
            int32_t _M0L6_2atmpS3445 = _M0L3valS3446 + 3;
            int32_t _M0L3valS3469;
            int32_t _M0L6_2atmpS3468;
            int32_t _M0L6_2atmpS3461;
            int32_t _M0L3valS3467;
            int32_t _M0L6_2atmpS3466;
            int32_t _M0L6_2atmpS3465;
            int32_t _M0L6_2atmpS3464;
            int32_t _M0L6_2atmpS3463;
            int32_t _M0L6_2atmpS3462;
            int32_t _M0L6_2atmpS3454;
            int32_t _M0L3valS3460;
            int32_t _M0L6_2atmpS3459;
            int32_t _M0L6_2atmpS3458;
            int32_t _M0L6_2atmpS3457;
            int32_t _M0L6_2atmpS3456;
            int32_t _M0L6_2atmpS3455;
            int32_t _M0L6_2atmpS3448;
            int32_t _M0L3valS3453;
            int32_t _M0L6_2atmpS3452;
            int32_t _M0L6_2atmpS3451;
            int32_t _M0L6_2atmpS3450;
            int32_t _M0L6_2atmpS3449;
            int32_t _M0L6_2atmpS3447;
            int32_t _M0L3valS3471;
            int32_t _M0L6_2atmpS3470;
            int32_t _M0L3valS3475;
            int32_t _M0L6_2atmpS3474;
            int32_t _M0L6_2atmpS3473;
            int32_t _M0L6_2atmpS3472;
            int32_t _M0L8_2afieldS3596;
            int32_t _M0L3valS3479;
            int32_t _M0L6_2atmpS3478;
            int32_t _M0L6_2atmpS3477;
            int32_t _M0L6_2atmpS3476;
            int32_t _M0L3valS3481;
            int32_t _M0L6_2atmpS3480;
            if (_M0L6_2atmpS3445 >= _M0L3lenS1288) {
              moonbit_decref(_M0L1cS1290);
              moonbit_decref(_M0L1iS1289);
              moonbit_decref(_M0L5bytesS1286);
              break;
            }
            _M0L3valS3469 = _M0L1cS1290->$0;
            _M0L6_2atmpS3468 = _M0L3valS3469 & 7;
            _M0L6_2atmpS3461 = _M0L6_2atmpS3468 << 18;
            _M0L3valS3467 = _M0L1iS1289->$0;
            _M0L6_2atmpS3466 = _M0L3valS3467 + 1;
            if (
              _M0L6_2atmpS3466 < 0
              || _M0L6_2atmpS3466 >= Moonbit_array_length(_M0L5bytesS1286)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3465 = _M0L5bytesS1286[_M0L6_2atmpS3466];
            _M0L6_2atmpS3464 = (int32_t)_M0L6_2atmpS3465;
            _M0L6_2atmpS3463 = _M0L6_2atmpS3464 & 63;
            _M0L6_2atmpS3462 = _M0L6_2atmpS3463 << 12;
            _M0L6_2atmpS3454 = _M0L6_2atmpS3461 | _M0L6_2atmpS3462;
            _M0L3valS3460 = _M0L1iS1289->$0;
            _M0L6_2atmpS3459 = _M0L3valS3460 + 2;
            if (
              _M0L6_2atmpS3459 < 0
              || _M0L6_2atmpS3459 >= Moonbit_array_length(_M0L5bytesS1286)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3458 = _M0L5bytesS1286[_M0L6_2atmpS3459];
            _M0L6_2atmpS3457 = (int32_t)_M0L6_2atmpS3458;
            _M0L6_2atmpS3456 = _M0L6_2atmpS3457 & 63;
            _M0L6_2atmpS3455 = _M0L6_2atmpS3456 << 6;
            _M0L6_2atmpS3448 = _M0L6_2atmpS3454 | _M0L6_2atmpS3455;
            _M0L3valS3453 = _M0L1iS1289->$0;
            _M0L6_2atmpS3452 = _M0L3valS3453 + 3;
            if (
              _M0L6_2atmpS3452 < 0
              || _M0L6_2atmpS3452 >= Moonbit_array_length(_M0L5bytesS1286)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3451 = _M0L5bytesS1286[_M0L6_2atmpS3452];
            _M0L6_2atmpS3450 = (int32_t)_M0L6_2atmpS3451;
            _M0L6_2atmpS3449 = _M0L6_2atmpS3450 & 63;
            _M0L6_2atmpS3447 = _M0L6_2atmpS3448 | _M0L6_2atmpS3449;
            _M0L1cS1290->$0 = _M0L6_2atmpS3447;
            _M0L3valS3471 = _M0L1cS1290->$0;
            _M0L6_2atmpS3470 = _M0L3valS3471 - 65536;
            _M0L1cS1290->$0 = _M0L6_2atmpS3470;
            _M0L3valS3475 = _M0L1cS1290->$0;
            _M0L6_2atmpS3474 = _M0L3valS3475 >> 10;
            _M0L6_2atmpS3473 = _M0L6_2atmpS3474 + 55296;
            _M0L6_2atmpS3472 = _M0L6_2atmpS3473;
            moonbit_incref(_M0L3resS1287);
            #line 242 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1287, _M0L6_2atmpS3472);
            _M0L8_2afieldS3596 = _M0L1cS1290->$0;
            moonbit_decref(_M0L1cS1290);
            _M0L3valS3479 = _M0L8_2afieldS3596;
            _M0L6_2atmpS3478 = _M0L3valS3479 & 1023;
            _M0L6_2atmpS3477 = _M0L6_2atmpS3478 + 56320;
            _M0L6_2atmpS3476 = _M0L6_2atmpS3477;
            moonbit_incref(_M0L3resS1287);
            #line 243 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1287, _M0L6_2atmpS3476);
            _M0L3valS3481 = _M0L1iS1289->$0;
            _M0L6_2atmpS3480 = _M0L3valS3481 + 4;
            _M0L1iS1289->$0 = _M0L6_2atmpS3480;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1289);
      moonbit_decref(_M0L5bytesS1286);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1287);
}

int32_t _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1279(
  int32_t _M0L6_2aenvS3392,
  moonbit_string_t _M0L1sS1280
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1281;
  int32_t _M0L3lenS1282;
  int32_t _M0L1iS1283;
  int32_t _M0L8_2afieldS3597;
  #line 197 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1281
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1281)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1281->$0 = 0;
  _M0L3lenS1282 = Moonbit_array_length(_M0L1sS1280);
  _M0L1iS1283 = 0;
  while (1) {
    if (_M0L1iS1283 < _M0L3lenS1282) {
      int32_t _M0L3valS3397 = _M0L3resS1281->$0;
      int32_t _M0L6_2atmpS3394 = _M0L3valS3397 * 10;
      int32_t _M0L6_2atmpS3396;
      int32_t _M0L6_2atmpS3395;
      int32_t _M0L6_2atmpS3393;
      int32_t _M0L6_2atmpS3398;
      if (
        _M0L1iS1283 < 0 || _M0L1iS1283 >= Moonbit_array_length(_M0L1sS1280)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3396 = _M0L1sS1280[_M0L1iS1283];
      _M0L6_2atmpS3395 = _M0L6_2atmpS3396 - 48;
      _M0L6_2atmpS3393 = _M0L6_2atmpS3394 + _M0L6_2atmpS3395;
      _M0L3resS1281->$0 = _M0L6_2atmpS3393;
      _M0L6_2atmpS3398 = _M0L1iS1283 + 1;
      _M0L1iS1283 = _M0L6_2atmpS3398;
      continue;
    } else {
      moonbit_decref(_M0L1sS1280);
    }
    break;
  }
  _M0L8_2afieldS3597 = _M0L3resS1281->$0;
  moonbit_decref(_M0L3resS1281);
  return _M0L8_2afieldS3597;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1259,
  moonbit_string_t _M0L12_2adiscard__S1260,
  int32_t _M0L12_2adiscard__S1261,
  struct _M0TWssbEu* _M0L12_2adiscard__S1262,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1263
) {
  struct moonbit_result_0 _result_4251;
  #line 34 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1263);
  moonbit_decref(_M0L12_2adiscard__S1262);
  moonbit_decref(_M0L12_2adiscard__S1260);
  moonbit_decref(_M0L12_2adiscard__S1259);
  _result_4251.tag = 1;
  _result_4251.data.ok = 0;
  return _result_4251;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1264,
  moonbit_string_t _M0L12_2adiscard__S1265,
  int32_t _M0L12_2adiscard__S1266,
  struct _M0TWssbEu* _M0L12_2adiscard__S1267,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1268
) {
  struct moonbit_result_0 _result_4252;
  #line 34 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1268);
  moonbit_decref(_M0L12_2adiscard__S1267);
  moonbit_decref(_M0L12_2adiscard__S1265);
  moonbit_decref(_M0L12_2adiscard__S1264);
  _result_4252.tag = 1;
  _result_4252.data.ok = 0;
  return _result_4252;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1269,
  moonbit_string_t _M0L12_2adiscard__S1270,
  int32_t _M0L12_2adiscard__S1271,
  struct _M0TWssbEu* _M0L12_2adiscard__S1272,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1273
) {
  struct moonbit_result_0 _result_4253;
  #line 34 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1273);
  moonbit_decref(_M0L12_2adiscard__S1272);
  moonbit_decref(_M0L12_2adiscard__S1270);
  moonbit_decref(_M0L12_2adiscard__S1269);
  _result_4253.tag = 1;
  _result_4253.data.ok = 0;
  return _result_4253;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal20rand__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1274,
  moonbit_string_t _M0L12_2adiscard__S1275,
  int32_t _M0L12_2adiscard__S1276,
  struct _M0TWssbEu* _M0L12_2adiscard__S1277,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1278
) {
  struct moonbit_result_0 _result_4254;
  #line 34 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1278);
  moonbit_decref(_M0L12_2adiscard__S1277);
  moonbit_decref(_M0L12_2adiscard__S1275);
  moonbit_decref(_M0L12_2adiscard__S1274);
  _result_4254.tag = 1;
  _result_4254.data.ok = 0;
  return _result_4254;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal20rand__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1258
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1258);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal20rand__blackbox__test39____test__72616e645f746573742e6d6274__0(
  
) {
  struct moonbit_result_1 _tmp_4255;
  moonbit_bytes_t _M0L5bytesS1257;
  int32_t _M0L6_2atmpS3598;
  int32_t _M0L6_2atmpS3388;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3389;
  struct _M0TPB6ToJson _M0L6_2atmpS3378;
  moonbit_string_t _M0L6_2atmpS3387;
  void* _M0L6_2atmpS3386;
  void* _M0L6_2atmpS3379;
  moonbit_string_t _M0L6_2atmpS3382;
  moonbit_string_t _M0L6_2atmpS3383;
  moonbit_string_t _M0L6_2atmpS3384;
  moonbit_string_t _M0L6_2atmpS3385;
  moonbit_string_t* _M0L6_2atmpS3381;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3380;
  #line 2 "E:\\moonbit\\clawteam\\internal\\rand\\rand_test.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\rand\\rand_test.mbt"
  _tmp_4255 = _M0FP48clawteam8clawteam8internal4rand5bytes(16);
  if (_tmp_4255.tag) {
    moonbit_bytes_t const _M0L5_2aokS3390 = _tmp_4255.data.ok;
    _M0L5bytesS1257 = _M0L5_2aokS3390;
  } else {
    void* const _M0L6_2aerrS3391 = _tmp_4255.data.err;
    struct moonbit_result_0 _result_4256;
    _result_4256.tag = 0;
    _result_4256.data.err = _M0L6_2aerrS3391;
    return _result_4256;
  }
  _M0L6_2atmpS3598 = Moonbit_array_length(_M0L5bytesS1257);
  moonbit_decref(_M0L5bytesS1257);
  _M0L6_2atmpS3388 = _M0L6_2atmpS3598;
  _M0L14_2aboxed__selfS3389
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3389)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3389->$0 = _M0L6_2atmpS3388;
  _M0L6_2atmpS3378
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3389
  };
  _M0L6_2atmpS3387 = 0;
  #line 4 "E:\\moonbit\\clawteam\\internal\\rand\\rand_test.mbt"
  _M0L6_2atmpS3386 = _M0MPC14json4Json6number(0x1p+4, _M0L6_2atmpS3387);
  _M0L6_2atmpS3379 = _M0L6_2atmpS3386;
  _M0L6_2atmpS3382 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3383 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3384 = 0;
  _M0L6_2atmpS3385 = 0;
  _M0L6_2atmpS3381 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3381[0] = _M0L6_2atmpS3382;
  _M0L6_2atmpS3381[1] = _M0L6_2atmpS3383;
  _M0L6_2atmpS3381[2] = _M0L6_2atmpS3384;
  _M0L6_2atmpS3381[3] = _M0L6_2atmpS3385;
  _M0L6_2atmpS3380
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3380)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3380->$0 = _M0L6_2atmpS3381;
  _M0L6_2atmpS3380->$1 = 4;
  #line 4 "E:\\moonbit\\clawteam\\internal\\rand\\rand_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3378, _M0L6_2atmpS3379, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3380);
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal4rand5bytes(
  int32_t _M0L1nS1255
) {
  moonbit_bytes_t _M0L3bufS1254;
  int32_t _M0L5errnoS1256;
  struct moonbit_result_1 _result_4258;
  #line 6 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  #line 7 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  _M0L3bufS1254 = _M0MPC15bytes5Bytes4make(_M0L1nS1255, 0);
  #line 8 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  _M0L5errnoS1256
  = _M0FP48clawteam8clawteam8internal4rand11rand__bytes(_M0L3bufS1254);
  if (_M0L5errnoS1256 != 0) {
    void* _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3377;
    struct moonbit_result_1 _result_4257;
    moonbit_decref(_M0L3bufS1254);
    _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3377
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno));
    Moonbit_object_header(_M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3377)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno) >> 2, 0, 1);
    ((struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno*)_M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3377)->$0
    = _M0L5errnoS1256;
    _result_4257.tag = 0;
    _result_4257.data.err
    = _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3377;
    return _result_4257;
  }
  _result_4258.tag = 1;
  _result_4258.data.ok = _M0L3bufS1254;
  return _result_4258;
}

int32_t _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(
  void* _M0L4selfS1251,
  struct _M0TPB6Logger _M0L6loggerS1244
) {
  int32_t _M0L6errnumS1242;
  struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno* _M0L8_2aErrnoS1252;
  int32_t _M0L8_2afieldS3600;
  int32_t _M0L9_2aerrnumS1253;
  void* _M0L6c__strS1243;
  #line 28 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  _M0L8_2aErrnoS1252
  = (struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno*)_M0L4selfS1251;
  _M0L8_2afieldS3600 = _M0L8_2aErrnoS1252->$0;
  moonbit_decref(_M0L8_2aErrnoS1252);
  _M0L9_2aerrnumS1253 = _M0L8_2afieldS3600;
  _M0L6errnumS1242 = _M0L9_2aerrnumS1253;
  goto join_1241;
  goto joinlet_4259;
  join_1241:;
  #line 30 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  _M0L6c__strS1243
  = _M0FP48clawteam8clawteam8internal5errno15errno__strerror(_M0L6errnumS1242);
  #line 31 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  if (
    _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(_M0L6c__strS1243)
  ) {
    moonbit_string_t _M0L6_2atmpS3370;
    moonbit_string_t _M0L6_2atmpS3599;
    moonbit_string_t _M0L6_2atmpS3369;
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3370
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6errnumS1242);
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3599
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3370);
    moonbit_decref(_M0L6_2atmpS3370);
    _M0L6_2atmpS3369 = _M0L6_2atmpS3599;
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6loggerS1244.$0->$method_0(_M0L6loggerS1244.$1, _M0L6_2atmpS3369);
  } else {
    uint64_t _M0L6_2atmpS3376;
    int32_t _M0L6c__lenS1245;
    moonbit_bytes_t _M0L3bufS1246;
    int32_t _M0L1iS1247;
    moonbit_bytes_t _M0L7_2abindS1250;
    int32_t _M0L6_2atmpS3375;
    int64_t _M0L6_2atmpS3374;
    struct _M0TPC15bytes9BytesView _M0L6_2atmpS3373;
    moonbit_string_t _M0L3strS1249;
    #line 34 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3376
    = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L6c__strS1243);
    _M0L6c__lenS1245 = (int32_t)_M0L6_2atmpS3376;
    _M0L3bufS1246 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6c__lenS1245, 0);
    _M0L1iS1247 = 0;
    while (1) {
      if (_M0L1iS1247 < _M0L6c__lenS1245) {
        int32_t _M0L6_2atmpS3371;
        int32_t _M0L6_2atmpS3372;
        #line 37 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
        _M0L6_2atmpS3371
        = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L6c__strS1243, _M0L1iS1247);
        if (
          _M0L1iS1247 < 0
          || _M0L1iS1247 >= Moonbit_array_length(_M0L3bufS1246)
        ) {
          #line 37 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
          moonbit_panic();
        }
        _M0L3bufS1246[_M0L1iS1247] = _M0L6_2atmpS3371;
        _M0L6_2atmpS3372 = _M0L1iS1247 + 1;
        _M0L1iS1247 = _M0L6_2atmpS3372;
        continue;
      }
      break;
    }
    _M0L7_2abindS1250 = _M0L3bufS1246;
    _M0L6_2atmpS3375 = Moonbit_array_length(_M0L7_2abindS1250);
    _M0L6_2atmpS3374 = (int64_t)_M0L6_2atmpS3375;
    #line 39 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3373
    = _M0MPC15bytes5Bytes12view_2einner(_M0L7_2abindS1250, 0, _M0L6_2atmpS3374);
    #line 39 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L3strS1249
    = _M0FPC28encoding4utf821decode__lossy_2einner(_M0L6_2atmpS3373, 0);
    #line 40 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6loggerS1244.$0->$method_0(_M0L6loggerS1244.$1, _M0L3strS1249);
  }
  joinlet_4259:;
  return 0;
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS1239,
  int32_t _M0L6offsetS1240
) {
  #line 145 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte(_M0L7pointerS1239, _M0L6offsetS1240);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void* _M0L4selfS1237,
  int32_t _M0L5indexS1238
) {
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS1237, _M0L5indexS1238);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(
  void* _M0L4selfS1236
) {
  void* _M0L6_2atmpS3368;
  #line 24 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0L6_2atmpS3368 = _M0L4selfS1236;
  #line 25 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0MP48clawteam8clawteam8internal1c7Pointer10__is__null(_M0L6_2atmpS3368);
}

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS1028,
  int32_t _M0L11ignore__bomS1029
) {
  struct _M0TPC15bytes9BytesView _M0L5bytesS1026;
  int32_t _M0L6_2atmpS3352;
  int32_t _M0L6_2atmpS3351;
  moonbit_bytes_t _M0L1tS1034;
  int32_t _M0L4tlenS1035;
  int32_t _M0L11_2aparam__0S1036;
  struct _M0TPC15bytes9BytesView _M0L11_2aparam__1S1037;
  moonbit_bytes_t _M0L6_2atmpS2670;
  int64_t _M0L6_2atmpS2671;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  if (_M0L11ignore__bomS1029) {
    int32_t _M0L3endS3354 = _M0L5bytesS1028.$2;
    int32_t _M0L5startS3355 = _M0L5bytesS1028.$1;
    int32_t _M0L6_2atmpS3353 = _M0L3endS3354 - _M0L5startS3355;
    if (_M0L6_2atmpS3353 >= 3) {
      moonbit_bytes_t _M0L8_2afieldS3922 = _M0L5bytesS1028.$0;
      moonbit_bytes_t _M0L5bytesS3366 = _M0L8_2afieldS3922;
      int32_t _M0L5startS3367 = _M0L5bytesS1028.$1;
      int32_t _M0L6_2atmpS3921 = _M0L5bytesS3366[_M0L5startS3367];
      int32_t _M0L4_2axS1031 = _M0L6_2atmpS3921;
      if (_M0L4_2axS1031 == 239) {
        moonbit_bytes_t _M0L8_2afieldS3920 = _M0L5bytesS1028.$0;
        moonbit_bytes_t _M0L5bytesS3363 = _M0L8_2afieldS3920;
        int32_t _M0L5startS3365 = _M0L5bytesS1028.$1;
        int32_t _M0L6_2atmpS3364 = _M0L5startS3365 + 1;
        int32_t _M0L6_2atmpS3919 = _M0L5bytesS3363[_M0L6_2atmpS3364];
        int32_t _M0L4_2axS1032 = _M0L6_2atmpS3919;
        if (_M0L4_2axS1032 == 187) {
          moonbit_bytes_t _M0L8_2afieldS3918 = _M0L5bytesS1028.$0;
          moonbit_bytes_t _M0L5bytesS3360 = _M0L8_2afieldS3918;
          int32_t _M0L5startS3362 = _M0L5bytesS1028.$1;
          int32_t _M0L6_2atmpS3361 = _M0L5startS3362 + 2;
          int32_t _M0L6_2atmpS3917 = _M0L5bytesS3360[_M0L6_2atmpS3361];
          int32_t _M0L4_2axS1033 = _M0L6_2atmpS3917;
          if (_M0L4_2axS1033 == 191) {
            moonbit_bytes_t _M0L8_2afieldS3916 = _M0L5bytesS1028.$0;
            moonbit_bytes_t _M0L5bytesS3356 = _M0L8_2afieldS3916;
            int32_t _M0L5startS3359 = _M0L5bytesS1028.$1;
            int32_t _M0L6_2atmpS3357 = _M0L5startS3359 + 3;
            int32_t _M0L8_2afieldS3915 = _M0L5bytesS1028.$2;
            int32_t _M0L3endS3358 = _M0L8_2afieldS3915;
            _M0L5bytesS1026
            = (struct _M0TPC15bytes9BytesView){
              _M0L6_2atmpS3357, _M0L3endS3358, _M0L5bytesS3356
            };
          } else {
            goto join_1030;
          }
        } else {
          goto join_1030;
        }
      } else {
        goto join_1030;
      }
    } else {
      goto join_1030;
    }
    goto joinlet_4262;
    join_1030:;
    goto join_1027;
    joinlet_4262:;
  } else {
    goto join_1027;
  }
  goto joinlet_4261;
  join_1027:;
  _M0L5bytesS1026 = _M0L5bytesS1028;
  joinlet_4261:;
  moonbit_incref(_M0L5bytesS1026.$0);
  #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS3352 = _M0MPC15bytes9BytesView6length(_M0L5bytesS1026);
  _M0L6_2atmpS3351 = _M0L6_2atmpS3352 * 2;
  _M0L1tS1034 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS3351, 0);
  _M0L11_2aparam__0S1036 = 0;
  _M0L11_2aparam__1S1037 = _M0L5bytesS1026;
  while (1) {
    int32_t _M0L4tlenS1039;
    struct _M0TPC15bytes9BytesView _M0L4restS1040;
    struct _M0TPC15bytes9BytesView _M0L4restS1043;
    int32_t _M0L4tlenS1044;
    struct _M0TPC15bytes9BytesView _M0L4restS1046;
    int32_t _M0L4tlenS1047;
    struct _M0TPC15bytes9BytesView _M0L4restS1049;
    int32_t _M0L4tlenS1050;
    int32_t _M0L4tlenS1052;
    int32_t _M0L2b0S1053;
    int32_t _M0L2b1S1054;
    int32_t _M0L2b2S1055;
    int32_t _M0L2b3S1056;
    struct _M0TPC15bytes9BytesView _M0L4restS1057;
    int32_t _M0L4tlenS1063;
    int32_t _M0L2b0S1064;
    int32_t _M0L2b1S1065;
    int32_t _M0L2b2S1066;
    struct _M0TPC15bytes9BytesView _M0L4restS1067;
    int32_t _M0L4tlenS1070;
    struct _M0TPC15bytes9BytesView _M0L4restS1071;
    int32_t _M0L2b0S1072;
    int32_t _M0L2b1S1073;
    int32_t _M0L4tlenS1076;
    struct _M0TPC15bytes9BytesView _M0L4restS1077;
    int32_t _M0L1bS1078;
    int32_t _M0L3endS2731 = _M0L11_2aparam__1S1037.$2;
    int32_t _M0L5startS2732 = _M0L11_2aparam__1S1037.$1;
    int32_t _M0L6_2atmpS2730 = _M0L3endS2731 - _M0L5startS2732;
    int32_t _M0L6_2atmpS2729;
    int32_t _M0L6_2atmpS2728;
    int32_t _M0L6_2atmpS2727;
    int32_t _M0L6_2atmpS2724;
    int32_t _M0L6_2atmpS2726;
    int32_t _M0L6_2atmpS2725;
    int32_t _M0L2chS1074;
    int32_t _M0L6_2atmpS2719;
    int32_t _M0L6_2atmpS2720;
    int32_t _M0L6_2atmpS2722;
    int32_t _M0L6_2atmpS2721;
    int32_t _M0L6_2atmpS2723;
    int32_t _M0L6_2atmpS2718;
    int32_t _M0L6_2atmpS2717;
    int32_t _M0L6_2atmpS2713;
    int32_t _M0L6_2atmpS2716;
    int32_t _M0L6_2atmpS2715;
    int32_t _M0L6_2atmpS2714;
    int32_t _M0L6_2atmpS2710;
    int32_t _M0L6_2atmpS2712;
    int32_t _M0L6_2atmpS2711;
    int32_t _M0L2chS1068;
    int32_t _M0L6_2atmpS2705;
    int32_t _M0L6_2atmpS2706;
    int32_t _M0L6_2atmpS2708;
    int32_t _M0L6_2atmpS2707;
    int32_t _M0L6_2atmpS2709;
    int32_t _M0L6_2atmpS2704;
    int32_t _M0L6_2atmpS2703;
    int32_t _M0L6_2atmpS2699;
    int32_t _M0L6_2atmpS2702;
    int32_t _M0L6_2atmpS2701;
    int32_t _M0L6_2atmpS2700;
    int32_t _M0L6_2atmpS2695;
    int32_t _M0L6_2atmpS2698;
    int32_t _M0L6_2atmpS2697;
    int32_t _M0L6_2atmpS2696;
    int32_t _M0L6_2atmpS2692;
    int32_t _M0L6_2atmpS2694;
    int32_t _M0L6_2atmpS2693;
    int32_t _M0L2chS1058;
    int32_t _M0L3chmS1059;
    int32_t _M0L6_2atmpS2691;
    int32_t _M0L3ch1S1060;
    int32_t _M0L6_2atmpS2690;
    int32_t _M0L3ch2S1061;
    int32_t _M0L6_2atmpS2680;
    int32_t _M0L6_2atmpS2681;
    int32_t _M0L6_2atmpS2683;
    int32_t _M0L6_2atmpS2682;
    int32_t _M0L6_2atmpS2684;
    int32_t _M0L6_2atmpS2685;
    int32_t _M0L6_2atmpS2686;
    int32_t _M0L6_2atmpS2688;
    int32_t _M0L6_2atmpS2687;
    int32_t _M0L6_2atmpS2689;
    int32_t _M0L6_2atmpS2678;
    int32_t _M0L6_2atmpS2679;
    int32_t _M0L6_2atmpS2676;
    int32_t _M0L6_2atmpS2677;
    int32_t _M0L6_2atmpS2674;
    int32_t _M0L6_2atmpS2675;
    int32_t _M0L6_2atmpS2672;
    int32_t _M0L6_2atmpS2673;
    if (_M0L6_2atmpS2730 == 0) {
      moonbit_decref(_M0L11_2aparam__1S1037.$0);
      _M0L4tlenS1035 = _M0L11_2aparam__0S1036;
    } else {
      int32_t _M0L3endS2734 = _M0L11_2aparam__1S1037.$2;
      int32_t _M0L5startS2735 = _M0L11_2aparam__1S1037.$1;
      int32_t _M0L6_2atmpS2733 = _M0L3endS2734 - _M0L5startS2735;
      if (_M0L6_2atmpS2733 >= 8) {
        moonbit_bytes_t _M0L8_2afieldS3722 = _M0L11_2aparam__1S1037.$0;
        moonbit_bytes_t _M0L5bytesS2959 = _M0L8_2afieldS3722;
        int32_t _M0L5startS2960 = _M0L11_2aparam__1S1037.$1;
        int32_t _M0L6_2atmpS3721 = _M0L5bytesS2959[_M0L5startS2960];
        int32_t _M0L4_2axS1079 = _M0L6_2atmpS3721;
        if (_M0L4_2axS1079 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS3630 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2956 = _M0L8_2afieldS3630;
          int32_t _M0L5startS2958 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2957 = _M0L5startS2958 + 1;
          int32_t _M0L6_2atmpS3629 = _M0L5bytesS2956[_M0L6_2atmpS2957];
          int32_t _M0L4_2axS1080 = _M0L6_2atmpS3629;
          if (_M0L4_2axS1080 <= 127) {
            moonbit_bytes_t _M0L8_2afieldS3626 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2953 = _M0L8_2afieldS3626;
            int32_t _M0L5startS2955 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2954 = _M0L5startS2955 + 2;
            int32_t _M0L6_2atmpS3625 = _M0L5bytesS2953[_M0L6_2atmpS2954];
            int32_t _M0L4_2axS1081 = _M0L6_2atmpS3625;
            if (_M0L4_2axS1081 <= 127) {
              moonbit_bytes_t _M0L8_2afieldS3622 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2950 = _M0L8_2afieldS3622;
              int32_t _M0L5startS2952 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2951 = _M0L5startS2952 + 3;
              int32_t _M0L6_2atmpS3621 = _M0L5bytesS2950[_M0L6_2atmpS2951];
              int32_t _M0L4_2axS1082 = _M0L6_2atmpS3621;
              if (_M0L4_2axS1082 <= 127) {
                moonbit_bytes_t _M0L8_2afieldS3618 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2947 = _M0L8_2afieldS3618;
                int32_t _M0L5startS2949 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2948 = _M0L5startS2949 + 4;
                int32_t _M0L6_2atmpS3617 = _M0L5bytesS2947[_M0L6_2atmpS2948];
                int32_t _M0L4_2axS1083 = _M0L6_2atmpS3617;
                if (_M0L4_2axS1083 <= 127) {
                  moonbit_bytes_t _M0L8_2afieldS3614 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS2944 = _M0L8_2afieldS3614;
                  int32_t _M0L5startS2946 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS2945 = _M0L5startS2946 + 5;
                  int32_t _M0L6_2atmpS3613 =
                    _M0L5bytesS2944[_M0L6_2atmpS2945];
                  int32_t _M0L4_2axS1084 = _M0L6_2atmpS3613;
                  if (_M0L4_2axS1084 <= 127) {
                    moonbit_bytes_t _M0L8_2afieldS3610 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS2941 = _M0L8_2afieldS3610;
                    int32_t _M0L5startS2943 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS2942 = _M0L5startS2943 + 6;
                    int32_t _M0L6_2atmpS3609 =
                      _M0L5bytesS2941[_M0L6_2atmpS2942];
                    int32_t _M0L4_2axS1085 = _M0L6_2atmpS3609;
                    if (_M0L4_2axS1085 <= 127) {
                      moonbit_bytes_t _M0L8_2afieldS3606 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS2938 = _M0L8_2afieldS3606;
                      int32_t _M0L5startS2940 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS2939 = _M0L5startS2940 + 7;
                      int32_t _M0L6_2atmpS3605 =
                        _M0L5bytesS2938[_M0L6_2atmpS2939];
                      int32_t _M0L4_2axS1086 = _M0L6_2atmpS3605;
                      if (_M0L4_2axS1086 <= 127) {
                        moonbit_bytes_t _M0L8_2afieldS3602 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS2934 = _M0L8_2afieldS3602;
                        int32_t _M0L5startS2937 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS2935 = _M0L5startS2937 + 8;
                        int32_t _M0L8_2afieldS3601 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS2936 = _M0L8_2afieldS3601;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1087 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2935,
                                                             _M0L3endS2936,
                                                             _M0L5bytesS2934};
                        int32_t _M0L6_2atmpS2926;
                        int32_t _M0L6_2atmpS2927;
                        int32_t _M0L6_2atmpS2928;
                        int32_t _M0L6_2atmpS2929;
                        int32_t _M0L6_2atmpS2930;
                        int32_t _M0L6_2atmpS2931;
                        int32_t _M0L6_2atmpS2932;
                        int32_t _M0L6_2atmpS2933;
                        _M0L1tS1034[_M0L11_2aparam__0S1036] = _M0L4_2axS1079;
                        _M0L6_2atmpS2926 = _M0L11_2aparam__0S1036 + 2;
                        _M0L1tS1034[_M0L6_2atmpS2926] = _M0L4_2axS1080;
                        _M0L6_2atmpS2927 = _M0L11_2aparam__0S1036 + 4;
                        _M0L1tS1034[_M0L6_2atmpS2927] = _M0L4_2axS1081;
                        _M0L6_2atmpS2928 = _M0L11_2aparam__0S1036 + 6;
                        _M0L1tS1034[_M0L6_2atmpS2928] = _M0L4_2axS1082;
                        _M0L6_2atmpS2929 = _M0L11_2aparam__0S1036 + 8;
                        _M0L1tS1034[_M0L6_2atmpS2929] = _M0L4_2axS1083;
                        _M0L6_2atmpS2930 = _M0L11_2aparam__0S1036 + 10;
                        _M0L1tS1034[_M0L6_2atmpS2930] = _M0L4_2axS1084;
                        _M0L6_2atmpS2931 = _M0L11_2aparam__0S1036 + 12;
                        _M0L1tS1034[_M0L6_2atmpS2931] = _M0L4_2axS1085;
                        _M0L6_2atmpS2932 = _M0L11_2aparam__0S1036 + 14;
                        _M0L1tS1034[_M0L6_2atmpS2932] = _M0L4_2axS1086;
                        _M0L6_2atmpS2933 = _M0L11_2aparam__0S1036 + 16;
                        _M0L11_2aparam__0S1036 = _M0L6_2atmpS2933;
                        _M0L11_2aparam__1S1037 = _M0L4_2axS1087;
                        continue;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3604 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS2922 = _M0L8_2afieldS3604;
                        int32_t _M0L5startS2925 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS2923 = _M0L5startS2925 + 1;
                        int32_t _M0L8_2afieldS3603 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS2924 = _M0L8_2afieldS3603;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1088 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2923,
                                                             _M0L3endS2924,
                                                             _M0L5bytesS2922};
                        _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
                        _M0L4restS1077 = _M0L4_2axS1088;
                        _M0L1bS1078 = _M0L4_2axS1079;
                        goto join_1075;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3608 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS2918 = _M0L8_2afieldS3608;
                      int32_t _M0L5startS2921 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS2919 = _M0L5startS2921 + 1;
                      int32_t _M0L8_2afieldS3607 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS2920 = _M0L8_2afieldS3607;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1089 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2919,
                                                           _M0L3endS2920,
                                                           _M0L5bytesS2918};
                      _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
                      _M0L4restS1077 = _M0L4_2axS1089;
                      _M0L1bS1078 = _M0L4_2axS1079;
                      goto join_1075;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3612 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS2914 = _M0L8_2afieldS3612;
                    int32_t _M0L5startS2917 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS2915 = _M0L5startS2917 + 1;
                    int32_t _M0L8_2afieldS3611 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L3endS2916 = _M0L8_2afieldS3611;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1090 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2915,
                                                         _M0L3endS2916,
                                                         _M0L5bytesS2914};
                    _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
                    _M0L4restS1077 = _M0L4_2axS1090;
                    _M0L1bS1078 = _M0L4_2axS1079;
                    goto join_1075;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3616 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS2910 = _M0L8_2afieldS3616;
                  int32_t _M0L5startS2913 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS2911 = _M0L5startS2913 + 1;
                  int32_t _M0L8_2afieldS3615 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS2912 = _M0L8_2afieldS3615;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1091 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2911,
                                                       _M0L3endS2912,
                                                       _M0L5bytesS2910};
                  _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
                  _M0L4restS1077 = _M0L4_2axS1091;
                  _M0L1bS1078 = _M0L4_2axS1079;
                  goto join_1075;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS3620 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2906 = _M0L8_2afieldS3620;
                int32_t _M0L5startS2909 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2907 = _M0L5startS2909 + 1;
                int32_t _M0L8_2afieldS3619 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2908 = _M0L8_2afieldS3619;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1092 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2907,
                                                     _M0L3endS2908,
                                                     _M0L5bytesS2906};
                _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
                _M0L4restS1077 = _M0L4_2axS1092;
                _M0L1bS1078 = _M0L4_2axS1079;
                goto join_1075;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3624 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2902 = _M0L8_2afieldS3624;
              int32_t _M0L5startS2905 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2903 = _M0L5startS2905 + 1;
              int32_t _M0L8_2afieldS3623 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2904 = _M0L8_2afieldS3623;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1093 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2903,
                                                   _M0L3endS2904,
                                                   _M0L5bytesS2902};
              _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
              _M0L4restS1077 = _M0L4_2axS1093;
              _M0L1bS1078 = _M0L4_2axS1079;
              goto join_1075;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3628 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2898 = _M0L8_2afieldS3628;
            int32_t _M0L5startS2901 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2899 = _M0L5startS2901 + 1;
            int32_t _M0L8_2afieldS3627 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2900 = _M0L8_2afieldS3627;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1094 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2899,
                                                 _M0L3endS2900,
                                                 _M0L5bytesS2898};
            _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
            _M0L4restS1077 = _M0L4_2axS1094;
            _M0L1bS1078 = _M0L4_2axS1079;
            goto join_1075;
          }
        } else if (_M0L4_2axS1079 >= 194 && _M0L4_2axS1079 <= 223) {
          moonbit_bytes_t _M0L8_2afieldS3636 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2895 = _M0L8_2afieldS3636;
          int32_t _M0L5startS2897 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2896 = _M0L5startS2897 + 1;
          int32_t _M0L6_2atmpS3635 = _M0L5bytesS2895[_M0L6_2atmpS2896];
          int32_t _M0L4_2axS1095 = _M0L6_2atmpS3635;
          if (_M0L4_2axS1095 >= 128 && _M0L4_2axS1095 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3632 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2891 = _M0L8_2afieldS3632;
            int32_t _M0L5startS2894 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2892 = _M0L5startS2894 + 2;
            int32_t _M0L8_2afieldS3631 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2893 = _M0L8_2afieldS3631;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1096 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2892,
                                                 _M0L3endS2893,
                                                 _M0L5bytesS2891};
            _M0L4tlenS1070 = _M0L11_2aparam__0S1036;
            _M0L4restS1071 = _M0L4_2axS1096;
            _M0L2b0S1072 = _M0L4_2axS1079;
            _M0L2b1S1073 = _M0L4_2axS1095;
            goto join_1069;
          } else {
            moonbit_bytes_t _M0L8_2afieldS3634 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2887 = _M0L8_2afieldS3634;
            int32_t _M0L5startS2890 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2888 = _M0L5startS2890 + 1;
            int32_t _M0L8_2afieldS3633 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2889 = _M0L8_2afieldS3633;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1097 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2888,
                                                 _M0L3endS2889,
                                                 _M0L5bytesS2887};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1097;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 == 224) {
          moonbit_bytes_t _M0L8_2afieldS3646 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2884 = _M0L8_2afieldS3646;
          int32_t _M0L5startS2886 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2885 = _M0L5startS2886 + 1;
          int32_t _M0L6_2atmpS3645 = _M0L5bytesS2884[_M0L6_2atmpS2885];
          int32_t _M0L4_2axS1098 = _M0L6_2atmpS3645;
          if (_M0L4_2axS1098 >= 160 && _M0L4_2axS1098 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3642 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2881 = _M0L8_2afieldS3642;
            int32_t _M0L5startS2883 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2882 = _M0L5startS2883 + 2;
            int32_t _M0L6_2atmpS3641 = _M0L5bytesS2881[_M0L6_2atmpS2882];
            int32_t _M0L4_2axS1099 = _M0L6_2atmpS3641;
            if (_M0L4_2axS1099 >= 128 && _M0L4_2axS1099 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3638 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2877 = _M0L8_2afieldS3638;
              int32_t _M0L5startS2880 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2878 = _M0L5startS2880 + 3;
              int32_t _M0L8_2afieldS3637 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2879 = _M0L8_2afieldS3637;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1100 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2878,
                                                   _M0L3endS2879,
                                                   _M0L5bytesS2877};
              _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
              _M0L2b0S1064 = _M0L4_2axS1079;
              _M0L2b1S1065 = _M0L4_2axS1098;
              _M0L2b2S1066 = _M0L4_2axS1099;
              _M0L4restS1067 = _M0L4_2axS1100;
              goto join_1062;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3640 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2873 = _M0L8_2afieldS3640;
              int32_t _M0L5startS2876 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2874 = _M0L5startS2876 + 2;
              int32_t _M0L8_2afieldS3639 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2875 = _M0L8_2afieldS3639;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1101 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2874,
                                                   _M0L3endS2875,
                                                   _M0L5bytesS2873};
              _M0L4restS1049 = _M0L4_2axS1101;
              _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
              goto join_1048;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3644 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2869 = _M0L8_2afieldS3644;
            int32_t _M0L5startS2872 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2870 = _M0L5startS2872 + 1;
            int32_t _M0L8_2afieldS3643 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2871 = _M0L8_2afieldS3643;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1102 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2870,
                                                 _M0L3endS2871,
                                                 _M0L5bytesS2869};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1102;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 >= 225 && _M0L4_2axS1079 <= 236) {
          moonbit_bytes_t _M0L8_2afieldS3656 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2866 = _M0L8_2afieldS3656;
          int32_t _M0L5startS2868 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2867 = _M0L5startS2868 + 1;
          int32_t _M0L6_2atmpS3655 = _M0L5bytesS2866[_M0L6_2atmpS2867];
          int32_t _M0L4_2axS1103 = _M0L6_2atmpS3655;
          if (_M0L4_2axS1103 >= 128 && _M0L4_2axS1103 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3652 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2863 = _M0L8_2afieldS3652;
            int32_t _M0L5startS2865 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2864 = _M0L5startS2865 + 2;
            int32_t _M0L6_2atmpS3651 = _M0L5bytesS2863[_M0L6_2atmpS2864];
            int32_t _M0L4_2axS1104 = _M0L6_2atmpS3651;
            if (_M0L4_2axS1104 >= 128 && _M0L4_2axS1104 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3648 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2859 = _M0L8_2afieldS3648;
              int32_t _M0L5startS2862 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2860 = _M0L5startS2862 + 3;
              int32_t _M0L8_2afieldS3647 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2861 = _M0L8_2afieldS3647;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1105 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2860,
                                                   _M0L3endS2861,
                                                   _M0L5bytesS2859};
              _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
              _M0L2b0S1064 = _M0L4_2axS1079;
              _M0L2b1S1065 = _M0L4_2axS1103;
              _M0L2b2S1066 = _M0L4_2axS1104;
              _M0L4restS1067 = _M0L4_2axS1105;
              goto join_1062;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3650 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2855 = _M0L8_2afieldS3650;
              int32_t _M0L5startS2858 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2856 = _M0L5startS2858 + 2;
              int32_t _M0L8_2afieldS3649 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2857 = _M0L8_2afieldS3649;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1106 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2856,
                                                   _M0L3endS2857,
                                                   _M0L5bytesS2855};
              _M0L4restS1049 = _M0L4_2axS1106;
              _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
              goto join_1048;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3654 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2851 = _M0L8_2afieldS3654;
            int32_t _M0L5startS2854 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2852 = _M0L5startS2854 + 1;
            int32_t _M0L8_2afieldS3653 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2853 = _M0L8_2afieldS3653;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1107 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2852,
                                                 _M0L3endS2853,
                                                 _M0L5bytesS2851};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1107;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 == 237) {
          moonbit_bytes_t _M0L8_2afieldS3666 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2848 = _M0L8_2afieldS3666;
          int32_t _M0L5startS2850 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2849 = _M0L5startS2850 + 1;
          int32_t _M0L6_2atmpS3665 = _M0L5bytesS2848[_M0L6_2atmpS2849];
          int32_t _M0L4_2axS1108 = _M0L6_2atmpS3665;
          if (_M0L4_2axS1108 >= 128 && _M0L4_2axS1108 <= 159) {
            moonbit_bytes_t _M0L8_2afieldS3662 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2845 = _M0L8_2afieldS3662;
            int32_t _M0L5startS2847 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2846 = _M0L5startS2847 + 2;
            int32_t _M0L6_2atmpS3661 = _M0L5bytesS2845[_M0L6_2atmpS2846];
            int32_t _M0L4_2axS1109 = _M0L6_2atmpS3661;
            if (_M0L4_2axS1109 >= 128 && _M0L4_2axS1109 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3658 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2841 = _M0L8_2afieldS3658;
              int32_t _M0L5startS2844 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2842 = _M0L5startS2844 + 3;
              int32_t _M0L8_2afieldS3657 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2843 = _M0L8_2afieldS3657;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1110 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2842,
                                                   _M0L3endS2843,
                                                   _M0L5bytesS2841};
              _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
              _M0L2b0S1064 = _M0L4_2axS1079;
              _M0L2b1S1065 = _M0L4_2axS1108;
              _M0L2b2S1066 = _M0L4_2axS1109;
              _M0L4restS1067 = _M0L4_2axS1110;
              goto join_1062;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3660 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2837 = _M0L8_2afieldS3660;
              int32_t _M0L5startS2840 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2838 = _M0L5startS2840 + 2;
              int32_t _M0L8_2afieldS3659 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2839 = _M0L8_2afieldS3659;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1111 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2838,
                                                   _M0L3endS2839,
                                                   _M0L5bytesS2837};
              _M0L4restS1049 = _M0L4_2axS1111;
              _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
              goto join_1048;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3664 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2833 = _M0L8_2afieldS3664;
            int32_t _M0L5startS2836 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2834 = _M0L5startS2836 + 1;
            int32_t _M0L8_2afieldS3663 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2835 = _M0L8_2afieldS3663;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1112 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2834,
                                                 _M0L3endS2835,
                                                 _M0L5bytesS2833};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1112;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 >= 238 && _M0L4_2axS1079 <= 239) {
          moonbit_bytes_t _M0L8_2afieldS3676 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2830 = _M0L8_2afieldS3676;
          int32_t _M0L5startS2832 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2831 = _M0L5startS2832 + 1;
          int32_t _M0L6_2atmpS3675 = _M0L5bytesS2830[_M0L6_2atmpS2831];
          int32_t _M0L4_2axS1113 = _M0L6_2atmpS3675;
          if (_M0L4_2axS1113 >= 128 && _M0L4_2axS1113 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3672 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2827 = _M0L8_2afieldS3672;
            int32_t _M0L5startS2829 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2828 = _M0L5startS2829 + 2;
            int32_t _M0L6_2atmpS3671 = _M0L5bytesS2827[_M0L6_2atmpS2828];
            int32_t _M0L4_2axS1114 = _M0L6_2atmpS3671;
            if (_M0L4_2axS1114 >= 128 && _M0L4_2axS1114 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3668 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2823 = _M0L8_2afieldS3668;
              int32_t _M0L5startS2826 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2824 = _M0L5startS2826 + 3;
              int32_t _M0L8_2afieldS3667 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2825 = _M0L8_2afieldS3667;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1115 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2824,
                                                   _M0L3endS2825,
                                                   _M0L5bytesS2823};
              _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
              _M0L2b0S1064 = _M0L4_2axS1079;
              _M0L2b1S1065 = _M0L4_2axS1113;
              _M0L2b2S1066 = _M0L4_2axS1114;
              _M0L4restS1067 = _M0L4_2axS1115;
              goto join_1062;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3670 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2819 = _M0L8_2afieldS3670;
              int32_t _M0L5startS2822 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2820 = _M0L5startS2822 + 2;
              int32_t _M0L8_2afieldS3669 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2821 = _M0L8_2afieldS3669;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1116 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2820,
                                                   _M0L3endS2821,
                                                   _M0L5bytesS2819};
              _M0L4restS1049 = _M0L4_2axS1116;
              _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
              goto join_1048;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3674 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2815 = _M0L8_2afieldS3674;
            int32_t _M0L5startS2818 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2816 = _M0L5startS2818 + 1;
            int32_t _M0L8_2afieldS3673 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2817 = _M0L8_2afieldS3673;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1117 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2816,
                                                 _M0L3endS2817,
                                                 _M0L5bytesS2815};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1117;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 == 240) {
          moonbit_bytes_t _M0L8_2afieldS3690 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2812 = _M0L8_2afieldS3690;
          int32_t _M0L5startS2814 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2813 = _M0L5startS2814 + 1;
          int32_t _M0L6_2atmpS3689 = _M0L5bytesS2812[_M0L6_2atmpS2813];
          int32_t _M0L4_2axS1118 = _M0L6_2atmpS3689;
          if (_M0L4_2axS1118 >= 144 && _M0L4_2axS1118 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3686 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2809 = _M0L8_2afieldS3686;
            int32_t _M0L5startS2811 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2810 = _M0L5startS2811 + 2;
            int32_t _M0L6_2atmpS3685 = _M0L5bytesS2809[_M0L6_2atmpS2810];
            int32_t _M0L4_2axS1119 = _M0L6_2atmpS3685;
            if (_M0L4_2axS1119 >= 128 && _M0L4_2axS1119 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3682 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2806 = _M0L8_2afieldS3682;
              int32_t _M0L5startS2808 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2807 = _M0L5startS2808 + 3;
              int32_t _M0L6_2atmpS3681 = _M0L5bytesS2806[_M0L6_2atmpS2807];
              int32_t _M0L4_2axS1120 = _M0L6_2atmpS3681;
              if (_M0L4_2axS1120 >= 128 && _M0L4_2axS1120 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3678 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2802 = _M0L8_2afieldS3678;
                int32_t _M0L5startS2805 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2803 = _M0L5startS2805 + 4;
                int32_t _M0L8_2afieldS3677 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2804 = _M0L8_2afieldS3677;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1121 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2803,
                                                     _M0L3endS2804,
                                                     _M0L5bytesS2802};
                _M0L4tlenS1052 = _M0L11_2aparam__0S1036;
                _M0L2b0S1053 = _M0L4_2axS1079;
                _M0L2b1S1054 = _M0L4_2axS1118;
                _M0L2b2S1055 = _M0L4_2axS1119;
                _M0L2b3S1056 = _M0L4_2axS1120;
                _M0L4restS1057 = _M0L4_2axS1121;
                goto join_1051;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3680 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2798 = _M0L8_2afieldS3680;
                int32_t _M0L5startS2801 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2799 = _M0L5startS2801 + 3;
                int32_t _M0L8_2afieldS3679 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2800 = _M0L8_2afieldS3679;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1122 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2799,
                                                     _M0L3endS2800,
                                                     _M0L5bytesS2798};
                _M0L4restS1046 = _M0L4_2axS1122;
                _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                goto join_1045;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3684 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2794 = _M0L8_2afieldS3684;
              int32_t _M0L5startS2797 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2795 = _M0L5startS2797 + 2;
              int32_t _M0L8_2afieldS3683 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2796 = _M0L8_2afieldS3683;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1123 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2795,
                                                   _M0L3endS2796,
                                                   _M0L5bytesS2794};
              _M0L4restS1043 = _M0L4_2axS1123;
              _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
              goto join_1042;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3688 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2790 = _M0L8_2afieldS3688;
            int32_t _M0L5startS2793 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2791 = _M0L5startS2793 + 1;
            int32_t _M0L8_2afieldS3687 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2792 = _M0L8_2afieldS3687;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1124 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2791,
                                                 _M0L3endS2792,
                                                 _M0L5bytesS2790};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1124;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 >= 241 && _M0L4_2axS1079 <= 243) {
          moonbit_bytes_t _M0L8_2afieldS3704 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2787 = _M0L8_2afieldS3704;
          int32_t _M0L5startS2789 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2788 = _M0L5startS2789 + 1;
          int32_t _M0L6_2atmpS3703 = _M0L5bytesS2787[_M0L6_2atmpS2788];
          int32_t _M0L4_2axS1125 = _M0L6_2atmpS3703;
          if (_M0L4_2axS1125 >= 128 && _M0L4_2axS1125 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3700 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2784 = _M0L8_2afieldS3700;
            int32_t _M0L5startS2786 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2785 = _M0L5startS2786 + 2;
            int32_t _M0L6_2atmpS3699 = _M0L5bytesS2784[_M0L6_2atmpS2785];
            int32_t _M0L4_2axS1126 = _M0L6_2atmpS3699;
            if (_M0L4_2axS1126 >= 128 && _M0L4_2axS1126 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3696 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2781 = _M0L8_2afieldS3696;
              int32_t _M0L5startS2783 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2782 = _M0L5startS2783 + 3;
              int32_t _M0L6_2atmpS3695 = _M0L5bytesS2781[_M0L6_2atmpS2782];
              int32_t _M0L4_2axS1127 = _M0L6_2atmpS3695;
              if (_M0L4_2axS1127 >= 128 && _M0L4_2axS1127 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3692 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2777 = _M0L8_2afieldS3692;
                int32_t _M0L5startS2780 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2778 = _M0L5startS2780 + 4;
                int32_t _M0L8_2afieldS3691 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2779 = _M0L8_2afieldS3691;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1128 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2778,
                                                     _M0L3endS2779,
                                                     _M0L5bytesS2777};
                _M0L4tlenS1052 = _M0L11_2aparam__0S1036;
                _M0L2b0S1053 = _M0L4_2axS1079;
                _M0L2b1S1054 = _M0L4_2axS1125;
                _M0L2b2S1055 = _M0L4_2axS1126;
                _M0L2b3S1056 = _M0L4_2axS1127;
                _M0L4restS1057 = _M0L4_2axS1128;
                goto join_1051;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3694 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2773 = _M0L8_2afieldS3694;
                int32_t _M0L5startS2776 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2774 = _M0L5startS2776 + 3;
                int32_t _M0L8_2afieldS3693 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2775 = _M0L8_2afieldS3693;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1129 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2774,
                                                     _M0L3endS2775,
                                                     _M0L5bytesS2773};
                _M0L4restS1046 = _M0L4_2axS1129;
                _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                goto join_1045;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3698 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2769 = _M0L8_2afieldS3698;
              int32_t _M0L5startS2772 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2770 = _M0L5startS2772 + 2;
              int32_t _M0L8_2afieldS3697 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2771 = _M0L8_2afieldS3697;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1130 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2770,
                                                   _M0L3endS2771,
                                                   _M0L5bytesS2769};
              _M0L4restS1043 = _M0L4_2axS1130;
              _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
              goto join_1042;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3702 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2765 = _M0L8_2afieldS3702;
            int32_t _M0L5startS2768 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2766 = _M0L5startS2768 + 1;
            int32_t _M0L8_2afieldS3701 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2767 = _M0L8_2afieldS3701;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1131 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2766,
                                                 _M0L3endS2767,
                                                 _M0L5bytesS2765};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1131;
            goto join_1038;
          }
        } else if (_M0L4_2axS1079 == 244) {
          moonbit_bytes_t _M0L8_2afieldS3718 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2762 = _M0L8_2afieldS3718;
          int32_t _M0L5startS2764 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2763 = _M0L5startS2764 + 1;
          int32_t _M0L6_2atmpS3717 = _M0L5bytesS2762[_M0L6_2atmpS2763];
          int32_t _M0L4_2axS1132 = _M0L6_2atmpS3717;
          if (_M0L4_2axS1132 >= 128 && _M0L4_2axS1132 <= 143) {
            moonbit_bytes_t _M0L8_2afieldS3714 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2759 = _M0L8_2afieldS3714;
            int32_t _M0L5startS2761 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2760 = _M0L5startS2761 + 2;
            int32_t _M0L6_2atmpS3713 = _M0L5bytesS2759[_M0L6_2atmpS2760];
            int32_t _M0L4_2axS1133 = _M0L6_2atmpS3713;
            if (_M0L4_2axS1133 >= 128 && _M0L4_2axS1133 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3710 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2756 = _M0L8_2afieldS3710;
              int32_t _M0L5startS2758 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2757 = _M0L5startS2758 + 3;
              int32_t _M0L6_2atmpS3709 = _M0L5bytesS2756[_M0L6_2atmpS2757];
              int32_t _M0L4_2axS1134 = _M0L6_2atmpS3709;
              if (_M0L4_2axS1134 >= 128 && _M0L4_2axS1134 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3706 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2752 = _M0L8_2afieldS3706;
                int32_t _M0L5startS2755 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2753 = _M0L5startS2755 + 4;
                int32_t _M0L8_2afieldS3705 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2754 = _M0L8_2afieldS3705;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1135 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2753,
                                                     _M0L3endS2754,
                                                     _M0L5bytesS2752};
                _M0L4tlenS1052 = _M0L11_2aparam__0S1036;
                _M0L2b0S1053 = _M0L4_2axS1079;
                _M0L2b1S1054 = _M0L4_2axS1132;
                _M0L2b2S1055 = _M0L4_2axS1133;
                _M0L2b3S1056 = _M0L4_2axS1134;
                _M0L4restS1057 = _M0L4_2axS1135;
                goto join_1051;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3708 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS2748 = _M0L8_2afieldS3708;
                int32_t _M0L5startS2751 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS2749 = _M0L5startS2751 + 3;
                int32_t _M0L8_2afieldS3707 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS2750 = _M0L8_2afieldS3707;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1136 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2749,
                                                     _M0L3endS2750,
                                                     _M0L5bytesS2748};
                _M0L4restS1046 = _M0L4_2axS1136;
                _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                goto join_1045;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3712 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS2744 = _M0L8_2afieldS3712;
              int32_t _M0L5startS2747 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2745 = _M0L5startS2747 + 2;
              int32_t _M0L8_2afieldS3711 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L3endS2746 = _M0L8_2afieldS3711;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1137 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2745,
                                                   _M0L3endS2746,
                                                   _M0L5bytesS2744};
              _M0L4restS1043 = _M0L4_2axS1137;
              _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
              goto join_1042;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3716 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS2740 = _M0L8_2afieldS3716;
            int32_t _M0L5startS2743 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS2741 = _M0L5startS2743 + 1;
            int32_t _M0L8_2afieldS3715 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS2742 = _M0L8_2afieldS3715;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1138 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2741,
                                                 _M0L3endS2742,
                                                 _M0L5bytesS2740};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1138;
            goto join_1038;
          }
        } else {
          moonbit_bytes_t _M0L8_2afieldS3720 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS2736 = _M0L8_2afieldS3720;
          int32_t _M0L5startS2739 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2737 = _M0L5startS2739 + 1;
          int32_t _M0L8_2afieldS3719 = _M0L11_2aparam__1S1037.$2;
          int32_t _M0L3endS2738 = _M0L8_2afieldS3719;
          struct _M0TPC15bytes9BytesView _M0L4_2axS1139 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2737,
                                               _M0L3endS2738,
                                               _M0L5bytesS2736};
          _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
          _M0L4restS1040 = _M0L4_2axS1139;
          goto join_1038;
        }
      } else {
        moonbit_bytes_t _M0L8_2afieldS3914 = _M0L11_2aparam__1S1037.$0;
        moonbit_bytes_t _M0L5bytesS3349 = _M0L8_2afieldS3914;
        int32_t _M0L5startS3350 = _M0L11_2aparam__1S1037.$1;
        int32_t _M0L6_2atmpS3913 = _M0L5bytesS3349[_M0L5startS3350];
        int32_t _M0L4_2axS1140 = _M0L6_2atmpS3913;
        if (_M0L4_2axS1140 >= 0 && _M0L4_2axS1140 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS3724 = _M0L11_2aparam__1S1037.$0;
          moonbit_bytes_t _M0L5bytesS3345 = _M0L8_2afieldS3724;
          int32_t _M0L5startS3348 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS3346 = _M0L5startS3348 + 1;
          int32_t _M0L8_2afieldS3723 = _M0L11_2aparam__1S1037.$2;
          int32_t _M0L3endS3347 = _M0L8_2afieldS3723;
          struct _M0TPC15bytes9BytesView _M0L4_2axS1141 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3346,
                                               _M0L3endS3347,
                                               _M0L5bytesS3345};
          _M0L4tlenS1076 = _M0L11_2aparam__0S1036;
          _M0L4restS1077 = _M0L4_2axS1141;
          _M0L1bS1078 = _M0L4_2axS1140;
          goto join_1075;
        } else {
          int32_t _M0L3endS2962 = _M0L11_2aparam__1S1037.$2;
          int32_t _M0L5startS2963 = _M0L11_2aparam__1S1037.$1;
          int32_t _M0L6_2atmpS2961 = _M0L3endS2962 - _M0L5startS2963;
          if (_M0L6_2atmpS2961 >= 2) {
            if (_M0L4_2axS1140 >= 194 && _M0L4_2axS1140 <= 223) {
              moonbit_bytes_t _M0L8_2afieldS3734 = _M0L11_2aparam__1S1037.$0;
              moonbit_bytes_t _M0L5bytesS3338 = _M0L8_2afieldS3734;
              int32_t _M0L5startS3340 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS3339 = _M0L5startS3340 + 1;
              int32_t _M0L6_2atmpS3733 = _M0L5bytesS3338[_M0L6_2atmpS3339];
              int32_t _M0L4_2axS1142 = _M0L6_2atmpS3733;
              if (_M0L4_2axS1142 >= 128 && _M0L4_2axS1142 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3726 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3334 = _M0L8_2afieldS3726;
                int32_t _M0L5startS3337 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3335 = _M0L5startS3337 + 2;
                int32_t _M0L8_2afieldS3725 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS3336 = _M0L8_2afieldS3725;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1143 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3335,
                                                     _M0L3endS3336,
                                                     _M0L5bytesS3334};
                _M0L4tlenS1070 = _M0L11_2aparam__0S1036;
                _M0L4restS1071 = _M0L4_2axS1143;
                _M0L2b0S1072 = _M0L4_2axS1140;
                _M0L2b1S1073 = _M0L4_2axS1142;
                goto join_1069;
              } else {
                int32_t _M0L3endS3317 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L5startS3318 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3316 = _M0L3endS3317 - _M0L5startS3318;
                if (_M0L6_2atmpS3316 >= 3) {
                  int32_t _M0L3endS3320 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L5startS3321 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3319 = _M0L3endS3320 - _M0L5startS3321;
                  if (_M0L6_2atmpS3319 >= 4) {
                    moonbit_bytes_t _M0L8_2afieldS3728 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3322 = _M0L8_2afieldS3728;
                    int32_t _M0L5startS3325 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3323 = _M0L5startS3325 + 1;
                    int32_t _M0L8_2afieldS3727 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L3endS3324 = _M0L8_2afieldS3727;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1144 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3323,
                                                         _M0L3endS3324,
                                                         _M0L5bytesS3322};
                    _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                    _M0L4restS1040 = _M0L4_2axS1144;
                    goto join_1038;
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3730 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3326 = _M0L8_2afieldS3730;
                    int32_t _M0L5startS3329 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3327 = _M0L5startS3329 + 1;
                    int32_t _M0L8_2afieldS3729 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L3endS3328 = _M0L8_2afieldS3729;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1145 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3327,
                                                         _M0L3endS3328,
                                                         _M0L5bytesS3326};
                    _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                    _M0L4restS1040 = _M0L4_2axS1145;
                    goto join_1038;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3732 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3330 = _M0L8_2afieldS3732;
                  int32_t _M0L5startS3333 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3331 = _M0L5startS3333 + 1;
                  int32_t _M0L8_2afieldS3731 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3332 = _M0L8_2afieldS3731;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1146 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3331,
                                                       _M0L3endS3332,
                                                       _M0L5bytesS3330};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1146;
                  goto join_1038;
                }
              }
            } else {
              int32_t _M0L3endS2965 = _M0L11_2aparam__1S1037.$2;
              int32_t _M0L5startS2966 = _M0L11_2aparam__1S1037.$1;
              int32_t _M0L6_2atmpS2964 = _M0L3endS2965 - _M0L5startS2966;
              if (_M0L6_2atmpS2964 >= 3) {
                if (_M0L4_2axS1140 == 224) {
                  moonbit_bytes_t _M0L8_2afieldS3748 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3232 = _M0L8_2afieldS3748;
                  int32_t _M0L5startS3234 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3233 = _M0L5startS3234 + 1;
                  int32_t _M0L6_2atmpS3747 =
                    _M0L5bytesS3232[_M0L6_2atmpS3233];
                  int32_t _M0L4_2axS1147 = _M0L6_2atmpS3747;
                  if (_M0L4_2axS1147 >= 160 && _M0L4_2axS1147 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3742 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3229 = _M0L8_2afieldS3742;
                    int32_t _M0L5startS3231 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3230 = _M0L5startS3231 + 2;
                    int32_t _M0L6_2atmpS3741 =
                      _M0L5bytesS3229[_M0L6_2atmpS3230];
                    int32_t _M0L4_2axS1148 = _M0L6_2atmpS3741;
                    if (_M0L4_2axS1148 >= 128 && _M0L4_2axS1148 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3736 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3225 = _M0L8_2afieldS3736;
                      int32_t _M0L5startS3228 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3226 = _M0L5startS3228 + 3;
                      int32_t _M0L8_2afieldS3735 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3227 = _M0L8_2afieldS3735;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1149 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3226,
                                                           _M0L3endS3227,
                                                           _M0L5bytesS3225};
                      _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
                      _M0L2b0S1064 = _M0L4_2axS1140;
                      _M0L2b1S1065 = _M0L4_2axS1147;
                      _M0L2b2S1066 = _M0L4_2axS1148;
                      _M0L4restS1067 = _M0L4_2axS1149;
                      goto join_1062;
                    } else {
                      int32_t _M0L3endS3215 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L5startS3216 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3214 =
                        _M0L3endS3215 - _M0L5startS3216;
                      if (_M0L6_2atmpS3214 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3738 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3217 = _M0L8_2afieldS3738;
                        int32_t _M0L5startS3220 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3218 = _M0L5startS3220 + 2;
                        int32_t _M0L8_2afieldS3737 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3219 = _M0L8_2afieldS3737;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1150 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3218,
                                                             _M0L3endS3219,
                                                             _M0L5bytesS3217};
                        _M0L4restS1049 = _M0L4_2axS1150;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3740 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3221 = _M0L8_2afieldS3740;
                        int32_t _M0L5startS3224 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3222 = _M0L5startS3224 + 2;
                        int32_t _M0L8_2afieldS3739 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3223 = _M0L8_2afieldS3739;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1151 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3222,
                                                             _M0L3endS3223,
                                                             _M0L5bytesS3221};
                        _M0L4restS1049 = _M0L4_2axS1151;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3204 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L5startS3205 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3203 =
                      _M0L3endS3204 - _M0L5startS3205;
                    if (_M0L6_2atmpS3203 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3744 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3206 = _M0L8_2afieldS3744;
                      int32_t _M0L5startS3209 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3207 = _M0L5startS3209 + 1;
                      int32_t _M0L8_2afieldS3743 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3208 = _M0L8_2afieldS3743;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1152 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3207,
                                                           _M0L3endS3208,
                                                           _M0L5bytesS3206};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1152;
                      goto join_1038;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3746 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3210 = _M0L8_2afieldS3746;
                      int32_t _M0L5startS3213 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3211 = _M0L5startS3213 + 1;
                      int32_t _M0L8_2afieldS3745 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3212 = _M0L8_2afieldS3745;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1153 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3211,
                                                           _M0L3endS3212,
                                                           _M0L5bytesS3210};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1153;
                      goto join_1038;
                    }
                  }
                } else if (_M0L4_2axS1140 >= 225 && _M0L4_2axS1140 <= 236) {
                  moonbit_bytes_t _M0L8_2afieldS3762 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3200 = _M0L8_2afieldS3762;
                  int32_t _M0L5startS3202 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3201 = _M0L5startS3202 + 1;
                  int32_t _M0L6_2atmpS3761 =
                    _M0L5bytesS3200[_M0L6_2atmpS3201];
                  int32_t _M0L4_2axS1154 = _M0L6_2atmpS3761;
                  if (_M0L4_2axS1154 >= 128 && _M0L4_2axS1154 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3756 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3197 = _M0L8_2afieldS3756;
                    int32_t _M0L5startS3199 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3198 = _M0L5startS3199 + 2;
                    int32_t _M0L6_2atmpS3755 =
                      _M0L5bytesS3197[_M0L6_2atmpS3198];
                    int32_t _M0L4_2axS1155 = _M0L6_2atmpS3755;
                    if (_M0L4_2axS1155 >= 128 && _M0L4_2axS1155 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3750 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3193 = _M0L8_2afieldS3750;
                      int32_t _M0L5startS3196 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3194 = _M0L5startS3196 + 3;
                      int32_t _M0L8_2afieldS3749 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3195 = _M0L8_2afieldS3749;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1156 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3194,
                                                           _M0L3endS3195,
                                                           _M0L5bytesS3193};
                      _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
                      _M0L2b0S1064 = _M0L4_2axS1140;
                      _M0L2b1S1065 = _M0L4_2axS1154;
                      _M0L2b2S1066 = _M0L4_2axS1155;
                      _M0L4restS1067 = _M0L4_2axS1156;
                      goto join_1062;
                    } else {
                      int32_t _M0L3endS3183 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L5startS3184 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3182 =
                        _M0L3endS3183 - _M0L5startS3184;
                      if (_M0L6_2atmpS3182 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3752 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3185 = _M0L8_2afieldS3752;
                        int32_t _M0L5startS3188 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3186 = _M0L5startS3188 + 2;
                        int32_t _M0L8_2afieldS3751 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3187 = _M0L8_2afieldS3751;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1157 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3186,
                                                             _M0L3endS3187,
                                                             _M0L5bytesS3185};
                        _M0L4restS1049 = _M0L4_2axS1157;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3754 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3189 = _M0L8_2afieldS3754;
                        int32_t _M0L5startS3192 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3190 = _M0L5startS3192 + 2;
                        int32_t _M0L8_2afieldS3753 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3191 = _M0L8_2afieldS3753;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1158 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3190,
                                                             _M0L3endS3191,
                                                             _M0L5bytesS3189};
                        _M0L4restS1049 = _M0L4_2axS1158;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3172 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L5startS3173 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3171 =
                      _M0L3endS3172 - _M0L5startS3173;
                    if (_M0L6_2atmpS3171 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3758 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3174 = _M0L8_2afieldS3758;
                      int32_t _M0L5startS3177 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3175 = _M0L5startS3177 + 1;
                      int32_t _M0L8_2afieldS3757 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3176 = _M0L8_2afieldS3757;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1159 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3175,
                                                           _M0L3endS3176,
                                                           _M0L5bytesS3174};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1159;
                      goto join_1038;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3760 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3178 = _M0L8_2afieldS3760;
                      int32_t _M0L5startS3181 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3179 = _M0L5startS3181 + 1;
                      int32_t _M0L8_2afieldS3759 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3180 = _M0L8_2afieldS3759;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1160 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3179,
                                                           _M0L3endS3180,
                                                           _M0L5bytesS3178};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1160;
                      goto join_1038;
                    }
                  }
                } else if (_M0L4_2axS1140 == 237) {
                  moonbit_bytes_t _M0L8_2afieldS3776 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3168 = _M0L8_2afieldS3776;
                  int32_t _M0L5startS3170 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3169 = _M0L5startS3170 + 1;
                  int32_t _M0L6_2atmpS3775 =
                    _M0L5bytesS3168[_M0L6_2atmpS3169];
                  int32_t _M0L4_2axS1161 = _M0L6_2atmpS3775;
                  if (_M0L4_2axS1161 >= 128 && _M0L4_2axS1161 <= 159) {
                    moonbit_bytes_t _M0L8_2afieldS3770 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3165 = _M0L8_2afieldS3770;
                    int32_t _M0L5startS3167 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3166 = _M0L5startS3167 + 2;
                    int32_t _M0L6_2atmpS3769 =
                      _M0L5bytesS3165[_M0L6_2atmpS3166];
                    int32_t _M0L4_2axS1162 = _M0L6_2atmpS3769;
                    if (_M0L4_2axS1162 >= 128 && _M0L4_2axS1162 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3764 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3161 = _M0L8_2afieldS3764;
                      int32_t _M0L5startS3164 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3162 = _M0L5startS3164 + 3;
                      int32_t _M0L8_2afieldS3763 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3163 = _M0L8_2afieldS3763;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1163 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3162,
                                                           _M0L3endS3163,
                                                           _M0L5bytesS3161};
                      _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
                      _M0L2b0S1064 = _M0L4_2axS1140;
                      _M0L2b1S1065 = _M0L4_2axS1161;
                      _M0L2b2S1066 = _M0L4_2axS1162;
                      _M0L4restS1067 = _M0L4_2axS1163;
                      goto join_1062;
                    } else {
                      int32_t _M0L3endS3151 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L5startS3152 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3150 =
                        _M0L3endS3151 - _M0L5startS3152;
                      if (_M0L6_2atmpS3150 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3766 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3153 = _M0L8_2afieldS3766;
                        int32_t _M0L5startS3156 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3154 = _M0L5startS3156 + 2;
                        int32_t _M0L8_2afieldS3765 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3155 = _M0L8_2afieldS3765;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1164 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3154,
                                                             _M0L3endS3155,
                                                             _M0L5bytesS3153};
                        _M0L4restS1049 = _M0L4_2axS1164;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3768 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3157 = _M0L8_2afieldS3768;
                        int32_t _M0L5startS3160 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3158 = _M0L5startS3160 + 2;
                        int32_t _M0L8_2afieldS3767 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3159 = _M0L8_2afieldS3767;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1165 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3158,
                                                             _M0L3endS3159,
                                                             _M0L5bytesS3157};
                        _M0L4restS1049 = _M0L4_2axS1165;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3140 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L5startS3141 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3139 =
                      _M0L3endS3140 - _M0L5startS3141;
                    if (_M0L6_2atmpS3139 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3772 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3142 = _M0L8_2afieldS3772;
                      int32_t _M0L5startS3145 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3143 = _M0L5startS3145 + 1;
                      int32_t _M0L8_2afieldS3771 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3144 = _M0L8_2afieldS3771;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1166 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3143,
                                                           _M0L3endS3144,
                                                           _M0L5bytesS3142};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1166;
                      goto join_1038;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3774 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3146 = _M0L8_2afieldS3774;
                      int32_t _M0L5startS3149 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3147 = _M0L5startS3149 + 1;
                      int32_t _M0L8_2afieldS3773 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3148 = _M0L8_2afieldS3773;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1167 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3147,
                                                           _M0L3endS3148,
                                                           _M0L5bytesS3146};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1167;
                      goto join_1038;
                    }
                  }
                } else if (_M0L4_2axS1140 >= 238 && _M0L4_2axS1140 <= 239) {
                  moonbit_bytes_t _M0L8_2afieldS3790 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3136 = _M0L8_2afieldS3790;
                  int32_t _M0L5startS3138 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3137 = _M0L5startS3138 + 1;
                  int32_t _M0L6_2atmpS3789 =
                    _M0L5bytesS3136[_M0L6_2atmpS3137];
                  int32_t _M0L4_2axS1168 = _M0L6_2atmpS3789;
                  if (_M0L4_2axS1168 >= 128 && _M0L4_2axS1168 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3784 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3133 = _M0L8_2afieldS3784;
                    int32_t _M0L5startS3135 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3134 = _M0L5startS3135 + 2;
                    int32_t _M0L6_2atmpS3783 =
                      _M0L5bytesS3133[_M0L6_2atmpS3134];
                    int32_t _M0L4_2axS1169 = _M0L6_2atmpS3783;
                    if (_M0L4_2axS1169 >= 128 && _M0L4_2axS1169 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3778 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3129 = _M0L8_2afieldS3778;
                      int32_t _M0L5startS3132 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3130 = _M0L5startS3132 + 3;
                      int32_t _M0L8_2afieldS3777 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3131 = _M0L8_2afieldS3777;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1170 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3130,
                                                           _M0L3endS3131,
                                                           _M0L5bytesS3129};
                      _M0L4tlenS1063 = _M0L11_2aparam__0S1036;
                      _M0L2b0S1064 = _M0L4_2axS1140;
                      _M0L2b1S1065 = _M0L4_2axS1168;
                      _M0L2b2S1066 = _M0L4_2axS1169;
                      _M0L4restS1067 = _M0L4_2axS1170;
                      goto join_1062;
                    } else {
                      int32_t _M0L3endS3119 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L5startS3120 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3118 =
                        _M0L3endS3119 - _M0L5startS3120;
                      if (_M0L6_2atmpS3118 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3780 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3121 = _M0L8_2afieldS3780;
                        int32_t _M0L5startS3124 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3122 = _M0L5startS3124 + 2;
                        int32_t _M0L8_2afieldS3779 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3123 = _M0L8_2afieldS3779;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1171 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3122,
                                                             _M0L3endS3123,
                                                             _M0L5bytesS3121};
                        _M0L4restS1049 = _M0L4_2axS1171;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3782 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3125 = _M0L8_2afieldS3782;
                        int32_t _M0L5startS3128 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3126 = _M0L5startS3128 + 2;
                        int32_t _M0L8_2afieldS3781 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3127 = _M0L8_2afieldS3781;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1172 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3126,
                                                             _M0L3endS3127,
                                                             _M0L5bytesS3125};
                        _M0L4restS1049 = _M0L4_2axS1172;
                        _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                        goto join_1048;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3108 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L5startS3109 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3107 =
                      _M0L3endS3108 - _M0L5startS3109;
                    if (_M0L6_2atmpS3107 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3786 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3110 = _M0L8_2afieldS3786;
                      int32_t _M0L5startS3113 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3111 = _M0L5startS3113 + 1;
                      int32_t _M0L8_2afieldS3785 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3112 = _M0L8_2afieldS3785;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1173 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3111,
                                                           _M0L3endS3112,
                                                           _M0L5bytesS3110};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1173;
                      goto join_1038;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3788 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3114 = _M0L8_2afieldS3788;
                      int32_t _M0L5startS3117 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3115 = _M0L5startS3117 + 1;
                      int32_t _M0L8_2afieldS3787 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3116 = _M0L8_2afieldS3787;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1174 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3115,
                                                           _M0L3endS3116,
                                                           _M0L5bytesS3114};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1174;
                      goto join_1038;
                    }
                  }
                } else {
                  int32_t _M0L3endS2968 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L5startS2969 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS2967 = _M0L3endS2968 - _M0L5startS2969;
                  if (_M0L6_2atmpS2967 >= 4) {
                    if (_M0L4_2axS1140 == 240) {
                      moonbit_bytes_t _M0L8_2afieldS3804 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3046 = _M0L8_2afieldS3804;
                      int32_t _M0L5startS3048 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3047 = _M0L5startS3048 + 1;
                      int32_t _M0L6_2atmpS3803 =
                        _M0L5bytesS3046[_M0L6_2atmpS3047];
                      int32_t _M0L4_2axS1175 = _M0L6_2atmpS3803;
                      if (_M0L4_2axS1175 >= 144 && _M0L4_2axS1175 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3800 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3043 = _M0L8_2afieldS3800;
                        int32_t _M0L5startS3045 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3044 = _M0L5startS3045 + 2;
                        int32_t _M0L6_2atmpS3799 =
                          _M0L5bytesS3043[_M0L6_2atmpS3044];
                        int32_t _M0L4_2axS1176 = _M0L6_2atmpS3799;
                        if (_M0L4_2axS1176 >= 128 && _M0L4_2axS1176 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS3796 =
                            _M0L11_2aparam__1S1037.$0;
                          moonbit_bytes_t _M0L5bytesS3040 =
                            _M0L8_2afieldS3796;
                          int32_t _M0L5startS3042 = _M0L11_2aparam__1S1037.$1;
                          int32_t _M0L6_2atmpS3041 = _M0L5startS3042 + 3;
                          int32_t _M0L6_2atmpS3795 =
                            _M0L5bytesS3040[_M0L6_2atmpS3041];
                          int32_t _M0L4_2axS1177 = _M0L6_2atmpS3795;
                          if (_M0L4_2axS1177 >= 128 && _M0L4_2axS1177 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS3792 =
                              _M0L11_2aparam__1S1037.$0;
                            moonbit_bytes_t _M0L5bytesS3036 =
                              _M0L8_2afieldS3792;
                            int32_t _M0L5startS3039 =
                              _M0L11_2aparam__1S1037.$1;
                            int32_t _M0L6_2atmpS3037 = _M0L5startS3039 + 4;
                            int32_t _M0L8_2afieldS3791 =
                              _M0L11_2aparam__1S1037.$2;
                            int32_t _M0L3endS3038 = _M0L8_2afieldS3791;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1178 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3037,
                                                                 _M0L3endS3038,
                                                                 _M0L5bytesS3036};
                            _M0L4tlenS1052 = _M0L11_2aparam__0S1036;
                            _M0L2b0S1053 = _M0L4_2axS1140;
                            _M0L2b1S1054 = _M0L4_2axS1175;
                            _M0L2b2S1055 = _M0L4_2axS1176;
                            _M0L2b3S1056 = _M0L4_2axS1177;
                            _M0L4restS1057 = _M0L4_2axS1178;
                            goto join_1051;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS3794 =
                              _M0L11_2aparam__1S1037.$0;
                            moonbit_bytes_t _M0L5bytesS3032 =
                              _M0L8_2afieldS3794;
                            int32_t _M0L5startS3035 =
                              _M0L11_2aparam__1S1037.$1;
                            int32_t _M0L6_2atmpS3033 = _M0L5startS3035 + 3;
                            int32_t _M0L8_2afieldS3793 =
                              _M0L11_2aparam__1S1037.$2;
                            int32_t _M0L3endS3034 = _M0L8_2afieldS3793;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1179 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3033,
                                                                 _M0L3endS3034,
                                                                 _M0L5bytesS3032};
                            _M0L4restS1046 = _M0L4_2axS1179;
                            _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                            goto join_1045;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS3798 =
                            _M0L11_2aparam__1S1037.$0;
                          moonbit_bytes_t _M0L5bytesS3028 =
                            _M0L8_2afieldS3798;
                          int32_t _M0L5startS3031 = _M0L11_2aparam__1S1037.$1;
                          int32_t _M0L6_2atmpS3029 = _M0L5startS3031 + 2;
                          int32_t _M0L8_2afieldS3797 =
                            _M0L11_2aparam__1S1037.$2;
                          int32_t _M0L3endS3030 = _M0L8_2afieldS3797;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS1180 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3029,
                                                               _M0L3endS3030,
                                                               _M0L5bytesS3028};
                          _M0L4restS1043 = _M0L4_2axS1180;
                          _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                          goto join_1042;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3802 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3024 = _M0L8_2afieldS3802;
                        int32_t _M0L5startS3027 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3025 = _M0L5startS3027 + 1;
                        int32_t _M0L8_2afieldS3801 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3026 = _M0L8_2afieldS3801;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1181 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3025,
                                                             _M0L3endS3026,
                                                             _M0L5bytesS3024};
                        _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                        _M0L4restS1040 = _M0L4_2axS1181;
                        goto join_1038;
                      }
                    } else if (
                             _M0L4_2axS1140 >= 241 && _M0L4_2axS1140 <= 243
                           ) {
                      moonbit_bytes_t _M0L8_2afieldS3818 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3021 = _M0L8_2afieldS3818;
                      int32_t _M0L5startS3023 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3022 = _M0L5startS3023 + 1;
                      int32_t _M0L6_2atmpS3817 =
                        _M0L5bytesS3021[_M0L6_2atmpS3022];
                      int32_t _M0L4_2axS1182 = _M0L6_2atmpS3817;
                      if (_M0L4_2axS1182 >= 128 && _M0L4_2axS1182 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3814 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3018 = _M0L8_2afieldS3814;
                        int32_t _M0L5startS3020 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3019 = _M0L5startS3020 + 2;
                        int32_t _M0L6_2atmpS3813 =
                          _M0L5bytesS3018[_M0L6_2atmpS3019];
                        int32_t _M0L4_2axS1183 = _M0L6_2atmpS3813;
                        if (_M0L4_2axS1183 >= 128 && _M0L4_2axS1183 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS3810 =
                            _M0L11_2aparam__1S1037.$0;
                          moonbit_bytes_t _M0L5bytesS3015 =
                            _M0L8_2afieldS3810;
                          int32_t _M0L5startS3017 = _M0L11_2aparam__1S1037.$1;
                          int32_t _M0L6_2atmpS3016 = _M0L5startS3017 + 3;
                          int32_t _M0L6_2atmpS3809 =
                            _M0L5bytesS3015[_M0L6_2atmpS3016];
                          int32_t _M0L4_2axS1184 = _M0L6_2atmpS3809;
                          if (_M0L4_2axS1184 >= 128 && _M0L4_2axS1184 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS3806 =
                              _M0L11_2aparam__1S1037.$0;
                            moonbit_bytes_t _M0L5bytesS3011 =
                              _M0L8_2afieldS3806;
                            int32_t _M0L5startS3014 =
                              _M0L11_2aparam__1S1037.$1;
                            int32_t _M0L6_2atmpS3012 = _M0L5startS3014 + 4;
                            int32_t _M0L8_2afieldS3805 =
                              _M0L11_2aparam__1S1037.$2;
                            int32_t _M0L3endS3013 = _M0L8_2afieldS3805;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1185 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3012,
                                                                 _M0L3endS3013,
                                                                 _M0L5bytesS3011};
                            _M0L4tlenS1052 = _M0L11_2aparam__0S1036;
                            _M0L2b0S1053 = _M0L4_2axS1140;
                            _M0L2b1S1054 = _M0L4_2axS1182;
                            _M0L2b2S1055 = _M0L4_2axS1183;
                            _M0L2b3S1056 = _M0L4_2axS1184;
                            _M0L4restS1057 = _M0L4_2axS1185;
                            goto join_1051;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS3808 =
                              _M0L11_2aparam__1S1037.$0;
                            moonbit_bytes_t _M0L5bytesS3007 =
                              _M0L8_2afieldS3808;
                            int32_t _M0L5startS3010 =
                              _M0L11_2aparam__1S1037.$1;
                            int32_t _M0L6_2atmpS3008 = _M0L5startS3010 + 3;
                            int32_t _M0L8_2afieldS3807 =
                              _M0L11_2aparam__1S1037.$2;
                            int32_t _M0L3endS3009 = _M0L8_2afieldS3807;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1186 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3008,
                                                                 _M0L3endS3009,
                                                                 _M0L5bytesS3007};
                            _M0L4restS1046 = _M0L4_2axS1186;
                            _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                            goto join_1045;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS3812 =
                            _M0L11_2aparam__1S1037.$0;
                          moonbit_bytes_t _M0L5bytesS3003 =
                            _M0L8_2afieldS3812;
                          int32_t _M0L5startS3006 = _M0L11_2aparam__1S1037.$1;
                          int32_t _M0L6_2atmpS3004 = _M0L5startS3006 + 2;
                          int32_t _M0L8_2afieldS3811 =
                            _M0L11_2aparam__1S1037.$2;
                          int32_t _M0L3endS3005 = _M0L8_2afieldS3811;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS1187 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3004,
                                                               _M0L3endS3005,
                                                               _M0L5bytesS3003};
                          _M0L4restS1043 = _M0L4_2axS1187;
                          _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                          goto join_1042;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3816 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS2999 = _M0L8_2afieldS3816;
                        int32_t _M0L5startS3002 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3000 = _M0L5startS3002 + 1;
                        int32_t _M0L8_2afieldS3815 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3001 = _M0L8_2afieldS3815;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1188 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3000,
                                                             _M0L3endS3001,
                                                             _M0L5bytesS2999};
                        _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                        _M0L4restS1040 = _M0L4_2axS1188;
                        goto join_1038;
                      }
                    } else if (_M0L4_2axS1140 == 244) {
                      moonbit_bytes_t _M0L8_2afieldS3832 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS2996 = _M0L8_2afieldS3832;
                      int32_t _M0L5startS2998 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS2997 = _M0L5startS2998 + 1;
                      int32_t _M0L6_2atmpS3831 =
                        _M0L5bytesS2996[_M0L6_2atmpS2997];
                      int32_t _M0L4_2axS1189 = _M0L6_2atmpS3831;
                      if (_M0L4_2axS1189 >= 128 && _M0L4_2axS1189 <= 143) {
                        moonbit_bytes_t _M0L8_2afieldS3828 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS2993 = _M0L8_2afieldS3828;
                        int32_t _M0L5startS2995 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS2994 = _M0L5startS2995 + 2;
                        int32_t _M0L6_2atmpS3827 =
                          _M0L5bytesS2993[_M0L6_2atmpS2994];
                        int32_t _M0L4_2axS1190 = _M0L6_2atmpS3827;
                        if (_M0L4_2axS1190 >= 128 && _M0L4_2axS1190 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS3824 =
                            _M0L11_2aparam__1S1037.$0;
                          moonbit_bytes_t _M0L5bytesS2990 =
                            _M0L8_2afieldS3824;
                          int32_t _M0L5startS2992 = _M0L11_2aparam__1S1037.$1;
                          int32_t _M0L6_2atmpS2991 = _M0L5startS2992 + 3;
                          int32_t _M0L6_2atmpS3823 =
                            _M0L5bytesS2990[_M0L6_2atmpS2991];
                          int32_t _M0L4_2axS1191 = _M0L6_2atmpS3823;
                          if (_M0L4_2axS1191 >= 128 && _M0L4_2axS1191 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS3820 =
                              _M0L11_2aparam__1S1037.$0;
                            moonbit_bytes_t _M0L5bytesS2986 =
                              _M0L8_2afieldS3820;
                            int32_t _M0L5startS2989 =
                              _M0L11_2aparam__1S1037.$1;
                            int32_t _M0L6_2atmpS2987 = _M0L5startS2989 + 4;
                            int32_t _M0L8_2afieldS3819 =
                              _M0L11_2aparam__1S1037.$2;
                            int32_t _M0L3endS2988 = _M0L8_2afieldS3819;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1192 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2987,
                                                                 _M0L3endS2988,
                                                                 _M0L5bytesS2986};
                            _M0L4tlenS1052 = _M0L11_2aparam__0S1036;
                            _M0L2b0S1053 = _M0L4_2axS1140;
                            _M0L2b1S1054 = _M0L4_2axS1189;
                            _M0L2b2S1055 = _M0L4_2axS1190;
                            _M0L2b3S1056 = _M0L4_2axS1191;
                            _M0L4restS1057 = _M0L4_2axS1192;
                            goto join_1051;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS3822 =
                              _M0L11_2aparam__1S1037.$0;
                            moonbit_bytes_t _M0L5bytesS2982 =
                              _M0L8_2afieldS3822;
                            int32_t _M0L5startS2985 =
                              _M0L11_2aparam__1S1037.$1;
                            int32_t _M0L6_2atmpS2983 = _M0L5startS2985 + 3;
                            int32_t _M0L8_2afieldS3821 =
                              _M0L11_2aparam__1S1037.$2;
                            int32_t _M0L3endS2984 = _M0L8_2afieldS3821;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1193 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2983,
                                                                 _M0L3endS2984,
                                                                 _M0L5bytesS2982};
                            _M0L4restS1046 = _M0L4_2axS1193;
                            _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                            goto join_1045;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS3826 =
                            _M0L11_2aparam__1S1037.$0;
                          moonbit_bytes_t _M0L5bytesS2978 =
                            _M0L8_2afieldS3826;
                          int32_t _M0L5startS2981 = _M0L11_2aparam__1S1037.$1;
                          int32_t _M0L6_2atmpS2979 = _M0L5startS2981 + 2;
                          int32_t _M0L8_2afieldS3825 =
                            _M0L11_2aparam__1S1037.$2;
                          int32_t _M0L3endS2980 = _M0L8_2afieldS3825;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS1194 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2979,
                                                               _M0L3endS2980,
                                                               _M0L5bytesS2978};
                          _M0L4restS1043 = _M0L4_2axS1194;
                          _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                          goto join_1042;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3830 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS2974 = _M0L8_2afieldS3830;
                        int32_t _M0L5startS2977 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS2975 = _M0L5startS2977 + 1;
                        int32_t _M0L8_2afieldS3829 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS2976 = _M0L8_2afieldS3829;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1195 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2975,
                                                             _M0L3endS2976,
                                                             _M0L5bytesS2974};
                        _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                        _M0L4restS1040 = _M0L4_2axS1195;
                        goto join_1038;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3834 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS2970 = _M0L8_2afieldS3834;
                      int32_t _M0L5startS2973 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS2971 = _M0L5startS2973 + 1;
                      int32_t _M0L8_2afieldS3833 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS2972 = _M0L8_2afieldS3833;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1196 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2971,
                                                           _M0L3endS2972,
                                                           _M0L5bytesS2970};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1196;
                      goto join_1038;
                    }
                  } else if (_M0L4_2axS1140 == 240) {
                    moonbit_bytes_t _M0L8_2afieldS3844 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3104 = _M0L8_2afieldS3844;
                    int32_t _M0L5startS3106 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3105 = _M0L5startS3106 + 1;
                    int32_t _M0L6_2atmpS3843 =
                      _M0L5bytesS3104[_M0L6_2atmpS3105];
                    int32_t _M0L4_2axS1197 = _M0L6_2atmpS3843;
                    if (_M0L4_2axS1197 >= 144 && _M0L4_2axS1197 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3840 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3101 = _M0L8_2afieldS3840;
                      int32_t _M0L5startS3103 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3102 = _M0L5startS3103 + 2;
                      int32_t _M0L6_2atmpS3839 =
                        _M0L5bytesS3101[_M0L6_2atmpS3102];
                      int32_t _M0L4_2axS1198 = _M0L6_2atmpS3839;
                      if (_M0L4_2axS1198 >= 128 && _M0L4_2axS1198 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3836 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3097 = _M0L8_2afieldS3836;
                        int32_t _M0L5startS3100 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3098 = _M0L5startS3100 + 3;
                        int32_t _M0L8_2afieldS3835 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3099 = _M0L8_2afieldS3835;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1199 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3098,
                                                             _M0L3endS3099,
                                                             _M0L5bytesS3097};
                        _M0L4restS1046 = _M0L4_2axS1199;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3838 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3093 = _M0L8_2afieldS3838;
                        int32_t _M0L5startS3096 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3094 = _M0L5startS3096 + 2;
                        int32_t _M0L8_2afieldS3837 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3095 = _M0L8_2afieldS3837;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1200 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3094,
                                                             _M0L3endS3095,
                                                             _M0L5bytesS3093};
                        _M0L4restS1043 = _M0L4_2axS1200;
                        _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                        goto join_1042;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3842 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3089 = _M0L8_2afieldS3842;
                      int32_t _M0L5startS3092 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3090 = _M0L5startS3092 + 1;
                      int32_t _M0L8_2afieldS3841 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3091 = _M0L8_2afieldS3841;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1201 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3090,
                                                           _M0L3endS3091,
                                                           _M0L5bytesS3089};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1201;
                      goto join_1038;
                    }
                  } else if (_M0L4_2axS1140 >= 241 && _M0L4_2axS1140 <= 243) {
                    moonbit_bytes_t _M0L8_2afieldS3854 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3086 = _M0L8_2afieldS3854;
                    int32_t _M0L5startS3088 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3087 = _M0L5startS3088 + 1;
                    int32_t _M0L6_2atmpS3853 =
                      _M0L5bytesS3086[_M0L6_2atmpS3087];
                    int32_t _M0L4_2axS1202 = _M0L6_2atmpS3853;
                    if (_M0L4_2axS1202 >= 128 && _M0L4_2axS1202 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3850 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3083 = _M0L8_2afieldS3850;
                      int32_t _M0L5startS3085 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3084 = _M0L5startS3085 + 2;
                      int32_t _M0L6_2atmpS3849 =
                        _M0L5bytesS3083[_M0L6_2atmpS3084];
                      int32_t _M0L4_2axS1203 = _M0L6_2atmpS3849;
                      if (_M0L4_2axS1203 >= 128 && _M0L4_2axS1203 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3846 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3079 = _M0L8_2afieldS3846;
                        int32_t _M0L5startS3082 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3080 = _M0L5startS3082 + 3;
                        int32_t _M0L8_2afieldS3845 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3081 = _M0L8_2afieldS3845;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1204 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3080,
                                                             _M0L3endS3081,
                                                             _M0L5bytesS3079};
                        _M0L4restS1046 = _M0L4_2axS1204;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3848 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3075 = _M0L8_2afieldS3848;
                        int32_t _M0L5startS3078 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3076 = _M0L5startS3078 + 2;
                        int32_t _M0L8_2afieldS3847 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3077 = _M0L8_2afieldS3847;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1205 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3076,
                                                             _M0L3endS3077,
                                                             _M0L5bytesS3075};
                        _M0L4restS1043 = _M0L4_2axS1205;
                        _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                        goto join_1042;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3852 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3071 = _M0L8_2afieldS3852;
                      int32_t _M0L5startS3074 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3072 = _M0L5startS3074 + 1;
                      int32_t _M0L8_2afieldS3851 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3073 = _M0L8_2afieldS3851;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1206 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3072,
                                                           _M0L3endS3073,
                                                           _M0L5bytesS3071};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1206;
                      goto join_1038;
                    }
                  } else if (_M0L4_2axS1140 == 244) {
                    moonbit_bytes_t _M0L8_2afieldS3864 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3068 = _M0L8_2afieldS3864;
                    int32_t _M0L5startS3070 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3069 = _M0L5startS3070 + 1;
                    int32_t _M0L6_2atmpS3863 =
                      _M0L5bytesS3068[_M0L6_2atmpS3069];
                    int32_t _M0L4_2axS1207 = _M0L6_2atmpS3863;
                    if (_M0L4_2axS1207 >= 128 && _M0L4_2axS1207 <= 143) {
                      moonbit_bytes_t _M0L8_2afieldS3860 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3065 = _M0L8_2afieldS3860;
                      int32_t _M0L5startS3067 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3066 = _M0L5startS3067 + 2;
                      int32_t _M0L6_2atmpS3859 =
                        _M0L5bytesS3065[_M0L6_2atmpS3066];
                      int32_t _M0L4_2axS1208 = _M0L6_2atmpS3859;
                      if (_M0L4_2axS1208 >= 128 && _M0L4_2axS1208 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3856 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3061 = _M0L8_2afieldS3856;
                        int32_t _M0L5startS3064 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3062 = _M0L5startS3064 + 3;
                        int32_t _M0L8_2afieldS3855 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3063 = _M0L8_2afieldS3855;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1209 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3062,
                                                             _M0L3endS3063,
                                                             _M0L5bytesS3061};
                        _M0L4restS1046 = _M0L4_2axS1209;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1036;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3858 =
                          _M0L11_2aparam__1S1037.$0;
                        moonbit_bytes_t _M0L5bytesS3057 = _M0L8_2afieldS3858;
                        int32_t _M0L5startS3060 = _M0L11_2aparam__1S1037.$1;
                        int32_t _M0L6_2atmpS3058 = _M0L5startS3060 + 2;
                        int32_t _M0L8_2afieldS3857 =
                          _M0L11_2aparam__1S1037.$2;
                        int32_t _M0L3endS3059 = _M0L8_2afieldS3857;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1210 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3058,
                                                             _M0L3endS3059,
                                                             _M0L5bytesS3057};
                        _M0L4restS1043 = _M0L4_2axS1210;
                        _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                        goto join_1042;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3862 =
                        _M0L11_2aparam__1S1037.$0;
                      moonbit_bytes_t _M0L5bytesS3053 = _M0L8_2afieldS3862;
                      int32_t _M0L5startS3056 = _M0L11_2aparam__1S1037.$1;
                      int32_t _M0L6_2atmpS3054 = _M0L5startS3056 + 1;
                      int32_t _M0L8_2afieldS3861 = _M0L11_2aparam__1S1037.$2;
                      int32_t _M0L3endS3055 = _M0L8_2afieldS3861;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1211 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3054,
                                                           _M0L3endS3055,
                                                           _M0L5bytesS3053};
                      _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                      _M0L4restS1040 = _M0L4_2axS1211;
                      goto join_1038;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3866 =
                      _M0L11_2aparam__1S1037.$0;
                    moonbit_bytes_t _M0L5bytesS3049 = _M0L8_2afieldS3866;
                    int32_t _M0L5startS3052 = _M0L11_2aparam__1S1037.$1;
                    int32_t _M0L6_2atmpS3050 = _M0L5startS3052 + 1;
                    int32_t _M0L8_2afieldS3865 = _M0L11_2aparam__1S1037.$2;
                    int32_t _M0L3endS3051 = _M0L8_2afieldS3865;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1212 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3050,
                                                         _M0L3endS3051,
                                                         _M0L5bytesS3049};
                    _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                    _M0L4restS1040 = _M0L4_2axS1212;
                    goto join_1038;
                  }
                }
              } else if (_M0L4_2axS1140 == 224) {
                moonbit_bytes_t _M0L8_2afieldS3872 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3313 = _M0L8_2afieldS3872;
                int32_t _M0L5startS3315 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3314 = _M0L5startS3315 + 1;
                int32_t _M0L6_2atmpS3871 = _M0L5bytesS3313[_M0L6_2atmpS3314];
                int32_t _M0L4_2axS1213 = _M0L6_2atmpS3871;
                if (_M0L4_2axS1213 >= 160 && _M0L4_2axS1213 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3868 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3309 = _M0L8_2afieldS3868;
                  int32_t _M0L5startS3312 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3310 = _M0L5startS3312 + 2;
                  int32_t _M0L8_2afieldS3867 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3311 = _M0L8_2afieldS3867;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1214 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3310,
                                                       _M0L3endS3311,
                                                       _M0L5bytesS3309};
                  _M0L4restS1049 = _M0L4_2axS1214;
                  _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                  goto join_1048;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3870 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3305 = _M0L8_2afieldS3870;
                  int32_t _M0L5startS3308 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3306 = _M0L5startS3308 + 1;
                  int32_t _M0L8_2afieldS3869 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3307 = _M0L8_2afieldS3869;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1215 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3306,
                                                       _M0L3endS3307,
                                                       _M0L5bytesS3305};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1215;
                  goto join_1038;
                }
              } else if (_M0L4_2axS1140 >= 225 && _M0L4_2axS1140 <= 236) {
                moonbit_bytes_t _M0L8_2afieldS3878 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3302 = _M0L8_2afieldS3878;
                int32_t _M0L5startS3304 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3303 = _M0L5startS3304 + 1;
                int32_t _M0L6_2atmpS3877 = _M0L5bytesS3302[_M0L6_2atmpS3303];
                int32_t _M0L4_2axS1216 = _M0L6_2atmpS3877;
                if (_M0L4_2axS1216 >= 128 && _M0L4_2axS1216 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3874 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3298 = _M0L8_2afieldS3874;
                  int32_t _M0L5startS3301 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3299 = _M0L5startS3301 + 2;
                  int32_t _M0L8_2afieldS3873 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3300 = _M0L8_2afieldS3873;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1217 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3299,
                                                       _M0L3endS3300,
                                                       _M0L5bytesS3298};
                  _M0L4restS1049 = _M0L4_2axS1217;
                  _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                  goto join_1048;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3876 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3294 = _M0L8_2afieldS3876;
                  int32_t _M0L5startS3297 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3295 = _M0L5startS3297 + 1;
                  int32_t _M0L8_2afieldS3875 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3296 = _M0L8_2afieldS3875;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1218 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3295,
                                                       _M0L3endS3296,
                                                       _M0L5bytesS3294};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1218;
                  goto join_1038;
                }
              } else if (_M0L4_2axS1140 == 237) {
                moonbit_bytes_t _M0L8_2afieldS3884 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3291 = _M0L8_2afieldS3884;
                int32_t _M0L5startS3293 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3292 = _M0L5startS3293 + 1;
                int32_t _M0L6_2atmpS3883 = _M0L5bytesS3291[_M0L6_2atmpS3292];
                int32_t _M0L4_2axS1219 = _M0L6_2atmpS3883;
                if (_M0L4_2axS1219 >= 128 && _M0L4_2axS1219 <= 159) {
                  moonbit_bytes_t _M0L8_2afieldS3880 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3287 = _M0L8_2afieldS3880;
                  int32_t _M0L5startS3290 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3288 = _M0L5startS3290 + 2;
                  int32_t _M0L8_2afieldS3879 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3289 = _M0L8_2afieldS3879;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1220 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3288,
                                                       _M0L3endS3289,
                                                       _M0L5bytesS3287};
                  _M0L4restS1049 = _M0L4_2axS1220;
                  _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                  goto join_1048;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3882 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3283 = _M0L8_2afieldS3882;
                  int32_t _M0L5startS3286 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3284 = _M0L5startS3286 + 1;
                  int32_t _M0L8_2afieldS3881 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3285 = _M0L8_2afieldS3881;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1221 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3284,
                                                       _M0L3endS3285,
                                                       _M0L5bytesS3283};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1221;
                  goto join_1038;
                }
              } else if (_M0L4_2axS1140 >= 238 && _M0L4_2axS1140 <= 239) {
                moonbit_bytes_t _M0L8_2afieldS3890 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3280 = _M0L8_2afieldS3890;
                int32_t _M0L5startS3282 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3281 = _M0L5startS3282 + 1;
                int32_t _M0L6_2atmpS3889 = _M0L5bytesS3280[_M0L6_2atmpS3281];
                int32_t _M0L4_2axS1222 = _M0L6_2atmpS3889;
                if (_M0L4_2axS1222 >= 128 && _M0L4_2axS1222 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3886 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3276 = _M0L8_2afieldS3886;
                  int32_t _M0L5startS3279 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3277 = _M0L5startS3279 + 2;
                  int32_t _M0L8_2afieldS3885 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3278 = _M0L8_2afieldS3885;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1223 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3277,
                                                       _M0L3endS3278,
                                                       _M0L5bytesS3276};
                  _M0L4restS1049 = _M0L4_2axS1223;
                  _M0L4tlenS1050 = _M0L11_2aparam__0S1036;
                  goto join_1048;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3888 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3272 = _M0L8_2afieldS3888;
                  int32_t _M0L5startS3275 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3273 = _M0L5startS3275 + 1;
                  int32_t _M0L8_2afieldS3887 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3274 = _M0L8_2afieldS3887;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1224 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3273,
                                                       _M0L3endS3274,
                                                       _M0L5bytesS3272};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1224;
                  goto join_1038;
                }
              } else if (_M0L4_2axS1140 == 240) {
                moonbit_bytes_t _M0L8_2afieldS3896 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3269 = _M0L8_2afieldS3896;
                int32_t _M0L5startS3271 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3270 = _M0L5startS3271 + 1;
                int32_t _M0L6_2atmpS3895 = _M0L5bytesS3269[_M0L6_2atmpS3270];
                int32_t _M0L4_2axS1225 = _M0L6_2atmpS3895;
                if (_M0L4_2axS1225 >= 144 && _M0L4_2axS1225 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3892 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3265 = _M0L8_2afieldS3892;
                  int32_t _M0L5startS3268 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3266 = _M0L5startS3268 + 2;
                  int32_t _M0L8_2afieldS3891 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3267 = _M0L8_2afieldS3891;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1226 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3266,
                                                       _M0L3endS3267,
                                                       _M0L5bytesS3265};
                  _M0L4restS1043 = _M0L4_2axS1226;
                  _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                  goto join_1042;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3894 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3261 = _M0L8_2afieldS3894;
                  int32_t _M0L5startS3264 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3262 = _M0L5startS3264 + 1;
                  int32_t _M0L8_2afieldS3893 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3263 = _M0L8_2afieldS3893;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1227 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3262,
                                                       _M0L3endS3263,
                                                       _M0L5bytesS3261};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1227;
                  goto join_1038;
                }
              } else if (_M0L4_2axS1140 >= 241 && _M0L4_2axS1140 <= 243) {
                moonbit_bytes_t _M0L8_2afieldS3902 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3258 = _M0L8_2afieldS3902;
                int32_t _M0L5startS3260 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3259 = _M0L5startS3260 + 1;
                int32_t _M0L6_2atmpS3901 = _M0L5bytesS3258[_M0L6_2atmpS3259];
                int32_t _M0L4_2axS1228 = _M0L6_2atmpS3901;
                if (_M0L4_2axS1228 >= 128 && _M0L4_2axS1228 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3898 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3254 = _M0L8_2afieldS3898;
                  int32_t _M0L5startS3257 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3255 = _M0L5startS3257 + 2;
                  int32_t _M0L8_2afieldS3897 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3256 = _M0L8_2afieldS3897;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1229 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3255,
                                                       _M0L3endS3256,
                                                       _M0L5bytesS3254};
                  _M0L4restS1043 = _M0L4_2axS1229;
                  _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                  goto join_1042;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3900 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3250 = _M0L8_2afieldS3900;
                  int32_t _M0L5startS3253 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3251 = _M0L5startS3253 + 1;
                  int32_t _M0L8_2afieldS3899 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3252 = _M0L8_2afieldS3899;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1230 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3251,
                                                       _M0L3endS3252,
                                                       _M0L5bytesS3250};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1230;
                  goto join_1038;
                }
              } else if (_M0L4_2axS1140 == 244) {
                moonbit_bytes_t _M0L8_2afieldS3908 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3247 = _M0L8_2afieldS3908;
                int32_t _M0L5startS3249 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3248 = _M0L5startS3249 + 1;
                int32_t _M0L6_2atmpS3907 = _M0L5bytesS3247[_M0L6_2atmpS3248];
                int32_t _M0L4_2axS1231 = _M0L6_2atmpS3907;
                if (_M0L4_2axS1231 >= 128 && _M0L4_2axS1231 <= 143) {
                  moonbit_bytes_t _M0L8_2afieldS3904 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3243 = _M0L8_2afieldS3904;
                  int32_t _M0L5startS3246 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3244 = _M0L5startS3246 + 2;
                  int32_t _M0L8_2afieldS3903 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3245 = _M0L8_2afieldS3903;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1232 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3244,
                                                       _M0L3endS3245,
                                                       _M0L5bytesS3243};
                  _M0L4restS1043 = _M0L4_2axS1232;
                  _M0L4tlenS1044 = _M0L11_2aparam__0S1036;
                  goto join_1042;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3906 =
                    _M0L11_2aparam__1S1037.$0;
                  moonbit_bytes_t _M0L5bytesS3239 = _M0L8_2afieldS3906;
                  int32_t _M0L5startS3242 = _M0L11_2aparam__1S1037.$1;
                  int32_t _M0L6_2atmpS3240 = _M0L5startS3242 + 1;
                  int32_t _M0L8_2afieldS3905 = _M0L11_2aparam__1S1037.$2;
                  int32_t _M0L3endS3241 = _M0L8_2afieldS3905;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1233 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3240,
                                                       _M0L3endS3241,
                                                       _M0L5bytesS3239};
                  _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                  _M0L4restS1040 = _M0L4_2axS1233;
                  goto join_1038;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS3910 =
                  _M0L11_2aparam__1S1037.$0;
                moonbit_bytes_t _M0L5bytesS3235 = _M0L8_2afieldS3910;
                int32_t _M0L5startS3238 = _M0L11_2aparam__1S1037.$1;
                int32_t _M0L6_2atmpS3236 = _M0L5startS3238 + 1;
                int32_t _M0L8_2afieldS3909 = _M0L11_2aparam__1S1037.$2;
                int32_t _M0L3endS3237 = _M0L8_2afieldS3909;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1234 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3236,
                                                     _M0L3endS3237,
                                                     _M0L5bytesS3235};
                _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
                _M0L4restS1040 = _M0L4_2axS1234;
                goto join_1038;
              }
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3912 = _M0L11_2aparam__1S1037.$0;
            moonbit_bytes_t _M0L5bytesS3341 = _M0L8_2afieldS3912;
            int32_t _M0L5startS3344 = _M0L11_2aparam__1S1037.$1;
            int32_t _M0L6_2atmpS3342 = _M0L5startS3344 + 1;
            int32_t _M0L8_2afieldS3911 = _M0L11_2aparam__1S1037.$2;
            int32_t _M0L3endS3343 = _M0L8_2afieldS3911;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1235 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3342,
                                                 _M0L3endS3343,
                                                 _M0L5bytesS3341};
            _M0L4tlenS1039 = _M0L11_2aparam__0S1036;
            _M0L4restS1040 = _M0L4_2axS1235;
            goto join_1038;
          }
        }
      }
    }
    goto joinlet_4271;
    join_1075:;
    _M0L1tS1034[_M0L4tlenS1076] = _M0L1bS1078;
    _M0L6_2atmpS2729 = _M0L4tlenS1076 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2729;
    _M0L11_2aparam__1S1037 = _M0L4restS1077;
    continue;
    joinlet_4271:;
    goto joinlet_4270;
    join_1069:;
    _M0L6_2atmpS2728 = (int32_t)_M0L2b0S1072;
    _M0L6_2atmpS2727 = _M0L6_2atmpS2728 & 31;
    _M0L6_2atmpS2724 = _M0L6_2atmpS2727 << 6;
    _M0L6_2atmpS2726 = (int32_t)_M0L2b1S1073;
    _M0L6_2atmpS2725 = _M0L6_2atmpS2726 & 63;
    _M0L2chS1074 = _M0L6_2atmpS2724 | _M0L6_2atmpS2725;
    _M0L6_2atmpS2719 = _M0L2chS1074 & 0xff;
    _M0L1tS1034[_M0L4tlenS1070] = _M0L6_2atmpS2719;
    _M0L6_2atmpS2720 = _M0L4tlenS1070 + 1;
    _M0L6_2atmpS2722 = _M0L2chS1074 >> 8;
    _M0L6_2atmpS2721 = _M0L6_2atmpS2722 & 0xff;
    _M0L1tS1034[_M0L6_2atmpS2720] = _M0L6_2atmpS2721;
    _M0L6_2atmpS2723 = _M0L4tlenS1070 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2723;
    _M0L11_2aparam__1S1037 = _M0L4restS1071;
    continue;
    joinlet_4270:;
    goto joinlet_4269;
    join_1062:;
    _M0L6_2atmpS2718 = (int32_t)_M0L2b0S1064;
    _M0L6_2atmpS2717 = _M0L6_2atmpS2718 & 15;
    _M0L6_2atmpS2713 = _M0L6_2atmpS2717 << 12;
    _M0L6_2atmpS2716 = (int32_t)_M0L2b1S1065;
    _M0L6_2atmpS2715 = _M0L6_2atmpS2716 & 63;
    _M0L6_2atmpS2714 = _M0L6_2atmpS2715 << 6;
    _M0L6_2atmpS2710 = _M0L6_2atmpS2713 | _M0L6_2atmpS2714;
    _M0L6_2atmpS2712 = (int32_t)_M0L2b2S1066;
    _M0L6_2atmpS2711 = _M0L6_2atmpS2712 & 63;
    _M0L2chS1068 = _M0L6_2atmpS2710 | _M0L6_2atmpS2711;
    _M0L6_2atmpS2705 = _M0L2chS1068 & 0xff;
    _M0L1tS1034[_M0L4tlenS1063] = _M0L6_2atmpS2705;
    _M0L6_2atmpS2706 = _M0L4tlenS1063 + 1;
    _M0L6_2atmpS2708 = _M0L2chS1068 >> 8;
    _M0L6_2atmpS2707 = _M0L6_2atmpS2708 & 0xff;
    _M0L1tS1034[_M0L6_2atmpS2706] = _M0L6_2atmpS2707;
    _M0L6_2atmpS2709 = _M0L4tlenS1063 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2709;
    _M0L11_2aparam__1S1037 = _M0L4restS1067;
    continue;
    joinlet_4269:;
    goto joinlet_4268;
    join_1051:;
    _M0L6_2atmpS2704 = (int32_t)_M0L2b0S1053;
    _M0L6_2atmpS2703 = _M0L6_2atmpS2704 & 7;
    _M0L6_2atmpS2699 = _M0L6_2atmpS2703 << 18;
    _M0L6_2atmpS2702 = (int32_t)_M0L2b1S1054;
    _M0L6_2atmpS2701 = _M0L6_2atmpS2702 & 63;
    _M0L6_2atmpS2700 = _M0L6_2atmpS2701 << 12;
    _M0L6_2atmpS2695 = _M0L6_2atmpS2699 | _M0L6_2atmpS2700;
    _M0L6_2atmpS2698 = (int32_t)_M0L2b2S1055;
    _M0L6_2atmpS2697 = _M0L6_2atmpS2698 & 63;
    _M0L6_2atmpS2696 = _M0L6_2atmpS2697 << 6;
    _M0L6_2atmpS2692 = _M0L6_2atmpS2695 | _M0L6_2atmpS2696;
    _M0L6_2atmpS2694 = (int32_t)_M0L2b3S1056;
    _M0L6_2atmpS2693 = _M0L6_2atmpS2694 & 63;
    _M0L2chS1058 = _M0L6_2atmpS2692 | _M0L6_2atmpS2693;
    _M0L3chmS1059 = _M0L2chS1058 - 65536;
    _M0L6_2atmpS2691 = _M0L3chmS1059 >> 10;
    _M0L3ch1S1060 = _M0L6_2atmpS2691 + 55296;
    _M0L6_2atmpS2690 = _M0L3chmS1059 & 1023;
    _M0L3ch2S1061 = _M0L6_2atmpS2690 + 56320;
    _M0L6_2atmpS2680 = _M0L3ch1S1060 & 0xff;
    _M0L1tS1034[_M0L4tlenS1052] = _M0L6_2atmpS2680;
    _M0L6_2atmpS2681 = _M0L4tlenS1052 + 1;
    _M0L6_2atmpS2683 = _M0L3ch1S1060 >> 8;
    _M0L6_2atmpS2682 = _M0L6_2atmpS2683 & 0xff;
    _M0L1tS1034[_M0L6_2atmpS2681] = _M0L6_2atmpS2682;
    _M0L6_2atmpS2684 = _M0L4tlenS1052 + 2;
    _M0L6_2atmpS2685 = _M0L3ch2S1061 & 0xff;
    _M0L1tS1034[_M0L6_2atmpS2684] = _M0L6_2atmpS2685;
    _M0L6_2atmpS2686 = _M0L4tlenS1052 + 3;
    _M0L6_2atmpS2688 = _M0L3ch2S1061 >> 8;
    _M0L6_2atmpS2687 = _M0L6_2atmpS2688 & 0xff;
    _M0L1tS1034[_M0L6_2atmpS2686] = _M0L6_2atmpS2687;
    _M0L6_2atmpS2689 = _M0L4tlenS1052 + 4;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2689;
    _M0L11_2aparam__1S1037 = _M0L4restS1057;
    continue;
    joinlet_4268:;
    goto joinlet_4267;
    join_1048:;
    _M0L1tS1034[_M0L4tlenS1050] = 253;
    _M0L6_2atmpS2678 = _M0L4tlenS1050 + 1;
    _M0L1tS1034[_M0L6_2atmpS2678] = 255;
    _M0L6_2atmpS2679 = _M0L4tlenS1050 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2679;
    _M0L11_2aparam__1S1037 = _M0L4restS1049;
    continue;
    joinlet_4267:;
    goto joinlet_4266;
    join_1045:;
    _M0L1tS1034[_M0L4tlenS1047] = 253;
    _M0L6_2atmpS2676 = _M0L4tlenS1047 + 1;
    _M0L1tS1034[_M0L6_2atmpS2676] = 255;
    _M0L6_2atmpS2677 = _M0L4tlenS1047 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2677;
    _M0L11_2aparam__1S1037 = _M0L4restS1046;
    continue;
    joinlet_4266:;
    goto joinlet_4265;
    join_1042:;
    _M0L1tS1034[_M0L4tlenS1044] = 253;
    _M0L6_2atmpS2674 = _M0L4tlenS1044 + 1;
    _M0L1tS1034[_M0L6_2atmpS2674] = 255;
    _M0L6_2atmpS2675 = _M0L4tlenS1044 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2675;
    _M0L11_2aparam__1S1037 = _M0L4restS1043;
    continue;
    joinlet_4265:;
    goto joinlet_4264;
    join_1038:;
    _M0L1tS1034[_M0L4tlenS1039] = 253;
    _M0L6_2atmpS2672 = _M0L4tlenS1039 + 1;
    _M0L1tS1034[_M0L6_2atmpS2672] = 255;
    _M0L6_2atmpS2673 = _M0L4tlenS1039 + 2;
    _M0L11_2aparam__0S1036 = _M0L6_2atmpS2673;
    _M0L11_2aparam__1S1037 = _M0L4restS1040;
    continue;
    joinlet_4264:;
    break;
  }
  _M0L6_2atmpS2670 = _M0L1tS1034;
  _M0L6_2atmpS2671 = (int64_t)_M0L4tlenS1035;
  #line 259 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2670, 0, _M0L6_2atmpS2671);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1021,
  void* _M0L7contentS1023,
  moonbit_string_t _M0L3locS1017,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1019
) {
  moonbit_string_t _M0L3locS1016;
  moonbit_string_t _M0L9args__locS1018;
  void* _M0L6_2atmpS2668;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2669;
  moonbit_string_t _M0L6actualS1020;
  moonbit_string_t _M0L4wantS1022;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1016 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1017);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1018 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1019);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2668 = _M0L3objS1021.$0->$method_0(_M0L3objS1021.$1);
  _M0L6_2atmpS2669 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1020
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2668, 0, 0, _M0L6_2atmpS2669);
  if (_M0L7contentS1023 == 0) {
    void* _M0L6_2atmpS2665;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2666;
    if (_M0L7contentS1023) {
      moonbit_decref(_M0L7contentS1023);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2665
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2666 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1022
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2665, 0, 0, _M0L6_2atmpS2666);
  } else {
    void* _M0L7_2aSomeS1024 = _M0L7contentS1023;
    void* _M0L4_2axS1025 = _M0L7_2aSomeS1024;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2667 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1022
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1025, 0, 0, _M0L6_2atmpS2667);
  }
  moonbit_incref(_M0L4wantS1022);
  moonbit_incref(_M0L6actualS1020);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1020, _M0L4wantS1022)
  ) {
    moonbit_string_t _M0L6_2atmpS2663;
    moonbit_string_t _M0L6_2atmpS3930;
    moonbit_string_t _M0L6_2atmpS2662;
    moonbit_string_t _M0L6_2atmpS3929;
    moonbit_string_t _M0L6_2atmpS2660;
    moonbit_string_t _M0L6_2atmpS2661;
    moonbit_string_t _M0L6_2atmpS3928;
    moonbit_string_t _M0L6_2atmpS2659;
    moonbit_string_t _M0L6_2atmpS3927;
    moonbit_string_t _M0L6_2atmpS2656;
    moonbit_string_t _M0L6_2atmpS2658;
    moonbit_string_t _M0L6_2atmpS2657;
    moonbit_string_t _M0L6_2atmpS3926;
    moonbit_string_t _M0L6_2atmpS2655;
    moonbit_string_t _M0L6_2atmpS3925;
    moonbit_string_t _M0L6_2atmpS2652;
    moonbit_string_t _M0L6_2atmpS2654;
    moonbit_string_t _M0L6_2atmpS2653;
    moonbit_string_t _M0L6_2atmpS3924;
    moonbit_string_t _M0L6_2atmpS2651;
    moonbit_string_t _M0L6_2atmpS3923;
    moonbit_string_t _M0L6_2atmpS2650;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2649;
    struct moonbit_result_0 _result_4272;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2663
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1016);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3930
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2663);
    moonbit_decref(_M0L6_2atmpS2663);
    _M0L6_2atmpS2662 = _M0L6_2atmpS3930;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3929
    = moonbit_add_string(_M0L6_2atmpS2662, (moonbit_string_t)moonbit_string_literal_14.data);
    moonbit_decref(_M0L6_2atmpS2662);
    _M0L6_2atmpS2660 = _M0L6_2atmpS3929;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2661
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1018);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3928 = moonbit_add_string(_M0L6_2atmpS2660, _M0L6_2atmpS2661);
    moonbit_decref(_M0L6_2atmpS2660);
    moonbit_decref(_M0L6_2atmpS2661);
    _M0L6_2atmpS2659 = _M0L6_2atmpS3928;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3927
    = moonbit_add_string(_M0L6_2atmpS2659, (moonbit_string_t)moonbit_string_literal_15.data);
    moonbit_decref(_M0L6_2atmpS2659);
    _M0L6_2atmpS2656 = _M0L6_2atmpS3927;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2658 = _M0MPC16string6String6escape(_M0L4wantS1022);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2657
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2658);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3926 = moonbit_add_string(_M0L6_2atmpS2656, _M0L6_2atmpS2657);
    moonbit_decref(_M0L6_2atmpS2656);
    moonbit_decref(_M0L6_2atmpS2657);
    _M0L6_2atmpS2655 = _M0L6_2atmpS3926;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3925
    = moonbit_add_string(_M0L6_2atmpS2655, (moonbit_string_t)moonbit_string_literal_16.data);
    moonbit_decref(_M0L6_2atmpS2655);
    _M0L6_2atmpS2652 = _M0L6_2atmpS3925;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2654 = _M0MPC16string6String6escape(_M0L6actualS1020);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2653
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2654);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3924 = moonbit_add_string(_M0L6_2atmpS2652, _M0L6_2atmpS2653);
    moonbit_decref(_M0L6_2atmpS2652);
    moonbit_decref(_M0L6_2atmpS2653);
    _M0L6_2atmpS2651 = _M0L6_2atmpS3924;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3923
    = moonbit_add_string(_M0L6_2atmpS2651, (moonbit_string_t)moonbit_string_literal_17.data);
    moonbit_decref(_M0L6_2atmpS2651);
    _M0L6_2atmpS2650 = _M0L6_2atmpS3923;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2649
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2649)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2649)->$0
    = _M0L6_2atmpS2650;
    _result_4272.tag = 0;
    _result_4272.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2649;
    return _result_4272;
  } else {
    int32_t _M0L6_2atmpS2664;
    struct moonbit_result_0 _result_4273;
    moonbit_decref(_M0L4wantS1022);
    moonbit_decref(_M0L6actualS1020);
    moonbit_decref(_M0L9args__locS1018);
    moonbit_decref(_M0L3locS1016);
    _M0L6_2atmpS2664 = 0;
    _result_4273.tag = 1;
    _result_4273.data.ok = _M0L6_2atmpS2664;
    return _result_4273;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1015,
  int32_t _M0L13escape__slashS987,
  int32_t _M0L6indentS982,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1008
) {
  struct _M0TPB13StringBuilder* _M0L3bufS974;
  void** _M0L6_2atmpS2648;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS975;
  int32_t _M0Lm5depthS976;
  void* _M0L6_2atmpS2647;
  void* _M0L8_2aparamS977;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS974 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2648 = (void**)moonbit_empty_ref_array;
  _M0L5stackS975
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS975)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS975->$0 = _M0L6_2atmpS2648;
  _M0L5stackS975->$1 = 0;
  _M0Lm5depthS976 = 0;
  _M0L6_2atmpS2647 = _M0L4selfS1015;
  _M0L8_2aparamS977 = _M0L6_2atmpS2647;
  _2aloop_993:;
  while (1) {
    if (_M0L8_2aparamS977 == 0) {
      int32_t _M0L3lenS2609;
      if (_M0L8_2aparamS977) {
        moonbit_decref(_M0L8_2aparamS977);
      }
      _M0L3lenS2609 = _M0L5stackS975->$1;
      if (_M0L3lenS2609 == 0) {
        if (_M0L8replacerS1008) {
          moonbit_decref(_M0L8replacerS1008);
        }
        moonbit_decref(_M0L5stackS975);
        break;
      } else {
        void** _M0L8_2afieldS3938 = _M0L5stackS975->$0;
        void** _M0L3bufS2633 = _M0L8_2afieldS3938;
        int32_t _M0L3lenS2635 = _M0L5stackS975->$1;
        int32_t _M0L6_2atmpS2634 = _M0L3lenS2635 - 1;
        void* _M0L6_2atmpS3937 = (void*)_M0L3bufS2633[_M0L6_2atmpS2634];
        void* _M0L4_2axS994 = _M0L6_2atmpS3937;
        switch (Moonbit_object_tag(_M0L4_2axS994)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS995 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS994;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3933 =
              _M0L8_2aArrayS995->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS996 =
              _M0L8_2afieldS3933;
            int32_t _M0L4_2aiS997 = _M0L8_2aArrayS995->$1;
            int32_t _M0L3lenS2621 = _M0L6_2aarrS996->$1;
            if (_M0L4_2aiS997 < _M0L3lenS2621) {
              int32_t _if__result_4275;
              void** _M0L8_2afieldS3932;
              void** _M0L3bufS2627;
              void* _M0L6_2atmpS3931;
              void* _M0L7elementS998;
              int32_t _M0L6_2atmpS2622;
              void* _M0L6_2atmpS2625;
              if (_M0L4_2aiS997 < 0) {
                _if__result_4275 = 1;
              } else {
                int32_t _M0L3lenS2626 = _M0L6_2aarrS996->$1;
                _if__result_4275 = _M0L4_2aiS997 >= _M0L3lenS2626;
              }
              if (_if__result_4275) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3932 = _M0L6_2aarrS996->$0;
              _M0L3bufS2627 = _M0L8_2afieldS3932;
              _M0L6_2atmpS3931 = (void*)_M0L3bufS2627[_M0L4_2aiS997];
              _M0L7elementS998 = _M0L6_2atmpS3931;
              _M0L6_2atmpS2622 = _M0L4_2aiS997 + 1;
              _M0L8_2aArrayS995->$1 = _M0L6_2atmpS2622;
              if (_M0L4_2aiS997 > 0) {
                int32_t _M0L6_2atmpS2624;
                moonbit_string_t _M0L6_2atmpS2623;
                moonbit_incref(_M0L7elementS998);
                moonbit_incref(_M0L3bufS974);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 44);
                _M0L6_2atmpS2624 = _M0Lm5depthS976;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2623
                = _M0FPC14json11indent__str(_M0L6_2atmpS2624, _M0L6indentS982);
                moonbit_incref(_M0L3bufS974);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2623);
              } else {
                moonbit_incref(_M0L7elementS998);
              }
              _M0L6_2atmpS2625 = _M0L7elementS998;
              _M0L8_2aparamS977 = _M0L6_2atmpS2625;
              goto _2aloop_993;
            } else {
              int32_t _M0L6_2atmpS2628 = _M0Lm5depthS976;
              void* _M0L6_2atmpS2629;
              int32_t _M0L6_2atmpS2631;
              moonbit_string_t _M0L6_2atmpS2630;
              void* _M0L6_2atmpS2632;
              _M0Lm5depthS976 = _M0L6_2atmpS2628 - 1;
              moonbit_incref(_M0L5stackS975);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2629
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS975);
              if (_M0L6_2atmpS2629) {
                moonbit_decref(_M0L6_2atmpS2629);
              }
              _M0L6_2atmpS2631 = _M0Lm5depthS976;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2630
              = _M0FPC14json11indent__str(_M0L6_2atmpS2631, _M0L6indentS982);
              moonbit_incref(_M0L3bufS974);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2630);
              moonbit_incref(_M0L3bufS974);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 93);
              _M0L6_2atmpS2632 = 0;
              _M0L8_2aparamS977 = _M0L6_2atmpS2632;
              goto _2aloop_993;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS999 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS994;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3936 =
              _M0L9_2aObjectS999->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1000 =
              _M0L8_2afieldS3936;
            int32_t _M0L8_2afirstS1001 = _M0L9_2aObjectS999->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1002;
            moonbit_incref(_M0L11_2aiteratorS1000);
            moonbit_incref(_M0L9_2aObjectS999);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1002
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1000);
            if (_M0L7_2abindS1002 == 0) {
              int32_t _M0L6_2atmpS2610;
              void* _M0L6_2atmpS2611;
              int32_t _M0L6_2atmpS2613;
              moonbit_string_t _M0L6_2atmpS2612;
              void* _M0L6_2atmpS2614;
              if (_M0L7_2abindS1002) {
                moonbit_decref(_M0L7_2abindS1002);
              }
              moonbit_decref(_M0L9_2aObjectS999);
              _M0L6_2atmpS2610 = _M0Lm5depthS976;
              _M0Lm5depthS976 = _M0L6_2atmpS2610 - 1;
              moonbit_incref(_M0L5stackS975);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2611
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS975);
              if (_M0L6_2atmpS2611) {
                moonbit_decref(_M0L6_2atmpS2611);
              }
              _M0L6_2atmpS2613 = _M0Lm5depthS976;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2612
              = _M0FPC14json11indent__str(_M0L6_2atmpS2613, _M0L6indentS982);
              moonbit_incref(_M0L3bufS974);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2612);
              moonbit_incref(_M0L3bufS974);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 125);
              _M0L6_2atmpS2614 = 0;
              _M0L8_2aparamS977 = _M0L6_2atmpS2614;
              goto _2aloop_993;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1003 = _M0L7_2abindS1002;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1004 = _M0L7_2aSomeS1003;
              moonbit_string_t _M0L8_2afieldS3935 = _M0L4_2axS1004->$0;
              moonbit_string_t _M0L4_2akS1005 = _M0L8_2afieldS3935;
              void* _M0L8_2afieldS3934 = _M0L4_2axS1004->$1;
              int32_t _M0L6_2acntS4152 =
                Moonbit_object_header(_M0L4_2axS1004)->rc;
              void* _M0L4_2avS1006;
              void* _M0Lm2v2S1007;
              moonbit_string_t _M0L6_2atmpS2618;
              void* _M0L6_2atmpS2620;
              void* _M0L6_2atmpS2619;
              if (_M0L6_2acntS4152 > 1) {
                int32_t _M0L11_2anew__cntS4153 = _M0L6_2acntS4152 - 1;
                Moonbit_object_header(_M0L4_2axS1004)->rc
                = _M0L11_2anew__cntS4153;
                moonbit_incref(_M0L8_2afieldS3934);
                moonbit_incref(_M0L4_2akS1005);
              } else if (_M0L6_2acntS4152 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1004);
              }
              _M0L4_2avS1006 = _M0L8_2afieldS3934;
              _M0Lm2v2S1007 = _M0L4_2avS1006;
              if (_M0L8replacerS1008 == 0) {
                moonbit_incref(_M0Lm2v2S1007);
                moonbit_decref(_M0L4_2avS1006);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1009 =
                  _M0L8replacerS1008;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1010 =
                  _M0L7_2aSomeS1009;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1011 =
                  _M0L11_2areplacerS1010;
                void* _M0L7_2abindS1012;
                moonbit_incref(_M0L7_2afuncS1011);
                moonbit_incref(_M0L4_2akS1005);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1012
                = _M0L7_2afuncS1011->code(_M0L7_2afuncS1011, _M0L4_2akS1005, _M0L4_2avS1006);
                if (_M0L7_2abindS1012 == 0) {
                  void* _M0L6_2atmpS2615;
                  if (_M0L7_2abindS1012) {
                    moonbit_decref(_M0L7_2abindS1012);
                  }
                  moonbit_decref(_M0L4_2akS1005);
                  moonbit_decref(_M0L9_2aObjectS999);
                  _M0L6_2atmpS2615 = 0;
                  _M0L8_2aparamS977 = _M0L6_2atmpS2615;
                  goto _2aloop_993;
                } else {
                  void* _M0L7_2aSomeS1013 = _M0L7_2abindS1012;
                  void* _M0L4_2avS1014 = _M0L7_2aSomeS1013;
                  _M0Lm2v2S1007 = _M0L4_2avS1014;
                }
              }
              if (!_M0L8_2afirstS1001) {
                int32_t _M0L6_2atmpS2617;
                moonbit_string_t _M0L6_2atmpS2616;
                moonbit_incref(_M0L3bufS974);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 44);
                _M0L6_2atmpS2617 = _M0Lm5depthS976;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2616
                = _M0FPC14json11indent__str(_M0L6_2atmpS2617, _M0L6indentS982);
                moonbit_incref(_M0L3bufS974);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2616);
              }
              moonbit_incref(_M0L3bufS974);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2618
              = _M0FPC14json6escape(_M0L4_2akS1005, _M0L13escape__slashS987);
              moonbit_incref(_M0L3bufS974);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2618);
              moonbit_incref(_M0L3bufS974);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 34);
              moonbit_incref(_M0L3bufS974);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 58);
              if (_M0L6indentS982 > 0) {
                moonbit_incref(_M0L3bufS974);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 32);
              }
              _M0L9_2aObjectS999->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS999);
              _M0L6_2atmpS2620 = _M0Lm2v2S1007;
              _M0L6_2atmpS2619 = _M0L6_2atmpS2620;
              _M0L8_2aparamS977 = _M0L6_2atmpS2619;
              goto _2aloop_993;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS978 = _M0L8_2aparamS977;
      void* _M0L8_2avalueS979 = _M0L7_2aSomeS978;
      void* _M0L6_2atmpS2646;
      switch (Moonbit_object_tag(_M0L8_2avalueS979)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS980 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS979;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3939 =
            _M0L9_2aObjectS980->$0;
          int32_t _M0L6_2acntS4154 =
            Moonbit_object_header(_M0L9_2aObjectS980)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS981;
          if (_M0L6_2acntS4154 > 1) {
            int32_t _M0L11_2anew__cntS4155 = _M0L6_2acntS4154 - 1;
            Moonbit_object_header(_M0L9_2aObjectS980)->rc
            = _M0L11_2anew__cntS4155;
            moonbit_incref(_M0L8_2afieldS3939);
          } else if (_M0L6_2acntS4154 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS980);
          }
          _M0L10_2amembersS981 = _M0L8_2afieldS3939;
          moonbit_incref(_M0L10_2amembersS981);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS981)) {
            moonbit_decref(_M0L10_2amembersS981);
            moonbit_incref(_M0L3bufS974);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_18.data);
          } else {
            int32_t _M0L6_2atmpS2641 = _M0Lm5depthS976;
            int32_t _M0L6_2atmpS2643;
            moonbit_string_t _M0L6_2atmpS2642;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2645;
            void* _M0L6ObjectS2644;
            _M0Lm5depthS976 = _M0L6_2atmpS2641 + 1;
            moonbit_incref(_M0L3bufS974);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 123);
            _M0L6_2atmpS2643 = _M0Lm5depthS976;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2642
            = _M0FPC14json11indent__str(_M0L6_2atmpS2643, _M0L6indentS982);
            moonbit_incref(_M0L3bufS974);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2642);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2645
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS981);
            _M0L6ObjectS2644
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2644)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2644)->$0
            = _M0L6_2atmpS2645;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2644)->$1
            = 1;
            moonbit_incref(_M0L5stackS975);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS975, _M0L6ObjectS2644);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS983 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS979;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3940 =
            _M0L8_2aArrayS983->$0;
          int32_t _M0L6_2acntS4156 =
            Moonbit_object_header(_M0L8_2aArrayS983)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS984;
          if (_M0L6_2acntS4156 > 1) {
            int32_t _M0L11_2anew__cntS4157 = _M0L6_2acntS4156 - 1;
            Moonbit_object_header(_M0L8_2aArrayS983)->rc
            = _M0L11_2anew__cntS4157;
            moonbit_incref(_M0L8_2afieldS3940);
          } else if (_M0L6_2acntS4156 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS983);
          }
          _M0L6_2aarrS984 = _M0L8_2afieldS3940;
          moonbit_incref(_M0L6_2aarrS984);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS984)) {
            moonbit_decref(_M0L6_2aarrS984);
            moonbit_incref(_M0L3bufS974);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_19.data);
          } else {
            int32_t _M0L6_2atmpS2637 = _M0Lm5depthS976;
            int32_t _M0L6_2atmpS2639;
            moonbit_string_t _M0L6_2atmpS2638;
            void* _M0L5ArrayS2640;
            _M0Lm5depthS976 = _M0L6_2atmpS2637 + 1;
            moonbit_incref(_M0L3bufS974);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 91);
            _M0L6_2atmpS2639 = _M0Lm5depthS976;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2638
            = _M0FPC14json11indent__str(_M0L6_2atmpS2639, _M0L6indentS982);
            moonbit_incref(_M0L3bufS974);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2638);
            _M0L5ArrayS2640
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2640)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2640)->$0
            = _M0L6_2aarrS984;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2640)->$1
            = 0;
            moonbit_incref(_M0L5stackS975);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS975, _M0L5ArrayS2640);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS985 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS979;
          moonbit_string_t _M0L8_2afieldS3941 = _M0L9_2aStringS985->$0;
          int32_t _M0L6_2acntS4158 =
            Moonbit_object_header(_M0L9_2aStringS985)->rc;
          moonbit_string_t _M0L4_2asS986;
          moonbit_string_t _M0L6_2atmpS2636;
          if (_M0L6_2acntS4158 > 1) {
            int32_t _M0L11_2anew__cntS4159 = _M0L6_2acntS4158 - 1;
            Moonbit_object_header(_M0L9_2aStringS985)->rc
            = _M0L11_2anew__cntS4159;
            moonbit_incref(_M0L8_2afieldS3941);
          } else if (_M0L6_2acntS4158 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS985);
          }
          _M0L4_2asS986 = _M0L8_2afieldS3941;
          moonbit_incref(_M0L3bufS974);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2636
          = _M0FPC14json6escape(_M0L4_2asS986, _M0L13escape__slashS987);
          moonbit_incref(_M0L3bufS974);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2636);
          moonbit_incref(_M0L3bufS974);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS988 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS979;
          double _M0L4_2anS989 = _M0L9_2aNumberS988->$0;
          moonbit_string_t _M0L8_2afieldS3942 = _M0L9_2aNumberS988->$1;
          int32_t _M0L6_2acntS4160 =
            Moonbit_object_header(_M0L9_2aNumberS988)->rc;
          moonbit_string_t _M0L7_2areprS990;
          if (_M0L6_2acntS4160 > 1) {
            int32_t _M0L11_2anew__cntS4161 = _M0L6_2acntS4160 - 1;
            Moonbit_object_header(_M0L9_2aNumberS988)->rc
            = _M0L11_2anew__cntS4161;
            if (_M0L8_2afieldS3942) {
              moonbit_incref(_M0L8_2afieldS3942);
            }
          } else if (_M0L6_2acntS4160 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS988);
          }
          _M0L7_2areprS990 = _M0L8_2afieldS3942;
          if (_M0L7_2areprS990 == 0) {
            if (_M0L7_2areprS990) {
              moonbit_decref(_M0L7_2areprS990);
            }
            moonbit_incref(_M0L3bufS974);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS974, _M0L4_2anS989);
          } else {
            moonbit_string_t _M0L7_2aSomeS991 = _M0L7_2areprS990;
            moonbit_string_t _M0L4_2arS992 = _M0L7_2aSomeS991;
            moonbit_incref(_M0L3bufS974);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L4_2arS992);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS974);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_20.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS974);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_21.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS979);
          moonbit_incref(_M0L3bufS974);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_22.data);
          break;
        }
      }
      _M0L6_2atmpS2646 = 0;
      _M0L8_2aparamS977 = _M0L6_2atmpS2646;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS974);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS973,
  int32_t _M0L6indentS971
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS971 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS972 = _M0L6indentS971 * _M0L5levelS973;
    switch (_M0L6spacesS972) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_23.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_24.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_25.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_26.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_27.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_28.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_29.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_30.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_31.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2608;
        moonbit_string_t _M0L6_2atmpS3943;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2608
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_32.data, _M0L6spacesS972);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3943
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS2608);
        moonbit_decref(_M0L6_2atmpS2608);
        return _M0L6_2atmpS3943;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS963,
  int32_t _M0L13escape__slashS968
) {
  int32_t _M0L6_2atmpS2607;
  struct _M0TPB13StringBuilder* _M0L3bufS962;
  struct _M0TWEOc* _M0L5_2aitS964;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2607 = Moonbit_array_length(_M0L3strS963);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS962 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2607);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS964 = _M0MPC16string6String4iter(_M0L3strS963);
  while (1) {
    int32_t _M0L7_2abindS965;
    moonbit_incref(_M0L5_2aitS964);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS965 = _M0MPB4Iter4nextGcE(_M0L5_2aitS964);
    if (_M0L7_2abindS965 == -1) {
      moonbit_decref(_M0L5_2aitS964);
    } else {
      int32_t _M0L7_2aSomeS966 = _M0L7_2abindS965;
      int32_t _M0L4_2acS967 = _M0L7_2aSomeS966;
      if (_M0L4_2acS967 == 34) {
        moonbit_incref(_M0L3bufS962);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_33.data);
      } else if (_M0L4_2acS967 == 92) {
        moonbit_incref(_M0L3bufS962);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_34.data);
      } else if (_M0L4_2acS967 == 47) {
        if (_M0L13escape__slashS968) {
          moonbit_incref(_M0L3bufS962);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_35.data);
        } else {
          moonbit_incref(_M0L3bufS962);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS962, _M0L4_2acS967);
        }
      } else if (_M0L4_2acS967 == 10) {
        moonbit_incref(_M0L3bufS962);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_36.data);
      } else if (_M0L4_2acS967 == 13) {
        moonbit_incref(_M0L3bufS962);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_37.data);
      } else if (_M0L4_2acS967 == 8) {
        moonbit_incref(_M0L3bufS962);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_38.data);
      } else if (_M0L4_2acS967 == 9) {
        moonbit_incref(_M0L3bufS962);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_39.data);
      } else {
        int32_t _M0L4codeS969 = _M0L4_2acS967;
        if (_M0L4codeS969 == 12) {
          moonbit_incref(_M0L3bufS962);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_40.data);
        } else if (_M0L4codeS969 < 32) {
          int32_t _M0L6_2atmpS2606;
          moonbit_string_t _M0L6_2atmpS2605;
          moonbit_incref(_M0L3bufS962);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, (moonbit_string_t)moonbit_string_literal_41.data);
          _M0L6_2atmpS2606 = _M0L4codeS969 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2605 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2606);
          moonbit_incref(_M0L3bufS962);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS962, _M0L6_2atmpS2605);
        } else {
          moonbit_incref(_M0L3bufS962);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS962, _M0L4_2acS967);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS962);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS961
) {
  int32_t _M0L8_2afieldS3944;
  int32_t _M0L3lenS2604;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3944 = _M0L4selfS961->$1;
  moonbit_decref(_M0L4selfS961);
  _M0L3lenS2604 = _M0L8_2afieldS3944;
  return _M0L3lenS2604 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS958
) {
  int32_t _M0L3lenS957;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS957 = _M0L4selfS958->$1;
  if (_M0L3lenS957 == 0) {
    moonbit_decref(_M0L4selfS958);
    return 0;
  } else {
    int32_t _M0L5indexS959 = _M0L3lenS957 - 1;
    void** _M0L8_2afieldS3948 = _M0L4selfS958->$0;
    void** _M0L3bufS2603 = _M0L8_2afieldS3948;
    void* _M0L6_2atmpS3947 = (void*)_M0L3bufS2603[_M0L5indexS959];
    void* _M0L1vS960 = _M0L6_2atmpS3947;
    void** _M0L8_2afieldS3946 = _M0L4selfS958->$0;
    void** _M0L3bufS2602 = _M0L8_2afieldS3946;
    void* _M0L6_2aoldS3945;
    if (
      _M0L5indexS959 < 0
      || _M0L5indexS959 >= Moonbit_array_length(_M0L3bufS2602)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3945 = (void*)_M0L3bufS2602[_M0L5indexS959];
    moonbit_incref(_M0L1vS960);
    moonbit_decref(_M0L6_2aoldS3945);
    if (
      _M0L5indexS959 < 0
      || _M0L5indexS959 >= Moonbit_array_length(_M0L3bufS2602)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2602[_M0L5indexS959]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS958->$1 = _M0L5indexS959;
    moonbit_decref(_M0L4selfS958);
    return _M0L1vS960;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS955,
  struct _M0TPB6Logger _M0L6loggerS956
) {
  moonbit_string_t _M0L6_2atmpS2601;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2600;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2601 = _M0L4selfS955;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2600 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2601);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2600, _M0L6loggerS956);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS932,
  struct _M0TPB6Logger _M0L6loggerS954
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3957;
  struct _M0TPC16string10StringView _M0L3pkgS931;
  moonbit_string_t _M0L7_2adataS933;
  int32_t _M0L8_2astartS934;
  int32_t _M0L6_2atmpS2599;
  int32_t _M0L6_2aendS935;
  int32_t _M0Lm9_2acursorS936;
  int32_t _M0Lm13accept__stateS937;
  int32_t _M0Lm10match__endS938;
  int32_t _M0Lm20match__tag__saver__0S939;
  int32_t _M0Lm6tag__0S940;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS941;
  struct _M0TPC16string10StringView _M0L8_2afieldS3956;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS950;
  void* _M0L8_2afieldS3955;
  int32_t _M0L6_2acntS4162;
  void* _M0L16_2apackage__nameS951;
  struct _M0TPC16string10StringView _M0L8_2afieldS3953;
  struct _M0TPC16string10StringView _M0L8filenameS2576;
  struct _M0TPC16string10StringView _M0L8_2afieldS3952;
  struct _M0TPC16string10StringView _M0L11start__lineS2577;
  struct _M0TPC16string10StringView _M0L8_2afieldS3951;
  struct _M0TPC16string10StringView _M0L13start__columnS2578;
  struct _M0TPC16string10StringView _M0L8_2afieldS3950;
  struct _M0TPC16string10StringView _M0L9end__lineS2579;
  struct _M0TPC16string10StringView _M0L8_2afieldS3949;
  int32_t _M0L6_2acntS4166;
  struct _M0TPC16string10StringView _M0L11end__columnS2580;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3957
  = (struct _M0TPC16string10StringView){
    _M0L4selfS932->$0_1, _M0L4selfS932->$0_2, _M0L4selfS932->$0_0
  };
  _M0L3pkgS931 = _M0L8_2afieldS3957;
  moonbit_incref(_M0L3pkgS931.$0);
  moonbit_incref(_M0L3pkgS931.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS933 = _M0MPC16string10StringView4data(_M0L3pkgS931);
  moonbit_incref(_M0L3pkgS931.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS934 = _M0MPC16string10StringView13start__offset(_M0L3pkgS931);
  moonbit_incref(_M0L3pkgS931.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2599 = _M0MPC16string10StringView6length(_M0L3pkgS931);
  _M0L6_2aendS935 = _M0L8_2astartS934 + _M0L6_2atmpS2599;
  _M0Lm9_2acursorS936 = _M0L8_2astartS934;
  _M0Lm13accept__stateS937 = -1;
  _M0Lm10match__endS938 = -1;
  _M0Lm20match__tag__saver__0S939 = -1;
  _M0Lm6tag__0S940 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2591 = _M0Lm9_2acursorS936;
    if (_M0L6_2atmpS2591 < _M0L6_2aendS935) {
      int32_t _M0L6_2atmpS2598 = _M0Lm9_2acursorS936;
      int32_t _M0L10next__charS945;
      int32_t _M0L6_2atmpS2592;
      moonbit_incref(_M0L7_2adataS933);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS945
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS933, _M0L6_2atmpS2598);
      _M0L6_2atmpS2592 = _M0Lm9_2acursorS936;
      _M0Lm9_2acursorS936 = _M0L6_2atmpS2592 + 1;
      if (_M0L10next__charS945 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2593;
          _M0Lm6tag__0S940 = _M0Lm9_2acursorS936;
          _M0L6_2atmpS2593 = _M0Lm9_2acursorS936;
          if (_M0L6_2atmpS2593 < _M0L6_2aendS935) {
            int32_t _M0L6_2atmpS2597 = _M0Lm9_2acursorS936;
            int32_t _M0L10next__charS946;
            int32_t _M0L6_2atmpS2594;
            moonbit_incref(_M0L7_2adataS933);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS946
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS933, _M0L6_2atmpS2597);
            _M0L6_2atmpS2594 = _M0Lm9_2acursorS936;
            _M0Lm9_2acursorS936 = _M0L6_2atmpS2594 + 1;
            if (_M0L10next__charS946 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2595 = _M0Lm9_2acursorS936;
                if (_M0L6_2atmpS2595 < _M0L6_2aendS935) {
                  int32_t _M0L6_2atmpS2596 = _M0Lm9_2acursorS936;
                  _M0Lm9_2acursorS936 = _M0L6_2atmpS2596 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S939 = _M0Lm6tag__0S940;
                  _M0Lm13accept__stateS937 = 0;
                  _M0Lm10match__endS938 = _M0Lm9_2acursorS936;
                  goto join_942;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_942;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_942;
    }
    break;
  }
  goto joinlet_4277;
  join_942:;
  switch (_M0Lm13accept__stateS937) {
    case 0: {
      int32_t _M0L6_2atmpS2589;
      int32_t _M0L6_2atmpS2588;
      int64_t _M0L6_2atmpS2585;
      int32_t _M0L6_2atmpS2587;
      int64_t _M0L6_2atmpS2586;
      struct _M0TPC16string10StringView _M0L13package__nameS943;
      int64_t _M0L6_2atmpS2582;
      int32_t _M0L6_2atmpS2584;
      int64_t _M0L6_2atmpS2583;
      struct _M0TPC16string10StringView _M0L12module__nameS944;
      void* _M0L4SomeS2581;
      moonbit_decref(_M0L3pkgS931.$0);
      _M0L6_2atmpS2589 = _M0Lm20match__tag__saver__0S939;
      _M0L6_2atmpS2588 = _M0L6_2atmpS2589 + 1;
      _M0L6_2atmpS2585 = (int64_t)_M0L6_2atmpS2588;
      _M0L6_2atmpS2587 = _M0Lm10match__endS938;
      _M0L6_2atmpS2586 = (int64_t)_M0L6_2atmpS2587;
      moonbit_incref(_M0L7_2adataS933);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS943
      = _M0MPC16string6String4view(_M0L7_2adataS933, _M0L6_2atmpS2585, _M0L6_2atmpS2586);
      _M0L6_2atmpS2582 = (int64_t)_M0L8_2astartS934;
      _M0L6_2atmpS2584 = _M0Lm20match__tag__saver__0S939;
      _M0L6_2atmpS2583 = (int64_t)_M0L6_2atmpS2584;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS944
      = _M0MPC16string6String4view(_M0L7_2adataS933, _M0L6_2atmpS2582, _M0L6_2atmpS2583);
      _M0L4SomeS2581
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2581)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2581)->$0_0
      = _M0L13package__nameS943.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2581)->$0_1
      = _M0L13package__nameS943.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2581)->$0_2
      = _M0L13package__nameS943.$2;
      _M0L7_2abindS941
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS941)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS941->$0_0 = _M0L12module__nameS944.$0;
      _M0L7_2abindS941->$0_1 = _M0L12module__nameS944.$1;
      _M0L7_2abindS941->$0_2 = _M0L12module__nameS944.$2;
      _M0L7_2abindS941->$1 = _M0L4SomeS2581;
      break;
    }
    default: {
      void* _M0L4NoneS2590;
      moonbit_decref(_M0L7_2adataS933);
      _M0L4NoneS2590
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS941
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS941)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS941->$0_0 = _M0L3pkgS931.$0;
      _M0L7_2abindS941->$0_1 = _M0L3pkgS931.$1;
      _M0L7_2abindS941->$0_2 = _M0L3pkgS931.$2;
      _M0L7_2abindS941->$1 = _M0L4NoneS2590;
      break;
    }
  }
  joinlet_4277:;
  _M0L8_2afieldS3956
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS941->$0_1, _M0L7_2abindS941->$0_2, _M0L7_2abindS941->$0_0
  };
  _M0L15_2amodule__nameS950 = _M0L8_2afieldS3956;
  _M0L8_2afieldS3955 = _M0L7_2abindS941->$1;
  _M0L6_2acntS4162 = Moonbit_object_header(_M0L7_2abindS941)->rc;
  if (_M0L6_2acntS4162 > 1) {
    int32_t _M0L11_2anew__cntS4163 = _M0L6_2acntS4162 - 1;
    Moonbit_object_header(_M0L7_2abindS941)->rc = _M0L11_2anew__cntS4163;
    moonbit_incref(_M0L8_2afieldS3955);
    moonbit_incref(_M0L15_2amodule__nameS950.$0);
  } else if (_M0L6_2acntS4162 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS941);
  }
  _M0L16_2apackage__nameS951 = _M0L8_2afieldS3955;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS951)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS952 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS951;
      struct _M0TPC16string10StringView _M0L8_2afieldS3954 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS952->$0_1,
                                              _M0L7_2aSomeS952->$0_2,
                                              _M0L7_2aSomeS952->$0_0};
      int32_t _M0L6_2acntS4164 = Moonbit_object_header(_M0L7_2aSomeS952)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS953;
      if (_M0L6_2acntS4164 > 1) {
        int32_t _M0L11_2anew__cntS4165 = _M0L6_2acntS4164 - 1;
        Moonbit_object_header(_M0L7_2aSomeS952)->rc = _M0L11_2anew__cntS4165;
        moonbit_incref(_M0L8_2afieldS3954.$0);
      } else if (_M0L6_2acntS4164 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS952);
      }
      _M0L12_2apkg__nameS953 = _M0L8_2afieldS3954;
      if (_M0L6loggerS954.$1) {
        moonbit_incref(_M0L6loggerS954.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L12_2apkg__nameS953);
      if (_M0L6loggerS954.$1) {
        moonbit_incref(_M0L6loggerS954.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS954.$0->$method_3(_M0L6loggerS954.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS951);
      break;
    }
  }
  _M0L8_2afieldS3953
  = (struct _M0TPC16string10StringView){
    _M0L4selfS932->$1_1, _M0L4selfS932->$1_2, _M0L4selfS932->$1_0
  };
  _M0L8filenameS2576 = _M0L8_2afieldS3953;
  moonbit_incref(_M0L8filenameS2576.$0);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L8filenameS2576);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_3(_M0L6loggerS954.$1, 58);
  _M0L8_2afieldS3952
  = (struct _M0TPC16string10StringView){
    _M0L4selfS932->$2_1, _M0L4selfS932->$2_2, _M0L4selfS932->$2_0
  };
  _M0L11start__lineS2577 = _M0L8_2afieldS3952;
  moonbit_incref(_M0L11start__lineS2577.$0);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L11start__lineS2577);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_3(_M0L6loggerS954.$1, 58);
  _M0L8_2afieldS3951
  = (struct _M0TPC16string10StringView){
    _M0L4selfS932->$3_1, _M0L4selfS932->$3_2, _M0L4selfS932->$3_0
  };
  _M0L13start__columnS2578 = _M0L8_2afieldS3951;
  moonbit_incref(_M0L13start__columnS2578.$0);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L13start__columnS2578);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_3(_M0L6loggerS954.$1, 45);
  _M0L8_2afieldS3950
  = (struct _M0TPC16string10StringView){
    _M0L4selfS932->$4_1, _M0L4selfS932->$4_2, _M0L4selfS932->$4_0
  };
  _M0L9end__lineS2579 = _M0L8_2afieldS3950;
  moonbit_incref(_M0L9end__lineS2579.$0);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L9end__lineS2579);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_3(_M0L6loggerS954.$1, 58);
  _M0L8_2afieldS3949
  = (struct _M0TPC16string10StringView){
    _M0L4selfS932->$5_1, _M0L4selfS932->$5_2, _M0L4selfS932->$5_0
  };
  _M0L6_2acntS4166 = Moonbit_object_header(_M0L4selfS932)->rc;
  if (_M0L6_2acntS4166 > 1) {
    int32_t _M0L11_2anew__cntS4172 = _M0L6_2acntS4166 - 1;
    Moonbit_object_header(_M0L4selfS932)->rc = _M0L11_2anew__cntS4172;
    moonbit_incref(_M0L8_2afieldS3949.$0);
  } else if (_M0L6_2acntS4166 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4171 =
      (struct _M0TPC16string10StringView){_M0L4selfS932->$4_1,
                                            _M0L4selfS932->$4_2,
                                            _M0L4selfS932->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4170;
    struct _M0TPC16string10StringView _M0L8_2afieldS4169;
    struct _M0TPC16string10StringView _M0L8_2afieldS4168;
    struct _M0TPC16string10StringView _M0L8_2afieldS4167;
    moonbit_decref(_M0L8_2afieldS4171.$0);
    _M0L8_2afieldS4170
    = (struct _M0TPC16string10StringView){
      _M0L4selfS932->$3_1, _M0L4selfS932->$3_2, _M0L4selfS932->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4170.$0);
    _M0L8_2afieldS4169
    = (struct _M0TPC16string10StringView){
      _M0L4selfS932->$2_1, _M0L4selfS932->$2_2, _M0L4selfS932->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4169.$0);
    _M0L8_2afieldS4168
    = (struct _M0TPC16string10StringView){
      _M0L4selfS932->$1_1, _M0L4selfS932->$1_2, _M0L4selfS932->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4168.$0);
    _M0L8_2afieldS4167
    = (struct _M0TPC16string10StringView){
      _M0L4selfS932->$0_1, _M0L4selfS932->$0_2, _M0L4selfS932->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4167.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS932);
  }
  _M0L11end__columnS2580 = _M0L8_2afieldS3949;
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L11end__columnS2580);
  if (_M0L6loggerS954.$1) {
    moonbit_incref(_M0L6loggerS954.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_3(_M0L6loggerS954.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS954.$0->$method_2(_M0L6loggerS954.$1, _M0L15_2amodule__nameS950);
  return 0;
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS923,
  int32_t _M0L5startS929,
  int64_t _M0L3endS925
) {
  int32_t _M0L3lenS922;
  int32_t _M0L3endS924;
  int32_t _M0L5startS928;
  int32_t _if__result_4281;
  #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3lenS922 = Moonbit_array_length(_M0L4selfS923);
  if (_M0L3endS925 == 4294967296ll) {
    _M0L3endS924 = _M0L3lenS922;
  } else {
    int64_t _M0L7_2aSomeS926 = _M0L3endS925;
    int32_t _M0L6_2aendS927 = (int32_t)_M0L7_2aSomeS926;
    if (_M0L6_2aendS927 < 0) {
      _M0L3endS924 = _M0L3lenS922 + _M0L6_2aendS927;
    } else {
      _M0L3endS924 = _M0L6_2aendS927;
    }
  }
  if (_M0L5startS929 < 0) {
    _M0L5startS928 = _M0L3lenS922 + _M0L5startS929;
  } else {
    _M0L5startS928 = _M0L5startS929;
  }
  if (_M0L5startS928 >= 0) {
    if (_M0L5startS928 <= _M0L3endS924) {
      _if__result_4281 = _M0L3endS924 <= _M0L3lenS922;
    } else {
      _if__result_4281 = 0;
    }
  } else {
    _if__result_4281 = 0;
  }
  if (_if__result_4281) {
    int32_t _M0L7_2abindS930 = _M0L3endS924 - _M0L5startS928;
    int32_t _M0L6_2atmpS2575 = _M0L5startS928 + _M0L7_2abindS930;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS928,
                                              _M0L6_2atmpS2575,
                                              _M0L4selfS923};
  } else {
    moonbit_decref(_M0L4selfS923);
    #line 180 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    return _M0FPB5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_43.data);
  }
}

int32_t _M0MPC15bytes9BytesView6length(
  struct _M0TPC15bytes9BytesView _M0L4selfS921
) {
  int32_t _M0L3endS2573;
  int32_t _M0L8_2afieldS3958;
  int32_t _M0L5startS2574;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3endS2573 = _M0L4selfS921.$2;
  _M0L8_2afieldS3958 = _M0L4selfS921.$1;
  moonbit_decref(_M0L4selfS921.$0);
  _M0L5startS2574 = _M0L8_2afieldS3958;
  return _M0L3endS2573 - _M0L5startS2574;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS920) {
  moonbit_string_t _M0L6_2atmpS2572;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2572 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS920);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2572);
  moonbit_decref(_M0L6_2atmpS2572);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS919,
  struct _M0TPB6Logger _M0L6loggerS918
) {
  moonbit_string_t _M0L6_2atmpS2571;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2571 = _M0MPC16double6Double10to__string(_M0L4selfS919);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS918.$0->$method_0(_M0L6loggerS918.$1, _M0L6_2atmpS2571);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS917) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS917);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS904) {
  uint64_t _M0L4bitsS905;
  uint64_t _M0L6_2atmpS2570;
  uint64_t _M0L6_2atmpS2569;
  int32_t _M0L8ieeeSignS906;
  uint64_t _M0L12ieeeMantissaS907;
  uint64_t _M0L6_2atmpS2568;
  uint64_t _M0L6_2atmpS2567;
  int32_t _M0L12ieeeExponentS908;
  int32_t _if__result_4282;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS909;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS910;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2566;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS904 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_44.data;
  }
  _M0L4bitsS905 = *(int64_t*)&_M0L3valS904;
  _M0L6_2atmpS2570 = _M0L4bitsS905 >> 63;
  _M0L6_2atmpS2569 = _M0L6_2atmpS2570 & 1ull;
  _M0L8ieeeSignS906 = _M0L6_2atmpS2569 != 0ull;
  _M0L12ieeeMantissaS907 = _M0L4bitsS905 & 4503599627370495ull;
  _M0L6_2atmpS2568 = _M0L4bitsS905 >> 52;
  _M0L6_2atmpS2567 = _M0L6_2atmpS2568 & 2047ull;
  _M0L12ieeeExponentS908 = (int32_t)_M0L6_2atmpS2567;
  if (_M0L12ieeeExponentS908 == 2047) {
    _if__result_4282 = 1;
  } else if (_M0L12ieeeExponentS908 == 0) {
    _if__result_4282 = _M0L12ieeeMantissaS907 == 0ull;
  } else {
    _if__result_4282 = 0;
  }
  if (_if__result_4282) {
    int32_t _M0L6_2atmpS2555 = _M0L12ieeeExponentS908 != 0;
    int32_t _M0L6_2atmpS2556 = _M0L12ieeeMantissaS907 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS906, _M0L6_2atmpS2555, _M0L6_2atmpS2556);
  }
  _M0Lm1vS909 = _M0FPB30ryu__to__string_2erecord_2f903;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS910
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS907, _M0L12ieeeExponentS908);
  if (_M0L5smallS910 == 0) {
    uint32_t _M0L6_2atmpS2557;
    if (_M0L5smallS910) {
      moonbit_decref(_M0L5smallS910);
    }
    _M0L6_2atmpS2557 = *(uint32_t*)&_M0L12ieeeExponentS908;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS909 = _M0FPB3d2d(_M0L12ieeeMantissaS907, _M0L6_2atmpS2557);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS911 = _M0L5smallS910;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS912 = _M0L7_2aSomeS911;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS913 = _M0L4_2afS912;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2565 = _M0Lm1xS913;
      uint64_t _M0L8_2afieldS3961 = _M0L6_2atmpS2565->$0;
      uint64_t _M0L8mantissaS2564 = _M0L8_2afieldS3961;
      uint64_t _M0L1qS914 = _M0L8mantissaS2564 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2563 = _M0Lm1xS913;
      uint64_t _M0L8_2afieldS3960 = _M0L6_2atmpS2563->$0;
      uint64_t _M0L8mantissaS2561 = _M0L8_2afieldS3960;
      uint64_t _M0L6_2atmpS2562 = 10ull * _M0L1qS914;
      uint64_t _M0L1rS915 = _M0L8mantissaS2561 - _M0L6_2atmpS2562;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2560;
      int32_t _M0L8_2afieldS3959;
      int32_t _M0L8exponentS2559;
      int32_t _M0L6_2atmpS2558;
      if (_M0L1rS915 != 0ull) {
        break;
      }
      _M0L6_2atmpS2560 = _M0Lm1xS913;
      _M0L8_2afieldS3959 = _M0L6_2atmpS2560->$1;
      moonbit_decref(_M0L6_2atmpS2560);
      _M0L8exponentS2559 = _M0L8_2afieldS3959;
      _M0L6_2atmpS2558 = _M0L8exponentS2559 + 1;
      _M0Lm1xS913
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS913)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS913->$0 = _M0L1qS914;
      _M0Lm1xS913->$1 = _M0L6_2atmpS2558;
      continue;
      break;
    }
    _M0Lm1vS909 = _M0Lm1xS913;
  }
  _M0L6_2atmpS2566 = _M0Lm1vS909;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2566, _M0L8ieeeSignS906);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS898,
  int32_t _M0L12ieeeExponentS900
) {
  uint64_t _M0L2m2S897;
  int32_t _M0L6_2atmpS2554;
  int32_t _M0L2e2S899;
  int32_t _M0L6_2atmpS2553;
  uint64_t _M0L6_2atmpS2552;
  uint64_t _M0L4maskS901;
  uint64_t _M0L8fractionS902;
  int32_t _M0L6_2atmpS2551;
  uint64_t _M0L6_2atmpS2550;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2549;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S897 = 4503599627370496ull | _M0L12ieeeMantissaS898;
  _M0L6_2atmpS2554 = _M0L12ieeeExponentS900 - 1023;
  _M0L2e2S899 = _M0L6_2atmpS2554 - 52;
  if (_M0L2e2S899 > 0) {
    return 0;
  }
  if (_M0L2e2S899 < -52) {
    return 0;
  }
  _M0L6_2atmpS2553 = -_M0L2e2S899;
  _M0L6_2atmpS2552 = 1ull << (_M0L6_2atmpS2553 & 63);
  _M0L4maskS901 = _M0L6_2atmpS2552 - 1ull;
  _M0L8fractionS902 = _M0L2m2S897 & _M0L4maskS901;
  if (_M0L8fractionS902 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2551 = -_M0L2e2S899;
  _M0L6_2atmpS2550 = _M0L2m2S897 >> (_M0L6_2atmpS2551 & 63);
  _M0L6_2atmpS2549
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2549)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2549->$0 = _M0L6_2atmpS2550;
  _M0L6_2atmpS2549->$1 = 0;
  return _M0L6_2atmpS2549;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS871,
  int32_t _M0L4signS869
) {
  int32_t _M0L6_2atmpS2548;
  moonbit_bytes_t _M0L6resultS867;
  int32_t _M0Lm5indexS868;
  uint64_t _M0Lm6outputS870;
  uint64_t _M0L6_2atmpS2547;
  int32_t _M0L7olengthS872;
  int32_t _M0L8_2afieldS3962;
  int32_t _M0L8exponentS2546;
  int32_t _M0L6_2atmpS2545;
  int32_t _M0Lm3expS873;
  int32_t _M0L6_2atmpS2544;
  int32_t _M0L6_2atmpS2542;
  int32_t _M0L18scientificNotationS874;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2548 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS867 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2548);
  _M0Lm5indexS868 = 0;
  if (_M0L4signS869) {
    int32_t _M0L6_2atmpS2417 = _M0Lm5indexS868;
    int32_t _M0L6_2atmpS2418;
    if (
      _M0L6_2atmpS2417 < 0
      || _M0L6_2atmpS2417 >= Moonbit_array_length(_M0L6resultS867)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS867[_M0L6_2atmpS2417] = 45;
    _M0L6_2atmpS2418 = _M0Lm5indexS868;
    _M0Lm5indexS868 = _M0L6_2atmpS2418 + 1;
  }
  _M0Lm6outputS870 = _M0L1vS871->$0;
  _M0L6_2atmpS2547 = _M0Lm6outputS870;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS872 = _M0FPB17decimal__length17(_M0L6_2atmpS2547);
  _M0L8_2afieldS3962 = _M0L1vS871->$1;
  moonbit_decref(_M0L1vS871);
  _M0L8exponentS2546 = _M0L8_2afieldS3962;
  _M0L6_2atmpS2545 = _M0L8exponentS2546 + _M0L7olengthS872;
  _M0Lm3expS873 = _M0L6_2atmpS2545 - 1;
  _M0L6_2atmpS2544 = _M0Lm3expS873;
  if (_M0L6_2atmpS2544 >= -6) {
    int32_t _M0L6_2atmpS2543 = _M0Lm3expS873;
    _M0L6_2atmpS2542 = _M0L6_2atmpS2543 < 21;
  } else {
    _M0L6_2atmpS2542 = 0;
  }
  _M0L18scientificNotationS874 = !_M0L6_2atmpS2542;
  if (_M0L18scientificNotationS874) {
    int32_t _M0L7_2abindS875 = _M0L7olengthS872 - 1;
    int32_t _M0L1iS876 = 0;
    int32_t _M0L6_2atmpS2428;
    uint64_t _M0L6_2atmpS2433;
    int32_t _M0L6_2atmpS2432;
    int32_t _M0L6_2atmpS2431;
    int32_t _M0L6_2atmpS2430;
    int32_t _M0L6_2atmpS2429;
    int32_t _M0L6_2atmpS2437;
    int32_t _M0L6_2atmpS2438;
    int32_t _M0L6_2atmpS2439;
    int32_t _M0L6_2atmpS2440;
    int32_t _M0L6_2atmpS2441;
    int32_t _M0L6_2atmpS2447;
    int32_t _M0L6_2atmpS2480;
    while (1) {
      if (_M0L1iS876 < _M0L7_2abindS875) {
        uint64_t _M0L6_2atmpS2426 = _M0Lm6outputS870;
        uint64_t _M0L1cS877 = _M0L6_2atmpS2426 % 10ull;
        uint64_t _M0L6_2atmpS2419 = _M0Lm6outputS870;
        int32_t _M0L6_2atmpS2425;
        int32_t _M0L6_2atmpS2424;
        int32_t _M0L6_2atmpS2420;
        int32_t _M0L6_2atmpS2423;
        int32_t _M0L6_2atmpS2422;
        int32_t _M0L6_2atmpS2421;
        int32_t _M0L6_2atmpS2427;
        _M0Lm6outputS870 = _M0L6_2atmpS2419 / 10ull;
        _M0L6_2atmpS2425 = _M0Lm5indexS868;
        _M0L6_2atmpS2424 = _M0L6_2atmpS2425 + _M0L7olengthS872;
        _M0L6_2atmpS2420 = _M0L6_2atmpS2424 - _M0L1iS876;
        _M0L6_2atmpS2423 = (int32_t)_M0L1cS877;
        _M0L6_2atmpS2422 = 48 + _M0L6_2atmpS2423;
        _M0L6_2atmpS2421 = _M0L6_2atmpS2422 & 0xff;
        if (
          _M0L6_2atmpS2420 < 0
          || _M0L6_2atmpS2420 >= Moonbit_array_length(_M0L6resultS867)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS867[_M0L6_2atmpS2420] = _M0L6_2atmpS2421;
        _M0L6_2atmpS2427 = _M0L1iS876 + 1;
        _M0L1iS876 = _M0L6_2atmpS2427;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2428 = _M0Lm5indexS868;
    _M0L6_2atmpS2433 = _M0Lm6outputS870;
    _M0L6_2atmpS2432 = (int32_t)_M0L6_2atmpS2433;
    _M0L6_2atmpS2431 = _M0L6_2atmpS2432 % 10;
    _M0L6_2atmpS2430 = 48 + _M0L6_2atmpS2431;
    _M0L6_2atmpS2429 = _M0L6_2atmpS2430 & 0xff;
    if (
      _M0L6_2atmpS2428 < 0
      || _M0L6_2atmpS2428 >= Moonbit_array_length(_M0L6resultS867)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS867[_M0L6_2atmpS2428] = _M0L6_2atmpS2429;
    if (_M0L7olengthS872 > 1) {
      int32_t _M0L6_2atmpS2435 = _M0Lm5indexS868;
      int32_t _M0L6_2atmpS2434 = _M0L6_2atmpS2435 + 1;
      if (
        _M0L6_2atmpS2434 < 0
        || _M0L6_2atmpS2434 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2434] = 46;
    } else {
      int32_t _M0L6_2atmpS2436 = _M0Lm5indexS868;
      _M0Lm5indexS868 = _M0L6_2atmpS2436 - 1;
    }
    _M0L6_2atmpS2437 = _M0Lm5indexS868;
    _M0L6_2atmpS2438 = _M0L7olengthS872 + 1;
    _M0Lm5indexS868 = _M0L6_2atmpS2437 + _M0L6_2atmpS2438;
    _M0L6_2atmpS2439 = _M0Lm5indexS868;
    if (
      _M0L6_2atmpS2439 < 0
      || _M0L6_2atmpS2439 >= Moonbit_array_length(_M0L6resultS867)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS867[_M0L6_2atmpS2439] = 101;
    _M0L6_2atmpS2440 = _M0Lm5indexS868;
    _M0Lm5indexS868 = _M0L6_2atmpS2440 + 1;
    _M0L6_2atmpS2441 = _M0Lm3expS873;
    if (_M0L6_2atmpS2441 < 0) {
      int32_t _M0L6_2atmpS2442 = _M0Lm5indexS868;
      int32_t _M0L6_2atmpS2443;
      int32_t _M0L6_2atmpS2444;
      if (
        _M0L6_2atmpS2442 < 0
        || _M0L6_2atmpS2442 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2442] = 45;
      _M0L6_2atmpS2443 = _M0Lm5indexS868;
      _M0Lm5indexS868 = _M0L6_2atmpS2443 + 1;
      _M0L6_2atmpS2444 = _M0Lm3expS873;
      _M0Lm3expS873 = -_M0L6_2atmpS2444;
    } else {
      int32_t _M0L6_2atmpS2445 = _M0Lm5indexS868;
      int32_t _M0L6_2atmpS2446;
      if (
        _M0L6_2atmpS2445 < 0
        || _M0L6_2atmpS2445 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2445] = 43;
      _M0L6_2atmpS2446 = _M0Lm5indexS868;
      _M0Lm5indexS868 = _M0L6_2atmpS2446 + 1;
    }
    _M0L6_2atmpS2447 = _M0Lm3expS873;
    if (_M0L6_2atmpS2447 >= 100) {
      int32_t _M0L6_2atmpS2463 = _M0Lm3expS873;
      int32_t _M0L1aS879 = _M0L6_2atmpS2463 / 100;
      int32_t _M0L6_2atmpS2462 = _M0Lm3expS873;
      int32_t _M0L6_2atmpS2461 = _M0L6_2atmpS2462 / 10;
      int32_t _M0L1bS880 = _M0L6_2atmpS2461 % 10;
      int32_t _M0L6_2atmpS2460 = _M0Lm3expS873;
      int32_t _M0L1cS881 = _M0L6_2atmpS2460 % 10;
      int32_t _M0L6_2atmpS2448 = _M0Lm5indexS868;
      int32_t _M0L6_2atmpS2450 = 48 + _M0L1aS879;
      int32_t _M0L6_2atmpS2449 = _M0L6_2atmpS2450 & 0xff;
      int32_t _M0L6_2atmpS2454;
      int32_t _M0L6_2atmpS2451;
      int32_t _M0L6_2atmpS2453;
      int32_t _M0L6_2atmpS2452;
      int32_t _M0L6_2atmpS2458;
      int32_t _M0L6_2atmpS2455;
      int32_t _M0L6_2atmpS2457;
      int32_t _M0L6_2atmpS2456;
      int32_t _M0L6_2atmpS2459;
      if (
        _M0L6_2atmpS2448 < 0
        || _M0L6_2atmpS2448 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2448] = _M0L6_2atmpS2449;
      _M0L6_2atmpS2454 = _M0Lm5indexS868;
      _M0L6_2atmpS2451 = _M0L6_2atmpS2454 + 1;
      _M0L6_2atmpS2453 = 48 + _M0L1bS880;
      _M0L6_2atmpS2452 = _M0L6_2atmpS2453 & 0xff;
      if (
        _M0L6_2atmpS2451 < 0
        || _M0L6_2atmpS2451 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2451] = _M0L6_2atmpS2452;
      _M0L6_2atmpS2458 = _M0Lm5indexS868;
      _M0L6_2atmpS2455 = _M0L6_2atmpS2458 + 2;
      _M0L6_2atmpS2457 = 48 + _M0L1cS881;
      _M0L6_2atmpS2456 = _M0L6_2atmpS2457 & 0xff;
      if (
        _M0L6_2atmpS2455 < 0
        || _M0L6_2atmpS2455 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2455] = _M0L6_2atmpS2456;
      _M0L6_2atmpS2459 = _M0Lm5indexS868;
      _M0Lm5indexS868 = _M0L6_2atmpS2459 + 3;
    } else {
      int32_t _M0L6_2atmpS2464 = _M0Lm3expS873;
      if (_M0L6_2atmpS2464 >= 10) {
        int32_t _M0L6_2atmpS2474 = _M0Lm3expS873;
        int32_t _M0L1aS882 = _M0L6_2atmpS2474 / 10;
        int32_t _M0L6_2atmpS2473 = _M0Lm3expS873;
        int32_t _M0L1bS883 = _M0L6_2atmpS2473 % 10;
        int32_t _M0L6_2atmpS2465 = _M0Lm5indexS868;
        int32_t _M0L6_2atmpS2467 = 48 + _M0L1aS882;
        int32_t _M0L6_2atmpS2466 = _M0L6_2atmpS2467 & 0xff;
        int32_t _M0L6_2atmpS2471;
        int32_t _M0L6_2atmpS2468;
        int32_t _M0L6_2atmpS2470;
        int32_t _M0L6_2atmpS2469;
        int32_t _M0L6_2atmpS2472;
        if (
          _M0L6_2atmpS2465 < 0
          || _M0L6_2atmpS2465 >= Moonbit_array_length(_M0L6resultS867)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS867[_M0L6_2atmpS2465] = _M0L6_2atmpS2466;
        _M0L6_2atmpS2471 = _M0Lm5indexS868;
        _M0L6_2atmpS2468 = _M0L6_2atmpS2471 + 1;
        _M0L6_2atmpS2470 = 48 + _M0L1bS883;
        _M0L6_2atmpS2469 = _M0L6_2atmpS2470 & 0xff;
        if (
          _M0L6_2atmpS2468 < 0
          || _M0L6_2atmpS2468 >= Moonbit_array_length(_M0L6resultS867)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS867[_M0L6_2atmpS2468] = _M0L6_2atmpS2469;
        _M0L6_2atmpS2472 = _M0Lm5indexS868;
        _M0Lm5indexS868 = _M0L6_2atmpS2472 + 2;
      } else {
        int32_t _M0L6_2atmpS2475 = _M0Lm5indexS868;
        int32_t _M0L6_2atmpS2478 = _M0Lm3expS873;
        int32_t _M0L6_2atmpS2477 = 48 + _M0L6_2atmpS2478;
        int32_t _M0L6_2atmpS2476 = _M0L6_2atmpS2477 & 0xff;
        int32_t _M0L6_2atmpS2479;
        if (
          _M0L6_2atmpS2475 < 0
          || _M0L6_2atmpS2475 >= Moonbit_array_length(_M0L6resultS867)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS867[_M0L6_2atmpS2475] = _M0L6_2atmpS2476;
        _M0L6_2atmpS2479 = _M0Lm5indexS868;
        _M0Lm5indexS868 = _M0L6_2atmpS2479 + 1;
      }
    }
    _M0L6_2atmpS2480 = _M0Lm5indexS868;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS867, 0, _M0L6_2atmpS2480);
  } else {
    int32_t _M0L6_2atmpS2481 = _M0Lm3expS873;
    int32_t _M0L6_2atmpS2541;
    if (_M0L6_2atmpS2481 < 0) {
      int32_t _M0L6_2atmpS2482 = _M0Lm5indexS868;
      int32_t _M0L6_2atmpS2483;
      int32_t _M0L6_2atmpS2484;
      int32_t _M0L6_2atmpS2485;
      int32_t _M0L1iS884;
      int32_t _M0L7currentS886;
      int32_t _M0L1iS887;
      if (
        _M0L6_2atmpS2482 < 0
        || _M0L6_2atmpS2482 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2482] = 48;
      _M0L6_2atmpS2483 = _M0Lm5indexS868;
      _M0Lm5indexS868 = _M0L6_2atmpS2483 + 1;
      _M0L6_2atmpS2484 = _M0Lm5indexS868;
      if (
        _M0L6_2atmpS2484 < 0
        || _M0L6_2atmpS2484 >= Moonbit_array_length(_M0L6resultS867)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS867[_M0L6_2atmpS2484] = 46;
      _M0L6_2atmpS2485 = _M0Lm5indexS868;
      _M0Lm5indexS868 = _M0L6_2atmpS2485 + 1;
      _M0L1iS884 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2486 = _M0Lm3expS873;
        if (_M0L1iS884 > _M0L6_2atmpS2486) {
          int32_t _M0L6_2atmpS2487 = _M0Lm5indexS868;
          int32_t _M0L6_2atmpS2488;
          int32_t _M0L6_2atmpS2489;
          if (
            _M0L6_2atmpS2487 < 0
            || _M0L6_2atmpS2487 >= Moonbit_array_length(_M0L6resultS867)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS867[_M0L6_2atmpS2487] = 48;
          _M0L6_2atmpS2488 = _M0Lm5indexS868;
          _M0Lm5indexS868 = _M0L6_2atmpS2488 + 1;
          _M0L6_2atmpS2489 = _M0L1iS884 - 1;
          _M0L1iS884 = _M0L6_2atmpS2489;
          continue;
        }
        break;
      }
      _M0L7currentS886 = _M0Lm5indexS868;
      _M0L1iS887 = 0;
      while (1) {
        if (_M0L1iS887 < _M0L7olengthS872) {
          int32_t _M0L6_2atmpS2497 = _M0L7currentS886 + _M0L7olengthS872;
          int32_t _M0L6_2atmpS2496 = _M0L6_2atmpS2497 - _M0L1iS887;
          int32_t _M0L6_2atmpS2490 = _M0L6_2atmpS2496 - 1;
          uint64_t _M0L6_2atmpS2495 = _M0Lm6outputS870;
          uint64_t _M0L6_2atmpS2494 = _M0L6_2atmpS2495 % 10ull;
          int32_t _M0L6_2atmpS2493 = (int32_t)_M0L6_2atmpS2494;
          int32_t _M0L6_2atmpS2492 = 48 + _M0L6_2atmpS2493;
          int32_t _M0L6_2atmpS2491 = _M0L6_2atmpS2492 & 0xff;
          uint64_t _M0L6_2atmpS2498;
          int32_t _M0L6_2atmpS2499;
          int32_t _M0L6_2atmpS2500;
          if (
            _M0L6_2atmpS2490 < 0
            || _M0L6_2atmpS2490 >= Moonbit_array_length(_M0L6resultS867)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS867[_M0L6_2atmpS2490] = _M0L6_2atmpS2491;
          _M0L6_2atmpS2498 = _M0Lm6outputS870;
          _M0Lm6outputS870 = _M0L6_2atmpS2498 / 10ull;
          _M0L6_2atmpS2499 = _M0Lm5indexS868;
          _M0Lm5indexS868 = _M0L6_2atmpS2499 + 1;
          _M0L6_2atmpS2500 = _M0L1iS887 + 1;
          _M0L1iS887 = _M0L6_2atmpS2500;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2502 = _M0Lm3expS873;
      int32_t _M0L6_2atmpS2501 = _M0L6_2atmpS2502 + 1;
      if (_M0L6_2atmpS2501 >= _M0L7olengthS872) {
        int32_t _M0L1iS889 = 0;
        int32_t _M0L6_2atmpS2514;
        int32_t _M0L6_2atmpS2518;
        int32_t _M0L7_2abindS891;
        int32_t _M0L2__S892;
        while (1) {
          if (_M0L1iS889 < _M0L7olengthS872) {
            int32_t _M0L6_2atmpS2511 = _M0Lm5indexS868;
            int32_t _M0L6_2atmpS2510 = _M0L6_2atmpS2511 + _M0L7olengthS872;
            int32_t _M0L6_2atmpS2509 = _M0L6_2atmpS2510 - _M0L1iS889;
            int32_t _M0L6_2atmpS2503 = _M0L6_2atmpS2509 - 1;
            uint64_t _M0L6_2atmpS2508 = _M0Lm6outputS870;
            uint64_t _M0L6_2atmpS2507 = _M0L6_2atmpS2508 % 10ull;
            int32_t _M0L6_2atmpS2506 = (int32_t)_M0L6_2atmpS2507;
            int32_t _M0L6_2atmpS2505 = 48 + _M0L6_2atmpS2506;
            int32_t _M0L6_2atmpS2504 = _M0L6_2atmpS2505 & 0xff;
            uint64_t _M0L6_2atmpS2512;
            int32_t _M0L6_2atmpS2513;
            if (
              _M0L6_2atmpS2503 < 0
              || _M0L6_2atmpS2503 >= Moonbit_array_length(_M0L6resultS867)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS867[_M0L6_2atmpS2503] = _M0L6_2atmpS2504;
            _M0L6_2atmpS2512 = _M0Lm6outputS870;
            _M0Lm6outputS870 = _M0L6_2atmpS2512 / 10ull;
            _M0L6_2atmpS2513 = _M0L1iS889 + 1;
            _M0L1iS889 = _M0L6_2atmpS2513;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2514 = _M0Lm5indexS868;
        _M0Lm5indexS868 = _M0L6_2atmpS2514 + _M0L7olengthS872;
        _M0L6_2atmpS2518 = _M0Lm3expS873;
        _M0L7_2abindS891 = _M0L6_2atmpS2518 + 1;
        _M0L2__S892 = _M0L7olengthS872;
        while (1) {
          if (_M0L2__S892 < _M0L7_2abindS891) {
            int32_t _M0L6_2atmpS2515 = _M0Lm5indexS868;
            int32_t _M0L6_2atmpS2516;
            int32_t _M0L6_2atmpS2517;
            if (
              _M0L6_2atmpS2515 < 0
              || _M0L6_2atmpS2515 >= Moonbit_array_length(_M0L6resultS867)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS867[_M0L6_2atmpS2515] = 48;
            _M0L6_2atmpS2516 = _M0Lm5indexS868;
            _M0Lm5indexS868 = _M0L6_2atmpS2516 + 1;
            _M0L6_2atmpS2517 = _M0L2__S892 + 1;
            _M0L2__S892 = _M0L6_2atmpS2517;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2540 = _M0Lm5indexS868;
        int32_t _M0Lm7currentS894 = _M0L6_2atmpS2540 + 1;
        int32_t _M0L1iS895 = 0;
        int32_t _M0L6_2atmpS2538;
        int32_t _M0L6_2atmpS2539;
        while (1) {
          if (_M0L1iS895 < _M0L7olengthS872) {
            int32_t _M0L6_2atmpS2521 = _M0L7olengthS872 - _M0L1iS895;
            int32_t _M0L6_2atmpS2519 = _M0L6_2atmpS2521 - 1;
            int32_t _M0L6_2atmpS2520 = _M0Lm3expS873;
            int32_t _M0L6_2atmpS2535;
            int32_t _M0L6_2atmpS2534;
            int32_t _M0L6_2atmpS2533;
            int32_t _M0L6_2atmpS2527;
            uint64_t _M0L6_2atmpS2532;
            uint64_t _M0L6_2atmpS2531;
            int32_t _M0L6_2atmpS2530;
            int32_t _M0L6_2atmpS2529;
            int32_t _M0L6_2atmpS2528;
            uint64_t _M0L6_2atmpS2536;
            int32_t _M0L6_2atmpS2537;
            if (_M0L6_2atmpS2519 == _M0L6_2atmpS2520) {
              int32_t _M0L6_2atmpS2525 = _M0Lm7currentS894;
              int32_t _M0L6_2atmpS2524 = _M0L6_2atmpS2525 + _M0L7olengthS872;
              int32_t _M0L6_2atmpS2523 = _M0L6_2atmpS2524 - _M0L1iS895;
              int32_t _M0L6_2atmpS2522 = _M0L6_2atmpS2523 - 1;
              int32_t _M0L6_2atmpS2526;
              if (
                _M0L6_2atmpS2522 < 0
                || _M0L6_2atmpS2522 >= Moonbit_array_length(_M0L6resultS867)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS867[_M0L6_2atmpS2522] = 46;
              _M0L6_2atmpS2526 = _M0Lm7currentS894;
              _M0Lm7currentS894 = _M0L6_2atmpS2526 - 1;
            }
            _M0L6_2atmpS2535 = _M0Lm7currentS894;
            _M0L6_2atmpS2534 = _M0L6_2atmpS2535 + _M0L7olengthS872;
            _M0L6_2atmpS2533 = _M0L6_2atmpS2534 - _M0L1iS895;
            _M0L6_2atmpS2527 = _M0L6_2atmpS2533 - 1;
            _M0L6_2atmpS2532 = _M0Lm6outputS870;
            _M0L6_2atmpS2531 = _M0L6_2atmpS2532 % 10ull;
            _M0L6_2atmpS2530 = (int32_t)_M0L6_2atmpS2531;
            _M0L6_2atmpS2529 = 48 + _M0L6_2atmpS2530;
            _M0L6_2atmpS2528 = _M0L6_2atmpS2529 & 0xff;
            if (
              _M0L6_2atmpS2527 < 0
              || _M0L6_2atmpS2527 >= Moonbit_array_length(_M0L6resultS867)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS867[_M0L6_2atmpS2527] = _M0L6_2atmpS2528;
            _M0L6_2atmpS2536 = _M0Lm6outputS870;
            _M0Lm6outputS870 = _M0L6_2atmpS2536 / 10ull;
            _M0L6_2atmpS2537 = _M0L1iS895 + 1;
            _M0L1iS895 = _M0L6_2atmpS2537;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2538 = _M0Lm5indexS868;
        _M0L6_2atmpS2539 = _M0L7olengthS872 + 1;
        _M0Lm5indexS868 = _M0L6_2atmpS2538 + _M0L6_2atmpS2539;
      }
    }
    _M0L6_2atmpS2541 = _M0Lm5indexS868;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS867, 0, _M0L6_2atmpS2541);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS813,
  uint32_t _M0L12ieeeExponentS812
) {
  int32_t _M0Lm2e2S810;
  uint64_t _M0Lm2m2S811;
  uint64_t _M0L6_2atmpS2416;
  uint64_t _M0L6_2atmpS2415;
  int32_t _M0L4evenS814;
  uint64_t _M0L6_2atmpS2414;
  uint64_t _M0L2mvS815;
  int32_t _M0L7mmShiftS816;
  uint64_t _M0Lm2vrS817;
  uint64_t _M0Lm2vpS818;
  uint64_t _M0Lm2vmS819;
  int32_t _M0Lm3e10S820;
  int32_t _M0Lm17vmIsTrailingZerosS821;
  int32_t _M0Lm17vrIsTrailingZerosS822;
  int32_t _M0L6_2atmpS2316;
  int32_t _M0Lm7removedS841;
  int32_t _M0Lm16lastRemovedDigitS842;
  uint64_t _M0Lm6outputS843;
  int32_t _M0L6_2atmpS2412;
  int32_t _M0L6_2atmpS2413;
  int32_t _M0L3expS866;
  uint64_t _M0L6_2atmpS2411;
  struct _M0TPB17FloatingDecimal64* _block_4295;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S810 = 0;
  _M0Lm2m2S811 = 0ull;
  if (_M0L12ieeeExponentS812 == 0u) {
    _M0Lm2e2S810 = -1076;
    _M0Lm2m2S811 = _M0L12ieeeMantissaS813;
  } else {
    int32_t _M0L6_2atmpS2315 = *(int32_t*)&_M0L12ieeeExponentS812;
    int32_t _M0L6_2atmpS2314 = _M0L6_2atmpS2315 - 1023;
    int32_t _M0L6_2atmpS2313 = _M0L6_2atmpS2314 - 52;
    _M0Lm2e2S810 = _M0L6_2atmpS2313 - 2;
    _M0Lm2m2S811 = 4503599627370496ull | _M0L12ieeeMantissaS813;
  }
  _M0L6_2atmpS2416 = _M0Lm2m2S811;
  _M0L6_2atmpS2415 = _M0L6_2atmpS2416 & 1ull;
  _M0L4evenS814 = _M0L6_2atmpS2415 == 0ull;
  _M0L6_2atmpS2414 = _M0Lm2m2S811;
  _M0L2mvS815 = 4ull * _M0L6_2atmpS2414;
  if (_M0L12ieeeMantissaS813 != 0ull) {
    _M0L7mmShiftS816 = 1;
  } else {
    _M0L7mmShiftS816 = _M0L12ieeeExponentS812 <= 1u;
  }
  _M0Lm2vrS817 = 0ull;
  _M0Lm2vpS818 = 0ull;
  _M0Lm2vmS819 = 0ull;
  _M0Lm3e10S820 = 0;
  _M0Lm17vmIsTrailingZerosS821 = 0;
  _M0Lm17vrIsTrailingZerosS822 = 0;
  _M0L6_2atmpS2316 = _M0Lm2e2S810;
  if (_M0L6_2atmpS2316 >= 0) {
    int32_t _M0L6_2atmpS2338 = _M0Lm2e2S810;
    int32_t _M0L6_2atmpS2334;
    int32_t _M0L6_2atmpS2337;
    int32_t _M0L6_2atmpS2336;
    int32_t _M0L6_2atmpS2335;
    int32_t _M0L1qS823;
    int32_t _M0L6_2atmpS2333;
    int32_t _M0L6_2atmpS2332;
    int32_t _M0L1kS824;
    int32_t _M0L6_2atmpS2331;
    int32_t _M0L6_2atmpS2330;
    int32_t _M0L6_2atmpS2329;
    int32_t _M0L1iS825;
    struct _M0TPB8Pow5Pair _M0L4pow5S826;
    uint64_t _M0L6_2atmpS2328;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS827;
    uint64_t _M0L8_2avrOutS828;
    uint64_t _M0L8_2avpOutS829;
    uint64_t _M0L8_2avmOutS830;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2334 = _M0FPB9log10Pow2(_M0L6_2atmpS2338);
    _M0L6_2atmpS2337 = _M0Lm2e2S810;
    _M0L6_2atmpS2336 = _M0L6_2atmpS2337 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2335 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2336);
    _M0L1qS823 = _M0L6_2atmpS2334 - _M0L6_2atmpS2335;
    _M0Lm3e10S820 = _M0L1qS823;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2333 = _M0FPB8pow5bits(_M0L1qS823);
    _M0L6_2atmpS2332 = 125 + _M0L6_2atmpS2333;
    _M0L1kS824 = _M0L6_2atmpS2332 - 1;
    _M0L6_2atmpS2331 = _M0Lm2e2S810;
    _M0L6_2atmpS2330 = -_M0L6_2atmpS2331;
    _M0L6_2atmpS2329 = _M0L6_2atmpS2330 + _M0L1qS823;
    _M0L1iS825 = _M0L6_2atmpS2329 + _M0L1kS824;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S826 = _M0FPB22double__computeInvPow5(_M0L1qS823);
    _M0L6_2atmpS2328 = _M0Lm2m2S811;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS827
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2328, _M0L4pow5S826, _M0L1iS825, _M0L7mmShiftS816);
    _M0L8_2avrOutS828 = _M0L7_2abindS827.$0;
    _M0L8_2avpOutS829 = _M0L7_2abindS827.$1;
    _M0L8_2avmOutS830 = _M0L7_2abindS827.$2;
    _M0Lm2vrS817 = _M0L8_2avrOutS828;
    _M0Lm2vpS818 = _M0L8_2avpOutS829;
    _M0Lm2vmS819 = _M0L8_2avmOutS830;
    if (_M0L1qS823 <= 21) {
      int32_t _M0L6_2atmpS2324 = (int32_t)_M0L2mvS815;
      uint64_t _M0L6_2atmpS2327 = _M0L2mvS815 / 5ull;
      int32_t _M0L6_2atmpS2326 = (int32_t)_M0L6_2atmpS2327;
      int32_t _M0L6_2atmpS2325 = 5 * _M0L6_2atmpS2326;
      int32_t _M0L6mvMod5S831 = _M0L6_2atmpS2324 - _M0L6_2atmpS2325;
      if (_M0L6mvMod5S831 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS822
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS815, _M0L1qS823);
      } else if (_M0L4evenS814) {
        uint64_t _M0L6_2atmpS2318 = _M0L2mvS815 - 1ull;
        uint64_t _M0L6_2atmpS2319;
        uint64_t _M0L6_2atmpS2317;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2319 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS816);
        _M0L6_2atmpS2317 = _M0L6_2atmpS2318 - _M0L6_2atmpS2319;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS821
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2317, _M0L1qS823);
      } else {
        uint64_t _M0L6_2atmpS2320 = _M0Lm2vpS818;
        uint64_t _M0L6_2atmpS2323 = _M0L2mvS815 + 2ull;
        int32_t _M0L6_2atmpS2322;
        uint64_t _M0L6_2atmpS2321;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2322
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2323, _M0L1qS823);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2321 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2322);
        _M0Lm2vpS818 = _M0L6_2atmpS2320 - _M0L6_2atmpS2321;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2352 = _M0Lm2e2S810;
    int32_t _M0L6_2atmpS2351 = -_M0L6_2atmpS2352;
    int32_t _M0L6_2atmpS2346;
    int32_t _M0L6_2atmpS2350;
    int32_t _M0L6_2atmpS2349;
    int32_t _M0L6_2atmpS2348;
    int32_t _M0L6_2atmpS2347;
    int32_t _M0L1qS832;
    int32_t _M0L6_2atmpS2339;
    int32_t _M0L6_2atmpS2345;
    int32_t _M0L6_2atmpS2344;
    int32_t _M0L1iS833;
    int32_t _M0L6_2atmpS2343;
    int32_t _M0L1kS834;
    int32_t _M0L1jS835;
    struct _M0TPB8Pow5Pair _M0L4pow5S836;
    uint64_t _M0L6_2atmpS2342;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS837;
    uint64_t _M0L8_2avrOutS838;
    uint64_t _M0L8_2avpOutS839;
    uint64_t _M0L8_2avmOutS840;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2346 = _M0FPB9log10Pow5(_M0L6_2atmpS2351);
    _M0L6_2atmpS2350 = _M0Lm2e2S810;
    _M0L6_2atmpS2349 = -_M0L6_2atmpS2350;
    _M0L6_2atmpS2348 = _M0L6_2atmpS2349 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2347 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2348);
    _M0L1qS832 = _M0L6_2atmpS2346 - _M0L6_2atmpS2347;
    _M0L6_2atmpS2339 = _M0Lm2e2S810;
    _M0Lm3e10S820 = _M0L1qS832 + _M0L6_2atmpS2339;
    _M0L6_2atmpS2345 = _M0Lm2e2S810;
    _M0L6_2atmpS2344 = -_M0L6_2atmpS2345;
    _M0L1iS833 = _M0L6_2atmpS2344 - _M0L1qS832;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2343 = _M0FPB8pow5bits(_M0L1iS833);
    _M0L1kS834 = _M0L6_2atmpS2343 - 125;
    _M0L1jS835 = _M0L1qS832 - _M0L1kS834;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S836 = _M0FPB19double__computePow5(_M0L1iS833);
    _M0L6_2atmpS2342 = _M0Lm2m2S811;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS837
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2342, _M0L4pow5S836, _M0L1jS835, _M0L7mmShiftS816);
    _M0L8_2avrOutS838 = _M0L7_2abindS837.$0;
    _M0L8_2avpOutS839 = _M0L7_2abindS837.$1;
    _M0L8_2avmOutS840 = _M0L7_2abindS837.$2;
    _M0Lm2vrS817 = _M0L8_2avrOutS838;
    _M0Lm2vpS818 = _M0L8_2avpOutS839;
    _M0Lm2vmS819 = _M0L8_2avmOutS840;
    if (_M0L1qS832 <= 1) {
      _M0Lm17vrIsTrailingZerosS822 = 1;
      if (_M0L4evenS814) {
        int32_t _M0L6_2atmpS2340;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2340 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS816);
        _M0Lm17vmIsTrailingZerosS821 = _M0L6_2atmpS2340 == 1;
      } else {
        uint64_t _M0L6_2atmpS2341 = _M0Lm2vpS818;
        _M0Lm2vpS818 = _M0L6_2atmpS2341 - 1ull;
      }
    } else if (_M0L1qS832 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS822
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS815, _M0L1qS832);
    }
  }
  _M0Lm7removedS841 = 0;
  _M0Lm16lastRemovedDigitS842 = 0;
  _M0Lm6outputS843 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS821 || _M0Lm17vrIsTrailingZerosS822) {
    int32_t _if__result_4292;
    uint64_t _M0L6_2atmpS2382;
    uint64_t _M0L6_2atmpS2388;
    uint64_t _M0L6_2atmpS2389;
    int32_t _if__result_4293;
    int32_t _M0L6_2atmpS2385;
    int64_t _M0L6_2atmpS2384;
    uint64_t _M0L6_2atmpS2383;
    while (1) {
      uint64_t _M0L6_2atmpS2365 = _M0Lm2vpS818;
      uint64_t _M0L7vpDiv10S844 = _M0L6_2atmpS2365 / 10ull;
      uint64_t _M0L6_2atmpS2364 = _M0Lm2vmS819;
      uint64_t _M0L7vmDiv10S845 = _M0L6_2atmpS2364 / 10ull;
      uint64_t _M0L6_2atmpS2363;
      int32_t _M0L6_2atmpS2360;
      int32_t _M0L6_2atmpS2362;
      int32_t _M0L6_2atmpS2361;
      int32_t _M0L7vmMod10S847;
      uint64_t _M0L6_2atmpS2359;
      uint64_t _M0L7vrDiv10S848;
      uint64_t _M0L6_2atmpS2358;
      int32_t _M0L6_2atmpS2355;
      int32_t _M0L6_2atmpS2357;
      int32_t _M0L6_2atmpS2356;
      int32_t _M0L7vrMod10S849;
      int32_t _M0L6_2atmpS2354;
      if (_M0L7vpDiv10S844 <= _M0L7vmDiv10S845) {
        break;
      }
      _M0L6_2atmpS2363 = _M0Lm2vmS819;
      _M0L6_2atmpS2360 = (int32_t)_M0L6_2atmpS2363;
      _M0L6_2atmpS2362 = (int32_t)_M0L7vmDiv10S845;
      _M0L6_2atmpS2361 = 10 * _M0L6_2atmpS2362;
      _M0L7vmMod10S847 = _M0L6_2atmpS2360 - _M0L6_2atmpS2361;
      _M0L6_2atmpS2359 = _M0Lm2vrS817;
      _M0L7vrDiv10S848 = _M0L6_2atmpS2359 / 10ull;
      _M0L6_2atmpS2358 = _M0Lm2vrS817;
      _M0L6_2atmpS2355 = (int32_t)_M0L6_2atmpS2358;
      _M0L6_2atmpS2357 = (int32_t)_M0L7vrDiv10S848;
      _M0L6_2atmpS2356 = 10 * _M0L6_2atmpS2357;
      _M0L7vrMod10S849 = _M0L6_2atmpS2355 - _M0L6_2atmpS2356;
      if (_M0Lm17vmIsTrailingZerosS821) {
        _M0Lm17vmIsTrailingZerosS821 = _M0L7vmMod10S847 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS821 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS822) {
        int32_t _M0L6_2atmpS2353 = _M0Lm16lastRemovedDigitS842;
        _M0Lm17vrIsTrailingZerosS822 = _M0L6_2atmpS2353 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS822 = 0;
      }
      _M0Lm16lastRemovedDigitS842 = _M0L7vrMod10S849;
      _M0Lm2vrS817 = _M0L7vrDiv10S848;
      _M0Lm2vpS818 = _M0L7vpDiv10S844;
      _M0Lm2vmS819 = _M0L7vmDiv10S845;
      _M0L6_2atmpS2354 = _M0Lm7removedS841;
      _M0Lm7removedS841 = _M0L6_2atmpS2354 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS821) {
      while (1) {
        uint64_t _M0L6_2atmpS2378 = _M0Lm2vmS819;
        uint64_t _M0L7vmDiv10S850 = _M0L6_2atmpS2378 / 10ull;
        uint64_t _M0L6_2atmpS2377 = _M0Lm2vmS819;
        int32_t _M0L6_2atmpS2374 = (int32_t)_M0L6_2atmpS2377;
        int32_t _M0L6_2atmpS2376 = (int32_t)_M0L7vmDiv10S850;
        int32_t _M0L6_2atmpS2375 = 10 * _M0L6_2atmpS2376;
        int32_t _M0L7vmMod10S851 = _M0L6_2atmpS2374 - _M0L6_2atmpS2375;
        uint64_t _M0L6_2atmpS2373;
        uint64_t _M0L7vpDiv10S853;
        uint64_t _M0L6_2atmpS2372;
        uint64_t _M0L7vrDiv10S854;
        uint64_t _M0L6_2atmpS2371;
        int32_t _M0L6_2atmpS2368;
        int32_t _M0L6_2atmpS2370;
        int32_t _M0L6_2atmpS2369;
        int32_t _M0L7vrMod10S855;
        int32_t _M0L6_2atmpS2367;
        if (_M0L7vmMod10S851 != 0) {
          break;
        }
        _M0L6_2atmpS2373 = _M0Lm2vpS818;
        _M0L7vpDiv10S853 = _M0L6_2atmpS2373 / 10ull;
        _M0L6_2atmpS2372 = _M0Lm2vrS817;
        _M0L7vrDiv10S854 = _M0L6_2atmpS2372 / 10ull;
        _M0L6_2atmpS2371 = _M0Lm2vrS817;
        _M0L6_2atmpS2368 = (int32_t)_M0L6_2atmpS2371;
        _M0L6_2atmpS2370 = (int32_t)_M0L7vrDiv10S854;
        _M0L6_2atmpS2369 = 10 * _M0L6_2atmpS2370;
        _M0L7vrMod10S855 = _M0L6_2atmpS2368 - _M0L6_2atmpS2369;
        if (_M0Lm17vrIsTrailingZerosS822) {
          int32_t _M0L6_2atmpS2366 = _M0Lm16lastRemovedDigitS842;
          _M0Lm17vrIsTrailingZerosS822 = _M0L6_2atmpS2366 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS822 = 0;
        }
        _M0Lm16lastRemovedDigitS842 = _M0L7vrMod10S855;
        _M0Lm2vrS817 = _M0L7vrDiv10S854;
        _M0Lm2vpS818 = _M0L7vpDiv10S853;
        _M0Lm2vmS819 = _M0L7vmDiv10S850;
        _M0L6_2atmpS2367 = _M0Lm7removedS841;
        _M0Lm7removedS841 = _M0L6_2atmpS2367 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS822) {
      int32_t _M0L6_2atmpS2381 = _M0Lm16lastRemovedDigitS842;
      if (_M0L6_2atmpS2381 == 5) {
        uint64_t _M0L6_2atmpS2380 = _M0Lm2vrS817;
        uint64_t _M0L6_2atmpS2379 = _M0L6_2atmpS2380 % 2ull;
        _if__result_4292 = _M0L6_2atmpS2379 == 0ull;
      } else {
        _if__result_4292 = 0;
      }
    } else {
      _if__result_4292 = 0;
    }
    if (_if__result_4292) {
      _M0Lm16lastRemovedDigitS842 = 4;
    }
    _M0L6_2atmpS2382 = _M0Lm2vrS817;
    _M0L6_2atmpS2388 = _M0Lm2vrS817;
    _M0L6_2atmpS2389 = _M0Lm2vmS819;
    if (_M0L6_2atmpS2388 == _M0L6_2atmpS2389) {
      if (!_M0L4evenS814) {
        _if__result_4293 = 1;
      } else {
        int32_t _M0L6_2atmpS2387 = _M0Lm17vmIsTrailingZerosS821;
        _if__result_4293 = !_M0L6_2atmpS2387;
      }
    } else {
      _if__result_4293 = 0;
    }
    if (_if__result_4293) {
      _M0L6_2atmpS2385 = 1;
    } else {
      int32_t _M0L6_2atmpS2386 = _M0Lm16lastRemovedDigitS842;
      _M0L6_2atmpS2385 = _M0L6_2atmpS2386 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2384 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2385);
    _M0L6_2atmpS2383 = *(uint64_t*)&_M0L6_2atmpS2384;
    _M0Lm6outputS843 = _M0L6_2atmpS2382 + _M0L6_2atmpS2383;
  } else {
    int32_t _M0Lm7roundUpS856 = 0;
    uint64_t _M0L6_2atmpS2410 = _M0Lm2vpS818;
    uint64_t _M0L8vpDiv100S857 = _M0L6_2atmpS2410 / 100ull;
    uint64_t _M0L6_2atmpS2409 = _M0Lm2vmS819;
    uint64_t _M0L8vmDiv100S858 = _M0L6_2atmpS2409 / 100ull;
    uint64_t _M0L6_2atmpS2404;
    uint64_t _M0L6_2atmpS2407;
    uint64_t _M0L6_2atmpS2408;
    int32_t _M0L6_2atmpS2406;
    uint64_t _M0L6_2atmpS2405;
    if (_M0L8vpDiv100S857 > _M0L8vmDiv100S858) {
      uint64_t _M0L6_2atmpS2395 = _M0Lm2vrS817;
      uint64_t _M0L8vrDiv100S859 = _M0L6_2atmpS2395 / 100ull;
      uint64_t _M0L6_2atmpS2394 = _M0Lm2vrS817;
      int32_t _M0L6_2atmpS2391 = (int32_t)_M0L6_2atmpS2394;
      int32_t _M0L6_2atmpS2393 = (int32_t)_M0L8vrDiv100S859;
      int32_t _M0L6_2atmpS2392 = 100 * _M0L6_2atmpS2393;
      int32_t _M0L8vrMod100S860 = _M0L6_2atmpS2391 - _M0L6_2atmpS2392;
      int32_t _M0L6_2atmpS2390;
      _M0Lm7roundUpS856 = _M0L8vrMod100S860 >= 50;
      _M0Lm2vrS817 = _M0L8vrDiv100S859;
      _M0Lm2vpS818 = _M0L8vpDiv100S857;
      _M0Lm2vmS819 = _M0L8vmDiv100S858;
      _M0L6_2atmpS2390 = _M0Lm7removedS841;
      _M0Lm7removedS841 = _M0L6_2atmpS2390 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2403 = _M0Lm2vpS818;
      uint64_t _M0L7vpDiv10S861 = _M0L6_2atmpS2403 / 10ull;
      uint64_t _M0L6_2atmpS2402 = _M0Lm2vmS819;
      uint64_t _M0L7vmDiv10S862 = _M0L6_2atmpS2402 / 10ull;
      uint64_t _M0L6_2atmpS2401;
      uint64_t _M0L7vrDiv10S864;
      uint64_t _M0L6_2atmpS2400;
      int32_t _M0L6_2atmpS2397;
      int32_t _M0L6_2atmpS2399;
      int32_t _M0L6_2atmpS2398;
      int32_t _M0L7vrMod10S865;
      int32_t _M0L6_2atmpS2396;
      if (_M0L7vpDiv10S861 <= _M0L7vmDiv10S862) {
        break;
      }
      _M0L6_2atmpS2401 = _M0Lm2vrS817;
      _M0L7vrDiv10S864 = _M0L6_2atmpS2401 / 10ull;
      _M0L6_2atmpS2400 = _M0Lm2vrS817;
      _M0L6_2atmpS2397 = (int32_t)_M0L6_2atmpS2400;
      _M0L6_2atmpS2399 = (int32_t)_M0L7vrDiv10S864;
      _M0L6_2atmpS2398 = 10 * _M0L6_2atmpS2399;
      _M0L7vrMod10S865 = _M0L6_2atmpS2397 - _M0L6_2atmpS2398;
      _M0Lm7roundUpS856 = _M0L7vrMod10S865 >= 5;
      _M0Lm2vrS817 = _M0L7vrDiv10S864;
      _M0Lm2vpS818 = _M0L7vpDiv10S861;
      _M0Lm2vmS819 = _M0L7vmDiv10S862;
      _M0L6_2atmpS2396 = _M0Lm7removedS841;
      _M0Lm7removedS841 = _M0L6_2atmpS2396 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2404 = _M0Lm2vrS817;
    _M0L6_2atmpS2407 = _M0Lm2vrS817;
    _M0L6_2atmpS2408 = _M0Lm2vmS819;
    _M0L6_2atmpS2406
    = _M0L6_2atmpS2407 == _M0L6_2atmpS2408 || _M0Lm7roundUpS856;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2405 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2406);
    _M0Lm6outputS843 = _M0L6_2atmpS2404 + _M0L6_2atmpS2405;
  }
  _M0L6_2atmpS2412 = _M0Lm3e10S820;
  _M0L6_2atmpS2413 = _M0Lm7removedS841;
  _M0L3expS866 = _M0L6_2atmpS2412 + _M0L6_2atmpS2413;
  _M0L6_2atmpS2411 = _M0Lm6outputS843;
  _block_4295
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4295)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4295->$0 = _M0L6_2atmpS2411;
  _block_4295->$1 = _M0L3expS866;
  return _block_4295;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS809) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS809) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS808) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS808) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS807) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS807) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS806) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS806 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS806 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS806 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS806 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS806 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS806 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS806 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS806 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS806 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS806 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS806 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS806 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS806 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS806 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS806 >= 100ull) {
    return 3;
  }
  if (_M0L1vS806 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS789) {
  int32_t _M0L6_2atmpS2312;
  int32_t _M0L6_2atmpS2311;
  int32_t _M0L4baseS788;
  int32_t _M0L5base2S790;
  int32_t _M0L6offsetS791;
  int32_t _M0L6_2atmpS2310;
  uint64_t _M0L4mul0S792;
  int32_t _M0L6_2atmpS2309;
  int32_t _M0L6_2atmpS2308;
  uint64_t _M0L4mul1S793;
  uint64_t _M0L1mS794;
  struct _M0TPB7Umul128 _M0L7_2abindS795;
  uint64_t _M0L7_2alow1S796;
  uint64_t _M0L8_2ahigh1S797;
  struct _M0TPB7Umul128 _M0L7_2abindS798;
  uint64_t _M0L7_2alow0S799;
  uint64_t _M0L8_2ahigh0S800;
  uint64_t _M0L3sumS801;
  uint64_t _M0Lm5high1S802;
  int32_t _M0L6_2atmpS2306;
  int32_t _M0L6_2atmpS2307;
  int32_t _M0L5deltaS803;
  uint64_t _M0L6_2atmpS2305;
  uint64_t _M0L6_2atmpS2297;
  int32_t _M0L6_2atmpS2304;
  uint32_t _M0L6_2atmpS2301;
  int32_t _M0L6_2atmpS2303;
  int32_t _M0L6_2atmpS2302;
  uint32_t _M0L6_2atmpS2300;
  uint32_t _M0L6_2atmpS2299;
  uint64_t _M0L6_2atmpS2298;
  uint64_t _M0L1aS804;
  uint64_t _M0L6_2atmpS2296;
  uint64_t _M0L1bS805;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2312 = _M0L1iS789 + 26;
  _M0L6_2atmpS2311 = _M0L6_2atmpS2312 - 1;
  _M0L4baseS788 = _M0L6_2atmpS2311 / 26;
  _M0L5base2S790 = _M0L4baseS788 * 26;
  _M0L6offsetS791 = _M0L5base2S790 - _M0L1iS789;
  _M0L6_2atmpS2310 = _M0L4baseS788 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S792
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2310);
  _M0L6_2atmpS2309 = _M0L4baseS788 * 2;
  _M0L6_2atmpS2308 = _M0L6_2atmpS2309 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S793
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2308);
  if (_M0L6offsetS791 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S792, _M0L4mul1S793};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS794
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS791);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS795 = _M0FPB7umul128(_M0L1mS794, _M0L4mul1S793);
  _M0L7_2alow1S796 = _M0L7_2abindS795.$0;
  _M0L8_2ahigh1S797 = _M0L7_2abindS795.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS798 = _M0FPB7umul128(_M0L1mS794, _M0L4mul0S792);
  _M0L7_2alow0S799 = _M0L7_2abindS798.$0;
  _M0L8_2ahigh0S800 = _M0L7_2abindS798.$1;
  _M0L3sumS801 = _M0L8_2ahigh0S800 + _M0L7_2alow1S796;
  _M0Lm5high1S802 = _M0L8_2ahigh1S797;
  if (_M0L3sumS801 < _M0L8_2ahigh0S800) {
    uint64_t _M0L6_2atmpS2295 = _M0Lm5high1S802;
    _M0Lm5high1S802 = _M0L6_2atmpS2295 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2306 = _M0FPB8pow5bits(_M0L5base2S790);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2307 = _M0FPB8pow5bits(_M0L1iS789);
  _M0L5deltaS803 = _M0L6_2atmpS2306 - _M0L6_2atmpS2307;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2305
  = _M0FPB13shiftright128(_M0L7_2alow0S799, _M0L3sumS801, _M0L5deltaS803);
  _M0L6_2atmpS2297 = _M0L6_2atmpS2305 + 1ull;
  _M0L6_2atmpS2304 = _M0L1iS789 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2301
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2304);
  _M0L6_2atmpS2303 = _M0L1iS789 % 16;
  _M0L6_2atmpS2302 = _M0L6_2atmpS2303 << 1;
  _M0L6_2atmpS2300 = _M0L6_2atmpS2301 >> (_M0L6_2atmpS2302 & 31);
  _M0L6_2atmpS2299 = _M0L6_2atmpS2300 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2298 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2299);
  _M0L1aS804 = _M0L6_2atmpS2297 + _M0L6_2atmpS2298;
  _M0L6_2atmpS2296 = _M0Lm5high1S802;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS805
  = _M0FPB13shiftright128(_M0L3sumS801, _M0L6_2atmpS2296, _M0L5deltaS803);
  return (struct _M0TPB8Pow5Pair){_M0L1aS804, _M0L1bS805};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS771) {
  int32_t _M0L4baseS770;
  int32_t _M0L5base2S772;
  int32_t _M0L6offsetS773;
  int32_t _M0L6_2atmpS2294;
  uint64_t _M0L4mul0S774;
  int32_t _M0L6_2atmpS2293;
  int32_t _M0L6_2atmpS2292;
  uint64_t _M0L4mul1S775;
  uint64_t _M0L1mS776;
  struct _M0TPB7Umul128 _M0L7_2abindS777;
  uint64_t _M0L7_2alow1S778;
  uint64_t _M0L8_2ahigh1S779;
  struct _M0TPB7Umul128 _M0L7_2abindS780;
  uint64_t _M0L7_2alow0S781;
  uint64_t _M0L8_2ahigh0S782;
  uint64_t _M0L3sumS783;
  uint64_t _M0Lm5high1S784;
  int32_t _M0L6_2atmpS2290;
  int32_t _M0L6_2atmpS2291;
  int32_t _M0L5deltaS785;
  uint64_t _M0L6_2atmpS2282;
  int32_t _M0L6_2atmpS2289;
  uint32_t _M0L6_2atmpS2286;
  int32_t _M0L6_2atmpS2288;
  int32_t _M0L6_2atmpS2287;
  uint32_t _M0L6_2atmpS2285;
  uint32_t _M0L6_2atmpS2284;
  uint64_t _M0L6_2atmpS2283;
  uint64_t _M0L1aS786;
  uint64_t _M0L6_2atmpS2281;
  uint64_t _M0L1bS787;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS770 = _M0L1iS771 / 26;
  _M0L5base2S772 = _M0L4baseS770 * 26;
  _M0L6offsetS773 = _M0L1iS771 - _M0L5base2S772;
  _M0L6_2atmpS2294 = _M0L4baseS770 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S774
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2294);
  _M0L6_2atmpS2293 = _M0L4baseS770 * 2;
  _M0L6_2atmpS2292 = _M0L6_2atmpS2293 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S775
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2292);
  if (_M0L6offsetS773 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S774, _M0L4mul1S775};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS776
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS773);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS777 = _M0FPB7umul128(_M0L1mS776, _M0L4mul1S775);
  _M0L7_2alow1S778 = _M0L7_2abindS777.$0;
  _M0L8_2ahigh1S779 = _M0L7_2abindS777.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS780 = _M0FPB7umul128(_M0L1mS776, _M0L4mul0S774);
  _M0L7_2alow0S781 = _M0L7_2abindS780.$0;
  _M0L8_2ahigh0S782 = _M0L7_2abindS780.$1;
  _M0L3sumS783 = _M0L8_2ahigh0S782 + _M0L7_2alow1S778;
  _M0Lm5high1S784 = _M0L8_2ahigh1S779;
  if (_M0L3sumS783 < _M0L8_2ahigh0S782) {
    uint64_t _M0L6_2atmpS2280 = _M0Lm5high1S784;
    _M0Lm5high1S784 = _M0L6_2atmpS2280 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2290 = _M0FPB8pow5bits(_M0L1iS771);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2291 = _M0FPB8pow5bits(_M0L5base2S772);
  _M0L5deltaS785 = _M0L6_2atmpS2290 - _M0L6_2atmpS2291;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2282
  = _M0FPB13shiftright128(_M0L7_2alow0S781, _M0L3sumS783, _M0L5deltaS785);
  _M0L6_2atmpS2289 = _M0L1iS771 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2286
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2289);
  _M0L6_2atmpS2288 = _M0L1iS771 % 16;
  _M0L6_2atmpS2287 = _M0L6_2atmpS2288 << 1;
  _M0L6_2atmpS2285 = _M0L6_2atmpS2286 >> (_M0L6_2atmpS2287 & 31);
  _M0L6_2atmpS2284 = _M0L6_2atmpS2285 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2283 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2284);
  _M0L1aS786 = _M0L6_2atmpS2282 + _M0L6_2atmpS2283;
  _M0L6_2atmpS2281 = _M0Lm5high1S784;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS787
  = _M0FPB13shiftright128(_M0L3sumS783, _M0L6_2atmpS2281, _M0L5deltaS785);
  return (struct _M0TPB8Pow5Pair){_M0L1aS786, _M0L1bS787};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS744,
  struct _M0TPB8Pow5Pair _M0L3mulS741,
  int32_t _M0L1jS757,
  int32_t _M0L7mmShiftS759
) {
  uint64_t _M0L7_2amul0S740;
  uint64_t _M0L7_2amul1S742;
  uint64_t _M0L1mS743;
  struct _M0TPB7Umul128 _M0L7_2abindS745;
  uint64_t _M0L5_2aloS746;
  uint64_t _M0L6_2atmpS747;
  struct _M0TPB7Umul128 _M0L7_2abindS748;
  uint64_t _M0L6_2alo2S749;
  uint64_t _M0L6_2ahi2S750;
  uint64_t _M0L3midS751;
  uint64_t _M0L6_2atmpS2279;
  uint64_t _M0L2hiS752;
  uint64_t _M0L3lo2S753;
  uint64_t _M0L6_2atmpS2277;
  uint64_t _M0L6_2atmpS2278;
  uint64_t _M0L4mid2S754;
  uint64_t _M0L6_2atmpS2276;
  uint64_t _M0L3hi2S755;
  int32_t _M0L6_2atmpS2275;
  int32_t _M0L6_2atmpS2274;
  uint64_t _M0L2vpS756;
  uint64_t _M0Lm2vmS758;
  int32_t _M0L6_2atmpS2273;
  int32_t _M0L6_2atmpS2272;
  uint64_t _M0L2vrS769;
  uint64_t _M0L6_2atmpS2271;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S740 = _M0L3mulS741.$0;
  _M0L7_2amul1S742 = _M0L3mulS741.$1;
  _M0L1mS743 = _M0L1mS744 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS745 = _M0FPB7umul128(_M0L1mS743, _M0L7_2amul0S740);
  _M0L5_2aloS746 = _M0L7_2abindS745.$0;
  _M0L6_2atmpS747 = _M0L7_2abindS745.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS748 = _M0FPB7umul128(_M0L1mS743, _M0L7_2amul1S742);
  _M0L6_2alo2S749 = _M0L7_2abindS748.$0;
  _M0L6_2ahi2S750 = _M0L7_2abindS748.$1;
  _M0L3midS751 = _M0L6_2atmpS747 + _M0L6_2alo2S749;
  if (_M0L3midS751 < _M0L6_2atmpS747) {
    _M0L6_2atmpS2279 = 1ull;
  } else {
    _M0L6_2atmpS2279 = 0ull;
  }
  _M0L2hiS752 = _M0L6_2ahi2S750 + _M0L6_2atmpS2279;
  _M0L3lo2S753 = _M0L5_2aloS746 + _M0L7_2amul0S740;
  _M0L6_2atmpS2277 = _M0L3midS751 + _M0L7_2amul1S742;
  if (_M0L3lo2S753 < _M0L5_2aloS746) {
    _M0L6_2atmpS2278 = 1ull;
  } else {
    _M0L6_2atmpS2278 = 0ull;
  }
  _M0L4mid2S754 = _M0L6_2atmpS2277 + _M0L6_2atmpS2278;
  if (_M0L4mid2S754 < _M0L3midS751) {
    _M0L6_2atmpS2276 = 1ull;
  } else {
    _M0L6_2atmpS2276 = 0ull;
  }
  _M0L3hi2S755 = _M0L2hiS752 + _M0L6_2atmpS2276;
  _M0L6_2atmpS2275 = _M0L1jS757 - 64;
  _M0L6_2atmpS2274 = _M0L6_2atmpS2275 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS756
  = _M0FPB13shiftright128(_M0L4mid2S754, _M0L3hi2S755, _M0L6_2atmpS2274);
  _M0Lm2vmS758 = 0ull;
  if (_M0L7mmShiftS759) {
    uint64_t _M0L3lo3S760 = _M0L5_2aloS746 - _M0L7_2amul0S740;
    uint64_t _M0L6_2atmpS2261 = _M0L3midS751 - _M0L7_2amul1S742;
    uint64_t _M0L6_2atmpS2262;
    uint64_t _M0L4mid3S761;
    uint64_t _M0L6_2atmpS2260;
    uint64_t _M0L3hi3S762;
    int32_t _M0L6_2atmpS2259;
    int32_t _M0L6_2atmpS2258;
    if (_M0L5_2aloS746 < _M0L3lo3S760) {
      _M0L6_2atmpS2262 = 1ull;
    } else {
      _M0L6_2atmpS2262 = 0ull;
    }
    _M0L4mid3S761 = _M0L6_2atmpS2261 - _M0L6_2atmpS2262;
    if (_M0L3midS751 < _M0L4mid3S761) {
      _M0L6_2atmpS2260 = 1ull;
    } else {
      _M0L6_2atmpS2260 = 0ull;
    }
    _M0L3hi3S762 = _M0L2hiS752 - _M0L6_2atmpS2260;
    _M0L6_2atmpS2259 = _M0L1jS757 - 64;
    _M0L6_2atmpS2258 = _M0L6_2atmpS2259 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS758
    = _M0FPB13shiftright128(_M0L4mid3S761, _M0L3hi3S762, _M0L6_2atmpS2258);
  } else {
    uint64_t _M0L3lo3S763 = _M0L5_2aloS746 + _M0L5_2aloS746;
    uint64_t _M0L6_2atmpS2269 = _M0L3midS751 + _M0L3midS751;
    uint64_t _M0L6_2atmpS2270;
    uint64_t _M0L4mid3S764;
    uint64_t _M0L6_2atmpS2267;
    uint64_t _M0L6_2atmpS2268;
    uint64_t _M0L3hi3S765;
    uint64_t _M0L3lo4S766;
    uint64_t _M0L6_2atmpS2265;
    uint64_t _M0L6_2atmpS2266;
    uint64_t _M0L4mid4S767;
    uint64_t _M0L6_2atmpS2264;
    uint64_t _M0L3hi4S768;
    int32_t _M0L6_2atmpS2263;
    if (_M0L3lo3S763 < _M0L5_2aloS746) {
      _M0L6_2atmpS2270 = 1ull;
    } else {
      _M0L6_2atmpS2270 = 0ull;
    }
    _M0L4mid3S764 = _M0L6_2atmpS2269 + _M0L6_2atmpS2270;
    _M0L6_2atmpS2267 = _M0L2hiS752 + _M0L2hiS752;
    if (_M0L4mid3S764 < _M0L3midS751) {
      _M0L6_2atmpS2268 = 1ull;
    } else {
      _M0L6_2atmpS2268 = 0ull;
    }
    _M0L3hi3S765 = _M0L6_2atmpS2267 + _M0L6_2atmpS2268;
    _M0L3lo4S766 = _M0L3lo3S763 - _M0L7_2amul0S740;
    _M0L6_2atmpS2265 = _M0L4mid3S764 - _M0L7_2amul1S742;
    if (_M0L3lo3S763 < _M0L3lo4S766) {
      _M0L6_2atmpS2266 = 1ull;
    } else {
      _M0L6_2atmpS2266 = 0ull;
    }
    _M0L4mid4S767 = _M0L6_2atmpS2265 - _M0L6_2atmpS2266;
    if (_M0L4mid3S764 < _M0L4mid4S767) {
      _M0L6_2atmpS2264 = 1ull;
    } else {
      _M0L6_2atmpS2264 = 0ull;
    }
    _M0L3hi4S768 = _M0L3hi3S765 - _M0L6_2atmpS2264;
    _M0L6_2atmpS2263 = _M0L1jS757 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS758
    = _M0FPB13shiftright128(_M0L4mid4S767, _M0L3hi4S768, _M0L6_2atmpS2263);
  }
  _M0L6_2atmpS2273 = _M0L1jS757 - 64;
  _M0L6_2atmpS2272 = _M0L6_2atmpS2273 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS769
  = _M0FPB13shiftright128(_M0L3midS751, _M0L2hiS752, _M0L6_2atmpS2272);
  _M0L6_2atmpS2271 = _M0Lm2vmS758;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS769,
                                                _M0L2vpS756,
                                                _M0L6_2atmpS2271};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS738,
  int32_t _M0L1pS739
) {
  uint64_t _M0L6_2atmpS2257;
  uint64_t _M0L6_2atmpS2256;
  uint64_t _M0L6_2atmpS2255;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2257 = 1ull << (_M0L1pS739 & 63);
  _M0L6_2atmpS2256 = _M0L6_2atmpS2257 - 1ull;
  _M0L6_2atmpS2255 = _M0L5valueS738 & _M0L6_2atmpS2256;
  return _M0L6_2atmpS2255 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS736,
  int32_t _M0L1pS737
) {
  int32_t _M0L6_2atmpS2254;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2254 = _M0FPB10pow5Factor(_M0L5valueS736);
  return _M0L6_2atmpS2254 >= _M0L1pS737;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS732) {
  uint64_t _M0L6_2atmpS2242;
  uint64_t _M0L6_2atmpS2243;
  uint64_t _M0L6_2atmpS2244;
  uint64_t _M0L6_2atmpS2245;
  int32_t _M0Lm5countS733;
  uint64_t _M0Lm5valueS734;
  uint64_t _M0L6_2atmpS2253;
  moonbit_string_t _M0L6_2atmpS2252;
  moonbit_string_t _M0L6_2atmpS3963;
  moonbit_string_t _M0L6_2atmpS2251;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2242 = _M0L5valueS732 % 5ull;
  if (_M0L6_2atmpS2242 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2243 = _M0L5valueS732 % 25ull;
  if (_M0L6_2atmpS2243 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2244 = _M0L5valueS732 % 125ull;
  if (_M0L6_2atmpS2244 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2245 = _M0L5valueS732 % 625ull;
  if (_M0L6_2atmpS2245 != 0ull) {
    return 3;
  }
  _M0Lm5countS733 = 4;
  _M0Lm5valueS734 = _M0L5valueS732 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2246 = _M0Lm5valueS734;
    if (_M0L6_2atmpS2246 > 0ull) {
      uint64_t _M0L6_2atmpS2248 = _M0Lm5valueS734;
      uint64_t _M0L6_2atmpS2247 = _M0L6_2atmpS2248 % 5ull;
      uint64_t _M0L6_2atmpS2249;
      int32_t _M0L6_2atmpS2250;
      if (_M0L6_2atmpS2247 != 0ull) {
        return _M0Lm5countS733;
      }
      _M0L6_2atmpS2249 = _M0Lm5valueS734;
      _M0Lm5valueS734 = _M0L6_2atmpS2249 / 5ull;
      _M0L6_2atmpS2250 = _M0Lm5countS733;
      _M0Lm5countS733 = _M0L6_2atmpS2250 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2253 = _M0Lm5valueS734;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2252
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2253);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3963
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS2252);
  moonbit_decref(_M0L6_2atmpS2252);
  _M0L6_2atmpS2251 = _M0L6_2atmpS3963;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2251, (moonbit_string_t)moonbit_string_literal_46.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS731,
  uint64_t _M0L2hiS729,
  int32_t _M0L4distS730
) {
  int32_t _M0L6_2atmpS2241;
  uint64_t _M0L6_2atmpS2239;
  uint64_t _M0L6_2atmpS2240;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2241 = 64 - _M0L4distS730;
  _M0L6_2atmpS2239 = _M0L2hiS729 << (_M0L6_2atmpS2241 & 63);
  _M0L6_2atmpS2240 = _M0L2loS731 >> (_M0L4distS730 & 63);
  return _M0L6_2atmpS2239 | _M0L6_2atmpS2240;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS719,
  uint64_t _M0L1bS722
) {
  uint64_t _M0L3aLoS718;
  uint64_t _M0L3aHiS720;
  uint64_t _M0L3bLoS721;
  uint64_t _M0L3bHiS723;
  uint64_t _M0L1xS724;
  uint64_t _M0L6_2atmpS2237;
  uint64_t _M0L6_2atmpS2238;
  uint64_t _M0L1yS725;
  uint64_t _M0L6_2atmpS2235;
  uint64_t _M0L6_2atmpS2236;
  uint64_t _M0L1zS726;
  uint64_t _M0L6_2atmpS2233;
  uint64_t _M0L6_2atmpS2234;
  uint64_t _M0L6_2atmpS2231;
  uint64_t _M0L6_2atmpS2232;
  uint64_t _M0L1wS727;
  uint64_t _M0L2loS728;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS718 = _M0L1aS719 & 4294967295ull;
  _M0L3aHiS720 = _M0L1aS719 >> 32;
  _M0L3bLoS721 = _M0L1bS722 & 4294967295ull;
  _M0L3bHiS723 = _M0L1bS722 >> 32;
  _M0L1xS724 = _M0L3aLoS718 * _M0L3bLoS721;
  _M0L6_2atmpS2237 = _M0L3aHiS720 * _M0L3bLoS721;
  _M0L6_2atmpS2238 = _M0L1xS724 >> 32;
  _M0L1yS725 = _M0L6_2atmpS2237 + _M0L6_2atmpS2238;
  _M0L6_2atmpS2235 = _M0L3aLoS718 * _M0L3bHiS723;
  _M0L6_2atmpS2236 = _M0L1yS725 & 4294967295ull;
  _M0L1zS726 = _M0L6_2atmpS2235 + _M0L6_2atmpS2236;
  _M0L6_2atmpS2233 = _M0L3aHiS720 * _M0L3bHiS723;
  _M0L6_2atmpS2234 = _M0L1yS725 >> 32;
  _M0L6_2atmpS2231 = _M0L6_2atmpS2233 + _M0L6_2atmpS2234;
  _M0L6_2atmpS2232 = _M0L1zS726 >> 32;
  _M0L1wS727 = _M0L6_2atmpS2231 + _M0L6_2atmpS2232;
  _M0L2loS728 = _M0L1aS719 * _M0L1bS722;
  return (struct _M0TPB7Umul128){_M0L2loS728, _M0L1wS727};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS713,
  int32_t _M0L4fromS717,
  int32_t _M0L2toS715
) {
  int32_t _M0L6_2atmpS2230;
  struct _M0TPB13StringBuilder* _M0L3bufS712;
  int32_t _M0L1iS714;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2230 = Moonbit_array_length(_M0L5bytesS713);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS712 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2230);
  _M0L1iS714 = _M0L4fromS717;
  while (1) {
    if (_M0L1iS714 < _M0L2toS715) {
      int32_t _M0L6_2atmpS2228;
      int32_t _M0L6_2atmpS2227;
      int32_t _M0L6_2atmpS2229;
      if (
        _M0L1iS714 < 0 || _M0L1iS714 >= Moonbit_array_length(_M0L5bytesS713)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2228 = (int32_t)_M0L5bytesS713[_M0L1iS714];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2227 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2228);
      moonbit_incref(_M0L3bufS712);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS712, _M0L6_2atmpS2227);
      _M0L6_2atmpS2229 = _M0L1iS714 + 1;
      _M0L1iS714 = _M0L6_2atmpS2229;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS713);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS712);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS711) {
  int32_t _M0L6_2atmpS2226;
  uint32_t _M0L6_2atmpS2225;
  uint32_t _M0L6_2atmpS2224;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2226 = _M0L1eS711 * 78913;
  _M0L6_2atmpS2225 = *(uint32_t*)&_M0L6_2atmpS2226;
  _M0L6_2atmpS2224 = _M0L6_2atmpS2225 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2224;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS710) {
  int32_t _M0L6_2atmpS2223;
  uint32_t _M0L6_2atmpS2222;
  uint32_t _M0L6_2atmpS2221;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2223 = _M0L1eS710 * 732923;
  _M0L6_2atmpS2222 = *(uint32_t*)&_M0L6_2atmpS2223;
  _M0L6_2atmpS2221 = _M0L6_2atmpS2222 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2221;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS708,
  int32_t _M0L8exponentS709,
  int32_t _M0L8mantissaS706
) {
  moonbit_string_t _M0L1sS707;
  moonbit_string_t _M0L6_2atmpS3964;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS706) {
    return (moonbit_string_t)moonbit_string_literal_47.data;
  }
  if (_M0L4signS708) {
    _M0L1sS707 = (moonbit_string_t)moonbit_string_literal_48.data;
  } else {
    _M0L1sS707 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS709) {
    moonbit_string_t _M0L6_2atmpS3965;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3965
    = moonbit_add_string(_M0L1sS707, (moonbit_string_t)moonbit_string_literal_49.data);
    moonbit_decref(_M0L1sS707);
    return _M0L6_2atmpS3965;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3964
  = moonbit_add_string(_M0L1sS707, (moonbit_string_t)moonbit_string_literal_50.data);
  moonbit_decref(_M0L1sS707);
  return _M0L6_2atmpS3964;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS705) {
  int32_t _M0L6_2atmpS2220;
  uint32_t _M0L6_2atmpS2219;
  uint32_t _M0L6_2atmpS2218;
  int32_t _M0L6_2atmpS2217;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2220 = _M0L1eS705 * 1217359;
  _M0L6_2atmpS2219 = *(uint32_t*)&_M0L6_2atmpS2220;
  _M0L6_2atmpS2218 = _M0L6_2atmpS2219 >> 19;
  _M0L6_2atmpS2217 = *(int32_t*)&_M0L6_2atmpS2218;
  return _M0L6_2atmpS2217 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS704,
  struct _M0TPB6Hasher* _M0L6hasherS703
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS703, _M0L4selfS704);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS702,
  struct _M0TPB6Hasher* _M0L6hasherS701
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS701, _M0L4selfS702);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS699,
  moonbit_string_t _M0L5valueS697
) {
  int32_t _M0L7_2abindS696;
  int32_t _M0L1iS698;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS696 = Moonbit_array_length(_M0L5valueS697);
  _M0L1iS698 = 0;
  while (1) {
    if (_M0L1iS698 < _M0L7_2abindS696) {
      int32_t _M0L6_2atmpS2215 = _M0L5valueS697[_M0L1iS698];
      int32_t _M0L6_2atmpS2214 = (int32_t)_M0L6_2atmpS2215;
      uint32_t _M0L6_2atmpS2213 = *(uint32_t*)&_M0L6_2atmpS2214;
      int32_t _M0L6_2atmpS2216;
      moonbit_incref(_M0L4selfS699);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS699, _M0L6_2atmpS2213);
      _M0L6_2atmpS2216 = _M0L1iS698 + 1;
      _M0L1iS698 = _M0L6_2atmpS2216;
      continue;
    } else {
      moonbit_decref(_M0L4selfS699);
      moonbit_decref(_M0L5valueS697);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS694,
  int32_t _M0L3idxS695
) {
  int32_t _M0L6_2atmpS3966;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3966 = _M0L4selfS694[_M0L3idxS695];
  moonbit_decref(_M0L4selfS694);
  return _M0L6_2atmpS3966;
}

moonbit_bytes_t _M0MPC15bytes5Bytes4make(
  int32_t _M0L3lenS692,
  int32_t _M0L4initS693
) {
  #line 1486 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  if (_M0L3lenS692 < 0) {
    return (moonbit_bytes_t)moonbit_bytes_literal_0.data;
  }
  #line 1490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return moonbit_make_bytes(_M0L3lenS692, _M0L4initS693);
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS691) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS691;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS690) {
  double _M0L6_2atmpS2211;
  moonbit_string_t _M0L6_2atmpS2212;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2211 = (double)_M0L4selfS690;
  _M0L6_2atmpS2212 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2211, _M0L6_2atmpS2212);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS683
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3967;
  int32_t _M0L6_2acntS4173;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2210;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS682;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__* _closure_4299;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2205;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3967 = _M0L4selfS683->$5;
  _M0L6_2acntS4173 = Moonbit_object_header(_M0L4selfS683)->rc;
  if (_M0L6_2acntS4173 > 1) {
    int32_t _M0L11_2anew__cntS4175 = _M0L6_2acntS4173 - 1;
    Moonbit_object_header(_M0L4selfS683)->rc = _M0L11_2anew__cntS4175;
    if (_M0L8_2afieldS3967) {
      moonbit_incref(_M0L8_2afieldS3967);
    }
  } else if (_M0L6_2acntS4173 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4174 = _M0L4selfS683->$0;
    moonbit_decref(_M0L8_2afieldS4174);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS683);
  }
  _M0L4headS2210 = _M0L8_2afieldS3967;
  _M0L11curr__entryS682
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS682)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS682->$0 = _M0L4headS2210;
  _closure_4299
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__));
  Moonbit_object_header(_closure_4299)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__, $0) >> 2, 1, 0);
  _closure_4299->code = &_M0MPB3Map4iterGsRPB4JsonEC2206l591;
  _closure_4299->$0 = _M0L11curr__entryS682;
  _M0L6_2atmpS2205 = (struct _M0TWEOUsRPB4JsonE*)_closure_4299;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2205);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2206l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2207
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__* _M0L14_2acasted__envS2208;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3973;
  int32_t _M0L6_2acntS4176;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS682;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3972;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS684;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2208
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2206__l591__*)_M0L6_2aenvS2207;
  _M0L8_2afieldS3973 = _M0L14_2acasted__envS2208->$0;
  _M0L6_2acntS4176 = Moonbit_object_header(_M0L14_2acasted__envS2208)->rc;
  if (_M0L6_2acntS4176 > 1) {
    int32_t _M0L11_2anew__cntS4177 = _M0L6_2acntS4176 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2208)->rc
    = _M0L11_2anew__cntS4177;
    moonbit_incref(_M0L8_2afieldS3973);
  } else if (_M0L6_2acntS4176 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2208);
  }
  _M0L11curr__entryS682 = _M0L8_2afieldS3973;
  _M0L8_2afieldS3972 = _M0L11curr__entryS682->$0;
  _M0L7_2abindS684 = _M0L8_2afieldS3972;
  if (_M0L7_2abindS684 == 0) {
    moonbit_decref(_M0L11curr__entryS682);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS685 = _M0L7_2abindS684;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS686 = _M0L7_2aSomeS685;
    moonbit_string_t _M0L8_2afieldS3971 = _M0L4_2axS686->$4;
    moonbit_string_t _M0L6_2akeyS687 = _M0L8_2afieldS3971;
    void* _M0L8_2afieldS3970 = _M0L4_2axS686->$5;
    void* _M0L8_2avalueS688 = _M0L8_2afieldS3970;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3969 = _M0L4_2axS686->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS689 = _M0L8_2afieldS3969;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3968 =
      _M0L11curr__entryS682->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2209;
    if (_M0L7_2anextS689) {
      moonbit_incref(_M0L7_2anextS689);
    }
    moonbit_incref(_M0L8_2avalueS688);
    moonbit_incref(_M0L6_2akeyS687);
    if (_M0L6_2aoldS3968) {
      moonbit_decref(_M0L6_2aoldS3968);
    }
    _M0L11curr__entryS682->$0 = _M0L7_2anextS689;
    moonbit_decref(_M0L11curr__entryS682);
    _M0L8_2atupleS2209
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2209)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2209->$0 = _M0L6_2akeyS687;
    _M0L8_2atupleS2209->$1 = _M0L8_2avalueS688;
    return _M0L8_2atupleS2209;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS681
) {
  int32_t _M0L8_2afieldS3974;
  int32_t _M0L4sizeS2204;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3974 = _M0L4selfS681->$1;
  moonbit_decref(_M0L4selfS681);
  _M0L4sizeS2204 = _M0L8_2afieldS3974;
  return _M0L4sizeS2204 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS668,
  int32_t _M0L3keyS664
) {
  int32_t _M0L4hashS663;
  int32_t _M0L14capacity__maskS2189;
  int32_t _M0L6_2atmpS2188;
  int32_t _M0L1iS665;
  int32_t _M0L3idxS666;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS663 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS664);
  _M0L14capacity__maskS2189 = _M0L4selfS668->$3;
  _M0L6_2atmpS2188 = _M0L4hashS663 & _M0L14capacity__maskS2189;
  _M0L1iS665 = 0;
  _M0L3idxS666 = _M0L6_2atmpS2188;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3978 =
      _M0L4selfS668->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2187 =
      _M0L8_2afieldS3978;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3977;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS667;
    if (
      _M0L3idxS666 < 0
      || _M0L3idxS666 >= Moonbit_array_length(_M0L7entriesS2187)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3977
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2187[
        _M0L3idxS666
      ];
    _M0L7_2abindS667 = _M0L6_2atmpS3977;
    if (_M0L7_2abindS667 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2176;
      if (_M0L7_2abindS667) {
        moonbit_incref(_M0L7_2abindS667);
      }
      moonbit_decref(_M0L4selfS668);
      if (_M0L7_2abindS667) {
        moonbit_decref(_M0L7_2abindS667);
      }
      _M0L6_2atmpS2176 = 0;
      return _M0L6_2atmpS2176;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS669 =
        _M0L7_2abindS667;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS670 =
        _M0L7_2aSomeS669;
      int32_t _M0L4hashS2178 = _M0L8_2aentryS670->$3;
      int32_t _if__result_4301;
      int32_t _M0L8_2afieldS3975;
      int32_t _M0L3pslS2181;
      int32_t _M0L6_2atmpS2183;
      int32_t _M0L6_2atmpS2185;
      int32_t _M0L14capacity__maskS2186;
      int32_t _M0L6_2atmpS2184;
      if (_M0L4hashS2178 == _M0L4hashS663) {
        int32_t _M0L3keyS2177 = _M0L8_2aentryS670->$4;
        _if__result_4301 = _M0L3keyS2177 == _M0L3keyS664;
      } else {
        _if__result_4301 = 0;
      }
      if (_if__result_4301) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3976;
        int32_t _M0L6_2acntS4178;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2180;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2179;
        moonbit_incref(_M0L8_2aentryS670);
        moonbit_decref(_M0L4selfS668);
        _M0L8_2afieldS3976 = _M0L8_2aentryS670->$5;
        _M0L6_2acntS4178 = Moonbit_object_header(_M0L8_2aentryS670)->rc;
        if (_M0L6_2acntS4178 > 1) {
          int32_t _M0L11_2anew__cntS4180 = _M0L6_2acntS4178 - 1;
          Moonbit_object_header(_M0L8_2aentryS670)->rc
          = _M0L11_2anew__cntS4180;
          moonbit_incref(_M0L8_2afieldS3976);
        } else if (_M0L6_2acntS4178 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4179 =
            _M0L8_2aentryS670->$1;
          if (_M0L8_2afieldS4179) {
            moonbit_decref(_M0L8_2afieldS4179);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS670);
        }
        _M0L5valueS2180 = _M0L8_2afieldS3976;
        _M0L6_2atmpS2179 = _M0L5valueS2180;
        return _M0L6_2atmpS2179;
      } else {
        moonbit_incref(_M0L8_2aentryS670);
      }
      _M0L8_2afieldS3975 = _M0L8_2aentryS670->$2;
      moonbit_decref(_M0L8_2aentryS670);
      _M0L3pslS2181 = _M0L8_2afieldS3975;
      if (_M0L1iS665 > _M0L3pslS2181) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2182;
        moonbit_decref(_M0L4selfS668);
        _M0L6_2atmpS2182 = 0;
        return _M0L6_2atmpS2182;
      }
      _M0L6_2atmpS2183 = _M0L1iS665 + 1;
      _M0L6_2atmpS2185 = _M0L3idxS666 + 1;
      _M0L14capacity__maskS2186 = _M0L4selfS668->$3;
      _M0L6_2atmpS2184 = _M0L6_2atmpS2185 & _M0L14capacity__maskS2186;
      _M0L1iS665 = _M0L6_2atmpS2183;
      _M0L3idxS666 = _M0L6_2atmpS2184;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS677,
  moonbit_string_t _M0L3keyS673
) {
  int32_t _M0L4hashS672;
  int32_t _M0L14capacity__maskS2203;
  int32_t _M0L6_2atmpS2202;
  int32_t _M0L1iS674;
  int32_t _M0L3idxS675;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS673);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS672 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS673);
  _M0L14capacity__maskS2203 = _M0L4selfS677->$3;
  _M0L6_2atmpS2202 = _M0L4hashS672 & _M0L14capacity__maskS2203;
  _M0L1iS674 = 0;
  _M0L3idxS675 = _M0L6_2atmpS2202;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3984 =
      _M0L4selfS677->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2201 =
      _M0L8_2afieldS3984;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3983;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS676;
    if (
      _M0L3idxS675 < 0
      || _M0L3idxS675 >= Moonbit_array_length(_M0L7entriesS2201)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3983
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2201[
        _M0L3idxS675
      ];
    _M0L7_2abindS676 = _M0L6_2atmpS3983;
    if (_M0L7_2abindS676 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2190;
      if (_M0L7_2abindS676) {
        moonbit_incref(_M0L7_2abindS676);
      }
      moonbit_decref(_M0L4selfS677);
      if (_M0L7_2abindS676) {
        moonbit_decref(_M0L7_2abindS676);
      }
      moonbit_decref(_M0L3keyS673);
      _M0L6_2atmpS2190 = 0;
      return _M0L6_2atmpS2190;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS678 =
        _M0L7_2abindS676;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS679 =
        _M0L7_2aSomeS678;
      int32_t _M0L4hashS2192 = _M0L8_2aentryS679->$3;
      int32_t _if__result_4303;
      int32_t _M0L8_2afieldS3979;
      int32_t _M0L3pslS2195;
      int32_t _M0L6_2atmpS2197;
      int32_t _M0L6_2atmpS2199;
      int32_t _M0L14capacity__maskS2200;
      int32_t _M0L6_2atmpS2198;
      if (_M0L4hashS2192 == _M0L4hashS672) {
        moonbit_string_t _M0L8_2afieldS3982 = _M0L8_2aentryS679->$4;
        moonbit_string_t _M0L3keyS2191 = _M0L8_2afieldS3982;
        int32_t _M0L6_2atmpS3981;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3981
        = moonbit_val_array_equal(_M0L3keyS2191, _M0L3keyS673);
        _if__result_4303 = _M0L6_2atmpS3981;
      } else {
        _if__result_4303 = 0;
      }
      if (_if__result_4303) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3980;
        int32_t _M0L6_2acntS4181;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2194;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2193;
        moonbit_incref(_M0L8_2aentryS679);
        moonbit_decref(_M0L4selfS677);
        moonbit_decref(_M0L3keyS673);
        _M0L8_2afieldS3980 = _M0L8_2aentryS679->$5;
        _M0L6_2acntS4181 = Moonbit_object_header(_M0L8_2aentryS679)->rc;
        if (_M0L6_2acntS4181 > 1) {
          int32_t _M0L11_2anew__cntS4184 = _M0L6_2acntS4181 - 1;
          Moonbit_object_header(_M0L8_2aentryS679)->rc
          = _M0L11_2anew__cntS4184;
          moonbit_incref(_M0L8_2afieldS3980);
        } else if (_M0L6_2acntS4181 == 1) {
          moonbit_string_t _M0L8_2afieldS4183 = _M0L8_2aentryS679->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4182;
          moonbit_decref(_M0L8_2afieldS4183);
          _M0L8_2afieldS4182 = _M0L8_2aentryS679->$1;
          if (_M0L8_2afieldS4182) {
            moonbit_decref(_M0L8_2afieldS4182);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS679);
        }
        _M0L5valueS2194 = _M0L8_2afieldS3980;
        _M0L6_2atmpS2193 = _M0L5valueS2194;
        return _M0L6_2atmpS2193;
      } else {
        moonbit_incref(_M0L8_2aentryS679);
      }
      _M0L8_2afieldS3979 = _M0L8_2aentryS679->$2;
      moonbit_decref(_M0L8_2aentryS679);
      _M0L3pslS2195 = _M0L8_2afieldS3979;
      if (_M0L1iS674 > _M0L3pslS2195) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2196;
        moonbit_decref(_M0L4selfS677);
        moonbit_decref(_M0L3keyS673);
        _M0L6_2atmpS2196 = 0;
        return _M0L6_2atmpS2196;
      }
      _M0L6_2atmpS2197 = _M0L1iS674 + 1;
      _M0L6_2atmpS2199 = _M0L3idxS675 + 1;
      _M0L14capacity__maskS2200 = _M0L4selfS677->$3;
      _M0L6_2atmpS2198 = _M0L6_2atmpS2199 & _M0L14capacity__maskS2200;
      _M0L1iS674 = _M0L6_2atmpS2197;
      _M0L3idxS675 = _M0L6_2atmpS2198;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS648
) {
  int32_t _M0L6lengthS647;
  int32_t _M0Lm8capacityS649;
  int32_t _M0L6_2atmpS2153;
  int32_t _M0L6_2atmpS2152;
  int32_t _M0L6_2atmpS2163;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS650;
  int32_t _M0L3endS2161;
  int32_t _M0L5startS2162;
  int32_t _M0L7_2abindS651;
  int32_t _M0L2__S652;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS648.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS647
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS648);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS649 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS647);
  _M0L6_2atmpS2153 = _M0Lm8capacityS649;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2152 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2153);
  if (_M0L6lengthS647 > _M0L6_2atmpS2152) {
    int32_t _M0L6_2atmpS2154 = _M0Lm8capacityS649;
    _M0Lm8capacityS649 = _M0L6_2atmpS2154 * 2;
  }
  _M0L6_2atmpS2163 = _M0Lm8capacityS649;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS650
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2163);
  _M0L3endS2161 = _M0L3arrS648.$2;
  _M0L5startS2162 = _M0L3arrS648.$1;
  _M0L7_2abindS651 = _M0L3endS2161 - _M0L5startS2162;
  _M0L2__S652 = 0;
  while (1) {
    if (_M0L2__S652 < _M0L7_2abindS651) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3988 =
        _M0L3arrS648.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2158 =
        _M0L8_2afieldS3988;
      int32_t _M0L5startS2160 = _M0L3arrS648.$1;
      int32_t _M0L6_2atmpS2159 = _M0L5startS2160 + _M0L2__S652;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3987 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2158[
          _M0L6_2atmpS2159
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS653 =
        _M0L6_2atmpS3987;
      moonbit_string_t _M0L8_2afieldS3986 = _M0L1eS653->$0;
      moonbit_string_t _M0L6_2atmpS2155 = _M0L8_2afieldS3986;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3985 =
        _M0L1eS653->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2156 =
        _M0L8_2afieldS3985;
      int32_t _M0L6_2atmpS2157;
      moonbit_incref(_M0L6_2atmpS2156);
      moonbit_incref(_M0L6_2atmpS2155);
      moonbit_incref(_M0L1mS650);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS650, _M0L6_2atmpS2155, _M0L6_2atmpS2156);
      _M0L6_2atmpS2157 = _M0L2__S652 + 1;
      _M0L2__S652 = _M0L6_2atmpS2157;
      continue;
    } else {
      moonbit_decref(_M0L3arrS648.$0);
    }
    break;
  }
  return _M0L1mS650;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS656
) {
  int32_t _M0L6lengthS655;
  int32_t _M0Lm8capacityS657;
  int32_t _M0L6_2atmpS2165;
  int32_t _M0L6_2atmpS2164;
  int32_t _M0L6_2atmpS2175;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS658;
  int32_t _M0L3endS2173;
  int32_t _M0L5startS2174;
  int32_t _M0L7_2abindS659;
  int32_t _M0L2__S660;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS656.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS655
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS656);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS657 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS655);
  _M0L6_2atmpS2165 = _M0Lm8capacityS657;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2164 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2165);
  if (_M0L6lengthS655 > _M0L6_2atmpS2164) {
    int32_t _M0L6_2atmpS2166 = _M0Lm8capacityS657;
    _M0Lm8capacityS657 = _M0L6_2atmpS2166 * 2;
  }
  _M0L6_2atmpS2175 = _M0Lm8capacityS657;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS658
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2175);
  _M0L3endS2173 = _M0L3arrS656.$2;
  _M0L5startS2174 = _M0L3arrS656.$1;
  _M0L7_2abindS659 = _M0L3endS2173 - _M0L5startS2174;
  _M0L2__S660 = 0;
  while (1) {
    if (_M0L2__S660 < _M0L7_2abindS659) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3991 =
        _M0L3arrS656.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2170 =
        _M0L8_2afieldS3991;
      int32_t _M0L5startS2172 = _M0L3arrS656.$1;
      int32_t _M0L6_2atmpS2171 = _M0L5startS2172 + _M0L2__S660;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3990 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2170[
          _M0L6_2atmpS2171
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS661 = _M0L6_2atmpS3990;
      int32_t _M0L6_2atmpS2167 = _M0L1eS661->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3989 =
        _M0L1eS661->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2168 =
        _M0L8_2afieldS3989;
      int32_t _M0L6_2atmpS2169;
      moonbit_incref(_M0L6_2atmpS2168);
      moonbit_incref(_M0L1mS658);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS658, _M0L6_2atmpS2167, _M0L6_2atmpS2168);
      _M0L6_2atmpS2169 = _M0L2__S660 + 1;
      _M0L2__S660 = _M0L6_2atmpS2169;
      continue;
    } else {
      moonbit_decref(_M0L3arrS656.$0);
    }
    break;
  }
  return _M0L1mS658;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS641,
  moonbit_string_t _M0L3keyS642,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS643
) {
  int32_t _M0L6_2atmpS2150;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS642);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2150 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS642);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641, _M0L3keyS642, _M0L5valueS643, _M0L6_2atmpS2150);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS644,
  int32_t _M0L3keyS645,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS646
) {
  int32_t _M0L6_2atmpS2151;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2151 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS645);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS644, _M0L3keyS645, _M0L5valueS646, _M0L6_2atmpS2151);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS620
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3998;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS619;
  int32_t _M0L8capacityS2142;
  int32_t _M0L13new__capacityS621;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2137;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2136;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3997;
  int32_t _M0L6_2atmpS2138;
  int32_t _M0L8capacityS2140;
  int32_t _M0L6_2atmpS2139;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2141;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3996;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS622;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3998 = _M0L4selfS620->$5;
  _M0L9old__headS619 = _M0L8_2afieldS3998;
  _M0L8capacityS2142 = _M0L4selfS620->$2;
  _M0L13new__capacityS621 = _M0L8capacityS2142 << 1;
  _M0L6_2atmpS2137 = 0;
  _M0L6_2atmpS2136
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS621, _M0L6_2atmpS2137);
  _M0L6_2aoldS3997 = _M0L4selfS620->$0;
  if (_M0L9old__headS619) {
    moonbit_incref(_M0L9old__headS619);
  }
  moonbit_decref(_M0L6_2aoldS3997);
  _M0L4selfS620->$0 = _M0L6_2atmpS2136;
  _M0L4selfS620->$2 = _M0L13new__capacityS621;
  _M0L6_2atmpS2138 = _M0L13new__capacityS621 - 1;
  _M0L4selfS620->$3 = _M0L6_2atmpS2138;
  _M0L8capacityS2140 = _M0L4selfS620->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2139 = _M0FPB21calc__grow__threshold(_M0L8capacityS2140);
  _M0L4selfS620->$4 = _M0L6_2atmpS2139;
  _M0L4selfS620->$1 = 0;
  _M0L6_2atmpS2141 = 0;
  _M0L6_2aoldS3996 = _M0L4selfS620->$5;
  if (_M0L6_2aoldS3996) {
    moonbit_decref(_M0L6_2aoldS3996);
  }
  _M0L4selfS620->$5 = _M0L6_2atmpS2141;
  _M0L4selfS620->$6 = -1;
  _M0L8_2aparamS622 = _M0L9old__headS619;
  while (1) {
    if (_M0L8_2aparamS622 == 0) {
      if (_M0L8_2aparamS622) {
        moonbit_decref(_M0L8_2aparamS622);
      }
      moonbit_decref(_M0L4selfS620);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS623 =
        _M0L8_2aparamS622;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS624 =
        _M0L7_2aSomeS623;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3995 =
        _M0L4_2axS624->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS625 =
        _M0L8_2afieldS3995;
      moonbit_string_t _M0L8_2afieldS3994 = _M0L4_2axS624->$4;
      moonbit_string_t _M0L6_2akeyS626 = _M0L8_2afieldS3994;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3993 =
        _M0L4_2axS624->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS627 =
        _M0L8_2afieldS3993;
      int32_t _M0L8_2afieldS3992 = _M0L4_2axS624->$3;
      int32_t _M0L6_2acntS4185 = Moonbit_object_header(_M0L4_2axS624)->rc;
      int32_t _M0L7_2ahashS628;
      if (_M0L6_2acntS4185 > 1) {
        int32_t _M0L11_2anew__cntS4186 = _M0L6_2acntS4185 - 1;
        Moonbit_object_header(_M0L4_2axS624)->rc = _M0L11_2anew__cntS4186;
        moonbit_incref(_M0L8_2avalueS627);
        moonbit_incref(_M0L6_2akeyS626);
        if (_M0L7_2anextS625) {
          moonbit_incref(_M0L7_2anextS625);
        }
      } else if (_M0L6_2acntS4185 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS624);
      }
      _M0L7_2ahashS628 = _M0L8_2afieldS3992;
      moonbit_incref(_M0L4selfS620);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS620, _M0L6_2akeyS626, _M0L8_2avalueS627, _M0L7_2ahashS628);
      _M0L8_2aparamS622 = _M0L7_2anextS625;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS631
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4004;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS630;
  int32_t _M0L8capacityS2149;
  int32_t _M0L13new__capacityS632;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2144;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2143;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4003;
  int32_t _M0L6_2atmpS2145;
  int32_t _M0L8capacityS2147;
  int32_t _M0L6_2atmpS2146;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2148;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4002;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS633;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4004 = _M0L4selfS631->$5;
  _M0L9old__headS630 = _M0L8_2afieldS4004;
  _M0L8capacityS2149 = _M0L4selfS631->$2;
  _M0L13new__capacityS632 = _M0L8capacityS2149 << 1;
  _M0L6_2atmpS2144 = 0;
  _M0L6_2atmpS2143
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS632, _M0L6_2atmpS2144);
  _M0L6_2aoldS4003 = _M0L4selfS631->$0;
  if (_M0L9old__headS630) {
    moonbit_incref(_M0L9old__headS630);
  }
  moonbit_decref(_M0L6_2aoldS4003);
  _M0L4selfS631->$0 = _M0L6_2atmpS2143;
  _M0L4selfS631->$2 = _M0L13new__capacityS632;
  _M0L6_2atmpS2145 = _M0L13new__capacityS632 - 1;
  _M0L4selfS631->$3 = _M0L6_2atmpS2145;
  _M0L8capacityS2147 = _M0L4selfS631->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2146 = _M0FPB21calc__grow__threshold(_M0L8capacityS2147);
  _M0L4selfS631->$4 = _M0L6_2atmpS2146;
  _M0L4selfS631->$1 = 0;
  _M0L6_2atmpS2148 = 0;
  _M0L6_2aoldS4002 = _M0L4selfS631->$5;
  if (_M0L6_2aoldS4002) {
    moonbit_decref(_M0L6_2aoldS4002);
  }
  _M0L4selfS631->$5 = _M0L6_2atmpS2148;
  _M0L4selfS631->$6 = -1;
  _M0L8_2aparamS633 = _M0L9old__headS630;
  while (1) {
    if (_M0L8_2aparamS633 == 0) {
      if (_M0L8_2aparamS633) {
        moonbit_decref(_M0L8_2aparamS633);
      }
      moonbit_decref(_M0L4selfS631);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS634 =
        _M0L8_2aparamS633;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS635 =
        _M0L7_2aSomeS634;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4001 =
        _M0L4_2axS635->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS636 =
        _M0L8_2afieldS4001;
      int32_t _M0L6_2akeyS637 = _M0L4_2axS635->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4000 =
        _M0L4_2axS635->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS638 =
        _M0L8_2afieldS4000;
      int32_t _M0L8_2afieldS3999 = _M0L4_2axS635->$3;
      int32_t _M0L6_2acntS4187 = Moonbit_object_header(_M0L4_2axS635)->rc;
      int32_t _M0L7_2ahashS639;
      if (_M0L6_2acntS4187 > 1) {
        int32_t _M0L11_2anew__cntS4188 = _M0L6_2acntS4187 - 1;
        Moonbit_object_header(_M0L4_2axS635)->rc = _M0L11_2anew__cntS4188;
        moonbit_incref(_M0L8_2avalueS638);
        if (_M0L7_2anextS636) {
          moonbit_incref(_M0L7_2anextS636);
        }
      } else if (_M0L6_2acntS4187 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS635);
      }
      _M0L7_2ahashS639 = _M0L8_2afieldS3999;
      moonbit_incref(_M0L4selfS631);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS631, _M0L6_2akeyS637, _M0L8_2avalueS638, _M0L7_2ahashS639);
      _M0L8_2aparamS633 = _M0L7_2anextS636;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS590,
  moonbit_string_t _M0L3keyS596,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS597,
  int32_t _M0L4hashS592
) {
  int32_t _M0L14capacity__maskS2117;
  int32_t _M0L6_2atmpS2116;
  int32_t _M0L3pslS587;
  int32_t _M0L3idxS588;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2117 = _M0L4selfS590->$3;
  _M0L6_2atmpS2116 = _M0L4hashS592 & _M0L14capacity__maskS2117;
  _M0L3pslS587 = 0;
  _M0L3idxS588 = _M0L6_2atmpS2116;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4009 =
      _M0L4selfS590->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2115 =
      _M0L8_2afieldS4009;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4008;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS589;
    if (
      _M0L3idxS588 < 0
      || _M0L3idxS588 >= Moonbit_array_length(_M0L7entriesS2115)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4008
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2115[
        _M0L3idxS588
      ];
    _M0L7_2abindS589 = _M0L6_2atmpS4008;
    if (_M0L7_2abindS589 == 0) {
      int32_t _M0L4sizeS2100 = _M0L4selfS590->$1;
      int32_t _M0L8grow__atS2101 = _M0L4selfS590->$4;
      int32_t _M0L7_2abindS593;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS594;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS595;
      if (_M0L4sizeS2100 >= _M0L8grow__atS2101) {
        int32_t _M0L14capacity__maskS2103;
        int32_t _M0L6_2atmpS2102;
        moonbit_incref(_M0L4selfS590);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS590);
        _M0L14capacity__maskS2103 = _M0L4selfS590->$3;
        _M0L6_2atmpS2102 = _M0L4hashS592 & _M0L14capacity__maskS2103;
        _M0L3pslS587 = 0;
        _M0L3idxS588 = _M0L6_2atmpS2102;
        continue;
      }
      _M0L7_2abindS593 = _M0L4selfS590->$6;
      _M0L7_2abindS594 = 0;
      _M0L5entryS595
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS595)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS595->$0 = _M0L7_2abindS593;
      _M0L5entryS595->$1 = _M0L7_2abindS594;
      _M0L5entryS595->$2 = _M0L3pslS587;
      _M0L5entryS595->$3 = _M0L4hashS592;
      _M0L5entryS595->$4 = _M0L3keyS596;
      _M0L5entryS595->$5 = _M0L5valueS597;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS590, _M0L3idxS588, _M0L5entryS595);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS598 =
        _M0L7_2abindS589;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS599 =
        _M0L7_2aSomeS598;
      int32_t _M0L4hashS2105 = _M0L14_2acurr__entryS599->$3;
      int32_t _if__result_4309;
      int32_t _M0L3pslS2106;
      int32_t _M0L6_2atmpS2111;
      int32_t _M0L6_2atmpS2113;
      int32_t _M0L14capacity__maskS2114;
      int32_t _M0L6_2atmpS2112;
      if (_M0L4hashS2105 == _M0L4hashS592) {
        moonbit_string_t _M0L8_2afieldS4007 = _M0L14_2acurr__entryS599->$4;
        moonbit_string_t _M0L3keyS2104 = _M0L8_2afieldS4007;
        int32_t _M0L6_2atmpS4006;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4006
        = moonbit_val_array_equal(_M0L3keyS2104, _M0L3keyS596);
        _if__result_4309 = _M0L6_2atmpS4006;
      } else {
        _if__result_4309 = 0;
      }
      if (_if__result_4309) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4005;
        moonbit_incref(_M0L14_2acurr__entryS599);
        moonbit_decref(_M0L3keyS596);
        moonbit_decref(_M0L4selfS590);
        _M0L6_2aoldS4005 = _M0L14_2acurr__entryS599->$5;
        moonbit_decref(_M0L6_2aoldS4005);
        _M0L14_2acurr__entryS599->$5 = _M0L5valueS597;
        moonbit_decref(_M0L14_2acurr__entryS599);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS599);
      }
      _M0L3pslS2106 = _M0L14_2acurr__entryS599->$2;
      if (_M0L3pslS587 > _M0L3pslS2106) {
        int32_t _M0L4sizeS2107 = _M0L4selfS590->$1;
        int32_t _M0L8grow__atS2108 = _M0L4selfS590->$4;
        int32_t _M0L7_2abindS600;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS601;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS602;
        if (_M0L4sizeS2107 >= _M0L8grow__atS2108) {
          int32_t _M0L14capacity__maskS2110;
          int32_t _M0L6_2atmpS2109;
          moonbit_decref(_M0L14_2acurr__entryS599);
          moonbit_incref(_M0L4selfS590);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS590);
          _M0L14capacity__maskS2110 = _M0L4selfS590->$3;
          _M0L6_2atmpS2109 = _M0L4hashS592 & _M0L14capacity__maskS2110;
          _M0L3pslS587 = 0;
          _M0L3idxS588 = _M0L6_2atmpS2109;
          continue;
        }
        moonbit_incref(_M0L4selfS590);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS590, _M0L3idxS588, _M0L14_2acurr__entryS599);
        _M0L7_2abindS600 = _M0L4selfS590->$6;
        _M0L7_2abindS601 = 0;
        _M0L5entryS602
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS602)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS602->$0 = _M0L7_2abindS600;
        _M0L5entryS602->$1 = _M0L7_2abindS601;
        _M0L5entryS602->$2 = _M0L3pslS587;
        _M0L5entryS602->$3 = _M0L4hashS592;
        _M0L5entryS602->$4 = _M0L3keyS596;
        _M0L5entryS602->$5 = _M0L5valueS597;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS590, _M0L3idxS588, _M0L5entryS602);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS599);
      }
      _M0L6_2atmpS2111 = _M0L3pslS587 + 1;
      _M0L6_2atmpS2113 = _M0L3idxS588 + 1;
      _M0L14capacity__maskS2114 = _M0L4selfS590->$3;
      _M0L6_2atmpS2112 = _M0L6_2atmpS2113 & _M0L14capacity__maskS2114;
      _M0L3pslS587 = _M0L6_2atmpS2111;
      _M0L3idxS588 = _M0L6_2atmpS2112;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS606,
  int32_t _M0L3keyS612,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS613,
  int32_t _M0L4hashS608
) {
  int32_t _M0L14capacity__maskS2135;
  int32_t _M0L6_2atmpS2134;
  int32_t _M0L3pslS603;
  int32_t _M0L3idxS604;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2135 = _M0L4selfS606->$3;
  _M0L6_2atmpS2134 = _M0L4hashS608 & _M0L14capacity__maskS2135;
  _M0L3pslS603 = 0;
  _M0L3idxS604 = _M0L6_2atmpS2134;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4012 =
      _M0L4selfS606->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2133 =
      _M0L8_2afieldS4012;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4011;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS605;
    if (
      _M0L3idxS604 < 0
      || _M0L3idxS604 >= Moonbit_array_length(_M0L7entriesS2133)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4011
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2133[
        _M0L3idxS604
      ];
    _M0L7_2abindS605 = _M0L6_2atmpS4011;
    if (_M0L7_2abindS605 == 0) {
      int32_t _M0L4sizeS2118 = _M0L4selfS606->$1;
      int32_t _M0L8grow__atS2119 = _M0L4selfS606->$4;
      int32_t _M0L7_2abindS609;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS610;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS611;
      if (_M0L4sizeS2118 >= _M0L8grow__atS2119) {
        int32_t _M0L14capacity__maskS2121;
        int32_t _M0L6_2atmpS2120;
        moonbit_incref(_M0L4selfS606);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS606);
        _M0L14capacity__maskS2121 = _M0L4selfS606->$3;
        _M0L6_2atmpS2120 = _M0L4hashS608 & _M0L14capacity__maskS2121;
        _M0L3pslS603 = 0;
        _M0L3idxS604 = _M0L6_2atmpS2120;
        continue;
      }
      _M0L7_2abindS609 = _M0L4selfS606->$6;
      _M0L7_2abindS610 = 0;
      _M0L5entryS611
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS611)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS611->$0 = _M0L7_2abindS609;
      _M0L5entryS611->$1 = _M0L7_2abindS610;
      _M0L5entryS611->$2 = _M0L3pslS603;
      _M0L5entryS611->$3 = _M0L4hashS608;
      _M0L5entryS611->$4 = _M0L3keyS612;
      _M0L5entryS611->$5 = _M0L5valueS613;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS606, _M0L3idxS604, _M0L5entryS611);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS614 =
        _M0L7_2abindS605;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS615 =
        _M0L7_2aSomeS614;
      int32_t _M0L4hashS2123 = _M0L14_2acurr__entryS615->$3;
      int32_t _if__result_4311;
      int32_t _M0L3pslS2124;
      int32_t _M0L6_2atmpS2129;
      int32_t _M0L6_2atmpS2131;
      int32_t _M0L14capacity__maskS2132;
      int32_t _M0L6_2atmpS2130;
      if (_M0L4hashS2123 == _M0L4hashS608) {
        int32_t _M0L3keyS2122 = _M0L14_2acurr__entryS615->$4;
        _if__result_4311 = _M0L3keyS2122 == _M0L3keyS612;
      } else {
        _if__result_4311 = 0;
      }
      if (_if__result_4311) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4010;
        moonbit_incref(_M0L14_2acurr__entryS615);
        moonbit_decref(_M0L4selfS606);
        _M0L6_2aoldS4010 = _M0L14_2acurr__entryS615->$5;
        moonbit_decref(_M0L6_2aoldS4010);
        _M0L14_2acurr__entryS615->$5 = _M0L5valueS613;
        moonbit_decref(_M0L14_2acurr__entryS615);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS615);
      }
      _M0L3pslS2124 = _M0L14_2acurr__entryS615->$2;
      if (_M0L3pslS603 > _M0L3pslS2124) {
        int32_t _M0L4sizeS2125 = _M0L4selfS606->$1;
        int32_t _M0L8grow__atS2126 = _M0L4selfS606->$4;
        int32_t _M0L7_2abindS616;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS617;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS618;
        if (_M0L4sizeS2125 >= _M0L8grow__atS2126) {
          int32_t _M0L14capacity__maskS2128;
          int32_t _M0L6_2atmpS2127;
          moonbit_decref(_M0L14_2acurr__entryS615);
          moonbit_incref(_M0L4selfS606);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS606);
          _M0L14capacity__maskS2128 = _M0L4selfS606->$3;
          _M0L6_2atmpS2127 = _M0L4hashS608 & _M0L14capacity__maskS2128;
          _M0L3pslS603 = 0;
          _M0L3idxS604 = _M0L6_2atmpS2127;
          continue;
        }
        moonbit_incref(_M0L4selfS606);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS606, _M0L3idxS604, _M0L14_2acurr__entryS615);
        _M0L7_2abindS616 = _M0L4selfS606->$6;
        _M0L7_2abindS617 = 0;
        _M0L5entryS618
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS618)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS618->$0 = _M0L7_2abindS616;
        _M0L5entryS618->$1 = _M0L7_2abindS617;
        _M0L5entryS618->$2 = _M0L3pslS603;
        _M0L5entryS618->$3 = _M0L4hashS608;
        _M0L5entryS618->$4 = _M0L3keyS612;
        _M0L5entryS618->$5 = _M0L5valueS613;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS606, _M0L3idxS604, _M0L5entryS618);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS615);
      }
      _M0L6_2atmpS2129 = _M0L3pslS603 + 1;
      _M0L6_2atmpS2131 = _M0L3idxS604 + 1;
      _M0L14capacity__maskS2132 = _M0L4selfS606->$3;
      _M0L6_2atmpS2130 = _M0L6_2atmpS2131 & _M0L14capacity__maskS2132;
      _M0L3pslS603 = _M0L6_2atmpS2129;
      _M0L3idxS604 = _M0L6_2atmpS2130;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS571,
  int32_t _M0L3idxS576,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS575
) {
  int32_t _M0L3pslS2083;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L6_2atmpS2081;
  int32_t _M0L14capacity__maskS2082;
  int32_t _M0L6_2atmpS2080;
  int32_t _M0L3pslS567;
  int32_t _M0L3idxS568;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS569;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2083 = _M0L5entryS575->$2;
  _M0L6_2atmpS2079 = _M0L3pslS2083 + 1;
  _M0L6_2atmpS2081 = _M0L3idxS576 + 1;
  _M0L14capacity__maskS2082 = _M0L4selfS571->$3;
  _M0L6_2atmpS2080 = _M0L6_2atmpS2081 & _M0L14capacity__maskS2082;
  _M0L3pslS567 = _M0L6_2atmpS2079;
  _M0L3idxS568 = _M0L6_2atmpS2080;
  _M0L5entryS569 = _M0L5entryS575;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4014 =
      _M0L4selfS571->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2078 =
      _M0L8_2afieldS4014;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4013;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS570;
    if (
      _M0L3idxS568 < 0
      || _M0L3idxS568 >= Moonbit_array_length(_M0L7entriesS2078)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4013
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2078[
        _M0L3idxS568
      ];
    _M0L7_2abindS570 = _M0L6_2atmpS4013;
    if (_M0L7_2abindS570 == 0) {
      _M0L5entryS569->$2 = _M0L3pslS567;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS571, _M0L5entryS569, _M0L3idxS568);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS573 =
        _M0L7_2abindS570;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS574 =
        _M0L7_2aSomeS573;
      int32_t _M0L3pslS2068 = _M0L14_2acurr__entryS574->$2;
      if (_M0L3pslS567 > _M0L3pslS2068) {
        int32_t _M0L3pslS2073;
        int32_t _M0L6_2atmpS2069;
        int32_t _M0L6_2atmpS2071;
        int32_t _M0L14capacity__maskS2072;
        int32_t _M0L6_2atmpS2070;
        _M0L5entryS569->$2 = _M0L3pslS567;
        moonbit_incref(_M0L14_2acurr__entryS574);
        moonbit_incref(_M0L4selfS571);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS571, _M0L5entryS569, _M0L3idxS568);
        _M0L3pslS2073 = _M0L14_2acurr__entryS574->$2;
        _M0L6_2atmpS2069 = _M0L3pslS2073 + 1;
        _M0L6_2atmpS2071 = _M0L3idxS568 + 1;
        _M0L14capacity__maskS2072 = _M0L4selfS571->$3;
        _M0L6_2atmpS2070 = _M0L6_2atmpS2071 & _M0L14capacity__maskS2072;
        _M0L3pslS567 = _M0L6_2atmpS2069;
        _M0L3idxS568 = _M0L6_2atmpS2070;
        _M0L5entryS569 = _M0L14_2acurr__entryS574;
        continue;
      } else {
        int32_t _M0L6_2atmpS2074 = _M0L3pslS567 + 1;
        int32_t _M0L6_2atmpS2076 = _M0L3idxS568 + 1;
        int32_t _M0L14capacity__maskS2077 = _M0L4selfS571->$3;
        int32_t _M0L6_2atmpS2075 =
          _M0L6_2atmpS2076 & _M0L14capacity__maskS2077;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4313 =
          _M0L5entryS569;
        _M0L3pslS567 = _M0L6_2atmpS2074;
        _M0L3idxS568 = _M0L6_2atmpS2075;
        _M0L5entryS569 = _tmp_4313;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS581,
  int32_t _M0L3idxS586,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS585
) {
  int32_t _M0L3pslS2099;
  int32_t _M0L6_2atmpS2095;
  int32_t _M0L6_2atmpS2097;
  int32_t _M0L14capacity__maskS2098;
  int32_t _M0L6_2atmpS2096;
  int32_t _M0L3pslS577;
  int32_t _M0L3idxS578;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS579;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2099 = _M0L5entryS585->$2;
  _M0L6_2atmpS2095 = _M0L3pslS2099 + 1;
  _M0L6_2atmpS2097 = _M0L3idxS586 + 1;
  _M0L14capacity__maskS2098 = _M0L4selfS581->$3;
  _M0L6_2atmpS2096 = _M0L6_2atmpS2097 & _M0L14capacity__maskS2098;
  _M0L3pslS577 = _M0L6_2atmpS2095;
  _M0L3idxS578 = _M0L6_2atmpS2096;
  _M0L5entryS579 = _M0L5entryS585;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4016 =
      _M0L4selfS581->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2094 =
      _M0L8_2afieldS4016;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4015;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS580;
    if (
      _M0L3idxS578 < 0
      || _M0L3idxS578 >= Moonbit_array_length(_M0L7entriesS2094)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4015
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2094[
        _M0L3idxS578
      ];
    _M0L7_2abindS580 = _M0L6_2atmpS4015;
    if (_M0L7_2abindS580 == 0) {
      _M0L5entryS579->$2 = _M0L3pslS577;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581, _M0L5entryS579, _M0L3idxS578);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS583 =
        _M0L7_2abindS580;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS584 =
        _M0L7_2aSomeS583;
      int32_t _M0L3pslS2084 = _M0L14_2acurr__entryS584->$2;
      if (_M0L3pslS577 > _M0L3pslS2084) {
        int32_t _M0L3pslS2089;
        int32_t _M0L6_2atmpS2085;
        int32_t _M0L6_2atmpS2087;
        int32_t _M0L14capacity__maskS2088;
        int32_t _M0L6_2atmpS2086;
        _M0L5entryS579->$2 = _M0L3pslS577;
        moonbit_incref(_M0L14_2acurr__entryS584);
        moonbit_incref(_M0L4selfS581);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581, _M0L5entryS579, _M0L3idxS578);
        _M0L3pslS2089 = _M0L14_2acurr__entryS584->$2;
        _M0L6_2atmpS2085 = _M0L3pslS2089 + 1;
        _M0L6_2atmpS2087 = _M0L3idxS578 + 1;
        _M0L14capacity__maskS2088 = _M0L4selfS581->$3;
        _M0L6_2atmpS2086 = _M0L6_2atmpS2087 & _M0L14capacity__maskS2088;
        _M0L3pslS577 = _M0L6_2atmpS2085;
        _M0L3idxS578 = _M0L6_2atmpS2086;
        _M0L5entryS579 = _M0L14_2acurr__entryS584;
        continue;
      } else {
        int32_t _M0L6_2atmpS2090 = _M0L3pslS577 + 1;
        int32_t _M0L6_2atmpS2092 = _M0L3idxS578 + 1;
        int32_t _M0L14capacity__maskS2093 = _M0L4selfS581->$3;
        int32_t _M0L6_2atmpS2091 =
          _M0L6_2atmpS2092 & _M0L14capacity__maskS2093;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4315 =
          _M0L5entryS579;
        _M0L3pslS577 = _M0L6_2atmpS2090;
        _M0L3idxS578 = _M0L6_2atmpS2091;
        _M0L5entryS579 = _tmp_4315;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS555,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS557,
  int32_t _M0L8new__idxS556
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4019;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2064;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2065;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4018;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4017;
  int32_t _M0L6_2acntS4189;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS558;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4019 = _M0L4selfS555->$0;
  _M0L7entriesS2064 = _M0L8_2afieldS4019;
  moonbit_incref(_M0L5entryS557);
  _M0L6_2atmpS2065 = _M0L5entryS557;
  if (
    _M0L8new__idxS556 < 0
    || _M0L8new__idxS556 >= Moonbit_array_length(_M0L7entriesS2064)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4018
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2064[
      _M0L8new__idxS556
    ];
  if (_M0L6_2aoldS4018) {
    moonbit_decref(_M0L6_2aoldS4018);
  }
  _M0L7entriesS2064[_M0L8new__idxS556] = _M0L6_2atmpS2065;
  _M0L8_2afieldS4017 = _M0L5entryS557->$1;
  _M0L6_2acntS4189 = Moonbit_object_header(_M0L5entryS557)->rc;
  if (_M0L6_2acntS4189 > 1) {
    int32_t _M0L11_2anew__cntS4192 = _M0L6_2acntS4189 - 1;
    Moonbit_object_header(_M0L5entryS557)->rc = _M0L11_2anew__cntS4192;
    if (_M0L8_2afieldS4017) {
      moonbit_incref(_M0L8_2afieldS4017);
    }
  } else if (_M0L6_2acntS4189 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4191 =
      _M0L5entryS557->$5;
    moonbit_string_t _M0L8_2afieldS4190;
    moonbit_decref(_M0L8_2afieldS4191);
    _M0L8_2afieldS4190 = _M0L5entryS557->$4;
    moonbit_decref(_M0L8_2afieldS4190);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS557);
  }
  _M0L7_2abindS558 = _M0L8_2afieldS4017;
  if (_M0L7_2abindS558 == 0) {
    if (_M0L7_2abindS558) {
      moonbit_decref(_M0L7_2abindS558);
    }
    _M0L4selfS555->$6 = _M0L8new__idxS556;
    moonbit_decref(_M0L4selfS555);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS559;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS560;
    moonbit_decref(_M0L4selfS555);
    _M0L7_2aSomeS559 = _M0L7_2abindS558;
    _M0L7_2anextS560 = _M0L7_2aSomeS559;
    _M0L7_2anextS560->$0 = _M0L8new__idxS556;
    moonbit_decref(_M0L7_2anextS560);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS561,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS563,
  int32_t _M0L8new__idxS562
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4022;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2066;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2067;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4021;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4020;
  int32_t _M0L6_2acntS4193;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS564;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4022 = _M0L4selfS561->$0;
  _M0L7entriesS2066 = _M0L8_2afieldS4022;
  moonbit_incref(_M0L5entryS563);
  _M0L6_2atmpS2067 = _M0L5entryS563;
  if (
    _M0L8new__idxS562 < 0
    || _M0L8new__idxS562 >= Moonbit_array_length(_M0L7entriesS2066)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4021
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2066[
      _M0L8new__idxS562
    ];
  if (_M0L6_2aoldS4021) {
    moonbit_decref(_M0L6_2aoldS4021);
  }
  _M0L7entriesS2066[_M0L8new__idxS562] = _M0L6_2atmpS2067;
  _M0L8_2afieldS4020 = _M0L5entryS563->$1;
  _M0L6_2acntS4193 = Moonbit_object_header(_M0L5entryS563)->rc;
  if (_M0L6_2acntS4193 > 1) {
    int32_t _M0L11_2anew__cntS4195 = _M0L6_2acntS4193 - 1;
    Moonbit_object_header(_M0L5entryS563)->rc = _M0L11_2anew__cntS4195;
    if (_M0L8_2afieldS4020) {
      moonbit_incref(_M0L8_2afieldS4020);
    }
  } else if (_M0L6_2acntS4193 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4194 =
      _M0L5entryS563->$5;
    moonbit_decref(_M0L8_2afieldS4194);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS563);
  }
  _M0L7_2abindS564 = _M0L8_2afieldS4020;
  if (_M0L7_2abindS564 == 0) {
    if (_M0L7_2abindS564) {
      moonbit_decref(_M0L7_2abindS564);
    }
    _M0L4selfS561->$6 = _M0L8new__idxS562;
    moonbit_decref(_M0L4selfS561);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS565;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS566;
    moonbit_decref(_M0L4selfS561);
    _M0L7_2aSomeS565 = _M0L7_2abindS564;
    _M0L7_2anextS566 = _M0L7_2aSomeS565;
    _M0L7_2anextS566->$0 = _M0L8new__idxS562;
    moonbit_decref(_M0L7_2anextS566);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS548,
  int32_t _M0L3idxS550,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS549
) {
  int32_t _M0L7_2abindS547;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4024;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2051;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2052;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4023;
  int32_t _M0L4sizeS2054;
  int32_t _M0L6_2atmpS2053;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS547 = _M0L4selfS548->$6;
  switch (_M0L7_2abindS547) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2046;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4025;
      moonbit_incref(_M0L5entryS549);
      _M0L6_2atmpS2046 = _M0L5entryS549;
      _M0L6_2aoldS4025 = _M0L4selfS548->$5;
      if (_M0L6_2aoldS4025) {
        moonbit_decref(_M0L6_2aoldS4025);
      }
      _M0L4selfS548->$5 = _M0L6_2atmpS2046;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4028 =
        _M0L4selfS548->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2050 =
        _M0L8_2afieldS4028;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4027;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2049;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2047;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2048;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4026;
      if (
        _M0L7_2abindS547 < 0
        || _M0L7_2abindS547 >= Moonbit_array_length(_M0L7entriesS2050)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4027
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2050[
          _M0L7_2abindS547
        ];
      _M0L6_2atmpS2049 = _M0L6_2atmpS4027;
      if (_M0L6_2atmpS2049) {
        moonbit_incref(_M0L6_2atmpS2049);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2047
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2049);
      moonbit_incref(_M0L5entryS549);
      _M0L6_2atmpS2048 = _M0L5entryS549;
      _M0L6_2aoldS4026 = _M0L6_2atmpS2047->$1;
      if (_M0L6_2aoldS4026) {
        moonbit_decref(_M0L6_2aoldS4026);
      }
      _M0L6_2atmpS2047->$1 = _M0L6_2atmpS2048;
      moonbit_decref(_M0L6_2atmpS2047);
      break;
    }
  }
  _M0L4selfS548->$6 = _M0L3idxS550;
  _M0L8_2afieldS4024 = _M0L4selfS548->$0;
  _M0L7entriesS2051 = _M0L8_2afieldS4024;
  _M0L6_2atmpS2052 = _M0L5entryS549;
  if (
    _M0L3idxS550 < 0
    || _M0L3idxS550 >= Moonbit_array_length(_M0L7entriesS2051)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4023
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2051[
      _M0L3idxS550
    ];
  if (_M0L6_2aoldS4023) {
    moonbit_decref(_M0L6_2aoldS4023);
  }
  _M0L7entriesS2051[_M0L3idxS550] = _M0L6_2atmpS2052;
  _M0L4sizeS2054 = _M0L4selfS548->$1;
  _M0L6_2atmpS2053 = _M0L4sizeS2054 + 1;
  _M0L4selfS548->$1 = _M0L6_2atmpS2053;
  moonbit_decref(_M0L4selfS548);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS552,
  int32_t _M0L3idxS554,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS553
) {
  int32_t _M0L7_2abindS551;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4030;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2060;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2061;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4029;
  int32_t _M0L4sizeS2063;
  int32_t _M0L6_2atmpS2062;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS551 = _M0L4selfS552->$6;
  switch (_M0L7_2abindS551) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2055;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4031;
      moonbit_incref(_M0L5entryS553);
      _M0L6_2atmpS2055 = _M0L5entryS553;
      _M0L6_2aoldS4031 = _M0L4selfS552->$5;
      if (_M0L6_2aoldS4031) {
        moonbit_decref(_M0L6_2aoldS4031);
      }
      _M0L4selfS552->$5 = _M0L6_2atmpS2055;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4034 =
        _M0L4selfS552->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2059 =
        _M0L8_2afieldS4034;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4033;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2058;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2056;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2057;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4032;
      if (
        _M0L7_2abindS551 < 0
        || _M0L7_2abindS551 >= Moonbit_array_length(_M0L7entriesS2059)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4033
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2059[
          _M0L7_2abindS551
        ];
      _M0L6_2atmpS2058 = _M0L6_2atmpS4033;
      if (_M0L6_2atmpS2058) {
        moonbit_incref(_M0L6_2atmpS2058);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2056
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2058);
      moonbit_incref(_M0L5entryS553);
      _M0L6_2atmpS2057 = _M0L5entryS553;
      _M0L6_2aoldS4032 = _M0L6_2atmpS2056->$1;
      if (_M0L6_2aoldS4032) {
        moonbit_decref(_M0L6_2aoldS4032);
      }
      _M0L6_2atmpS2056->$1 = _M0L6_2atmpS2057;
      moonbit_decref(_M0L6_2atmpS2056);
      break;
    }
  }
  _M0L4selfS552->$6 = _M0L3idxS554;
  _M0L8_2afieldS4030 = _M0L4selfS552->$0;
  _M0L7entriesS2060 = _M0L8_2afieldS4030;
  _M0L6_2atmpS2061 = _M0L5entryS553;
  if (
    _M0L3idxS554 < 0
    || _M0L3idxS554 >= Moonbit_array_length(_M0L7entriesS2060)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4029
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2060[
      _M0L3idxS554
    ];
  if (_M0L6_2aoldS4029) {
    moonbit_decref(_M0L6_2aoldS4029);
  }
  _M0L7entriesS2060[_M0L3idxS554] = _M0L6_2atmpS2061;
  _M0L4sizeS2063 = _M0L4selfS552->$1;
  _M0L6_2atmpS2062 = _M0L4sizeS2063 + 1;
  _M0L4selfS552->$1 = _M0L6_2atmpS2062;
  moonbit_decref(_M0L4selfS552);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS536
) {
  int32_t _M0L8capacityS535;
  int32_t _M0L7_2abindS537;
  int32_t _M0L7_2abindS538;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2044;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS539;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS540;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4316;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS535
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS536);
  _M0L7_2abindS537 = _M0L8capacityS535 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS538 = _M0FPB21calc__grow__threshold(_M0L8capacityS535);
  _M0L6_2atmpS2044 = 0;
  _M0L7_2abindS539
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS535, _M0L6_2atmpS2044);
  _M0L7_2abindS540 = 0;
  _block_4316
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4316)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4316->$0 = _M0L7_2abindS539;
  _block_4316->$1 = 0;
  _block_4316->$2 = _M0L8capacityS535;
  _block_4316->$3 = _M0L7_2abindS537;
  _block_4316->$4 = _M0L7_2abindS538;
  _block_4316->$5 = _M0L7_2abindS540;
  _block_4316->$6 = -1;
  return _block_4316;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS542
) {
  int32_t _M0L8capacityS541;
  int32_t _M0L7_2abindS543;
  int32_t _M0L7_2abindS544;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2045;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS545;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS546;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4317;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS541
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS542);
  _M0L7_2abindS543 = _M0L8capacityS541 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS544 = _M0FPB21calc__grow__threshold(_M0L8capacityS541);
  _M0L6_2atmpS2045 = 0;
  _M0L7_2abindS545
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS541, _M0L6_2atmpS2045);
  _M0L7_2abindS546 = 0;
  _block_4317
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4317)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4317->$0 = _M0L7_2abindS545;
  _block_4317->$1 = 0;
  _block_4317->$2 = _M0L8capacityS541;
  _block_4317->$3 = _M0L7_2abindS543;
  _block_4317->$4 = _M0L7_2abindS544;
  _block_4317->$5 = _M0L7_2abindS546;
  _block_4317->$6 = -1;
  return _block_4317;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS534) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS534 >= 0) {
    int32_t _M0L6_2atmpS2043;
    int32_t _M0L6_2atmpS2042;
    int32_t _M0L6_2atmpS2041;
    int32_t _M0L6_2atmpS2040;
    if (_M0L4selfS534 <= 1) {
      return 1;
    }
    if (_M0L4selfS534 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2043 = _M0L4selfS534 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2042 = moonbit_clz32(_M0L6_2atmpS2043);
    _M0L6_2atmpS2041 = _M0L6_2atmpS2042 - 1;
    _M0L6_2atmpS2040 = 2147483647 >> (_M0L6_2atmpS2041 & 31);
    return _M0L6_2atmpS2040 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS533) {
  int32_t _M0L6_2atmpS2039;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2039 = _M0L8capacityS533 * 13;
  return _M0L6_2atmpS2039 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS529
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS529 == 0) {
    if (_M0L4selfS529) {
      moonbit_decref(_M0L4selfS529);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS530 =
      _M0L4selfS529;
    return _M0L7_2aSomeS530;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS531
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS531 == 0) {
    if (_M0L4selfS531) {
      moonbit_decref(_M0L4selfS531);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS532 =
      _M0L4selfS531;
    return _M0L7_2aSomeS532;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS528
) {
  moonbit_string_t* _M0L6_2atmpS2038;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2038 = _M0L4selfS528;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2038);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS524,
  int32_t _M0L5indexS525
) {
  uint64_t* _M0L6_2atmpS2036;
  uint64_t _M0L6_2atmpS4035;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2036 = _M0L4selfS524;
  if (
    _M0L5indexS525 < 0
    || _M0L5indexS525 >= Moonbit_array_length(_M0L6_2atmpS2036)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4035 = (uint64_t)_M0L6_2atmpS2036[_M0L5indexS525];
  moonbit_decref(_M0L6_2atmpS2036);
  return _M0L6_2atmpS4035;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS526,
  int32_t _M0L5indexS527
) {
  uint32_t* _M0L6_2atmpS2037;
  uint32_t _M0L6_2atmpS4036;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2037 = _M0L4selfS526;
  if (
    _M0L5indexS527 < 0
    || _M0L5indexS527 >= Moonbit_array_length(_M0L6_2atmpS2037)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4036 = (uint32_t)_M0L6_2atmpS2037[_M0L5indexS527];
  moonbit_decref(_M0L6_2atmpS2037);
  return _M0L6_2atmpS4036;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS523
) {
  moonbit_string_t* _M0L6_2atmpS2034;
  int32_t _M0L6_2atmpS4037;
  int32_t _M0L6_2atmpS2035;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2033;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS523);
  _M0L6_2atmpS2034 = _M0L4selfS523;
  _M0L6_2atmpS4037 = Moonbit_array_length(_M0L4selfS523);
  moonbit_decref(_M0L4selfS523);
  _M0L6_2atmpS2035 = _M0L6_2atmpS4037;
  _M0L6_2atmpS2033
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2035, _M0L6_2atmpS2034
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2033);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS521
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS520;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__* _closure_4318;
  struct _M0TWEOs* _M0L6_2atmpS2021;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS520
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS520)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS520->$0 = 0;
  _closure_4318
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__));
  Moonbit_object_header(_closure_4318)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__, $0_0) >> 2, 2, 0);
  _closure_4318->code = &_M0MPC15array9ArrayView4iterGsEC2022l570;
  _closure_4318->$0_0 = _M0L4selfS521.$0;
  _closure_4318->$0_1 = _M0L4selfS521.$1;
  _closure_4318->$0_2 = _M0L4selfS521.$2;
  _closure_4318->$1 = _M0L1iS520;
  _M0L6_2atmpS2021 = (struct _M0TWEOs*)_closure_4318;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2021);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2022l570(
  struct _M0TWEOs* _M0L6_2aenvS2023
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__* _M0L14_2acasted__envS2024;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4042;
  struct _M0TPC13ref3RefGiE* _M0L1iS520;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4041;
  int32_t _M0L6_2acntS4196;
  struct _M0TPB9ArrayViewGsE _M0L4selfS521;
  int32_t _M0L3valS2025;
  int32_t _M0L6_2atmpS2026;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2024
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2022__l570__*)_M0L6_2aenvS2023;
  _M0L8_2afieldS4042 = _M0L14_2acasted__envS2024->$1;
  _M0L1iS520 = _M0L8_2afieldS4042;
  _M0L8_2afieldS4041
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2024->$0_1,
      _M0L14_2acasted__envS2024->$0_2,
      _M0L14_2acasted__envS2024->$0_0
  };
  _M0L6_2acntS4196 = Moonbit_object_header(_M0L14_2acasted__envS2024)->rc;
  if (_M0L6_2acntS4196 > 1) {
    int32_t _M0L11_2anew__cntS4197 = _M0L6_2acntS4196 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2024)->rc
    = _M0L11_2anew__cntS4197;
    moonbit_incref(_M0L1iS520);
    moonbit_incref(_M0L8_2afieldS4041.$0);
  } else if (_M0L6_2acntS4196 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2024);
  }
  _M0L4selfS521 = _M0L8_2afieldS4041;
  _M0L3valS2025 = _M0L1iS520->$0;
  moonbit_incref(_M0L4selfS521.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2026 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS521);
  if (_M0L3valS2025 < _M0L6_2atmpS2026) {
    moonbit_string_t* _M0L8_2afieldS4040 = _M0L4selfS521.$0;
    moonbit_string_t* _M0L3bufS2029 = _M0L8_2afieldS4040;
    int32_t _M0L8_2afieldS4039 = _M0L4selfS521.$1;
    int32_t _M0L5startS2031 = _M0L8_2afieldS4039;
    int32_t _M0L3valS2032 = _M0L1iS520->$0;
    int32_t _M0L6_2atmpS2030 = _M0L5startS2031 + _M0L3valS2032;
    moonbit_string_t _M0L6_2atmpS4038 =
      (moonbit_string_t)_M0L3bufS2029[_M0L6_2atmpS2030];
    moonbit_string_t _M0L4elemS522;
    int32_t _M0L3valS2028;
    int32_t _M0L6_2atmpS2027;
    moonbit_incref(_M0L6_2atmpS4038);
    moonbit_decref(_M0L3bufS2029);
    _M0L4elemS522 = _M0L6_2atmpS4038;
    _M0L3valS2028 = _M0L1iS520->$0;
    _M0L6_2atmpS2027 = _M0L3valS2028 + 1;
    _M0L1iS520->$0 = _M0L6_2atmpS2027;
    moonbit_decref(_M0L1iS520);
    return _M0L4elemS522;
  } else {
    moonbit_decref(_M0L4selfS521.$0);
    moonbit_decref(_M0L1iS520);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS519
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS519;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS518,
  struct _M0TPB6Logger _M0L6loggerS517
) {
  moonbit_string_t _M0L6_2atmpS2020;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2020
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS518, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS517.$0->$method_0(_M0L6loggerS517.$1, _M0L6_2atmpS2020);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS516,
  struct _M0TPB6Logger _M0L6loggerS515
) {
  moonbit_string_t _M0L6_2atmpS2019;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2019 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS516, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS515.$0->$method_0(_M0L6loggerS515.$1, _M0L6_2atmpS2019);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS510) {
  int32_t _M0L3lenS509;
  struct _M0TPC13ref3RefGiE* _M0L5indexS511;
  struct _M0R38String_3a_3aiter_2eanon__u2003__l247__* _closure_4319;
  struct _M0TWEOc* _M0L6_2atmpS2002;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS509 = Moonbit_array_length(_M0L4selfS510);
  _M0L5indexS511
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS511)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS511->$0 = 0;
  _closure_4319
  = (struct _M0R38String_3a_3aiter_2eanon__u2003__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2003__l247__));
  Moonbit_object_header(_closure_4319)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2003__l247__, $0) >> 2, 2, 0);
  _closure_4319->code = &_M0MPC16string6String4iterC2003l247;
  _closure_4319->$0 = _M0L5indexS511;
  _closure_4319->$1 = _M0L4selfS510;
  _closure_4319->$2 = _M0L3lenS509;
  _M0L6_2atmpS2002 = (struct _M0TWEOc*)_closure_4319;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2002);
}

int32_t _M0MPC16string6String4iterC2003l247(
  struct _M0TWEOc* _M0L6_2aenvS2004
) {
  struct _M0R38String_3a_3aiter_2eanon__u2003__l247__* _M0L14_2acasted__envS2005;
  int32_t _M0L3lenS509;
  moonbit_string_t _M0L8_2afieldS4045;
  moonbit_string_t _M0L4selfS510;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4044;
  int32_t _M0L6_2acntS4198;
  struct _M0TPC13ref3RefGiE* _M0L5indexS511;
  int32_t _M0L3valS2006;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2005
  = (struct _M0R38String_3a_3aiter_2eanon__u2003__l247__*)_M0L6_2aenvS2004;
  _M0L3lenS509 = _M0L14_2acasted__envS2005->$2;
  _M0L8_2afieldS4045 = _M0L14_2acasted__envS2005->$1;
  _M0L4selfS510 = _M0L8_2afieldS4045;
  _M0L8_2afieldS4044 = _M0L14_2acasted__envS2005->$0;
  _M0L6_2acntS4198 = Moonbit_object_header(_M0L14_2acasted__envS2005)->rc;
  if (_M0L6_2acntS4198 > 1) {
    int32_t _M0L11_2anew__cntS4199 = _M0L6_2acntS4198 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2005)->rc
    = _M0L11_2anew__cntS4199;
    moonbit_incref(_M0L4selfS510);
    moonbit_incref(_M0L8_2afieldS4044);
  } else if (_M0L6_2acntS4198 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2005);
  }
  _M0L5indexS511 = _M0L8_2afieldS4044;
  _M0L3valS2006 = _M0L5indexS511->$0;
  if (_M0L3valS2006 < _M0L3lenS509) {
    int32_t _M0L3valS2018 = _M0L5indexS511->$0;
    int32_t _M0L2c1S512 = _M0L4selfS510[_M0L3valS2018];
    int32_t _if__result_4320;
    int32_t _M0L3valS2016;
    int32_t _M0L6_2atmpS2015;
    int32_t _M0L6_2atmpS2017;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S512)) {
      int32_t _M0L3valS2008 = _M0L5indexS511->$0;
      int32_t _M0L6_2atmpS2007 = _M0L3valS2008 + 1;
      _if__result_4320 = _M0L6_2atmpS2007 < _M0L3lenS509;
    } else {
      _if__result_4320 = 0;
    }
    if (_if__result_4320) {
      int32_t _M0L3valS2014 = _M0L5indexS511->$0;
      int32_t _M0L6_2atmpS2013 = _M0L3valS2014 + 1;
      int32_t _M0L6_2atmpS4043 = _M0L4selfS510[_M0L6_2atmpS2013];
      int32_t _M0L2c2S513;
      moonbit_decref(_M0L4selfS510);
      _M0L2c2S513 = _M0L6_2atmpS4043;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S513)) {
        int32_t _M0L6_2atmpS2011 = (int32_t)_M0L2c1S512;
        int32_t _M0L6_2atmpS2012 = (int32_t)_M0L2c2S513;
        int32_t _M0L1cS514;
        int32_t _M0L3valS2010;
        int32_t _M0L6_2atmpS2009;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS514
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2011, _M0L6_2atmpS2012);
        _M0L3valS2010 = _M0L5indexS511->$0;
        _M0L6_2atmpS2009 = _M0L3valS2010 + 2;
        _M0L5indexS511->$0 = _M0L6_2atmpS2009;
        moonbit_decref(_M0L5indexS511);
        return _M0L1cS514;
      }
    } else {
      moonbit_decref(_M0L4selfS510);
    }
    _M0L3valS2016 = _M0L5indexS511->$0;
    _M0L6_2atmpS2015 = _M0L3valS2016 + 1;
    _M0L5indexS511->$0 = _M0L6_2atmpS2015;
    moonbit_decref(_M0L5indexS511);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2017 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S512);
    return _M0L6_2atmpS2017;
  } else {
    moonbit_decref(_M0L5indexS511);
    moonbit_decref(_M0L4selfS510);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS500,
  moonbit_string_t _M0L5valueS502
) {
  int32_t _M0L3lenS1987;
  moonbit_string_t* _M0L6_2atmpS1989;
  int32_t _M0L6_2atmpS4048;
  int32_t _M0L6_2atmpS1988;
  int32_t _M0L6lengthS501;
  moonbit_string_t* _M0L8_2afieldS4047;
  moonbit_string_t* _M0L3bufS1990;
  moonbit_string_t _M0L6_2aoldS4046;
  int32_t _M0L6_2atmpS1991;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1987 = _M0L4selfS500->$1;
  moonbit_incref(_M0L4selfS500);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1989 = _M0MPC15array5Array6bufferGsE(_M0L4selfS500);
  _M0L6_2atmpS4048 = Moonbit_array_length(_M0L6_2atmpS1989);
  moonbit_decref(_M0L6_2atmpS1989);
  _M0L6_2atmpS1988 = _M0L6_2atmpS4048;
  if (_M0L3lenS1987 == _M0L6_2atmpS1988) {
    moonbit_incref(_M0L4selfS500);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS500);
  }
  _M0L6lengthS501 = _M0L4selfS500->$1;
  _M0L8_2afieldS4047 = _M0L4selfS500->$0;
  _M0L3bufS1990 = _M0L8_2afieldS4047;
  _M0L6_2aoldS4046 = (moonbit_string_t)_M0L3bufS1990[_M0L6lengthS501];
  moonbit_decref(_M0L6_2aoldS4046);
  _M0L3bufS1990[_M0L6lengthS501] = _M0L5valueS502;
  _M0L6_2atmpS1991 = _M0L6lengthS501 + 1;
  _M0L4selfS500->$1 = _M0L6_2atmpS1991;
  moonbit_decref(_M0L4selfS500);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS503,
  struct _M0TUsiE* _M0L5valueS505
) {
  int32_t _M0L3lenS1992;
  struct _M0TUsiE** _M0L6_2atmpS1994;
  int32_t _M0L6_2atmpS4051;
  int32_t _M0L6_2atmpS1993;
  int32_t _M0L6lengthS504;
  struct _M0TUsiE** _M0L8_2afieldS4050;
  struct _M0TUsiE** _M0L3bufS1995;
  struct _M0TUsiE* _M0L6_2aoldS4049;
  int32_t _M0L6_2atmpS1996;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1992 = _M0L4selfS503->$1;
  moonbit_incref(_M0L4selfS503);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1994 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS503);
  _M0L6_2atmpS4051 = Moonbit_array_length(_M0L6_2atmpS1994);
  moonbit_decref(_M0L6_2atmpS1994);
  _M0L6_2atmpS1993 = _M0L6_2atmpS4051;
  if (_M0L3lenS1992 == _M0L6_2atmpS1993) {
    moonbit_incref(_M0L4selfS503);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS503);
  }
  _M0L6lengthS504 = _M0L4selfS503->$1;
  _M0L8_2afieldS4050 = _M0L4selfS503->$0;
  _M0L3bufS1995 = _M0L8_2afieldS4050;
  _M0L6_2aoldS4049 = (struct _M0TUsiE*)_M0L3bufS1995[_M0L6lengthS504];
  if (_M0L6_2aoldS4049) {
    moonbit_decref(_M0L6_2aoldS4049);
  }
  _M0L3bufS1995[_M0L6lengthS504] = _M0L5valueS505;
  _M0L6_2atmpS1996 = _M0L6lengthS504 + 1;
  _M0L4selfS503->$1 = _M0L6_2atmpS1996;
  moonbit_decref(_M0L4selfS503);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS506,
  void* _M0L5valueS508
) {
  int32_t _M0L3lenS1997;
  void** _M0L6_2atmpS1999;
  int32_t _M0L6_2atmpS4054;
  int32_t _M0L6_2atmpS1998;
  int32_t _M0L6lengthS507;
  void** _M0L8_2afieldS4053;
  void** _M0L3bufS2000;
  void* _M0L6_2aoldS4052;
  int32_t _M0L6_2atmpS2001;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1997 = _M0L4selfS506->$1;
  moonbit_incref(_M0L4selfS506);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1999
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS506);
  _M0L6_2atmpS4054 = Moonbit_array_length(_M0L6_2atmpS1999);
  moonbit_decref(_M0L6_2atmpS1999);
  _M0L6_2atmpS1998 = _M0L6_2atmpS4054;
  if (_M0L3lenS1997 == _M0L6_2atmpS1998) {
    moonbit_incref(_M0L4selfS506);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS506);
  }
  _M0L6lengthS507 = _M0L4selfS506->$1;
  _M0L8_2afieldS4053 = _M0L4selfS506->$0;
  _M0L3bufS2000 = _M0L8_2afieldS4053;
  _M0L6_2aoldS4052 = (void*)_M0L3bufS2000[_M0L6lengthS507];
  moonbit_decref(_M0L6_2aoldS4052);
  _M0L3bufS2000[_M0L6lengthS507] = _M0L5valueS508;
  _M0L6_2atmpS2001 = _M0L6lengthS507 + 1;
  _M0L4selfS506->$1 = _M0L6_2atmpS2001;
  moonbit_decref(_M0L4selfS506);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS492) {
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
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS492, _M0L8new__capS493);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS495
) {
  int32_t _M0L8old__capS494;
  int32_t _M0L8new__capS496;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS494 = _M0L4selfS495->$1;
  if (_M0L8old__capS494 == 0) {
    _M0L8new__capS496 = 8;
  } else {
    _M0L8new__capS496 = _M0L8old__capS494 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS495, _M0L8new__capS496);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS498
) {
  int32_t _M0L8old__capS497;
  int32_t _M0L8new__capS499;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS497 = _M0L4selfS498->$1;
  if (_M0L8old__capS497 == 0) {
    _M0L8new__capS499 = 8;
  } else {
    _M0L8new__capS499 = _M0L8old__capS497 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS498, _M0L8new__capS499);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS476,
  int32_t _M0L13new__capacityS474
) {
  moonbit_string_t* _M0L8new__bufS473;
  moonbit_string_t* _M0L8_2afieldS4056;
  moonbit_string_t* _M0L8old__bufS475;
  int32_t _M0L8old__capS477;
  int32_t _M0L9copy__lenS478;
  moonbit_string_t* _M0L6_2aoldS4055;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS473
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS474, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4056 = _M0L4selfS476->$0;
  _M0L8old__bufS475 = _M0L8_2afieldS4056;
  _M0L8old__capS477 = Moonbit_array_length(_M0L8old__bufS475);
  if (_M0L8old__capS477 < _M0L13new__capacityS474) {
    _M0L9copy__lenS478 = _M0L8old__capS477;
  } else {
    _M0L9copy__lenS478 = _M0L13new__capacityS474;
  }
  moonbit_incref(_M0L8old__bufS475);
  moonbit_incref(_M0L8new__bufS473);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS473, 0, _M0L8old__bufS475, 0, _M0L9copy__lenS478);
  _M0L6_2aoldS4055 = _M0L4selfS476->$0;
  moonbit_decref(_M0L6_2aoldS4055);
  _M0L4selfS476->$0 = _M0L8new__bufS473;
  moonbit_decref(_M0L4selfS476);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS482,
  int32_t _M0L13new__capacityS480
) {
  struct _M0TUsiE** _M0L8new__bufS479;
  struct _M0TUsiE** _M0L8_2afieldS4058;
  struct _M0TUsiE** _M0L8old__bufS481;
  int32_t _M0L8old__capS483;
  int32_t _M0L9copy__lenS484;
  struct _M0TUsiE** _M0L6_2aoldS4057;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS479
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS480, 0);
  _M0L8_2afieldS4058 = _M0L4selfS482->$0;
  _M0L8old__bufS481 = _M0L8_2afieldS4058;
  _M0L8old__capS483 = Moonbit_array_length(_M0L8old__bufS481);
  if (_M0L8old__capS483 < _M0L13new__capacityS480) {
    _M0L9copy__lenS484 = _M0L8old__capS483;
  } else {
    _M0L9copy__lenS484 = _M0L13new__capacityS480;
  }
  moonbit_incref(_M0L8old__bufS481);
  moonbit_incref(_M0L8new__bufS479);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS479, 0, _M0L8old__bufS481, 0, _M0L9copy__lenS484);
  _M0L6_2aoldS4057 = _M0L4selfS482->$0;
  moonbit_decref(_M0L6_2aoldS4057);
  _M0L4selfS482->$0 = _M0L8new__bufS479;
  moonbit_decref(_M0L4selfS482);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS488,
  int32_t _M0L13new__capacityS486
) {
  void** _M0L8new__bufS485;
  void** _M0L8_2afieldS4060;
  void** _M0L8old__bufS487;
  int32_t _M0L8old__capS489;
  int32_t _M0L9copy__lenS490;
  void** _M0L6_2aoldS4059;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS485
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS486, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4060 = _M0L4selfS488->$0;
  _M0L8old__bufS487 = _M0L8_2afieldS4060;
  _M0L8old__capS489 = Moonbit_array_length(_M0L8old__bufS487);
  if (_M0L8old__capS489 < _M0L13new__capacityS486) {
    _M0L9copy__lenS490 = _M0L8old__capS489;
  } else {
    _M0L9copy__lenS490 = _M0L13new__capacityS486;
  }
  moonbit_incref(_M0L8old__bufS487);
  moonbit_incref(_M0L8new__bufS485);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS485, 0, _M0L8old__bufS487, 0, _M0L9copy__lenS490);
  _M0L6_2aoldS4059 = _M0L4selfS488->$0;
  moonbit_decref(_M0L6_2aoldS4059);
  _M0L4selfS488->$0 = _M0L8new__bufS485;
  moonbit_decref(_M0L4selfS488);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS472
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS472 == 0) {
    moonbit_string_t* _M0L6_2atmpS1985 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4321 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4321)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4321->$0 = _M0L6_2atmpS1985;
    _block_4321->$1 = 0;
    return _block_4321;
  } else {
    moonbit_string_t* _M0L6_2atmpS1986 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS472, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4322 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4322)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4322->$0 = _M0L6_2atmpS1986;
    _block_4322->$1 = 0;
    return _block_4322;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS466,
  int32_t _M0L1nS465
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS465 <= 0) {
    moonbit_decref(_M0L4selfS466);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS465 == 1) {
    return _M0L4selfS466;
  } else {
    int32_t _M0L3lenS467 = Moonbit_array_length(_M0L4selfS466);
    int32_t _M0L6_2atmpS1984 = _M0L3lenS467 * _M0L1nS465;
    struct _M0TPB13StringBuilder* _M0L3bufS468;
    moonbit_string_t _M0L3strS469;
    int32_t _M0L2__S470;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS468 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1984);
    _M0L3strS469 = _M0L4selfS466;
    _M0L2__S470 = 0;
    while (1) {
      if (_M0L2__S470 < _M0L1nS465) {
        int32_t _M0L6_2atmpS1983;
        moonbit_incref(_M0L3strS469);
        moonbit_incref(_M0L3bufS468);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS468, _M0L3strS469);
        _M0L6_2atmpS1983 = _M0L2__S470 + 1;
        _M0L2__S470 = _M0L6_2atmpS1983;
        continue;
      } else {
        moonbit_decref(_M0L3strS469);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS468);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS463,
  struct _M0TPC16string10StringView _M0L3strS464
) {
  int32_t _M0L3lenS1971;
  int32_t _M0L6_2atmpS1973;
  int32_t _M0L6_2atmpS1972;
  int32_t _M0L6_2atmpS1970;
  moonbit_bytes_t _M0L8_2afieldS4061;
  moonbit_bytes_t _M0L4dataS1974;
  int32_t _M0L3lenS1975;
  moonbit_string_t _M0L6_2atmpS1976;
  int32_t _M0L6_2atmpS1977;
  int32_t _M0L6_2atmpS1978;
  int32_t _M0L3lenS1980;
  int32_t _M0L6_2atmpS1982;
  int32_t _M0L6_2atmpS1981;
  int32_t _M0L6_2atmpS1979;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1971 = _M0L4selfS463->$1;
  moonbit_incref(_M0L3strS464.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1973 = _M0MPC16string10StringView6length(_M0L3strS464);
  _M0L6_2atmpS1972 = _M0L6_2atmpS1973 * 2;
  _M0L6_2atmpS1970 = _M0L3lenS1971 + _M0L6_2atmpS1972;
  moonbit_incref(_M0L4selfS463);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS463, _M0L6_2atmpS1970);
  _M0L8_2afieldS4061 = _M0L4selfS463->$0;
  _M0L4dataS1974 = _M0L8_2afieldS4061;
  _M0L3lenS1975 = _M0L4selfS463->$1;
  moonbit_incref(_M0L4dataS1974);
  moonbit_incref(_M0L3strS464.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1976 = _M0MPC16string10StringView4data(_M0L3strS464);
  moonbit_incref(_M0L3strS464.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1977 = _M0MPC16string10StringView13start__offset(_M0L3strS464);
  moonbit_incref(_M0L3strS464.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1978 = _M0MPC16string10StringView6length(_M0L3strS464);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1974, _M0L3lenS1975, _M0L6_2atmpS1976, _M0L6_2atmpS1977, _M0L6_2atmpS1978);
  _M0L3lenS1980 = _M0L4selfS463->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1982 = _M0MPC16string10StringView6length(_M0L3strS464);
  _M0L6_2atmpS1981 = _M0L6_2atmpS1982 * 2;
  _M0L6_2atmpS1979 = _M0L3lenS1980 + _M0L6_2atmpS1981;
  _M0L4selfS463->$1 = _M0L6_2atmpS1979;
  moonbit_decref(_M0L4selfS463);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS455,
  int32_t _M0L3lenS458,
  int32_t _M0L13start__offsetS462,
  int64_t _M0L11end__offsetS453
) {
  int32_t _M0L11end__offsetS452;
  int32_t _M0L5indexS456;
  int32_t _M0L5countS457;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS453 == 4294967296ll) {
    _M0L11end__offsetS452 = Moonbit_array_length(_M0L4selfS455);
  } else {
    int64_t _M0L7_2aSomeS454 = _M0L11end__offsetS453;
    _M0L11end__offsetS452 = (int32_t)_M0L7_2aSomeS454;
  }
  _M0L5indexS456 = _M0L13start__offsetS462;
  _M0L5countS457 = 0;
  while (1) {
    int32_t _if__result_4325;
    if (_M0L5indexS456 < _M0L11end__offsetS452) {
      _if__result_4325 = _M0L5countS457 < _M0L3lenS458;
    } else {
      _if__result_4325 = 0;
    }
    if (_if__result_4325) {
      int32_t _M0L2c1S459 = _M0L4selfS455[_M0L5indexS456];
      int32_t _if__result_4326;
      int32_t _M0L6_2atmpS1968;
      int32_t _M0L6_2atmpS1969;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S459)) {
        int32_t _M0L6_2atmpS1964 = _M0L5indexS456 + 1;
        _if__result_4326 = _M0L6_2atmpS1964 < _M0L11end__offsetS452;
      } else {
        _if__result_4326 = 0;
      }
      if (_if__result_4326) {
        int32_t _M0L6_2atmpS1967 = _M0L5indexS456 + 1;
        int32_t _M0L2c2S460 = _M0L4selfS455[_M0L6_2atmpS1967];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S460)) {
          int32_t _M0L6_2atmpS1965 = _M0L5indexS456 + 2;
          int32_t _M0L6_2atmpS1966 = _M0L5countS457 + 1;
          _M0L5indexS456 = _M0L6_2atmpS1965;
          _M0L5countS457 = _M0L6_2atmpS1966;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_51.data, (moonbit_string_t)moonbit_string_literal_52.data);
        }
      }
      _M0L6_2atmpS1968 = _M0L5indexS456 + 1;
      _M0L6_2atmpS1969 = _M0L5countS457 + 1;
      _M0L5indexS456 = _M0L6_2atmpS1968;
      _M0L5countS457 = _M0L6_2atmpS1969;
      continue;
    } else {
      moonbit_decref(_M0L4selfS455);
      return _M0L5countS457 >= _M0L3lenS458;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS449
) {
  int32_t _M0L3endS1958;
  int32_t _M0L8_2afieldS4062;
  int32_t _M0L5startS1959;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1958 = _M0L4selfS449.$2;
  _M0L8_2afieldS4062 = _M0L4selfS449.$1;
  moonbit_decref(_M0L4selfS449.$0);
  _M0L5startS1959 = _M0L8_2afieldS4062;
  return _M0L3endS1958 - _M0L5startS1959;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS450
) {
  int32_t _M0L3endS1960;
  int32_t _M0L8_2afieldS4063;
  int32_t _M0L5startS1961;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1960 = _M0L4selfS450.$2;
  _M0L8_2afieldS4063 = _M0L4selfS450.$1;
  moonbit_decref(_M0L4selfS450.$0);
  _M0L5startS1961 = _M0L8_2afieldS4063;
  return _M0L3endS1960 - _M0L5startS1961;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS451
) {
  int32_t _M0L3endS1962;
  int32_t _M0L8_2afieldS4064;
  int32_t _M0L5startS1963;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1962 = _M0L4selfS451.$2;
  _M0L8_2afieldS4064 = _M0L4selfS451.$1;
  moonbit_decref(_M0L4selfS451.$0);
  _M0L5startS1963 = _M0L8_2afieldS4064;
  return _M0L3endS1962 - _M0L5startS1963;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS447,
  int64_t _M0L19start__offset_2eoptS445,
  int64_t _M0L11end__offsetS448
) {
  int32_t _M0L13start__offsetS444;
  if (_M0L19start__offset_2eoptS445 == 4294967296ll) {
    _M0L13start__offsetS444 = 0;
  } else {
    int64_t _M0L7_2aSomeS446 = _M0L19start__offset_2eoptS445;
    _M0L13start__offsetS444 = (int32_t)_M0L7_2aSomeS446;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS447, _M0L13start__offsetS444, _M0L11end__offsetS448);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS442,
  int32_t _M0L13start__offsetS443,
  int64_t _M0L11end__offsetS440
) {
  int32_t _M0L11end__offsetS439;
  int32_t _if__result_4327;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS440 == 4294967296ll) {
    _M0L11end__offsetS439 = Moonbit_array_length(_M0L4selfS442);
  } else {
    int64_t _M0L7_2aSomeS441 = _M0L11end__offsetS440;
    _M0L11end__offsetS439 = (int32_t)_M0L7_2aSomeS441;
  }
  if (_M0L13start__offsetS443 >= 0) {
    if (_M0L13start__offsetS443 <= _M0L11end__offsetS439) {
      int32_t _M0L6_2atmpS1957 = Moonbit_array_length(_M0L4selfS442);
      _if__result_4327 = _M0L11end__offsetS439 <= _M0L6_2atmpS1957;
    } else {
      _if__result_4327 = 0;
    }
  } else {
    _if__result_4327 = 0;
  }
  if (_if__result_4327) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS443,
                                                 _M0L11end__offsetS439,
                                                 _M0L4selfS442};
  } else {
    moonbit_decref(_M0L4selfS442);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_53.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS438
) {
  moonbit_string_t _M0L8_2afieldS4066;
  moonbit_string_t _M0L3strS1954;
  int32_t _M0L5startS1955;
  int32_t _M0L8_2afieldS4065;
  int32_t _M0L3endS1956;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4066 = _M0L4selfS438.$0;
  _M0L3strS1954 = _M0L8_2afieldS4066;
  _M0L5startS1955 = _M0L4selfS438.$1;
  _M0L8_2afieldS4065 = _M0L4selfS438.$2;
  _M0L3endS1956 = _M0L8_2afieldS4065;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1954, _M0L5startS1955, _M0L3endS1956);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS436,
  struct _M0TPB6Logger _M0L6loggerS437
) {
  moonbit_string_t _M0L8_2afieldS4068;
  moonbit_string_t _M0L3strS1951;
  int32_t _M0L5startS1952;
  int32_t _M0L8_2afieldS4067;
  int32_t _M0L3endS1953;
  moonbit_string_t _M0L6substrS435;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4068 = _M0L4selfS436.$0;
  _M0L3strS1951 = _M0L8_2afieldS4068;
  _M0L5startS1952 = _M0L4selfS436.$1;
  _M0L8_2afieldS4067 = _M0L4selfS436.$2;
  _M0L3endS1953 = _M0L8_2afieldS4067;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS435
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1951, _M0L5startS1952, _M0L3endS1953);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS435, _M0L6loggerS437);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS427,
  struct _M0TPB6Logger _M0L6loggerS425
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS426;
  int32_t _M0L3lenS428;
  int32_t _M0L1iS429;
  int32_t _M0L3segS430;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS425.$1) {
    moonbit_incref(_M0L6loggerS425.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS425.$0->$method_3(_M0L6loggerS425.$1, 34);
  moonbit_incref(_M0L4selfS427);
  if (_M0L6loggerS425.$1) {
    moonbit_incref(_M0L6loggerS425.$1);
  }
  _M0L6_2aenvS426
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS426)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS426->$0 = _M0L4selfS427;
  _M0L6_2aenvS426->$1_0 = _M0L6loggerS425.$0;
  _M0L6_2aenvS426->$1_1 = _M0L6loggerS425.$1;
  _M0L3lenS428 = Moonbit_array_length(_M0L4selfS427);
  _M0L1iS429 = 0;
  _M0L3segS430 = 0;
  _2afor_431:;
  while (1) {
    int32_t _M0L4codeS432;
    int32_t _M0L1cS434;
    int32_t _M0L6_2atmpS1935;
    int32_t _M0L6_2atmpS1936;
    int32_t _M0L6_2atmpS1937;
    int32_t _tmp_4331;
    int32_t _tmp_4332;
    if (_M0L1iS429 >= _M0L3lenS428) {
      moonbit_decref(_M0L4selfS427);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
      break;
    }
    _M0L4codeS432 = _M0L4selfS427[_M0L1iS429];
    switch (_M0L4codeS432) {
      case 34: {
        _M0L1cS434 = _M0L4codeS432;
        goto join_433;
        break;
      }
      
      case 92: {
        _M0L1cS434 = _M0L4codeS432;
        goto join_433;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1938;
        int32_t _M0L6_2atmpS1939;
        moonbit_incref(_M0L6_2aenvS426);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
        if (_M0L6loggerS425.$1) {
          moonbit_incref(_M0L6loggerS425.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS425.$0->$method_0(_M0L6loggerS425.$1, (moonbit_string_t)moonbit_string_literal_36.data);
        _M0L6_2atmpS1938 = _M0L1iS429 + 1;
        _M0L6_2atmpS1939 = _M0L1iS429 + 1;
        _M0L1iS429 = _M0L6_2atmpS1938;
        _M0L3segS430 = _M0L6_2atmpS1939;
        goto _2afor_431;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1940;
        int32_t _M0L6_2atmpS1941;
        moonbit_incref(_M0L6_2aenvS426);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
        if (_M0L6loggerS425.$1) {
          moonbit_incref(_M0L6loggerS425.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS425.$0->$method_0(_M0L6loggerS425.$1, (moonbit_string_t)moonbit_string_literal_37.data);
        _M0L6_2atmpS1940 = _M0L1iS429 + 1;
        _M0L6_2atmpS1941 = _M0L1iS429 + 1;
        _M0L1iS429 = _M0L6_2atmpS1940;
        _M0L3segS430 = _M0L6_2atmpS1941;
        goto _2afor_431;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1942;
        int32_t _M0L6_2atmpS1943;
        moonbit_incref(_M0L6_2aenvS426);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
        if (_M0L6loggerS425.$1) {
          moonbit_incref(_M0L6loggerS425.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS425.$0->$method_0(_M0L6loggerS425.$1, (moonbit_string_t)moonbit_string_literal_38.data);
        _M0L6_2atmpS1942 = _M0L1iS429 + 1;
        _M0L6_2atmpS1943 = _M0L1iS429 + 1;
        _M0L1iS429 = _M0L6_2atmpS1942;
        _M0L3segS430 = _M0L6_2atmpS1943;
        goto _2afor_431;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1944;
        int32_t _M0L6_2atmpS1945;
        moonbit_incref(_M0L6_2aenvS426);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
        if (_M0L6loggerS425.$1) {
          moonbit_incref(_M0L6loggerS425.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS425.$0->$method_0(_M0L6loggerS425.$1, (moonbit_string_t)moonbit_string_literal_39.data);
        _M0L6_2atmpS1944 = _M0L1iS429 + 1;
        _M0L6_2atmpS1945 = _M0L1iS429 + 1;
        _M0L1iS429 = _M0L6_2atmpS1944;
        _M0L3segS430 = _M0L6_2atmpS1945;
        goto _2afor_431;
        break;
      }
      default: {
        if (_M0L4codeS432 < 32) {
          int32_t _M0L6_2atmpS1947;
          moonbit_string_t _M0L6_2atmpS1946;
          int32_t _M0L6_2atmpS1948;
          int32_t _M0L6_2atmpS1949;
          moonbit_incref(_M0L6_2aenvS426);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
          if (_M0L6loggerS425.$1) {
            moonbit_incref(_M0L6loggerS425.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS425.$0->$method_0(_M0L6loggerS425.$1, (moonbit_string_t)moonbit_string_literal_54.data);
          _M0L6_2atmpS1947 = _M0L4codeS432 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1946 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1947);
          if (_M0L6loggerS425.$1) {
            moonbit_incref(_M0L6loggerS425.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS425.$0->$method_0(_M0L6loggerS425.$1, _M0L6_2atmpS1946);
          if (_M0L6loggerS425.$1) {
            moonbit_incref(_M0L6loggerS425.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS425.$0->$method_3(_M0L6loggerS425.$1, 125);
          _M0L6_2atmpS1948 = _M0L1iS429 + 1;
          _M0L6_2atmpS1949 = _M0L1iS429 + 1;
          _M0L1iS429 = _M0L6_2atmpS1948;
          _M0L3segS430 = _M0L6_2atmpS1949;
          goto _2afor_431;
        } else {
          int32_t _M0L6_2atmpS1950 = _M0L1iS429 + 1;
          int32_t _tmp_4330 = _M0L3segS430;
          _M0L1iS429 = _M0L6_2atmpS1950;
          _M0L3segS430 = _tmp_4330;
          goto _2afor_431;
        }
        break;
      }
    }
    goto joinlet_4329;
    join_433:;
    moonbit_incref(_M0L6_2aenvS426);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS426, _M0L3segS430, _M0L1iS429);
    if (_M0L6loggerS425.$1) {
      moonbit_incref(_M0L6loggerS425.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS425.$0->$method_3(_M0L6loggerS425.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1935 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS434);
    if (_M0L6loggerS425.$1) {
      moonbit_incref(_M0L6loggerS425.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS425.$0->$method_3(_M0L6loggerS425.$1, _M0L6_2atmpS1935);
    _M0L6_2atmpS1936 = _M0L1iS429 + 1;
    _M0L6_2atmpS1937 = _M0L1iS429 + 1;
    _M0L1iS429 = _M0L6_2atmpS1936;
    _M0L3segS430 = _M0L6_2atmpS1937;
    continue;
    joinlet_4329:;
    _tmp_4331 = _M0L1iS429;
    _tmp_4332 = _M0L3segS430;
    _M0L1iS429 = _tmp_4331;
    _M0L3segS430 = _tmp_4332;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS425.$0->$method_3(_M0L6loggerS425.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS421,
  int32_t _M0L3segS424,
  int32_t _M0L1iS423
) {
  struct _M0TPB6Logger _M0L8_2afieldS4070;
  struct _M0TPB6Logger _M0L6loggerS420;
  moonbit_string_t _M0L8_2afieldS4069;
  int32_t _M0L6_2acntS4200;
  moonbit_string_t _M0L4selfS422;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4070
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS421->$1_0, _M0L6_2aenvS421->$1_1
  };
  _M0L6loggerS420 = _M0L8_2afieldS4070;
  _M0L8_2afieldS4069 = _M0L6_2aenvS421->$0;
  _M0L6_2acntS4200 = Moonbit_object_header(_M0L6_2aenvS421)->rc;
  if (_M0L6_2acntS4200 > 1) {
    int32_t _M0L11_2anew__cntS4201 = _M0L6_2acntS4200 - 1;
    Moonbit_object_header(_M0L6_2aenvS421)->rc = _M0L11_2anew__cntS4201;
    if (_M0L6loggerS420.$1) {
      moonbit_incref(_M0L6loggerS420.$1);
    }
    moonbit_incref(_M0L8_2afieldS4069);
  } else if (_M0L6_2acntS4200 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS421);
  }
  _M0L4selfS422 = _M0L8_2afieldS4069;
  if (_M0L1iS423 > _M0L3segS424) {
    int32_t _M0L6_2atmpS1934 = _M0L1iS423 - _M0L3segS424;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS420.$0->$method_1(_M0L6loggerS420.$1, _M0L4selfS422, _M0L3segS424, _M0L6_2atmpS1934);
  } else {
    moonbit_decref(_M0L4selfS422);
    if (_M0L6loggerS420.$1) {
      moonbit_decref(_M0L6loggerS420.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS419) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS418;
  int32_t _M0L6_2atmpS1931;
  int32_t _M0L6_2atmpS1930;
  int32_t _M0L6_2atmpS1933;
  int32_t _M0L6_2atmpS1932;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1929;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS418 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1931 = _M0IPC14byte4BytePB3Div3div(_M0L1bS419, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1930
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1931);
  moonbit_incref(_M0L7_2aselfS418);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS418, _M0L6_2atmpS1930);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1933 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS419, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1932
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1933);
  moonbit_incref(_M0L7_2aselfS418);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS418, _M0L6_2atmpS1932);
  _M0L6_2atmpS1929 = _M0L7_2aselfS418;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1929);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS417) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS417 < 10) {
    int32_t _M0L6_2atmpS1926;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1926 = _M0IPC14byte4BytePB3Add3add(_M0L1iS417, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1926);
  } else {
    int32_t _M0L6_2atmpS1928;
    int32_t _M0L6_2atmpS1927;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1928 = _M0IPC14byte4BytePB3Add3add(_M0L1iS417, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1927 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1928, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1927);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS415,
  int32_t _M0L4thatS416
) {
  int32_t _M0L6_2atmpS1924;
  int32_t _M0L6_2atmpS1925;
  int32_t _M0L6_2atmpS1923;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1924 = (int32_t)_M0L4selfS415;
  _M0L6_2atmpS1925 = (int32_t)_M0L4thatS416;
  _M0L6_2atmpS1923 = _M0L6_2atmpS1924 - _M0L6_2atmpS1925;
  return _M0L6_2atmpS1923 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS413,
  int32_t _M0L4thatS414
) {
  int32_t _M0L6_2atmpS1921;
  int32_t _M0L6_2atmpS1922;
  int32_t _M0L6_2atmpS1920;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1921 = (int32_t)_M0L4selfS413;
  _M0L6_2atmpS1922 = (int32_t)_M0L4thatS414;
  _M0L6_2atmpS1920 = _M0L6_2atmpS1921 % _M0L6_2atmpS1922;
  return _M0L6_2atmpS1920 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS411,
  int32_t _M0L4thatS412
) {
  int32_t _M0L6_2atmpS1918;
  int32_t _M0L6_2atmpS1919;
  int32_t _M0L6_2atmpS1917;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1918 = (int32_t)_M0L4selfS411;
  _M0L6_2atmpS1919 = (int32_t)_M0L4thatS412;
  _M0L6_2atmpS1917 = _M0L6_2atmpS1918 / _M0L6_2atmpS1919;
  return _M0L6_2atmpS1917 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS409,
  int32_t _M0L4thatS410
) {
  int32_t _M0L6_2atmpS1915;
  int32_t _M0L6_2atmpS1916;
  int32_t _M0L6_2atmpS1914;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1915 = (int32_t)_M0L4selfS409;
  _M0L6_2atmpS1916 = (int32_t)_M0L4thatS410;
  _M0L6_2atmpS1914 = _M0L6_2atmpS1915 + _M0L6_2atmpS1916;
  return _M0L6_2atmpS1914 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS406,
  int32_t _M0L5startS404,
  int32_t _M0L3endS405
) {
  int32_t _if__result_4333;
  int32_t _M0L3lenS407;
  int32_t _M0L6_2atmpS1912;
  int32_t _M0L6_2atmpS1913;
  moonbit_bytes_t _M0L5bytesS408;
  moonbit_bytes_t _M0L6_2atmpS1911;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS404 == 0) {
    int32_t _M0L6_2atmpS1910 = Moonbit_array_length(_M0L3strS406);
    _if__result_4333 = _M0L3endS405 == _M0L6_2atmpS1910;
  } else {
    _if__result_4333 = 0;
  }
  if (_if__result_4333) {
    return _M0L3strS406;
  }
  _M0L3lenS407 = _M0L3endS405 - _M0L5startS404;
  _M0L6_2atmpS1912 = _M0L3lenS407 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1913 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS408
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1912, _M0L6_2atmpS1913);
  moonbit_incref(_M0L5bytesS408);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS408, 0, _M0L3strS406, _M0L5startS404, _M0L3lenS407);
  _M0L6_2atmpS1911 = _M0L5bytesS408;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1911, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS401) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS401;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS402
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS402;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS403) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS403;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS393,
  int32_t _M0L5radixS392
) {
  int32_t _if__result_4334;
  uint16_t* _M0L6bufferS394;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS392 < 2) {
    _if__result_4334 = 1;
  } else {
    _if__result_4334 = _M0L5radixS392 > 36;
  }
  if (_if__result_4334) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_56.data);
  }
  if (_M0L4selfS393 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_44.data;
  }
  switch (_M0L5radixS392) {
    case 10: {
      int32_t _M0L3lenS395;
      uint16_t* _M0L6bufferS396;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS395 = _M0FPB12dec__count64(_M0L4selfS393);
      _M0L6bufferS396 = (uint16_t*)moonbit_make_string(_M0L3lenS395, 0);
      moonbit_incref(_M0L6bufferS396);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS396, _M0L4selfS393, 0, _M0L3lenS395);
      _M0L6bufferS394 = _M0L6bufferS396;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS397;
      uint16_t* _M0L6bufferS398;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS397 = _M0FPB12hex__count64(_M0L4selfS393);
      _M0L6bufferS398 = (uint16_t*)moonbit_make_string(_M0L3lenS397, 0);
      moonbit_incref(_M0L6bufferS398);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS398, _M0L4selfS393, 0, _M0L3lenS397);
      _M0L6bufferS394 = _M0L6bufferS398;
      break;
    }
    default: {
      int32_t _M0L3lenS399;
      uint16_t* _M0L6bufferS400;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS399 = _M0FPB14radix__count64(_M0L4selfS393, _M0L5radixS392);
      _M0L6bufferS400 = (uint16_t*)moonbit_make_string(_M0L3lenS399, 0);
      moonbit_incref(_M0L6bufferS400);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS400, _M0L4selfS393, 0, _M0L3lenS399, _M0L5radixS392);
      _M0L6bufferS394 = _M0L6bufferS400;
      break;
    }
  }
  return _M0L6bufferS394;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS382,
  uint64_t _M0L3numS370,
  int32_t _M0L12digit__startS373,
  int32_t _M0L10total__lenS372
) {
  uint64_t _M0Lm3numS369;
  int32_t _M0Lm6offsetS371;
  uint64_t _M0L6_2atmpS1909;
  int32_t _M0Lm9remainingS384;
  int32_t _M0L6_2atmpS1890;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS369 = _M0L3numS370;
  _M0Lm6offsetS371 = _M0L10total__lenS372 - _M0L12digit__startS373;
  while (1) {
    uint64_t _M0L6_2atmpS1853 = _M0Lm3numS369;
    if (_M0L6_2atmpS1853 >= 10000ull) {
      uint64_t _M0L6_2atmpS1876 = _M0Lm3numS369;
      uint64_t _M0L1tS374 = _M0L6_2atmpS1876 / 10000ull;
      uint64_t _M0L6_2atmpS1875 = _M0Lm3numS369;
      uint64_t _M0L6_2atmpS1874 = _M0L6_2atmpS1875 % 10000ull;
      int32_t _M0L1rS375 = (int32_t)_M0L6_2atmpS1874;
      int32_t _M0L2d1S376;
      int32_t _M0L2d2S377;
      int32_t _M0L6_2atmpS1854;
      int32_t _M0L6_2atmpS1873;
      int32_t _M0L6_2atmpS1872;
      int32_t _M0L6d1__hiS378;
      int32_t _M0L6_2atmpS1871;
      int32_t _M0L6_2atmpS1870;
      int32_t _M0L6d1__loS379;
      int32_t _M0L6_2atmpS1869;
      int32_t _M0L6_2atmpS1868;
      int32_t _M0L6d2__hiS380;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1866;
      int32_t _M0L6d2__loS381;
      int32_t _M0L6_2atmpS1856;
      int32_t _M0L6_2atmpS1855;
      int32_t _M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1858;
      int32_t _M0L6_2atmpS1857;
      int32_t _M0L6_2atmpS1862;
      int32_t _M0L6_2atmpS1861;
      int32_t _M0L6_2atmpS1860;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1864;
      int32_t _M0L6_2atmpS1863;
      _M0Lm3numS369 = _M0L1tS374;
      _M0L2d1S376 = _M0L1rS375 / 100;
      _M0L2d2S377 = _M0L1rS375 % 100;
      _M0L6_2atmpS1854 = _M0Lm6offsetS371;
      _M0Lm6offsetS371 = _M0L6_2atmpS1854 - 4;
      _M0L6_2atmpS1873 = _M0L2d1S376 / 10;
      _M0L6_2atmpS1872 = 48 + _M0L6_2atmpS1873;
      _M0L6d1__hiS378 = (uint16_t)_M0L6_2atmpS1872;
      _M0L6_2atmpS1871 = _M0L2d1S376 % 10;
      _M0L6_2atmpS1870 = 48 + _M0L6_2atmpS1871;
      _M0L6d1__loS379 = (uint16_t)_M0L6_2atmpS1870;
      _M0L6_2atmpS1869 = _M0L2d2S377 / 10;
      _M0L6_2atmpS1868 = 48 + _M0L6_2atmpS1869;
      _M0L6d2__hiS380 = (uint16_t)_M0L6_2atmpS1868;
      _M0L6_2atmpS1867 = _M0L2d2S377 % 10;
      _M0L6_2atmpS1866 = 48 + _M0L6_2atmpS1867;
      _M0L6d2__loS381 = (uint16_t)_M0L6_2atmpS1866;
      _M0L6_2atmpS1856 = _M0Lm6offsetS371;
      _M0L6_2atmpS1855 = _M0L12digit__startS373 + _M0L6_2atmpS1856;
      _M0L6bufferS382[_M0L6_2atmpS1855] = _M0L6d1__hiS378;
      _M0L6_2atmpS1859 = _M0Lm6offsetS371;
      _M0L6_2atmpS1858 = _M0L12digit__startS373 + _M0L6_2atmpS1859;
      _M0L6_2atmpS1857 = _M0L6_2atmpS1858 + 1;
      _M0L6bufferS382[_M0L6_2atmpS1857] = _M0L6d1__loS379;
      _M0L6_2atmpS1862 = _M0Lm6offsetS371;
      _M0L6_2atmpS1861 = _M0L12digit__startS373 + _M0L6_2atmpS1862;
      _M0L6_2atmpS1860 = _M0L6_2atmpS1861 + 2;
      _M0L6bufferS382[_M0L6_2atmpS1860] = _M0L6d2__hiS380;
      _M0L6_2atmpS1865 = _M0Lm6offsetS371;
      _M0L6_2atmpS1864 = _M0L12digit__startS373 + _M0L6_2atmpS1865;
      _M0L6_2atmpS1863 = _M0L6_2atmpS1864 + 3;
      _M0L6bufferS382[_M0L6_2atmpS1863] = _M0L6d2__loS381;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1909 = _M0Lm3numS369;
  _M0Lm9remainingS384 = (int32_t)_M0L6_2atmpS1909;
  while (1) {
    int32_t _M0L6_2atmpS1877 = _M0Lm9remainingS384;
    if (_M0L6_2atmpS1877 >= 100) {
      int32_t _M0L6_2atmpS1889 = _M0Lm9remainingS384;
      int32_t _M0L1tS385 = _M0L6_2atmpS1889 / 100;
      int32_t _M0L6_2atmpS1888 = _M0Lm9remainingS384;
      int32_t _M0L1dS386 = _M0L6_2atmpS1888 % 100;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6_2atmpS1887;
      int32_t _M0L6_2atmpS1886;
      int32_t _M0L5d__hiS387;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L5d__loS388;
      int32_t _M0L6_2atmpS1880;
      int32_t _M0L6_2atmpS1879;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L6_2atmpS1881;
      _M0Lm9remainingS384 = _M0L1tS385;
      _M0L6_2atmpS1878 = _M0Lm6offsetS371;
      _M0Lm6offsetS371 = _M0L6_2atmpS1878 - 2;
      _M0L6_2atmpS1887 = _M0L1dS386 / 10;
      _M0L6_2atmpS1886 = 48 + _M0L6_2atmpS1887;
      _M0L5d__hiS387 = (uint16_t)_M0L6_2atmpS1886;
      _M0L6_2atmpS1885 = _M0L1dS386 % 10;
      _M0L6_2atmpS1884 = 48 + _M0L6_2atmpS1885;
      _M0L5d__loS388 = (uint16_t)_M0L6_2atmpS1884;
      _M0L6_2atmpS1880 = _M0Lm6offsetS371;
      _M0L6_2atmpS1879 = _M0L12digit__startS373 + _M0L6_2atmpS1880;
      _M0L6bufferS382[_M0L6_2atmpS1879] = _M0L5d__hiS387;
      _M0L6_2atmpS1883 = _M0Lm6offsetS371;
      _M0L6_2atmpS1882 = _M0L12digit__startS373 + _M0L6_2atmpS1883;
      _M0L6_2atmpS1881 = _M0L6_2atmpS1882 + 1;
      _M0L6bufferS382[_M0L6_2atmpS1881] = _M0L5d__loS388;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1890 = _M0Lm9remainingS384;
  if (_M0L6_2atmpS1890 >= 10) {
    int32_t _M0L6_2atmpS1891 = _M0Lm6offsetS371;
    int32_t _M0L6_2atmpS1902;
    int32_t _M0L6_2atmpS1901;
    int32_t _M0L6_2atmpS1900;
    int32_t _M0L5d__hiS390;
    int32_t _M0L6_2atmpS1899;
    int32_t _M0L6_2atmpS1898;
    int32_t _M0L6_2atmpS1897;
    int32_t _M0L5d__loS391;
    int32_t _M0L6_2atmpS1893;
    int32_t _M0L6_2atmpS1892;
    int32_t _M0L6_2atmpS1896;
    int32_t _M0L6_2atmpS1895;
    int32_t _M0L6_2atmpS1894;
    _M0Lm6offsetS371 = _M0L6_2atmpS1891 - 2;
    _M0L6_2atmpS1902 = _M0Lm9remainingS384;
    _M0L6_2atmpS1901 = _M0L6_2atmpS1902 / 10;
    _M0L6_2atmpS1900 = 48 + _M0L6_2atmpS1901;
    _M0L5d__hiS390 = (uint16_t)_M0L6_2atmpS1900;
    _M0L6_2atmpS1899 = _M0Lm9remainingS384;
    _M0L6_2atmpS1898 = _M0L6_2atmpS1899 % 10;
    _M0L6_2atmpS1897 = 48 + _M0L6_2atmpS1898;
    _M0L5d__loS391 = (uint16_t)_M0L6_2atmpS1897;
    _M0L6_2atmpS1893 = _M0Lm6offsetS371;
    _M0L6_2atmpS1892 = _M0L12digit__startS373 + _M0L6_2atmpS1893;
    _M0L6bufferS382[_M0L6_2atmpS1892] = _M0L5d__hiS390;
    _M0L6_2atmpS1896 = _M0Lm6offsetS371;
    _M0L6_2atmpS1895 = _M0L12digit__startS373 + _M0L6_2atmpS1896;
    _M0L6_2atmpS1894 = _M0L6_2atmpS1895 + 1;
    _M0L6bufferS382[_M0L6_2atmpS1894] = _M0L5d__loS391;
    moonbit_decref(_M0L6bufferS382);
  } else {
    int32_t _M0L6_2atmpS1903 = _M0Lm6offsetS371;
    int32_t _M0L6_2atmpS1908;
    int32_t _M0L6_2atmpS1904;
    int32_t _M0L6_2atmpS1907;
    int32_t _M0L6_2atmpS1906;
    int32_t _M0L6_2atmpS1905;
    _M0Lm6offsetS371 = _M0L6_2atmpS1903 - 1;
    _M0L6_2atmpS1908 = _M0Lm6offsetS371;
    _M0L6_2atmpS1904 = _M0L12digit__startS373 + _M0L6_2atmpS1908;
    _M0L6_2atmpS1907 = _M0Lm9remainingS384;
    _M0L6_2atmpS1906 = 48 + _M0L6_2atmpS1907;
    _M0L6_2atmpS1905 = (uint16_t)_M0L6_2atmpS1906;
    _M0L6bufferS382[_M0L6_2atmpS1904] = _M0L6_2atmpS1905;
    moonbit_decref(_M0L6bufferS382);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS364,
  uint64_t _M0L3numS358,
  int32_t _M0L12digit__startS356,
  int32_t _M0L10total__lenS355,
  int32_t _M0L5radixS360
) {
  int32_t _M0Lm6offsetS354;
  uint64_t _M0Lm1nS357;
  uint64_t _M0L4baseS359;
  int32_t _M0L6_2atmpS1835;
  int32_t _M0L6_2atmpS1834;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS354 = _M0L10total__lenS355 - _M0L12digit__startS356;
  _M0Lm1nS357 = _M0L3numS358;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS359 = _M0MPC13int3Int10to__uint64(_M0L5radixS360);
  _M0L6_2atmpS1835 = _M0L5radixS360 - 1;
  _M0L6_2atmpS1834 = _M0L5radixS360 & _M0L6_2atmpS1835;
  if (_M0L6_2atmpS1834 == 0) {
    int32_t _M0L5shiftS361;
    uint64_t _M0L4maskS362;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS361 = moonbit_ctz32(_M0L5radixS360);
    _M0L4maskS362 = _M0L4baseS359 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1836 = _M0Lm1nS357;
      if (_M0L6_2atmpS1836 > 0ull) {
        int32_t _M0L6_2atmpS1837 = _M0Lm6offsetS354;
        uint64_t _M0L6_2atmpS1843;
        uint64_t _M0L6_2atmpS1842;
        int32_t _M0L5digitS363;
        int32_t _M0L6_2atmpS1840;
        int32_t _M0L6_2atmpS1838;
        int32_t _M0L6_2atmpS1839;
        uint64_t _M0L6_2atmpS1841;
        _M0Lm6offsetS354 = _M0L6_2atmpS1837 - 1;
        _M0L6_2atmpS1843 = _M0Lm1nS357;
        _M0L6_2atmpS1842 = _M0L6_2atmpS1843 & _M0L4maskS362;
        _M0L5digitS363 = (int32_t)_M0L6_2atmpS1842;
        _M0L6_2atmpS1840 = _M0Lm6offsetS354;
        _M0L6_2atmpS1838 = _M0L12digit__startS356 + _M0L6_2atmpS1840;
        _M0L6_2atmpS1839
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS363
        ];
        _M0L6bufferS364[_M0L6_2atmpS1838] = _M0L6_2atmpS1839;
        _M0L6_2atmpS1841 = _M0Lm1nS357;
        _M0Lm1nS357 = _M0L6_2atmpS1841 >> (_M0L5shiftS361 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS364);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1844 = _M0Lm1nS357;
      if (_M0L6_2atmpS1844 > 0ull) {
        int32_t _M0L6_2atmpS1845 = _M0Lm6offsetS354;
        uint64_t _M0L6_2atmpS1852;
        uint64_t _M0L1qS366;
        uint64_t _M0L6_2atmpS1850;
        uint64_t _M0L6_2atmpS1851;
        uint64_t _M0L6_2atmpS1849;
        int32_t _M0L5digitS367;
        int32_t _M0L6_2atmpS1848;
        int32_t _M0L6_2atmpS1846;
        int32_t _M0L6_2atmpS1847;
        _M0Lm6offsetS354 = _M0L6_2atmpS1845 - 1;
        _M0L6_2atmpS1852 = _M0Lm1nS357;
        _M0L1qS366 = _M0L6_2atmpS1852 / _M0L4baseS359;
        _M0L6_2atmpS1850 = _M0Lm1nS357;
        _M0L6_2atmpS1851 = _M0L1qS366 * _M0L4baseS359;
        _M0L6_2atmpS1849 = _M0L6_2atmpS1850 - _M0L6_2atmpS1851;
        _M0L5digitS367 = (int32_t)_M0L6_2atmpS1849;
        _M0L6_2atmpS1848 = _M0Lm6offsetS354;
        _M0L6_2atmpS1846 = _M0L12digit__startS356 + _M0L6_2atmpS1848;
        _M0L6_2atmpS1847
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS367
        ];
        _M0L6bufferS364[_M0L6_2atmpS1846] = _M0L6_2atmpS1847;
        _M0Lm1nS357 = _M0L1qS366;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS364);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS351,
  uint64_t _M0L3numS347,
  int32_t _M0L12digit__startS345,
  int32_t _M0L10total__lenS344
) {
  int32_t _M0Lm6offsetS343;
  uint64_t _M0Lm1nS346;
  int32_t _M0L6_2atmpS1830;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS343 = _M0L10total__lenS344 - _M0L12digit__startS345;
  _M0Lm1nS346 = _M0L3numS347;
  while (1) {
    int32_t _M0L6_2atmpS1818 = _M0Lm6offsetS343;
    if (_M0L6_2atmpS1818 >= 2) {
      int32_t _M0L6_2atmpS1819 = _M0Lm6offsetS343;
      uint64_t _M0L6_2atmpS1829;
      uint64_t _M0L6_2atmpS1828;
      int32_t _M0L9byte__valS348;
      int32_t _M0L2hiS349;
      int32_t _M0L2loS350;
      int32_t _M0L6_2atmpS1822;
      int32_t _M0L6_2atmpS1820;
      int32_t _M0L6_2atmpS1821;
      int32_t _M0L6_2atmpS1826;
      int32_t _M0L6_2atmpS1825;
      int32_t _M0L6_2atmpS1823;
      int32_t _M0L6_2atmpS1824;
      uint64_t _M0L6_2atmpS1827;
      _M0Lm6offsetS343 = _M0L6_2atmpS1819 - 2;
      _M0L6_2atmpS1829 = _M0Lm1nS346;
      _M0L6_2atmpS1828 = _M0L6_2atmpS1829 & 255ull;
      _M0L9byte__valS348 = (int32_t)_M0L6_2atmpS1828;
      _M0L2hiS349 = _M0L9byte__valS348 / 16;
      _M0L2loS350 = _M0L9byte__valS348 % 16;
      _M0L6_2atmpS1822 = _M0Lm6offsetS343;
      _M0L6_2atmpS1820 = _M0L12digit__startS345 + _M0L6_2atmpS1822;
      _M0L6_2atmpS1821
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2hiS349
      ];
      _M0L6bufferS351[_M0L6_2atmpS1820] = _M0L6_2atmpS1821;
      _M0L6_2atmpS1826 = _M0Lm6offsetS343;
      _M0L6_2atmpS1825 = _M0L12digit__startS345 + _M0L6_2atmpS1826;
      _M0L6_2atmpS1823 = _M0L6_2atmpS1825 + 1;
      _M0L6_2atmpS1824
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2loS350
      ];
      _M0L6bufferS351[_M0L6_2atmpS1823] = _M0L6_2atmpS1824;
      _M0L6_2atmpS1827 = _M0Lm1nS346;
      _M0Lm1nS346 = _M0L6_2atmpS1827 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1830 = _M0Lm6offsetS343;
  if (_M0L6_2atmpS1830 == 1) {
    uint64_t _M0L6_2atmpS1833 = _M0Lm1nS346;
    uint64_t _M0L6_2atmpS1832 = _M0L6_2atmpS1833 & 15ull;
    int32_t _M0L6nibbleS353 = (int32_t)_M0L6_2atmpS1832;
    int32_t _M0L6_2atmpS1831 =
      ((moonbit_string_t)moonbit_string_literal_57.data)[_M0L6nibbleS353];
    _M0L6bufferS351[_M0L12digit__startS345] = _M0L6_2atmpS1831;
    moonbit_decref(_M0L6bufferS351);
  } else {
    moonbit_decref(_M0L6bufferS351);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS337,
  int32_t _M0L5radixS340
) {
  uint64_t _M0Lm3numS338;
  uint64_t _M0L4baseS339;
  int32_t _M0Lm5countS341;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS337 == 0ull) {
    return 1;
  }
  _M0Lm3numS338 = _M0L5valueS337;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS339 = _M0MPC13int3Int10to__uint64(_M0L5radixS340);
  _M0Lm5countS341 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1815 = _M0Lm3numS338;
    if (_M0L6_2atmpS1815 > 0ull) {
      int32_t _M0L6_2atmpS1816 = _M0Lm5countS341;
      uint64_t _M0L6_2atmpS1817;
      _M0Lm5countS341 = _M0L6_2atmpS1816 + 1;
      _M0L6_2atmpS1817 = _M0Lm3numS338;
      _M0Lm3numS338 = _M0L6_2atmpS1817 / _M0L4baseS339;
      continue;
    }
    break;
  }
  return _M0Lm5countS341;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS335) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS335 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS336;
    int32_t _M0L6_2atmpS1814;
    int32_t _M0L6_2atmpS1813;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS336 = moonbit_clz64(_M0L5valueS335);
    _M0L6_2atmpS1814 = 63 - _M0L14leading__zerosS336;
    _M0L6_2atmpS1813 = _M0L6_2atmpS1814 / 4;
    return _M0L6_2atmpS1813 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS334) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS334 >= 10000000000ull) {
    if (_M0L5valueS334 >= 100000000000000ull) {
      if (_M0L5valueS334 >= 10000000000000000ull) {
        if (_M0L5valueS334 >= 1000000000000000000ull) {
          if (_M0L5valueS334 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS334 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS334 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS334 >= 1000000000000ull) {
      if (_M0L5valueS334 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS334 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS334 >= 100000ull) {
    if (_M0L5valueS334 >= 10000000ull) {
      if (_M0L5valueS334 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS334 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS334 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS334 >= 1000ull) {
    if (_M0L5valueS334 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS334 >= 100ull) {
    return 3;
  } else if (_M0L5valueS334 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS318,
  int32_t _M0L5radixS317
) {
  int32_t _if__result_4341;
  int32_t _M0L12is__negativeS319;
  uint32_t _M0L3numS320;
  uint16_t* _M0L6bufferS321;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS317 < 2) {
    _if__result_4341 = 1;
  } else {
    _if__result_4341 = _M0L5radixS317 > 36;
  }
  if (_if__result_4341) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_58.data);
  }
  if (_M0L4selfS318 == 0) {
    return (moonbit_string_t)moonbit_string_literal_44.data;
  }
  _M0L12is__negativeS319 = _M0L4selfS318 < 0;
  if (_M0L12is__negativeS319) {
    int32_t _M0L6_2atmpS1812 = -_M0L4selfS318;
    _M0L3numS320 = *(uint32_t*)&_M0L6_2atmpS1812;
  } else {
    _M0L3numS320 = *(uint32_t*)&_M0L4selfS318;
  }
  switch (_M0L5radixS317) {
    case 10: {
      int32_t _M0L10digit__lenS322;
      int32_t _M0L6_2atmpS1809;
      int32_t _M0L10total__lenS323;
      uint16_t* _M0L6bufferS324;
      int32_t _M0L12digit__startS325;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS322 = _M0FPB12dec__count32(_M0L3numS320);
      if (_M0L12is__negativeS319) {
        _M0L6_2atmpS1809 = 1;
      } else {
        _M0L6_2atmpS1809 = 0;
      }
      _M0L10total__lenS323 = _M0L10digit__lenS322 + _M0L6_2atmpS1809;
      _M0L6bufferS324
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS323, 0);
      if (_M0L12is__negativeS319) {
        _M0L12digit__startS325 = 1;
      } else {
        _M0L12digit__startS325 = 0;
      }
      moonbit_incref(_M0L6bufferS324);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS324, _M0L3numS320, _M0L12digit__startS325, _M0L10total__lenS323);
      _M0L6bufferS321 = _M0L6bufferS324;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS326;
      int32_t _M0L6_2atmpS1810;
      int32_t _M0L10total__lenS327;
      uint16_t* _M0L6bufferS328;
      int32_t _M0L12digit__startS329;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS326 = _M0FPB12hex__count32(_M0L3numS320);
      if (_M0L12is__negativeS319) {
        _M0L6_2atmpS1810 = 1;
      } else {
        _M0L6_2atmpS1810 = 0;
      }
      _M0L10total__lenS327 = _M0L10digit__lenS326 + _M0L6_2atmpS1810;
      _M0L6bufferS328
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS327, 0);
      if (_M0L12is__negativeS319) {
        _M0L12digit__startS329 = 1;
      } else {
        _M0L12digit__startS329 = 0;
      }
      moonbit_incref(_M0L6bufferS328);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS328, _M0L3numS320, _M0L12digit__startS329, _M0L10total__lenS327);
      _M0L6bufferS321 = _M0L6bufferS328;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS330;
      int32_t _M0L6_2atmpS1811;
      int32_t _M0L10total__lenS331;
      uint16_t* _M0L6bufferS332;
      int32_t _M0L12digit__startS333;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS330
      = _M0FPB14radix__count32(_M0L3numS320, _M0L5radixS317);
      if (_M0L12is__negativeS319) {
        _M0L6_2atmpS1811 = 1;
      } else {
        _M0L6_2atmpS1811 = 0;
      }
      _M0L10total__lenS331 = _M0L10digit__lenS330 + _M0L6_2atmpS1811;
      _M0L6bufferS332
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS331, 0);
      if (_M0L12is__negativeS319) {
        _M0L12digit__startS333 = 1;
      } else {
        _M0L12digit__startS333 = 0;
      }
      moonbit_incref(_M0L6bufferS332);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS332, _M0L3numS320, _M0L12digit__startS333, _M0L10total__lenS331, _M0L5radixS317);
      _M0L6bufferS321 = _M0L6bufferS332;
      break;
    }
  }
  if (_M0L12is__negativeS319) {
    _M0L6bufferS321[0] = 45;
  }
  return _M0L6bufferS321;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS311,
  int32_t _M0L5radixS314
) {
  uint32_t _M0Lm3numS312;
  uint32_t _M0L4baseS313;
  int32_t _M0Lm5countS315;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS311 == 0u) {
    return 1;
  }
  _M0Lm3numS312 = _M0L5valueS311;
  _M0L4baseS313 = *(uint32_t*)&_M0L5radixS314;
  _M0Lm5countS315 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1806 = _M0Lm3numS312;
    if (_M0L6_2atmpS1806 > 0u) {
      int32_t _M0L6_2atmpS1807 = _M0Lm5countS315;
      uint32_t _M0L6_2atmpS1808;
      _M0Lm5countS315 = _M0L6_2atmpS1807 + 1;
      _M0L6_2atmpS1808 = _M0Lm3numS312;
      _M0Lm3numS312 = _M0L6_2atmpS1808 / _M0L4baseS313;
      continue;
    }
    break;
  }
  return _M0Lm5countS315;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS309) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS309 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS310;
    int32_t _M0L6_2atmpS1805;
    int32_t _M0L6_2atmpS1804;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS310 = moonbit_clz32(_M0L5valueS309);
    _M0L6_2atmpS1805 = 31 - _M0L14leading__zerosS310;
    _M0L6_2atmpS1804 = _M0L6_2atmpS1805 / 4;
    return _M0L6_2atmpS1804 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS308) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS308 >= 100000u) {
    if (_M0L5valueS308 >= 10000000u) {
      if (_M0L5valueS308 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS308 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS308 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS308 >= 1000u) {
    if (_M0L5valueS308 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS308 >= 100u) {
    return 3;
  } else if (_M0L5valueS308 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS298,
  uint32_t _M0L3numS286,
  int32_t _M0L12digit__startS289,
  int32_t _M0L10total__lenS288
) {
  uint32_t _M0Lm3numS285;
  int32_t _M0Lm6offsetS287;
  uint32_t _M0L6_2atmpS1803;
  int32_t _M0Lm9remainingS300;
  int32_t _M0L6_2atmpS1784;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS285 = _M0L3numS286;
  _M0Lm6offsetS287 = _M0L10total__lenS288 - _M0L12digit__startS289;
  while (1) {
    uint32_t _M0L6_2atmpS1747 = _M0Lm3numS285;
    if (_M0L6_2atmpS1747 >= 10000u) {
      uint32_t _M0L6_2atmpS1770 = _M0Lm3numS285;
      uint32_t _M0L1tS290 = _M0L6_2atmpS1770 / 10000u;
      uint32_t _M0L6_2atmpS1769 = _M0Lm3numS285;
      uint32_t _M0L6_2atmpS1768 = _M0L6_2atmpS1769 % 10000u;
      int32_t _M0L1rS291 = *(int32_t*)&_M0L6_2atmpS1768;
      int32_t _M0L2d1S292;
      int32_t _M0L2d2S293;
      int32_t _M0L6_2atmpS1748;
      int32_t _M0L6_2atmpS1767;
      int32_t _M0L6_2atmpS1766;
      int32_t _M0L6d1__hiS294;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6d1__loS295;
      int32_t _M0L6_2atmpS1763;
      int32_t _M0L6_2atmpS1762;
      int32_t _M0L6d2__hiS296;
      int32_t _M0L6_2atmpS1761;
      int32_t _M0L6_2atmpS1760;
      int32_t _M0L6d2__loS297;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L6_2atmpS1749;
      int32_t _M0L6_2atmpS1753;
      int32_t _M0L6_2atmpS1752;
      int32_t _M0L6_2atmpS1751;
      int32_t _M0L6_2atmpS1756;
      int32_t _M0L6_2atmpS1755;
      int32_t _M0L6_2atmpS1754;
      int32_t _M0L6_2atmpS1759;
      int32_t _M0L6_2atmpS1758;
      int32_t _M0L6_2atmpS1757;
      _M0Lm3numS285 = _M0L1tS290;
      _M0L2d1S292 = _M0L1rS291 / 100;
      _M0L2d2S293 = _M0L1rS291 % 100;
      _M0L6_2atmpS1748 = _M0Lm6offsetS287;
      _M0Lm6offsetS287 = _M0L6_2atmpS1748 - 4;
      _M0L6_2atmpS1767 = _M0L2d1S292 / 10;
      _M0L6_2atmpS1766 = 48 + _M0L6_2atmpS1767;
      _M0L6d1__hiS294 = (uint16_t)_M0L6_2atmpS1766;
      _M0L6_2atmpS1765 = _M0L2d1S292 % 10;
      _M0L6_2atmpS1764 = 48 + _M0L6_2atmpS1765;
      _M0L6d1__loS295 = (uint16_t)_M0L6_2atmpS1764;
      _M0L6_2atmpS1763 = _M0L2d2S293 / 10;
      _M0L6_2atmpS1762 = 48 + _M0L6_2atmpS1763;
      _M0L6d2__hiS296 = (uint16_t)_M0L6_2atmpS1762;
      _M0L6_2atmpS1761 = _M0L2d2S293 % 10;
      _M0L6_2atmpS1760 = 48 + _M0L6_2atmpS1761;
      _M0L6d2__loS297 = (uint16_t)_M0L6_2atmpS1760;
      _M0L6_2atmpS1750 = _M0Lm6offsetS287;
      _M0L6_2atmpS1749 = _M0L12digit__startS289 + _M0L6_2atmpS1750;
      _M0L6bufferS298[_M0L6_2atmpS1749] = _M0L6d1__hiS294;
      _M0L6_2atmpS1753 = _M0Lm6offsetS287;
      _M0L6_2atmpS1752 = _M0L12digit__startS289 + _M0L6_2atmpS1753;
      _M0L6_2atmpS1751 = _M0L6_2atmpS1752 + 1;
      _M0L6bufferS298[_M0L6_2atmpS1751] = _M0L6d1__loS295;
      _M0L6_2atmpS1756 = _M0Lm6offsetS287;
      _M0L6_2atmpS1755 = _M0L12digit__startS289 + _M0L6_2atmpS1756;
      _M0L6_2atmpS1754 = _M0L6_2atmpS1755 + 2;
      _M0L6bufferS298[_M0L6_2atmpS1754] = _M0L6d2__hiS296;
      _M0L6_2atmpS1759 = _M0Lm6offsetS287;
      _M0L6_2atmpS1758 = _M0L12digit__startS289 + _M0L6_2atmpS1759;
      _M0L6_2atmpS1757 = _M0L6_2atmpS1758 + 3;
      _M0L6bufferS298[_M0L6_2atmpS1757] = _M0L6d2__loS297;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1803 = _M0Lm3numS285;
  _M0Lm9remainingS300 = *(int32_t*)&_M0L6_2atmpS1803;
  while (1) {
    int32_t _M0L6_2atmpS1771 = _M0Lm9remainingS300;
    if (_M0L6_2atmpS1771 >= 100) {
      int32_t _M0L6_2atmpS1783 = _M0Lm9remainingS300;
      int32_t _M0L1tS301 = _M0L6_2atmpS1783 / 100;
      int32_t _M0L6_2atmpS1782 = _M0Lm9remainingS300;
      int32_t _M0L1dS302 = _M0L6_2atmpS1782 % 100;
      int32_t _M0L6_2atmpS1772;
      int32_t _M0L6_2atmpS1781;
      int32_t _M0L6_2atmpS1780;
      int32_t _M0L5d__hiS303;
      int32_t _M0L6_2atmpS1779;
      int32_t _M0L6_2atmpS1778;
      int32_t _M0L5d__loS304;
      int32_t _M0L6_2atmpS1774;
      int32_t _M0L6_2atmpS1773;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6_2atmpS1776;
      int32_t _M0L6_2atmpS1775;
      _M0Lm9remainingS300 = _M0L1tS301;
      _M0L6_2atmpS1772 = _M0Lm6offsetS287;
      _M0Lm6offsetS287 = _M0L6_2atmpS1772 - 2;
      _M0L6_2atmpS1781 = _M0L1dS302 / 10;
      _M0L6_2atmpS1780 = 48 + _M0L6_2atmpS1781;
      _M0L5d__hiS303 = (uint16_t)_M0L6_2atmpS1780;
      _M0L6_2atmpS1779 = _M0L1dS302 % 10;
      _M0L6_2atmpS1778 = 48 + _M0L6_2atmpS1779;
      _M0L5d__loS304 = (uint16_t)_M0L6_2atmpS1778;
      _M0L6_2atmpS1774 = _M0Lm6offsetS287;
      _M0L6_2atmpS1773 = _M0L12digit__startS289 + _M0L6_2atmpS1774;
      _M0L6bufferS298[_M0L6_2atmpS1773] = _M0L5d__hiS303;
      _M0L6_2atmpS1777 = _M0Lm6offsetS287;
      _M0L6_2atmpS1776 = _M0L12digit__startS289 + _M0L6_2atmpS1777;
      _M0L6_2atmpS1775 = _M0L6_2atmpS1776 + 1;
      _M0L6bufferS298[_M0L6_2atmpS1775] = _M0L5d__loS304;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1784 = _M0Lm9remainingS300;
  if (_M0L6_2atmpS1784 >= 10) {
    int32_t _M0L6_2atmpS1785 = _M0Lm6offsetS287;
    int32_t _M0L6_2atmpS1796;
    int32_t _M0L6_2atmpS1795;
    int32_t _M0L6_2atmpS1794;
    int32_t _M0L5d__hiS306;
    int32_t _M0L6_2atmpS1793;
    int32_t _M0L6_2atmpS1792;
    int32_t _M0L6_2atmpS1791;
    int32_t _M0L5d__loS307;
    int32_t _M0L6_2atmpS1787;
    int32_t _M0L6_2atmpS1786;
    int32_t _M0L6_2atmpS1790;
    int32_t _M0L6_2atmpS1789;
    int32_t _M0L6_2atmpS1788;
    _M0Lm6offsetS287 = _M0L6_2atmpS1785 - 2;
    _M0L6_2atmpS1796 = _M0Lm9remainingS300;
    _M0L6_2atmpS1795 = _M0L6_2atmpS1796 / 10;
    _M0L6_2atmpS1794 = 48 + _M0L6_2atmpS1795;
    _M0L5d__hiS306 = (uint16_t)_M0L6_2atmpS1794;
    _M0L6_2atmpS1793 = _M0Lm9remainingS300;
    _M0L6_2atmpS1792 = _M0L6_2atmpS1793 % 10;
    _M0L6_2atmpS1791 = 48 + _M0L6_2atmpS1792;
    _M0L5d__loS307 = (uint16_t)_M0L6_2atmpS1791;
    _M0L6_2atmpS1787 = _M0Lm6offsetS287;
    _M0L6_2atmpS1786 = _M0L12digit__startS289 + _M0L6_2atmpS1787;
    _M0L6bufferS298[_M0L6_2atmpS1786] = _M0L5d__hiS306;
    _M0L6_2atmpS1790 = _M0Lm6offsetS287;
    _M0L6_2atmpS1789 = _M0L12digit__startS289 + _M0L6_2atmpS1790;
    _M0L6_2atmpS1788 = _M0L6_2atmpS1789 + 1;
    _M0L6bufferS298[_M0L6_2atmpS1788] = _M0L5d__loS307;
    moonbit_decref(_M0L6bufferS298);
  } else {
    int32_t _M0L6_2atmpS1797 = _M0Lm6offsetS287;
    int32_t _M0L6_2atmpS1802;
    int32_t _M0L6_2atmpS1798;
    int32_t _M0L6_2atmpS1801;
    int32_t _M0L6_2atmpS1800;
    int32_t _M0L6_2atmpS1799;
    _M0Lm6offsetS287 = _M0L6_2atmpS1797 - 1;
    _M0L6_2atmpS1802 = _M0Lm6offsetS287;
    _M0L6_2atmpS1798 = _M0L12digit__startS289 + _M0L6_2atmpS1802;
    _M0L6_2atmpS1801 = _M0Lm9remainingS300;
    _M0L6_2atmpS1800 = 48 + _M0L6_2atmpS1801;
    _M0L6_2atmpS1799 = (uint16_t)_M0L6_2atmpS1800;
    _M0L6bufferS298[_M0L6_2atmpS1798] = _M0L6_2atmpS1799;
    moonbit_decref(_M0L6bufferS298);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS280,
  uint32_t _M0L3numS274,
  int32_t _M0L12digit__startS272,
  int32_t _M0L10total__lenS271,
  int32_t _M0L5radixS276
) {
  int32_t _M0Lm6offsetS270;
  uint32_t _M0Lm1nS273;
  uint32_t _M0L4baseS275;
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L6_2atmpS1728;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS270 = _M0L10total__lenS271 - _M0L12digit__startS272;
  _M0Lm1nS273 = _M0L3numS274;
  _M0L4baseS275 = *(uint32_t*)&_M0L5radixS276;
  _M0L6_2atmpS1729 = _M0L5radixS276 - 1;
  _M0L6_2atmpS1728 = _M0L5radixS276 & _M0L6_2atmpS1729;
  if (_M0L6_2atmpS1728 == 0) {
    int32_t _M0L5shiftS277;
    uint32_t _M0L4maskS278;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS277 = moonbit_ctz32(_M0L5radixS276);
    _M0L4maskS278 = _M0L4baseS275 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1730 = _M0Lm1nS273;
      if (_M0L6_2atmpS1730 > 0u) {
        int32_t _M0L6_2atmpS1731 = _M0Lm6offsetS270;
        uint32_t _M0L6_2atmpS1737;
        uint32_t _M0L6_2atmpS1736;
        int32_t _M0L5digitS279;
        int32_t _M0L6_2atmpS1734;
        int32_t _M0L6_2atmpS1732;
        int32_t _M0L6_2atmpS1733;
        uint32_t _M0L6_2atmpS1735;
        _M0Lm6offsetS270 = _M0L6_2atmpS1731 - 1;
        _M0L6_2atmpS1737 = _M0Lm1nS273;
        _M0L6_2atmpS1736 = _M0L6_2atmpS1737 & _M0L4maskS278;
        _M0L5digitS279 = *(int32_t*)&_M0L6_2atmpS1736;
        _M0L6_2atmpS1734 = _M0Lm6offsetS270;
        _M0L6_2atmpS1732 = _M0L12digit__startS272 + _M0L6_2atmpS1734;
        _M0L6_2atmpS1733
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS279
        ];
        _M0L6bufferS280[_M0L6_2atmpS1732] = _M0L6_2atmpS1733;
        _M0L6_2atmpS1735 = _M0Lm1nS273;
        _M0Lm1nS273 = _M0L6_2atmpS1735 >> (_M0L5shiftS277 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS280);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1738 = _M0Lm1nS273;
      if (_M0L6_2atmpS1738 > 0u) {
        int32_t _M0L6_2atmpS1739 = _M0Lm6offsetS270;
        uint32_t _M0L6_2atmpS1746;
        uint32_t _M0L1qS282;
        uint32_t _M0L6_2atmpS1744;
        uint32_t _M0L6_2atmpS1745;
        uint32_t _M0L6_2atmpS1743;
        int32_t _M0L5digitS283;
        int32_t _M0L6_2atmpS1742;
        int32_t _M0L6_2atmpS1740;
        int32_t _M0L6_2atmpS1741;
        _M0Lm6offsetS270 = _M0L6_2atmpS1739 - 1;
        _M0L6_2atmpS1746 = _M0Lm1nS273;
        _M0L1qS282 = _M0L6_2atmpS1746 / _M0L4baseS275;
        _M0L6_2atmpS1744 = _M0Lm1nS273;
        _M0L6_2atmpS1745 = _M0L1qS282 * _M0L4baseS275;
        _M0L6_2atmpS1743 = _M0L6_2atmpS1744 - _M0L6_2atmpS1745;
        _M0L5digitS283 = *(int32_t*)&_M0L6_2atmpS1743;
        _M0L6_2atmpS1742 = _M0Lm6offsetS270;
        _M0L6_2atmpS1740 = _M0L12digit__startS272 + _M0L6_2atmpS1742;
        _M0L6_2atmpS1741
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS283
        ];
        _M0L6bufferS280[_M0L6_2atmpS1740] = _M0L6_2atmpS1741;
        _M0Lm1nS273 = _M0L1qS282;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS280);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS267,
  uint32_t _M0L3numS263,
  int32_t _M0L12digit__startS261,
  int32_t _M0L10total__lenS260
) {
  int32_t _M0Lm6offsetS259;
  uint32_t _M0Lm1nS262;
  int32_t _M0L6_2atmpS1724;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS259 = _M0L10total__lenS260 - _M0L12digit__startS261;
  _M0Lm1nS262 = _M0L3numS263;
  while (1) {
    int32_t _M0L6_2atmpS1712 = _M0Lm6offsetS259;
    if (_M0L6_2atmpS1712 >= 2) {
      int32_t _M0L6_2atmpS1713 = _M0Lm6offsetS259;
      uint32_t _M0L6_2atmpS1723;
      uint32_t _M0L6_2atmpS1722;
      int32_t _M0L9byte__valS264;
      int32_t _M0L2hiS265;
      int32_t _M0L2loS266;
      int32_t _M0L6_2atmpS1716;
      int32_t _M0L6_2atmpS1714;
      int32_t _M0L6_2atmpS1715;
      int32_t _M0L6_2atmpS1720;
      int32_t _M0L6_2atmpS1719;
      int32_t _M0L6_2atmpS1717;
      int32_t _M0L6_2atmpS1718;
      uint32_t _M0L6_2atmpS1721;
      _M0Lm6offsetS259 = _M0L6_2atmpS1713 - 2;
      _M0L6_2atmpS1723 = _M0Lm1nS262;
      _M0L6_2atmpS1722 = _M0L6_2atmpS1723 & 255u;
      _M0L9byte__valS264 = *(int32_t*)&_M0L6_2atmpS1722;
      _M0L2hiS265 = _M0L9byte__valS264 / 16;
      _M0L2loS266 = _M0L9byte__valS264 % 16;
      _M0L6_2atmpS1716 = _M0Lm6offsetS259;
      _M0L6_2atmpS1714 = _M0L12digit__startS261 + _M0L6_2atmpS1716;
      _M0L6_2atmpS1715
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2hiS265
      ];
      _M0L6bufferS267[_M0L6_2atmpS1714] = _M0L6_2atmpS1715;
      _M0L6_2atmpS1720 = _M0Lm6offsetS259;
      _M0L6_2atmpS1719 = _M0L12digit__startS261 + _M0L6_2atmpS1720;
      _M0L6_2atmpS1717 = _M0L6_2atmpS1719 + 1;
      _M0L6_2atmpS1718
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2loS266
      ];
      _M0L6bufferS267[_M0L6_2atmpS1717] = _M0L6_2atmpS1718;
      _M0L6_2atmpS1721 = _M0Lm1nS262;
      _M0Lm1nS262 = _M0L6_2atmpS1721 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1724 = _M0Lm6offsetS259;
  if (_M0L6_2atmpS1724 == 1) {
    uint32_t _M0L6_2atmpS1727 = _M0Lm1nS262;
    uint32_t _M0L6_2atmpS1726 = _M0L6_2atmpS1727 & 15u;
    int32_t _M0L6nibbleS269 = *(int32_t*)&_M0L6_2atmpS1726;
    int32_t _M0L6_2atmpS1725 =
      ((moonbit_string_t)moonbit_string_literal_57.data)[_M0L6nibbleS269];
    _M0L6bufferS267[_M0L12digit__startS261] = _M0L6_2atmpS1725;
    moonbit_decref(_M0L6bufferS267);
  } else {
    moonbit_decref(_M0L6bufferS267);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS254) {
  struct _M0TWEOs* _M0L7_2afuncS253;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS253 = _M0L4selfS254;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS253->code(_M0L7_2afuncS253);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS256
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS255;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS255 = _M0L4selfS256;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS255->code(_M0L7_2afuncS255);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS258) {
  struct _M0TWEOc* _M0L7_2afuncS257;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS257 = _M0L4selfS258;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS257->code(_M0L7_2afuncS257);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS244
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS243;
  struct _M0TPB6Logger _M0L6_2atmpS1707;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS243 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS243);
  _M0L6_2atmpS1707
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS243
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS244, _M0L6_2atmpS1707);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS243);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS246
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS245;
  struct _M0TPB6Logger _M0L6_2atmpS1708;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS245 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS245);
  _M0L6_2atmpS1708
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS245
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS246, _M0L6_2atmpS1708);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS245);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS247;
  struct _M0TPB6Logger _M0L6_2atmpS1709;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS247);
  _M0L6_2atmpS1709
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS247
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS248, _M0L6_2atmpS1709);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS247);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(
  void* _M0L4selfS250
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS249;
  struct _M0TPB6Logger _M0L6_2atmpS1710;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS249 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS249);
  _M0L6_2atmpS1710
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS249
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(_M0L4selfS250, _M0L6_2atmpS1710);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS249);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS252
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS251;
  struct _M0TPB6Logger _M0L6_2atmpS1711;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS251 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS251);
  _M0L6_2atmpS1711
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS251
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS252, _M0L6_2atmpS1711);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS251);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS242
) {
  int32_t _M0L8_2afieldS4071;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4071 = _M0L4selfS242.$1;
  moonbit_decref(_M0L4selfS242.$0);
  return _M0L8_2afieldS4071;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS241
) {
  int32_t _M0L3endS1705;
  int32_t _M0L8_2afieldS4072;
  int32_t _M0L5startS1706;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1705 = _M0L4selfS241.$2;
  _M0L8_2afieldS4072 = _M0L4selfS241.$1;
  moonbit_decref(_M0L4selfS241.$0);
  _M0L5startS1706 = _M0L8_2afieldS4072;
  return _M0L3endS1705 - _M0L5startS1706;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS240
) {
  moonbit_string_t _M0L8_2afieldS4073;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4073 = _M0L4selfS240.$0;
  return _M0L8_2afieldS4073;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS236,
  moonbit_string_t _M0L5valueS237,
  int32_t _M0L5startS238,
  int32_t _M0L3lenS239
) {
  int32_t _M0L6_2atmpS1704;
  int64_t _M0L6_2atmpS1703;
  struct _M0TPC16string10StringView _M0L6_2atmpS1702;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1704 = _M0L5startS238 + _M0L3lenS239;
  _M0L6_2atmpS1703 = (int64_t)_M0L6_2atmpS1704;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1702
  = _M0MPC16string6String11sub_2einner(_M0L5valueS237, _M0L5startS238, _M0L6_2atmpS1703);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS236, _M0L6_2atmpS1702);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS229,
  int32_t _M0L5startS235,
  int64_t _M0L3endS231
) {
  int32_t _M0L3lenS228;
  int32_t _M0L3endS230;
  int32_t _M0L5startS234;
  int32_t _if__result_4348;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS228 = Moonbit_array_length(_M0L4selfS229);
  if (_M0L3endS231 == 4294967296ll) {
    _M0L3endS230 = _M0L3lenS228;
  } else {
    int64_t _M0L7_2aSomeS232 = _M0L3endS231;
    int32_t _M0L6_2aendS233 = (int32_t)_M0L7_2aSomeS232;
    if (_M0L6_2aendS233 < 0) {
      _M0L3endS230 = _M0L3lenS228 + _M0L6_2aendS233;
    } else {
      _M0L3endS230 = _M0L6_2aendS233;
    }
  }
  if (_M0L5startS235 < 0) {
    _M0L5startS234 = _M0L3lenS228 + _M0L5startS235;
  } else {
    _M0L5startS234 = _M0L5startS235;
  }
  if (_M0L5startS234 >= 0) {
    if (_M0L5startS234 <= _M0L3endS230) {
      _if__result_4348 = _M0L3endS230 <= _M0L3lenS228;
    } else {
      _if__result_4348 = 0;
    }
  } else {
    _if__result_4348 = 0;
  }
  if (_if__result_4348) {
    if (_M0L5startS234 < _M0L3lenS228) {
      int32_t _M0L6_2atmpS1699 = _M0L4selfS229[_M0L5startS234];
      int32_t _M0L6_2atmpS1698;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1698
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1699);
      if (!_M0L6_2atmpS1698) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS230 < _M0L3lenS228) {
      int32_t _M0L6_2atmpS1701 = _M0L4selfS229[_M0L3endS230];
      int32_t _M0L6_2atmpS1700;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1700
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1701);
      if (!_M0L6_2atmpS1700) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS234,
                                                 _M0L3endS230,
                                                 _M0L4selfS229};
  } else {
    moonbit_decref(_M0L4selfS229);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS225) {
  struct _M0TPB6Hasher* _M0L1hS224;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS224 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS224);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS224, _M0L4selfS225);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS224);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS227
) {
  struct _M0TPB6Hasher* _M0L1hS226;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS226 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS226);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS226, _M0L4selfS227);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS226);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS222) {
  int32_t _M0L4seedS221;
  if (_M0L10seed_2eoptS222 == 4294967296ll) {
    _M0L4seedS221 = 0;
  } else {
    int64_t _M0L7_2aSomeS223 = _M0L10seed_2eoptS222;
    _M0L4seedS221 = (int32_t)_M0L7_2aSomeS223;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS221);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS220) {
  uint32_t _M0L6_2atmpS1697;
  uint32_t _M0L6_2atmpS1696;
  struct _M0TPB6Hasher* _block_4349;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1697 = *(uint32_t*)&_M0L4seedS220;
  _M0L6_2atmpS1696 = _M0L6_2atmpS1697 + 374761393u;
  _block_4349
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4349)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4349->$0 = _M0L6_2atmpS1696;
  return _block_4349;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS219) {
  uint32_t _M0L6_2atmpS1695;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1695 = _M0MPB6Hasher9avalanche(_M0L4selfS219);
  return *(int32_t*)&_M0L6_2atmpS1695;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS218) {
  uint32_t _M0L8_2afieldS4074;
  uint32_t _M0Lm3accS217;
  uint32_t _M0L6_2atmpS1684;
  uint32_t _M0L6_2atmpS1686;
  uint32_t _M0L6_2atmpS1685;
  uint32_t _M0L6_2atmpS1687;
  uint32_t _M0L6_2atmpS1688;
  uint32_t _M0L6_2atmpS1690;
  uint32_t _M0L6_2atmpS1689;
  uint32_t _M0L6_2atmpS1691;
  uint32_t _M0L6_2atmpS1692;
  uint32_t _M0L6_2atmpS1694;
  uint32_t _M0L6_2atmpS1693;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4074 = _M0L4selfS218->$0;
  moonbit_decref(_M0L4selfS218);
  _M0Lm3accS217 = _M0L8_2afieldS4074;
  _M0L6_2atmpS1684 = _M0Lm3accS217;
  _M0L6_2atmpS1686 = _M0Lm3accS217;
  _M0L6_2atmpS1685 = _M0L6_2atmpS1686 >> 15;
  _M0Lm3accS217 = _M0L6_2atmpS1684 ^ _M0L6_2atmpS1685;
  _M0L6_2atmpS1687 = _M0Lm3accS217;
  _M0Lm3accS217 = _M0L6_2atmpS1687 * 2246822519u;
  _M0L6_2atmpS1688 = _M0Lm3accS217;
  _M0L6_2atmpS1690 = _M0Lm3accS217;
  _M0L6_2atmpS1689 = _M0L6_2atmpS1690 >> 13;
  _M0Lm3accS217 = _M0L6_2atmpS1688 ^ _M0L6_2atmpS1689;
  _M0L6_2atmpS1691 = _M0Lm3accS217;
  _M0Lm3accS217 = _M0L6_2atmpS1691 * 3266489917u;
  _M0L6_2atmpS1692 = _M0Lm3accS217;
  _M0L6_2atmpS1694 = _M0Lm3accS217;
  _M0L6_2atmpS1693 = _M0L6_2atmpS1694 >> 16;
  _M0Lm3accS217 = _M0L6_2atmpS1692 ^ _M0L6_2atmpS1693;
  return _M0Lm3accS217;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS215,
  moonbit_string_t _M0L1yS216
) {
  int32_t _M0L6_2atmpS4075;
  int32_t _M0L6_2atmpS1683;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4075 = moonbit_val_array_equal(_M0L1xS215, _M0L1yS216);
  moonbit_decref(_M0L1xS215);
  moonbit_decref(_M0L1yS216);
  _M0L6_2atmpS1683 = _M0L6_2atmpS4075;
  return !_M0L6_2atmpS1683;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS212,
  int32_t _M0L5valueS211
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS211, _M0L4selfS212);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS214,
  moonbit_string_t _M0L5valueS213
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS213, _M0L4selfS214);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS210) {
  int64_t _M0L6_2atmpS1682;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1682 = (int64_t)_M0L4selfS210;
  return *(uint64_t*)&_M0L6_2atmpS1682;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS208,
  moonbit_string_t _M0L4reprS209
) {
  void* _block_4350;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4350 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_4350)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_4350)->$0 = _M0L6numberS208;
  ((struct _M0DTPB4Json6Number*)_block_4350)->$1 = _M0L4reprS209;
  return _block_4350;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS206,
  int32_t _M0L5valueS207
) {
  uint32_t _M0L6_2atmpS1681;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1681 = *(uint32_t*)&_M0L5valueS207;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS206, _M0L6_2atmpS1681);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS199
) {
  struct _M0TPB13StringBuilder* _M0L3bufS197;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS198;
  int32_t _M0L7_2abindS200;
  int32_t _M0L1iS201;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS197 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS198 = _M0L4selfS199;
  moonbit_incref(_M0L3bufS197);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS197, 91);
  _M0L7_2abindS200 = _M0L7_2aselfS198->$1;
  _M0L1iS201 = 0;
  while (1) {
    if (_M0L1iS201 < _M0L7_2abindS200) {
      int32_t _if__result_4352;
      moonbit_string_t* _M0L8_2afieldS4077;
      moonbit_string_t* _M0L3bufS1679;
      moonbit_string_t _M0L6_2atmpS4076;
      moonbit_string_t _M0L4itemS202;
      int32_t _M0L6_2atmpS1680;
      if (_M0L1iS201 != 0) {
        moonbit_incref(_M0L3bufS197);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, (moonbit_string_t)moonbit_string_literal_59.data);
      }
      if (_M0L1iS201 < 0) {
        _if__result_4352 = 1;
      } else {
        int32_t _M0L3lenS1678 = _M0L7_2aselfS198->$1;
        _if__result_4352 = _M0L1iS201 >= _M0L3lenS1678;
      }
      if (_if__result_4352) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4077 = _M0L7_2aselfS198->$0;
      _M0L3bufS1679 = _M0L8_2afieldS4077;
      _M0L6_2atmpS4076 = (moonbit_string_t)_M0L3bufS1679[_M0L1iS201];
      _M0L4itemS202 = _M0L6_2atmpS4076;
      if (_M0L4itemS202 == 0) {
        moonbit_incref(_M0L3bufS197);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, (moonbit_string_t)moonbit_string_literal_22.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS203 = _M0L4itemS202;
        moonbit_string_t _M0L6_2alocS204 = _M0L7_2aSomeS203;
        moonbit_string_t _M0L6_2atmpS1677;
        moonbit_incref(_M0L6_2alocS204);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1677
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS204);
        moonbit_incref(_M0L3bufS197);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, _M0L6_2atmpS1677);
      }
      _M0L6_2atmpS1680 = _M0L1iS201 + 1;
      _M0L1iS201 = _M0L6_2atmpS1680;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS198);
    }
    break;
  }
  moonbit_incref(_M0L3bufS197);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS197, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS197);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS196
) {
  moonbit_string_t _M0L6_2atmpS1676;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1675;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1676 = _M0L4selfS196;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1675 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1676);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1675);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS195
) {
  struct _M0TPB13StringBuilder* _M0L2sbS194;
  struct _M0TPC16string10StringView _M0L8_2afieldS4090;
  struct _M0TPC16string10StringView _M0L3pkgS1660;
  moonbit_string_t _M0L6_2atmpS1659;
  moonbit_string_t _M0L6_2atmpS4089;
  moonbit_string_t _M0L6_2atmpS1658;
  moonbit_string_t _M0L6_2atmpS4088;
  moonbit_string_t _M0L6_2atmpS1657;
  struct _M0TPC16string10StringView _M0L8_2afieldS4087;
  struct _M0TPC16string10StringView _M0L8filenameS1661;
  struct _M0TPC16string10StringView _M0L8_2afieldS4086;
  struct _M0TPC16string10StringView _M0L11start__lineS1664;
  moonbit_string_t _M0L6_2atmpS1663;
  moonbit_string_t _M0L6_2atmpS4085;
  moonbit_string_t _M0L6_2atmpS1662;
  struct _M0TPC16string10StringView _M0L8_2afieldS4084;
  struct _M0TPC16string10StringView _M0L13start__columnS1667;
  moonbit_string_t _M0L6_2atmpS1666;
  moonbit_string_t _M0L6_2atmpS4083;
  moonbit_string_t _M0L6_2atmpS1665;
  struct _M0TPC16string10StringView _M0L8_2afieldS4082;
  struct _M0TPC16string10StringView _M0L9end__lineS1670;
  moonbit_string_t _M0L6_2atmpS1669;
  moonbit_string_t _M0L6_2atmpS4081;
  moonbit_string_t _M0L6_2atmpS1668;
  struct _M0TPC16string10StringView _M0L8_2afieldS4080;
  int32_t _M0L6_2acntS4202;
  struct _M0TPC16string10StringView _M0L11end__columnS1674;
  moonbit_string_t _M0L6_2atmpS1673;
  moonbit_string_t _M0L6_2atmpS4079;
  moonbit_string_t _M0L6_2atmpS1672;
  moonbit_string_t _M0L6_2atmpS4078;
  moonbit_string_t _M0L6_2atmpS1671;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS194 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4090
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$0_1, _M0L4selfS195->$0_2, _M0L4selfS195->$0_0
  };
  _M0L3pkgS1660 = _M0L8_2afieldS4090;
  moonbit_incref(_M0L3pkgS1660.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1659
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1660);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4089
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_60.data, _M0L6_2atmpS1659);
  moonbit_decref(_M0L6_2atmpS1659);
  _M0L6_2atmpS1658 = _M0L6_2atmpS4089;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4088
  = moonbit_add_string(_M0L6_2atmpS1658, (moonbit_string_t)moonbit_string_literal_61.data);
  moonbit_decref(_M0L6_2atmpS1658);
  _M0L6_2atmpS1657 = _M0L6_2atmpS4088;
  moonbit_incref(_M0L2sbS194);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1657);
  moonbit_incref(_M0L2sbS194);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, (moonbit_string_t)moonbit_string_literal_62.data);
  _M0L8_2afieldS4087
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$1_1, _M0L4selfS195->$1_2, _M0L4selfS195->$1_0
  };
  _M0L8filenameS1661 = _M0L8_2afieldS4087;
  moonbit_incref(_M0L8filenameS1661.$0);
  moonbit_incref(_M0L2sbS194);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS194, _M0L8filenameS1661);
  _M0L8_2afieldS4086
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$2_1, _M0L4selfS195->$2_2, _M0L4selfS195->$2_0
  };
  _M0L11start__lineS1664 = _M0L8_2afieldS4086;
  moonbit_incref(_M0L11start__lineS1664.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1663
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1664);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4085
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_63.data, _M0L6_2atmpS1663);
  moonbit_decref(_M0L6_2atmpS1663);
  _M0L6_2atmpS1662 = _M0L6_2atmpS4085;
  moonbit_incref(_M0L2sbS194);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1662);
  _M0L8_2afieldS4084
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$3_1, _M0L4selfS195->$3_2, _M0L4selfS195->$3_0
  };
  _M0L13start__columnS1667 = _M0L8_2afieldS4084;
  moonbit_incref(_M0L13start__columnS1667.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1666
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1667);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4083
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_64.data, _M0L6_2atmpS1666);
  moonbit_decref(_M0L6_2atmpS1666);
  _M0L6_2atmpS1665 = _M0L6_2atmpS4083;
  moonbit_incref(_M0L2sbS194);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1665);
  _M0L8_2afieldS4082
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$4_1, _M0L4selfS195->$4_2, _M0L4selfS195->$4_0
  };
  _M0L9end__lineS1670 = _M0L8_2afieldS4082;
  moonbit_incref(_M0L9end__lineS1670.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1669
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1670);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4081
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_65.data, _M0L6_2atmpS1669);
  moonbit_decref(_M0L6_2atmpS1669);
  _M0L6_2atmpS1668 = _M0L6_2atmpS4081;
  moonbit_incref(_M0L2sbS194);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1668);
  _M0L8_2afieldS4080
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$5_1, _M0L4selfS195->$5_2, _M0L4selfS195->$5_0
  };
  _M0L6_2acntS4202 = Moonbit_object_header(_M0L4selfS195)->rc;
  if (_M0L6_2acntS4202 > 1) {
    int32_t _M0L11_2anew__cntS4208 = _M0L6_2acntS4202 - 1;
    Moonbit_object_header(_M0L4selfS195)->rc = _M0L11_2anew__cntS4208;
    moonbit_incref(_M0L8_2afieldS4080.$0);
  } else if (_M0L6_2acntS4202 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4207 =
      (struct _M0TPC16string10StringView){_M0L4selfS195->$4_1,
                                            _M0L4selfS195->$4_2,
                                            _M0L4selfS195->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4206;
    struct _M0TPC16string10StringView _M0L8_2afieldS4205;
    struct _M0TPC16string10StringView _M0L8_2afieldS4204;
    struct _M0TPC16string10StringView _M0L8_2afieldS4203;
    moonbit_decref(_M0L8_2afieldS4207.$0);
    _M0L8_2afieldS4206
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$3_1, _M0L4selfS195->$3_2, _M0L4selfS195->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4206.$0);
    _M0L8_2afieldS4205
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$2_1, _M0L4selfS195->$2_2, _M0L4selfS195->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4205.$0);
    _M0L8_2afieldS4204
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$1_1, _M0L4selfS195->$1_2, _M0L4selfS195->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4204.$0);
    _M0L8_2afieldS4203
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$0_1, _M0L4selfS195->$0_2, _M0L4selfS195->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4203.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS195);
  }
  _M0L11end__columnS1674 = _M0L8_2afieldS4080;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1673
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1674);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4079
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS1673);
  moonbit_decref(_M0L6_2atmpS1673);
  _M0L6_2atmpS1672 = _M0L6_2atmpS4079;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4078
  = moonbit_add_string(_M0L6_2atmpS1672, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1672);
  _M0L6_2atmpS1671 = _M0L6_2atmpS4078;
  moonbit_incref(_M0L2sbS194);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1671);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS194);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS192,
  moonbit_string_t _M0L3strS193
) {
  int32_t _M0L3lenS1647;
  int32_t _M0L6_2atmpS1649;
  int32_t _M0L6_2atmpS1648;
  int32_t _M0L6_2atmpS1646;
  moonbit_bytes_t _M0L8_2afieldS4092;
  moonbit_bytes_t _M0L4dataS1650;
  int32_t _M0L3lenS1651;
  int32_t _M0L6_2atmpS1652;
  int32_t _M0L3lenS1654;
  int32_t _M0L6_2atmpS4091;
  int32_t _M0L6_2atmpS1656;
  int32_t _M0L6_2atmpS1655;
  int32_t _M0L6_2atmpS1653;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1647 = _M0L4selfS192->$1;
  _M0L6_2atmpS1649 = Moonbit_array_length(_M0L3strS193);
  _M0L6_2atmpS1648 = _M0L6_2atmpS1649 * 2;
  _M0L6_2atmpS1646 = _M0L3lenS1647 + _M0L6_2atmpS1648;
  moonbit_incref(_M0L4selfS192);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS192, _M0L6_2atmpS1646);
  _M0L8_2afieldS4092 = _M0L4selfS192->$0;
  _M0L4dataS1650 = _M0L8_2afieldS4092;
  _M0L3lenS1651 = _M0L4selfS192->$1;
  _M0L6_2atmpS1652 = Moonbit_array_length(_M0L3strS193);
  moonbit_incref(_M0L4dataS1650);
  moonbit_incref(_M0L3strS193);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1650, _M0L3lenS1651, _M0L3strS193, 0, _M0L6_2atmpS1652);
  _M0L3lenS1654 = _M0L4selfS192->$1;
  _M0L6_2atmpS4091 = Moonbit_array_length(_M0L3strS193);
  moonbit_decref(_M0L3strS193);
  _M0L6_2atmpS1656 = _M0L6_2atmpS4091;
  _M0L6_2atmpS1655 = _M0L6_2atmpS1656 * 2;
  _M0L6_2atmpS1653 = _M0L3lenS1654 + _M0L6_2atmpS1655;
  _M0L4selfS192->$1 = _M0L6_2atmpS1653;
  moonbit_decref(_M0L4selfS192);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS184,
  int32_t _M0L13bytes__offsetS179,
  moonbit_string_t _M0L3strS186,
  int32_t _M0L11str__offsetS182,
  int32_t _M0L6lengthS180
) {
  int32_t _M0L6_2atmpS1645;
  int32_t _M0L6_2atmpS1644;
  int32_t _M0L2e1S178;
  int32_t _M0L6_2atmpS1643;
  int32_t _M0L2e2S181;
  int32_t _M0L4len1S183;
  int32_t _M0L4len2S185;
  int32_t _if__result_4353;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1645 = _M0L6lengthS180 * 2;
  _M0L6_2atmpS1644 = _M0L13bytes__offsetS179 + _M0L6_2atmpS1645;
  _M0L2e1S178 = _M0L6_2atmpS1644 - 1;
  _M0L6_2atmpS1643 = _M0L11str__offsetS182 + _M0L6lengthS180;
  _M0L2e2S181 = _M0L6_2atmpS1643 - 1;
  _M0L4len1S183 = Moonbit_array_length(_M0L4selfS184);
  _M0L4len2S185 = Moonbit_array_length(_M0L3strS186);
  if (_M0L6lengthS180 >= 0) {
    if (_M0L13bytes__offsetS179 >= 0) {
      if (_M0L2e1S178 < _M0L4len1S183) {
        if (_M0L11str__offsetS182 >= 0) {
          _if__result_4353 = _M0L2e2S181 < _M0L4len2S185;
        } else {
          _if__result_4353 = 0;
        }
      } else {
        _if__result_4353 = 0;
      }
    } else {
      _if__result_4353 = 0;
    }
  } else {
    _if__result_4353 = 0;
  }
  if (_if__result_4353) {
    int32_t _M0L16end__str__offsetS187 =
      _M0L11str__offsetS182 + _M0L6lengthS180;
    int32_t _M0L1iS188 = _M0L11str__offsetS182;
    int32_t _M0L1jS189 = _M0L13bytes__offsetS179;
    while (1) {
      if (_M0L1iS188 < _M0L16end__str__offsetS187) {
        int32_t _M0L6_2atmpS1640 = _M0L3strS186[_M0L1iS188];
        int32_t _M0L6_2atmpS1639 = (int32_t)_M0L6_2atmpS1640;
        uint32_t _M0L1cS190 = *(uint32_t*)&_M0L6_2atmpS1639;
        uint32_t _M0L6_2atmpS1635 = _M0L1cS190 & 255u;
        int32_t _M0L6_2atmpS1634;
        int32_t _M0L6_2atmpS1636;
        uint32_t _M0L6_2atmpS1638;
        int32_t _M0L6_2atmpS1637;
        int32_t _M0L6_2atmpS1641;
        int32_t _M0L6_2atmpS1642;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1634 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1635);
        if (
          _M0L1jS189 < 0 || _M0L1jS189 >= Moonbit_array_length(_M0L4selfS184)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS184[_M0L1jS189] = _M0L6_2atmpS1634;
        _M0L6_2atmpS1636 = _M0L1jS189 + 1;
        _M0L6_2atmpS1638 = _M0L1cS190 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1637 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1638);
        if (
          _M0L6_2atmpS1636 < 0
          || _M0L6_2atmpS1636 >= Moonbit_array_length(_M0L4selfS184)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS184[_M0L6_2atmpS1636] = _M0L6_2atmpS1637;
        _M0L6_2atmpS1641 = _M0L1iS188 + 1;
        _M0L6_2atmpS1642 = _M0L1jS189 + 2;
        _M0L1iS188 = _M0L6_2atmpS1641;
        _M0L1jS189 = _M0L6_2atmpS1642;
        continue;
      } else {
        moonbit_decref(_M0L3strS186);
        moonbit_decref(_M0L4selfS184);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS186);
    moonbit_decref(_M0L4selfS184);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS175,
  double _M0L3objS174
) {
  struct _M0TPB6Logger _M0L6_2atmpS1632;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1632
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS175
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS174, _M0L6_2atmpS1632);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS177,
  struct _M0TPC16string10StringView _M0L3objS176
) {
  struct _M0TPB6Logger _M0L6_2atmpS1633;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1633
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS177
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS176, _M0L6_2atmpS1633);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS120
) {
  int32_t _M0L6_2atmpS1631;
  struct _M0TPC16string10StringView _M0L7_2abindS119;
  moonbit_string_t _M0L7_2adataS121;
  int32_t _M0L8_2astartS122;
  int32_t _M0L6_2atmpS1630;
  int32_t _M0L6_2aendS123;
  int32_t _M0Lm9_2acursorS124;
  int32_t _M0Lm13accept__stateS125;
  int32_t _M0Lm10match__endS126;
  int32_t _M0Lm20match__tag__saver__0S127;
  int32_t _M0Lm20match__tag__saver__1S128;
  int32_t _M0Lm20match__tag__saver__2S129;
  int32_t _M0Lm20match__tag__saver__3S130;
  int32_t _M0Lm20match__tag__saver__4S131;
  int32_t _M0Lm6tag__0S132;
  int32_t _M0Lm6tag__1S133;
  int32_t _M0Lm9tag__1__1S134;
  int32_t _M0Lm9tag__1__2S135;
  int32_t _M0Lm6tag__3S136;
  int32_t _M0Lm6tag__2S137;
  int32_t _M0Lm9tag__2__1S138;
  int32_t _M0Lm6tag__4S139;
  int32_t _M0L6_2atmpS1588;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1631 = Moonbit_array_length(_M0L4reprS120);
  _M0L7_2abindS119
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1631, _M0L4reprS120
  };
  moonbit_incref(_M0L7_2abindS119.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS121 = _M0MPC16string10StringView4data(_M0L7_2abindS119);
  moonbit_incref(_M0L7_2abindS119.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS122
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS119);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1630 = _M0MPC16string10StringView6length(_M0L7_2abindS119);
  _M0L6_2aendS123 = _M0L8_2astartS122 + _M0L6_2atmpS1630;
  _M0Lm9_2acursorS124 = _M0L8_2astartS122;
  _M0Lm13accept__stateS125 = -1;
  _M0Lm10match__endS126 = -1;
  _M0Lm20match__tag__saver__0S127 = -1;
  _M0Lm20match__tag__saver__1S128 = -1;
  _M0Lm20match__tag__saver__2S129 = -1;
  _M0Lm20match__tag__saver__3S130 = -1;
  _M0Lm20match__tag__saver__4S131 = -1;
  _M0Lm6tag__0S132 = -1;
  _M0Lm6tag__1S133 = -1;
  _M0Lm9tag__1__1S134 = -1;
  _M0Lm9tag__1__2S135 = -1;
  _M0Lm6tag__3S136 = -1;
  _M0Lm6tag__2S137 = -1;
  _M0Lm9tag__2__1S138 = -1;
  _M0Lm6tag__4S139 = -1;
  _M0L6_2atmpS1588 = _M0Lm9_2acursorS124;
  if (_M0L6_2atmpS1588 < _M0L6_2aendS123) {
    int32_t _M0L6_2atmpS1590 = _M0Lm9_2acursorS124;
    int32_t _M0L6_2atmpS1589;
    moonbit_incref(_M0L7_2adataS121);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1589
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1590);
    if (_M0L6_2atmpS1589 == 64) {
      int32_t _M0L6_2atmpS1591 = _M0Lm9_2acursorS124;
      _M0Lm9_2acursorS124 = _M0L6_2atmpS1591 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1592;
        _M0Lm6tag__0S132 = _M0Lm9_2acursorS124;
        _M0L6_2atmpS1592 = _M0Lm9_2acursorS124;
        if (_M0L6_2atmpS1592 < _M0L6_2aendS123) {
          int32_t _M0L6_2atmpS1629 = _M0Lm9_2acursorS124;
          int32_t _M0L10next__charS147;
          int32_t _M0L6_2atmpS1593;
          moonbit_incref(_M0L7_2adataS121);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS147
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1629);
          _M0L6_2atmpS1593 = _M0Lm9_2acursorS124;
          _M0Lm9_2acursorS124 = _M0L6_2atmpS1593 + 1;
          if (_M0L10next__charS147 == 58) {
            int32_t _M0L6_2atmpS1594 = _M0Lm9_2acursorS124;
            if (_M0L6_2atmpS1594 < _M0L6_2aendS123) {
              int32_t _M0L6_2atmpS1595 = _M0Lm9_2acursorS124;
              int32_t _M0L12dispatch__15S148;
              _M0Lm9_2acursorS124 = _M0L6_2atmpS1595 + 1;
              _M0L12dispatch__15S148 = 0;
              loop__label__15_151:;
              while (1) {
                int32_t _M0L6_2atmpS1596;
                switch (_M0L12dispatch__15S148) {
                  case 3: {
                    int32_t _M0L6_2atmpS1599;
                    _M0Lm9tag__1__2S135 = _M0Lm9tag__1__1S134;
                    _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1599 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1599 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1604 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS155;
                      int32_t _M0L6_2atmpS1600;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS155
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1604);
                      _M0L6_2atmpS1600 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1600 + 1;
                      if (_M0L10next__charS155 < 58) {
                        if (_M0L10next__charS155 < 48) {
                          goto join_154;
                        } else {
                          int32_t _M0L6_2atmpS1601;
                          _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                          _M0Lm9tag__2__1S138 = _M0Lm6tag__2S137;
                          _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                          _M0Lm6tag__3S136 = _M0Lm9_2acursorS124;
                          _M0L6_2atmpS1601 = _M0Lm9_2acursorS124;
                          if (_M0L6_2atmpS1601 < _M0L6_2aendS123) {
                            int32_t _M0L6_2atmpS1603 = _M0Lm9_2acursorS124;
                            int32_t _M0L10next__charS157;
                            int32_t _M0L6_2atmpS1602;
                            moonbit_incref(_M0L7_2adataS121);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS157
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1603);
                            _M0L6_2atmpS1602 = _M0Lm9_2acursorS124;
                            _M0Lm9_2acursorS124 = _M0L6_2atmpS1602 + 1;
                            if (_M0L10next__charS157 < 48) {
                              if (_M0L10next__charS157 == 45) {
                                goto join_149;
                              } else {
                                goto join_156;
                              }
                            } else if (_M0L10next__charS157 > 57) {
                              if (_M0L10next__charS157 < 59) {
                                _M0L12dispatch__15S148 = 3;
                                goto loop__label__15_151;
                              } else {
                                goto join_156;
                              }
                            } else {
                              _M0L12dispatch__15S148 = 6;
                              goto loop__label__15_151;
                            }
                            join_156:;
                            _M0L12dispatch__15S148 = 0;
                            goto loop__label__15_151;
                          } else {
                            goto join_140;
                          }
                        }
                      } else if (_M0L10next__charS155 > 58) {
                        goto join_154;
                      } else {
                        _M0L12dispatch__15S148 = 1;
                        goto loop__label__15_151;
                      }
                      join_154:;
                      _M0L12dispatch__15S148 = 0;
                      goto loop__label__15_151;
                    } else {
                      goto join_140;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1605;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1605 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1605 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1607 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS159;
                      int32_t _M0L6_2atmpS1606;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS159
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1607);
                      _M0L6_2atmpS1606 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1606 + 1;
                      if (_M0L10next__charS159 < 58) {
                        if (_M0L10next__charS159 < 48) {
                          goto join_158;
                        } else {
                          _M0L12dispatch__15S148 = 2;
                          goto loop__label__15_151;
                        }
                      } else if (_M0L10next__charS159 > 58) {
                        goto join_158;
                      } else {
                        _M0L12dispatch__15S148 = 3;
                        goto loop__label__15_151;
                      }
                      join_158:;
                      _M0L12dispatch__15S148 = 0;
                      goto loop__label__15_151;
                    } else {
                      goto join_140;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1608;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1608 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1608 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1610 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1609;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1610);
                      _M0L6_2atmpS1609 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1609 + 1;
                      if (_M0L10next__charS160 == 58) {
                        _M0L12dispatch__15S148 = 1;
                        goto loop__label__15_151;
                      } else {
                        _M0L12dispatch__15S148 = 0;
                        goto loop__label__15_151;
                      }
                    } else {
                      goto join_140;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1611;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__4S139 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1611 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1611 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1619 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS162;
                      int32_t _M0L6_2atmpS1612;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS162
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1619);
                      _M0L6_2atmpS1612 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1612 + 1;
                      if (_M0L10next__charS162 < 58) {
                        if (_M0L10next__charS162 < 48) {
                          goto join_161;
                        } else {
                          _M0L12dispatch__15S148 = 4;
                          goto loop__label__15_151;
                        }
                      } else if (_M0L10next__charS162 > 58) {
                        goto join_161;
                      } else {
                        int32_t _M0L6_2atmpS1613;
                        _M0Lm9tag__1__2S135 = _M0Lm9tag__1__1S134;
                        _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                        _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                        _M0L6_2atmpS1613 = _M0Lm9_2acursorS124;
                        if (_M0L6_2atmpS1613 < _M0L6_2aendS123) {
                          int32_t _M0L6_2atmpS1618 = _M0Lm9_2acursorS124;
                          int32_t _M0L10next__charS164;
                          int32_t _M0L6_2atmpS1614;
                          moonbit_incref(_M0L7_2adataS121);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS164
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1618);
                          _M0L6_2atmpS1614 = _M0Lm9_2acursorS124;
                          _M0Lm9_2acursorS124 = _M0L6_2atmpS1614 + 1;
                          if (_M0L10next__charS164 < 58) {
                            if (_M0L10next__charS164 < 48) {
                              goto join_163;
                            } else {
                              int32_t _M0L6_2atmpS1615;
                              _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                              _M0Lm9tag__2__1S138 = _M0Lm6tag__2S137;
                              _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                              _M0L6_2atmpS1615 = _M0Lm9_2acursorS124;
                              if (_M0L6_2atmpS1615 < _M0L6_2aendS123) {
                                int32_t _M0L6_2atmpS1617 =
                                  _M0Lm9_2acursorS124;
                                int32_t _M0L10next__charS166;
                                int32_t _M0L6_2atmpS1616;
                                moonbit_incref(_M0L7_2adataS121);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS166
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1617);
                                _M0L6_2atmpS1616 = _M0Lm9_2acursorS124;
                                _M0Lm9_2acursorS124 = _M0L6_2atmpS1616 + 1;
                                if (_M0L10next__charS166 < 58) {
                                  if (_M0L10next__charS166 < 48) {
                                    goto join_165;
                                  } else {
                                    _M0L12dispatch__15S148 = 5;
                                    goto loop__label__15_151;
                                  }
                                } else if (_M0L10next__charS166 > 58) {
                                  goto join_165;
                                } else {
                                  _M0L12dispatch__15S148 = 3;
                                  goto loop__label__15_151;
                                }
                                join_165:;
                                _M0L12dispatch__15S148 = 0;
                                goto loop__label__15_151;
                              } else {
                                goto join_153;
                              }
                            }
                          } else if (_M0L10next__charS164 > 58) {
                            goto join_163;
                          } else {
                            _M0L12dispatch__15S148 = 1;
                            goto loop__label__15_151;
                          }
                          join_163:;
                          _M0L12dispatch__15S148 = 0;
                          goto loop__label__15_151;
                        } else {
                          goto join_140;
                        }
                      }
                      join_161:;
                      _M0L12dispatch__15S148 = 0;
                      goto loop__label__15_151;
                    } else {
                      goto join_140;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1620;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1620 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1620 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1622 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1621;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1622);
                      _M0L6_2atmpS1621 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1621 + 1;
                      if (_M0L10next__charS168 < 58) {
                        if (_M0L10next__charS168 < 48) {
                          goto join_167;
                        } else {
                          _M0L12dispatch__15S148 = 5;
                          goto loop__label__15_151;
                        }
                      } else if (_M0L10next__charS168 > 58) {
                        goto join_167;
                      } else {
                        _M0L12dispatch__15S148 = 3;
                        goto loop__label__15_151;
                      }
                      join_167:;
                      _M0L12dispatch__15S148 = 0;
                      goto loop__label__15_151;
                    } else {
                      goto join_153;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1623;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__3S136 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1623 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1623 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1625 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1624;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1625);
                      _M0L6_2atmpS1624 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1624 + 1;
                      if (_M0L10next__charS170 < 48) {
                        if (_M0L10next__charS170 == 45) {
                          goto join_149;
                        } else {
                          goto join_169;
                        }
                      } else if (_M0L10next__charS170 > 57) {
                        if (_M0L10next__charS170 < 59) {
                          _M0L12dispatch__15S148 = 3;
                          goto loop__label__15_151;
                        } else {
                          goto join_169;
                        }
                      } else {
                        _M0L12dispatch__15S148 = 6;
                        goto loop__label__15_151;
                      }
                      join_169:;
                      _M0L12dispatch__15S148 = 0;
                      goto loop__label__15_151;
                    } else {
                      goto join_140;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1626;
                    _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1626 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1626 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1628 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS172;
                      int32_t _M0L6_2atmpS1627;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS172
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1628);
                      _M0L6_2atmpS1627 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1627 + 1;
                      if (_M0L10next__charS172 < 58) {
                        if (_M0L10next__charS172 < 48) {
                          goto join_171;
                        } else {
                          _M0L12dispatch__15S148 = 2;
                          goto loop__label__15_151;
                        }
                      } else if (_M0L10next__charS172 > 58) {
                        goto join_171;
                      } else {
                        _M0L12dispatch__15S148 = 1;
                        goto loop__label__15_151;
                      }
                      join_171:;
                      _M0L12dispatch__15S148 = 0;
                      goto loop__label__15_151;
                    } else {
                      goto join_140;
                    }
                    break;
                  }
                  default: {
                    goto join_140;
                    break;
                  }
                }
                join_153:;
                _M0Lm6tag__1S133 = _M0Lm9tag__1__2S135;
                _M0Lm6tag__2S137 = _M0Lm9tag__2__1S138;
                _M0Lm20match__tag__saver__0S127 = _M0Lm6tag__0S132;
                _M0Lm20match__tag__saver__1S128 = _M0Lm6tag__1S133;
                _M0Lm20match__tag__saver__2S129 = _M0Lm6tag__2S137;
                _M0Lm20match__tag__saver__3S130 = _M0Lm6tag__3S136;
                _M0Lm20match__tag__saver__4S131 = _M0Lm6tag__4S139;
                _M0Lm13accept__stateS125 = 0;
                _M0Lm10match__endS126 = _M0Lm9_2acursorS124;
                goto join_140;
                join_149:;
                _M0Lm9tag__1__1S134 = _M0Lm9tag__1__2S135;
                _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                _M0Lm6tag__2S137 = _M0Lm9tag__2__1S138;
                _M0L6_2atmpS1596 = _M0Lm9_2acursorS124;
                if (_M0L6_2atmpS1596 < _M0L6_2aendS123) {
                  int32_t _M0L6_2atmpS1598 = _M0Lm9_2acursorS124;
                  int32_t _M0L10next__charS152;
                  int32_t _M0L6_2atmpS1597;
                  moonbit_incref(_M0L7_2adataS121);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS152
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1598);
                  _M0L6_2atmpS1597 = _M0Lm9_2acursorS124;
                  _M0Lm9_2acursorS124 = _M0L6_2atmpS1597 + 1;
                  if (_M0L10next__charS152 < 58) {
                    if (_M0L10next__charS152 < 48) {
                      goto join_150;
                    } else {
                      _M0L12dispatch__15S148 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS152 > 58) {
                    goto join_150;
                  } else {
                    _M0L12dispatch__15S148 = 1;
                    continue;
                  }
                  join_150:;
                  _M0L12dispatch__15S148 = 0;
                  continue;
                } else {
                  goto join_140;
                }
                break;
              }
            } else {
              goto join_140;
            }
          } else {
            continue;
          }
        } else {
          goto join_140;
        }
        break;
      }
    } else {
      goto join_140;
    }
  } else {
    goto join_140;
  }
  join_140:;
  switch (_M0Lm13accept__stateS125) {
    case 0: {
      int32_t _M0L6_2atmpS1587 = _M0Lm20match__tag__saver__1S128;
      int32_t _M0L6_2atmpS1586 = _M0L6_2atmpS1587 + 1;
      int64_t _M0L6_2atmpS1583 = (int64_t)_M0L6_2atmpS1586;
      int32_t _M0L6_2atmpS1585 = _M0Lm20match__tag__saver__2S129;
      int64_t _M0L6_2atmpS1584 = (int64_t)_M0L6_2atmpS1585;
      struct _M0TPC16string10StringView _M0L11start__lineS141;
      int32_t _M0L6_2atmpS1582;
      int32_t _M0L6_2atmpS1581;
      int64_t _M0L6_2atmpS1578;
      int32_t _M0L6_2atmpS1580;
      int64_t _M0L6_2atmpS1579;
      struct _M0TPC16string10StringView _M0L13start__columnS142;
      int32_t _M0L6_2atmpS1577;
      int64_t _M0L6_2atmpS1574;
      int32_t _M0L6_2atmpS1576;
      int64_t _M0L6_2atmpS1575;
      struct _M0TPC16string10StringView _M0L3pkgS143;
      int32_t _M0L6_2atmpS1573;
      int32_t _M0L6_2atmpS1572;
      int64_t _M0L6_2atmpS1569;
      int32_t _M0L6_2atmpS1571;
      int64_t _M0L6_2atmpS1570;
      struct _M0TPC16string10StringView _M0L8filenameS144;
      int32_t _M0L6_2atmpS1568;
      int32_t _M0L6_2atmpS1567;
      int64_t _M0L6_2atmpS1564;
      int32_t _M0L6_2atmpS1566;
      int64_t _M0L6_2atmpS1565;
      struct _M0TPC16string10StringView _M0L9end__lineS145;
      int32_t _M0L6_2atmpS1563;
      int32_t _M0L6_2atmpS1562;
      int64_t _M0L6_2atmpS1559;
      int32_t _M0L6_2atmpS1561;
      int64_t _M0L6_2atmpS1560;
      struct _M0TPC16string10StringView _M0L11end__columnS146;
      struct _M0TPB13SourceLocRepr* _block_4370;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS141
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1583, _M0L6_2atmpS1584);
      _M0L6_2atmpS1582 = _M0Lm20match__tag__saver__2S129;
      _M0L6_2atmpS1581 = _M0L6_2atmpS1582 + 1;
      _M0L6_2atmpS1578 = (int64_t)_M0L6_2atmpS1581;
      _M0L6_2atmpS1580 = _M0Lm20match__tag__saver__3S130;
      _M0L6_2atmpS1579 = (int64_t)_M0L6_2atmpS1580;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS142
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1578, _M0L6_2atmpS1579);
      _M0L6_2atmpS1577 = _M0L8_2astartS122 + 1;
      _M0L6_2atmpS1574 = (int64_t)_M0L6_2atmpS1577;
      _M0L6_2atmpS1576 = _M0Lm20match__tag__saver__0S127;
      _M0L6_2atmpS1575 = (int64_t)_M0L6_2atmpS1576;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS143
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1574, _M0L6_2atmpS1575);
      _M0L6_2atmpS1573 = _M0Lm20match__tag__saver__0S127;
      _M0L6_2atmpS1572 = _M0L6_2atmpS1573 + 1;
      _M0L6_2atmpS1569 = (int64_t)_M0L6_2atmpS1572;
      _M0L6_2atmpS1571 = _M0Lm20match__tag__saver__1S128;
      _M0L6_2atmpS1570 = (int64_t)_M0L6_2atmpS1571;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS144
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1569, _M0L6_2atmpS1570);
      _M0L6_2atmpS1568 = _M0Lm20match__tag__saver__3S130;
      _M0L6_2atmpS1567 = _M0L6_2atmpS1568 + 1;
      _M0L6_2atmpS1564 = (int64_t)_M0L6_2atmpS1567;
      _M0L6_2atmpS1566 = _M0Lm20match__tag__saver__4S131;
      _M0L6_2atmpS1565 = (int64_t)_M0L6_2atmpS1566;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS145
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1564, _M0L6_2atmpS1565);
      _M0L6_2atmpS1563 = _M0Lm20match__tag__saver__4S131;
      _M0L6_2atmpS1562 = _M0L6_2atmpS1563 + 1;
      _M0L6_2atmpS1559 = (int64_t)_M0L6_2atmpS1562;
      _M0L6_2atmpS1561 = _M0Lm10match__endS126;
      _M0L6_2atmpS1560 = (int64_t)_M0L6_2atmpS1561;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS146
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1559, _M0L6_2atmpS1560);
      _block_4370
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4370)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4370->$0_0 = _M0L3pkgS143.$0;
      _block_4370->$0_1 = _M0L3pkgS143.$1;
      _block_4370->$0_2 = _M0L3pkgS143.$2;
      _block_4370->$1_0 = _M0L8filenameS144.$0;
      _block_4370->$1_1 = _M0L8filenameS144.$1;
      _block_4370->$1_2 = _M0L8filenameS144.$2;
      _block_4370->$2_0 = _M0L11start__lineS141.$0;
      _block_4370->$2_1 = _M0L11start__lineS141.$1;
      _block_4370->$2_2 = _M0L11start__lineS141.$2;
      _block_4370->$3_0 = _M0L13start__columnS142.$0;
      _block_4370->$3_1 = _M0L13start__columnS142.$1;
      _block_4370->$3_2 = _M0L13start__columnS142.$2;
      _block_4370->$4_0 = _M0L9end__lineS145.$0;
      _block_4370->$4_1 = _M0L9end__lineS145.$1;
      _block_4370->$4_2 = _M0L9end__lineS145.$2;
      _block_4370->$5_0 = _M0L11end__columnS146.$0;
      _block_4370->$5_1 = _M0L11end__columnS146.$1;
      _block_4370->$5_2 = _M0L11end__columnS146.$2;
      return _block_4370;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS121);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS117,
  int32_t _M0L5indexS118
) {
  int32_t _M0L3lenS116;
  int32_t _if__result_4371;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS116 = _M0L4selfS117->$1;
  if (_M0L5indexS118 >= 0) {
    _if__result_4371 = _M0L5indexS118 < _M0L3lenS116;
  } else {
    _if__result_4371 = 0;
  }
  if (_if__result_4371) {
    moonbit_string_t* _M0L6_2atmpS1558;
    moonbit_string_t _M0L6_2atmpS4093;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1558 = _M0MPC15array5Array6bufferGsE(_M0L4selfS117);
    if (
      _M0L5indexS118 < 0
      || _M0L5indexS118 >= Moonbit_array_length(_M0L6_2atmpS1558)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4093 = (moonbit_string_t)_M0L6_2atmpS1558[_M0L5indexS118];
    moonbit_incref(_M0L6_2atmpS4093);
    moonbit_decref(_M0L6_2atmpS1558);
    return _M0L6_2atmpS4093;
  } else {
    moonbit_decref(_M0L4selfS117);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS113
) {
  moonbit_string_t* _M0L8_2afieldS4094;
  int32_t _M0L6_2acntS4209;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4094 = _M0L4selfS113->$0;
  _M0L6_2acntS4209 = Moonbit_object_header(_M0L4selfS113)->rc;
  if (_M0L6_2acntS4209 > 1) {
    int32_t _M0L11_2anew__cntS4210 = _M0L6_2acntS4209 - 1;
    Moonbit_object_header(_M0L4selfS113)->rc = _M0L11_2anew__cntS4210;
    moonbit_incref(_M0L8_2afieldS4094);
  } else if (_M0L6_2acntS4209 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS113);
  }
  return _M0L8_2afieldS4094;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS114
) {
  struct _M0TUsiE** _M0L8_2afieldS4095;
  int32_t _M0L6_2acntS4211;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4095 = _M0L4selfS114->$0;
  _M0L6_2acntS4211 = Moonbit_object_header(_M0L4selfS114)->rc;
  if (_M0L6_2acntS4211 > 1) {
    int32_t _M0L11_2anew__cntS4212 = _M0L6_2acntS4211 - 1;
    Moonbit_object_header(_M0L4selfS114)->rc = _M0L11_2anew__cntS4212;
    moonbit_incref(_M0L8_2afieldS4095);
  } else if (_M0L6_2acntS4211 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS114);
  }
  return _M0L8_2afieldS4095;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS115
) {
  void** _M0L8_2afieldS4096;
  int32_t _M0L6_2acntS4213;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4096 = _M0L4selfS115->$0;
  _M0L6_2acntS4213 = Moonbit_object_header(_M0L4selfS115)->rc;
  if (_M0L6_2acntS4213 > 1) {
    int32_t _M0L11_2anew__cntS4214 = _M0L6_2acntS4213 - 1;
    Moonbit_object_header(_M0L4selfS115)->rc = _M0L11_2anew__cntS4214;
    moonbit_incref(_M0L8_2afieldS4096);
  } else if (_M0L6_2acntS4213 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS115);
  }
  return _M0L8_2afieldS4096;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS112) {
  struct _M0TPB13StringBuilder* _M0L3bufS111;
  struct _M0TPB6Logger _M0L6_2atmpS1557;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS111 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS111);
  _M0L6_2atmpS1557
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS111
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS112, _M0L6_2atmpS1557);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS111);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS110) {
  int32_t _M0L6_2atmpS1556;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1556 = (int32_t)_M0L4selfS110;
  return _M0L6_2atmpS1556;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS108,
  int32_t _M0L8trailingS109
) {
  int32_t _M0L6_2atmpS1555;
  int32_t _M0L6_2atmpS1554;
  int32_t _M0L6_2atmpS1553;
  int32_t _M0L6_2atmpS1552;
  int32_t _M0L6_2atmpS1551;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1555 = _M0L7leadingS108 - 55296;
  _M0L6_2atmpS1554 = _M0L6_2atmpS1555 * 1024;
  _M0L6_2atmpS1553 = _M0L6_2atmpS1554 + _M0L8trailingS109;
  _M0L6_2atmpS1552 = _M0L6_2atmpS1553 - 56320;
  _M0L6_2atmpS1551 = _M0L6_2atmpS1552 + 65536;
  return _M0L6_2atmpS1551;
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
  int32_t _M0L3lenS1546;
  int32_t _M0L6_2atmpS1545;
  moonbit_bytes_t _M0L8_2afieldS4097;
  moonbit_bytes_t _M0L4dataS1549;
  int32_t _M0L3lenS1550;
  int32_t _M0L3incS104;
  int32_t _M0L3lenS1548;
  int32_t _M0L6_2atmpS1547;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1546 = _M0L4selfS103->$1;
  _M0L6_2atmpS1545 = _M0L3lenS1546 + 4;
  moonbit_incref(_M0L4selfS103);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS103, _M0L6_2atmpS1545);
  _M0L8_2afieldS4097 = _M0L4selfS103->$0;
  _M0L4dataS1549 = _M0L8_2afieldS4097;
  _M0L3lenS1550 = _M0L4selfS103->$1;
  moonbit_incref(_M0L4dataS1549);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS104
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1549, _M0L3lenS1550, _M0L2chS105);
  _M0L3lenS1548 = _M0L4selfS103->$1;
  _M0L6_2atmpS1547 = _M0L3lenS1548 + _M0L3incS104;
  _M0L4selfS103->$1 = _M0L6_2atmpS1547;
  moonbit_decref(_M0L4selfS103);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS98,
  int32_t _M0L8requiredS99
) {
  moonbit_bytes_t _M0L8_2afieldS4101;
  moonbit_bytes_t _M0L4dataS1544;
  int32_t _M0L6_2atmpS4100;
  int32_t _M0L12current__lenS97;
  int32_t _M0Lm13enough__spaceS100;
  int32_t _M0L6_2atmpS1542;
  int32_t _M0L6_2atmpS1543;
  moonbit_bytes_t _M0L9new__dataS102;
  moonbit_bytes_t _M0L8_2afieldS4099;
  moonbit_bytes_t _M0L4dataS1540;
  int32_t _M0L3lenS1541;
  moonbit_bytes_t _M0L6_2aoldS4098;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4101 = _M0L4selfS98->$0;
  _M0L4dataS1544 = _M0L8_2afieldS4101;
  _M0L6_2atmpS4100 = Moonbit_array_length(_M0L4dataS1544);
  _M0L12current__lenS97 = _M0L6_2atmpS4100;
  if (_M0L8requiredS99 <= _M0L12current__lenS97) {
    moonbit_decref(_M0L4selfS98);
    return 0;
  }
  _M0Lm13enough__spaceS100 = _M0L12current__lenS97;
  while (1) {
    int32_t _M0L6_2atmpS1538 = _M0Lm13enough__spaceS100;
    if (_M0L6_2atmpS1538 < _M0L8requiredS99) {
      int32_t _M0L6_2atmpS1539 = _M0Lm13enough__spaceS100;
      _M0Lm13enough__spaceS100 = _M0L6_2atmpS1539 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1542 = _M0Lm13enough__spaceS100;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1543 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS102
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1542, _M0L6_2atmpS1543);
  _M0L8_2afieldS4099 = _M0L4selfS98->$0;
  _M0L4dataS1540 = _M0L8_2afieldS4099;
  _M0L3lenS1541 = _M0L4selfS98->$1;
  moonbit_incref(_M0L4dataS1540);
  moonbit_incref(_M0L9new__dataS102);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS102, 0, _M0L4dataS1540, 0, _M0L3lenS1541);
  _M0L6_2aoldS4098 = _M0L4selfS98->$0;
  moonbit_decref(_M0L6_2aoldS4098);
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
    uint32_t _M0L6_2atmpS1521 = _M0L4codeS90 & 255u;
    int32_t _M0L6_2atmpS1520;
    int32_t _M0L6_2atmpS1522;
    uint32_t _M0L6_2atmpS1524;
    int32_t _M0L6_2atmpS1523;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1520 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1521);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1520;
    _M0L6_2atmpS1522 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1524 = _M0L4codeS90 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1523 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1524);
    if (
      _M0L6_2atmpS1522 < 0
      || _M0L6_2atmpS1522 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1522] = _M0L6_2atmpS1523;
    moonbit_decref(_M0L4selfS92);
    return 2;
  } else if (_M0L4codeS90 < 1114112u) {
    uint32_t _M0L2hiS94 = _M0L4codeS90 - 65536u;
    uint32_t _M0L6_2atmpS1537 = _M0L2hiS94 >> 10;
    uint32_t _M0L2loS95 = _M0L6_2atmpS1537 | 55296u;
    uint32_t _M0L6_2atmpS1536 = _M0L2hiS94 & 1023u;
    uint32_t _M0L2hiS96 = _M0L6_2atmpS1536 | 56320u;
    uint32_t _M0L6_2atmpS1526 = _M0L2loS95 & 255u;
    int32_t _M0L6_2atmpS1525;
    int32_t _M0L6_2atmpS1527;
    uint32_t _M0L6_2atmpS1529;
    int32_t _M0L6_2atmpS1528;
    int32_t _M0L6_2atmpS1530;
    uint32_t _M0L6_2atmpS1532;
    int32_t _M0L6_2atmpS1531;
    int32_t _M0L6_2atmpS1533;
    uint32_t _M0L6_2atmpS1535;
    int32_t _M0L6_2atmpS1534;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1525 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1526);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1525;
    _M0L6_2atmpS1527 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1529 = _M0L2loS95 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1528 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1529);
    if (
      _M0L6_2atmpS1527 < 0
      || _M0L6_2atmpS1527 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1527] = _M0L6_2atmpS1528;
    _M0L6_2atmpS1530 = _M0L6offsetS93 + 2;
    _M0L6_2atmpS1532 = _M0L2hiS96 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1531 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1532);
    if (
      _M0L6_2atmpS1530 < 0
      || _M0L6_2atmpS1530 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1530] = _M0L6_2atmpS1531;
    _M0L6_2atmpS1533 = _M0L6offsetS93 + 3;
    _M0L6_2atmpS1535 = _M0L2hiS96 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1534 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1535);
    if (
      _M0L6_2atmpS1533 < 0
      || _M0L6_2atmpS1533 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1533] = _M0L6_2atmpS1534;
    moonbit_decref(_M0L4selfS92);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS92);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_67.data, (moonbit_string_t)moonbit_string_literal_68.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1519;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1519 = *(int32_t*)&_M0L4selfS89;
  return _M0L6_2atmpS1519 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS88) {
  int32_t _M0L6_2atmpS1518;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1518 = _M0L4selfS88;
  return *(uint32_t*)&_M0L6_2atmpS1518;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS87
) {
  moonbit_bytes_t _M0L8_2afieldS4103;
  moonbit_bytes_t _M0L4dataS1517;
  moonbit_bytes_t _M0L6_2atmpS1514;
  int32_t _M0L8_2afieldS4102;
  int32_t _M0L3lenS1516;
  int64_t _M0L6_2atmpS1515;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4103 = _M0L4selfS87->$0;
  _M0L4dataS1517 = _M0L8_2afieldS4103;
  moonbit_incref(_M0L4dataS1517);
  _M0L6_2atmpS1514 = _M0L4dataS1517;
  _M0L8_2afieldS4102 = _M0L4selfS87->$1;
  moonbit_decref(_M0L4selfS87);
  _M0L3lenS1516 = _M0L8_2afieldS4102;
  _M0L6_2atmpS1515 = (int64_t)_M0L3lenS1516;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1514, 0, _M0L6_2atmpS1515);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS82,
  int32_t _M0L6offsetS86,
  int64_t _M0L6lengthS84
) {
  int32_t _M0L3lenS81;
  int32_t _M0L6lengthS83;
  int32_t _if__result_4373;
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
      int32_t _M0L6_2atmpS1513 = _M0L6offsetS86 + _M0L6lengthS83;
      _if__result_4373 = _M0L6_2atmpS1513 <= _M0L3lenS81;
    } else {
      _if__result_4373 = 0;
    }
  } else {
    _if__result_4373 = 0;
  }
  if (_if__result_4373) {
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
  struct _M0TPB13StringBuilder* _block_4374;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS79 < 1) {
    _M0L7initialS78 = 1;
  } else {
    _M0L7initialS78 = _M0L10size__hintS79;
  }
  _M0L4dataS80 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS78, 0);
  _block_4374
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4374)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4374->$0 = _M0L4dataS80;
  _block_4374->$1 = 0;
  return _block_4374;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS77) {
  int32_t _M0L6_2atmpS1512;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1512 = (int32_t)_M0L4selfS77;
  return _M0L6_2atmpS1512;
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
  int32_t _if__result_4375;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS26 == _M0L3srcS27) {
    _if__result_4375 = _M0L11dst__offsetS28 < _M0L11src__offsetS29;
  } else {
    _if__result_4375 = 0;
  }
  if (_if__result_4375) {
    int32_t _M0L1iS30 = 0;
    while (1) {
      if (_M0L1iS30 < _M0L3lenS31) {
        int32_t _M0L6_2atmpS1476 = _M0L11dst__offsetS28 + _M0L1iS30;
        int32_t _M0L6_2atmpS1478 = _M0L11src__offsetS29 + _M0L1iS30;
        int32_t _M0L6_2atmpS1477;
        int32_t _M0L6_2atmpS1479;
        if (
          _M0L6_2atmpS1478 < 0
          || _M0L6_2atmpS1478 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1477 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1478];
        if (
          _M0L6_2atmpS1476 < 0
          || _M0L6_2atmpS1476 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1476] = _M0L6_2atmpS1477;
        _M0L6_2atmpS1479 = _M0L1iS30 + 1;
        _M0L1iS30 = _M0L6_2atmpS1479;
        continue;
      } else {
        moonbit_decref(_M0L3srcS27);
        moonbit_decref(_M0L3dstS26);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1484 = _M0L3lenS31 - 1;
    int32_t _M0L1iS33 = _M0L6_2atmpS1484;
    while (1) {
      if (_M0L1iS33 >= 0) {
        int32_t _M0L6_2atmpS1480 = _M0L11dst__offsetS28 + _M0L1iS33;
        int32_t _M0L6_2atmpS1482 = _M0L11src__offsetS29 + _M0L1iS33;
        int32_t _M0L6_2atmpS1481;
        int32_t _M0L6_2atmpS1483;
        if (
          _M0L6_2atmpS1482 < 0
          || _M0L6_2atmpS1482 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1481 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1482];
        if (
          _M0L6_2atmpS1480 < 0
          || _M0L6_2atmpS1480 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1480] = _M0L6_2atmpS1481;
        _M0L6_2atmpS1483 = _M0L1iS33 - 1;
        _M0L1iS33 = _M0L6_2atmpS1483;
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
  int32_t _if__result_4378;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS35 == _M0L3srcS36) {
    _if__result_4378 = _M0L11dst__offsetS37 < _M0L11src__offsetS38;
  } else {
    _if__result_4378 = 0;
  }
  if (_if__result_4378) {
    int32_t _M0L1iS39 = 0;
    while (1) {
      if (_M0L1iS39 < _M0L3lenS40) {
        int32_t _M0L6_2atmpS1485 = _M0L11dst__offsetS37 + _M0L1iS39;
        int32_t _M0L6_2atmpS1487 = _M0L11src__offsetS38 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS4105;
        moonbit_string_t _M0L6_2atmpS1486;
        moonbit_string_t _M0L6_2aoldS4104;
        int32_t _M0L6_2atmpS1488;
        if (
          _M0L6_2atmpS1487 < 0
          || _M0L6_2atmpS1487 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4105 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1487];
        _M0L6_2atmpS1486 = _M0L6_2atmpS4105;
        if (
          _M0L6_2atmpS1485 < 0
          || _M0L6_2atmpS1485 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4104 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1485];
        moonbit_incref(_M0L6_2atmpS1486);
        moonbit_decref(_M0L6_2aoldS4104);
        _M0L3dstS35[_M0L6_2atmpS1485] = _M0L6_2atmpS1486;
        _M0L6_2atmpS1488 = _M0L1iS39 + 1;
        _M0L1iS39 = _M0L6_2atmpS1488;
        continue;
      } else {
        moonbit_decref(_M0L3srcS36);
        moonbit_decref(_M0L3dstS35);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1493 = _M0L3lenS40 - 1;
    int32_t _M0L1iS42 = _M0L6_2atmpS1493;
    while (1) {
      if (_M0L1iS42 >= 0) {
        int32_t _M0L6_2atmpS1489 = _M0L11dst__offsetS37 + _M0L1iS42;
        int32_t _M0L6_2atmpS1491 = _M0L11src__offsetS38 + _M0L1iS42;
        moonbit_string_t _M0L6_2atmpS4107;
        moonbit_string_t _M0L6_2atmpS1490;
        moonbit_string_t _M0L6_2aoldS4106;
        int32_t _M0L6_2atmpS1492;
        if (
          _M0L6_2atmpS1491 < 0
          || _M0L6_2atmpS1491 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4107 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1491];
        _M0L6_2atmpS1490 = _M0L6_2atmpS4107;
        if (
          _M0L6_2atmpS1489 < 0
          || _M0L6_2atmpS1489 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4106 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1489];
        moonbit_incref(_M0L6_2atmpS1490);
        moonbit_decref(_M0L6_2aoldS4106);
        _M0L3dstS35[_M0L6_2atmpS1489] = _M0L6_2atmpS1490;
        _M0L6_2atmpS1492 = _M0L1iS42 - 1;
        _M0L1iS42 = _M0L6_2atmpS1492;
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
  int32_t _if__result_4381;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS44 == _M0L3srcS45) {
    _if__result_4381 = _M0L11dst__offsetS46 < _M0L11src__offsetS47;
  } else {
    _if__result_4381 = 0;
  }
  if (_if__result_4381) {
    int32_t _M0L1iS48 = 0;
    while (1) {
      if (_M0L1iS48 < _M0L3lenS49) {
        int32_t _M0L6_2atmpS1494 = _M0L11dst__offsetS46 + _M0L1iS48;
        int32_t _M0L6_2atmpS1496 = _M0L11src__offsetS47 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS4109;
        struct _M0TUsiE* _M0L6_2atmpS1495;
        struct _M0TUsiE* _M0L6_2aoldS4108;
        int32_t _M0L6_2atmpS1497;
        if (
          _M0L6_2atmpS1496 < 0
          || _M0L6_2atmpS1496 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4109 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1496];
        _M0L6_2atmpS1495 = _M0L6_2atmpS4109;
        if (
          _M0L6_2atmpS1494 < 0
          || _M0L6_2atmpS1494 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4108 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1494];
        if (_M0L6_2atmpS1495) {
          moonbit_incref(_M0L6_2atmpS1495);
        }
        if (_M0L6_2aoldS4108) {
          moonbit_decref(_M0L6_2aoldS4108);
        }
        _M0L3dstS44[_M0L6_2atmpS1494] = _M0L6_2atmpS1495;
        _M0L6_2atmpS1497 = _M0L1iS48 + 1;
        _M0L1iS48 = _M0L6_2atmpS1497;
        continue;
      } else {
        moonbit_decref(_M0L3srcS45);
        moonbit_decref(_M0L3dstS44);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1502 = _M0L3lenS49 - 1;
    int32_t _M0L1iS51 = _M0L6_2atmpS1502;
    while (1) {
      if (_M0L1iS51 >= 0) {
        int32_t _M0L6_2atmpS1498 = _M0L11dst__offsetS46 + _M0L1iS51;
        int32_t _M0L6_2atmpS1500 = _M0L11src__offsetS47 + _M0L1iS51;
        struct _M0TUsiE* _M0L6_2atmpS4111;
        struct _M0TUsiE* _M0L6_2atmpS1499;
        struct _M0TUsiE* _M0L6_2aoldS4110;
        int32_t _M0L6_2atmpS1501;
        if (
          _M0L6_2atmpS1500 < 0
          || _M0L6_2atmpS1500 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4111 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1500];
        _M0L6_2atmpS1499 = _M0L6_2atmpS4111;
        if (
          _M0L6_2atmpS1498 < 0
          || _M0L6_2atmpS1498 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4110 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1498];
        if (_M0L6_2atmpS1499) {
          moonbit_incref(_M0L6_2atmpS1499);
        }
        if (_M0L6_2aoldS4110) {
          moonbit_decref(_M0L6_2aoldS4110);
        }
        _M0L3dstS44[_M0L6_2atmpS1498] = _M0L6_2atmpS1499;
        _M0L6_2atmpS1501 = _M0L1iS51 - 1;
        _M0L1iS51 = _M0L6_2atmpS1501;
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
  int32_t _if__result_4384;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS53 == _M0L3srcS54) {
    _if__result_4384 = _M0L11dst__offsetS55 < _M0L11src__offsetS56;
  } else {
    _if__result_4384 = 0;
  }
  if (_if__result_4384) {
    int32_t _M0L1iS57 = 0;
    while (1) {
      if (_M0L1iS57 < _M0L3lenS58) {
        int32_t _M0L6_2atmpS1503 = _M0L11dst__offsetS55 + _M0L1iS57;
        int32_t _M0L6_2atmpS1505 = _M0L11src__offsetS56 + _M0L1iS57;
        void* _M0L6_2atmpS4113;
        void* _M0L6_2atmpS1504;
        void* _M0L6_2aoldS4112;
        int32_t _M0L6_2atmpS1506;
        if (
          _M0L6_2atmpS1505 < 0
          || _M0L6_2atmpS1505 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4113 = (void*)_M0L3srcS54[_M0L6_2atmpS1505];
        _M0L6_2atmpS1504 = _M0L6_2atmpS4113;
        if (
          _M0L6_2atmpS1503 < 0
          || _M0L6_2atmpS1503 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4112 = (void*)_M0L3dstS53[_M0L6_2atmpS1503];
        moonbit_incref(_M0L6_2atmpS1504);
        moonbit_decref(_M0L6_2aoldS4112);
        _M0L3dstS53[_M0L6_2atmpS1503] = _M0L6_2atmpS1504;
        _M0L6_2atmpS1506 = _M0L1iS57 + 1;
        _M0L1iS57 = _M0L6_2atmpS1506;
        continue;
      } else {
        moonbit_decref(_M0L3srcS54);
        moonbit_decref(_M0L3dstS53);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1511 = _M0L3lenS58 - 1;
    int32_t _M0L1iS60 = _M0L6_2atmpS1511;
    while (1) {
      if (_M0L1iS60 >= 0) {
        int32_t _M0L6_2atmpS1507 = _M0L11dst__offsetS55 + _M0L1iS60;
        int32_t _M0L6_2atmpS1509 = _M0L11src__offsetS56 + _M0L1iS60;
        void* _M0L6_2atmpS4115;
        void* _M0L6_2atmpS1508;
        void* _M0L6_2aoldS4114;
        int32_t _M0L6_2atmpS1510;
        if (
          _M0L6_2atmpS1509 < 0
          || _M0L6_2atmpS1509 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4115 = (void*)_M0L3srcS54[_M0L6_2atmpS1509];
        _M0L6_2atmpS1508 = _M0L6_2atmpS4115;
        if (
          _M0L6_2atmpS1507 < 0
          || _M0L6_2atmpS1507 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4114 = (void*)_M0L3dstS53[_M0L6_2atmpS1507];
        moonbit_incref(_M0L6_2atmpS1508);
        moonbit_decref(_M0L6_2aoldS4114);
        _M0L3dstS53[_M0L6_2atmpS1507] = _M0L6_2atmpS1508;
        _M0L6_2atmpS1510 = _M0L1iS60 - 1;
        _M0L1iS60 = _M0L6_2atmpS1510;
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
  moonbit_string_t _M0L6_2atmpS1460;
  moonbit_string_t _M0L6_2atmpS4118;
  moonbit_string_t _M0L6_2atmpS1458;
  moonbit_string_t _M0L6_2atmpS1459;
  moonbit_string_t _M0L6_2atmpS4117;
  moonbit_string_t _M0L6_2atmpS1457;
  moonbit_string_t _M0L6_2atmpS4116;
  moonbit_string_t _M0L6_2atmpS1456;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1460 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4118
  = moonbit_add_string(_M0L6_2atmpS1460, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1460);
  _M0L6_2atmpS1458 = _M0L6_2atmpS4118;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1459
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4117 = moonbit_add_string(_M0L6_2atmpS1458, _M0L6_2atmpS1459);
  moonbit_decref(_M0L6_2atmpS1458);
  moonbit_decref(_M0L6_2atmpS1459);
  _M0L6_2atmpS1457 = _M0L6_2atmpS4117;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4116
  = moonbit_add_string(_M0L6_2atmpS1457, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1457);
  _M0L6_2atmpS1456 = _M0L6_2atmpS4116;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1456);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1465;
  moonbit_string_t _M0L6_2atmpS4121;
  moonbit_string_t _M0L6_2atmpS1463;
  moonbit_string_t _M0L6_2atmpS1464;
  moonbit_string_t _M0L6_2atmpS4120;
  moonbit_string_t _M0L6_2atmpS1462;
  moonbit_string_t _M0L6_2atmpS4119;
  moonbit_string_t _M0L6_2atmpS1461;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1465 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4121
  = moonbit_add_string(_M0L6_2atmpS1465, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1465);
  _M0L6_2atmpS1463 = _M0L6_2atmpS4121;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1464
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4120 = moonbit_add_string(_M0L6_2atmpS1463, _M0L6_2atmpS1464);
  moonbit_decref(_M0L6_2atmpS1463);
  moonbit_decref(_M0L6_2atmpS1464);
  _M0L6_2atmpS1462 = _M0L6_2atmpS4120;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4119
  = moonbit_add_string(_M0L6_2atmpS1462, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1462);
  _M0L6_2atmpS1461 = _M0L6_2atmpS4119;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1461);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1470;
  moonbit_string_t _M0L6_2atmpS4124;
  moonbit_string_t _M0L6_2atmpS1468;
  moonbit_string_t _M0L6_2atmpS1469;
  moonbit_string_t _M0L6_2atmpS4123;
  moonbit_string_t _M0L6_2atmpS1467;
  moonbit_string_t _M0L6_2atmpS4122;
  moonbit_string_t _M0L6_2atmpS1466;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1470 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4124
  = moonbit_add_string(_M0L6_2atmpS1470, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1470);
  _M0L6_2atmpS1468 = _M0L6_2atmpS4124;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1469
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4123 = moonbit_add_string(_M0L6_2atmpS1468, _M0L6_2atmpS1469);
  moonbit_decref(_M0L6_2atmpS1468);
  moonbit_decref(_M0L6_2atmpS1469);
  _M0L6_2atmpS1467 = _M0L6_2atmpS4123;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4122
  = moonbit_add_string(_M0L6_2atmpS1467, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1467);
  _M0L6_2atmpS1466 = _M0L6_2atmpS4122;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1466);
}

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1475;
  moonbit_string_t _M0L6_2atmpS4127;
  moonbit_string_t _M0L6_2atmpS1473;
  moonbit_string_t _M0L6_2atmpS1474;
  moonbit_string_t _M0L6_2atmpS4126;
  moonbit_string_t _M0L6_2atmpS1472;
  moonbit_string_t _M0L6_2atmpS4125;
  moonbit_string_t _M0L6_2atmpS1471;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1475 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4127
  = moonbit_add_string(_M0L6_2atmpS1475, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1475);
  _M0L6_2atmpS1473 = _M0L6_2atmpS4127;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1474
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4126 = moonbit_add_string(_M0L6_2atmpS1473, _M0L6_2atmpS1474);
  moonbit_decref(_M0L6_2atmpS1473);
  moonbit_decref(_M0L6_2atmpS1474);
  _M0L6_2atmpS1472 = _M0L6_2atmpS4126;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4125
  = moonbit_add_string(_M0L6_2atmpS1472, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1472);
  _M0L6_2atmpS1471 = _M0L6_2atmpS4125;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC15bytes9BytesViewE(_M0L6_2atmpS1471);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1455;
  uint32_t _M0L6_2atmpS1454;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1455 = _M0L4selfS16->$0;
  _M0L6_2atmpS1454 = _M0L3accS1455 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1454;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1452;
  uint32_t _M0L6_2atmpS1453;
  uint32_t _M0L6_2atmpS1451;
  uint32_t _M0L6_2atmpS1450;
  uint32_t _M0L6_2atmpS1449;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1452 = _M0L4selfS14->$0;
  _M0L6_2atmpS1453 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1451 = _M0L3accS1452 + _M0L6_2atmpS1453;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1450 = _M0FPB4rotl(_M0L6_2atmpS1451, 17);
  _M0L6_2atmpS1449 = _M0L6_2atmpS1450 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1449;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1446;
  int32_t _M0L6_2atmpS1448;
  uint32_t _M0L6_2atmpS1447;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1446 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1448 = 32 - _M0L1rS13;
  _M0L6_2atmpS1447 = _M0L1xS12 >> (_M0L6_2atmpS1448 & 31);
  return _M0L6_2atmpS1446 | _M0L6_2atmpS1447;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS4128;
  int32_t _M0L6_2acntS4215;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS4128 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS4215 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS4215 > 1) {
    int32_t _M0L11_2anew__cntS4216 = _M0L6_2acntS4215 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS4216;
    moonbit_incref(_M0L8_2afieldS4128);
  } else if (_M0L6_2acntS4215 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS4128;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_70.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_71.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_4387;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4387 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4387)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4387)->$0 = _M0L4selfS7;
  return _block_4387;
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

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1401) {
  switch (Moonbit_object_tag(_M0L4_2aeS1401)) {
    case 5: {
      moonbit_decref(_M0L4_2aeS1401);
      return (moonbit_string_t)moonbit_string_literal_72.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1401);
      return (moonbit_string_t)moonbit_string_literal_73.data;
      break;
    }
    
    case 1: {
      return _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(_M0L4_2aeS1401);
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1401);
      return (moonbit_string_t)moonbit_string_literal_74.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1401);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1401);
      return (moonbit_string_t)moonbit_string_literal_75.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1427,
  int32_t _M0L8_2aparamS1426
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1425 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1427;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1425, _M0L8_2aparamS1426);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1424,
  struct _M0TPC16string10StringView _M0L8_2aparamS1423
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1422 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1424;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1422, _M0L8_2aparamS1423);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1421,
  moonbit_string_t _M0L8_2aparamS1418,
  int32_t _M0L8_2aparamS1419,
  int32_t _M0L8_2aparamS1420
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1417 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1421;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1417, _M0L8_2aparamS1418, _M0L8_2aparamS1419, _M0L8_2aparamS1420);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1416,
  moonbit_string_t _M0L8_2aparamS1415
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1414 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1416;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1414, _M0L8_2aparamS1415);
  return 0;
}

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1412
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1413 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1412;
  int32_t _M0L8_2afieldS4129 = _M0L14_2aboxed__selfS1413->$0;
  int32_t _M0L7_2aselfS1411;
  moonbit_decref(_M0L14_2aboxed__selfS1413);
  _M0L7_2aselfS1411 = _M0L8_2afieldS4129;
  return _M0IPC13int3IntPB6ToJson8to__json(_M0L7_2aselfS1411);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1445 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1444;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1443;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1327;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1442;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1441;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1440;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1435;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1328;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1439;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1438;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1437;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1436;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1326;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1434;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1433;
  _M0L6_2atmpS1445[0] = (moonbit_string_t)moonbit_string_literal_76.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal20rand__blackbox__test45____test__72616e645f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1444
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1444)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1444->$0
  = _M0FP48clawteam8clawteam8internal20rand__blackbox__test45____test__72616e645f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1444->$1 = _M0L6_2atmpS1445;
  _M0L8_2atupleS1443
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1443)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1443->$0 = 0;
  _M0L8_2atupleS1443->$1 = _M0L8_2atupleS1444;
  _M0L7_2abindS1327
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1327[0] = _M0L8_2atupleS1443;
  _M0L6_2atmpS1442 = _M0L7_2abindS1327;
  _M0L6_2atmpS1441
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1442
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1440
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1441);
  _M0L8_2atupleS1435
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1435)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1435->$0 = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L8_2atupleS1435->$1 = _M0L6_2atmpS1440;
  _M0L7_2abindS1328
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1439 = _M0L7_2abindS1328;
  _M0L6_2atmpS1438
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1439
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1437
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1438);
  _M0L8_2atupleS1436
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1436)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1436->$0 = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L8_2atupleS1436->$1 = _M0L6_2atmpS1437;
  _M0L7_2abindS1326
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1326[0] = _M0L8_2atupleS1435;
  _M0L7_2abindS1326[1] = _M0L8_2atupleS1436;
  _M0L6_2atmpS1434 = _M0L7_2abindS1326;
  _M0L6_2atmpS1433
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1434
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal20rand__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1433);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1432;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1395;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1396;
  int32_t _M0L7_2abindS1397;
  int32_t _M0L2__S1398;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1432
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1395
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1395)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1395->$0 = _M0L6_2atmpS1432;
  _M0L12async__testsS1395->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1396
  = _M0FP48clawteam8clawteam8internal20rand__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1397 = _M0L7_2abindS1396->$1;
  _M0L2__S1398 = 0;
  while (1) {
    if (_M0L2__S1398 < _M0L7_2abindS1397) {
      struct _M0TUsiE** _M0L8_2afieldS4133 = _M0L7_2abindS1396->$0;
      struct _M0TUsiE** _M0L3bufS1431 = _M0L8_2afieldS4133;
      struct _M0TUsiE* _M0L6_2atmpS4132 =
        (struct _M0TUsiE*)_M0L3bufS1431[_M0L2__S1398];
      struct _M0TUsiE* _M0L3argS1399 = _M0L6_2atmpS4132;
      moonbit_string_t _M0L8_2afieldS4131 = _M0L3argS1399->$0;
      moonbit_string_t _M0L6_2atmpS1428 = _M0L8_2afieldS4131;
      int32_t _M0L8_2afieldS4130 = _M0L3argS1399->$1;
      int32_t _M0L6_2atmpS1429 = _M0L8_2afieldS4130;
      int32_t _M0L6_2atmpS1430;
      moonbit_incref(_M0L6_2atmpS1428);
      moonbit_incref(_M0L12async__testsS1395);
      #line 441 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal20rand__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1395, _M0L6_2atmpS1428, _M0L6_2atmpS1429);
      _M0L6_2atmpS1430 = _M0L2__S1398 + 1;
      _M0L2__S1398 = _M0L6_2atmpS1430;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1396);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\internal\\rand\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal20rand__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal20rand__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1395);
  return 0;
}