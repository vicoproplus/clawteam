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

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPB4Json5Array;

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

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0TPB19MulShiftAll64Result;

struct _M0TPB5ArrayGiE;

struct _M0TWEOUsRPB4JsonE;

struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGbRP58clawteam8clawteam8internal5httpx22status__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__;

struct _M0KTPB6ToJsonS3Int;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160;

struct _M0R38String_3a_3aiter_2eanon__u1804__l247__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGbRP58clawteam8clawteam8internal5httpx22status__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0DTPC15error5Error133clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0DTPC16result6ResultGuRPB7FailureE3Err;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok;

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

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TPB5ArrayGiE {
  int32_t $1;
  int32_t* $0;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0DTPC16result6ResultGbRP58clawteam8clawteam8internal5httpx22status__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0R38String_3a_3aiter_2eanon__u1804__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
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

struct _M0DTPC16result6ResultGbRP58clawteam8clawteam8internal5httpx22status__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0DTPC15error5Error133clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGuRPB7FailureE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok {
  int32_t $0;
  
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

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test53____test__7374617475735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test51____test__636c6173735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1169(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1160(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testC3449l432(
  struct _M0TWEOc*
);

int32_t _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testC3445l433(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1091(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1086(
  int32_t
);

moonbit_string_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1073(
  int32_t,
  moonbit_string_t
);

#define _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP58clawteam8clawteam8internal5httpx22status__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test41____test__636c6173735f746573742e6d6274__0(
  
);

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test43____test__7374617475735f746573742e6d6274__0(
  
);

int32_t _M0FP58clawteam8clawteam8internal5httpx6status8classify(int32_t);

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

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t,
  moonbit_string_t,
  moonbit_string_t
);

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

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2007l591(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1823l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1804l247(struct _M0TWEOc*);

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

struct moonbit_result_0 _M0FPB4failGuE(moonbit_string_t, moonbit_string_t);

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

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_142 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 53, 58, 53, 51, 45, 52, 53, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 55, 58, 49, 54, 45, 51, 55, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_176 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 54, 58, 51, 45, 53, 54, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 55, 58, 52, 57, 45, 49, 55, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_136 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 51, 58, 52, 51, 45, 52, 51, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 53, 58, 49, 54, 45, 49, 53, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_154 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 57, 58, 52, 57, 45, 52, 57, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 58, 52, 49, 45, 56, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_174 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 54, 58, 49, 54, 45, 53, 54, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 48, 58, 49, 54, 45, 52, 48, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_171 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 53, 58, 49, 54, 45, 53, 53, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 49, 58, 51, 45, 49, 49, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 58, 49, 54, 45, 51, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_173 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 53, 58, 51, 45, 53, 53, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 51, 58, 51, 45, 51, 51, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_181 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 56, 58, 52, 56, 45, 53, 56, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_144 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 54, 58, 49, 54, 45, 52, 54, 58, 51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_162 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 50, 58, 49, 54, 45, 53, 50, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_220 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 53, 58, 52, 57, 45, 49, 53, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 54, 58, 51, 45, 51, 54, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 49, 58, 49, 54, 45, 49, 49, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_221 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_231 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_155 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 57, 58, 51, 45, 52, 57, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_188 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 48, 58, 51, 45, 54, 48, 58, 53, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 56, 58, 49, 54, 45, 51, 56, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_143 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 53, 58, 51, 45, 52, 53, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 48, 58, 49, 54, 45, 49, 48, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_202 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 58, 51, 45, 54, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_227 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_149 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 55, 58, 51, 45, 52, 55, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 50, 58, 51, 45, 51, 50, 58, 54, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 51, 58, 49, 54, 45, 50, 51, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 58, 52, 52, 45, 54, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_203 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 53, 58, 51, 56, 45, 51, 53, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 57, 58, 51, 45, 49, 57, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 57, 58, 51, 45, 50, 57, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_219 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 50, 58, 54, 49, 45, 51, 50, 58, 54, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 50, 58, 52, 50, 45, 50, 50, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_213 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 49, 58, 53, 51, 45, 52, 49, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_187 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 48, 58, 53, 53, 45, 54, 48, 58, 53, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_147 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 55, 58, 49, 54, 45, 52, 55, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 57, 58, 51, 45, 51, 57, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 48, 58, 51, 45, 50, 48, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 52, 58, 49, 54, 45, 51, 52, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_270 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    99, 108, 97, 115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_240 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 57, 58, 49, 54, 45, 50, 57, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 52, 58, 49, 54, 45, 49, 52, 58, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_223 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 52, 58, 51, 45, 49, 52, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 48, 58, 53, 48, 45, 51, 48, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 58, 51, 45, 56, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_252 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 50, 58, 51, 45, 50, 50, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 99, 108, 97, 
    115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 58, 
    53, 45, 49, 48, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 52, 58, 51, 45, 50, 52, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_249 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[79]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 78), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 99, 108, 97, 
    115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 58, 53, 
    45, 55, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_193 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 50, 58, 52, 54, 45, 54, 50, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_172 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 53, 58, 52, 56, 45, 53, 53, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 57, 58, 52, 50, 45, 50, 57, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    55, 58, 49, 54, 45, 55, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 58, 49, 54, 45, 54, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_239 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_245 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 49, 58, 52, 51, 45, 49, 49, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_218 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 57, 58, 49, 54, 45, 51, 57, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_246 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_164 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 50, 58, 51, 45, 53, 50, 58, 54, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 55, 58, 53, 50, 45, 51, 55, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 48, 58, 51, 45, 51, 48, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_209 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 54, 58, 49, 54, 45, 50, 54, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_201 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 52, 58, 53, 49, 45, 50, 52, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 55, 58, 49, 54, 45, 49, 55, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_271 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    99, 108, 97, 115, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_232 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_189 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 49, 58, 49, 54, 45, 54, 49, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_258 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_197 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 51, 58, 51, 45, 54, 51, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 55, 58, 51, 45, 51, 55, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 58, 52, 52, 45, 53, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_247 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_242 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 56, 58, 52, 51, 45, 50, 56, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_182 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 56, 58, 51, 45, 53, 56, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 56, 58, 49, 54, 45, 49, 56, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 99, 108, 97, 
    115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 57, 58, 
    53, 45, 49, 57, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 58, 51, 45, 53, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_140 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 52, 58, 51, 45, 52, 52, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_158 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 48, 58, 51, 45, 53, 48, 58, 53, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 49, 58, 52, 53, 45, 50, 49, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_159 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 49, 58, 49, 54, 45, 53, 49, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 58, 49, 54, 45, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_191 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 49, 58, 51, 45, 54, 49, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 58, 49, 54, 45, 53, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_260 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_198 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 52, 58, 49, 54, 45, 54, 52, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 54, 58, 51, 45, 50, 54, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_212 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_184 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 57, 58, 53, 55, 45, 53, 57, 58, 54, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_250 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 57, 58, 51, 57, 45, 49, 57, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_237 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_257 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_168 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 52, 58, 49, 54, 45, 53, 52, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 48, 58, 49, 54, 45, 50, 48, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_269 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    99, 108, 97, 115, 115, 105, 102, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_185 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 57, 58, 51, 45, 53, 57, 58, 54, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_208 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_179 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 55, 58, 51, 45, 53, 55, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_163 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 50, 58, 54, 49, 45, 53, 50, 58, 54, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 54, 58, 49, 54, 45, 51, 54, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 49, 58, 51, 45, 50, 49, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_251 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_226 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 55, 58, 51, 45, 49, 55, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_256 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_222 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 51, 58, 49, 54, 45, 51, 51, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 48, 58, 52, 50, 45, 50, 48, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_161 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 49, 58, 51, 45, 53, 49, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 50, 58, 49, 54, 45, 49, 50, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_236 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 57, 58, 49, 54, 45, 49, 57, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_215 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_148 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 55, 58, 53, 48, 45, 52, 55, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_207 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_134 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 50, 58, 51, 45, 52, 50, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 51, 58, 52, 56, 45, 51, 51, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 51, 58, 49, 54, 45, 49, 51, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_165 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 51, 58, 49, 54, 45, 53, 51, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_135 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 51, 58, 49, 54, 45, 52, 51, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 99, 108, 97, 
    115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 54, 58, 
    53, 45, 49, 54, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_262 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_224 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_175 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 54, 58, 52, 52, 45, 53, 54, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 48, 58, 51, 45, 49, 48, 58, 54, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_255 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_235 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_233 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    57, 58, 51, 45, 57, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_180 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 56, 58, 49, 54, 45, 53, 56, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_186 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 48, 58, 49, 54, 45, 54, 48, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_137 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 51, 58, 51, 45, 52, 51, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_266 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 116, 97, 116, 117, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_230 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_216 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_139 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 52, 58, 53, 50, 45, 52, 52, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 54, 58, 49, 54, 45, 49, 54, 58, 51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    55, 58, 51, 45, 55, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 56, 58, 52, 57, 45, 51, 56, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 52, 58, 51, 45, 51, 52, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 55, 58, 51, 45, 50, 55, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 99, 108, 97, 
    115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 
    53, 45, 49, 51, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_210 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 51, 58, 51, 45, 50, 51, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_261 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_169 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 52, 58, 53, 51, 45, 53, 52, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 48, 58, 54, 49, 45, 49, 48, 58, 54, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 53, 58, 51, 45, 50, 53, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_156 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 48, 58, 49, 54, 45, 53, 48, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_178 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 55, 58, 53, 50, 45, 53, 55, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 56, 58, 49, 54, 45, 50, 56, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 54, 58, 52, 48, 45, 49, 54, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_253 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_170 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 52, 58, 51, 45, 53, 52, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 50, 58, 53, 49, 45, 52, 50, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_254 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 52, 58, 49, 54, 45, 50, 52, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_177 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 55, 58, 49, 54, 45, 53, 55, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 58, 52, 50, 45, 51, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_238 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    96, 32, 105, 115, 32, 110, 111, 116, 32, 116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_228 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_225 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_234 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_145 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 54, 58, 52, 48, 45, 52, 54, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 50, 58, 49, 54, 45, 51, 50, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 48, 58, 53, 52, 45, 52, 48, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_200 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 52, 58, 51, 45, 54, 52, 58, 54, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_214 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[120]; 
} const moonbit_string_literal_263 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 119), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 116, 
    116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 97, 
    99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 
    114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 54, 58, 51, 45, 49, 54, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[79]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 78), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 99, 108, 97, 
    115, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 52, 58, 53, 
    45, 52, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_195 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 51, 58, 49, 54, 45, 54, 51, 58, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 51, 58, 52, 56, 45, 49, 51, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_194 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 50, 58, 51, 45, 54, 50, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 54, 58, 52, 56, 45, 51, 54, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_167 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 51, 58, 51, 45, 53, 51, 58, 54, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 50, 58, 52, 54, 45, 49, 50, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_160 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 49, 58, 52, 57, 45, 53, 49, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    57, 58, 49, 54, 45, 57, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 51, 58, 51, 45, 49, 51, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 58, 51, 45, 52, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_153 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 57, 58, 49, 54, 45, 52, 57, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 48, 58, 49, 54, 45, 51, 48, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 57, 58, 52, 52, 45, 51, 57, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 49, 58, 51, 45, 51, 49, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 52, 58, 52, 50, 45, 51, 52, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_229 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_206 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_199 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 52, 58, 54, 51, 45, 54, 52, 58, 54, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_259 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_196 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 51, 58, 52, 53, 45, 54, 51, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 53, 58, 52, 52, 45, 50, 53, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 53, 58, 51, 45, 49, 53, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_244 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 49, 58, 49, 54, 45, 50, 49, 58, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_268 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 116, 97, 116, 117, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 116, 116, 112, 
    120, 47, 115, 116, 97, 116, 117, 115, 34, 44, 32, 34, 102, 105, 108, 
    101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_267 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    115, 116, 97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 50, 58, 49, 54, 45, 52, 50, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_157 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 48, 58, 53, 52, 45, 53, 48, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 50, 58, 51, 45, 49, 50, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    55, 58, 51, 54, 45, 55, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 58, 49, 54, 45, 52, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_204 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_150 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 56, 58, 49, 54, 45, 52, 56, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[122]; 
} const moonbit_string_literal_264 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 121), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 116, 
    116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 97, 
    99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_141 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 53, 58, 49, 54, 45, 52, 53, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 54, 58, 52, 54, 45, 50, 54, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_265 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_211 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 53, 58, 51, 45, 51, 53, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_217 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 49, 58, 49, 54, 45, 52, 49, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_183 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 57, 58, 49, 54, 45, 53, 57, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 55, 58, 52, 57, 45, 50, 55, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 55, 58, 49, 54, 45, 50, 55, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 56, 58, 53, 48, 45, 49, 56, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 52, 58, 52, 53, 45, 49, 52, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 58, 51, 45, 51, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_152 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 56, 58, 51, 45, 52, 56, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 49, 58, 52, 55, 45, 51, 49, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_241 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 53, 58, 49, 54, 45, 50, 53, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_205 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 51, 58, 53, 49, 45, 50, 51, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_192 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 50, 58, 49, 54, 45, 54, 50, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_243 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 56, 58, 51, 45, 50, 56, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_190 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 49, 58, 53, 51, 45, 54, 49, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_248 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    57, 58, 52, 50, 45, 57, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 58, 53, 50, 45, 52, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 48, 58, 51, 45, 52, 48, 58, 53, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 49, 58, 51, 45, 52, 49, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 56, 58, 51, 45, 51, 56, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_146 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 54, 58, 51, 45, 52, 54, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 56, 58, 51, 45, 49, 56, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 53, 58, 49, 54, 45, 51, 53, 58, 50, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_166 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 51, 58, 54, 48, 45, 53, 51, 58, 54, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 49, 58, 49, 54, 45, 51, 49, 58, 51, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 50, 58, 49, 54, 45, 50, 50, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_138 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 52, 58, 49, 54, 45, 52, 52, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[83]; 
} const moonbit_string_literal_151 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 82), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 104, 
    116, 116, 112, 120, 47, 115, 116, 97, 116, 117, 115, 95, 98, 108, 
    97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 115, 116, 
    97, 116, 117, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 56, 58, 52, 50, 45, 52, 56, 58, 52, 53, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1169$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1169
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test51____test__636c6173735f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test51____test__636c6173735f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test53____test__7374617475735f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test53____test__7374617475735f746573742e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test47____test__636c6173735f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test51____test__636c6173735f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test49____test__7374617475735f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test53____test__7374617475735f746573742e6d6274__0_2edyncall$closure.data;

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
} _M0FPB30ryu__to__string_2erecord_2f907$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f907 =
  &_M0FPB30ryu__to__string_2erecord_2f907$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test53____test__7374617475735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3481
) {
  return _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test43____test__7374617475735f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test51____test__636c6173735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3480
) {
  return _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test41____test__636c6173735f746573742e6d6274__0();
}

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1190,
  moonbit_string_t _M0L8filenameS1165,
  int32_t _M0L5indexS1168
) {
  struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160* _closure_3811;
  struct _M0TWssbEu* _M0L14handle__resultS1160;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1169;
  void* _M0L11_2atry__errS1184;
  struct moonbit_result_0 _tmp_3813;
  int32_t _handle__error__result_3814;
  int32_t _M0L6_2atmpS3468;
  void* _M0L3errS1185;
  moonbit_string_t _M0L4nameS1187;
  struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1188;
  moonbit_string_t _M0L8_2afieldS3482;
  int32_t _M0L6_2acntS3728;
  moonbit_string_t _M0L7_2anameS1189;
  #line 531 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1165);
  _closure_3811
  = (struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160*)moonbit_malloc(sizeof(struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160));
  Moonbit_object_header(_closure_3811)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160, $1) >> 2, 1, 0);
  _closure_3811->code
  = &_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1160;
  _closure_3811->$0 = _M0L5indexS1168;
  _closure_3811->$1 = _M0L8filenameS1165;
  _M0L14handle__resultS1160 = (struct _M0TWssbEu*)_closure_3811;
  _M0L17error__to__stringS1169
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1169$closure.data;
  moonbit_incref(_M0L12async__testsS1190);
  moonbit_incref(_M0L17error__to__stringS1169);
  moonbit_incref(_M0L8filenameS1165);
  moonbit_incref(_M0L14handle__resultS1160);
  #line 565 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3813
  = _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1190, _M0L8filenameS1165, _M0L5indexS1168, _M0L14handle__resultS1160, _M0L17error__to__stringS1169);
  if (_tmp_3813.tag) {
    int32_t const _M0L5_2aokS3477 = _tmp_3813.data.ok;
    _handle__error__result_3814 = _M0L5_2aokS3477;
  } else {
    void* const _M0L6_2aerrS3478 = _tmp_3813.data.err;
    moonbit_decref(_M0L12async__testsS1190);
    moonbit_decref(_M0L17error__to__stringS1169);
    moonbit_decref(_M0L8filenameS1165);
    _M0L11_2atry__errS1184 = _M0L6_2aerrS3478;
    goto join_1183;
  }
  if (_handle__error__result_3814) {
    moonbit_decref(_M0L12async__testsS1190);
    moonbit_decref(_M0L17error__to__stringS1169);
    moonbit_decref(_M0L8filenameS1165);
    _M0L6_2atmpS3468 = 1;
  } else {
    struct moonbit_result_0 _tmp_3815;
    int32_t _handle__error__result_3816;
    moonbit_incref(_M0L12async__testsS1190);
    moonbit_incref(_M0L17error__to__stringS1169);
    moonbit_incref(_M0L8filenameS1165);
    moonbit_incref(_M0L14handle__resultS1160);
    #line 568 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    _tmp_3815
    = _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1190, _M0L8filenameS1165, _M0L5indexS1168, _M0L14handle__resultS1160, _M0L17error__to__stringS1169);
    if (_tmp_3815.tag) {
      int32_t const _M0L5_2aokS3475 = _tmp_3815.data.ok;
      _handle__error__result_3816 = _M0L5_2aokS3475;
    } else {
      void* const _M0L6_2aerrS3476 = _tmp_3815.data.err;
      moonbit_decref(_M0L12async__testsS1190);
      moonbit_decref(_M0L17error__to__stringS1169);
      moonbit_decref(_M0L8filenameS1165);
      _M0L11_2atry__errS1184 = _M0L6_2aerrS3476;
      goto join_1183;
    }
    if (_handle__error__result_3816) {
      moonbit_decref(_M0L12async__testsS1190);
      moonbit_decref(_M0L17error__to__stringS1169);
      moonbit_decref(_M0L8filenameS1165);
      _M0L6_2atmpS3468 = 1;
    } else {
      struct moonbit_result_0 _tmp_3817;
      int32_t _handle__error__result_3818;
      moonbit_incref(_M0L12async__testsS1190);
      moonbit_incref(_M0L17error__to__stringS1169);
      moonbit_incref(_M0L8filenameS1165);
      moonbit_incref(_M0L14handle__resultS1160);
      #line 571 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _tmp_3817
      = _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1190, _M0L8filenameS1165, _M0L5indexS1168, _M0L14handle__resultS1160, _M0L17error__to__stringS1169);
      if (_tmp_3817.tag) {
        int32_t const _M0L5_2aokS3473 = _tmp_3817.data.ok;
        _handle__error__result_3818 = _M0L5_2aokS3473;
      } else {
        void* const _M0L6_2aerrS3474 = _tmp_3817.data.err;
        moonbit_decref(_M0L12async__testsS1190);
        moonbit_decref(_M0L17error__to__stringS1169);
        moonbit_decref(_M0L8filenameS1165);
        _M0L11_2atry__errS1184 = _M0L6_2aerrS3474;
        goto join_1183;
      }
      if (_handle__error__result_3818) {
        moonbit_decref(_M0L12async__testsS1190);
        moonbit_decref(_M0L17error__to__stringS1169);
        moonbit_decref(_M0L8filenameS1165);
        _M0L6_2atmpS3468 = 1;
      } else {
        struct moonbit_result_0 _tmp_3819;
        int32_t _handle__error__result_3820;
        moonbit_incref(_M0L12async__testsS1190);
        moonbit_incref(_M0L17error__to__stringS1169);
        moonbit_incref(_M0L8filenameS1165);
        moonbit_incref(_M0L14handle__resultS1160);
        #line 574 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        _tmp_3819
        = _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1190, _M0L8filenameS1165, _M0L5indexS1168, _M0L14handle__resultS1160, _M0L17error__to__stringS1169);
        if (_tmp_3819.tag) {
          int32_t const _M0L5_2aokS3471 = _tmp_3819.data.ok;
          _handle__error__result_3820 = _M0L5_2aokS3471;
        } else {
          void* const _M0L6_2aerrS3472 = _tmp_3819.data.err;
          moonbit_decref(_M0L12async__testsS1190);
          moonbit_decref(_M0L17error__to__stringS1169);
          moonbit_decref(_M0L8filenameS1165);
          _M0L11_2atry__errS1184 = _M0L6_2aerrS3472;
          goto join_1183;
        }
        if (_handle__error__result_3820) {
          moonbit_decref(_M0L12async__testsS1190);
          moonbit_decref(_M0L17error__to__stringS1169);
          moonbit_decref(_M0L8filenameS1165);
          _M0L6_2atmpS3468 = 1;
        } else {
          struct moonbit_result_0 _tmp_3821;
          moonbit_incref(_M0L14handle__resultS1160);
          #line 577 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
          _tmp_3821
          = _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1190, _M0L8filenameS1165, _M0L5indexS1168, _M0L14handle__resultS1160, _M0L17error__to__stringS1169);
          if (_tmp_3821.tag) {
            int32_t const _M0L5_2aokS3469 = _tmp_3821.data.ok;
            _M0L6_2atmpS3468 = _M0L5_2aokS3469;
          } else {
            void* const _M0L6_2aerrS3470 = _tmp_3821.data.err;
            _M0L11_2atry__errS1184 = _M0L6_2aerrS3470;
            goto join_1183;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3468) {
    void* _M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3479 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3479)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3479)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1184
    = _M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3479;
    goto join_1183;
  } else {
    moonbit_decref(_M0L14handle__resultS1160);
  }
  goto joinlet_3812;
  join_1183:;
  _M0L3errS1185 = _M0L11_2atry__errS1184;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1188
  = (struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1185;
  _M0L8_2afieldS3482 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1188->$0;
  _M0L6_2acntS3728
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1188)->rc;
  if (_M0L6_2acntS3728 > 1) {
    int32_t _M0L11_2anew__cntS3729 = _M0L6_2acntS3728 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1188)->rc
    = _M0L11_2anew__cntS3729;
    moonbit_incref(_M0L8_2afieldS3482);
  } else if (_M0L6_2acntS3728 == 1) {
    #line 584 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1188);
  }
  _M0L7_2anameS1189 = _M0L8_2afieldS3482;
  _M0L4nameS1187 = _M0L7_2anameS1189;
  goto join_1186;
  goto joinlet_3822;
  join_1186:;
  #line 585 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1160(_M0L14handle__resultS1160, _M0L4nameS1187, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3822:;
  joinlet_3812:;
  return 0;
}

moonbit_string_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1169(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3467,
  void* _M0L3errS1170
) {
  void* _M0L1eS1172;
  moonbit_string_t _M0L1eS1174;
  #line 554 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3467);
  switch (Moonbit_object_tag(_M0L3errS1170)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1175 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1170;
      moonbit_string_t _M0L8_2afieldS3483 = _M0L10_2aFailureS1175->$0;
      int32_t _M0L6_2acntS3730 =
        Moonbit_object_header(_M0L10_2aFailureS1175)->rc;
      moonbit_string_t _M0L4_2aeS1176;
      if (_M0L6_2acntS3730 > 1) {
        int32_t _M0L11_2anew__cntS3731 = _M0L6_2acntS3730 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1175)->rc
        = _M0L11_2anew__cntS3731;
        moonbit_incref(_M0L8_2afieldS3483);
      } else if (_M0L6_2acntS3730 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1175);
      }
      _M0L4_2aeS1176 = _M0L8_2afieldS3483;
      _M0L1eS1174 = _M0L4_2aeS1176;
      goto join_1173;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1177 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1170;
      moonbit_string_t _M0L8_2afieldS3484 = _M0L15_2aInspectErrorS1177->$0;
      int32_t _M0L6_2acntS3732 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1177)->rc;
      moonbit_string_t _M0L4_2aeS1178;
      if (_M0L6_2acntS3732 > 1) {
        int32_t _M0L11_2anew__cntS3733 = _M0L6_2acntS3732 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1177)->rc
        = _M0L11_2anew__cntS3733;
        moonbit_incref(_M0L8_2afieldS3484);
      } else if (_M0L6_2acntS3732 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1177);
      }
      _M0L4_2aeS1178 = _M0L8_2afieldS3484;
      _M0L1eS1174 = _M0L4_2aeS1178;
      goto join_1173;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1179 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1170;
      moonbit_string_t _M0L8_2afieldS3485 = _M0L16_2aSnapshotErrorS1179->$0;
      int32_t _M0L6_2acntS3734 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1179)->rc;
      moonbit_string_t _M0L4_2aeS1180;
      if (_M0L6_2acntS3734 > 1) {
        int32_t _M0L11_2anew__cntS3735 = _M0L6_2acntS3734 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1179)->rc
        = _M0L11_2anew__cntS3735;
        moonbit_incref(_M0L8_2afieldS3485);
      } else if (_M0L6_2acntS3734 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1179);
      }
      _M0L4_2aeS1180 = _M0L8_2afieldS3485;
      _M0L1eS1174 = _M0L4_2aeS1180;
      goto join_1173;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error133clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1181 =
        (struct _M0DTPC15error5Error133clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1170;
      moonbit_string_t _M0L8_2afieldS3486 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1181->$0;
      int32_t _M0L6_2acntS3736 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1181)->rc;
      moonbit_string_t _M0L4_2aeS1182;
      if (_M0L6_2acntS3736 > 1) {
        int32_t _M0L11_2anew__cntS3737 = _M0L6_2acntS3736 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1181)->rc
        = _M0L11_2anew__cntS3737;
        moonbit_incref(_M0L8_2afieldS3486);
      } else if (_M0L6_2acntS3736 == 1) {
        #line 555 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1181);
      }
      _M0L4_2aeS1182 = _M0L8_2afieldS3486;
      _M0L1eS1174 = _M0L4_2aeS1182;
      goto join_1173;
      break;
    }
    default: {
      _M0L1eS1172 = _M0L3errS1170;
      goto join_1171;
      break;
    }
  }
  join_1173:;
  return _M0L1eS1174;
  join_1171:;
  #line 560 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1172);
}

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1160(
  struct _M0TWssbEu* _M0L6_2aenvS3453,
  moonbit_string_t _M0L8testnameS1161,
  moonbit_string_t _M0L7messageS1162,
  int32_t _M0L7skippedS1163
) {
  struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160* _M0L14_2acasted__envS3454;
  moonbit_string_t _M0L8_2afieldS3496;
  moonbit_string_t _M0L8filenameS1165;
  int32_t _M0L8_2afieldS3495;
  int32_t _M0L6_2acntS3738;
  int32_t _M0L5indexS1168;
  int32_t _if__result_3825;
  moonbit_string_t _M0L10file__nameS1164;
  moonbit_string_t _M0L10test__nameS1166;
  moonbit_string_t _M0L7messageS1167;
  moonbit_string_t _M0L6_2atmpS3466;
  moonbit_string_t _M0L6_2atmpS3494;
  moonbit_string_t _M0L6_2atmpS3465;
  moonbit_string_t _M0L6_2atmpS3493;
  moonbit_string_t _M0L6_2atmpS3463;
  moonbit_string_t _M0L6_2atmpS3464;
  moonbit_string_t _M0L6_2atmpS3492;
  moonbit_string_t _M0L6_2atmpS3462;
  moonbit_string_t _M0L6_2atmpS3491;
  moonbit_string_t _M0L6_2atmpS3460;
  moonbit_string_t _M0L6_2atmpS3461;
  moonbit_string_t _M0L6_2atmpS3490;
  moonbit_string_t _M0L6_2atmpS3459;
  moonbit_string_t _M0L6_2atmpS3489;
  moonbit_string_t _M0L6_2atmpS3457;
  moonbit_string_t _M0L6_2atmpS3458;
  moonbit_string_t _M0L6_2atmpS3488;
  moonbit_string_t _M0L6_2atmpS3456;
  moonbit_string_t _M0L6_2atmpS3487;
  moonbit_string_t _M0L6_2atmpS3455;
  #line 538 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3454
  = (struct _M0R137_24clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1160*)_M0L6_2aenvS3453;
  _M0L8_2afieldS3496 = _M0L14_2acasted__envS3454->$1;
  _M0L8filenameS1165 = _M0L8_2afieldS3496;
  _M0L8_2afieldS3495 = _M0L14_2acasted__envS3454->$0;
  _M0L6_2acntS3738 = Moonbit_object_header(_M0L14_2acasted__envS3454)->rc;
  if (_M0L6_2acntS3738 > 1) {
    int32_t _M0L11_2anew__cntS3739 = _M0L6_2acntS3738 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3454)->rc
    = _M0L11_2anew__cntS3739;
    moonbit_incref(_M0L8filenameS1165);
  } else if (_M0L6_2acntS3738 == 1) {
    #line 538 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3454);
  }
  _M0L5indexS1168 = _M0L8_2afieldS3495;
  if (!_M0L7skippedS1163) {
    _if__result_3825 = 1;
  } else {
    _if__result_3825 = 0;
  }
  if (_if__result_3825) {
    
  }
  #line 544 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1164 = _M0MPC16string6String6escape(_M0L8filenameS1165);
  #line 545 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1166 = _M0MPC16string6String6escape(_M0L8testnameS1161);
  #line 546 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1167 = _M0MPC16string6String6escape(_M0L7messageS1162);
  #line 547 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 549 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3466
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1164);
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3494
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3466);
  moonbit_decref(_M0L6_2atmpS3466);
  _M0L6_2atmpS3465 = _M0L6_2atmpS3494;
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3493
  = moonbit_add_string(_M0L6_2atmpS3465, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3465);
  _M0L6_2atmpS3463 = _M0L6_2atmpS3493;
  #line 549 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3464
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1168);
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3492 = moonbit_add_string(_M0L6_2atmpS3463, _M0L6_2atmpS3464);
  moonbit_decref(_M0L6_2atmpS3463);
  moonbit_decref(_M0L6_2atmpS3464);
  _M0L6_2atmpS3462 = _M0L6_2atmpS3492;
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3491
  = moonbit_add_string(_M0L6_2atmpS3462, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3462);
  _M0L6_2atmpS3460 = _M0L6_2atmpS3491;
  #line 549 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3461
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1166);
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3490 = moonbit_add_string(_M0L6_2atmpS3460, _M0L6_2atmpS3461);
  moonbit_decref(_M0L6_2atmpS3460);
  moonbit_decref(_M0L6_2atmpS3461);
  _M0L6_2atmpS3459 = _M0L6_2atmpS3490;
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3489
  = moonbit_add_string(_M0L6_2atmpS3459, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3459);
  _M0L6_2atmpS3457 = _M0L6_2atmpS3489;
  #line 549 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3458
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1167);
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3488 = moonbit_add_string(_M0L6_2atmpS3457, _M0L6_2atmpS3458);
  moonbit_decref(_M0L6_2atmpS3457);
  moonbit_decref(_M0L6_2atmpS3458);
  _M0L6_2atmpS3456 = _M0L6_2atmpS3488;
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3487
  = moonbit_add_string(_M0L6_2atmpS3456, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3456);
  _M0L6_2atmpS3455 = _M0L6_2atmpS3487;
  #line 548 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3455);
  #line 551 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1159,
  moonbit_string_t _M0L8filenameS1156,
  int32_t _M0L5indexS1150,
  struct _M0TWssbEu* _M0L14handle__resultS1146,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1148
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1126;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1155;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1128;
  moonbit_string_t* _M0L5attrsS1129;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1149;
  moonbit_string_t _M0L4nameS1132;
  moonbit_string_t _M0L4nameS1130;
  int32_t _M0L6_2atmpS3452;
  struct _M0TWEOs* _M0L5_2aitS1134;
  struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__* _closure_3834;
  struct _M0TWEOc* _M0L6_2atmpS3443;
  struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__* _closure_3835;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3444;
  struct moonbit_result_0 _result_3836;
  #line 412 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1159);
  moonbit_incref(_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 419 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1155
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1156);
  if (_M0L7_2abindS1155 == 0) {
    struct moonbit_result_0 _result_3827;
    if (_M0L7_2abindS1155) {
      moonbit_decref(_M0L7_2abindS1155);
    }
    moonbit_decref(_M0L17error__to__stringS1148);
    moonbit_decref(_M0L14handle__resultS1146);
    _result_3827.tag = 1;
    _result_3827.data.ok = 0;
    return _result_3827;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1157 =
      _M0L7_2abindS1155;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1158 =
      _M0L7_2aSomeS1157;
    _M0L10index__mapS1126 = _M0L13_2aindex__mapS1158;
    goto join_1125;
  }
  join_1125:;
  #line 421 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1149
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1126, _M0L5indexS1150);
  if (_M0L7_2abindS1149 == 0) {
    struct moonbit_result_0 _result_3829;
    if (_M0L7_2abindS1149) {
      moonbit_decref(_M0L7_2abindS1149);
    }
    moonbit_decref(_M0L17error__to__stringS1148);
    moonbit_decref(_M0L14handle__resultS1146);
    _result_3829.tag = 1;
    _result_3829.data.ok = 0;
    return _result_3829;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1151 =
      _M0L7_2abindS1149;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1152 = _M0L7_2aSomeS1151;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3500 = _M0L4_2axS1152->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1153 = _M0L8_2afieldS3500;
    moonbit_string_t* _M0L8_2afieldS3499 = _M0L4_2axS1152->$1;
    int32_t _M0L6_2acntS3740 = Moonbit_object_header(_M0L4_2axS1152)->rc;
    moonbit_string_t* _M0L8_2aattrsS1154;
    if (_M0L6_2acntS3740 > 1) {
      int32_t _M0L11_2anew__cntS3741 = _M0L6_2acntS3740 - 1;
      Moonbit_object_header(_M0L4_2axS1152)->rc = _M0L11_2anew__cntS3741;
      moonbit_incref(_M0L8_2afieldS3499);
      moonbit_incref(_M0L4_2afS1153);
    } else if (_M0L6_2acntS3740 == 1) {
      #line 419 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1152);
    }
    _M0L8_2aattrsS1154 = _M0L8_2afieldS3499;
    _M0L1fS1128 = _M0L4_2afS1153;
    _M0L5attrsS1129 = _M0L8_2aattrsS1154;
    goto join_1127;
  }
  join_1127:;
  _M0L6_2atmpS3452 = Moonbit_array_length(_M0L5attrsS1129);
  if (_M0L6_2atmpS3452 >= 1) {
    moonbit_string_t _M0L6_2atmpS3498 = (moonbit_string_t)_M0L5attrsS1129[0];
    moonbit_string_t _M0L7_2anameS1133 = _M0L6_2atmpS3498;
    moonbit_incref(_M0L7_2anameS1133);
    _M0L4nameS1132 = _M0L7_2anameS1133;
    goto join_1131;
  } else {
    _M0L4nameS1130 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3830;
  join_1131:;
  _M0L4nameS1130 = _M0L4nameS1132;
  joinlet_3830:;
  #line 422 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1134 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1129);
  while (1) {
    moonbit_string_t _M0L4attrS1136;
    moonbit_string_t _M0L7_2abindS1143;
    int32_t _M0L6_2atmpS3436;
    int64_t _M0L6_2atmpS3435;
    moonbit_incref(_M0L5_2aitS1134);
    #line 424 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1143 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1134);
    if (_M0L7_2abindS1143 == 0) {
      if (_M0L7_2abindS1143) {
        moonbit_decref(_M0L7_2abindS1143);
      }
      moonbit_decref(_M0L5_2aitS1134);
    } else {
      moonbit_string_t _M0L7_2aSomeS1144 = _M0L7_2abindS1143;
      moonbit_string_t _M0L7_2aattrS1145 = _M0L7_2aSomeS1144;
      _M0L4attrS1136 = _M0L7_2aattrS1145;
      goto join_1135;
    }
    goto joinlet_3832;
    join_1135:;
    _M0L6_2atmpS3436 = Moonbit_array_length(_M0L4attrS1136);
    _M0L6_2atmpS3435 = (int64_t)_M0L6_2atmpS3436;
    moonbit_incref(_M0L4attrS1136);
    #line 425 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1136, 5, 0, _M0L6_2atmpS3435)
    ) {
      int32_t _M0L6_2atmpS3442 = _M0L4attrS1136[0];
      int32_t _M0L4_2axS1137 = _M0L6_2atmpS3442;
      if (_M0L4_2axS1137 == 112) {
        int32_t _M0L6_2atmpS3441 = _M0L4attrS1136[1];
        int32_t _M0L4_2axS1138 = _M0L6_2atmpS3441;
        if (_M0L4_2axS1138 == 97) {
          int32_t _M0L6_2atmpS3440 = _M0L4attrS1136[2];
          int32_t _M0L4_2axS1139 = _M0L6_2atmpS3440;
          if (_M0L4_2axS1139 == 110) {
            int32_t _M0L6_2atmpS3439 = _M0L4attrS1136[3];
            int32_t _M0L4_2axS1140 = _M0L6_2atmpS3439;
            if (_M0L4_2axS1140 == 105) {
              int32_t _M0L6_2atmpS3497 = _M0L4attrS1136[4];
              int32_t _M0L6_2atmpS3438;
              int32_t _M0L4_2axS1141;
              moonbit_decref(_M0L4attrS1136);
              _M0L6_2atmpS3438 = _M0L6_2atmpS3497;
              _M0L4_2axS1141 = _M0L6_2atmpS3438;
              if (_M0L4_2axS1141 == 99) {
                void* _M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3437;
                struct moonbit_result_0 _result_3833;
                moonbit_decref(_M0L17error__to__stringS1148);
                moonbit_decref(_M0L14handle__resultS1146);
                moonbit_decref(_M0L5_2aitS1134);
                moonbit_decref(_M0L1fS1128);
                _M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3437
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3437)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3437)->$0
                = _M0L4nameS1130;
                _result_3833.tag = 0;
                _result_3833.data.err
                = _M0L135clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3437;
                return _result_3833;
              }
            } else {
              moonbit_decref(_M0L4attrS1136);
            }
          } else {
            moonbit_decref(_M0L4attrS1136);
          }
        } else {
          moonbit_decref(_M0L4attrS1136);
        }
      } else {
        moonbit_decref(_M0L4attrS1136);
      }
    } else {
      moonbit_decref(_M0L4attrS1136);
    }
    continue;
    joinlet_3832:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1146);
  moonbit_incref(_M0L4nameS1130);
  _closure_3834
  = (struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__*)moonbit_malloc(sizeof(struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__));
  Moonbit_object_header(_closure_3834)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__, $0) >> 2, 2, 0);
  _closure_3834->code
  = &_M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testC3449l432;
  _closure_3834->$0 = _M0L14handle__resultS1146;
  _closure_3834->$1 = _M0L4nameS1130;
  _M0L6_2atmpS3443 = (struct _M0TWEOc*)_closure_3834;
  _closure_3835
  = (struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__*)moonbit_malloc(sizeof(struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__));
  Moonbit_object_header(_closure_3835)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__, $0) >> 2, 3, 0);
  _closure_3835->code
  = &_M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testC3445l433;
  _closure_3835->$0 = _M0L17error__to__stringS1148;
  _closure_3835->$1 = _M0L14handle__resultS1146;
  _closure_3835->$2 = _M0L4nameS1130;
  _M0L6_2atmpS3444 = (struct _M0TWRPC15error5ErrorEu*)_closure_3835;
  #line 430 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1128, _M0L6_2atmpS3443, _M0L6_2atmpS3444);
  _result_3836.tag = 1;
  _result_3836.data.ok = 1;
  return _result_3836;
}

int32_t _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testC3449l432(
  struct _M0TWEOc* _M0L6_2aenvS3450
) {
  struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__* _M0L14_2acasted__envS3451;
  moonbit_string_t _M0L8_2afieldS3502;
  moonbit_string_t _M0L4nameS1130;
  struct _M0TWssbEu* _M0L8_2afieldS3501;
  int32_t _M0L6_2acntS3742;
  struct _M0TWssbEu* _M0L14handle__resultS1146;
  #line 432 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3451
  = (struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3449__l432__*)_M0L6_2aenvS3450;
  _M0L8_2afieldS3502 = _M0L14_2acasted__envS3451->$1;
  _M0L4nameS1130 = _M0L8_2afieldS3502;
  _M0L8_2afieldS3501 = _M0L14_2acasted__envS3451->$0;
  _M0L6_2acntS3742 = Moonbit_object_header(_M0L14_2acasted__envS3451)->rc;
  if (_M0L6_2acntS3742 > 1) {
    int32_t _M0L11_2anew__cntS3743 = _M0L6_2acntS3742 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3451)->rc
    = _M0L11_2anew__cntS3743;
    moonbit_incref(_M0L4nameS1130);
    moonbit_incref(_M0L8_2afieldS3501);
  } else if (_M0L6_2acntS3742 == 1) {
    #line 432 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3451);
  }
  _M0L14handle__resultS1146 = _M0L8_2afieldS3501;
  #line 432 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1146->code(_M0L14handle__resultS1146, _M0L4nameS1130, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP58clawteam8clawteam8internal5httpx22status__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testC3445l433(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3446,
  void* _M0L3errS1147
) {
  struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__* _M0L14_2acasted__envS3447;
  moonbit_string_t _M0L8_2afieldS3505;
  moonbit_string_t _M0L4nameS1130;
  struct _M0TWssbEu* _M0L8_2afieldS3504;
  struct _M0TWssbEu* _M0L14handle__resultS1146;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3503;
  int32_t _M0L6_2acntS3744;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1148;
  moonbit_string_t _M0L6_2atmpS3448;
  #line 433 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3447
  = (struct _M0R243_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fhttpx_2fstatus__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3445__l433__*)_M0L6_2aenvS3446;
  _M0L8_2afieldS3505 = _M0L14_2acasted__envS3447->$2;
  _M0L4nameS1130 = _M0L8_2afieldS3505;
  _M0L8_2afieldS3504 = _M0L14_2acasted__envS3447->$1;
  _M0L14handle__resultS1146 = _M0L8_2afieldS3504;
  _M0L8_2afieldS3503 = _M0L14_2acasted__envS3447->$0;
  _M0L6_2acntS3744 = Moonbit_object_header(_M0L14_2acasted__envS3447)->rc;
  if (_M0L6_2acntS3744 > 1) {
    int32_t _M0L11_2anew__cntS3745 = _M0L6_2acntS3744 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3447)->rc
    = _M0L11_2anew__cntS3745;
    moonbit_incref(_M0L4nameS1130);
    moonbit_incref(_M0L14handle__resultS1146);
    moonbit_incref(_M0L8_2afieldS3503);
  } else if (_M0L6_2acntS3744 == 1) {
    #line 433 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3447);
  }
  _M0L17error__to__stringS1148 = _M0L8_2afieldS3503;
  #line 433 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3448
  = _M0L17error__to__stringS1148->code(_M0L17error__to__stringS1148, _M0L3errS1147);
  #line 433 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1146->code(_M0L14handle__resultS1146, _M0L4nameS1130, _M0L6_2atmpS3448, 0);
  return 0;
}

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1118,
  struct _M0TWEOc* _M0L6on__okS1119,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1116
) {
  void* _M0L11_2atry__errS1114;
  struct moonbit_result_0 _tmp_3838;
  void* _M0L3errS1115;
  #line 375 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3838 = _M0L1fS1118->code(_M0L1fS1118);
  if (_tmp_3838.tag) {
    int32_t const _M0L5_2aokS3433 = _tmp_3838.data.ok;
    moonbit_decref(_M0L7on__errS1116);
  } else {
    void* const _M0L6_2aerrS3434 = _tmp_3838.data.err;
    moonbit_decref(_M0L6on__okS1119);
    _M0L11_2atry__errS1114 = _M0L6_2aerrS3434;
    goto join_1113;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1119->code(_M0L6on__okS1119);
  goto joinlet_3837;
  join_1113:;
  _M0L3errS1115 = _M0L11_2atry__errS1114;
  #line 383 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1116->code(_M0L7on__errS1116, _M0L3errS1115);
  joinlet_3837:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1073;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1086;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1091;
  struct _M0TUsiE** _M0L6_2atmpS3432;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1098;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1099;
  moonbit_string_t _M0L6_2atmpS3431;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1100;
  int32_t _M0L7_2abindS1101;
  int32_t _M0L2__S1102;
  #line 193 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1073 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1086
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1091 = 0;
  _M0L6_2atmpS3432 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1098
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1098)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1098->$0 = _M0L6_2atmpS3432;
  _M0L16file__and__indexS1098->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1099
  = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1086(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1086);
  #line 284 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3431 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1099, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1100
  = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1091(_M0L51moonbit__test__driver__internal__split__mbt__stringS1091, _M0L6_2atmpS3431, 47);
  _M0L7_2abindS1101 = _M0L10test__argsS1100->$1;
  _M0L2__S1102 = 0;
  while (1) {
    if (_M0L2__S1102 < _M0L7_2abindS1101) {
      moonbit_string_t* _M0L8_2afieldS3507 = _M0L10test__argsS1100->$0;
      moonbit_string_t* _M0L3bufS3430 = _M0L8_2afieldS3507;
      moonbit_string_t _M0L6_2atmpS3506 =
        (moonbit_string_t)_M0L3bufS3430[_M0L2__S1102];
      moonbit_string_t _M0L3argS1103 = _M0L6_2atmpS3506;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1104;
      moonbit_string_t _M0L4fileS1105;
      moonbit_string_t _M0L5rangeS1106;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1107;
      moonbit_string_t _M0L6_2atmpS3428;
      int32_t _M0L5startS1108;
      moonbit_string_t _M0L6_2atmpS3427;
      int32_t _M0L3endS1109;
      int32_t _M0L1iS1110;
      int32_t _M0L6_2atmpS3429;
      moonbit_incref(_M0L3argS1103);
      #line 288 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1104
      = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1091(_M0L51moonbit__test__driver__internal__split__mbt__stringS1091, _M0L3argS1103, 58);
      moonbit_incref(_M0L16file__and__rangeS1104);
      #line 289 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1105
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1104, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1106
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1104, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1107
      = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1091(_M0L51moonbit__test__driver__internal__split__mbt__stringS1091, _M0L5rangeS1106, 45);
      moonbit_incref(_M0L15start__and__endS1107);
      #line 294 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3428
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1107, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1108
      = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1073(_M0L45moonbit__test__driver__internal__parse__int__S1073, _M0L6_2atmpS3428);
      #line 295 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3427
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1107, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1109
      = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1073(_M0L45moonbit__test__driver__internal__parse__int__S1073, _M0L6_2atmpS3427);
      _M0L1iS1110 = _M0L5startS1108;
      while (1) {
        if (_M0L1iS1110 < _M0L3endS1109) {
          struct _M0TUsiE* _M0L8_2atupleS3425;
          int32_t _M0L6_2atmpS3426;
          moonbit_incref(_M0L4fileS1105);
          _M0L8_2atupleS3425
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3425)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3425->$0 = _M0L4fileS1105;
          _M0L8_2atupleS3425->$1 = _M0L1iS1110;
          moonbit_incref(_M0L16file__and__indexS1098);
          #line 297 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1098, _M0L8_2atupleS3425);
          _M0L6_2atmpS3426 = _M0L1iS1110 + 1;
          _M0L1iS1110 = _M0L6_2atmpS3426;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1105);
        }
        break;
      }
      _M0L6_2atmpS3429 = _M0L2__S1102 + 1;
      _M0L2__S1102 = _M0L6_2atmpS3429;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1100);
    }
    break;
  }
  return _M0L16file__and__indexS1098;
}

struct _M0TPB5ArrayGsE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1091(
  int32_t _M0L6_2aenvS3406,
  moonbit_string_t _M0L1sS1092,
  int32_t _M0L3sepS1093
) {
  moonbit_string_t* _M0L6_2atmpS3424;
  struct _M0TPB5ArrayGsE* _M0L3resS1094;
  struct _M0TPC13ref3RefGiE* _M0L1iS1095;
  struct _M0TPC13ref3RefGiE* _M0L5startS1096;
  int32_t _M0L3valS3419;
  int32_t _M0L6_2atmpS3420;
  #line 261 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3424 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1094
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1094)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1094->$0 = _M0L6_2atmpS3424;
  _M0L3resS1094->$1 = 0;
  _M0L1iS1095
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1095)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1095->$0 = 0;
  _M0L5startS1096
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1096)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1096->$0 = 0;
  while (1) {
    int32_t _M0L3valS3407 = _M0L1iS1095->$0;
    int32_t _M0L6_2atmpS3408 = Moonbit_array_length(_M0L1sS1092);
    if (_M0L3valS3407 < _M0L6_2atmpS3408) {
      int32_t _M0L3valS3411 = _M0L1iS1095->$0;
      int32_t _M0L6_2atmpS3410;
      int32_t _M0L6_2atmpS3409;
      int32_t _M0L3valS3418;
      int32_t _M0L6_2atmpS3417;
      if (
        _M0L3valS3411 < 0
        || _M0L3valS3411 >= Moonbit_array_length(_M0L1sS1092)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3410 = _M0L1sS1092[_M0L3valS3411];
      _M0L6_2atmpS3409 = _M0L6_2atmpS3410;
      if (_M0L6_2atmpS3409 == _M0L3sepS1093) {
        int32_t _M0L3valS3413 = _M0L5startS1096->$0;
        int32_t _M0L3valS3414 = _M0L1iS1095->$0;
        moonbit_string_t _M0L6_2atmpS3412;
        int32_t _M0L3valS3416;
        int32_t _M0L6_2atmpS3415;
        moonbit_incref(_M0L1sS1092);
        #line 270 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3412
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1092, _M0L3valS3413, _M0L3valS3414);
        moonbit_incref(_M0L3resS1094);
        #line 270 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1094, _M0L6_2atmpS3412);
        _M0L3valS3416 = _M0L1iS1095->$0;
        _M0L6_2atmpS3415 = _M0L3valS3416 + 1;
        _M0L5startS1096->$0 = _M0L6_2atmpS3415;
      }
      _M0L3valS3418 = _M0L1iS1095->$0;
      _M0L6_2atmpS3417 = _M0L3valS3418 + 1;
      _M0L1iS1095->$0 = _M0L6_2atmpS3417;
      continue;
    } else {
      moonbit_decref(_M0L1iS1095);
    }
    break;
  }
  _M0L3valS3419 = _M0L5startS1096->$0;
  _M0L6_2atmpS3420 = Moonbit_array_length(_M0L1sS1092);
  if (_M0L3valS3419 < _M0L6_2atmpS3420) {
    int32_t _M0L8_2afieldS3508 = _M0L5startS1096->$0;
    int32_t _M0L3valS3422;
    int32_t _M0L6_2atmpS3423;
    moonbit_string_t _M0L6_2atmpS3421;
    moonbit_decref(_M0L5startS1096);
    _M0L3valS3422 = _M0L8_2afieldS3508;
    _M0L6_2atmpS3423 = Moonbit_array_length(_M0L1sS1092);
    #line 276 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3421
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1092, _M0L3valS3422, _M0L6_2atmpS3423);
    moonbit_incref(_M0L3resS1094);
    #line 276 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1094, _M0L6_2atmpS3421);
  } else {
    moonbit_decref(_M0L5startS1096);
    moonbit_decref(_M0L1sS1092);
  }
  return _M0L3resS1094;
}

struct _M0TPB5ArrayGsE* _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1086(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079
) {
  moonbit_bytes_t* _M0L3tmpS1087;
  int32_t _M0L6_2atmpS3405;
  struct _M0TPB5ArrayGsE* _M0L3resS1088;
  int32_t _M0L1iS1089;
  #line 250 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1087
  = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3405 = Moonbit_array_length(_M0L3tmpS1087);
  #line 254 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1088 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3405);
  _M0L1iS1089 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3401 = Moonbit_array_length(_M0L3tmpS1087);
    if (_M0L1iS1089 < _M0L6_2atmpS3401) {
      moonbit_bytes_t _M0L6_2atmpS3509;
      moonbit_bytes_t _M0L6_2atmpS3403;
      moonbit_string_t _M0L6_2atmpS3402;
      int32_t _M0L6_2atmpS3404;
      if (
        _M0L1iS1089 < 0 || _M0L1iS1089 >= Moonbit_array_length(_M0L3tmpS1087)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3509 = (moonbit_bytes_t)_M0L3tmpS1087[_M0L1iS1089];
      _M0L6_2atmpS3403 = _M0L6_2atmpS3509;
      moonbit_incref(_M0L6_2atmpS3403);
      #line 256 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3402
      = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079, _M0L6_2atmpS3403);
      moonbit_incref(_M0L3resS1088);
      #line 256 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1088, _M0L6_2atmpS3402);
      _M0L6_2atmpS3404 = _M0L1iS1089 + 1;
      _M0L1iS1089 = _M0L6_2atmpS3404;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1087);
    }
    break;
  }
  return _M0L3resS1088;
}

moonbit_string_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1079(
  int32_t _M0L6_2aenvS3315,
  moonbit_bytes_t _M0L5bytesS1080
) {
  struct _M0TPB13StringBuilder* _M0L3resS1081;
  int32_t _M0L3lenS1082;
  struct _M0TPC13ref3RefGiE* _M0L1iS1083;
  #line 206 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1081 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1082 = Moonbit_array_length(_M0L5bytesS1080);
  _M0L1iS1083
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1083)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1083->$0 = 0;
  while (1) {
    int32_t _M0L3valS3316 = _M0L1iS1083->$0;
    if (_M0L3valS3316 < _M0L3lenS1082) {
      int32_t _M0L3valS3400 = _M0L1iS1083->$0;
      int32_t _M0L6_2atmpS3399;
      int32_t _M0L6_2atmpS3398;
      struct _M0TPC13ref3RefGiE* _M0L1cS1084;
      int32_t _M0L3valS3317;
      if (
        _M0L3valS3400 < 0
        || _M0L3valS3400 >= Moonbit_array_length(_M0L5bytesS1080)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3399 = _M0L5bytesS1080[_M0L3valS3400];
      _M0L6_2atmpS3398 = (int32_t)_M0L6_2atmpS3399;
      _M0L1cS1084
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1084)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1084->$0 = _M0L6_2atmpS3398;
      _M0L3valS3317 = _M0L1cS1084->$0;
      if (_M0L3valS3317 < 128) {
        int32_t _M0L8_2afieldS3510 = _M0L1cS1084->$0;
        int32_t _M0L3valS3319;
        int32_t _M0L6_2atmpS3318;
        int32_t _M0L3valS3321;
        int32_t _M0L6_2atmpS3320;
        moonbit_decref(_M0L1cS1084);
        _M0L3valS3319 = _M0L8_2afieldS3510;
        _M0L6_2atmpS3318 = _M0L3valS3319;
        moonbit_incref(_M0L3resS1081);
        #line 215 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1081, _M0L6_2atmpS3318);
        _M0L3valS3321 = _M0L1iS1083->$0;
        _M0L6_2atmpS3320 = _M0L3valS3321 + 1;
        _M0L1iS1083->$0 = _M0L6_2atmpS3320;
      } else {
        int32_t _M0L3valS3322 = _M0L1cS1084->$0;
        if (_M0L3valS3322 < 224) {
          int32_t _M0L3valS3324 = _M0L1iS1083->$0;
          int32_t _M0L6_2atmpS3323 = _M0L3valS3324 + 1;
          int32_t _M0L3valS3333;
          int32_t _M0L6_2atmpS3332;
          int32_t _M0L6_2atmpS3326;
          int32_t _M0L3valS3331;
          int32_t _M0L6_2atmpS3330;
          int32_t _M0L6_2atmpS3329;
          int32_t _M0L6_2atmpS3328;
          int32_t _M0L6_2atmpS3327;
          int32_t _M0L6_2atmpS3325;
          int32_t _M0L8_2afieldS3511;
          int32_t _M0L3valS3335;
          int32_t _M0L6_2atmpS3334;
          int32_t _M0L3valS3337;
          int32_t _M0L6_2atmpS3336;
          if (_M0L6_2atmpS3323 >= _M0L3lenS1082) {
            moonbit_decref(_M0L1cS1084);
            moonbit_decref(_M0L1iS1083);
            moonbit_decref(_M0L5bytesS1080);
            break;
          }
          _M0L3valS3333 = _M0L1cS1084->$0;
          _M0L6_2atmpS3332 = _M0L3valS3333 & 31;
          _M0L6_2atmpS3326 = _M0L6_2atmpS3332 << 6;
          _M0L3valS3331 = _M0L1iS1083->$0;
          _M0L6_2atmpS3330 = _M0L3valS3331 + 1;
          if (
            _M0L6_2atmpS3330 < 0
            || _M0L6_2atmpS3330 >= Moonbit_array_length(_M0L5bytesS1080)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3329 = _M0L5bytesS1080[_M0L6_2atmpS3330];
          _M0L6_2atmpS3328 = (int32_t)_M0L6_2atmpS3329;
          _M0L6_2atmpS3327 = _M0L6_2atmpS3328 & 63;
          _M0L6_2atmpS3325 = _M0L6_2atmpS3326 | _M0L6_2atmpS3327;
          _M0L1cS1084->$0 = _M0L6_2atmpS3325;
          _M0L8_2afieldS3511 = _M0L1cS1084->$0;
          moonbit_decref(_M0L1cS1084);
          _M0L3valS3335 = _M0L8_2afieldS3511;
          _M0L6_2atmpS3334 = _M0L3valS3335;
          moonbit_incref(_M0L3resS1081);
          #line 222 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1081, _M0L6_2atmpS3334);
          _M0L3valS3337 = _M0L1iS1083->$0;
          _M0L6_2atmpS3336 = _M0L3valS3337 + 2;
          _M0L1iS1083->$0 = _M0L6_2atmpS3336;
        } else {
          int32_t _M0L3valS3338 = _M0L1cS1084->$0;
          if (_M0L3valS3338 < 240) {
            int32_t _M0L3valS3340 = _M0L1iS1083->$0;
            int32_t _M0L6_2atmpS3339 = _M0L3valS3340 + 2;
            int32_t _M0L3valS3356;
            int32_t _M0L6_2atmpS3355;
            int32_t _M0L6_2atmpS3348;
            int32_t _M0L3valS3354;
            int32_t _M0L6_2atmpS3353;
            int32_t _M0L6_2atmpS3352;
            int32_t _M0L6_2atmpS3351;
            int32_t _M0L6_2atmpS3350;
            int32_t _M0L6_2atmpS3349;
            int32_t _M0L6_2atmpS3342;
            int32_t _M0L3valS3347;
            int32_t _M0L6_2atmpS3346;
            int32_t _M0L6_2atmpS3345;
            int32_t _M0L6_2atmpS3344;
            int32_t _M0L6_2atmpS3343;
            int32_t _M0L6_2atmpS3341;
            int32_t _M0L8_2afieldS3512;
            int32_t _M0L3valS3358;
            int32_t _M0L6_2atmpS3357;
            int32_t _M0L3valS3360;
            int32_t _M0L6_2atmpS3359;
            if (_M0L6_2atmpS3339 >= _M0L3lenS1082) {
              moonbit_decref(_M0L1cS1084);
              moonbit_decref(_M0L1iS1083);
              moonbit_decref(_M0L5bytesS1080);
              break;
            }
            _M0L3valS3356 = _M0L1cS1084->$0;
            _M0L6_2atmpS3355 = _M0L3valS3356 & 15;
            _M0L6_2atmpS3348 = _M0L6_2atmpS3355 << 12;
            _M0L3valS3354 = _M0L1iS1083->$0;
            _M0L6_2atmpS3353 = _M0L3valS3354 + 1;
            if (
              _M0L6_2atmpS3353 < 0
              || _M0L6_2atmpS3353 >= Moonbit_array_length(_M0L5bytesS1080)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3352 = _M0L5bytesS1080[_M0L6_2atmpS3353];
            _M0L6_2atmpS3351 = (int32_t)_M0L6_2atmpS3352;
            _M0L6_2atmpS3350 = _M0L6_2atmpS3351 & 63;
            _M0L6_2atmpS3349 = _M0L6_2atmpS3350 << 6;
            _M0L6_2atmpS3342 = _M0L6_2atmpS3348 | _M0L6_2atmpS3349;
            _M0L3valS3347 = _M0L1iS1083->$0;
            _M0L6_2atmpS3346 = _M0L3valS3347 + 2;
            if (
              _M0L6_2atmpS3346 < 0
              || _M0L6_2atmpS3346 >= Moonbit_array_length(_M0L5bytesS1080)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3345 = _M0L5bytesS1080[_M0L6_2atmpS3346];
            _M0L6_2atmpS3344 = (int32_t)_M0L6_2atmpS3345;
            _M0L6_2atmpS3343 = _M0L6_2atmpS3344 & 63;
            _M0L6_2atmpS3341 = _M0L6_2atmpS3342 | _M0L6_2atmpS3343;
            _M0L1cS1084->$0 = _M0L6_2atmpS3341;
            _M0L8_2afieldS3512 = _M0L1cS1084->$0;
            moonbit_decref(_M0L1cS1084);
            _M0L3valS3358 = _M0L8_2afieldS3512;
            _M0L6_2atmpS3357 = _M0L3valS3358;
            moonbit_incref(_M0L3resS1081);
            #line 231 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1081, _M0L6_2atmpS3357);
            _M0L3valS3360 = _M0L1iS1083->$0;
            _M0L6_2atmpS3359 = _M0L3valS3360 + 3;
            _M0L1iS1083->$0 = _M0L6_2atmpS3359;
          } else {
            int32_t _M0L3valS3362 = _M0L1iS1083->$0;
            int32_t _M0L6_2atmpS3361 = _M0L3valS3362 + 3;
            int32_t _M0L3valS3385;
            int32_t _M0L6_2atmpS3384;
            int32_t _M0L6_2atmpS3377;
            int32_t _M0L3valS3383;
            int32_t _M0L6_2atmpS3382;
            int32_t _M0L6_2atmpS3381;
            int32_t _M0L6_2atmpS3380;
            int32_t _M0L6_2atmpS3379;
            int32_t _M0L6_2atmpS3378;
            int32_t _M0L6_2atmpS3370;
            int32_t _M0L3valS3376;
            int32_t _M0L6_2atmpS3375;
            int32_t _M0L6_2atmpS3374;
            int32_t _M0L6_2atmpS3373;
            int32_t _M0L6_2atmpS3372;
            int32_t _M0L6_2atmpS3371;
            int32_t _M0L6_2atmpS3364;
            int32_t _M0L3valS3369;
            int32_t _M0L6_2atmpS3368;
            int32_t _M0L6_2atmpS3367;
            int32_t _M0L6_2atmpS3366;
            int32_t _M0L6_2atmpS3365;
            int32_t _M0L6_2atmpS3363;
            int32_t _M0L3valS3387;
            int32_t _M0L6_2atmpS3386;
            int32_t _M0L3valS3391;
            int32_t _M0L6_2atmpS3390;
            int32_t _M0L6_2atmpS3389;
            int32_t _M0L6_2atmpS3388;
            int32_t _M0L8_2afieldS3513;
            int32_t _M0L3valS3395;
            int32_t _M0L6_2atmpS3394;
            int32_t _M0L6_2atmpS3393;
            int32_t _M0L6_2atmpS3392;
            int32_t _M0L3valS3397;
            int32_t _M0L6_2atmpS3396;
            if (_M0L6_2atmpS3361 >= _M0L3lenS1082) {
              moonbit_decref(_M0L1cS1084);
              moonbit_decref(_M0L1iS1083);
              moonbit_decref(_M0L5bytesS1080);
              break;
            }
            _M0L3valS3385 = _M0L1cS1084->$0;
            _M0L6_2atmpS3384 = _M0L3valS3385 & 7;
            _M0L6_2atmpS3377 = _M0L6_2atmpS3384 << 18;
            _M0L3valS3383 = _M0L1iS1083->$0;
            _M0L6_2atmpS3382 = _M0L3valS3383 + 1;
            if (
              _M0L6_2atmpS3382 < 0
              || _M0L6_2atmpS3382 >= Moonbit_array_length(_M0L5bytesS1080)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3381 = _M0L5bytesS1080[_M0L6_2atmpS3382];
            _M0L6_2atmpS3380 = (int32_t)_M0L6_2atmpS3381;
            _M0L6_2atmpS3379 = _M0L6_2atmpS3380 & 63;
            _M0L6_2atmpS3378 = _M0L6_2atmpS3379 << 12;
            _M0L6_2atmpS3370 = _M0L6_2atmpS3377 | _M0L6_2atmpS3378;
            _M0L3valS3376 = _M0L1iS1083->$0;
            _M0L6_2atmpS3375 = _M0L3valS3376 + 2;
            if (
              _M0L6_2atmpS3375 < 0
              || _M0L6_2atmpS3375 >= Moonbit_array_length(_M0L5bytesS1080)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3374 = _M0L5bytesS1080[_M0L6_2atmpS3375];
            _M0L6_2atmpS3373 = (int32_t)_M0L6_2atmpS3374;
            _M0L6_2atmpS3372 = _M0L6_2atmpS3373 & 63;
            _M0L6_2atmpS3371 = _M0L6_2atmpS3372 << 6;
            _M0L6_2atmpS3364 = _M0L6_2atmpS3370 | _M0L6_2atmpS3371;
            _M0L3valS3369 = _M0L1iS1083->$0;
            _M0L6_2atmpS3368 = _M0L3valS3369 + 3;
            if (
              _M0L6_2atmpS3368 < 0
              || _M0L6_2atmpS3368 >= Moonbit_array_length(_M0L5bytesS1080)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3367 = _M0L5bytesS1080[_M0L6_2atmpS3368];
            _M0L6_2atmpS3366 = (int32_t)_M0L6_2atmpS3367;
            _M0L6_2atmpS3365 = _M0L6_2atmpS3366 & 63;
            _M0L6_2atmpS3363 = _M0L6_2atmpS3364 | _M0L6_2atmpS3365;
            _M0L1cS1084->$0 = _M0L6_2atmpS3363;
            _M0L3valS3387 = _M0L1cS1084->$0;
            _M0L6_2atmpS3386 = _M0L3valS3387 - 65536;
            _M0L1cS1084->$0 = _M0L6_2atmpS3386;
            _M0L3valS3391 = _M0L1cS1084->$0;
            _M0L6_2atmpS3390 = _M0L3valS3391 >> 10;
            _M0L6_2atmpS3389 = _M0L6_2atmpS3390 + 55296;
            _M0L6_2atmpS3388 = _M0L6_2atmpS3389;
            moonbit_incref(_M0L3resS1081);
            #line 242 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1081, _M0L6_2atmpS3388);
            _M0L8_2afieldS3513 = _M0L1cS1084->$0;
            moonbit_decref(_M0L1cS1084);
            _M0L3valS3395 = _M0L8_2afieldS3513;
            _M0L6_2atmpS3394 = _M0L3valS3395 & 1023;
            _M0L6_2atmpS3393 = _M0L6_2atmpS3394 + 56320;
            _M0L6_2atmpS3392 = _M0L6_2atmpS3393;
            moonbit_incref(_M0L3resS1081);
            #line 243 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1081, _M0L6_2atmpS3392);
            _M0L3valS3397 = _M0L1iS1083->$0;
            _M0L6_2atmpS3396 = _M0L3valS3397 + 4;
            _M0L1iS1083->$0 = _M0L6_2atmpS3396;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1083);
      moonbit_decref(_M0L5bytesS1080);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1081);
}

int32_t _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1073(
  int32_t _M0L6_2aenvS3308,
  moonbit_string_t _M0L1sS1074
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1075;
  int32_t _M0L3lenS1076;
  int32_t _M0L1iS1077;
  int32_t _M0L8_2afieldS3514;
  #line 197 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1075
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1075)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1075->$0 = 0;
  _M0L3lenS1076 = Moonbit_array_length(_M0L1sS1074);
  _M0L1iS1077 = 0;
  while (1) {
    if (_M0L1iS1077 < _M0L3lenS1076) {
      int32_t _M0L3valS3313 = _M0L3resS1075->$0;
      int32_t _M0L6_2atmpS3310 = _M0L3valS3313 * 10;
      int32_t _M0L6_2atmpS3312;
      int32_t _M0L6_2atmpS3311;
      int32_t _M0L6_2atmpS3309;
      int32_t _M0L6_2atmpS3314;
      if (
        _M0L1iS1077 < 0 || _M0L1iS1077 >= Moonbit_array_length(_M0L1sS1074)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3312 = _M0L1sS1074[_M0L1iS1077];
      _M0L6_2atmpS3311 = _M0L6_2atmpS3312 - 48;
      _M0L6_2atmpS3309 = _M0L6_2atmpS3310 + _M0L6_2atmpS3311;
      _M0L3resS1075->$0 = _M0L6_2atmpS3309;
      _M0L6_2atmpS3314 = _M0L1iS1077 + 1;
      _M0L1iS1077 = _M0L6_2atmpS3314;
      continue;
    } else {
      moonbit_decref(_M0L1sS1074);
    }
    break;
  }
  _M0L8_2afieldS3514 = _M0L3resS1075->$0;
  moonbit_decref(_M0L3resS1075);
  return _M0L8_2afieldS3514;
}

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1053,
  moonbit_string_t _M0L12_2adiscard__S1054,
  int32_t _M0L12_2adiscard__S1055,
  struct _M0TWssbEu* _M0L12_2adiscard__S1056,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1057
) {
  struct moonbit_result_0 _result_3845;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1057);
  moonbit_decref(_M0L12_2adiscard__S1056);
  moonbit_decref(_M0L12_2adiscard__S1054);
  moonbit_decref(_M0L12_2adiscard__S1053);
  _result_3845.tag = 1;
  _result_3845.data.ok = 0;
  return _result_3845;
}

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1058,
  moonbit_string_t _M0L12_2adiscard__S1059,
  int32_t _M0L12_2adiscard__S1060,
  struct _M0TWssbEu* _M0L12_2adiscard__S1061,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1062
) {
  struct moonbit_result_0 _result_3846;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1062);
  moonbit_decref(_M0L12_2adiscard__S1061);
  moonbit_decref(_M0L12_2adiscard__S1059);
  moonbit_decref(_M0L12_2adiscard__S1058);
  _result_3846.tag = 1;
  _result_3846.data.ok = 0;
  return _result_3846;
}

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1063,
  moonbit_string_t _M0L12_2adiscard__S1064,
  int32_t _M0L12_2adiscard__S1065,
  struct _M0TWssbEu* _M0L12_2adiscard__S1066,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1067
) {
  struct moonbit_result_0 _result_3847;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1067);
  moonbit_decref(_M0L12_2adiscard__S1066);
  moonbit_decref(_M0L12_2adiscard__S1064);
  moonbit_decref(_M0L12_2adiscard__S1063);
  _result_3847.tag = 1;
  _result_3847.data.ok = 0;
  return _result_3847;
}

struct moonbit_result_0 _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test21MoonBit__Test__Driver9run__testGRP58clawteam8clawteam8internal5httpx22status__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1068,
  moonbit_string_t _M0L12_2adiscard__S1069,
  int32_t _M0L12_2adiscard__S1070,
  struct _M0TWssbEu* _M0L12_2adiscard__S1071,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1072
) {
  struct moonbit_result_0 _result_3848;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1072);
  moonbit_decref(_M0L12_2adiscard__S1071);
  moonbit_decref(_M0L12_2adiscard__S1069);
  moonbit_decref(_M0L12_2adiscard__S1068);
  _result_3848.tag = 1;
  _result_3848.data.ok = 0;
  return _result_3848;
}

int32_t _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP58clawteam8clawteam8internal5httpx22status__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1052
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1052);
  return 0;
}

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test41____test__636c6173735f746573742e6d6274__0(
  
) {
  int32_t _M0L7_2abindS1021;
  int32_t _M0L7_2abindS1022;
  int32_t _M0L1iS1023;
  int32_t _M0L7_2abindS1026;
  int32_t _M0L7_2abindS1027;
  int32_t _M0L1iS1028;
  int32_t _M0L7_2abindS1031;
  int32_t _M0L7_2abindS1032;
  int32_t _M0L1iS1033;
  int32_t _M0L7_2abindS1036;
  int32_t _M0L7_2abindS1037;
  int32_t _M0L1iS1038;
  int32_t _M0L7_2abindS1041;
  int32_t _M0L7_2abindS1042;
  int32_t _M0L1iS1043;
  int32_t* _M0L6_2atmpS3307;
  struct _M0TPB5ArrayGiE* _M0L7_2abindS1046;
  int32_t _M0L7_2abindS1047;
  int32_t _M0L6_2atmpS3300;
  int32_t _M0L2__S1048;
  struct moonbit_result_0 _result_3867;
  #line 2 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
  _M0L7_2abindS1021 = 100;
  _M0L7_2abindS1022 = 199;
  _M0L1iS1023 = _M0L7_2abindS1021;
  while (1) {
    if (_M0L1iS1023 <= _M0L7_2abindS1022) {
      int32_t _M0L7_2abindS1024;
      int32_t _M0L6_2atmpS3275;
      moonbit_string_t _M0L6_2atmpS3276;
      struct moonbit_result_0 _tmp_3850;
      int32_t _M0L6_2atmpS3279;
      #line 4 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _M0L7_2abindS1024
      = _M0FP58clawteam8clawteam8internal5httpx6status8classify(_M0L1iS1023);
      switch (_M0L7_2abindS1024) {
        case 0: {
          _M0L6_2atmpS3275 = 1;
          break;
        }
        default: {
          _M0L6_2atmpS3275 = 0;
          break;
        }
      }
      _M0L6_2atmpS3276 = 0;
      #line 4 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _tmp_3850
      = _M0FPB12assert__true(_M0L6_2atmpS3275, _M0L6_2atmpS3276, (moonbit_string_t)moonbit_string_literal_9.data);
      if (_tmp_3850.tag) {
        int32_t const _M0L5_2aokS3277 = _tmp_3850.data.ok;
      } else {
        void* const _M0L6_2aerrS3278 = _tmp_3850.data.err;
        struct moonbit_result_0 _result_3851;
        _result_3851.tag = 0;
        _result_3851.data.err = _M0L6_2aerrS3278;
        return _result_3851;
      }
      _M0L6_2atmpS3279 = _M0L1iS1023 + 1;
      _M0L1iS1023 = _M0L6_2atmpS3279;
      continue;
    }
    break;
  }
  _M0L7_2abindS1026 = 200;
  _M0L7_2abindS1027 = 299;
  _M0L1iS1028 = _M0L7_2abindS1026;
  while (1) {
    if (_M0L1iS1028 <= _M0L7_2abindS1027) {
      int32_t _M0L7_2abindS1029;
      int32_t _M0L6_2atmpS3280;
      moonbit_string_t _M0L6_2atmpS3281;
      struct moonbit_result_0 _tmp_3853;
      int32_t _M0L6_2atmpS3284;
      #line 7 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _M0L7_2abindS1029
      = _M0FP58clawteam8clawteam8internal5httpx6status8classify(_M0L1iS1028);
      switch (_M0L7_2abindS1029) {
        case 1: {
          _M0L6_2atmpS3280 = 1;
          break;
        }
        default: {
          _M0L6_2atmpS3280 = 0;
          break;
        }
      }
      _M0L6_2atmpS3281 = 0;
      #line 7 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _tmp_3853
      = _M0FPB12assert__true(_M0L6_2atmpS3280, _M0L6_2atmpS3281, (moonbit_string_t)moonbit_string_literal_10.data);
      if (_tmp_3853.tag) {
        int32_t const _M0L5_2aokS3282 = _tmp_3853.data.ok;
      } else {
        void* const _M0L6_2aerrS3283 = _tmp_3853.data.err;
        struct moonbit_result_0 _result_3854;
        _result_3854.tag = 0;
        _result_3854.data.err = _M0L6_2aerrS3283;
        return _result_3854;
      }
      _M0L6_2atmpS3284 = _M0L1iS1028 + 1;
      _M0L1iS1028 = _M0L6_2atmpS3284;
      continue;
    }
    break;
  }
  _M0L7_2abindS1031 = 300;
  _M0L7_2abindS1032 = 399;
  _M0L1iS1033 = _M0L7_2abindS1031;
  while (1) {
    if (_M0L1iS1033 <= _M0L7_2abindS1032) {
      int32_t _M0L7_2abindS1034;
      int32_t _M0L6_2atmpS3285;
      moonbit_string_t _M0L6_2atmpS3286;
      struct moonbit_result_0 _tmp_3856;
      int32_t _M0L6_2atmpS3289;
      #line 10 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _M0L7_2abindS1034
      = _M0FP58clawteam8clawteam8internal5httpx6status8classify(_M0L1iS1033);
      switch (_M0L7_2abindS1034) {
        case 2: {
          _M0L6_2atmpS3285 = 1;
          break;
        }
        default: {
          _M0L6_2atmpS3285 = 0;
          break;
        }
      }
      _M0L6_2atmpS3286 = 0;
      #line 10 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _tmp_3856
      = _M0FPB12assert__true(_M0L6_2atmpS3285, _M0L6_2atmpS3286, (moonbit_string_t)moonbit_string_literal_11.data);
      if (_tmp_3856.tag) {
        int32_t const _M0L5_2aokS3287 = _tmp_3856.data.ok;
      } else {
        void* const _M0L6_2aerrS3288 = _tmp_3856.data.err;
        struct moonbit_result_0 _result_3857;
        _result_3857.tag = 0;
        _result_3857.data.err = _M0L6_2aerrS3288;
        return _result_3857;
      }
      _M0L6_2atmpS3289 = _M0L1iS1033 + 1;
      _M0L1iS1033 = _M0L6_2atmpS3289;
      continue;
    }
    break;
  }
  _M0L7_2abindS1036 = 400;
  _M0L7_2abindS1037 = 499;
  _M0L1iS1038 = _M0L7_2abindS1036;
  while (1) {
    if (_M0L1iS1038 <= _M0L7_2abindS1037) {
      int32_t _M0L7_2abindS1039;
      int32_t _M0L6_2atmpS3290;
      moonbit_string_t _M0L6_2atmpS3291;
      struct moonbit_result_0 _tmp_3859;
      int32_t _M0L6_2atmpS3294;
      #line 13 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _M0L7_2abindS1039
      = _M0FP58clawteam8clawteam8internal5httpx6status8classify(_M0L1iS1038);
      switch (_M0L7_2abindS1039) {
        case 3: {
          _M0L6_2atmpS3290 = 1;
          break;
        }
        default: {
          _M0L6_2atmpS3290 = 0;
          break;
        }
      }
      _M0L6_2atmpS3291 = 0;
      #line 13 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _tmp_3859
      = _M0FPB12assert__true(_M0L6_2atmpS3290, _M0L6_2atmpS3291, (moonbit_string_t)moonbit_string_literal_12.data);
      if (_tmp_3859.tag) {
        int32_t const _M0L5_2aokS3292 = _tmp_3859.data.ok;
      } else {
        void* const _M0L6_2aerrS3293 = _tmp_3859.data.err;
        struct moonbit_result_0 _result_3860;
        _result_3860.tag = 0;
        _result_3860.data.err = _M0L6_2aerrS3293;
        return _result_3860;
      }
      _M0L6_2atmpS3294 = _M0L1iS1038 + 1;
      _M0L1iS1038 = _M0L6_2atmpS3294;
      continue;
    }
    break;
  }
  _M0L7_2abindS1041 = 500;
  _M0L7_2abindS1042 = 599;
  _M0L1iS1043 = _M0L7_2abindS1041;
  while (1) {
    if (_M0L1iS1043 <= _M0L7_2abindS1042) {
      int32_t _M0L7_2abindS1044;
      int32_t _M0L6_2atmpS3295;
      moonbit_string_t _M0L6_2atmpS3296;
      struct moonbit_result_0 _tmp_3862;
      int32_t _M0L6_2atmpS3299;
      #line 16 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _M0L7_2abindS1044
      = _M0FP58clawteam8clawteam8internal5httpx6status8classify(_M0L1iS1043);
      switch (_M0L7_2abindS1044) {
        case 4: {
          _M0L6_2atmpS3295 = 1;
          break;
        }
        default: {
          _M0L6_2atmpS3295 = 0;
          break;
        }
      }
      _M0L6_2atmpS3296 = 0;
      #line 16 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _tmp_3862
      = _M0FPB12assert__true(_M0L6_2atmpS3295, _M0L6_2atmpS3296, (moonbit_string_t)moonbit_string_literal_13.data);
      if (_tmp_3862.tag) {
        int32_t const _M0L5_2aokS3297 = _tmp_3862.data.ok;
      } else {
        void* const _M0L6_2aerrS3298 = _tmp_3862.data.err;
        struct moonbit_result_0 _result_3863;
        _result_3863.tag = 0;
        _result_3863.data.err = _M0L6_2aerrS3298;
        return _result_3863;
      }
      _M0L6_2atmpS3299 = _M0L1iS1043 + 1;
      _M0L1iS1043 = _M0L6_2atmpS3299;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3307 = (int32_t*)moonbit_make_int32_array_raw(5);
  _M0L6_2atmpS3307[0] = 99;
  _M0L6_2atmpS3307[1] = 600;
  _M0L6_2atmpS3307[2] = 700;
  _M0L6_2atmpS3307[3] = 800;
  _M0L6_2atmpS3307[4] = 9999;
  _M0L7_2abindS1046
  = (struct _M0TPB5ArrayGiE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGiE));
  Moonbit_object_header(_M0L7_2abindS1046)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGiE, $0) >> 2, 1, 0);
  _M0L7_2abindS1046->$0 = _M0L6_2atmpS3307;
  _M0L7_2abindS1046->$1 = 5;
  _M0L7_2abindS1047 = _M0L7_2abindS1046->$1;
  _M0L2__S1048 = 0;
  while (1) {
    if (_M0L2__S1048 < _M0L7_2abindS1047) {
      int32_t* _M0L8_2afieldS3516 = _M0L7_2abindS1046->$0;
      int32_t* _M0L3bufS3306 = _M0L8_2afieldS3516;
      int32_t _M0L6_2atmpS3515 = (int32_t)_M0L3bufS3306[_M0L2__S1048];
      int32_t _M0L1iS1049 = _M0L6_2atmpS3515;
      int32_t _M0L7_2abindS1050;
      int32_t _M0L6_2atmpS3301;
      moonbit_string_t _M0L6_2atmpS3302;
      struct moonbit_result_0 _tmp_3865;
      int32_t _M0L6_2atmpS3305;
      #line 19 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _M0L7_2abindS1050
      = _M0FP58clawteam8clawteam8internal5httpx6status8classify(_M0L1iS1049);
      switch (_M0L7_2abindS1050) {
        case 5: {
          _M0L6_2atmpS3301 = 1;
          break;
        }
        default: {
          _M0L6_2atmpS3301 = 0;
          break;
        }
      }
      _M0L6_2atmpS3302 = 0;
      #line 19 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class_test.mbt"
      _tmp_3865
      = _M0FPB12assert__true(_M0L6_2atmpS3301, _M0L6_2atmpS3302, (moonbit_string_t)moonbit_string_literal_14.data);
      if (_tmp_3865.tag) {
        int32_t const _M0L5_2aokS3303 = _tmp_3865.data.ok;
      } else {
        void* const _M0L6_2aerrS3304 = _tmp_3865.data.err;
        struct moonbit_result_0 _result_3866;
        moonbit_decref(_M0L7_2abindS1046);
        _result_3866.tag = 0;
        _result_3866.data.err = _M0L6_2aerrS3304;
        return _result_3866;
      }
      _M0L6_2atmpS3305 = _M0L2__S1048 + 1;
      _M0L2__S1048 = _M0L6_2atmpS3305;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1046);
      _M0L6_2atmpS3300 = 0;
    }
    break;
  }
  _result_3867.tag = 1;
  _result_3867.data.ok = _M0L6_2atmpS3300;
  return _result_3867;
}

struct moonbit_result_0 _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test43____test__7374617475735f746573742e6d6274__0(
  
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS2481;
  struct _M0TPB6ToJson _M0L6_2atmpS2471;
  moonbit_string_t _M0L6_2atmpS2480;
  void* _M0L6_2atmpS2479;
  void* _M0L6_2atmpS2472;
  moonbit_string_t _M0L6_2atmpS2475;
  moonbit_string_t _M0L6_2atmpS2476;
  moonbit_string_t _M0L6_2atmpS2477;
  moonbit_string_t _M0L6_2atmpS2478;
  moonbit_string_t* _M0L6_2atmpS2474;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2473;
  struct moonbit_result_0 _tmp_3868;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2494;
  struct _M0TPB6ToJson _M0L6_2atmpS2484;
  moonbit_string_t _M0L6_2atmpS2493;
  void* _M0L6_2atmpS2492;
  void* _M0L6_2atmpS2485;
  moonbit_string_t _M0L6_2atmpS2488;
  moonbit_string_t _M0L6_2atmpS2489;
  moonbit_string_t _M0L6_2atmpS2490;
  moonbit_string_t _M0L6_2atmpS2491;
  moonbit_string_t* _M0L6_2atmpS2487;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2486;
  struct moonbit_result_0 _tmp_3870;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2507;
  struct _M0TPB6ToJson _M0L6_2atmpS2497;
  moonbit_string_t _M0L6_2atmpS2506;
  void* _M0L6_2atmpS2505;
  void* _M0L6_2atmpS2498;
  moonbit_string_t _M0L6_2atmpS2501;
  moonbit_string_t _M0L6_2atmpS2502;
  moonbit_string_t _M0L6_2atmpS2503;
  moonbit_string_t _M0L6_2atmpS2504;
  moonbit_string_t* _M0L6_2atmpS2500;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2499;
  struct moonbit_result_0 _tmp_3872;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2520;
  struct _M0TPB6ToJson _M0L6_2atmpS2510;
  moonbit_string_t _M0L6_2atmpS2519;
  void* _M0L6_2atmpS2518;
  void* _M0L6_2atmpS2511;
  moonbit_string_t _M0L6_2atmpS2514;
  moonbit_string_t _M0L6_2atmpS2515;
  moonbit_string_t _M0L6_2atmpS2516;
  moonbit_string_t _M0L6_2atmpS2517;
  moonbit_string_t* _M0L6_2atmpS2513;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2512;
  struct moonbit_result_0 _tmp_3874;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2533;
  struct _M0TPB6ToJson _M0L6_2atmpS2523;
  moonbit_string_t _M0L6_2atmpS2532;
  void* _M0L6_2atmpS2531;
  void* _M0L6_2atmpS2524;
  moonbit_string_t _M0L6_2atmpS2527;
  moonbit_string_t _M0L6_2atmpS2528;
  moonbit_string_t _M0L6_2atmpS2529;
  moonbit_string_t _M0L6_2atmpS2530;
  moonbit_string_t* _M0L6_2atmpS2526;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2525;
  struct moonbit_result_0 _tmp_3876;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2546;
  struct _M0TPB6ToJson _M0L6_2atmpS2536;
  moonbit_string_t _M0L6_2atmpS2545;
  void* _M0L6_2atmpS2544;
  void* _M0L6_2atmpS2537;
  moonbit_string_t _M0L6_2atmpS2540;
  moonbit_string_t _M0L6_2atmpS2541;
  moonbit_string_t _M0L6_2atmpS2542;
  moonbit_string_t _M0L6_2atmpS2543;
  moonbit_string_t* _M0L6_2atmpS2539;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2538;
  struct moonbit_result_0 _tmp_3878;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2559;
  struct _M0TPB6ToJson _M0L6_2atmpS2549;
  moonbit_string_t _M0L6_2atmpS2558;
  void* _M0L6_2atmpS2557;
  void* _M0L6_2atmpS2550;
  moonbit_string_t _M0L6_2atmpS2553;
  moonbit_string_t _M0L6_2atmpS2554;
  moonbit_string_t _M0L6_2atmpS2555;
  moonbit_string_t _M0L6_2atmpS2556;
  moonbit_string_t* _M0L6_2atmpS2552;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2551;
  struct moonbit_result_0 _tmp_3880;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2572;
  struct _M0TPB6ToJson _M0L6_2atmpS2562;
  moonbit_string_t _M0L6_2atmpS2571;
  void* _M0L6_2atmpS2570;
  void* _M0L6_2atmpS2563;
  moonbit_string_t _M0L6_2atmpS2566;
  moonbit_string_t _M0L6_2atmpS2567;
  moonbit_string_t _M0L6_2atmpS2568;
  moonbit_string_t _M0L6_2atmpS2569;
  moonbit_string_t* _M0L6_2atmpS2565;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2564;
  struct moonbit_result_0 _tmp_3882;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2585;
  struct _M0TPB6ToJson _M0L6_2atmpS2575;
  moonbit_string_t _M0L6_2atmpS2584;
  void* _M0L6_2atmpS2583;
  void* _M0L6_2atmpS2576;
  moonbit_string_t _M0L6_2atmpS2579;
  moonbit_string_t _M0L6_2atmpS2580;
  moonbit_string_t _M0L6_2atmpS2581;
  moonbit_string_t _M0L6_2atmpS2582;
  moonbit_string_t* _M0L6_2atmpS2578;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2577;
  struct moonbit_result_0 _tmp_3884;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2598;
  struct _M0TPB6ToJson _M0L6_2atmpS2588;
  moonbit_string_t _M0L6_2atmpS2597;
  void* _M0L6_2atmpS2596;
  void* _M0L6_2atmpS2589;
  moonbit_string_t _M0L6_2atmpS2592;
  moonbit_string_t _M0L6_2atmpS2593;
  moonbit_string_t _M0L6_2atmpS2594;
  moonbit_string_t _M0L6_2atmpS2595;
  moonbit_string_t* _M0L6_2atmpS2591;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2590;
  struct moonbit_result_0 _tmp_3886;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2611;
  struct _M0TPB6ToJson _M0L6_2atmpS2601;
  moonbit_string_t _M0L6_2atmpS2610;
  void* _M0L6_2atmpS2609;
  void* _M0L6_2atmpS2602;
  moonbit_string_t _M0L6_2atmpS2605;
  moonbit_string_t _M0L6_2atmpS2606;
  moonbit_string_t _M0L6_2atmpS2607;
  moonbit_string_t _M0L6_2atmpS2608;
  moonbit_string_t* _M0L6_2atmpS2604;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2603;
  struct moonbit_result_0 _tmp_3888;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2624;
  struct _M0TPB6ToJson _M0L6_2atmpS2614;
  moonbit_string_t _M0L6_2atmpS2623;
  void* _M0L6_2atmpS2622;
  void* _M0L6_2atmpS2615;
  moonbit_string_t _M0L6_2atmpS2618;
  moonbit_string_t _M0L6_2atmpS2619;
  moonbit_string_t _M0L6_2atmpS2620;
  moonbit_string_t _M0L6_2atmpS2621;
  moonbit_string_t* _M0L6_2atmpS2617;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2616;
  struct moonbit_result_0 _tmp_3890;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2637;
  struct _M0TPB6ToJson _M0L6_2atmpS2627;
  moonbit_string_t _M0L6_2atmpS2636;
  void* _M0L6_2atmpS2635;
  void* _M0L6_2atmpS2628;
  moonbit_string_t _M0L6_2atmpS2631;
  moonbit_string_t _M0L6_2atmpS2632;
  moonbit_string_t _M0L6_2atmpS2633;
  moonbit_string_t _M0L6_2atmpS2634;
  moonbit_string_t* _M0L6_2atmpS2630;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2629;
  struct moonbit_result_0 _tmp_3892;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2650;
  struct _M0TPB6ToJson _M0L6_2atmpS2640;
  moonbit_string_t _M0L6_2atmpS2649;
  void* _M0L6_2atmpS2648;
  void* _M0L6_2atmpS2641;
  moonbit_string_t _M0L6_2atmpS2644;
  moonbit_string_t _M0L6_2atmpS2645;
  moonbit_string_t _M0L6_2atmpS2646;
  moonbit_string_t _M0L6_2atmpS2647;
  moonbit_string_t* _M0L6_2atmpS2643;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2642;
  struct moonbit_result_0 _tmp_3894;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2663;
  struct _M0TPB6ToJson _M0L6_2atmpS2653;
  moonbit_string_t _M0L6_2atmpS2662;
  void* _M0L6_2atmpS2661;
  void* _M0L6_2atmpS2654;
  moonbit_string_t _M0L6_2atmpS2657;
  moonbit_string_t _M0L6_2atmpS2658;
  moonbit_string_t _M0L6_2atmpS2659;
  moonbit_string_t _M0L6_2atmpS2660;
  moonbit_string_t* _M0L6_2atmpS2656;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2655;
  struct moonbit_result_0 _tmp_3896;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2676;
  struct _M0TPB6ToJson _M0L6_2atmpS2666;
  moonbit_string_t _M0L6_2atmpS2675;
  void* _M0L6_2atmpS2674;
  void* _M0L6_2atmpS2667;
  moonbit_string_t _M0L6_2atmpS2670;
  moonbit_string_t _M0L6_2atmpS2671;
  moonbit_string_t _M0L6_2atmpS2672;
  moonbit_string_t _M0L6_2atmpS2673;
  moonbit_string_t* _M0L6_2atmpS2669;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2668;
  struct moonbit_result_0 _tmp_3898;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2689;
  struct _M0TPB6ToJson _M0L6_2atmpS2679;
  moonbit_string_t _M0L6_2atmpS2688;
  void* _M0L6_2atmpS2687;
  void* _M0L6_2atmpS2680;
  moonbit_string_t _M0L6_2atmpS2683;
  moonbit_string_t _M0L6_2atmpS2684;
  moonbit_string_t _M0L6_2atmpS2685;
  moonbit_string_t _M0L6_2atmpS2686;
  moonbit_string_t* _M0L6_2atmpS2682;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2681;
  struct moonbit_result_0 _tmp_3900;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2702;
  struct _M0TPB6ToJson _M0L6_2atmpS2692;
  moonbit_string_t _M0L6_2atmpS2701;
  void* _M0L6_2atmpS2700;
  void* _M0L6_2atmpS2693;
  moonbit_string_t _M0L6_2atmpS2696;
  moonbit_string_t _M0L6_2atmpS2697;
  moonbit_string_t _M0L6_2atmpS2698;
  moonbit_string_t _M0L6_2atmpS2699;
  moonbit_string_t* _M0L6_2atmpS2695;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2694;
  struct moonbit_result_0 _tmp_3902;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2715;
  struct _M0TPB6ToJson _M0L6_2atmpS2705;
  moonbit_string_t _M0L6_2atmpS2714;
  void* _M0L6_2atmpS2713;
  void* _M0L6_2atmpS2706;
  moonbit_string_t _M0L6_2atmpS2709;
  moonbit_string_t _M0L6_2atmpS2710;
  moonbit_string_t _M0L6_2atmpS2711;
  moonbit_string_t _M0L6_2atmpS2712;
  moonbit_string_t* _M0L6_2atmpS2708;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2707;
  struct moonbit_result_0 _tmp_3904;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2728;
  struct _M0TPB6ToJson _M0L6_2atmpS2718;
  moonbit_string_t _M0L6_2atmpS2727;
  void* _M0L6_2atmpS2726;
  void* _M0L6_2atmpS2719;
  moonbit_string_t _M0L6_2atmpS2722;
  moonbit_string_t _M0L6_2atmpS2723;
  moonbit_string_t _M0L6_2atmpS2724;
  moonbit_string_t _M0L6_2atmpS2725;
  moonbit_string_t* _M0L6_2atmpS2721;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2720;
  struct moonbit_result_0 _tmp_3906;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2741;
  struct _M0TPB6ToJson _M0L6_2atmpS2731;
  moonbit_string_t _M0L6_2atmpS2740;
  void* _M0L6_2atmpS2739;
  void* _M0L6_2atmpS2732;
  moonbit_string_t _M0L6_2atmpS2735;
  moonbit_string_t _M0L6_2atmpS2736;
  moonbit_string_t _M0L6_2atmpS2737;
  moonbit_string_t _M0L6_2atmpS2738;
  moonbit_string_t* _M0L6_2atmpS2734;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2733;
  struct moonbit_result_0 _tmp_3908;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2754;
  struct _M0TPB6ToJson _M0L6_2atmpS2744;
  moonbit_string_t _M0L6_2atmpS2753;
  void* _M0L6_2atmpS2752;
  void* _M0L6_2atmpS2745;
  moonbit_string_t _M0L6_2atmpS2748;
  moonbit_string_t _M0L6_2atmpS2749;
  moonbit_string_t _M0L6_2atmpS2750;
  moonbit_string_t _M0L6_2atmpS2751;
  moonbit_string_t* _M0L6_2atmpS2747;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2746;
  struct moonbit_result_0 _tmp_3910;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2767;
  struct _M0TPB6ToJson _M0L6_2atmpS2757;
  moonbit_string_t _M0L6_2atmpS2766;
  void* _M0L6_2atmpS2765;
  void* _M0L6_2atmpS2758;
  moonbit_string_t _M0L6_2atmpS2761;
  moonbit_string_t _M0L6_2atmpS2762;
  moonbit_string_t _M0L6_2atmpS2763;
  moonbit_string_t _M0L6_2atmpS2764;
  moonbit_string_t* _M0L6_2atmpS2760;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2759;
  struct moonbit_result_0 _tmp_3912;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2780;
  struct _M0TPB6ToJson _M0L6_2atmpS2770;
  moonbit_string_t _M0L6_2atmpS2779;
  void* _M0L6_2atmpS2778;
  void* _M0L6_2atmpS2771;
  moonbit_string_t _M0L6_2atmpS2774;
  moonbit_string_t _M0L6_2atmpS2775;
  moonbit_string_t _M0L6_2atmpS2776;
  moonbit_string_t _M0L6_2atmpS2777;
  moonbit_string_t* _M0L6_2atmpS2773;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2772;
  struct moonbit_result_0 _tmp_3914;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2793;
  struct _M0TPB6ToJson _M0L6_2atmpS2783;
  moonbit_string_t _M0L6_2atmpS2792;
  void* _M0L6_2atmpS2791;
  void* _M0L6_2atmpS2784;
  moonbit_string_t _M0L6_2atmpS2787;
  moonbit_string_t _M0L6_2atmpS2788;
  moonbit_string_t _M0L6_2atmpS2789;
  moonbit_string_t _M0L6_2atmpS2790;
  moonbit_string_t* _M0L6_2atmpS2786;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2785;
  struct moonbit_result_0 _tmp_3916;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2806;
  struct _M0TPB6ToJson _M0L6_2atmpS2796;
  moonbit_string_t _M0L6_2atmpS2805;
  void* _M0L6_2atmpS2804;
  void* _M0L6_2atmpS2797;
  moonbit_string_t _M0L6_2atmpS2800;
  moonbit_string_t _M0L6_2atmpS2801;
  moonbit_string_t _M0L6_2atmpS2802;
  moonbit_string_t _M0L6_2atmpS2803;
  moonbit_string_t* _M0L6_2atmpS2799;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2798;
  struct moonbit_result_0 _tmp_3918;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2819;
  struct _M0TPB6ToJson _M0L6_2atmpS2809;
  moonbit_string_t _M0L6_2atmpS2818;
  void* _M0L6_2atmpS2817;
  void* _M0L6_2atmpS2810;
  moonbit_string_t _M0L6_2atmpS2813;
  moonbit_string_t _M0L6_2atmpS2814;
  moonbit_string_t _M0L6_2atmpS2815;
  moonbit_string_t _M0L6_2atmpS2816;
  moonbit_string_t* _M0L6_2atmpS2812;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2811;
  struct moonbit_result_0 _tmp_3920;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2832;
  struct _M0TPB6ToJson _M0L6_2atmpS2822;
  moonbit_string_t _M0L6_2atmpS2831;
  void* _M0L6_2atmpS2830;
  void* _M0L6_2atmpS2823;
  moonbit_string_t _M0L6_2atmpS2826;
  moonbit_string_t _M0L6_2atmpS2827;
  moonbit_string_t _M0L6_2atmpS2828;
  moonbit_string_t _M0L6_2atmpS2829;
  moonbit_string_t* _M0L6_2atmpS2825;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2824;
  struct moonbit_result_0 _tmp_3922;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2845;
  struct _M0TPB6ToJson _M0L6_2atmpS2835;
  moonbit_string_t _M0L6_2atmpS2844;
  void* _M0L6_2atmpS2843;
  void* _M0L6_2atmpS2836;
  moonbit_string_t _M0L6_2atmpS2839;
  moonbit_string_t _M0L6_2atmpS2840;
  moonbit_string_t _M0L6_2atmpS2841;
  moonbit_string_t _M0L6_2atmpS2842;
  moonbit_string_t* _M0L6_2atmpS2838;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2837;
  struct moonbit_result_0 _tmp_3924;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2858;
  struct _M0TPB6ToJson _M0L6_2atmpS2848;
  moonbit_string_t _M0L6_2atmpS2857;
  void* _M0L6_2atmpS2856;
  void* _M0L6_2atmpS2849;
  moonbit_string_t _M0L6_2atmpS2852;
  moonbit_string_t _M0L6_2atmpS2853;
  moonbit_string_t _M0L6_2atmpS2854;
  moonbit_string_t _M0L6_2atmpS2855;
  moonbit_string_t* _M0L6_2atmpS2851;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2850;
  struct moonbit_result_0 _tmp_3926;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2871;
  struct _M0TPB6ToJson _M0L6_2atmpS2861;
  moonbit_string_t _M0L6_2atmpS2870;
  void* _M0L6_2atmpS2869;
  void* _M0L6_2atmpS2862;
  moonbit_string_t _M0L6_2atmpS2865;
  moonbit_string_t _M0L6_2atmpS2866;
  moonbit_string_t _M0L6_2atmpS2867;
  moonbit_string_t _M0L6_2atmpS2868;
  moonbit_string_t* _M0L6_2atmpS2864;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2863;
  struct moonbit_result_0 _tmp_3928;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2884;
  struct _M0TPB6ToJson _M0L6_2atmpS2874;
  moonbit_string_t _M0L6_2atmpS2883;
  void* _M0L6_2atmpS2882;
  void* _M0L6_2atmpS2875;
  moonbit_string_t _M0L6_2atmpS2878;
  moonbit_string_t _M0L6_2atmpS2879;
  moonbit_string_t _M0L6_2atmpS2880;
  moonbit_string_t _M0L6_2atmpS2881;
  moonbit_string_t* _M0L6_2atmpS2877;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2876;
  struct moonbit_result_0 _tmp_3930;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2897;
  struct _M0TPB6ToJson _M0L6_2atmpS2887;
  moonbit_string_t _M0L6_2atmpS2896;
  void* _M0L6_2atmpS2895;
  void* _M0L6_2atmpS2888;
  moonbit_string_t _M0L6_2atmpS2891;
  moonbit_string_t _M0L6_2atmpS2892;
  moonbit_string_t _M0L6_2atmpS2893;
  moonbit_string_t _M0L6_2atmpS2894;
  moonbit_string_t* _M0L6_2atmpS2890;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2889;
  struct moonbit_result_0 _tmp_3932;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2910;
  struct _M0TPB6ToJson _M0L6_2atmpS2900;
  moonbit_string_t _M0L6_2atmpS2909;
  void* _M0L6_2atmpS2908;
  void* _M0L6_2atmpS2901;
  moonbit_string_t _M0L6_2atmpS2904;
  moonbit_string_t _M0L6_2atmpS2905;
  moonbit_string_t _M0L6_2atmpS2906;
  moonbit_string_t _M0L6_2atmpS2907;
  moonbit_string_t* _M0L6_2atmpS2903;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2902;
  struct moonbit_result_0 _tmp_3934;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2923;
  struct _M0TPB6ToJson _M0L6_2atmpS2913;
  moonbit_string_t _M0L6_2atmpS2922;
  void* _M0L6_2atmpS2921;
  void* _M0L6_2atmpS2914;
  moonbit_string_t _M0L6_2atmpS2917;
  moonbit_string_t _M0L6_2atmpS2918;
  moonbit_string_t _M0L6_2atmpS2919;
  moonbit_string_t _M0L6_2atmpS2920;
  moonbit_string_t* _M0L6_2atmpS2916;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2915;
  struct moonbit_result_0 _tmp_3936;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2936;
  struct _M0TPB6ToJson _M0L6_2atmpS2926;
  moonbit_string_t _M0L6_2atmpS2935;
  void* _M0L6_2atmpS2934;
  void* _M0L6_2atmpS2927;
  moonbit_string_t _M0L6_2atmpS2930;
  moonbit_string_t _M0L6_2atmpS2931;
  moonbit_string_t _M0L6_2atmpS2932;
  moonbit_string_t _M0L6_2atmpS2933;
  moonbit_string_t* _M0L6_2atmpS2929;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2928;
  struct moonbit_result_0 _tmp_3938;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2949;
  struct _M0TPB6ToJson _M0L6_2atmpS2939;
  moonbit_string_t _M0L6_2atmpS2948;
  void* _M0L6_2atmpS2947;
  void* _M0L6_2atmpS2940;
  moonbit_string_t _M0L6_2atmpS2943;
  moonbit_string_t _M0L6_2atmpS2944;
  moonbit_string_t _M0L6_2atmpS2945;
  moonbit_string_t _M0L6_2atmpS2946;
  moonbit_string_t* _M0L6_2atmpS2942;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2941;
  struct moonbit_result_0 _tmp_3940;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2962;
  struct _M0TPB6ToJson _M0L6_2atmpS2952;
  moonbit_string_t _M0L6_2atmpS2961;
  void* _M0L6_2atmpS2960;
  void* _M0L6_2atmpS2953;
  moonbit_string_t _M0L6_2atmpS2956;
  moonbit_string_t _M0L6_2atmpS2957;
  moonbit_string_t _M0L6_2atmpS2958;
  moonbit_string_t _M0L6_2atmpS2959;
  moonbit_string_t* _M0L6_2atmpS2955;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2954;
  struct moonbit_result_0 _tmp_3942;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2975;
  struct _M0TPB6ToJson _M0L6_2atmpS2965;
  moonbit_string_t _M0L6_2atmpS2974;
  void* _M0L6_2atmpS2973;
  void* _M0L6_2atmpS2966;
  moonbit_string_t _M0L6_2atmpS2969;
  moonbit_string_t _M0L6_2atmpS2970;
  moonbit_string_t _M0L6_2atmpS2971;
  moonbit_string_t _M0L6_2atmpS2972;
  moonbit_string_t* _M0L6_2atmpS2968;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2967;
  struct moonbit_result_0 _tmp_3944;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2988;
  struct _M0TPB6ToJson _M0L6_2atmpS2978;
  moonbit_string_t _M0L6_2atmpS2987;
  void* _M0L6_2atmpS2986;
  void* _M0L6_2atmpS2979;
  moonbit_string_t _M0L6_2atmpS2982;
  moonbit_string_t _M0L6_2atmpS2983;
  moonbit_string_t _M0L6_2atmpS2984;
  moonbit_string_t _M0L6_2atmpS2985;
  moonbit_string_t* _M0L6_2atmpS2981;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2980;
  struct moonbit_result_0 _tmp_3946;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3001;
  struct _M0TPB6ToJson _M0L6_2atmpS2991;
  moonbit_string_t _M0L6_2atmpS3000;
  void* _M0L6_2atmpS2999;
  void* _M0L6_2atmpS2992;
  moonbit_string_t _M0L6_2atmpS2995;
  moonbit_string_t _M0L6_2atmpS2996;
  moonbit_string_t _M0L6_2atmpS2997;
  moonbit_string_t _M0L6_2atmpS2998;
  moonbit_string_t* _M0L6_2atmpS2994;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2993;
  struct moonbit_result_0 _tmp_3948;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3014;
  struct _M0TPB6ToJson _M0L6_2atmpS3004;
  moonbit_string_t _M0L6_2atmpS3013;
  void* _M0L6_2atmpS3012;
  void* _M0L6_2atmpS3005;
  moonbit_string_t _M0L6_2atmpS3008;
  moonbit_string_t _M0L6_2atmpS3009;
  moonbit_string_t _M0L6_2atmpS3010;
  moonbit_string_t _M0L6_2atmpS3011;
  moonbit_string_t* _M0L6_2atmpS3007;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3006;
  struct moonbit_result_0 _tmp_3950;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3027;
  struct _M0TPB6ToJson _M0L6_2atmpS3017;
  moonbit_string_t _M0L6_2atmpS3026;
  void* _M0L6_2atmpS3025;
  void* _M0L6_2atmpS3018;
  moonbit_string_t _M0L6_2atmpS3021;
  moonbit_string_t _M0L6_2atmpS3022;
  moonbit_string_t _M0L6_2atmpS3023;
  moonbit_string_t _M0L6_2atmpS3024;
  moonbit_string_t* _M0L6_2atmpS3020;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3019;
  struct moonbit_result_0 _tmp_3952;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3040;
  struct _M0TPB6ToJson _M0L6_2atmpS3030;
  moonbit_string_t _M0L6_2atmpS3039;
  void* _M0L6_2atmpS3038;
  void* _M0L6_2atmpS3031;
  moonbit_string_t _M0L6_2atmpS3034;
  moonbit_string_t _M0L6_2atmpS3035;
  moonbit_string_t _M0L6_2atmpS3036;
  moonbit_string_t _M0L6_2atmpS3037;
  moonbit_string_t* _M0L6_2atmpS3033;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3032;
  struct moonbit_result_0 _tmp_3954;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3053;
  struct _M0TPB6ToJson _M0L6_2atmpS3043;
  moonbit_string_t _M0L6_2atmpS3052;
  void* _M0L6_2atmpS3051;
  void* _M0L6_2atmpS3044;
  moonbit_string_t _M0L6_2atmpS3047;
  moonbit_string_t _M0L6_2atmpS3048;
  moonbit_string_t _M0L6_2atmpS3049;
  moonbit_string_t _M0L6_2atmpS3050;
  moonbit_string_t* _M0L6_2atmpS3046;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3045;
  struct moonbit_result_0 _tmp_3956;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3066;
  struct _M0TPB6ToJson _M0L6_2atmpS3056;
  moonbit_string_t _M0L6_2atmpS3065;
  void* _M0L6_2atmpS3064;
  void* _M0L6_2atmpS3057;
  moonbit_string_t _M0L6_2atmpS3060;
  moonbit_string_t _M0L6_2atmpS3061;
  moonbit_string_t _M0L6_2atmpS3062;
  moonbit_string_t _M0L6_2atmpS3063;
  moonbit_string_t* _M0L6_2atmpS3059;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3058;
  struct moonbit_result_0 _tmp_3958;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3079;
  struct _M0TPB6ToJson _M0L6_2atmpS3069;
  moonbit_string_t _M0L6_2atmpS3078;
  void* _M0L6_2atmpS3077;
  void* _M0L6_2atmpS3070;
  moonbit_string_t _M0L6_2atmpS3073;
  moonbit_string_t _M0L6_2atmpS3074;
  moonbit_string_t _M0L6_2atmpS3075;
  moonbit_string_t _M0L6_2atmpS3076;
  moonbit_string_t* _M0L6_2atmpS3072;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3071;
  struct moonbit_result_0 _tmp_3960;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3092;
  struct _M0TPB6ToJson _M0L6_2atmpS3082;
  moonbit_string_t _M0L6_2atmpS3091;
  void* _M0L6_2atmpS3090;
  void* _M0L6_2atmpS3083;
  moonbit_string_t _M0L6_2atmpS3086;
  moonbit_string_t _M0L6_2atmpS3087;
  moonbit_string_t _M0L6_2atmpS3088;
  moonbit_string_t _M0L6_2atmpS3089;
  moonbit_string_t* _M0L6_2atmpS3085;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3084;
  struct moonbit_result_0 _tmp_3962;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3105;
  struct _M0TPB6ToJson _M0L6_2atmpS3095;
  moonbit_string_t _M0L6_2atmpS3104;
  void* _M0L6_2atmpS3103;
  void* _M0L6_2atmpS3096;
  moonbit_string_t _M0L6_2atmpS3099;
  moonbit_string_t _M0L6_2atmpS3100;
  moonbit_string_t _M0L6_2atmpS3101;
  moonbit_string_t _M0L6_2atmpS3102;
  moonbit_string_t* _M0L6_2atmpS3098;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3097;
  struct moonbit_result_0 _tmp_3964;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3118;
  struct _M0TPB6ToJson _M0L6_2atmpS3108;
  moonbit_string_t _M0L6_2atmpS3117;
  void* _M0L6_2atmpS3116;
  void* _M0L6_2atmpS3109;
  moonbit_string_t _M0L6_2atmpS3112;
  moonbit_string_t _M0L6_2atmpS3113;
  moonbit_string_t _M0L6_2atmpS3114;
  moonbit_string_t _M0L6_2atmpS3115;
  moonbit_string_t* _M0L6_2atmpS3111;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3110;
  struct moonbit_result_0 _tmp_3966;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3131;
  struct _M0TPB6ToJson _M0L6_2atmpS3121;
  moonbit_string_t _M0L6_2atmpS3130;
  void* _M0L6_2atmpS3129;
  void* _M0L6_2atmpS3122;
  moonbit_string_t _M0L6_2atmpS3125;
  moonbit_string_t _M0L6_2atmpS3126;
  moonbit_string_t _M0L6_2atmpS3127;
  moonbit_string_t _M0L6_2atmpS3128;
  moonbit_string_t* _M0L6_2atmpS3124;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3123;
  struct moonbit_result_0 _tmp_3968;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3144;
  struct _M0TPB6ToJson _M0L6_2atmpS3134;
  moonbit_string_t _M0L6_2atmpS3143;
  void* _M0L6_2atmpS3142;
  void* _M0L6_2atmpS3135;
  moonbit_string_t _M0L6_2atmpS3138;
  moonbit_string_t _M0L6_2atmpS3139;
  moonbit_string_t _M0L6_2atmpS3140;
  moonbit_string_t _M0L6_2atmpS3141;
  moonbit_string_t* _M0L6_2atmpS3137;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3136;
  struct moonbit_result_0 _tmp_3970;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3157;
  struct _M0TPB6ToJson _M0L6_2atmpS3147;
  moonbit_string_t _M0L6_2atmpS3156;
  void* _M0L6_2atmpS3155;
  void* _M0L6_2atmpS3148;
  moonbit_string_t _M0L6_2atmpS3151;
  moonbit_string_t _M0L6_2atmpS3152;
  moonbit_string_t _M0L6_2atmpS3153;
  moonbit_string_t _M0L6_2atmpS3154;
  moonbit_string_t* _M0L6_2atmpS3150;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3149;
  struct moonbit_result_0 _tmp_3972;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3170;
  struct _M0TPB6ToJson _M0L6_2atmpS3160;
  moonbit_string_t _M0L6_2atmpS3169;
  void* _M0L6_2atmpS3168;
  void* _M0L6_2atmpS3161;
  moonbit_string_t _M0L6_2atmpS3164;
  moonbit_string_t _M0L6_2atmpS3165;
  moonbit_string_t _M0L6_2atmpS3166;
  moonbit_string_t _M0L6_2atmpS3167;
  moonbit_string_t* _M0L6_2atmpS3163;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3162;
  struct moonbit_result_0 _tmp_3974;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3183;
  struct _M0TPB6ToJson _M0L6_2atmpS3173;
  moonbit_string_t _M0L6_2atmpS3182;
  void* _M0L6_2atmpS3181;
  void* _M0L6_2atmpS3174;
  moonbit_string_t _M0L6_2atmpS3177;
  moonbit_string_t _M0L6_2atmpS3178;
  moonbit_string_t _M0L6_2atmpS3179;
  moonbit_string_t _M0L6_2atmpS3180;
  moonbit_string_t* _M0L6_2atmpS3176;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3175;
  struct moonbit_result_0 _tmp_3976;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3196;
  struct _M0TPB6ToJson _M0L6_2atmpS3186;
  moonbit_string_t _M0L6_2atmpS3195;
  void* _M0L6_2atmpS3194;
  void* _M0L6_2atmpS3187;
  moonbit_string_t _M0L6_2atmpS3190;
  moonbit_string_t _M0L6_2atmpS3191;
  moonbit_string_t _M0L6_2atmpS3192;
  moonbit_string_t _M0L6_2atmpS3193;
  moonbit_string_t* _M0L6_2atmpS3189;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3188;
  struct moonbit_result_0 _tmp_3978;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3209;
  struct _M0TPB6ToJson _M0L6_2atmpS3199;
  moonbit_string_t _M0L6_2atmpS3208;
  void* _M0L6_2atmpS3207;
  void* _M0L6_2atmpS3200;
  moonbit_string_t _M0L6_2atmpS3203;
  moonbit_string_t _M0L6_2atmpS3204;
  moonbit_string_t _M0L6_2atmpS3205;
  moonbit_string_t _M0L6_2atmpS3206;
  moonbit_string_t* _M0L6_2atmpS3202;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3201;
  struct moonbit_result_0 _tmp_3980;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3222;
  struct _M0TPB6ToJson _M0L6_2atmpS3212;
  moonbit_string_t _M0L6_2atmpS3221;
  void* _M0L6_2atmpS3220;
  void* _M0L6_2atmpS3213;
  moonbit_string_t _M0L6_2atmpS3216;
  moonbit_string_t _M0L6_2atmpS3217;
  moonbit_string_t _M0L6_2atmpS3218;
  moonbit_string_t _M0L6_2atmpS3219;
  moonbit_string_t* _M0L6_2atmpS3215;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3214;
  struct moonbit_result_0 _tmp_3982;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3235;
  struct _M0TPB6ToJson _M0L6_2atmpS3225;
  moonbit_string_t _M0L6_2atmpS3234;
  void* _M0L6_2atmpS3233;
  void* _M0L6_2atmpS3226;
  moonbit_string_t _M0L6_2atmpS3229;
  moonbit_string_t _M0L6_2atmpS3230;
  moonbit_string_t _M0L6_2atmpS3231;
  moonbit_string_t _M0L6_2atmpS3232;
  moonbit_string_t* _M0L6_2atmpS3228;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3227;
  struct moonbit_result_0 _tmp_3984;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3248;
  struct _M0TPB6ToJson _M0L6_2atmpS3238;
  moonbit_string_t _M0L6_2atmpS3247;
  void* _M0L6_2atmpS3246;
  void* _M0L6_2atmpS3239;
  moonbit_string_t _M0L6_2atmpS3242;
  moonbit_string_t _M0L6_2atmpS3243;
  moonbit_string_t _M0L6_2atmpS3244;
  moonbit_string_t _M0L6_2atmpS3245;
  moonbit_string_t* _M0L6_2atmpS3241;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3240;
  struct moonbit_result_0 _tmp_3986;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3261;
  struct _M0TPB6ToJson _M0L6_2atmpS3251;
  moonbit_string_t _M0L6_2atmpS3260;
  void* _M0L6_2atmpS3259;
  void* _M0L6_2atmpS3252;
  moonbit_string_t _M0L6_2atmpS3255;
  moonbit_string_t _M0L6_2atmpS3256;
  moonbit_string_t _M0L6_2atmpS3257;
  moonbit_string_t _M0L6_2atmpS3258;
  moonbit_string_t* _M0L6_2atmpS3254;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3253;
  struct moonbit_result_0 _tmp_3988;
  struct _M0Y3Int* _M0L14_2aboxed__selfS3274;
  struct _M0TPB6ToJson _M0L6_2atmpS3264;
  moonbit_string_t _M0L6_2atmpS3273;
  void* _M0L6_2atmpS3272;
  void* _M0L6_2atmpS3265;
  moonbit_string_t _M0L6_2atmpS3268;
  moonbit_string_t _M0L6_2atmpS3269;
  moonbit_string_t _M0L6_2atmpS3270;
  moonbit_string_t _M0L6_2atmpS3271;
  moonbit_string_t* _M0L6_2atmpS3267;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3266;
  #line 2 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L14_2aboxed__selfS2481
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2481)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2481->$0 = 100;
  _M0L6_2atmpS2471
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2481
  };
  _M0L6_2atmpS2480 = 0;
  #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2479 = _M0MPC14json4Json6number(0x1.9p+6, _M0L6_2atmpS2480);
  _M0L6_2atmpS2472 = _M0L6_2atmpS2479;
  _M0L6_2atmpS2475 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2476 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS2477 = 0;
  _M0L6_2atmpS2478 = 0;
  _M0L6_2atmpS2474 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2474[0] = _M0L6_2atmpS2475;
  _M0L6_2atmpS2474[1] = _M0L6_2atmpS2476;
  _M0L6_2atmpS2474[2] = _M0L6_2atmpS2477;
  _M0L6_2atmpS2474[3] = _M0L6_2atmpS2478;
  _M0L6_2atmpS2473
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2473)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2473->$0 = _M0L6_2atmpS2474;
  _M0L6_2atmpS2473->$1 = 4;
  #line 3 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3868
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2471, _M0L6_2atmpS2472, (moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS2473);
  if (_tmp_3868.tag) {
    int32_t const _M0L5_2aokS2482 = _tmp_3868.data.ok;
  } else {
    void* const _M0L6_2aerrS2483 = _tmp_3868.data.err;
    struct moonbit_result_0 _result_3869;
    _result_3869.tag = 0;
    _result_3869.data.err = _M0L6_2aerrS2483;
    return _result_3869;
  }
  _M0L14_2aboxed__selfS2494
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2494)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2494->$0 = 101;
  _M0L6_2atmpS2484
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2494
  };
  _M0L6_2atmpS2493 = 0;
  #line 4 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2492 = _M0MPC14json4Json6number(0x1.94p+6, _M0L6_2atmpS2493);
  _M0L6_2atmpS2485 = _M0L6_2atmpS2492;
  _M0L6_2atmpS2488 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS2489 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS2490 = 0;
  _M0L6_2atmpS2491 = 0;
  _M0L6_2atmpS2487 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2487[0] = _M0L6_2atmpS2488;
  _M0L6_2atmpS2487[1] = _M0L6_2atmpS2489;
  _M0L6_2atmpS2487[2] = _M0L6_2atmpS2490;
  _M0L6_2atmpS2487[3] = _M0L6_2atmpS2491;
  _M0L6_2atmpS2486
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2486)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2486->$0 = _M0L6_2atmpS2487;
  _M0L6_2atmpS2486->$1 = 4;
  #line 4 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3870
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2484, _M0L6_2atmpS2485, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS2486);
  if (_tmp_3870.tag) {
    int32_t const _M0L5_2aokS2495 = _tmp_3870.data.ok;
  } else {
    void* const _M0L6_2aerrS2496 = _tmp_3870.data.err;
    struct moonbit_result_0 _result_3871;
    _result_3871.tag = 0;
    _result_3871.data.err = _M0L6_2aerrS2496;
    return _result_3871;
  }
  _M0L14_2aboxed__selfS2507
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2507)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2507->$0 = 102;
  _M0L6_2atmpS2497
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2507
  };
  _M0L6_2atmpS2506 = 0;
  #line 5 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2505 = _M0MPC14json4Json6number(0x1.98p+6, _M0L6_2atmpS2506);
  _M0L6_2atmpS2498 = _M0L6_2atmpS2505;
  _M0L6_2atmpS2501 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS2502 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS2503 = 0;
  _M0L6_2atmpS2504 = 0;
  _M0L6_2atmpS2500 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2500[0] = _M0L6_2atmpS2501;
  _M0L6_2atmpS2500[1] = _M0L6_2atmpS2502;
  _M0L6_2atmpS2500[2] = _M0L6_2atmpS2503;
  _M0L6_2atmpS2500[3] = _M0L6_2atmpS2504;
  _M0L6_2atmpS2499
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2499)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2499->$0 = _M0L6_2atmpS2500;
  _M0L6_2atmpS2499->$1 = 4;
  #line 5 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3872
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2497, _M0L6_2atmpS2498, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS2499);
  if (_tmp_3872.tag) {
    int32_t const _M0L5_2aokS2508 = _tmp_3872.data.ok;
  } else {
    void* const _M0L6_2aerrS2509 = _tmp_3872.data.err;
    struct moonbit_result_0 _result_3873;
    _result_3873.tag = 0;
    _result_3873.data.err = _M0L6_2aerrS2509;
    return _result_3873;
  }
  _M0L14_2aboxed__selfS2520
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2520)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2520->$0 = 103;
  _M0L6_2atmpS2510
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2520
  };
  _M0L6_2atmpS2519 = 0;
  #line 6 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2518 = _M0MPC14json4Json6number(0x1.9cp+6, _M0L6_2atmpS2519);
  _M0L6_2atmpS2511 = _M0L6_2atmpS2518;
  _M0L6_2atmpS2514 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS2515 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS2516 = 0;
  _M0L6_2atmpS2517 = 0;
  _M0L6_2atmpS2513 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2513[0] = _M0L6_2atmpS2514;
  _M0L6_2atmpS2513[1] = _M0L6_2atmpS2515;
  _M0L6_2atmpS2513[2] = _M0L6_2atmpS2516;
  _M0L6_2atmpS2513[3] = _M0L6_2atmpS2517;
  _M0L6_2atmpS2512
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2512)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2512->$0 = _M0L6_2atmpS2513;
  _M0L6_2atmpS2512->$1 = 4;
  #line 6 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3874
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2510, _M0L6_2atmpS2511, (moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS2512);
  if (_tmp_3874.tag) {
    int32_t const _M0L5_2aokS2521 = _tmp_3874.data.ok;
  } else {
    void* const _M0L6_2aerrS2522 = _tmp_3874.data.err;
    struct moonbit_result_0 _result_3875;
    _result_3875.tag = 0;
    _result_3875.data.err = _M0L6_2aerrS2522;
    return _result_3875;
  }
  _M0L14_2aboxed__selfS2533
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2533)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2533->$0 = 200;
  _M0L6_2atmpS2523
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2533
  };
  _M0L6_2atmpS2532 = 0;
  #line 7 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2531 = _M0MPC14json4Json6number(0x1.9p+7, _M0L6_2atmpS2532);
  _M0L6_2atmpS2524 = _M0L6_2atmpS2531;
  _M0L6_2atmpS2527 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS2528 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS2529 = 0;
  _M0L6_2atmpS2530 = 0;
  _M0L6_2atmpS2526 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2526[0] = _M0L6_2atmpS2527;
  _M0L6_2atmpS2526[1] = _M0L6_2atmpS2528;
  _M0L6_2atmpS2526[2] = _M0L6_2atmpS2529;
  _M0L6_2atmpS2526[3] = _M0L6_2atmpS2530;
  _M0L6_2atmpS2525
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2525)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2525->$0 = _M0L6_2atmpS2526;
  _M0L6_2atmpS2525->$1 = 4;
  #line 7 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3876
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2523, _M0L6_2atmpS2524, (moonbit_string_t)moonbit_string_literal_29.data, _M0L6_2atmpS2525);
  if (_tmp_3876.tag) {
    int32_t const _M0L5_2aokS2534 = _tmp_3876.data.ok;
  } else {
    void* const _M0L6_2aerrS2535 = _tmp_3876.data.err;
    struct moonbit_result_0 _result_3877;
    _result_3877.tag = 0;
    _result_3877.data.err = _M0L6_2aerrS2535;
    return _result_3877;
  }
  _M0L14_2aboxed__selfS2546
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2546)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2546->$0 = 201;
  _M0L6_2atmpS2536
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2546
  };
  _M0L6_2atmpS2545 = 0;
  #line 8 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2544 = _M0MPC14json4Json6number(0x1.92p+7, _M0L6_2atmpS2545);
  _M0L6_2atmpS2537 = _M0L6_2atmpS2544;
  _M0L6_2atmpS2540 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS2541 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS2542 = 0;
  _M0L6_2atmpS2543 = 0;
  _M0L6_2atmpS2539 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2539[0] = _M0L6_2atmpS2540;
  _M0L6_2atmpS2539[1] = _M0L6_2atmpS2541;
  _M0L6_2atmpS2539[2] = _M0L6_2atmpS2542;
  _M0L6_2atmpS2539[3] = _M0L6_2atmpS2543;
  _M0L6_2atmpS2538
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2538)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2538->$0 = _M0L6_2atmpS2539;
  _M0L6_2atmpS2538->$1 = 4;
  #line 8 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3878
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2536, _M0L6_2atmpS2537, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS2538);
  if (_tmp_3878.tag) {
    int32_t const _M0L5_2aokS2547 = _tmp_3878.data.ok;
  } else {
    void* const _M0L6_2aerrS2548 = _tmp_3878.data.err;
    struct moonbit_result_0 _result_3879;
    _result_3879.tag = 0;
    _result_3879.data.err = _M0L6_2aerrS2548;
    return _result_3879;
  }
  _M0L14_2aboxed__selfS2559
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2559)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2559->$0 = 202;
  _M0L6_2atmpS2549
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2559
  };
  _M0L6_2atmpS2558 = 0;
  #line 9 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2557 = _M0MPC14json4Json6number(0x1.94p+7, _M0L6_2atmpS2558);
  _M0L6_2atmpS2550 = _M0L6_2atmpS2557;
  _M0L6_2atmpS2553 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS2554 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS2555 = 0;
  _M0L6_2atmpS2556 = 0;
  _M0L6_2atmpS2552 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2552[0] = _M0L6_2atmpS2553;
  _M0L6_2atmpS2552[1] = _M0L6_2atmpS2554;
  _M0L6_2atmpS2552[2] = _M0L6_2atmpS2555;
  _M0L6_2atmpS2552[3] = _M0L6_2atmpS2556;
  _M0L6_2atmpS2551
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2551)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2551->$0 = _M0L6_2atmpS2552;
  _M0L6_2atmpS2551->$1 = 4;
  #line 9 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3880
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2549, _M0L6_2atmpS2550, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS2551);
  if (_tmp_3880.tag) {
    int32_t const _M0L5_2aokS2560 = _tmp_3880.data.ok;
  } else {
    void* const _M0L6_2aerrS2561 = _tmp_3880.data.err;
    struct moonbit_result_0 _result_3881;
    _result_3881.tag = 0;
    _result_3881.data.err = _M0L6_2aerrS2561;
    return _result_3881;
  }
  _M0L14_2aboxed__selfS2572
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2572)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2572->$0 = 203;
  _M0L6_2atmpS2562
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2572
  };
  _M0L6_2atmpS2571 = 0;
  #line 10 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2570 = _M0MPC14json4Json6number(0x1.96p+7, _M0L6_2atmpS2571);
  _M0L6_2atmpS2563 = _M0L6_2atmpS2570;
  _M0L6_2atmpS2566 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS2567 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS2568 = 0;
  _M0L6_2atmpS2569 = 0;
  _M0L6_2atmpS2565 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2565[0] = _M0L6_2atmpS2566;
  _M0L6_2atmpS2565[1] = _M0L6_2atmpS2567;
  _M0L6_2atmpS2565[2] = _M0L6_2atmpS2568;
  _M0L6_2atmpS2565[3] = _M0L6_2atmpS2569;
  _M0L6_2atmpS2564
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2564)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2564->$0 = _M0L6_2atmpS2565;
  _M0L6_2atmpS2564->$1 = 4;
  #line 10 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3882
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2562, _M0L6_2atmpS2563, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS2564);
  if (_tmp_3882.tag) {
    int32_t const _M0L5_2aokS2573 = _tmp_3882.data.ok;
  } else {
    void* const _M0L6_2aerrS2574 = _tmp_3882.data.err;
    struct moonbit_result_0 _result_3883;
    _result_3883.tag = 0;
    _result_3883.data.err = _M0L6_2aerrS2574;
    return _result_3883;
  }
  _M0L14_2aboxed__selfS2585
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2585)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2585->$0 = 204;
  _M0L6_2atmpS2575
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2585
  };
  _M0L6_2atmpS2584 = 0;
  #line 11 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2583 = _M0MPC14json4Json6number(0x1.98p+7, _M0L6_2atmpS2584);
  _M0L6_2atmpS2576 = _M0L6_2atmpS2583;
  _M0L6_2atmpS2579 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS2580 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS2581 = 0;
  _M0L6_2atmpS2582 = 0;
  _M0L6_2atmpS2578 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2578[0] = _M0L6_2atmpS2579;
  _M0L6_2atmpS2578[1] = _M0L6_2atmpS2580;
  _M0L6_2atmpS2578[2] = _M0L6_2atmpS2581;
  _M0L6_2atmpS2578[3] = _M0L6_2atmpS2582;
  _M0L6_2atmpS2577
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2577)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2577->$0 = _M0L6_2atmpS2578;
  _M0L6_2atmpS2577->$1 = 4;
  #line 11 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3884
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2575, _M0L6_2atmpS2576, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS2577);
  if (_tmp_3884.tag) {
    int32_t const _M0L5_2aokS2586 = _tmp_3884.data.ok;
  } else {
    void* const _M0L6_2aerrS2587 = _tmp_3884.data.err;
    struct moonbit_result_0 _result_3885;
    _result_3885.tag = 0;
    _result_3885.data.err = _M0L6_2aerrS2587;
    return _result_3885;
  }
  _M0L14_2aboxed__selfS2598
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2598)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2598->$0 = 205;
  _M0L6_2atmpS2588
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2598
  };
  _M0L6_2atmpS2597 = 0;
  #line 12 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2596 = _M0MPC14json4Json6number(0x1.9ap+7, _M0L6_2atmpS2597);
  _M0L6_2atmpS2589 = _M0L6_2atmpS2596;
  _M0L6_2atmpS2592 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS2593 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS2594 = 0;
  _M0L6_2atmpS2595 = 0;
  _M0L6_2atmpS2591 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2591[0] = _M0L6_2atmpS2592;
  _M0L6_2atmpS2591[1] = _M0L6_2atmpS2593;
  _M0L6_2atmpS2591[2] = _M0L6_2atmpS2594;
  _M0L6_2atmpS2591[3] = _M0L6_2atmpS2595;
  _M0L6_2atmpS2590
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2590)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2590->$0 = _M0L6_2atmpS2591;
  _M0L6_2atmpS2590->$1 = 4;
  #line 12 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3886
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2588, _M0L6_2atmpS2589, (moonbit_string_t)moonbit_string_literal_44.data, _M0L6_2atmpS2590);
  if (_tmp_3886.tag) {
    int32_t const _M0L5_2aokS2599 = _tmp_3886.data.ok;
  } else {
    void* const _M0L6_2aerrS2600 = _tmp_3886.data.err;
    struct moonbit_result_0 _result_3887;
    _result_3887.tag = 0;
    _result_3887.data.err = _M0L6_2aerrS2600;
    return _result_3887;
  }
  _M0L14_2aboxed__selfS2611
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2611)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2611->$0 = 206;
  _M0L6_2atmpS2601
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2611
  };
  _M0L6_2atmpS2610 = 0;
  #line 13 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2609 = _M0MPC14json4Json6number(0x1.9cp+7, _M0L6_2atmpS2610);
  _M0L6_2atmpS2602 = _M0L6_2atmpS2609;
  _M0L6_2atmpS2605 = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS2606 = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS2607 = 0;
  _M0L6_2atmpS2608 = 0;
  _M0L6_2atmpS2604 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2604[0] = _M0L6_2atmpS2605;
  _M0L6_2atmpS2604[1] = _M0L6_2atmpS2606;
  _M0L6_2atmpS2604[2] = _M0L6_2atmpS2607;
  _M0L6_2atmpS2604[3] = _M0L6_2atmpS2608;
  _M0L6_2atmpS2603
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2603)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2603->$0 = _M0L6_2atmpS2604;
  _M0L6_2atmpS2603->$1 = 4;
  #line 13 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3888
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2601, _M0L6_2atmpS2602, (moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS2603);
  if (_tmp_3888.tag) {
    int32_t const _M0L5_2aokS2612 = _tmp_3888.data.ok;
  } else {
    void* const _M0L6_2aerrS2613 = _tmp_3888.data.err;
    struct moonbit_result_0 _result_3889;
    _result_3889.tag = 0;
    _result_3889.data.err = _M0L6_2aerrS2613;
    return _result_3889;
  }
  _M0L14_2aboxed__selfS2624
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2624)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2624->$0 = 207;
  _M0L6_2atmpS2614
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2624
  };
  _M0L6_2atmpS2623 = 0;
  #line 14 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2622 = _M0MPC14json4Json6number(0x1.9ep+7, _M0L6_2atmpS2623);
  _M0L6_2atmpS2615 = _M0L6_2atmpS2622;
  _M0L6_2atmpS2618 = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS2619 = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS2620 = 0;
  _M0L6_2atmpS2621 = 0;
  _M0L6_2atmpS2617 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2617[0] = _M0L6_2atmpS2618;
  _M0L6_2atmpS2617[1] = _M0L6_2atmpS2619;
  _M0L6_2atmpS2617[2] = _M0L6_2atmpS2620;
  _M0L6_2atmpS2617[3] = _M0L6_2atmpS2621;
  _M0L6_2atmpS2616
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2616)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2616->$0 = _M0L6_2atmpS2617;
  _M0L6_2atmpS2616->$1 = 4;
  #line 14 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3890
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2614, _M0L6_2atmpS2615, (moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS2616);
  if (_tmp_3890.tag) {
    int32_t const _M0L5_2aokS2625 = _tmp_3890.data.ok;
  } else {
    void* const _M0L6_2aerrS2626 = _tmp_3890.data.err;
    struct moonbit_result_0 _result_3891;
    _result_3891.tag = 0;
    _result_3891.data.err = _M0L6_2aerrS2626;
    return _result_3891;
  }
  _M0L14_2aboxed__selfS2637
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2637)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2637->$0 = 208;
  _M0L6_2atmpS2627
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2637
  };
  _M0L6_2atmpS2636 = 0;
  #line 15 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2635 = _M0MPC14json4Json6number(0x1.ap+7, _M0L6_2atmpS2636);
  _M0L6_2atmpS2628 = _M0L6_2atmpS2635;
  _M0L6_2atmpS2631 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS2632 = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS2633 = 0;
  _M0L6_2atmpS2634 = 0;
  _M0L6_2atmpS2630 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2630[0] = _M0L6_2atmpS2631;
  _M0L6_2atmpS2630[1] = _M0L6_2atmpS2632;
  _M0L6_2atmpS2630[2] = _M0L6_2atmpS2633;
  _M0L6_2atmpS2630[3] = _M0L6_2atmpS2634;
  _M0L6_2atmpS2629
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2629)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2629->$0 = _M0L6_2atmpS2630;
  _M0L6_2atmpS2629->$1 = 4;
  #line 15 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3892
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2627, _M0L6_2atmpS2628, (moonbit_string_t)moonbit_string_literal_53.data, _M0L6_2atmpS2629);
  if (_tmp_3892.tag) {
    int32_t const _M0L5_2aokS2638 = _tmp_3892.data.ok;
  } else {
    void* const _M0L6_2aerrS2639 = _tmp_3892.data.err;
    struct moonbit_result_0 _result_3893;
    _result_3893.tag = 0;
    _result_3893.data.err = _M0L6_2aerrS2639;
    return _result_3893;
  }
  _M0L14_2aboxed__selfS2650
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2650)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2650->$0 = 226;
  _M0L6_2atmpS2640
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2650
  };
  _M0L6_2atmpS2649 = 0;
  #line 16 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2648 = _M0MPC14json4Json6number(0x1.c4p+7, _M0L6_2atmpS2649);
  _M0L6_2atmpS2641 = _M0L6_2atmpS2648;
  _M0L6_2atmpS2644 = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS2645 = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS2646 = 0;
  _M0L6_2atmpS2647 = 0;
  _M0L6_2atmpS2643 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2643[0] = _M0L6_2atmpS2644;
  _M0L6_2atmpS2643[1] = _M0L6_2atmpS2645;
  _M0L6_2atmpS2643[2] = _M0L6_2atmpS2646;
  _M0L6_2atmpS2643[3] = _M0L6_2atmpS2647;
  _M0L6_2atmpS2642
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2642)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2642->$0 = _M0L6_2atmpS2643;
  _M0L6_2atmpS2642->$1 = 4;
  #line 16 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3894
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2640, _M0L6_2atmpS2641, (moonbit_string_t)moonbit_string_literal_56.data, _M0L6_2atmpS2642);
  if (_tmp_3894.tag) {
    int32_t const _M0L5_2aokS2651 = _tmp_3894.data.ok;
  } else {
    void* const _M0L6_2aerrS2652 = _tmp_3894.data.err;
    struct moonbit_result_0 _result_3895;
    _result_3895.tag = 0;
    _result_3895.data.err = _M0L6_2aerrS2652;
    return _result_3895;
  }
  _M0L14_2aboxed__selfS2663
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2663)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2663->$0 = 300;
  _M0L6_2atmpS2653
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2663
  };
  _M0L6_2atmpS2662 = 0;
  #line 17 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2661 = _M0MPC14json4Json6number(0x1.2cp+8, _M0L6_2atmpS2662);
  _M0L6_2atmpS2654 = _M0L6_2atmpS2661;
  _M0L6_2atmpS2657 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS2658 = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS2659 = 0;
  _M0L6_2atmpS2660 = 0;
  _M0L6_2atmpS2656 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2656[0] = _M0L6_2atmpS2657;
  _M0L6_2atmpS2656[1] = _M0L6_2atmpS2658;
  _M0L6_2atmpS2656[2] = _M0L6_2atmpS2659;
  _M0L6_2atmpS2656[3] = _M0L6_2atmpS2660;
  _M0L6_2atmpS2655
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2655)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2655->$0 = _M0L6_2atmpS2656;
  _M0L6_2atmpS2655->$1 = 4;
  #line 17 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3896
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2653, _M0L6_2atmpS2654, (moonbit_string_t)moonbit_string_literal_59.data, _M0L6_2atmpS2655);
  if (_tmp_3896.tag) {
    int32_t const _M0L5_2aokS2664 = _tmp_3896.data.ok;
  } else {
    void* const _M0L6_2aerrS2665 = _tmp_3896.data.err;
    struct moonbit_result_0 _result_3897;
    _result_3897.tag = 0;
    _result_3897.data.err = _M0L6_2aerrS2665;
    return _result_3897;
  }
  _M0L14_2aboxed__selfS2676
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2676)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2676->$0 = 301;
  _M0L6_2atmpS2666
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2676
  };
  _M0L6_2atmpS2675 = 0;
  #line 18 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2674 = _M0MPC14json4Json6number(0x1.2dp+8, _M0L6_2atmpS2675);
  _M0L6_2atmpS2667 = _M0L6_2atmpS2674;
  _M0L6_2atmpS2670 = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS2671 = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS2672 = 0;
  _M0L6_2atmpS2673 = 0;
  _M0L6_2atmpS2669 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2669[0] = _M0L6_2atmpS2670;
  _M0L6_2atmpS2669[1] = _M0L6_2atmpS2671;
  _M0L6_2atmpS2669[2] = _M0L6_2atmpS2672;
  _M0L6_2atmpS2669[3] = _M0L6_2atmpS2673;
  _M0L6_2atmpS2668
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2668)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2668->$0 = _M0L6_2atmpS2669;
  _M0L6_2atmpS2668->$1 = 4;
  #line 18 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3898
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2666, _M0L6_2atmpS2667, (moonbit_string_t)moonbit_string_literal_62.data, _M0L6_2atmpS2668);
  if (_tmp_3898.tag) {
    int32_t const _M0L5_2aokS2677 = _tmp_3898.data.ok;
  } else {
    void* const _M0L6_2aerrS2678 = _tmp_3898.data.err;
    struct moonbit_result_0 _result_3899;
    _result_3899.tag = 0;
    _result_3899.data.err = _M0L6_2aerrS2678;
    return _result_3899;
  }
  _M0L14_2aboxed__selfS2689
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2689)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2689->$0 = 302;
  _M0L6_2atmpS2679
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2689
  };
  _M0L6_2atmpS2688 = 0;
  #line 19 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2687 = _M0MPC14json4Json6number(0x1.2ep+8, _M0L6_2atmpS2688);
  _M0L6_2atmpS2680 = _M0L6_2atmpS2687;
  _M0L6_2atmpS2683 = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS2684 = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS2685 = 0;
  _M0L6_2atmpS2686 = 0;
  _M0L6_2atmpS2682 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2682[0] = _M0L6_2atmpS2683;
  _M0L6_2atmpS2682[1] = _M0L6_2atmpS2684;
  _M0L6_2atmpS2682[2] = _M0L6_2atmpS2685;
  _M0L6_2atmpS2682[3] = _M0L6_2atmpS2686;
  _M0L6_2atmpS2681
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2681)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2681->$0 = _M0L6_2atmpS2682;
  _M0L6_2atmpS2681->$1 = 4;
  #line 19 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3900
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2679, _M0L6_2atmpS2680, (moonbit_string_t)moonbit_string_literal_65.data, _M0L6_2atmpS2681);
  if (_tmp_3900.tag) {
    int32_t const _M0L5_2aokS2690 = _tmp_3900.data.ok;
  } else {
    void* const _M0L6_2aerrS2691 = _tmp_3900.data.err;
    struct moonbit_result_0 _result_3901;
    _result_3901.tag = 0;
    _result_3901.data.err = _M0L6_2aerrS2691;
    return _result_3901;
  }
  _M0L14_2aboxed__selfS2702
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2702)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2702->$0 = 303;
  _M0L6_2atmpS2692
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2702
  };
  _M0L6_2atmpS2701 = 0;
  #line 20 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2700 = _M0MPC14json4Json6number(0x1.2fp+8, _M0L6_2atmpS2701);
  _M0L6_2atmpS2693 = _M0L6_2atmpS2700;
  _M0L6_2atmpS2696 = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS2697 = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS2698 = 0;
  _M0L6_2atmpS2699 = 0;
  _M0L6_2atmpS2695 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2695[0] = _M0L6_2atmpS2696;
  _M0L6_2atmpS2695[1] = _M0L6_2atmpS2697;
  _M0L6_2atmpS2695[2] = _M0L6_2atmpS2698;
  _M0L6_2atmpS2695[3] = _M0L6_2atmpS2699;
  _M0L6_2atmpS2694
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2694)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2694->$0 = _M0L6_2atmpS2695;
  _M0L6_2atmpS2694->$1 = 4;
  #line 20 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3902
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2692, _M0L6_2atmpS2693, (moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS2694);
  if (_tmp_3902.tag) {
    int32_t const _M0L5_2aokS2703 = _tmp_3902.data.ok;
  } else {
    void* const _M0L6_2aerrS2704 = _tmp_3902.data.err;
    struct moonbit_result_0 _result_3903;
    _result_3903.tag = 0;
    _result_3903.data.err = _M0L6_2aerrS2704;
    return _result_3903;
  }
  _M0L14_2aboxed__selfS2715
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2715)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2715->$0 = 304;
  _M0L6_2atmpS2705
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2715
  };
  _M0L6_2atmpS2714 = 0;
  #line 21 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2713 = _M0MPC14json4Json6number(0x1.3p+8, _M0L6_2atmpS2714);
  _M0L6_2atmpS2706 = _M0L6_2atmpS2713;
  _M0L6_2atmpS2709 = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS2710 = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS2711 = 0;
  _M0L6_2atmpS2712 = 0;
  _M0L6_2atmpS2708 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2708[0] = _M0L6_2atmpS2709;
  _M0L6_2atmpS2708[1] = _M0L6_2atmpS2710;
  _M0L6_2atmpS2708[2] = _M0L6_2atmpS2711;
  _M0L6_2atmpS2708[3] = _M0L6_2atmpS2712;
  _M0L6_2atmpS2707
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2707)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2707->$0 = _M0L6_2atmpS2708;
  _M0L6_2atmpS2707->$1 = 4;
  #line 21 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3904
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2705, _M0L6_2atmpS2706, (moonbit_string_t)moonbit_string_literal_71.data, _M0L6_2atmpS2707);
  if (_tmp_3904.tag) {
    int32_t const _M0L5_2aokS2716 = _tmp_3904.data.ok;
  } else {
    void* const _M0L6_2aerrS2717 = _tmp_3904.data.err;
    struct moonbit_result_0 _result_3905;
    _result_3905.tag = 0;
    _result_3905.data.err = _M0L6_2aerrS2717;
    return _result_3905;
  }
  _M0L14_2aboxed__selfS2728
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2728)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2728->$0 = 305;
  _M0L6_2atmpS2718
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2728
  };
  _M0L6_2atmpS2727 = 0;
  #line 22 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2726 = _M0MPC14json4Json6number(0x1.31p+8, _M0L6_2atmpS2727);
  _M0L6_2atmpS2719 = _M0L6_2atmpS2726;
  _M0L6_2atmpS2722 = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS2723 = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS2724 = 0;
  _M0L6_2atmpS2725 = 0;
  _M0L6_2atmpS2721 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2721[0] = _M0L6_2atmpS2722;
  _M0L6_2atmpS2721[1] = _M0L6_2atmpS2723;
  _M0L6_2atmpS2721[2] = _M0L6_2atmpS2724;
  _M0L6_2atmpS2721[3] = _M0L6_2atmpS2725;
  _M0L6_2atmpS2720
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2720)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2720->$0 = _M0L6_2atmpS2721;
  _M0L6_2atmpS2720->$1 = 4;
  #line 22 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3906
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2718, _M0L6_2atmpS2719, (moonbit_string_t)moonbit_string_literal_74.data, _M0L6_2atmpS2720);
  if (_tmp_3906.tag) {
    int32_t const _M0L5_2aokS2729 = _tmp_3906.data.ok;
  } else {
    void* const _M0L6_2aerrS2730 = _tmp_3906.data.err;
    struct moonbit_result_0 _result_3907;
    _result_3907.tag = 0;
    _result_3907.data.err = _M0L6_2aerrS2730;
    return _result_3907;
  }
  _M0L14_2aboxed__selfS2741
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2741)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2741->$0 = 307;
  _M0L6_2atmpS2731
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2741
  };
  _M0L6_2atmpS2740 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2739 = _M0MPC14json4Json6number(0x1.33p+8, _M0L6_2atmpS2740);
  _M0L6_2atmpS2732 = _M0L6_2atmpS2739;
  _M0L6_2atmpS2735 = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS2736 = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS2737 = 0;
  _M0L6_2atmpS2738 = 0;
  _M0L6_2atmpS2734 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2734[0] = _M0L6_2atmpS2735;
  _M0L6_2atmpS2734[1] = _M0L6_2atmpS2736;
  _M0L6_2atmpS2734[2] = _M0L6_2atmpS2737;
  _M0L6_2atmpS2734[3] = _M0L6_2atmpS2738;
  _M0L6_2atmpS2733
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2733)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2733->$0 = _M0L6_2atmpS2734;
  _M0L6_2atmpS2733->$1 = 4;
  #line 23 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3908
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2731, _M0L6_2atmpS2732, (moonbit_string_t)moonbit_string_literal_77.data, _M0L6_2atmpS2733);
  if (_tmp_3908.tag) {
    int32_t const _M0L5_2aokS2742 = _tmp_3908.data.ok;
  } else {
    void* const _M0L6_2aerrS2743 = _tmp_3908.data.err;
    struct moonbit_result_0 _result_3909;
    _result_3909.tag = 0;
    _result_3909.data.err = _M0L6_2aerrS2743;
    return _result_3909;
  }
  _M0L14_2aboxed__selfS2754
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2754)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2754->$0 = 308;
  _M0L6_2atmpS2744
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2754
  };
  _M0L6_2atmpS2753 = 0;
  #line 24 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2752 = _M0MPC14json4Json6number(0x1.34p+8, _M0L6_2atmpS2753);
  _M0L6_2atmpS2745 = _M0L6_2atmpS2752;
  _M0L6_2atmpS2748 = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS2749 = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS2750 = 0;
  _M0L6_2atmpS2751 = 0;
  _M0L6_2atmpS2747 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2747[0] = _M0L6_2atmpS2748;
  _M0L6_2atmpS2747[1] = _M0L6_2atmpS2749;
  _M0L6_2atmpS2747[2] = _M0L6_2atmpS2750;
  _M0L6_2atmpS2747[3] = _M0L6_2atmpS2751;
  _M0L6_2atmpS2746
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2746)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2746->$0 = _M0L6_2atmpS2747;
  _M0L6_2atmpS2746->$1 = 4;
  #line 24 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3910
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2744, _M0L6_2atmpS2745, (moonbit_string_t)moonbit_string_literal_80.data, _M0L6_2atmpS2746);
  if (_tmp_3910.tag) {
    int32_t const _M0L5_2aokS2755 = _tmp_3910.data.ok;
  } else {
    void* const _M0L6_2aerrS2756 = _tmp_3910.data.err;
    struct moonbit_result_0 _result_3911;
    _result_3911.tag = 0;
    _result_3911.data.err = _M0L6_2aerrS2756;
    return _result_3911;
  }
  _M0L14_2aboxed__selfS2767
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2767)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2767->$0 = 400;
  _M0L6_2atmpS2757
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2767
  };
  _M0L6_2atmpS2766 = 0;
  #line 25 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2765 = _M0MPC14json4Json6number(0x1.9p+8, _M0L6_2atmpS2766);
  _M0L6_2atmpS2758 = _M0L6_2atmpS2765;
  _M0L6_2atmpS2761 = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS2762 = (moonbit_string_t)moonbit_string_literal_82.data;
  _M0L6_2atmpS2763 = 0;
  _M0L6_2atmpS2764 = 0;
  _M0L6_2atmpS2760 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2760[0] = _M0L6_2atmpS2761;
  _M0L6_2atmpS2760[1] = _M0L6_2atmpS2762;
  _M0L6_2atmpS2760[2] = _M0L6_2atmpS2763;
  _M0L6_2atmpS2760[3] = _M0L6_2atmpS2764;
  _M0L6_2atmpS2759
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2759)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2759->$0 = _M0L6_2atmpS2760;
  _M0L6_2atmpS2759->$1 = 4;
  #line 25 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3912
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2757, _M0L6_2atmpS2758, (moonbit_string_t)moonbit_string_literal_83.data, _M0L6_2atmpS2759);
  if (_tmp_3912.tag) {
    int32_t const _M0L5_2aokS2768 = _tmp_3912.data.ok;
  } else {
    void* const _M0L6_2aerrS2769 = _tmp_3912.data.err;
    struct moonbit_result_0 _result_3913;
    _result_3913.tag = 0;
    _result_3913.data.err = _M0L6_2aerrS2769;
    return _result_3913;
  }
  _M0L14_2aboxed__selfS2780
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2780)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2780->$0 = 401;
  _M0L6_2atmpS2770
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2780
  };
  _M0L6_2atmpS2779 = 0;
  #line 26 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2778 = _M0MPC14json4Json6number(0x1.91p+8, _M0L6_2atmpS2779);
  _M0L6_2atmpS2771 = _M0L6_2atmpS2778;
  _M0L6_2atmpS2774 = (moonbit_string_t)moonbit_string_literal_84.data;
  _M0L6_2atmpS2775 = (moonbit_string_t)moonbit_string_literal_85.data;
  _M0L6_2atmpS2776 = 0;
  _M0L6_2atmpS2777 = 0;
  _M0L6_2atmpS2773 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2773[0] = _M0L6_2atmpS2774;
  _M0L6_2atmpS2773[1] = _M0L6_2atmpS2775;
  _M0L6_2atmpS2773[2] = _M0L6_2atmpS2776;
  _M0L6_2atmpS2773[3] = _M0L6_2atmpS2777;
  _M0L6_2atmpS2772
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2772)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2772->$0 = _M0L6_2atmpS2773;
  _M0L6_2atmpS2772->$1 = 4;
  #line 26 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3914
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2770, _M0L6_2atmpS2771, (moonbit_string_t)moonbit_string_literal_86.data, _M0L6_2atmpS2772);
  if (_tmp_3914.tag) {
    int32_t const _M0L5_2aokS2781 = _tmp_3914.data.ok;
  } else {
    void* const _M0L6_2aerrS2782 = _tmp_3914.data.err;
    struct moonbit_result_0 _result_3915;
    _result_3915.tag = 0;
    _result_3915.data.err = _M0L6_2aerrS2782;
    return _result_3915;
  }
  _M0L14_2aboxed__selfS2793
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2793)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2793->$0 = 402;
  _M0L6_2atmpS2783
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2793
  };
  _M0L6_2atmpS2792 = 0;
  #line 27 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2791 = _M0MPC14json4Json6number(0x1.92p+8, _M0L6_2atmpS2792);
  _M0L6_2atmpS2784 = _M0L6_2atmpS2791;
  _M0L6_2atmpS2787 = (moonbit_string_t)moonbit_string_literal_87.data;
  _M0L6_2atmpS2788 = (moonbit_string_t)moonbit_string_literal_88.data;
  _M0L6_2atmpS2789 = 0;
  _M0L6_2atmpS2790 = 0;
  _M0L6_2atmpS2786 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2786[0] = _M0L6_2atmpS2787;
  _M0L6_2atmpS2786[1] = _M0L6_2atmpS2788;
  _M0L6_2atmpS2786[2] = _M0L6_2atmpS2789;
  _M0L6_2atmpS2786[3] = _M0L6_2atmpS2790;
  _M0L6_2atmpS2785
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2785)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2785->$0 = _M0L6_2atmpS2786;
  _M0L6_2atmpS2785->$1 = 4;
  #line 27 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3916
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2783, _M0L6_2atmpS2784, (moonbit_string_t)moonbit_string_literal_89.data, _M0L6_2atmpS2785);
  if (_tmp_3916.tag) {
    int32_t const _M0L5_2aokS2794 = _tmp_3916.data.ok;
  } else {
    void* const _M0L6_2aerrS2795 = _tmp_3916.data.err;
    struct moonbit_result_0 _result_3917;
    _result_3917.tag = 0;
    _result_3917.data.err = _M0L6_2aerrS2795;
    return _result_3917;
  }
  _M0L14_2aboxed__selfS2806
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2806)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2806->$0 = 403;
  _M0L6_2atmpS2796
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2806
  };
  _M0L6_2atmpS2805 = 0;
  #line 28 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2804 = _M0MPC14json4Json6number(0x1.93p+8, _M0L6_2atmpS2805);
  _M0L6_2atmpS2797 = _M0L6_2atmpS2804;
  _M0L6_2atmpS2800 = (moonbit_string_t)moonbit_string_literal_90.data;
  _M0L6_2atmpS2801 = (moonbit_string_t)moonbit_string_literal_91.data;
  _M0L6_2atmpS2802 = 0;
  _M0L6_2atmpS2803 = 0;
  _M0L6_2atmpS2799 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2799[0] = _M0L6_2atmpS2800;
  _M0L6_2atmpS2799[1] = _M0L6_2atmpS2801;
  _M0L6_2atmpS2799[2] = _M0L6_2atmpS2802;
  _M0L6_2atmpS2799[3] = _M0L6_2atmpS2803;
  _M0L6_2atmpS2798
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2798)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2798->$0 = _M0L6_2atmpS2799;
  _M0L6_2atmpS2798->$1 = 4;
  #line 28 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3918
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2796, _M0L6_2atmpS2797, (moonbit_string_t)moonbit_string_literal_92.data, _M0L6_2atmpS2798);
  if (_tmp_3918.tag) {
    int32_t const _M0L5_2aokS2807 = _tmp_3918.data.ok;
  } else {
    void* const _M0L6_2aerrS2808 = _tmp_3918.data.err;
    struct moonbit_result_0 _result_3919;
    _result_3919.tag = 0;
    _result_3919.data.err = _M0L6_2aerrS2808;
    return _result_3919;
  }
  _M0L14_2aboxed__selfS2819
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2819)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2819->$0 = 404;
  _M0L6_2atmpS2809
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2819
  };
  _M0L6_2atmpS2818 = 0;
  #line 29 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2817 = _M0MPC14json4Json6number(0x1.94p+8, _M0L6_2atmpS2818);
  _M0L6_2atmpS2810 = _M0L6_2atmpS2817;
  _M0L6_2atmpS2813 = (moonbit_string_t)moonbit_string_literal_93.data;
  _M0L6_2atmpS2814 = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS2815 = 0;
  _M0L6_2atmpS2816 = 0;
  _M0L6_2atmpS2812 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2812[0] = _M0L6_2atmpS2813;
  _M0L6_2atmpS2812[1] = _M0L6_2atmpS2814;
  _M0L6_2atmpS2812[2] = _M0L6_2atmpS2815;
  _M0L6_2atmpS2812[3] = _M0L6_2atmpS2816;
  _M0L6_2atmpS2811
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2811)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2811->$0 = _M0L6_2atmpS2812;
  _M0L6_2atmpS2811->$1 = 4;
  #line 29 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3920
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2809, _M0L6_2atmpS2810, (moonbit_string_t)moonbit_string_literal_95.data, _M0L6_2atmpS2811);
  if (_tmp_3920.tag) {
    int32_t const _M0L5_2aokS2820 = _tmp_3920.data.ok;
  } else {
    void* const _M0L6_2aerrS2821 = _tmp_3920.data.err;
    struct moonbit_result_0 _result_3921;
    _result_3921.tag = 0;
    _result_3921.data.err = _M0L6_2aerrS2821;
    return _result_3921;
  }
  _M0L14_2aboxed__selfS2832
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2832)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2832->$0 = 405;
  _M0L6_2atmpS2822
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2832
  };
  _M0L6_2atmpS2831 = 0;
  #line 30 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2830 = _M0MPC14json4Json6number(0x1.95p+8, _M0L6_2atmpS2831);
  _M0L6_2atmpS2823 = _M0L6_2atmpS2830;
  _M0L6_2atmpS2826 = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L6_2atmpS2827 = (moonbit_string_t)moonbit_string_literal_97.data;
  _M0L6_2atmpS2828 = 0;
  _M0L6_2atmpS2829 = 0;
  _M0L6_2atmpS2825 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2825[0] = _M0L6_2atmpS2826;
  _M0L6_2atmpS2825[1] = _M0L6_2atmpS2827;
  _M0L6_2atmpS2825[2] = _M0L6_2atmpS2828;
  _M0L6_2atmpS2825[3] = _M0L6_2atmpS2829;
  _M0L6_2atmpS2824
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2824)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2824->$0 = _M0L6_2atmpS2825;
  _M0L6_2atmpS2824->$1 = 4;
  #line 30 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3922
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2822, _M0L6_2atmpS2823, (moonbit_string_t)moonbit_string_literal_98.data, _M0L6_2atmpS2824);
  if (_tmp_3922.tag) {
    int32_t const _M0L5_2aokS2833 = _tmp_3922.data.ok;
  } else {
    void* const _M0L6_2aerrS2834 = _tmp_3922.data.err;
    struct moonbit_result_0 _result_3923;
    _result_3923.tag = 0;
    _result_3923.data.err = _M0L6_2aerrS2834;
    return _result_3923;
  }
  _M0L14_2aboxed__selfS2845
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2845)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2845->$0 = 406;
  _M0L6_2atmpS2835
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2845
  };
  _M0L6_2atmpS2844 = 0;
  #line 31 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2843 = _M0MPC14json4Json6number(0x1.96p+8, _M0L6_2atmpS2844);
  _M0L6_2atmpS2836 = _M0L6_2atmpS2843;
  _M0L6_2atmpS2839 = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L6_2atmpS2840 = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L6_2atmpS2841 = 0;
  _M0L6_2atmpS2842 = 0;
  _M0L6_2atmpS2838 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2838[0] = _M0L6_2atmpS2839;
  _M0L6_2atmpS2838[1] = _M0L6_2atmpS2840;
  _M0L6_2atmpS2838[2] = _M0L6_2atmpS2841;
  _M0L6_2atmpS2838[3] = _M0L6_2atmpS2842;
  _M0L6_2atmpS2837
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2837)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2837->$0 = _M0L6_2atmpS2838;
  _M0L6_2atmpS2837->$1 = 4;
  #line 31 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3924
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2835, _M0L6_2atmpS2836, (moonbit_string_t)moonbit_string_literal_101.data, _M0L6_2atmpS2837);
  if (_tmp_3924.tag) {
    int32_t const _M0L5_2aokS2846 = _tmp_3924.data.ok;
  } else {
    void* const _M0L6_2aerrS2847 = _tmp_3924.data.err;
    struct moonbit_result_0 _result_3925;
    _result_3925.tag = 0;
    _result_3925.data.err = _M0L6_2aerrS2847;
    return _result_3925;
  }
  _M0L14_2aboxed__selfS2858
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2858)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2858->$0 = 407;
  _M0L6_2atmpS2848
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2858
  };
  _M0L6_2atmpS2857 = 0;
  #line 32 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2856 = _M0MPC14json4Json6number(0x1.97p+8, _M0L6_2atmpS2857);
  _M0L6_2atmpS2849 = _M0L6_2atmpS2856;
  _M0L6_2atmpS2852 = (moonbit_string_t)moonbit_string_literal_102.data;
  _M0L6_2atmpS2853 = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L6_2atmpS2854 = 0;
  _M0L6_2atmpS2855 = 0;
  _M0L6_2atmpS2851 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2851[0] = _M0L6_2atmpS2852;
  _M0L6_2atmpS2851[1] = _M0L6_2atmpS2853;
  _M0L6_2atmpS2851[2] = _M0L6_2atmpS2854;
  _M0L6_2atmpS2851[3] = _M0L6_2atmpS2855;
  _M0L6_2atmpS2850
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2850)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2850->$0 = _M0L6_2atmpS2851;
  _M0L6_2atmpS2850->$1 = 4;
  #line 32 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3926
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2848, _M0L6_2atmpS2849, (moonbit_string_t)moonbit_string_literal_104.data, _M0L6_2atmpS2850);
  if (_tmp_3926.tag) {
    int32_t const _M0L5_2aokS2859 = _tmp_3926.data.ok;
  } else {
    void* const _M0L6_2aerrS2860 = _tmp_3926.data.err;
    struct moonbit_result_0 _result_3927;
    _result_3927.tag = 0;
    _result_3927.data.err = _M0L6_2aerrS2860;
    return _result_3927;
  }
  _M0L14_2aboxed__selfS2871
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2871)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2871->$0 = 408;
  _M0L6_2atmpS2861
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2871
  };
  _M0L6_2atmpS2870 = 0;
  #line 33 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2869 = _M0MPC14json4Json6number(0x1.98p+8, _M0L6_2atmpS2870);
  _M0L6_2atmpS2862 = _M0L6_2atmpS2869;
  _M0L6_2atmpS2865 = (moonbit_string_t)moonbit_string_literal_105.data;
  _M0L6_2atmpS2866 = (moonbit_string_t)moonbit_string_literal_106.data;
  _M0L6_2atmpS2867 = 0;
  _M0L6_2atmpS2868 = 0;
  _M0L6_2atmpS2864 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2864[0] = _M0L6_2atmpS2865;
  _M0L6_2atmpS2864[1] = _M0L6_2atmpS2866;
  _M0L6_2atmpS2864[2] = _M0L6_2atmpS2867;
  _M0L6_2atmpS2864[3] = _M0L6_2atmpS2868;
  _M0L6_2atmpS2863
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2863)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2863->$0 = _M0L6_2atmpS2864;
  _M0L6_2atmpS2863->$1 = 4;
  #line 33 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3928
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2861, _M0L6_2atmpS2862, (moonbit_string_t)moonbit_string_literal_107.data, _M0L6_2atmpS2863);
  if (_tmp_3928.tag) {
    int32_t const _M0L5_2aokS2872 = _tmp_3928.data.ok;
  } else {
    void* const _M0L6_2aerrS2873 = _tmp_3928.data.err;
    struct moonbit_result_0 _result_3929;
    _result_3929.tag = 0;
    _result_3929.data.err = _M0L6_2aerrS2873;
    return _result_3929;
  }
  _M0L14_2aboxed__selfS2884
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2884)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2884->$0 = 409;
  _M0L6_2atmpS2874
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2884
  };
  _M0L6_2atmpS2883 = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2882 = _M0MPC14json4Json6number(0x1.99p+8, _M0L6_2atmpS2883);
  _M0L6_2atmpS2875 = _M0L6_2atmpS2882;
  _M0L6_2atmpS2878 = (moonbit_string_t)moonbit_string_literal_108.data;
  _M0L6_2atmpS2879 = (moonbit_string_t)moonbit_string_literal_109.data;
  _M0L6_2atmpS2880 = 0;
  _M0L6_2atmpS2881 = 0;
  _M0L6_2atmpS2877 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2877[0] = _M0L6_2atmpS2878;
  _M0L6_2atmpS2877[1] = _M0L6_2atmpS2879;
  _M0L6_2atmpS2877[2] = _M0L6_2atmpS2880;
  _M0L6_2atmpS2877[3] = _M0L6_2atmpS2881;
  _M0L6_2atmpS2876
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2876)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2876->$0 = _M0L6_2atmpS2877;
  _M0L6_2atmpS2876->$1 = 4;
  #line 34 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3930
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2874, _M0L6_2atmpS2875, (moonbit_string_t)moonbit_string_literal_110.data, _M0L6_2atmpS2876);
  if (_tmp_3930.tag) {
    int32_t const _M0L5_2aokS2885 = _tmp_3930.data.ok;
  } else {
    void* const _M0L6_2aerrS2886 = _tmp_3930.data.err;
    struct moonbit_result_0 _result_3931;
    _result_3931.tag = 0;
    _result_3931.data.err = _M0L6_2aerrS2886;
    return _result_3931;
  }
  _M0L14_2aboxed__selfS2897
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2897)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2897->$0 = 410;
  _M0L6_2atmpS2887
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2897
  };
  _M0L6_2atmpS2896 = 0;
  #line 35 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2895 = _M0MPC14json4Json6number(0x1.9ap+8, _M0L6_2atmpS2896);
  _M0L6_2atmpS2888 = _M0L6_2atmpS2895;
  _M0L6_2atmpS2891 = (moonbit_string_t)moonbit_string_literal_111.data;
  _M0L6_2atmpS2892 = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L6_2atmpS2893 = 0;
  _M0L6_2atmpS2894 = 0;
  _M0L6_2atmpS2890 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2890[0] = _M0L6_2atmpS2891;
  _M0L6_2atmpS2890[1] = _M0L6_2atmpS2892;
  _M0L6_2atmpS2890[2] = _M0L6_2atmpS2893;
  _M0L6_2atmpS2890[3] = _M0L6_2atmpS2894;
  _M0L6_2atmpS2889
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2889)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2889->$0 = _M0L6_2atmpS2890;
  _M0L6_2atmpS2889->$1 = 4;
  #line 35 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3932
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2887, _M0L6_2atmpS2888, (moonbit_string_t)moonbit_string_literal_113.data, _M0L6_2atmpS2889);
  if (_tmp_3932.tag) {
    int32_t const _M0L5_2aokS2898 = _tmp_3932.data.ok;
  } else {
    void* const _M0L6_2aerrS2899 = _tmp_3932.data.err;
    struct moonbit_result_0 _result_3933;
    _result_3933.tag = 0;
    _result_3933.data.err = _M0L6_2aerrS2899;
    return _result_3933;
  }
  _M0L14_2aboxed__selfS2910
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2910)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2910->$0 = 411;
  _M0L6_2atmpS2900
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2910
  };
  _M0L6_2atmpS2909 = 0;
  #line 36 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2908 = _M0MPC14json4Json6number(0x1.9bp+8, _M0L6_2atmpS2909);
  _M0L6_2atmpS2901 = _M0L6_2atmpS2908;
  _M0L6_2atmpS2904 = (moonbit_string_t)moonbit_string_literal_114.data;
  _M0L6_2atmpS2905 = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L6_2atmpS2906 = 0;
  _M0L6_2atmpS2907 = 0;
  _M0L6_2atmpS2903 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2903[0] = _M0L6_2atmpS2904;
  _M0L6_2atmpS2903[1] = _M0L6_2atmpS2905;
  _M0L6_2atmpS2903[2] = _M0L6_2atmpS2906;
  _M0L6_2atmpS2903[3] = _M0L6_2atmpS2907;
  _M0L6_2atmpS2902
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2902)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2902->$0 = _M0L6_2atmpS2903;
  _M0L6_2atmpS2902->$1 = 4;
  #line 36 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3934
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2900, _M0L6_2atmpS2901, (moonbit_string_t)moonbit_string_literal_116.data, _M0L6_2atmpS2902);
  if (_tmp_3934.tag) {
    int32_t const _M0L5_2aokS2911 = _tmp_3934.data.ok;
  } else {
    void* const _M0L6_2aerrS2912 = _tmp_3934.data.err;
    struct moonbit_result_0 _result_3935;
    _result_3935.tag = 0;
    _result_3935.data.err = _M0L6_2aerrS2912;
    return _result_3935;
  }
  _M0L14_2aboxed__selfS2923
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2923)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2923->$0 = 412;
  _M0L6_2atmpS2913
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2923
  };
  _M0L6_2atmpS2922 = 0;
  #line 37 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2921 = _M0MPC14json4Json6number(0x1.9cp+8, _M0L6_2atmpS2922);
  _M0L6_2atmpS2914 = _M0L6_2atmpS2921;
  _M0L6_2atmpS2917 = (moonbit_string_t)moonbit_string_literal_117.data;
  _M0L6_2atmpS2918 = (moonbit_string_t)moonbit_string_literal_118.data;
  _M0L6_2atmpS2919 = 0;
  _M0L6_2atmpS2920 = 0;
  _M0L6_2atmpS2916 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2916[0] = _M0L6_2atmpS2917;
  _M0L6_2atmpS2916[1] = _M0L6_2atmpS2918;
  _M0L6_2atmpS2916[2] = _M0L6_2atmpS2919;
  _M0L6_2atmpS2916[3] = _M0L6_2atmpS2920;
  _M0L6_2atmpS2915
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2915)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2915->$0 = _M0L6_2atmpS2916;
  _M0L6_2atmpS2915->$1 = 4;
  #line 37 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3936
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2913, _M0L6_2atmpS2914, (moonbit_string_t)moonbit_string_literal_119.data, _M0L6_2atmpS2915);
  if (_tmp_3936.tag) {
    int32_t const _M0L5_2aokS2924 = _tmp_3936.data.ok;
  } else {
    void* const _M0L6_2aerrS2925 = _tmp_3936.data.err;
    struct moonbit_result_0 _result_3937;
    _result_3937.tag = 0;
    _result_3937.data.err = _M0L6_2aerrS2925;
    return _result_3937;
  }
  _M0L14_2aboxed__selfS2936
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2936)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2936->$0 = 413;
  _M0L6_2atmpS2926
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2936
  };
  _M0L6_2atmpS2935 = 0;
  #line 38 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2934 = _M0MPC14json4Json6number(0x1.9dp+8, _M0L6_2atmpS2935);
  _M0L6_2atmpS2927 = _M0L6_2atmpS2934;
  _M0L6_2atmpS2930 = (moonbit_string_t)moonbit_string_literal_120.data;
  _M0L6_2atmpS2931 = (moonbit_string_t)moonbit_string_literal_121.data;
  _M0L6_2atmpS2932 = 0;
  _M0L6_2atmpS2933 = 0;
  _M0L6_2atmpS2929 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2929[0] = _M0L6_2atmpS2930;
  _M0L6_2atmpS2929[1] = _M0L6_2atmpS2931;
  _M0L6_2atmpS2929[2] = _M0L6_2atmpS2932;
  _M0L6_2atmpS2929[3] = _M0L6_2atmpS2933;
  _M0L6_2atmpS2928
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2928)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2928->$0 = _M0L6_2atmpS2929;
  _M0L6_2atmpS2928->$1 = 4;
  #line 38 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3938
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2926, _M0L6_2atmpS2927, (moonbit_string_t)moonbit_string_literal_122.data, _M0L6_2atmpS2928);
  if (_tmp_3938.tag) {
    int32_t const _M0L5_2aokS2937 = _tmp_3938.data.ok;
  } else {
    void* const _M0L6_2aerrS2938 = _tmp_3938.data.err;
    struct moonbit_result_0 _result_3939;
    _result_3939.tag = 0;
    _result_3939.data.err = _M0L6_2aerrS2938;
    return _result_3939;
  }
  _M0L14_2aboxed__selfS2949
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2949)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2949->$0 = 414;
  _M0L6_2atmpS2939
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2949
  };
  _M0L6_2atmpS2948 = 0;
  #line 39 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2947 = _M0MPC14json4Json6number(0x1.9ep+8, _M0L6_2atmpS2948);
  _M0L6_2atmpS2940 = _M0L6_2atmpS2947;
  _M0L6_2atmpS2943 = (moonbit_string_t)moonbit_string_literal_123.data;
  _M0L6_2atmpS2944 = (moonbit_string_t)moonbit_string_literal_124.data;
  _M0L6_2atmpS2945 = 0;
  _M0L6_2atmpS2946 = 0;
  _M0L6_2atmpS2942 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2942[0] = _M0L6_2atmpS2943;
  _M0L6_2atmpS2942[1] = _M0L6_2atmpS2944;
  _M0L6_2atmpS2942[2] = _M0L6_2atmpS2945;
  _M0L6_2atmpS2942[3] = _M0L6_2atmpS2946;
  _M0L6_2atmpS2941
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2941)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2941->$0 = _M0L6_2atmpS2942;
  _M0L6_2atmpS2941->$1 = 4;
  #line 39 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3940
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2939, _M0L6_2atmpS2940, (moonbit_string_t)moonbit_string_literal_125.data, _M0L6_2atmpS2941);
  if (_tmp_3940.tag) {
    int32_t const _M0L5_2aokS2950 = _tmp_3940.data.ok;
  } else {
    void* const _M0L6_2aerrS2951 = _tmp_3940.data.err;
    struct moonbit_result_0 _result_3941;
    _result_3941.tag = 0;
    _result_3941.data.err = _M0L6_2aerrS2951;
    return _result_3941;
  }
  _M0L14_2aboxed__selfS2962
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2962)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2962->$0 = 415;
  _M0L6_2atmpS2952
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2962
  };
  _M0L6_2atmpS2961 = 0;
  #line 40 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2960 = _M0MPC14json4Json6number(0x1.9fp+8, _M0L6_2atmpS2961);
  _M0L6_2atmpS2953 = _M0L6_2atmpS2960;
  _M0L6_2atmpS2956 = (moonbit_string_t)moonbit_string_literal_126.data;
  _M0L6_2atmpS2957 = (moonbit_string_t)moonbit_string_literal_127.data;
  _M0L6_2atmpS2958 = 0;
  _M0L6_2atmpS2959 = 0;
  _M0L6_2atmpS2955 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2955[0] = _M0L6_2atmpS2956;
  _M0L6_2atmpS2955[1] = _M0L6_2atmpS2957;
  _M0L6_2atmpS2955[2] = _M0L6_2atmpS2958;
  _M0L6_2atmpS2955[3] = _M0L6_2atmpS2959;
  _M0L6_2atmpS2954
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2954)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2954->$0 = _M0L6_2atmpS2955;
  _M0L6_2atmpS2954->$1 = 4;
  #line 40 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3942
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2952, _M0L6_2atmpS2953, (moonbit_string_t)moonbit_string_literal_128.data, _M0L6_2atmpS2954);
  if (_tmp_3942.tag) {
    int32_t const _M0L5_2aokS2963 = _tmp_3942.data.ok;
  } else {
    void* const _M0L6_2aerrS2964 = _tmp_3942.data.err;
    struct moonbit_result_0 _result_3943;
    _result_3943.tag = 0;
    _result_3943.data.err = _M0L6_2aerrS2964;
    return _result_3943;
  }
  _M0L14_2aboxed__selfS2975
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2975)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2975->$0 = 416;
  _M0L6_2atmpS2965
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2975
  };
  _M0L6_2atmpS2974 = 0;
  #line 41 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2973 = _M0MPC14json4Json6number(0x1.ap+8, _M0L6_2atmpS2974);
  _M0L6_2atmpS2966 = _M0L6_2atmpS2973;
  _M0L6_2atmpS2969 = (moonbit_string_t)moonbit_string_literal_129.data;
  _M0L6_2atmpS2970 = (moonbit_string_t)moonbit_string_literal_130.data;
  _M0L6_2atmpS2971 = 0;
  _M0L6_2atmpS2972 = 0;
  _M0L6_2atmpS2968 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2968[0] = _M0L6_2atmpS2969;
  _M0L6_2atmpS2968[1] = _M0L6_2atmpS2970;
  _M0L6_2atmpS2968[2] = _M0L6_2atmpS2971;
  _M0L6_2atmpS2968[3] = _M0L6_2atmpS2972;
  _M0L6_2atmpS2967
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2967)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2967->$0 = _M0L6_2atmpS2968;
  _M0L6_2atmpS2967->$1 = 4;
  #line 41 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3944
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2965, _M0L6_2atmpS2966, (moonbit_string_t)moonbit_string_literal_131.data, _M0L6_2atmpS2967);
  if (_tmp_3944.tag) {
    int32_t const _M0L5_2aokS2976 = _tmp_3944.data.ok;
  } else {
    void* const _M0L6_2aerrS2977 = _tmp_3944.data.err;
    struct moonbit_result_0 _result_3945;
    _result_3945.tag = 0;
    _result_3945.data.err = _M0L6_2aerrS2977;
    return _result_3945;
  }
  _M0L14_2aboxed__selfS2988
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2988)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2988->$0 = 417;
  _M0L6_2atmpS2978
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2988
  };
  _M0L6_2atmpS2987 = 0;
  #line 42 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2986 = _M0MPC14json4Json6number(0x1.a1p+8, _M0L6_2atmpS2987);
  _M0L6_2atmpS2979 = _M0L6_2atmpS2986;
  _M0L6_2atmpS2982 = (moonbit_string_t)moonbit_string_literal_132.data;
  _M0L6_2atmpS2983 = (moonbit_string_t)moonbit_string_literal_133.data;
  _M0L6_2atmpS2984 = 0;
  _M0L6_2atmpS2985 = 0;
  _M0L6_2atmpS2981 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2981[0] = _M0L6_2atmpS2982;
  _M0L6_2atmpS2981[1] = _M0L6_2atmpS2983;
  _M0L6_2atmpS2981[2] = _M0L6_2atmpS2984;
  _M0L6_2atmpS2981[3] = _M0L6_2atmpS2985;
  _M0L6_2atmpS2980
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2980)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2980->$0 = _M0L6_2atmpS2981;
  _M0L6_2atmpS2980->$1 = 4;
  #line 42 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3946
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2978, _M0L6_2atmpS2979, (moonbit_string_t)moonbit_string_literal_134.data, _M0L6_2atmpS2980);
  if (_tmp_3946.tag) {
    int32_t const _M0L5_2aokS2989 = _tmp_3946.data.ok;
  } else {
    void* const _M0L6_2aerrS2990 = _tmp_3946.data.err;
    struct moonbit_result_0 _result_3947;
    _result_3947.tag = 0;
    _result_3947.data.err = _M0L6_2aerrS2990;
    return _result_3947;
  }
  _M0L14_2aboxed__selfS3001
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3001)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3001->$0 = 418;
  _M0L6_2atmpS2991
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3001
  };
  _M0L6_2atmpS3000 = 0;
  #line 43 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS2999 = _M0MPC14json4Json6number(0x1.a2p+8, _M0L6_2atmpS3000);
  _M0L6_2atmpS2992 = _M0L6_2atmpS2999;
  _M0L6_2atmpS2995 = (moonbit_string_t)moonbit_string_literal_135.data;
  _M0L6_2atmpS2996 = (moonbit_string_t)moonbit_string_literal_136.data;
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
  #line 43 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3948
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2991, _M0L6_2atmpS2992, (moonbit_string_t)moonbit_string_literal_137.data, _M0L6_2atmpS2993);
  if (_tmp_3948.tag) {
    int32_t const _M0L5_2aokS3002 = _tmp_3948.data.ok;
  } else {
    void* const _M0L6_2aerrS3003 = _tmp_3948.data.err;
    struct moonbit_result_0 _result_3949;
    _result_3949.tag = 0;
    _result_3949.data.err = _M0L6_2aerrS3003;
    return _result_3949;
  }
  _M0L14_2aboxed__selfS3014
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3014)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3014->$0 = 421;
  _M0L6_2atmpS3004
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3014
  };
  _M0L6_2atmpS3013 = 0;
  #line 44 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3012 = _M0MPC14json4Json6number(0x1.a5p+8, _M0L6_2atmpS3013);
  _M0L6_2atmpS3005 = _M0L6_2atmpS3012;
  _M0L6_2atmpS3008 = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L6_2atmpS3009 = (moonbit_string_t)moonbit_string_literal_139.data;
  _M0L6_2atmpS3010 = 0;
  _M0L6_2atmpS3011 = 0;
  _M0L6_2atmpS3007 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3007[0] = _M0L6_2atmpS3008;
  _M0L6_2atmpS3007[1] = _M0L6_2atmpS3009;
  _M0L6_2atmpS3007[2] = _M0L6_2atmpS3010;
  _M0L6_2atmpS3007[3] = _M0L6_2atmpS3011;
  _M0L6_2atmpS3006
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3006)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3006->$0 = _M0L6_2atmpS3007;
  _M0L6_2atmpS3006->$1 = 4;
  #line 44 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3950
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3004, _M0L6_2atmpS3005, (moonbit_string_t)moonbit_string_literal_140.data, _M0L6_2atmpS3006);
  if (_tmp_3950.tag) {
    int32_t const _M0L5_2aokS3015 = _tmp_3950.data.ok;
  } else {
    void* const _M0L6_2aerrS3016 = _tmp_3950.data.err;
    struct moonbit_result_0 _result_3951;
    _result_3951.tag = 0;
    _result_3951.data.err = _M0L6_2aerrS3016;
    return _result_3951;
  }
  _M0L14_2aboxed__selfS3027
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3027)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3027->$0 = 422;
  _M0L6_2atmpS3017
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3027
  };
  _M0L6_2atmpS3026 = 0;
  #line 45 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3025 = _M0MPC14json4Json6number(0x1.a6p+8, _M0L6_2atmpS3026);
  _M0L6_2atmpS3018 = _M0L6_2atmpS3025;
  _M0L6_2atmpS3021 = (moonbit_string_t)moonbit_string_literal_141.data;
  _M0L6_2atmpS3022 = (moonbit_string_t)moonbit_string_literal_142.data;
  _M0L6_2atmpS3023 = 0;
  _M0L6_2atmpS3024 = 0;
  _M0L6_2atmpS3020 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3020[0] = _M0L6_2atmpS3021;
  _M0L6_2atmpS3020[1] = _M0L6_2atmpS3022;
  _M0L6_2atmpS3020[2] = _M0L6_2atmpS3023;
  _M0L6_2atmpS3020[3] = _M0L6_2atmpS3024;
  _M0L6_2atmpS3019
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3019)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3019->$0 = _M0L6_2atmpS3020;
  _M0L6_2atmpS3019->$1 = 4;
  #line 45 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3952
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3017, _M0L6_2atmpS3018, (moonbit_string_t)moonbit_string_literal_143.data, _M0L6_2atmpS3019);
  if (_tmp_3952.tag) {
    int32_t const _M0L5_2aokS3028 = _tmp_3952.data.ok;
  } else {
    void* const _M0L6_2aerrS3029 = _tmp_3952.data.err;
    struct moonbit_result_0 _result_3953;
    _result_3953.tag = 0;
    _result_3953.data.err = _M0L6_2aerrS3029;
    return _result_3953;
  }
  _M0L14_2aboxed__selfS3040
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3040)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3040->$0 = 423;
  _M0L6_2atmpS3030
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3040
  };
  _M0L6_2atmpS3039 = 0;
  #line 46 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3038 = _M0MPC14json4Json6number(0x1.a7p+8, _M0L6_2atmpS3039);
  _M0L6_2atmpS3031 = _M0L6_2atmpS3038;
  _M0L6_2atmpS3034 = (moonbit_string_t)moonbit_string_literal_144.data;
  _M0L6_2atmpS3035 = (moonbit_string_t)moonbit_string_literal_145.data;
  _M0L6_2atmpS3036 = 0;
  _M0L6_2atmpS3037 = 0;
  _M0L6_2atmpS3033 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3033[0] = _M0L6_2atmpS3034;
  _M0L6_2atmpS3033[1] = _M0L6_2atmpS3035;
  _M0L6_2atmpS3033[2] = _M0L6_2atmpS3036;
  _M0L6_2atmpS3033[3] = _M0L6_2atmpS3037;
  _M0L6_2atmpS3032
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3032)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3032->$0 = _M0L6_2atmpS3033;
  _M0L6_2atmpS3032->$1 = 4;
  #line 46 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3954
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3030, _M0L6_2atmpS3031, (moonbit_string_t)moonbit_string_literal_146.data, _M0L6_2atmpS3032);
  if (_tmp_3954.tag) {
    int32_t const _M0L5_2aokS3041 = _tmp_3954.data.ok;
  } else {
    void* const _M0L6_2aerrS3042 = _tmp_3954.data.err;
    struct moonbit_result_0 _result_3955;
    _result_3955.tag = 0;
    _result_3955.data.err = _M0L6_2aerrS3042;
    return _result_3955;
  }
  _M0L14_2aboxed__selfS3053
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3053)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3053->$0 = 424;
  _M0L6_2atmpS3043
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3053
  };
  _M0L6_2atmpS3052 = 0;
  #line 47 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3051 = _M0MPC14json4Json6number(0x1.a8p+8, _M0L6_2atmpS3052);
  _M0L6_2atmpS3044 = _M0L6_2atmpS3051;
  _M0L6_2atmpS3047 = (moonbit_string_t)moonbit_string_literal_147.data;
  _M0L6_2atmpS3048 = (moonbit_string_t)moonbit_string_literal_148.data;
  _M0L6_2atmpS3049 = 0;
  _M0L6_2atmpS3050 = 0;
  _M0L6_2atmpS3046 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3046[0] = _M0L6_2atmpS3047;
  _M0L6_2atmpS3046[1] = _M0L6_2atmpS3048;
  _M0L6_2atmpS3046[2] = _M0L6_2atmpS3049;
  _M0L6_2atmpS3046[3] = _M0L6_2atmpS3050;
  _M0L6_2atmpS3045
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3045)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3045->$0 = _M0L6_2atmpS3046;
  _M0L6_2atmpS3045->$1 = 4;
  #line 47 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3956
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3043, _M0L6_2atmpS3044, (moonbit_string_t)moonbit_string_literal_149.data, _M0L6_2atmpS3045);
  if (_tmp_3956.tag) {
    int32_t const _M0L5_2aokS3054 = _tmp_3956.data.ok;
  } else {
    void* const _M0L6_2aerrS3055 = _tmp_3956.data.err;
    struct moonbit_result_0 _result_3957;
    _result_3957.tag = 0;
    _result_3957.data.err = _M0L6_2aerrS3055;
    return _result_3957;
  }
  _M0L14_2aboxed__selfS3066
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3066)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3066->$0 = 425;
  _M0L6_2atmpS3056
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3066
  };
  _M0L6_2atmpS3065 = 0;
  #line 48 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3064 = _M0MPC14json4Json6number(0x1.a9p+8, _M0L6_2atmpS3065);
  _M0L6_2atmpS3057 = _M0L6_2atmpS3064;
  _M0L6_2atmpS3060 = (moonbit_string_t)moonbit_string_literal_150.data;
  _M0L6_2atmpS3061 = (moonbit_string_t)moonbit_string_literal_151.data;
  _M0L6_2atmpS3062 = 0;
  _M0L6_2atmpS3063 = 0;
  _M0L6_2atmpS3059 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3059[0] = _M0L6_2atmpS3060;
  _M0L6_2atmpS3059[1] = _M0L6_2atmpS3061;
  _M0L6_2atmpS3059[2] = _M0L6_2atmpS3062;
  _M0L6_2atmpS3059[3] = _M0L6_2atmpS3063;
  _M0L6_2atmpS3058
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3058)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3058->$0 = _M0L6_2atmpS3059;
  _M0L6_2atmpS3058->$1 = 4;
  #line 48 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3958
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3056, _M0L6_2atmpS3057, (moonbit_string_t)moonbit_string_literal_152.data, _M0L6_2atmpS3058);
  if (_tmp_3958.tag) {
    int32_t const _M0L5_2aokS3067 = _tmp_3958.data.ok;
  } else {
    void* const _M0L6_2aerrS3068 = _tmp_3958.data.err;
    struct moonbit_result_0 _result_3959;
    _result_3959.tag = 0;
    _result_3959.data.err = _M0L6_2aerrS3068;
    return _result_3959;
  }
  _M0L14_2aboxed__selfS3079
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3079)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3079->$0 = 426;
  _M0L6_2atmpS3069
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3079
  };
  _M0L6_2atmpS3078 = 0;
  #line 49 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3077 = _M0MPC14json4Json6number(0x1.aap+8, _M0L6_2atmpS3078);
  _M0L6_2atmpS3070 = _M0L6_2atmpS3077;
  _M0L6_2atmpS3073 = (moonbit_string_t)moonbit_string_literal_153.data;
  _M0L6_2atmpS3074 = (moonbit_string_t)moonbit_string_literal_154.data;
  _M0L6_2atmpS3075 = 0;
  _M0L6_2atmpS3076 = 0;
  _M0L6_2atmpS3072 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3072[0] = _M0L6_2atmpS3073;
  _M0L6_2atmpS3072[1] = _M0L6_2atmpS3074;
  _M0L6_2atmpS3072[2] = _M0L6_2atmpS3075;
  _M0L6_2atmpS3072[3] = _M0L6_2atmpS3076;
  _M0L6_2atmpS3071
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3071)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3071->$0 = _M0L6_2atmpS3072;
  _M0L6_2atmpS3071->$1 = 4;
  #line 49 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3960
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3069, _M0L6_2atmpS3070, (moonbit_string_t)moonbit_string_literal_155.data, _M0L6_2atmpS3071);
  if (_tmp_3960.tag) {
    int32_t const _M0L5_2aokS3080 = _tmp_3960.data.ok;
  } else {
    void* const _M0L6_2aerrS3081 = _tmp_3960.data.err;
    struct moonbit_result_0 _result_3961;
    _result_3961.tag = 0;
    _result_3961.data.err = _M0L6_2aerrS3081;
    return _result_3961;
  }
  _M0L14_2aboxed__selfS3092
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3092)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3092->$0 = 428;
  _M0L6_2atmpS3082
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3092
  };
  _M0L6_2atmpS3091 = 0;
  #line 50 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3090 = _M0MPC14json4Json6number(0x1.acp+8, _M0L6_2atmpS3091);
  _M0L6_2atmpS3083 = _M0L6_2atmpS3090;
  _M0L6_2atmpS3086 = (moonbit_string_t)moonbit_string_literal_156.data;
  _M0L6_2atmpS3087 = (moonbit_string_t)moonbit_string_literal_157.data;
  _M0L6_2atmpS3088 = 0;
  _M0L6_2atmpS3089 = 0;
  _M0L6_2atmpS3085 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3085[0] = _M0L6_2atmpS3086;
  _M0L6_2atmpS3085[1] = _M0L6_2atmpS3087;
  _M0L6_2atmpS3085[2] = _M0L6_2atmpS3088;
  _M0L6_2atmpS3085[3] = _M0L6_2atmpS3089;
  _M0L6_2atmpS3084
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3084)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3084->$0 = _M0L6_2atmpS3085;
  _M0L6_2atmpS3084->$1 = 4;
  #line 50 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3962
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3082, _M0L6_2atmpS3083, (moonbit_string_t)moonbit_string_literal_158.data, _M0L6_2atmpS3084);
  if (_tmp_3962.tag) {
    int32_t const _M0L5_2aokS3093 = _tmp_3962.data.ok;
  } else {
    void* const _M0L6_2aerrS3094 = _tmp_3962.data.err;
    struct moonbit_result_0 _result_3963;
    _result_3963.tag = 0;
    _result_3963.data.err = _M0L6_2aerrS3094;
    return _result_3963;
  }
  _M0L14_2aboxed__selfS3105
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3105)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3105->$0 = 429;
  _M0L6_2atmpS3095
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3105
  };
  _M0L6_2atmpS3104 = 0;
  #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3103 = _M0MPC14json4Json6number(0x1.adp+8, _M0L6_2atmpS3104);
  _M0L6_2atmpS3096 = _M0L6_2atmpS3103;
  _M0L6_2atmpS3099 = (moonbit_string_t)moonbit_string_literal_159.data;
  _M0L6_2atmpS3100 = (moonbit_string_t)moonbit_string_literal_160.data;
  _M0L6_2atmpS3101 = 0;
  _M0L6_2atmpS3102 = 0;
  _M0L6_2atmpS3098 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3098[0] = _M0L6_2atmpS3099;
  _M0L6_2atmpS3098[1] = _M0L6_2atmpS3100;
  _M0L6_2atmpS3098[2] = _M0L6_2atmpS3101;
  _M0L6_2atmpS3098[3] = _M0L6_2atmpS3102;
  _M0L6_2atmpS3097
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3097)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3097->$0 = _M0L6_2atmpS3098;
  _M0L6_2atmpS3097->$1 = 4;
  #line 51 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3964
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3095, _M0L6_2atmpS3096, (moonbit_string_t)moonbit_string_literal_161.data, _M0L6_2atmpS3097);
  if (_tmp_3964.tag) {
    int32_t const _M0L5_2aokS3106 = _tmp_3964.data.ok;
  } else {
    void* const _M0L6_2aerrS3107 = _tmp_3964.data.err;
    struct moonbit_result_0 _result_3965;
    _result_3965.tag = 0;
    _result_3965.data.err = _M0L6_2aerrS3107;
    return _result_3965;
  }
  _M0L14_2aboxed__selfS3118
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3118)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3118->$0 = 431;
  _M0L6_2atmpS3108
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3118
  };
  _M0L6_2atmpS3117 = 0;
  #line 52 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3116 = _M0MPC14json4Json6number(0x1.afp+8, _M0L6_2atmpS3117);
  _M0L6_2atmpS3109 = _M0L6_2atmpS3116;
  _M0L6_2atmpS3112 = (moonbit_string_t)moonbit_string_literal_162.data;
  _M0L6_2atmpS3113 = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS3114 = 0;
  _M0L6_2atmpS3115 = 0;
  _M0L6_2atmpS3111 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3111[0] = _M0L6_2atmpS3112;
  _M0L6_2atmpS3111[1] = _M0L6_2atmpS3113;
  _M0L6_2atmpS3111[2] = _M0L6_2atmpS3114;
  _M0L6_2atmpS3111[3] = _M0L6_2atmpS3115;
  _M0L6_2atmpS3110
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3110)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3110->$0 = _M0L6_2atmpS3111;
  _M0L6_2atmpS3110->$1 = 4;
  #line 52 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3966
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3108, _M0L6_2atmpS3109, (moonbit_string_t)moonbit_string_literal_164.data, _M0L6_2atmpS3110);
  if (_tmp_3966.tag) {
    int32_t const _M0L5_2aokS3119 = _tmp_3966.data.ok;
  } else {
    void* const _M0L6_2aerrS3120 = _tmp_3966.data.err;
    struct moonbit_result_0 _result_3967;
    _result_3967.tag = 0;
    _result_3967.data.err = _M0L6_2aerrS3120;
    return _result_3967;
  }
  _M0L14_2aboxed__selfS3131
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3131)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3131->$0 = 451;
  _M0L6_2atmpS3121
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3131
  };
  _M0L6_2atmpS3130 = 0;
  #line 53 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3129 = _M0MPC14json4Json6number(0x1.c3p+8, _M0L6_2atmpS3130);
  _M0L6_2atmpS3122 = _M0L6_2atmpS3129;
  _M0L6_2atmpS3125 = (moonbit_string_t)moonbit_string_literal_165.data;
  _M0L6_2atmpS3126 = (moonbit_string_t)moonbit_string_literal_166.data;
  _M0L6_2atmpS3127 = 0;
  _M0L6_2atmpS3128 = 0;
  _M0L6_2atmpS3124 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3124[0] = _M0L6_2atmpS3125;
  _M0L6_2atmpS3124[1] = _M0L6_2atmpS3126;
  _M0L6_2atmpS3124[2] = _M0L6_2atmpS3127;
  _M0L6_2atmpS3124[3] = _M0L6_2atmpS3128;
  _M0L6_2atmpS3123
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3123)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3123->$0 = _M0L6_2atmpS3124;
  _M0L6_2atmpS3123->$1 = 4;
  #line 53 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3968
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3121, _M0L6_2atmpS3122, (moonbit_string_t)moonbit_string_literal_167.data, _M0L6_2atmpS3123);
  if (_tmp_3968.tag) {
    int32_t const _M0L5_2aokS3132 = _tmp_3968.data.ok;
  } else {
    void* const _M0L6_2aerrS3133 = _tmp_3968.data.err;
    struct moonbit_result_0 _result_3969;
    _result_3969.tag = 0;
    _result_3969.data.err = _M0L6_2aerrS3133;
    return _result_3969;
  }
  _M0L14_2aboxed__selfS3144
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3144)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3144->$0 = 500;
  _M0L6_2atmpS3134
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3144
  };
  _M0L6_2atmpS3143 = 0;
  #line 54 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3142 = _M0MPC14json4Json6number(0x1.f4p+8, _M0L6_2atmpS3143);
  _M0L6_2atmpS3135 = _M0L6_2atmpS3142;
  _M0L6_2atmpS3138 = (moonbit_string_t)moonbit_string_literal_168.data;
  _M0L6_2atmpS3139 = (moonbit_string_t)moonbit_string_literal_169.data;
  _M0L6_2atmpS3140 = 0;
  _M0L6_2atmpS3141 = 0;
  _M0L6_2atmpS3137 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3137[0] = _M0L6_2atmpS3138;
  _M0L6_2atmpS3137[1] = _M0L6_2atmpS3139;
  _M0L6_2atmpS3137[2] = _M0L6_2atmpS3140;
  _M0L6_2atmpS3137[3] = _M0L6_2atmpS3141;
  _M0L6_2atmpS3136
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3136)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3136->$0 = _M0L6_2atmpS3137;
  _M0L6_2atmpS3136->$1 = 4;
  #line 54 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3970
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3134, _M0L6_2atmpS3135, (moonbit_string_t)moonbit_string_literal_170.data, _M0L6_2atmpS3136);
  if (_tmp_3970.tag) {
    int32_t const _M0L5_2aokS3145 = _tmp_3970.data.ok;
  } else {
    void* const _M0L6_2aerrS3146 = _tmp_3970.data.err;
    struct moonbit_result_0 _result_3971;
    _result_3971.tag = 0;
    _result_3971.data.err = _M0L6_2aerrS3146;
    return _result_3971;
  }
  _M0L14_2aboxed__selfS3157
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3157)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3157->$0 = 501;
  _M0L6_2atmpS3147
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3157
  };
  _M0L6_2atmpS3156 = 0;
  #line 55 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3155 = _M0MPC14json4Json6number(0x1.f5p+8, _M0L6_2atmpS3156);
  _M0L6_2atmpS3148 = _M0L6_2atmpS3155;
  _M0L6_2atmpS3151 = (moonbit_string_t)moonbit_string_literal_171.data;
  _M0L6_2atmpS3152 = (moonbit_string_t)moonbit_string_literal_172.data;
  _M0L6_2atmpS3153 = 0;
  _M0L6_2atmpS3154 = 0;
  _M0L6_2atmpS3150 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3150[0] = _M0L6_2atmpS3151;
  _M0L6_2atmpS3150[1] = _M0L6_2atmpS3152;
  _M0L6_2atmpS3150[2] = _M0L6_2atmpS3153;
  _M0L6_2atmpS3150[3] = _M0L6_2atmpS3154;
  _M0L6_2atmpS3149
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3149)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3149->$0 = _M0L6_2atmpS3150;
  _M0L6_2atmpS3149->$1 = 4;
  #line 55 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3972
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3147, _M0L6_2atmpS3148, (moonbit_string_t)moonbit_string_literal_173.data, _M0L6_2atmpS3149);
  if (_tmp_3972.tag) {
    int32_t const _M0L5_2aokS3158 = _tmp_3972.data.ok;
  } else {
    void* const _M0L6_2aerrS3159 = _tmp_3972.data.err;
    struct moonbit_result_0 _result_3973;
    _result_3973.tag = 0;
    _result_3973.data.err = _M0L6_2aerrS3159;
    return _result_3973;
  }
  _M0L14_2aboxed__selfS3170
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3170)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3170->$0 = 502;
  _M0L6_2atmpS3160
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3170
  };
  _M0L6_2atmpS3169 = 0;
  #line 56 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3168 = _M0MPC14json4Json6number(0x1.f6p+8, _M0L6_2atmpS3169);
  _M0L6_2atmpS3161 = _M0L6_2atmpS3168;
  _M0L6_2atmpS3164 = (moonbit_string_t)moonbit_string_literal_174.data;
  _M0L6_2atmpS3165 = (moonbit_string_t)moonbit_string_literal_175.data;
  _M0L6_2atmpS3166 = 0;
  _M0L6_2atmpS3167 = 0;
  _M0L6_2atmpS3163 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3163[0] = _M0L6_2atmpS3164;
  _M0L6_2atmpS3163[1] = _M0L6_2atmpS3165;
  _M0L6_2atmpS3163[2] = _M0L6_2atmpS3166;
  _M0L6_2atmpS3163[3] = _M0L6_2atmpS3167;
  _M0L6_2atmpS3162
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3162)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3162->$0 = _M0L6_2atmpS3163;
  _M0L6_2atmpS3162->$1 = 4;
  #line 56 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3974
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3160, _M0L6_2atmpS3161, (moonbit_string_t)moonbit_string_literal_176.data, _M0L6_2atmpS3162);
  if (_tmp_3974.tag) {
    int32_t const _M0L5_2aokS3171 = _tmp_3974.data.ok;
  } else {
    void* const _M0L6_2aerrS3172 = _tmp_3974.data.err;
    struct moonbit_result_0 _result_3975;
    _result_3975.tag = 0;
    _result_3975.data.err = _M0L6_2aerrS3172;
    return _result_3975;
  }
  _M0L14_2aboxed__selfS3183
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3183)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3183->$0 = 503;
  _M0L6_2atmpS3173
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3183
  };
  _M0L6_2atmpS3182 = 0;
  #line 57 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3181 = _M0MPC14json4Json6number(0x1.f7p+8, _M0L6_2atmpS3182);
  _M0L6_2atmpS3174 = _M0L6_2atmpS3181;
  _M0L6_2atmpS3177 = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS3178 = (moonbit_string_t)moonbit_string_literal_178.data;
  _M0L6_2atmpS3179 = 0;
  _M0L6_2atmpS3180 = 0;
  _M0L6_2atmpS3176 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3176[0] = _M0L6_2atmpS3177;
  _M0L6_2atmpS3176[1] = _M0L6_2atmpS3178;
  _M0L6_2atmpS3176[2] = _M0L6_2atmpS3179;
  _M0L6_2atmpS3176[3] = _M0L6_2atmpS3180;
  _M0L6_2atmpS3175
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3175)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3175->$0 = _M0L6_2atmpS3176;
  _M0L6_2atmpS3175->$1 = 4;
  #line 57 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3976
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3173, _M0L6_2atmpS3174, (moonbit_string_t)moonbit_string_literal_179.data, _M0L6_2atmpS3175);
  if (_tmp_3976.tag) {
    int32_t const _M0L5_2aokS3184 = _tmp_3976.data.ok;
  } else {
    void* const _M0L6_2aerrS3185 = _tmp_3976.data.err;
    struct moonbit_result_0 _result_3977;
    _result_3977.tag = 0;
    _result_3977.data.err = _M0L6_2aerrS3185;
    return _result_3977;
  }
  _M0L14_2aboxed__selfS3196
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3196)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3196->$0 = 504;
  _M0L6_2atmpS3186
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3196
  };
  _M0L6_2atmpS3195 = 0;
  #line 58 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3194 = _M0MPC14json4Json6number(0x1.f8p+8, _M0L6_2atmpS3195);
  _M0L6_2atmpS3187 = _M0L6_2atmpS3194;
  _M0L6_2atmpS3190 = (moonbit_string_t)moonbit_string_literal_180.data;
  _M0L6_2atmpS3191 = (moonbit_string_t)moonbit_string_literal_181.data;
  _M0L6_2atmpS3192 = 0;
  _M0L6_2atmpS3193 = 0;
  _M0L6_2atmpS3189 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3189[0] = _M0L6_2atmpS3190;
  _M0L6_2atmpS3189[1] = _M0L6_2atmpS3191;
  _M0L6_2atmpS3189[2] = _M0L6_2atmpS3192;
  _M0L6_2atmpS3189[3] = _M0L6_2atmpS3193;
  _M0L6_2atmpS3188
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3188)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3188->$0 = _M0L6_2atmpS3189;
  _M0L6_2atmpS3188->$1 = 4;
  #line 58 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3978
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3186, _M0L6_2atmpS3187, (moonbit_string_t)moonbit_string_literal_182.data, _M0L6_2atmpS3188);
  if (_tmp_3978.tag) {
    int32_t const _M0L5_2aokS3197 = _tmp_3978.data.ok;
  } else {
    void* const _M0L6_2aerrS3198 = _tmp_3978.data.err;
    struct moonbit_result_0 _result_3979;
    _result_3979.tag = 0;
    _result_3979.data.err = _M0L6_2aerrS3198;
    return _result_3979;
  }
  _M0L14_2aboxed__selfS3209
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3209)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3209->$0 = 505;
  _M0L6_2atmpS3199
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3209
  };
  _M0L6_2atmpS3208 = 0;
  #line 59 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3207 = _M0MPC14json4Json6number(0x1.f9p+8, _M0L6_2atmpS3208);
  _M0L6_2atmpS3200 = _M0L6_2atmpS3207;
  _M0L6_2atmpS3203 = (moonbit_string_t)moonbit_string_literal_183.data;
  _M0L6_2atmpS3204 = (moonbit_string_t)moonbit_string_literal_184.data;
  _M0L6_2atmpS3205 = 0;
  _M0L6_2atmpS3206 = 0;
  _M0L6_2atmpS3202 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3202[0] = _M0L6_2atmpS3203;
  _M0L6_2atmpS3202[1] = _M0L6_2atmpS3204;
  _M0L6_2atmpS3202[2] = _M0L6_2atmpS3205;
  _M0L6_2atmpS3202[3] = _M0L6_2atmpS3206;
  _M0L6_2atmpS3201
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3201)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3201->$0 = _M0L6_2atmpS3202;
  _M0L6_2atmpS3201->$1 = 4;
  #line 59 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3980
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3199, _M0L6_2atmpS3200, (moonbit_string_t)moonbit_string_literal_185.data, _M0L6_2atmpS3201);
  if (_tmp_3980.tag) {
    int32_t const _M0L5_2aokS3210 = _tmp_3980.data.ok;
  } else {
    void* const _M0L6_2aerrS3211 = _tmp_3980.data.err;
    struct moonbit_result_0 _result_3981;
    _result_3981.tag = 0;
    _result_3981.data.err = _M0L6_2aerrS3211;
    return _result_3981;
  }
  _M0L14_2aboxed__selfS3222
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3222)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3222->$0 = 506;
  _M0L6_2atmpS3212
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3222
  };
  _M0L6_2atmpS3221 = 0;
  #line 60 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3220 = _M0MPC14json4Json6number(0x1.fap+8, _M0L6_2atmpS3221);
  _M0L6_2atmpS3213 = _M0L6_2atmpS3220;
  _M0L6_2atmpS3216 = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L6_2atmpS3217 = (moonbit_string_t)moonbit_string_literal_187.data;
  _M0L6_2atmpS3218 = 0;
  _M0L6_2atmpS3219 = 0;
  _M0L6_2atmpS3215 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3215[0] = _M0L6_2atmpS3216;
  _M0L6_2atmpS3215[1] = _M0L6_2atmpS3217;
  _M0L6_2atmpS3215[2] = _M0L6_2atmpS3218;
  _M0L6_2atmpS3215[3] = _M0L6_2atmpS3219;
  _M0L6_2atmpS3214
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3214)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3214->$0 = _M0L6_2atmpS3215;
  _M0L6_2atmpS3214->$1 = 4;
  #line 60 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3982
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3212, _M0L6_2atmpS3213, (moonbit_string_t)moonbit_string_literal_188.data, _M0L6_2atmpS3214);
  if (_tmp_3982.tag) {
    int32_t const _M0L5_2aokS3223 = _tmp_3982.data.ok;
  } else {
    void* const _M0L6_2aerrS3224 = _tmp_3982.data.err;
    struct moonbit_result_0 _result_3983;
    _result_3983.tag = 0;
    _result_3983.data.err = _M0L6_2aerrS3224;
    return _result_3983;
  }
  _M0L14_2aboxed__selfS3235
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3235)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3235->$0 = 507;
  _M0L6_2atmpS3225
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3235
  };
  _M0L6_2atmpS3234 = 0;
  #line 61 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3233 = _M0MPC14json4Json6number(0x1.fbp+8, _M0L6_2atmpS3234);
  _M0L6_2atmpS3226 = _M0L6_2atmpS3233;
  _M0L6_2atmpS3229 = (moonbit_string_t)moonbit_string_literal_189.data;
  _M0L6_2atmpS3230 = (moonbit_string_t)moonbit_string_literal_190.data;
  _M0L6_2atmpS3231 = 0;
  _M0L6_2atmpS3232 = 0;
  _M0L6_2atmpS3228 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3228[0] = _M0L6_2atmpS3229;
  _M0L6_2atmpS3228[1] = _M0L6_2atmpS3230;
  _M0L6_2atmpS3228[2] = _M0L6_2atmpS3231;
  _M0L6_2atmpS3228[3] = _M0L6_2atmpS3232;
  _M0L6_2atmpS3227
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3227)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3227->$0 = _M0L6_2atmpS3228;
  _M0L6_2atmpS3227->$1 = 4;
  #line 61 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3984
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3225, _M0L6_2atmpS3226, (moonbit_string_t)moonbit_string_literal_191.data, _M0L6_2atmpS3227);
  if (_tmp_3984.tag) {
    int32_t const _M0L5_2aokS3236 = _tmp_3984.data.ok;
  } else {
    void* const _M0L6_2aerrS3237 = _tmp_3984.data.err;
    struct moonbit_result_0 _result_3985;
    _result_3985.tag = 0;
    _result_3985.data.err = _M0L6_2aerrS3237;
    return _result_3985;
  }
  _M0L14_2aboxed__selfS3248
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3248)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3248->$0 = 508;
  _M0L6_2atmpS3238
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3248
  };
  _M0L6_2atmpS3247 = 0;
  #line 62 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3246 = _M0MPC14json4Json6number(0x1.fcp+8, _M0L6_2atmpS3247);
  _M0L6_2atmpS3239 = _M0L6_2atmpS3246;
  _M0L6_2atmpS3242 = (moonbit_string_t)moonbit_string_literal_192.data;
  _M0L6_2atmpS3243 = (moonbit_string_t)moonbit_string_literal_193.data;
  _M0L6_2atmpS3244 = 0;
  _M0L6_2atmpS3245 = 0;
  _M0L6_2atmpS3241 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3241[0] = _M0L6_2atmpS3242;
  _M0L6_2atmpS3241[1] = _M0L6_2atmpS3243;
  _M0L6_2atmpS3241[2] = _M0L6_2atmpS3244;
  _M0L6_2atmpS3241[3] = _M0L6_2atmpS3245;
  _M0L6_2atmpS3240
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3240)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3240->$0 = _M0L6_2atmpS3241;
  _M0L6_2atmpS3240->$1 = 4;
  #line 62 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3986
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3238, _M0L6_2atmpS3239, (moonbit_string_t)moonbit_string_literal_194.data, _M0L6_2atmpS3240);
  if (_tmp_3986.tag) {
    int32_t const _M0L5_2aokS3249 = _tmp_3986.data.ok;
  } else {
    void* const _M0L6_2aerrS3250 = _tmp_3986.data.err;
    struct moonbit_result_0 _result_3987;
    _result_3987.tag = 0;
    _result_3987.data.err = _M0L6_2aerrS3250;
    return _result_3987;
  }
  _M0L14_2aboxed__selfS3261
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3261)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3261->$0 = 510;
  _M0L6_2atmpS3251
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3261
  };
  _M0L6_2atmpS3260 = 0;
  #line 63 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3259 = _M0MPC14json4Json6number(0x1.fep+8, _M0L6_2atmpS3260);
  _M0L6_2atmpS3252 = _M0L6_2atmpS3259;
  _M0L6_2atmpS3255 = (moonbit_string_t)moonbit_string_literal_195.data;
  _M0L6_2atmpS3256 = (moonbit_string_t)moonbit_string_literal_196.data;
  _M0L6_2atmpS3257 = 0;
  _M0L6_2atmpS3258 = 0;
  _M0L6_2atmpS3254 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3254[0] = _M0L6_2atmpS3255;
  _M0L6_2atmpS3254[1] = _M0L6_2atmpS3256;
  _M0L6_2atmpS3254[2] = _M0L6_2atmpS3257;
  _M0L6_2atmpS3254[3] = _M0L6_2atmpS3258;
  _M0L6_2atmpS3253
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3253)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3253->$0 = _M0L6_2atmpS3254;
  _M0L6_2atmpS3253->$1 = 4;
  #line 63 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _tmp_3988
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3251, _M0L6_2atmpS3252, (moonbit_string_t)moonbit_string_literal_197.data, _M0L6_2atmpS3253);
  if (_tmp_3988.tag) {
    int32_t const _M0L5_2aokS3262 = _tmp_3988.data.ok;
  } else {
    void* const _M0L6_2aerrS3263 = _tmp_3988.data.err;
    struct moonbit_result_0 _result_3989;
    _result_3989.tag = 0;
    _result_3989.data.err = _M0L6_2aerrS3263;
    return _result_3989;
  }
  _M0L14_2aboxed__selfS3274
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS3274)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3274->$0 = 511;
  _M0L6_2atmpS3264
  = (struct _M0TPB6ToJson){
    _M0FP078Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3274
  };
  _M0L6_2atmpS3273 = 0;
  #line 64 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  _M0L6_2atmpS3272 = _M0MPC14json4Json6number(0x1.ffp+8, _M0L6_2atmpS3273);
  _M0L6_2atmpS3265 = _M0L6_2atmpS3272;
  _M0L6_2atmpS3268 = (moonbit_string_t)moonbit_string_literal_198.data;
  _M0L6_2atmpS3269 = (moonbit_string_t)moonbit_string_literal_199.data;
  _M0L6_2atmpS3270 = 0;
  _M0L6_2atmpS3271 = 0;
  _M0L6_2atmpS3267 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3267[0] = _M0L6_2atmpS3268;
  _M0L6_2atmpS3267[1] = _M0L6_2atmpS3269;
  _M0L6_2atmpS3267[2] = _M0L6_2atmpS3270;
  _M0L6_2atmpS3267[3] = _M0L6_2atmpS3271;
  _M0L6_2atmpS3266
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3266)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3266->$0 = _M0L6_2atmpS3267;
  _M0L6_2atmpS3266->$1 = 4;
  #line 64 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\status_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3264, _M0L6_2atmpS3265, (moonbit_string_t)moonbit_string_literal_200.data, _M0L6_2atmpS3266);
}

int32_t _M0FP58clawteam8clawteam8internal5httpx6status8classify(
  int32_t _M0L4codeS1020
) {
  #line 28 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\class.mbt"
  if (_M0L4codeS1020 >= 100 && _M0L4codeS1020 <= 199) {
    return 0;
  } else if (_M0L4codeS1020 >= 200 && _M0L4codeS1020 <= 299) {
    return 1;
  } else if (_M0L4codeS1020 >= 300 && _M0L4codeS1020 <= 399) {
    return 2;
  } else if (_M0L4codeS1020 >= 400 && _M0L4codeS1020 <= 499) {
    return 3;
  } else if (_M0L4codeS1020 >= 500 && _M0L4codeS1020 <= 599) {
    return 4;
  } else {
    return 5;
  }
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1015,
  void* _M0L7contentS1017,
  moonbit_string_t _M0L3locS1011,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1013
) {
  moonbit_string_t _M0L3locS1010;
  moonbit_string_t _M0L9args__locS1012;
  void* _M0L6_2atmpS2469;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2470;
  moonbit_string_t _M0L6actualS1014;
  moonbit_string_t _M0L4wantS1016;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1010 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1011);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1012 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1013);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2469 = _M0L3objS1015.$0->$method_0(_M0L3objS1015.$1);
  _M0L6_2atmpS2470 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1014
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2469, 0, 0, _M0L6_2atmpS2470);
  if (_M0L7contentS1017 == 0) {
    void* _M0L6_2atmpS2466;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2467;
    if (_M0L7contentS1017) {
      moonbit_decref(_M0L7contentS1017);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2466
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2467 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1016
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2466, 0, 0, _M0L6_2atmpS2467);
  } else {
    void* _M0L7_2aSomeS1018 = _M0L7contentS1017;
    void* _M0L4_2axS1019 = _M0L7_2aSomeS1018;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2468 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1016
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1019, 0, 0, _M0L6_2atmpS2468);
  }
  moonbit_incref(_M0L4wantS1016);
  moonbit_incref(_M0L6actualS1014);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1014, _M0L4wantS1016)
  ) {
    moonbit_string_t _M0L6_2atmpS2464;
    moonbit_string_t _M0L6_2atmpS3524;
    moonbit_string_t _M0L6_2atmpS2463;
    moonbit_string_t _M0L6_2atmpS3523;
    moonbit_string_t _M0L6_2atmpS2461;
    moonbit_string_t _M0L6_2atmpS2462;
    moonbit_string_t _M0L6_2atmpS3522;
    moonbit_string_t _M0L6_2atmpS2460;
    moonbit_string_t _M0L6_2atmpS3521;
    moonbit_string_t _M0L6_2atmpS2457;
    moonbit_string_t _M0L6_2atmpS2459;
    moonbit_string_t _M0L6_2atmpS2458;
    moonbit_string_t _M0L6_2atmpS3520;
    moonbit_string_t _M0L6_2atmpS2456;
    moonbit_string_t _M0L6_2atmpS3519;
    moonbit_string_t _M0L6_2atmpS2453;
    moonbit_string_t _M0L6_2atmpS2455;
    moonbit_string_t _M0L6_2atmpS2454;
    moonbit_string_t _M0L6_2atmpS3518;
    moonbit_string_t _M0L6_2atmpS2452;
    moonbit_string_t _M0L6_2atmpS3517;
    moonbit_string_t _M0L6_2atmpS2451;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2450;
    struct moonbit_result_0 _result_3990;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2464
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1010);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3524
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_201.data, _M0L6_2atmpS2464);
    moonbit_decref(_M0L6_2atmpS2464);
    _M0L6_2atmpS2463 = _M0L6_2atmpS3524;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3523
    = moonbit_add_string(_M0L6_2atmpS2463, (moonbit_string_t)moonbit_string_literal_202.data);
    moonbit_decref(_M0L6_2atmpS2463);
    _M0L6_2atmpS2461 = _M0L6_2atmpS3523;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2462
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1012);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3522 = moonbit_add_string(_M0L6_2atmpS2461, _M0L6_2atmpS2462);
    moonbit_decref(_M0L6_2atmpS2461);
    moonbit_decref(_M0L6_2atmpS2462);
    _M0L6_2atmpS2460 = _M0L6_2atmpS3522;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3521
    = moonbit_add_string(_M0L6_2atmpS2460, (moonbit_string_t)moonbit_string_literal_203.data);
    moonbit_decref(_M0L6_2atmpS2460);
    _M0L6_2atmpS2457 = _M0L6_2atmpS3521;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2459 = _M0MPC16string6String6escape(_M0L4wantS1016);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2458
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2459);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3520 = moonbit_add_string(_M0L6_2atmpS2457, _M0L6_2atmpS2458);
    moonbit_decref(_M0L6_2atmpS2457);
    moonbit_decref(_M0L6_2atmpS2458);
    _M0L6_2atmpS2456 = _M0L6_2atmpS3520;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3519
    = moonbit_add_string(_M0L6_2atmpS2456, (moonbit_string_t)moonbit_string_literal_204.data);
    moonbit_decref(_M0L6_2atmpS2456);
    _M0L6_2atmpS2453 = _M0L6_2atmpS3519;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2455 = _M0MPC16string6String6escape(_M0L6actualS1014);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2454
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2455);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3518 = moonbit_add_string(_M0L6_2atmpS2453, _M0L6_2atmpS2454);
    moonbit_decref(_M0L6_2atmpS2453);
    moonbit_decref(_M0L6_2atmpS2454);
    _M0L6_2atmpS2452 = _M0L6_2atmpS3518;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3517
    = moonbit_add_string(_M0L6_2atmpS2452, (moonbit_string_t)moonbit_string_literal_205.data);
    moonbit_decref(_M0L6_2atmpS2452);
    _M0L6_2atmpS2451 = _M0L6_2atmpS3517;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2450
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2450)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2450)->$0
    = _M0L6_2atmpS2451;
    _result_3990.tag = 0;
    _result_3990.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2450;
    return _result_3990;
  } else {
    int32_t _M0L6_2atmpS2465;
    struct moonbit_result_0 _result_3991;
    moonbit_decref(_M0L4wantS1016);
    moonbit_decref(_M0L6actualS1014);
    moonbit_decref(_M0L9args__locS1012);
    moonbit_decref(_M0L3locS1010);
    _M0L6_2atmpS2465 = 0;
    _result_3991.tag = 1;
    _result_3991.data.ok = _M0L6_2atmpS2465;
    return _result_3991;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1009,
  int32_t _M0L13escape__slashS981,
  int32_t _M0L6indentS976,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1002
) {
  struct _M0TPB13StringBuilder* _M0L3bufS968;
  void** _M0L6_2atmpS2449;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS969;
  int32_t _M0Lm5depthS970;
  void* _M0L6_2atmpS2448;
  void* _M0L8_2aparamS971;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS968 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2449 = (void**)moonbit_empty_ref_array;
  _M0L5stackS969
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS969)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS969->$0 = _M0L6_2atmpS2449;
  _M0L5stackS969->$1 = 0;
  _M0Lm5depthS970 = 0;
  _M0L6_2atmpS2448 = _M0L4selfS1009;
  _M0L8_2aparamS971 = _M0L6_2atmpS2448;
  _2aloop_987:;
  while (1) {
    if (_M0L8_2aparamS971 == 0) {
      int32_t _M0L3lenS2410;
      if (_M0L8_2aparamS971) {
        moonbit_decref(_M0L8_2aparamS971);
      }
      _M0L3lenS2410 = _M0L5stackS969->$1;
      if (_M0L3lenS2410 == 0) {
        if (_M0L8replacerS1002) {
          moonbit_decref(_M0L8replacerS1002);
        }
        moonbit_decref(_M0L5stackS969);
        break;
      } else {
        void** _M0L8_2afieldS3532 = _M0L5stackS969->$0;
        void** _M0L3bufS2434 = _M0L8_2afieldS3532;
        int32_t _M0L3lenS2436 = _M0L5stackS969->$1;
        int32_t _M0L6_2atmpS2435 = _M0L3lenS2436 - 1;
        void* _M0L6_2atmpS3531 = (void*)_M0L3bufS2434[_M0L6_2atmpS2435];
        void* _M0L4_2axS988 = _M0L6_2atmpS3531;
        switch (Moonbit_object_tag(_M0L4_2axS988)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS989 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS988;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3527 =
              _M0L8_2aArrayS989->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS990 =
              _M0L8_2afieldS3527;
            int32_t _M0L4_2aiS991 = _M0L8_2aArrayS989->$1;
            int32_t _M0L3lenS2422 = _M0L6_2aarrS990->$1;
            if (_M0L4_2aiS991 < _M0L3lenS2422) {
              int32_t _if__result_3993;
              void** _M0L8_2afieldS3526;
              void** _M0L3bufS2428;
              void* _M0L6_2atmpS3525;
              void* _M0L7elementS992;
              int32_t _M0L6_2atmpS2423;
              void* _M0L6_2atmpS2426;
              if (_M0L4_2aiS991 < 0) {
                _if__result_3993 = 1;
              } else {
                int32_t _M0L3lenS2427 = _M0L6_2aarrS990->$1;
                _if__result_3993 = _M0L4_2aiS991 >= _M0L3lenS2427;
              }
              if (_if__result_3993) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3526 = _M0L6_2aarrS990->$0;
              _M0L3bufS2428 = _M0L8_2afieldS3526;
              _M0L6_2atmpS3525 = (void*)_M0L3bufS2428[_M0L4_2aiS991];
              _M0L7elementS992 = _M0L6_2atmpS3525;
              _M0L6_2atmpS2423 = _M0L4_2aiS991 + 1;
              _M0L8_2aArrayS989->$1 = _M0L6_2atmpS2423;
              if (_M0L4_2aiS991 > 0) {
                int32_t _M0L6_2atmpS2425;
                moonbit_string_t _M0L6_2atmpS2424;
                moonbit_incref(_M0L7elementS992);
                moonbit_incref(_M0L3bufS968);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 44);
                _M0L6_2atmpS2425 = _M0Lm5depthS970;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2424
                = _M0FPC14json11indent__str(_M0L6_2atmpS2425, _M0L6indentS976);
                moonbit_incref(_M0L3bufS968);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2424);
              } else {
                moonbit_incref(_M0L7elementS992);
              }
              _M0L6_2atmpS2426 = _M0L7elementS992;
              _M0L8_2aparamS971 = _M0L6_2atmpS2426;
              goto _2aloop_987;
            } else {
              int32_t _M0L6_2atmpS2429 = _M0Lm5depthS970;
              void* _M0L6_2atmpS2430;
              int32_t _M0L6_2atmpS2432;
              moonbit_string_t _M0L6_2atmpS2431;
              void* _M0L6_2atmpS2433;
              _M0Lm5depthS970 = _M0L6_2atmpS2429 - 1;
              moonbit_incref(_M0L5stackS969);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2430
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS969);
              if (_M0L6_2atmpS2430) {
                moonbit_decref(_M0L6_2atmpS2430);
              }
              _M0L6_2atmpS2432 = _M0Lm5depthS970;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2431
              = _M0FPC14json11indent__str(_M0L6_2atmpS2432, _M0L6indentS976);
              moonbit_incref(_M0L3bufS968);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2431);
              moonbit_incref(_M0L3bufS968);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 93);
              _M0L6_2atmpS2433 = 0;
              _M0L8_2aparamS971 = _M0L6_2atmpS2433;
              goto _2aloop_987;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS993 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS988;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3530 =
              _M0L9_2aObjectS993->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS994 =
              _M0L8_2afieldS3530;
            int32_t _M0L8_2afirstS995 = _M0L9_2aObjectS993->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS996;
            moonbit_incref(_M0L11_2aiteratorS994);
            moonbit_incref(_M0L9_2aObjectS993);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS996
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS994);
            if (_M0L7_2abindS996 == 0) {
              int32_t _M0L6_2atmpS2411;
              void* _M0L6_2atmpS2412;
              int32_t _M0L6_2atmpS2414;
              moonbit_string_t _M0L6_2atmpS2413;
              void* _M0L6_2atmpS2415;
              if (_M0L7_2abindS996) {
                moonbit_decref(_M0L7_2abindS996);
              }
              moonbit_decref(_M0L9_2aObjectS993);
              _M0L6_2atmpS2411 = _M0Lm5depthS970;
              _M0Lm5depthS970 = _M0L6_2atmpS2411 - 1;
              moonbit_incref(_M0L5stackS969);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2412
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS969);
              if (_M0L6_2atmpS2412) {
                moonbit_decref(_M0L6_2atmpS2412);
              }
              _M0L6_2atmpS2414 = _M0Lm5depthS970;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2413
              = _M0FPC14json11indent__str(_M0L6_2atmpS2414, _M0L6indentS976);
              moonbit_incref(_M0L3bufS968);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2413);
              moonbit_incref(_M0L3bufS968);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 125);
              _M0L6_2atmpS2415 = 0;
              _M0L8_2aparamS971 = _M0L6_2atmpS2415;
              goto _2aloop_987;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS997 = _M0L7_2abindS996;
              struct _M0TUsRPB4JsonE* _M0L4_2axS998 = _M0L7_2aSomeS997;
              moonbit_string_t _M0L8_2afieldS3529 = _M0L4_2axS998->$0;
              moonbit_string_t _M0L4_2akS999 = _M0L8_2afieldS3529;
              void* _M0L8_2afieldS3528 = _M0L4_2axS998->$1;
              int32_t _M0L6_2acntS3746 =
                Moonbit_object_header(_M0L4_2axS998)->rc;
              void* _M0L4_2avS1000;
              void* _M0Lm2v2S1001;
              moonbit_string_t _M0L6_2atmpS2419;
              void* _M0L6_2atmpS2421;
              void* _M0L6_2atmpS2420;
              if (_M0L6_2acntS3746 > 1) {
                int32_t _M0L11_2anew__cntS3747 = _M0L6_2acntS3746 - 1;
                Moonbit_object_header(_M0L4_2axS998)->rc
                = _M0L11_2anew__cntS3747;
                moonbit_incref(_M0L8_2afieldS3528);
                moonbit_incref(_M0L4_2akS999);
              } else if (_M0L6_2acntS3746 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS998);
              }
              _M0L4_2avS1000 = _M0L8_2afieldS3528;
              _M0Lm2v2S1001 = _M0L4_2avS1000;
              if (_M0L8replacerS1002 == 0) {
                moonbit_incref(_M0Lm2v2S1001);
                moonbit_decref(_M0L4_2avS1000);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1003 =
                  _M0L8replacerS1002;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1004 =
                  _M0L7_2aSomeS1003;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1005 =
                  _M0L11_2areplacerS1004;
                void* _M0L7_2abindS1006;
                moonbit_incref(_M0L7_2afuncS1005);
                moonbit_incref(_M0L4_2akS999);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1006
                = _M0L7_2afuncS1005->code(_M0L7_2afuncS1005, _M0L4_2akS999, _M0L4_2avS1000);
                if (_M0L7_2abindS1006 == 0) {
                  void* _M0L6_2atmpS2416;
                  if (_M0L7_2abindS1006) {
                    moonbit_decref(_M0L7_2abindS1006);
                  }
                  moonbit_decref(_M0L4_2akS999);
                  moonbit_decref(_M0L9_2aObjectS993);
                  _M0L6_2atmpS2416 = 0;
                  _M0L8_2aparamS971 = _M0L6_2atmpS2416;
                  goto _2aloop_987;
                } else {
                  void* _M0L7_2aSomeS1007 = _M0L7_2abindS1006;
                  void* _M0L4_2avS1008 = _M0L7_2aSomeS1007;
                  _M0Lm2v2S1001 = _M0L4_2avS1008;
                }
              }
              if (!_M0L8_2afirstS995) {
                int32_t _M0L6_2atmpS2418;
                moonbit_string_t _M0L6_2atmpS2417;
                moonbit_incref(_M0L3bufS968);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 44);
                _M0L6_2atmpS2418 = _M0Lm5depthS970;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2417
                = _M0FPC14json11indent__str(_M0L6_2atmpS2418, _M0L6indentS976);
                moonbit_incref(_M0L3bufS968);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2417);
              }
              moonbit_incref(_M0L3bufS968);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2419
              = _M0FPC14json6escape(_M0L4_2akS999, _M0L13escape__slashS981);
              moonbit_incref(_M0L3bufS968);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2419);
              moonbit_incref(_M0L3bufS968);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 34);
              moonbit_incref(_M0L3bufS968);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 58);
              if (_M0L6indentS976 > 0) {
                moonbit_incref(_M0L3bufS968);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 32);
              }
              _M0L9_2aObjectS993->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS993);
              _M0L6_2atmpS2421 = _M0Lm2v2S1001;
              _M0L6_2atmpS2420 = _M0L6_2atmpS2421;
              _M0L8_2aparamS971 = _M0L6_2atmpS2420;
              goto _2aloop_987;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS972 = _M0L8_2aparamS971;
      void* _M0L8_2avalueS973 = _M0L7_2aSomeS972;
      void* _M0L6_2atmpS2447;
      switch (Moonbit_object_tag(_M0L8_2avalueS973)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS974 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS973;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3533 =
            _M0L9_2aObjectS974->$0;
          int32_t _M0L6_2acntS3748 =
            Moonbit_object_header(_M0L9_2aObjectS974)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS975;
          if (_M0L6_2acntS3748 > 1) {
            int32_t _M0L11_2anew__cntS3749 = _M0L6_2acntS3748 - 1;
            Moonbit_object_header(_M0L9_2aObjectS974)->rc
            = _M0L11_2anew__cntS3749;
            moonbit_incref(_M0L8_2afieldS3533);
          } else if (_M0L6_2acntS3748 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS974);
          }
          _M0L10_2amembersS975 = _M0L8_2afieldS3533;
          moonbit_incref(_M0L10_2amembersS975);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS975)) {
            moonbit_decref(_M0L10_2amembersS975);
            moonbit_incref(_M0L3bufS968);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, (moonbit_string_t)moonbit_string_literal_206.data);
          } else {
            int32_t _M0L6_2atmpS2442 = _M0Lm5depthS970;
            int32_t _M0L6_2atmpS2444;
            moonbit_string_t _M0L6_2atmpS2443;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2446;
            void* _M0L6ObjectS2445;
            _M0Lm5depthS970 = _M0L6_2atmpS2442 + 1;
            moonbit_incref(_M0L3bufS968);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 123);
            _M0L6_2atmpS2444 = _M0Lm5depthS970;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2443
            = _M0FPC14json11indent__str(_M0L6_2atmpS2444, _M0L6indentS976);
            moonbit_incref(_M0L3bufS968);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2443);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2446
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS975);
            _M0L6ObjectS2445
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2445)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2445)->$0
            = _M0L6_2atmpS2446;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2445)->$1
            = 1;
            moonbit_incref(_M0L5stackS969);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS969, _M0L6ObjectS2445);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS977 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS973;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3534 =
            _M0L8_2aArrayS977->$0;
          int32_t _M0L6_2acntS3750 =
            Moonbit_object_header(_M0L8_2aArrayS977)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS978;
          if (_M0L6_2acntS3750 > 1) {
            int32_t _M0L11_2anew__cntS3751 = _M0L6_2acntS3750 - 1;
            Moonbit_object_header(_M0L8_2aArrayS977)->rc
            = _M0L11_2anew__cntS3751;
            moonbit_incref(_M0L8_2afieldS3534);
          } else if (_M0L6_2acntS3750 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS977);
          }
          _M0L6_2aarrS978 = _M0L8_2afieldS3534;
          moonbit_incref(_M0L6_2aarrS978);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS978)) {
            moonbit_decref(_M0L6_2aarrS978);
            moonbit_incref(_M0L3bufS968);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, (moonbit_string_t)moonbit_string_literal_207.data);
          } else {
            int32_t _M0L6_2atmpS2438 = _M0Lm5depthS970;
            int32_t _M0L6_2atmpS2440;
            moonbit_string_t _M0L6_2atmpS2439;
            void* _M0L5ArrayS2441;
            _M0Lm5depthS970 = _M0L6_2atmpS2438 + 1;
            moonbit_incref(_M0L3bufS968);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 91);
            _M0L6_2atmpS2440 = _M0Lm5depthS970;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2439
            = _M0FPC14json11indent__str(_M0L6_2atmpS2440, _M0L6indentS976);
            moonbit_incref(_M0L3bufS968);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2439);
            _M0L5ArrayS2441
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2441)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2441)->$0
            = _M0L6_2aarrS978;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2441)->$1
            = 0;
            moonbit_incref(_M0L5stackS969);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS969, _M0L5ArrayS2441);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS979 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS973;
          moonbit_string_t _M0L8_2afieldS3535 = _M0L9_2aStringS979->$0;
          int32_t _M0L6_2acntS3752 =
            Moonbit_object_header(_M0L9_2aStringS979)->rc;
          moonbit_string_t _M0L4_2asS980;
          moonbit_string_t _M0L6_2atmpS2437;
          if (_M0L6_2acntS3752 > 1) {
            int32_t _M0L11_2anew__cntS3753 = _M0L6_2acntS3752 - 1;
            Moonbit_object_header(_M0L9_2aStringS979)->rc
            = _M0L11_2anew__cntS3753;
            moonbit_incref(_M0L8_2afieldS3535);
          } else if (_M0L6_2acntS3752 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS979);
          }
          _M0L4_2asS980 = _M0L8_2afieldS3535;
          moonbit_incref(_M0L3bufS968);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2437
          = _M0FPC14json6escape(_M0L4_2asS980, _M0L13escape__slashS981);
          moonbit_incref(_M0L3bufS968);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L6_2atmpS2437);
          moonbit_incref(_M0L3bufS968);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS968, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS982 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS973;
          double _M0L4_2anS983 = _M0L9_2aNumberS982->$0;
          moonbit_string_t _M0L8_2afieldS3536 = _M0L9_2aNumberS982->$1;
          int32_t _M0L6_2acntS3754 =
            Moonbit_object_header(_M0L9_2aNumberS982)->rc;
          moonbit_string_t _M0L7_2areprS984;
          if (_M0L6_2acntS3754 > 1) {
            int32_t _M0L11_2anew__cntS3755 = _M0L6_2acntS3754 - 1;
            Moonbit_object_header(_M0L9_2aNumberS982)->rc
            = _M0L11_2anew__cntS3755;
            if (_M0L8_2afieldS3536) {
              moonbit_incref(_M0L8_2afieldS3536);
            }
          } else if (_M0L6_2acntS3754 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS982);
          }
          _M0L7_2areprS984 = _M0L8_2afieldS3536;
          if (_M0L7_2areprS984 == 0) {
            if (_M0L7_2areprS984) {
              moonbit_decref(_M0L7_2areprS984);
            }
            moonbit_incref(_M0L3bufS968);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS968, _M0L4_2anS983);
          } else {
            moonbit_string_t _M0L7_2aSomeS985 = _M0L7_2areprS984;
            moonbit_string_t _M0L4_2arS986 = _M0L7_2aSomeS985;
            moonbit_incref(_M0L3bufS968);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, _M0L4_2arS986);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS968);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, (moonbit_string_t)moonbit_string_literal_208.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS968);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, (moonbit_string_t)moonbit_string_literal_209.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS973);
          moonbit_incref(_M0L3bufS968);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS968, (moonbit_string_t)moonbit_string_literal_210.data);
          break;
        }
      }
      _M0L6_2atmpS2447 = 0;
      _M0L8_2aparamS971 = _M0L6_2atmpS2447;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS968);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS967,
  int32_t _M0L6indentS965
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS965 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS966 = _M0L6indentS965 * _M0L5levelS967;
    switch (_M0L6spacesS966) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_211.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_212.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_213.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_214.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_215.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_216.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_217.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_218.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_219.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2409;
        moonbit_string_t _M0L6_2atmpS3537;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2409
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_220.data, _M0L6spacesS966);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3537
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_211.data, _M0L6_2atmpS2409);
        moonbit_decref(_M0L6_2atmpS2409);
        return _M0L6_2atmpS3537;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS957,
  int32_t _M0L13escape__slashS962
) {
  int32_t _M0L6_2atmpS2408;
  struct _M0TPB13StringBuilder* _M0L3bufS956;
  struct _M0TWEOc* _M0L5_2aitS958;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2408 = Moonbit_array_length(_M0L3strS957);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS956 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2408);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS958 = _M0MPC16string6String4iter(_M0L3strS957);
  while (1) {
    int32_t _M0L7_2abindS959;
    moonbit_incref(_M0L5_2aitS958);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS959 = _M0MPB4Iter4nextGcE(_M0L5_2aitS958);
    if (_M0L7_2abindS959 == -1) {
      moonbit_decref(_M0L5_2aitS958);
    } else {
      int32_t _M0L7_2aSomeS960 = _M0L7_2abindS959;
      int32_t _M0L4_2acS961 = _M0L7_2aSomeS960;
      if (_M0L4_2acS961 == 34) {
        moonbit_incref(_M0L3bufS956);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_221.data);
      } else if (_M0L4_2acS961 == 92) {
        moonbit_incref(_M0L3bufS956);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_222.data);
      } else if (_M0L4_2acS961 == 47) {
        if (_M0L13escape__slashS962) {
          moonbit_incref(_M0L3bufS956);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_223.data);
        } else {
          moonbit_incref(_M0L3bufS956);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS956, _M0L4_2acS961);
        }
      } else if (_M0L4_2acS961 == 10) {
        moonbit_incref(_M0L3bufS956);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_224.data);
      } else if (_M0L4_2acS961 == 13) {
        moonbit_incref(_M0L3bufS956);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_225.data);
      } else if (_M0L4_2acS961 == 8) {
        moonbit_incref(_M0L3bufS956);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_226.data);
      } else if (_M0L4_2acS961 == 9) {
        moonbit_incref(_M0L3bufS956);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_227.data);
      } else {
        int32_t _M0L4codeS963 = _M0L4_2acS961;
        if (_M0L4codeS963 == 12) {
          moonbit_incref(_M0L3bufS956);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_228.data);
        } else if (_M0L4codeS963 < 32) {
          int32_t _M0L6_2atmpS2407;
          moonbit_string_t _M0L6_2atmpS2406;
          moonbit_incref(_M0L3bufS956);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, (moonbit_string_t)moonbit_string_literal_229.data);
          _M0L6_2atmpS2407 = _M0L4codeS963 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2406 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2407);
          moonbit_incref(_M0L3bufS956);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS956, _M0L6_2atmpS2406);
        } else {
          moonbit_incref(_M0L3bufS956);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS956, _M0L4_2acS961);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS956);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS955
) {
  int32_t _M0L8_2afieldS3538;
  int32_t _M0L3lenS2405;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3538 = _M0L4selfS955->$1;
  moonbit_decref(_M0L4selfS955);
  _M0L3lenS2405 = _M0L8_2afieldS3538;
  return _M0L3lenS2405 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS952
) {
  int32_t _M0L3lenS951;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS951 = _M0L4selfS952->$1;
  if (_M0L3lenS951 == 0) {
    moonbit_decref(_M0L4selfS952);
    return 0;
  } else {
    int32_t _M0L5indexS953 = _M0L3lenS951 - 1;
    void** _M0L8_2afieldS3542 = _M0L4selfS952->$0;
    void** _M0L3bufS2404 = _M0L8_2afieldS3542;
    void* _M0L6_2atmpS3541 = (void*)_M0L3bufS2404[_M0L5indexS953];
    void* _M0L1vS954 = _M0L6_2atmpS3541;
    void** _M0L8_2afieldS3540 = _M0L4selfS952->$0;
    void** _M0L3bufS2403 = _M0L8_2afieldS3540;
    void* _M0L6_2aoldS3539;
    if (
      _M0L5indexS953 < 0
      || _M0L5indexS953 >= Moonbit_array_length(_M0L3bufS2403)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3539 = (void*)_M0L3bufS2403[_M0L5indexS953];
    moonbit_incref(_M0L1vS954);
    moonbit_decref(_M0L6_2aoldS3539);
    if (
      _M0L5indexS953 < 0
      || _M0L5indexS953 >= Moonbit_array_length(_M0L3bufS2403)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2403[_M0L5indexS953]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS952->$1 = _M0L5indexS953;
    moonbit_decref(_M0L4selfS952);
    return _M0L1vS954;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS949,
  struct _M0TPB6Logger _M0L6loggerS950
) {
  moonbit_string_t _M0L6_2atmpS2402;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2401;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2402 = _M0L4selfS949;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2401 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2402);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2401, _M0L6loggerS950);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS926,
  struct _M0TPB6Logger _M0L6loggerS948
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3551;
  struct _M0TPC16string10StringView _M0L3pkgS925;
  moonbit_string_t _M0L7_2adataS927;
  int32_t _M0L8_2astartS928;
  int32_t _M0L6_2atmpS2400;
  int32_t _M0L6_2aendS929;
  int32_t _M0Lm9_2acursorS930;
  int32_t _M0Lm13accept__stateS931;
  int32_t _M0Lm10match__endS932;
  int32_t _M0Lm20match__tag__saver__0S933;
  int32_t _M0Lm6tag__0S934;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS935;
  struct _M0TPC16string10StringView _M0L8_2afieldS3550;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS944;
  void* _M0L8_2afieldS3549;
  int32_t _M0L6_2acntS3756;
  void* _M0L16_2apackage__nameS945;
  struct _M0TPC16string10StringView _M0L8_2afieldS3547;
  struct _M0TPC16string10StringView _M0L8filenameS2377;
  struct _M0TPC16string10StringView _M0L8_2afieldS3546;
  struct _M0TPC16string10StringView _M0L11start__lineS2378;
  struct _M0TPC16string10StringView _M0L8_2afieldS3545;
  struct _M0TPC16string10StringView _M0L13start__columnS2379;
  struct _M0TPC16string10StringView _M0L8_2afieldS3544;
  struct _M0TPC16string10StringView _M0L9end__lineS2380;
  struct _M0TPC16string10StringView _M0L8_2afieldS3543;
  int32_t _M0L6_2acntS3760;
  struct _M0TPC16string10StringView _M0L11end__columnS2381;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3551
  = (struct _M0TPC16string10StringView){
    _M0L4selfS926->$0_1, _M0L4selfS926->$0_2, _M0L4selfS926->$0_0
  };
  _M0L3pkgS925 = _M0L8_2afieldS3551;
  moonbit_incref(_M0L3pkgS925.$0);
  moonbit_incref(_M0L3pkgS925.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS927 = _M0MPC16string10StringView4data(_M0L3pkgS925);
  moonbit_incref(_M0L3pkgS925.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS928 = _M0MPC16string10StringView13start__offset(_M0L3pkgS925);
  moonbit_incref(_M0L3pkgS925.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2400 = _M0MPC16string10StringView6length(_M0L3pkgS925);
  _M0L6_2aendS929 = _M0L8_2astartS928 + _M0L6_2atmpS2400;
  _M0Lm9_2acursorS930 = _M0L8_2astartS928;
  _M0Lm13accept__stateS931 = -1;
  _M0Lm10match__endS932 = -1;
  _M0Lm20match__tag__saver__0S933 = -1;
  _M0Lm6tag__0S934 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2392 = _M0Lm9_2acursorS930;
    if (_M0L6_2atmpS2392 < _M0L6_2aendS929) {
      int32_t _M0L6_2atmpS2399 = _M0Lm9_2acursorS930;
      int32_t _M0L10next__charS939;
      int32_t _M0L6_2atmpS2393;
      moonbit_incref(_M0L7_2adataS927);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS939
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS927, _M0L6_2atmpS2399);
      _M0L6_2atmpS2393 = _M0Lm9_2acursorS930;
      _M0Lm9_2acursorS930 = _M0L6_2atmpS2393 + 1;
      if (_M0L10next__charS939 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2394;
          _M0Lm6tag__0S934 = _M0Lm9_2acursorS930;
          _M0L6_2atmpS2394 = _M0Lm9_2acursorS930;
          if (_M0L6_2atmpS2394 < _M0L6_2aendS929) {
            int32_t _M0L6_2atmpS2398 = _M0Lm9_2acursorS930;
            int32_t _M0L10next__charS940;
            int32_t _M0L6_2atmpS2395;
            moonbit_incref(_M0L7_2adataS927);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS940
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS927, _M0L6_2atmpS2398);
            _M0L6_2atmpS2395 = _M0Lm9_2acursorS930;
            _M0Lm9_2acursorS930 = _M0L6_2atmpS2395 + 1;
            if (_M0L10next__charS940 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2396 = _M0Lm9_2acursorS930;
                if (_M0L6_2atmpS2396 < _M0L6_2aendS929) {
                  int32_t _M0L6_2atmpS2397 = _M0Lm9_2acursorS930;
                  _M0Lm9_2acursorS930 = _M0L6_2atmpS2397 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S933 = _M0Lm6tag__0S934;
                  _M0Lm13accept__stateS931 = 0;
                  _M0Lm10match__endS932 = _M0Lm9_2acursorS930;
                  goto join_936;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_936;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_936;
    }
    break;
  }
  goto joinlet_3995;
  join_936:;
  switch (_M0Lm13accept__stateS931) {
    case 0: {
      int32_t _M0L6_2atmpS2390;
      int32_t _M0L6_2atmpS2389;
      int64_t _M0L6_2atmpS2386;
      int32_t _M0L6_2atmpS2388;
      int64_t _M0L6_2atmpS2387;
      struct _M0TPC16string10StringView _M0L13package__nameS937;
      int64_t _M0L6_2atmpS2383;
      int32_t _M0L6_2atmpS2385;
      int64_t _M0L6_2atmpS2384;
      struct _M0TPC16string10StringView _M0L12module__nameS938;
      void* _M0L4SomeS2382;
      moonbit_decref(_M0L3pkgS925.$0);
      _M0L6_2atmpS2390 = _M0Lm20match__tag__saver__0S933;
      _M0L6_2atmpS2389 = _M0L6_2atmpS2390 + 1;
      _M0L6_2atmpS2386 = (int64_t)_M0L6_2atmpS2389;
      _M0L6_2atmpS2388 = _M0Lm10match__endS932;
      _M0L6_2atmpS2387 = (int64_t)_M0L6_2atmpS2388;
      moonbit_incref(_M0L7_2adataS927);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS937
      = _M0MPC16string6String4view(_M0L7_2adataS927, _M0L6_2atmpS2386, _M0L6_2atmpS2387);
      _M0L6_2atmpS2383 = (int64_t)_M0L8_2astartS928;
      _M0L6_2atmpS2385 = _M0Lm20match__tag__saver__0S933;
      _M0L6_2atmpS2384 = (int64_t)_M0L6_2atmpS2385;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS938
      = _M0MPC16string6String4view(_M0L7_2adataS927, _M0L6_2atmpS2383, _M0L6_2atmpS2384);
      _M0L4SomeS2382
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2382)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2382)->$0_0
      = _M0L13package__nameS937.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2382)->$0_1
      = _M0L13package__nameS937.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2382)->$0_2
      = _M0L13package__nameS937.$2;
      _M0L7_2abindS935
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS935)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS935->$0_0 = _M0L12module__nameS938.$0;
      _M0L7_2abindS935->$0_1 = _M0L12module__nameS938.$1;
      _M0L7_2abindS935->$0_2 = _M0L12module__nameS938.$2;
      _M0L7_2abindS935->$1 = _M0L4SomeS2382;
      break;
    }
    default: {
      void* _M0L4NoneS2391;
      moonbit_decref(_M0L7_2adataS927);
      _M0L4NoneS2391
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS935
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS935)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS935->$0_0 = _M0L3pkgS925.$0;
      _M0L7_2abindS935->$0_1 = _M0L3pkgS925.$1;
      _M0L7_2abindS935->$0_2 = _M0L3pkgS925.$2;
      _M0L7_2abindS935->$1 = _M0L4NoneS2391;
      break;
    }
  }
  joinlet_3995:;
  _M0L8_2afieldS3550
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS935->$0_1, _M0L7_2abindS935->$0_2, _M0L7_2abindS935->$0_0
  };
  _M0L15_2amodule__nameS944 = _M0L8_2afieldS3550;
  _M0L8_2afieldS3549 = _M0L7_2abindS935->$1;
  _M0L6_2acntS3756 = Moonbit_object_header(_M0L7_2abindS935)->rc;
  if (_M0L6_2acntS3756 > 1) {
    int32_t _M0L11_2anew__cntS3757 = _M0L6_2acntS3756 - 1;
    Moonbit_object_header(_M0L7_2abindS935)->rc = _M0L11_2anew__cntS3757;
    moonbit_incref(_M0L8_2afieldS3549);
    moonbit_incref(_M0L15_2amodule__nameS944.$0);
  } else if (_M0L6_2acntS3756 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS935);
  }
  _M0L16_2apackage__nameS945 = _M0L8_2afieldS3549;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS945)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS946 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS945;
      struct _M0TPC16string10StringView _M0L8_2afieldS3548 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS946->$0_1,
                                              _M0L7_2aSomeS946->$0_2,
                                              _M0L7_2aSomeS946->$0_0};
      int32_t _M0L6_2acntS3758 = Moonbit_object_header(_M0L7_2aSomeS946)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS947;
      if (_M0L6_2acntS3758 > 1) {
        int32_t _M0L11_2anew__cntS3759 = _M0L6_2acntS3758 - 1;
        Moonbit_object_header(_M0L7_2aSomeS946)->rc = _M0L11_2anew__cntS3759;
        moonbit_incref(_M0L8_2afieldS3548.$0);
      } else if (_M0L6_2acntS3758 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS946);
      }
      _M0L12_2apkg__nameS947 = _M0L8_2afieldS3548;
      if (_M0L6loggerS948.$1) {
        moonbit_incref(_M0L6loggerS948.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L12_2apkg__nameS947);
      if (_M0L6loggerS948.$1) {
        moonbit_incref(_M0L6loggerS948.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS948.$0->$method_3(_M0L6loggerS948.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS945);
      break;
    }
  }
  _M0L8_2afieldS3547
  = (struct _M0TPC16string10StringView){
    _M0L4selfS926->$1_1, _M0L4selfS926->$1_2, _M0L4selfS926->$1_0
  };
  _M0L8filenameS2377 = _M0L8_2afieldS3547;
  moonbit_incref(_M0L8filenameS2377.$0);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L8filenameS2377);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_3(_M0L6loggerS948.$1, 58);
  _M0L8_2afieldS3546
  = (struct _M0TPC16string10StringView){
    _M0L4selfS926->$2_1, _M0L4selfS926->$2_2, _M0L4selfS926->$2_0
  };
  _M0L11start__lineS2378 = _M0L8_2afieldS3546;
  moonbit_incref(_M0L11start__lineS2378.$0);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L11start__lineS2378);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_3(_M0L6loggerS948.$1, 58);
  _M0L8_2afieldS3545
  = (struct _M0TPC16string10StringView){
    _M0L4selfS926->$3_1, _M0L4selfS926->$3_2, _M0L4selfS926->$3_0
  };
  _M0L13start__columnS2379 = _M0L8_2afieldS3545;
  moonbit_incref(_M0L13start__columnS2379.$0);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L13start__columnS2379);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_3(_M0L6loggerS948.$1, 45);
  _M0L8_2afieldS3544
  = (struct _M0TPC16string10StringView){
    _M0L4selfS926->$4_1, _M0L4selfS926->$4_2, _M0L4selfS926->$4_0
  };
  _M0L9end__lineS2380 = _M0L8_2afieldS3544;
  moonbit_incref(_M0L9end__lineS2380.$0);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L9end__lineS2380);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_3(_M0L6loggerS948.$1, 58);
  _M0L8_2afieldS3543
  = (struct _M0TPC16string10StringView){
    _M0L4selfS926->$5_1, _M0L4selfS926->$5_2, _M0L4selfS926->$5_0
  };
  _M0L6_2acntS3760 = Moonbit_object_header(_M0L4selfS926)->rc;
  if (_M0L6_2acntS3760 > 1) {
    int32_t _M0L11_2anew__cntS3766 = _M0L6_2acntS3760 - 1;
    Moonbit_object_header(_M0L4selfS926)->rc = _M0L11_2anew__cntS3766;
    moonbit_incref(_M0L8_2afieldS3543.$0);
  } else if (_M0L6_2acntS3760 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3765 =
      (struct _M0TPC16string10StringView){_M0L4selfS926->$4_1,
                                            _M0L4selfS926->$4_2,
                                            _M0L4selfS926->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3764;
    struct _M0TPC16string10StringView _M0L8_2afieldS3763;
    struct _M0TPC16string10StringView _M0L8_2afieldS3762;
    struct _M0TPC16string10StringView _M0L8_2afieldS3761;
    moonbit_decref(_M0L8_2afieldS3765.$0);
    _M0L8_2afieldS3764
    = (struct _M0TPC16string10StringView){
      _M0L4selfS926->$3_1, _M0L4selfS926->$3_2, _M0L4selfS926->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3764.$0);
    _M0L8_2afieldS3763
    = (struct _M0TPC16string10StringView){
      _M0L4selfS926->$2_1, _M0L4selfS926->$2_2, _M0L4selfS926->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3763.$0);
    _M0L8_2afieldS3762
    = (struct _M0TPC16string10StringView){
      _M0L4selfS926->$1_1, _M0L4selfS926->$1_2, _M0L4selfS926->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3762.$0);
    _M0L8_2afieldS3761
    = (struct _M0TPC16string10StringView){
      _M0L4selfS926->$0_1, _M0L4selfS926->$0_2, _M0L4selfS926->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3761.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS926);
  }
  _M0L11end__columnS2381 = _M0L8_2afieldS3543;
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L11end__columnS2381);
  if (_M0L6loggerS948.$1) {
    moonbit_incref(_M0L6loggerS948.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_3(_M0L6loggerS948.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS948.$0->$method_2(_M0L6loggerS948.$1, _M0L15_2amodule__nameS944);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS924) {
  moonbit_string_t _M0L6_2atmpS2376;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2376 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS924);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2376);
  moonbit_decref(_M0L6_2atmpS2376);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS923,
  struct _M0TPB6Logger _M0L6loggerS922
) {
  moonbit_string_t _M0L6_2atmpS2375;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2375 = _M0MPC16double6Double10to__string(_M0L4selfS923);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS922.$0->$method_0(_M0L6loggerS922.$1, _M0L6_2atmpS2375);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS921) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS921);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS908) {
  uint64_t _M0L4bitsS909;
  uint64_t _M0L6_2atmpS2374;
  uint64_t _M0L6_2atmpS2373;
  int32_t _M0L8ieeeSignS910;
  uint64_t _M0L12ieeeMantissaS911;
  uint64_t _M0L6_2atmpS2372;
  uint64_t _M0L6_2atmpS2371;
  int32_t _M0L12ieeeExponentS912;
  int32_t _if__result_3999;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS913;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS914;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2370;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS908 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_230.data;
  }
  _M0L4bitsS909 = *(int64_t*)&_M0L3valS908;
  _M0L6_2atmpS2374 = _M0L4bitsS909 >> 63;
  _M0L6_2atmpS2373 = _M0L6_2atmpS2374 & 1ull;
  _M0L8ieeeSignS910 = _M0L6_2atmpS2373 != 0ull;
  _M0L12ieeeMantissaS911 = _M0L4bitsS909 & 4503599627370495ull;
  _M0L6_2atmpS2372 = _M0L4bitsS909 >> 52;
  _M0L6_2atmpS2371 = _M0L6_2atmpS2372 & 2047ull;
  _M0L12ieeeExponentS912 = (int32_t)_M0L6_2atmpS2371;
  if (_M0L12ieeeExponentS912 == 2047) {
    _if__result_3999 = 1;
  } else if (_M0L12ieeeExponentS912 == 0) {
    _if__result_3999 = _M0L12ieeeMantissaS911 == 0ull;
  } else {
    _if__result_3999 = 0;
  }
  if (_if__result_3999) {
    int32_t _M0L6_2atmpS2359 = _M0L12ieeeExponentS912 != 0;
    int32_t _M0L6_2atmpS2360 = _M0L12ieeeMantissaS911 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS910, _M0L6_2atmpS2359, _M0L6_2atmpS2360);
  }
  _M0Lm1vS913 = _M0FPB30ryu__to__string_2erecord_2f907;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS914
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS911, _M0L12ieeeExponentS912);
  if (_M0L5smallS914 == 0) {
    uint32_t _M0L6_2atmpS2361;
    if (_M0L5smallS914) {
      moonbit_decref(_M0L5smallS914);
    }
    _M0L6_2atmpS2361 = *(uint32_t*)&_M0L12ieeeExponentS912;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS913 = _M0FPB3d2d(_M0L12ieeeMantissaS911, _M0L6_2atmpS2361);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS915 = _M0L5smallS914;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS916 = _M0L7_2aSomeS915;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS917 = _M0L4_2afS916;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2369 = _M0Lm1xS917;
      uint64_t _M0L8_2afieldS3554 = _M0L6_2atmpS2369->$0;
      uint64_t _M0L8mantissaS2368 = _M0L8_2afieldS3554;
      uint64_t _M0L1qS918 = _M0L8mantissaS2368 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2367 = _M0Lm1xS917;
      uint64_t _M0L8_2afieldS3553 = _M0L6_2atmpS2367->$0;
      uint64_t _M0L8mantissaS2365 = _M0L8_2afieldS3553;
      uint64_t _M0L6_2atmpS2366 = 10ull * _M0L1qS918;
      uint64_t _M0L1rS919 = _M0L8mantissaS2365 - _M0L6_2atmpS2366;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2364;
      int32_t _M0L8_2afieldS3552;
      int32_t _M0L8exponentS2363;
      int32_t _M0L6_2atmpS2362;
      if (_M0L1rS919 != 0ull) {
        break;
      }
      _M0L6_2atmpS2364 = _M0Lm1xS917;
      _M0L8_2afieldS3552 = _M0L6_2atmpS2364->$1;
      moonbit_decref(_M0L6_2atmpS2364);
      _M0L8exponentS2363 = _M0L8_2afieldS3552;
      _M0L6_2atmpS2362 = _M0L8exponentS2363 + 1;
      _M0Lm1xS917
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS917)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS917->$0 = _M0L1qS918;
      _M0Lm1xS917->$1 = _M0L6_2atmpS2362;
      continue;
      break;
    }
    _M0Lm1vS913 = _M0Lm1xS917;
  }
  _M0L6_2atmpS2370 = _M0Lm1vS913;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2370, _M0L8ieeeSignS910);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS902,
  int32_t _M0L12ieeeExponentS904
) {
  uint64_t _M0L2m2S901;
  int32_t _M0L6_2atmpS2358;
  int32_t _M0L2e2S903;
  int32_t _M0L6_2atmpS2357;
  uint64_t _M0L6_2atmpS2356;
  uint64_t _M0L4maskS905;
  uint64_t _M0L8fractionS906;
  int32_t _M0L6_2atmpS2355;
  uint64_t _M0L6_2atmpS2354;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2353;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S901 = 4503599627370496ull | _M0L12ieeeMantissaS902;
  _M0L6_2atmpS2358 = _M0L12ieeeExponentS904 - 1023;
  _M0L2e2S903 = _M0L6_2atmpS2358 - 52;
  if (_M0L2e2S903 > 0) {
    return 0;
  }
  if (_M0L2e2S903 < -52) {
    return 0;
  }
  _M0L6_2atmpS2357 = -_M0L2e2S903;
  _M0L6_2atmpS2356 = 1ull << (_M0L6_2atmpS2357 & 63);
  _M0L4maskS905 = _M0L6_2atmpS2356 - 1ull;
  _M0L8fractionS906 = _M0L2m2S901 & _M0L4maskS905;
  if (_M0L8fractionS906 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2355 = -_M0L2e2S903;
  _M0L6_2atmpS2354 = _M0L2m2S901 >> (_M0L6_2atmpS2355 & 63);
  _M0L6_2atmpS2353
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2353)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2353->$0 = _M0L6_2atmpS2354;
  _M0L6_2atmpS2353->$1 = 0;
  return _M0L6_2atmpS2353;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS875,
  int32_t _M0L4signS873
) {
  int32_t _M0L6_2atmpS2352;
  moonbit_bytes_t _M0L6resultS871;
  int32_t _M0Lm5indexS872;
  uint64_t _M0Lm6outputS874;
  uint64_t _M0L6_2atmpS2351;
  int32_t _M0L7olengthS876;
  int32_t _M0L8_2afieldS3555;
  int32_t _M0L8exponentS2350;
  int32_t _M0L6_2atmpS2349;
  int32_t _M0Lm3expS877;
  int32_t _M0L6_2atmpS2348;
  int32_t _M0L6_2atmpS2346;
  int32_t _M0L18scientificNotationS878;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2352 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS871 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2352);
  _M0Lm5indexS872 = 0;
  if (_M0L4signS873) {
    int32_t _M0L6_2atmpS2221 = _M0Lm5indexS872;
    int32_t _M0L6_2atmpS2222;
    if (
      _M0L6_2atmpS2221 < 0
      || _M0L6_2atmpS2221 >= Moonbit_array_length(_M0L6resultS871)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS871[_M0L6_2atmpS2221] = 45;
    _M0L6_2atmpS2222 = _M0Lm5indexS872;
    _M0Lm5indexS872 = _M0L6_2atmpS2222 + 1;
  }
  _M0Lm6outputS874 = _M0L1vS875->$0;
  _M0L6_2atmpS2351 = _M0Lm6outputS874;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS876 = _M0FPB17decimal__length17(_M0L6_2atmpS2351);
  _M0L8_2afieldS3555 = _M0L1vS875->$1;
  moonbit_decref(_M0L1vS875);
  _M0L8exponentS2350 = _M0L8_2afieldS3555;
  _M0L6_2atmpS2349 = _M0L8exponentS2350 + _M0L7olengthS876;
  _M0Lm3expS877 = _M0L6_2atmpS2349 - 1;
  _M0L6_2atmpS2348 = _M0Lm3expS877;
  if (_M0L6_2atmpS2348 >= -6) {
    int32_t _M0L6_2atmpS2347 = _M0Lm3expS877;
    _M0L6_2atmpS2346 = _M0L6_2atmpS2347 < 21;
  } else {
    _M0L6_2atmpS2346 = 0;
  }
  _M0L18scientificNotationS878 = !_M0L6_2atmpS2346;
  if (_M0L18scientificNotationS878) {
    int32_t _M0L7_2abindS879 = _M0L7olengthS876 - 1;
    int32_t _M0L1iS880 = 0;
    int32_t _M0L6_2atmpS2232;
    uint64_t _M0L6_2atmpS2237;
    int32_t _M0L6_2atmpS2236;
    int32_t _M0L6_2atmpS2235;
    int32_t _M0L6_2atmpS2234;
    int32_t _M0L6_2atmpS2233;
    int32_t _M0L6_2atmpS2241;
    int32_t _M0L6_2atmpS2242;
    int32_t _M0L6_2atmpS2243;
    int32_t _M0L6_2atmpS2244;
    int32_t _M0L6_2atmpS2245;
    int32_t _M0L6_2atmpS2251;
    int32_t _M0L6_2atmpS2284;
    while (1) {
      if (_M0L1iS880 < _M0L7_2abindS879) {
        uint64_t _M0L6_2atmpS2230 = _M0Lm6outputS874;
        uint64_t _M0L1cS881 = _M0L6_2atmpS2230 % 10ull;
        uint64_t _M0L6_2atmpS2223 = _M0Lm6outputS874;
        int32_t _M0L6_2atmpS2229;
        int32_t _M0L6_2atmpS2228;
        int32_t _M0L6_2atmpS2224;
        int32_t _M0L6_2atmpS2227;
        int32_t _M0L6_2atmpS2226;
        int32_t _M0L6_2atmpS2225;
        int32_t _M0L6_2atmpS2231;
        _M0Lm6outputS874 = _M0L6_2atmpS2223 / 10ull;
        _M0L6_2atmpS2229 = _M0Lm5indexS872;
        _M0L6_2atmpS2228 = _M0L6_2atmpS2229 + _M0L7olengthS876;
        _M0L6_2atmpS2224 = _M0L6_2atmpS2228 - _M0L1iS880;
        _M0L6_2atmpS2227 = (int32_t)_M0L1cS881;
        _M0L6_2atmpS2226 = 48 + _M0L6_2atmpS2227;
        _M0L6_2atmpS2225 = _M0L6_2atmpS2226 & 0xff;
        if (
          _M0L6_2atmpS2224 < 0
          || _M0L6_2atmpS2224 >= Moonbit_array_length(_M0L6resultS871)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS871[_M0L6_2atmpS2224] = _M0L6_2atmpS2225;
        _M0L6_2atmpS2231 = _M0L1iS880 + 1;
        _M0L1iS880 = _M0L6_2atmpS2231;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2232 = _M0Lm5indexS872;
    _M0L6_2atmpS2237 = _M0Lm6outputS874;
    _M0L6_2atmpS2236 = (int32_t)_M0L6_2atmpS2237;
    _M0L6_2atmpS2235 = _M0L6_2atmpS2236 % 10;
    _M0L6_2atmpS2234 = 48 + _M0L6_2atmpS2235;
    _M0L6_2atmpS2233 = _M0L6_2atmpS2234 & 0xff;
    if (
      _M0L6_2atmpS2232 < 0
      || _M0L6_2atmpS2232 >= Moonbit_array_length(_M0L6resultS871)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS871[_M0L6_2atmpS2232] = _M0L6_2atmpS2233;
    if (_M0L7olengthS876 > 1) {
      int32_t _M0L6_2atmpS2239 = _M0Lm5indexS872;
      int32_t _M0L6_2atmpS2238 = _M0L6_2atmpS2239 + 1;
      if (
        _M0L6_2atmpS2238 < 0
        || _M0L6_2atmpS2238 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2238] = 46;
    } else {
      int32_t _M0L6_2atmpS2240 = _M0Lm5indexS872;
      _M0Lm5indexS872 = _M0L6_2atmpS2240 - 1;
    }
    _M0L6_2atmpS2241 = _M0Lm5indexS872;
    _M0L6_2atmpS2242 = _M0L7olengthS876 + 1;
    _M0Lm5indexS872 = _M0L6_2atmpS2241 + _M0L6_2atmpS2242;
    _M0L6_2atmpS2243 = _M0Lm5indexS872;
    if (
      _M0L6_2atmpS2243 < 0
      || _M0L6_2atmpS2243 >= Moonbit_array_length(_M0L6resultS871)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS871[_M0L6_2atmpS2243] = 101;
    _M0L6_2atmpS2244 = _M0Lm5indexS872;
    _M0Lm5indexS872 = _M0L6_2atmpS2244 + 1;
    _M0L6_2atmpS2245 = _M0Lm3expS877;
    if (_M0L6_2atmpS2245 < 0) {
      int32_t _M0L6_2atmpS2246 = _M0Lm5indexS872;
      int32_t _M0L6_2atmpS2247;
      int32_t _M0L6_2atmpS2248;
      if (
        _M0L6_2atmpS2246 < 0
        || _M0L6_2atmpS2246 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2246] = 45;
      _M0L6_2atmpS2247 = _M0Lm5indexS872;
      _M0Lm5indexS872 = _M0L6_2atmpS2247 + 1;
      _M0L6_2atmpS2248 = _M0Lm3expS877;
      _M0Lm3expS877 = -_M0L6_2atmpS2248;
    } else {
      int32_t _M0L6_2atmpS2249 = _M0Lm5indexS872;
      int32_t _M0L6_2atmpS2250;
      if (
        _M0L6_2atmpS2249 < 0
        || _M0L6_2atmpS2249 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2249] = 43;
      _M0L6_2atmpS2250 = _M0Lm5indexS872;
      _M0Lm5indexS872 = _M0L6_2atmpS2250 + 1;
    }
    _M0L6_2atmpS2251 = _M0Lm3expS877;
    if (_M0L6_2atmpS2251 >= 100) {
      int32_t _M0L6_2atmpS2267 = _M0Lm3expS877;
      int32_t _M0L1aS883 = _M0L6_2atmpS2267 / 100;
      int32_t _M0L6_2atmpS2266 = _M0Lm3expS877;
      int32_t _M0L6_2atmpS2265 = _M0L6_2atmpS2266 / 10;
      int32_t _M0L1bS884 = _M0L6_2atmpS2265 % 10;
      int32_t _M0L6_2atmpS2264 = _M0Lm3expS877;
      int32_t _M0L1cS885 = _M0L6_2atmpS2264 % 10;
      int32_t _M0L6_2atmpS2252 = _M0Lm5indexS872;
      int32_t _M0L6_2atmpS2254 = 48 + _M0L1aS883;
      int32_t _M0L6_2atmpS2253 = _M0L6_2atmpS2254 & 0xff;
      int32_t _M0L6_2atmpS2258;
      int32_t _M0L6_2atmpS2255;
      int32_t _M0L6_2atmpS2257;
      int32_t _M0L6_2atmpS2256;
      int32_t _M0L6_2atmpS2262;
      int32_t _M0L6_2atmpS2259;
      int32_t _M0L6_2atmpS2261;
      int32_t _M0L6_2atmpS2260;
      int32_t _M0L6_2atmpS2263;
      if (
        _M0L6_2atmpS2252 < 0
        || _M0L6_2atmpS2252 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2252] = _M0L6_2atmpS2253;
      _M0L6_2atmpS2258 = _M0Lm5indexS872;
      _M0L6_2atmpS2255 = _M0L6_2atmpS2258 + 1;
      _M0L6_2atmpS2257 = 48 + _M0L1bS884;
      _M0L6_2atmpS2256 = _M0L6_2atmpS2257 & 0xff;
      if (
        _M0L6_2atmpS2255 < 0
        || _M0L6_2atmpS2255 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2255] = _M0L6_2atmpS2256;
      _M0L6_2atmpS2262 = _M0Lm5indexS872;
      _M0L6_2atmpS2259 = _M0L6_2atmpS2262 + 2;
      _M0L6_2atmpS2261 = 48 + _M0L1cS885;
      _M0L6_2atmpS2260 = _M0L6_2atmpS2261 & 0xff;
      if (
        _M0L6_2atmpS2259 < 0
        || _M0L6_2atmpS2259 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2259] = _M0L6_2atmpS2260;
      _M0L6_2atmpS2263 = _M0Lm5indexS872;
      _M0Lm5indexS872 = _M0L6_2atmpS2263 + 3;
    } else {
      int32_t _M0L6_2atmpS2268 = _M0Lm3expS877;
      if (_M0L6_2atmpS2268 >= 10) {
        int32_t _M0L6_2atmpS2278 = _M0Lm3expS877;
        int32_t _M0L1aS886 = _M0L6_2atmpS2278 / 10;
        int32_t _M0L6_2atmpS2277 = _M0Lm3expS877;
        int32_t _M0L1bS887 = _M0L6_2atmpS2277 % 10;
        int32_t _M0L6_2atmpS2269 = _M0Lm5indexS872;
        int32_t _M0L6_2atmpS2271 = 48 + _M0L1aS886;
        int32_t _M0L6_2atmpS2270 = _M0L6_2atmpS2271 & 0xff;
        int32_t _M0L6_2atmpS2275;
        int32_t _M0L6_2atmpS2272;
        int32_t _M0L6_2atmpS2274;
        int32_t _M0L6_2atmpS2273;
        int32_t _M0L6_2atmpS2276;
        if (
          _M0L6_2atmpS2269 < 0
          || _M0L6_2atmpS2269 >= Moonbit_array_length(_M0L6resultS871)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS871[_M0L6_2atmpS2269] = _M0L6_2atmpS2270;
        _M0L6_2atmpS2275 = _M0Lm5indexS872;
        _M0L6_2atmpS2272 = _M0L6_2atmpS2275 + 1;
        _M0L6_2atmpS2274 = 48 + _M0L1bS887;
        _M0L6_2atmpS2273 = _M0L6_2atmpS2274 & 0xff;
        if (
          _M0L6_2atmpS2272 < 0
          || _M0L6_2atmpS2272 >= Moonbit_array_length(_M0L6resultS871)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS871[_M0L6_2atmpS2272] = _M0L6_2atmpS2273;
        _M0L6_2atmpS2276 = _M0Lm5indexS872;
        _M0Lm5indexS872 = _M0L6_2atmpS2276 + 2;
      } else {
        int32_t _M0L6_2atmpS2279 = _M0Lm5indexS872;
        int32_t _M0L6_2atmpS2282 = _M0Lm3expS877;
        int32_t _M0L6_2atmpS2281 = 48 + _M0L6_2atmpS2282;
        int32_t _M0L6_2atmpS2280 = _M0L6_2atmpS2281 & 0xff;
        int32_t _M0L6_2atmpS2283;
        if (
          _M0L6_2atmpS2279 < 0
          || _M0L6_2atmpS2279 >= Moonbit_array_length(_M0L6resultS871)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS871[_M0L6_2atmpS2279] = _M0L6_2atmpS2280;
        _M0L6_2atmpS2283 = _M0Lm5indexS872;
        _M0Lm5indexS872 = _M0L6_2atmpS2283 + 1;
      }
    }
    _M0L6_2atmpS2284 = _M0Lm5indexS872;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS871, 0, _M0L6_2atmpS2284);
  } else {
    int32_t _M0L6_2atmpS2285 = _M0Lm3expS877;
    int32_t _M0L6_2atmpS2345;
    if (_M0L6_2atmpS2285 < 0) {
      int32_t _M0L6_2atmpS2286 = _M0Lm5indexS872;
      int32_t _M0L6_2atmpS2287;
      int32_t _M0L6_2atmpS2288;
      int32_t _M0L6_2atmpS2289;
      int32_t _M0L1iS888;
      int32_t _M0L7currentS890;
      int32_t _M0L1iS891;
      if (
        _M0L6_2atmpS2286 < 0
        || _M0L6_2atmpS2286 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2286] = 48;
      _M0L6_2atmpS2287 = _M0Lm5indexS872;
      _M0Lm5indexS872 = _M0L6_2atmpS2287 + 1;
      _M0L6_2atmpS2288 = _M0Lm5indexS872;
      if (
        _M0L6_2atmpS2288 < 0
        || _M0L6_2atmpS2288 >= Moonbit_array_length(_M0L6resultS871)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS871[_M0L6_2atmpS2288] = 46;
      _M0L6_2atmpS2289 = _M0Lm5indexS872;
      _M0Lm5indexS872 = _M0L6_2atmpS2289 + 1;
      _M0L1iS888 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2290 = _M0Lm3expS877;
        if (_M0L1iS888 > _M0L6_2atmpS2290) {
          int32_t _M0L6_2atmpS2291 = _M0Lm5indexS872;
          int32_t _M0L6_2atmpS2292;
          int32_t _M0L6_2atmpS2293;
          if (
            _M0L6_2atmpS2291 < 0
            || _M0L6_2atmpS2291 >= Moonbit_array_length(_M0L6resultS871)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS871[_M0L6_2atmpS2291] = 48;
          _M0L6_2atmpS2292 = _M0Lm5indexS872;
          _M0Lm5indexS872 = _M0L6_2atmpS2292 + 1;
          _M0L6_2atmpS2293 = _M0L1iS888 - 1;
          _M0L1iS888 = _M0L6_2atmpS2293;
          continue;
        }
        break;
      }
      _M0L7currentS890 = _M0Lm5indexS872;
      _M0L1iS891 = 0;
      while (1) {
        if (_M0L1iS891 < _M0L7olengthS876) {
          int32_t _M0L6_2atmpS2301 = _M0L7currentS890 + _M0L7olengthS876;
          int32_t _M0L6_2atmpS2300 = _M0L6_2atmpS2301 - _M0L1iS891;
          int32_t _M0L6_2atmpS2294 = _M0L6_2atmpS2300 - 1;
          uint64_t _M0L6_2atmpS2299 = _M0Lm6outputS874;
          uint64_t _M0L6_2atmpS2298 = _M0L6_2atmpS2299 % 10ull;
          int32_t _M0L6_2atmpS2297 = (int32_t)_M0L6_2atmpS2298;
          int32_t _M0L6_2atmpS2296 = 48 + _M0L6_2atmpS2297;
          int32_t _M0L6_2atmpS2295 = _M0L6_2atmpS2296 & 0xff;
          uint64_t _M0L6_2atmpS2302;
          int32_t _M0L6_2atmpS2303;
          int32_t _M0L6_2atmpS2304;
          if (
            _M0L6_2atmpS2294 < 0
            || _M0L6_2atmpS2294 >= Moonbit_array_length(_M0L6resultS871)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS871[_M0L6_2atmpS2294] = _M0L6_2atmpS2295;
          _M0L6_2atmpS2302 = _M0Lm6outputS874;
          _M0Lm6outputS874 = _M0L6_2atmpS2302 / 10ull;
          _M0L6_2atmpS2303 = _M0Lm5indexS872;
          _M0Lm5indexS872 = _M0L6_2atmpS2303 + 1;
          _M0L6_2atmpS2304 = _M0L1iS891 + 1;
          _M0L1iS891 = _M0L6_2atmpS2304;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2306 = _M0Lm3expS877;
      int32_t _M0L6_2atmpS2305 = _M0L6_2atmpS2306 + 1;
      if (_M0L6_2atmpS2305 >= _M0L7olengthS876) {
        int32_t _M0L1iS893 = 0;
        int32_t _M0L6_2atmpS2318;
        int32_t _M0L6_2atmpS2322;
        int32_t _M0L7_2abindS895;
        int32_t _M0L2__S896;
        while (1) {
          if (_M0L1iS893 < _M0L7olengthS876) {
            int32_t _M0L6_2atmpS2315 = _M0Lm5indexS872;
            int32_t _M0L6_2atmpS2314 = _M0L6_2atmpS2315 + _M0L7olengthS876;
            int32_t _M0L6_2atmpS2313 = _M0L6_2atmpS2314 - _M0L1iS893;
            int32_t _M0L6_2atmpS2307 = _M0L6_2atmpS2313 - 1;
            uint64_t _M0L6_2atmpS2312 = _M0Lm6outputS874;
            uint64_t _M0L6_2atmpS2311 = _M0L6_2atmpS2312 % 10ull;
            int32_t _M0L6_2atmpS2310 = (int32_t)_M0L6_2atmpS2311;
            int32_t _M0L6_2atmpS2309 = 48 + _M0L6_2atmpS2310;
            int32_t _M0L6_2atmpS2308 = _M0L6_2atmpS2309 & 0xff;
            uint64_t _M0L6_2atmpS2316;
            int32_t _M0L6_2atmpS2317;
            if (
              _M0L6_2atmpS2307 < 0
              || _M0L6_2atmpS2307 >= Moonbit_array_length(_M0L6resultS871)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS871[_M0L6_2atmpS2307] = _M0L6_2atmpS2308;
            _M0L6_2atmpS2316 = _M0Lm6outputS874;
            _M0Lm6outputS874 = _M0L6_2atmpS2316 / 10ull;
            _M0L6_2atmpS2317 = _M0L1iS893 + 1;
            _M0L1iS893 = _M0L6_2atmpS2317;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2318 = _M0Lm5indexS872;
        _M0Lm5indexS872 = _M0L6_2atmpS2318 + _M0L7olengthS876;
        _M0L6_2atmpS2322 = _M0Lm3expS877;
        _M0L7_2abindS895 = _M0L6_2atmpS2322 + 1;
        _M0L2__S896 = _M0L7olengthS876;
        while (1) {
          if (_M0L2__S896 < _M0L7_2abindS895) {
            int32_t _M0L6_2atmpS2319 = _M0Lm5indexS872;
            int32_t _M0L6_2atmpS2320;
            int32_t _M0L6_2atmpS2321;
            if (
              _M0L6_2atmpS2319 < 0
              || _M0L6_2atmpS2319 >= Moonbit_array_length(_M0L6resultS871)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS871[_M0L6_2atmpS2319] = 48;
            _M0L6_2atmpS2320 = _M0Lm5indexS872;
            _M0Lm5indexS872 = _M0L6_2atmpS2320 + 1;
            _M0L6_2atmpS2321 = _M0L2__S896 + 1;
            _M0L2__S896 = _M0L6_2atmpS2321;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2344 = _M0Lm5indexS872;
        int32_t _M0Lm7currentS898 = _M0L6_2atmpS2344 + 1;
        int32_t _M0L1iS899 = 0;
        int32_t _M0L6_2atmpS2342;
        int32_t _M0L6_2atmpS2343;
        while (1) {
          if (_M0L1iS899 < _M0L7olengthS876) {
            int32_t _M0L6_2atmpS2325 = _M0L7olengthS876 - _M0L1iS899;
            int32_t _M0L6_2atmpS2323 = _M0L6_2atmpS2325 - 1;
            int32_t _M0L6_2atmpS2324 = _M0Lm3expS877;
            int32_t _M0L6_2atmpS2339;
            int32_t _M0L6_2atmpS2338;
            int32_t _M0L6_2atmpS2337;
            int32_t _M0L6_2atmpS2331;
            uint64_t _M0L6_2atmpS2336;
            uint64_t _M0L6_2atmpS2335;
            int32_t _M0L6_2atmpS2334;
            int32_t _M0L6_2atmpS2333;
            int32_t _M0L6_2atmpS2332;
            uint64_t _M0L6_2atmpS2340;
            int32_t _M0L6_2atmpS2341;
            if (_M0L6_2atmpS2323 == _M0L6_2atmpS2324) {
              int32_t _M0L6_2atmpS2329 = _M0Lm7currentS898;
              int32_t _M0L6_2atmpS2328 = _M0L6_2atmpS2329 + _M0L7olengthS876;
              int32_t _M0L6_2atmpS2327 = _M0L6_2atmpS2328 - _M0L1iS899;
              int32_t _M0L6_2atmpS2326 = _M0L6_2atmpS2327 - 1;
              int32_t _M0L6_2atmpS2330;
              if (
                _M0L6_2atmpS2326 < 0
                || _M0L6_2atmpS2326 >= Moonbit_array_length(_M0L6resultS871)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS871[_M0L6_2atmpS2326] = 46;
              _M0L6_2atmpS2330 = _M0Lm7currentS898;
              _M0Lm7currentS898 = _M0L6_2atmpS2330 - 1;
            }
            _M0L6_2atmpS2339 = _M0Lm7currentS898;
            _M0L6_2atmpS2338 = _M0L6_2atmpS2339 + _M0L7olengthS876;
            _M0L6_2atmpS2337 = _M0L6_2atmpS2338 - _M0L1iS899;
            _M0L6_2atmpS2331 = _M0L6_2atmpS2337 - 1;
            _M0L6_2atmpS2336 = _M0Lm6outputS874;
            _M0L6_2atmpS2335 = _M0L6_2atmpS2336 % 10ull;
            _M0L6_2atmpS2334 = (int32_t)_M0L6_2atmpS2335;
            _M0L6_2atmpS2333 = 48 + _M0L6_2atmpS2334;
            _M0L6_2atmpS2332 = _M0L6_2atmpS2333 & 0xff;
            if (
              _M0L6_2atmpS2331 < 0
              || _M0L6_2atmpS2331 >= Moonbit_array_length(_M0L6resultS871)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS871[_M0L6_2atmpS2331] = _M0L6_2atmpS2332;
            _M0L6_2atmpS2340 = _M0Lm6outputS874;
            _M0Lm6outputS874 = _M0L6_2atmpS2340 / 10ull;
            _M0L6_2atmpS2341 = _M0L1iS899 + 1;
            _M0L1iS899 = _M0L6_2atmpS2341;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2342 = _M0Lm5indexS872;
        _M0L6_2atmpS2343 = _M0L7olengthS876 + 1;
        _M0Lm5indexS872 = _M0L6_2atmpS2342 + _M0L6_2atmpS2343;
      }
    }
    _M0L6_2atmpS2345 = _M0Lm5indexS872;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS871, 0, _M0L6_2atmpS2345);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS817,
  uint32_t _M0L12ieeeExponentS816
) {
  int32_t _M0Lm2e2S814;
  uint64_t _M0Lm2m2S815;
  uint64_t _M0L6_2atmpS2220;
  uint64_t _M0L6_2atmpS2219;
  int32_t _M0L4evenS818;
  uint64_t _M0L6_2atmpS2218;
  uint64_t _M0L2mvS819;
  int32_t _M0L7mmShiftS820;
  uint64_t _M0Lm2vrS821;
  uint64_t _M0Lm2vpS822;
  uint64_t _M0Lm2vmS823;
  int32_t _M0Lm3e10S824;
  int32_t _M0Lm17vmIsTrailingZerosS825;
  int32_t _M0Lm17vrIsTrailingZerosS826;
  int32_t _M0L6_2atmpS2120;
  int32_t _M0Lm7removedS845;
  int32_t _M0Lm16lastRemovedDigitS846;
  uint64_t _M0Lm6outputS847;
  int32_t _M0L6_2atmpS2216;
  int32_t _M0L6_2atmpS2217;
  int32_t _M0L3expS870;
  uint64_t _M0L6_2atmpS2215;
  struct _M0TPB17FloatingDecimal64* _block_4012;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S814 = 0;
  _M0Lm2m2S815 = 0ull;
  if (_M0L12ieeeExponentS816 == 0u) {
    _M0Lm2e2S814 = -1076;
    _M0Lm2m2S815 = _M0L12ieeeMantissaS817;
  } else {
    int32_t _M0L6_2atmpS2119 = *(int32_t*)&_M0L12ieeeExponentS816;
    int32_t _M0L6_2atmpS2118 = _M0L6_2atmpS2119 - 1023;
    int32_t _M0L6_2atmpS2117 = _M0L6_2atmpS2118 - 52;
    _M0Lm2e2S814 = _M0L6_2atmpS2117 - 2;
    _M0Lm2m2S815 = 4503599627370496ull | _M0L12ieeeMantissaS817;
  }
  _M0L6_2atmpS2220 = _M0Lm2m2S815;
  _M0L6_2atmpS2219 = _M0L6_2atmpS2220 & 1ull;
  _M0L4evenS818 = _M0L6_2atmpS2219 == 0ull;
  _M0L6_2atmpS2218 = _M0Lm2m2S815;
  _M0L2mvS819 = 4ull * _M0L6_2atmpS2218;
  if (_M0L12ieeeMantissaS817 != 0ull) {
    _M0L7mmShiftS820 = 1;
  } else {
    _M0L7mmShiftS820 = _M0L12ieeeExponentS816 <= 1u;
  }
  _M0Lm2vrS821 = 0ull;
  _M0Lm2vpS822 = 0ull;
  _M0Lm2vmS823 = 0ull;
  _M0Lm3e10S824 = 0;
  _M0Lm17vmIsTrailingZerosS825 = 0;
  _M0Lm17vrIsTrailingZerosS826 = 0;
  _M0L6_2atmpS2120 = _M0Lm2e2S814;
  if (_M0L6_2atmpS2120 >= 0) {
    int32_t _M0L6_2atmpS2142 = _M0Lm2e2S814;
    int32_t _M0L6_2atmpS2138;
    int32_t _M0L6_2atmpS2141;
    int32_t _M0L6_2atmpS2140;
    int32_t _M0L6_2atmpS2139;
    int32_t _M0L1qS827;
    int32_t _M0L6_2atmpS2137;
    int32_t _M0L6_2atmpS2136;
    int32_t _M0L1kS828;
    int32_t _M0L6_2atmpS2135;
    int32_t _M0L6_2atmpS2134;
    int32_t _M0L6_2atmpS2133;
    int32_t _M0L1iS829;
    struct _M0TPB8Pow5Pair _M0L4pow5S830;
    uint64_t _M0L6_2atmpS2132;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS831;
    uint64_t _M0L8_2avrOutS832;
    uint64_t _M0L8_2avpOutS833;
    uint64_t _M0L8_2avmOutS834;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2138 = _M0FPB9log10Pow2(_M0L6_2atmpS2142);
    _M0L6_2atmpS2141 = _M0Lm2e2S814;
    _M0L6_2atmpS2140 = _M0L6_2atmpS2141 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2139 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2140);
    _M0L1qS827 = _M0L6_2atmpS2138 - _M0L6_2atmpS2139;
    _M0Lm3e10S824 = _M0L1qS827;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2137 = _M0FPB8pow5bits(_M0L1qS827);
    _M0L6_2atmpS2136 = 125 + _M0L6_2atmpS2137;
    _M0L1kS828 = _M0L6_2atmpS2136 - 1;
    _M0L6_2atmpS2135 = _M0Lm2e2S814;
    _M0L6_2atmpS2134 = -_M0L6_2atmpS2135;
    _M0L6_2atmpS2133 = _M0L6_2atmpS2134 + _M0L1qS827;
    _M0L1iS829 = _M0L6_2atmpS2133 + _M0L1kS828;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S830 = _M0FPB22double__computeInvPow5(_M0L1qS827);
    _M0L6_2atmpS2132 = _M0Lm2m2S815;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS831
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2132, _M0L4pow5S830, _M0L1iS829, _M0L7mmShiftS820);
    _M0L8_2avrOutS832 = _M0L7_2abindS831.$0;
    _M0L8_2avpOutS833 = _M0L7_2abindS831.$1;
    _M0L8_2avmOutS834 = _M0L7_2abindS831.$2;
    _M0Lm2vrS821 = _M0L8_2avrOutS832;
    _M0Lm2vpS822 = _M0L8_2avpOutS833;
    _M0Lm2vmS823 = _M0L8_2avmOutS834;
    if (_M0L1qS827 <= 21) {
      int32_t _M0L6_2atmpS2128 = (int32_t)_M0L2mvS819;
      uint64_t _M0L6_2atmpS2131 = _M0L2mvS819 / 5ull;
      int32_t _M0L6_2atmpS2130 = (int32_t)_M0L6_2atmpS2131;
      int32_t _M0L6_2atmpS2129 = 5 * _M0L6_2atmpS2130;
      int32_t _M0L6mvMod5S835 = _M0L6_2atmpS2128 - _M0L6_2atmpS2129;
      if (_M0L6mvMod5S835 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS826
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS819, _M0L1qS827);
      } else if (_M0L4evenS818) {
        uint64_t _M0L6_2atmpS2122 = _M0L2mvS819 - 1ull;
        uint64_t _M0L6_2atmpS2123;
        uint64_t _M0L6_2atmpS2121;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2123 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS820);
        _M0L6_2atmpS2121 = _M0L6_2atmpS2122 - _M0L6_2atmpS2123;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS825
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2121, _M0L1qS827);
      } else {
        uint64_t _M0L6_2atmpS2124 = _M0Lm2vpS822;
        uint64_t _M0L6_2atmpS2127 = _M0L2mvS819 + 2ull;
        int32_t _M0L6_2atmpS2126;
        uint64_t _M0L6_2atmpS2125;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2126
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2127, _M0L1qS827);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2125 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2126);
        _M0Lm2vpS822 = _M0L6_2atmpS2124 - _M0L6_2atmpS2125;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2156 = _M0Lm2e2S814;
    int32_t _M0L6_2atmpS2155 = -_M0L6_2atmpS2156;
    int32_t _M0L6_2atmpS2150;
    int32_t _M0L6_2atmpS2154;
    int32_t _M0L6_2atmpS2153;
    int32_t _M0L6_2atmpS2152;
    int32_t _M0L6_2atmpS2151;
    int32_t _M0L1qS836;
    int32_t _M0L6_2atmpS2143;
    int32_t _M0L6_2atmpS2149;
    int32_t _M0L6_2atmpS2148;
    int32_t _M0L1iS837;
    int32_t _M0L6_2atmpS2147;
    int32_t _M0L1kS838;
    int32_t _M0L1jS839;
    struct _M0TPB8Pow5Pair _M0L4pow5S840;
    uint64_t _M0L6_2atmpS2146;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS841;
    uint64_t _M0L8_2avrOutS842;
    uint64_t _M0L8_2avpOutS843;
    uint64_t _M0L8_2avmOutS844;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2150 = _M0FPB9log10Pow5(_M0L6_2atmpS2155);
    _M0L6_2atmpS2154 = _M0Lm2e2S814;
    _M0L6_2atmpS2153 = -_M0L6_2atmpS2154;
    _M0L6_2atmpS2152 = _M0L6_2atmpS2153 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2151 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2152);
    _M0L1qS836 = _M0L6_2atmpS2150 - _M0L6_2atmpS2151;
    _M0L6_2atmpS2143 = _M0Lm2e2S814;
    _M0Lm3e10S824 = _M0L1qS836 + _M0L6_2atmpS2143;
    _M0L6_2atmpS2149 = _M0Lm2e2S814;
    _M0L6_2atmpS2148 = -_M0L6_2atmpS2149;
    _M0L1iS837 = _M0L6_2atmpS2148 - _M0L1qS836;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2147 = _M0FPB8pow5bits(_M0L1iS837);
    _M0L1kS838 = _M0L6_2atmpS2147 - 125;
    _M0L1jS839 = _M0L1qS836 - _M0L1kS838;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S840 = _M0FPB19double__computePow5(_M0L1iS837);
    _M0L6_2atmpS2146 = _M0Lm2m2S815;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS841
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2146, _M0L4pow5S840, _M0L1jS839, _M0L7mmShiftS820);
    _M0L8_2avrOutS842 = _M0L7_2abindS841.$0;
    _M0L8_2avpOutS843 = _M0L7_2abindS841.$1;
    _M0L8_2avmOutS844 = _M0L7_2abindS841.$2;
    _M0Lm2vrS821 = _M0L8_2avrOutS842;
    _M0Lm2vpS822 = _M0L8_2avpOutS843;
    _M0Lm2vmS823 = _M0L8_2avmOutS844;
    if (_M0L1qS836 <= 1) {
      _M0Lm17vrIsTrailingZerosS826 = 1;
      if (_M0L4evenS818) {
        int32_t _M0L6_2atmpS2144;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2144 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS820);
        _M0Lm17vmIsTrailingZerosS825 = _M0L6_2atmpS2144 == 1;
      } else {
        uint64_t _M0L6_2atmpS2145 = _M0Lm2vpS822;
        _M0Lm2vpS822 = _M0L6_2atmpS2145 - 1ull;
      }
    } else if (_M0L1qS836 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS826
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS819, _M0L1qS836);
    }
  }
  _M0Lm7removedS845 = 0;
  _M0Lm16lastRemovedDigitS846 = 0;
  _M0Lm6outputS847 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS825 || _M0Lm17vrIsTrailingZerosS826) {
    int32_t _if__result_4009;
    uint64_t _M0L6_2atmpS2186;
    uint64_t _M0L6_2atmpS2192;
    uint64_t _M0L6_2atmpS2193;
    int32_t _if__result_4010;
    int32_t _M0L6_2atmpS2189;
    int64_t _M0L6_2atmpS2188;
    uint64_t _M0L6_2atmpS2187;
    while (1) {
      uint64_t _M0L6_2atmpS2169 = _M0Lm2vpS822;
      uint64_t _M0L7vpDiv10S848 = _M0L6_2atmpS2169 / 10ull;
      uint64_t _M0L6_2atmpS2168 = _M0Lm2vmS823;
      uint64_t _M0L7vmDiv10S849 = _M0L6_2atmpS2168 / 10ull;
      uint64_t _M0L6_2atmpS2167;
      int32_t _M0L6_2atmpS2164;
      int32_t _M0L6_2atmpS2166;
      int32_t _M0L6_2atmpS2165;
      int32_t _M0L7vmMod10S851;
      uint64_t _M0L6_2atmpS2163;
      uint64_t _M0L7vrDiv10S852;
      uint64_t _M0L6_2atmpS2162;
      int32_t _M0L6_2atmpS2159;
      int32_t _M0L6_2atmpS2161;
      int32_t _M0L6_2atmpS2160;
      int32_t _M0L7vrMod10S853;
      int32_t _M0L6_2atmpS2158;
      if (_M0L7vpDiv10S848 <= _M0L7vmDiv10S849) {
        break;
      }
      _M0L6_2atmpS2167 = _M0Lm2vmS823;
      _M0L6_2atmpS2164 = (int32_t)_M0L6_2atmpS2167;
      _M0L6_2atmpS2166 = (int32_t)_M0L7vmDiv10S849;
      _M0L6_2atmpS2165 = 10 * _M0L6_2atmpS2166;
      _M0L7vmMod10S851 = _M0L6_2atmpS2164 - _M0L6_2atmpS2165;
      _M0L6_2atmpS2163 = _M0Lm2vrS821;
      _M0L7vrDiv10S852 = _M0L6_2atmpS2163 / 10ull;
      _M0L6_2atmpS2162 = _M0Lm2vrS821;
      _M0L6_2atmpS2159 = (int32_t)_M0L6_2atmpS2162;
      _M0L6_2atmpS2161 = (int32_t)_M0L7vrDiv10S852;
      _M0L6_2atmpS2160 = 10 * _M0L6_2atmpS2161;
      _M0L7vrMod10S853 = _M0L6_2atmpS2159 - _M0L6_2atmpS2160;
      if (_M0Lm17vmIsTrailingZerosS825) {
        _M0Lm17vmIsTrailingZerosS825 = _M0L7vmMod10S851 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS825 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS826) {
        int32_t _M0L6_2atmpS2157 = _M0Lm16lastRemovedDigitS846;
        _M0Lm17vrIsTrailingZerosS826 = _M0L6_2atmpS2157 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS826 = 0;
      }
      _M0Lm16lastRemovedDigitS846 = _M0L7vrMod10S853;
      _M0Lm2vrS821 = _M0L7vrDiv10S852;
      _M0Lm2vpS822 = _M0L7vpDiv10S848;
      _M0Lm2vmS823 = _M0L7vmDiv10S849;
      _M0L6_2atmpS2158 = _M0Lm7removedS845;
      _M0Lm7removedS845 = _M0L6_2atmpS2158 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS825) {
      while (1) {
        uint64_t _M0L6_2atmpS2182 = _M0Lm2vmS823;
        uint64_t _M0L7vmDiv10S854 = _M0L6_2atmpS2182 / 10ull;
        uint64_t _M0L6_2atmpS2181 = _M0Lm2vmS823;
        int32_t _M0L6_2atmpS2178 = (int32_t)_M0L6_2atmpS2181;
        int32_t _M0L6_2atmpS2180 = (int32_t)_M0L7vmDiv10S854;
        int32_t _M0L6_2atmpS2179 = 10 * _M0L6_2atmpS2180;
        int32_t _M0L7vmMod10S855 = _M0L6_2atmpS2178 - _M0L6_2atmpS2179;
        uint64_t _M0L6_2atmpS2177;
        uint64_t _M0L7vpDiv10S857;
        uint64_t _M0L6_2atmpS2176;
        uint64_t _M0L7vrDiv10S858;
        uint64_t _M0L6_2atmpS2175;
        int32_t _M0L6_2atmpS2172;
        int32_t _M0L6_2atmpS2174;
        int32_t _M0L6_2atmpS2173;
        int32_t _M0L7vrMod10S859;
        int32_t _M0L6_2atmpS2171;
        if (_M0L7vmMod10S855 != 0) {
          break;
        }
        _M0L6_2atmpS2177 = _M0Lm2vpS822;
        _M0L7vpDiv10S857 = _M0L6_2atmpS2177 / 10ull;
        _M0L6_2atmpS2176 = _M0Lm2vrS821;
        _M0L7vrDiv10S858 = _M0L6_2atmpS2176 / 10ull;
        _M0L6_2atmpS2175 = _M0Lm2vrS821;
        _M0L6_2atmpS2172 = (int32_t)_M0L6_2atmpS2175;
        _M0L6_2atmpS2174 = (int32_t)_M0L7vrDiv10S858;
        _M0L6_2atmpS2173 = 10 * _M0L6_2atmpS2174;
        _M0L7vrMod10S859 = _M0L6_2atmpS2172 - _M0L6_2atmpS2173;
        if (_M0Lm17vrIsTrailingZerosS826) {
          int32_t _M0L6_2atmpS2170 = _M0Lm16lastRemovedDigitS846;
          _M0Lm17vrIsTrailingZerosS826 = _M0L6_2atmpS2170 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS826 = 0;
        }
        _M0Lm16lastRemovedDigitS846 = _M0L7vrMod10S859;
        _M0Lm2vrS821 = _M0L7vrDiv10S858;
        _M0Lm2vpS822 = _M0L7vpDiv10S857;
        _M0Lm2vmS823 = _M0L7vmDiv10S854;
        _M0L6_2atmpS2171 = _M0Lm7removedS845;
        _M0Lm7removedS845 = _M0L6_2atmpS2171 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS826) {
      int32_t _M0L6_2atmpS2185 = _M0Lm16lastRemovedDigitS846;
      if (_M0L6_2atmpS2185 == 5) {
        uint64_t _M0L6_2atmpS2184 = _M0Lm2vrS821;
        uint64_t _M0L6_2atmpS2183 = _M0L6_2atmpS2184 % 2ull;
        _if__result_4009 = _M0L6_2atmpS2183 == 0ull;
      } else {
        _if__result_4009 = 0;
      }
    } else {
      _if__result_4009 = 0;
    }
    if (_if__result_4009) {
      _M0Lm16lastRemovedDigitS846 = 4;
    }
    _M0L6_2atmpS2186 = _M0Lm2vrS821;
    _M0L6_2atmpS2192 = _M0Lm2vrS821;
    _M0L6_2atmpS2193 = _M0Lm2vmS823;
    if (_M0L6_2atmpS2192 == _M0L6_2atmpS2193) {
      if (!_M0L4evenS818) {
        _if__result_4010 = 1;
      } else {
        int32_t _M0L6_2atmpS2191 = _M0Lm17vmIsTrailingZerosS825;
        _if__result_4010 = !_M0L6_2atmpS2191;
      }
    } else {
      _if__result_4010 = 0;
    }
    if (_if__result_4010) {
      _M0L6_2atmpS2189 = 1;
    } else {
      int32_t _M0L6_2atmpS2190 = _M0Lm16lastRemovedDigitS846;
      _M0L6_2atmpS2189 = _M0L6_2atmpS2190 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2188 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2189);
    _M0L6_2atmpS2187 = *(uint64_t*)&_M0L6_2atmpS2188;
    _M0Lm6outputS847 = _M0L6_2atmpS2186 + _M0L6_2atmpS2187;
  } else {
    int32_t _M0Lm7roundUpS860 = 0;
    uint64_t _M0L6_2atmpS2214 = _M0Lm2vpS822;
    uint64_t _M0L8vpDiv100S861 = _M0L6_2atmpS2214 / 100ull;
    uint64_t _M0L6_2atmpS2213 = _M0Lm2vmS823;
    uint64_t _M0L8vmDiv100S862 = _M0L6_2atmpS2213 / 100ull;
    uint64_t _M0L6_2atmpS2208;
    uint64_t _M0L6_2atmpS2211;
    uint64_t _M0L6_2atmpS2212;
    int32_t _M0L6_2atmpS2210;
    uint64_t _M0L6_2atmpS2209;
    if (_M0L8vpDiv100S861 > _M0L8vmDiv100S862) {
      uint64_t _M0L6_2atmpS2199 = _M0Lm2vrS821;
      uint64_t _M0L8vrDiv100S863 = _M0L6_2atmpS2199 / 100ull;
      uint64_t _M0L6_2atmpS2198 = _M0Lm2vrS821;
      int32_t _M0L6_2atmpS2195 = (int32_t)_M0L6_2atmpS2198;
      int32_t _M0L6_2atmpS2197 = (int32_t)_M0L8vrDiv100S863;
      int32_t _M0L6_2atmpS2196 = 100 * _M0L6_2atmpS2197;
      int32_t _M0L8vrMod100S864 = _M0L6_2atmpS2195 - _M0L6_2atmpS2196;
      int32_t _M0L6_2atmpS2194;
      _M0Lm7roundUpS860 = _M0L8vrMod100S864 >= 50;
      _M0Lm2vrS821 = _M0L8vrDiv100S863;
      _M0Lm2vpS822 = _M0L8vpDiv100S861;
      _M0Lm2vmS823 = _M0L8vmDiv100S862;
      _M0L6_2atmpS2194 = _M0Lm7removedS845;
      _M0Lm7removedS845 = _M0L6_2atmpS2194 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2207 = _M0Lm2vpS822;
      uint64_t _M0L7vpDiv10S865 = _M0L6_2atmpS2207 / 10ull;
      uint64_t _M0L6_2atmpS2206 = _M0Lm2vmS823;
      uint64_t _M0L7vmDiv10S866 = _M0L6_2atmpS2206 / 10ull;
      uint64_t _M0L6_2atmpS2205;
      uint64_t _M0L7vrDiv10S868;
      uint64_t _M0L6_2atmpS2204;
      int32_t _M0L6_2atmpS2201;
      int32_t _M0L6_2atmpS2203;
      int32_t _M0L6_2atmpS2202;
      int32_t _M0L7vrMod10S869;
      int32_t _M0L6_2atmpS2200;
      if (_M0L7vpDiv10S865 <= _M0L7vmDiv10S866) {
        break;
      }
      _M0L6_2atmpS2205 = _M0Lm2vrS821;
      _M0L7vrDiv10S868 = _M0L6_2atmpS2205 / 10ull;
      _M0L6_2atmpS2204 = _M0Lm2vrS821;
      _M0L6_2atmpS2201 = (int32_t)_M0L6_2atmpS2204;
      _M0L6_2atmpS2203 = (int32_t)_M0L7vrDiv10S868;
      _M0L6_2atmpS2202 = 10 * _M0L6_2atmpS2203;
      _M0L7vrMod10S869 = _M0L6_2atmpS2201 - _M0L6_2atmpS2202;
      _M0Lm7roundUpS860 = _M0L7vrMod10S869 >= 5;
      _M0Lm2vrS821 = _M0L7vrDiv10S868;
      _M0Lm2vpS822 = _M0L7vpDiv10S865;
      _M0Lm2vmS823 = _M0L7vmDiv10S866;
      _M0L6_2atmpS2200 = _M0Lm7removedS845;
      _M0Lm7removedS845 = _M0L6_2atmpS2200 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2208 = _M0Lm2vrS821;
    _M0L6_2atmpS2211 = _M0Lm2vrS821;
    _M0L6_2atmpS2212 = _M0Lm2vmS823;
    _M0L6_2atmpS2210
    = _M0L6_2atmpS2211 == _M0L6_2atmpS2212 || _M0Lm7roundUpS860;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2209 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2210);
    _M0Lm6outputS847 = _M0L6_2atmpS2208 + _M0L6_2atmpS2209;
  }
  _M0L6_2atmpS2216 = _M0Lm3e10S824;
  _M0L6_2atmpS2217 = _M0Lm7removedS845;
  _M0L3expS870 = _M0L6_2atmpS2216 + _M0L6_2atmpS2217;
  _M0L6_2atmpS2215 = _M0Lm6outputS847;
  _block_4012
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4012)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4012->$0 = _M0L6_2atmpS2215;
  _block_4012->$1 = _M0L3expS870;
  return _block_4012;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS813) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS813) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS812) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS812) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS811) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS811) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS810) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS810 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS810 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS810 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS810 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS810 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS810 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS810 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS810 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS810 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS810 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS810 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS810 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS810 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS810 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS810 >= 100ull) {
    return 3;
  }
  if (_M0L1vS810 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS793) {
  int32_t _M0L6_2atmpS2116;
  int32_t _M0L6_2atmpS2115;
  int32_t _M0L4baseS792;
  int32_t _M0L5base2S794;
  int32_t _M0L6offsetS795;
  int32_t _M0L6_2atmpS2114;
  uint64_t _M0L4mul0S796;
  int32_t _M0L6_2atmpS2113;
  int32_t _M0L6_2atmpS2112;
  uint64_t _M0L4mul1S797;
  uint64_t _M0L1mS798;
  struct _M0TPB7Umul128 _M0L7_2abindS799;
  uint64_t _M0L7_2alow1S800;
  uint64_t _M0L8_2ahigh1S801;
  struct _M0TPB7Umul128 _M0L7_2abindS802;
  uint64_t _M0L7_2alow0S803;
  uint64_t _M0L8_2ahigh0S804;
  uint64_t _M0L3sumS805;
  uint64_t _M0Lm5high1S806;
  int32_t _M0L6_2atmpS2110;
  int32_t _M0L6_2atmpS2111;
  int32_t _M0L5deltaS807;
  uint64_t _M0L6_2atmpS2109;
  uint64_t _M0L6_2atmpS2101;
  int32_t _M0L6_2atmpS2108;
  uint32_t _M0L6_2atmpS2105;
  int32_t _M0L6_2atmpS2107;
  int32_t _M0L6_2atmpS2106;
  uint32_t _M0L6_2atmpS2104;
  uint32_t _M0L6_2atmpS2103;
  uint64_t _M0L6_2atmpS2102;
  uint64_t _M0L1aS808;
  uint64_t _M0L6_2atmpS2100;
  uint64_t _M0L1bS809;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2116 = _M0L1iS793 + 26;
  _M0L6_2atmpS2115 = _M0L6_2atmpS2116 - 1;
  _M0L4baseS792 = _M0L6_2atmpS2115 / 26;
  _M0L5base2S794 = _M0L4baseS792 * 26;
  _M0L6offsetS795 = _M0L5base2S794 - _M0L1iS793;
  _M0L6_2atmpS2114 = _M0L4baseS792 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S796
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2114);
  _M0L6_2atmpS2113 = _M0L4baseS792 * 2;
  _M0L6_2atmpS2112 = _M0L6_2atmpS2113 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S797
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2112);
  if (_M0L6offsetS795 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S796, _M0L4mul1S797};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS798
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS795);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS799 = _M0FPB7umul128(_M0L1mS798, _M0L4mul1S797);
  _M0L7_2alow1S800 = _M0L7_2abindS799.$0;
  _M0L8_2ahigh1S801 = _M0L7_2abindS799.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS802 = _M0FPB7umul128(_M0L1mS798, _M0L4mul0S796);
  _M0L7_2alow0S803 = _M0L7_2abindS802.$0;
  _M0L8_2ahigh0S804 = _M0L7_2abindS802.$1;
  _M0L3sumS805 = _M0L8_2ahigh0S804 + _M0L7_2alow1S800;
  _M0Lm5high1S806 = _M0L8_2ahigh1S801;
  if (_M0L3sumS805 < _M0L8_2ahigh0S804) {
    uint64_t _M0L6_2atmpS2099 = _M0Lm5high1S806;
    _M0Lm5high1S806 = _M0L6_2atmpS2099 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2110 = _M0FPB8pow5bits(_M0L5base2S794);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2111 = _M0FPB8pow5bits(_M0L1iS793);
  _M0L5deltaS807 = _M0L6_2atmpS2110 - _M0L6_2atmpS2111;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2109
  = _M0FPB13shiftright128(_M0L7_2alow0S803, _M0L3sumS805, _M0L5deltaS807);
  _M0L6_2atmpS2101 = _M0L6_2atmpS2109 + 1ull;
  _M0L6_2atmpS2108 = _M0L1iS793 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2105
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2108);
  _M0L6_2atmpS2107 = _M0L1iS793 % 16;
  _M0L6_2atmpS2106 = _M0L6_2atmpS2107 << 1;
  _M0L6_2atmpS2104 = _M0L6_2atmpS2105 >> (_M0L6_2atmpS2106 & 31);
  _M0L6_2atmpS2103 = _M0L6_2atmpS2104 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2102 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2103);
  _M0L1aS808 = _M0L6_2atmpS2101 + _M0L6_2atmpS2102;
  _M0L6_2atmpS2100 = _M0Lm5high1S806;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS809
  = _M0FPB13shiftright128(_M0L3sumS805, _M0L6_2atmpS2100, _M0L5deltaS807);
  return (struct _M0TPB8Pow5Pair){_M0L1aS808, _M0L1bS809};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS775) {
  int32_t _M0L4baseS774;
  int32_t _M0L5base2S776;
  int32_t _M0L6offsetS777;
  int32_t _M0L6_2atmpS2098;
  uint64_t _M0L4mul0S778;
  int32_t _M0L6_2atmpS2097;
  int32_t _M0L6_2atmpS2096;
  uint64_t _M0L4mul1S779;
  uint64_t _M0L1mS780;
  struct _M0TPB7Umul128 _M0L7_2abindS781;
  uint64_t _M0L7_2alow1S782;
  uint64_t _M0L8_2ahigh1S783;
  struct _M0TPB7Umul128 _M0L7_2abindS784;
  uint64_t _M0L7_2alow0S785;
  uint64_t _M0L8_2ahigh0S786;
  uint64_t _M0L3sumS787;
  uint64_t _M0Lm5high1S788;
  int32_t _M0L6_2atmpS2094;
  int32_t _M0L6_2atmpS2095;
  int32_t _M0L5deltaS789;
  uint64_t _M0L6_2atmpS2086;
  int32_t _M0L6_2atmpS2093;
  uint32_t _M0L6_2atmpS2090;
  int32_t _M0L6_2atmpS2092;
  int32_t _M0L6_2atmpS2091;
  uint32_t _M0L6_2atmpS2089;
  uint32_t _M0L6_2atmpS2088;
  uint64_t _M0L6_2atmpS2087;
  uint64_t _M0L1aS790;
  uint64_t _M0L6_2atmpS2085;
  uint64_t _M0L1bS791;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS774 = _M0L1iS775 / 26;
  _M0L5base2S776 = _M0L4baseS774 * 26;
  _M0L6offsetS777 = _M0L1iS775 - _M0L5base2S776;
  _M0L6_2atmpS2098 = _M0L4baseS774 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S778
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2098);
  _M0L6_2atmpS2097 = _M0L4baseS774 * 2;
  _M0L6_2atmpS2096 = _M0L6_2atmpS2097 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S779
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2096);
  if (_M0L6offsetS777 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S778, _M0L4mul1S779};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS780
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS777);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS781 = _M0FPB7umul128(_M0L1mS780, _M0L4mul1S779);
  _M0L7_2alow1S782 = _M0L7_2abindS781.$0;
  _M0L8_2ahigh1S783 = _M0L7_2abindS781.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS784 = _M0FPB7umul128(_M0L1mS780, _M0L4mul0S778);
  _M0L7_2alow0S785 = _M0L7_2abindS784.$0;
  _M0L8_2ahigh0S786 = _M0L7_2abindS784.$1;
  _M0L3sumS787 = _M0L8_2ahigh0S786 + _M0L7_2alow1S782;
  _M0Lm5high1S788 = _M0L8_2ahigh1S783;
  if (_M0L3sumS787 < _M0L8_2ahigh0S786) {
    uint64_t _M0L6_2atmpS2084 = _M0Lm5high1S788;
    _M0Lm5high1S788 = _M0L6_2atmpS2084 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2094 = _M0FPB8pow5bits(_M0L1iS775);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2095 = _M0FPB8pow5bits(_M0L5base2S776);
  _M0L5deltaS789 = _M0L6_2atmpS2094 - _M0L6_2atmpS2095;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2086
  = _M0FPB13shiftright128(_M0L7_2alow0S785, _M0L3sumS787, _M0L5deltaS789);
  _M0L6_2atmpS2093 = _M0L1iS775 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2090
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2093);
  _M0L6_2atmpS2092 = _M0L1iS775 % 16;
  _M0L6_2atmpS2091 = _M0L6_2atmpS2092 << 1;
  _M0L6_2atmpS2089 = _M0L6_2atmpS2090 >> (_M0L6_2atmpS2091 & 31);
  _M0L6_2atmpS2088 = _M0L6_2atmpS2089 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2087 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2088);
  _M0L1aS790 = _M0L6_2atmpS2086 + _M0L6_2atmpS2087;
  _M0L6_2atmpS2085 = _M0Lm5high1S788;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS791
  = _M0FPB13shiftright128(_M0L3sumS787, _M0L6_2atmpS2085, _M0L5deltaS789);
  return (struct _M0TPB8Pow5Pair){_M0L1aS790, _M0L1bS791};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS748,
  struct _M0TPB8Pow5Pair _M0L3mulS745,
  int32_t _M0L1jS761,
  int32_t _M0L7mmShiftS763
) {
  uint64_t _M0L7_2amul0S744;
  uint64_t _M0L7_2amul1S746;
  uint64_t _M0L1mS747;
  struct _M0TPB7Umul128 _M0L7_2abindS749;
  uint64_t _M0L5_2aloS750;
  uint64_t _M0L6_2atmpS751;
  struct _M0TPB7Umul128 _M0L7_2abindS752;
  uint64_t _M0L6_2alo2S753;
  uint64_t _M0L6_2ahi2S754;
  uint64_t _M0L3midS755;
  uint64_t _M0L6_2atmpS2083;
  uint64_t _M0L2hiS756;
  uint64_t _M0L3lo2S757;
  uint64_t _M0L6_2atmpS2081;
  uint64_t _M0L6_2atmpS2082;
  uint64_t _M0L4mid2S758;
  uint64_t _M0L6_2atmpS2080;
  uint64_t _M0L3hi2S759;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L6_2atmpS2078;
  uint64_t _M0L2vpS760;
  uint64_t _M0Lm2vmS762;
  int32_t _M0L6_2atmpS2077;
  int32_t _M0L6_2atmpS2076;
  uint64_t _M0L2vrS773;
  uint64_t _M0L6_2atmpS2075;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S744 = _M0L3mulS745.$0;
  _M0L7_2amul1S746 = _M0L3mulS745.$1;
  _M0L1mS747 = _M0L1mS748 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS749 = _M0FPB7umul128(_M0L1mS747, _M0L7_2amul0S744);
  _M0L5_2aloS750 = _M0L7_2abindS749.$0;
  _M0L6_2atmpS751 = _M0L7_2abindS749.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS752 = _M0FPB7umul128(_M0L1mS747, _M0L7_2amul1S746);
  _M0L6_2alo2S753 = _M0L7_2abindS752.$0;
  _M0L6_2ahi2S754 = _M0L7_2abindS752.$1;
  _M0L3midS755 = _M0L6_2atmpS751 + _M0L6_2alo2S753;
  if (_M0L3midS755 < _M0L6_2atmpS751) {
    _M0L6_2atmpS2083 = 1ull;
  } else {
    _M0L6_2atmpS2083 = 0ull;
  }
  _M0L2hiS756 = _M0L6_2ahi2S754 + _M0L6_2atmpS2083;
  _M0L3lo2S757 = _M0L5_2aloS750 + _M0L7_2amul0S744;
  _M0L6_2atmpS2081 = _M0L3midS755 + _M0L7_2amul1S746;
  if (_M0L3lo2S757 < _M0L5_2aloS750) {
    _M0L6_2atmpS2082 = 1ull;
  } else {
    _M0L6_2atmpS2082 = 0ull;
  }
  _M0L4mid2S758 = _M0L6_2atmpS2081 + _M0L6_2atmpS2082;
  if (_M0L4mid2S758 < _M0L3midS755) {
    _M0L6_2atmpS2080 = 1ull;
  } else {
    _M0L6_2atmpS2080 = 0ull;
  }
  _M0L3hi2S759 = _M0L2hiS756 + _M0L6_2atmpS2080;
  _M0L6_2atmpS2079 = _M0L1jS761 - 64;
  _M0L6_2atmpS2078 = _M0L6_2atmpS2079 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS760
  = _M0FPB13shiftright128(_M0L4mid2S758, _M0L3hi2S759, _M0L6_2atmpS2078);
  _M0Lm2vmS762 = 0ull;
  if (_M0L7mmShiftS763) {
    uint64_t _M0L3lo3S764 = _M0L5_2aloS750 - _M0L7_2amul0S744;
    uint64_t _M0L6_2atmpS2065 = _M0L3midS755 - _M0L7_2amul1S746;
    uint64_t _M0L6_2atmpS2066;
    uint64_t _M0L4mid3S765;
    uint64_t _M0L6_2atmpS2064;
    uint64_t _M0L3hi3S766;
    int32_t _M0L6_2atmpS2063;
    int32_t _M0L6_2atmpS2062;
    if (_M0L5_2aloS750 < _M0L3lo3S764) {
      _M0L6_2atmpS2066 = 1ull;
    } else {
      _M0L6_2atmpS2066 = 0ull;
    }
    _M0L4mid3S765 = _M0L6_2atmpS2065 - _M0L6_2atmpS2066;
    if (_M0L3midS755 < _M0L4mid3S765) {
      _M0L6_2atmpS2064 = 1ull;
    } else {
      _M0L6_2atmpS2064 = 0ull;
    }
    _M0L3hi3S766 = _M0L2hiS756 - _M0L6_2atmpS2064;
    _M0L6_2atmpS2063 = _M0L1jS761 - 64;
    _M0L6_2atmpS2062 = _M0L6_2atmpS2063 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS762
    = _M0FPB13shiftright128(_M0L4mid3S765, _M0L3hi3S766, _M0L6_2atmpS2062);
  } else {
    uint64_t _M0L3lo3S767 = _M0L5_2aloS750 + _M0L5_2aloS750;
    uint64_t _M0L6_2atmpS2073 = _M0L3midS755 + _M0L3midS755;
    uint64_t _M0L6_2atmpS2074;
    uint64_t _M0L4mid3S768;
    uint64_t _M0L6_2atmpS2071;
    uint64_t _M0L6_2atmpS2072;
    uint64_t _M0L3hi3S769;
    uint64_t _M0L3lo4S770;
    uint64_t _M0L6_2atmpS2069;
    uint64_t _M0L6_2atmpS2070;
    uint64_t _M0L4mid4S771;
    uint64_t _M0L6_2atmpS2068;
    uint64_t _M0L3hi4S772;
    int32_t _M0L6_2atmpS2067;
    if (_M0L3lo3S767 < _M0L5_2aloS750) {
      _M0L6_2atmpS2074 = 1ull;
    } else {
      _M0L6_2atmpS2074 = 0ull;
    }
    _M0L4mid3S768 = _M0L6_2atmpS2073 + _M0L6_2atmpS2074;
    _M0L6_2atmpS2071 = _M0L2hiS756 + _M0L2hiS756;
    if (_M0L4mid3S768 < _M0L3midS755) {
      _M0L6_2atmpS2072 = 1ull;
    } else {
      _M0L6_2atmpS2072 = 0ull;
    }
    _M0L3hi3S769 = _M0L6_2atmpS2071 + _M0L6_2atmpS2072;
    _M0L3lo4S770 = _M0L3lo3S767 - _M0L7_2amul0S744;
    _M0L6_2atmpS2069 = _M0L4mid3S768 - _M0L7_2amul1S746;
    if (_M0L3lo3S767 < _M0L3lo4S770) {
      _M0L6_2atmpS2070 = 1ull;
    } else {
      _M0L6_2atmpS2070 = 0ull;
    }
    _M0L4mid4S771 = _M0L6_2atmpS2069 - _M0L6_2atmpS2070;
    if (_M0L4mid3S768 < _M0L4mid4S771) {
      _M0L6_2atmpS2068 = 1ull;
    } else {
      _M0L6_2atmpS2068 = 0ull;
    }
    _M0L3hi4S772 = _M0L3hi3S769 - _M0L6_2atmpS2068;
    _M0L6_2atmpS2067 = _M0L1jS761 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS762
    = _M0FPB13shiftright128(_M0L4mid4S771, _M0L3hi4S772, _M0L6_2atmpS2067);
  }
  _M0L6_2atmpS2077 = _M0L1jS761 - 64;
  _M0L6_2atmpS2076 = _M0L6_2atmpS2077 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS773
  = _M0FPB13shiftright128(_M0L3midS755, _M0L2hiS756, _M0L6_2atmpS2076);
  _M0L6_2atmpS2075 = _M0Lm2vmS762;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS773,
                                                _M0L2vpS760,
                                                _M0L6_2atmpS2075};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS742,
  int32_t _M0L1pS743
) {
  uint64_t _M0L6_2atmpS2061;
  uint64_t _M0L6_2atmpS2060;
  uint64_t _M0L6_2atmpS2059;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2061 = 1ull << (_M0L1pS743 & 63);
  _M0L6_2atmpS2060 = _M0L6_2atmpS2061 - 1ull;
  _M0L6_2atmpS2059 = _M0L5valueS742 & _M0L6_2atmpS2060;
  return _M0L6_2atmpS2059 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS740,
  int32_t _M0L1pS741
) {
  int32_t _M0L6_2atmpS2058;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2058 = _M0FPB10pow5Factor(_M0L5valueS740);
  return _M0L6_2atmpS2058 >= _M0L1pS741;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS736) {
  uint64_t _M0L6_2atmpS2046;
  uint64_t _M0L6_2atmpS2047;
  uint64_t _M0L6_2atmpS2048;
  uint64_t _M0L6_2atmpS2049;
  int32_t _M0Lm5countS737;
  uint64_t _M0Lm5valueS738;
  uint64_t _M0L6_2atmpS2057;
  moonbit_string_t _M0L6_2atmpS2056;
  moonbit_string_t _M0L6_2atmpS3556;
  moonbit_string_t _M0L6_2atmpS2055;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2046 = _M0L5valueS736 % 5ull;
  if (_M0L6_2atmpS2046 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2047 = _M0L5valueS736 % 25ull;
  if (_M0L6_2atmpS2047 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2048 = _M0L5valueS736 % 125ull;
  if (_M0L6_2atmpS2048 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2049 = _M0L5valueS736 % 625ull;
  if (_M0L6_2atmpS2049 != 0ull) {
    return 3;
  }
  _M0Lm5countS737 = 4;
  _M0Lm5valueS738 = _M0L5valueS736 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2050 = _M0Lm5valueS738;
    if (_M0L6_2atmpS2050 > 0ull) {
      uint64_t _M0L6_2atmpS2052 = _M0Lm5valueS738;
      uint64_t _M0L6_2atmpS2051 = _M0L6_2atmpS2052 % 5ull;
      uint64_t _M0L6_2atmpS2053;
      int32_t _M0L6_2atmpS2054;
      if (_M0L6_2atmpS2051 != 0ull) {
        return _M0Lm5countS737;
      }
      _M0L6_2atmpS2053 = _M0Lm5valueS738;
      _M0Lm5valueS738 = _M0L6_2atmpS2053 / 5ull;
      _M0L6_2atmpS2054 = _M0Lm5countS737;
      _M0Lm5countS737 = _M0L6_2atmpS2054 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2057 = _M0Lm5valueS738;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2056
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2057);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3556
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_231.data, _M0L6_2atmpS2056);
  moonbit_decref(_M0L6_2atmpS2056);
  _M0L6_2atmpS2055 = _M0L6_2atmpS3556;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2055, (moonbit_string_t)moonbit_string_literal_232.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS735,
  uint64_t _M0L2hiS733,
  int32_t _M0L4distS734
) {
  int32_t _M0L6_2atmpS2045;
  uint64_t _M0L6_2atmpS2043;
  uint64_t _M0L6_2atmpS2044;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2045 = 64 - _M0L4distS734;
  _M0L6_2atmpS2043 = _M0L2hiS733 << (_M0L6_2atmpS2045 & 63);
  _M0L6_2atmpS2044 = _M0L2loS735 >> (_M0L4distS734 & 63);
  return _M0L6_2atmpS2043 | _M0L6_2atmpS2044;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS723,
  uint64_t _M0L1bS726
) {
  uint64_t _M0L3aLoS722;
  uint64_t _M0L3aHiS724;
  uint64_t _M0L3bLoS725;
  uint64_t _M0L3bHiS727;
  uint64_t _M0L1xS728;
  uint64_t _M0L6_2atmpS2041;
  uint64_t _M0L6_2atmpS2042;
  uint64_t _M0L1yS729;
  uint64_t _M0L6_2atmpS2039;
  uint64_t _M0L6_2atmpS2040;
  uint64_t _M0L1zS730;
  uint64_t _M0L6_2atmpS2037;
  uint64_t _M0L6_2atmpS2038;
  uint64_t _M0L6_2atmpS2035;
  uint64_t _M0L6_2atmpS2036;
  uint64_t _M0L1wS731;
  uint64_t _M0L2loS732;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS722 = _M0L1aS723 & 4294967295ull;
  _M0L3aHiS724 = _M0L1aS723 >> 32;
  _M0L3bLoS725 = _M0L1bS726 & 4294967295ull;
  _M0L3bHiS727 = _M0L1bS726 >> 32;
  _M0L1xS728 = _M0L3aLoS722 * _M0L3bLoS725;
  _M0L6_2atmpS2041 = _M0L3aHiS724 * _M0L3bLoS725;
  _M0L6_2atmpS2042 = _M0L1xS728 >> 32;
  _M0L1yS729 = _M0L6_2atmpS2041 + _M0L6_2atmpS2042;
  _M0L6_2atmpS2039 = _M0L3aLoS722 * _M0L3bHiS727;
  _M0L6_2atmpS2040 = _M0L1yS729 & 4294967295ull;
  _M0L1zS730 = _M0L6_2atmpS2039 + _M0L6_2atmpS2040;
  _M0L6_2atmpS2037 = _M0L3aHiS724 * _M0L3bHiS727;
  _M0L6_2atmpS2038 = _M0L1yS729 >> 32;
  _M0L6_2atmpS2035 = _M0L6_2atmpS2037 + _M0L6_2atmpS2038;
  _M0L6_2atmpS2036 = _M0L1zS730 >> 32;
  _M0L1wS731 = _M0L6_2atmpS2035 + _M0L6_2atmpS2036;
  _M0L2loS732 = _M0L1aS723 * _M0L1bS726;
  return (struct _M0TPB7Umul128){_M0L2loS732, _M0L1wS731};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS717,
  int32_t _M0L4fromS721,
  int32_t _M0L2toS719
) {
  int32_t _M0L6_2atmpS2034;
  struct _M0TPB13StringBuilder* _M0L3bufS716;
  int32_t _M0L1iS718;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2034 = Moonbit_array_length(_M0L5bytesS717);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS716 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2034);
  _M0L1iS718 = _M0L4fromS721;
  while (1) {
    if (_M0L1iS718 < _M0L2toS719) {
      int32_t _M0L6_2atmpS2032;
      int32_t _M0L6_2atmpS2031;
      int32_t _M0L6_2atmpS2033;
      if (
        _M0L1iS718 < 0 || _M0L1iS718 >= Moonbit_array_length(_M0L5bytesS717)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2032 = (int32_t)_M0L5bytesS717[_M0L1iS718];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2031 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2032);
      moonbit_incref(_M0L3bufS716);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS716, _M0L6_2atmpS2031);
      _M0L6_2atmpS2033 = _M0L1iS718 + 1;
      _M0L1iS718 = _M0L6_2atmpS2033;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS717);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS716);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS715) {
  int32_t _M0L6_2atmpS2030;
  uint32_t _M0L6_2atmpS2029;
  uint32_t _M0L6_2atmpS2028;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2030 = _M0L1eS715 * 78913;
  _M0L6_2atmpS2029 = *(uint32_t*)&_M0L6_2atmpS2030;
  _M0L6_2atmpS2028 = _M0L6_2atmpS2029 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2028;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS714) {
  int32_t _M0L6_2atmpS2027;
  uint32_t _M0L6_2atmpS2026;
  uint32_t _M0L6_2atmpS2025;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2027 = _M0L1eS714 * 732923;
  _M0L6_2atmpS2026 = *(uint32_t*)&_M0L6_2atmpS2027;
  _M0L6_2atmpS2025 = _M0L6_2atmpS2026 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2025;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS712,
  int32_t _M0L8exponentS713,
  int32_t _M0L8mantissaS710
) {
  moonbit_string_t _M0L1sS711;
  moonbit_string_t _M0L6_2atmpS3557;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS710) {
    return (moonbit_string_t)moonbit_string_literal_233.data;
  }
  if (_M0L4signS712) {
    _M0L1sS711 = (moonbit_string_t)moonbit_string_literal_234.data;
  } else {
    _M0L1sS711 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS713) {
    moonbit_string_t _M0L6_2atmpS3558;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3558
    = moonbit_add_string(_M0L1sS711, (moonbit_string_t)moonbit_string_literal_235.data);
    moonbit_decref(_M0L1sS711);
    return _M0L6_2atmpS3558;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3557
  = moonbit_add_string(_M0L1sS711, (moonbit_string_t)moonbit_string_literal_236.data);
  moonbit_decref(_M0L1sS711);
  return _M0L6_2atmpS3557;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS709) {
  int32_t _M0L6_2atmpS2024;
  uint32_t _M0L6_2atmpS2023;
  uint32_t _M0L6_2atmpS2022;
  int32_t _M0L6_2atmpS2021;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2024 = _M0L1eS709 * 1217359;
  _M0L6_2atmpS2023 = *(uint32_t*)&_M0L6_2atmpS2024;
  _M0L6_2atmpS2022 = _M0L6_2atmpS2023 >> 19;
  _M0L6_2atmpS2021 = *(int32_t*)&_M0L6_2atmpS2022;
  return _M0L6_2atmpS2021 + 1;
}

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t _M0L1xS704,
  moonbit_string_t _M0L3msgS706,
  moonbit_string_t _M0L3locS708
) {
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (!_M0L1xS704) {
    moonbit_string_t _M0L9fail__msgS705;
    if (_M0L3msgS706 == 0) {
      moonbit_string_t _M0L6_2atmpS2019;
      moonbit_string_t _M0L6_2atmpS3560;
      moonbit_string_t _M0L6_2atmpS2018;
      moonbit_string_t _M0L6_2atmpS3559;
      if (_M0L3msgS706) {
        moonbit_decref(_M0L3msgS706);
      }
      #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2019
      = _M0IP016_24default__implPB4Show10to__stringGbE(_M0L1xS704);
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS3560
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_237.data, _M0L6_2atmpS2019);
      moonbit_decref(_M0L6_2atmpS2019);
      _M0L6_2atmpS2018 = _M0L6_2atmpS3560;
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS3559
      = moonbit_add_string(_M0L6_2atmpS2018, (moonbit_string_t)moonbit_string_literal_238.data);
      moonbit_decref(_M0L6_2atmpS2018);
      _M0L9fail__msgS705 = _M0L6_2atmpS3559;
    } else {
      moonbit_string_t _M0L7_2aSomeS707 = _M0L3msgS706;
      _M0L9fail__msgS705 = _M0L7_2aSomeS707;
    }
    #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS705, _M0L3locS708);
  } else {
    int32_t _M0L6_2atmpS2020;
    struct moonbit_result_0 _result_4015;
    moonbit_decref(_M0L3locS708);
    if (_M0L3msgS706) {
      moonbit_decref(_M0L3msgS706);
    }
    _M0L6_2atmpS2020 = 0;
    _result_4015.tag = 1;
    _result_4015.data.ok = _M0L6_2atmpS2020;
    return _result_4015;
  }
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS703,
  struct _M0TPB6Hasher* _M0L6hasherS702
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS702, _M0L4selfS703);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS701,
  struct _M0TPB6Hasher* _M0L6hasherS700
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS700, _M0L4selfS701);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS698,
  moonbit_string_t _M0L5valueS696
) {
  int32_t _M0L7_2abindS695;
  int32_t _M0L1iS697;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS695 = Moonbit_array_length(_M0L5valueS696);
  _M0L1iS697 = 0;
  while (1) {
    if (_M0L1iS697 < _M0L7_2abindS695) {
      int32_t _M0L6_2atmpS2016 = _M0L5valueS696[_M0L1iS697];
      int32_t _M0L6_2atmpS2015 = (int32_t)_M0L6_2atmpS2016;
      uint32_t _M0L6_2atmpS2014 = *(uint32_t*)&_M0L6_2atmpS2015;
      int32_t _M0L6_2atmpS2017;
      moonbit_incref(_M0L4selfS698);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS698, _M0L6_2atmpS2014);
      _M0L6_2atmpS2017 = _M0L1iS697 + 1;
      _M0L1iS697 = _M0L6_2atmpS2017;
      continue;
    } else {
      moonbit_decref(_M0L4selfS698);
      moonbit_decref(_M0L5valueS696);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS693,
  int32_t _M0L3idxS694
) {
  int32_t _M0L6_2atmpS3561;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3561 = _M0L4selfS693[_M0L3idxS694];
  moonbit_decref(_M0L4selfS693);
  return _M0L6_2atmpS3561;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS692) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS692;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS691) {
  double _M0L6_2atmpS2012;
  moonbit_string_t _M0L6_2atmpS2013;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2012 = (double)_M0L4selfS691;
  _M0L6_2atmpS2013 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2012, _M0L6_2atmpS2013);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS684
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3562;
  int32_t _M0L6_2acntS3767;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2011;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS683;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__* _closure_4017;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2006;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3562 = _M0L4selfS684->$5;
  _M0L6_2acntS3767 = Moonbit_object_header(_M0L4selfS684)->rc;
  if (_M0L6_2acntS3767 > 1) {
    int32_t _M0L11_2anew__cntS3769 = _M0L6_2acntS3767 - 1;
    Moonbit_object_header(_M0L4selfS684)->rc = _M0L11_2anew__cntS3769;
    if (_M0L8_2afieldS3562) {
      moonbit_incref(_M0L8_2afieldS3562);
    }
  } else if (_M0L6_2acntS3767 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3768 = _M0L4selfS684->$0;
    moonbit_decref(_M0L8_2afieldS3768);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS684);
  }
  _M0L4headS2011 = _M0L8_2afieldS3562;
  _M0L11curr__entryS683
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS683)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS683->$0 = _M0L4headS2011;
  _closure_4017
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__));
  Moonbit_object_header(_closure_4017)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__, $0) >> 2, 1, 0);
  _closure_4017->code = &_M0MPB3Map4iterGsRPB4JsonEC2007l591;
  _closure_4017->$0 = _M0L11curr__entryS683;
  _M0L6_2atmpS2006 = (struct _M0TWEOUsRPB4JsonE*)_closure_4017;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2006);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2007l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2008
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__* _M0L14_2acasted__envS2009;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3568;
  int32_t _M0L6_2acntS3770;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS683;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3567;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS685;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2009
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2007__l591__*)_M0L6_2aenvS2008;
  _M0L8_2afieldS3568 = _M0L14_2acasted__envS2009->$0;
  _M0L6_2acntS3770 = Moonbit_object_header(_M0L14_2acasted__envS2009)->rc;
  if (_M0L6_2acntS3770 > 1) {
    int32_t _M0L11_2anew__cntS3771 = _M0L6_2acntS3770 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2009)->rc
    = _M0L11_2anew__cntS3771;
    moonbit_incref(_M0L8_2afieldS3568);
  } else if (_M0L6_2acntS3770 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2009);
  }
  _M0L11curr__entryS683 = _M0L8_2afieldS3568;
  _M0L8_2afieldS3567 = _M0L11curr__entryS683->$0;
  _M0L7_2abindS685 = _M0L8_2afieldS3567;
  if (_M0L7_2abindS685 == 0) {
    moonbit_decref(_M0L11curr__entryS683);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS686 = _M0L7_2abindS685;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS687 = _M0L7_2aSomeS686;
    moonbit_string_t _M0L8_2afieldS3566 = _M0L4_2axS687->$4;
    moonbit_string_t _M0L6_2akeyS688 = _M0L8_2afieldS3566;
    void* _M0L8_2afieldS3565 = _M0L4_2axS687->$5;
    void* _M0L8_2avalueS689 = _M0L8_2afieldS3565;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3564 = _M0L4_2axS687->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS690 = _M0L8_2afieldS3564;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3563 =
      _M0L11curr__entryS683->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2010;
    if (_M0L7_2anextS690) {
      moonbit_incref(_M0L7_2anextS690);
    }
    moonbit_incref(_M0L8_2avalueS689);
    moonbit_incref(_M0L6_2akeyS688);
    if (_M0L6_2aoldS3563) {
      moonbit_decref(_M0L6_2aoldS3563);
    }
    _M0L11curr__entryS683->$0 = _M0L7_2anextS690;
    moonbit_decref(_M0L11curr__entryS683);
    _M0L8_2atupleS2010
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2010)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2010->$0 = _M0L6_2akeyS688;
    _M0L8_2atupleS2010->$1 = _M0L8_2avalueS689;
    return _M0L8_2atupleS2010;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS682
) {
  int32_t _M0L8_2afieldS3569;
  int32_t _M0L4sizeS2005;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3569 = _M0L4selfS682->$1;
  moonbit_decref(_M0L4selfS682);
  _M0L4sizeS2005 = _M0L8_2afieldS3569;
  return _M0L4sizeS2005 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS669,
  int32_t _M0L3keyS665
) {
  int32_t _M0L4hashS664;
  int32_t _M0L14capacity__maskS1990;
  int32_t _M0L6_2atmpS1989;
  int32_t _M0L1iS666;
  int32_t _M0L3idxS667;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS664 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS665);
  _M0L14capacity__maskS1990 = _M0L4selfS669->$3;
  _M0L6_2atmpS1989 = _M0L4hashS664 & _M0L14capacity__maskS1990;
  _M0L1iS666 = 0;
  _M0L3idxS667 = _M0L6_2atmpS1989;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3573 =
      _M0L4selfS669->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1988 =
      _M0L8_2afieldS3573;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3572;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS668;
    if (
      _M0L3idxS667 < 0
      || _M0L3idxS667 >= Moonbit_array_length(_M0L7entriesS1988)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3572
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1988[
        _M0L3idxS667
      ];
    _M0L7_2abindS668 = _M0L6_2atmpS3572;
    if (_M0L7_2abindS668 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1977;
      if (_M0L7_2abindS668) {
        moonbit_incref(_M0L7_2abindS668);
      }
      moonbit_decref(_M0L4selfS669);
      if (_M0L7_2abindS668) {
        moonbit_decref(_M0L7_2abindS668);
      }
      _M0L6_2atmpS1977 = 0;
      return _M0L6_2atmpS1977;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS670 =
        _M0L7_2abindS668;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS671 =
        _M0L7_2aSomeS670;
      int32_t _M0L4hashS1979 = _M0L8_2aentryS671->$3;
      int32_t _if__result_4019;
      int32_t _M0L8_2afieldS3570;
      int32_t _M0L3pslS1982;
      int32_t _M0L6_2atmpS1984;
      int32_t _M0L6_2atmpS1986;
      int32_t _M0L14capacity__maskS1987;
      int32_t _M0L6_2atmpS1985;
      if (_M0L4hashS1979 == _M0L4hashS664) {
        int32_t _M0L3keyS1978 = _M0L8_2aentryS671->$4;
        _if__result_4019 = _M0L3keyS1978 == _M0L3keyS665;
      } else {
        _if__result_4019 = 0;
      }
      if (_if__result_4019) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3571;
        int32_t _M0L6_2acntS3772;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1981;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1980;
        moonbit_incref(_M0L8_2aentryS671);
        moonbit_decref(_M0L4selfS669);
        _M0L8_2afieldS3571 = _M0L8_2aentryS671->$5;
        _M0L6_2acntS3772 = Moonbit_object_header(_M0L8_2aentryS671)->rc;
        if (_M0L6_2acntS3772 > 1) {
          int32_t _M0L11_2anew__cntS3774 = _M0L6_2acntS3772 - 1;
          Moonbit_object_header(_M0L8_2aentryS671)->rc
          = _M0L11_2anew__cntS3774;
          moonbit_incref(_M0L8_2afieldS3571);
        } else if (_M0L6_2acntS3772 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3773 =
            _M0L8_2aentryS671->$1;
          if (_M0L8_2afieldS3773) {
            moonbit_decref(_M0L8_2afieldS3773);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS671);
        }
        _M0L5valueS1981 = _M0L8_2afieldS3571;
        _M0L6_2atmpS1980 = _M0L5valueS1981;
        return _M0L6_2atmpS1980;
      } else {
        moonbit_incref(_M0L8_2aentryS671);
      }
      _M0L8_2afieldS3570 = _M0L8_2aentryS671->$2;
      moonbit_decref(_M0L8_2aentryS671);
      _M0L3pslS1982 = _M0L8_2afieldS3570;
      if (_M0L1iS666 > _M0L3pslS1982) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1983;
        moonbit_decref(_M0L4selfS669);
        _M0L6_2atmpS1983 = 0;
        return _M0L6_2atmpS1983;
      }
      _M0L6_2atmpS1984 = _M0L1iS666 + 1;
      _M0L6_2atmpS1986 = _M0L3idxS667 + 1;
      _M0L14capacity__maskS1987 = _M0L4selfS669->$3;
      _M0L6_2atmpS1985 = _M0L6_2atmpS1986 & _M0L14capacity__maskS1987;
      _M0L1iS666 = _M0L6_2atmpS1984;
      _M0L3idxS667 = _M0L6_2atmpS1985;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS678,
  moonbit_string_t _M0L3keyS674
) {
  int32_t _M0L4hashS673;
  int32_t _M0L14capacity__maskS2004;
  int32_t _M0L6_2atmpS2003;
  int32_t _M0L1iS675;
  int32_t _M0L3idxS676;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS674);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS673 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS674);
  _M0L14capacity__maskS2004 = _M0L4selfS678->$3;
  _M0L6_2atmpS2003 = _M0L4hashS673 & _M0L14capacity__maskS2004;
  _M0L1iS675 = 0;
  _M0L3idxS676 = _M0L6_2atmpS2003;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3579 =
      _M0L4selfS678->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2002 =
      _M0L8_2afieldS3579;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3578;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS677;
    if (
      _M0L3idxS676 < 0
      || _M0L3idxS676 >= Moonbit_array_length(_M0L7entriesS2002)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3578
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2002[
        _M0L3idxS676
      ];
    _M0L7_2abindS677 = _M0L6_2atmpS3578;
    if (_M0L7_2abindS677 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1991;
      if (_M0L7_2abindS677) {
        moonbit_incref(_M0L7_2abindS677);
      }
      moonbit_decref(_M0L4selfS678);
      if (_M0L7_2abindS677) {
        moonbit_decref(_M0L7_2abindS677);
      }
      moonbit_decref(_M0L3keyS674);
      _M0L6_2atmpS1991 = 0;
      return _M0L6_2atmpS1991;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS679 =
        _M0L7_2abindS677;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS680 =
        _M0L7_2aSomeS679;
      int32_t _M0L4hashS1993 = _M0L8_2aentryS680->$3;
      int32_t _if__result_4021;
      int32_t _M0L8_2afieldS3574;
      int32_t _M0L3pslS1996;
      int32_t _M0L6_2atmpS1998;
      int32_t _M0L6_2atmpS2000;
      int32_t _M0L14capacity__maskS2001;
      int32_t _M0L6_2atmpS1999;
      if (_M0L4hashS1993 == _M0L4hashS673) {
        moonbit_string_t _M0L8_2afieldS3577 = _M0L8_2aentryS680->$4;
        moonbit_string_t _M0L3keyS1992 = _M0L8_2afieldS3577;
        int32_t _M0L6_2atmpS3576;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3576
        = moonbit_val_array_equal(_M0L3keyS1992, _M0L3keyS674);
        _if__result_4021 = _M0L6_2atmpS3576;
      } else {
        _if__result_4021 = 0;
      }
      if (_if__result_4021) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3575;
        int32_t _M0L6_2acntS3775;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1995;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1994;
        moonbit_incref(_M0L8_2aentryS680);
        moonbit_decref(_M0L4selfS678);
        moonbit_decref(_M0L3keyS674);
        _M0L8_2afieldS3575 = _M0L8_2aentryS680->$5;
        _M0L6_2acntS3775 = Moonbit_object_header(_M0L8_2aentryS680)->rc;
        if (_M0L6_2acntS3775 > 1) {
          int32_t _M0L11_2anew__cntS3778 = _M0L6_2acntS3775 - 1;
          Moonbit_object_header(_M0L8_2aentryS680)->rc
          = _M0L11_2anew__cntS3778;
          moonbit_incref(_M0L8_2afieldS3575);
        } else if (_M0L6_2acntS3775 == 1) {
          moonbit_string_t _M0L8_2afieldS3777 = _M0L8_2aentryS680->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3776;
          moonbit_decref(_M0L8_2afieldS3777);
          _M0L8_2afieldS3776 = _M0L8_2aentryS680->$1;
          if (_M0L8_2afieldS3776) {
            moonbit_decref(_M0L8_2afieldS3776);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS680);
        }
        _M0L5valueS1995 = _M0L8_2afieldS3575;
        _M0L6_2atmpS1994 = _M0L5valueS1995;
        return _M0L6_2atmpS1994;
      } else {
        moonbit_incref(_M0L8_2aentryS680);
      }
      _M0L8_2afieldS3574 = _M0L8_2aentryS680->$2;
      moonbit_decref(_M0L8_2aentryS680);
      _M0L3pslS1996 = _M0L8_2afieldS3574;
      if (_M0L1iS675 > _M0L3pslS1996) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1997;
        moonbit_decref(_M0L4selfS678);
        moonbit_decref(_M0L3keyS674);
        _M0L6_2atmpS1997 = 0;
        return _M0L6_2atmpS1997;
      }
      _M0L6_2atmpS1998 = _M0L1iS675 + 1;
      _M0L6_2atmpS2000 = _M0L3idxS676 + 1;
      _M0L14capacity__maskS2001 = _M0L4selfS678->$3;
      _M0L6_2atmpS1999 = _M0L6_2atmpS2000 & _M0L14capacity__maskS2001;
      _M0L1iS675 = _M0L6_2atmpS1998;
      _M0L3idxS676 = _M0L6_2atmpS1999;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS649
) {
  int32_t _M0L6lengthS648;
  int32_t _M0Lm8capacityS650;
  int32_t _M0L6_2atmpS1954;
  int32_t _M0L6_2atmpS1953;
  int32_t _M0L6_2atmpS1964;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS651;
  int32_t _M0L3endS1962;
  int32_t _M0L5startS1963;
  int32_t _M0L7_2abindS652;
  int32_t _M0L2__S653;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS649.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS648
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS649);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS650 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS648);
  _M0L6_2atmpS1954 = _M0Lm8capacityS650;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1953 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1954);
  if (_M0L6lengthS648 > _M0L6_2atmpS1953) {
    int32_t _M0L6_2atmpS1955 = _M0Lm8capacityS650;
    _M0Lm8capacityS650 = _M0L6_2atmpS1955 * 2;
  }
  _M0L6_2atmpS1964 = _M0Lm8capacityS650;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS651
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1964);
  _M0L3endS1962 = _M0L3arrS649.$2;
  _M0L5startS1963 = _M0L3arrS649.$1;
  _M0L7_2abindS652 = _M0L3endS1962 - _M0L5startS1963;
  _M0L2__S653 = 0;
  while (1) {
    if (_M0L2__S653 < _M0L7_2abindS652) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3583 =
        _M0L3arrS649.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1959 =
        _M0L8_2afieldS3583;
      int32_t _M0L5startS1961 = _M0L3arrS649.$1;
      int32_t _M0L6_2atmpS1960 = _M0L5startS1961 + _M0L2__S653;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3582 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1959[
          _M0L6_2atmpS1960
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS654 =
        _M0L6_2atmpS3582;
      moonbit_string_t _M0L8_2afieldS3581 = _M0L1eS654->$0;
      moonbit_string_t _M0L6_2atmpS1956 = _M0L8_2afieldS3581;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3580 =
        _M0L1eS654->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1957 =
        _M0L8_2afieldS3580;
      int32_t _M0L6_2atmpS1958;
      moonbit_incref(_M0L6_2atmpS1957);
      moonbit_incref(_M0L6_2atmpS1956);
      moonbit_incref(_M0L1mS651);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS651, _M0L6_2atmpS1956, _M0L6_2atmpS1957);
      _M0L6_2atmpS1958 = _M0L2__S653 + 1;
      _M0L2__S653 = _M0L6_2atmpS1958;
      continue;
    } else {
      moonbit_decref(_M0L3arrS649.$0);
    }
    break;
  }
  return _M0L1mS651;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS657
) {
  int32_t _M0L6lengthS656;
  int32_t _M0Lm8capacityS658;
  int32_t _M0L6_2atmpS1966;
  int32_t _M0L6_2atmpS1965;
  int32_t _M0L6_2atmpS1976;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS659;
  int32_t _M0L3endS1974;
  int32_t _M0L5startS1975;
  int32_t _M0L7_2abindS660;
  int32_t _M0L2__S661;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS657.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS656
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS657);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS658 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS656);
  _M0L6_2atmpS1966 = _M0Lm8capacityS658;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1965 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1966);
  if (_M0L6lengthS656 > _M0L6_2atmpS1965) {
    int32_t _M0L6_2atmpS1967 = _M0Lm8capacityS658;
    _M0Lm8capacityS658 = _M0L6_2atmpS1967 * 2;
  }
  _M0L6_2atmpS1976 = _M0Lm8capacityS658;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS659
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1976);
  _M0L3endS1974 = _M0L3arrS657.$2;
  _M0L5startS1975 = _M0L3arrS657.$1;
  _M0L7_2abindS660 = _M0L3endS1974 - _M0L5startS1975;
  _M0L2__S661 = 0;
  while (1) {
    if (_M0L2__S661 < _M0L7_2abindS660) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3586 =
        _M0L3arrS657.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1971 =
        _M0L8_2afieldS3586;
      int32_t _M0L5startS1973 = _M0L3arrS657.$1;
      int32_t _M0L6_2atmpS1972 = _M0L5startS1973 + _M0L2__S661;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3585 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1971[
          _M0L6_2atmpS1972
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS662 = _M0L6_2atmpS3585;
      int32_t _M0L6_2atmpS1968 = _M0L1eS662->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3584 =
        _M0L1eS662->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1969 =
        _M0L8_2afieldS3584;
      int32_t _M0L6_2atmpS1970;
      moonbit_incref(_M0L6_2atmpS1969);
      moonbit_incref(_M0L1mS659);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS659, _M0L6_2atmpS1968, _M0L6_2atmpS1969);
      _M0L6_2atmpS1970 = _M0L2__S661 + 1;
      _M0L2__S661 = _M0L6_2atmpS1970;
      continue;
    } else {
      moonbit_decref(_M0L3arrS657.$0);
    }
    break;
  }
  return _M0L1mS659;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS642,
  moonbit_string_t _M0L3keyS643,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS644
) {
  int32_t _M0L6_2atmpS1951;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS643);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1951 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS643);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS642, _M0L3keyS643, _M0L5valueS644, _M0L6_2atmpS1951);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS645,
  int32_t _M0L3keyS646,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS647
) {
  int32_t _M0L6_2atmpS1952;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1952 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS646);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS645, _M0L3keyS646, _M0L5valueS647, _M0L6_2atmpS1952);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS621
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3593;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS620;
  int32_t _M0L8capacityS1943;
  int32_t _M0L13new__capacityS622;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1938;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1937;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3592;
  int32_t _M0L6_2atmpS1939;
  int32_t _M0L8capacityS1941;
  int32_t _M0L6_2atmpS1940;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1942;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3591;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS623;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3593 = _M0L4selfS621->$5;
  _M0L9old__headS620 = _M0L8_2afieldS3593;
  _M0L8capacityS1943 = _M0L4selfS621->$2;
  _M0L13new__capacityS622 = _M0L8capacityS1943 << 1;
  _M0L6_2atmpS1938 = 0;
  _M0L6_2atmpS1937
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS622, _M0L6_2atmpS1938);
  _M0L6_2aoldS3592 = _M0L4selfS621->$0;
  if (_M0L9old__headS620) {
    moonbit_incref(_M0L9old__headS620);
  }
  moonbit_decref(_M0L6_2aoldS3592);
  _M0L4selfS621->$0 = _M0L6_2atmpS1937;
  _M0L4selfS621->$2 = _M0L13new__capacityS622;
  _M0L6_2atmpS1939 = _M0L13new__capacityS622 - 1;
  _M0L4selfS621->$3 = _M0L6_2atmpS1939;
  _M0L8capacityS1941 = _M0L4selfS621->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1940 = _M0FPB21calc__grow__threshold(_M0L8capacityS1941);
  _M0L4selfS621->$4 = _M0L6_2atmpS1940;
  _M0L4selfS621->$1 = 0;
  _M0L6_2atmpS1942 = 0;
  _M0L6_2aoldS3591 = _M0L4selfS621->$5;
  if (_M0L6_2aoldS3591) {
    moonbit_decref(_M0L6_2aoldS3591);
  }
  _M0L4selfS621->$5 = _M0L6_2atmpS1942;
  _M0L4selfS621->$6 = -1;
  _M0L8_2aparamS623 = _M0L9old__headS620;
  while (1) {
    if (_M0L8_2aparamS623 == 0) {
      if (_M0L8_2aparamS623) {
        moonbit_decref(_M0L8_2aparamS623);
      }
      moonbit_decref(_M0L4selfS621);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS624 =
        _M0L8_2aparamS623;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS625 =
        _M0L7_2aSomeS624;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3590 =
        _M0L4_2axS625->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS626 =
        _M0L8_2afieldS3590;
      moonbit_string_t _M0L8_2afieldS3589 = _M0L4_2axS625->$4;
      moonbit_string_t _M0L6_2akeyS627 = _M0L8_2afieldS3589;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3588 =
        _M0L4_2axS625->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS628 =
        _M0L8_2afieldS3588;
      int32_t _M0L8_2afieldS3587 = _M0L4_2axS625->$3;
      int32_t _M0L6_2acntS3779 = Moonbit_object_header(_M0L4_2axS625)->rc;
      int32_t _M0L7_2ahashS629;
      if (_M0L6_2acntS3779 > 1) {
        int32_t _M0L11_2anew__cntS3780 = _M0L6_2acntS3779 - 1;
        Moonbit_object_header(_M0L4_2axS625)->rc = _M0L11_2anew__cntS3780;
        moonbit_incref(_M0L8_2avalueS628);
        moonbit_incref(_M0L6_2akeyS627);
        if (_M0L7_2anextS626) {
          moonbit_incref(_M0L7_2anextS626);
        }
      } else if (_M0L6_2acntS3779 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS625);
      }
      _M0L7_2ahashS629 = _M0L8_2afieldS3587;
      moonbit_incref(_M0L4selfS621);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS621, _M0L6_2akeyS627, _M0L8_2avalueS628, _M0L7_2ahashS629);
      _M0L8_2aparamS623 = _M0L7_2anextS626;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS632
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3599;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS631;
  int32_t _M0L8capacityS1950;
  int32_t _M0L13new__capacityS633;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1945;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1944;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3598;
  int32_t _M0L6_2atmpS1946;
  int32_t _M0L8capacityS1948;
  int32_t _M0L6_2atmpS1947;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1949;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3597;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS634;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3599 = _M0L4selfS632->$5;
  _M0L9old__headS631 = _M0L8_2afieldS3599;
  _M0L8capacityS1950 = _M0L4selfS632->$2;
  _M0L13new__capacityS633 = _M0L8capacityS1950 << 1;
  _M0L6_2atmpS1945 = 0;
  _M0L6_2atmpS1944
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS633, _M0L6_2atmpS1945);
  _M0L6_2aoldS3598 = _M0L4selfS632->$0;
  if (_M0L9old__headS631) {
    moonbit_incref(_M0L9old__headS631);
  }
  moonbit_decref(_M0L6_2aoldS3598);
  _M0L4selfS632->$0 = _M0L6_2atmpS1944;
  _M0L4selfS632->$2 = _M0L13new__capacityS633;
  _M0L6_2atmpS1946 = _M0L13new__capacityS633 - 1;
  _M0L4selfS632->$3 = _M0L6_2atmpS1946;
  _M0L8capacityS1948 = _M0L4selfS632->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1947 = _M0FPB21calc__grow__threshold(_M0L8capacityS1948);
  _M0L4selfS632->$4 = _M0L6_2atmpS1947;
  _M0L4selfS632->$1 = 0;
  _M0L6_2atmpS1949 = 0;
  _M0L6_2aoldS3597 = _M0L4selfS632->$5;
  if (_M0L6_2aoldS3597) {
    moonbit_decref(_M0L6_2aoldS3597);
  }
  _M0L4selfS632->$5 = _M0L6_2atmpS1949;
  _M0L4selfS632->$6 = -1;
  _M0L8_2aparamS634 = _M0L9old__headS631;
  while (1) {
    if (_M0L8_2aparamS634 == 0) {
      if (_M0L8_2aparamS634) {
        moonbit_decref(_M0L8_2aparamS634);
      }
      moonbit_decref(_M0L4selfS632);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS635 =
        _M0L8_2aparamS634;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS636 =
        _M0L7_2aSomeS635;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3596 =
        _M0L4_2axS636->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS637 =
        _M0L8_2afieldS3596;
      int32_t _M0L6_2akeyS638 = _M0L4_2axS636->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3595 =
        _M0L4_2axS636->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS639 =
        _M0L8_2afieldS3595;
      int32_t _M0L8_2afieldS3594 = _M0L4_2axS636->$3;
      int32_t _M0L6_2acntS3781 = Moonbit_object_header(_M0L4_2axS636)->rc;
      int32_t _M0L7_2ahashS640;
      if (_M0L6_2acntS3781 > 1) {
        int32_t _M0L11_2anew__cntS3782 = _M0L6_2acntS3781 - 1;
        Moonbit_object_header(_M0L4_2axS636)->rc = _M0L11_2anew__cntS3782;
        moonbit_incref(_M0L8_2avalueS639);
        if (_M0L7_2anextS637) {
          moonbit_incref(_M0L7_2anextS637);
        }
      } else if (_M0L6_2acntS3781 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS636);
      }
      _M0L7_2ahashS640 = _M0L8_2afieldS3594;
      moonbit_incref(_M0L4selfS632);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS632, _M0L6_2akeyS638, _M0L8_2avalueS639, _M0L7_2ahashS640);
      _M0L8_2aparamS634 = _M0L7_2anextS637;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS591,
  moonbit_string_t _M0L3keyS597,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS598,
  int32_t _M0L4hashS593
) {
  int32_t _M0L14capacity__maskS1918;
  int32_t _M0L6_2atmpS1917;
  int32_t _M0L3pslS588;
  int32_t _M0L3idxS589;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1918 = _M0L4selfS591->$3;
  _M0L6_2atmpS1917 = _M0L4hashS593 & _M0L14capacity__maskS1918;
  _M0L3pslS588 = 0;
  _M0L3idxS589 = _M0L6_2atmpS1917;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3604 =
      _M0L4selfS591->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1916 =
      _M0L8_2afieldS3604;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3603;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS590;
    if (
      _M0L3idxS589 < 0
      || _M0L3idxS589 >= Moonbit_array_length(_M0L7entriesS1916)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3603
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1916[
        _M0L3idxS589
      ];
    _M0L7_2abindS590 = _M0L6_2atmpS3603;
    if (_M0L7_2abindS590 == 0) {
      int32_t _M0L4sizeS1901 = _M0L4selfS591->$1;
      int32_t _M0L8grow__atS1902 = _M0L4selfS591->$4;
      int32_t _M0L7_2abindS594;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS595;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS596;
      if (_M0L4sizeS1901 >= _M0L8grow__atS1902) {
        int32_t _M0L14capacity__maskS1904;
        int32_t _M0L6_2atmpS1903;
        moonbit_incref(_M0L4selfS591);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS591);
        _M0L14capacity__maskS1904 = _M0L4selfS591->$3;
        _M0L6_2atmpS1903 = _M0L4hashS593 & _M0L14capacity__maskS1904;
        _M0L3pslS588 = 0;
        _M0L3idxS589 = _M0L6_2atmpS1903;
        continue;
      }
      _M0L7_2abindS594 = _M0L4selfS591->$6;
      _M0L7_2abindS595 = 0;
      _M0L5entryS596
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS596)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS596->$0 = _M0L7_2abindS594;
      _M0L5entryS596->$1 = _M0L7_2abindS595;
      _M0L5entryS596->$2 = _M0L3pslS588;
      _M0L5entryS596->$3 = _M0L4hashS593;
      _M0L5entryS596->$4 = _M0L3keyS597;
      _M0L5entryS596->$5 = _M0L5valueS598;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS591, _M0L3idxS589, _M0L5entryS596);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS599 =
        _M0L7_2abindS590;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS600 =
        _M0L7_2aSomeS599;
      int32_t _M0L4hashS1906 = _M0L14_2acurr__entryS600->$3;
      int32_t _if__result_4027;
      int32_t _M0L3pslS1907;
      int32_t _M0L6_2atmpS1912;
      int32_t _M0L6_2atmpS1914;
      int32_t _M0L14capacity__maskS1915;
      int32_t _M0L6_2atmpS1913;
      if (_M0L4hashS1906 == _M0L4hashS593) {
        moonbit_string_t _M0L8_2afieldS3602 = _M0L14_2acurr__entryS600->$4;
        moonbit_string_t _M0L3keyS1905 = _M0L8_2afieldS3602;
        int32_t _M0L6_2atmpS3601;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3601
        = moonbit_val_array_equal(_M0L3keyS1905, _M0L3keyS597);
        _if__result_4027 = _M0L6_2atmpS3601;
      } else {
        _if__result_4027 = 0;
      }
      if (_if__result_4027) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3600;
        moonbit_incref(_M0L14_2acurr__entryS600);
        moonbit_decref(_M0L3keyS597);
        moonbit_decref(_M0L4selfS591);
        _M0L6_2aoldS3600 = _M0L14_2acurr__entryS600->$5;
        moonbit_decref(_M0L6_2aoldS3600);
        _M0L14_2acurr__entryS600->$5 = _M0L5valueS598;
        moonbit_decref(_M0L14_2acurr__entryS600);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS600);
      }
      _M0L3pslS1907 = _M0L14_2acurr__entryS600->$2;
      if (_M0L3pslS588 > _M0L3pslS1907) {
        int32_t _M0L4sizeS1908 = _M0L4selfS591->$1;
        int32_t _M0L8grow__atS1909 = _M0L4selfS591->$4;
        int32_t _M0L7_2abindS601;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS602;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS603;
        if (_M0L4sizeS1908 >= _M0L8grow__atS1909) {
          int32_t _M0L14capacity__maskS1911;
          int32_t _M0L6_2atmpS1910;
          moonbit_decref(_M0L14_2acurr__entryS600);
          moonbit_incref(_M0L4selfS591);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS591);
          _M0L14capacity__maskS1911 = _M0L4selfS591->$3;
          _M0L6_2atmpS1910 = _M0L4hashS593 & _M0L14capacity__maskS1911;
          _M0L3pslS588 = 0;
          _M0L3idxS589 = _M0L6_2atmpS1910;
          continue;
        }
        moonbit_incref(_M0L4selfS591);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS591, _M0L3idxS589, _M0L14_2acurr__entryS600);
        _M0L7_2abindS601 = _M0L4selfS591->$6;
        _M0L7_2abindS602 = 0;
        _M0L5entryS603
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS603)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS603->$0 = _M0L7_2abindS601;
        _M0L5entryS603->$1 = _M0L7_2abindS602;
        _M0L5entryS603->$2 = _M0L3pslS588;
        _M0L5entryS603->$3 = _M0L4hashS593;
        _M0L5entryS603->$4 = _M0L3keyS597;
        _M0L5entryS603->$5 = _M0L5valueS598;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS591, _M0L3idxS589, _M0L5entryS603);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS600);
      }
      _M0L6_2atmpS1912 = _M0L3pslS588 + 1;
      _M0L6_2atmpS1914 = _M0L3idxS589 + 1;
      _M0L14capacity__maskS1915 = _M0L4selfS591->$3;
      _M0L6_2atmpS1913 = _M0L6_2atmpS1914 & _M0L14capacity__maskS1915;
      _M0L3pslS588 = _M0L6_2atmpS1912;
      _M0L3idxS589 = _M0L6_2atmpS1913;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS607,
  int32_t _M0L3keyS613,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS614,
  int32_t _M0L4hashS609
) {
  int32_t _M0L14capacity__maskS1936;
  int32_t _M0L6_2atmpS1935;
  int32_t _M0L3pslS604;
  int32_t _M0L3idxS605;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1936 = _M0L4selfS607->$3;
  _M0L6_2atmpS1935 = _M0L4hashS609 & _M0L14capacity__maskS1936;
  _M0L3pslS604 = 0;
  _M0L3idxS605 = _M0L6_2atmpS1935;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3607 =
      _M0L4selfS607->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1934 =
      _M0L8_2afieldS3607;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3606;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS606;
    if (
      _M0L3idxS605 < 0
      || _M0L3idxS605 >= Moonbit_array_length(_M0L7entriesS1934)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3606
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1934[
        _M0L3idxS605
      ];
    _M0L7_2abindS606 = _M0L6_2atmpS3606;
    if (_M0L7_2abindS606 == 0) {
      int32_t _M0L4sizeS1919 = _M0L4selfS607->$1;
      int32_t _M0L8grow__atS1920 = _M0L4selfS607->$4;
      int32_t _M0L7_2abindS610;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS611;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS612;
      if (_M0L4sizeS1919 >= _M0L8grow__atS1920) {
        int32_t _M0L14capacity__maskS1922;
        int32_t _M0L6_2atmpS1921;
        moonbit_incref(_M0L4selfS607);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS607);
        _M0L14capacity__maskS1922 = _M0L4selfS607->$3;
        _M0L6_2atmpS1921 = _M0L4hashS609 & _M0L14capacity__maskS1922;
        _M0L3pslS604 = 0;
        _M0L3idxS605 = _M0L6_2atmpS1921;
        continue;
      }
      _M0L7_2abindS610 = _M0L4selfS607->$6;
      _M0L7_2abindS611 = 0;
      _M0L5entryS612
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS612)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS612->$0 = _M0L7_2abindS610;
      _M0L5entryS612->$1 = _M0L7_2abindS611;
      _M0L5entryS612->$2 = _M0L3pslS604;
      _M0L5entryS612->$3 = _M0L4hashS609;
      _M0L5entryS612->$4 = _M0L3keyS613;
      _M0L5entryS612->$5 = _M0L5valueS614;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS607, _M0L3idxS605, _M0L5entryS612);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS615 =
        _M0L7_2abindS606;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS616 =
        _M0L7_2aSomeS615;
      int32_t _M0L4hashS1924 = _M0L14_2acurr__entryS616->$3;
      int32_t _if__result_4029;
      int32_t _M0L3pslS1925;
      int32_t _M0L6_2atmpS1930;
      int32_t _M0L6_2atmpS1932;
      int32_t _M0L14capacity__maskS1933;
      int32_t _M0L6_2atmpS1931;
      if (_M0L4hashS1924 == _M0L4hashS609) {
        int32_t _M0L3keyS1923 = _M0L14_2acurr__entryS616->$4;
        _if__result_4029 = _M0L3keyS1923 == _M0L3keyS613;
      } else {
        _if__result_4029 = 0;
      }
      if (_if__result_4029) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3605;
        moonbit_incref(_M0L14_2acurr__entryS616);
        moonbit_decref(_M0L4selfS607);
        _M0L6_2aoldS3605 = _M0L14_2acurr__entryS616->$5;
        moonbit_decref(_M0L6_2aoldS3605);
        _M0L14_2acurr__entryS616->$5 = _M0L5valueS614;
        moonbit_decref(_M0L14_2acurr__entryS616);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS616);
      }
      _M0L3pslS1925 = _M0L14_2acurr__entryS616->$2;
      if (_M0L3pslS604 > _M0L3pslS1925) {
        int32_t _M0L4sizeS1926 = _M0L4selfS607->$1;
        int32_t _M0L8grow__atS1927 = _M0L4selfS607->$4;
        int32_t _M0L7_2abindS617;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS618;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS619;
        if (_M0L4sizeS1926 >= _M0L8grow__atS1927) {
          int32_t _M0L14capacity__maskS1929;
          int32_t _M0L6_2atmpS1928;
          moonbit_decref(_M0L14_2acurr__entryS616);
          moonbit_incref(_M0L4selfS607);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS607);
          _M0L14capacity__maskS1929 = _M0L4selfS607->$3;
          _M0L6_2atmpS1928 = _M0L4hashS609 & _M0L14capacity__maskS1929;
          _M0L3pslS604 = 0;
          _M0L3idxS605 = _M0L6_2atmpS1928;
          continue;
        }
        moonbit_incref(_M0L4selfS607);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS607, _M0L3idxS605, _M0L14_2acurr__entryS616);
        _M0L7_2abindS617 = _M0L4selfS607->$6;
        _M0L7_2abindS618 = 0;
        _M0L5entryS619
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS619)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS619->$0 = _M0L7_2abindS617;
        _M0L5entryS619->$1 = _M0L7_2abindS618;
        _M0L5entryS619->$2 = _M0L3pslS604;
        _M0L5entryS619->$3 = _M0L4hashS609;
        _M0L5entryS619->$4 = _M0L3keyS613;
        _M0L5entryS619->$5 = _M0L5valueS614;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS607, _M0L3idxS605, _M0L5entryS619);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS616);
      }
      _M0L6_2atmpS1930 = _M0L3pslS604 + 1;
      _M0L6_2atmpS1932 = _M0L3idxS605 + 1;
      _M0L14capacity__maskS1933 = _M0L4selfS607->$3;
      _M0L6_2atmpS1931 = _M0L6_2atmpS1932 & _M0L14capacity__maskS1933;
      _M0L3pslS604 = _M0L6_2atmpS1930;
      _M0L3idxS605 = _M0L6_2atmpS1931;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS572,
  int32_t _M0L3idxS577,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS576
) {
  int32_t _M0L3pslS1884;
  int32_t _M0L6_2atmpS1880;
  int32_t _M0L6_2atmpS1882;
  int32_t _M0L14capacity__maskS1883;
  int32_t _M0L6_2atmpS1881;
  int32_t _M0L3pslS568;
  int32_t _M0L3idxS569;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS570;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1884 = _M0L5entryS576->$2;
  _M0L6_2atmpS1880 = _M0L3pslS1884 + 1;
  _M0L6_2atmpS1882 = _M0L3idxS577 + 1;
  _M0L14capacity__maskS1883 = _M0L4selfS572->$3;
  _M0L6_2atmpS1881 = _M0L6_2atmpS1882 & _M0L14capacity__maskS1883;
  _M0L3pslS568 = _M0L6_2atmpS1880;
  _M0L3idxS569 = _M0L6_2atmpS1881;
  _M0L5entryS570 = _M0L5entryS576;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3609 =
      _M0L4selfS572->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1879 =
      _M0L8_2afieldS3609;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3608;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS571;
    if (
      _M0L3idxS569 < 0
      || _M0L3idxS569 >= Moonbit_array_length(_M0L7entriesS1879)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3608
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1879[
        _M0L3idxS569
      ];
    _M0L7_2abindS571 = _M0L6_2atmpS3608;
    if (_M0L7_2abindS571 == 0) {
      _M0L5entryS570->$2 = _M0L3pslS568;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS572, _M0L5entryS570, _M0L3idxS569);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS574 =
        _M0L7_2abindS571;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS575 =
        _M0L7_2aSomeS574;
      int32_t _M0L3pslS1869 = _M0L14_2acurr__entryS575->$2;
      if (_M0L3pslS568 > _M0L3pslS1869) {
        int32_t _M0L3pslS1874;
        int32_t _M0L6_2atmpS1870;
        int32_t _M0L6_2atmpS1872;
        int32_t _M0L14capacity__maskS1873;
        int32_t _M0L6_2atmpS1871;
        _M0L5entryS570->$2 = _M0L3pslS568;
        moonbit_incref(_M0L14_2acurr__entryS575);
        moonbit_incref(_M0L4selfS572);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS572, _M0L5entryS570, _M0L3idxS569);
        _M0L3pslS1874 = _M0L14_2acurr__entryS575->$2;
        _M0L6_2atmpS1870 = _M0L3pslS1874 + 1;
        _M0L6_2atmpS1872 = _M0L3idxS569 + 1;
        _M0L14capacity__maskS1873 = _M0L4selfS572->$3;
        _M0L6_2atmpS1871 = _M0L6_2atmpS1872 & _M0L14capacity__maskS1873;
        _M0L3pslS568 = _M0L6_2atmpS1870;
        _M0L3idxS569 = _M0L6_2atmpS1871;
        _M0L5entryS570 = _M0L14_2acurr__entryS575;
        continue;
      } else {
        int32_t _M0L6_2atmpS1875 = _M0L3pslS568 + 1;
        int32_t _M0L6_2atmpS1877 = _M0L3idxS569 + 1;
        int32_t _M0L14capacity__maskS1878 = _M0L4selfS572->$3;
        int32_t _M0L6_2atmpS1876 =
          _M0L6_2atmpS1877 & _M0L14capacity__maskS1878;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4031 =
          _M0L5entryS570;
        _M0L3pslS568 = _M0L6_2atmpS1875;
        _M0L3idxS569 = _M0L6_2atmpS1876;
        _M0L5entryS570 = _tmp_4031;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS582,
  int32_t _M0L3idxS587,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS586
) {
  int32_t _M0L3pslS1900;
  int32_t _M0L6_2atmpS1896;
  int32_t _M0L6_2atmpS1898;
  int32_t _M0L14capacity__maskS1899;
  int32_t _M0L6_2atmpS1897;
  int32_t _M0L3pslS578;
  int32_t _M0L3idxS579;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS580;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1900 = _M0L5entryS586->$2;
  _M0L6_2atmpS1896 = _M0L3pslS1900 + 1;
  _M0L6_2atmpS1898 = _M0L3idxS587 + 1;
  _M0L14capacity__maskS1899 = _M0L4selfS582->$3;
  _M0L6_2atmpS1897 = _M0L6_2atmpS1898 & _M0L14capacity__maskS1899;
  _M0L3pslS578 = _M0L6_2atmpS1896;
  _M0L3idxS579 = _M0L6_2atmpS1897;
  _M0L5entryS580 = _M0L5entryS586;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3611 =
      _M0L4selfS582->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1895 =
      _M0L8_2afieldS3611;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3610;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS581;
    if (
      _M0L3idxS579 < 0
      || _M0L3idxS579 >= Moonbit_array_length(_M0L7entriesS1895)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3610
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1895[
        _M0L3idxS579
      ];
    _M0L7_2abindS581 = _M0L6_2atmpS3610;
    if (_M0L7_2abindS581 == 0) {
      _M0L5entryS580->$2 = _M0L3pslS578;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS582, _M0L5entryS580, _M0L3idxS579);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS584 =
        _M0L7_2abindS581;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS585 =
        _M0L7_2aSomeS584;
      int32_t _M0L3pslS1885 = _M0L14_2acurr__entryS585->$2;
      if (_M0L3pslS578 > _M0L3pslS1885) {
        int32_t _M0L3pslS1890;
        int32_t _M0L6_2atmpS1886;
        int32_t _M0L6_2atmpS1888;
        int32_t _M0L14capacity__maskS1889;
        int32_t _M0L6_2atmpS1887;
        _M0L5entryS580->$2 = _M0L3pslS578;
        moonbit_incref(_M0L14_2acurr__entryS585);
        moonbit_incref(_M0L4selfS582);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS582, _M0L5entryS580, _M0L3idxS579);
        _M0L3pslS1890 = _M0L14_2acurr__entryS585->$2;
        _M0L6_2atmpS1886 = _M0L3pslS1890 + 1;
        _M0L6_2atmpS1888 = _M0L3idxS579 + 1;
        _M0L14capacity__maskS1889 = _M0L4selfS582->$3;
        _M0L6_2atmpS1887 = _M0L6_2atmpS1888 & _M0L14capacity__maskS1889;
        _M0L3pslS578 = _M0L6_2atmpS1886;
        _M0L3idxS579 = _M0L6_2atmpS1887;
        _M0L5entryS580 = _M0L14_2acurr__entryS585;
        continue;
      } else {
        int32_t _M0L6_2atmpS1891 = _M0L3pslS578 + 1;
        int32_t _M0L6_2atmpS1893 = _M0L3idxS579 + 1;
        int32_t _M0L14capacity__maskS1894 = _M0L4selfS582->$3;
        int32_t _M0L6_2atmpS1892 =
          _M0L6_2atmpS1893 & _M0L14capacity__maskS1894;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4033 =
          _M0L5entryS580;
        _M0L3pslS578 = _M0L6_2atmpS1891;
        _M0L3idxS579 = _M0L6_2atmpS1892;
        _M0L5entryS580 = _tmp_4033;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS556,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS558,
  int32_t _M0L8new__idxS557
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3614;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1865;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1866;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3613;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3612;
  int32_t _M0L6_2acntS3783;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS559;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3614 = _M0L4selfS556->$0;
  _M0L7entriesS1865 = _M0L8_2afieldS3614;
  moonbit_incref(_M0L5entryS558);
  _M0L6_2atmpS1866 = _M0L5entryS558;
  if (
    _M0L8new__idxS557 < 0
    || _M0L8new__idxS557 >= Moonbit_array_length(_M0L7entriesS1865)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3613
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1865[
      _M0L8new__idxS557
    ];
  if (_M0L6_2aoldS3613) {
    moonbit_decref(_M0L6_2aoldS3613);
  }
  _M0L7entriesS1865[_M0L8new__idxS557] = _M0L6_2atmpS1866;
  _M0L8_2afieldS3612 = _M0L5entryS558->$1;
  _M0L6_2acntS3783 = Moonbit_object_header(_M0L5entryS558)->rc;
  if (_M0L6_2acntS3783 > 1) {
    int32_t _M0L11_2anew__cntS3786 = _M0L6_2acntS3783 - 1;
    Moonbit_object_header(_M0L5entryS558)->rc = _M0L11_2anew__cntS3786;
    if (_M0L8_2afieldS3612) {
      moonbit_incref(_M0L8_2afieldS3612);
    }
  } else if (_M0L6_2acntS3783 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3785 =
      _M0L5entryS558->$5;
    moonbit_string_t _M0L8_2afieldS3784;
    moonbit_decref(_M0L8_2afieldS3785);
    _M0L8_2afieldS3784 = _M0L5entryS558->$4;
    moonbit_decref(_M0L8_2afieldS3784);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS558);
  }
  _M0L7_2abindS559 = _M0L8_2afieldS3612;
  if (_M0L7_2abindS559 == 0) {
    if (_M0L7_2abindS559) {
      moonbit_decref(_M0L7_2abindS559);
    }
    _M0L4selfS556->$6 = _M0L8new__idxS557;
    moonbit_decref(_M0L4selfS556);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS560;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS561;
    moonbit_decref(_M0L4selfS556);
    _M0L7_2aSomeS560 = _M0L7_2abindS559;
    _M0L7_2anextS561 = _M0L7_2aSomeS560;
    _M0L7_2anextS561->$0 = _M0L8new__idxS557;
    moonbit_decref(_M0L7_2anextS561);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS562,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS564,
  int32_t _M0L8new__idxS563
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3617;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1867;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1868;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3616;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3615;
  int32_t _M0L6_2acntS3787;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS565;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3617 = _M0L4selfS562->$0;
  _M0L7entriesS1867 = _M0L8_2afieldS3617;
  moonbit_incref(_M0L5entryS564);
  _M0L6_2atmpS1868 = _M0L5entryS564;
  if (
    _M0L8new__idxS563 < 0
    || _M0L8new__idxS563 >= Moonbit_array_length(_M0L7entriesS1867)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3616
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1867[
      _M0L8new__idxS563
    ];
  if (_M0L6_2aoldS3616) {
    moonbit_decref(_M0L6_2aoldS3616);
  }
  _M0L7entriesS1867[_M0L8new__idxS563] = _M0L6_2atmpS1868;
  _M0L8_2afieldS3615 = _M0L5entryS564->$1;
  _M0L6_2acntS3787 = Moonbit_object_header(_M0L5entryS564)->rc;
  if (_M0L6_2acntS3787 > 1) {
    int32_t _M0L11_2anew__cntS3789 = _M0L6_2acntS3787 - 1;
    Moonbit_object_header(_M0L5entryS564)->rc = _M0L11_2anew__cntS3789;
    if (_M0L8_2afieldS3615) {
      moonbit_incref(_M0L8_2afieldS3615);
    }
  } else if (_M0L6_2acntS3787 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3788 =
      _M0L5entryS564->$5;
    moonbit_decref(_M0L8_2afieldS3788);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS564);
  }
  _M0L7_2abindS565 = _M0L8_2afieldS3615;
  if (_M0L7_2abindS565 == 0) {
    if (_M0L7_2abindS565) {
      moonbit_decref(_M0L7_2abindS565);
    }
    _M0L4selfS562->$6 = _M0L8new__idxS563;
    moonbit_decref(_M0L4selfS562);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS566;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS567;
    moonbit_decref(_M0L4selfS562);
    _M0L7_2aSomeS566 = _M0L7_2abindS565;
    _M0L7_2anextS567 = _M0L7_2aSomeS566;
    _M0L7_2anextS567->$0 = _M0L8new__idxS563;
    moonbit_decref(_M0L7_2anextS567);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS549,
  int32_t _M0L3idxS551,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS550
) {
  int32_t _M0L7_2abindS548;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3619;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1852;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1853;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3618;
  int32_t _M0L4sizeS1855;
  int32_t _M0L6_2atmpS1854;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS548 = _M0L4selfS549->$6;
  switch (_M0L7_2abindS548) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1847;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3620;
      moonbit_incref(_M0L5entryS550);
      _M0L6_2atmpS1847 = _M0L5entryS550;
      _M0L6_2aoldS3620 = _M0L4selfS549->$5;
      if (_M0L6_2aoldS3620) {
        moonbit_decref(_M0L6_2aoldS3620);
      }
      _M0L4selfS549->$5 = _M0L6_2atmpS1847;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3623 =
        _M0L4selfS549->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1851 =
        _M0L8_2afieldS3623;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3622;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1850;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1848;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1849;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3621;
      if (
        _M0L7_2abindS548 < 0
        || _M0L7_2abindS548 >= Moonbit_array_length(_M0L7entriesS1851)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3622
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1851[
          _M0L7_2abindS548
        ];
      _M0L6_2atmpS1850 = _M0L6_2atmpS3622;
      if (_M0L6_2atmpS1850) {
        moonbit_incref(_M0L6_2atmpS1850);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1848
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1850);
      moonbit_incref(_M0L5entryS550);
      _M0L6_2atmpS1849 = _M0L5entryS550;
      _M0L6_2aoldS3621 = _M0L6_2atmpS1848->$1;
      if (_M0L6_2aoldS3621) {
        moonbit_decref(_M0L6_2aoldS3621);
      }
      _M0L6_2atmpS1848->$1 = _M0L6_2atmpS1849;
      moonbit_decref(_M0L6_2atmpS1848);
      break;
    }
  }
  _M0L4selfS549->$6 = _M0L3idxS551;
  _M0L8_2afieldS3619 = _M0L4selfS549->$0;
  _M0L7entriesS1852 = _M0L8_2afieldS3619;
  _M0L6_2atmpS1853 = _M0L5entryS550;
  if (
    _M0L3idxS551 < 0
    || _M0L3idxS551 >= Moonbit_array_length(_M0L7entriesS1852)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3618
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1852[
      _M0L3idxS551
    ];
  if (_M0L6_2aoldS3618) {
    moonbit_decref(_M0L6_2aoldS3618);
  }
  _M0L7entriesS1852[_M0L3idxS551] = _M0L6_2atmpS1853;
  _M0L4sizeS1855 = _M0L4selfS549->$1;
  _M0L6_2atmpS1854 = _M0L4sizeS1855 + 1;
  _M0L4selfS549->$1 = _M0L6_2atmpS1854;
  moonbit_decref(_M0L4selfS549);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS553,
  int32_t _M0L3idxS555,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS554
) {
  int32_t _M0L7_2abindS552;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3625;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1861;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1862;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3624;
  int32_t _M0L4sizeS1864;
  int32_t _M0L6_2atmpS1863;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS552 = _M0L4selfS553->$6;
  switch (_M0L7_2abindS552) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1856;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3626;
      moonbit_incref(_M0L5entryS554);
      _M0L6_2atmpS1856 = _M0L5entryS554;
      _M0L6_2aoldS3626 = _M0L4selfS553->$5;
      if (_M0L6_2aoldS3626) {
        moonbit_decref(_M0L6_2aoldS3626);
      }
      _M0L4selfS553->$5 = _M0L6_2atmpS1856;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3629 =
        _M0L4selfS553->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1860 =
        _M0L8_2afieldS3629;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3628;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1859;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1857;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1858;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3627;
      if (
        _M0L7_2abindS552 < 0
        || _M0L7_2abindS552 >= Moonbit_array_length(_M0L7entriesS1860)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3628
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1860[
          _M0L7_2abindS552
        ];
      _M0L6_2atmpS1859 = _M0L6_2atmpS3628;
      if (_M0L6_2atmpS1859) {
        moonbit_incref(_M0L6_2atmpS1859);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1857
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1859);
      moonbit_incref(_M0L5entryS554);
      _M0L6_2atmpS1858 = _M0L5entryS554;
      _M0L6_2aoldS3627 = _M0L6_2atmpS1857->$1;
      if (_M0L6_2aoldS3627) {
        moonbit_decref(_M0L6_2aoldS3627);
      }
      _M0L6_2atmpS1857->$1 = _M0L6_2atmpS1858;
      moonbit_decref(_M0L6_2atmpS1857);
      break;
    }
  }
  _M0L4selfS553->$6 = _M0L3idxS555;
  _M0L8_2afieldS3625 = _M0L4selfS553->$0;
  _M0L7entriesS1861 = _M0L8_2afieldS3625;
  _M0L6_2atmpS1862 = _M0L5entryS554;
  if (
    _M0L3idxS555 < 0
    || _M0L3idxS555 >= Moonbit_array_length(_M0L7entriesS1861)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3624
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1861[
      _M0L3idxS555
    ];
  if (_M0L6_2aoldS3624) {
    moonbit_decref(_M0L6_2aoldS3624);
  }
  _M0L7entriesS1861[_M0L3idxS555] = _M0L6_2atmpS1862;
  _M0L4sizeS1864 = _M0L4selfS553->$1;
  _M0L6_2atmpS1863 = _M0L4sizeS1864 + 1;
  _M0L4selfS553->$1 = _M0L6_2atmpS1863;
  moonbit_decref(_M0L4selfS553);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS537
) {
  int32_t _M0L8capacityS536;
  int32_t _M0L7_2abindS538;
  int32_t _M0L7_2abindS539;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1845;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS540;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS541;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4034;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS536
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS537);
  _M0L7_2abindS538 = _M0L8capacityS536 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS539 = _M0FPB21calc__grow__threshold(_M0L8capacityS536);
  _M0L6_2atmpS1845 = 0;
  _M0L7_2abindS540
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS536, _M0L6_2atmpS1845);
  _M0L7_2abindS541 = 0;
  _block_4034
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4034)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4034->$0 = _M0L7_2abindS540;
  _block_4034->$1 = 0;
  _block_4034->$2 = _M0L8capacityS536;
  _block_4034->$3 = _M0L7_2abindS538;
  _block_4034->$4 = _M0L7_2abindS539;
  _block_4034->$5 = _M0L7_2abindS541;
  _block_4034->$6 = -1;
  return _block_4034;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS543
) {
  int32_t _M0L8capacityS542;
  int32_t _M0L7_2abindS544;
  int32_t _M0L7_2abindS545;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1846;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS546;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS547;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4035;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS542
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS543);
  _M0L7_2abindS544 = _M0L8capacityS542 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS545 = _M0FPB21calc__grow__threshold(_M0L8capacityS542);
  _M0L6_2atmpS1846 = 0;
  _M0L7_2abindS546
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS542, _M0L6_2atmpS1846);
  _M0L7_2abindS547 = 0;
  _block_4035
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4035)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4035->$0 = _M0L7_2abindS546;
  _block_4035->$1 = 0;
  _block_4035->$2 = _M0L8capacityS542;
  _block_4035->$3 = _M0L7_2abindS544;
  _block_4035->$4 = _M0L7_2abindS545;
  _block_4035->$5 = _M0L7_2abindS547;
  _block_4035->$6 = -1;
  return _block_4035;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS535) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS535 >= 0) {
    int32_t _M0L6_2atmpS1844;
    int32_t _M0L6_2atmpS1843;
    int32_t _M0L6_2atmpS1842;
    int32_t _M0L6_2atmpS1841;
    if (_M0L4selfS535 <= 1) {
      return 1;
    }
    if (_M0L4selfS535 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1844 = _M0L4selfS535 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1843 = moonbit_clz32(_M0L6_2atmpS1844);
    _M0L6_2atmpS1842 = _M0L6_2atmpS1843 - 1;
    _M0L6_2atmpS1841 = 2147483647 >> (_M0L6_2atmpS1842 & 31);
    return _M0L6_2atmpS1841 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS534) {
  int32_t _M0L6_2atmpS1840;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1840 = _M0L8capacityS534 * 13;
  return _M0L6_2atmpS1840 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS530
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS530 == 0) {
    if (_M0L4selfS530) {
      moonbit_decref(_M0L4selfS530);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS531 =
      _M0L4selfS530;
    return _M0L7_2aSomeS531;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS532
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS532 == 0) {
    if (_M0L4selfS532) {
      moonbit_decref(_M0L4selfS532);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS533 =
      _M0L4selfS532;
    return _M0L7_2aSomeS533;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS529
) {
  moonbit_string_t* _M0L6_2atmpS1839;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1839 = _M0L4selfS529;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1839);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS525,
  int32_t _M0L5indexS526
) {
  uint64_t* _M0L6_2atmpS1837;
  uint64_t _M0L6_2atmpS3630;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1837 = _M0L4selfS525;
  if (
    _M0L5indexS526 < 0
    || _M0L5indexS526 >= Moonbit_array_length(_M0L6_2atmpS1837)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3630 = (uint64_t)_M0L6_2atmpS1837[_M0L5indexS526];
  moonbit_decref(_M0L6_2atmpS1837);
  return _M0L6_2atmpS3630;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS527,
  int32_t _M0L5indexS528
) {
  uint32_t* _M0L6_2atmpS1838;
  uint32_t _M0L6_2atmpS3631;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1838 = _M0L4selfS527;
  if (
    _M0L5indexS528 < 0
    || _M0L5indexS528 >= Moonbit_array_length(_M0L6_2atmpS1838)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3631 = (uint32_t)_M0L6_2atmpS1838[_M0L5indexS528];
  moonbit_decref(_M0L6_2atmpS1838);
  return _M0L6_2atmpS3631;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS524
) {
  moonbit_string_t* _M0L6_2atmpS1835;
  int32_t _M0L6_2atmpS3632;
  int32_t _M0L6_2atmpS1836;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1834;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS524);
  _M0L6_2atmpS1835 = _M0L4selfS524;
  _M0L6_2atmpS3632 = Moonbit_array_length(_M0L4selfS524);
  moonbit_decref(_M0L4selfS524);
  _M0L6_2atmpS1836 = _M0L6_2atmpS3632;
  _M0L6_2atmpS1834
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1836, _M0L6_2atmpS1835
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1834);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS522
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS521;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__* _closure_4036;
  struct _M0TWEOs* _M0L6_2atmpS1822;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS521
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS521)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS521->$0 = 0;
  _closure_4036
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__));
  Moonbit_object_header(_closure_4036)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__, $0_0) >> 2, 2, 0);
  _closure_4036->code = &_M0MPC15array9ArrayView4iterGsEC1823l570;
  _closure_4036->$0_0 = _M0L4selfS522.$0;
  _closure_4036->$0_1 = _M0L4selfS522.$1;
  _closure_4036->$0_2 = _M0L4selfS522.$2;
  _closure_4036->$1 = _M0L1iS521;
  _M0L6_2atmpS1822 = (struct _M0TWEOs*)_closure_4036;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1822);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1823l570(
  struct _M0TWEOs* _M0L6_2aenvS1824
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__* _M0L14_2acasted__envS1825;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3637;
  struct _M0TPC13ref3RefGiE* _M0L1iS521;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3636;
  int32_t _M0L6_2acntS3790;
  struct _M0TPB9ArrayViewGsE _M0L4selfS522;
  int32_t _M0L3valS1826;
  int32_t _M0L6_2atmpS1827;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1825
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1823__l570__*)_M0L6_2aenvS1824;
  _M0L8_2afieldS3637 = _M0L14_2acasted__envS1825->$1;
  _M0L1iS521 = _M0L8_2afieldS3637;
  _M0L8_2afieldS3636
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1825->$0_1,
      _M0L14_2acasted__envS1825->$0_2,
      _M0L14_2acasted__envS1825->$0_0
  };
  _M0L6_2acntS3790 = Moonbit_object_header(_M0L14_2acasted__envS1825)->rc;
  if (_M0L6_2acntS3790 > 1) {
    int32_t _M0L11_2anew__cntS3791 = _M0L6_2acntS3790 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1825)->rc
    = _M0L11_2anew__cntS3791;
    moonbit_incref(_M0L1iS521);
    moonbit_incref(_M0L8_2afieldS3636.$0);
  } else if (_M0L6_2acntS3790 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1825);
  }
  _M0L4selfS522 = _M0L8_2afieldS3636;
  _M0L3valS1826 = _M0L1iS521->$0;
  moonbit_incref(_M0L4selfS522.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1827 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS522);
  if (_M0L3valS1826 < _M0L6_2atmpS1827) {
    moonbit_string_t* _M0L8_2afieldS3635 = _M0L4selfS522.$0;
    moonbit_string_t* _M0L3bufS1830 = _M0L8_2afieldS3635;
    int32_t _M0L8_2afieldS3634 = _M0L4selfS522.$1;
    int32_t _M0L5startS1832 = _M0L8_2afieldS3634;
    int32_t _M0L3valS1833 = _M0L1iS521->$0;
    int32_t _M0L6_2atmpS1831 = _M0L5startS1832 + _M0L3valS1833;
    moonbit_string_t _M0L6_2atmpS3633 =
      (moonbit_string_t)_M0L3bufS1830[_M0L6_2atmpS1831];
    moonbit_string_t _M0L4elemS523;
    int32_t _M0L3valS1829;
    int32_t _M0L6_2atmpS1828;
    moonbit_incref(_M0L6_2atmpS3633);
    moonbit_decref(_M0L3bufS1830);
    _M0L4elemS523 = _M0L6_2atmpS3633;
    _M0L3valS1829 = _M0L1iS521->$0;
    _M0L6_2atmpS1828 = _M0L3valS1829 + 1;
    _M0L1iS521->$0 = _M0L6_2atmpS1828;
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
  moonbit_string_t _M0L6_2atmpS1821;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1821
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS519, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS518.$0->$method_0(_M0L6loggerS518.$1, _M0L6_2atmpS1821);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS517,
  struct _M0TPB6Logger _M0L6loggerS516
) {
  moonbit_string_t _M0L6_2atmpS1820;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1820 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS517, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS516.$0->$method_0(_M0L6loggerS516.$1, _M0L6_2atmpS1820);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS514,
  struct _M0TPB6Logger _M0L6loggerS515
) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS514) {
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS515.$0->$method_0(_M0L6loggerS515.$1, (moonbit_string_t)moonbit_string_literal_208.data);
  } else {
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS515.$0->$method_0(_M0L6loggerS515.$1, (moonbit_string_t)moonbit_string_literal_209.data);
  }
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS509) {
  int32_t _M0L3lenS508;
  struct _M0TPC13ref3RefGiE* _M0L5indexS510;
  struct _M0R38String_3a_3aiter_2eanon__u1804__l247__* _closure_4037;
  struct _M0TWEOc* _M0L6_2atmpS1803;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS508 = Moonbit_array_length(_M0L4selfS509);
  _M0L5indexS510
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS510)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS510->$0 = 0;
  _closure_4037
  = (struct _M0R38String_3a_3aiter_2eanon__u1804__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1804__l247__));
  Moonbit_object_header(_closure_4037)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1804__l247__, $0) >> 2, 2, 0);
  _closure_4037->code = &_M0MPC16string6String4iterC1804l247;
  _closure_4037->$0 = _M0L5indexS510;
  _closure_4037->$1 = _M0L4selfS509;
  _closure_4037->$2 = _M0L3lenS508;
  _M0L6_2atmpS1803 = (struct _M0TWEOc*)_closure_4037;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1803);
}

int32_t _M0MPC16string6String4iterC1804l247(
  struct _M0TWEOc* _M0L6_2aenvS1805
) {
  struct _M0R38String_3a_3aiter_2eanon__u1804__l247__* _M0L14_2acasted__envS1806;
  int32_t _M0L3lenS508;
  moonbit_string_t _M0L8_2afieldS3640;
  moonbit_string_t _M0L4selfS509;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3639;
  int32_t _M0L6_2acntS3792;
  struct _M0TPC13ref3RefGiE* _M0L5indexS510;
  int32_t _M0L3valS1807;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1806
  = (struct _M0R38String_3a_3aiter_2eanon__u1804__l247__*)_M0L6_2aenvS1805;
  _M0L3lenS508 = _M0L14_2acasted__envS1806->$2;
  _M0L8_2afieldS3640 = _M0L14_2acasted__envS1806->$1;
  _M0L4selfS509 = _M0L8_2afieldS3640;
  _M0L8_2afieldS3639 = _M0L14_2acasted__envS1806->$0;
  _M0L6_2acntS3792 = Moonbit_object_header(_M0L14_2acasted__envS1806)->rc;
  if (_M0L6_2acntS3792 > 1) {
    int32_t _M0L11_2anew__cntS3793 = _M0L6_2acntS3792 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1806)->rc
    = _M0L11_2anew__cntS3793;
    moonbit_incref(_M0L4selfS509);
    moonbit_incref(_M0L8_2afieldS3639);
  } else if (_M0L6_2acntS3792 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1806);
  }
  _M0L5indexS510 = _M0L8_2afieldS3639;
  _M0L3valS1807 = _M0L5indexS510->$0;
  if (_M0L3valS1807 < _M0L3lenS508) {
    int32_t _M0L3valS1819 = _M0L5indexS510->$0;
    int32_t _M0L2c1S511 = _M0L4selfS509[_M0L3valS1819];
    int32_t _if__result_4038;
    int32_t _M0L3valS1817;
    int32_t _M0L6_2atmpS1816;
    int32_t _M0L6_2atmpS1818;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S511)) {
      int32_t _M0L3valS1809 = _M0L5indexS510->$0;
      int32_t _M0L6_2atmpS1808 = _M0L3valS1809 + 1;
      _if__result_4038 = _M0L6_2atmpS1808 < _M0L3lenS508;
    } else {
      _if__result_4038 = 0;
    }
    if (_if__result_4038) {
      int32_t _M0L3valS1815 = _M0L5indexS510->$0;
      int32_t _M0L6_2atmpS1814 = _M0L3valS1815 + 1;
      int32_t _M0L6_2atmpS3638 = _M0L4selfS509[_M0L6_2atmpS1814];
      int32_t _M0L2c2S512;
      moonbit_decref(_M0L4selfS509);
      _M0L2c2S512 = _M0L6_2atmpS3638;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S512)) {
        int32_t _M0L6_2atmpS1812 = (int32_t)_M0L2c1S511;
        int32_t _M0L6_2atmpS1813 = (int32_t)_M0L2c2S512;
        int32_t _M0L1cS513;
        int32_t _M0L3valS1811;
        int32_t _M0L6_2atmpS1810;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS513
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1812, _M0L6_2atmpS1813);
        _M0L3valS1811 = _M0L5indexS510->$0;
        _M0L6_2atmpS1810 = _M0L3valS1811 + 2;
        _M0L5indexS510->$0 = _M0L6_2atmpS1810;
        moonbit_decref(_M0L5indexS510);
        return _M0L1cS513;
      }
    } else {
      moonbit_decref(_M0L4selfS509);
    }
    _M0L3valS1817 = _M0L5indexS510->$0;
    _M0L6_2atmpS1816 = _M0L3valS1817 + 1;
    _M0L5indexS510->$0 = _M0L6_2atmpS1816;
    moonbit_decref(_M0L5indexS510);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS1818 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S511);
    return _M0L6_2atmpS1818;
  } else {
    moonbit_decref(_M0L5indexS510);
    moonbit_decref(_M0L4selfS509);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS499,
  moonbit_string_t _M0L5valueS501
) {
  int32_t _M0L3lenS1788;
  moonbit_string_t* _M0L6_2atmpS1790;
  int32_t _M0L6_2atmpS3643;
  int32_t _M0L6_2atmpS1789;
  int32_t _M0L6lengthS500;
  moonbit_string_t* _M0L8_2afieldS3642;
  moonbit_string_t* _M0L3bufS1791;
  moonbit_string_t _M0L6_2aoldS3641;
  int32_t _M0L6_2atmpS1792;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1788 = _M0L4selfS499->$1;
  moonbit_incref(_M0L4selfS499);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1790 = _M0MPC15array5Array6bufferGsE(_M0L4selfS499);
  _M0L6_2atmpS3643 = Moonbit_array_length(_M0L6_2atmpS1790);
  moonbit_decref(_M0L6_2atmpS1790);
  _M0L6_2atmpS1789 = _M0L6_2atmpS3643;
  if (_M0L3lenS1788 == _M0L6_2atmpS1789) {
    moonbit_incref(_M0L4selfS499);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS499);
  }
  _M0L6lengthS500 = _M0L4selfS499->$1;
  _M0L8_2afieldS3642 = _M0L4selfS499->$0;
  _M0L3bufS1791 = _M0L8_2afieldS3642;
  _M0L6_2aoldS3641 = (moonbit_string_t)_M0L3bufS1791[_M0L6lengthS500];
  moonbit_decref(_M0L6_2aoldS3641);
  _M0L3bufS1791[_M0L6lengthS500] = _M0L5valueS501;
  _M0L6_2atmpS1792 = _M0L6lengthS500 + 1;
  _M0L4selfS499->$1 = _M0L6_2atmpS1792;
  moonbit_decref(_M0L4selfS499);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS502,
  struct _M0TUsiE* _M0L5valueS504
) {
  int32_t _M0L3lenS1793;
  struct _M0TUsiE** _M0L6_2atmpS1795;
  int32_t _M0L6_2atmpS3646;
  int32_t _M0L6_2atmpS1794;
  int32_t _M0L6lengthS503;
  struct _M0TUsiE** _M0L8_2afieldS3645;
  struct _M0TUsiE** _M0L3bufS1796;
  struct _M0TUsiE* _M0L6_2aoldS3644;
  int32_t _M0L6_2atmpS1797;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1793 = _M0L4selfS502->$1;
  moonbit_incref(_M0L4selfS502);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1795 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS502);
  _M0L6_2atmpS3646 = Moonbit_array_length(_M0L6_2atmpS1795);
  moonbit_decref(_M0L6_2atmpS1795);
  _M0L6_2atmpS1794 = _M0L6_2atmpS3646;
  if (_M0L3lenS1793 == _M0L6_2atmpS1794) {
    moonbit_incref(_M0L4selfS502);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS502);
  }
  _M0L6lengthS503 = _M0L4selfS502->$1;
  _M0L8_2afieldS3645 = _M0L4selfS502->$0;
  _M0L3bufS1796 = _M0L8_2afieldS3645;
  _M0L6_2aoldS3644 = (struct _M0TUsiE*)_M0L3bufS1796[_M0L6lengthS503];
  if (_M0L6_2aoldS3644) {
    moonbit_decref(_M0L6_2aoldS3644);
  }
  _M0L3bufS1796[_M0L6lengthS503] = _M0L5valueS504;
  _M0L6_2atmpS1797 = _M0L6lengthS503 + 1;
  _M0L4selfS502->$1 = _M0L6_2atmpS1797;
  moonbit_decref(_M0L4selfS502);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS505,
  void* _M0L5valueS507
) {
  int32_t _M0L3lenS1798;
  void** _M0L6_2atmpS1800;
  int32_t _M0L6_2atmpS3649;
  int32_t _M0L6_2atmpS1799;
  int32_t _M0L6lengthS506;
  void** _M0L8_2afieldS3648;
  void** _M0L3bufS1801;
  void* _M0L6_2aoldS3647;
  int32_t _M0L6_2atmpS1802;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1798 = _M0L4selfS505->$1;
  moonbit_incref(_M0L4selfS505);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1800
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS505);
  _M0L6_2atmpS3649 = Moonbit_array_length(_M0L6_2atmpS1800);
  moonbit_decref(_M0L6_2atmpS1800);
  _M0L6_2atmpS1799 = _M0L6_2atmpS3649;
  if (_M0L3lenS1798 == _M0L6_2atmpS1799) {
    moonbit_incref(_M0L4selfS505);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS505);
  }
  _M0L6lengthS506 = _M0L4selfS505->$1;
  _M0L8_2afieldS3648 = _M0L4selfS505->$0;
  _M0L3bufS1801 = _M0L8_2afieldS3648;
  _M0L6_2aoldS3647 = (void*)_M0L3bufS1801[_M0L6lengthS506];
  moonbit_decref(_M0L6_2aoldS3647);
  _M0L3bufS1801[_M0L6lengthS506] = _M0L5valueS507;
  _M0L6_2atmpS1802 = _M0L6lengthS506 + 1;
  _M0L4selfS505->$1 = _M0L6_2atmpS1802;
  moonbit_decref(_M0L4selfS505);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS491) {
  int32_t _M0L8old__capS490;
  int32_t _M0L8new__capS492;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS490 = _M0L4selfS491->$1;
  if (_M0L8old__capS490 == 0) {
    _M0L8new__capS492 = 8;
  } else {
    _M0L8new__capS492 = _M0L8old__capS490 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS491, _M0L8new__capS492);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS494
) {
  int32_t _M0L8old__capS493;
  int32_t _M0L8new__capS495;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS493 = _M0L4selfS494->$1;
  if (_M0L8old__capS493 == 0) {
    _M0L8new__capS495 = 8;
  } else {
    _M0L8new__capS495 = _M0L8old__capS493 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS494, _M0L8new__capS495);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS497
) {
  int32_t _M0L8old__capS496;
  int32_t _M0L8new__capS498;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS496 = _M0L4selfS497->$1;
  if (_M0L8old__capS496 == 0) {
    _M0L8new__capS498 = 8;
  } else {
    _M0L8new__capS498 = _M0L8old__capS496 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS497, _M0L8new__capS498);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS475,
  int32_t _M0L13new__capacityS473
) {
  moonbit_string_t* _M0L8new__bufS472;
  moonbit_string_t* _M0L8_2afieldS3651;
  moonbit_string_t* _M0L8old__bufS474;
  int32_t _M0L8old__capS476;
  int32_t _M0L9copy__lenS477;
  moonbit_string_t* _M0L6_2aoldS3650;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS472
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS473, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3651 = _M0L4selfS475->$0;
  _M0L8old__bufS474 = _M0L8_2afieldS3651;
  _M0L8old__capS476 = Moonbit_array_length(_M0L8old__bufS474);
  if (_M0L8old__capS476 < _M0L13new__capacityS473) {
    _M0L9copy__lenS477 = _M0L8old__capS476;
  } else {
    _M0L9copy__lenS477 = _M0L13new__capacityS473;
  }
  moonbit_incref(_M0L8old__bufS474);
  moonbit_incref(_M0L8new__bufS472);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS472, 0, _M0L8old__bufS474, 0, _M0L9copy__lenS477);
  _M0L6_2aoldS3650 = _M0L4selfS475->$0;
  moonbit_decref(_M0L6_2aoldS3650);
  _M0L4selfS475->$0 = _M0L8new__bufS472;
  moonbit_decref(_M0L4selfS475);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS481,
  int32_t _M0L13new__capacityS479
) {
  struct _M0TUsiE** _M0L8new__bufS478;
  struct _M0TUsiE** _M0L8_2afieldS3653;
  struct _M0TUsiE** _M0L8old__bufS480;
  int32_t _M0L8old__capS482;
  int32_t _M0L9copy__lenS483;
  struct _M0TUsiE** _M0L6_2aoldS3652;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS478
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS479, 0);
  _M0L8_2afieldS3653 = _M0L4selfS481->$0;
  _M0L8old__bufS480 = _M0L8_2afieldS3653;
  _M0L8old__capS482 = Moonbit_array_length(_M0L8old__bufS480);
  if (_M0L8old__capS482 < _M0L13new__capacityS479) {
    _M0L9copy__lenS483 = _M0L8old__capS482;
  } else {
    _M0L9copy__lenS483 = _M0L13new__capacityS479;
  }
  moonbit_incref(_M0L8old__bufS480);
  moonbit_incref(_M0L8new__bufS478);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS478, 0, _M0L8old__bufS480, 0, _M0L9copy__lenS483);
  _M0L6_2aoldS3652 = _M0L4selfS481->$0;
  moonbit_decref(_M0L6_2aoldS3652);
  _M0L4selfS481->$0 = _M0L8new__bufS478;
  moonbit_decref(_M0L4selfS481);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS487,
  int32_t _M0L13new__capacityS485
) {
  void** _M0L8new__bufS484;
  void** _M0L8_2afieldS3655;
  void** _M0L8old__bufS486;
  int32_t _M0L8old__capS488;
  int32_t _M0L9copy__lenS489;
  void** _M0L6_2aoldS3654;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS484
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS485, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3655 = _M0L4selfS487->$0;
  _M0L8old__bufS486 = _M0L8_2afieldS3655;
  _M0L8old__capS488 = Moonbit_array_length(_M0L8old__bufS486);
  if (_M0L8old__capS488 < _M0L13new__capacityS485) {
    _M0L9copy__lenS489 = _M0L8old__capS488;
  } else {
    _M0L9copy__lenS489 = _M0L13new__capacityS485;
  }
  moonbit_incref(_M0L8old__bufS486);
  moonbit_incref(_M0L8new__bufS484);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS484, 0, _M0L8old__bufS486, 0, _M0L9copy__lenS489);
  _M0L6_2aoldS3654 = _M0L4selfS487->$0;
  moonbit_decref(_M0L6_2aoldS3654);
  _M0L4selfS487->$0 = _M0L8new__bufS484;
  moonbit_decref(_M0L4selfS487);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS471
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS471 == 0) {
    moonbit_string_t* _M0L6_2atmpS1786 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4039 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4039)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4039->$0 = _M0L6_2atmpS1786;
    _block_4039->$1 = 0;
    return _block_4039;
  } else {
    moonbit_string_t* _M0L6_2atmpS1787 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS471, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4040 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4040)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4040->$0 = _M0L6_2atmpS1787;
    _block_4040->$1 = 0;
    return _block_4040;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS465,
  int32_t _M0L1nS464
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS464 <= 0) {
    moonbit_decref(_M0L4selfS465);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS464 == 1) {
    return _M0L4selfS465;
  } else {
    int32_t _M0L3lenS466 = Moonbit_array_length(_M0L4selfS465);
    int32_t _M0L6_2atmpS1785 = _M0L3lenS466 * _M0L1nS464;
    struct _M0TPB13StringBuilder* _M0L3bufS467;
    moonbit_string_t _M0L3strS468;
    int32_t _M0L2__S469;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS467 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1785);
    _M0L3strS468 = _M0L4selfS465;
    _M0L2__S469 = 0;
    while (1) {
      if (_M0L2__S469 < _M0L1nS464) {
        int32_t _M0L6_2atmpS1784;
        moonbit_incref(_M0L3strS468);
        moonbit_incref(_M0L3bufS467);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS467, _M0L3strS468);
        _M0L6_2atmpS1784 = _M0L2__S469 + 1;
        _M0L2__S469 = _M0L6_2atmpS1784;
        continue;
      } else {
        moonbit_decref(_M0L3strS468);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS467);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS462,
  struct _M0TPC16string10StringView _M0L3strS463
) {
  int32_t _M0L3lenS1772;
  int32_t _M0L6_2atmpS1774;
  int32_t _M0L6_2atmpS1773;
  int32_t _M0L6_2atmpS1771;
  moonbit_bytes_t _M0L8_2afieldS3656;
  moonbit_bytes_t _M0L4dataS1775;
  int32_t _M0L3lenS1776;
  moonbit_string_t _M0L6_2atmpS1777;
  int32_t _M0L6_2atmpS1778;
  int32_t _M0L6_2atmpS1779;
  int32_t _M0L3lenS1781;
  int32_t _M0L6_2atmpS1783;
  int32_t _M0L6_2atmpS1782;
  int32_t _M0L6_2atmpS1780;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1772 = _M0L4selfS462->$1;
  moonbit_incref(_M0L3strS463.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1774 = _M0MPC16string10StringView6length(_M0L3strS463);
  _M0L6_2atmpS1773 = _M0L6_2atmpS1774 * 2;
  _M0L6_2atmpS1771 = _M0L3lenS1772 + _M0L6_2atmpS1773;
  moonbit_incref(_M0L4selfS462);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS462, _M0L6_2atmpS1771);
  _M0L8_2afieldS3656 = _M0L4selfS462->$0;
  _M0L4dataS1775 = _M0L8_2afieldS3656;
  _M0L3lenS1776 = _M0L4selfS462->$1;
  moonbit_incref(_M0L4dataS1775);
  moonbit_incref(_M0L3strS463.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1777 = _M0MPC16string10StringView4data(_M0L3strS463);
  moonbit_incref(_M0L3strS463.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1778 = _M0MPC16string10StringView13start__offset(_M0L3strS463);
  moonbit_incref(_M0L3strS463.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1779 = _M0MPC16string10StringView6length(_M0L3strS463);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1775, _M0L3lenS1776, _M0L6_2atmpS1777, _M0L6_2atmpS1778, _M0L6_2atmpS1779);
  _M0L3lenS1781 = _M0L4selfS462->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1783 = _M0MPC16string10StringView6length(_M0L3strS463);
  _M0L6_2atmpS1782 = _M0L6_2atmpS1783 * 2;
  _M0L6_2atmpS1780 = _M0L3lenS1781 + _M0L6_2atmpS1782;
  _M0L4selfS462->$1 = _M0L6_2atmpS1780;
  moonbit_decref(_M0L4selfS462);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS454,
  int32_t _M0L3lenS457,
  int32_t _M0L13start__offsetS461,
  int64_t _M0L11end__offsetS452
) {
  int32_t _M0L11end__offsetS451;
  int32_t _M0L5indexS455;
  int32_t _M0L5countS456;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS452 == 4294967296ll) {
    _M0L11end__offsetS451 = Moonbit_array_length(_M0L4selfS454);
  } else {
    int64_t _M0L7_2aSomeS453 = _M0L11end__offsetS452;
    _M0L11end__offsetS451 = (int32_t)_M0L7_2aSomeS453;
  }
  _M0L5indexS455 = _M0L13start__offsetS461;
  _M0L5countS456 = 0;
  while (1) {
    int32_t _if__result_4043;
    if (_M0L5indexS455 < _M0L11end__offsetS451) {
      _if__result_4043 = _M0L5countS456 < _M0L3lenS457;
    } else {
      _if__result_4043 = 0;
    }
    if (_if__result_4043) {
      int32_t _M0L2c1S458 = _M0L4selfS454[_M0L5indexS455];
      int32_t _if__result_4044;
      int32_t _M0L6_2atmpS1769;
      int32_t _M0L6_2atmpS1770;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S458)) {
        int32_t _M0L6_2atmpS1765 = _M0L5indexS455 + 1;
        _if__result_4044 = _M0L6_2atmpS1765 < _M0L11end__offsetS451;
      } else {
        _if__result_4044 = 0;
      }
      if (_if__result_4044) {
        int32_t _M0L6_2atmpS1768 = _M0L5indexS455 + 1;
        int32_t _M0L2c2S459 = _M0L4selfS454[_M0L6_2atmpS1768];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S459)) {
          int32_t _M0L6_2atmpS1766 = _M0L5indexS455 + 2;
          int32_t _M0L6_2atmpS1767 = _M0L5countS456 + 1;
          _M0L5indexS455 = _M0L6_2atmpS1766;
          _M0L5countS456 = _M0L6_2atmpS1767;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_239.data, (moonbit_string_t)moonbit_string_literal_240.data);
        }
      }
      _M0L6_2atmpS1769 = _M0L5indexS455 + 1;
      _M0L6_2atmpS1770 = _M0L5countS456 + 1;
      _M0L5indexS455 = _M0L6_2atmpS1769;
      _M0L5countS456 = _M0L6_2atmpS1770;
      continue;
    } else {
      moonbit_decref(_M0L4selfS454);
      return _M0L5countS456 >= _M0L3lenS457;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS448
) {
  int32_t _M0L3endS1759;
  int32_t _M0L8_2afieldS3657;
  int32_t _M0L5startS1760;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1759 = _M0L4selfS448.$2;
  _M0L8_2afieldS3657 = _M0L4selfS448.$1;
  moonbit_decref(_M0L4selfS448.$0);
  _M0L5startS1760 = _M0L8_2afieldS3657;
  return _M0L3endS1759 - _M0L5startS1760;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS449
) {
  int32_t _M0L3endS1761;
  int32_t _M0L8_2afieldS3658;
  int32_t _M0L5startS1762;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1761 = _M0L4selfS449.$2;
  _M0L8_2afieldS3658 = _M0L4selfS449.$1;
  moonbit_decref(_M0L4selfS449.$0);
  _M0L5startS1762 = _M0L8_2afieldS3658;
  return _M0L3endS1761 - _M0L5startS1762;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS450
) {
  int32_t _M0L3endS1763;
  int32_t _M0L8_2afieldS3659;
  int32_t _M0L5startS1764;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1763 = _M0L4selfS450.$2;
  _M0L8_2afieldS3659 = _M0L4selfS450.$1;
  moonbit_decref(_M0L4selfS450.$0);
  _M0L5startS1764 = _M0L8_2afieldS3659;
  return _M0L3endS1763 - _M0L5startS1764;
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
  int32_t _if__result_4045;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS439 == 4294967296ll) {
    _M0L11end__offsetS438 = Moonbit_array_length(_M0L4selfS441);
  } else {
    int64_t _M0L7_2aSomeS440 = _M0L11end__offsetS439;
    _M0L11end__offsetS438 = (int32_t)_M0L7_2aSomeS440;
  }
  if (_M0L13start__offsetS442 >= 0) {
    if (_M0L13start__offsetS442 <= _M0L11end__offsetS438) {
      int32_t _M0L6_2atmpS1758 = Moonbit_array_length(_M0L4selfS441);
      _if__result_4045 = _M0L11end__offsetS438 <= _M0L6_2atmpS1758;
    } else {
      _if__result_4045 = 0;
    }
  } else {
    _if__result_4045 = 0;
  }
  if (_if__result_4045) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS442,
                                                 _M0L11end__offsetS438,
                                                 _M0L4selfS441};
  } else {
    moonbit_decref(_M0L4selfS441);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_241.data, (moonbit_string_t)moonbit_string_literal_242.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS437
) {
  moonbit_string_t _M0L8_2afieldS3661;
  moonbit_string_t _M0L3strS1755;
  int32_t _M0L5startS1756;
  int32_t _M0L8_2afieldS3660;
  int32_t _M0L3endS1757;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3661 = _M0L4selfS437.$0;
  _M0L3strS1755 = _M0L8_2afieldS3661;
  _M0L5startS1756 = _M0L4selfS437.$1;
  _M0L8_2afieldS3660 = _M0L4selfS437.$2;
  _M0L3endS1757 = _M0L8_2afieldS3660;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1755, _M0L5startS1756, _M0L3endS1757);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS435,
  struct _M0TPB6Logger _M0L6loggerS436
) {
  moonbit_string_t _M0L8_2afieldS3663;
  moonbit_string_t _M0L3strS1752;
  int32_t _M0L5startS1753;
  int32_t _M0L8_2afieldS3662;
  int32_t _M0L3endS1754;
  moonbit_string_t _M0L6substrS434;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3663 = _M0L4selfS435.$0;
  _M0L3strS1752 = _M0L8_2afieldS3663;
  _M0L5startS1753 = _M0L4selfS435.$1;
  _M0L8_2afieldS3662 = _M0L4selfS435.$2;
  _M0L3endS1754 = _M0L8_2afieldS3662;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS434
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1752, _M0L5startS1753, _M0L3endS1754);
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
    int32_t _M0L6_2atmpS1736;
    int32_t _M0L6_2atmpS1737;
    int32_t _M0L6_2atmpS1738;
    int32_t _tmp_4049;
    int32_t _tmp_4050;
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
        int32_t _M0L6_2atmpS1739;
        int32_t _M0L6_2atmpS1740;
        moonbit_incref(_M0L6_2aenvS425);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_224.data);
        _M0L6_2atmpS1739 = _M0L1iS428 + 1;
        _M0L6_2atmpS1740 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1739;
        _M0L3segS429 = _M0L6_2atmpS1740;
        goto _2afor_430;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1741;
        int32_t _M0L6_2atmpS1742;
        moonbit_incref(_M0L6_2aenvS425);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_225.data);
        _M0L6_2atmpS1741 = _M0L1iS428 + 1;
        _M0L6_2atmpS1742 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1741;
        _M0L3segS429 = _M0L6_2atmpS1742;
        goto _2afor_430;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1743;
        int32_t _M0L6_2atmpS1744;
        moonbit_incref(_M0L6_2aenvS425);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_226.data);
        _M0L6_2atmpS1743 = _M0L1iS428 + 1;
        _M0L6_2atmpS1744 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1743;
        _M0L3segS429 = _M0L6_2atmpS1744;
        goto _2afor_430;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1745;
        int32_t _M0L6_2atmpS1746;
        moonbit_incref(_M0L6_2aenvS425);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
        if (_M0L6loggerS424.$1) {
          moonbit_incref(_M0L6loggerS424.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_227.data);
        _M0L6_2atmpS1745 = _M0L1iS428 + 1;
        _M0L6_2atmpS1746 = _M0L1iS428 + 1;
        _M0L1iS428 = _M0L6_2atmpS1745;
        _M0L3segS429 = _M0L6_2atmpS1746;
        goto _2afor_430;
        break;
      }
      default: {
        if (_M0L4codeS431 < 32) {
          int32_t _M0L6_2atmpS1748;
          moonbit_string_t _M0L6_2atmpS1747;
          int32_t _M0L6_2atmpS1749;
          int32_t _M0L6_2atmpS1750;
          moonbit_incref(_M0L6_2aenvS425);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS425, _M0L3segS429, _M0L1iS428);
          if (_M0L6loggerS424.$1) {
            moonbit_incref(_M0L6loggerS424.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, (moonbit_string_t)moonbit_string_literal_243.data);
          _M0L6_2atmpS1748 = _M0L4codeS431 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1747 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1748);
          if (_M0L6loggerS424.$1) {
            moonbit_incref(_M0L6loggerS424.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS424.$0->$method_0(_M0L6loggerS424.$1, _M0L6_2atmpS1747);
          if (_M0L6loggerS424.$1) {
            moonbit_incref(_M0L6loggerS424.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, 125);
          _M0L6_2atmpS1749 = _M0L1iS428 + 1;
          _M0L6_2atmpS1750 = _M0L1iS428 + 1;
          _M0L1iS428 = _M0L6_2atmpS1749;
          _M0L3segS429 = _M0L6_2atmpS1750;
          goto _2afor_430;
        } else {
          int32_t _M0L6_2atmpS1751 = _M0L1iS428 + 1;
          int32_t _tmp_4048 = _M0L3segS429;
          _M0L1iS428 = _M0L6_2atmpS1751;
          _M0L3segS429 = _tmp_4048;
          goto _2afor_430;
        }
        break;
      }
    }
    goto joinlet_4047;
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
    _M0L6_2atmpS1736 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS433);
    if (_M0L6loggerS424.$1) {
      moonbit_incref(_M0L6loggerS424.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS424.$0->$method_3(_M0L6loggerS424.$1, _M0L6_2atmpS1736);
    _M0L6_2atmpS1737 = _M0L1iS428 + 1;
    _M0L6_2atmpS1738 = _M0L1iS428 + 1;
    _M0L1iS428 = _M0L6_2atmpS1737;
    _M0L3segS429 = _M0L6_2atmpS1738;
    continue;
    joinlet_4047:;
    _tmp_4049 = _M0L1iS428;
    _tmp_4050 = _M0L3segS429;
    _M0L1iS428 = _tmp_4049;
    _M0L3segS429 = _tmp_4050;
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
  struct _M0TPB6Logger _M0L8_2afieldS3665;
  struct _M0TPB6Logger _M0L6loggerS419;
  moonbit_string_t _M0L8_2afieldS3664;
  int32_t _M0L6_2acntS3794;
  moonbit_string_t _M0L4selfS421;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3665
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS420->$1_0, _M0L6_2aenvS420->$1_1
  };
  _M0L6loggerS419 = _M0L8_2afieldS3665;
  _M0L8_2afieldS3664 = _M0L6_2aenvS420->$0;
  _M0L6_2acntS3794 = Moonbit_object_header(_M0L6_2aenvS420)->rc;
  if (_M0L6_2acntS3794 > 1) {
    int32_t _M0L11_2anew__cntS3795 = _M0L6_2acntS3794 - 1;
    Moonbit_object_header(_M0L6_2aenvS420)->rc = _M0L11_2anew__cntS3795;
    if (_M0L6loggerS419.$1) {
      moonbit_incref(_M0L6loggerS419.$1);
    }
    moonbit_incref(_M0L8_2afieldS3664);
  } else if (_M0L6_2acntS3794 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS420);
  }
  _M0L4selfS421 = _M0L8_2afieldS3664;
  if (_M0L1iS422 > _M0L3segS423) {
    int32_t _M0L6_2atmpS1735 = _M0L1iS422 - _M0L3segS423;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS419.$0->$method_1(_M0L6loggerS419.$1, _M0L4selfS421, _M0L3segS423, _M0L6_2atmpS1735);
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
  int32_t _M0L6_2atmpS1732;
  int32_t _M0L6_2atmpS1731;
  int32_t _M0L6_2atmpS1734;
  int32_t _M0L6_2atmpS1733;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1730;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS417 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1732 = _M0IPC14byte4BytePB3Div3div(_M0L1bS418, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1731
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1732);
  moonbit_incref(_M0L7_2aselfS417);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS417, _M0L6_2atmpS1731);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1734 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS418, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1733
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1734);
  moonbit_incref(_M0L7_2aselfS417);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS417, _M0L6_2atmpS1733);
  _M0L6_2atmpS1730 = _M0L7_2aselfS417;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1730);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS416) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS416 < 10) {
    int32_t _M0L6_2atmpS1727;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1727 = _M0IPC14byte4BytePB3Add3add(_M0L1iS416, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1727);
  } else {
    int32_t _M0L6_2atmpS1729;
    int32_t _M0L6_2atmpS1728;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1729 = _M0IPC14byte4BytePB3Add3add(_M0L1iS416, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1728 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1729, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1728);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS414,
  int32_t _M0L4thatS415
) {
  int32_t _M0L6_2atmpS1725;
  int32_t _M0L6_2atmpS1726;
  int32_t _M0L6_2atmpS1724;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1725 = (int32_t)_M0L4selfS414;
  _M0L6_2atmpS1726 = (int32_t)_M0L4thatS415;
  _M0L6_2atmpS1724 = _M0L6_2atmpS1725 - _M0L6_2atmpS1726;
  return _M0L6_2atmpS1724 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS412,
  int32_t _M0L4thatS413
) {
  int32_t _M0L6_2atmpS1722;
  int32_t _M0L6_2atmpS1723;
  int32_t _M0L6_2atmpS1721;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1722 = (int32_t)_M0L4selfS412;
  _M0L6_2atmpS1723 = (int32_t)_M0L4thatS413;
  _M0L6_2atmpS1721 = _M0L6_2atmpS1722 % _M0L6_2atmpS1723;
  return _M0L6_2atmpS1721 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS410,
  int32_t _M0L4thatS411
) {
  int32_t _M0L6_2atmpS1719;
  int32_t _M0L6_2atmpS1720;
  int32_t _M0L6_2atmpS1718;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1719 = (int32_t)_M0L4selfS410;
  _M0L6_2atmpS1720 = (int32_t)_M0L4thatS411;
  _M0L6_2atmpS1718 = _M0L6_2atmpS1719 / _M0L6_2atmpS1720;
  return _M0L6_2atmpS1718 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS408,
  int32_t _M0L4thatS409
) {
  int32_t _M0L6_2atmpS1716;
  int32_t _M0L6_2atmpS1717;
  int32_t _M0L6_2atmpS1715;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1716 = (int32_t)_M0L4selfS408;
  _M0L6_2atmpS1717 = (int32_t)_M0L4thatS409;
  _M0L6_2atmpS1715 = _M0L6_2atmpS1716 + _M0L6_2atmpS1717;
  return _M0L6_2atmpS1715 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS405,
  int32_t _M0L5startS403,
  int32_t _M0L3endS404
) {
  int32_t _if__result_4051;
  int32_t _M0L3lenS406;
  int32_t _M0L6_2atmpS1713;
  int32_t _M0L6_2atmpS1714;
  moonbit_bytes_t _M0L5bytesS407;
  moonbit_bytes_t _M0L6_2atmpS1712;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS403 == 0) {
    int32_t _M0L6_2atmpS1711 = Moonbit_array_length(_M0L3strS405);
    _if__result_4051 = _M0L3endS404 == _M0L6_2atmpS1711;
  } else {
    _if__result_4051 = 0;
  }
  if (_if__result_4051) {
    return _M0L3strS405;
  }
  _M0L3lenS406 = _M0L3endS404 - _M0L5startS403;
  _M0L6_2atmpS1713 = _M0L3lenS406 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1714 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS407
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1713, _M0L6_2atmpS1714);
  moonbit_incref(_M0L5bytesS407);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS407, 0, _M0L3strS405, _M0L5startS403, _M0L3lenS406);
  _M0L6_2atmpS1712 = _M0L5bytesS407;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1712, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS400) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS400;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS401
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS401;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS402) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS402;
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS399,
  moonbit_string_t _M0L3locS398
) {
  moonbit_string_t _M0L6_2atmpS1710;
  moonbit_string_t _M0L6_2atmpS3667;
  moonbit_string_t _M0L6_2atmpS1708;
  moonbit_string_t _M0L6_2atmpS1709;
  moonbit_string_t _M0L6_2atmpS3666;
  moonbit_string_t _M0L6_2atmpS1707;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1706;
  struct moonbit_result_0 _result_4052;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1710
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS398);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS3667
  = moonbit_add_string(_M0L6_2atmpS1710, (moonbit_string_t)moonbit_string_literal_244.data);
  moonbit_decref(_M0L6_2atmpS1710);
  _M0L6_2atmpS1708 = _M0L6_2atmpS3667;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1709 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS399);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS3666 = moonbit_add_string(_M0L6_2atmpS1708, _M0L6_2atmpS1709);
  moonbit_decref(_M0L6_2atmpS1708);
  moonbit_decref(_M0L6_2atmpS1709);
  _M0L6_2atmpS1707 = _M0L6_2atmpS3666;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1706
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1706)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1706)->$0
  = _M0L6_2atmpS1707;
  _result_4052.tag = 0;
  _result_4052.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1706;
  return _result_4052;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS390,
  int32_t _M0L5radixS389
) {
  int32_t _if__result_4053;
  uint16_t* _M0L6bufferS391;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS389 < 2) {
    _if__result_4053 = 1;
  } else {
    _if__result_4053 = _M0L5radixS389 > 36;
  }
  if (_if__result_4053) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_245.data, (moonbit_string_t)moonbit_string_literal_246.data);
  }
  if (_M0L4selfS390 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_230.data;
  }
  switch (_M0L5radixS389) {
    case 10: {
      int32_t _M0L3lenS392;
      uint16_t* _M0L6bufferS393;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS392 = _M0FPB12dec__count64(_M0L4selfS390);
      _M0L6bufferS393 = (uint16_t*)moonbit_make_string(_M0L3lenS392, 0);
      moonbit_incref(_M0L6bufferS393);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS393, _M0L4selfS390, 0, _M0L3lenS392);
      _M0L6bufferS391 = _M0L6bufferS393;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS394;
      uint16_t* _M0L6bufferS395;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS394 = _M0FPB12hex__count64(_M0L4selfS390);
      _M0L6bufferS395 = (uint16_t*)moonbit_make_string(_M0L3lenS394, 0);
      moonbit_incref(_M0L6bufferS395);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS395, _M0L4selfS390, 0, _M0L3lenS394);
      _M0L6bufferS391 = _M0L6bufferS395;
      break;
    }
    default: {
      int32_t _M0L3lenS396;
      uint16_t* _M0L6bufferS397;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS396 = _M0FPB14radix__count64(_M0L4selfS390, _M0L5radixS389);
      _M0L6bufferS397 = (uint16_t*)moonbit_make_string(_M0L3lenS396, 0);
      moonbit_incref(_M0L6bufferS397);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS397, _M0L4selfS390, 0, _M0L3lenS396, _M0L5radixS389);
      _M0L6bufferS391 = _M0L6bufferS397;
      break;
    }
  }
  return _M0L6bufferS391;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS379,
  uint64_t _M0L3numS367,
  int32_t _M0L12digit__startS370,
  int32_t _M0L10total__lenS369
) {
  uint64_t _M0Lm3numS366;
  int32_t _M0Lm6offsetS368;
  uint64_t _M0L6_2atmpS1705;
  int32_t _M0Lm9remainingS381;
  int32_t _M0L6_2atmpS1686;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS366 = _M0L3numS367;
  _M0Lm6offsetS368 = _M0L10total__lenS369 - _M0L12digit__startS370;
  while (1) {
    uint64_t _M0L6_2atmpS1649 = _M0Lm3numS366;
    if (_M0L6_2atmpS1649 >= 10000ull) {
      uint64_t _M0L6_2atmpS1672 = _M0Lm3numS366;
      uint64_t _M0L1tS371 = _M0L6_2atmpS1672 / 10000ull;
      uint64_t _M0L6_2atmpS1671 = _M0Lm3numS366;
      uint64_t _M0L6_2atmpS1670 = _M0L6_2atmpS1671 % 10000ull;
      int32_t _M0L1rS372 = (int32_t)_M0L6_2atmpS1670;
      int32_t _M0L2d1S373;
      int32_t _M0L2d2S374;
      int32_t _M0L6_2atmpS1650;
      int32_t _M0L6_2atmpS1669;
      int32_t _M0L6_2atmpS1668;
      int32_t _M0L6d1__hiS375;
      int32_t _M0L6_2atmpS1667;
      int32_t _M0L6_2atmpS1666;
      int32_t _M0L6d1__loS376;
      int32_t _M0L6_2atmpS1665;
      int32_t _M0L6_2atmpS1664;
      int32_t _M0L6d2__hiS377;
      int32_t _M0L6_2atmpS1663;
      int32_t _M0L6_2atmpS1662;
      int32_t _M0L6d2__loS378;
      int32_t _M0L6_2atmpS1652;
      int32_t _M0L6_2atmpS1651;
      int32_t _M0L6_2atmpS1655;
      int32_t _M0L6_2atmpS1654;
      int32_t _M0L6_2atmpS1653;
      int32_t _M0L6_2atmpS1658;
      int32_t _M0L6_2atmpS1657;
      int32_t _M0L6_2atmpS1656;
      int32_t _M0L6_2atmpS1661;
      int32_t _M0L6_2atmpS1660;
      int32_t _M0L6_2atmpS1659;
      _M0Lm3numS366 = _M0L1tS371;
      _M0L2d1S373 = _M0L1rS372 / 100;
      _M0L2d2S374 = _M0L1rS372 % 100;
      _M0L6_2atmpS1650 = _M0Lm6offsetS368;
      _M0Lm6offsetS368 = _M0L6_2atmpS1650 - 4;
      _M0L6_2atmpS1669 = _M0L2d1S373 / 10;
      _M0L6_2atmpS1668 = 48 + _M0L6_2atmpS1669;
      _M0L6d1__hiS375 = (uint16_t)_M0L6_2atmpS1668;
      _M0L6_2atmpS1667 = _M0L2d1S373 % 10;
      _M0L6_2atmpS1666 = 48 + _M0L6_2atmpS1667;
      _M0L6d1__loS376 = (uint16_t)_M0L6_2atmpS1666;
      _M0L6_2atmpS1665 = _M0L2d2S374 / 10;
      _M0L6_2atmpS1664 = 48 + _M0L6_2atmpS1665;
      _M0L6d2__hiS377 = (uint16_t)_M0L6_2atmpS1664;
      _M0L6_2atmpS1663 = _M0L2d2S374 % 10;
      _M0L6_2atmpS1662 = 48 + _M0L6_2atmpS1663;
      _M0L6d2__loS378 = (uint16_t)_M0L6_2atmpS1662;
      _M0L6_2atmpS1652 = _M0Lm6offsetS368;
      _M0L6_2atmpS1651 = _M0L12digit__startS370 + _M0L6_2atmpS1652;
      _M0L6bufferS379[_M0L6_2atmpS1651] = _M0L6d1__hiS375;
      _M0L6_2atmpS1655 = _M0Lm6offsetS368;
      _M0L6_2atmpS1654 = _M0L12digit__startS370 + _M0L6_2atmpS1655;
      _M0L6_2atmpS1653 = _M0L6_2atmpS1654 + 1;
      _M0L6bufferS379[_M0L6_2atmpS1653] = _M0L6d1__loS376;
      _M0L6_2atmpS1658 = _M0Lm6offsetS368;
      _M0L6_2atmpS1657 = _M0L12digit__startS370 + _M0L6_2atmpS1658;
      _M0L6_2atmpS1656 = _M0L6_2atmpS1657 + 2;
      _M0L6bufferS379[_M0L6_2atmpS1656] = _M0L6d2__hiS377;
      _M0L6_2atmpS1661 = _M0Lm6offsetS368;
      _M0L6_2atmpS1660 = _M0L12digit__startS370 + _M0L6_2atmpS1661;
      _M0L6_2atmpS1659 = _M0L6_2atmpS1660 + 3;
      _M0L6bufferS379[_M0L6_2atmpS1659] = _M0L6d2__loS378;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1705 = _M0Lm3numS366;
  _M0Lm9remainingS381 = (int32_t)_M0L6_2atmpS1705;
  while (1) {
    int32_t _M0L6_2atmpS1673 = _M0Lm9remainingS381;
    if (_M0L6_2atmpS1673 >= 100) {
      int32_t _M0L6_2atmpS1685 = _M0Lm9remainingS381;
      int32_t _M0L1tS382 = _M0L6_2atmpS1685 / 100;
      int32_t _M0L6_2atmpS1684 = _M0Lm9remainingS381;
      int32_t _M0L1dS383 = _M0L6_2atmpS1684 % 100;
      int32_t _M0L6_2atmpS1674;
      int32_t _M0L6_2atmpS1683;
      int32_t _M0L6_2atmpS1682;
      int32_t _M0L5d__hiS384;
      int32_t _M0L6_2atmpS1681;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L5d__loS385;
      int32_t _M0L6_2atmpS1676;
      int32_t _M0L6_2atmpS1675;
      int32_t _M0L6_2atmpS1679;
      int32_t _M0L6_2atmpS1678;
      int32_t _M0L6_2atmpS1677;
      _M0Lm9remainingS381 = _M0L1tS382;
      _M0L6_2atmpS1674 = _M0Lm6offsetS368;
      _M0Lm6offsetS368 = _M0L6_2atmpS1674 - 2;
      _M0L6_2atmpS1683 = _M0L1dS383 / 10;
      _M0L6_2atmpS1682 = 48 + _M0L6_2atmpS1683;
      _M0L5d__hiS384 = (uint16_t)_M0L6_2atmpS1682;
      _M0L6_2atmpS1681 = _M0L1dS383 % 10;
      _M0L6_2atmpS1680 = 48 + _M0L6_2atmpS1681;
      _M0L5d__loS385 = (uint16_t)_M0L6_2atmpS1680;
      _M0L6_2atmpS1676 = _M0Lm6offsetS368;
      _M0L6_2atmpS1675 = _M0L12digit__startS370 + _M0L6_2atmpS1676;
      _M0L6bufferS379[_M0L6_2atmpS1675] = _M0L5d__hiS384;
      _M0L6_2atmpS1679 = _M0Lm6offsetS368;
      _M0L6_2atmpS1678 = _M0L12digit__startS370 + _M0L6_2atmpS1679;
      _M0L6_2atmpS1677 = _M0L6_2atmpS1678 + 1;
      _M0L6bufferS379[_M0L6_2atmpS1677] = _M0L5d__loS385;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1686 = _M0Lm9remainingS381;
  if (_M0L6_2atmpS1686 >= 10) {
    int32_t _M0L6_2atmpS1687 = _M0Lm6offsetS368;
    int32_t _M0L6_2atmpS1698;
    int32_t _M0L6_2atmpS1697;
    int32_t _M0L6_2atmpS1696;
    int32_t _M0L5d__hiS387;
    int32_t _M0L6_2atmpS1695;
    int32_t _M0L6_2atmpS1694;
    int32_t _M0L6_2atmpS1693;
    int32_t _M0L5d__loS388;
    int32_t _M0L6_2atmpS1689;
    int32_t _M0L6_2atmpS1688;
    int32_t _M0L6_2atmpS1692;
    int32_t _M0L6_2atmpS1691;
    int32_t _M0L6_2atmpS1690;
    _M0Lm6offsetS368 = _M0L6_2atmpS1687 - 2;
    _M0L6_2atmpS1698 = _M0Lm9remainingS381;
    _M0L6_2atmpS1697 = _M0L6_2atmpS1698 / 10;
    _M0L6_2atmpS1696 = 48 + _M0L6_2atmpS1697;
    _M0L5d__hiS387 = (uint16_t)_M0L6_2atmpS1696;
    _M0L6_2atmpS1695 = _M0Lm9remainingS381;
    _M0L6_2atmpS1694 = _M0L6_2atmpS1695 % 10;
    _M0L6_2atmpS1693 = 48 + _M0L6_2atmpS1694;
    _M0L5d__loS388 = (uint16_t)_M0L6_2atmpS1693;
    _M0L6_2atmpS1689 = _M0Lm6offsetS368;
    _M0L6_2atmpS1688 = _M0L12digit__startS370 + _M0L6_2atmpS1689;
    _M0L6bufferS379[_M0L6_2atmpS1688] = _M0L5d__hiS387;
    _M0L6_2atmpS1692 = _M0Lm6offsetS368;
    _M0L6_2atmpS1691 = _M0L12digit__startS370 + _M0L6_2atmpS1692;
    _M0L6_2atmpS1690 = _M0L6_2atmpS1691 + 1;
    _M0L6bufferS379[_M0L6_2atmpS1690] = _M0L5d__loS388;
    moonbit_decref(_M0L6bufferS379);
  } else {
    int32_t _M0L6_2atmpS1699 = _M0Lm6offsetS368;
    int32_t _M0L6_2atmpS1704;
    int32_t _M0L6_2atmpS1700;
    int32_t _M0L6_2atmpS1703;
    int32_t _M0L6_2atmpS1702;
    int32_t _M0L6_2atmpS1701;
    _M0Lm6offsetS368 = _M0L6_2atmpS1699 - 1;
    _M0L6_2atmpS1704 = _M0Lm6offsetS368;
    _M0L6_2atmpS1700 = _M0L12digit__startS370 + _M0L6_2atmpS1704;
    _M0L6_2atmpS1703 = _M0Lm9remainingS381;
    _M0L6_2atmpS1702 = 48 + _M0L6_2atmpS1703;
    _M0L6_2atmpS1701 = (uint16_t)_M0L6_2atmpS1702;
    _M0L6bufferS379[_M0L6_2atmpS1700] = _M0L6_2atmpS1701;
    moonbit_decref(_M0L6bufferS379);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS361,
  uint64_t _M0L3numS355,
  int32_t _M0L12digit__startS353,
  int32_t _M0L10total__lenS352,
  int32_t _M0L5radixS357
) {
  int32_t _M0Lm6offsetS351;
  uint64_t _M0Lm1nS354;
  uint64_t _M0L4baseS356;
  int32_t _M0L6_2atmpS1631;
  int32_t _M0L6_2atmpS1630;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS351 = _M0L10total__lenS352 - _M0L12digit__startS353;
  _M0Lm1nS354 = _M0L3numS355;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS356 = _M0MPC13int3Int10to__uint64(_M0L5radixS357);
  _M0L6_2atmpS1631 = _M0L5radixS357 - 1;
  _M0L6_2atmpS1630 = _M0L5radixS357 & _M0L6_2atmpS1631;
  if (_M0L6_2atmpS1630 == 0) {
    int32_t _M0L5shiftS358;
    uint64_t _M0L4maskS359;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS358 = moonbit_ctz32(_M0L5radixS357);
    _M0L4maskS359 = _M0L4baseS356 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1632 = _M0Lm1nS354;
      if (_M0L6_2atmpS1632 > 0ull) {
        int32_t _M0L6_2atmpS1633 = _M0Lm6offsetS351;
        uint64_t _M0L6_2atmpS1639;
        uint64_t _M0L6_2atmpS1638;
        int32_t _M0L5digitS360;
        int32_t _M0L6_2atmpS1636;
        int32_t _M0L6_2atmpS1634;
        int32_t _M0L6_2atmpS1635;
        uint64_t _M0L6_2atmpS1637;
        _M0Lm6offsetS351 = _M0L6_2atmpS1633 - 1;
        _M0L6_2atmpS1639 = _M0Lm1nS354;
        _M0L6_2atmpS1638 = _M0L6_2atmpS1639 & _M0L4maskS359;
        _M0L5digitS360 = (int32_t)_M0L6_2atmpS1638;
        _M0L6_2atmpS1636 = _M0Lm6offsetS351;
        _M0L6_2atmpS1634 = _M0L12digit__startS353 + _M0L6_2atmpS1636;
        _M0L6_2atmpS1635
        = ((moonbit_string_t)moonbit_string_literal_247.data)[
          _M0L5digitS360
        ];
        _M0L6bufferS361[_M0L6_2atmpS1634] = _M0L6_2atmpS1635;
        _M0L6_2atmpS1637 = _M0Lm1nS354;
        _M0Lm1nS354 = _M0L6_2atmpS1637 >> (_M0L5shiftS358 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS361);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1640 = _M0Lm1nS354;
      if (_M0L6_2atmpS1640 > 0ull) {
        int32_t _M0L6_2atmpS1641 = _M0Lm6offsetS351;
        uint64_t _M0L6_2atmpS1648;
        uint64_t _M0L1qS363;
        uint64_t _M0L6_2atmpS1646;
        uint64_t _M0L6_2atmpS1647;
        uint64_t _M0L6_2atmpS1645;
        int32_t _M0L5digitS364;
        int32_t _M0L6_2atmpS1644;
        int32_t _M0L6_2atmpS1642;
        int32_t _M0L6_2atmpS1643;
        _M0Lm6offsetS351 = _M0L6_2atmpS1641 - 1;
        _M0L6_2atmpS1648 = _M0Lm1nS354;
        _M0L1qS363 = _M0L6_2atmpS1648 / _M0L4baseS356;
        _M0L6_2atmpS1646 = _M0Lm1nS354;
        _M0L6_2atmpS1647 = _M0L1qS363 * _M0L4baseS356;
        _M0L6_2atmpS1645 = _M0L6_2atmpS1646 - _M0L6_2atmpS1647;
        _M0L5digitS364 = (int32_t)_M0L6_2atmpS1645;
        _M0L6_2atmpS1644 = _M0Lm6offsetS351;
        _M0L6_2atmpS1642 = _M0L12digit__startS353 + _M0L6_2atmpS1644;
        _M0L6_2atmpS1643
        = ((moonbit_string_t)moonbit_string_literal_247.data)[
          _M0L5digitS364
        ];
        _M0L6bufferS361[_M0L6_2atmpS1642] = _M0L6_2atmpS1643;
        _M0Lm1nS354 = _M0L1qS363;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS361);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS348,
  uint64_t _M0L3numS344,
  int32_t _M0L12digit__startS342,
  int32_t _M0L10total__lenS341
) {
  int32_t _M0Lm6offsetS340;
  uint64_t _M0Lm1nS343;
  int32_t _M0L6_2atmpS1626;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS340 = _M0L10total__lenS341 - _M0L12digit__startS342;
  _M0Lm1nS343 = _M0L3numS344;
  while (1) {
    int32_t _M0L6_2atmpS1614 = _M0Lm6offsetS340;
    if (_M0L6_2atmpS1614 >= 2) {
      int32_t _M0L6_2atmpS1615 = _M0Lm6offsetS340;
      uint64_t _M0L6_2atmpS1625;
      uint64_t _M0L6_2atmpS1624;
      int32_t _M0L9byte__valS345;
      int32_t _M0L2hiS346;
      int32_t _M0L2loS347;
      int32_t _M0L6_2atmpS1618;
      int32_t _M0L6_2atmpS1616;
      int32_t _M0L6_2atmpS1617;
      int32_t _M0L6_2atmpS1622;
      int32_t _M0L6_2atmpS1621;
      int32_t _M0L6_2atmpS1619;
      int32_t _M0L6_2atmpS1620;
      uint64_t _M0L6_2atmpS1623;
      _M0Lm6offsetS340 = _M0L6_2atmpS1615 - 2;
      _M0L6_2atmpS1625 = _M0Lm1nS343;
      _M0L6_2atmpS1624 = _M0L6_2atmpS1625 & 255ull;
      _M0L9byte__valS345 = (int32_t)_M0L6_2atmpS1624;
      _M0L2hiS346 = _M0L9byte__valS345 / 16;
      _M0L2loS347 = _M0L9byte__valS345 % 16;
      _M0L6_2atmpS1618 = _M0Lm6offsetS340;
      _M0L6_2atmpS1616 = _M0L12digit__startS342 + _M0L6_2atmpS1618;
      _M0L6_2atmpS1617
      = ((moonbit_string_t)moonbit_string_literal_247.data)[
        _M0L2hiS346
      ];
      _M0L6bufferS348[_M0L6_2atmpS1616] = _M0L6_2atmpS1617;
      _M0L6_2atmpS1622 = _M0Lm6offsetS340;
      _M0L6_2atmpS1621 = _M0L12digit__startS342 + _M0L6_2atmpS1622;
      _M0L6_2atmpS1619 = _M0L6_2atmpS1621 + 1;
      _M0L6_2atmpS1620
      = ((moonbit_string_t)moonbit_string_literal_247.data)[
        _M0L2loS347
      ];
      _M0L6bufferS348[_M0L6_2atmpS1619] = _M0L6_2atmpS1620;
      _M0L6_2atmpS1623 = _M0Lm1nS343;
      _M0Lm1nS343 = _M0L6_2atmpS1623 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1626 = _M0Lm6offsetS340;
  if (_M0L6_2atmpS1626 == 1) {
    uint64_t _M0L6_2atmpS1629 = _M0Lm1nS343;
    uint64_t _M0L6_2atmpS1628 = _M0L6_2atmpS1629 & 15ull;
    int32_t _M0L6nibbleS350 = (int32_t)_M0L6_2atmpS1628;
    int32_t _M0L6_2atmpS1627 =
      ((moonbit_string_t)moonbit_string_literal_247.data)[_M0L6nibbleS350];
    _M0L6bufferS348[_M0L12digit__startS342] = _M0L6_2atmpS1627;
    moonbit_decref(_M0L6bufferS348);
  } else {
    moonbit_decref(_M0L6bufferS348);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS334,
  int32_t _M0L5radixS337
) {
  uint64_t _M0Lm3numS335;
  uint64_t _M0L4baseS336;
  int32_t _M0Lm5countS338;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS334 == 0ull) {
    return 1;
  }
  _M0Lm3numS335 = _M0L5valueS334;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS336 = _M0MPC13int3Int10to__uint64(_M0L5radixS337);
  _M0Lm5countS338 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1611 = _M0Lm3numS335;
    if (_M0L6_2atmpS1611 > 0ull) {
      int32_t _M0L6_2atmpS1612 = _M0Lm5countS338;
      uint64_t _M0L6_2atmpS1613;
      _M0Lm5countS338 = _M0L6_2atmpS1612 + 1;
      _M0L6_2atmpS1613 = _M0Lm3numS335;
      _M0Lm3numS335 = _M0L6_2atmpS1613 / _M0L4baseS336;
      continue;
    }
    break;
  }
  return _M0Lm5countS338;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS332) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS332 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS333;
    int32_t _M0L6_2atmpS1610;
    int32_t _M0L6_2atmpS1609;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS333 = moonbit_clz64(_M0L5valueS332);
    _M0L6_2atmpS1610 = 63 - _M0L14leading__zerosS333;
    _M0L6_2atmpS1609 = _M0L6_2atmpS1610 / 4;
    return _M0L6_2atmpS1609 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS331) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS331 >= 10000000000ull) {
    if (_M0L5valueS331 >= 100000000000000ull) {
      if (_M0L5valueS331 >= 10000000000000000ull) {
        if (_M0L5valueS331 >= 1000000000000000000ull) {
          if (_M0L5valueS331 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS331 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS331 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS331 >= 1000000000000ull) {
      if (_M0L5valueS331 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS331 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS331 >= 100000ull) {
    if (_M0L5valueS331 >= 10000000ull) {
      if (_M0L5valueS331 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS331 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS331 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS331 >= 1000ull) {
    if (_M0L5valueS331 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS331 >= 100ull) {
    return 3;
  } else if (_M0L5valueS331 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS315,
  int32_t _M0L5radixS314
) {
  int32_t _if__result_4060;
  int32_t _M0L12is__negativeS316;
  uint32_t _M0L3numS317;
  uint16_t* _M0L6bufferS318;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS314 < 2) {
    _if__result_4060 = 1;
  } else {
    _if__result_4060 = _M0L5radixS314 > 36;
  }
  if (_if__result_4060) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_245.data, (moonbit_string_t)moonbit_string_literal_248.data);
  }
  if (_M0L4selfS315 == 0) {
    return (moonbit_string_t)moonbit_string_literal_230.data;
  }
  _M0L12is__negativeS316 = _M0L4selfS315 < 0;
  if (_M0L12is__negativeS316) {
    int32_t _M0L6_2atmpS1608 = -_M0L4selfS315;
    _M0L3numS317 = *(uint32_t*)&_M0L6_2atmpS1608;
  } else {
    _M0L3numS317 = *(uint32_t*)&_M0L4selfS315;
  }
  switch (_M0L5radixS314) {
    case 10: {
      int32_t _M0L10digit__lenS319;
      int32_t _M0L6_2atmpS1605;
      int32_t _M0L10total__lenS320;
      uint16_t* _M0L6bufferS321;
      int32_t _M0L12digit__startS322;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS319 = _M0FPB12dec__count32(_M0L3numS317);
      if (_M0L12is__negativeS316) {
        _M0L6_2atmpS1605 = 1;
      } else {
        _M0L6_2atmpS1605 = 0;
      }
      _M0L10total__lenS320 = _M0L10digit__lenS319 + _M0L6_2atmpS1605;
      _M0L6bufferS321
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS320, 0);
      if (_M0L12is__negativeS316) {
        _M0L12digit__startS322 = 1;
      } else {
        _M0L12digit__startS322 = 0;
      }
      moonbit_incref(_M0L6bufferS321);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS321, _M0L3numS317, _M0L12digit__startS322, _M0L10total__lenS320);
      _M0L6bufferS318 = _M0L6bufferS321;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS323;
      int32_t _M0L6_2atmpS1606;
      int32_t _M0L10total__lenS324;
      uint16_t* _M0L6bufferS325;
      int32_t _M0L12digit__startS326;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS323 = _M0FPB12hex__count32(_M0L3numS317);
      if (_M0L12is__negativeS316) {
        _M0L6_2atmpS1606 = 1;
      } else {
        _M0L6_2atmpS1606 = 0;
      }
      _M0L10total__lenS324 = _M0L10digit__lenS323 + _M0L6_2atmpS1606;
      _M0L6bufferS325
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS324, 0);
      if (_M0L12is__negativeS316) {
        _M0L12digit__startS326 = 1;
      } else {
        _M0L12digit__startS326 = 0;
      }
      moonbit_incref(_M0L6bufferS325);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS325, _M0L3numS317, _M0L12digit__startS326, _M0L10total__lenS324);
      _M0L6bufferS318 = _M0L6bufferS325;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS327;
      int32_t _M0L6_2atmpS1607;
      int32_t _M0L10total__lenS328;
      uint16_t* _M0L6bufferS329;
      int32_t _M0L12digit__startS330;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS327
      = _M0FPB14radix__count32(_M0L3numS317, _M0L5radixS314);
      if (_M0L12is__negativeS316) {
        _M0L6_2atmpS1607 = 1;
      } else {
        _M0L6_2atmpS1607 = 0;
      }
      _M0L10total__lenS328 = _M0L10digit__lenS327 + _M0L6_2atmpS1607;
      _M0L6bufferS329
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS328, 0);
      if (_M0L12is__negativeS316) {
        _M0L12digit__startS330 = 1;
      } else {
        _M0L12digit__startS330 = 0;
      }
      moonbit_incref(_M0L6bufferS329);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS329, _M0L3numS317, _M0L12digit__startS330, _M0L10total__lenS328, _M0L5radixS314);
      _M0L6bufferS318 = _M0L6bufferS329;
      break;
    }
  }
  if (_M0L12is__negativeS316) {
    _M0L6bufferS318[0] = 45;
  }
  return _M0L6bufferS318;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS308,
  int32_t _M0L5radixS311
) {
  uint32_t _M0Lm3numS309;
  uint32_t _M0L4baseS310;
  int32_t _M0Lm5countS312;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS308 == 0u) {
    return 1;
  }
  _M0Lm3numS309 = _M0L5valueS308;
  _M0L4baseS310 = *(uint32_t*)&_M0L5radixS311;
  _M0Lm5countS312 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1602 = _M0Lm3numS309;
    if (_M0L6_2atmpS1602 > 0u) {
      int32_t _M0L6_2atmpS1603 = _M0Lm5countS312;
      uint32_t _M0L6_2atmpS1604;
      _M0Lm5countS312 = _M0L6_2atmpS1603 + 1;
      _M0L6_2atmpS1604 = _M0Lm3numS309;
      _M0Lm3numS309 = _M0L6_2atmpS1604 / _M0L4baseS310;
      continue;
    }
    break;
  }
  return _M0Lm5countS312;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS306) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS306 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS307;
    int32_t _M0L6_2atmpS1601;
    int32_t _M0L6_2atmpS1600;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS307 = moonbit_clz32(_M0L5valueS306);
    _M0L6_2atmpS1601 = 31 - _M0L14leading__zerosS307;
    _M0L6_2atmpS1600 = _M0L6_2atmpS1601 / 4;
    return _M0L6_2atmpS1600 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS305) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS305 >= 100000u) {
    if (_M0L5valueS305 >= 10000000u) {
      if (_M0L5valueS305 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS305 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS305 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS305 >= 1000u) {
    if (_M0L5valueS305 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS305 >= 100u) {
    return 3;
  } else if (_M0L5valueS305 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS295,
  uint32_t _M0L3numS283,
  int32_t _M0L12digit__startS286,
  int32_t _M0L10total__lenS285
) {
  uint32_t _M0Lm3numS282;
  int32_t _M0Lm6offsetS284;
  uint32_t _M0L6_2atmpS1599;
  int32_t _M0Lm9remainingS297;
  int32_t _M0L6_2atmpS1580;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS282 = _M0L3numS283;
  _M0Lm6offsetS284 = _M0L10total__lenS285 - _M0L12digit__startS286;
  while (1) {
    uint32_t _M0L6_2atmpS1543 = _M0Lm3numS282;
    if (_M0L6_2atmpS1543 >= 10000u) {
      uint32_t _M0L6_2atmpS1566 = _M0Lm3numS282;
      uint32_t _M0L1tS287 = _M0L6_2atmpS1566 / 10000u;
      uint32_t _M0L6_2atmpS1565 = _M0Lm3numS282;
      uint32_t _M0L6_2atmpS1564 = _M0L6_2atmpS1565 % 10000u;
      int32_t _M0L1rS288 = *(int32_t*)&_M0L6_2atmpS1564;
      int32_t _M0L2d1S289;
      int32_t _M0L2d2S290;
      int32_t _M0L6_2atmpS1544;
      int32_t _M0L6_2atmpS1563;
      int32_t _M0L6_2atmpS1562;
      int32_t _M0L6d1__hiS291;
      int32_t _M0L6_2atmpS1561;
      int32_t _M0L6_2atmpS1560;
      int32_t _M0L6d1__loS292;
      int32_t _M0L6_2atmpS1559;
      int32_t _M0L6_2atmpS1558;
      int32_t _M0L6d2__hiS293;
      int32_t _M0L6_2atmpS1557;
      int32_t _M0L6_2atmpS1556;
      int32_t _M0L6d2__loS294;
      int32_t _M0L6_2atmpS1546;
      int32_t _M0L6_2atmpS1545;
      int32_t _M0L6_2atmpS1549;
      int32_t _M0L6_2atmpS1548;
      int32_t _M0L6_2atmpS1547;
      int32_t _M0L6_2atmpS1552;
      int32_t _M0L6_2atmpS1551;
      int32_t _M0L6_2atmpS1550;
      int32_t _M0L6_2atmpS1555;
      int32_t _M0L6_2atmpS1554;
      int32_t _M0L6_2atmpS1553;
      _M0Lm3numS282 = _M0L1tS287;
      _M0L2d1S289 = _M0L1rS288 / 100;
      _M0L2d2S290 = _M0L1rS288 % 100;
      _M0L6_2atmpS1544 = _M0Lm6offsetS284;
      _M0Lm6offsetS284 = _M0L6_2atmpS1544 - 4;
      _M0L6_2atmpS1563 = _M0L2d1S289 / 10;
      _M0L6_2atmpS1562 = 48 + _M0L6_2atmpS1563;
      _M0L6d1__hiS291 = (uint16_t)_M0L6_2atmpS1562;
      _M0L6_2atmpS1561 = _M0L2d1S289 % 10;
      _M0L6_2atmpS1560 = 48 + _M0L6_2atmpS1561;
      _M0L6d1__loS292 = (uint16_t)_M0L6_2atmpS1560;
      _M0L6_2atmpS1559 = _M0L2d2S290 / 10;
      _M0L6_2atmpS1558 = 48 + _M0L6_2atmpS1559;
      _M0L6d2__hiS293 = (uint16_t)_M0L6_2atmpS1558;
      _M0L6_2atmpS1557 = _M0L2d2S290 % 10;
      _M0L6_2atmpS1556 = 48 + _M0L6_2atmpS1557;
      _M0L6d2__loS294 = (uint16_t)_M0L6_2atmpS1556;
      _M0L6_2atmpS1546 = _M0Lm6offsetS284;
      _M0L6_2atmpS1545 = _M0L12digit__startS286 + _M0L6_2atmpS1546;
      _M0L6bufferS295[_M0L6_2atmpS1545] = _M0L6d1__hiS291;
      _M0L6_2atmpS1549 = _M0Lm6offsetS284;
      _M0L6_2atmpS1548 = _M0L12digit__startS286 + _M0L6_2atmpS1549;
      _M0L6_2atmpS1547 = _M0L6_2atmpS1548 + 1;
      _M0L6bufferS295[_M0L6_2atmpS1547] = _M0L6d1__loS292;
      _M0L6_2atmpS1552 = _M0Lm6offsetS284;
      _M0L6_2atmpS1551 = _M0L12digit__startS286 + _M0L6_2atmpS1552;
      _M0L6_2atmpS1550 = _M0L6_2atmpS1551 + 2;
      _M0L6bufferS295[_M0L6_2atmpS1550] = _M0L6d2__hiS293;
      _M0L6_2atmpS1555 = _M0Lm6offsetS284;
      _M0L6_2atmpS1554 = _M0L12digit__startS286 + _M0L6_2atmpS1555;
      _M0L6_2atmpS1553 = _M0L6_2atmpS1554 + 3;
      _M0L6bufferS295[_M0L6_2atmpS1553] = _M0L6d2__loS294;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1599 = _M0Lm3numS282;
  _M0Lm9remainingS297 = *(int32_t*)&_M0L6_2atmpS1599;
  while (1) {
    int32_t _M0L6_2atmpS1567 = _M0Lm9remainingS297;
    if (_M0L6_2atmpS1567 >= 100) {
      int32_t _M0L6_2atmpS1579 = _M0Lm9remainingS297;
      int32_t _M0L1tS298 = _M0L6_2atmpS1579 / 100;
      int32_t _M0L6_2atmpS1578 = _M0Lm9remainingS297;
      int32_t _M0L1dS299 = _M0L6_2atmpS1578 % 100;
      int32_t _M0L6_2atmpS1568;
      int32_t _M0L6_2atmpS1577;
      int32_t _M0L6_2atmpS1576;
      int32_t _M0L5d__hiS300;
      int32_t _M0L6_2atmpS1575;
      int32_t _M0L6_2atmpS1574;
      int32_t _M0L5d__loS301;
      int32_t _M0L6_2atmpS1570;
      int32_t _M0L6_2atmpS1569;
      int32_t _M0L6_2atmpS1573;
      int32_t _M0L6_2atmpS1572;
      int32_t _M0L6_2atmpS1571;
      _M0Lm9remainingS297 = _M0L1tS298;
      _M0L6_2atmpS1568 = _M0Lm6offsetS284;
      _M0Lm6offsetS284 = _M0L6_2atmpS1568 - 2;
      _M0L6_2atmpS1577 = _M0L1dS299 / 10;
      _M0L6_2atmpS1576 = 48 + _M0L6_2atmpS1577;
      _M0L5d__hiS300 = (uint16_t)_M0L6_2atmpS1576;
      _M0L6_2atmpS1575 = _M0L1dS299 % 10;
      _M0L6_2atmpS1574 = 48 + _M0L6_2atmpS1575;
      _M0L5d__loS301 = (uint16_t)_M0L6_2atmpS1574;
      _M0L6_2atmpS1570 = _M0Lm6offsetS284;
      _M0L6_2atmpS1569 = _M0L12digit__startS286 + _M0L6_2atmpS1570;
      _M0L6bufferS295[_M0L6_2atmpS1569] = _M0L5d__hiS300;
      _M0L6_2atmpS1573 = _M0Lm6offsetS284;
      _M0L6_2atmpS1572 = _M0L12digit__startS286 + _M0L6_2atmpS1573;
      _M0L6_2atmpS1571 = _M0L6_2atmpS1572 + 1;
      _M0L6bufferS295[_M0L6_2atmpS1571] = _M0L5d__loS301;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1580 = _M0Lm9remainingS297;
  if (_M0L6_2atmpS1580 >= 10) {
    int32_t _M0L6_2atmpS1581 = _M0Lm6offsetS284;
    int32_t _M0L6_2atmpS1592;
    int32_t _M0L6_2atmpS1591;
    int32_t _M0L6_2atmpS1590;
    int32_t _M0L5d__hiS303;
    int32_t _M0L6_2atmpS1589;
    int32_t _M0L6_2atmpS1588;
    int32_t _M0L6_2atmpS1587;
    int32_t _M0L5d__loS304;
    int32_t _M0L6_2atmpS1583;
    int32_t _M0L6_2atmpS1582;
    int32_t _M0L6_2atmpS1586;
    int32_t _M0L6_2atmpS1585;
    int32_t _M0L6_2atmpS1584;
    _M0Lm6offsetS284 = _M0L6_2atmpS1581 - 2;
    _M0L6_2atmpS1592 = _M0Lm9remainingS297;
    _M0L6_2atmpS1591 = _M0L6_2atmpS1592 / 10;
    _M0L6_2atmpS1590 = 48 + _M0L6_2atmpS1591;
    _M0L5d__hiS303 = (uint16_t)_M0L6_2atmpS1590;
    _M0L6_2atmpS1589 = _M0Lm9remainingS297;
    _M0L6_2atmpS1588 = _M0L6_2atmpS1589 % 10;
    _M0L6_2atmpS1587 = 48 + _M0L6_2atmpS1588;
    _M0L5d__loS304 = (uint16_t)_M0L6_2atmpS1587;
    _M0L6_2atmpS1583 = _M0Lm6offsetS284;
    _M0L6_2atmpS1582 = _M0L12digit__startS286 + _M0L6_2atmpS1583;
    _M0L6bufferS295[_M0L6_2atmpS1582] = _M0L5d__hiS303;
    _M0L6_2atmpS1586 = _M0Lm6offsetS284;
    _M0L6_2atmpS1585 = _M0L12digit__startS286 + _M0L6_2atmpS1586;
    _M0L6_2atmpS1584 = _M0L6_2atmpS1585 + 1;
    _M0L6bufferS295[_M0L6_2atmpS1584] = _M0L5d__loS304;
    moonbit_decref(_M0L6bufferS295);
  } else {
    int32_t _M0L6_2atmpS1593 = _M0Lm6offsetS284;
    int32_t _M0L6_2atmpS1598;
    int32_t _M0L6_2atmpS1594;
    int32_t _M0L6_2atmpS1597;
    int32_t _M0L6_2atmpS1596;
    int32_t _M0L6_2atmpS1595;
    _M0Lm6offsetS284 = _M0L6_2atmpS1593 - 1;
    _M0L6_2atmpS1598 = _M0Lm6offsetS284;
    _M0L6_2atmpS1594 = _M0L12digit__startS286 + _M0L6_2atmpS1598;
    _M0L6_2atmpS1597 = _M0Lm9remainingS297;
    _M0L6_2atmpS1596 = 48 + _M0L6_2atmpS1597;
    _M0L6_2atmpS1595 = (uint16_t)_M0L6_2atmpS1596;
    _M0L6bufferS295[_M0L6_2atmpS1594] = _M0L6_2atmpS1595;
    moonbit_decref(_M0L6bufferS295);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS277,
  uint32_t _M0L3numS271,
  int32_t _M0L12digit__startS269,
  int32_t _M0L10total__lenS268,
  int32_t _M0L5radixS273
) {
  int32_t _M0Lm6offsetS267;
  uint32_t _M0Lm1nS270;
  uint32_t _M0L4baseS272;
  int32_t _M0L6_2atmpS1525;
  int32_t _M0L6_2atmpS1524;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS267 = _M0L10total__lenS268 - _M0L12digit__startS269;
  _M0Lm1nS270 = _M0L3numS271;
  _M0L4baseS272 = *(uint32_t*)&_M0L5radixS273;
  _M0L6_2atmpS1525 = _M0L5radixS273 - 1;
  _M0L6_2atmpS1524 = _M0L5radixS273 & _M0L6_2atmpS1525;
  if (_M0L6_2atmpS1524 == 0) {
    int32_t _M0L5shiftS274;
    uint32_t _M0L4maskS275;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS274 = moonbit_ctz32(_M0L5radixS273);
    _M0L4maskS275 = _M0L4baseS272 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1526 = _M0Lm1nS270;
      if (_M0L6_2atmpS1526 > 0u) {
        int32_t _M0L6_2atmpS1527 = _M0Lm6offsetS267;
        uint32_t _M0L6_2atmpS1533;
        uint32_t _M0L6_2atmpS1532;
        int32_t _M0L5digitS276;
        int32_t _M0L6_2atmpS1530;
        int32_t _M0L6_2atmpS1528;
        int32_t _M0L6_2atmpS1529;
        uint32_t _M0L6_2atmpS1531;
        _M0Lm6offsetS267 = _M0L6_2atmpS1527 - 1;
        _M0L6_2atmpS1533 = _M0Lm1nS270;
        _M0L6_2atmpS1532 = _M0L6_2atmpS1533 & _M0L4maskS275;
        _M0L5digitS276 = *(int32_t*)&_M0L6_2atmpS1532;
        _M0L6_2atmpS1530 = _M0Lm6offsetS267;
        _M0L6_2atmpS1528 = _M0L12digit__startS269 + _M0L6_2atmpS1530;
        _M0L6_2atmpS1529
        = ((moonbit_string_t)moonbit_string_literal_247.data)[
          _M0L5digitS276
        ];
        _M0L6bufferS277[_M0L6_2atmpS1528] = _M0L6_2atmpS1529;
        _M0L6_2atmpS1531 = _M0Lm1nS270;
        _M0Lm1nS270 = _M0L6_2atmpS1531 >> (_M0L5shiftS274 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS277);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1534 = _M0Lm1nS270;
      if (_M0L6_2atmpS1534 > 0u) {
        int32_t _M0L6_2atmpS1535 = _M0Lm6offsetS267;
        uint32_t _M0L6_2atmpS1542;
        uint32_t _M0L1qS279;
        uint32_t _M0L6_2atmpS1540;
        uint32_t _M0L6_2atmpS1541;
        uint32_t _M0L6_2atmpS1539;
        int32_t _M0L5digitS280;
        int32_t _M0L6_2atmpS1538;
        int32_t _M0L6_2atmpS1536;
        int32_t _M0L6_2atmpS1537;
        _M0Lm6offsetS267 = _M0L6_2atmpS1535 - 1;
        _M0L6_2atmpS1542 = _M0Lm1nS270;
        _M0L1qS279 = _M0L6_2atmpS1542 / _M0L4baseS272;
        _M0L6_2atmpS1540 = _M0Lm1nS270;
        _M0L6_2atmpS1541 = _M0L1qS279 * _M0L4baseS272;
        _M0L6_2atmpS1539 = _M0L6_2atmpS1540 - _M0L6_2atmpS1541;
        _M0L5digitS280 = *(int32_t*)&_M0L6_2atmpS1539;
        _M0L6_2atmpS1538 = _M0Lm6offsetS267;
        _M0L6_2atmpS1536 = _M0L12digit__startS269 + _M0L6_2atmpS1538;
        _M0L6_2atmpS1537
        = ((moonbit_string_t)moonbit_string_literal_247.data)[
          _M0L5digitS280
        ];
        _M0L6bufferS277[_M0L6_2atmpS1536] = _M0L6_2atmpS1537;
        _M0Lm1nS270 = _M0L1qS279;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS277);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS264,
  uint32_t _M0L3numS260,
  int32_t _M0L12digit__startS258,
  int32_t _M0L10total__lenS257
) {
  int32_t _M0Lm6offsetS256;
  uint32_t _M0Lm1nS259;
  int32_t _M0L6_2atmpS1520;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS256 = _M0L10total__lenS257 - _M0L12digit__startS258;
  _M0Lm1nS259 = _M0L3numS260;
  while (1) {
    int32_t _M0L6_2atmpS1508 = _M0Lm6offsetS256;
    if (_M0L6_2atmpS1508 >= 2) {
      int32_t _M0L6_2atmpS1509 = _M0Lm6offsetS256;
      uint32_t _M0L6_2atmpS1519;
      uint32_t _M0L6_2atmpS1518;
      int32_t _M0L9byte__valS261;
      int32_t _M0L2hiS262;
      int32_t _M0L2loS263;
      int32_t _M0L6_2atmpS1512;
      int32_t _M0L6_2atmpS1510;
      int32_t _M0L6_2atmpS1511;
      int32_t _M0L6_2atmpS1516;
      int32_t _M0L6_2atmpS1515;
      int32_t _M0L6_2atmpS1513;
      int32_t _M0L6_2atmpS1514;
      uint32_t _M0L6_2atmpS1517;
      _M0Lm6offsetS256 = _M0L6_2atmpS1509 - 2;
      _M0L6_2atmpS1519 = _M0Lm1nS259;
      _M0L6_2atmpS1518 = _M0L6_2atmpS1519 & 255u;
      _M0L9byte__valS261 = *(int32_t*)&_M0L6_2atmpS1518;
      _M0L2hiS262 = _M0L9byte__valS261 / 16;
      _M0L2loS263 = _M0L9byte__valS261 % 16;
      _M0L6_2atmpS1512 = _M0Lm6offsetS256;
      _M0L6_2atmpS1510 = _M0L12digit__startS258 + _M0L6_2atmpS1512;
      _M0L6_2atmpS1511
      = ((moonbit_string_t)moonbit_string_literal_247.data)[
        _M0L2hiS262
      ];
      _M0L6bufferS264[_M0L6_2atmpS1510] = _M0L6_2atmpS1511;
      _M0L6_2atmpS1516 = _M0Lm6offsetS256;
      _M0L6_2atmpS1515 = _M0L12digit__startS258 + _M0L6_2atmpS1516;
      _M0L6_2atmpS1513 = _M0L6_2atmpS1515 + 1;
      _M0L6_2atmpS1514
      = ((moonbit_string_t)moonbit_string_literal_247.data)[
        _M0L2loS263
      ];
      _M0L6bufferS264[_M0L6_2atmpS1513] = _M0L6_2atmpS1514;
      _M0L6_2atmpS1517 = _M0Lm1nS259;
      _M0Lm1nS259 = _M0L6_2atmpS1517 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1520 = _M0Lm6offsetS256;
  if (_M0L6_2atmpS1520 == 1) {
    uint32_t _M0L6_2atmpS1523 = _M0Lm1nS259;
    uint32_t _M0L6_2atmpS1522 = _M0L6_2atmpS1523 & 15u;
    int32_t _M0L6nibbleS266 = *(int32_t*)&_M0L6_2atmpS1522;
    int32_t _M0L6_2atmpS1521 =
      ((moonbit_string_t)moonbit_string_literal_247.data)[_M0L6nibbleS266];
    _M0L6bufferS264[_M0L12digit__startS258] = _M0L6_2atmpS1521;
    moonbit_decref(_M0L6bufferS264);
  } else {
    moonbit_decref(_M0L6bufferS264);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS251) {
  struct _M0TWEOs* _M0L7_2afuncS250;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS250 = _M0L4selfS251;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS250->code(_M0L7_2afuncS250);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS253
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS252;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS252 = _M0L4selfS253;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS252->code(_M0L7_2afuncS252);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS255) {
  struct _M0TWEOc* _M0L7_2afuncS254;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS254 = _M0L4selfS255;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS254->code(_M0L7_2afuncS254);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS241
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS240;
  struct _M0TPB6Logger _M0L6_2atmpS1503;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS240 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS240);
  _M0L6_2atmpS1503
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS240
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS241, _M0L6_2atmpS1503);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS240);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS243
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS242;
  struct _M0TPB6Logger _M0L6_2atmpS1504;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS242 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS242);
  _M0L6_2atmpS1504
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS242
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS243, _M0L6_2atmpS1504);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS242);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(
  int32_t _M0L4selfS245
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS244;
  struct _M0TPB6Logger _M0L6_2atmpS1505;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS244 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS244);
  _M0L6_2atmpS1505
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS244
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14bool4BoolPB4Show6output(_M0L4selfS245, _M0L6_2atmpS1505);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS244);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS247
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS246;
  struct _M0TPB6Logger _M0L6_2atmpS1506;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS246 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS246);
  _M0L6_2atmpS1506
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS246
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS247, _M0L6_2atmpS1506);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS246);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS249
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS248;
  struct _M0TPB6Logger _M0L6_2atmpS1507;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS248 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS248);
  _M0L6_2atmpS1507
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS248
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS249, _M0L6_2atmpS1507);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS248);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS239
) {
  int32_t _M0L8_2afieldS3668;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3668 = _M0L4selfS239.$1;
  moonbit_decref(_M0L4selfS239.$0);
  return _M0L8_2afieldS3668;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS238
) {
  int32_t _M0L3endS1501;
  int32_t _M0L8_2afieldS3669;
  int32_t _M0L5startS1502;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1501 = _M0L4selfS238.$2;
  _M0L8_2afieldS3669 = _M0L4selfS238.$1;
  moonbit_decref(_M0L4selfS238.$0);
  _M0L5startS1502 = _M0L8_2afieldS3669;
  return _M0L3endS1501 - _M0L5startS1502;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS237
) {
  moonbit_string_t _M0L8_2afieldS3670;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3670 = _M0L4selfS237.$0;
  return _M0L8_2afieldS3670;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS233,
  moonbit_string_t _M0L5valueS234,
  int32_t _M0L5startS235,
  int32_t _M0L3lenS236
) {
  int32_t _M0L6_2atmpS1500;
  int64_t _M0L6_2atmpS1499;
  struct _M0TPC16string10StringView _M0L6_2atmpS1498;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1500 = _M0L5startS235 + _M0L3lenS236;
  _M0L6_2atmpS1499 = (int64_t)_M0L6_2atmpS1500;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1498
  = _M0MPC16string6String11sub_2einner(_M0L5valueS234, _M0L5startS235, _M0L6_2atmpS1499);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS233, _M0L6_2atmpS1498);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS226,
  int32_t _M0L5startS232,
  int64_t _M0L3endS228
) {
  int32_t _M0L3lenS225;
  int32_t _M0L3endS227;
  int32_t _M0L5startS231;
  int32_t _if__result_4067;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS225 = Moonbit_array_length(_M0L4selfS226);
  if (_M0L3endS228 == 4294967296ll) {
    _M0L3endS227 = _M0L3lenS225;
  } else {
    int64_t _M0L7_2aSomeS229 = _M0L3endS228;
    int32_t _M0L6_2aendS230 = (int32_t)_M0L7_2aSomeS229;
    if (_M0L6_2aendS230 < 0) {
      _M0L3endS227 = _M0L3lenS225 + _M0L6_2aendS230;
    } else {
      _M0L3endS227 = _M0L6_2aendS230;
    }
  }
  if (_M0L5startS232 < 0) {
    _M0L5startS231 = _M0L3lenS225 + _M0L5startS232;
  } else {
    _M0L5startS231 = _M0L5startS232;
  }
  if (_M0L5startS231 >= 0) {
    if (_M0L5startS231 <= _M0L3endS227) {
      _if__result_4067 = _M0L3endS227 <= _M0L3lenS225;
    } else {
      _if__result_4067 = 0;
    }
  } else {
    _if__result_4067 = 0;
  }
  if (_if__result_4067) {
    if (_M0L5startS231 < _M0L3lenS225) {
      int32_t _M0L6_2atmpS1495 = _M0L4selfS226[_M0L5startS231];
      int32_t _M0L6_2atmpS1494;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1494
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1495);
      if (!_M0L6_2atmpS1494) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS227 < _M0L3lenS225) {
      int32_t _M0L6_2atmpS1497 = _M0L4selfS226[_M0L3endS227];
      int32_t _M0L6_2atmpS1496;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1496
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1497);
      if (!_M0L6_2atmpS1496) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS231,
                                                 _M0L3endS227,
                                                 _M0L4selfS226};
  } else {
    moonbit_decref(_M0L4selfS226);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS222) {
  struct _M0TPB6Hasher* _M0L1hS221;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS221 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS221);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS221, _M0L4selfS222);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS221);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS224
) {
  struct _M0TPB6Hasher* _M0L1hS223;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS223 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS223);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS223, _M0L4selfS224);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS223);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS219) {
  int32_t _M0L4seedS218;
  if (_M0L10seed_2eoptS219 == 4294967296ll) {
    _M0L4seedS218 = 0;
  } else {
    int64_t _M0L7_2aSomeS220 = _M0L10seed_2eoptS219;
    _M0L4seedS218 = (int32_t)_M0L7_2aSomeS220;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS218);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS217) {
  uint32_t _M0L6_2atmpS1493;
  uint32_t _M0L6_2atmpS1492;
  struct _M0TPB6Hasher* _block_4068;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1493 = *(uint32_t*)&_M0L4seedS217;
  _M0L6_2atmpS1492 = _M0L6_2atmpS1493 + 374761393u;
  _block_4068
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4068)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4068->$0 = _M0L6_2atmpS1492;
  return _block_4068;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS216) {
  uint32_t _M0L6_2atmpS1491;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1491 = _M0MPB6Hasher9avalanche(_M0L4selfS216);
  return *(int32_t*)&_M0L6_2atmpS1491;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS215) {
  uint32_t _M0L8_2afieldS3671;
  uint32_t _M0Lm3accS214;
  uint32_t _M0L6_2atmpS1480;
  uint32_t _M0L6_2atmpS1482;
  uint32_t _M0L6_2atmpS1481;
  uint32_t _M0L6_2atmpS1483;
  uint32_t _M0L6_2atmpS1484;
  uint32_t _M0L6_2atmpS1486;
  uint32_t _M0L6_2atmpS1485;
  uint32_t _M0L6_2atmpS1487;
  uint32_t _M0L6_2atmpS1488;
  uint32_t _M0L6_2atmpS1490;
  uint32_t _M0L6_2atmpS1489;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3671 = _M0L4selfS215->$0;
  moonbit_decref(_M0L4selfS215);
  _M0Lm3accS214 = _M0L8_2afieldS3671;
  _M0L6_2atmpS1480 = _M0Lm3accS214;
  _M0L6_2atmpS1482 = _M0Lm3accS214;
  _M0L6_2atmpS1481 = _M0L6_2atmpS1482 >> 15;
  _M0Lm3accS214 = _M0L6_2atmpS1480 ^ _M0L6_2atmpS1481;
  _M0L6_2atmpS1483 = _M0Lm3accS214;
  _M0Lm3accS214 = _M0L6_2atmpS1483 * 2246822519u;
  _M0L6_2atmpS1484 = _M0Lm3accS214;
  _M0L6_2atmpS1486 = _M0Lm3accS214;
  _M0L6_2atmpS1485 = _M0L6_2atmpS1486 >> 13;
  _M0Lm3accS214 = _M0L6_2atmpS1484 ^ _M0L6_2atmpS1485;
  _M0L6_2atmpS1487 = _M0Lm3accS214;
  _M0Lm3accS214 = _M0L6_2atmpS1487 * 3266489917u;
  _M0L6_2atmpS1488 = _M0Lm3accS214;
  _M0L6_2atmpS1490 = _M0Lm3accS214;
  _M0L6_2atmpS1489 = _M0L6_2atmpS1490 >> 16;
  _M0Lm3accS214 = _M0L6_2atmpS1488 ^ _M0L6_2atmpS1489;
  return _M0Lm3accS214;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS212,
  moonbit_string_t _M0L1yS213
) {
  int32_t _M0L6_2atmpS3672;
  int32_t _M0L6_2atmpS1479;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3672 = moonbit_val_array_equal(_M0L1xS212, _M0L1yS213);
  moonbit_decref(_M0L1xS212);
  moonbit_decref(_M0L1yS213);
  _M0L6_2atmpS1479 = _M0L6_2atmpS3672;
  return !_M0L6_2atmpS1479;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS209,
  int32_t _M0L5valueS208
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS208, _M0L4selfS209);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS211,
  moonbit_string_t _M0L5valueS210
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS210, _M0L4selfS211);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS207) {
  int64_t _M0L6_2atmpS1478;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1478 = (int64_t)_M0L4selfS207;
  return *(uint64_t*)&_M0L6_2atmpS1478;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS205,
  moonbit_string_t _M0L4reprS206
) {
  void* _block_4069;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4069 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_4069)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_4069)->$0 = _M0L6numberS205;
  ((struct _M0DTPB4Json6Number*)_block_4069)->$1 = _M0L4reprS206;
  return _block_4069;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS203,
  int32_t _M0L5valueS204
) {
  uint32_t _M0L6_2atmpS1477;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1477 = *(uint32_t*)&_M0L5valueS204;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS203, _M0L6_2atmpS1477);
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
      int32_t _if__result_4071;
      moonbit_string_t* _M0L8_2afieldS3674;
      moonbit_string_t* _M0L3bufS1475;
      moonbit_string_t _M0L6_2atmpS3673;
      moonbit_string_t _M0L4itemS199;
      int32_t _M0L6_2atmpS1476;
      if (_M0L1iS198 != 0) {
        moonbit_incref(_M0L3bufS194);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS194, (moonbit_string_t)moonbit_string_literal_249.data);
      }
      if (_M0L1iS198 < 0) {
        _if__result_4071 = 1;
      } else {
        int32_t _M0L3lenS1474 = _M0L7_2aselfS195->$1;
        _if__result_4071 = _M0L1iS198 >= _M0L3lenS1474;
      }
      if (_if__result_4071) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3674 = _M0L7_2aselfS195->$0;
      _M0L3bufS1475 = _M0L8_2afieldS3674;
      _M0L6_2atmpS3673 = (moonbit_string_t)_M0L3bufS1475[_M0L1iS198];
      _M0L4itemS199 = _M0L6_2atmpS3673;
      if (_M0L4itemS199 == 0) {
        moonbit_incref(_M0L3bufS194);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS194, (moonbit_string_t)moonbit_string_literal_210.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS200 = _M0L4itemS199;
        moonbit_string_t _M0L6_2alocS201 = _M0L7_2aSomeS200;
        moonbit_string_t _M0L6_2atmpS1473;
        moonbit_incref(_M0L6_2alocS201);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1473
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS201);
        moonbit_incref(_M0L3bufS194);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS194, _M0L6_2atmpS1473);
      }
      _M0L6_2atmpS1476 = _M0L1iS198 + 1;
      _M0L1iS198 = _M0L6_2atmpS1476;
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
  moonbit_string_t _M0L6_2atmpS1472;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1471;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1472 = _M0L4selfS193;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1471 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1472);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1471);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS192
) {
  struct _M0TPB13StringBuilder* _M0L2sbS191;
  struct _M0TPC16string10StringView _M0L8_2afieldS3687;
  struct _M0TPC16string10StringView _M0L3pkgS1456;
  moonbit_string_t _M0L6_2atmpS1455;
  moonbit_string_t _M0L6_2atmpS3686;
  moonbit_string_t _M0L6_2atmpS1454;
  moonbit_string_t _M0L6_2atmpS3685;
  moonbit_string_t _M0L6_2atmpS1453;
  struct _M0TPC16string10StringView _M0L8_2afieldS3684;
  struct _M0TPC16string10StringView _M0L8filenameS1457;
  struct _M0TPC16string10StringView _M0L8_2afieldS3683;
  struct _M0TPC16string10StringView _M0L11start__lineS1460;
  moonbit_string_t _M0L6_2atmpS1459;
  moonbit_string_t _M0L6_2atmpS3682;
  moonbit_string_t _M0L6_2atmpS1458;
  struct _M0TPC16string10StringView _M0L8_2afieldS3681;
  struct _M0TPC16string10StringView _M0L13start__columnS1463;
  moonbit_string_t _M0L6_2atmpS1462;
  moonbit_string_t _M0L6_2atmpS3680;
  moonbit_string_t _M0L6_2atmpS1461;
  struct _M0TPC16string10StringView _M0L8_2afieldS3679;
  struct _M0TPC16string10StringView _M0L9end__lineS1466;
  moonbit_string_t _M0L6_2atmpS1465;
  moonbit_string_t _M0L6_2atmpS3678;
  moonbit_string_t _M0L6_2atmpS1464;
  struct _M0TPC16string10StringView _M0L8_2afieldS3677;
  int32_t _M0L6_2acntS3796;
  struct _M0TPC16string10StringView _M0L11end__columnS1470;
  moonbit_string_t _M0L6_2atmpS1469;
  moonbit_string_t _M0L6_2atmpS3676;
  moonbit_string_t _M0L6_2atmpS1468;
  moonbit_string_t _M0L6_2atmpS3675;
  moonbit_string_t _M0L6_2atmpS1467;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS191 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3687
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$0_1, _M0L4selfS192->$0_2, _M0L4selfS192->$0_0
  };
  _M0L3pkgS1456 = _M0L8_2afieldS3687;
  moonbit_incref(_M0L3pkgS1456.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1455
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1456);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3686
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_250.data, _M0L6_2atmpS1455);
  moonbit_decref(_M0L6_2atmpS1455);
  _M0L6_2atmpS1454 = _M0L6_2atmpS3686;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3685
  = moonbit_add_string(_M0L6_2atmpS1454, (moonbit_string_t)moonbit_string_literal_251.data);
  moonbit_decref(_M0L6_2atmpS1454);
  _M0L6_2atmpS1453 = _M0L6_2atmpS3685;
  moonbit_incref(_M0L2sbS191);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1453);
  moonbit_incref(_M0L2sbS191);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, (moonbit_string_t)moonbit_string_literal_252.data);
  _M0L8_2afieldS3684
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$1_1, _M0L4selfS192->$1_2, _M0L4selfS192->$1_0
  };
  _M0L8filenameS1457 = _M0L8_2afieldS3684;
  moonbit_incref(_M0L8filenameS1457.$0);
  moonbit_incref(_M0L2sbS191);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS191, _M0L8filenameS1457);
  _M0L8_2afieldS3683
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$2_1, _M0L4selfS192->$2_2, _M0L4selfS192->$2_0
  };
  _M0L11start__lineS1460 = _M0L8_2afieldS3683;
  moonbit_incref(_M0L11start__lineS1460.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1459
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1460);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3682
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_253.data, _M0L6_2atmpS1459);
  moonbit_decref(_M0L6_2atmpS1459);
  _M0L6_2atmpS1458 = _M0L6_2atmpS3682;
  moonbit_incref(_M0L2sbS191);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1458);
  _M0L8_2afieldS3681
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$3_1, _M0L4selfS192->$3_2, _M0L4selfS192->$3_0
  };
  _M0L13start__columnS1463 = _M0L8_2afieldS3681;
  moonbit_incref(_M0L13start__columnS1463.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1462
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1463);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3680
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_254.data, _M0L6_2atmpS1462);
  moonbit_decref(_M0L6_2atmpS1462);
  _M0L6_2atmpS1461 = _M0L6_2atmpS3680;
  moonbit_incref(_M0L2sbS191);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1461);
  _M0L8_2afieldS3679
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$4_1, _M0L4selfS192->$4_2, _M0L4selfS192->$4_0
  };
  _M0L9end__lineS1466 = _M0L8_2afieldS3679;
  moonbit_incref(_M0L9end__lineS1466.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1465
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1466);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3678
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_255.data, _M0L6_2atmpS1465);
  moonbit_decref(_M0L6_2atmpS1465);
  _M0L6_2atmpS1464 = _M0L6_2atmpS3678;
  moonbit_incref(_M0L2sbS191);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1464);
  _M0L8_2afieldS3677
  = (struct _M0TPC16string10StringView){
    _M0L4selfS192->$5_1, _M0L4selfS192->$5_2, _M0L4selfS192->$5_0
  };
  _M0L6_2acntS3796 = Moonbit_object_header(_M0L4selfS192)->rc;
  if (_M0L6_2acntS3796 > 1) {
    int32_t _M0L11_2anew__cntS3802 = _M0L6_2acntS3796 - 1;
    Moonbit_object_header(_M0L4selfS192)->rc = _M0L11_2anew__cntS3802;
    moonbit_incref(_M0L8_2afieldS3677.$0);
  } else if (_M0L6_2acntS3796 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3801 =
      (struct _M0TPC16string10StringView){_M0L4selfS192->$4_1,
                                            _M0L4selfS192->$4_2,
                                            _M0L4selfS192->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3800;
    struct _M0TPC16string10StringView _M0L8_2afieldS3799;
    struct _M0TPC16string10StringView _M0L8_2afieldS3798;
    struct _M0TPC16string10StringView _M0L8_2afieldS3797;
    moonbit_decref(_M0L8_2afieldS3801.$0);
    _M0L8_2afieldS3800
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$3_1, _M0L4selfS192->$3_2, _M0L4selfS192->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3800.$0);
    _M0L8_2afieldS3799
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$2_1, _M0L4selfS192->$2_2, _M0L4selfS192->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3799.$0);
    _M0L8_2afieldS3798
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$1_1, _M0L4selfS192->$1_2, _M0L4selfS192->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3798.$0);
    _M0L8_2afieldS3797
    = (struct _M0TPC16string10StringView){
      _M0L4selfS192->$0_1, _M0L4selfS192->$0_2, _M0L4selfS192->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3797.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS192);
  }
  _M0L11end__columnS1470 = _M0L8_2afieldS3677;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1469
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1470);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3676
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_256.data, _M0L6_2atmpS1469);
  moonbit_decref(_M0L6_2atmpS1469);
  _M0L6_2atmpS1468 = _M0L6_2atmpS3676;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3675
  = moonbit_add_string(_M0L6_2atmpS1468, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1468);
  _M0L6_2atmpS1467 = _M0L6_2atmpS3675;
  moonbit_incref(_M0L2sbS191);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS191, _M0L6_2atmpS1467);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS191);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS189,
  moonbit_string_t _M0L3strS190
) {
  int32_t _M0L3lenS1443;
  int32_t _M0L6_2atmpS1445;
  int32_t _M0L6_2atmpS1444;
  int32_t _M0L6_2atmpS1442;
  moonbit_bytes_t _M0L8_2afieldS3689;
  moonbit_bytes_t _M0L4dataS1446;
  int32_t _M0L3lenS1447;
  int32_t _M0L6_2atmpS1448;
  int32_t _M0L3lenS1450;
  int32_t _M0L6_2atmpS3688;
  int32_t _M0L6_2atmpS1452;
  int32_t _M0L6_2atmpS1451;
  int32_t _M0L6_2atmpS1449;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1443 = _M0L4selfS189->$1;
  _M0L6_2atmpS1445 = Moonbit_array_length(_M0L3strS190);
  _M0L6_2atmpS1444 = _M0L6_2atmpS1445 * 2;
  _M0L6_2atmpS1442 = _M0L3lenS1443 + _M0L6_2atmpS1444;
  moonbit_incref(_M0L4selfS189);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS189, _M0L6_2atmpS1442);
  _M0L8_2afieldS3689 = _M0L4selfS189->$0;
  _M0L4dataS1446 = _M0L8_2afieldS3689;
  _M0L3lenS1447 = _M0L4selfS189->$1;
  _M0L6_2atmpS1448 = Moonbit_array_length(_M0L3strS190);
  moonbit_incref(_M0L4dataS1446);
  moonbit_incref(_M0L3strS190);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1446, _M0L3lenS1447, _M0L3strS190, 0, _M0L6_2atmpS1448);
  _M0L3lenS1450 = _M0L4selfS189->$1;
  _M0L6_2atmpS3688 = Moonbit_array_length(_M0L3strS190);
  moonbit_decref(_M0L3strS190);
  _M0L6_2atmpS1452 = _M0L6_2atmpS3688;
  _M0L6_2atmpS1451 = _M0L6_2atmpS1452 * 2;
  _M0L6_2atmpS1449 = _M0L3lenS1450 + _M0L6_2atmpS1451;
  _M0L4selfS189->$1 = _M0L6_2atmpS1449;
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
  int32_t _M0L6_2atmpS1441;
  int32_t _M0L6_2atmpS1440;
  int32_t _M0L2e1S175;
  int32_t _M0L6_2atmpS1439;
  int32_t _M0L2e2S178;
  int32_t _M0L4len1S180;
  int32_t _M0L4len2S182;
  int32_t _if__result_4072;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1441 = _M0L6lengthS177 * 2;
  _M0L6_2atmpS1440 = _M0L13bytes__offsetS176 + _M0L6_2atmpS1441;
  _M0L2e1S175 = _M0L6_2atmpS1440 - 1;
  _M0L6_2atmpS1439 = _M0L11str__offsetS179 + _M0L6lengthS177;
  _M0L2e2S178 = _M0L6_2atmpS1439 - 1;
  _M0L4len1S180 = Moonbit_array_length(_M0L4selfS181);
  _M0L4len2S182 = Moonbit_array_length(_M0L3strS183);
  if (_M0L6lengthS177 >= 0) {
    if (_M0L13bytes__offsetS176 >= 0) {
      if (_M0L2e1S175 < _M0L4len1S180) {
        if (_M0L11str__offsetS179 >= 0) {
          _if__result_4072 = _M0L2e2S178 < _M0L4len2S182;
        } else {
          _if__result_4072 = 0;
        }
      } else {
        _if__result_4072 = 0;
      }
    } else {
      _if__result_4072 = 0;
    }
  } else {
    _if__result_4072 = 0;
  }
  if (_if__result_4072) {
    int32_t _M0L16end__str__offsetS184 =
      _M0L11str__offsetS179 + _M0L6lengthS177;
    int32_t _M0L1iS185 = _M0L11str__offsetS179;
    int32_t _M0L1jS186 = _M0L13bytes__offsetS176;
    while (1) {
      if (_M0L1iS185 < _M0L16end__str__offsetS184) {
        int32_t _M0L6_2atmpS1436 = _M0L3strS183[_M0L1iS185];
        int32_t _M0L6_2atmpS1435 = (int32_t)_M0L6_2atmpS1436;
        uint32_t _M0L1cS187 = *(uint32_t*)&_M0L6_2atmpS1435;
        uint32_t _M0L6_2atmpS1431 = _M0L1cS187 & 255u;
        int32_t _M0L6_2atmpS1430;
        int32_t _M0L6_2atmpS1432;
        uint32_t _M0L6_2atmpS1434;
        int32_t _M0L6_2atmpS1433;
        int32_t _M0L6_2atmpS1437;
        int32_t _M0L6_2atmpS1438;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1430 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1431);
        if (
          _M0L1jS186 < 0 || _M0L1jS186 >= Moonbit_array_length(_M0L4selfS181)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS181[_M0L1jS186] = _M0L6_2atmpS1430;
        _M0L6_2atmpS1432 = _M0L1jS186 + 1;
        _M0L6_2atmpS1434 = _M0L1cS187 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1433 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1434);
        if (
          _M0L6_2atmpS1432 < 0
          || _M0L6_2atmpS1432 >= Moonbit_array_length(_M0L4selfS181)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS181[_M0L6_2atmpS1432] = _M0L6_2atmpS1433;
        _M0L6_2atmpS1437 = _M0L1iS185 + 1;
        _M0L6_2atmpS1438 = _M0L1jS186 + 2;
        _M0L1iS185 = _M0L6_2atmpS1437;
        _M0L1jS186 = _M0L6_2atmpS1438;
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
  struct _M0TPB6Logger _M0L6_2atmpS1428;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1428
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS172
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS171, _M0L6_2atmpS1428);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS174,
  struct _M0TPC16string10StringView _M0L3objS173
) {
  struct _M0TPB6Logger _M0L6_2atmpS1429;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1429
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS174
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS173, _M0L6_2atmpS1429);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS117
) {
  int32_t _M0L6_2atmpS1427;
  struct _M0TPC16string10StringView _M0L7_2abindS116;
  moonbit_string_t _M0L7_2adataS118;
  int32_t _M0L8_2astartS119;
  int32_t _M0L6_2atmpS1426;
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
  int32_t _M0L6_2atmpS1384;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1427 = Moonbit_array_length(_M0L4reprS117);
  _M0L7_2abindS116
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1427, _M0L4reprS117
  };
  moonbit_incref(_M0L7_2abindS116.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS118 = _M0MPC16string10StringView4data(_M0L7_2abindS116);
  moonbit_incref(_M0L7_2abindS116.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS119
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS116);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1426 = _M0MPC16string10StringView6length(_M0L7_2abindS116);
  _M0L6_2aendS120 = _M0L8_2astartS119 + _M0L6_2atmpS1426;
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
  _M0L6_2atmpS1384 = _M0Lm9_2acursorS121;
  if (_M0L6_2atmpS1384 < _M0L6_2aendS120) {
    int32_t _M0L6_2atmpS1386 = _M0Lm9_2acursorS121;
    int32_t _M0L6_2atmpS1385;
    moonbit_incref(_M0L7_2adataS118);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1385
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1386);
    if (_M0L6_2atmpS1385 == 64) {
      int32_t _M0L6_2atmpS1387 = _M0Lm9_2acursorS121;
      _M0Lm9_2acursorS121 = _M0L6_2atmpS1387 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1388;
        _M0Lm6tag__0S129 = _M0Lm9_2acursorS121;
        _M0L6_2atmpS1388 = _M0Lm9_2acursorS121;
        if (_M0L6_2atmpS1388 < _M0L6_2aendS120) {
          int32_t _M0L6_2atmpS1425 = _M0Lm9_2acursorS121;
          int32_t _M0L10next__charS144;
          int32_t _M0L6_2atmpS1389;
          moonbit_incref(_M0L7_2adataS118);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS144
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1425);
          _M0L6_2atmpS1389 = _M0Lm9_2acursorS121;
          _M0Lm9_2acursorS121 = _M0L6_2atmpS1389 + 1;
          if (_M0L10next__charS144 == 58) {
            int32_t _M0L6_2atmpS1390 = _M0Lm9_2acursorS121;
            if (_M0L6_2atmpS1390 < _M0L6_2aendS120) {
              int32_t _M0L6_2atmpS1391 = _M0Lm9_2acursorS121;
              int32_t _M0L12dispatch__15S145;
              _M0Lm9_2acursorS121 = _M0L6_2atmpS1391 + 1;
              _M0L12dispatch__15S145 = 0;
              loop__label__15_148:;
              while (1) {
                int32_t _M0L6_2atmpS1392;
                switch (_M0L12dispatch__15S145) {
                  case 3: {
                    int32_t _M0L6_2atmpS1395;
                    _M0Lm9tag__1__2S132 = _M0Lm9tag__1__1S131;
                    _M0Lm9tag__1__1S131 = _M0Lm6tag__1S130;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1395 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1395 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1400 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS152;
                      int32_t _M0L6_2atmpS1396;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS152
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1400);
                      _M0L6_2atmpS1396 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1396 + 1;
                      if (_M0L10next__charS152 < 58) {
                        if (_M0L10next__charS152 < 48) {
                          goto join_151;
                        } else {
                          int32_t _M0L6_2atmpS1397;
                          _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                          _M0Lm9tag__2__1S135 = _M0Lm6tag__2S134;
                          _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                          _M0Lm6tag__3S133 = _M0Lm9_2acursorS121;
                          _M0L6_2atmpS1397 = _M0Lm9_2acursorS121;
                          if (_M0L6_2atmpS1397 < _M0L6_2aendS120) {
                            int32_t _M0L6_2atmpS1399 = _M0Lm9_2acursorS121;
                            int32_t _M0L10next__charS154;
                            int32_t _M0L6_2atmpS1398;
                            moonbit_incref(_M0L7_2adataS118);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS154
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1399);
                            _M0L6_2atmpS1398 = _M0Lm9_2acursorS121;
                            _M0Lm9_2acursorS121 = _M0L6_2atmpS1398 + 1;
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
                    int32_t _M0L6_2atmpS1401;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1401 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1401 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1403 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS156;
                      int32_t _M0L6_2atmpS1402;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS156
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1403);
                      _M0L6_2atmpS1402 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1402 + 1;
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
                    int32_t _M0L6_2atmpS1404;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1404 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1404 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1406 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS157;
                      int32_t _M0L6_2atmpS1405;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS157
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1406);
                      _M0L6_2atmpS1405 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1405 + 1;
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
                    int32_t _M0L6_2atmpS1407;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__4S136 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1407 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1407 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1415 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS159;
                      int32_t _M0L6_2atmpS1408;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS159
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1415);
                      _M0L6_2atmpS1408 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1408 + 1;
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
                        int32_t _M0L6_2atmpS1409;
                        _M0Lm9tag__1__2S132 = _M0Lm9tag__1__1S131;
                        _M0Lm9tag__1__1S131 = _M0Lm6tag__1S130;
                        _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                        _M0L6_2atmpS1409 = _M0Lm9_2acursorS121;
                        if (_M0L6_2atmpS1409 < _M0L6_2aendS120) {
                          int32_t _M0L6_2atmpS1414 = _M0Lm9_2acursorS121;
                          int32_t _M0L10next__charS161;
                          int32_t _M0L6_2atmpS1410;
                          moonbit_incref(_M0L7_2adataS118);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS161
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1414);
                          _M0L6_2atmpS1410 = _M0Lm9_2acursorS121;
                          _M0Lm9_2acursorS121 = _M0L6_2atmpS1410 + 1;
                          if (_M0L10next__charS161 < 58) {
                            if (_M0L10next__charS161 < 48) {
                              goto join_160;
                            } else {
                              int32_t _M0L6_2atmpS1411;
                              _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                              _M0Lm9tag__2__1S135 = _M0Lm6tag__2S134;
                              _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                              _M0L6_2atmpS1411 = _M0Lm9_2acursorS121;
                              if (_M0L6_2atmpS1411 < _M0L6_2aendS120) {
                                int32_t _M0L6_2atmpS1413 =
                                  _M0Lm9_2acursorS121;
                                int32_t _M0L10next__charS163;
                                int32_t _M0L6_2atmpS1412;
                                moonbit_incref(_M0L7_2adataS118);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS163
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1413);
                                _M0L6_2atmpS1412 = _M0Lm9_2acursorS121;
                                _M0Lm9_2acursorS121 = _M0L6_2atmpS1412 + 1;
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
                    int32_t _M0L6_2atmpS1416;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1416 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1416 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1418 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS165;
                      int32_t _M0L6_2atmpS1417;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS165
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1418);
                      _M0L6_2atmpS1417 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1417 + 1;
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
                    int32_t _M0L6_2atmpS1419;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__2S134 = _M0Lm9_2acursorS121;
                    _M0Lm6tag__3S133 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1419 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1419 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1421 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS167;
                      int32_t _M0L6_2atmpS1420;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS167
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1421);
                      _M0L6_2atmpS1420 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1420 + 1;
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
                    int32_t _M0L6_2atmpS1422;
                    _M0Lm9tag__1__1S131 = _M0Lm6tag__1S130;
                    _M0Lm6tag__1S130 = _M0Lm9_2acursorS121;
                    _M0L6_2atmpS1422 = _M0Lm9_2acursorS121;
                    if (_M0L6_2atmpS1422 < _M0L6_2aendS120) {
                      int32_t _M0L6_2atmpS1424 = _M0Lm9_2acursorS121;
                      int32_t _M0L10next__charS169;
                      int32_t _M0L6_2atmpS1423;
                      moonbit_incref(_M0L7_2adataS118);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS169
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1424);
                      _M0L6_2atmpS1423 = _M0Lm9_2acursorS121;
                      _M0Lm9_2acursorS121 = _M0L6_2atmpS1423 + 1;
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
                _M0L6_2atmpS1392 = _M0Lm9_2acursorS121;
                if (_M0L6_2atmpS1392 < _M0L6_2aendS120) {
                  int32_t _M0L6_2atmpS1394 = _M0Lm9_2acursorS121;
                  int32_t _M0L10next__charS149;
                  int32_t _M0L6_2atmpS1393;
                  moonbit_incref(_M0L7_2adataS118);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS149
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS118, _M0L6_2atmpS1394);
                  _M0L6_2atmpS1393 = _M0Lm9_2acursorS121;
                  _M0Lm9_2acursorS121 = _M0L6_2atmpS1393 + 1;
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
      int32_t _M0L6_2atmpS1383 = _M0Lm20match__tag__saver__1S125;
      int32_t _M0L6_2atmpS1382 = _M0L6_2atmpS1383 + 1;
      int64_t _M0L6_2atmpS1379 = (int64_t)_M0L6_2atmpS1382;
      int32_t _M0L6_2atmpS1381 = _M0Lm20match__tag__saver__2S126;
      int64_t _M0L6_2atmpS1380 = (int64_t)_M0L6_2atmpS1381;
      struct _M0TPC16string10StringView _M0L11start__lineS138;
      int32_t _M0L6_2atmpS1378;
      int32_t _M0L6_2atmpS1377;
      int64_t _M0L6_2atmpS1374;
      int32_t _M0L6_2atmpS1376;
      int64_t _M0L6_2atmpS1375;
      struct _M0TPC16string10StringView _M0L13start__columnS139;
      int32_t _M0L6_2atmpS1373;
      int64_t _M0L6_2atmpS1370;
      int32_t _M0L6_2atmpS1372;
      int64_t _M0L6_2atmpS1371;
      struct _M0TPC16string10StringView _M0L3pkgS140;
      int32_t _M0L6_2atmpS1369;
      int32_t _M0L6_2atmpS1368;
      int64_t _M0L6_2atmpS1365;
      int32_t _M0L6_2atmpS1367;
      int64_t _M0L6_2atmpS1366;
      struct _M0TPC16string10StringView _M0L8filenameS141;
      int32_t _M0L6_2atmpS1364;
      int32_t _M0L6_2atmpS1363;
      int64_t _M0L6_2atmpS1360;
      int32_t _M0L6_2atmpS1362;
      int64_t _M0L6_2atmpS1361;
      struct _M0TPC16string10StringView _M0L9end__lineS142;
      int32_t _M0L6_2atmpS1359;
      int32_t _M0L6_2atmpS1358;
      int64_t _M0L6_2atmpS1355;
      int32_t _M0L6_2atmpS1357;
      int64_t _M0L6_2atmpS1356;
      struct _M0TPC16string10StringView _M0L11end__columnS143;
      struct _M0TPB13SourceLocRepr* _block_4089;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS138
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1379, _M0L6_2atmpS1380);
      _M0L6_2atmpS1378 = _M0Lm20match__tag__saver__2S126;
      _M0L6_2atmpS1377 = _M0L6_2atmpS1378 + 1;
      _M0L6_2atmpS1374 = (int64_t)_M0L6_2atmpS1377;
      _M0L6_2atmpS1376 = _M0Lm20match__tag__saver__3S127;
      _M0L6_2atmpS1375 = (int64_t)_M0L6_2atmpS1376;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS139
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1374, _M0L6_2atmpS1375);
      _M0L6_2atmpS1373 = _M0L8_2astartS119 + 1;
      _M0L6_2atmpS1370 = (int64_t)_M0L6_2atmpS1373;
      _M0L6_2atmpS1372 = _M0Lm20match__tag__saver__0S124;
      _M0L6_2atmpS1371 = (int64_t)_M0L6_2atmpS1372;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS140
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1370, _M0L6_2atmpS1371);
      _M0L6_2atmpS1369 = _M0Lm20match__tag__saver__0S124;
      _M0L6_2atmpS1368 = _M0L6_2atmpS1369 + 1;
      _M0L6_2atmpS1365 = (int64_t)_M0L6_2atmpS1368;
      _M0L6_2atmpS1367 = _M0Lm20match__tag__saver__1S125;
      _M0L6_2atmpS1366 = (int64_t)_M0L6_2atmpS1367;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS141
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1365, _M0L6_2atmpS1366);
      _M0L6_2atmpS1364 = _M0Lm20match__tag__saver__3S127;
      _M0L6_2atmpS1363 = _M0L6_2atmpS1364 + 1;
      _M0L6_2atmpS1360 = (int64_t)_M0L6_2atmpS1363;
      _M0L6_2atmpS1362 = _M0Lm20match__tag__saver__4S128;
      _M0L6_2atmpS1361 = (int64_t)_M0L6_2atmpS1362;
      moonbit_incref(_M0L7_2adataS118);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS142
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1360, _M0L6_2atmpS1361);
      _M0L6_2atmpS1359 = _M0Lm20match__tag__saver__4S128;
      _M0L6_2atmpS1358 = _M0L6_2atmpS1359 + 1;
      _M0L6_2atmpS1355 = (int64_t)_M0L6_2atmpS1358;
      _M0L6_2atmpS1357 = _M0Lm10match__endS123;
      _M0L6_2atmpS1356 = (int64_t)_M0L6_2atmpS1357;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS143
      = _M0MPC16string6String4view(_M0L7_2adataS118, _M0L6_2atmpS1355, _M0L6_2atmpS1356);
      _block_4089
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4089)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4089->$0_0 = _M0L3pkgS140.$0;
      _block_4089->$0_1 = _M0L3pkgS140.$1;
      _block_4089->$0_2 = _M0L3pkgS140.$2;
      _block_4089->$1_0 = _M0L8filenameS141.$0;
      _block_4089->$1_1 = _M0L8filenameS141.$1;
      _block_4089->$1_2 = _M0L8filenameS141.$2;
      _block_4089->$2_0 = _M0L11start__lineS138.$0;
      _block_4089->$2_1 = _M0L11start__lineS138.$1;
      _block_4089->$2_2 = _M0L11start__lineS138.$2;
      _block_4089->$3_0 = _M0L13start__columnS139.$0;
      _block_4089->$3_1 = _M0L13start__columnS139.$1;
      _block_4089->$3_2 = _M0L13start__columnS139.$2;
      _block_4089->$4_0 = _M0L9end__lineS142.$0;
      _block_4089->$4_1 = _M0L9end__lineS142.$1;
      _block_4089->$4_2 = _M0L9end__lineS142.$2;
      _block_4089->$5_0 = _M0L11end__columnS143.$0;
      _block_4089->$5_1 = _M0L11end__columnS143.$1;
      _block_4089->$5_2 = _M0L11end__columnS143.$2;
      return _block_4089;
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
  int32_t _if__result_4090;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS113 = _M0L4selfS114->$1;
  if (_M0L5indexS115 >= 0) {
    _if__result_4090 = _M0L5indexS115 < _M0L3lenS113;
  } else {
    _if__result_4090 = 0;
  }
  if (_if__result_4090) {
    moonbit_string_t* _M0L6_2atmpS1354;
    moonbit_string_t _M0L6_2atmpS3690;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1354 = _M0MPC15array5Array6bufferGsE(_M0L4selfS114);
    if (
      _M0L5indexS115 < 0
      || _M0L5indexS115 >= Moonbit_array_length(_M0L6_2atmpS1354)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3690 = (moonbit_string_t)_M0L6_2atmpS1354[_M0L5indexS115];
    moonbit_incref(_M0L6_2atmpS3690);
    moonbit_decref(_M0L6_2atmpS1354);
    return _M0L6_2atmpS3690;
  } else {
    moonbit_decref(_M0L4selfS114);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS110
) {
  moonbit_string_t* _M0L8_2afieldS3691;
  int32_t _M0L6_2acntS3803;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3691 = _M0L4selfS110->$0;
  _M0L6_2acntS3803 = Moonbit_object_header(_M0L4selfS110)->rc;
  if (_M0L6_2acntS3803 > 1) {
    int32_t _M0L11_2anew__cntS3804 = _M0L6_2acntS3803 - 1;
    Moonbit_object_header(_M0L4selfS110)->rc = _M0L11_2anew__cntS3804;
    moonbit_incref(_M0L8_2afieldS3691);
  } else if (_M0L6_2acntS3803 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS110);
  }
  return _M0L8_2afieldS3691;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS111
) {
  struct _M0TUsiE** _M0L8_2afieldS3692;
  int32_t _M0L6_2acntS3805;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3692 = _M0L4selfS111->$0;
  _M0L6_2acntS3805 = Moonbit_object_header(_M0L4selfS111)->rc;
  if (_M0L6_2acntS3805 > 1) {
    int32_t _M0L11_2anew__cntS3806 = _M0L6_2acntS3805 - 1;
    Moonbit_object_header(_M0L4selfS111)->rc = _M0L11_2anew__cntS3806;
    moonbit_incref(_M0L8_2afieldS3692);
  } else if (_M0L6_2acntS3805 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS111);
  }
  return _M0L8_2afieldS3692;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS112
) {
  void** _M0L8_2afieldS3693;
  int32_t _M0L6_2acntS3807;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3693 = _M0L4selfS112->$0;
  _M0L6_2acntS3807 = Moonbit_object_header(_M0L4selfS112)->rc;
  if (_M0L6_2acntS3807 > 1) {
    int32_t _M0L11_2anew__cntS3808 = _M0L6_2acntS3807 - 1;
    Moonbit_object_header(_M0L4selfS112)->rc = _M0L11_2anew__cntS3808;
    moonbit_incref(_M0L8_2afieldS3693);
  } else if (_M0L6_2acntS3807 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS112);
  }
  return _M0L8_2afieldS3693;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS109) {
  struct _M0TPB13StringBuilder* _M0L3bufS108;
  struct _M0TPB6Logger _M0L6_2atmpS1353;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS108 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS108);
  _M0L6_2atmpS1353
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS108
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS109, _M0L6_2atmpS1353);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS108);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS107) {
  int32_t _M0L6_2atmpS1352;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1352 = (int32_t)_M0L4selfS107;
  return _M0L6_2atmpS1352;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS105,
  int32_t _M0L8trailingS106
) {
  int32_t _M0L6_2atmpS1351;
  int32_t _M0L6_2atmpS1350;
  int32_t _M0L6_2atmpS1349;
  int32_t _M0L6_2atmpS1348;
  int32_t _M0L6_2atmpS1347;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1351 = _M0L7leadingS105 - 55296;
  _M0L6_2atmpS1350 = _M0L6_2atmpS1351 * 1024;
  _M0L6_2atmpS1349 = _M0L6_2atmpS1350 + _M0L8trailingS106;
  _M0L6_2atmpS1348 = _M0L6_2atmpS1349 - 56320;
  _M0L6_2atmpS1347 = _M0L6_2atmpS1348 + 65536;
  return _M0L6_2atmpS1347;
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
  int32_t _M0L3lenS1342;
  int32_t _M0L6_2atmpS1341;
  moonbit_bytes_t _M0L8_2afieldS3694;
  moonbit_bytes_t _M0L4dataS1345;
  int32_t _M0L3lenS1346;
  int32_t _M0L3incS101;
  int32_t _M0L3lenS1344;
  int32_t _M0L6_2atmpS1343;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1342 = _M0L4selfS100->$1;
  _M0L6_2atmpS1341 = _M0L3lenS1342 + 4;
  moonbit_incref(_M0L4selfS100);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS100, _M0L6_2atmpS1341);
  _M0L8_2afieldS3694 = _M0L4selfS100->$0;
  _M0L4dataS1345 = _M0L8_2afieldS3694;
  _M0L3lenS1346 = _M0L4selfS100->$1;
  moonbit_incref(_M0L4dataS1345);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS101
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1345, _M0L3lenS1346, _M0L2chS102);
  _M0L3lenS1344 = _M0L4selfS100->$1;
  _M0L6_2atmpS1343 = _M0L3lenS1344 + _M0L3incS101;
  _M0L4selfS100->$1 = _M0L6_2atmpS1343;
  moonbit_decref(_M0L4selfS100);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS95,
  int32_t _M0L8requiredS96
) {
  moonbit_bytes_t _M0L8_2afieldS3698;
  moonbit_bytes_t _M0L4dataS1340;
  int32_t _M0L6_2atmpS3697;
  int32_t _M0L12current__lenS94;
  int32_t _M0Lm13enough__spaceS97;
  int32_t _M0L6_2atmpS1338;
  int32_t _M0L6_2atmpS1339;
  moonbit_bytes_t _M0L9new__dataS99;
  moonbit_bytes_t _M0L8_2afieldS3696;
  moonbit_bytes_t _M0L4dataS1336;
  int32_t _M0L3lenS1337;
  moonbit_bytes_t _M0L6_2aoldS3695;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3698 = _M0L4selfS95->$0;
  _M0L4dataS1340 = _M0L8_2afieldS3698;
  _M0L6_2atmpS3697 = Moonbit_array_length(_M0L4dataS1340);
  _M0L12current__lenS94 = _M0L6_2atmpS3697;
  if (_M0L8requiredS96 <= _M0L12current__lenS94) {
    moonbit_decref(_M0L4selfS95);
    return 0;
  }
  _M0Lm13enough__spaceS97 = _M0L12current__lenS94;
  while (1) {
    int32_t _M0L6_2atmpS1334 = _M0Lm13enough__spaceS97;
    if (_M0L6_2atmpS1334 < _M0L8requiredS96) {
      int32_t _M0L6_2atmpS1335 = _M0Lm13enough__spaceS97;
      _M0Lm13enough__spaceS97 = _M0L6_2atmpS1335 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1338 = _M0Lm13enough__spaceS97;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1339 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS99
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1338, _M0L6_2atmpS1339);
  _M0L8_2afieldS3696 = _M0L4selfS95->$0;
  _M0L4dataS1336 = _M0L8_2afieldS3696;
  _M0L3lenS1337 = _M0L4selfS95->$1;
  moonbit_incref(_M0L4dataS1336);
  moonbit_incref(_M0L9new__dataS99);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS99, 0, _M0L4dataS1336, 0, _M0L3lenS1337);
  _M0L6_2aoldS3695 = _M0L4selfS95->$0;
  moonbit_decref(_M0L6_2aoldS3695);
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
    uint32_t _M0L6_2atmpS1317 = _M0L4codeS87 & 255u;
    int32_t _M0L6_2atmpS1316;
    int32_t _M0L6_2atmpS1318;
    uint32_t _M0L6_2atmpS1320;
    int32_t _M0L6_2atmpS1319;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1316 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1317);
    if (
      _M0L6offsetS90 < 0
      || _M0L6offsetS90 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6offsetS90] = _M0L6_2atmpS1316;
    _M0L6_2atmpS1318 = _M0L6offsetS90 + 1;
    _M0L6_2atmpS1320 = _M0L4codeS87 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1319 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1320);
    if (
      _M0L6_2atmpS1318 < 0
      || _M0L6_2atmpS1318 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1318] = _M0L6_2atmpS1319;
    moonbit_decref(_M0L4selfS89);
    return 2;
  } else if (_M0L4codeS87 < 1114112u) {
    uint32_t _M0L2hiS91 = _M0L4codeS87 - 65536u;
    uint32_t _M0L6_2atmpS1333 = _M0L2hiS91 >> 10;
    uint32_t _M0L2loS92 = _M0L6_2atmpS1333 | 55296u;
    uint32_t _M0L6_2atmpS1332 = _M0L2hiS91 & 1023u;
    uint32_t _M0L2hiS93 = _M0L6_2atmpS1332 | 56320u;
    uint32_t _M0L6_2atmpS1322 = _M0L2loS92 & 255u;
    int32_t _M0L6_2atmpS1321;
    int32_t _M0L6_2atmpS1323;
    uint32_t _M0L6_2atmpS1325;
    int32_t _M0L6_2atmpS1324;
    int32_t _M0L6_2atmpS1326;
    uint32_t _M0L6_2atmpS1328;
    int32_t _M0L6_2atmpS1327;
    int32_t _M0L6_2atmpS1329;
    uint32_t _M0L6_2atmpS1331;
    int32_t _M0L6_2atmpS1330;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1321 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1322);
    if (
      _M0L6offsetS90 < 0
      || _M0L6offsetS90 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6offsetS90] = _M0L6_2atmpS1321;
    _M0L6_2atmpS1323 = _M0L6offsetS90 + 1;
    _M0L6_2atmpS1325 = _M0L2loS92 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1324 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1325);
    if (
      _M0L6_2atmpS1323 < 0
      || _M0L6_2atmpS1323 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1323] = _M0L6_2atmpS1324;
    _M0L6_2atmpS1326 = _M0L6offsetS90 + 2;
    _M0L6_2atmpS1328 = _M0L2hiS93 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1327 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1328);
    if (
      _M0L6_2atmpS1326 < 0
      || _M0L6_2atmpS1326 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1326] = _M0L6_2atmpS1327;
    _M0L6_2atmpS1329 = _M0L6offsetS90 + 3;
    _M0L6_2atmpS1331 = _M0L2hiS93 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1330 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1331);
    if (
      _M0L6_2atmpS1329 < 0
      || _M0L6_2atmpS1329 >= Moonbit_array_length(_M0L4selfS89)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS89[_M0L6_2atmpS1329] = _M0L6_2atmpS1330;
    moonbit_decref(_M0L4selfS89);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS89);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_257.data, (moonbit_string_t)moonbit_string_literal_258.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS86) {
  int32_t _M0L6_2atmpS1315;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1315 = *(int32_t*)&_M0L4selfS86;
  return _M0L6_2atmpS1315 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS85) {
  int32_t _M0L6_2atmpS1314;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1314 = _M0L4selfS85;
  return *(uint32_t*)&_M0L6_2atmpS1314;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS84
) {
  moonbit_bytes_t _M0L8_2afieldS3700;
  moonbit_bytes_t _M0L4dataS1313;
  moonbit_bytes_t _M0L6_2atmpS1310;
  int32_t _M0L8_2afieldS3699;
  int32_t _M0L3lenS1312;
  int64_t _M0L6_2atmpS1311;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3700 = _M0L4selfS84->$0;
  _M0L4dataS1313 = _M0L8_2afieldS3700;
  moonbit_incref(_M0L4dataS1313);
  _M0L6_2atmpS1310 = _M0L4dataS1313;
  _M0L8_2afieldS3699 = _M0L4selfS84->$1;
  moonbit_decref(_M0L4selfS84);
  _M0L3lenS1312 = _M0L8_2afieldS3699;
  _M0L6_2atmpS1311 = (int64_t)_M0L3lenS1312;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1310, 0, _M0L6_2atmpS1311);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS79,
  int32_t _M0L6offsetS83,
  int64_t _M0L6lengthS81
) {
  int32_t _M0L3lenS78;
  int32_t _M0L6lengthS80;
  int32_t _if__result_4092;
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
      int32_t _M0L6_2atmpS1309 = _M0L6offsetS83 + _M0L6lengthS80;
      _if__result_4092 = _M0L6_2atmpS1309 <= _M0L3lenS78;
    } else {
      _if__result_4092 = 0;
    }
  } else {
    _if__result_4092 = 0;
  }
  if (_if__result_4092) {
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
  struct _M0TPB13StringBuilder* _block_4093;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS76 < 1) {
    _M0L7initialS75 = 1;
  } else {
    _M0L7initialS75 = _M0L10size__hintS76;
  }
  _M0L4dataS77 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS75, 0);
  _block_4093
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4093)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4093->$0 = _M0L4dataS77;
  _block_4093->$1 = 0;
  return _block_4093;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS74) {
  int32_t _M0L6_2atmpS1308;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1308 = (int32_t)_M0L4selfS74;
  return _M0L6_2atmpS1308;
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
  int32_t _if__result_4094;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4094 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4094 = 0;
  }
  if (_if__result_4094) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS1272 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS1274 = _M0L11src__offsetS26 + _M0L1iS27;
        int32_t _M0L6_2atmpS1273;
        int32_t _M0L6_2atmpS1275;
        if (
          _M0L6_2atmpS1274 < 0
          || _M0L6_2atmpS1274 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1273 = (int32_t)_M0L3srcS24[_M0L6_2atmpS1274];
        if (
          _M0L6_2atmpS1272 < 0
          || _M0L6_2atmpS1272 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS23[_M0L6_2atmpS1272] = _M0L6_2atmpS1273;
        _M0L6_2atmpS1275 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS1275;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1280 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS1280;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS1276 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS1278 = _M0L11src__offsetS26 + _M0L1iS30;
        int32_t _M0L6_2atmpS1277;
        int32_t _M0L6_2atmpS1279;
        if (
          _M0L6_2atmpS1278 < 0
          || _M0L6_2atmpS1278 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1277 = (int32_t)_M0L3srcS24[_M0L6_2atmpS1278];
        if (
          _M0L6_2atmpS1276 < 0
          || _M0L6_2atmpS1276 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS23[_M0L6_2atmpS1276] = _M0L6_2atmpS1277;
        _M0L6_2atmpS1279 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS1279;
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
  int32_t _if__result_4097;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4097 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4097 = 0;
  }
  if (_if__result_4097) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS1281 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS1283 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS3702;
        moonbit_string_t _M0L6_2atmpS1282;
        moonbit_string_t _M0L6_2aoldS3701;
        int32_t _M0L6_2atmpS1284;
        if (
          _M0L6_2atmpS1283 < 0
          || _M0L6_2atmpS1283 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3702 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1283];
        _M0L6_2atmpS1282 = _M0L6_2atmpS3702;
        if (
          _M0L6_2atmpS1281 < 0
          || _M0L6_2atmpS1281 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3701 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1281];
        moonbit_incref(_M0L6_2atmpS1282);
        moonbit_decref(_M0L6_2aoldS3701);
        _M0L3dstS32[_M0L6_2atmpS1281] = _M0L6_2atmpS1282;
        _M0L6_2atmpS1284 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS1284;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1289 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS1289;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS1285 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS1287 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS3704;
        moonbit_string_t _M0L6_2atmpS1286;
        moonbit_string_t _M0L6_2aoldS3703;
        int32_t _M0L6_2atmpS1288;
        if (
          _M0L6_2atmpS1287 < 0
          || _M0L6_2atmpS1287 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3704 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS1287];
        _M0L6_2atmpS1286 = _M0L6_2atmpS3704;
        if (
          _M0L6_2atmpS1285 < 0
          || _M0L6_2atmpS1285 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3703 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS1285];
        moonbit_incref(_M0L6_2atmpS1286);
        moonbit_decref(_M0L6_2aoldS3703);
        _M0L3dstS32[_M0L6_2atmpS1285] = _M0L6_2atmpS1286;
        _M0L6_2atmpS1288 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS1288;
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
  int32_t _if__result_4100;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4100 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4100 = 0;
  }
  if (_if__result_4100) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS1290 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS1292 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsiE* _M0L6_2atmpS3706;
        struct _M0TUsiE* _M0L6_2atmpS1291;
        struct _M0TUsiE* _M0L6_2aoldS3705;
        int32_t _M0L6_2atmpS1293;
        if (
          _M0L6_2atmpS1292 < 0
          || _M0L6_2atmpS1292 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3706 = (struct _M0TUsiE*)_M0L3srcS42[_M0L6_2atmpS1292];
        _M0L6_2atmpS1291 = _M0L6_2atmpS3706;
        if (
          _M0L6_2atmpS1290 < 0
          || _M0L6_2atmpS1290 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3705 = (struct _M0TUsiE*)_M0L3dstS41[_M0L6_2atmpS1290];
        if (_M0L6_2atmpS1291) {
          moonbit_incref(_M0L6_2atmpS1291);
        }
        if (_M0L6_2aoldS3705) {
          moonbit_decref(_M0L6_2aoldS3705);
        }
        _M0L3dstS41[_M0L6_2atmpS1290] = _M0L6_2atmpS1291;
        _M0L6_2atmpS1293 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS1293;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1298 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS1298;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS1294 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS1296 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS3708;
        struct _M0TUsiE* _M0L6_2atmpS1295;
        struct _M0TUsiE* _M0L6_2aoldS3707;
        int32_t _M0L6_2atmpS1297;
        if (
          _M0L6_2atmpS1296 < 0
          || _M0L6_2atmpS1296 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3708 = (struct _M0TUsiE*)_M0L3srcS42[_M0L6_2atmpS1296];
        _M0L6_2atmpS1295 = _M0L6_2atmpS3708;
        if (
          _M0L6_2atmpS1294 < 0
          || _M0L6_2atmpS1294 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3707 = (struct _M0TUsiE*)_M0L3dstS41[_M0L6_2atmpS1294];
        if (_M0L6_2atmpS1295) {
          moonbit_incref(_M0L6_2atmpS1295);
        }
        if (_M0L6_2aoldS3707) {
          moonbit_decref(_M0L6_2aoldS3707);
        }
        _M0L3dstS41[_M0L6_2atmpS1294] = _M0L6_2atmpS1295;
        _M0L6_2atmpS1297 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS1297;
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
  int32_t _if__result_4103;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS50 == _M0L3srcS51) {
    _if__result_4103 = _M0L11dst__offsetS52 < _M0L11src__offsetS53;
  } else {
    _if__result_4103 = 0;
  }
  if (_if__result_4103) {
    int32_t _M0L1iS54 = 0;
    while (1) {
      if (_M0L1iS54 < _M0L3lenS55) {
        int32_t _M0L6_2atmpS1299 = _M0L11dst__offsetS52 + _M0L1iS54;
        int32_t _M0L6_2atmpS1301 = _M0L11src__offsetS53 + _M0L1iS54;
        void* _M0L6_2atmpS3710;
        void* _M0L6_2atmpS1300;
        void* _M0L6_2aoldS3709;
        int32_t _M0L6_2atmpS1302;
        if (
          _M0L6_2atmpS1301 < 0
          || _M0L6_2atmpS1301 >= Moonbit_array_length(_M0L3srcS51)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3710 = (void*)_M0L3srcS51[_M0L6_2atmpS1301];
        _M0L6_2atmpS1300 = _M0L6_2atmpS3710;
        if (
          _M0L6_2atmpS1299 < 0
          || _M0L6_2atmpS1299 >= Moonbit_array_length(_M0L3dstS50)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3709 = (void*)_M0L3dstS50[_M0L6_2atmpS1299];
        moonbit_incref(_M0L6_2atmpS1300);
        moonbit_decref(_M0L6_2aoldS3709);
        _M0L3dstS50[_M0L6_2atmpS1299] = _M0L6_2atmpS1300;
        _M0L6_2atmpS1302 = _M0L1iS54 + 1;
        _M0L1iS54 = _M0L6_2atmpS1302;
        continue;
      } else {
        moonbit_decref(_M0L3srcS51);
        moonbit_decref(_M0L3dstS50);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1307 = _M0L3lenS55 - 1;
    int32_t _M0L1iS57 = _M0L6_2atmpS1307;
    while (1) {
      if (_M0L1iS57 >= 0) {
        int32_t _M0L6_2atmpS1303 = _M0L11dst__offsetS52 + _M0L1iS57;
        int32_t _M0L6_2atmpS1305 = _M0L11src__offsetS53 + _M0L1iS57;
        void* _M0L6_2atmpS3712;
        void* _M0L6_2atmpS1304;
        void* _M0L6_2aoldS3711;
        int32_t _M0L6_2atmpS1306;
        if (
          _M0L6_2atmpS1305 < 0
          || _M0L6_2atmpS1305 >= Moonbit_array_length(_M0L3srcS51)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3712 = (void*)_M0L3srcS51[_M0L6_2atmpS1305];
        _M0L6_2atmpS1304 = _M0L6_2atmpS3712;
        if (
          _M0L6_2atmpS1303 < 0
          || _M0L6_2atmpS1303 >= Moonbit_array_length(_M0L3dstS50)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3711 = (void*)_M0L3dstS50[_M0L6_2atmpS1303];
        moonbit_incref(_M0L6_2atmpS1304);
        moonbit_decref(_M0L6_2aoldS3711);
        _M0L3dstS50[_M0L6_2atmpS1303] = _M0L6_2atmpS1304;
        _M0L6_2atmpS1306 = _M0L1iS57 - 1;
        _M0L1iS57 = _M0L6_2atmpS1306;
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
  moonbit_string_t _M0L6_2atmpS1261;
  moonbit_string_t _M0L6_2atmpS3715;
  moonbit_string_t _M0L6_2atmpS1259;
  moonbit_string_t _M0L6_2atmpS1260;
  moonbit_string_t _M0L6_2atmpS3714;
  moonbit_string_t _M0L6_2atmpS1258;
  moonbit_string_t _M0L6_2atmpS3713;
  moonbit_string_t _M0L6_2atmpS1257;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1261 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3715
  = moonbit_add_string(_M0L6_2atmpS1261, (moonbit_string_t)moonbit_string_literal_259.data);
  moonbit_decref(_M0L6_2atmpS1261);
  _M0L6_2atmpS1259 = _M0L6_2atmpS3715;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1260
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3714 = moonbit_add_string(_M0L6_2atmpS1259, _M0L6_2atmpS1260);
  moonbit_decref(_M0L6_2atmpS1259);
  moonbit_decref(_M0L6_2atmpS1260);
  _M0L6_2atmpS1258 = _M0L6_2atmpS3714;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3713
  = moonbit_add_string(_M0L6_2atmpS1258, (moonbit_string_t)moonbit_string_literal_211.data);
  moonbit_decref(_M0L6_2atmpS1258);
  _M0L6_2atmpS1257 = _M0L6_2atmpS3713;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1257);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS19,
  moonbit_string_t _M0L3locS20
) {
  moonbit_string_t _M0L6_2atmpS1266;
  moonbit_string_t _M0L6_2atmpS3718;
  moonbit_string_t _M0L6_2atmpS1264;
  moonbit_string_t _M0L6_2atmpS1265;
  moonbit_string_t _M0L6_2atmpS3717;
  moonbit_string_t _M0L6_2atmpS1263;
  moonbit_string_t _M0L6_2atmpS3716;
  moonbit_string_t _M0L6_2atmpS1262;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1266 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3718
  = moonbit_add_string(_M0L6_2atmpS1266, (moonbit_string_t)moonbit_string_literal_259.data);
  moonbit_decref(_M0L6_2atmpS1266);
  _M0L6_2atmpS1264 = _M0L6_2atmpS3718;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1265
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3717 = moonbit_add_string(_M0L6_2atmpS1264, _M0L6_2atmpS1265);
  moonbit_decref(_M0L6_2atmpS1264);
  moonbit_decref(_M0L6_2atmpS1265);
  _M0L6_2atmpS1263 = _M0L6_2atmpS3717;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3716
  = moonbit_add_string(_M0L6_2atmpS1263, (moonbit_string_t)moonbit_string_literal_211.data);
  moonbit_decref(_M0L6_2atmpS1263);
  _M0L6_2atmpS1262 = _M0L6_2atmpS3716;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1262);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1271;
  moonbit_string_t _M0L6_2atmpS3721;
  moonbit_string_t _M0L6_2atmpS1269;
  moonbit_string_t _M0L6_2atmpS1270;
  moonbit_string_t _M0L6_2atmpS3720;
  moonbit_string_t _M0L6_2atmpS1268;
  moonbit_string_t _M0L6_2atmpS3719;
  moonbit_string_t _M0L6_2atmpS1267;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1271 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3721
  = moonbit_add_string(_M0L6_2atmpS1271, (moonbit_string_t)moonbit_string_literal_259.data);
  moonbit_decref(_M0L6_2atmpS1271);
  _M0L6_2atmpS1269 = _M0L6_2atmpS3721;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1270
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3720 = moonbit_add_string(_M0L6_2atmpS1269, _M0L6_2atmpS1270);
  moonbit_decref(_M0L6_2atmpS1269);
  moonbit_decref(_M0L6_2atmpS1270);
  _M0L6_2atmpS1268 = _M0L6_2atmpS3720;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3719
  = moonbit_add_string(_M0L6_2atmpS1268, (moonbit_string_t)moonbit_string_literal_211.data);
  moonbit_decref(_M0L6_2atmpS1268);
  _M0L6_2atmpS1267 = _M0L6_2atmpS3719;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1267);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5valueS16
) {
  uint32_t _M0L3accS1256;
  uint32_t _M0L6_2atmpS1255;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1256 = _M0L4selfS15->$0;
  _M0L6_2atmpS1255 = _M0L3accS1256 + 4u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1255;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS15, _M0L5valueS16);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS13,
  uint32_t _M0L5inputS14
) {
  uint32_t _M0L3accS1253;
  uint32_t _M0L6_2atmpS1254;
  uint32_t _M0L6_2atmpS1252;
  uint32_t _M0L6_2atmpS1251;
  uint32_t _M0L6_2atmpS1250;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1253 = _M0L4selfS13->$0;
  _M0L6_2atmpS1254 = _M0L5inputS14 * 3266489917u;
  _M0L6_2atmpS1252 = _M0L3accS1253 + _M0L6_2atmpS1254;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1251 = _M0FPB4rotl(_M0L6_2atmpS1252, 17);
  _M0L6_2atmpS1250 = _M0L6_2atmpS1251 * 668265263u;
  _M0L4selfS13->$0 = _M0L6_2atmpS1250;
  moonbit_decref(_M0L4selfS13);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS11, int32_t _M0L1rS12) {
  uint32_t _M0L6_2atmpS1247;
  int32_t _M0L6_2atmpS1249;
  uint32_t _M0L6_2atmpS1248;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1247 = _M0L1xS11 << (_M0L1rS12 & 31);
  _M0L6_2atmpS1249 = 32 - _M0L1rS12;
  _M0L6_2atmpS1248 = _M0L1xS11 >> (_M0L6_2atmpS1249 & 31);
  return _M0L6_2atmpS1247 | _M0L6_2atmpS1248;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S7,
  struct _M0TPB6Logger _M0L10_2ax__4934S10
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS8;
  moonbit_string_t _M0L8_2afieldS3722;
  int32_t _M0L6_2acntS3809;
  moonbit_string_t _M0L15_2a_2aarg__4935S9;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS8
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S7;
  _M0L8_2afieldS3722 = _M0L10_2aFailureS8->$0;
  _M0L6_2acntS3809 = Moonbit_object_header(_M0L10_2aFailureS8)->rc;
  if (_M0L6_2acntS3809 > 1) {
    int32_t _M0L11_2anew__cntS3810 = _M0L6_2acntS3809 - 1;
    Moonbit_object_header(_M0L10_2aFailureS8)->rc = _M0L11_2anew__cntS3810;
    moonbit_incref(_M0L8_2afieldS3722);
  } else if (_M0L6_2acntS3809 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS8);
  }
  _M0L15_2a_2aarg__4935S9 = _M0L8_2afieldS3722;
  if (_M0L10_2ax__4934S10.$1) {
    moonbit_incref(_M0L10_2ax__4934S10.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S10.$0->$method_0(_M0L10_2ax__4934S10.$1, (moonbit_string_t)moonbit_string_literal_260.data);
  if (_M0L10_2ax__4934S10.$1) {
    moonbit_incref(_M0L10_2ax__4934S10.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S10, _M0L15_2a_2aarg__4935S9);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S10.$0->$method_0(_M0L10_2ax__4934S10.$1, (moonbit_string_t)moonbit_string_literal_261.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS6) {
  void* _block_4106;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4106 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4106)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4106)->$0 = _M0L4selfS6;
  return _block_4106;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1197) {
  switch (Moonbit_object_tag(_M0L4_2aeS1197)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1197);
      return (moonbit_string_t)moonbit_string_literal_262.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1197);
      return (moonbit_string_t)moonbit_string_literal_263.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1197);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1197);
      return (moonbit_string_t)moonbit_string_literal_264.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1197);
      return (moonbit_string_t)moonbit_string_literal_265.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1217,
  int32_t _M0L8_2aparamS1216
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1215 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1217;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1215, _M0L8_2aparamS1216);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1214,
  struct _M0TPC16string10StringView _M0L8_2aparamS1213
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1212 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1214;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1212, _M0L8_2aparamS1213);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1211,
  moonbit_string_t _M0L8_2aparamS1208,
  int32_t _M0L8_2aparamS1209,
  int32_t _M0L8_2aparamS1210
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1207 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1211;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1207, _M0L8_2aparamS1208, _M0L8_2aparamS1209, _M0L8_2aparamS1210);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1206,
  moonbit_string_t _M0L8_2aparamS1205
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1204 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1206;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1204, _M0L8_2aparamS1205);
  return 0;
}

void* _M0IPC13int3IntPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1202
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1203 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1202;
  int32_t _M0L8_2afieldS3723 = _M0L14_2aboxed__selfS1203->$0;
  int32_t _M0L7_2aselfS1201;
  moonbit_decref(_M0L14_2aboxed__selfS1203);
  _M0L7_2aselfS1201 = _M0L8_2afieldS3723;
  return _M0IPC13int3IntPB6ToJson8to__json(_M0L7_2aselfS1201);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1246 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1245;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1244;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1121;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1243;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1242;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1241;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1225;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1122;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1240;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1239;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1238;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1226;
  moonbit_string_t* _M0L6_2atmpS1237;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1236;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1235;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1123;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1234;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1233;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1232;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1227;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1124;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1231;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1230;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1229;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1228;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1120;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1224;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1223;
  _M0L6_2atmpS1246[0] = (moonbit_string_t)moonbit_string_literal_266.data;
  moonbit_incref(_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test49____test__7374617475735f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1245
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1245)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1245->$0
  = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test49____test__7374617475735f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1245->$1 = _M0L6_2atmpS1246;
  _M0L8_2atupleS1244
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1244)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1244->$0 = 0;
  _M0L8_2atupleS1244->$1 = _M0L8_2atupleS1245;
  _M0L7_2abindS1121
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1121[0] = _M0L8_2atupleS1244;
  _M0L6_2atmpS1243 = _M0L7_2abindS1121;
  _M0L6_2atmpS1242
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1243
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1241
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1242);
  _M0L8_2atupleS1225
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1225)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1225->$0 = (moonbit_string_t)moonbit_string_literal_267.data;
  _M0L8_2atupleS1225->$1 = _M0L6_2atmpS1241;
  _M0L7_2abindS1122
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1240 = _M0L7_2abindS1122;
  _M0L6_2atmpS1239
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1240
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1238
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1239);
  _M0L8_2atupleS1226
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1226)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1226->$0 = (moonbit_string_t)moonbit_string_literal_268.data;
  _M0L8_2atupleS1226->$1 = _M0L6_2atmpS1238;
  _M0L6_2atmpS1237 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1237[0] = (moonbit_string_t)moonbit_string_literal_269.data;
  moonbit_incref(_M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test47____test__636c6173735f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1236
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1236)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1236->$0
  = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test47____test__636c6173735f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1236->$1 = _M0L6_2atmpS1237;
  _M0L8_2atupleS1235
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1235)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1235->$0 = 0;
  _M0L8_2atupleS1235->$1 = _M0L8_2atupleS1236;
  _M0L7_2abindS1123
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1123[0] = _M0L8_2atupleS1235;
  _M0L6_2atmpS1234 = _M0L7_2abindS1123;
  _M0L6_2atmpS1233
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1234
  };
  #line 403 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1232
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1233);
  _M0L8_2atupleS1227
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1227)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1227->$0 = (moonbit_string_t)moonbit_string_literal_270.data;
  _M0L8_2atupleS1227->$1 = _M0L6_2atmpS1232;
  _M0L7_2abindS1124
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1231 = _M0L7_2abindS1124;
  _M0L6_2atmpS1230
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1231
  };
  #line 406 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1229
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1230);
  _M0L8_2atupleS1228
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1228)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1228->$0 = (moonbit_string_t)moonbit_string_literal_271.data;
  _M0L8_2atupleS1228->$1 = _M0L6_2atmpS1229;
  _M0L7_2abindS1120
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1120[0] = _M0L8_2atupleS1225;
  _M0L7_2abindS1120[1] = _M0L8_2atupleS1226;
  _M0L7_2abindS1120[2] = _M0L8_2atupleS1227;
  _M0L7_2abindS1120[3] = _M0L8_2atupleS1228;
  _M0L6_2atmpS1224 = _M0L7_2abindS1120;
  _M0L6_2atmpS1223
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 4, _M0L6_2atmpS1224
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1223);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1222;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1191;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1192;
  int32_t _M0L7_2abindS1193;
  int32_t _M0L2__S1194;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1222
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1191
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1191)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1191->$0 = _M0L6_2atmpS1222;
  _M0L12async__testsS1191->$1 = 0;
  #line 445 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1192
  = _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1193 = _M0L7_2abindS1192->$1;
  _M0L2__S1194 = 0;
  while (1) {
    if (_M0L2__S1194 < _M0L7_2abindS1193) {
      struct _M0TUsiE** _M0L8_2afieldS3727 = _M0L7_2abindS1192->$0;
      struct _M0TUsiE** _M0L3bufS1221 = _M0L8_2afieldS3727;
      struct _M0TUsiE* _M0L6_2atmpS3726 =
        (struct _M0TUsiE*)_M0L3bufS1221[_M0L2__S1194];
      struct _M0TUsiE* _M0L3argS1195 = _M0L6_2atmpS3726;
      moonbit_string_t _M0L8_2afieldS3725 = _M0L3argS1195->$0;
      moonbit_string_t _M0L6_2atmpS1218 = _M0L8_2afieldS3725;
      int32_t _M0L8_2afieldS3724 = _M0L3argS1195->$1;
      int32_t _M0L6_2atmpS1219 = _M0L8_2afieldS3724;
      int32_t _M0L6_2atmpS1220;
      moonbit_incref(_M0L6_2atmpS1218);
      moonbit_incref(_M0L12async__testsS1191);
      #line 446 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
      _M0FP58clawteam8clawteam8internal5httpx22status__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1191, _M0L6_2atmpS1218, _M0L6_2atmpS1219);
      _M0L6_2atmpS1220 = _M0L2__S1194 + 1;
      _M0L2__S1194 = _M0L6_2atmpS1220;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1192);
    }
    break;
  }
  #line 448 "E:\\moonbit\\clawteam\\internal\\httpx\\status\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP58clawteam8clawteam8internal5httpx22status__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP58clawteam8clawteam8internal5httpx22status__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1191);
  return 0;
}