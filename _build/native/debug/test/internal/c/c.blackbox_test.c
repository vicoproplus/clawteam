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

struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__;

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

struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0DTPC15error5Error120clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__;

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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal17c__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0KTPB6ToJsonS6UInt64;

struct _M0Y6UInt64;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal17c__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0R38String_3a_3aiter_2eanon__u1911__l247__;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

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

struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__ {
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

struct _M0DTPC15error5Error120clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
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

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal17c__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0KTPB6ToJsonS6UInt64 {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0Y6UInt64 {
  uint64_t $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal17c__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u1911__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__7374726c656e5f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__6d656d6370795f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1205(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1196(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testC2772l446(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testC2768l447(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__3(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__2(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__0(
  
);

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1117(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1112(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1099(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal17c__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0(
  
);

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0N8_2adeferS1076(
  void*
);

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0N8_2adeferS1069(
  void*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__7374726c656e5f746573742e6d6274__0(
  
);

int32_t _M0FP48clawteam8clawteam8internal1c4exitGuE(int32_t);

int32_t _M0FP48clawteam8clawteam8internal1c6__exit(int32_t);

int32_t _M0FP48clawteam8clawteam8internal1c4freeGyE(void*);

int32_t _M0FP48clawteam8clawteam8internal1c4freeGiE(void*);

int32_t _M0FP48clawteam8clawteam8internal1c7c__free(void*);

void* _M0FP48clawteam8clawteam8internal1c6mallocGyE(uint64_t);

void* _M0FP48clawteam8clawteam8internal1c6mallocGiE(uint64_t);

#define _M0FP48clawteam8clawteam8internal1c9c__malloc moonbit_moonclaw_c_malloc

#define _M0FP48clawteam8clawteam8internal1c6memcpy moonbit_moonclaw_c_memcpy

int32_t _M0IPC13int3IntP48clawteam8clawteam8internal1c5Store5store(
  void*,
  int32_t,
  int32_t
);

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c5Store5store(
  void*,
  int32_t,
  int32_t
);

#define _M0FP48clawteam8clawteam8internal1c22moonbit__c__store__int moonbit_moonclaw_c_store_int

#define _M0FP48clawteam8clawteam8internal1c23moonbit__c__store__byte moonbit_moonclaw_c_store_byte

int32_t _M0IPC13int3IntP48clawteam8clawteam8internal1c4Load4load(
  void*,
  int32_t
);

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void*,
  int32_t
);

#define _M0FP48clawteam8clawteam8internal1c21moonbit__c__load__int moonbit_moonclaw_c_load_int

#define _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte moonbit_moonclaw_c_load_byte

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer13store_2einnerGiE(
  void*,
  int32_t,
  int32_t
);

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer12load_2einnerGiE(
  void*,
  int32_t
);

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(
  void*,
  int32_t,
  int32_t
);

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void*,
  int32_t
);

#define _M0FP48clawteam8clawteam8internal1c6strlen moonbit_moonclaw_c_strlen

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

int32_t _M0IPC14byte4BytePB2Eq10not__equal(int32_t, int32_t);

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

void* _M0IPC16uint646UInt64PB6ToJson8to__json(uint64_t);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2121l591(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1937l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC14byte4BytePB4Show6output(int32_t, struct _M0TPB6Logger);

moonbit_string_t _M0MPC14byte4Byte10to__string(int32_t);

moonbit_string_t _M0FPB8alphabet(int32_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1911l247(struct _M0TWEOc*);

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

struct moonbit_result_0 _M0FPB10assert__eqGiE(
  int32_t,
  int32_t,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB10assert__eqGyE(
  int32_t,
  int32_t,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB4failGuE(moonbit_string_t, moonbit_string_t);

moonbit_string_t _M0FPB13debug__stringGiE(int32_t);

moonbit_string_t _M0FPB13debug__stringGyE(int32_t);

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

moonbit_string_t _M0FPB5abortGsE(moonbit_string_t, moonbit_string_t);

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

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t);

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

void* _M0IPC16uint646UInt64PB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

int32_t moonbit_moonclaw_c_load_byte(void*, int32_t);

int32_t moonbit_moonclaw_c_store_int(void*, int32_t, int32_t);

void* moonbit_moonclaw_c_malloc(uint64_t);

void moonbit_moonclaw_c_free(void*);

int32_t moonbit_moonclaw_c_store_byte(void*, int32_t, int32_t);

int32_t moonbit_moonclaw_c_load_int(void*, int32_t);

uint64_t moonbit_moonclaw_c_strlen(void*);

int32_t moonbit_moonclaw_c_memcpy(void*, void*, uint64_t);

void exit(int32_t);

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_64 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 55, 0};

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
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    98, 39, 92, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 46, 109, 98, 116, 58, 49, 56, 48, 58, 49, 48, 45, 49, 56, 
    48, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_60 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[71]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 70), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    115, 116, 114, 108, 101, 110, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 49, 48, 58, 51, 45, 49, 48, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 58, 53, 54, 
    58, 51, 53, 45, 53, 54, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[109]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 108), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 95, 98, 
    108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 
    111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 
    114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 
    111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_59 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_50 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 34, 44, 32, 34, 
    102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_71 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 69, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 58, 55, 54, 
    58, 51, 45, 55, 54, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_99 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_63 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_62 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_40 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_31 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_67 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 65, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    112, 111, 105, 110, 116, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 116, 114, 108, 101, 110, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    110, 117, 108, 108, 97, 98, 108, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_92 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[72]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 71), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    115, 116, 114, 108, 101, 110, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 49, 48, 58, 51, 56, 45, 49, 48, 58, 52, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_22 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    101, 120, 105, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[72]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 71), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    115, 116, 114, 108, 101, 110, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 58, 49, 48, 58, 49, 54, 45, 49, 48, 58, 50, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_54 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    58, 101, 120, 105, 116, 46, 109, 98, 116, 58, 49, 54, 58, 51, 45, 
    49, 54, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_11 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 58, 53, 54, 
    58, 51, 45, 53, 54, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    115, 116, 114, 108, 101, 110, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_66 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    109, 101, 109, 99, 112, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_80 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 58, 55, 55, 
    58, 51, 45, 55, 55, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    109, 101, 109, 99, 112, 121, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_70 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 68, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    105, 109, 112, 111, 115, 115, 105, 98, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_69 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 67, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 116, 114, 108, 101, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[70]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 69), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 58, 53, 54, 
    58, 49, 54, 45, 53, 54, 58, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    109, 97, 108, 108, 111, 99, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_72 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 70, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_65 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_61 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    82, 69, 65, 68, 77, 69, 46, 109, 98, 116, 46, 109, 100, 58, 50, 56, 
    58, 51, 45, 50, 56, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    101, 120, 105, 116, 32, 115, 104, 111, 117, 108, 100, 32, 110, 101, 
    118, 101, 114, 32, 114, 101, 116, 117, 114, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    32, 33, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[111]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 110), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 95, 98, 
    108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 
    111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 
    114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 
    101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    109, 101, 109, 99, 112, 121, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_68 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 66, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_58 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 39, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1205$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1205
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__6d656d6370795f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__6d656d6370795f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__7374726c656e5f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__7374726c656e5f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__3_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__6d656d6370795f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__6d656d6370795f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__7374726c656e5f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__7374726c656e5f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__1_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP081UInt64_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16uint646UInt64PB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP081UInt64_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP081UInt64_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB30ryu__to__string_2erecord_2f923$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f923 =
  &_M0FPB30ryu__to__string_2erecord_2f923$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal17c__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2808
) {
  return _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__1();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2807
) {
  return _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__2();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2806
) {
  return _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__7374726c656e5f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2805
) {
  return _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__7374726c656e5f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test53____test__6d656d6370795f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2804
) {
  return _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__524541444d452e6d62742e6d64__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2803
) {
  return _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__3();
}

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1226,
  moonbit_string_t _M0L8filenameS1201,
  int32_t _M0L5indexS1204
) {
  struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196* _closure_3148;
  struct _M0TWssbEu* _M0L14handle__resultS1196;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1205;
  void* _M0L11_2atry__errS1220;
  struct moonbit_result_0 _tmp_3150;
  int32_t _handle__error__result_3151;
  int32_t _M0L6_2atmpS2791;
  void* _M0L3errS1221;
  moonbit_string_t _M0L4nameS1223;
  struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1224;
  moonbit_string_t _M0L8_2afieldS2809;
  int32_t _M0L6_2acntS3065;
  moonbit_string_t _M0L7_2anameS1225;
  #line 545 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1201);
  _closure_3148
  = (struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196*)moonbit_malloc(sizeof(struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196));
  Moonbit_object_header(_closure_3148)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196, $1) >> 2, 1, 0);
  _closure_3148->code
  = &_M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1196;
  _closure_3148->$0 = _M0L5indexS1204;
  _closure_3148->$1 = _M0L8filenameS1201;
  _M0L14handle__resultS1196 = (struct _M0TWssbEu*)_closure_3148;
  _M0L17error__to__stringS1205
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1205$closure.data;
  moonbit_incref(_M0L12async__testsS1226);
  moonbit_incref(_M0L17error__to__stringS1205);
  moonbit_incref(_M0L8filenameS1201);
  moonbit_incref(_M0L14handle__resultS1196);
  #line 579 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3150
  = _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1226, _M0L8filenameS1201, _M0L5indexS1204, _M0L14handle__resultS1196, _M0L17error__to__stringS1205);
  if (_tmp_3150.tag) {
    int32_t const _M0L5_2aokS2800 = _tmp_3150.data.ok;
    _handle__error__result_3151 = _M0L5_2aokS2800;
  } else {
    void* const _M0L6_2aerrS2801 = _tmp_3150.data.err;
    moonbit_decref(_M0L12async__testsS1226);
    moonbit_decref(_M0L17error__to__stringS1205);
    moonbit_decref(_M0L8filenameS1201);
    _M0L11_2atry__errS1220 = _M0L6_2aerrS2801;
    goto join_1219;
  }
  if (_handle__error__result_3151) {
    moonbit_decref(_M0L12async__testsS1226);
    moonbit_decref(_M0L17error__to__stringS1205);
    moonbit_decref(_M0L8filenameS1201);
    _M0L6_2atmpS2791 = 1;
  } else {
    struct moonbit_result_0 _tmp_3152;
    int32_t _handle__error__result_3153;
    moonbit_incref(_M0L12async__testsS1226);
    moonbit_incref(_M0L17error__to__stringS1205);
    moonbit_incref(_M0L8filenameS1201);
    moonbit_incref(_M0L14handle__resultS1196);
    #line 582 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    _tmp_3152
    = _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1226, _M0L8filenameS1201, _M0L5indexS1204, _M0L14handle__resultS1196, _M0L17error__to__stringS1205);
    if (_tmp_3152.tag) {
      int32_t const _M0L5_2aokS2798 = _tmp_3152.data.ok;
      _handle__error__result_3153 = _M0L5_2aokS2798;
    } else {
      void* const _M0L6_2aerrS2799 = _tmp_3152.data.err;
      moonbit_decref(_M0L12async__testsS1226);
      moonbit_decref(_M0L17error__to__stringS1205);
      moonbit_decref(_M0L8filenameS1201);
      _M0L11_2atry__errS1220 = _M0L6_2aerrS2799;
      goto join_1219;
    }
    if (_handle__error__result_3153) {
      moonbit_decref(_M0L12async__testsS1226);
      moonbit_decref(_M0L17error__to__stringS1205);
      moonbit_decref(_M0L8filenameS1201);
      _M0L6_2atmpS2791 = 1;
    } else {
      struct moonbit_result_0 _tmp_3154;
      int32_t _handle__error__result_3155;
      moonbit_incref(_M0L12async__testsS1226);
      moonbit_incref(_M0L17error__to__stringS1205);
      moonbit_incref(_M0L8filenameS1201);
      moonbit_incref(_M0L14handle__resultS1196);
      #line 585 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _tmp_3154
      = _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1226, _M0L8filenameS1201, _M0L5indexS1204, _M0L14handle__resultS1196, _M0L17error__to__stringS1205);
      if (_tmp_3154.tag) {
        int32_t const _M0L5_2aokS2796 = _tmp_3154.data.ok;
        _handle__error__result_3155 = _M0L5_2aokS2796;
      } else {
        void* const _M0L6_2aerrS2797 = _tmp_3154.data.err;
        moonbit_decref(_M0L12async__testsS1226);
        moonbit_decref(_M0L17error__to__stringS1205);
        moonbit_decref(_M0L8filenameS1201);
        _M0L11_2atry__errS1220 = _M0L6_2aerrS2797;
        goto join_1219;
      }
      if (_handle__error__result_3155) {
        moonbit_decref(_M0L12async__testsS1226);
        moonbit_decref(_M0L17error__to__stringS1205);
        moonbit_decref(_M0L8filenameS1201);
        _M0L6_2atmpS2791 = 1;
      } else {
        struct moonbit_result_0 _tmp_3156;
        int32_t _handle__error__result_3157;
        moonbit_incref(_M0L12async__testsS1226);
        moonbit_incref(_M0L17error__to__stringS1205);
        moonbit_incref(_M0L8filenameS1201);
        moonbit_incref(_M0L14handle__resultS1196);
        #line 588 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        _tmp_3156
        = _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1226, _M0L8filenameS1201, _M0L5indexS1204, _M0L14handle__resultS1196, _M0L17error__to__stringS1205);
        if (_tmp_3156.tag) {
          int32_t const _M0L5_2aokS2794 = _tmp_3156.data.ok;
          _handle__error__result_3157 = _M0L5_2aokS2794;
        } else {
          void* const _M0L6_2aerrS2795 = _tmp_3156.data.err;
          moonbit_decref(_M0L12async__testsS1226);
          moonbit_decref(_M0L17error__to__stringS1205);
          moonbit_decref(_M0L8filenameS1201);
          _M0L11_2atry__errS1220 = _M0L6_2aerrS2795;
          goto join_1219;
        }
        if (_handle__error__result_3157) {
          moonbit_decref(_M0L12async__testsS1226);
          moonbit_decref(_M0L17error__to__stringS1205);
          moonbit_decref(_M0L8filenameS1201);
          _M0L6_2atmpS2791 = 1;
        } else {
          struct moonbit_result_0 _tmp_3158;
          moonbit_incref(_M0L14handle__resultS1196);
          #line 591 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
          _tmp_3158
          = _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1226, _M0L8filenameS1201, _M0L5indexS1204, _M0L14handle__resultS1196, _M0L17error__to__stringS1205);
          if (_tmp_3158.tag) {
            int32_t const _M0L5_2aokS2792 = _tmp_3158.data.ok;
            _M0L6_2atmpS2791 = _M0L5_2aokS2792;
          } else {
            void* const _M0L6_2aerrS2793 = _tmp_3158.data.err;
            _M0L11_2atry__errS1220 = _M0L6_2aerrS2793;
            goto join_1219;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2791) {
    void* _M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2802 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2802)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2802)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1220
    = _M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2802;
    goto join_1219;
  } else {
    moonbit_decref(_M0L14handle__resultS1196);
  }
  goto joinlet_3149;
  join_1219:;
  _M0L3errS1221 = _M0L11_2atry__errS1220;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1224
  = (struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1221;
  _M0L8_2afieldS2809 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1224->$0;
  _M0L6_2acntS3065
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1224)->rc;
  if (_M0L6_2acntS3065 > 1) {
    int32_t _M0L11_2anew__cntS3066 = _M0L6_2acntS3065 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1224)->rc
    = _M0L11_2anew__cntS3066;
    moonbit_incref(_M0L8_2afieldS2809);
  } else if (_M0L6_2acntS3065 == 1) {
    #line 598 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1224);
  }
  _M0L7_2anameS1225 = _M0L8_2afieldS2809;
  _M0L4nameS1223 = _M0L7_2anameS1225;
  goto join_1222;
  goto joinlet_3159;
  join_1222:;
  #line 599 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1196(_M0L14handle__resultS1196, _M0L4nameS1223, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3159:;
  joinlet_3149:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1205(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2790,
  void* _M0L3errS1206
) {
  void* _M0L1eS1208;
  moonbit_string_t _M0L1eS1210;
  #line 568 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2790);
  switch (Moonbit_object_tag(_M0L3errS1206)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1211 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1206;
      moonbit_string_t _M0L8_2afieldS2810 = _M0L10_2aFailureS1211->$0;
      int32_t _M0L6_2acntS3067 =
        Moonbit_object_header(_M0L10_2aFailureS1211)->rc;
      moonbit_string_t _M0L4_2aeS1212;
      if (_M0L6_2acntS3067 > 1) {
        int32_t _M0L11_2anew__cntS3068 = _M0L6_2acntS3067 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1211)->rc
        = _M0L11_2anew__cntS3068;
        moonbit_incref(_M0L8_2afieldS2810);
      } else if (_M0L6_2acntS3067 == 1) {
        #line 569 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1211);
      }
      _M0L4_2aeS1212 = _M0L8_2afieldS2810;
      _M0L1eS1210 = _M0L4_2aeS1212;
      goto join_1209;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1213 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1206;
      moonbit_string_t _M0L8_2afieldS2811 = _M0L15_2aInspectErrorS1213->$0;
      int32_t _M0L6_2acntS3069 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1213)->rc;
      moonbit_string_t _M0L4_2aeS1214;
      if (_M0L6_2acntS3069 > 1) {
        int32_t _M0L11_2anew__cntS3070 = _M0L6_2acntS3069 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1213)->rc
        = _M0L11_2anew__cntS3070;
        moonbit_incref(_M0L8_2afieldS2811);
      } else if (_M0L6_2acntS3069 == 1) {
        #line 569 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1213);
      }
      _M0L4_2aeS1214 = _M0L8_2afieldS2811;
      _M0L1eS1210 = _M0L4_2aeS1214;
      goto join_1209;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1215 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1206;
      moonbit_string_t _M0L8_2afieldS2812 = _M0L16_2aSnapshotErrorS1215->$0;
      int32_t _M0L6_2acntS3071 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1215)->rc;
      moonbit_string_t _M0L4_2aeS1216;
      if (_M0L6_2acntS3071 > 1) {
        int32_t _M0L11_2anew__cntS3072 = _M0L6_2acntS3071 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1215)->rc
        = _M0L11_2anew__cntS3072;
        moonbit_incref(_M0L8_2afieldS2812);
      } else if (_M0L6_2acntS3071 == 1) {
        #line 569 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1215);
      }
      _M0L4_2aeS1216 = _M0L8_2afieldS2812;
      _M0L1eS1210 = _M0L4_2aeS1216;
      goto join_1209;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error120clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1217 =
        (struct _M0DTPC15error5Error120clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1206;
      moonbit_string_t _M0L8_2afieldS2813 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1217->$0;
      int32_t _M0L6_2acntS3073 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1217)->rc;
      moonbit_string_t _M0L4_2aeS1218;
      if (_M0L6_2acntS3073 > 1) {
        int32_t _M0L11_2anew__cntS3074 = _M0L6_2acntS3073 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1217)->rc
        = _M0L11_2anew__cntS3074;
        moonbit_incref(_M0L8_2afieldS2813);
      } else if (_M0L6_2acntS3073 == 1) {
        #line 569 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1217);
      }
      _M0L4_2aeS1218 = _M0L8_2afieldS2813;
      _M0L1eS1210 = _M0L4_2aeS1218;
      goto join_1209;
      break;
    }
    default: {
      _M0L1eS1208 = _M0L3errS1206;
      goto join_1207;
      break;
    }
  }
  join_1209:;
  return _M0L1eS1210;
  join_1207:;
  #line 574 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1208);
}

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1196(
  struct _M0TWssbEu* _M0L6_2aenvS2776,
  moonbit_string_t _M0L8testnameS1197,
  moonbit_string_t _M0L7messageS1198,
  int32_t _M0L7skippedS1199
) {
  struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196* _M0L14_2acasted__envS2777;
  moonbit_string_t _M0L8_2afieldS2823;
  moonbit_string_t _M0L8filenameS1201;
  int32_t _M0L8_2afieldS2822;
  int32_t _M0L6_2acntS3075;
  int32_t _M0L5indexS1204;
  int32_t _if__result_3162;
  moonbit_string_t _M0L10file__nameS1200;
  moonbit_string_t _M0L10test__nameS1202;
  moonbit_string_t _M0L7messageS1203;
  moonbit_string_t _M0L6_2atmpS2789;
  moonbit_string_t _M0L6_2atmpS2821;
  moonbit_string_t _M0L6_2atmpS2788;
  moonbit_string_t _M0L6_2atmpS2820;
  moonbit_string_t _M0L6_2atmpS2786;
  moonbit_string_t _M0L6_2atmpS2787;
  moonbit_string_t _M0L6_2atmpS2819;
  moonbit_string_t _M0L6_2atmpS2785;
  moonbit_string_t _M0L6_2atmpS2818;
  moonbit_string_t _M0L6_2atmpS2783;
  moonbit_string_t _M0L6_2atmpS2784;
  moonbit_string_t _M0L6_2atmpS2817;
  moonbit_string_t _M0L6_2atmpS2782;
  moonbit_string_t _M0L6_2atmpS2816;
  moonbit_string_t _M0L6_2atmpS2780;
  moonbit_string_t _M0L6_2atmpS2781;
  moonbit_string_t _M0L6_2atmpS2815;
  moonbit_string_t _M0L6_2atmpS2779;
  moonbit_string_t _M0L6_2atmpS2814;
  moonbit_string_t _M0L6_2atmpS2778;
  #line 552 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2777
  = (struct _M0R124_24clawteam_2fclawteam_2finternal_2fc__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1196*)_M0L6_2aenvS2776;
  _M0L8_2afieldS2823 = _M0L14_2acasted__envS2777->$1;
  _M0L8filenameS1201 = _M0L8_2afieldS2823;
  _M0L8_2afieldS2822 = _M0L14_2acasted__envS2777->$0;
  _M0L6_2acntS3075 = Moonbit_object_header(_M0L14_2acasted__envS2777)->rc;
  if (_M0L6_2acntS3075 > 1) {
    int32_t _M0L11_2anew__cntS3076 = _M0L6_2acntS3075 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2777)->rc
    = _M0L11_2anew__cntS3076;
    moonbit_incref(_M0L8filenameS1201);
  } else if (_M0L6_2acntS3075 == 1) {
    #line 552 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2777);
  }
  _M0L5indexS1204 = _M0L8_2afieldS2822;
  if (!_M0L7skippedS1199) {
    _if__result_3162 = 1;
  } else {
    _if__result_3162 = 0;
  }
  if (_if__result_3162) {
    
  }
  #line 558 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1200 = _M0MPC16string6String6escape(_M0L8filenameS1201);
  #line 559 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1202 = _M0MPC16string6String6escape(_M0L8testnameS1197);
  #line 560 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1203 = _M0MPC16string6String6escape(_M0L7messageS1198);
  #line 561 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 563 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2789
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1200);
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2821
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2789);
  moonbit_decref(_M0L6_2atmpS2789);
  _M0L6_2atmpS2788 = _M0L6_2atmpS2821;
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2820
  = moonbit_add_string(_M0L6_2atmpS2788, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2788);
  _M0L6_2atmpS2786 = _M0L6_2atmpS2820;
  #line 563 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2787
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1204);
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2819 = moonbit_add_string(_M0L6_2atmpS2786, _M0L6_2atmpS2787);
  moonbit_decref(_M0L6_2atmpS2786);
  moonbit_decref(_M0L6_2atmpS2787);
  _M0L6_2atmpS2785 = _M0L6_2atmpS2819;
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2818
  = moonbit_add_string(_M0L6_2atmpS2785, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2785);
  _M0L6_2atmpS2783 = _M0L6_2atmpS2818;
  #line 563 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2784
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1202);
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2817 = moonbit_add_string(_M0L6_2atmpS2783, _M0L6_2atmpS2784);
  moonbit_decref(_M0L6_2atmpS2783);
  moonbit_decref(_M0L6_2atmpS2784);
  _M0L6_2atmpS2782 = _M0L6_2atmpS2817;
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2816
  = moonbit_add_string(_M0L6_2atmpS2782, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2782);
  _M0L6_2atmpS2780 = _M0L6_2atmpS2816;
  #line 563 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2781
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1203);
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2815 = moonbit_add_string(_M0L6_2atmpS2780, _M0L6_2atmpS2781);
  moonbit_decref(_M0L6_2atmpS2780);
  moonbit_decref(_M0L6_2atmpS2781);
  _M0L6_2atmpS2779 = _M0L6_2atmpS2815;
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2814
  = moonbit_add_string(_M0L6_2atmpS2779, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2779);
  _M0L6_2atmpS2778 = _M0L6_2atmpS2814;
  #line 562 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2778);
  #line 565 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1195,
  moonbit_string_t _M0L8filenameS1192,
  int32_t _M0L5indexS1186,
  struct _M0TWssbEu* _M0L14handle__resultS1182,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1184
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1162;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1191;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1164;
  moonbit_string_t* _M0L5attrsS1165;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1185;
  moonbit_string_t _M0L4nameS1168;
  moonbit_string_t _M0L4nameS1166;
  int32_t _M0L6_2atmpS2775;
  struct _M0TWEOs* _M0L5_2aitS1170;
  struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__* _closure_3171;
  struct _M0TWEOc* _M0L6_2atmpS2766;
  struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__* _closure_3172;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2767;
  struct moonbit_result_0 _result_3173;
  #line 426 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1195);
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 433 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1191
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal17c__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1192);
  if (_M0L7_2abindS1191 == 0) {
    struct moonbit_result_0 _result_3164;
    if (_M0L7_2abindS1191) {
      moonbit_decref(_M0L7_2abindS1191);
    }
    moonbit_decref(_M0L17error__to__stringS1184);
    moonbit_decref(_M0L14handle__resultS1182);
    _result_3164.tag = 1;
    _result_3164.data.ok = 0;
    return _result_3164;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1193 =
      _M0L7_2abindS1191;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1194 =
      _M0L7_2aSomeS1193;
    _M0L10index__mapS1162 = _M0L13_2aindex__mapS1194;
    goto join_1161;
  }
  join_1161:;
  #line 435 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1185
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1162, _M0L5indexS1186);
  if (_M0L7_2abindS1185 == 0) {
    struct moonbit_result_0 _result_3166;
    if (_M0L7_2abindS1185) {
      moonbit_decref(_M0L7_2abindS1185);
    }
    moonbit_decref(_M0L17error__to__stringS1184);
    moonbit_decref(_M0L14handle__resultS1182);
    _result_3166.tag = 1;
    _result_3166.data.ok = 0;
    return _result_3166;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1187 =
      _M0L7_2abindS1185;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1188 = _M0L7_2aSomeS1187;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS2827 = _M0L4_2axS1188->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1189 = _M0L8_2afieldS2827;
    moonbit_string_t* _M0L8_2afieldS2826 = _M0L4_2axS1188->$1;
    int32_t _M0L6_2acntS3077 = Moonbit_object_header(_M0L4_2axS1188)->rc;
    moonbit_string_t* _M0L8_2aattrsS1190;
    if (_M0L6_2acntS3077 > 1) {
      int32_t _M0L11_2anew__cntS3078 = _M0L6_2acntS3077 - 1;
      Moonbit_object_header(_M0L4_2axS1188)->rc = _M0L11_2anew__cntS3078;
      moonbit_incref(_M0L8_2afieldS2826);
      moonbit_incref(_M0L4_2afS1189);
    } else if (_M0L6_2acntS3077 == 1) {
      #line 433 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1188);
    }
    _M0L8_2aattrsS1190 = _M0L8_2afieldS2826;
    _M0L1fS1164 = _M0L4_2afS1189;
    _M0L5attrsS1165 = _M0L8_2aattrsS1190;
    goto join_1163;
  }
  join_1163:;
  _M0L6_2atmpS2775 = Moonbit_array_length(_M0L5attrsS1165);
  if (_M0L6_2atmpS2775 >= 1) {
    moonbit_string_t _M0L6_2atmpS2825 = (moonbit_string_t)_M0L5attrsS1165[0];
    moonbit_string_t _M0L7_2anameS1169 = _M0L6_2atmpS2825;
    moonbit_incref(_M0L7_2anameS1169);
    _M0L4nameS1168 = _M0L7_2anameS1169;
    goto join_1167;
  } else {
    _M0L4nameS1166 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3167;
  join_1167:;
  _M0L4nameS1166 = _M0L4nameS1168;
  joinlet_3167:;
  #line 436 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1170 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1165);
  while (1) {
    moonbit_string_t _M0L4attrS1172;
    moonbit_string_t _M0L7_2abindS1179;
    int32_t _M0L6_2atmpS2759;
    int64_t _M0L6_2atmpS2758;
    moonbit_incref(_M0L5_2aitS1170);
    #line 438 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1179 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1170);
    if (_M0L7_2abindS1179 == 0) {
      if (_M0L7_2abindS1179) {
        moonbit_decref(_M0L7_2abindS1179);
      }
      moonbit_decref(_M0L5_2aitS1170);
    } else {
      moonbit_string_t _M0L7_2aSomeS1180 = _M0L7_2abindS1179;
      moonbit_string_t _M0L7_2aattrS1181 = _M0L7_2aSomeS1180;
      _M0L4attrS1172 = _M0L7_2aattrS1181;
      goto join_1171;
    }
    goto joinlet_3169;
    join_1171:;
    _M0L6_2atmpS2759 = Moonbit_array_length(_M0L4attrS1172);
    _M0L6_2atmpS2758 = (int64_t)_M0L6_2atmpS2759;
    moonbit_incref(_M0L4attrS1172);
    #line 439 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1172, 5, 0, _M0L6_2atmpS2758)
    ) {
      int32_t _M0L6_2atmpS2765 = _M0L4attrS1172[0];
      int32_t _M0L4_2axS1173 = _M0L6_2atmpS2765;
      if (_M0L4_2axS1173 == 112) {
        int32_t _M0L6_2atmpS2764 = _M0L4attrS1172[1];
        int32_t _M0L4_2axS1174 = _M0L6_2atmpS2764;
        if (_M0L4_2axS1174 == 97) {
          int32_t _M0L6_2atmpS2763 = _M0L4attrS1172[2];
          int32_t _M0L4_2axS1175 = _M0L6_2atmpS2763;
          if (_M0L4_2axS1175 == 110) {
            int32_t _M0L6_2atmpS2762 = _M0L4attrS1172[3];
            int32_t _M0L4_2axS1176 = _M0L6_2atmpS2762;
            if (_M0L4_2axS1176 == 105) {
              int32_t _M0L6_2atmpS2824 = _M0L4attrS1172[4];
              int32_t _M0L6_2atmpS2761;
              int32_t _M0L4_2axS1177;
              moonbit_decref(_M0L4attrS1172);
              _M0L6_2atmpS2761 = _M0L6_2atmpS2824;
              _M0L4_2axS1177 = _M0L6_2atmpS2761;
              if (_M0L4_2axS1177 == 99) {
                void* _M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2760;
                struct moonbit_result_0 _result_3170;
                moonbit_decref(_M0L17error__to__stringS1184);
                moonbit_decref(_M0L14handle__resultS1182);
                moonbit_decref(_M0L5_2aitS1170);
                moonbit_decref(_M0L1fS1164);
                _M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2760
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2760)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2760)->$0
                = _M0L4nameS1166;
                _result_3170.tag = 0;
                _result_3170.data.err
                = _M0L122clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2760;
                return _result_3170;
              }
            } else {
              moonbit_decref(_M0L4attrS1172);
            }
          } else {
            moonbit_decref(_M0L4attrS1172);
          }
        } else {
          moonbit_decref(_M0L4attrS1172);
        }
      } else {
        moonbit_decref(_M0L4attrS1172);
      }
    } else {
      moonbit_decref(_M0L4attrS1172);
    }
    continue;
    joinlet_3169:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1182);
  moonbit_incref(_M0L4nameS1166);
  _closure_3171
  = (struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__*)moonbit_malloc(sizeof(struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__));
  Moonbit_object_header(_closure_3171)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__, $0) >> 2, 2, 0);
  _closure_3171->code
  = &_M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testC2772l446;
  _closure_3171->$0 = _M0L14handle__resultS1182;
  _closure_3171->$1 = _M0L4nameS1166;
  _M0L6_2atmpS2766 = (struct _M0TWEOc*)_closure_3171;
  _closure_3172
  = (struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__*)moonbit_malloc(sizeof(struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__));
  Moonbit_object_header(_closure_3172)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__, $0) >> 2, 3, 0);
  _closure_3172->code
  = &_M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testC2768l447;
  _closure_3172->$0 = _M0L17error__to__stringS1184;
  _closure_3172->$1 = _M0L14handle__resultS1182;
  _closure_3172->$2 = _M0L4nameS1166;
  _M0L6_2atmpS2767 = (struct _M0TWRPC15error5ErrorEu*)_closure_3172;
  #line 444 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal17c__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1164, _M0L6_2atmpS2766, _M0L6_2atmpS2767);
  _result_3173.tag = 1;
  _result_3173.data.ok = 1;
  return _result_3173;
}

int32_t _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testC2772l446(
  struct _M0TWEOc* _M0L6_2aenvS2773
) {
  struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__* _M0L14_2acasted__envS2774;
  moonbit_string_t _M0L8_2afieldS2829;
  moonbit_string_t _M0L4nameS1166;
  struct _M0TWssbEu* _M0L8_2afieldS2828;
  int32_t _M0L6_2acntS3079;
  struct _M0TWssbEu* _M0L14handle__resultS1182;
  #line 446 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2774
  = (struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2772__l446__*)_M0L6_2aenvS2773;
  _M0L8_2afieldS2829 = _M0L14_2acasted__envS2774->$1;
  _M0L4nameS1166 = _M0L8_2afieldS2829;
  _M0L8_2afieldS2828 = _M0L14_2acasted__envS2774->$0;
  _M0L6_2acntS3079 = Moonbit_object_header(_M0L14_2acasted__envS2774)->rc;
  if (_M0L6_2acntS3079 > 1) {
    int32_t _M0L11_2anew__cntS3080 = _M0L6_2acntS3079 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2774)->rc
    = _M0L11_2anew__cntS3080;
    moonbit_incref(_M0L4nameS1166);
    moonbit_incref(_M0L8_2afieldS2828);
  } else if (_M0L6_2acntS3079 == 1) {
    #line 446 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2774);
  }
  _M0L14handle__resultS1182 = _M0L8_2afieldS2828;
  #line 446 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1182->code(_M0L14handle__resultS1182, _M0L4nameS1166, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal17c__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testC2768l447(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2769,
  void* _M0L3errS1183
) {
  struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__* _M0L14_2acasted__envS2770;
  moonbit_string_t _M0L8_2afieldS2832;
  moonbit_string_t _M0L4nameS1166;
  struct _M0TWssbEu* _M0L8_2afieldS2831;
  struct _M0TWssbEu* _M0L14handle__resultS1182;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2830;
  int32_t _M0L6_2acntS3081;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1184;
  moonbit_string_t _M0L6_2atmpS2771;
  #line 447 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2770
  = (struct _M0R217_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fc__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2768__l447__*)_M0L6_2aenvS2769;
  _M0L8_2afieldS2832 = _M0L14_2acasted__envS2770->$2;
  _M0L4nameS1166 = _M0L8_2afieldS2832;
  _M0L8_2afieldS2831 = _M0L14_2acasted__envS2770->$1;
  _M0L14handle__resultS1182 = _M0L8_2afieldS2831;
  _M0L8_2afieldS2830 = _M0L14_2acasted__envS2770->$0;
  _M0L6_2acntS3081 = Moonbit_object_header(_M0L14_2acasted__envS2770)->rc;
  if (_M0L6_2acntS3081 > 1) {
    int32_t _M0L11_2anew__cntS3082 = _M0L6_2acntS3081 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2770)->rc
    = _M0L11_2anew__cntS3082;
    moonbit_incref(_M0L4nameS1166);
    moonbit_incref(_M0L14handle__resultS1182);
    moonbit_incref(_M0L8_2afieldS2830);
  } else if (_M0L6_2acntS3081 == 1) {
    #line 447 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2770);
  }
  _M0L17error__to__stringS1184 = _M0L8_2afieldS2830;
  #line 447 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2771
  = _M0L17error__to__stringS1184->code(_M0L17error__to__stringS1184, _M0L3errS1183);
  #line 447 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1182->code(_M0L14handle__resultS1182, _M0L4nameS1166, _M0L6_2atmpS2771, 0);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__3(
  
) {
  int32_t _M0L6_2atmpS2757;
  struct moonbit_result_0 _result_3174;
  #line 89 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  #line 90 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2757 = _M0FP48clawteam8clawteam8internal1c4exitGuE(0);
  _result_3174.tag = 1;
  _result_3174.data.ok = _M0L6_2atmpS2757;
  return _result_3174;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__2(
  
) {
  void* _M0L3srcS1149;
  void* _M0L3dstS1150;
  void* _M0L6_2atmpS2746;
  void* _M0L6_2atmpS2747;
  int32_t _M0L6_2atmpS2748;
  moonbit_string_t _M0L6_2atmpS2749;
  struct moonbit_result_0 _tmp_3175;
  int32_t _M0L6_2atmpS2752;
  moonbit_string_t _M0L6_2atmpS2753;
  struct moonbit_result_0 _tmp_3177;
  int32_t _M0L6_2atmpS2756;
  struct moonbit_result_0 _result_3179;
  #line 68 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  #line 69 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L3srcS1149 = _M0FP48clawteam8clawteam8internal1c6mallocGyE(4ull);
  #line 70 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L3dstS1150 = _M0FP48clawteam8clawteam8internal1c6mallocGyE(4ull);
  #line 71 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L3srcS1149, 0, 1);
  #line 72 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L3srcS1149, 1, 2);
  #line 73 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L3srcS1149, 2, 3);
  #line 74 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L3srcS1149, 3, 4);
  _M0L6_2atmpS2746 = _M0L3dstS1150;
  _M0L6_2atmpS2747 = _M0L3srcS1149;
  #line 75 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0FP48clawteam8clawteam8internal1c6memcpy(_M0L6_2atmpS2746, _M0L6_2atmpS2747, 4ull);
  #line 76 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2748
  = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L3dstS1150, 0);
  _M0L6_2atmpS2749 = 0;
  #line 76 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _tmp_3175
  = _M0FPB10assert__eqGyE(_M0L6_2atmpS2748, 1, _M0L6_2atmpS2749, (moonbit_string_t)moonbit_string_literal_9.data);
  if (_tmp_3175.tag) {
    int32_t const _M0L5_2aokS2750 = _tmp_3175.data.ok;
  } else {
    void* const _M0L6_2aerrS2751 = _tmp_3175.data.err;
    struct moonbit_result_0 _result_3176;
    _result_3176.tag = 0;
    _result_3176.data.err = _M0L6_2aerrS2751;
    return _result_3176;
  }
  #line 77 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2752
  = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L3dstS1150, 3);
  _M0L6_2atmpS2753 = 0;
  #line 77 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _tmp_3177
  = _M0FPB10assert__eqGyE(_M0L6_2atmpS2752, 4, _M0L6_2atmpS2753, (moonbit_string_t)moonbit_string_literal_10.data);
  if (_tmp_3177.tag) {
    int32_t const _M0L5_2aokS2754 = _tmp_3177.data.ok;
  } else {
    void* const _M0L6_2aerrS2755 = _tmp_3177.data.err;
    struct moonbit_result_0 _result_3178;
    _result_3178.tag = 0;
    _result_3178.data.err = _M0L6_2aerrS2755;
    return _result_3178;
  }
  #line 78 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0FP48clawteam8clawteam8internal1c4freeGyE(_M0L3srcS1149);
  #line 79 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2756
  = _M0FP48clawteam8clawteam8internal1c4freeGyE(_M0L3dstS1150);
  _result_3179.tag = 1;
  _result_3179.data.ok = _M0L6_2atmpS2756;
  return _result_3179;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__1(
  
) {
  void* _M0L1sS1148;
  uint64_t _M0L6_2atmpS2741;
  struct _M0Y6UInt64* _M0L14_2aboxed__selfS2742;
  struct _M0TPB6ToJson _M0L6_2atmpS2732;
  void* _M0L6_2atmpS2740;
  void* _M0L6_2atmpS2733;
  moonbit_string_t _M0L6_2atmpS2736;
  moonbit_string_t _M0L6_2atmpS2737;
  moonbit_string_t _M0L6_2atmpS2738;
  moonbit_string_t _M0L6_2atmpS2739;
  moonbit_string_t* _M0L6_2atmpS2735;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2734;
  struct moonbit_result_0 _tmp_3180;
  int32_t _M0L6_2atmpS2745;
  struct moonbit_result_0 _result_3182;
  #line 48 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  #line 49 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L1sS1148 = _M0FP48clawteam8clawteam8internal1c6mallocGyE(6ull);
  #line 50 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1148, 0, 104);
  #line 51 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1148, 1, 101);
  #line 52 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1148, 2, 108);
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1148, 3, 108);
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1148, 4, 111);
  #line 55 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1148, 5, 0);
  #line 56 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2741 = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L1sS1148);
  _M0L14_2aboxed__selfS2742
  = (struct _M0Y6UInt64*)moonbit_malloc(sizeof(struct _M0Y6UInt64));
  Moonbit_object_header(_M0L14_2aboxed__selfS2742)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y6UInt64) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2742->$0 = _M0L6_2atmpS2741;
  _M0L6_2atmpS2732
  = (struct _M0TPB6ToJson){
    _M0FP081UInt64_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2742
  };
  #line 56 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2740
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L6_2atmpS2733 = _M0L6_2atmpS2740;
  _M0L6_2atmpS2736 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS2737 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS2738 = 0;
  _M0L6_2atmpS2739 = 0;
  _M0L6_2atmpS2735 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2735[0] = _M0L6_2atmpS2736;
  _M0L6_2atmpS2735[1] = _M0L6_2atmpS2737;
  _M0L6_2atmpS2735[2] = _M0L6_2atmpS2738;
  _M0L6_2atmpS2735[3] = _M0L6_2atmpS2739;
  _M0L6_2atmpS2734
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2734)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2734->$0 = _M0L6_2atmpS2735;
  _M0L6_2atmpS2734->$1 = 4;
  #line 56 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _tmp_3180
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2732, _M0L6_2atmpS2733, (moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS2734);
  if (_tmp_3180.tag) {
    int32_t const _M0L5_2aokS2743 = _tmp_3180.data.ok;
  } else {
    void* const _M0L6_2aerrS2744 = _tmp_3180.data.err;
    struct moonbit_result_0 _result_3181;
    _result_3181.tag = 0;
    _result_3181.data.err = _M0L6_2aerrS2744;
    return _result_3181;
  }
  #line 57 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2745 = _M0FP48clawteam8clawteam8internal1c4freeGyE(_M0L1sS1148);
  _result_3182.tag = 1;
  _result_3182.data.ok = _M0L6_2atmpS2745;
  return _result_3182;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test39____test__524541444d452e6d62742e6d64__0(
  
) {
  void* _M0L3ptrS1146;
  int32_t _M0L5valueS1147;
  moonbit_string_t _M0L6_2atmpS2728;
  struct moonbit_result_0 _tmp_3183;
  int32_t _M0L6_2atmpS2731;
  struct moonbit_result_0 _result_3185;
  #line 21 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  #line 23 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L3ptrS1146 = _M0FP48clawteam8clawteam8internal1c6mallocGiE(8ull);
  #line 25 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0MP48clawteam8clawteam8internal1c7Pointer13store_2einnerGiE(_M0L3ptrS1146, 0, 42);
  #line 27 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L5valueS1147
  = _M0MP48clawteam8clawteam8internal1c7Pointer12load_2einnerGiE(_M0L3ptrS1146, 0);
  _M0L6_2atmpS2728 = 0;
  #line 28 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _tmp_3183
  = _M0FPB10assert__eqGiE(_M0L5valueS1147, 42, _M0L6_2atmpS2728, (moonbit_string_t)moonbit_string_literal_15.data);
  if (_tmp_3183.tag) {
    int32_t const _M0L5_2aokS2729 = _tmp_3183.data.ok;
  } else {
    void* const _M0L6_2aerrS2730 = _tmp_3183.data.err;
    struct moonbit_result_0 _result_3184;
    _result_3184.tag = 0;
    _result_3184.data.err = _M0L6_2aerrS2730;
    return _result_3184;
  }
  #line 30 "E:\\moonbit\\clawteam\\internal\\c\\README.mbt.md"
  _M0L6_2atmpS2731
  = _M0FP48clawteam8clawteam8internal1c4freeGiE(_M0L3ptrS1146);
  _result_3185.tag = 1;
  _result_3185.data.ok = _M0L6_2atmpS2731;
  return _result_3185;
}

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1144,
  struct _M0TWEOc* _M0L6on__okS1145,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1142
) {
  void* _M0L11_2atry__errS1140;
  struct moonbit_result_0 _tmp_3187;
  void* _M0L3errS1141;
  #line 375 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3187 = _M0L1fS1144->code(_M0L1fS1144);
  if (_tmp_3187.tag) {
    int32_t const _M0L5_2aokS2726 = _tmp_3187.data.ok;
    moonbit_decref(_M0L7on__errS1142);
  } else {
    void* const _M0L6_2aerrS2727 = _tmp_3187.data.err;
    moonbit_decref(_M0L6on__okS1145);
    _M0L11_2atry__errS1140 = _M0L6_2aerrS2727;
    goto join_1139;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1145->code(_M0L6on__okS1145);
  goto joinlet_3186;
  join_1139:;
  _M0L3errS1141 = _M0L11_2atry__errS1140;
  #line 383 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1142->code(_M0L7on__errS1142, _M0L3errS1141);
  joinlet_3186:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1099;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1112;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1117;
  struct _M0TUsiE** _M0L6_2atmpS2725;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1124;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1125;
  moonbit_string_t _M0L6_2atmpS2724;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1126;
  int32_t _M0L7_2abindS1127;
  int32_t _M0L2__S1128;
  #line 193 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1099 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1112
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1117 = 0;
  _M0L6_2atmpS2725 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1124
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1124)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1124->$0 = _M0L6_2atmpS2725;
  _M0L16file__and__indexS1124->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1125
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1112(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1112);
  #line 284 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2724 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1125, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1126
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1117(_M0L51moonbit__test__driver__internal__split__mbt__stringS1117, _M0L6_2atmpS2724, 47);
  _M0L7_2abindS1127 = _M0L10test__argsS1126->$1;
  _M0L2__S1128 = 0;
  while (1) {
    if (_M0L2__S1128 < _M0L7_2abindS1127) {
      moonbit_string_t* _M0L8_2afieldS2834 = _M0L10test__argsS1126->$0;
      moonbit_string_t* _M0L3bufS2723 = _M0L8_2afieldS2834;
      moonbit_string_t _M0L6_2atmpS2833 =
        (moonbit_string_t)_M0L3bufS2723[_M0L2__S1128];
      moonbit_string_t _M0L3argS1129 = _M0L6_2atmpS2833;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1130;
      moonbit_string_t _M0L4fileS1131;
      moonbit_string_t _M0L5rangeS1132;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1133;
      moonbit_string_t _M0L6_2atmpS2721;
      int32_t _M0L5startS1134;
      moonbit_string_t _M0L6_2atmpS2720;
      int32_t _M0L3endS1135;
      int32_t _M0L1iS1136;
      int32_t _M0L6_2atmpS2722;
      moonbit_incref(_M0L3argS1129);
      #line 288 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1130
      = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1117(_M0L51moonbit__test__driver__internal__split__mbt__stringS1117, _M0L3argS1129, 58);
      moonbit_incref(_M0L16file__and__rangeS1130);
      #line 289 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1131
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1130, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1132
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1130, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1133
      = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1117(_M0L51moonbit__test__driver__internal__split__mbt__stringS1117, _M0L5rangeS1132, 45);
      moonbit_incref(_M0L15start__and__endS1133);
      #line 294 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2721
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1133, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1134
      = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1099(_M0L45moonbit__test__driver__internal__parse__int__S1099, _M0L6_2atmpS2721);
      #line 295 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2720
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1133, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1135
      = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1099(_M0L45moonbit__test__driver__internal__parse__int__S1099, _M0L6_2atmpS2720);
      _M0L1iS1136 = _M0L5startS1134;
      while (1) {
        if (_M0L1iS1136 < _M0L3endS1135) {
          struct _M0TUsiE* _M0L8_2atupleS2718;
          int32_t _M0L6_2atmpS2719;
          moonbit_incref(_M0L4fileS1131);
          _M0L8_2atupleS2718
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2718)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2718->$0 = _M0L4fileS1131;
          _M0L8_2atupleS2718->$1 = _M0L1iS1136;
          moonbit_incref(_M0L16file__and__indexS1124);
          #line 297 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1124, _M0L8_2atupleS2718);
          _M0L6_2atmpS2719 = _M0L1iS1136 + 1;
          _M0L1iS1136 = _M0L6_2atmpS2719;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1131);
        }
        break;
      }
      _M0L6_2atmpS2722 = _M0L2__S1128 + 1;
      _M0L2__S1128 = _M0L6_2atmpS2722;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1126);
    }
    break;
  }
  return _M0L16file__and__indexS1124;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1117(
  int32_t _M0L6_2aenvS2699,
  moonbit_string_t _M0L1sS1118,
  int32_t _M0L3sepS1119
) {
  moonbit_string_t* _M0L6_2atmpS2717;
  struct _M0TPB5ArrayGsE* _M0L3resS1120;
  struct _M0TPC13ref3RefGiE* _M0L1iS1121;
  struct _M0TPC13ref3RefGiE* _M0L5startS1122;
  int32_t _M0L3valS2712;
  int32_t _M0L6_2atmpS2713;
  #line 261 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2717 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1120
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1120)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1120->$0 = _M0L6_2atmpS2717;
  _M0L3resS1120->$1 = 0;
  _M0L1iS1121
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1121)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1121->$0 = 0;
  _M0L5startS1122
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1122)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1122->$0 = 0;
  while (1) {
    int32_t _M0L3valS2700 = _M0L1iS1121->$0;
    int32_t _M0L6_2atmpS2701 = Moonbit_array_length(_M0L1sS1118);
    if (_M0L3valS2700 < _M0L6_2atmpS2701) {
      int32_t _M0L3valS2704 = _M0L1iS1121->$0;
      int32_t _M0L6_2atmpS2703;
      int32_t _M0L6_2atmpS2702;
      int32_t _M0L3valS2711;
      int32_t _M0L6_2atmpS2710;
      if (
        _M0L3valS2704 < 0
        || _M0L3valS2704 >= Moonbit_array_length(_M0L1sS1118)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2703 = _M0L1sS1118[_M0L3valS2704];
      _M0L6_2atmpS2702 = _M0L6_2atmpS2703;
      if (_M0L6_2atmpS2702 == _M0L3sepS1119) {
        int32_t _M0L3valS2706 = _M0L5startS1122->$0;
        int32_t _M0L3valS2707 = _M0L1iS1121->$0;
        moonbit_string_t _M0L6_2atmpS2705;
        int32_t _M0L3valS2709;
        int32_t _M0L6_2atmpS2708;
        moonbit_incref(_M0L1sS1118);
        #line 270 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS2705
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1118, _M0L3valS2706, _M0L3valS2707);
        moonbit_incref(_M0L3resS1120);
        #line 270 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1120, _M0L6_2atmpS2705);
        _M0L3valS2709 = _M0L1iS1121->$0;
        _M0L6_2atmpS2708 = _M0L3valS2709 + 1;
        _M0L5startS1122->$0 = _M0L6_2atmpS2708;
      }
      _M0L3valS2711 = _M0L1iS1121->$0;
      _M0L6_2atmpS2710 = _M0L3valS2711 + 1;
      _M0L1iS1121->$0 = _M0L6_2atmpS2710;
      continue;
    } else {
      moonbit_decref(_M0L1iS1121);
    }
    break;
  }
  _M0L3valS2712 = _M0L5startS1122->$0;
  _M0L6_2atmpS2713 = Moonbit_array_length(_M0L1sS1118);
  if (_M0L3valS2712 < _M0L6_2atmpS2713) {
    int32_t _M0L8_2afieldS2835 = _M0L5startS1122->$0;
    int32_t _M0L3valS2715;
    int32_t _M0L6_2atmpS2716;
    moonbit_string_t _M0L6_2atmpS2714;
    moonbit_decref(_M0L5startS1122);
    _M0L3valS2715 = _M0L8_2afieldS2835;
    _M0L6_2atmpS2716 = Moonbit_array_length(_M0L1sS1118);
    #line 276 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS2714
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1118, _M0L3valS2715, _M0L6_2atmpS2716);
    moonbit_incref(_M0L3resS1120);
    #line 276 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1120, _M0L6_2atmpS2714);
  } else {
    moonbit_decref(_M0L5startS1122);
    moonbit_decref(_M0L1sS1118);
  }
  return _M0L3resS1120;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1112(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105
) {
  moonbit_bytes_t* _M0L3tmpS1113;
  int32_t _M0L6_2atmpS2698;
  struct _M0TPB5ArrayGsE* _M0L3resS1114;
  int32_t _M0L1iS1115;
  #line 250 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1113
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2698 = Moonbit_array_length(_M0L3tmpS1113);
  #line 254 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1114 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2698);
  _M0L1iS1115 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2694 = Moonbit_array_length(_M0L3tmpS1113);
    if (_M0L1iS1115 < _M0L6_2atmpS2694) {
      moonbit_bytes_t _M0L6_2atmpS2836;
      moonbit_bytes_t _M0L6_2atmpS2696;
      moonbit_string_t _M0L6_2atmpS2695;
      int32_t _M0L6_2atmpS2697;
      if (
        _M0L1iS1115 < 0 || _M0L1iS1115 >= Moonbit_array_length(_M0L3tmpS1113)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2836 = (moonbit_bytes_t)_M0L3tmpS1113[_M0L1iS1115];
      _M0L6_2atmpS2696 = _M0L6_2atmpS2836;
      moonbit_incref(_M0L6_2atmpS2696);
      #line 256 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2695
      = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105, _M0L6_2atmpS2696);
      moonbit_incref(_M0L3resS1114);
      #line 256 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1114, _M0L6_2atmpS2695);
      _M0L6_2atmpS2697 = _M0L1iS1115 + 1;
      _M0L1iS1115 = _M0L6_2atmpS2697;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1113);
    }
    break;
  }
  return _M0L3resS1114;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1105(
  int32_t _M0L6_2aenvS2608,
  moonbit_bytes_t _M0L5bytesS1106
) {
  struct _M0TPB13StringBuilder* _M0L3resS1107;
  int32_t _M0L3lenS1108;
  struct _M0TPC13ref3RefGiE* _M0L1iS1109;
  #line 206 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1107 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1108 = Moonbit_array_length(_M0L5bytesS1106);
  _M0L1iS1109
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1109)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1109->$0 = 0;
  while (1) {
    int32_t _M0L3valS2609 = _M0L1iS1109->$0;
    if (_M0L3valS2609 < _M0L3lenS1108) {
      int32_t _M0L3valS2693 = _M0L1iS1109->$0;
      int32_t _M0L6_2atmpS2692;
      int32_t _M0L6_2atmpS2691;
      struct _M0TPC13ref3RefGiE* _M0L1cS1110;
      int32_t _M0L3valS2610;
      if (
        _M0L3valS2693 < 0
        || _M0L3valS2693 >= Moonbit_array_length(_M0L5bytesS1106)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2692 = _M0L5bytesS1106[_M0L3valS2693];
      _M0L6_2atmpS2691 = (int32_t)_M0L6_2atmpS2692;
      _M0L1cS1110
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1110)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1110->$0 = _M0L6_2atmpS2691;
      _M0L3valS2610 = _M0L1cS1110->$0;
      if (_M0L3valS2610 < 128) {
        int32_t _M0L8_2afieldS2837 = _M0L1cS1110->$0;
        int32_t _M0L3valS2612;
        int32_t _M0L6_2atmpS2611;
        int32_t _M0L3valS2614;
        int32_t _M0L6_2atmpS2613;
        moonbit_decref(_M0L1cS1110);
        _M0L3valS2612 = _M0L8_2afieldS2837;
        _M0L6_2atmpS2611 = _M0L3valS2612;
        moonbit_incref(_M0L3resS1107);
        #line 215 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1107, _M0L6_2atmpS2611);
        _M0L3valS2614 = _M0L1iS1109->$0;
        _M0L6_2atmpS2613 = _M0L3valS2614 + 1;
        _M0L1iS1109->$0 = _M0L6_2atmpS2613;
      } else {
        int32_t _M0L3valS2615 = _M0L1cS1110->$0;
        if (_M0L3valS2615 < 224) {
          int32_t _M0L3valS2617 = _M0L1iS1109->$0;
          int32_t _M0L6_2atmpS2616 = _M0L3valS2617 + 1;
          int32_t _M0L3valS2626;
          int32_t _M0L6_2atmpS2625;
          int32_t _M0L6_2atmpS2619;
          int32_t _M0L3valS2624;
          int32_t _M0L6_2atmpS2623;
          int32_t _M0L6_2atmpS2622;
          int32_t _M0L6_2atmpS2621;
          int32_t _M0L6_2atmpS2620;
          int32_t _M0L6_2atmpS2618;
          int32_t _M0L8_2afieldS2838;
          int32_t _M0L3valS2628;
          int32_t _M0L6_2atmpS2627;
          int32_t _M0L3valS2630;
          int32_t _M0L6_2atmpS2629;
          if (_M0L6_2atmpS2616 >= _M0L3lenS1108) {
            moonbit_decref(_M0L1cS1110);
            moonbit_decref(_M0L1iS1109);
            moonbit_decref(_M0L5bytesS1106);
            break;
          }
          _M0L3valS2626 = _M0L1cS1110->$0;
          _M0L6_2atmpS2625 = _M0L3valS2626 & 31;
          _M0L6_2atmpS2619 = _M0L6_2atmpS2625 << 6;
          _M0L3valS2624 = _M0L1iS1109->$0;
          _M0L6_2atmpS2623 = _M0L3valS2624 + 1;
          if (
            _M0L6_2atmpS2623 < 0
            || _M0L6_2atmpS2623 >= Moonbit_array_length(_M0L5bytesS1106)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2622 = _M0L5bytesS1106[_M0L6_2atmpS2623];
          _M0L6_2atmpS2621 = (int32_t)_M0L6_2atmpS2622;
          _M0L6_2atmpS2620 = _M0L6_2atmpS2621 & 63;
          _M0L6_2atmpS2618 = _M0L6_2atmpS2619 | _M0L6_2atmpS2620;
          _M0L1cS1110->$0 = _M0L6_2atmpS2618;
          _M0L8_2afieldS2838 = _M0L1cS1110->$0;
          moonbit_decref(_M0L1cS1110);
          _M0L3valS2628 = _M0L8_2afieldS2838;
          _M0L6_2atmpS2627 = _M0L3valS2628;
          moonbit_incref(_M0L3resS1107);
          #line 222 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1107, _M0L6_2atmpS2627);
          _M0L3valS2630 = _M0L1iS1109->$0;
          _M0L6_2atmpS2629 = _M0L3valS2630 + 2;
          _M0L1iS1109->$0 = _M0L6_2atmpS2629;
        } else {
          int32_t _M0L3valS2631 = _M0L1cS1110->$0;
          if (_M0L3valS2631 < 240) {
            int32_t _M0L3valS2633 = _M0L1iS1109->$0;
            int32_t _M0L6_2atmpS2632 = _M0L3valS2633 + 2;
            int32_t _M0L3valS2649;
            int32_t _M0L6_2atmpS2648;
            int32_t _M0L6_2atmpS2641;
            int32_t _M0L3valS2647;
            int32_t _M0L6_2atmpS2646;
            int32_t _M0L6_2atmpS2645;
            int32_t _M0L6_2atmpS2644;
            int32_t _M0L6_2atmpS2643;
            int32_t _M0L6_2atmpS2642;
            int32_t _M0L6_2atmpS2635;
            int32_t _M0L3valS2640;
            int32_t _M0L6_2atmpS2639;
            int32_t _M0L6_2atmpS2638;
            int32_t _M0L6_2atmpS2637;
            int32_t _M0L6_2atmpS2636;
            int32_t _M0L6_2atmpS2634;
            int32_t _M0L8_2afieldS2839;
            int32_t _M0L3valS2651;
            int32_t _M0L6_2atmpS2650;
            int32_t _M0L3valS2653;
            int32_t _M0L6_2atmpS2652;
            if (_M0L6_2atmpS2632 >= _M0L3lenS1108) {
              moonbit_decref(_M0L1cS1110);
              moonbit_decref(_M0L1iS1109);
              moonbit_decref(_M0L5bytesS1106);
              break;
            }
            _M0L3valS2649 = _M0L1cS1110->$0;
            _M0L6_2atmpS2648 = _M0L3valS2649 & 15;
            _M0L6_2atmpS2641 = _M0L6_2atmpS2648 << 12;
            _M0L3valS2647 = _M0L1iS1109->$0;
            _M0L6_2atmpS2646 = _M0L3valS2647 + 1;
            if (
              _M0L6_2atmpS2646 < 0
              || _M0L6_2atmpS2646 >= Moonbit_array_length(_M0L5bytesS1106)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2645 = _M0L5bytesS1106[_M0L6_2atmpS2646];
            _M0L6_2atmpS2644 = (int32_t)_M0L6_2atmpS2645;
            _M0L6_2atmpS2643 = _M0L6_2atmpS2644 & 63;
            _M0L6_2atmpS2642 = _M0L6_2atmpS2643 << 6;
            _M0L6_2atmpS2635 = _M0L6_2atmpS2641 | _M0L6_2atmpS2642;
            _M0L3valS2640 = _M0L1iS1109->$0;
            _M0L6_2atmpS2639 = _M0L3valS2640 + 2;
            if (
              _M0L6_2atmpS2639 < 0
              || _M0L6_2atmpS2639 >= Moonbit_array_length(_M0L5bytesS1106)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2638 = _M0L5bytesS1106[_M0L6_2atmpS2639];
            _M0L6_2atmpS2637 = (int32_t)_M0L6_2atmpS2638;
            _M0L6_2atmpS2636 = _M0L6_2atmpS2637 & 63;
            _M0L6_2atmpS2634 = _M0L6_2atmpS2635 | _M0L6_2atmpS2636;
            _M0L1cS1110->$0 = _M0L6_2atmpS2634;
            _M0L8_2afieldS2839 = _M0L1cS1110->$0;
            moonbit_decref(_M0L1cS1110);
            _M0L3valS2651 = _M0L8_2afieldS2839;
            _M0L6_2atmpS2650 = _M0L3valS2651;
            moonbit_incref(_M0L3resS1107);
            #line 231 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1107, _M0L6_2atmpS2650);
            _M0L3valS2653 = _M0L1iS1109->$0;
            _M0L6_2atmpS2652 = _M0L3valS2653 + 3;
            _M0L1iS1109->$0 = _M0L6_2atmpS2652;
          } else {
            int32_t _M0L3valS2655 = _M0L1iS1109->$0;
            int32_t _M0L6_2atmpS2654 = _M0L3valS2655 + 3;
            int32_t _M0L3valS2678;
            int32_t _M0L6_2atmpS2677;
            int32_t _M0L6_2atmpS2670;
            int32_t _M0L3valS2676;
            int32_t _M0L6_2atmpS2675;
            int32_t _M0L6_2atmpS2674;
            int32_t _M0L6_2atmpS2673;
            int32_t _M0L6_2atmpS2672;
            int32_t _M0L6_2atmpS2671;
            int32_t _M0L6_2atmpS2663;
            int32_t _M0L3valS2669;
            int32_t _M0L6_2atmpS2668;
            int32_t _M0L6_2atmpS2667;
            int32_t _M0L6_2atmpS2666;
            int32_t _M0L6_2atmpS2665;
            int32_t _M0L6_2atmpS2664;
            int32_t _M0L6_2atmpS2657;
            int32_t _M0L3valS2662;
            int32_t _M0L6_2atmpS2661;
            int32_t _M0L6_2atmpS2660;
            int32_t _M0L6_2atmpS2659;
            int32_t _M0L6_2atmpS2658;
            int32_t _M0L6_2atmpS2656;
            int32_t _M0L3valS2680;
            int32_t _M0L6_2atmpS2679;
            int32_t _M0L3valS2684;
            int32_t _M0L6_2atmpS2683;
            int32_t _M0L6_2atmpS2682;
            int32_t _M0L6_2atmpS2681;
            int32_t _M0L8_2afieldS2840;
            int32_t _M0L3valS2688;
            int32_t _M0L6_2atmpS2687;
            int32_t _M0L6_2atmpS2686;
            int32_t _M0L6_2atmpS2685;
            int32_t _M0L3valS2690;
            int32_t _M0L6_2atmpS2689;
            if (_M0L6_2atmpS2654 >= _M0L3lenS1108) {
              moonbit_decref(_M0L1cS1110);
              moonbit_decref(_M0L1iS1109);
              moonbit_decref(_M0L5bytesS1106);
              break;
            }
            _M0L3valS2678 = _M0L1cS1110->$0;
            _M0L6_2atmpS2677 = _M0L3valS2678 & 7;
            _M0L6_2atmpS2670 = _M0L6_2atmpS2677 << 18;
            _M0L3valS2676 = _M0L1iS1109->$0;
            _M0L6_2atmpS2675 = _M0L3valS2676 + 1;
            if (
              _M0L6_2atmpS2675 < 0
              || _M0L6_2atmpS2675 >= Moonbit_array_length(_M0L5bytesS1106)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2674 = _M0L5bytesS1106[_M0L6_2atmpS2675];
            _M0L6_2atmpS2673 = (int32_t)_M0L6_2atmpS2674;
            _M0L6_2atmpS2672 = _M0L6_2atmpS2673 & 63;
            _M0L6_2atmpS2671 = _M0L6_2atmpS2672 << 12;
            _M0L6_2atmpS2663 = _M0L6_2atmpS2670 | _M0L6_2atmpS2671;
            _M0L3valS2669 = _M0L1iS1109->$0;
            _M0L6_2atmpS2668 = _M0L3valS2669 + 2;
            if (
              _M0L6_2atmpS2668 < 0
              || _M0L6_2atmpS2668 >= Moonbit_array_length(_M0L5bytesS1106)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2667 = _M0L5bytesS1106[_M0L6_2atmpS2668];
            _M0L6_2atmpS2666 = (int32_t)_M0L6_2atmpS2667;
            _M0L6_2atmpS2665 = _M0L6_2atmpS2666 & 63;
            _M0L6_2atmpS2664 = _M0L6_2atmpS2665 << 6;
            _M0L6_2atmpS2657 = _M0L6_2atmpS2663 | _M0L6_2atmpS2664;
            _M0L3valS2662 = _M0L1iS1109->$0;
            _M0L6_2atmpS2661 = _M0L3valS2662 + 3;
            if (
              _M0L6_2atmpS2661 < 0
              || _M0L6_2atmpS2661 >= Moonbit_array_length(_M0L5bytesS1106)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2660 = _M0L5bytesS1106[_M0L6_2atmpS2661];
            _M0L6_2atmpS2659 = (int32_t)_M0L6_2atmpS2660;
            _M0L6_2atmpS2658 = _M0L6_2atmpS2659 & 63;
            _M0L6_2atmpS2656 = _M0L6_2atmpS2657 | _M0L6_2atmpS2658;
            _M0L1cS1110->$0 = _M0L6_2atmpS2656;
            _M0L3valS2680 = _M0L1cS1110->$0;
            _M0L6_2atmpS2679 = _M0L3valS2680 - 65536;
            _M0L1cS1110->$0 = _M0L6_2atmpS2679;
            _M0L3valS2684 = _M0L1cS1110->$0;
            _M0L6_2atmpS2683 = _M0L3valS2684 >> 10;
            _M0L6_2atmpS2682 = _M0L6_2atmpS2683 + 55296;
            _M0L6_2atmpS2681 = _M0L6_2atmpS2682;
            moonbit_incref(_M0L3resS1107);
            #line 242 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1107, _M0L6_2atmpS2681);
            _M0L8_2afieldS2840 = _M0L1cS1110->$0;
            moonbit_decref(_M0L1cS1110);
            _M0L3valS2688 = _M0L8_2afieldS2840;
            _M0L6_2atmpS2687 = _M0L3valS2688 & 1023;
            _M0L6_2atmpS2686 = _M0L6_2atmpS2687 + 56320;
            _M0L6_2atmpS2685 = _M0L6_2atmpS2686;
            moonbit_incref(_M0L3resS1107);
            #line 243 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1107, _M0L6_2atmpS2685);
            _M0L3valS2690 = _M0L1iS1109->$0;
            _M0L6_2atmpS2689 = _M0L3valS2690 + 4;
            _M0L1iS1109->$0 = _M0L6_2atmpS2689;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1109);
      moonbit_decref(_M0L5bytesS1106);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1107);
}

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1099(
  int32_t _M0L6_2aenvS2601,
  moonbit_string_t _M0L1sS1100
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1101;
  int32_t _M0L3lenS1102;
  int32_t _M0L1iS1103;
  int32_t _M0L8_2afieldS2841;
  #line 197 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1101
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1101)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1101->$0 = 0;
  _M0L3lenS1102 = Moonbit_array_length(_M0L1sS1100);
  _M0L1iS1103 = 0;
  while (1) {
    if (_M0L1iS1103 < _M0L3lenS1102) {
      int32_t _M0L3valS2606 = _M0L3resS1101->$0;
      int32_t _M0L6_2atmpS2603 = _M0L3valS2606 * 10;
      int32_t _M0L6_2atmpS2605;
      int32_t _M0L6_2atmpS2604;
      int32_t _M0L6_2atmpS2602;
      int32_t _M0L6_2atmpS2607;
      if (
        _M0L1iS1103 < 0 || _M0L1iS1103 >= Moonbit_array_length(_M0L1sS1100)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2605 = _M0L1sS1100[_M0L1iS1103];
      _M0L6_2atmpS2604 = _M0L6_2atmpS2605 - 48;
      _M0L6_2atmpS2602 = _M0L6_2atmpS2603 + _M0L6_2atmpS2604;
      _M0L3resS1101->$0 = _M0L6_2atmpS2602;
      _M0L6_2atmpS2607 = _M0L1iS1103 + 1;
      _M0L1iS1103 = _M0L6_2atmpS2607;
      continue;
    } else {
      moonbit_decref(_M0L1sS1100);
    }
    break;
  }
  _M0L8_2afieldS2841 = _M0L3resS1101->$0;
  moonbit_decref(_M0L3resS1101);
  return _M0L8_2afieldS2841;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1079,
  moonbit_string_t _M0L12_2adiscard__S1080,
  int32_t _M0L12_2adiscard__S1081,
  struct _M0TWssbEu* _M0L12_2adiscard__S1082,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1083
) {
  struct moonbit_result_0 _result_3194;
  #line 34 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1083);
  moonbit_decref(_M0L12_2adiscard__S1082);
  moonbit_decref(_M0L12_2adiscard__S1080);
  moonbit_decref(_M0L12_2adiscard__S1079);
  _result_3194.tag = 1;
  _result_3194.data.ok = 0;
  return _result_3194;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1084,
  moonbit_string_t _M0L12_2adiscard__S1085,
  int32_t _M0L12_2adiscard__S1086,
  struct _M0TWssbEu* _M0L12_2adiscard__S1087,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1088
) {
  struct moonbit_result_0 _result_3195;
  #line 34 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1088);
  moonbit_decref(_M0L12_2adiscard__S1087);
  moonbit_decref(_M0L12_2adiscard__S1085);
  moonbit_decref(_M0L12_2adiscard__S1084);
  _result_3195.tag = 1;
  _result_3195.data.ok = 0;
  return _result_3195;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1089,
  moonbit_string_t _M0L12_2adiscard__S1090,
  int32_t _M0L12_2adiscard__S1091,
  struct _M0TWssbEu* _M0L12_2adiscard__S1092,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1093
) {
  struct moonbit_result_0 _result_3196;
  #line 34 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1093);
  moonbit_decref(_M0L12_2adiscard__S1092);
  moonbit_decref(_M0L12_2adiscard__S1090);
  moonbit_decref(_M0L12_2adiscard__S1089);
  _result_3196.tag = 1;
  _result_3196.data.ok = 0;
  return _result_3196;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal17c__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1094,
  moonbit_string_t _M0L12_2adiscard__S1095,
  int32_t _M0L12_2adiscard__S1096,
  struct _M0TWssbEu* _M0L12_2adiscard__S1097,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1098
) {
  struct moonbit_result_0 _result_3197;
  #line 34 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1098);
  moonbit_decref(_M0L12_2adiscard__S1097);
  moonbit_decref(_M0L12_2adiscard__S1095);
  moonbit_decref(_M0L12_2adiscard__S1094);
  _result_3197.tag = 1;
  _result_3197.data.ok = 0;
  return _result_3197;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal17c__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1078
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1078);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0(
  
) {
  void* _M0L3srcS1064;
  int32_t _M0L7_2abindS1065;
  int32_t _M0L7_2abindS1066;
  int32_t _M0L1iS1067;
  void* _M0L8_2adeferS1069;
  void* _M0L3dstS1071;
  int32_t _M0L7_2abindS1072;
  int32_t _M0L7_2abindS1073;
  int32_t _M0L1iS1074;
  void* _M0L8_2adeferS1076;
  int32_t _M0L16_2adefer__resultS1077;
  int32_t _M0L16_2adefer__resultS1070;
  struct moonbit_result_0 _result_3200;
  #line 2 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0L3srcS1064 = _M0FP48clawteam8clawteam8internal1c6mallocGyE(16ull);
  _M0L7_2abindS1065 = 0;
  _M0L7_2abindS1066 = 16;
  _M0L1iS1067 = _M0L7_2abindS1065;
  while (1) {
    if (_M0L1iS1067 < _M0L7_2abindS1066) {
      int32_t _M0L6_2atmpS2598 = _M0L1iS1067 & 0xff;
      int32_t _M0L6_2atmpS2599;
      #line 5 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
      _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L3srcS1064, _M0L1iS1067, _M0L6_2atmpS2598);
      _M0L6_2atmpS2599 = _M0L1iS1067 + 1;
      _M0L1iS1067 = _M0L6_2atmpS2599;
      continue;
    }
    break;
  }
  _M0L8_2adeferS1069 = _M0L3srcS1064;
  #line 8 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0L3dstS1071 = _M0FP48clawteam8clawteam8internal1c6mallocGyE(16ull);
  _M0L7_2abindS1072 = 0;
  _M0L7_2abindS1073 = 16;
  _M0L1iS1074 = _M0L7_2abindS1072;
  while (1) {
    if (_M0L1iS1074 < _M0L7_2abindS1073) {
      int32_t _M0L6_2atmpS2600;
      #line 10 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
      _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L3dstS1071, _M0L1iS1074, 0);
      _M0L6_2atmpS2600 = _M0L1iS1074 + 1;
      _M0L1iS1074 = _M0L6_2atmpS2600;
      continue;
    }
    break;
  }
  _M0L8_2adeferS1076 = _M0L3dstS1071;
  #line 13 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0L16_2adefer__resultS1077
  = _M0FP48clawteam8clawteam8internal1c6memcpy(_M0L3dstS1071, _M0L3srcS1064, 16ull);
  #line 12 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0N8_2adeferS1076(_M0L8_2adeferS1076);
  _M0L16_2adefer__resultS1070 = _M0L16_2adefer__resultS1077;
  #line 7 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0N8_2adeferS1069(_M0L8_2adeferS1069);
  _result_3200.tag = 1;
  _result_3200.data.ok = _M0L16_2adefer__resultS1070;
  return _result_3200;
}

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0N8_2adeferS1076(
  void* _M0L3dstS1071
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0FP48clawteam8clawteam8internal1c4freeGyE(_M0L3dstS1071);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__6d656d6370795f746573742e6d6274__0N8_2adeferS1069(
  void* _M0L3srcS1064
) {
  #line 7 "E:\\moonbit\\clawteam\\internal\\c\\memcpy_test.mbt"
  _M0FP48clawteam8clawteam8internal1c4freeGyE(_M0L3srcS1064);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal17c__blackbox__test43____test__7374726c656e5f746573742e6d6274__0(
  
) {
  void* _M0L1sS1063;
  uint64_t _M0L6_2atmpS2596;
  struct _M0Y6UInt64* _M0L14_2aboxed__selfS2597;
  struct _M0TPB6ToJson _M0L6_2atmpS2587;
  void* _M0L6_2atmpS2595;
  void* _M0L6_2atmpS2588;
  moonbit_string_t _M0L6_2atmpS2591;
  moonbit_string_t _M0L6_2atmpS2592;
  moonbit_string_t _M0L6_2atmpS2593;
  moonbit_string_t _M0L6_2atmpS2594;
  moonbit_string_t* _M0L6_2atmpS2590;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2589;
  #line 2 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0L1sS1063 = _M0FP48clawteam8clawteam8internal1c6mallocGyE(6ull);
  #line 4 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1063, 0, 104);
  #line 5 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1063, 1, 101);
  #line 6 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1063, 2, 108);
  #line 7 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1063, 3, 108);
  #line 8 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1063, 4, 111);
  #line 9 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(_M0L1sS1063, 5, 0);
  #line 10 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0L6_2atmpS2596 = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L1sS1063);
  _M0L14_2aboxed__selfS2597
  = (struct _M0Y6UInt64*)moonbit_malloc(sizeof(struct _M0Y6UInt64));
  Moonbit_object_header(_M0L14_2aboxed__selfS2597)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y6UInt64) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2597->$0 = _M0L6_2atmpS2596;
  _M0L6_2atmpS2587
  = (struct _M0TPB6ToJson){
    _M0FP081UInt64_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2597
  };
  #line 10 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  _M0L6_2atmpS2595
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L6_2atmpS2588 = _M0L6_2atmpS2595;
  _M0L6_2atmpS2591 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS2592 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS2593 = 0;
  _M0L6_2atmpS2594 = 0;
  _M0L6_2atmpS2590 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2590[0] = _M0L6_2atmpS2591;
  _M0L6_2atmpS2590[1] = _M0L6_2atmpS2592;
  _M0L6_2atmpS2590[2] = _M0L6_2atmpS2593;
  _M0L6_2atmpS2590[3] = _M0L6_2atmpS2594;
  _M0L6_2atmpS2589
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2589)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2589->$0 = _M0L6_2atmpS2590;
  _M0L6_2atmpS2589->$1 = 4;
  #line 10 "E:\\moonbit\\clawteam\\internal\\c\\strlen_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2587, _M0L6_2atmpS2588, (moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS2589);
}

int32_t _M0FP48clawteam8clawteam8internal1c4exitGuE(int32_t _M0L6statusS1062) {
  #line 14 "E:\\moonbit\\clawteam\\internal\\c\\exit.mbt"
  #line 15 "E:\\moonbit\\clawteam\\internal\\c\\exit.mbt"
  _M0FP48clawteam8clawteam8internal1c6__exit(_M0L6statusS1062);
  #line 16 "E:\\moonbit\\clawteam\\internal\\c\\exit.mbt"
  _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal1c6__exit(
  int32_t _M0L8_2aparamS1253
) {
  exit(_M0L8_2aparamS1253);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal1c4freeGyE(void* _M0L3ptrS1060) {
  void* _M0L6_2atmpS2585;
  #line 27 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  _M0L6_2atmpS2585 = _M0L3ptrS1060;
  #line 28 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  _M0FP48clawteam8clawteam8internal1c7c__free(_M0L6_2atmpS2585);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal1c4freeGiE(void* _M0L3ptrS1061) {
  void* _M0L6_2atmpS2586;
  #line 27 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  _M0L6_2atmpS2586 = _M0L3ptrS1061;
  #line 28 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  _M0FP48clawteam8clawteam8internal1c7c__free(_M0L6_2atmpS2586);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal1c7c__free(void* _M0L8_2aparamS1252) {
  moonbit_moonclaw_c_free(_M0L8_2aparamS1252);
  return 0;
}

void* _M0FP48clawteam8clawteam8internal1c6mallocGyE(uint64_t _M0L4sizeS1058) {
  void* _M0L6_2atmpS2583;
  #line 15 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  #line 16 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  _M0L6_2atmpS2583
  = _M0FP48clawteam8clawteam8internal1c9c__malloc(_M0L4sizeS1058);
  return _M0L6_2atmpS2583;
}

void* _M0FP48clawteam8clawteam8internal1c6mallocGiE(uint64_t _M0L4sizeS1059) {
  void* _M0L6_2atmpS2584;
  #line 15 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  #line 16 "E:\\moonbit\\clawteam\\internal\\c\\malloc.mbt"
  _M0L6_2atmpS2584
  = _M0FP48clawteam8clawteam8internal1c9c__malloc(_M0L4sizeS1059);
  return _M0L6_2atmpS2584;
}

int32_t _M0IPC13int3IntP48clawteam8clawteam8internal1c5Store5store(
  void* _M0L7pointerS1055,
  int32_t _M0L6offsetS1056,
  int32_t _M0L5valueS1057
) {
  #line 290 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 295 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0FP48clawteam8clawteam8internal1c22moonbit__c__store__int(_M0L7pointerS1055, _M0L6offsetS1056, _M0L5valueS1057);
  return 0;
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c5Store5store(
  void* _M0L7pointerS1052,
  int32_t _M0L6offsetS1053,
  int32_t _M0L5valueS1054
) {
  #line 263 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 268 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0FP48clawteam8clawteam8internal1c23moonbit__c__store__byte(_M0L7pointerS1052, _M0L6offsetS1053, _M0L5valueS1054);
  return 0;
}

int32_t _M0IPC13int3IntP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS1050,
  int32_t _M0L6offsetS1051
) {
  #line 160 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 161 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c21moonbit__c__load__int(_M0L7pointerS1050, _M0L6offsetS1051);
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS1048,
  int32_t _M0L6offsetS1049
) {
  #line 145 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte(_M0L7pointerS1048, _M0L6offsetS1049);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer13store_2einnerGiE(
  void* _M0L4selfS1045,
  int32_t _M0L6offsetS1046,
  int32_t _M0L5valueS1047
) {
  #line 75 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 80 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0IPC13int3IntP48clawteam8clawteam8internal1c5Store5store(_M0L4selfS1045, _M0L6offsetS1046, _M0L5valueS1047);
  return 0;
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer12load_2einnerGiE(
  void* _M0L4selfS1043,
  int32_t _M0L6offsetS1044
) {
  #line 69 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 70 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC13int3IntP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS1043, _M0L6offsetS1044);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__setGyE(
  void* _M0L4selfS1040,
  int32_t _M0L5indexS1041,
  int32_t _M0L5valueS1042
) {
  #line 59 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 64 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0IPC14byte4ByteP48clawteam8clawteam8internal1c5Store5store(_M0L4selfS1040, _M0L5indexS1041, _M0L5valueS1042);
  return 0;
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void* _M0L4selfS1038,
  int32_t _M0L5indexS1039
) {
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS1038, _M0L5indexS1039);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1033,
  void* _M0L7contentS1035,
  moonbit_string_t _M0L3locS1029,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1031
) {
  moonbit_string_t _M0L3locS1028;
  moonbit_string_t _M0L9args__locS1030;
  void* _M0L6_2atmpS2581;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2582;
  moonbit_string_t _M0L6actualS1032;
  moonbit_string_t _M0L4wantS1034;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1028 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1029);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1030 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1031);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2581 = _M0L3objS1033.$0->$method_0(_M0L3objS1033.$1);
  _M0L6_2atmpS2582 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1032
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2581, 0, 0, _M0L6_2atmpS2582);
  if (_M0L7contentS1035 == 0) {
    void* _M0L6_2atmpS2578;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2579;
    if (_M0L7contentS1035) {
      moonbit_decref(_M0L7contentS1035);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2578
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2579 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1034
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2578, 0, 0, _M0L6_2atmpS2579);
  } else {
    void* _M0L7_2aSomeS1036 = _M0L7contentS1035;
    void* _M0L4_2axS1037 = _M0L7_2aSomeS1036;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2580 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1034
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1037, 0, 0, _M0L6_2atmpS2580);
  }
  moonbit_incref(_M0L4wantS1034);
  moonbit_incref(_M0L6actualS1032);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1032, _M0L4wantS1034)
  ) {
    moonbit_string_t _M0L6_2atmpS2576;
    moonbit_string_t _M0L6_2atmpS2849;
    moonbit_string_t _M0L6_2atmpS2575;
    moonbit_string_t _M0L6_2atmpS2848;
    moonbit_string_t _M0L6_2atmpS2573;
    moonbit_string_t _M0L6_2atmpS2574;
    moonbit_string_t _M0L6_2atmpS2847;
    moonbit_string_t _M0L6_2atmpS2572;
    moonbit_string_t _M0L6_2atmpS2846;
    moonbit_string_t _M0L6_2atmpS2569;
    moonbit_string_t _M0L6_2atmpS2571;
    moonbit_string_t _M0L6_2atmpS2570;
    moonbit_string_t _M0L6_2atmpS2845;
    moonbit_string_t _M0L6_2atmpS2568;
    moonbit_string_t _M0L6_2atmpS2844;
    moonbit_string_t _M0L6_2atmpS2565;
    moonbit_string_t _M0L6_2atmpS2567;
    moonbit_string_t _M0L6_2atmpS2566;
    moonbit_string_t _M0L6_2atmpS2843;
    moonbit_string_t _M0L6_2atmpS2564;
    moonbit_string_t _M0L6_2atmpS2842;
    moonbit_string_t _M0L6_2atmpS2563;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2562;
    struct moonbit_result_0 _result_3201;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2576
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1028);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2849
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_21.data, _M0L6_2atmpS2576);
    moonbit_decref(_M0L6_2atmpS2576);
    _M0L6_2atmpS2575 = _M0L6_2atmpS2849;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2848
    = moonbit_add_string(_M0L6_2atmpS2575, (moonbit_string_t)moonbit_string_literal_22.data);
    moonbit_decref(_M0L6_2atmpS2575);
    _M0L6_2atmpS2573 = _M0L6_2atmpS2848;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2574
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1030);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2847 = moonbit_add_string(_M0L6_2atmpS2573, _M0L6_2atmpS2574);
    moonbit_decref(_M0L6_2atmpS2573);
    moonbit_decref(_M0L6_2atmpS2574);
    _M0L6_2atmpS2572 = _M0L6_2atmpS2847;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2846
    = moonbit_add_string(_M0L6_2atmpS2572, (moonbit_string_t)moonbit_string_literal_23.data);
    moonbit_decref(_M0L6_2atmpS2572);
    _M0L6_2atmpS2569 = _M0L6_2atmpS2846;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2571 = _M0MPC16string6String6escape(_M0L4wantS1034);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2570
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2571);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2845 = moonbit_add_string(_M0L6_2atmpS2569, _M0L6_2atmpS2570);
    moonbit_decref(_M0L6_2atmpS2569);
    moonbit_decref(_M0L6_2atmpS2570);
    _M0L6_2atmpS2568 = _M0L6_2atmpS2845;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2844
    = moonbit_add_string(_M0L6_2atmpS2568, (moonbit_string_t)moonbit_string_literal_24.data);
    moonbit_decref(_M0L6_2atmpS2568);
    _M0L6_2atmpS2565 = _M0L6_2atmpS2844;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2567 = _M0MPC16string6String6escape(_M0L6actualS1032);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2566
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2567);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2843 = moonbit_add_string(_M0L6_2atmpS2565, _M0L6_2atmpS2566);
    moonbit_decref(_M0L6_2atmpS2565);
    moonbit_decref(_M0L6_2atmpS2566);
    _M0L6_2atmpS2564 = _M0L6_2atmpS2843;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2842
    = moonbit_add_string(_M0L6_2atmpS2564, (moonbit_string_t)moonbit_string_literal_25.data);
    moonbit_decref(_M0L6_2atmpS2564);
    _M0L6_2atmpS2563 = _M0L6_2atmpS2842;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2562
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2562)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2562)->$0
    = _M0L6_2atmpS2563;
    _result_3201.tag = 0;
    _result_3201.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2562;
    return _result_3201;
  } else {
    int32_t _M0L6_2atmpS2577;
    struct moonbit_result_0 _result_3202;
    moonbit_decref(_M0L4wantS1034);
    moonbit_decref(_M0L6actualS1032);
    moonbit_decref(_M0L9args__locS1030);
    moonbit_decref(_M0L3locS1028);
    _M0L6_2atmpS2577 = 0;
    _result_3202.tag = 1;
    _result_3202.data.ok = _M0L6_2atmpS2577;
    return _result_3202;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1027,
  int32_t _M0L13escape__slashS999,
  int32_t _M0L6indentS994,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1020
) {
  struct _M0TPB13StringBuilder* _M0L3bufS986;
  void** _M0L6_2atmpS2561;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS987;
  int32_t _M0Lm5depthS988;
  void* _M0L6_2atmpS2560;
  void* _M0L8_2aparamS989;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS986 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2561 = (void**)moonbit_empty_ref_array;
  _M0L5stackS987
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS987)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS987->$0 = _M0L6_2atmpS2561;
  _M0L5stackS987->$1 = 0;
  _M0Lm5depthS988 = 0;
  _M0L6_2atmpS2560 = _M0L4selfS1027;
  _M0L8_2aparamS989 = _M0L6_2atmpS2560;
  _2aloop_1005:;
  while (1) {
    if (_M0L8_2aparamS989 == 0) {
      int32_t _M0L3lenS2522;
      if (_M0L8_2aparamS989) {
        moonbit_decref(_M0L8_2aparamS989);
      }
      _M0L3lenS2522 = _M0L5stackS987->$1;
      if (_M0L3lenS2522 == 0) {
        if (_M0L8replacerS1020) {
          moonbit_decref(_M0L8replacerS1020);
        }
        moonbit_decref(_M0L5stackS987);
        break;
      } else {
        void** _M0L8_2afieldS2857 = _M0L5stackS987->$0;
        void** _M0L3bufS2546 = _M0L8_2afieldS2857;
        int32_t _M0L3lenS2548 = _M0L5stackS987->$1;
        int32_t _M0L6_2atmpS2547 = _M0L3lenS2548 - 1;
        void* _M0L6_2atmpS2856 = (void*)_M0L3bufS2546[_M0L6_2atmpS2547];
        void* _M0L4_2axS1006 = _M0L6_2atmpS2856;
        switch (Moonbit_object_tag(_M0L4_2axS1006)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1007 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1006;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2852 =
              _M0L8_2aArrayS1007->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1008 =
              _M0L8_2afieldS2852;
            int32_t _M0L4_2aiS1009 = _M0L8_2aArrayS1007->$1;
            int32_t _M0L3lenS2534 = _M0L6_2aarrS1008->$1;
            if (_M0L4_2aiS1009 < _M0L3lenS2534) {
              int32_t _if__result_3204;
              void** _M0L8_2afieldS2851;
              void** _M0L3bufS2540;
              void* _M0L6_2atmpS2850;
              void* _M0L7elementS1010;
              int32_t _M0L6_2atmpS2535;
              void* _M0L6_2atmpS2538;
              if (_M0L4_2aiS1009 < 0) {
                _if__result_3204 = 1;
              } else {
                int32_t _M0L3lenS2539 = _M0L6_2aarrS1008->$1;
                _if__result_3204 = _M0L4_2aiS1009 >= _M0L3lenS2539;
              }
              if (_if__result_3204) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS2851 = _M0L6_2aarrS1008->$0;
              _M0L3bufS2540 = _M0L8_2afieldS2851;
              _M0L6_2atmpS2850 = (void*)_M0L3bufS2540[_M0L4_2aiS1009];
              _M0L7elementS1010 = _M0L6_2atmpS2850;
              _M0L6_2atmpS2535 = _M0L4_2aiS1009 + 1;
              _M0L8_2aArrayS1007->$1 = _M0L6_2atmpS2535;
              if (_M0L4_2aiS1009 > 0) {
                int32_t _M0L6_2atmpS2537;
                moonbit_string_t _M0L6_2atmpS2536;
                moonbit_incref(_M0L7elementS1010);
                moonbit_incref(_M0L3bufS986);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 44);
                _M0L6_2atmpS2537 = _M0Lm5depthS988;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2536
                = _M0FPC14json11indent__str(_M0L6_2atmpS2537, _M0L6indentS994);
                moonbit_incref(_M0L3bufS986);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2536);
              } else {
                moonbit_incref(_M0L7elementS1010);
              }
              _M0L6_2atmpS2538 = _M0L7elementS1010;
              _M0L8_2aparamS989 = _M0L6_2atmpS2538;
              goto _2aloop_1005;
            } else {
              int32_t _M0L6_2atmpS2541 = _M0Lm5depthS988;
              void* _M0L6_2atmpS2542;
              int32_t _M0L6_2atmpS2544;
              moonbit_string_t _M0L6_2atmpS2543;
              void* _M0L6_2atmpS2545;
              _M0Lm5depthS988 = _M0L6_2atmpS2541 - 1;
              moonbit_incref(_M0L5stackS987);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2542
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS987);
              if (_M0L6_2atmpS2542) {
                moonbit_decref(_M0L6_2atmpS2542);
              }
              _M0L6_2atmpS2544 = _M0Lm5depthS988;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2543
              = _M0FPC14json11indent__str(_M0L6_2atmpS2544, _M0L6indentS994);
              moonbit_incref(_M0L3bufS986);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2543);
              moonbit_incref(_M0L3bufS986);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 93);
              _M0L6_2atmpS2545 = 0;
              _M0L8_2aparamS989 = _M0L6_2atmpS2545;
              goto _2aloop_1005;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1011 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1006;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS2855 =
              _M0L9_2aObjectS1011->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1012 =
              _M0L8_2afieldS2855;
            int32_t _M0L8_2afirstS1013 = _M0L9_2aObjectS1011->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1014;
            moonbit_incref(_M0L11_2aiteratorS1012);
            moonbit_incref(_M0L9_2aObjectS1011);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1014
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1012);
            if (_M0L7_2abindS1014 == 0) {
              int32_t _M0L6_2atmpS2523;
              void* _M0L6_2atmpS2524;
              int32_t _M0L6_2atmpS2526;
              moonbit_string_t _M0L6_2atmpS2525;
              void* _M0L6_2atmpS2527;
              if (_M0L7_2abindS1014) {
                moonbit_decref(_M0L7_2abindS1014);
              }
              moonbit_decref(_M0L9_2aObjectS1011);
              _M0L6_2atmpS2523 = _M0Lm5depthS988;
              _M0Lm5depthS988 = _M0L6_2atmpS2523 - 1;
              moonbit_incref(_M0L5stackS987);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2524
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS987);
              if (_M0L6_2atmpS2524) {
                moonbit_decref(_M0L6_2atmpS2524);
              }
              _M0L6_2atmpS2526 = _M0Lm5depthS988;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2525
              = _M0FPC14json11indent__str(_M0L6_2atmpS2526, _M0L6indentS994);
              moonbit_incref(_M0L3bufS986);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2525);
              moonbit_incref(_M0L3bufS986);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 125);
              _M0L6_2atmpS2527 = 0;
              _M0L8_2aparamS989 = _M0L6_2atmpS2527;
              goto _2aloop_1005;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1015 = _M0L7_2abindS1014;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1016 = _M0L7_2aSomeS1015;
              moonbit_string_t _M0L8_2afieldS2854 = _M0L4_2axS1016->$0;
              moonbit_string_t _M0L4_2akS1017 = _M0L8_2afieldS2854;
              void* _M0L8_2afieldS2853 = _M0L4_2axS1016->$1;
              int32_t _M0L6_2acntS3083 =
                Moonbit_object_header(_M0L4_2axS1016)->rc;
              void* _M0L4_2avS1018;
              void* _M0Lm2v2S1019;
              moonbit_string_t _M0L6_2atmpS2531;
              void* _M0L6_2atmpS2533;
              void* _M0L6_2atmpS2532;
              if (_M0L6_2acntS3083 > 1) {
                int32_t _M0L11_2anew__cntS3084 = _M0L6_2acntS3083 - 1;
                Moonbit_object_header(_M0L4_2axS1016)->rc
                = _M0L11_2anew__cntS3084;
                moonbit_incref(_M0L8_2afieldS2853);
                moonbit_incref(_M0L4_2akS1017);
              } else if (_M0L6_2acntS3083 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1016);
              }
              _M0L4_2avS1018 = _M0L8_2afieldS2853;
              _M0Lm2v2S1019 = _M0L4_2avS1018;
              if (_M0L8replacerS1020 == 0) {
                moonbit_incref(_M0Lm2v2S1019);
                moonbit_decref(_M0L4_2avS1018);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1021 =
                  _M0L8replacerS1020;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1022 =
                  _M0L7_2aSomeS1021;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1023 =
                  _M0L11_2areplacerS1022;
                void* _M0L7_2abindS1024;
                moonbit_incref(_M0L7_2afuncS1023);
                moonbit_incref(_M0L4_2akS1017);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1024
                = _M0L7_2afuncS1023->code(_M0L7_2afuncS1023, _M0L4_2akS1017, _M0L4_2avS1018);
                if (_M0L7_2abindS1024 == 0) {
                  void* _M0L6_2atmpS2528;
                  if (_M0L7_2abindS1024) {
                    moonbit_decref(_M0L7_2abindS1024);
                  }
                  moonbit_decref(_M0L4_2akS1017);
                  moonbit_decref(_M0L9_2aObjectS1011);
                  _M0L6_2atmpS2528 = 0;
                  _M0L8_2aparamS989 = _M0L6_2atmpS2528;
                  goto _2aloop_1005;
                } else {
                  void* _M0L7_2aSomeS1025 = _M0L7_2abindS1024;
                  void* _M0L4_2avS1026 = _M0L7_2aSomeS1025;
                  _M0Lm2v2S1019 = _M0L4_2avS1026;
                }
              }
              if (!_M0L8_2afirstS1013) {
                int32_t _M0L6_2atmpS2530;
                moonbit_string_t _M0L6_2atmpS2529;
                moonbit_incref(_M0L3bufS986);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 44);
                _M0L6_2atmpS2530 = _M0Lm5depthS988;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2529
                = _M0FPC14json11indent__str(_M0L6_2atmpS2530, _M0L6indentS994);
                moonbit_incref(_M0L3bufS986);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2529);
              }
              moonbit_incref(_M0L3bufS986);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2531
              = _M0FPC14json6escape(_M0L4_2akS1017, _M0L13escape__slashS999);
              moonbit_incref(_M0L3bufS986);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2531);
              moonbit_incref(_M0L3bufS986);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 34);
              moonbit_incref(_M0L3bufS986);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 58);
              if (_M0L6indentS994 > 0) {
                moonbit_incref(_M0L3bufS986);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 32);
              }
              _M0L9_2aObjectS1011->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1011);
              _M0L6_2atmpS2533 = _M0Lm2v2S1019;
              _M0L6_2atmpS2532 = _M0L6_2atmpS2533;
              _M0L8_2aparamS989 = _M0L6_2atmpS2532;
              goto _2aloop_1005;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS990 = _M0L8_2aparamS989;
      void* _M0L8_2avalueS991 = _M0L7_2aSomeS990;
      void* _M0L6_2atmpS2559;
      switch (Moonbit_object_tag(_M0L8_2avalueS991)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS992 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS991;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS2858 =
            _M0L9_2aObjectS992->$0;
          int32_t _M0L6_2acntS3085 =
            Moonbit_object_header(_M0L9_2aObjectS992)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS993;
          if (_M0L6_2acntS3085 > 1) {
            int32_t _M0L11_2anew__cntS3086 = _M0L6_2acntS3085 - 1;
            Moonbit_object_header(_M0L9_2aObjectS992)->rc
            = _M0L11_2anew__cntS3086;
            moonbit_incref(_M0L8_2afieldS2858);
          } else if (_M0L6_2acntS3085 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS992);
          }
          _M0L10_2amembersS993 = _M0L8_2afieldS2858;
          moonbit_incref(_M0L10_2amembersS993);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS993)) {
            moonbit_decref(_M0L10_2amembersS993);
            moonbit_incref(_M0L3bufS986);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, (moonbit_string_t)moonbit_string_literal_26.data);
          } else {
            int32_t _M0L6_2atmpS2554 = _M0Lm5depthS988;
            int32_t _M0L6_2atmpS2556;
            moonbit_string_t _M0L6_2atmpS2555;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2558;
            void* _M0L6ObjectS2557;
            _M0Lm5depthS988 = _M0L6_2atmpS2554 + 1;
            moonbit_incref(_M0L3bufS986);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 123);
            _M0L6_2atmpS2556 = _M0Lm5depthS988;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2555
            = _M0FPC14json11indent__str(_M0L6_2atmpS2556, _M0L6indentS994);
            moonbit_incref(_M0L3bufS986);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2555);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2558
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS993);
            _M0L6ObjectS2557
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2557)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2557)->$0
            = _M0L6_2atmpS2558;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2557)->$1
            = 1;
            moonbit_incref(_M0L5stackS987);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS987, _M0L6ObjectS2557);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS995 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS991;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS2859 =
            _M0L8_2aArrayS995->$0;
          int32_t _M0L6_2acntS3087 =
            Moonbit_object_header(_M0L8_2aArrayS995)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS996;
          if (_M0L6_2acntS3087 > 1) {
            int32_t _M0L11_2anew__cntS3088 = _M0L6_2acntS3087 - 1;
            Moonbit_object_header(_M0L8_2aArrayS995)->rc
            = _M0L11_2anew__cntS3088;
            moonbit_incref(_M0L8_2afieldS2859);
          } else if (_M0L6_2acntS3087 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS995);
          }
          _M0L6_2aarrS996 = _M0L8_2afieldS2859;
          moonbit_incref(_M0L6_2aarrS996);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS996)) {
            moonbit_decref(_M0L6_2aarrS996);
            moonbit_incref(_M0L3bufS986);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, (moonbit_string_t)moonbit_string_literal_27.data);
          } else {
            int32_t _M0L6_2atmpS2550 = _M0Lm5depthS988;
            int32_t _M0L6_2atmpS2552;
            moonbit_string_t _M0L6_2atmpS2551;
            void* _M0L5ArrayS2553;
            _M0Lm5depthS988 = _M0L6_2atmpS2550 + 1;
            moonbit_incref(_M0L3bufS986);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 91);
            _M0L6_2atmpS2552 = _M0Lm5depthS988;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2551
            = _M0FPC14json11indent__str(_M0L6_2atmpS2552, _M0L6indentS994);
            moonbit_incref(_M0L3bufS986);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2551);
            _M0L5ArrayS2553
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2553)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2553)->$0
            = _M0L6_2aarrS996;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2553)->$1
            = 0;
            moonbit_incref(_M0L5stackS987);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS987, _M0L5ArrayS2553);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS997 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS991;
          moonbit_string_t _M0L8_2afieldS2860 = _M0L9_2aStringS997->$0;
          int32_t _M0L6_2acntS3089 =
            Moonbit_object_header(_M0L9_2aStringS997)->rc;
          moonbit_string_t _M0L4_2asS998;
          moonbit_string_t _M0L6_2atmpS2549;
          if (_M0L6_2acntS3089 > 1) {
            int32_t _M0L11_2anew__cntS3090 = _M0L6_2acntS3089 - 1;
            Moonbit_object_header(_M0L9_2aStringS997)->rc
            = _M0L11_2anew__cntS3090;
            moonbit_incref(_M0L8_2afieldS2860);
          } else if (_M0L6_2acntS3089 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS997);
          }
          _M0L4_2asS998 = _M0L8_2afieldS2860;
          moonbit_incref(_M0L3bufS986);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2549
          = _M0FPC14json6escape(_M0L4_2asS998, _M0L13escape__slashS999);
          moonbit_incref(_M0L3bufS986);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L6_2atmpS2549);
          moonbit_incref(_M0L3bufS986);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS986, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1000 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS991;
          double _M0L4_2anS1001 = _M0L9_2aNumberS1000->$0;
          moonbit_string_t _M0L8_2afieldS2861 = _M0L9_2aNumberS1000->$1;
          int32_t _M0L6_2acntS3091 =
            Moonbit_object_header(_M0L9_2aNumberS1000)->rc;
          moonbit_string_t _M0L7_2areprS1002;
          if (_M0L6_2acntS3091 > 1) {
            int32_t _M0L11_2anew__cntS3092 = _M0L6_2acntS3091 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1000)->rc
            = _M0L11_2anew__cntS3092;
            if (_M0L8_2afieldS2861) {
              moonbit_incref(_M0L8_2afieldS2861);
            }
          } else if (_M0L6_2acntS3091 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1000);
          }
          _M0L7_2areprS1002 = _M0L8_2afieldS2861;
          if (_M0L7_2areprS1002 == 0) {
            if (_M0L7_2areprS1002) {
              moonbit_decref(_M0L7_2areprS1002);
            }
            moonbit_incref(_M0L3bufS986);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS986, _M0L4_2anS1001);
          } else {
            moonbit_string_t _M0L7_2aSomeS1003 = _M0L7_2areprS1002;
            moonbit_string_t _M0L4_2arS1004 = _M0L7_2aSomeS1003;
            moonbit_incref(_M0L3bufS986);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, _M0L4_2arS1004);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS986);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, (moonbit_string_t)moonbit_string_literal_28.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS986);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, (moonbit_string_t)moonbit_string_literal_29.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS991);
          moonbit_incref(_M0L3bufS986);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS986, (moonbit_string_t)moonbit_string_literal_30.data);
          break;
        }
      }
      _M0L6_2atmpS2559 = 0;
      _M0L8_2aparamS989 = _M0L6_2atmpS2559;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS986);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS985,
  int32_t _M0L6indentS983
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS983 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS984 = _M0L6indentS983 * _M0L5levelS985;
    switch (_M0L6spacesS984) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_31.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_32.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_33.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_34.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_35.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_36.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_37.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_38.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_39.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2521;
        moonbit_string_t _M0L6_2atmpS2862;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2521
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_40.data, _M0L6spacesS984);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2862
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_31.data, _M0L6_2atmpS2521);
        moonbit_decref(_M0L6_2atmpS2521);
        return _M0L6_2atmpS2862;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS975,
  int32_t _M0L13escape__slashS980
) {
  int32_t _M0L6_2atmpS2520;
  struct _M0TPB13StringBuilder* _M0L3bufS974;
  struct _M0TWEOc* _M0L5_2aitS976;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2520 = Moonbit_array_length(_M0L3strS975);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS974 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2520);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS976 = _M0MPC16string6String4iter(_M0L3strS975);
  while (1) {
    int32_t _M0L7_2abindS977;
    moonbit_incref(_M0L5_2aitS976);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS977 = _M0MPB4Iter4nextGcE(_M0L5_2aitS976);
    if (_M0L7_2abindS977 == -1) {
      moonbit_decref(_M0L5_2aitS976);
    } else {
      int32_t _M0L7_2aSomeS978 = _M0L7_2abindS977;
      int32_t _M0L4_2acS979 = _M0L7_2aSomeS978;
      if (_M0L4_2acS979 == 34) {
        moonbit_incref(_M0L3bufS974);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_41.data);
      } else if (_M0L4_2acS979 == 92) {
        moonbit_incref(_M0L3bufS974);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_42.data);
      } else if (_M0L4_2acS979 == 47) {
        if (_M0L13escape__slashS980) {
          moonbit_incref(_M0L3bufS974);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_43.data);
        } else {
          moonbit_incref(_M0L3bufS974);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, _M0L4_2acS979);
        }
      } else if (_M0L4_2acS979 == 10) {
        moonbit_incref(_M0L3bufS974);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_44.data);
      } else if (_M0L4_2acS979 == 13) {
        moonbit_incref(_M0L3bufS974);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_45.data);
      } else if (_M0L4_2acS979 == 8) {
        moonbit_incref(_M0L3bufS974);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_46.data);
      } else if (_M0L4_2acS979 == 9) {
        moonbit_incref(_M0L3bufS974);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_47.data);
      } else {
        int32_t _M0L4codeS981 = _M0L4_2acS979;
        if (_M0L4codeS981 == 12) {
          moonbit_incref(_M0L3bufS974);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_48.data);
        } else if (_M0L4codeS981 < 32) {
          int32_t _M0L6_2atmpS2519;
          moonbit_string_t _M0L6_2atmpS2518;
          moonbit_incref(_M0L3bufS974);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, (moonbit_string_t)moonbit_string_literal_49.data);
          _M0L6_2atmpS2519 = _M0L4codeS981 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2518 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2519);
          moonbit_incref(_M0L3bufS974);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS974, _M0L6_2atmpS2518);
        } else {
          moonbit_incref(_M0L3bufS974);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS974, _M0L4_2acS979);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS974);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS973
) {
  int32_t _M0L8_2afieldS2863;
  int32_t _M0L3lenS2517;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS2863 = _M0L4selfS973->$1;
  moonbit_decref(_M0L4selfS973);
  _M0L3lenS2517 = _M0L8_2afieldS2863;
  return _M0L3lenS2517 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS970
) {
  int32_t _M0L3lenS969;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS969 = _M0L4selfS970->$1;
  if (_M0L3lenS969 == 0) {
    moonbit_decref(_M0L4selfS970);
    return 0;
  } else {
    int32_t _M0L5indexS971 = _M0L3lenS969 - 1;
    void** _M0L8_2afieldS2867 = _M0L4selfS970->$0;
    void** _M0L3bufS2516 = _M0L8_2afieldS2867;
    void* _M0L6_2atmpS2866 = (void*)_M0L3bufS2516[_M0L5indexS971];
    void* _M0L1vS972 = _M0L6_2atmpS2866;
    void** _M0L8_2afieldS2865 = _M0L4selfS970->$0;
    void** _M0L3bufS2515 = _M0L8_2afieldS2865;
    void* _M0L6_2aoldS2864;
    if (
      _M0L5indexS971 < 0
      || _M0L5indexS971 >= Moonbit_array_length(_M0L3bufS2515)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS2864 = (void*)_M0L3bufS2515[_M0L5indexS971];
    moonbit_incref(_M0L1vS972);
    moonbit_decref(_M0L6_2aoldS2864);
    if (
      _M0L5indexS971 < 0
      || _M0L5indexS971 >= Moonbit_array_length(_M0L3bufS2515)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2515[_M0L5indexS971]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS970->$1 = _M0L5indexS971;
    moonbit_decref(_M0L4selfS970);
    return _M0L1vS972;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS967,
  struct _M0TPB6Logger _M0L6loggerS968
) {
  moonbit_string_t _M0L6_2atmpS2514;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2513;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2514 = _M0L4selfS967;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2513 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2514);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2513, _M0L6loggerS968);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS944,
  struct _M0TPB6Logger _M0L6loggerS966
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2876;
  struct _M0TPC16string10StringView _M0L3pkgS943;
  moonbit_string_t _M0L7_2adataS945;
  int32_t _M0L8_2astartS946;
  int32_t _M0L6_2atmpS2512;
  int32_t _M0L6_2aendS947;
  int32_t _M0Lm9_2acursorS948;
  int32_t _M0Lm13accept__stateS949;
  int32_t _M0Lm10match__endS950;
  int32_t _M0Lm20match__tag__saver__0S951;
  int32_t _M0Lm6tag__0S952;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS953;
  struct _M0TPC16string10StringView _M0L8_2afieldS2875;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS962;
  void* _M0L8_2afieldS2874;
  int32_t _M0L6_2acntS3093;
  void* _M0L16_2apackage__nameS963;
  struct _M0TPC16string10StringView _M0L8_2afieldS2872;
  struct _M0TPC16string10StringView _M0L8filenameS2489;
  struct _M0TPC16string10StringView _M0L8_2afieldS2871;
  struct _M0TPC16string10StringView _M0L11start__lineS2490;
  struct _M0TPC16string10StringView _M0L8_2afieldS2870;
  struct _M0TPC16string10StringView _M0L13start__columnS2491;
  struct _M0TPC16string10StringView _M0L8_2afieldS2869;
  struct _M0TPC16string10StringView _M0L9end__lineS2492;
  struct _M0TPC16string10StringView _M0L8_2afieldS2868;
  int32_t _M0L6_2acntS3097;
  struct _M0TPC16string10StringView _M0L11end__columnS2493;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2876
  = (struct _M0TPC16string10StringView){
    _M0L4selfS944->$0_1, _M0L4selfS944->$0_2, _M0L4selfS944->$0_0
  };
  _M0L3pkgS943 = _M0L8_2afieldS2876;
  moonbit_incref(_M0L3pkgS943.$0);
  moonbit_incref(_M0L3pkgS943.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS945 = _M0MPC16string10StringView4data(_M0L3pkgS943);
  moonbit_incref(_M0L3pkgS943.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS946 = _M0MPC16string10StringView13start__offset(_M0L3pkgS943);
  moonbit_incref(_M0L3pkgS943.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2512 = _M0MPC16string10StringView6length(_M0L3pkgS943);
  _M0L6_2aendS947 = _M0L8_2astartS946 + _M0L6_2atmpS2512;
  _M0Lm9_2acursorS948 = _M0L8_2astartS946;
  _M0Lm13accept__stateS949 = -1;
  _M0Lm10match__endS950 = -1;
  _M0Lm20match__tag__saver__0S951 = -1;
  _M0Lm6tag__0S952 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2504 = _M0Lm9_2acursorS948;
    if (_M0L6_2atmpS2504 < _M0L6_2aendS947) {
      int32_t _M0L6_2atmpS2511 = _M0Lm9_2acursorS948;
      int32_t _M0L10next__charS957;
      int32_t _M0L6_2atmpS2505;
      moonbit_incref(_M0L7_2adataS945);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS957
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS945, _M0L6_2atmpS2511);
      _M0L6_2atmpS2505 = _M0Lm9_2acursorS948;
      _M0Lm9_2acursorS948 = _M0L6_2atmpS2505 + 1;
      if (_M0L10next__charS957 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2506;
          _M0Lm6tag__0S952 = _M0Lm9_2acursorS948;
          _M0L6_2atmpS2506 = _M0Lm9_2acursorS948;
          if (_M0L6_2atmpS2506 < _M0L6_2aendS947) {
            int32_t _M0L6_2atmpS2510 = _M0Lm9_2acursorS948;
            int32_t _M0L10next__charS958;
            int32_t _M0L6_2atmpS2507;
            moonbit_incref(_M0L7_2adataS945);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS958
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS945, _M0L6_2atmpS2510);
            _M0L6_2atmpS2507 = _M0Lm9_2acursorS948;
            _M0Lm9_2acursorS948 = _M0L6_2atmpS2507 + 1;
            if (_M0L10next__charS958 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2508 = _M0Lm9_2acursorS948;
                if (_M0L6_2atmpS2508 < _M0L6_2aendS947) {
                  int32_t _M0L6_2atmpS2509 = _M0Lm9_2acursorS948;
                  _M0Lm9_2acursorS948 = _M0L6_2atmpS2509 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S951 = _M0Lm6tag__0S952;
                  _M0Lm13accept__stateS949 = 0;
                  _M0Lm10match__endS950 = _M0Lm9_2acursorS948;
                  goto join_954;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_954;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_954;
    }
    break;
  }
  goto joinlet_3206;
  join_954:;
  switch (_M0Lm13accept__stateS949) {
    case 0: {
      int32_t _M0L6_2atmpS2502;
      int32_t _M0L6_2atmpS2501;
      int64_t _M0L6_2atmpS2498;
      int32_t _M0L6_2atmpS2500;
      int64_t _M0L6_2atmpS2499;
      struct _M0TPC16string10StringView _M0L13package__nameS955;
      int64_t _M0L6_2atmpS2495;
      int32_t _M0L6_2atmpS2497;
      int64_t _M0L6_2atmpS2496;
      struct _M0TPC16string10StringView _M0L12module__nameS956;
      void* _M0L4SomeS2494;
      moonbit_decref(_M0L3pkgS943.$0);
      _M0L6_2atmpS2502 = _M0Lm20match__tag__saver__0S951;
      _M0L6_2atmpS2501 = _M0L6_2atmpS2502 + 1;
      _M0L6_2atmpS2498 = (int64_t)_M0L6_2atmpS2501;
      _M0L6_2atmpS2500 = _M0Lm10match__endS950;
      _M0L6_2atmpS2499 = (int64_t)_M0L6_2atmpS2500;
      moonbit_incref(_M0L7_2adataS945);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS955
      = _M0MPC16string6String4view(_M0L7_2adataS945, _M0L6_2atmpS2498, _M0L6_2atmpS2499);
      _M0L6_2atmpS2495 = (int64_t)_M0L8_2astartS946;
      _M0L6_2atmpS2497 = _M0Lm20match__tag__saver__0S951;
      _M0L6_2atmpS2496 = (int64_t)_M0L6_2atmpS2497;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS956
      = _M0MPC16string6String4view(_M0L7_2adataS945, _M0L6_2atmpS2495, _M0L6_2atmpS2496);
      _M0L4SomeS2494
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2494)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2494)->$0_0
      = _M0L13package__nameS955.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2494)->$0_1
      = _M0L13package__nameS955.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2494)->$0_2
      = _M0L13package__nameS955.$2;
      _M0L7_2abindS953
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS953)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS953->$0_0 = _M0L12module__nameS956.$0;
      _M0L7_2abindS953->$0_1 = _M0L12module__nameS956.$1;
      _M0L7_2abindS953->$0_2 = _M0L12module__nameS956.$2;
      _M0L7_2abindS953->$1 = _M0L4SomeS2494;
      break;
    }
    default: {
      void* _M0L4NoneS2503;
      moonbit_decref(_M0L7_2adataS945);
      _M0L4NoneS2503
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS953
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS953)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS953->$0_0 = _M0L3pkgS943.$0;
      _M0L7_2abindS953->$0_1 = _M0L3pkgS943.$1;
      _M0L7_2abindS953->$0_2 = _M0L3pkgS943.$2;
      _M0L7_2abindS953->$1 = _M0L4NoneS2503;
      break;
    }
  }
  joinlet_3206:;
  _M0L8_2afieldS2875
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS953->$0_1, _M0L7_2abindS953->$0_2, _M0L7_2abindS953->$0_0
  };
  _M0L15_2amodule__nameS962 = _M0L8_2afieldS2875;
  _M0L8_2afieldS2874 = _M0L7_2abindS953->$1;
  _M0L6_2acntS3093 = Moonbit_object_header(_M0L7_2abindS953)->rc;
  if (_M0L6_2acntS3093 > 1) {
    int32_t _M0L11_2anew__cntS3094 = _M0L6_2acntS3093 - 1;
    Moonbit_object_header(_M0L7_2abindS953)->rc = _M0L11_2anew__cntS3094;
    moonbit_incref(_M0L8_2afieldS2874);
    moonbit_incref(_M0L15_2amodule__nameS962.$0);
  } else if (_M0L6_2acntS3093 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS953);
  }
  _M0L16_2apackage__nameS963 = _M0L8_2afieldS2874;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS963)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS964 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS963;
      struct _M0TPC16string10StringView _M0L8_2afieldS2873 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS964->$0_1,
                                              _M0L7_2aSomeS964->$0_2,
                                              _M0L7_2aSomeS964->$0_0};
      int32_t _M0L6_2acntS3095 = Moonbit_object_header(_M0L7_2aSomeS964)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS965;
      if (_M0L6_2acntS3095 > 1) {
        int32_t _M0L11_2anew__cntS3096 = _M0L6_2acntS3095 - 1;
        Moonbit_object_header(_M0L7_2aSomeS964)->rc = _M0L11_2anew__cntS3096;
        moonbit_incref(_M0L8_2afieldS2873.$0);
      } else if (_M0L6_2acntS3095 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS964);
      }
      _M0L12_2apkg__nameS965 = _M0L8_2afieldS2873;
      if (_M0L6loggerS966.$1) {
        moonbit_incref(_M0L6loggerS966.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L12_2apkg__nameS965);
      if (_M0L6loggerS966.$1) {
        moonbit_incref(_M0L6loggerS966.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS966.$0->$method_3(_M0L6loggerS966.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS963);
      break;
    }
  }
  _M0L8_2afieldS2872
  = (struct _M0TPC16string10StringView){
    _M0L4selfS944->$1_1, _M0L4selfS944->$1_2, _M0L4selfS944->$1_0
  };
  _M0L8filenameS2489 = _M0L8_2afieldS2872;
  moonbit_incref(_M0L8filenameS2489.$0);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L8filenameS2489);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_3(_M0L6loggerS966.$1, 58);
  _M0L8_2afieldS2871
  = (struct _M0TPC16string10StringView){
    _M0L4selfS944->$2_1, _M0L4selfS944->$2_2, _M0L4selfS944->$2_0
  };
  _M0L11start__lineS2490 = _M0L8_2afieldS2871;
  moonbit_incref(_M0L11start__lineS2490.$0);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L11start__lineS2490);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_3(_M0L6loggerS966.$1, 58);
  _M0L8_2afieldS2870
  = (struct _M0TPC16string10StringView){
    _M0L4selfS944->$3_1, _M0L4selfS944->$3_2, _M0L4selfS944->$3_0
  };
  _M0L13start__columnS2491 = _M0L8_2afieldS2870;
  moonbit_incref(_M0L13start__columnS2491.$0);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L13start__columnS2491);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_3(_M0L6loggerS966.$1, 45);
  _M0L8_2afieldS2869
  = (struct _M0TPC16string10StringView){
    _M0L4selfS944->$4_1, _M0L4selfS944->$4_2, _M0L4selfS944->$4_0
  };
  _M0L9end__lineS2492 = _M0L8_2afieldS2869;
  moonbit_incref(_M0L9end__lineS2492.$0);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L9end__lineS2492);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_3(_M0L6loggerS966.$1, 58);
  _M0L8_2afieldS2868
  = (struct _M0TPC16string10StringView){
    _M0L4selfS944->$5_1, _M0L4selfS944->$5_2, _M0L4selfS944->$5_0
  };
  _M0L6_2acntS3097 = Moonbit_object_header(_M0L4selfS944)->rc;
  if (_M0L6_2acntS3097 > 1) {
    int32_t _M0L11_2anew__cntS3103 = _M0L6_2acntS3097 - 1;
    Moonbit_object_header(_M0L4selfS944)->rc = _M0L11_2anew__cntS3103;
    moonbit_incref(_M0L8_2afieldS2868.$0);
  } else if (_M0L6_2acntS3097 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3102 =
      (struct _M0TPC16string10StringView){_M0L4selfS944->$4_1,
                                            _M0L4selfS944->$4_2,
                                            _M0L4selfS944->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3101;
    struct _M0TPC16string10StringView _M0L8_2afieldS3100;
    struct _M0TPC16string10StringView _M0L8_2afieldS3099;
    struct _M0TPC16string10StringView _M0L8_2afieldS3098;
    moonbit_decref(_M0L8_2afieldS3102.$0);
    _M0L8_2afieldS3101
    = (struct _M0TPC16string10StringView){
      _M0L4selfS944->$3_1, _M0L4selfS944->$3_2, _M0L4selfS944->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3101.$0);
    _M0L8_2afieldS3100
    = (struct _M0TPC16string10StringView){
      _M0L4selfS944->$2_1, _M0L4selfS944->$2_2, _M0L4selfS944->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3100.$0);
    _M0L8_2afieldS3099
    = (struct _M0TPC16string10StringView){
      _M0L4selfS944->$1_1, _M0L4selfS944->$1_2, _M0L4selfS944->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3099.$0);
    _M0L8_2afieldS3098
    = (struct _M0TPC16string10StringView){
      _M0L4selfS944->$0_1, _M0L4selfS944->$0_2, _M0L4selfS944->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3098.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS944);
  }
  _M0L11end__columnS2493 = _M0L8_2afieldS2868;
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L11end__columnS2493);
  if (_M0L6loggerS966.$1) {
    moonbit_incref(_M0L6loggerS966.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_3(_M0L6loggerS966.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS966.$0->$method_2(_M0L6loggerS966.$1, _M0L15_2amodule__nameS962);
  return 0;
}

int32_t _M0IPC14byte4BytePB2Eq10not__equal(
  int32_t _M0L4selfS941,
  int32_t _M0L4thatS942
) {
  int32_t _M0L6_2atmpS2487;
  int32_t _M0L6_2atmpS2488;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2487 = (int32_t)_M0L4selfS941;
  _M0L6_2atmpS2488 = (int32_t)_M0L4thatS942;
  return _M0L6_2atmpS2487 != _M0L6_2atmpS2488;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS940) {
  moonbit_string_t _M0L6_2atmpS2486;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2486 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS940);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2486);
  moonbit_decref(_M0L6_2atmpS2486);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS939,
  struct _M0TPB6Logger _M0L6loggerS938
) {
  moonbit_string_t _M0L6_2atmpS2485;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2485 = _M0MPC16double6Double10to__string(_M0L4selfS939);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS938.$0->$method_0(_M0L6loggerS938.$1, _M0L6_2atmpS2485);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS937) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS937);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS924) {
  uint64_t _M0L4bitsS925;
  uint64_t _M0L6_2atmpS2484;
  uint64_t _M0L6_2atmpS2483;
  int32_t _M0L8ieeeSignS926;
  uint64_t _M0L12ieeeMantissaS927;
  uint64_t _M0L6_2atmpS2482;
  uint64_t _M0L6_2atmpS2481;
  int32_t _M0L12ieeeExponentS928;
  int32_t _if__result_3210;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS929;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS930;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2480;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS924 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_50.data;
  }
  _M0L4bitsS925 = *(int64_t*)&_M0L3valS924;
  _M0L6_2atmpS2484 = _M0L4bitsS925 >> 63;
  _M0L6_2atmpS2483 = _M0L6_2atmpS2484 & 1ull;
  _M0L8ieeeSignS926 = _M0L6_2atmpS2483 != 0ull;
  _M0L12ieeeMantissaS927 = _M0L4bitsS925 & 4503599627370495ull;
  _M0L6_2atmpS2482 = _M0L4bitsS925 >> 52;
  _M0L6_2atmpS2481 = _M0L6_2atmpS2482 & 2047ull;
  _M0L12ieeeExponentS928 = (int32_t)_M0L6_2atmpS2481;
  if (_M0L12ieeeExponentS928 == 2047) {
    _if__result_3210 = 1;
  } else if (_M0L12ieeeExponentS928 == 0) {
    _if__result_3210 = _M0L12ieeeMantissaS927 == 0ull;
  } else {
    _if__result_3210 = 0;
  }
  if (_if__result_3210) {
    int32_t _M0L6_2atmpS2469 = _M0L12ieeeExponentS928 != 0;
    int32_t _M0L6_2atmpS2470 = _M0L12ieeeMantissaS927 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS926, _M0L6_2atmpS2469, _M0L6_2atmpS2470);
  }
  _M0Lm1vS929 = _M0FPB30ryu__to__string_2erecord_2f923;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS930
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS927, _M0L12ieeeExponentS928);
  if (_M0L5smallS930 == 0) {
    uint32_t _M0L6_2atmpS2471;
    if (_M0L5smallS930) {
      moonbit_decref(_M0L5smallS930);
    }
    _M0L6_2atmpS2471 = *(uint32_t*)&_M0L12ieeeExponentS928;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS929 = _M0FPB3d2d(_M0L12ieeeMantissaS927, _M0L6_2atmpS2471);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS931 = _M0L5smallS930;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS932 = _M0L7_2aSomeS931;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS933 = _M0L4_2afS932;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2479 = _M0Lm1xS933;
      uint64_t _M0L8_2afieldS2879 = _M0L6_2atmpS2479->$0;
      uint64_t _M0L8mantissaS2478 = _M0L8_2afieldS2879;
      uint64_t _M0L1qS934 = _M0L8mantissaS2478 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2477 = _M0Lm1xS933;
      uint64_t _M0L8_2afieldS2878 = _M0L6_2atmpS2477->$0;
      uint64_t _M0L8mantissaS2475 = _M0L8_2afieldS2878;
      uint64_t _M0L6_2atmpS2476 = 10ull * _M0L1qS934;
      uint64_t _M0L1rS935 = _M0L8mantissaS2475 - _M0L6_2atmpS2476;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2474;
      int32_t _M0L8_2afieldS2877;
      int32_t _M0L8exponentS2473;
      int32_t _M0L6_2atmpS2472;
      if (_M0L1rS935 != 0ull) {
        break;
      }
      _M0L6_2atmpS2474 = _M0Lm1xS933;
      _M0L8_2afieldS2877 = _M0L6_2atmpS2474->$1;
      moonbit_decref(_M0L6_2atmpS2474);
      _M0L8exponentS2473 = _M0L8_2afieldS2877;
      _M0L6_2atmpS2472 = _M0L8exponentS2473 + 1;
      _M0Lm1xS933
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS933)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS933->$0 = _M0L1qS934;
      _M0Lm1xS933->$1 = _M0L6_2atmpS2472;
      continue;
      break;
    }
    _M0Lm1vS929 = _M0Lm1xS933;
  }
  _M0L6_2atmpS2480 = _M0Lm1vS929;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2480, _M0L8ieeeSignS926);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS918,
  int32_t _M0L12ieeeExponentS920
) {
  uint64_t _M0L2m2S917;
  int32_t _M0L6_2atmpS2468;
  int32_t _M0L2e2S919;
  int32_t _M0L6_2atmpS2467;
  uint64_t _M0L6_2atmpS2466;
  uint64_t _M0L4maskS921;
  uint64_t _M0L8fractionS922;
  int32_t _M0L6_2atmpS2465;
  uint64_t _M0L6_2atmpS2464;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2463;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S917 = 4503599627370496ull | _M0L12ieeeMantissaS918;
  _M0L6_2atmpS2468 = _M0L12ieeeExponentS920 - 1023;
  _M0L2e2S919 = _M0L6_2atmpS2468 - 52;
  if (_M0L2e2S919 > 0) {
    return 0;
  }
  if (_M0L2e2S919 < -52) {
    return 0;
  }
  _M0L6_2atmpS2467 = -_M0L2e2S919;
  _M0L6_2atmpS2466 = 1ull << (_M0L6_2atmpS2467 & 63);
  _M0L4maskS921 = _M0L6_2atmpS2466 - 1ull;
  _M0L8fractionS922 = _M0L2m2S917 & _M0L4maskS921;
  if (_M0L8fractionS922 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2465 = -_M0L2e2S919;
  _M0L6_2atmpS2464 = _M0L2m2S917 >> (_M0L6_2atmpS2465 & 63);
  _M0L6_2atmpS2463
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2463)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2463->$0 = _M0L6_2atmpS2464;
  _M0L6_2atmpS2463->$1 = 0;
  return _M0L6_2atmpS2463;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS891,
  int32_t _M0L4signS889
) {
  int32_t _M0L6_2atmpS2462;
  moonbit_bytes_t _M0L6resultS887;
  int32_t _M0Lm5indexS888;
  uint64_t _M0Lm6outputS890;
  uint64_t _M0L6_2atmpS2461;
  int32_t _M0L7olengthS892;
  int32_t _M0L8_2afieldS2880;
  int32_t _M0L8exponentS2460;
  int32_t _M0L6_2atmpS2459;
  int32_t _M0Lm3expS893;
  int32_t _M0L6_2atmpS2458;
  int32_t _M0L6_2atmpS2456;
  int32_t _M0L18scientificNotationS894;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2462 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS887 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2462);
  _M0Lm5indexS888 = 0;
  if (_M0L4signS889) {
    int32_t _M0L6_2atmpS2331 = _M0Lm5indexS888;
    int32_t _M0L6_2atmpS2332;
    if (
      _M0L6_2atmpS2331 < 0
      || _M0L6_2atmpS2331 >= Moonbit_array_length(_M0L6resultS887)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS887[_M0L6_2atmpS2331] = 45;
    _M0L6_2atmpS2332 = _M0Lm5indexS888;
    _M0Lm5indexS888 = _M0L6_2atmpS2332 + 1;
  }
  _M0Lm6outputS890 = _M0L1vS891->$0;
  _M0L6_2atmpS2461 = _M0Lm6outputS890;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS892 = _M0FPB17decimal__length17(_M0L6_2atmpS2461);
  _M0L8_2afieldS2880 = _M0L1vS891->$1;
  moonbit_decref(_M0L1vS891);
  _M0L8exponentS2460 = _M0L8_2afieldS2880;
  _M0L6_2atmpS2459 = _M0L8exponentS2460 + _M0L7olengthS892;
  _M0Lm3expS893 = _M0L6_2atmpS2459 - 1;
  _M0L6_2atmpS2458 = _M0Lm3expS893;
  if (_M0L6_2atmpS2458 >= -6) {
    int32_t _M0L6_2atmpS2457 = _M0Lm3expS893;
    _M0L6_2atmpS2456 = _M0L6_2atmpS2457 < 21;
  } else {
    _M0L6_2atmpS2456 = 0;
  }
  _M0L18scientificNotationS894 = !_M0L6_2atmpS2456;
  if (_M0L18scientificNotationS894) {
    int32_t _M0L7_2abindS895 = _M0L7olengthS892 - 1;
    int32_t _M0L1iS896 = 0;
    int32_t _M0L6_2atmpS2342;
    uint64_t _M0L6_2atmpS2347;
    int32_t _M0L6_2atmpS2346;
    int32_t _M0L6_2atmpS2345;
    int32_t _M0L6_2atmpS2344;
    int32_t _M0L6_2atmpS2343;
    int32_t _M0L6_2atmpS2351;
    int32_t _M0L6_2atmpS2352;
    int32_t _M0L6_2atmpS2353;
    int32_t _M0L6_2atmpS2354;
    int32_t _M0L6_2atmpS2355;
    int32_t _M0L6_2atmpS2361;
    int32_t _M0L6_2atmpS2394;
    while (1) {
      if (_M0L1iS896 < _M0L7_2abindS895) {
        uint64_t _M0L6_2atmpS2340 = _M0Lm6outputS890;
        uint64_t _M0L1cS897 = _M0L6_2atmpS2340 % 10ull;
        uint64_t _M0L6_2atmpS2333 = _M0Lm6outputS890;
        int32_t _M0L6_2atmpS2339;
        int32_t _M0L6_2atmpS2338;
        int32_t _M0L6_2atmpS2334;
        int32_t _M0L6_2atmpS2337;
        int32_t _M0L6_2atmpS2336;
        int32_t _M0L6_2atmpS2335;
        int32_t _M0L6_2atmpS2341;
        _M0Lm6outputS890 = _M0L6_2atmpS2333 / 10ull;
        _M0L6_2atmpS2339 = _M0Lm5indexS888;
        _M0L6_2atmpS2338 = _M0L6_2atmpS2339 + _M0L7olengthS892;
        _M0L6_2atmpS2334 = _M0L6_2atmpS2338 - _M0L1iS896;
        _M0L6_2atmpS2337 = (int32_t)_M0L1cS897;
        _M0L6_2atmpS2336 = 48 + _M0L6_2atmpS2337;
        _M0L6_2atmpS2335 = _M0L6_2atmpS2336 & 0xff;
        if (
          _M0L6_2atmpS2334 < 0
          || _M0L6_2atmpS2334 >= Moonbit_array_length(_M0L6resultS887)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS887[_M0L6_2atmpS2334] = _M0L6_2atmpS2335;
        _M0L6_2atmpS2341 = _M0L1iS896 + 1;
        _M0L1iS896 = _M0L6_2atmpS2341;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2342 = _M0Lm5indexS888;
    _M0L6_2atmpS2347 = _M0Lm6outputS890;
    _M0L6_2atmpS2346 = (int32_t)_M0L6_2atmpS2347;
    _M0L6_2atmpS2345 = _M0L6_2atmpS2346 % 10;
    _M0L6_2atmpS2344 = 48 + _M0L6_2atmpS2345;
    _M0L6_2atmpS2343 = _M0L6_2atmpS2344 & 0xff;
    if (
      _M0L6_2atmpS2342 < 0
      || _M0L6_2atmpS2342 >= Moonbit_array_length(_M0L6resultS887)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS887[_M0L6_2atmpS2342] = _M0L6_2atmpS2343;
    if (_M0L7olengthS892 > 1) {
      int32_t _M0L6_2atmpS2349 = _M0Lm5indexS888;
      int32_t _M0L6_2atmpS2348 = _M0L6_2atmpS2349 + 1;
      if (
        _M0L6_2atmpS2348 < 0
        || _M0L6_2atmpS2348 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2348] = 46;
    } else {
      int32_t _M0L6_2atmpS2350 = _M0Lm5indexS888;
      _M0Lm5indexS888 = _M0L6_2atmpS2350 - 1;
    }
    _M0L6_2atmpS2351 = _M0Lm5indexS888;
    _M0L6_2atmpS2352 = _M0L7olengthS892 + 1;
    _M0Lm5indexS888 = _M0L6_2atmpS2351 + _M0L6_2atmpS2352;
    _M0L6_2atmpS2353 = _M0Lm5indexS888;
    if (
      _M0L6_2atmpS2353 < 0
      || _M0L6_2atmpS2353 >= Moonbit_array_length(_M0L6resultS887)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS887[_M0L6_2atmpS2353] = 101;
    _M0L6_2atmpS2354 = _M0Lm5indexS888;
    _M0Lm5indexS888 = _M0L6_2atmpS2354 + 1;
    _M0L6_2atmpS2355 = _M0Lm3expS893;
    if (_M0L6_2atmpS2355 < 0) {
      int32_t _M0L6_2atmpS2356 = _M0Lm5indexS888;
      int32_t _M0L6_2atmpS2357;
      int32_t _M0L6_2atmpS2358;
      if (
        _M0L6_2atmpS2356 < 0
        || _M0L6_2atmpS2356 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2356] = 45;
      _M0L6_2atmpS2357 = _M0Lm5indexS888;
      _M0Lm5indexS888 = _M0L6_2atmpS2357 + 1;
      _M0L6_2atmpS2358 = _M0Lm3expS893;
      _M0Lm3expS893 = -_M0L6_2atmpS2358;
    } else {
      int32_t _M0L6_2atmpS2359 = _M0Lm5indexS888;
      int32_t _M0L6_2atmpS2360;
      if (
        _M0L6_2atmpS2359 < 0
        || _M0L6_2atmpS2359 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2359] = 43;
      _M0L6_2atmpS2360 = _M0Lm5indexS888;
      _M0Lm5indexS888 = _M0L6_2atmpS2360 + 1;
    }
    _M0L6_2atmpS2361 = _M0Lm3expS893;
    if (_M0L6_2atmpS2361 >= 100) {
      int32_t _M0L6_2atmpS2377 = _M0Lm3expS893;
      int32_t _M0L1aS899 = _M0L6_2atmpS2377 / 100;
      int32_t _M0L6_2atmpS2376 = _M0Lm3expS893;
      int32_t _M0L6_2atmpS2375 = _M0L6_2atmpS2376 / 10;
      int32_t _M0L1bS900 = _M0L6_2atmpS2375 % 10;
      int32_t _M0L6_2atmpS2374 = _M0Lm3expS893;
      int32_t _M0L1cS901 = _M0L6_2atmpS2374 % 10;
      int32_t _M0L6_2atmpS2362 = _M0Lm5indexS888;
      int32_t _M0L6_2atmpS2364 = 48 + _M0L1aS899;
      int32_t _M0L6_2atmpS2363 = _M0L6_2atmpS2364 & 0xff;
      int32_t _M0L6_2atmpS2368;
      int32_t _M0L6_2atmpS2365;
      int32_t _M0L6_2atmpS2367;
      int32_t _M0L6_2atmpS2366;
      int32_t _M0L6_2atmpS2372;
      int32_t _M0L6_2atmpS2369;
      int32_t _M0L6_2atmpS2371;
      int32_t _M0L6_2atmpS2370;
      int32_t _M0L6_2atmpS2373;
      if (
        _M0L6_2atmpS2362 < 0
        || _M0L6_2atmpS2362 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2362] = _M0L6_2atmpS2363;
      _M0L6_2atmpS2368 = _M0Lm5indexS888;
      _M0L6_2atmpS2365 = _M0L6_2atmpS2368 + 1;
      _M0L6_2atmpS2367 = 48 + _M0L1bS900;
      _M0L6_2atmpS2366 = _M0L6_2atmpS2367 & 0xff;
      if (
        _M0L6_2atmpS2365 < 0
        || _M0L6_2atmpS2365 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2365] = _M0L6_2atmpS2366;
      _M0L6_2atmpS2372 = _M0Lm5indexS888;
      _M0L6_2atmpS2369 = _M0L6_2atmpS2372 + 2;
      _M0L6_2atmpS2371 = 48 + _M0L1cS901;
      _M0L6_2atmpS2370 = _M0L6_2atmpS2371 & 0xff;
      if (
        _M0L6_2atmpS2369 < 0
        || _M0L6_2atmpS2369 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2369] = _M0L6_2atmpS2370;
      _M0L6_2atmpS2373 = _M0Lm5indexS888;
      _M0Lm5indexS888 = _M0L6_2atmpS2373 + 3;
    } else {
      int32_t _M0L6_2atmpS2378 = _M0Lm3expS893;
      if (_M0L6_2atmpS2378 >= 10) {
        int32_t _M0L6_2atmpS2388 = _M0Lm3expS893;
        int32_t _M0L1aS902 = _M0L6_2atmpS2388 / 10;
        int32_t _M0L6_2atmpS2387 = _M0Lm3expS893;
        int32_t _M0L1bS903 = _M0L6_2atmpS2387 % 10;
        int32_t _M0L6_2atmpS2379 = _M0Lm5indexS888;
        int32_t _M0L6_2atmpS2381 = 48 + _M0L1aS902;
        int32_t _M0L6_2atmpS2380 = _M0L6_2atmpS2381 & 0xff;
        int32_t _M0L6_2atmpS2385;
        int32_t _M0L6_2atmpS2382;
        int32_t _M0L6_2atmpS2384;
        int32_t _M0L6_2atmpS2383;
        int32_t _M0L6_2atmpS2386;
        if (
          _M0L6_2atmpS2379 < 0
          || _M0L6_2atmpS2379 >= Moonbit_array_length(_M0L6resultS887)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS887[_M0L6_2atmpS2379] = _M0L6_2atmpS2380;
        _M0L6_2atmpS2385 = _M0Lm5indexS888;
        _M0L6_2atmpS2382 = _M0L6_2atmpS2385 + 1;
        _M0L6_2atmpS2384 = 48 + _M0L1bS903;
        _M0L6_2atmpS2383 = _M0L6_2atmpS2384 & 0xff;
        if (
          _M0L6_2atmpS2382 < 0
          || _M0L6_2atmpS2382 >= Moonbit_array_length(_M0L6resultS887)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS887[_M0L6_2atmpS2382] = _M0L6_2atmpS2383;
        _M0L6_2atmpS2386 = _M0Lm5indexS888;
        _M0Lm5indexS888 = _M0L6_2atmpS2386 + 2;
      } else {
        int32_t _M0L6_2atmpS2389 = _M0Lm5indexS888;
        int32_t _M0L6_2atmpS2392 = _M0Lm3expS893;
        int32_t _M0L6_2atmpS2391 = 48 + _M0L6_2atmpS2392;
        int32_t _M0L6_2atmpS2390 = _M0L6_2atmpS2391 & 0xff;
        int32_t _M0L6_2atmpS2393;
        if (
          _M0L6_2atmpS2389 < 0
          || _M0L6_2atmpS2389 >= Moonbit_array_length(_M0L6resultS887)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS887[_M0L6_2atmpS2389] = _M0L6_2atmpS2390;
        _M0L6_2atmpS2393 = _M0Lm5indexS888;
        _M0Lm5indexS888 = _M0L6_2atmpS2393 + 1;
      }
    }
    _M0L6_2atmpS2394 = _M0Lm5indexS888;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS887, 0, _M0L6_2atmpS2394);
  } else {
    int32_t _M0L6_2atmpS2395 = _M0Lm3expS893;
    int32_t _M0L6_2atmpS2455;
    if (_M0L6_2atmpS2395 < 0) {
      int32_t _M0L6_2atmpS2396 = _M0Lm5indexS888;
      int32_t _M0L6_2atmpS2397;
      int32_t _M0L6_2atmpS2398;
      int32_t _M0L6_2atmpS2399;
      int32_t _M0L1iS904;
      int32_t _M0L7currentS906;
      int32_t _M0L1iS907;
      if (
        _M0L6_2atmpS2396 < 0
        || _M0L6_2atmpS2396 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2396] = 48;
      _M0L6_2atmpS2397 = _M0Lm5indexS888;
      _M0Lm5indexS888 = _M0L6_2atmpS2397 + 1;
      _M0L6_2atmpS2398 = _M0Lm5indexS888;
      if (
        _M0L6_2atmpS2398 < 0
        || _M0L6_2atmpS2398 >= Moonbit_array_length(_M0L6resultS887)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS887[_M0L6_2atmpS2398] = 46;
      _M0L6_2atmpS2399 = _M0Lm5indexS888;
      _M0Lm5indexS888 = _M0L6_2atmpS2399 + 1;
      _M0L1iS904 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2400 = _M0Lm3expS893;
        if (_M0L1iS904 > _M0L6_2atmpS2400) {
          int32_t _M0L6_2atmpS2401 = _M0Lm5indexS888;
          int32_t _M0L6_2atmpS2402;
          int32_t _M0L6_2atmpS2403;
          if (
            _M0L6_2atmpS2401 < 0
            || _M0L6_2atmpS2401 >= Moonbit_array_length(_M0L6resultS887)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS887[_M0L6_2atmpS2401] = 48;
          _M0L6_2atmpS2402 = _M0Lm5indexS888;
          _M0Lm5indexS888 = _M0L6_2atmpS2402 + 1;
          _M0L6_2atmpS2403 = _M0L1iS904 - 1;
          _M0L1iS904 = _M0L6_2atmpS2403;
          continue;
        }
        break;
      }
      _M0L7currentS906 = _M0Lm5indexS888;
      _M0L1iS907 = 0;
      while (1) {
        if (_M0L1iS907 < _M0L7olengthS892) {
          int32_t _M0L6_2atmpS2411 = _M0L7currentS906 + _M0L7olengthS892;
          int32_t _M0L6_2atmpS2410 = _M0L6_2atmpS2411 - _M0L1iS907;
          int32_t _M0L6_2atmpS2404 = _M0L6_2atmpS2410 - 1;
          uint64_t _M0L6_2atmpS2409 = _M0Lm6outputS890;
          uint64_t _M0L6_2atmpS2408 = _M0L6_2atmpS2409 % 10ull;
          int32_t _M0L6_2atmpS2407 = (int32_t)_M0L6_2atmpS2408;
          int32_t _M0L6_2atmpS2406 = 48 + _M0L6_2atmpS2407;
          int32_t _M0L6_2atmpS2405 = _M0L6_2atmpS2406 & 0xff;
          uint64_t _M0L6_2atmpS2412;
          int32_t _M0L6_2atmpS2413;
          int32_t _M0L6_2atmpS2414;
          if (
            _M0L6_2atmpS2404 < 0
            || _M0L6_2atmpS2404 >= Moonbit_array_length(_M0L6resultS887)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS887[_M0L6_2atmpS2404] = _M0L6_2atmpS2405;
          _M0L6_2atmpS2412 = _M0Lm6outputS890;
          _M0Lm6outputS890 = _M0L6_2atmpS2412 / 10ull;
          _M0L6_2atmpS2413 = _M0Lm5indexS888;
          _M0Lm5indexS888 = _M0L6_2atmpS2413 + 1;
          _M0L6_2atmpS2414 = _M0L1iS907 + 1;
          _M0L1iS907 = _M0L6_2atmpS2414;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2416 = _M0Lm3expS893;
      int32_t _M0L6_2atmpS2415 = _M0L6_2atmpS2416 + 1;
      if (_M0L6_2atmpS2415 >= _M0L7olengthS892) {
        int32_t _M0L1iS909 = 0;
        int32_t _M0L6_2atmpS2428;
        int32_t _M0L6_2atmpS2432;
        int32_t _M0L7_2abindS911;
        int32_t _M0L2__S912;
        while (1) {
          if (_M0L1iS909 < _M0L7olengthS892) {
            int32_t _M0L6_2atmpS2425 = _M0Lm5indexS888;
            int32_t _M0L6_2atmpS2424 = _M0L6_2atmpS2425 + _M0L7olengthS892;
            int32_t _M0L6_2atmpS2423 = _M0L6_2atmpS2424 - _M0L1iS909;
            int32_t _M0L6_2atmpS2417 = _M0L6_2atmpS2423 - 1;
            uint64_t _M0L6_2atmpS2422 = _M0Lm6outputS890;
            uint64_t _M0L6_2atmpS2421 = _M0L6_2atmpS2422 % 10ull;
            int32_t _M0L6_2atmpS2420 = (int32_t)_M0L6_2atmpS2421;
            int32_t _M0L6_2atmpS2419 = 48 + _M0L6_2atmpS2420;
            int32_t _M0L6_2atmpS2418 = _M0L6_2atmpS2419 & 0xff;
            uint64_t _M0L6_2atmpS2426;
            int32_t _M0L6_2atmpS2427;
            if (
              _M0L6_2atmpS2417 < 0
              || _M0L6_2atmpS2417 >= Moonbit_array_length(_M0L6resultS887)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS887[_M0L6_2atmpS2417] = _M0L6_2atmpS2418;
            _M0L6_2atmpS2426 = _M0Lm6outputS890;
            _M0Lm6outputS890 = _M0L6_2atmpS2426 / 10ull;
            _M0L6_2atmpS2427 = _M0L1iS909 + 1;
            _M0L1iS909 = _M0L6_2atmpS2427;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2428 = _M0Lm5indexS888;
        _M0Lm5indexS888 = _M0L6_2atmpS2428 + _M0L7olengthS892;
        _M0L6_2atmpS2432 = _M0Lm3expS893;
        _M0L7_2abindS911 = _M0L6_2atmpS2432 + 1;
        _M0L2__S912 = _M0L7olengthS892;
        while (1) {
          if (_M0L2__S912 < _M0L7_2abindS911) {
            int32_t _M0L6_2atmpS2429 = _M0Lm5indexS888;
            int32_t _M0L6_2atmpS2430;
            int32_t _M0L6_2atmpS2431;
            if (
              _M0L6_2atmpS2429 < 0
              || _M0L6_2atmpS2429 >= Moonbit_array_length(_M0L6resultS887)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS887[_M0L6_2atmpS2429] = 48;
            _M0L6_2atmpS2430 = _M0Lm5indexS888;
            _M0Lm5indexS888 = _M0L6_2atmpS2430 + 1;
            _M0L6_2atmpS2431 = _M0L2__S912 + 1;
            _M0L2__S912 = _M0L6_2atmpS2431;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2454 = _M0Lm5indexS888;
        int32_t _M0Lm7currentS914 = _M0L6_2atmpS2454 + 1;
        int32_t _M0L1iS915 = 0;
        int32_t _M0L6_2atmpS2452;
        int32_t _M0L6_2atmpS2453;
        while (1) {
          if (_M0L1iS915 < _M0L7olengthS892) {
            int32_t _M0L6_2atmpS2435 = _M0L7olengthS892 - _M0L1iS915;
            int32_t _M0L6_2atmpS2433 = _M0L6_2atmpS2435 - 1;
            int32_t _M0L6_2atmpS2434 = _M0Lm3expS893;
            int32_t _M0L6_2atmpS2449;
            int32_t _M0L6_2atmpS2448;
            int32_t _M0L6_2atmpS2447;
            int32_t _M0L6_2atmpS2441;
            uint64_t _M0L6_2atmpS2446;
            uint64_t _M0L6_2atmpS2445;
            int32_t _M0L6_2atmpS2444;
            int32_t _M0L6_2atmpS2443;
            int32_t _M0L6_2atmpS2442;
            uint64_t _M0L6_2atmpS2450;
            int32_t _M0L6_2atmpS2451;
            if (_M0L6_2atmpS2433 == _M0L6_2atmpS2434) {
              int32_t _M0L6_2atmpS2439 = _M0Lm7currentS914;
              int32_t _M0L6_2atmpS2438 = _M0L6_2atmpS2439 + _M0L7olengthS892;
              int32_t _M0L6_2atmpS2437 = _M0L6_2atmpS2438 - _M0L1iS915;
              int32_t _M0L6_2atmpS2436 = _M0L6_2atmpS2437 - 1;
              int32_t _M0L6_2atmpS2440;
              if (
                _M0L6_2atmpS2436 < 0
                || _M0L6_2atmpS2436 >= Moonbit_array_length(_M0L6resultS887)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS887[_M0L6_2atmpS2436] = 46;
              _M0L6_2atmpS2440 = _M0Lm7currentS914;
              _M0Lm7currentS914 = _M0L6_2atmpS2440 - 1;
            }
            _M0L6_2atmpS2449 = _M0Lm7currentS914;
            _M0L6_2atmpS2448 = _M0L6_2atmpS2449 + _M0L7olengthS892;
            _M0L6_2atmpS2447 = _M0L6_2atmpS2448 - _M0L1iS915;
            _M0L6_2atmpS2441 = _M0L6_2atmpS2447 - 1;
            _M0L6_2atmpS2446 = _M0Lm6outputS890;
            _M0L6_2atmpS2445 = _M0L6_2atmpS2446 % 10ull;
            _M0L6_2atmpS2444 = (int32_t)_M0L6_2atmpS2445;
            _M0L6_2atmpS2443 = 48 + _M0L6_2atmpS2444;
            _M0L6_2atmpS2442 = _M0L6_2atmpS2443 & 0xff;
            if (
              _M0L6_2atmpS2441 < 0
              || _M0L6_2atmpS2441 >= Moonbit_array_length(_M0L6resultS887)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS887[_M0L6_2atmpS2441] = _M0L6_2atmpS2442;
            _M0L6_2atmpS2450 = _M0Lm6outputS890;
            _M0Lm6outputS890 = _M0L6_2atmpS2450 / 10ull;
            _M0L6_2atmpS2451 = _M0L1iS915 + 1;
            _M0L1iS915 = _M0L6_2atmpS2451;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2452 = _M0Lm5indexS888;
        _M0L6_2atmpS2453 = _M0L7olengthS892 + 1;
        _M0Lm5indexS888 = _M0L6_2atmpS2452 + _M0L6_2atmpS2453;
      }
    }
    _M0L6_2atmpS2455 = _M0Lm5indexS888;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS887, 0, _M0L6_2atmpS2455);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS833,
  uint32_t _M0L12ieeeExponentS832
) {
  int32_t _M0Lm2e2S830;
  uint64_t _M0Lm2m2S831;
  uint64_t _M0L6_2atmpS2330;
  uint64_t _M0L6_2atmpS2329;
  int32_t _M0L4evenS834;
  uint64_t _M0L6_2atmpS2328;
  uint64_t _M0L2mvS835;
  int32_t _M0L7mmShiftS836;
  uint64_t _M0Lm2vrS837;
  uint64_t _M0Lm2vpS838;
  uint64_t _M0Lm2vmS839;
  int32_t _M0Lm3e10S840;
  int32_t _M0Lm17vmIsTrailingZerosS841;
  int32_t _M0Lm17vrIsTrailingZerosS842;
  int32_t _M0L6_2atmpS2230;
  int32_t _M0Lm7removedS861;
  int32_t _M0Lm16lastRemovedDigitS862;
  uint64_t _M0Lm6outputS863;
  int32_t _M0L6_2atmpS2326;
  int32_t _M0L6_2atmpS2327;
  int32_t _M0L3expS886;
  uint64_t _M0L6_2atmpS2325;
  struct _M0TPB17FloatingDecimal64* _block_3223;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S830 = 0;
  _M0Lm2m2S831 = 0ull;
  if (_M0L12ieeeExponentS832 == 0u) {
    _M0Lm2e2S830 = -1076;
    _M0Lm2m2S831 = _M0L12ieeeMantissaS833;
  } else {
    int32_t _M0L6_2atmpS2229 = *(int32_t*)&_M0L12ieeeExponentS832;
    int32_t _M0L6_2atmpS2228 = _M0L6_2atmpS2229 - 1023;
    int32_t _M0L6_2atmpS2227 = _M0L6_2atmpS2228 - 52;
    _M0Lm2e2S830 = _M0L6_2atmpS2227 - 2;
    _M0Lm2m2S831 = 4503599627370496ull | _M0L12ieeeMantissaS833;
  }
  _M0L6_2atmpS2330 = _M0Lm2m2S831;
  _M0L6_2atmpS2329 = _M0L6_2atmpS2330 & 1ull;
  _M0L4evenS834 = _M0L6_2atmpS2329 == 0ull;
  _M0L6_2atmpS2328 = _M0Lm2m2S831;
  _M0L2mvS835 = 4ull * _M0L6_2atmpS2328;
  if (_M0L12ieeeMantissaS833 != 0ull) {
    _M0L7mmShiftS836 = 1;
  } else {
    _M0L7mmShiftS836 = _M0L12ieeeExponentS832 <= 1u;
  }
  _M0Lm2vrS837 = 0ull;
  _M0Lm2vpS838 = 0ull;
  _M0Lm2vmS839 = 0ull;
  _M0Lm3e10S840 = 0;
  _M0Lm17vmIsTrailingZerosS841 = 0;
  _M0Lm17vrIsTrailingZerosS842 = 0;
  _M0L6_2atmpS2230 = _M0Lm2e2S830;
  if (_M0L6_2atmpS2230 >= 0) {
    int32_t _M0L6_2atmpS2252 = _M0Lm2e2S830;
    int32_t _M0L6_2atmpS2248;
    int32_t _M0L6_2atmpS2251;
    int32_t _M0L6_2atmpS2250;
    int32_t _M0L6_2atmpS2249;
    int32_t _M0L1qS843;
    int32_t _M0L6_2atmpS2247;
    int32_t _M0L6_2atmpS2246;
    int32_t _M0L1kS844;
    int32_t _M0L6_2atmpS2245;
    int32_t _M0L6_2atmpS2244;
    int32_t _M0L6_2atmpS2243;
    int32_t _M0L1iS845;
    struct _M0TPB8Pow5Pair _M0L4pow5S846;
    uint64_t _M0L6_2atmpS2242;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS847;
    uint64_t _M0L8_2avrOutS848;
    uint64_t _M0L8_2avpOutS849;
    uint64_t _M0L8_2avmOutS850;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2248 = _M0FPB9log10Pow2(_M0L6_2atmpS2252);
    _M0L6_2atmpS2251 = _M0Lm2e2S830;
    _M0L6_2atmpS2250 = _M0L6_2atmpS2251 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2249 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2250);
    _M0L1qS843 = _M0L6_2atmpS2248 - _M0L6_2atmpS2249;
    _M0Lm3e10S840 = _M0L1qS843;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2247 = _M0FPB8pow5bits(_M0L1qS843);
    _M0L6_2atmpS2246 = 125 + _M0L6_2atmpS2247;
    _M0L1kS844 = _M0L6_2atmpS2246 - 1;
    _M0L6_2atmpS2245 = _M0Lm2e2S830;
    _M0L6_2atmpS2244 = -_M0L6_2atmpS2245;
    _M0L6_2atmpS2243 = _M0L6_2atmpS2244 + _M0L1qS843;
    _M0L1iS845 = _M0L6_2atmpS2243 + _M0L1kS844;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S846 = _M0FPB22double__computeInvPow5(_M0L1qS843);
    _M0L6_2atmpS2242 = _M0Lm2m2S831;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS847
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2242, _M0L4pow5S846, _M0L1iS845, _M0L7mmShiftS836);
    _M0L8_2avrOutS848 = _M0L7_2abindS847.$0;
    _M0L8_2avpOutS849 = _M0L7_2abindS847.$1;
    _M0L8_2avmOutS850 = _M0L7_2abindS847.$2;
    _M0Lm2vrS837 = _M0L8_2avrOutS848;
    _M0Lm2vpS838 = _M0L8_2avpOutS849;
    _M0Lm2vmS839 = _M0L8_2avmOutS850;
    if (_M0L1qS843 <= 21) {
      int32_t _M0L6_2atmpS2238 = (int32_t)_M0L2mvS835;
      uint64_t _M0L6_2atmpS2241 = _M0L2mvS835 / 5ull;
      int32_t _M0L6_2atmpS2240 = (int32_t)_M0L6_2atmpS2241;
      int32_t _M0L6_2atmpS2239 = 5 * _M0L6_2atmpS2240;
      int32_t _M0L6mvMod5S851 = _M0L6_2atmpS2238 - _M0L6_2atmpS2239;
      if (_M0L6mvMod5S851 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS842
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS835, _M0L1qS843);
      } else if (_M0L4evenS834) {
        uint64_t _M0L6_2atmpS2232 = _M0L2mvS835 - 1ull;
        uint64_t _M0L6_2atmpS2233;
        uint64_t _M0L6_2atmpS2231;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2233 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS836);
        _M0L6_2atmpS2231 = _M0L6_2atmpS2232 - _M0L6_2atmpS2233;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS841
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2231, _M0L1qS843);
      } else {
        uint64_t _M0L6_2atmpS2234 = _M0Lm2vpS838;
        uint64_t _M0L6_2atmpS2237 = _M0L2mvS835 + 2ull;
        int32_t _M0L6_2atmpS2236;
        uint64_t _M0L6_2atmpS2235;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2236
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2237, _M0L1qS843);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2235 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2236);
        _M0Lm2vpS838 = _M0L6_2atmpS2234 - _M0L6_2atmpS2235;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2266 = _M0Lm2e2S830;
    int32_t _M0L6_2atmpS2265 = -_M0L6_2atmpS2266;
    int32_t _M0L6_2atmpS2260;
    int32_t _M0L6_2atmpS2264;
    int32_t _M0L6_2atmpS2263;
    int32_t _M0L6_2atmpS2262;
    int32_t _M0L6_2atmpS2261;
    int32_t _M0L1qS852;
    int32_t _M0L6_2atmpS2253;
    int32_t _M0L6_2atmpS2259;
    int32_t _M0L6_2atmpS2258;
    int32_t _M0L1iS853;
    int32_t _M0L6_2atmpS2257;
    int32_t _M0L1kS854;
    int32_t _M0L1jS855;
    struct _M0TPB8Pow5Pair _M0L4pow5S856;
    uint64_t _M0L6_2atmpS2256;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS857;
    uint64_t _M0L8_2avrOutS858;
    uint64_t _M0L8_2avpOutS859;
    uint64_t _M0L8_2avmOutS860;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2260 = _M0FPB9log10Pow5(_M0L6_2atmpS2265);
    _M0L6_2atmpS2264 = _M0Lm2e2S830;
    _M0L6_2atmpS2263 = -_M0L6_2atmpS2264;
    _M0L6_2atmpS2262 = _M0L6_2atmpS2263 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2261 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2262);
    _M0L1qS852 = _M0L6_2atmpS2260 - _M0L6_2atmpS2261;
    _M0L6_2atmpS2253 = _M0Lm2e2S830;
    _M0Lm3e10S840 = _M0L1qS852 + _M0L6_2atmpS2253;
    _M0L6_2atmpS2259 = _M0Lm2e2S830;
    _M0L6_2atmpS2258 = -_M0L6_2atmpS2259;
    _M0L1iS853 = _M0L6_2atmpS2258 - _M0L1qS852;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2257 = _M0FPB8pow5bits(_M0L1iS853);
    _M0L1kS854 = _M0L6_2atmpS2257 - 125;
    _M0L1jS855 = _M0L1qS852 - _M0L1kS854;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S856 = _M0FPB19double__computePow5(_M0L1iS853);
    _M0L6_2atmpS2256 = _M0Lm2m2S831;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS857
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2256, _M0L4pow5S856, _M0L1jS855, _M0L7mmShiftS836);
    _M0L8_2avrOutS858 = _M0L7_2abindS857.$0;
    _M0L8_2avpOutS859 = _M0L7_2abindS857.$1;
    _M0L8_2avmOutS860 = _M0L7_2abindS857.$2;
    _M0Lm2vrS837 = _M0L8_2avrOutS858;
    _M0Lm2vpS838 = _M0L8_2avpOutS859;
    _M0Lm2vmS839 = _M0L8_2avmOutS860;
    if (_M0L1qS852 <= 1) {
      _M0Lm17vrIsTrailingZerosS842 = 1;
      if (_M0L4evenS834) {
        int32_t _M0L6_2atmpS2254;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2254 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS836);
        _M0Lm17vmIsTrailingZerosS841 = _M0L6_2atmpS2254 == 1;
      } else {
        uint64_t _M0L6_2atmpS2255 = _M0Lm2vpS838;
        _M0Lm2vpS838 = _M0L6_2atmpS2255 - 1ull;
      }
    } else if (_M0L1qS852 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS842
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS835, _M0L1qS852);
    }
  }
  _M0Lm7removedS861 = 0;
  _M0Lm16lastRemovedDigitS862 = 0;
  _M0Lm6outputS863 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS841 || _M0Lm17vrIsTrailingZerosS842) {
    int32_t _if__result_3220;
    uint64_t _M0L6_2atmpS2296;
    uint64_t _M0L6_2atmpS2302;
    uint64_t _M0L6_2atmpS2303;
    int32_t _if__result_3221;
    int32_t _M0L6_2atmpS2299;
    int64_t _M0L6_2atmpS2298;
    uint64_t _M0L6_2atmpS2297;
    while (1) {
      uint64_t _M0L6_2atmpS2279 = _M0Lm2vpS838;
      uint64_t _M0L7vpDiv10S864 = _M0L6_2atmpS2279 / 10ull;
      uint64_t _M0L6_2atmpS2278 = _M0Lm2vmS839;
      uint64_t _M0L7vmDiv10S865 = _M0L6_2atmpS2278 / 10ull;
      uint64_t _M0L6_2atmpS2277;
      int32_t _M0L6_2atmpS2274;
      int32_t _M0L6_2atmpS2276;
      int32_t _M0L6_2atmpS2275;
      int32_t _M0L7vmMod10S867;
      uint64_t _M0L6_2atmpS2273;
      uint64_t _M0L7vrDiv10S868;
      uint64_t _M0L6_2atmpS2272;
      int32_t _M0L6_2atmpS2269;
      int32_t _M0L6_2atmpS2271;
      int32_t _M0L6_2atmpS2270;
      int32_t _M0L7vrMod10S869;
      int32_t _M0L6_2atmpS2268;
      if (_M0L7vpDiv10S864 <= _M0L7vmDiv10S865) {
        break;
      }
      _M0L6_2atmpS2277 = _M0Lm2vmS839;
      _M0L6_2atmpS2274 = (int32_t)_M0L6_2atmpS2277;
      _M0L6_2atmpS2276 = (int32_t)_M0L7vmDiv10S865;
      _M0L6_2atmpS2275 = 10 * _M0L6_2atmpS2276;
      _M0L7vmMod10S867 = _M0L6_2atmpS2274 - _M0L6_2atmpS2275;
      _M0L6_2atmpS2273 = _M0Lm2vrS837;
      _M0L7vrDiv10S868 = _M0L6_2atmpS2273 / 10ull;
      _M0L6_2atmpS2272 = _M0Lm2vrS837;
      _M0L6_2atmpS2269 = (int32_t)_M0L6_2atmpS2272;
      _M0L6_2atmpS2271 = (int32_t)_M0L7vrDiv10S868;
      _M0L6_2atmpS2270 = 10 * _M0L6_2atmpS2271;
      _M0L7vrMod10S869 = _M0L6_2atmpS2269 - _M0L6_2atmpS2270;
      if (_M0Lm17vmIsTrailingZerosS841) {
        _M0Lm17vmIsTrailingZerosS841 = _M0L7vmMod10S867 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS841 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS842) {
        int32_t _M0L6_2atmpS2267 = _M0Lm16lastRemovedDigitS862;
        _M0Lm17vrIsTrailingZerosS842 = _M0L6_2atmpS2267 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS842 = 0;
      }
      _M0Lm16lastRemovedDigitS862 = _M0L7vrMod10S869;
      _M0Lm2vrS837 = _M0L7vrDiv10S868;
      _M0Lm2vpS838 = _M0L7vpDiv10S864;
      _M0Lm2vmS839 = _M0L7vmDiv10S865;
      _M0L6_2atmpS2268 = _M0Lm7removedS861;
      _M0Lm7removedS861 = _M0L6_2atmpS2268 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS841) {
      while (1) {
        uint64_t _M0L6_2atmpS2292 = _M0Lm2vmS839;
        uint64_t _M0L7vmDiv10S870 = _M0L6_2atmpS2292 / 10ull;
        uint64_t _M0L6_2atmpS2291 = _M0Lm2vmS839;
        int32_t _M0L6_2atmpS2288 = (int32_t)_M0L6_2atmpS2291;
        int32_t _M0L6_2atmpS2290 = (int32_t)_M0L7vmDiv10S870;
        int32_t _M0L6_2atmpS2289 = 10 * _M0L6_2atmpS2290;
        int32_t _M0L7vmMod10S871 = _M0L6_2atmpS2288 - _M0L6_2atmpS2289;
        uint64_t _M0L6_2atmpS2287;
        uint64_t _M0L7vpDiv10S873;
        uint64_t _M0L6_2atmpS2286;
        uint64_t _M0L7vrDiv10S874;
        uint64_t _M0L6_2atmpS2285;
        int32_t _M0L6_2atmpS2282;
        int32_t _M0L6_2atmpS2284;
        int32_t _M0L6_2atmpS2283;
        int32_t _M0L7vrMod10S875;
        int32_t _M0L6_2atmpS2281;
        if (_M0L7vmMod10S871 != 0) {
          break;
        }
        _M0L6_2atmpS2287 = _M0Lm2vpS838;
        _M0L7vpDiv10S873 = _M0L6_2atmpS2287 / 10ull;
        _M0L6_2atmpS2286 = _M0Lm2vrS837;
        _M0L7vrDiv10S874 = _M0L6_2atmpS2286 / 10ull;
        _M0L6_2atmpS2285 = _M0Lm2vrS837;
        _M0L6_2atmpS2282 = (int32_t)_M0L6_2atmpS2285;
        _M0L6_2atmpS2284 = (int32_t)_M0L7vrDiv10S874;
        _M0L6_2atmpS2283 = 10 * _M0L6_2atmpS2284;
        _M0L7vrMod10S875 = _M0L6_2atmpS2282 - _M0L6_2atmpS2283;
        if (_M0Lm17vrIsTrailingZerosS842) {
          int32_t _M0L6_2atmpS2280 = _M0Lm16lastRemovedDigitS862;
          _M0Lm17vrIsTrailingZerosS842 = _M0L6_2atmpS2280 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS842 = 0;
        }
        _M0Lm16lastRemovedDigitS862 = _M0L7vrMod10S875;
        _M0Lm2vrS837 = _M0L7vrDiv10S874;
        _M0Lm2vpS838 = _M0L7vpDiv10S873;
        _M0Lm2vmS839 = _M0L7vmDiv10S870;
        _M0L6_2atmpS2281 = _M0Lm7removedS861;
        _M0Lm7removedS861 = _M0L6_2atmpS2281 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS842) {
      int32_t _M0L6_2atmpS2295 = _M0Lm16lastRemovedDigitS862;
      if (_M0L6_2atmpS2295 == 5) {
        uint64_t _M0L6_2atmpS2294 = _M0Lm2vrS837;
        uint64_t _M0L6_2atmpS2293 = _M0L6_2atmpS2294 % 2ull;
        _if__result_3220 = _M0L6_2atmpS2293 == 0ull;
      } else {
        _if__result_3220 = 0;
      }
    } else {
      _if__result_3220 = 0;
    }
    if (_if__result_3220) {
      _M0Lm16lastRemovedDigitS862 = 4;
    }
    _M0L6_2atmpS2296 = _M0Lm2vrS837;
    _M0L6_2atmpS2302 = _M0Lm2vrS837;
    _M0L6_2atmpS2303 = _M0Lm2vmS839;
    if (_M0L6_2atmpS2302 == _M0L6_2atmpS2303) {
      if (!_M0L4evenS834) {
        _if__result_3221 = 1;
      } else {
        int32_t _M0L6_2atmpS2301 = _M0Lm17vmIsTrailingZerosS841;
        _if__result_3221 = !_M0L6_2atmpS2301;
      }
    } else {
      _if__result_3221 = 0;
    }
    if (_if__result_3221) {
      _M0L6_2atmpS2299 = 1;
    } else {
      int32_t _M0L6_2atmpS2300 = _M0Lm16lastRemovedDigitS862;
      _M0L6_2atmpS2299 = _M0L6_2atmpS2300 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2298 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2299);
    _M0L6_2atmpS2297 = *(uint64_t*)&_M0L6_2atmpS2298;
    _M0Lm6outputS863 = _M0L6_2atmpS2296 + _M0L6_2atmpS2297;
  } else {
    int32_t _M0Lm7roundUpS876 = 0;
    uint64_t _M0L6_2atmpS2324 = _M0Lm2vpS838;
    uint64_t _M0L8vpDiv100S877 = _M0L6_2atmpS2324 / 100ull;
    uint64_t _M0L6_2atmpS2323 = _M0Lm2vmS839;
    uint64_t _M0L8vmDiv100S878 = _M0L6_2atmpS2323 / 100ull;
    uint64_t _M0L6_2atmpS2318;
    uint64_t _M0L6_2atmpS2321;
    uint64_t _M0L6_2atmpS2322;
    int32_t _M0L6_2atmpS2320;
    uint64_t _M0L6_2atmpS2319;
    if (_M0L8vpDiv100S877 > _M0L8vmDiv100S878) {
      uint64_t _M0L6_2atmpS2309 = _M0Lm2vrS837;
      uint64_t _M0L8vrDiv100S879 = _M0L6_2atmpS2309 / 100ull;
      uint64_t _M0L6_2atmpS2308 = _M0Lm2vrS837;
      int32_t _M0L6_2atmpS2305 = (int32_t)_M0L6_2atmpS2308;
      int32_t _M0L6_2atmpS2307 = (int32_t)_M0L8vrDiv100S879;
      int32_t _M0L6_2atmpS2306 = 100 * _M0L6_2atmpS2307;
      int32_t _M0L8vrMod100S880 = _M0L6_2atmpS2305 - _M0L6_2atmpS2306;
      int32_t _M0L6_2atmpS2304;
      _M0Lm7roundUpS876 = _M0L8vrMod100S880 >= 50;
      _M0Lm2vrS837 = _M0L8vrDiv100S879;
      _M0Lm2vpS838 = _M0L8vpDiv100S877;
      _M0Lm2vmS839 = _M0L8vmDiv100S878;
      _M0L6_2atmpS2304 = _M0Lm7removedS861;
      _M0Lm7removedS861 = _M0L6_2atmpS2304 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2317 = _M0Lm2vpS838;
      uint64_t _M0L7vpDiv10S881 = _M0L6_2atmpS2317 / 10ull;
      uint64_t _M0L6_2atmpS2316 = _M0Lm2vmS839;
      uint64_t _M0L7vmDiv10S882 = _M0L6_2atmpS2316 / 10ull;
      uint64_t _M0L6_2atmpS2315;
      uint64_t _M0L7vrDiv10S884;
      uint64_t _M0L6_2atmpS2314;
      int32_t _M0L6_2atmpS2311;
      int32_t _M0L6_2atmpS2313;
      int32_t _M0L6_2atmpS2312;
      int32_t _M0L7vrMod10S885;
      int32_t _M0L6_2atmpS2310;
      if (_M0L7vpDiv10S881 <= _M0L7vmDiv10S882) {
        break;
      }
      _M0L6_2atmpS2315 = _M0Lm2vrS837;
      _M0L7vrDiv10S884 = _M0L6_2atmpS2315 / 10ull;
      _M0L6_2atmpS2314 = _M0Lm2vrS837;
      _M0L6_2atmpS2311 = (int32_t)_M0L6_2atmpS2314;
      _M0L6_2atmpS2313 = (int32_t)_M0L7vrDiv10S884;
      _M0L6_2atmpS2312 = 10 * _M0L6_2atmpS2313;
      _M0L7vrMod10S885 = _M0L6_2atmpS2311 - _M0L6_2atmpS2312;
      _M0Lm7roundUpS876 = _M0L7vrMod10S885 >= 5;
      _M0Lm2vrS837 = _M0L7vrDiv10S884;
      _M0Lm2vpS838 = _M0L7vpDiv10S881;
      _M0Lm2vmS839 = _M0L7vmDiv10S882;
      _M0L6_2atmpS2310 = _M0Lm7removedS861;
      _M0Lm7removedS861 = _M0L6_2atmpS2310 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2318 = _M0Lm2vrS837;
    _M0L6_2atmpS2321 = _M0Lm2vrS837;
    _M0L6_2atmpS2322 = _M0Lm2vmS839;
    _M0L6_2atmpS2320
    = _M0L6_2atmpS2321 == _M0L6_2atmpS2322 || _M0Lm7roundUpS876;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2319 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2320);
    _M0Lm6outputS863 = _M0L6_2atmpS2318 + _M0L6_2atmpS2319;
  }
  _M0L6_2atmpS2326 = _M0Lm3e10S840;
  _M0L6_2atmpS2327 = _M0Lm7removedS861;
  _M0L3expS886 = _M0L6_2atmpS2326 + _M0L6_2atmpS2327;
  _M0L6_2atmpS2325 = _M0Lm6outputS863;
  _block_3223
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3223)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3223->$0 = _M0L6_2atmpS2325;
  _block_3223->$1 = _M0L3expS886;
  return _block_3223;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS829) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS829) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS828) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS828) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS827) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS827) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS826) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS826 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS826 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS826 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS826 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS826 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS826 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS826 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS826 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS826 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS826 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS826 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS826 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS826 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS826 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS826 >= 100ull) {
    return 3;
  }
  if (_M0L1vS826 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS809) {
  int32_t _M0L6_2atmpS2226;
  int32_t _M0L6_2atmpS2225;
  int32_t _M0L4baseS808;
  int32_t _M0L5base2S810;
  int32_t _M0L6offsetS811;
  int32_t _M0L6_2atmpS2224;
  uint64_t _M0L4mul0S812;
  int32_t _M0L6_2atmpS2223;
  int32_t _M0L6_2atmpS2222;
  uint64_t _M0L4mul1S813;
  uint64_t _M0L1mS814;
  struct _M0TPB7Umul128 _M0L7_2abindS815;
  uint64_t _M0L7_2alow1S816;
  uint64_t _M0L8_2ahigh1S817;
  struct _M0TPB7Umul128 _M0L7_2abindS818;
  uint64_t _M0L7_2alow0S819;
  uint64_t _M0L8_2ahigh0S820;
  uint64_t _M0L3sumS821;
  uint64_t _M0Lm5high1S822;
  int32_t _M0L6_2atmpS2220;
  int32_t _M0L6_2atmpS2221;
  int32_t _M0L5deltaS823;
  uint64_t _M0L6_2atmpS2219;
  uint64_t _M0L6_2atmpS2211;
  int32_t _M0L6_2atmpS2218;
  uint32_t _M0L6_2atmpS2215;
  int32_t _M0L6_2atmpS2217;
  int32_t _M0L6_2atmpS2216;
  uint32_t _M0L6_2atmpS2214;
  uint32_t _M0L6_2atmpS2213;
  uint64_t _M0L6_2atmpS2212;
  uint64_t _M0L1aS824;
  uint64_t _M0L6_2atmpS2210;
  uint64_t _M0L1bS825;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2226 = _M0L1iS809 + 26;
  _M0L6_2atmpS2225 = _M0L6_2atmpS2226 - 1;
  _M0L4baseS808 = _M0L6_2atmpS2225 / 26;
  _M0L5base2S810 = _M0L4baseS808 * 26;
  _M0L6offsetS811 = _M0L5base2S810 - _M0L1iS809;
  _M0L6_2atmpS2224 = _M0L4baseS808 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S812
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2224);
  _M0L6_2atmpS2223 = _M0L4baseS808 * 2;
  _M0L6_2atmpS2222 = _M0L6_2atmpS2223 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S813
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2222);
  if (_M0L6offsetS811 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S812, _M0L4mul1S813};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS814
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS811);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS815 = _M0FPB7umul128(_M0L1mS814, _M0L4mul1S813);
  _M0L7_2alow1S816 = _M0L7_2abindS815.$0;
  _M0L8_2ahigh1S817 = _M0L7_2abindS815.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS818 = _M0FPB7umul128(_M0L1mS814, _M0L4mul0S812);
  _M0L7_2alow0S819 = _M0L7_2abindS818.$0;
  _M0L8_2ahigh0S820 = _M0L7_2abindS818.$1;
  _M0L3sumS821 = _M0L8_2ahigh0S820 + _M0L7_2alow1S816;
  _M0Lm5high1S822 = _M0L8_2ahigh1S817;
  if (_M0L3sumS821 < _M0L8_2ahigh0S820) {
    uint64_t _M0L6_2atmpS2209 = _M0Lm5high1S822;
    _M0Lm5high1S822 = _M0L6_2atmpS2209 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2220 = _M0FPB8pow5bits(_M0L5base2S810);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2221 = _M0FPB8pow5bits(_M0L1iS809);
  _M0L5deltaS823 = _M0L6_2atmpS2220 - _M0L6_2atmpS2221;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2219
  = _M0FPB13shiftright128(_M0L7_2alow0S819, _M0L3sumS821, _M0L5deltaS823);
  _M0L6_2atmpS2211 = _M0L6_2atmpS2219 + 1ull;
  _M0L6_2atmpS2218 = _M0L1iS809 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2215
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2218);
  _M0L6_2atmpS2217 = _M0L1iS809 % 16;
  _M0L6_2atmpS2216 = _M0L6_2atmpS2217 << 1;
  _M0L6_2atmpS2214 = _M0L6_2atmpS2215 >> (_M0L6_2atmpS2216 & 31);
  _M0L6_2atmpS2213 = _M0L6_2atmpS2214 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2212 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2213);
  _M0L1aS824 = _M0L6_2atmpS2211 + _M0L6_2atmpS2212;
  _M0L6_2atmpS2210 = _M0Lm5high1S822;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS825
  = _M0FPB13shiftright128(_M0L3sumS821, _M0L6_2atmpS2210, _M0L5deltaS823);
  return (struct _M0TPB8Pow5Pair){_M0L1aS824, _M0L1bS825};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS791) {
  int32_t _M0L4baseS790;
  int32_t _M0L5base2S792;
  int32_t _M0L6offsetS793;
  int32_t _M0L6_2atmpS2208;
  uint64_t _M0L4mul0S794;
  int32_t _M0L6_2atmpS2207;
  int32_t _M0L6_2atmpS2206;
  uint64_t _M0L4mul1S795;
  uint64_t _M0L1mS796;
  struct _M0TPB7Umul128 _M0L7_2abindS797;
  uint64_t _M0L7_2alow1S798;
  uint64_t _M0L8_2ahigh1S799;
  struct _M0TPB7Umul128 _M0L7_2abindS800;
  uint64_t _M0L7_2alow0S801;
  uint64_t _M0L8_2ahigh0S802;
  uint64_t _M0L3sumS803;
  uint64_t _M0Lm5high1S804;
  int32_t _M0L6_2atmpS2204;
  int32_t _M0L6_2atmpS2205;
  int32_t _M0L5deltaS805;
  uint64_t _M0L6_2atmpS2196;
  int32_t _M0L6_2atmpS2203;
  uint32_t _M0L6_2atmpS2200;
  int32_t _M0L6_2atmpS2202;
  int32_t _M0L6_2atmpS2201;
  uint32_t _M0L6_2atmpS2199;
  uint32_t _M0L6_2atmpS2198;
  uint64_t _M0L6_2atmpS2197;
  uint64_t _M0L1aS806;
  uint64_t _M0L6_2atmpS2195;
  uint64_t _M0L1bS807;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS790 = _M0L1iS791 / 26;
  _M0L5base2S792 = _M0L4baseS790 * 26;
  _M0L6offsetS793 = _M0L1iS791 - _M0L5base2S792;
  _M0L6_2atmpS2208 = _M0L4baseS790 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S794
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2208);
  _M0L6_2atmpS2207 = _M0L4baseS790 * 2;
  _M0L6_2atmpS2206 = _M0L6_2atmpS2207 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S795
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2206);
  if (_M0L6offsetS793 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S794, _M0L4mul1S795};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS796
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS793);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS797 = _M0FPB7umul128(_M0L1mS796, _M0L4mul1S795);
  _M0L7_2alow1S798 = _M0L7_2abindS797.$0;
  _M0L8_2ahigh1S799 = _M0L7_2abindS797.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS800 = _M0FPB7umul128(_M0L1mS796, _M0L4mul0S794);
  _M0L7_2alow0S801 = _M0L7_2abindS800.$0;
  _M0L8_2ahigh0S802 = _M0L7_2abindS800.$1;
  _M0L3sumS803 = _M0L8_2ahigh0S802 + _M0L7_2alow1S798;
  _M0Lm5high1S804 = _M0L8_2ahigh1S799;
  if (_M0L3sumS803 < _M0L8_2ahigh0S802) {
    uint64_t _M0L6_2atmpS2194 = _M0Lm5high1S804;
    _M0Lm5high1S804 = _M0L6_2atmpS2194 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2204 = _M0FPB8pow5bits(_M0L1iS791);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2205 = _M0FPB8pow5bits(_M0L5base2S792);
  _M0L5deltaS805 = _M0L6_2atmpS2204 - _M0L6_2atmpS2205;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2196
  = _M0FPB13shiftright128(_M0L7_2alow0S801, _M0L3sumS803, _M0L5deltaS805);
  _M0L6_2atmpS2203 = _M0L1iS791 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2200
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2203);
  _M0L6_2atmpS2202 = _M0L1iS791 % 16;
  _M0L6_2atmpS2201 = _M0L6_2atmpS2202 << 1;
  _M0L6_2atmpS2199 = _M0L6_2atmpS2200 >> (_M0L6_2atmpS2201 & 31);
  _M0L6_2atmpS2198 = _M0L6_2atmpS2199 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2197 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2198);
  _M0L1aS806 = _M0L6_2atmpS2196 + _M0L6_2atmpS2197;
  _M0L6_2atmpS2195 = _M0Lm5high1S804;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS807
  = _M0FPB13shiftright128(_M0L3sumS803, _M0L6_2atmpS2195, _M0L5deltaS805);
  return (struct _M0TPB8Pow5Pair){_M0L1aS806, _M0L1bS807};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS764,
  struct _M0TPB8Pow5Pair _M0L3mulS761,
  int32_t _M0L1jS777,
  int32_t _M0L7mmShiftS779
) {
  uint64_t _M0L7_2amul0S760;
  uint64_t _M0L7_2amul1S762;
  uint64_t _M0L1mS763;
  struct _M0TPB7Umul128 _M0L7_2abindS765;
  uint64_t _M0L5_2aloS766;
  uint64_t _M0L6_2atmpS767;
  struct _M0TPB7Umul128 _M0L7_2abindS768;
  uint64_t _M0L6_2alo2S769;
  uint64_t _M0L6_2ahi2S770;
  uint64_t _M0L3midS771;
  uint64_t _M0L6_2atmpS2193;
  uint64_t _M0L2hiS772;
  uint64_t _M0L3lo2S773;
  uint64_t _M0L6_2atmpS2191;
  uint64_t _M0L6_2atmpS2192;
  uint64_t _M0L4mid2S774;
  uint64_t _M0L6_2atmpS2190;
  uint64_t _M0L3hi2S775;
  int32_t _M0L6_2atmpS2189;
  int32_t _M0L6_2atmpS2188;
  uint64_t _M0L2vpS776;
  uint64_t _M0Lm2vmS778;
  int32_t _M0L6_2atmpS2187;
  int32_t _M0L6_2atmpS2186;
  uint64_t _M0L2vrS789;
  uint64_t _M0L6_2atmpS2185;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S760 = _M0L3mulS761.$0;
  _M0L7_2amul1S762 = _M0L3mulS761.$1;
  _M0L1mS763 = _M0L1mS764 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS765 = _M0FPB7umul128(_M0L1mS763, _M0L7_2amul0S760);
  _M0L5_2aloS766 = _M0L7_2abindS765.$0;
  _M0L6_2atmpS767 = _M0L7_2abindS765.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS768 = _M0FPB7umul128(_M0L1mS763, _M0L7_2amul1S762);
  _M0L6_2alo2S769 = _M0L7_2abindS768.$0;
  _M0L6_2ahi2S770 = _M0L7_2abindS768.$1;
  _M0L3midS771 = _M0L6_2atmpS767 + _M0L6_2alo2S769;
  if (_M0L3midS771 < _M0L6_2atmpS767) {
    _M0L6_2atmpS2193 = 1ull;
  } else {
    _M0L6_2atmpS2193 = 0ull;
  }
  _M0L2hiS772 = _M0L6_2ahi2S770 + _M0L6_2atmpS2193;
  _M0L3lo2S773 = _M0L5_2aloS766 + _M0L7_2amul0S760;
  _M0L6_2atmpS2191 = _M0L3midS771 + _M0L7_2amul1S762;
  if (_M0L3lo2S773 < _M0L5_2aloS766) {
    _M0L6_2atmpS2192 = 1ull;
  } else {
    _M0L6_2atmpS2192 = 0ull;
  }
  _M0L4mid2S774 = _M0L6_2atmpS2191 + _M0L6_2atmpS2192;
  if (_M0L4mid2S774 < _M0L3midS771) {
    _M0L6_2atmpS2190 = 1ull;
  } else {
    _M0L6_2atmpS2190 = 0ull;
  }
  _M0L3hi2S775 = _M0L2hiS772 + _M0L6_2atmpS2190;
  _M0L6_2atmpS2189 = _M0L1jS777 - 64;
  _M0L6_2atmpS2188 = _M0L6_2atmpS2189 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS776
  = _M0FPB13shiftright128(_M0L4mid2S774, _M0L3hi2S775, _M0L6_2atmpS2188);
  _M0Lm2vmS778 = 0ull;
  if (_M0L7mmShiftS779) {
    uint64_t _M0L3lo3S780 = _M0L5_2aloS766 - _M0L7_2amul0S760;
    uint64_t _M0L6_2atmpS2175 = _M0L3midS771 - _M0L7_2amul1S762;
    uint64_t _M0L6_2atmpS2176;
    uint64_t _M0L4mid3S781;
    uint64_t _M0L6_2atmpS2174;
    uint64_t _M0L3hi3S782;
    int32_t _M0L6_2atmpS2173;
    int32_t _M0L6_2atmpS2172;
    if (_M0L5_2aloS766 < _M0L3lo3S780) {
      _M0L6_2atmpS2176 = 1ull;
    } else {
      _M0L6_2atmpS2176 = 0ull;
    }
    _M0L4mid3S781 = _M0L6_2atmpS2175 - _M0L6_2atmpS2176;
    if (_M0L3midS771 < _M0L4mid3S781) {
      _M0L6_2atmpS2174 = 1ull;
    } else {
      _M0L6_2atmpS2174 = 0ull;
    }
    _M0L3hi3S782 = _M0L2hiS772 - _M0L6_2atmpS2174;
    _M0L6_2atmpS2173 = _M0L1jS777 - 64;
    _M0L6_2atmpS2172 = _M0L6_2atmpS2173 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS778
    = _M0FPB13shiftright128(_M0L4mid3S781, _M0L3hi3S782, _M0L6_2atmpS2172);
  } else {
    uint64_t _M0L3lo3S783 = _M0L5_2aloS766 + _M0L5_2aloS766;
    uint64_t _M0L6_2atmpS2183 = _M0L3midS771 + _M0L3midS771;
    uint64_t _M0L6_2atmpS2184;
    uint64_t _M0L4mid3S784;
    uint64_t _M0L6_2atmpS2181;
    uint64_t _M0L6_2atmpS2182;
    uint64_t _M0L3hi3S785;
    uint64_t _M0L3lo4S786;
    uint64_t _M0L6_2atmpS2179;
    uint64_t _M0L6_2atmpS2180;
    uint64_t _M0L4mid4S787;
    uint64_t _M0L6_2atmpS2178;
    uint64_t _M0L3hi4S788;
    int32_t _M0L6_2atmpS2177;
    if (_M0L3lo3S783 < _M0L5_2aloS766) {
      _M0L6_2atmpS2184 = 1ull;
    } else {
      _M0L6_2atmpS2184 = 0ull;
    }
    _M0L4mid3S784 = _M0L6_2atmpS2183 + _M0L6_2atmpS2184;
    _M0L6_2atmpS2181 = _M0L2hiS772 + _M0L2hiS772;
    if (_M0L4mid3S784 < _M0L3midS771) {
      _M0L6_2atmpS2182 = 1ull;
    } else {
      _M0L6_2atmpS2182 = 0ull;
    }
    _M0L3hi3S785 = _M0L6_2atmpS2181 + _M0L6_2atmpS2182;
    _M0L3lo4S786 = _M0L3lo3S783 - _M0L7_2amul0S760;
    _M0L6_2atmpS2179 = _M0L4mid3S784 - _M0L7_2amul1S762;
    if (_M0L3lo3S783 < _M0L3lo4S786) {
      _M0L6_2atmpS2180 = 1ull;
    } else {
      _M0L6_2atmpS2180 = 0ull;
    }
    _M0L4mid4S787 = _M0L6_2atmpS2179 - _M0L6_2atmpS2180;
    if (_M0L4mid3S784 < _M0L4mid4S787) {
      _M0L6_2atmpS2178 = 1ull;
    } else {
      _M0L6_2atmpS2178 = 0ull;
    }
    _M0L3hi4S788 = _M0L3hi3S785 - _M0L6_2atmpS2178;
    _M0L6_2atmpS2177 = _M0L1jS777 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS778
    = _M0FPB13shiftright128(_M0L4mid4S787, _M0L3hi4S788, _M0L6_2atmpS2177);
  }
  _M0L6_2atmpS2187 = _M0L1jS777 - 64;
  _M0L6_2atmpS2186 = _M0L6_2atmpS2187 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS789
  = _M0FPB13shiftright128(_M0L3midS771, _M0L2hiS772, _M0L6_2atmpS2186);
  _M0L6_2atmpS2185 = _M0Lm2vmS778;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS789,
                                                _M0L2vpS776,
                                                _M0L6_2atmpS2185};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS758,
  int32_t _M0L1pS759
) {
  uint64_t _M0L6_2atmpS2171;
  uint64_t _M0L6_2atmpS2170;
  uint64_t _M0L6_2atmpS2169;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2171 = 1ull << (_M0L1pS759 & 63);
  _M0L6_2atmpS2170 = _M0L6_2atmpS2171 - 1ull;
  _M0L6_2atmpS2169 = _M0L5valueS758 & _M0L6_2atmpS2170;
  return _M0L6_2atmpS2169 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS756,
  int32_t _M0L1pS757
) {
  int32_t _M0L6_2atmpS2168;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2168 = _M0FPB10pow5Factor(_M0L5valueS756);
  return _M0L6_2atmpS2168 >= _M0L1pS757;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS752) {
  uint64_t _M0L6_2atmpS2156;
  uint64_t _M0L6_2atmpS2157;
  uint64_t _M0L6_2atmpS2158;
  uint64_t _M0L6_2atmpS2159;
  int32_t _M0Lm5countS753;
  uint64_t _M0Lm5valueS754;
  uint64_t _M0L6_2atmpS2167;
  moonbit_string_t _M0L6_2atmpS2166;
  moonbit_string_t _M0L6_2atmpS2881;
  moonbit_string_t _M0L6_2atmpS2165;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2156 = _M0L5valueS752 % 5ull;
  if (_M0L6_2atmpS2156 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2157 = _M0L5valueS752 % 25ull;
  if (_M0L6_2atmpS2157 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2158 = _M0L5valueS752 % 125ull;
  if (_M0L6_2atmpS2158 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2159 = _M0L5valueS752 % 625ull;
  if (_M0L6_2atmpS2159 != 0ull) {
    return 3;
  }
  _M0Lm5countS753 = 4;
  _M0Lm5valueS754 = _M0L5valueS752 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2160 = _M0Lm5valueS754;
    if (_M0L6_2atmpS2160 > 0ull) {
      uint64_t _M0L6_2atmpS2162 = _M0Lm5valueS754;
      uint64_t _M0L6_2atmpS2161 = _M0L6_2atmpS2162 % 5ull;
      uint64_t _M0L6_2atmpS2163;
      int32_t _M0L6_2atmpS2164;
      if (_M0L6_2atmpS2161 != 0ull) {
        return _M0Lm5countS753;
      }
      _M0L6_2atmpS2163 = _M0Lm5valueS754;
      _M0Lm5valueS754 = _M0L6_2atmpS2163 / 5ull;
      _M0L6_2atmpS2164 = _M0Lm5countS753;
      _M0Lm5countS753 = _M0L6_2atmpS2164 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2167 = _M0Lm5valueS754;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2166
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2167);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2881
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_51.data, _M0L6_2atmpS2166);
  moonbit_decref(_M0L6_2atmpS2166);
  _M0L6_2atmpS2165 = _M0L6_2atmpS2881;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2165, (moonbit_string_t)moonbit_string_literal_52.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS751,
  uint64_t _M0L2hiS749,
  int32_t _M0L4distS750
) {
  int32_t _M0L6_2atmpS2155;
  uint64_t _M0L6_2atmpS2153;
  uint64_t _M0L6_2atmpS2154;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2155 = 64 - _M0L4distS750;
  _M0L6_2atmpS2153 = _M0L2hiS749 << (_M0L6_2atmpS2155 & 63);
  _M0L6_2atmpS2154 = _M0L2loS751 >> (_M0L4distS750 & 63);
  return _M0L6_2atmpS2153 | _M0L6_2atmpS2154;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS739,
  uint64_t _M0L1bS742
) {
  uint64_t _M0L3aLoS738;
  uint64_t _M0L3aHiS740;
  uint64_t _M0L3bLoS741;
  uint64_t _M0L3bHiS743;
  uint64_t _M0L1xS744;
  uint64_t _M0L6_2atmpS2151;
  uint64_t _M0L6_2atmpS2152;
  uint64_t _M0L1yS745;
  uint64_t _M0L6_2atmpS2149;
  uint64_t _M0L6_2atmpS2150;
  uint64_t _M0L1zS746;
  uint64_t _M0L6_2atmpS2147;
  uint64_t _M0L6_2atmpS2148;
  uint64_t _M0L6_2atmpS2145;
  uint64_t _M0L6_2atmpS2146;
  uint64_t _M0L1wS747;
  uint64_t _M0L2loS748;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS738 = _M0L1aS739 & 4294967295ull;
  _M0L3aHiS740 = _M0L1aS739 >> 32;
  _M0L3bLoS741 = _M0L1bS742 & 4294967295ull;
  _M0L3bHiS743 = _M0L1bS742 >> 32;
  _M0L1xS744 = _M0L3aLoS738 * _M0L3bLoS741;
  _M0L6_2atmpS2151 = _M0L3aHiS740 * _M0L3bLoS741;
  _M0L6_2atmpS2152 = _M0L1xS744 >> 32;
  _M0L1yS745 = _M0L6_2atmpS2151 + _M0L6_2atmpS2152;
  _M0L6_2atmpS2149 = _M0L3aLoS738 * _M0L3bHiS743;
  _M0L6_2atmpS2150 = _M0L1yS745 & 4294967295ull;
  _M0L1zS746 = _M0L6_2atmpS2149 + _M0L6_2atmpS2150;
  _M0L6_2atmpS2147 = _M0L3aHiS740 * _M0L3bHiS743;
  _M0L6_2atmpS2148 = _M0L1yS745 >> 32;
  _M0L6_2atmpS2145 = _M0L6_2atmpS2147 + _M0L6_2atmpS2148;
  _M0L6_2atmpS2146 = _M0L1zS746 >> 32;
  _M0L1wS747 = _M0L6_2atmpS2145 + _M0L6_2atmpS2146;
  _M0L2loS748 = _M0L1aS739 * _M0L1bS742;
  return (struct _M0TPB7Umul128){_M0L2loS748, _M0L1wS747};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS733,
  int32_t _M0L4fromS737,
  int32_t _M0L2toS735
) {
  int32_t _M0L6_2atmpS2144;
  struct _M0TPB13StringBuilder* _M0L3bufS732;
  int32_t _M0L1iS734;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2144 = Moonbit_array_length(_M0L5bytesS733);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS732 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2144);
  _M0L1iS734 = _M0L4fromS737;
  while (1) {
    if (_M0L1iS734 < _M0L2toS735) {
      int32_t _M0L6_2atmpS2142;
      int32_t _M0L6_2atmpS2141;
      int32_t _M0L6_2atmpS2143;
      if (
        _M0L1iS734 < 0 || _M0L1iS734 >= Moonbit_array_length(_M0L5bytesS733)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2142 = (int32_t)_M0L5bytesS733[_M0L1iS734];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2141 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2142);
      moonbit_incref(_M0L3bufS732);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS732, _M0L6_2atmpS2141);
      _M0L6_2atmpS2143 = _M0L1iS734 + 1;
      _M0L1iS734 = _M0L6_2atmpS2143;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS733);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS732);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS731) {
  int32_t _M0L6_2atmpS2140;
  uint32_t _M0L6_2atmpS2139;
  uint32_t _M0L6_2atmpS2138;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2140 = _M0L1eS731 * 78913;
  _M0L6_2atmpS2139 = *(uint32_t*)&_M0L6_2atmpS2140;
  _M0L6_2atmpS2138 = _M0L6_2atmpS2139 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2138;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS730) {
  int32_t _M0L6_2atmpS2137;
  uint32_t _M0L6_2atmpS2136;
  uint32_t _M0L6_2atmpS2135;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2137 = _M0L1eS730 * 732923;
  _M0L6_2atmpS2136 = *(uint32_t*)&_M0L6_2atmpS2137;
  _M0L6_2atmpS2135 = _M0L6_2atmpS2136 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2135;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS728,
  int32_t _M0L8exponentS729,
  int32_t _M0L8mantissaS726
) {
  moonbit_string_t _M0L1sS727;
  moonbit_string_t _M0L6_2atmpS2882;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS726) {
    return (moonbit_string_t)moonbit_string_literal_53.data;
  }
  if (_M0L4signS728) {
    _M0L1sS727 = (moonbit_string_t)moonbit_string_literal_54.data;
  } else {
    _M0L1sS727 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS729) {
    moonbit_string_t _M0L6_2atmpS2883;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2883
    = moonbit_add_string(_M0L1sS727, (moonbit_string_t)moonbit_string_literal_55.data);
    moonbit_decref(_M0L1sS727);
    return _M0L6_2atmpS2883;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2882
  = moonbit_add_string(_M0L1sS727, (moonbit_string_t)moonbit_string_literal_56.data);
  moonbit_decref(_M0L1sS727);
  return _M0L6_2atmpS2882;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS725) {
  int32_t _M0L6_2atmpS2134;
  uint32_t _M0L6_2atmpS2133;
  uint32_t _M0L6_2atmpS2132;
  int32_t _M0L6_2atmpS2131;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2134 = _M0L1eS725 * 1217359;
  _M0L6_2atmpS2133 = *(uint32_t*)&_M0L6_2atmpS2134;
  _M0L6_2atmpS2132 = _M0L6_2atmpS2133 >> 19;
  _M0L6_2atmpS2131 = *(int32_t*)&_M0L6_2atmpS2132;
  return _M0L6_2atmpS2131 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS724,
  struct _M0TPB6Hasher* _M0L6hasherS723
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS723, _M0L4selfS724);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS722,
  struct _M0TPB6Hasher* _M0L6hasherS721
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS721, _M0L4selfS722);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS719,
  moonbit_string_t _M0L5valueS717
) {
  int32_t _M0L7_2abindS716;
  int32_t _M0L1iS718;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS716 = Moonbit_array_length(_M0L5valueS717);
  _M0L1iS718 = 0;
  while (1) {
    if (_M0L1iS718 < _M0L7_2abindS716) {
      int32_t _M0L6_2atmpS2129 = _M0L5valueS717[_M0L1iS718];
      int32_t _M0L6_2atmpS2128 = (int32_t)_M0L6_2atmpS2129;
      uint32_t _M0L6_2atmpS2127 = *(uint32_t*)&_M0L6_2atmpS2128;
      int32_t _M0L6_2atmpS2130;
      moonbit_incref(_M0L4selfS719);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS719, _M0L6_2atmpS2127);
      _M0L6_2atmpS2130 = _M0L1iS718 + 1;
      _M0L1iS718 = _M0L6_2atmpS2130;
      continue;
    } else {
      moonbit_decref(_M0L4selfS719);
      moonbit_decref(_M0L5valueS717);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS714,
  int32_t _M0L3idxS715
) {
  int32_t _M0L6_2atmpS2884;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2884 = _M0L4selfS714[_M0L3idxS715];
  moonbit_decref(_M0L4selfS714);
  return _M0L6_2atmpS2884;
}

void* _M0IPC16uint646UInt64PB6ToJson8to__json(uint64_t _M0L4selfS713) {
  moonbit_string_t _M0L6_2atmpS2126;
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2126
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS713, 10);
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS2126);
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS712) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS712;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS711) {
  void* _block_3227;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3227 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3227)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3227)->$0 = _M0L6stringS711;
  return _block_3227;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS704
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2885;
  int32_t _M0L6_2acntS3104;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2125;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS703;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__* _closure_3228;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2120;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2885 = _M0L4selfS704->$5;
  _M0L6_2acntS3104 = Moonbit_object_header(_M0L4selfS704)->rc;
  if (_M0L6_2acntS3104 > 1) {
    int32_t _M0L11_2anew__cntS3106 = _M0L6_2acntS3104 - 1;
    Moonbit_object_header(_M0L4selfS704)->rc = _M0L11_2anew__cntS3106;
    if (_M0L8_2afieldS2885) {
      moonbit_incref(_M0L8_2afieldS2885);
    }
  } else if (_M0L6_2acntS3104 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3105 = _M0L4selfS704->$0;
    moonbit_decref(_M0L8_2afieldS3105);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS704);
  }
  _M0L4headS2125 = _M0L8_2afieldS2885;
  _M0L11curr__entryS703
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS703)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS703->$0 = _M0L4headS2125;
  _closure_3228
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__));
  Moonbit_object_header(_closure_3228)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__, $0) >> 2, 1, 0);
  _closure_3228->code = &_M0MPB3Map4iterGsRPB4JsonEC2121l591;
  _closure_3228->$0 = _M0L11curr__entryS703;
  _M0L6_2atmpS2120 = (struct _M0TWEOUsRPB4JsonE*)_closure_3228;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2120);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2121l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2122
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__* _M0L14_2acasted__envS2123;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS2891;
  int32_t _M0L6_2acntS3107;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS703;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2890;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS705;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2123
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2121__l591__*)_M0L6_2aenvS2122;
  _M0L8_2afieldS2891 = _M0L14_2acasted__envS2123->$0;
  _M0L6_2acntS3107 = Moonbit_object_header(_M0L14_2acasted__envS2123)->rc;
  if (_M0L6_2acntS3107 > 1) {
    int32_t _M0L11_2anew__cntS3108 = _M0L6_2acntS3107 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2123)->rc
    = _M0L11_2anew__cntS3108;
    moonbit_incref(_M0L8_2afieldS2891);
  } else if (_M0L6_2acntS3107 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2123);
  }
  _M0L11curr__entryS703 = _M0L8_2afieldS2891;
  _M0L8_2afieldS2890 = _M0L11curr__entryS703->$0;
  _M0L7_2abindS705 = _M0L8_2afieldS2890;
  if (_M0L7_2abindS705 == 0) {
    moonbit_decref(_M0L11curr__entryS703);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS706 = _M0L7_2abindS705;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS707 = _M0L7_2aSomeS706;
    moonbit_string_t _M0L8_2afieldS2889 = _M0L4_2axS707->$4;
    moonbit_string_t _M0L6_2akeyS708 = _M0L8_2afieldS2889;
    void* _M0L8_2afieldS2888 = _M0L4_2axS707->$5;
    void* _M0L8_2avalueS709 = _M0L8_2afieldS2888;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS2887 = _M0L4_2axS707->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS710 = _M0L8_2afieldS2887;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS2886 =
      _M0L11curr__entryS703->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2124;
    if (_M0L7_2anextS710) {
      moonbit_incref(_M0L7_2anextS710);
    }
    moonbit_incref(_M0L8_2avalueS709);
    moonbit_incref(_M0L6_2akeyS708);
    if (_M0L6_2aoldS2886) {
      moonbit_decref(_M0L6_2aoldS2886);
    }
    _M0L11curr__entryS703->$0 = _M0L7_2anextS710;
    moonbit_decref(_M0L11curr__entryS703);
    _M0L8_2atupleS2124
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2124)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2124->$0 = _M0L6_2akeyS708;
    _M0L8_2atupleS2124->$1 = _M0L8_2avalueS709;
    return _M0L8_2atupleS2124;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS702
) {
  int32_t _M0L8_2afieldS2892;
  int32_t _M0L4sizeS2119;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2892 = _M0L4selfS702->$1;
  moonbit_decref(_M0L4selfS702);
  _M0L4sizeS2119 = _M0L8_2afieldS2892;
  return _M0L4sizeS2119 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS689,
  int32_t _M0L3keyS685
) {
  int32_t _M0L4hashS684;
  int32_t _M0L14capacity__maskS2104;
  int32_t _M0L6_2atmpS2103;
  int32_t _M0L1iS686;
  int32_t _M0L3idxS687;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS684 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS685);
  _M0L14capacity__maskS2104 = _M0L4selfS689->$3;
  _M0L6_2atmpS2103 = _M0L4hashS684 & _M0L14capacity__maskS2104;
  _M0L1iS686 = 0;
  _M0L3idxS687 = _M0L6_2atmpS2103;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2896 =
      _M0L4selfS689->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2102 =
      _M0L8_2afieldS2896;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2895;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS688;
    if (
      _M0L3idxS687 < 0
      || _M0L3idxS687 >= Moonbit_array_length(_M0L7entriesS2102)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2895
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2102[
        _M0L3idxS687
      ];
    _M0L7_2abindS688 = _M0L6_2atmpS2895;
    if (_M0L7_2abindS688 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2091;
      if (_M0L7_2abindS688) {
        moonbit_incref(_M0L7_2abindS688);
      }
      moonbit_decref(_M0L4selfS689);
      if (_M0L7_2abindS688) {
        moonbit_decref(_M0L7_2abindS688);
      }
      _M0L6_2atmpS2091 = 0;
      return _M0L6_2atmpS2091;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS690 =
        _M0L7_2abindS688;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS691 =
        _M0L7_2aSomeS690;
      int32_t _M0L4hashS2093 = _M0L8_2aentryS691->$3;
      int32_t _if__result_3230;
      int32_t _M0L8_2afieldS2893;
      int32_t _M0L3pslS2096;
      int32_t _M0L6_2atmpS2098;
      int32_t _M0L6_2atmpS2100;
      int32_t _M0L14capacity__maskS2101;
      int32_t _M0L6_2atmpS2099;
      if (_M0L4hashS2093 == _M0L4hashS684) {
        int32_t _M0L3keyS2092 = _M0L8_2aentryS691->$4;
        _if__result_3230 = _M0L3keyS2092 == _M0L3keyS685;
      } else {
        _if__result_3230 = 0;
      }
      if (_if__result_3230) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2894;
        int32_t _M0L6_2acntS3109;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2095;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2094;
        moonbit_incref(_M0L8_2aentryS691);
        moonbit_decref(_M0L4selfS689);
        _M0L8_2afieldS2894 = _M0L8_2aentryS691->$5;
        _M0L6_2acntS3109 = Moonbit_object_header(_M0L8_2aentryS691)->rc;
        if (_M0L6_2acntS3109 > 1) {
          int32_t _M0L11_2anew__cntS3111 = _M0L6_2acntS3109 - 1;
          Moonbit_object_header(_M0L8_2aentryS691)->rc
          = _M0L11_2anew__cntS3111;
          moonbit_incref(_M0L8_2afieldS2894);
        } else if (_M0L6_2acntS3109 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3110 =
            _M0L8_2aentryS691->$1;
          if (_M0L8_2afieldS3110) {
            moonbit_decref(_M0L8_2afieldS3110);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS691);
        }
        _M0L5valueS2095 = _M0L8_2afieldS2894;
        _M0L6_2atmpS2094 = _M0L5valueS2095;
        return _M0L6_2atmpS2094;
      } else {
        moonbit_incref(_M0L8_2aentryS691);
      }
      _M0L8_2afieldS2893 = _M0L8_2aentryS691->$2;
      moonbit_decref(_M0L8_2aentryS691);
      _M0L3pslS2096 = _M0L8_2afieldS2893;
      if (_M0L1iS686 > _M0L3pslS2096) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2097;
        moonbit_decref(_M0L4selfS689);
        _M0L6_2atmpS2097 = 0;
        return _M0L6_2atmpS2097;
      }
      _M0L6_2atmpS2098 = _M0L1iS686 + 1;
      _M0L6_2atmpS2100 = _M0L3idxS687 + 1;
      _M0L14capacity__maskS2101 = _M0L4selfS689->$3;
      _M0L6_2atmpS2099 = _M0L6_2atmpS2100 & _M0L14capacity__maskS2101;
      _M0L1iS686 = _M0L6_2atmpS2098;
      _M0L3idxS687 = _M0L6_2atmpS2099;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS698,
  moonbit_string_t _M0L3keyS694
) {
  int32_t _M0L4hashS693;
  int32_t _M0L14capacity__maskS2118;
  int32_t _M0L6_2atmpS2117;
  int32_t _M0L1iS695;
  int32_t _M0L3idxS696;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS694);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS693 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS694);
  _M0L14capacity__maskS2118 = _M0L4selfS698->$3;
  _M0L6_2atmpS2117 = _M0L4hashS693 & _M0L14capacity__maskS2118;
  _M0L1iS695 = 0;
  _M0L3idxS696 = _M0L6_2atmpS2117;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2902 =
      _M0L4selfS698->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2116 =
      _M0L8_2afieldS2902;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2901;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS697;
    if (
      _M0L3idxS696 < 0
      || _M0L3idxS696 >= Moonbit_array_length(_M0L7entriesS2116)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2901
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2116[
        _M0L3idxS696
      ];
    _M0L7_2abindS697 = _M0L6_2atmpS2901;
    if (_M0L7_2abindS697 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2105;
      if (_M0L7_2abindS697) {
        moonbit_incref(_M0L7_2abindS697);
      }
      moonbit_decref(_M0L4selfS698);
      if (_M0L7_2abindS697) {
        moonbit_decref(_M0L7_2abindS697);
      }
      moonbit_decref(_M0L3keyS694);
      _M0L6_2atmpS2105 = 0;
      return _M0L6_2atmpS2105;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS699 =
        _M0L7_2abindS697;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS700 =
        _M0L7_2aSomeS699;
      int32_t _M0L4hashS2107 = _M0L8_2aentryS700->$3;
      int32_t _if__result_3232;
      int32_t _M0L8_2afieldS2897;
      int32_t _M0L3pslS2110;
      int32_t _M0L6_2atmpS2112;
      int32_t _M0L6_2atmpS2114;
      int32_t _M0L14capacity__maskS2115;
      int32_t _M0L6_2atmpS2113;
      if (_M0L4hashS2107 == _M0L4hashS693) {
        moonbit_string_t _M0L8_2afieldS2900 = _M0L8_2aentryS700->$4;
        moonbit_string_t _M0L3keyS2106 = _M0L8_2afieldS2900;
        int32_t _M0L6_2atmpS2899;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2899
        = moonbit_val_array_equal(_M0L3keyS2106, _M0L3keyS694);
        _if__result_3232 = _M0L6_2atmpS2899;
      } else {
        _if__result_3232 = 0;
      }
      if (_if__result_3232) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2898;
        int32_t _M0L6_2acntS3112;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2109;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2108;
        moonbit_incref(_M0L8_2aentryS700);
        moonbit_decref(_M0L4selfS698);
        moonbit_decref(_M0L3keyS694);
        _M0L8_2afieldS2898 = _M0L8_2aentryS700->$5;
        _M0L6_2acntS3112 = Moonbit_object_header(_M0L8_2aentryS700)->rc;
        if (_M0L6_2acntS3112 > 1) {
          int32_t _M0L11_2anew__cntS3115 = _M0L6_2acntS3112 - 1;
          Moonbit_object_header(_M0L8_2aentryS700)->rc
          = _M0L11_2anew__cntS3115;
          moonbit_incref(_M0L8_2afieldS2898);
        } else if (_M0L6_2acntS3112 == 1) {
          moonbit_string_t _M0L8_2afieldS3114 = _M0L8_2aentryS700->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3113;
          moonbit_decref(_M0L8_2afieldS3114);
          _M0L8_2afieldS3113 = _M0L8_2aentryS700->$1;
          if (_M0L8_2afieldS3113) {
            moonbit_decref(_M0L8_2afieldS3113);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS700);
        }
        _M0L5valueS2109 = _M0L8_2afieldS2898;
        _M0L6_2atmpS2108 = _M0L5valueS2109;
        return _M0L6_2atmpS2108;
      } else {
        moonbit_incref(_M0L8_2aentryS700);
      }
      _M0L8_2afieldS2897 = _M0L8_2aentryS700->$2;
      moonbit_decref(_M0L8_2aentryS700);
      _M0L3pslS2110 = _M0L8_2afieldS2897;
      if (_M0L1iS695 > _M0L3pslS2110) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2111;
        moonbit_decref(_M0L4selfS698);
        moonbit_decref(_M0L3keyS694);
        _M0L6_2atmpS2111 = 0;
        return _M0L6_2atmpS2111;
      }
      _M0L6_2atmpS2112 = _M0L1iS695 + 1;
      _M0L6_2atmpS2114 = _M0L3idxS696 + 1;
      _M0L14capacity__maskS2115 = _M0L4selfS698->$3;
      _M0L6_2atmpS2113 = _M0L6_2atmpS2114 & _M0L14capacity__maskS2115;
      _M0L1iS695 = _M0L6_2atmpS2112;
      _M0L3idxS696 = _M0L6_2atmpS2113;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS669
) {
  int32_t _M0L6lengthS668;
  int32_t _M0Lm8capacityS670;
  int32_t _M0L6_2atmpS2068;
  int32_t _M0L6_2atmpS2067;
  int32_t _M0L6_2atmpS2078;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS671;
  int32_t _M0L3endS2076;
  int32_t _M0L5startS2077;
  int32_t _M0L7_2abindS672;
  int32_t _M0L2__S673;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS669.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS668
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS669);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS670 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS668);
  _M0L6_2atmpS2068 = _M0Lm8capacityS670;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2067 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2068);
  if (_M0L6lengthS668 > _M0L6_2atmpS2067) {
    int32_t _M0L6_2atmpS2069 = _M0Lm8capacityS670;
    _M0Lm8capacityS670 = _M0L6_2atmpS2069 * 2;
  }
  _M0L6_2atmpS2078 = _M0Lm8capacityS670;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS671
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2078);
  _M0L3endS2076 = _M0L3arrS669.$2;
  _M0L5startS2077 = _M0L3arrS669.$1;
  _M0L7_2abindS672 = _M0L3endS2076 - _M0L5startS2077;
  _M0L2__S673 = 0;
  while (1) {
    if (_M0L2__S673 < _M0L7_2abindS672) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2906 =
        _M0L3arrS669.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2073 =
        _M0L8_2afieldS2906;
      int32_t _M0L5startS2075 = _M0L3arrS669.$1;
      int32_t _M0L6_2atmpS2074 = _M0L5startS2075 + _M0L2__S673;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2905 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2073[
          _M0L6_2atmpS2074
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS674 =
        _M0L6_2atmpS2905;
      moonbit_string_t _M0L8_2afieldS2904 = _M0L1eS674->$0;
      moonbit_string_t _M0L6_2atmpS2070 = _M0L8_2afieldS2904;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2903 =
        _M0L1eS674->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2071 =
        _M0L8_2afieldS2903;
      int32_t _M0L6_2atmpS2072;
      moonbit_incref(_M0L6_2atmpS2071);
      moonbit_incref(_M0L6_2atmpS2070);
      moonbit_incref(_M0L1mS671);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS671, _M0L6_2atmpS2070, _M0L6_2atmpS2071);
      _M0L6_2atmpS2072 = _M0L2__S673 + 1;
      _M0L2__S673 = _M0L6_2atmpS2072;
      continue;
    } else {
      moonbit_decref(_M0L3arrS669.$0);
    }
    break;
  }
  return _M0L1mS671;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS677
) {
  int32_t _M0L6lengthS676;
  int32_t _M0Lm8capacityS678;
  int32_t _M0L6_2atmpS2080;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L6_2atmpS2090;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS679;
  int32_t _M0L3endS2088;
  int32_t _M0L5startS2089;
  int32_t _M0L7_2abindS680;
  int32_t _M0L2__S681;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS677.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS676
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS677);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS678 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS676);
  _M0L6_2atmpS2080 = _M0Lm8capacityS678;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2079 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2080);
  if (_M0L6lengthS676 > _M0L6_2atmpS2079) {
    int32_t _M0L6_2atmpS2081 = _M0Lm8capacityS678;
    _M0Lm8capacityS678 = _M0L6_2atmpS2081 * 2;
  }
  _M0L6_2atmpS2090 = _M0Lm8capacityS678;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS679
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2090);
  _M0L3endS2088 = _M0L3arrS677.$2;
  _M0L5startS2089 = _M0L3arrS677.$1;
  _M0L7_2abindS680 = _M0L3endS2088 - _M0L5startS2089;
  _M0L2__S681 = 0;
  while (1) {
    if (_M0L2__S681 < _M0L7_2abindS680) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2909 =
        _M0L3arrS677.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2085 =
        _M0L8_2afieldS2909;
      int32_t _M0L5startS2087 = _M0L3arrS677.$1;
      int32_t _M0L6_2atmpS2086 = _M0L5startS2087 + _M0L2__S681;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2908 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2085[
          _M0L6_2atmpS2086
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS682 = _M0L6_2atmpS2908;
      int32_t _M0L6_2atmpS2082 = _M0L1eS682->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2907 =
        _M0L1eS682->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2083 =
        _M0L8_2afieldS2907;
      int32_t _M0L6_2atmpS2084;
      moonbit_incref(_M0L6_2atmpS2083);
      moonbit_incref(_M0L1mS679);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS679, _M0L6_2atmpS2082, _M0L6_2atmpS2083);
      _M0L6_2atmpS2084 = _M0L2__S681 + 1;
      _M0L2__S681 = _M0L6_2atmpS2084;
      continue;
    } else {
      moonbit_decref(_M0L3arrS677.$0);
    }
    break;
  }
  return _M0L1mS679;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS662,
  moonbit_string_t _M0L3keyS663,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS664
) {
  int32_t _M0L6_2atmpS2065;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS663);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2065 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS663);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS662, _M0L3keyS663, _M0L5valueS664, _M0L6_2atmpS2065);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS665,
  int32_t _M0L3keyS666,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS667
) {
  int32_t _M0L6_2atmpS2066;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2066 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS666);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS665, _M0L3keyS666, _M0L5valueS667, _M0L6_2atmpS2066);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS641
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2916;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS640;
  int32_t _M0L8capacityS2057;
  int32_t _M0L13new__capacityS642;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2052;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2051;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2915;
  int32_t _M0L6_2atmpS2053;
  int32_t _M0L8capacityS2055;
  int32_t _M0L6_2atmpS2054;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2056;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2914;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS643;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2916 = _M0L4selfS641->$5;
  _M0L9old__headS640 = _M0L8_2afieldS2916;
  _M0L8capacityS2057 = _M0L4selfS641->$2;
  _M0L13new__capacityS642 = _M0L8capacityS2057 << 1;
  _M0L6_2atmpS2052 = 0;
  _M0L6_2atmpS2051
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS642, _M0L6_2atmpS2052);
  _M0L6_2aoldS2915 = _M0L4selfS641->$0;
  if (_M0L9old__headS640) {
    moonbit_incref(_M0L9old__headS640);
  }
  moonbit_decref(_M0L6_2aoldS2915);
  _M0L4selfS641->$0 = _M0L6_2atmpS2051;
  _M0L4selfS641->$2 = _M0L13new__capacityS642;
  _M0L6_2atmpS2053 = _M0L13new__capacityS642 - 1;
  _M0L4selfS641->$3 = _M0L6_2atmpS2053;
  _M0L8capacityS2055 = _M0L4selfS641->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2054 = _M0FPB21calc__grow__threshold(_M0L8capacityS2055);
  _M0L4selfS641->$4 = _M0L6_2atmpS2054;
  _M0L4selfS641->$1 = 0;
  _M0L6_2atmpS2056 = 0;
  _M0L6_2aoldS2914 = _M0L4selfS641->$5;
  if (_M0L6_2aoldS2914) {
    moonbit_decref(_M0L6_2aoldS2914);
  }
  _M0L4selfS641->$5 = _M0L6_2atmpS2056;
  _M0L4selfS641->$6 = -1;
  _M0L8_2aparamS643 = _M0L9old__headS640;
  while (1) {
    if (_M0L8_2aparamS643 == 0) {
      if (_M0L8_2aparamS643) {
        moonbit_decref(_M0L8_2aparamS643);
      }
      moonbit_decref(_M0L4selfS641);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS644 =
        _M0L8_2aparamS643;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS645 =
        _M0L7_2aSomeS644;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2913 =
        _M0L4_2axS645->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS646 =
        _M0L8_2afieldS2913;
      moonbit_string_t _M0L8_2afieldS2912 = _M0L4_2axS645->$4;
      moonbit_string_t _M0L6_2akeyS647 = _M0L8_2afieldS2912;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2911 =
        _M0L4_2axS645->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS648 =
        _M0L8_2afieldS2911;
      int32_t _M0L8_2afieldS2910 = _M0L4_2axS645->$3;
      int32_t _M0L6_2acntS3116 = Moonbit_object_header(_M0L4_2axS645)->rc;
      int32_t _M0L7_2ahashS649;
      if (_M0L6_2acntS3116 > 1) {
        int32_t _M0L11_2anew__cntS3117 = _M0L6_2acntS3116 - 1;
        Moonbit_object_header(_M0L4_2axS645)->rc = _M0L11_2anew__cntS3117;
        moonbit_incref(_M0L8_2avalueS648);
        moonbit_incref(_M0L6_2akeyS647);
        if (_M0L7_2anextS646) {
          moonbit_incref(_M0L7_2anextS646);
        }
      } else if (_M0L6_2acntS3116 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS645);
      }
      _M0L7_2ahashS649 = _M0L8_2afieldS2910;
      moonbit_incref(_M0L4selfS641);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS641, _M0L6_2akeyS647, _M0L8_2avalueS648, _M0L7_2ahashS649);
      _M0L8_2aparamS643 = _M0L7_2anextS646;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS652
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2922;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS651;
  int32_t _M0L8capacityS2064;
  int32_t _M0L13new__capacityS653;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2059;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2058;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2921;
  int32_t _M0L6_2atmpS2060;
  int32_t _M0L8capacityS2062;
  int32_t _M0L6_2atmpS2061;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2063;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2920;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS654;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2922 = _M0L4selfS652->$5;
  _M0L9old__headS651 = _M0L8_2afieldS2922;
  _M0L8capacityS2064 = _M0L4selfS652->$2;
  _M0L13new__capacityS653 = _M0L8capacityS2064 << 1;
  _M0L6_2atmpS2059 = 0;
  _M0L6_2atmpS2058
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS653, _M0L6_2atmpS2059);
  _M0L6_2aoldS2921 = _M0L4selfS652->$0;
  if (_M0L9old__headS651) {
    moonbit_incref(_M0L9old__headS651);
  }
  moonbit_decref(_M0L6_2aoldS2921);
  _M0L4selfS652->$0 = _M0L6_2atmpS2058;
  _M0L4selfS652->$2 = _M0L13new__capacityS653;
  _M0L6_2atmpS2060 = _M0L13new__capacityS653 - 1;
  _M0L4selfS652->$3 = _M0L6_2atmpS2060;
  _M0L8capacityS2062 = _M0L4selfS652->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2061 = _M0FPB21calc__grow__threshold(_M0L8capacityS2062);
  _M0L4selfS652->$4 = _M0L6_2atmpS2061;
  _M0L4selfS652->$1 = 0;
  _M0L6_2atmpS2063 = 0;
  _M0L6_2aoldS2920 = _M0L4selfS652->$5;
  if (_M0L6_2aoldS2920) {
    moonbit_decref(_M0L6_2aoldS2920);
  }
  _M0L4selfS652->$5 = _M0L6_2atmpS2063;
  _M0L4selfS652->$6 = -1;
  _M0L8_2aparamS654 = _M0L9old__headS651;
  while (1) {
    if (_M0L8_2aparamS654 == 0) {
      if (_M0L8_2aparamS654) {
        moonbit_decref(_M0L8_2aparamS654);
      }
      moonbit_decref(_M0L4selfS652);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS655 =
        _M0L8_2aparamS654;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS656 =
        _M0L7_2aSomeS655;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2919 =
        _M0L4_2axS656->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS657 =
        _M0L8_2afieldS2919;
      int32_t _M0L6_2akeyS658 = _M0L4_2axS656->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2918 =
        _M0L4_2axS656->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS659 =
        _M0L8_2afieldS2918;
      int32_t _M0L8_2afieldS2917 = _M0L4_2axS656->$3;
      int32_t _M0L6_2acntS3118 = Moonbit_object_header(_M0L4_2axS656)->rc;
      int32_t _M0L7_2ahashS660;
      if (_M0L6_2acntS3118 > 1) {
        int32_t _M0L11_2anew__cntS3119 = _M0L6_2acntS3118 - 1;
        Moonbit_object_header(_M0L4_2axS656)->rc = _M0L11_2anew__cntS3119;
        moonbit_incref(_M0L8_2avalueS659);
        if (_M0L7_2anextS657) {
          moonbit_incref(_M0L7_2anextS657);
        }
      } else if (_M0L6_2acntS3118 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS656);
      }
      _M0L7_2ahashS660 = _M0L8_2afieldS2917;
      moonbit_incref(_M0L4selfS652);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS652, _M0L6_2akeyS658, _M0L8_2avalueS659, _M0L7_2ahashS660);
      _M0L8_2aparamS654 = _M0L7_2anextS657;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS611,
  moonbit_string_t _M0L3keyS617,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS618,
  int32_t _M0L4hashS613
) {
  int32_t _M0L14capacity__maskS2032;
  int32_t _M0L6_2atmpS2031;
  int32_t _M0L3pslS608;
  int32_t _M0L3idxS609;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2032 = _M0L4selfS611->$3;
  _M0L6_2atmpS2031 = _M0L4hashS613 & _M0L14capacity__maskS2032;
  _M0L3pslS608 = 0;
  _M0L3idxS609 = _M0L6_2atmpS2031;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2927 =
      _M0L4selfS611->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2030 =
      _M0L8_2afieldS2927;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2926;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS610;
    if (
      _M0L3idxS609 < 0
      || _M0L3idxS609 >= Moonbit_array_length(_M0L7entriesS2030)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2926
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2030[
        _M0L3idxS609
      ];
    _M0L7_2abindS610 = _M0L6_2atmpS2926;
    if (_M0L7_2abindS610 == 0) {
      int32_t _M0L4sizeS2015 = _M0L4selfS611->$1;
      int32_t _M0L8grow__atS2016 = _M0L4selfS611->$4;
      int32_t _M0L7_2abindS614;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS615;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS616;
      if (_M0L4sizeS2015 >= _M0L8grow__atS2016) {
        int32_t _M0L14capacity__maskS2018;
        int32_t _M0L6_2atmpS2017;
        moonbit_incref(_M0L4selfS611);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS611);
        _M0L14capacity__maskS2018 = _M0L4selfS611->$3;
        _M0L6_2atmpS2017 = _M0L4hashS613 & _M0L14capacity__maskS2018;
        _M0L3pslS608 = 0;
        _M0L3idxS609 = _M0L6_2atmpS2017;
        continue;
      }
      _M0L7_2abindS614 = _M0L4selfS611->$6;
      _M0L7_2abindS615 = 0;
      _M0L5entryS616
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS616)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS616->$0 = _M0L7_2abindS614;
      _M0L5entryS616->$1 = _M0L7_2abindS615;
      _M0L5entryS616->$2 = _M0L3pslS608;
      _M0L5entryS616->$3 = _M0L4hashS613;
      _M0L5entryS616->$4 = _M0L3keyS617;
      _M0L5entryS616->$5 = _M0L5valueS618;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS611, _M0L3idxS609, _M0L5entryS616);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS619 =
        _M0L7_2abindS610;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS620 =
        _M0L7_2aSomeS619;
      int32_t _M0L4hashS2020 = _M0L14_2acurr__entryS620->$3;
      int32_t _if__result_3238;
      int32_t _M0L3pslS2021;
      int32_t _M0L6_2atmpS2026;
      int32_t _M0L6_2atmpS2028;
      int32_t _M0L14capacity__maskS2029;
      int32_t _M0L6_2atmpS2027;
      if (_M0L4hashS2020 == _M0L4hashS613) {
        moonbit_string_t _M0L8_2afieldS2925 = _M0L14_2acurr__entryS620->$4;
        moonbit_string_t _M0L3keyS2019 = _M0L8_2afieldS2925;
        int32_t _M0L6_2atmpS2924;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2924
        = moonbit_val_array_equal(_M0L3keyS2019, _M0L3keyS617);
        _if__result_3238 = _M0L6_2atmpS2924;
      } else {
        _if__result_3238 = 0;
      }
      if (_if__result_3238) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2923;
        moonbit_incref(_M0L14_2acurr__entryS620);
        moonbit_decref(_M0L3keyS617);
        moonbit_decref(_M0L4selfS611);
        _M0L6_2aoldS2923 = _M0L14_2acurr__entryS620->$5;
        moonbit_decref(_M0L6_2aoldS2923);
        _M0L14_2acurr__entryS620->$5 = _M0L5valueS618;
        moonbit_decref(_M0L14_2acurr__entryS620);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS620);
      }
      _M0L3pslS2021 = _M0L14_2acurr__entryS620->$2;
      if (_M0L3pslS608 > _M0L3pslS2021) {
        int32_t _M0L4sizeS2022 = _M0L4selfS611->$1;
        int32_t _M0L8grow__atS2023 = _M0L4selfS611->$4;
        int32_t _M0L7_2abindS621;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS622;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS623;
        if (_M0L4sizeS2022 >= _M0L8grow__atS2023) {
          int32_t _M0L14capacity__maskS2025;
          int32_t _M0L6_2atmpS2024;
          moonbit_decref(_M0L14_2acurr__entryS620);
          moonbit_incref(_M0L4selfS611);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS611);
          _M0L14capacity__maskS2025 = _M0L4selfS611->$3;
          _M0L6_2atmpS2024 = _M0L4hashS613 & _M0L14capacity__maskS2025;
          _M0L3pslS608 = 0;
          _M0L3idxS609 = _M0L6_2atmpS2024;
          continue;
        }
        moonbit_incref(_M0L4selfS611);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS611, _M0L3idxS609, _M0L14_2acurr__entryS620);
        _M0L7_2abindS621 = _M0L4selfS611->$6;
        _M0L7_2abindS622 = 0;
        _M0L5entryS623
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS623)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS623->$0 = _M0L7_2abindS621;
        _M0L5entryS623->$1 = _M0L7_2abindS622;
        _M0L5entryS623->$2 = _M0L3pslS608;
        _M0L5entryS623->$3 = _M0L4hashS613;
        _M0L5entryS623->$4 = _M0L3keyS617;
        _M0L5entryS623->$5 = _M0L5valueS618;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS611, _M0L3idxS609, _M0L5entryS623);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS620);
      }
      _M0L6_2atmpS2026 = _M0L3pslS608 + 1;
      _M0L6_2atmpS2028 = _M0L3idxS609 + 1;
      _M0L14capacity__maskS2029 = _M0L4selfS611->$3;
      _M0L6_2atmpS2027 = _M0L6_2atmpS2028 & _M0L14capacity__maskS2029;
      _M0L3pslS608 = _M0L6_2atmpS2026;
      _M0L3idxS609 = _M0L6_2atmpS2027;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS627,
  int32_t _M0L3keyS633,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS634,
  int32_t _M0L4hashS629
) {
  int32_t _M0L14capacity__maskS2050;
  int32_t _M0L6_2atmpS2049;
  int32_t _M0L3pslS624;
  int32_t _M0L3idxS625;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2050 = _M0L4selfS627->$3;
  _M0L6_2atmpS2049 = _M0L4hashS629 & _M0L14capacity__maskS2050;
  _M0L3pslS624 = 0;
  _M0L3idxS625 = _M0L6_2atmpS2049;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2930 =
      _M0L4selfS627->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2048 =
      _M0L8_2afieldS2930;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2929;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS626;
    if (
      _M0L3idxS625 < 0
      || _M0L3idxS625 >= Moonbit_array_length(_M0L7entriesS2048)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2929
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2048[
        _M0L3idxS625
      ];
    _M0L7_2abindS626 = _M0L6_2atmpS2929;
    if (_M0L7_2abindS626 == 0) {
      int32_t _M0L4sizeS2033 = _M0L4selfS627->$1;
      int32_t _M0L8grow__atS2034 = _M0L4selfS627->$4;
      int32_t _M0L7_2abindS630;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS631;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS632;
      if (_M0L4sizeS2033 >= _M0L8grow__atS2034) {
        int32_t _M0L14capacity__maskS2036;
        int32_t _M0L6_2atmpS2035;
        moonbit_incref(_M0L4selfS627);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627);
        _M0L14capacity__maskS2036 = _M0L4selfS627->$3;
        _M0L6_2atmpS2035 = _M0L4hashS629 & _M0L14capacity__maskS2036;
        _M0L3pslS624 = 0;
        _M0L3idxS625 = _M0L6_2atmpS2035;
        continue;
      }
      _M0L7_2abindS630 = _M0L4selfS627->$6;
      _M0L7_2abindS631 = 0;
      _M0L5entryS632
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS632)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS632->$0 = _M0L7_2abindS630;
      _M0L5entryS632->$1 = _M0L7_2abindS631;
      _M0L5entryS632->$2 = _M0L3pslS624;
      _M0L5entryS632->$3 = _M0L4hashS629;
      _M0L5entryS632->$4 = _M0L3keyS633;
      _M0L5entryS632->$5 = _M0L5valueS634;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L3idxS625, _M0L5entryS632);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS635 =
        _M0L7_2abindS626;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS636 =
        _M0L7_2aSomeS635;
      int32_t _M0L4hashS2038 = _M0L14_2acurr__entryS636->$3;
      int32_t _if__result_3240;
      int32_t _M0L3pslS2039;
      int32_t _M0L6_2atmpS2044;
      int32_t _M0L6_2atmpS2046;
      int32_t _M0L14capacity__maskS2047;
      int32_t _M0L6_2atmpS2045;
      if (_M0L4hashS2038 == _M0L4hashS629) {
        int32_t _M0L3keyS2037 = _M0L14_2acurr__entryS636->$4;
        _if__result_3240 = _M0L3keyS2037 == _M0L3keyS633;
      } else {
        _if__result_3240 = 0;
      }
      if (_if__result_3240) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2928;
        moonbit_incref(_M0L14_2acurr__entryS636);
        moonbit_decref(_M0L4selfS627);
        _M0L6_2aoldS2928 = _M0L14_2acurr__entryS636->$5;
        moonbit_decref(_M0L6_2aoldS2928);
        _M0L14_2acurr__entryS636->$5 = _M0L5valueS634;
        moonbit_decref(_M0L14_2acurr__entryS636);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS636);
      }
      _M0L3pslS2039 = _M0L14_2acurr__entryS636->$2;
      if (_M0L3pslS624 > _M0L3pslS2039) {
        int32_t _M0L4sizeS2040 = _M0L4selfS627->$1;
        int32_t _M0L8grow__atS2041 = _M0L4selfS627->$4;
        int32_t _M0L7_2abindS637;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS638;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS639;
        if (_M0L4sizeS2040 >= _M0L8grow__atS2041) {
          int32_t _M0L14capacity__maskS2043;
          int32_t _M0L6_2atmpS2042;
          moonbit_decref(_M0L14_2acurr__entryS636);
          moonbit_incref(_M0L4selfS627);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627);
          _M0L14capacity__maskS2043 = _M0L4selfS627->$3;
          _M0L6_2atmpS2042 = _M0L4hashS629 & _M0L14capacity__maskS2043;
          _M0L3pslS624 = 0;
          _M0L3idxS625 = _M0L6_2atmpS2042;
          continue;
        }
        moonbit_incref(_M0L4selfS627);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L3idxS625, _M0L14_2acurr__entryS636);
        _M0L7_2abindS637 = _M0L4selfS627->$6;
        _M0L7_2abindS638 = 0;
        _M0L5entryS639
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS639)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS639->$0 = _M0L7_2abindS637;
        _M0L5entryS639->$1 = _M0L7_2abindS638;
        _M0L5entryS639->$2 = _M0L3pslS624;
        _M0L5entryS639->$3 = _M0L4hashS629;
        _M0L5entryS639->$4 = _M0L3keyS633;
        _M0L5entryS639->$5 = _M0L5valueS634;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS627, _M0L3idxS625, _M0L5entryS639);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS636);
      }
      _M0L6_2atmpS2044 = _M0L3pslS624 + 1;
      _M0L6_2atmpS2046 = _M0L3idxS625 + 1;
      _M0L14capacity__maskS2047 = _M0L4selfS627->$3;
      _M0L6_2atmpS2045 = _M0L6_2atmpS2046 & _M0L14capacity__maskS2047;
      _M0L3pslS624 = _M0L6_2atmpS2044;
      _M0L3idxS625 = _M0L6_2atmpS2045;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS592,
  int32_t _M0L3idxS597,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS596
) {
  int32_t _M0L3pslS1998;
  int32_t _M0L6_2atmpS1994;
  int32_t _M0L6_2atmpS1996;
  int32_t _M0L14capacity__maskS1997;
  int32_t _M0L6_2atmpS1995;
  int32_t _M0L3pslS588;
  int32_t _M0L3idxS589;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS590;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1998 = _M0L5entryS596->$2;
  _M0L6_2atmpS1994 = _M0L3pslS1998 + 1;
  _M0L6_2atmpS1996 = _M0L3idxS597 + 1;
  _M0L14capacity__maskS1997 = _M0L4selfS592->$3;
  _M0L6_2atmpS1995 = _M0L6_2atmpS1996 & _M0L14capacity__maskS1997;
  _M0L3pslS588 = _M0L6_2atmpS1994;
  _M0L3idxS589 = _M0L6_2atmpS1995;
  _M0L5entryS590 = _M0L5entryS596;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2932 =
      _M0L4selfS592->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1993 =
      _M0L8_2afieldS2932;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2931;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS591;
    if (
      _M0L3idxS589 < 0
      || _M0L3idxS589 >= Moonbit_array_length(_M0L7entriesS1993)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2931
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1993[
        _M0L3idxS589
      ];
    _M0L7_2abindS591 = _M0L6_2atmpS2931;
    if (_M0L7_2abindS591 == 0) {
      _M0L5entryS590->$2 = _M0L3pslS588;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS592, _M0L5entryS590, _M0L3idxS589);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS594 =
        _M0L7_2abindS591;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS595 =
        _M0L7_2aSomeS594;
      int32_t _M0L3pslS1983 = _M0L14_2acurr__entryS595->$2;
      if (_M0L3pslS588 > _M0L3pslS1983) {
        int32_t _M0L3pslS1988;
        int32_t _M0L6_2atmpS1984;
        int32_t _M0L6_2atmpS1986;
        int32_t _M0L14capacity__maskS1987;
        int32_t _M0L6_2atmpS1985;
        _M0L5entryS590->$2 = _M0L3pslS588;
        moonbit_incref(_M0L14_2acurr__entryS595);
        moonbit_incref(_M0L4selfS592);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS592, _M0L5entryS590, _M0L3idxS589);
        _M0L3pslS1988 = _M0L14_2acurr__entryS595->$2;
        _M0L6_2atmpS1984 = _M0L3pslS1988 + 1;
        _M0L6_2atmpS1986 = _M0L3idxS589 + 1;
        _M0L14capacity__maskS1987 = _M0L4selfS592->$3;
        _M0L6_2atmpS1985 = _M0L6_2atmpS1986 & _M0L14capacity__maskS1987;
        _M0L3pslS588 = _M0L6_2atmpS1984;
        _M0L3idxS589 = _M0L6_2atmpS1985;
        _M0L5entryS590 = _M0L14_2acurr__entryS595;
        continue;
      } else {
        int32_t _M0L6_2atmpS1989 = _M0L3pslS588 + 1;
        int32_t _M0L6_2atmpS1991 = _M0L3idxS589 + 1;
        int32_t _M0L14capacity__maskS1992 = _M0L4selfS592->$3;
        int32_t _M0L6_2atmpS1990 =
          _M0L6_2atmpS1991 & _M0L14capacity__maskS1992;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3242 =
          _M0L5entryS590;
        _M0L3pslS588 = _M0L6_2atmpS1989;
        _M0L3idxS589 = _M0L6_2atmpS1990;
        _M0L5entryS590 = _tmp_3242;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS602,
  int32_t _M0L3idxS607,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS606
) {
  int32_t _M0L3pslS2014;
  int32_t _M0L6_2atmpS2010;
  int32_t _M0L6_2atmpS2012;
  int32_t _M0L14capacity__maskS2013;
  int32_t _M0L6_2atmpS2011;
  int32_t _M0L3pslS598;
  int32_t _M0L3idxS599;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS600;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2014 = _M0L5entryS606->$2;
  _M0L6_2atmpS2010 = _M0L3pslS2014 + 1;
  _M0L6_2atmpS2012 = _M0L3idxS607 + 1;
  _M0L14capacity__maskS2013 = _M0L4selfS602->$3;
  _M0L6_2atmpS2011 = _M0L6_2atmpS2012 & _M0L14capacity__maskS2013;
  _M0L3pslS598 = _M0L6_2atmpS2010;
  _M0L3idxS599 = _M0L6_2atmpS2011;
  _M0L5entryS600 = _M0L5entryS606;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2934 =
      _M0L4selfS602->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2009 =
      _M0L8_2afieldS2934;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2933;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS601;
    if (
      _M0L3idxS599 < 0
      || _M0L3idxS599 >= Moonbit_array_length(_M0L7entriesS2009)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2933
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2009[
        _M0L3idxS599
      ];
    _M0L7_2abindS601 = _M0L6_2atmpS2933;
    if (_M0L7_2abindS601 == 0) {
      _M0L5entryS600->$2 = _M0L3pslS598;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602, _M0L5entryS600, _M0L3idxS599);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS604 =
        _M0L7_2abindS601;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS605 =
        _M0L7_2aSomeS604;
      int32_t _M0L3pslS1999 = _M0L14_2acurr__entryS605->$2;
      if (_M0L3pslS598 > _M0L3pslS1999) {
        int32_t _M0L3pslS2004;
        int32_t _M0L6_2atmpS2000;
        int32_t _M0L6_2atmpS2002;
        int32_t _M0L14capacity__maskS2003;
        int32_t _M0L6_2atmpS2001;
        _M0L5entryS600->$2 = _M0L3pslS598;
        moonbit_incref(_M0L14_2acurr__entryS605);
        moonbit_incref(_M0L4selfS602);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS602, _M0L5entryS600, _M0L3idxS599);
        _M0L3pslS2004 = _M0L14_2acurr__entryS605->$2;
        _M0L6_2atmpS2000 = _M0L3pslS2004 + 1;
        _M0L6_2atmpS2002 = _M0L3idxS599 + 1;
        _M0L14capacity__maskS2003 = _M0L4selfS602->$3;
        _M0L6_2atmpS2001 = _M0L6_2atmpS2002 & _M0L14capacity__maskS2003;
        _M0L3pslS598 = _M0L6_2atmpS2000;
        _M0L3idxS599 = _M0L6_2atmpS2001;
        _M0L5entryS600 = _M0L14_2acurr__entryS605;
        continue;
      } else {
        int32_t _M0L6_2atmpS2005 = _M0L3pslS598 + 1;
        int32_t _M0L6_2atmpS2007 = _M0L3idxS599 + 1;
        int32_t _M0L14capacity__maskS2008 = _M0L4selfS602->$3;
        int32_t _M0L6_2atmpS2006 =
          _M0L6_2atmpS2007 & _M0L14capacity__maskS2008;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3244 =
          _M0L5entryS600;
        _M0L3pslS598 = _M0L6_2atmpS2005;
        _M0L3idxS599 = _M0L6_2atmpS2006;
        _M0L5entryS600 = _tmp_3244;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS576,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS578,
  int32_t _M0L8new__idxS577
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2937;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1979;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1980;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2936;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2935;
  int32_t _M0L6_2acntS3120;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS579;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2937 = _M0L4selfS576->$0;
  _M0L7entriesS1979 = _M0L8_2afieldS2937;
  moonbit_incref(_M0L5entryS578);
  _M0L6_2atmpS1980 = _M0L5entryS578;
  if (
    _M0L8new__idxS577 < 0
    || _M0L8new__idxS577 >= Moonbit_array_length(_M0L7entriesS1979)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2936
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1979[
      _M0L8new__idxS577
    ];
  if (_M0L6_2aoldS2936) {
    moonbit_decref(_M0L6_2aoldS2936);
  }
  _M0L7entriesS1979[_M0L8new__idxS577] = _M0L6_2atmpS1980;
  _M0L8_2afieldS2935 = _M0L5entryS578->$1;
  _M0L6_2acntS3120 = Moonbit_object_header(_M0L5entryS578)->rc;
  if (_M0L6_2acntS3120 > 1) {
    int32_t _M0L11_2anew__cntS3123 = _M0L6_2acntS3120 - 1;
    Moonbit_object_header(_M0L5entryS578)->rc = _M0L11_2anew__cntS3123;
    if (_M0L8_2afieldS2935) {
      moonbit_incref(_M0L8_2afieldS2935);
    }
  } else if (_M0L6_2acntS3120 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3122 =
      _M0L5entryS578->$5;
    moonbit_string_t _M0L8_2afieldS3121;
    moonbit_decref(_M0L8_2afieldS3122);
    _M0L8_2afieldS3121 = _M0L5entryS578->$4;
    moonbit_decref(_M0L8_2afieldS3121);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS578);
  }
  _M0L7_2abindS579 = _M0L8_2afieldS2935;
  if (_M0L7_2abindS579 == 0) {
    if (_M0L7_2abindS579) {
      moonbit_decref(_M0L7_2abindS579);
    }
    _M0L4selfS576->$6 = _M0L8new__idxS577;
    moonbit_decref(_M0L4selfS576);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS580;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS581;
    moonbit_decref(_M0L4selfS576);
    _M0L7_2aSomeS580 = _M0L7_2abindS579;
    _M0L7_2anextS581 = _M0L7_2aSomeS580;
    _M0L7_2anextS581->$0 = _M0L8new__idxS577;
    moonbit_decref(_M0L7_2anextS581);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS582,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS584,
  int32_t _M0L8new__idxS583
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2940;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1981;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1982;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2939;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2938;
  int32_t _M0L6_2acntS3124;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS585;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2940 = _M0L4selfS582->$0;
  _M0L7entriesS1981 = _M0L8_2afieldS2940;
  moonbit_incref(_M0L5entryS584);
  _M0L6_2atmpS1982 = _M0L5entryS584;
  if (
    _M0L8new__idxS583 < 0
    || _M0L8new__idxS583 >= Moonbit_array_length(_M0L7entriesS1981)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2939
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1981[
      _M0L8new__idxS583
    ];
  if (_M0L6_2aoldS2939) {
    moonbit_decref(_M0L6_2aoldS2939);
  }
  _M0L7entriesS1981[_M0L8new__idxS583] = _M0L6_2atmpS1982;
  _M0L8_2afieldS2938 = _M0L5entryS584->$1;
  _M0L6_2acntS3124 = Moonbit_object_header(_M0L5entryS584)->rc;
  if (_M0L6_2acntS3124 > 1) {
    int32_t _M0L11_2anew__cntS3126 = _M0L6_2acntS3124 - 1;
    Moonbit_object_header(_M0L5entryS584)->rc = _M0L11_2anew__cntS3126;
    if (_M0L8_2afieldS2938) {
      moonbit_incref(_M0L8_2afieldS2938);
    }
  } else if (_M0L6_2acntS3124 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3125 =
      _M0L5entryS584->$5;
    moonbit_decref(_M0L8_2afieldS3125);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS584);
  }
  _M0L7_2abindS585 = _M0L8_2afieldS2938;
  if (_M0L7_2abindS585 == 0) {
    if (_M0L7_2abindS585) {
      moonbit_decref(_M0L7_2abindS585);
    }
    _M0L4selfS582->$6 = _M0L8new__idxS583;
    moonbit_decref(_M0L4selfS582);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS586;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS587;
    moonbit_decref(_M0L4selfS582);
    _M0L7_2aSomeS586 = _M0L7_2abindS585;
    _M0L7_2anextS587 = _M0L7_2aSomeS586;
    _M0L7_2anextS587->$0 = _M0L8new__idxS583;
    moonbit_decref(_M0L7_2anextS587);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS569,
  int32_t _M0L3idxS571,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS570
) {
  int32_t _M0L7_2abindS568;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2942;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1966;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1967;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2941;
  int32_t _M0L4sizeS1969;
  int32_t _M0L6_2atmpS1968;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS568 = _M0L4selfS569->$6;
  switch (_M0L7_2abindS568) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1961;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2943;
      moonbit_incref(_M0L5entryS570);
      _M0L6_2atmpS1961 = _M0L5entryS570;
      _M0L6_2aoldS2943 = _M0L4selfS569->$5;
      if (_M0L6_2aoldS2943) {
        moonbit_decref(_M0L6_2aoldS2943);
      }
      _M0L4selfS569->$5 = _M0L6_2atmpS1961;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2946 =
        _M0L4selfS569->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1965 =
        _M0L8_2afieldS2946;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2945;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1964;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1962;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1963;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2944;
      if (
        _M0L7_2abindS568 < 0
        || _M0L7_2abindS568 >= Moonbit_array_length(_M0L7entriesS1965)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2945
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1965[
          _M0L7_2abindS568
        ];
      _M0L6_2atmpS1964 = _M0L6_2atmpS2945;
      if (_M0L6_2atmpS1964) {
        moonbit_incref(_M0L6_2atmpS1964);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1962
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1964);
      moonbit_incref(_M0L5entryS570);
      _M0L6_2atmpS1963 = _M0L5entryS570;
      _M0L6_2aoldS2944 = _M0L6_2atmpS1962->$1;
      if (_M0L6_2aoldS2944) {
        moonbit_decref(_M0L6_2aoldS2944);
      }
      _M0L6_2atmpS1962->$1 = _M0L6_2atmpS1963;
      moonbit_decref(_M0L6_2atmpS1962);
      break;
    }
  }
  _M0L4selfS569->$6 = _M0L3idxS571;
  _M0L8_2afieldS2942 = _M0L4selfS569->$0;
  _M0L7entriesS1966 = _M0L8_2afieldS2942;
  _M0L6_2atmpS1967 = _M0L5entryS570;
  if (
    _M0L3idxS571 < 0
    || _M0L3idxS571 >= Moonbit_array_length(_M0L7entriesS1966)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2941
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1966[
      _M0L3idxS571
    ];
  if (_M0L6_2aoldS2941) {
    moonbit_decref(_M0L6_2aoldS2941);
  }
  _M0L7entriesS1966[_M0L3idxS571] = _M0L6_2atmpS1967;
  _M0L4sizeS1969 = _M0L4selfS569->$1;
  _M0L6_2atmpS1968 = _M0L4sizeS1969 + 1;
  _M0L4selfS569->$1 = _M0L6_2atmpS1968;
  moonbit_decref(_M0L4selfS569);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS573,
  int32_t _M0L3idxS575,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS574
) {
  int32_t _M0L7_2abindS572;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2948;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1975;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1976;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2947;
  int32_t _M0L4sizeS1978;
  int32_t _M0L6_2atmpS1977;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS572 = _M0L4selfS573->$6;
  switch (_M0L7_2abindS572) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1970;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2949;
      moonbit_incref(_M0L5entryS574);
      _M0L6_2atmpS1970 = _M0L5entryS574;
      _M0L6_2aoldS2949 = _M0L4selfS573->$5;
      if (_M0L6_2aoldS2949) {
        moonbit_decref(_M0L6_2aoldS2949);
      }
      _M0L4selfS573->$5 = _M0L6_2atmpS1970;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2952 =
        _M0L4selfS573->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1974 =
        _M0L8_2afieldS2952;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2951;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1973;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1971;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1972;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2950;
      if (
        _M0L7_2abindS572 < 0
        || _M0L7_2abindS572 >= Moonbit_array_length(_M0L7entriesS1974)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2951
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1974[
          _M0L7_2abindS572
        ];
      _M0L6_2atmpS1973 = _M0L6_2atmpS2951;
      if (_M0L6_2atmpS1973) {
        moonbit_incref(_M0L6_2atmpS1973);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1971
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1973);
      moonbit_incref(_M0L5entryS574);
      _M0L6_2atmpS1972 = _M0L5entryS574;
      _M0L6_2aoldS2950 = _M0L6_2atmpS1971->$1;
      if (_M0L6_2aoldS2950) {
        moonbit_decref(_M0L6_2aoldS2950);
      }
      _M0L6_2atmpS1971->$1 = _M0L6_2atmpS1972;
      moonbit_decref(_M0L6_2atmpS1971);
      break;
    }
  }
  _M0L4selfS573->$6 = _M0L3idxS575;
  _M0L8_2afieldS2948 = _M0L4selfS573->$0;
  _M0L7entriesS1975 = _M0L8_2afieldS2948;
  _M0L6_2atmpS1976 = _M0L5entryS574;
  if (
    _M0L3idxS575 < 0
    || _M0L3idxS575 >= Moonbit_array_length(_M0L7entriesS1975)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2947
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1975[
      _M0L3idxS575
    ];
  if (_M0L6_2aoldS2947) {
    moonbit_decref(_M0L6_2aoldS2947);
  }
  _M0L7entriesS1975[_M0L3idxS575] = _M0L6_2atmpS1976;
  _M0L4sizeS1978 = _M0L4selfS573->$1;
  _M0L6_2atmpS1977 = _M0L4sizeS1978 + 1;
  _M0L4selfS573->$1 = _M0L6_2atmpS1977;
  moonbit_decref(_M0L4selfS573);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS557
) {
  int32_t _M0L8capacityS556;
  int32_t _M0L7_2abindS558;
  int32_t _M0L7_2abindS559;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1959;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS560;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS561;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3245;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS556
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS557);
  _M0L7_2abindS558 = _M0L8capacityS556 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS559 = _M0FPB21calc__grow__threshold(_M0L8capacityS556);
  _M0L6_2atmpS1959 = 0;
  _M0L7_2abindS560
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS556, _M0L6_2atmpS1959);
  _M0L7_2abindS561 = 0;
  _block_3245
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3245)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3245->$0 = _M0L7_2abindS560;
  _block_3245->$1 = 0;
  _block_3245->$2 = _M0L8capacityS556;
  _block_3245->$3 = _M0L7_2abindS558;
  _block_3245->$4 = _M0L7_2abindS559;
  _block_3245->$5 = _M0L7_2abindS561;
  _block_3245->$6 = -1;
  return _block_3245;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS563
) {
  int32_t _M0L8capacityS562;
  int32_t _M0L7_2abindS564;
  int32_t _M0L7_2abindS565;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1960;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS566;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS567;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3246;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS562
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS563);
  _M0L7_2abindS564 = _M0L8capacityS562 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS565 = _M0FPB21calc__grow__threshold(_M0L8capacityS562);
  _M0L6_2atmpS1960 = 0;
  _M0L7_2abindS566
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS562, _M0L6_2atmpS1960);
  _M0L7_2abindS567 = 0;
  _block_3246
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3246)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3246->$0 = _M0L7_2abindS566;
  _block_3246->$1 = 0;
  _block_3246->$2 = _M0L8capacityS562;
  _block_3246->$3 = _M0L7_2abindS564;
  _block_3246->$4 = _M0L7_2abindS565;
  _block_3246->$5 = _M0L7_2abindS567;
  _block_3246->$6 = -1;
  return _block_3246;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS555) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS555 >= 0) {
    int32_t _M0L6_2atmpS1958;
    int32_t _M0L6_2atmpS1957;
    int32_t _M0L6_2atmpS1956;
    int32_t _M0L6_2atmpS1955;
    if (_M0L4selfS555 <= 1) {
      return 1;
    }
    if (_M0L4selfS555 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1958 = _M0L4selfS555 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1957 = moonbit_clz32(_M0L6_2atmpS1958);
    _M0L6_2atmpS1956 = _M0L6_2atmpS1957 - 1;
    _M0L6_2atmpS1955 = 2147483647 >> (_M0L6_2atmpS1956 & 31);
    return _M0L6_2atmpS1955 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS554) {
  int32_t _M0L6_2atmpS1954;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1954 = _M0L8capacityS554 * 13;
  return _M0L6_2atmpS1954 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS550
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS550 == 0) {
    if (_M0L4selfS550) {
      moonbit_decref(_M0L4selfS550);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS551 =
      _M0L4selfS550;
    return _M0L7_2aSomeS551;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS552
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS552 == 0) {
    if (_M0L4selfS552) {
      moonbit_decref(_M0L4selfS552);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS553 =
      _M0L4selfS552;
    return _M0L7_2aSomeS553;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS549
) {
  moonbit_string_t* _M0L6_2atmpS1953;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1953 = _M0L4selfS549;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1953);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS545,
  int32_t _M0L5indexS546
) {
  uint64_t* _M0L6_2atmpS1951;
  uint64_t _M0L6_2atmpS2953;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1951 = _M0L4selfS545;
  if (
    _M0L5indexS546 < 0
    || _M0L5indexS546 >= Moonbit_array_length(_M0L6_2atmpS1951)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2953 = (uint64_t)_M0L6_2atmpS1951[_M0L5indexS546];
  moonbit_decref(_M0L6_2atmpS1951);
  return _M0L6_2atmpS2953;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS547,
  int32_t _M0L5indexS548
) {
  uint32_t* _M0L6_2atmpS1952;
  uint32_t _M0L6_2atmpS2954;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1952 = _M0L4selfS547;
  if (
    _M0L5indexS548 < 0
    || _M0L5indexS548 >= Moonbit_array_length(_M0L6_2atmpS1952)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2954 = (uint32_t)_M0L6_2atmpS1952[_M0L5indexS548];
  moonbit_decref(_M0L6_2atmpS1952);
  return _M0L6_2atmpS2954;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS544
) {
  moonbit_string_t* _M0L6_2atmpS1949;
  int32_t _M0L6_2atmpS2955;
  int32_t _M0L6_2atmpS1950;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1948;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS544);
  _M0L6_2atmpS1949 = _M0L4selfS544;
  _M0L6_2atmpS2955 = Moonbit_array_length(_M0L4selfS544);
  moonbit_decref(_M0L4selfS544);
  _M0L6_2atmpS1950 = _M0L6_2atmpS2955;
  _M0L6_2atmpS1948
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1950, _M0L6_2atmpS1949
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1948);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS542
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS541;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__* _closure_3247;
  struct _M0TWEOs* _M0L6_2atmpS1936;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS541
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS541)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS541->$0 = 0;
  _closure_3247
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__));
  Moonbit_object_header(_closure_3247)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__, $0_0) >> 2, 2, 0);
  _closure_3247->code = &_M0MPC15array9ArrayView4iterGsEC1937l570;
  _closure_3247->$0_0 = _M0L4selfS542.$0;
  _closure_3247->$0_1 = _M0L4selfS542.$1;
  _closure_3247->$0_2 = _M0L4selfS542.$2;
  _closure_3247->$1 = _M0L1iS541;
  _M0L6_2atmpS1936 = (struct _M0TWEOs*)_closure_3247;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1936);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1937l570(
  struct _M0TWEOs* _M0L6_2aenvS1938
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__* _M0L14_2acasted__envS1939;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2960;
  struct _M0TPC13ref3RefGiE* _M0L1iS541;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2959;
  int32_t _M0L6_2acntS3127;
  struct _M0TPB9ArrayViewGsE _M0L4selfS542;
  int32_t _M0L3valS1940;
  int32_t _M0L6_2atmpS1941;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1939
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1937__l570__*)_M0L6_2aenvS1938;
  _M0L8_2afieldS2960 = _M0L14_2acasted__envS1939->$1;
  _M0L1iS541 = _M0L8_2afieldS2960;
  _M0L8_2afieldS2959
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1939->$0_1,
      _M0L14_2acasted__envS1939->$0_2,
      _M0L14_2acasted__envS1939->$0_0
  };
  _M0L6_2acntS3127 = Moonbit_object_header(_M0L14_2acasted__envS1939)->rc;
  if (_M0L6_2acntS3127 > 1) {
    int32_t _M0L11_2anew__cntS3128 = _M0L6_2acntS3127 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1939)->rc
    = _M0L11_2anew__cntS3128;
    moonbit_incref(_M0L1iS541);
    moonbit_incref(_M0L8_2afieldS2959.$0);
  } else if (_M0L6_2acntS3127 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1939);
  }
  _M0L4selfS542 = _M0L8_2afieldS2959;
  _M0L3valS1940 = _M0L1iS541->$0;
  moonbit_incref(_M0L4selfS542.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1941 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS542);
  if (_M0L3valS1940 < _M0L6_2atmpS1941) {
    moonbit_string_t* _M0L8_2afieldS2958 = _M0L4selfS542.$0;
    moonbit_string_t* _M0L3bufS1944 = _M0L8_2afieldS2958;
    int32_t _M0L8_2afieldS2957 = _M0L4selfS542.$1;
    int32_t _M0L5startS1946 = _M0L8_2afieldS2957;
    int32_t _M0L3valS1947 = _M0L1iS541->$0;
    int32_t _M0L6_2atmpS1945 = _M0L5startS1946 + _M0L3valS1947;
    moonbit_string_t _M0L6_2atmpS2956 =
      (moonbit_string_t)_M0L3bufS1944[_M0L6_2atmpS1945];
    moonbit_string_t _M0L4elemS543;
    int32_t _M0L3valS1943;
    int32_t _M0L6_2atmpS1942;
    moonbit_incref(_M0L6_2atmpS2956);
    moonbit_decref(_M0L3bufS1944);
    _M0L4elemS543 = _M0L6_2atmpS2956;
    _M0L3valS1943 = _M0L1iS541->$0;
    _M0L6_2atmpS1942 = _M0L3valS1943 + 1;
    _M0L1iS541->$0 = _M0L6_2atmpS1942;
    moonbit_decref(_M0L1iS541);
    return _M0L4elemS543;
  } else {
    moonbit_decref(_M0L4selfS542.$0);
    moonbit_decref(_M0L1iS541);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS540
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS540;
}

int32_t _M0IPC14byte4BytePB4Show6output(
  int32_t _M0L4selfS539,
  struct _M0TPB6Logger _M0L6loggerS538
) {
  moonbit_string_t _M0L6_2atmpS1935;
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 51 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1935 = _M0MPC14byte4Byte10to__string(_M0L4selfS539);
  #line 51 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS538.$0->$method_0(_M0L6loggerS538.$1, _M0L6_2atmpS1935);
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte10to__string(int32_t _M0L4selfS535) {
  int32_t _M0L1iS534;
  int32_t _M0L6_2atmpS1934;
  moonbit_string_t _M0L2hiS536;
  int32_t _M0L6_2atmpS1933;
  moonbit_string_t _M0L2loS537;
  moonbit_string_t _M0L6_2atmpS1932;
  moonbit_string_t _M0L6_2atmpS2963;
  moonbit_string_t _M0L6_2atmpS1930;
  moonbit_string_t _M0L6_2atmpS1931;
  moonbit_string_t _M0L6_2atmpS2962;
  moonbit_string_t _M0L6_2atmpS1929;
  moonbit_string_t _M0L6_2atmpS2961;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L1iS534 = (int32_t)_M0L4selfS535;
  _M0L6_2atmpS1934 = _M0L1iS534 / 16;
  #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L2hiS536 = _M0FPB8alphabet(_M0L6_2atmpS1934);
  _M0L6_2atmpS1933 = _M0L1iS534 % 16;
  #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L2loS537 = _M0FPB8alphabet(_M0L6_2atmpS1933);
  #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1932 = _M0IPC16string6StringPB4Show10to__string(_M0L2hiS536);
  #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2963
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_57.data, _M0L6_2atmpS1932);
  moonbit_decref(_M0L6_2atmpS1932);
  _M0L6_2atmpS1930 = _M0L6_2atmpS2963;
  #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1931 = _M0IPC16string6StringPB4Show10to__string(_M0L2loS537);
  #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2962 = moonbit_add_string(_M0L6_2atmpS1930, _M0L6_2atmpS1931);
  moonbit_decref(_M0L6_2atmpS1930);
  moonbit_decref(_M0L6_2atmpS1931);
  _M0L6_2atmpS1929 = _M0L6_2atmpS2962;
  #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2961
  = moonbit_add_string(_M0L6_2atmpS1929, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1929);
  return _M0L6_2atmpS2961;
}

moonbit_string_t _M0FPB8alphabet(int32_t _M0L1xS533) {
  #line 162 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  switch (_M0L1xS533) {
    case 0: {
      return (moonbit_string_t)moonbit_string_literal_50.data;
      break;
    }
    
    case 1: {
      return (moonbit_string_t)moonbit_string_literal_59.data;
      break;
    }
    
    case 2: {
      return (moonbit_string_t)moonbit_string_literal_60.data;
      break;
    }
    
    case 3: {
      return (moonbit_string_t)moonbit_string_literal_61.data;
      break;
    }
    
    case 4: {
      return (moonbit_string_t)moonbit_string_literal_62.data;
      break;
    }
    
    case 5: {
      return (moonbit_string_t)moonbit_string_literal_11.data;
      break;
    }
    
    case 6: {
      return (moonbit_string_t)moonbit_string_literal_63.data;
      break;
    }
    
    case 7: {
      return (moonbit_string_t)moonbit_string_literal_64.data;
      break;
    }
    
    case 8: {
      return (moonbit_string_t)moonbit_string_literal_65.data;
      break;
    }
    
    case 9: {
      return (moonbit_string_t)moonbit_string_literal_66.data;
      break;
    }
    
    case 10: {
      return (moonbit_string_t)moonbit_string_literal_67.data;
      break;
    }
    
    case 11: {
      return (moonbit_string_t)moonbit_string_literal_68.data;
      break;
    }
    
    case 12: {
      return (moonbit_string_t)moonbit_string_literal_69.data;
      break;
    }
    
    case 13: {
      return (moonbit_string_t)moonbit_string_literal_70.data;
      break;
    }
    
    case 14: {
      return (moonbit_string_t)moonbit_string_literal_71.data;
      break;
    }
    
    case 15: {
      return (moonbit_string_t)moonbit_string_literal_72.data;
      break;
    }
    default: {
      #line 180 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
      return _M0FPB5abortGsE((moonbit_string_t)moonbit_string_literal_73.data, (moonbit_string_t)moonbit_string_literal_74.data);
      break;
    }
  }
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS532,
  struct _M0TPB6Logger _M0L6loggerS531
) {
  moonbit_string_t _M0L6_2atmpS1928;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1928
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS532, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS531.$0->$method_0(_M0L6loggerS531.$1, _M0L6_2atmpS1928);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS530,
  struct _M0TPB6Logger _M0L6loggerS529
) {
  moonbit_string_t _M0L6_2atmpS1927;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1927 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS530, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS529.$0->$method_0(_M0L6loggerS529.$1, _M0L6_2atmpS1927);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS524) {
  int32_t _M0L3lenS523;
  struct _M0TPC13ref3RefGiE* _M0L5indexS525;
  struct _M0R38String_3a_3aiter_2eanon__u1911__l247__* _closure_3248;
  struct _M0TWEOc* _M0L6_2atmpS1910;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS523 = Moonbit_array_length(_M0L4selfS524);
  _M0L5indexS525
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS525)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS525->$0 = 0;
  _closure_3248
  = (struct _M0R38String_3a_3aiter_2eanon__u1911__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1911__l247__));
  Moonbit_object_header(_closure_3248)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1911__l247__, $0) >> 2, 2, 0);
  _closure_3248->code = &_M0MPC16string6String4iterC1911l247;
  _closure_3248->$0 = _M0L5indexS525;
  _closure_3248->$1 = _M0L4selfS524;
  _closure_3248->$2 = _M0L3lenS523;
  _M0L6_2atmpS1910 = (struct _M0TWEOc*)_closure_3248;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1910);
}

int32_t _M0MPC16string6String4iterC1911l247(
  struct _M0TWEOc* _M0L6_2aenvS1912
) {
  struct _M0R38String_3a_3aiter_2eanon__u1911__l247__* _M0L14_2acasted__envS1913;
  int32_t _M0L3lenS523;
  moonbit_string_t _M0L8_2afieldS2966;
  moonbit_string_t _M0L4selfS524;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2965;
  int32_t _M0L6_2acntS3129;
  struct _M0TPC13ref3RefGiE* _M0L5indexS525;
  int32_t _M0L3valS1914;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1913
  = (struct _M0R38String_3a_3aiter_2eanon__u1911__l247__*)_M0L6_2aenvS1912;
  _M0L3lenS523 = _M0L14_2acasted__envS1913->$2;
  _M0L8_2afieldS2966 = _M0L14_2acasted__envS1913->$1;
  _M0L4selfS524 = _M0L8_2afieldS2966;
  _M0L8_2afieldS2965 = _M0L14_2acasted__envS1913->$0;
  _M0L6_2acntS3129 = Moonbit_object_header(_M0L14_2acasted__envS1913)->rc;
  if (_M0L6_2acntS3129 > 1) {
    int32_t _M0L11_2anew__cntS3130 = _M0L6_2acntS3129 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1913)->rc
    = _M0L11_2anew__cntS3130;
    moonbit_incref(_M0L4selfS524);
    moonbit_incref(_M0L8_2afieldS2965);
  } else if (_M0L6_2acntS3129 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1913);
  }
  _M0L5indexS525 = _M0L8_2afieldS2965;
  _M0L3valS1914 = _M0L5indexS525->$0;
  if (_M0L3valS1914 < _M0L3lenS523) {
    int32_t _M0L3valS1926 = _M0L5indexS525->$0;
    int32_t _M0L2c1S526 = _M0L4selfS524[_M0L3valS1926];
    int32_t _if__result_3249;
    int32_t _M0L3valS1924;
    int32_t _M0L6_2atmpS1923;
    int32_t _M0L6_2atmpS1925;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S526)) {
      int32_t _M0L3valS1916 = _M0L5indexS525->$0;
      int32_t _M0L6_2atmpS1915 = _M0L3valS1916 + 1;
      _if__result_3249 = _M0L6_2atmpS1915 < _M0L3lenS523;
    } else {
      _if__result_3249 = 0;
    }
    if (_if__result_3249) {
      int32_t _M0L3valS1922 = _M0L5indexS525->$0;
      int32_t _M0L6_2atmpS1921 = _M0L3valS1922 + 1;
      int32_t _M0L6_2atmpS2964 = _M0L4selfS524[_M0L6_2atmpS1921];
      int32_t _M0L2c2S527;
      moonbit_decref(_M0L4selfS524);
      _M0L2c2S527 = _M0L6_2atmpS2964;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S527)) {
        int32_t _M0L6_2atmpS1919 = (int32_t)_M0L2c1S526;
        int32_t _M0L6_2atmpS1920 = (int32_t)_M0L2c2S527;
        int32_t _M0L1cS528;
        int32_t _M0L3valS1918;
        int32_t _M0L6_2atmpS1917;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS528
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1919, _M0L6_2atmpS1920);
        _M0L3valS1918 = _M0L5indexS525->$0;
        _M0L6_2atmpS1917 = _M0L3valS1918 + 2;
        _M0L5indexS525->$0 = _M0L6_2atmpS1917;
        moonbit_decref(_M0L5indexS525);
        return _M0L1cS528;
      }
    } else {
      moonbit_decref(_M0L4selfS524);
    }
    _M0L3valS1924 = _M0L5indexS525->$0;
    _M0L6_2atmpS1923 = _M0L3valS1924 + 1;
    _M0L5indexS525->$0 = _M0L6_2atmpS1923;
    moonbit_decref(_M0L5indexS525);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS1925 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S526);
    return _M0L6_2atmpS1925;
  } else {
    moonbit_decref(_M0L5indexS525);
    moonbit_decref(_M0L4selfS524);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS514,
  moonbit_string_t _M0L5valueS516
) {
  int32_t _M0L3lenS1895;
  moonbit_string_t* _M0L6_2atmpS1897;
  int32_t _M0L6_2atmpS2969;
  int32_t _M0L6_2atmpS1896;
  int32_t _M0L6lengthS515;
  moonbit_string_t* _M0L8_2afieldS2968;
  moonbit_string_t* _M0L3bufS1898;
  moonbit_string_t _M0L6_2aoldS2967;
  int32_t _M0L6_2atmpS1899;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1895 = _M0L4selfS514->$1;
  moonbit_incref(_M0L4selfS514);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1897 = _M0MPC15array5Array6bufferGsE(_M0L4selfS514);
  _M0L6_2atmpS2969 = Moonbit_array_length(_M0L6_2atmpS1897);
  moonbit_decref(_M0L6_2atmpS1897);
  _M0L6_2atmpS1896 = _M0L6_2atmpS2969;
  if (_M0L3lenS1895 == _M0L6_2atmpS1896) {
    moonbit_incref(_M0L4selfS514);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS514);
  }
  _M0L6lengthS515 = _M0L4selfS514->$1;
  _M0L8_2afieldS2968 = _M0L4selfS514->$0;
  _M0L3bufS1898 = _M0L8_2afieldS2968;
  _M0L6_2aoldS2967 = (moonbit_string_t)_M0L3bufS1898[_M0L6lengthS515];
  moonbit_decref(_M0L6_2aoldS2967);
  _M0L3bufS1898[_M0L6lengthS515] = _M0L5valueS516;
  _M0L6_2atmpS1899 = _M0L6lengthS515 + 1;
  _M0L4selfS514->$1 = _M0L6_2atmpS1899;
  moonbit_decref(_M0L4selfS514);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS517,
  struct _M0TUsiE* _M0L5valueS519
) {
  int32_t _M0L3lenS1900;
  struct _M0TUsiE** _M0L6_2atmpS1902;
  int32_t _M0L6_2atmpS2972;
  int32_t _M0L6_2atmpS1901;
  int32_t _M0L6lengthS518;
  struct _M0TUsiE** _M0L8_2afieldS2971;
  struct _M0TUsiE** _M0L3bufS1903;
  struct _M0TUsiE* _M0L6_2aoldS2970;
  int32_t _M0L6_2atmpS1904;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1900 = _M0L4selfS517->$1;
  moonbit_incref(_M0L4selfS517);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1902 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS517);
  _M0L6_2atmpS2972 = Moonbit_array_length(_M0L6_2atmpS1902);
  moonbit_decref(_M0L6_2atmpS1902);
  _M0L6_2atmpS1901 = _M0L6_2atmpS2972;
  if (_M0L3lenS1900 == _M0L6_2atmpS1901) {
    moonbit_incref(_M0L4selfS517);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS517);
  }
  _M0L6lengthS518 = _M0L4selfS517->$1;
  _M0L8_2afieldS2971 = _M0L4selfS517->$0;
  _M0L3bufS1903 = _M0L8_2afieldS2971;
  _M0L6_2aoldS2970 = (struct _M0TUsiE*)_M0L3bufS1903[_M0L6lengthS518];
  if (_M0L6_2aoldS2970) {
    moonbit_decref(_M0L6_2aoldS2970);
  }
  _M0L3bufS1903[_M0L6lengthS518] = _M0L5valueS519;
  _M0L6_2atmpS1904 = _M0L6lengthS518 + 1;
  _M0L4selfS517->$1 = _M0L6_2atmpS1904;
  moonbit_decref(_M0L4selfS517);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS520,
  void* _M0L5valueS522
) {
  int32_t _M0L3lenS1905;
  void** _M0L6_2atmpS1907;
  int32_t _M0L6_2atmpS2975;
  int32_t _M0L6_2atmpS1906;
  int32_t _M0L6lengthS521;
  void** _M0L8_2afieldS2974;
  void** _M0L3bufS1908;
  void* _M0L6_2aoldS2973;
  int32_t _M0L6_2atmpS1909;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1905 = _M0L4selfS520->$1;
  moonbit_incref(_M0L4selfS520);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1907
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS520);
  _M0L6_2atmpS2975 = Moonbit_array_length(_M0L6_2atmpS1907);
  moonbit_decref(_M0L6_2atmpS1907);
  _M0L6_2atmpS1906 = _M0L6_2atmpS2975;
  if (_M0L3lenS1905 == _M0L6_2atmpS1906) {
    moonbit_incref(_M0L4selfS520);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS520);
  }
  _M0L6lengthS521 = _M0L4selfS520->$1;
  _M0L8_2afieldS2974 = _M0L4selfS520->$0;
  _M0L3bufS1908 = _M0L8_2afieldS2974;
  _M0L6_2aoldS2973 = (void*)_M0L3bufS1908[_M0L6lengthS521];
  moonbit_decref(_M0L6_2aoldS2973);
  _M0L3bufS1908[_M0L6lengthS521] = _M0L5valueS522;
  _M0L6_2atmpS1909 = _M0L6lengthS521 + 1;
  _M0L4selfS520->$1 = _M0L6_2atmpS1909;
  moonbit_decref(_M0L4selfS520);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS506) {
  int32_t _M0L8old__capS505;
  int32_t _M0L8new__capS507;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS505 = _M0L4selfS506->$1;
  if (_M0L8old__capS505 == 0) {
    _M0L8new__capS507 = 8;
  } else {
    _M0L8new__capS507 = _M0L8old__capS505 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS506, _M0L8new__capS507);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS509
) {
  int32_t _M0L8old__capS508;
  int32_t _M0L8new__capS510;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS508 = _M0L4selfS509->$1;
  if (_M0L8old__capS508 == 0) {
    _M0L8new__capS510 = 8;
  } else {
    _M0L8new__capS510 = _M0L8old__capS508 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS509, _M0L8new__capS510);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS512
) {
  int32_t _M0L8old__capS511;
  int32_t _M0L8new__capS513;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS511 = _M0L4selfS512->$1;
  if (_M0L8old__capS511 == 0) {
    _M0L8new__capS513 = 8;
  } else {
    _M0L8new__capS513 = _M0L8old__capS511 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS512, _M0L8new__capS513);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS490,
  int32_t _M0L13new__capacityS488
) {
  moonbit_string_t* _M0L8new__bufS487;
  moonbit_string_t* _M0L8_2afieldS2977;
  moonbit_string_t* _M0L8old__bufS489;
  int32_t _M0L8old__capS491;
  int32_t _M0L9copy__lenS492;
  moonbit_string_t* _M0L6_2aoldS2976;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS487
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS488, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2977 = _M0L4selfS490->$0;
  _M0L8old__bufS489 = _M0L8_2afieldS2977;
  _M0L8old__capS491 = Moonbit_array_length(_M0L8old__bufS489);
  if (_M0L8old__capS491 < _M0L13new__capacityS488) {
    _M0L9copy__lenS492 = _M0L8old__capS491;
  } else {
    _M0L9copy__lenS492 = _M0L13new__capacityS488;
  }
  moonbit_incref(_M0L8old__bufS489);
  moonbit_incref(_M0L8new__bufS487);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS487, 0, _M0L8old__bufS489, 0, _M0L9copy__lenS492);
  _M0L6_2aoldS2976 = _M0L4selfS490->$0;
  moonbit_decref(_M0L6_2aoldS2976);
  _M0L4selfS490->$0 = _M0L8new__bufS487;
  moonbit_decref(_M0L4selfS490);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS496,
  int32_t _M0L13new__capacityS494
) {
  struct _M0TUsiE** _M0L8new__bufS493;
  struct _M0TUsiE** _M0L8_2afieldS2979;
  struct _M0TUsiE** _M0L8old__bufS495;
  int32_t _M0L8old__capS497;
  int32_t _M0L9copy__lenS498;
  struct _M0TUsiE** _M0L6_2aoldS2978;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS493
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS494, 0);
  _M0L8_2afieldS2979 = _M0L4selfS496->$0;
  _M0L8old__bufS495 = _M0L8_2afieldS2979;
  _M0L8old__capS497 = Moonbit_array_length(_M0L8old__bufS495);
  if (_M0L8old__capS497 < _M0L13new__capacityS494) {
    _M0L9copy__lenS498 = _M0L8old__capS497;
  } else {
    _M0L9copy__lenS498 = _M0L13new__capacityS494;
  }
  moonbit_incref(_M0L8old__bufS495);
  moonbit_incref(_M0L8new__bufS493);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS493, 0, _M0L8old__bufS495, 0, _M0L9copy__lenS498);
  _M0L6_2aoldS2978 = _M0L4selfS496->$0;
  moonbit_decref(_M0L6_2aoldS2978);
  _M0L4selfS496->$0 = _M0L8new__bufS493;
  moonbit_decref(_M0L4selfS496);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS502,
  int32_t _M0L13new__capacityS500
) {
  void** _M0L8new__bufS499;
  void** _M0L8_2afieldS2981;
  void** _M0L8old__bufS501;
  int32_t _M0L8old__capS503;
  int32_t _M0L9copy__lenS504;
  void** _M0L6_2aoldS2980;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS499
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS500, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS2981 = _M0L4selfS502->$0;
  _M0L8old__bufS501 = _M0L8_2afieldS2981;
  _M0L8old__capS503 = Moonbit_array_length(_M0L8old__bufS501);
  if (_M0L8old__capS503 < _M0L13new__capacityS500) {
    _M0L9copy__lenS504 = _M0L8old__capS503;
  } else {
    _M0L9copy__lenS504 = _M0L13new__capacityS500;
  }
  moonbit_incref(_M0L8old__bufS501);
  moonbit_incref(_M0L8new__bufS499);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS499, 0, _M0L8old__bufS501, 0, _M0L9copy__lenS504);
  _M0L6_2aoldS2980 = _M0L4selfS502->$0;
  moonbit_decref(_M0L6_2aoldS2980);
  _M0L4selfS502->$0 = _M0L8new__bufS499;
  moonbit_decref(_M0L4selfS502);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS486
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS486 == 0) {
    moonbit_string_t* _M0L6_2atmpS1893 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3250 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3250)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3250->$0 = _M0L6_2atmpS1893;
    _block_3250->$1 = 0;
    return _block_3250;
  } else {
    moonbit_string_t* _M0L6_2atmpS1894 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS486, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3251 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3251)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3251->$0 = _M0L6_2atmpS1894;
    _block_3251->$1 = 0;
    return _block_3251;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS480,
  int32_t _M0L1nS479
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS479 <= 0) {
    moonbit_decref(_M0L4selfS480);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS479 == 1) {
    return _M0L4selfS480;
  } else {
    int32_t _M0L3lenS481 = Moonbit_array_length(_M0L4selfS480);
    int32_t _M0L6_2atmpS1892 = _M0L3lenS481 * _M0L1nS479;
    struct _M0TPB13StringBuilder* _M0L3bufS482;
    moonbit_string_t _M0L3strS483;
    int32_t _M0L2__S484;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS482 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1892);
    _M0L3strS483 = _M0L4selfS480;
    _M0L2__S484 = 0;
    while (1) {
      if (_M0L2__S484 < _M0L1nS479) {
        int32_t _M0L6_2atmpS1891;
        moonbit_incref(_M0L3strS483);
        moonbit_incref(_M0L3bufS482);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS482, _M0L3strS483);
        _M0L6_2atmpS1891 = _M0L2__S484 + 1;
        _M0L2__S484 = _M0L6_2atmpS1891;
        continue;
      } else {
        moonbit_decref(_M0L3strS483);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS482);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS477,
  struct _M0TPC16string10StringView _M0L3strS478
) {
  int32_t _M0L3lenS1879;
  int32_t _M0L6_2atmpS1881;
  int32_t _M0L6_2atmpS1880;
  int32_t _M0L6_2atmpS1878;
  moonbit_bytes_t _M0L8_2afieldS2982;
  moonbit_bytes_t _M0L4dataS1882;
  int32_t _M0L3lenS1883;
  moonbit_string_t _M0L6_2atmpS1884;
  int32_t _M0L6_2atmpS1885;
  int32_t _M0L6_2atmpS1886;
  int32_t _M0L3lenS1888;
  int32_t _M0L6_2atmpS1890;
  int32_t _M0L6_2atmpS1889;
  int32_t _M0L6_2atmpS1887;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1879 = _M0L4selfS477->$1;
  moonbit_incref(_M0L3strS478.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1881 = _M0MPC16string10StringView6length(_M0L3strS478);
  _M0L6_2atmpS1880 = _M0L6_2atmpS1881 * 2;
  _M0L6_2atmpS1878 = _M0L3lenS1879 + _M0L6_2atmpS1880;
  moonbit_incref(_M0L4selfS477);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS477, _M0L6_2atmpS1878);
  _M0L8_2afieldS2982 = _M0L4selfS477->$0;
  _M0L4dataS1882 = _M0L8_2afieldS2982;
  _M0L3lenS1883 = _M0L4selfS477->$1;
  moonbit_incref(_M0L4dataS1882);
  moonbit_incref(_M0L3strS478.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1884 = _M0MPC16string10StringView4data(_M0L3strS478);
  moonbit_incref(_M0L3strS478.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1885 = _M0MPC16string10StringView13start__offset(_M0L3strS478);
  moonbit_incref(_M0L3strS478.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1886 = _M0MPC16string10StringView6length(_M0L3strS478);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1882, _M0L3lenS1883, _M0L6_2atmpS1884, _M0L6_2atmpS1885, _M0L6_2atmpS1886);
  _M0L3lenS1888 = _M0L4selfS477->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1890 = _M0MPC16string10StringView6length(_M0L3strS478);
  _M0L6_2atmpS1889 = _M0L6_2atmpS1890 * 2;
  _M0L6_2atmpS1887 = _M0L3lenS1888 + _M0L6_2atmpS1889;
  _M0L4selfS477->$1 = _M0L6_2atmpS1887;
  moonbit_decref(_M0L4selfS477);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS469,
  int32_t _M0L3lenS472,
  int32_t _M0L13start__offsetS476,
  int64_t _M0L11end__offsetS467
) {
  int32_t _M0L11end__offsetS466;
  int32_t _M0L5indexS470;
  int32_t _M0L5countS471;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS467 == 4294967296ll) {
    _M0L11end__offsetS466 = Moonbit_array_length(_M0L4selfS469);
  } else {
    int64_t _M0L7_2aSomeS468 = _M0L11end__offsetS467;
    _M0L11end__offsetS466 = (int32_t)_M0L7_2aSomeS468;
  }
  _M0L5indexS470 = _M0L13start__offsetS476;
  _M0L5countS471 = 0;
  while (1) {
    int32_t _if__result_3254;
    if (_M0L5indexS470 < _M0L11end__offsetS466) {
      _if__result_3254 = _M0L5countS471 < _M0L3lenS472;
    } else {
      _if__result_3254 = 0;
    }
    if (_if__result_3254) {
      int32_t _M0L2c1S473 = _M0L4selfS469[_M0L5indexS470];
      int32_t _if__result_3255;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1877;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S473)) {
        int32_t _M0L6_2atmpS1872 = _M0L5indexS470 + 1;
        _if__result_3255 = _M0L6_2atmpS1872 < _M0L11end__offsetS466;
      } else {
        _if__result_3255 = 0;
      }
      if (_if__result_3255) {
        int32_t _M0L6_2atmpS1875 = _M0L5indexS470 + 1;
        int32_t _M0L2c2S474 = _M0L4selfS469[_M0L6_2atmpS1875];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S474)) {
          int32_t _M0L6_2atmpS1873 = _M0L5indexS470 + 2;
          int32_t _M0L6_2atmpS1874 = _M0L5countS471 + 1;
          _M0L5indexS470 = _M0L6_2atmpS1873;
          _M0L5countS471 = _M0L6_2atmpS1874;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_75.data, (moonbit_string_t)moonbit_string_literal_76.data);
        }
      }
      _M0L6_2atmpS1876 = _M0L5indexS470 + 1;
      _M0L6_2atmpS1877 = _M0L5countS471 + 1;
      _M0L5indexS470 = _M0L6_2atmpS1876;
      _M0L5countS471 = _M0L6_2atmpS1877;
      continue;
    } else {
      moonbit_decref(_M0L4selfS469);
      return _M0L5countS471 >= _M0L3lenS472;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS463
) {
  int32_t _M0L3endS1866;
  int32_t _M0L8_2afieldS2983;
  int32_t _M0L5startS1867;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1866 = _M0L4selfS463.$2;
  _M0L8_2afieldS2983 = _M0L4selfS463.$1;
  moonbit_decref(_M0L4selfS463.$0);
  _M0L5startS1867 = _M0L8_2afieldS2983;
  return _M0L3endS1866 - _M0L5startS1867;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS464
) {
  int32_t _M0L3endS1868;
  int32_t _M0L8_2afieldS2984;
  int32_t _M0L5startS1869;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1868 = _M0L4selfS464.$2;
  _M0L8_2afieldS2984 = _M0L4selfS464.$1;
  moonbit_decref(_M0L4selfS464.$0);
  _M0L5startS1869 = _M0L8_2afieldS2984;
  return _M0L3endS1868 - _M0L5startS1869;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS465
) {
  int32_t _M0L3endS1870;
  int32_t _M0L8_2afieldS2985;
  int32_t _M0L5startS1871;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1870 = _M0L4selfS465.$2;
  _M0L8_2afieldS2985 = _M0L4selfS465.$1;
  moonbit_decref(_M0L4selfS465.$0);
  _M0L5startS1871 = _M0L8_2afieldS2985;
  return _M0L3endS1870 - _M0L5startS1871;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS461,
  int64_t _M0L19start__offset_2eoptS459,
  int64_t _M0L11end__offsetS462
) {
  int32_t _M0L13start__offsetS458;
  if (_M0L19start__offset_2eoptS459 == 4294967296ll) {
    _M0L13start__offsetS458 = 0;
  } else {
    int64_t _M0L7_2aSomeS460 = _M0L19start__offset_2eoptS459;
    _M0L13start__offsetS458 = (int32_t)_M0L7_2aSomeS460;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS461, _M0L13start__offsetS458, _M0L11end__offsetS462);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS456,
  int32_t _M0L13start__offsetS457,
  int64_t _M0L11end__offsetS454
) {
  int32_t _M0L11end__offsetS453;
  int32_t _if__result_3256;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS454 == 4294967296ll) {
    _M0L11end__offsetS453 = Moonbit_array_length(_M0L4selfS456);
  } else {
    int64_t _M0L7_2aSomeS455 = _M0L11end__offsetS454;
    _M0L11end__offsetS453 = (int32_t)_M0L7_2aSomeS455;
  }
  if (_M0L13start__offsetS457 >= 0) {
    if (_M0L13start__offsetS457 <= _M0L11end__offsetS453) {
      int32_t _M0L6_2atmpS1865 = Moonbit_array_length(_M0L4selfS456);
      _if__result_3256 = _M0L11end__offsetS453 <= _M0L6_2atmpS1865;
    } else {
      _if__result_3256 = 0;
    }
  } else {
    _if__result_3256 = 0;
  }
  if (_if__result_3256) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS457,
                                                 _M0L11end__offsetS453,
                                                 _M0L4selfS456};
  } else {
    moonbit_decref(_M0L4selfS456);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_77.data, (moonbit_string_t)moonbit_string_literal_78.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS452
) {
  moonbit_string_t _M0L8_2afieldS2987;
  moonbit_string_t _M0L3strS1862;
  int32_t _M0L5startS1863;
  int32_t _M0L8_2afieldS2986;
  int32_t _M0L3endS1864;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2987 = _M0L4selfS452.$0;
  _M0L3strS1862 = _M0L8_2afieldS2987;
  _M0L5startS1863 = _M0L4selfS452.$1;
  _M0L8_2afieldS2986 = _M0L4selfS452.$2;
  _M0L3endS1864 = _M0L8_2afieldS2986;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1862, _M0L5startS1863, _M0L3endS1864);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS450,
  struct _M0TPB6Logger _M0L6loggerS451
) {
  moonbit_string_t _M0L8_2afieldS2989;
  moonbit_string_t _M0L3strS1859;
  int32_t _M0L5startS1860;
  int32_t _M0L8_2afieldS2988;
  int32_t _M0L3endS1861;
  moonbit_string_t _M0L6substrS449;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2989 = _M0L4selfS450.$0;
  _M0L3strS1859 = _M0L8_2afieldS2989;
  _M0L5startS1860 = _M0L4selfS450.$1;
  _M0L8_2afieldS2988 = _M0L4selfS450.$2;
  _M0L3endS1861 = _M0L8_2afieldS2988;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS449
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1859, _M0L5startS1860, _M0L3endS1861);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS449, _M0L6loggerS451);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS441,
  struct _M0TPB6Logger _M0L6loggerS439
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS440;
  int32_t _M0L3lenS442;
  int32_t _M0L1iS443;
  int32_t _M0L3segS444;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS439.$1) {
    moonbit_incref(_M0L6loggerS439.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS439.$0->$method_3(_M0L6loggerS439.$1, 34);
  moonbit_incref(_M0L4selfS441);
  if (_M0L6loggerS439.$1) {
    moonbit_incref(_M0L6loggerS439.$1);
  }
  _M0L6_2aenvS440
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS440)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS440->$0 = _M0L4selfS441;
  _M0L6_2aenvS440->$1_0 = _M0L6loggerS439.$0;
  _M0L6_2aenvS440->$1_1 = _M0L6loggerS439.$1;
  _M0L3lenS442 = Moonbit_array_length(_M0L4selfS441);
  _M0L1iS443 = 0;
  _M0L3segS444 = 0;
  _2afor_445:;
  while (1) {
    int32_t _M0L4codeS446;
    int32_t _M0L1cS448;
    int32_t _M0L6_2atmpS1843;
    int32_t _M0L6_2atmpS1844;
    int32_t _M0L6_2atmpS1845;
    int32_t _tmp_3260;
    int32_t _tmp_3261;
    if (_M0L1iS443 >= _M0L3lenS442) {
      moonbit_decref(_M0L4selfS441);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
      break;
    }
    _M0L4codeS446 = _M0L4selfS441[_M0L1iS443];
    switch (_M0L4codeS446) {
      case 34: {
        _M0L1cS448 = _M0L4codeS446;
        goto join_447;
        break;
      }
      
      case 92: {
        _M0L1cS448 = _M0L4codeS446;
        goto join_447;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1846;
        int32_t _M0L6_2atmpS1847;
        moonbit_incref(_M0L6_2aenvS440);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
        if (_M0L6loggerS439.$1) {
          moonbit_incref(_M0L6loggerS439.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS439.$0->$method_0(_M0L6loggerS439.$1, (moonbit_string_t)moonbit_string_literal_44.data);
        _M0L6_2atmpS1846 = _M0L1iS443 + 1;
        _M0L6_2atmpS1847 = _M0L1iS443 + 1;
        _M0L1iS443 = _M0L6_2atmpS1846;
        _M0L3segS444 = _M0L6_2atmpS1847;
        goto _2afor_445;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1848;
        int32_t _M0L6_2atmpS1849;
        moonbit_incref(_M0L6_2aenvS440);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
        if (_M0L6loggerS439.$1) {
          moonbit_incref(_M0L6loggerS439.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS439.$0->$method_0(_M0L6loggerS439.$1, (moonbit_string_t)moonbit_string_literal_45.data);
        _M0L6_2atmpS1848 = _M0L1iS443 + 1;
        _M0L6_2atmpS1849 = _M0L1iS443 + 1;
        _M0L1iS443 = _M0L6_2atmpS1848;
        _M0L3segS444 = _M0L6_2atmpS1849;
        goto _2afor_445;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1850;
        int32_t _M0L6_2atmpS1851;
        moonbit_incref(_M0L6_2aenvS440);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
        if (_M0L6loggerS439.$1) {
          moonbit_incref(_M0L6loggerS439.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS439.$0->$method_0(_M0L6loggerS439.$1, (moonbit_string_t)moonbit_string_literal_46.data);
        _M0L6_2atmpS1850 = _M0L1iS443 + 1;
        _M0L6_2atmpS1851 = _M0L1iS443 + 1;
        _M0L1iS443 = _M0L6_2atmpS1850;
        _M0L3segS444 = _M0L6_2atmpS1851;
        goto _2afor_445;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1852;
        int32_t _M0L6_2atmpS1853;
        moonbit_incref(_M0L6_2aenvS440);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
        if (_M0L6loggerS439.$1) {
          moonbit_incref(_M0L6loggerS439.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS439.$0->$method_0(_M0L6loggerS439.$1, (moonbit_string_t)moonbit_string_literal_47.data);
        _M0L6_2atmpS1852 = _M0L1iS443 + 1;
        _M0L6_2atmpS1853 = _M0L1iS443 + 1;
        _M0L1iS443 = _M0L6_2atmpS1852;
        _M0L3segS444 = _M0L6_2atmpS1853;
        goto _2afor_445;
        break;
      }
      default: {
        if (_M0L4codeS446 < 32) {
          int32_t _M0L6_2atmpS1855;
          moonbit_string_t _M0L6_2atmpS1854;
          int32_t _M0L6_2atmpS1856;
          int32_t _M0L6_2atmpS1857;
          moonbit_incref(_M0L6_2aenvS440);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
          if (_M0L6loggerS439.$1) {
            moonbit_incref(_M0L6loggerS439.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS439.$0->$method_0(_M0L6loggerS439.$1, (moonbit_string_t)moonbit_string_literal_79.data);
          _M0L6_2atmpS1855 = _M0L4codeS446 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1854 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1855);
          if (_M0L6loggerS439.$1) {
            moonbit_incref(_M0L6loggerS439.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS439.$0->$method_0(_M0L6loggerS439.$1, _M0L6_2atmpS1854);
          if (_M0L6loggerS439.$1) {
            moonbit_incref(_M0L6loggerS439.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS439.$0->$method_3(_M0L6loggerS439.$1, 125);
          _M0L6_2atmpS1856 = _M0L1iS443 + 1;
          _M0L6_2atmpS1857 = _M0L1iS443 + 1;
          _M0L1iS443 = _M0L6_2atmpS1856;
          _M0L3segS444 = _M0L6_2atmpS1857;
          goto _2afor_445;
        } else {
          int32_t _M0L6_2atmpS1858 = _M0L1iS443 + 1;
          int32_t _tmp_3259 = _M0L3segS444;
          _M0L1iS443 = _M0L6_2atmpS1858;
          _M0L3segS444 = _tmp_3259;
          goto _2afor_445;
        }
        break;
      }
    }
    goto joinlet_3258;
    join_447:;
    moonbit_incref(_M0L6_2aenvS440);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS440, _M0L3segS444, _M0L1iS443);
    if (_M0L6loggerS439.$1) {
      moonbit_incref(_M0L6loggerS439.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS439.$0->$method_3(_M0L6loggerS439.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1843 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS448);
    if (_M0L6loggerS439.$1) {
      moonbit_incref(_M0L6loggerS439.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS439.$0->$method_3(_M0L6loggerS439.$1, _M0L6_2atmpS1843);
    _M0L6_2atmpS1844 = _M0L1iS443 + 1;
    _M0L6_2atmpS1845 = _M0L1iS443 + 1;
    _M0L1iS443 = _M0L6_2atmpS1844;
    _M0L3segS444 = _M0L6_2atmpS1845;
    continue;
    joinlet_3258:;
    _tmp_3260 = _M0L1iS443;
    _tmp_3261 = _M0L3segS444;
    _M0L1iS443 = _tmp_3260;
    _M0L3segS444 = _tmp_3261;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS439.$0->$method_3(_M0L6loggerS439.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS435,
  int32_t _M0L3segS438,
  int32_t _M0L1iS437
) {
  struct _M0TPB6Logger _M0L8_2afieldS2991;
  struct _M0TPB6Logger _M0L6loggerS434;
  moonbit_string_t _M0L8_2afieldS2990;
  int32_t _M0L6_2acntS3131;
  moonbit_string_t _M0L4selfS436;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS2991
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS435->$1_0, _M0L6_2aenvS435->$1_1
  };
  _M0L6loggerS434 = _M0L8_2afieldS2991;
  _M0L8_2afieldS2990 = _M0L6_2aenvS435->$0;
  _M0L6_2acntS3131 = Moonbit_object_header(_M0L6_2aenvS435)->rc;
  if (_M0L6_2acntS3131 > 1) {
    int32_t _M0L11_2anew__cntS3132 = _M0L6_2acntS3131 - 1;
    Moonbit_object_header(_M0L6_2aenvS435)->rc = _M0L11_2anew__cntS3132;
    if (_M0L6loggerS434.$1) {
      moonbit_incref(_M0L6loggerS434.$1);
    }
    moonbit_incref(_M0L8_2afieldS2990);
  } else if (_M0L6_2acntS3131 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS435);
  }
  _M0L4selfS436 = _M0L8_2afieldS2990;
  if (_M0L1iS437 > _M0L3segS438) {
    int32_t _M0L6_2atmpS1842 = _M0L1iS437 - _M0L3segS438;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS434.$0->$method_1(_M0L6loggerS434.$1, _M0L4selfS436, _M0L3segS438, _M0L6_2atmpS1842);
  } else {
    moonbit_decref(_M0L4selfS436);
    if (_M0L6loggerS434.$1) {
      moonbit_decref(_M0L6loggerS434.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS433) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS432;
  int32_t _M0L6_2atmpS1839;
  int32_t _M0L6_2atmpS1838;
  int32_t _M0L6_2atmpS1841;
  int32_t _M0L6_2atmpS1840;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1837;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS432 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1839 = _M0IPC14byte4BytePB3Div3div(_M0L1bS433, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1838
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1839);
  moonbit_incref(_M0L7_2aselfS432);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS432, _M0L6_2atmpS1838);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1841 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS433, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1840
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1841);
  moonbit_incref(_M0L7_2aselfS432);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS432, _M0L6_2atmpS1840);
  _M0L6_2atmpS1837 = _M0L7_2aselfS432;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1837);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS431) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS431 < 10) {
    int32_t _M0L6_2atmpS1834;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1834 = _M0IPC14byte4BytePB3Add3add(_M0L1iS431, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1834);
  } else {
    int32_t _M0L6_2atmpS1836;
    int32_t _M0L6_2atmpS1835;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1836 = _M0IPC14byte4BytePB3Add3add(_M0L1iS431, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1835 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1836, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1835);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS429,
  int32_t _M0L4thatS430
) {
  int32_t _M0L6_2atmpS1832;
  int32_t _M0L6_2atmpS1833;
  int32_t _M0L6_2atmpS1831;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1832 = (int32_t)_M0L4selfS429;
  _M0L6_2atmpS1833 = (int32_t)_M0L4thatS430;
  _M0L6_2atmpS1831 = _M0L6_2atmpS1832 - _M0L6_2atmpS1833;
  return _M0L6_2atmpS1831 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS427,
  int32_t _M0L4thatS428
) {
  int32_t _M0L6_2atmpS1829;
  int32_t _M0L6_2atmpS1830;
  int32_t _M0L6_2atmpS1828;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1829 = (int32_t)_M0L4selfS427;
  _M0L6_2atmpS1830 = (int32_t)_M0L4thatS428;
  _M0L6_2atmpS1828 = _M0L6_2atmpS1829 % _M0L6_2atmpS1830;
  return _M0L6_2atmpS1828 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS425,
  int32_t _M0L4thatS426
) {
  int32_t _M0L6_2atmpS1826;
  int32_t _M0L6_2atmpS1827;
  int32_t _M0L6_2atmpS1825;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1826 = (int32_t)_M0L4selfS425;
  _M0L6_2atmpS1827 = (int32_t)_M0L4thatS426;
  _M0L6_2atmpS1825 = _M0L6_2atmpS1826 / _M0L6_2atmpS1827;
  return _M0L6_2atmpS1825 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS423,
  int32_t _M0L4thatS424
) {
  int32_t _M0L6_2atmpS1823;
  int32_t _M0L6_2atmpS1824;
  int32_t _M0L6_2atmpS1822;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1823 = (int32_t)_M0L4selfS423;
  _M0L6_2atmpS1824 = (int32_t)_M0L4thatS424;
  _M0L6_2atmpS1822 = _M0L6_2atmpS1823 + _M0L6_2atmpS1824;
  return _M0L6_2atmpS1822 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS420,
  int32_t _M0L5startS418,
  int32_t _M0L3endS419
) {
  int32_t _if__result_3262;
  int32_t _M0L3lenS421;
  int32_t _M0L6_2atmpS1820;
  int32_t _M0L6_2atmpS1821;
  moonbit_bytes_t _M0L5bytesS422;
  moonbit_bytes_t _M0L6_2atmpS1819;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS418 == 0) {
    int32_t _M0L6_2atmpS1818 = Moonbit_array_length(_M0L3strS420);
    _if__result_3262 = _M0L3endS419 == _M0L6_2atmpS1818;
  } else {
    _if__result_3262 = 0;
  }
  if (_if__result_3262) {
    return _M0L3strS420;
  }
  _M0L3lenS421 = _M0L3endS419 - _M0L5startS418;
  _M0L6_2atmpS1820 = _M0L3lenS421 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1821 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS422
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1820, _M0L6_2atmpS1821);
  moonbit_incref(_M0L5bytesS422);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS422, 0, _M0L3strS420, _M0L5startS418, _M0L3lenS421);
  _M0L6_2atmpS1819 = _M0L5bytesS422;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1819, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS415) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS415;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS416
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS416;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS417) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS417;
}

struct moonbit_result_0 _M0FPB10assert__eqGiE(
  int32_t _M0L1aS403,
  int32_t _M0L1bS404,
  moonbit_string_t _M0L3msgS406,
  moonbit_string_t _M0L3locS408
) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (_M0L1aS403 != _M0L1bS404) {
    moonbit_string_t _M0L9fail__msgS405;
    if (_M0L3msgS406 == 0) {
      moonbit_string_t _M0L6_2atmpS1808;
      moonbit_string_t _M0L6_2atmpS1807;
      moonbit_string_t _M0L6_2atmpS2995;
      moonbit_string_t _M0L6_2atmpS1806;
      moonbit_string_t _M0L6_2atmpS2994;
      moonbit_string_t _M0L6_2atmpS1803;
      moonbit_string_t _M0L6_2atmpS1805;
      moonbit_string_t _M0L6_2atmpS1804;
      moonbit_string_t _M0L6_2atmpS2993;
      moonbit_string_t _M0L6_2atmpS1802;
      moonbit_string_t _M0L6_2atmpS2992;
      if (_M0L3msgS406) {
        moonbit_decref(_M0L3msgS406);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1808 = _M0FPB13debug__stringGiE(_M0L1aS403);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1807
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1808);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2995
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_80.data, _M0L6_2atmpS1807);
      moonbit_decref(_M0L6_2atmpS1807);
      _M0L6_2atmpS1806 = _M0L6_2atmpS2995;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2994
      = moonbit_add_string(_M0L6_2atmpS1806, (moonbit_string_t)moonbit_string_literal_81.data);
      moonbit_decref(_M0L6_2atmpS1806);
      _M0L6_2atmpS1803 = _M0L6_2atmpS2994;
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1805 = _M0FPB13debug__stringGiE(_M0L1bS404);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1804
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1805);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2993
      = moonbit_add_string(_M0L6_2atmpS1803, _M0L6_2atmpS1804);
      moonbit_decref(_M0L6_2atmpS1803);
      moonbit_decref(_M0L6_2atmpS1804);
      _M0L6_2atmpS1802 = _M0L6_2atmpS2993;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2992
      = moonbit_add_string(_M0L6_2atmpS1802, (moonbit_string_t)moonbit_string_literal_80.data);
      moonbit_decref(_M0L6_2atmpS1802);
      _M0L9fail__msgS405 = _M0L6_2atmpS2992;
    } else {
      moonbit_string_t _M0L7_2aSomeS407 = _M0L3msgS406;
      _M0L9fail__msgS405 = _M0L7_2aSomeS407;
    }
    #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS405, _M0L3locS408);
  } else {
    int32_t _M0L6_2atmpS1809;
    struct moonbit_result_0 _result_3263;
    moonbit_decref(_M0L3locS408);
    if (_M0L3msgS406) {
      moonbit_decref(_M0L3msgS406);
    }
    _M0L6_2atmpS1809 = 0;
    _result_3263.tag = 1;
    _result_3263.data.ok = _M0L6_2atmpS1809;
    return _result_3263;
  }
}

struct moonbit_result_0 _M0FPB10assert__eqGyE(
  int32_t _M0L1aS409,
  int32_t _M0L1bS410,
  moonbit_string_t _M0L3msgS412,
  moonbit_string_t _M0L3locS414
) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (_M0IPC14byte4BytePB2Eq10not__equal(_M0L1aS409, _M0L1bS410)) {
    moonbit_string_t _M0L9fail__msgS411;
    if (_M0L3msgS412 == 0) {
      moonbit_string_t _M0L6_2atmpS1816;
      moonbit_string_t _M0L6_2atmpS1815;
      moonbit_string_t _M0L6_2atmpS2999;
      moonbit_string_t _M0L6_2atmpS1814;
      moonbit_string_t _M0L6_2atmpS2998;
      moonbit_string_t _M0L6_2atmpS1811;
      moonbit_string_t _M0L6_2atmpS1813;
      moonbit_string_t _M0L6_2atmpS1812;
      moonbit_string_t _M0L6_2atmpS2997;
      moonbit_string_t _M0L6_2atmpS1810;
      moonbit_string_t _M0L6_2atmpS2996;
      if (_M0L3msgS412) {
        moonbit_decref(_M0L3msgS412);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1816 = _M0FPB13debug__stringGyE(_M0L1aS409);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1815
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1816);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2999
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_80.data, _M0L6_2atmpS1815);
      moonbit_decref(_M0L6_2atmpS1815);
      _M0L6_2atmpS1814 = _M0L6_2atmpS2999;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2998
      = moonbit_add_string(_M0L6_2atmpS1814, (moonbit_string_t)moonbit_string_literal_81.data);
      moonbit_decref(_M0L6_2atmpS1814);
      _M0L6_2atmpS1811 = _M0L6_2atmpS2998;
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1813 = _M0FPB13debug__stringGyE(_M0L1bS410);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1812
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1813);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2997
      = moonbit_add_string(_M0L6_2atmpS1811, _M0L6_2atmpS1812);
      moonbit_decref(_M0L6_2atmpS1811);
      moonbit_decref(_M0L6_2atmpS1812);
      _M0L6_2atmpS1810 = _M0L6_2atmpS2997;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2996
      = moonbit_add_string(_M0L6_2atmpS1810, (moonbit_string_t)moonbit_string_literal_80.data);
      moonbit_decref(_M0L6_2atmpS1810);
      _M0L9fail__msgS411 = _M0L6_2atmpS2996;
    } else {
      moonbit_string_t _M0L7_2aSomeS413 = _M0L3msgS412;
      _M0L9fail__msgS411 = _M0L7_2aSomeS413;
    }
    #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS411, _M0L3locS414);
  } else {
    int32_t _M0L6_2atmpS1817;
    struct moonbit_result_0 _result_3264;
    moonbit_decref(_M0L3locS414);
    if (_M0L3msgS412) {
      moonbit_decref(_M0L3msgS412);
    }
    _M0L6_2atmpS1817 = 0;
    _result_3264.tag = 1;
    _result_3264.data.ok = _M0L6_2atmpS1817;
    return _result_3264;
  }
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS402,
  moonbit_string_t _M0L3locS401
) {
  moonbit_string_t _M0L6_2atmpS1801;
  moonbit_string_t _M0L6_2atmpS3001;
  moonbit_string_t _M0L6_2atmpS1799;
  moonbit_string_t _M0L6_2atmpS1800;
  moonbit_string_t _M0L6_2atmpS3000;
  moonbit_string_t _M0L6_2atmpS1798;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1797;
  struct moonbit_result_0 _result_3265;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1801
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS401);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS3001
  = moonbit_add_string(_M0L6_2atmpS1801, (moonbit_string_t)moonbit_string_literal_82.data);
  moonbit_decref(_M0L6_2atmpS1801);
  _M0L6_2atmpS1799 = _M0L6_2atmpS3001;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1800 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS402);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS3000 = moonbit_add_string(_M0L6_2atmpS1799, _M0L6_2atmpS1800);
  moonbit_decref(_M0L6_2atmpS1799);
  moonbit_decref(_M0L6_2atmpS1800);
  _M0L6_2atmpS1798 = _M0L6_2atmpS3000;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1797
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1797)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1797)->$0
  = _M0L6_2atmpS1798;
  _result_3265.tag = 0;
  _result_3265.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1797;
  return _result_3265;
}

moonbit_string_t _M0FPB13debug__stringGiE(int32_t _M0L1tS398) {
  struct _M0TPB13StringBuilder* _M0L3bufS397;
  struct _M0TPB6Logger _M0L6_2atmpS1795;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0L3bufS397 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS397);
  _M0L6_2atmpS1795
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS397
  };
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L1tS398, _M0L6_2atmpS1795);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS397);
}

moonbit_string_t _M0FPB13debug__stringGyE(int32_t _M0L1tS400) {
  struct _M0TPB13StringBuilder* _M0L3bufS399;
  struct _M0TPB6Logger _M0L6_2atmpS1796;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0L3bufS399 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS399);
  _M0L6_2atmpS1796
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS399
  };
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0IPC14byte4BytePB4Show6output(_M0L1tS400, _M0L6_2atmpS1796);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS399);
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS389,
  int32_t _M0L5radixS388
) {
  int32_t _if__result_3266;
  uint16_t* _M0L6bufferS390;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS388 < 2) {
    _if__result_3266 = 1;
  } else {
    _if__result_3266 = _M0L5radixS388 > 36;
  }
  if (_if__result_3266) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_83.data, (moonbit_string_t)moonbit_string_literal_84.data);
  }
  if (_M0L4selfS389 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_50.data;
  }
  switch (_M0L5radixS388) {
    case 10: {
      int32_t _M0L3lenS391;
      uint16_t* _M0L6bufferS392;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS391 = _M0FPB12dec__count64(_M0L4selfS389);
      _M0L6bufferS392 = (uint16_t*)moonbit_make_string(_M0L3lenS391, 0);
      moonbit_incref(_M0L6bufferS392);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS392, _M0L4selfS389, 0, _M0L3lenS391);
      _M0L6bufferS390 = _M0L6bufferS392;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS393;
      uint16_t* _M0L6bufferS394;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS393 = _M0FPB12hex__count64(_M0L4selfS389);
      _M0L6bufferS394 = (uint16_t*)moonbit_make_string(_M0L3lenS393, 0);
      moonbit_incref(_M0L6bufferS394);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS394, _M0L4selfS389, 0, _M0L3lenS393);
      _M0L6bufferS390 = _M0L6bufferS394;
      break;
    }
    default: {
      int32_t _M0L3lenS395;
      uint16_t* _M0L6bufferS396;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS395 = _M0FPB14radix__count64(_M0L4selfS389, _M0L5radixS388);
      _M0L6bufferS396 = (uint16_t*)moonbit_make_string(_M0L3lenS395, 0);
      moonbit_incref(_M0L6bufferS396);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS396, _M0L4selfS389, 0, _M0L3lenS395, _M0L5radixS388);
      _M0L6bufferS390 = _M0L6bufferS396;
      break;
    }
  }
  return _M0L6bufferS390;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS378,
  uint64_t _M0L3numS366,
  int32_t _M0L12digit__startS369,
  int32_t _M0L10total__lenS368
) {
  uint64_t _M0Lm3numS365;
  int32_t _M0Lm6offsetS367;
  uint64_t _M0L6_2atmpS1794;
  int32_t _M0Lm9remainingS380;
  int32_t _M0L6_2atmpS1775;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS365 = _M0L3numS366;
  _M0Lm6offsetS367 = _M0L10total__lenS368 - _M0L12digit__startS369;
  while (1) {
    uint64_t _M0L6_2atmpS1738 = _M0Lm3numS365;
    if (_M0L6_2atmpS1738 >= 10000ull) {
      uint64_t _M0L6_2atmpS1761 = _M0Lm3numS365;
      uint64_t _M0L1tS370 = _M0L6_2atmpS1761 / 10000ull;
      uint64_t _M0L6_2atmpS1760 = _M0Lm3numS365;
      uint64_t _M0L6_2atmpS1759 = _M0L6_2atmpS1760 % 10000ull;
      int32_t _M0L1rS371 = (int32_t)_M0L6_2atmpS1759;
      int32_t _M0L2d1S372;
      int32_t _M0L2d2S373;
      int32_t _M0L6_2atmpS1739;
      int32_t _M0L6_2atmpS1758;
      int32_t _M0L6_2atmpS1757;
      int32_t _M0L6d1__hiS374;
      int32_t _M0L6_2atmpS1756;
      int32_t _M0L6_2atmpS1755;
      int32_t _M0L6d1__loS375;
      int32_t _M0L6_2atmpS1754;
      int32_t _M0L6_2atmpS1753;
      int32_t _M0L6d2__hiS376;
      int32_t _M0L6_2atmpS1752;
      int32_t _M0L6_2atmpS1751;
      int32_t _M0L6d2__loS377;
      int32_t _M0L6_2atmpS1741;
      int32_t _M0L6_2atmpS1740;
      int32_t _M0L6_2atmpS1744;
      int32_t _M0L6_2atmpS1743;
      int32_t _M0L6_2atmpS1742;
      int32_t _M0L6_2atmpS1747;
      int32_t _M0L6_2atmpS1746;
      int32_t _M0L6_2atmpS1745;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L6_2atmpS1749;
      int32_t _M0L6_2atmpS1748;
      _M0Lm3numS365 = _M0L1tS370;
      _M0L2d1S372 = _M0L1rS371 / 100;
      _M0L2d2S373 = _M0L1rS371 % 100;
      _M0L6_2atmpS1739 = _M0Lm6offsetS367;
      _M0Lm6offsetS367 = _M0L6_2atmpS1739 - 4;
      _M0L6_2atmpS1758 = _M0L2d1S372 / 10;
      _M0L6_2atmpS1757 = 48 + _M0L6_2atmpS1758;
      _M0L6d1__hiS374 = (uint16_t)_M0L6_2atmpS1757;
      _M0L6_2atmpS1756 = _M0L2d1S372 % 10;
      _M0L6_2atmpS1755 = 48 + _M0L6_2atmpS1756;
      _M0L6d1__loS375 = (uint16_t)_M0L6_2atmpS1755;
      _M0L6_2atmpS1754 = _M0L2d2S373 / 10;
      _M0L6_2atmpS1753 = 48 + _M0L6_2atmpS1754;
      _M0L6d2__hiS376 = (uint16_t)_M0L6_2atmpS1753;
      _M0L6_2atmpS1752 = _M0L2d2S373 % 10;
      _M0L6_2atmpS1751 = 48 + _M0L6_2atmpS1752;
      _M0L6d2__loS377 = (uint16_t)_M0L6_2atmpS1751;
      _M0L6_2atmpS1741 = _M0Lm6offsetS367;
      _M0L6_2atmpS1740 = _M0L12digit__startS369 + _M0L6_2atmpS1741;
      _M0L6bufferS378[_M0L6_2atmpS1740] = _M0L6d1__hiS374;
      _M0L6_2atmpS1744 = _M0Lm6offsetS367;
      _M0L6_2atmpS1743 = _M0L12digit__startS369 + _M0L6_2atmpS1744;
      _M0L6_2atmpS1742 = _M0L6_2atmpS1743 + 1;
      _M0L6bufferS378[_M0L6_2atmpS1742] = _M0L6d1__loS375;
      _M0L6_2atmpS1747 = _M0Lm6offsetS367;
      _M0L6_2atmpS1746 = _M0L12digit__startS369 + _M0L6_2atmpS1747;
      _M0L6_2atmpS1745 = _M0L6_2atmpS1746 + 2;
      _M0L6bufferS378[_M0L6_2atmpS1745] = _M0L6d2__hiS376;
      _M0L6_2atmpS1750 = _M0Lm6offsetS367;
      _M0L6_2atmpS1749 = _M0L12digit__startS369 + _M0L6_2atmpS1750;
      _M0L6_2atmpS1748 = _M0L6_2atmpS1749 + 3;
      _M0L6bufferS378[_M0L6_2atmpS1748] = _M0L6d2__loS377;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1794 = _M0Lm3numS365;
  _M0Lm9remainingS380 = (int32_t)_M0L6_2atmpS1794;
  while (1) {
    int32_t _M0L6_2atmpS1762 = _M0Lm9remainingS380;
    if (_M0L6_2atmpS1762 >= 100) {
      int32_t _M0L6_2atmpS1774 = _M0Lm9remainingS380;
      int32_t _M0L1tS381 = _M0L6_2atmpS1774 / 100;
      int32_t _M0L6_2atmpS1773 = _M0Lm9remainingS380;
      int32_t _M0L1dS382 = _M0L6_2atmpS1773 % 100;
      int32_t _M0L6_2atmpS1763;
      int32_t _M0L6_2atmpS1772;
      int32_t _M0L6_2atmpS1771;
      int32_t _M0L5d__hiS383;
      int32_t _M0L6_2atmpS1770;
      int32_t _M0L6_2atmpS1769;
      int32_t _M0L5d__loS384;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6_2atmpS1768;
      int32_t _M0L6_2atmpS1767;
      int32_t _M0L6_2atmpS1766;
      _M0Lm9remainingS380 = _M0L1tS381;
      _M0L6_2atmpS1763 = _M0Lm6offsetS367;
      _M0Lm6offsetS367 = _M0L6_2atmpS1763 - 2;
      _M0L6_2atmpS1772 = _M0L1dS382 / 10;
      _M0L6_2atmpS1771 = 48 + _M0L6_2atmpS1772;
      _M0L5d__hiS383 = (uint16_t)_M0L6_2atmpS1771;
      _M0L6_2atmpS1770 = _M0L1dS382 % 10;
      _M0L6_2atmpS1769 = 48 + _M0L6_2atmpS1770;
      _M0L5d__loS384 = (uint16_t)_M0L6_2atmpS1769;
      _M0L6_2atmpS1765 = _M0Lm6offsetS367;
      _M0L6_2atmpS1764 = _M0L12digit__startS369 + _M0L6_2atmpS1765;
      _M0L6bufferS378[_M0L6_2atmpS1764] = _M0L5d__hiS383;
      _M0L6_2atmpS1768 = _M0Lm6offsetS367;
      _M0L6_2atmpS1767 = _M0L12digit__startS369 + _M0L6_2atmpS1768;
      _M0L6_2atmpS1766 = _M0L6_2atmpS1767 + 1;
      _M0L6bufferS378[_M0L6_2atmpS1766] = _M0L5d__loS384;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1775 = _M0Lm9remainingS380;
  if (_M0L6_2atmpS1775 >= 10) {
    int32_t _M0L6_2atmpS1776 = _M0Lm6offsetS367;
    int32_t _M0L6_2atmpS1787;
    int32_t _M0L6_2atmpS1786;
    int32_t _M0L6_2atmpS1785;
    int32_t _M0L5d__hiS386;
    int32_t _M0L6_2atmpS1784;
    int32_t _M0L6_2atmpS1783;
    int32_t _M0L6_2atmpS1782;
    int32_t _M0L5d__loS387;
    int32_t _M0L6_2atmpS1778;
    int32_t _M0L6_2atmpS1777;
    int32_t _M0L6_2atmpS1781;
    int32_t _M0L6_2atmpS1780;
    int32_t _M0L6_2atmpS1779;
    _M0Lm6offsetS367 = _M0L6_2atmpS1776 - 2;
    _M0L6_2atmpS1787 = _M0Lm9remainingS380;
    _M0L6_2atmpS1786 = _M0L6_2atmpS1787 / 10;
    _M0L6_2atmpS1785 = 48 + _M0L6_2atmpS1786;
    _M0L5d__hiS386 = (uint16_t)_M0L6_2atmpS1785;
    _M0L6_2atmpS1784 = _M0Lm9remainingS380;
    _M0L6_2atmpS1783 = _M0L6_2atmpS1784 % 10;
    _M0L6_2atmpS1782 = 48 + _M0L6_2atmpS1783;
    _M0L5d__loS387 = (uint16_t)_M0L6_2atmpS1782;
    _M0L6_2atmpS1778 = _M0Lm6offsetS367;
    _M0L6_2atmpS1777 = _M0L12digit__startS369 + _M0L6_2atmpS1778;
    _M0L6bufferS378[_M0L6_2atmpS1777] = _M0L5d__hiS386;
    _M0L6_2atmpS1781 = _M0Lm6offsetS367;
    _M0L6_2atmpS1780 = _M0L12digit__startS369 + _M0L6_2atmpS1781;
    _M0L6_2atmpS1779 = _M0L6_2atmpS1780 + 1;
    _M0L6bufferS378[_M0L6_2atmpS1779] = _M0L5d__loS387;
    moonbit_decref(_M0L6bufferS378);
  } else {
    int32_t _M0L6_2atmpS1788 = _M0Lm6offsetS367;
    int32_t _M0L6_2atmpS1793;
    int32_t _M0L6_2atmpS1789;
    int32_t _M0L6_2atmpS1792;
    int32_t _M0L6_2atmpS1791;
    int32_t _M0L6_2atmpS1790;
    _M0Lm6offsetS367 = _M0L6_2atmpS1788 - 1;
    _M0L6_2atmpS1793 = _M0Lm6offsetS367;
    _M0L6_2atmpS1789 = _M0L12digit__startS369 + _M0L6_2atmpS1793;
    _M0L6_2atmpS1792 = _M0Lm9remainingS380;
    _M0L6_2atmpS1791 = 48 + _M0L6_2atmpS1792;
    _M0L6_2atmpS1790 = (uint16_t)_M0L6_2atmpS1791;
    _M0L6bufferS378[_M0L6_2atmpS1789] = _M0L6_2atmpS1790;
    moonbit_decref(_M0L6bufferS378);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS360,
  uint64_t _M0L3numS354,
  int32_t _M0L12digit__startS352,
  int32_t _M0L10total__lenS351,
  int32_t _M0L5radixS356
) {
  int32_t _M0Lm6offsetS350;
  uint64_t _M0Lm1nS353;
  uint64_t _M0L4baseS355;
  int32_t _M0L6_2atmpS1720;
  int32_t _M0L6_2atmpS1719;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS350 = _M0L10total__lenS351 - _M0L12digit__startS352;
  _M0Lm1nS353 = _M0L3numS354;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS355 = _M0MPC13int3Int10to__uint64(_M0L5radixS356);
  _M0L6_2atmpS1720 = _M0L5radixS356 - 1;
  _M0L6_2atmpS1719 = _M0L5radixS356 & _M0L6_2atmpS1720;
  if (_M0L6_2atmpS1719 == 0) {
    int32_t _M0L5shiftS357;
    uint64_t _M0L4maskS358;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS357 = moonbit_ctz32(_M0L5radixS356);
    _M0L4maskS358 = _M0L4baseS355 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1721 = _M0Lm1nS353;
      if (_M0L6_2atmpS1721 > 0ull) {
        int32_t _M0L6_2atmpS1722 = _M0Lm6offsetS350;
        uint64_t _M0L6_2atmpS1728;
        uint64_t _M0L6_2atmpS1727;
        int32_t _M0L5digitS359;
        int32_t _M0L6_2atmpS1725;
        int32_t _M0L6_2atmpS1723;
        int32_t _M0L6_2atmpS1724;
        uint64_t _M0L6_2atmpS1726;
        _M0Lm6offsetS350 = _M0L6_2atmpS1722 - 1;
        _M0L6_2atmpS1728 = _M0Lm1nS353;
        _M0L6_2atmpS1727 = _M0L6_2atmpS1728 & _M0L4maskS358;
        _M0L5digitS359 = (int32_t)_M0L6_2atmpS1727;
        _M0L6_2atmpS1725 = _M0Lm6offsetS350;
        _M0L6_2atmpS1723 = _M0L12digit__startS352 + _M0L6_2atmpS1725;
        _M0L6_2atmpS1724
        = ((moonbit_string_t)moonbit_string_literal_85.data)[
          _M0L5digitS359
        ];
        _M0L6bufferS360[_M0L6_2atmpS1723] = _M0L6_2atmpS1724;
        _M0L6_2atmpS1726 = _M0Lm1nS353;
        _M0Lm1nS353 = _M0L6_2atmpS1726 >> (_M0L5shiftS357 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS360);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1729 = _M0Lm1nS353;
      if (_M0L6_2atmpS1729 > 0ull) {
        int32_t _M0L6_2atmpS1730 = _M0Lm6offsetS350;
        uint64_t _M0L6_2atmpS1737;
        uint64_t _M0L1qS362;
        uint64_t _M0L6_2atmpS1735;
        uint64_t _M0L6_2atmpS1736;
        uint64_t _M0L6_2atmpS1734;
        int32_t _M0L5digitS363;
        int32_t _M0L6_2atmpS1733;
        int32_t _M0L6_2atmpS1731;
        int32_t _M0L6_2atmpS1732;
        _M0Lm6offsetS350 = _M0L6_2atmpS1730 - 1;
        _M0L6_2atmpS1737 = _M0Lm1nS353;
        _M0L1qS362 = _M0L6_2atmpS1737 / _M0L4baseS355;
        _M0L6_2atmpS1735 = _M0Lm1nS353;
        _M0L6_2atmpS1736 = _M0L1qS362 * _M0L4baseS355;
        _M0L6_2atmpS1734 = _M0L6_2atmpS1735 - _M0L6_2atmpS1736;
        _M0L5digitS363 = (int32_t)_M0L6_2atmpS1734;
        _M0L6_2atmpS1733 = _M0Lm6offsetS350;
        _M0L6_2atmpS1731 = _M0L12digit__startS352 + _M0L6_2atmpS1733;
        _M0L6_2atmpS1732
        = ((moonbit_string_t)moonbit_string_literal_85.data)[
          _M0L5digitS363
        ];
        _M0L6bufferS360[_M0L6_2atmpS1731] = _M0L6_2atmpS1732;
        _M0Lm1nS353 = _M0L1qS362;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS360);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS347,
  uint64_t _M0L3numS343,
  int32_t _M0L12digit__startS341,
  int32_t _M0L10total__lenS340
) {
  int32_t _M0Lm6offsetS339;
  uint64_t _M0Lm1nS342;
  int32_t _M0L6_2atmpS1715;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS339 = _M0L10total__lenS340 - _M0L12digit__startS341;
  _M0Lm1nS342 = _M0L3numS343;
  while (1) {
    int32_t _M0L6_2atmpS1703 = _M0Lm6offsetS339;
    if (_M0L6_2atmpS1703 >= 2) {
      int32_t _M0L6_2atmpS1704 = _M0Lm6offsetS339;
      uint64_t _M0L6_2atmpS1714;
      uint64_t _M0L6_2atmpS1713;
      int32_t _M0L9byte__valS344;
      int32_t _M0L2hiS345;
      int32_t _M0L2loS346;
      int32_t _M0L6_2atmpS1707;
      int32_t _M0L6_2atmpS1705;
      int32_t _M0L6_2atmpS1706;
      int32_t _M0L6_2atmpS1711;
      int32_t _M0L6_2atmpS1710;
      int32_t _M0L6_2atmpS1708;
      int32_t _M0L6_2atmpS1709;
      uint64_t _M0L6_2atmpS1712;
      _M0Lm6offsetS339 = _M0L6_2atmpS1704 - 2;
      _M0L6_2atmpS1714 = _M0Lm1nS342;
      _M0L6_2atmpS1713 = _M0L6_2atmpS1714 & 255ull;
      _M0L9byte__valS344 = (int32_t)_M0L6_2atmpS1713;
      _M0L2hiS345 = _M0L9byte__valS344 / 16;
      _M0L2loS346 = _M0L9byte__valS344 % 16;
      _M0L6_2atmpS1707 = _M0Lm6offsetS339;
      _M0L6_2atmpS1705 = _M0L12digit__startS341 + _M0L6_2atmpS1707;
      _M0L6_2atmpS1706
      = ((moonbit_string_t)moonbit_string_literal_85.data)[
        _M0L2hiS345
      ];
      _M0L6bufferS347[_M0L6_2atmpS1705] = _M0L6_2atmpS1706;
      _M0L6_2atmpS1711 = _M0Lm6offsetS339;
      _M0L6_2atmpS1710 = _M0L12digit__startS341 + _M0L6_2atmpS1711;
      _M0L6_2atmpS1708 = _M0L6_2atmpS1710 + 1;
      _M0L6_2atmpS1709
      = ((moonbit_string_t)moonbit_string_literal_85.data)[
        _M0L2loS346
      ];
      _M0L6bufferS347[_M0L6_2atmpS1708] = _M0L6_2atmpS1709;
      _M0L6_2atmpS1712 = _M0Lm1nS342;
      _M0Lm1nS342 = _M0L6_2atmpS1712 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1715 = _M0Lm6offsetS339;
  if (_M0L6_2atmpS1715 == 1) {
    uint64_t _M0L6_2atmpS1718 = _M0Lm1nS342;
    uint64_t _M0L6_2atmpS1717 = _M0L6_2atmpS1718 & 15ull;
    int32_t _M0L6nibbleS349 = (int32_t)_M0L6_2atmpS1717;
    int32_t _M0L6_2atmpS1716 =
      ((moonbit_string_t)moonbit_string_literal_85.data)[_M0L6nibbleS349];
    _M0L6bufferS347[_M0L12digit__startS341] = _M0L6_2atmpS1716;
    moonbit_decref(_M0L6bufferS347);
  } else {
    moonbit_decref(_M0L6bufferS347);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS333,
  int32_t _M0L5radixS336
) {
  uint64_t _M0Lm3numS334;
  uint64_t _M0L4baseS335;
  int32_t _M0Lm5countS337;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS333 == 0ull) {
    return 1;
  }
  _M0Lm3numS334 = _M0L5valueS333;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS335 = _M0MPC13int3Int10to__uint64(_M0L5radixS336);
  _M0Lm5countS337 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1700 = _M0Lm3numS334;
    if (_M0L6_2atmpS1700 > 0ull) {
      int32_t _M0L6_2atmpS1701 = _M0Lm5countS337;
      uint64_t _M0L6_2atmpS1702;
      _M0Lm5countS337 = _M0L6_2atmpS1701 + 1;
      _M0L6_2atmpS1702 = _M0Lm3numS334;
      _M0Lm3numS334 = _M0L6_2atmpS1702 / _M0L4baseS335;
      continue;
    }
    break;
  }
  return _M0Lm5countS337;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS331) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS331 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS332;
    int32_t _M0L6_2atmpS1699;
    int32_t _M0L6_2atmpS1698;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS332 = moonbit_clz64(_M0L5valueS331);
    _M0L6_2atmpS1699 = 63 - _M0L14leading__zerosS332;
    _M0L6_2atmpS1698 = _M0L6_2atmpS1699 / 4;
    return _M0L6_2atmpS1698 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS330) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS330 >= 10000000000ull) {
    if (_M0L5valueS330 >= 100000000000000ull) {
      if (_M0L5valueS330 >= 10000000000000000ull) {
        if (_M0L5valueS330 >= 1000000000000000000ull) {
          if (_M0L5valueS330 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS330 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS330 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS330 >= 1000000000000ull) {
      if (_M0L5valueS330 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS330 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS330 >= 100000ull) {
    if (_M0L5valueS330 >= 10000000ull) {
      if (_M0L5valueS330 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS330 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS330 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS330 >= 1000ull) {
    if (_M0L5valueS330 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS330 >= 100ull) {
    return 3;
  } else if (_M0L5valueS330 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS314,
  int32_t _M0L5radixS313
) {
  int32_t _if__result_3273;
  int32_t _M0L12is__negativeS315;
  uint32_t _M0L3numS316;
  uint16_t* _M0L6bufferS317;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS313 < 2) {
    _if__result_3273 = 1;
  } else {
    _if__result_3273 = _M0L5radixS313 > 36;
  }
  if (_if__result_3273) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_83.data, (moonbit_string_t)moonbit_string_literal_86.data);
  }
  if (_M0L4selfS314 == 0) {
    return (moonbit_string_t)moonbit_string_literal_50.data;
  }
  _M0L12is__negativeS315 = _M0L4selfS314 < 0;
  if (_M0L12is__negativeS315) {
    int32_t _M0L6_2atmpS1697 = -_M0L4selfS314;
    _M0L3numS316 = *(uint32_t*)&_M0L6_2atmpS1697;
  } else {
    _M0L3numS316 = *(uint32_t*)&_M0L4selfS314;
  }
  switch (_M0L5radixS313) {
    case 10: {
      int32_t _M0L10digit__lenS318;
      int32_t _M0L6_2atmpS1694;
      int32_t _M0L10total__lenS319;
      uint16_t* _M0L6bufferS320;
      int32_t _M0L12digit__startS321;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS318 = _M0FPB12dec__count32(_M0L3numS316);
      if (_M0L12is__negativeS315) {
        _M0L6_2atmpS1694 = 1;
      } else {
        _M0L6_2atmpS1694 = 0;
      }
      _M0L10total__lenS319 = _M0L10digit__lenS318 + _M0L6_2atmpS1694;
      _M0L6bufferS320
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS319, 0);
      if (_M0L12is__negativeS315) {
        _M0L12digit__startS321 = 1;
      } else {
        _M0L12digit__startS321 = 0;
      }
      moonbit_incref(_M0L6bufferS320);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS320, _M0L3numS316, _M0L12digit__startS321, _M0L10total__lenS319);
      _M0L6bufferS317 = _M0L6bufferS320;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS322;
      int32_t _M0L6_2atmpS1695;
      int32_t _M0L10total__lenS323;
      uint16_t* _M0L6bufferS324;
      int32_t _M0L12digit__startS325;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS322 = _M0FPB12hex__count32(_M0L3numS316);
      if (_M0L12is__negativeS315) {
        _M0L6_2atmpS1695 = 1;
      } else {
        _M0L6_2atmpS1695 = 0;
      }
      _M0L10total__lenS323 = _M0L10digit__lenS322 + _M0L6_2atmpS1695;
      _M0L6bufferS324
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS323, 0);
      if (_M0L12is__negativeS315) {
        _M0L12digit__startS325 = 1;
      } else {
        _M0L12digit__startS325 = 0;
      }
      moonbit_incref(_M0L6bufferS324);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS324, _M0L3numS316, _M0L12digit__startS325, _M0L10total__lenS323);
      _M0L6bufferS317 = _M0L6bufferS324;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS326;
      int32_t _M0L6_2atmpS1696;
      int32_t _M0L10total__lenS327;
      uint16_t* _M0L6bufferS328;
      int32_t _M0L12digit__startS329;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS326
      = _M0FPB14radix__count32(_M0L3numS316, _M0L5radixS313);
      if (_M0L12is__negativeS315) {
        _M0L6_2atmpS1696 = 1;
      } else {
        _M0L6_2atmpS1696 = 0;
      }
      _M0L10total__lenS327 = _M0L10digit__lenS326 + _M0L6_2atmpS1696;
      _M0L6bufferS328
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS327, 0);
      if (_M0L12is__negativeS315) {
        _M0L12digit__startS329 = 1;
      } else {
        _M0L12digit__startS329 = 0;
      }
      moonbit_incref(_M0L6bufferS328);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS328, _M0L3numS316, _M0L12digit__startS329, _M0L10total__lenS327, _M0L5radixS313);
      _M0L6bufferS317 = _M0L6bufferS328;
      break;
    }
  }
  if (_M0L12is__negativeS315) {
    _M0L6bufferS317[0] = 45;
  }
  return _M0L6bufferS317;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS307,
  int32_t _M0L5radixS310
) {
  uint32_t _M0Lm3numS308;
  uint32_t _M0L4baseS309;
  int32_t _M0Lm5countS311;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS307 == 0u) {
    return 1;
  }
  _M0Lm3numS308 = _M0L5valueS307;
  _M0L4baseS309 = *(uint32_t*)&_M0L5radixS310;
  _M0Lm5countS311 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1691 = _M0Lm3numS308;
    if (_M0L6_2atmpS1691 > 0u) {
      int32_t _M0L6_2atmpS1692 = _M0Lm5countS311;
      uint32_t _M0L6_2atmpS1693;
      _M0Lm5countS311 = _M0L6_2atmpS1692 + 1;
      _M0L6_2atmpS1693 = _M0Lm3numS308;
      _M0Lm3numS308 = _M0L6_2atmpS1693 / _M0L4baseS309;
      continue;
    }
    break;
  }
  return _M0Lm5countS311;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS305) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS305 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS306;
    int32_t _M0L6_2atmpS1690;
    int32_t _M0L6_2atmpS1689;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS306 = moonbit_clz32(_M0L5valueS305);
    _M0L6_2atmpS1690 = 31 - _M0L14leading__zerosS306;
    _M0L6_2atmpS1689 = _M0L6_2atmpS1690 / 4;
    return _M0L6_2atmpS1689 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS304) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS304 >= 100000u) {
    if (_M0L5valueS304 >= 10000000u) {
      if (_M0L5valueS304 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS304 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS304 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS304 >= 1000u) {
    if (_M0L5valueS304 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS304 >= 100u) {
    return 3;
  } else if (_M0L5valueS304 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS294,
  uint32_t _M0L3numS282,
  int32_t _M0L12digit__startS285,
  int32_t _M0L10total__lenS284
) {
  uint32_t _M0Lm3numS281;
  int32_t _M0Lm6offsetS283;
  uint32_t _M0L6_2atmpS1688;
  int32_t _M0Lm9remainingS296;
  int32_t _M0L6_2atmpS1669;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS281 = _M0L3numS282;
  _M0Lm6offsetS283 = _M0L10total__lenS284 - _M0L12digit__startS285;
  while (1) {
    uint32_t _M0L6_2atmpS1632 = _M0Lm3numS281;
    if (_M0L6_2atmpS1632 >= 10000u) {
      uint32_t _M0L6_2atmpS1655 = _M0Lm3numS281;
      uint32_t _M0L1tS286 = _M0L6_2atmpS1655 / 10000u;
      uint32_t _M0L6_2atmpS1654 = _M0Lm3numS281;
      uint32_t _M0L6_2atmpS1653 = _M0L6_2atmpS1654 % 10000u;
      int32_t _M0L1rS287 = *(int32_t*)&_M0L6_2atmpS1653;
      int32_t _M0L2d1S288;
      int32_t _M0L2d2S289;
      int32_t _M0L6_2atmpS1633;
      int32_t _M0L6_2atmpS1652;
      int32_t _M0L6_2atmpS1651;
      int32_t _M0L6d1__hiS290;
      int32_t _M0L6_2atmpS1650;
      int32_t _M0L6_2atmpS1649;
      int32_t _M0L6d1__loS291;
      int32_t _M0L6_2atmpS1648;
      int32_t _M0L6_2atmpS1647;
      int32_t _M0L6d2__hiS292;
      int32_t _M0L6_2atmpS1646;
      int32_t _M0L6_2atmpS1645;
      int32_t _M0L6d2__loS293;
      int32_t _M0L6_2atmpS1635;
      int32_t _M0L6_2atmpS1634;
      int32_t _M0L6_2atmpS1638;
      int32_t _M0L6_2atmpS1637;
      int32_t _M0L6_2atmpS1636;
      int32_t _M0L6_2atmpS1641;
      int32_t _M0L6_2atmpS1640;
      int32_t _M0L6_2atmpS1639;
      int32_t _M0L6_2atmpS1644;
      int32_t _M0L6_2atmpS1643;
      int32_t _M0L6_2atmpS1642;
      _M0Lm3numS281 = _M0L1tS286;
      _M0L2d1S288 = _M0L1rS287 / 100;
      _M0L2d2S289 = _M0L1rS287 % 100;
      _M0L6_2atmpS1633 = _M0Lm6offsetS283;
      _M0Lm6offsetS283 = _M0L6_2atmpS1633 - 4;
      _M0L6_2atmpS1652 = _M0L2d1S288 / 10;
      _M0L6_2atmpS1651 = 48 + _M0L6_2atmpS1652;
      _M0L6d1__hiS290 = (uint16_t)_M0L6_2atmpS1651;
      _M0L6_2atmpS1650 = _M0L2d1S288 % 10;
      _M0L6_2atmpS1649 = 48 + _M0L6_2atmpS1650;
      _M0L6d1__loS291 = (uint16_t)_M0L6_2atmpS1649;
      _M0L6_2atmpS1648 = _M0L2d2S289 / 10;
      _M0L6_2atmpS1647 = 48 + _M0L6_2atmpS1648;
      _M0L6d2__hiS292 = (uint16_t)_M0L6_2atmpS1647;
      _M0L6_2atmpS1646 = _M0L2d2S289 % 10;
      _M0L6_2atmpS1645 = 48 + _M0L6_2atmpS1646;
      _M0L6d2__loS293 = (uint16_t)_M0L6_2atmpS1645;
      _M0L6_2atmpS1635 = _M0Lm6offsetS283;
      _M0L6_2atmpS1634 = _M0L12digit__startS285 + _M0L6_2atmpS1635;
      _M0L6bufferS294[_M0L6_2atmpS1634] = _M0L6d1__hiS290;
      _M0L6_2atmpS1638 = _M0Lm6offsetS283;
      _M0L6_2atmpS1637 = _M0L12digit__startS285 + _M0L6_2atmpS1638;
      _M0L6_2atmpS1636 = _M0L6_2atmpS1637 + 1;
      _M0L6bufferS294[_M0L6_2atmpS1636] = _M0L6d1__loS291;
      _M0L6_2atmpS1641 = _M0Lm6offsetS283;
      _M0L6_2atmpS1640 = _M0L12digit__startS285 + _M0L6_2atmpS1641;
      _M0L6_2atmpS1639 = _M0L6_2atmpS1640 + 2;
      _M0L6bufferS294[_M0L6_2atmpS1639] = _M0L6d2__hiS292;
      _M0L6_2atmpS1644 = _M0Lm6offsetS283;
      _M0L6_2atmpS1643 = _M0L12digit__startS285 + _M0L6_2atmpS1644;
      _M0L6_2atmpS1642 = _M0L6_2atmpS1643 + 3;
      _M0L6bufferS294[_M0L6_2atmpS1642] = _M0L6d2__loS293;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1688 = _M0Lm3numS281;
  _M0Lm9remainingS296 = *(int32_t*)&_M0L6_2atmpS1688;
  while (1) {
    int32_t _M0L6_2atmpS1656 = _M0Lm9remainingS296;
    if (_M0L6_2atmpS1656 >= 100) {
      int32_t _M0L6_2atmpS1668 = _M0Lm9remainingS296;
      int32_t _M0L1tS297 = _M0L6_2atmpS1668 / 100;
      int32_t _M0L6_2atmpS1667 = _M0Lm9remainingS296;
      int32_t _M0L1dS298 = _M0L6_2atmpS1667 % 100;
      int32_t _M0L6_2atmpS1657;
      int32_t _M0L6_2atmpS1666;
      int32_t _M0L6_2atmpS1665;
      int32_t _M0L5d__hiS299;
      int32_t _M0L6_2atmpS1664;
      int32_t _M0L6_2atmpS1663;
      int32_t _M0L5d__loS300;
      int32_t _M0L6_2atmpS1659;
      int32_t _M0L6_2atmpS1658;
      int32_t _M0L6_2atmpS1662;
      int32_t _M0L6_2atmpS1661;
      int32_t _M0L6_2atmpS1660;
      _M0Lm9remainingS296 = _M0L1tS297;
      _M0L6_2atmpS1657 = _M0Lm6offsetS283;
      _M0Lm6offsetS283 = _M0L6_2atmpS1657 - 2;
      _M0L6_2atmpS1666 = _M0L1dS298 / 10;
      _M0L6_2atmpS1665 = 48 + _M0L6_2atmpS1666;
      _M0L5d__hiS299 = (uint16_t)_M0L6_2atmpS1665;
      _M0L6_2atmpS1664 = _M0L1dS298 % 10;
      _M0L6_2atmpS1663 = 48 + _M0L6_2atmpS1664;
      _M0L5d__loS300 = (uint16_t)_M0L6_2atmpS1663;
      _M0L6_2atmpS1659 = _M0Lm6offsetS283;
      _M0L6_2atmpS1658 = _M0L12digit__startS285 + _M0L6_2atmpS1659;
      _M0L6bufferS294[_M0L6_2atmpS1658] = _M0L5d__hiS299;
      _M0L6_2atmpS1662 = _M0Lm6offsetS283;
      _M0L6_2atmpS1661 = _M0L12digit__startS285 + _M0L6_2atmpS1662;
      _M0L6_2atmpS1660 = _M0L6_2atmpS1661 + 1;
      _M0L6bufferS294[_M0L6_2atmpS1660] = _M0L5d__loS300;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1669 = _M0Lm9remainingS296;
  if (_M0L6_2atmpS1669 >= 10) {
    int32_t _M0L6_2atmpS1670 = _M0Lm6offsetS283;
    int32_t _M0L6_2atmpS1681;
    int32_t _M0L6_2atmpS1680;
    int32_t _M0L6_2atmpS1679;
    int32_t _M0L5d__hiS302;
    int32_t _M0L6_2atmpS1678;
    int32_t _M0L6_2atmpS1677;
    int32_t _M0L6_2atmpS1676;
    int32_t _M0L5d__loS303;
    int32_t _M0L6_2atmpS1672;
    int32_t _M0L6_2atmpS1671;
    int32_t _M0L6_2atmpS1675;
    int32_t _M0L6_2atmpS1674;
    int32_t _M0L6_2atmpS1673;
    _M0Lm6offsetS283 = _M0L6_2atmpS1670 - 2;
    _M0L6_2atmpS1681 = _M0Lm9remainingS296;
    _M0L6_2atmpS1680 = _M0L6_2atmpS1681 / 10;
    _M0L6_2atmpS1679 = 48 + _M0L6_2atmpS1680;
    _M0L5d__hiS302 = (uint16_t)_M0L6_2atmpS1679;
    _M0L6_2atmpS1678 = _M0Lm9remainingS296;
    _M0L6_2atmpS1677 = _M0L6_2atmpS1678 % 10;
    _M0L6_2atmpS1676 = 48 + _M0L6_2atmpS1677;
    _M0L5d__loS303 = (uint16_t)_M0L6_2atmpS1676;
    _M0L6_2atmpS1672 = _M0Lm6offsetS283;
    _M0L6_2atmpS1671 = _M0L12digit__startS285 + _M0L6_2atmpS1672;
    _M0L6bufferS294[_M0L6_2atmpS1671] = _M0L5d__hiS302;
    _M0L6_2atmpS1675 = _M0Lm6offsetS283;
    _M0L6_2atmpS1674 = _M0L12digit__startS285 + _M0L6_2atmpS1675;
    _M0L6_2atmpS1673 = _M0L6_2atmpS1674 + 1;
    _M0L6bufferS294[_M0L6_2atmpS1673] = _M0L5d__loS303;
    moonbit_decref(_M0L6bufferS294);
  } else {
    int32_t _M0L6_2atmpS1682 = _M0Lm6offsetS283;
    int32_t _M0L6_2atmpS1687;
    int32_t _M0L6_2atmpS1683;
    int32_t _M0L6_2atmpS1686;
    int32_t _M0L6_2atmpS1685;
    int32_t _M0L6_2atmpS1684;
    _M0Lm6offsetS283 = _M0L6_2atmpS1682 - 1;
    _M0L6_2atmpS1687 = _M0Lm6offsetS283;
    _M0L6_2atmpS1683 = _M0L12digit__startS285 + _M0L6_2atmpS1687;
    _M0L6_2atmpS1686 = _M0Lm9remainingS296;
    _M0L6_2atmpS1685 = 48 + _M0L6_2atmpS1686;
    _M0L6_2atmpS1684 = (uint16_t)_M0L6_2atmpS1685;
    _M0L6bufferS294[_M0L6_2atmpS1683] = _M0L6_2atmpS1684;
    moonbit_decref(_M0L6bufferS294);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS276,
  uint32_t _M0L3numS270,
  int32_t _M0L12digit__startS268,
  int32_t _M0L10total__lenS267,
  int32_t _M0L5radixS272
) {
  int32_t _M0Lm6offsetS266;
  uint32_t _M0Lm1nS269;
  uint32_t _M0L4baseS271;
  int32_t _M0L6_2atmpS1614;
  int32_t _M0L6_2atmpS1613;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS266 = _M0L10total__lenS267 - _M0L12digit__startS268;
  _M0Lm1nS269 = _M0L3numS270;
  _M0L4baseS271 = *(uint32_t*)&_M0L5radixS272;
  _M0L6_2atmpS1614 = _M0L5radixS272 - 1;
  _M0L6_2atmpS1613 = _M0L5radixS272 & _M0L6_2atmpS1614;
  if (_M0L6_2atmpS1613 == 0) {
    int32_t _M0L5shiftS273;
    uint32_t _M0L4maskS274;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS273 = moonbit_ctz32(_M0L5radixS272);
    _M0L4maskS274 = _M0L4baseS271 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1615 = _M0Lm1nS269;
      if (_M0L6_2atmpS1615 > 0u) {
        int32_t _M0L6_2atmpS1616 = _M0Lm6offsetS266;
        uint32_t _M0L6_2atmpS1622;
        uint32_t _M0L6_2atmpS1621;
        int32_t _M0L5digitS275;
        int32_t _M0L6_2atmpS1619;
        int32_t _M0L6_2atmpS1617;
        int32_t _M0L6_2atmpS1618;
        uint32_t _M0L6_2atmpS1620;
        _M0Lm6offsetS266 = _M0L6_2atmpS1616 - 1;
        _M0L6_2atmpS1622 = _M0Lm1nS269;
        _M0L6_2atmpS1621 = _M0L6_2atmpS1622 & _M0L4maskS274;
        _M0L5digitS275 = *(int32_t*)&_M0L6_2atmpS1621;
        _M0L6_2atmpS1619 = _M0Lm6offsetS266;
        _M0L6_2atmpS1617 = _M0L12digit__startS268 + _M0L6_2atmpS1619;
        _M0L6_2atmpS1618
        = ((moonbit_string_t)moonbit_string_literal_85.data)[
          _M0L5digitS275
        ];
        _M0L6bufferS276[_M0L6_2atmpS1617] = _M0L6_2atmpS1618;
        _M0L6_2atmpS1620 = _M0Lm1nS269;
        _M0Lm1nS269 = _M0L6_2atmpS1620 >> (_M0L5shiftS273 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS276);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1623 = _M0Lm1nS269;
      if (_M0L6_2atmpS1623 > 0u) {
        int32_t _M0L6_2atmpS1624 = _M0Lm6offsetS266;
        uint32_t _M0L6_2atmpS1631;
        uint32_t _M0L1qS278;
        uint32_t _M0L6_2atmpS1629;
        uint32_t _M0L6_2atmpS1630;
        uint32_t _M0L6_2atmpS1628;
        int32_t _M0L5digitS279;
        int32_t _M0L6_2atmpS1627;
        int32_t _M0L6_2atmpS1625;
        int32_t _M0L6_2atmpS1626;
        _M0Lm6offsetS266 = _M0L6_2atmpS1624 - 1;
        _M0L6_2atmpS1631 = _M0Lm1nS269;
        _M0L1qS278 = _M0L6_2atmpS1631 / _M0L4baseS271;
        _M0L6_2atmpS1629 = _M0Lm1nS269;
        _M0L6_2atmpS1630 = _M0L1qS278 * _M0L4baseS271;
        _M0L6_2atmpS1628 = _M0L6_2atmpS1629 - _M0L6_2atmpS1630;
        _M0L5digitS279 = *(int32_t*)&_M0L6_2atmpS1628;
        _M0L6_2atmpS1627 = _M0Lm6offsetS266;
        _M0L6_2atmpS1625 = _M0L12digit__startS268 + _M0L6_2atmpS1627;
        _M0L6_2atmpS1626
        = ((moonbit_string_t)moonbit_string_literal_85.data)[
          _M0L5digitS279
        ];
        _M0L6bufferS276[_M0L6_2atmpS1625] = _M0L6_2atmpS1626;
        _M0Lm1nS269 = _M0L1qS278;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS276);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS263,
  uint32_t _M0L3numS259,
  int32_t _M0L12digit__startS257,
  int32_t _M0L10total__lenS256
) {
  int32_t _M0Lm6offsetS255;
  uint32_t _M0Lm1nS258;
  int32_t _M0L6_2atmpS1609;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS255 = _M0L10total__lenS256 - _M0L12digit__startS257;
  _M0Lm1nS258 = _M0L3numS259;
  while (1) {
    int32_t _M0L6_2atmpS1597 = _M0Lm6offsetS255;
    if (_M0L6_2atmpS1597 >= 2) {
      int32_t _M0L6_2atmpS1598 = _M0Lm6offsetS255;
      uint32_t _M0L6_2atmpS1608;
      uint32_t _M0L6_2atmpS1607;
      int32_t _M0L9byte__valS260;
      int32_t _M0L2hiS261;
      int32_t _M0L2loS262;
      int32_t _M0L6_2atmpS1601;
      int32_t _M0L6_2atmpS1599;
      int32_t _M0L6_2atmpS1600;
      int32_t _M0L6_2atmpS1605;
      int32_t _M0L6_2atmpS1604;
      int32_t _M0L6_2atmpS1602;
      int32_t _M0L6_2atmpS1603;
      uint32_t _M0L6_2atmpS1606;
      _M0Lm6offsetS255 = _M0L6_2atmpS1598 - 2;
      _M0L6_2atmpS1608 = _M0Lm1nS258;
      _M0L6_2atmpS1607 = _M0L6_2atmpS1608 & 255u;
      _M0L9byte__valS260 = *(int32_t*)&_M0L6_2atmpS1607;
      _M0L2hiS261 = _M0L9byte__valS260 / 16;
      _M0L2loS262 = _M0L9byte__valS260 % 16;
      _M0L6_2atmpS1601 = _M0Lm6offsetS255;
      _M0L6_2atmpS1599 = _M0L12digit__startS257 + _M0L6_2atmpS1601;
      _M0L6_2atmpS1600
      = ((moonbit_string_t)moonbit_string_literal_85.data)[
        _M0L2hiS261
      ];
      _M0L6bufferS263[_M0L6_2atmpS1599] = _M0L6_2atmpS1600;
      _M0L6_2atmpS1605 = _M0Lm6offsetS255;
      _M0L6_2atmpS1604 = _M0L12digit__startS257 + _M0L6_2atmpS1605;
      _M0L6_2atmpS1602 = _M0L6_2atmpS1604 + 1;
      _M0L6_2atmpS1603
      = ((moonbit_string_t)moonbit_string_literal_85.data)[
        _M0L2loS262
      ];
      _M0L6bufferS263[_M0L6_2atmpS1602] = _M0L6_2atmpS1603;
      _M0L6_2atmpS1606 = _M0Lm1nS258;
      _M0Lm1nS258 = _M0L6_2atmpS1606 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1609 = _M0Lm6offsetS255;
  if (_M0L6_2atmpS1609 == 1) {
    uint32_t _M0L6_2atmpS1612 = _M0Lm1nS258;
    uint32_t _M0L6_2atmpS1611 = _M0L6_2atmpS1612 & 15u;
    int32_t _M0L6nibbleS265 = *(int32_t*)&_M0L6_2atmpS1611;
    int32_t _M0L6_2atmpS1610 =
      ((moonbit_string_t)moonbit_string_literal_85.data)[_M0L6nibbleS265];
    _M0L6bufferS263[_M0L12digit__startS257] = _M0L6_2atmpS1610;
    moonbit_decref(_M0L6bufferS263);
  } else {
    moonbit_decref(_M0L6bufferS263);
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

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS254) {
  struct _M0TWEOc* _M0L7_2afuncS253;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS253 = _M0L4selfS254;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS253->code(_M0L7_2afuncS253);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS242
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS241;
  struct _M0TPB6Logger _M0L6_2atmpS1593;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS241 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS241);
  _M0L6_2atmpS1593
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS241
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS242, _M0L6_2atmpS1593);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS241);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS244
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS243;
  struct _M0TPB6Logger _M0L6_2atmpS1594;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS243 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS243);
  _M0L6_2atmpS1594
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS243
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS244, _M0L6_2atmpS1594);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS243);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS246
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS245;
  struct _M0TPB6Logger _M0L6_2atmpS1595;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS245 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS245);
  _M0L6_2atmpS1595
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS245
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS246, _M0L6_2atmpS1595);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS245);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS247;
  struct _M0TPB6Logger _M0L6_2atmpS1596;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS247);
  _M0L6_2atmpS1596
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS247
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS248, _M0L6_2atmpS1596);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS247);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS240
) {
  int32_t _M0L8_2afieldS3002;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3002 = _M0L4selfS240.$1;
  moonbit_decref(_M0L4selfS240.$0);
  return _M0L8_2afieldS3002;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS239
) {
  int32_t _M0L3endS1591;
  int32_t _M0L8_2afieldS3003;
  int32_t _M0L5startS1592;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1591 = _M0L4selfS239.$2;
  _M0L8_2afieldS3003 = _M0L4selfS239.$1;
  moonbit_decref(_M0L4selfS239.$0);
  _M0L5startS1592 = _M0L8_2afieldS3003;
  return _M0L3endS1591 - _M0L5startS1592;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS238
) {
  moonbit_string_t _M0L8_2afieldS3004;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3004 = _M0L4selfS238.$0;
  return _M0L8_2afieldS3004;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS234,
  moonbit_string_t _M0L5valueS235,
  int32_t _M0L5startS236,
  int32_t _M0L3lenS237
) {
  int32_t _M0L6_2atmpS1590;
  int64_t _M0L6_2atmpS1589;
  struct _M0TPC16string10StringView _M0L6_2atmpS1588;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1590 = _M0L5startS236 + _M0L3lenS237;
  _M0L6_2atmpS1589 = (int64_t)_M0L6_2atmpS1590;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1588
  = _M0MPC16string6String11sub_2einner(_M0L5valueS235, _M0L5startS236, _M0L6_2atmpS1589);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS234, _M0L6_2atmpS1588);
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
  int32_t _if__result_3280;
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
      _if__result_3280 = _M0L3endS228 <= _M0L3lenS226;
    } else {
      _if__result_3280 = 0;
    }
  } else {
    _if__result_3280 = 0;
  }
  if (_if__result_3280) {
    if (_M0L5startS232 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1585 = _M0L4selfS227[_M0L5startS232];
      int32_t _M0L6_2atmpS1584;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1584
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1585);
      if (!_M0L6_2atmpS1584) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS228 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1587 = _M0L4selfS227[_M0L3endS228];
      int32_t _M0L6_2atmpS1586;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1586
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1587);
      if (!_M0L6_2atmpS1586) {
        
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
  uint32_t _M0L6_2atmpS1583;
  uint32_t _M0L6_2atmpS1582;
  struct _M0TPB6Hasher* _block_3281;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1583 = *(uint32_t*)&_M0L4seedS218;
  _M0L6_2atmpS1582 = _M0L6_2atmpS1583 + 374761393u;
  _block_3281
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3281)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3281->$0 = _M0L6_2atmpS1582;
  return _block_3281;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS217) {
  uint32_t _M0L6_2atmpS1581;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1581 = _M0MPB6Hasher9avalanche(_M0L4selfS217);
  return *(int32_t*)&_M0L6_2atmpS1581;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS216) {
  uint32_t _M0L8_2afieldS3005;
  uint32_t _M0Lm3accS215;
  uint32_t _M0L6_2atmpS1570;
  uint32_t _M0L6_2atmpS1572;
  uint32_t _M0L6_2atmpS1571;
  uint32_t _M0L6_2atmpS1573;
  uint32_t _M0L6_2atmpS1574;
  uint32_t _M0L6_2atmpS1576;
  uint32_t _M0L6_2atmpS1575;
  uint32_t _M0L6_2atmpS1577;
  uint32_t _M0L6_2atmpS1578;
  uint32_t _M0L6_2atmpS1580;
  uint32_t _M0L6_2atmpS1579;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3005 = _M0L4selfS216->$0;
  moonbit_decref(_M0L4selfS216);
  _M0Lm3accS215 = _M0L8_2afieldS3005;
  _M0L6_2atmpS1570 = _M0Lm3accS215;
  _M0L6_2atmpS1572 = _M0Lm3accS215;
  _M0L6_2atmpS1571 = _M0L6_2atmpS1572 >> 15;
  _M0Lm3accS215 = _M0L6_2atmpS1570 ^ _M0L6_2atmpS1571;
  _M0L6_2atmpS1573 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1573 * 2246822519u;
  _M0L6_2atmpS1574 = _M0Lm3accS215;
  _M0L6_2atmpS1576 = _M0Lm3accS215;
  _M0L6_2atmpS1575 = _M0L6_2atmpS1576 >> 13;
  _M0Lm3accS215 = _M0L6_2atmpS1574 ^ _M0L6_2atmpS1575;
  _M0L6_2atmpS1577 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1577 * 3266489917u;
  _M0L6_2atmpS1578 = _M0Lm3accS215;
  _M0L6_2atmpS1580 = _M0Lm3accS215;
  _M0L6_2atmpS1579 = _M0L6_2atmpS1580 >> 16;
  _M0Lm3accS215 = _M0L6_2atmpS1578 ^ _M0L6_2atmpS1579;
  return _M0Lm3accS215;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS213,
  moonbit_string_t _M0L1yS214
) {
  int32_t _M0L6_2atmpS3006;
  int32_t _M0L6_2atmpS1569;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3006 = moonbit_val_array_equal(_M0L1xS213, _M0L1yS214);
  moonbit_decref(_M0L1xS213);
  moonbit_decref(_M0L1yS214);
  _M0L6_2atmpS1569 = _M0L6_2atmpS3006;
  return !_M0L6_2atmpS1569;
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
  int64_t _M0L6_2atmpS1568;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1568 = (int64_t)_M0L4selfS208;
  return *(uint64_t*)&_M0L6_2atmpS1568;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS206,
  int32_t _M0L5valueS207
) {
  uint32_t _M0L6_2atmpS1567;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1567 = *(uint32_t*)&_M0L5valueS207;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS206, _M0L6_2atmpS1567);
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
      int32_t _if__result_3283;
      moonbit_string_t* _M0L8_2afieldS3008;
      moonbit_string_t* _M0L3bufS1565;
      moonbit_string_t _M0L6_2atmpS3007;
      moonbit_string_t _M0L4itemS202;
      int32_t _M0L6_2atmpS1566;
      if (_M0L1iS201 != 0) {
        moonbit_incref(_M0L3bufS197);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, (moonbit_string_t)moonbit_string_literal_87.data);
      }
      if (_M0L1iS201 < 0) {
        _if__result_3283 = 1;
      } else {
        int32_t _M0L3lenS1564 = _M0L7_2aselfS198->$1;
        _if__result_3283 = _M0L1iS201 >= _M0L3lenS1564;
      }
      if (_if__result_3283) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3008 = _M0L7_2aselfS198->$0;
      _M0L3bufS1565 = _M0L8_2afieldS3008;
      _M0L6_2atmpS3007 = (moonbit_string_t)_M0L3bufS1565[_M0L1iS201];
      _M0L4itemS202 = _M0L6_2atmpS3007;
      if (_M0L4itemS202 == 0) {
        moonbit_incref(_M0L3bufS197);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, (moonbit_string_t)moonbit_string_literal_30.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS203 = _M0L4itemS202;
        moonbit_string_t _M0L6_2alocS204 = _M0L7_2aSomeS203;
        moonbit_string_t _M0L6_2atmpS1563;
        moonbit_incref(_M0L6_2alocS204);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1563
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS204);
        moonbit_incref(_M0L3bufS197);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, _M0L6_2atmpS1563);
      }
      _M0L6_2atmpS1566 = _M0L1iS201 + 1;
      _M0L1iS201 = _M0L6_2atmpS1566;
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
  moonbit_string_t _M0L6_2atmpS1562;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1561;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1562 = _M0L4selfS196;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1561 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1562);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1561);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS195
) {
  struct _M0TPB13StringBuilder* _M0L2sbS194;
  struct _M0TPC16string10StringView _M0L8_2afieldS3021;
  struct _M0TPC16string10StringView _M0L3pkgS1546;
  moonbit_string_t _M0L6_2atmpS1545;
  moonbit_string_t _M0L6_2atmpS3020;
  moonbit_string_t _M0L6_2atmpS1544;
  moonbit_string_t _M0L6_2atmpS3019;
  moonbit_string_t _M0L6_2atmpS1543;
  struct _M0TPC16string10StringView _M0L8_2afieldS3018;
  struct _M0TPC16string10StringView _M0L8filenameS1547;
  struct _M0TPC16string10StringView _M0L8_2afieldS3017;
  struct _M0TPC16string10StringView _M0L11start__lineS1550;
  moonbit_string_t _M0L6_2atmpS1549;
  moonbit_string_t _M0L6_2atmpS3016;
  moonbit_string_t _M0L6_2atmpS1548;
  struct _M0TPC16string10StringView _M0L8_2afieldS3015;
  struct _M0TPC16string10StringView _M0L13start__columnS1553;
  moonbit_string_t _M0L6_2atmpS1552;
  moonbit_string_t _M0L6_2atmpS3014;
  moonbit_string_t _M0L6_2atmpS1551;
  struct _M0TPC16string10StringView _M0L8_2afieldS3013;
  struct _M0TPC16string10StringView _M0L9end__lineS1556;
  moonbit_string_t _M0L6_2atmpS1555;
  moonbit_string_t _M0L6_2atmpS3012;
  moonbit_string_t _M0L6_2atmpS1554;
  struct _M0TPC16string10StringView _M0L8_2afieldS3011;
  int32_t _M0L6_2acntS3133;
  struct _M0TPC16string10StringView _M0L11end__columnS1560;
  moonbit_string_t _M0L6_2atmpS1559;
  moonbit_string_t _M0L6_2atmpS3010;
  moonbit_string_t _M0L6_2atmpS1558;
  moonbit_string_t _M0L6_2atmpS3009;
  moonbit_string_t _M0L6_2atmpS1557;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS194 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3021
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$0_1, _M0L4selfS195->$0_2, _M0L4selfS195->$0_0
  };
  _M0L3pkgS1546 = _M0L8_2afieldS3021;
  moonbit_incref(_M0L3pkgS1546.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1545
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1546);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3020
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_88.data, _M0L6_2atmpS1545);
  moonbit_decref(_M0L6_2atmpS1545);
  _M0L6_2atmpS1544 = _M0L6_2atmpS3020;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3019
  = moonbit_add_string(_M0L6_2atmpS1544, (moonbit_string_t)moonbit_string_literal_89.data);
  moonbit_decref(_M0L6_2atmpS1544);
  _M0L6_2atmpS1543 = _M0L6_2atmpS3019;
  moonbit_incref(_M0L2sbS194);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1543);
  moonbit_incref(_M0L2sbS194);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, (moonbit_string_t)moonbit_string_literal_90.data);
  _M0L8_2afieldS3018
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$1_1, _M0L4selfS195->$1_2, _M0L4selfS195->$1_0
  };
  _M0L8filenameS1547 = _M0L8_2afieldS3018;
  moonbit_incref(_M0L8filenameS1547.$0);
  moonbit_incref(_M0L2sbS194);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS194, _M0L8filenameS1547);
  _M0L8_2afieldS3017
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$2_1, _M0L4selfS195->$2_2, _M0L4selfS195->$2_0
  };
  _M0L11start__lineS1550 = _M0L8_2afieldS3017;
  moonbit_incref(_M0L11start__lineS1550.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1549
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1550);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3016
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_91.data, _M0L6_2atmpS1549);
  moonbit_decref(_M0L6_2atmpS1549);
  _M0L6_2atmpS1548 = _M0L6_2atmpS3016;
  moonbit_incref(_M0L2sbS194);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1548);
  _M0L8_2afieldS3015
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$3_1, _M0L4selfS195->$3_2, _M0L4selfS195->$3_0
  };
  _M0L13start__columnS1553 = _M0L8_2afieldS3015;
  moonbit_incref(_M0L13start__columnS1553.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1552
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1553);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3014
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_92.data, _M0L6_2atmpS1552);
  moonbit_decref(_M0L6_2atmpS1552);
  _M0L6_2atmpS1551 = _M0L6_2atmpS3014;
  moonbit_incref(_M0L2sbS194);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1551);
  _M0L8_2afieldS3013
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$4_1, _M0L4selfS195->$4_2, _M0L4selfS195->$4_0
  };
  _M0L9end__lineS1556 = _M0L8_2afieldS3013;
  moonbit_incref(_M0L9end__lineS1556.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1555
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1556);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3012
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS1555);
  moonbit_decref(_M0L6_2atmpS1555);
  _M0L6_2atmpS1554 = _M0L6_2atmpS3012;
  moonbit_incref(_M0L2sbS194);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1554);
  _M0L8_2afieldS3011
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$5_1, _M0L4selfS195->$5_2, _M0L4selfS195->$5_0
  };
  _M0L6_2acntS3133 = Moonbit_object_header(_M0L4selfS195)->rc;
  if (_M0L6_2acntS3133 > 1) {
    int32_t _M0L11_2anew__cntS3139 = _M0L6_2acntS3133 - 1;
    Moonbit_object_header(_M0L4selfS195)->rc = _M0L11_2anew__cntS3139;
    moonbit_incref(_M0L8_2afieldS3011.$0);
  } else if (_M0L6_2acntS3133 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3138 =
      (struct _M0TPC16string10StringView){_M0L4selfS195->$4_1,
                                            _M0L4selfS195->$4_2,
                                            _M0L4selfS195->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3137;
    struct _M0TPC16string10StringView _M0L8_2afieldS3136;
    struct _M0TPC16string10StringView _M0L8_2afieldS3135;
    struct _M0TPC16string10StringView _M0L8_2afieldS3134;
    moonbit_decref(_M0L8_2afieldS3138.$0);
    _M0L8_2afieldS3137
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$3_1, _M0L4selfS195->$3_2, _M0L4selfS195->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3137.$0);
    _M0L8_2afieldS3136
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$2_1, _M0L4selfS195->$2_2, _M0L4selfS195->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3136.$0);
    _M0L8_2afieldS3135
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$1_1, _M0L4selfS195->$1_2, _M0L4selfS195->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3135.$0);
    _M0L8_2afieldS3134
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$0_1, _M0L4selfS195->$0_2, _M0L4selfS195->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3134.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS195);
  }
  _M0L11end__columnS1560 = _M0L8_2afieldS3011;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1559
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1560);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3010
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_94.data, _M0L6_2atmpS1559);
  moonbit_decref(_M0L6_2atmpS1559);
  _M0L6_2atmpS1558 = _M0L6_2atmpS3010;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3009
  = moonbit_add_string(_M0L6_2atmpS1558, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1558);
  _M0L6_2atmpS1557 = _M0L6_2atmpS3009;
  moonbit_incref(_M0L2sbS194);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1557);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS194);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS192,
  moonbit_string_t _M0L3strS193
) {
  int32_t _M0L3lenS1533;
  int32_t _M0L6_2atmpS1535;
  int32_t _M0L6_2atmpS1534;
  int32_t _M0L6_2atmpS1532;
  moonbit_bytes_t _M0L8_2afieldS3023;
  moonbit_bytes_t _M0L4dataS1536;
  int32_t _M0L3lenS1537;
  int32_t _M0L6_2atmpS1538;
  int32_t _M0L3lenS1540;
  int32_t _M0L6_2atmpS3022;
  int32_t _M0L6_2atmpS1542;
  int32_t _M0L6_2atmpS1541;
  int32_t _M0L6_2atmpS1539;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1533 = _M0L4selfS192->$1;
  _M0L6_2atmpS1535 = Moonbit_array_length(_M0L3strS193);
  _M0L6_2atmpS1534 = _M0L6_2atmpS1535 * 2;
  _M0L6_2atmpS1532 = _M0L3lenS1533 + _M0L6_2atmpS1534;
  moonbit_incref(_M0L4selfS192);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS192, _M0L6_2atmpS1532);
  _M0L8_2afieldS3023 = _M0L4selfS192->$0;
  _M0L4dataS1536 = _M0L8_2afieldS3023;
  _M0L3lenS1537 = _M0L4selfS192->$1;
  _M0L6_2atmpS1538 = Moonbit_array_length(_M0L3strS193);
  moonbit_incref(_M0L4dataS1536);
  moonbit_incref(_M0L3strS193);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1536, _M0L3lenS1537, _M0L3strS193, 0, _M0L6_2atmpS1538);
  _M0L3lenS1540 = _M0L4selfS192->$1;
  _M0L6_2atmpS3022 = Moonbit_array_length(_M0L3strS193);
  moonbit_decref(_M0L3strS193);
  _M0L6_2atmpS1542 = _M0L6_2atmpS3022;
  _M0L6_2atmpS1541 = _M0L6_2atmpS1542 * 2;
  _M0L6_2atmpS1539 = _M0L3lenS1540 + _M0L6_2atmpS1541;
  _M0L4selfS192->$1 = _M0L6_2atmpS1539;
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
  int32_t _M0L6_2atmpS1531;
  int32_t _M0L6_2atmpS1530;
  int32_t _M0L2e1S178;
  int32_t _M0L6_2atmpS1529;
  int32_t _M0L2e2S181;
  int32_t _M0L4len1S183;
  int32_t _M0L4len2S185;
  int32_t _if__result_3284;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1531 = _M0L6lengthS180 * 2;
  _M0L6_2atmpS1530 = _M0L13bytes__offsetS179 + _M0L6_2atmpS1531;
  _M0L2e1S178 = _M0L6_2atmpS1530 - 1;
  _M0L6_2atmpS1529 = _M0L11str__offsetS182 + _M0L6lengthS180;
  _M0L2e2S181 = _M0L6_2atmpS1529 - 1;
  _M0L4len1S183 = Moonbit_array_length(_M0L4selfS184);
  _M0L4len2S185 = Moonbit_array_length(_M0L3strS186);
  if (_M0L6lengthS180 >= 0) {
    if (_M0L13bytes__offsetS179 >= 0) {
      if (_M0L2e1S178 < _M0L4len1S183) {
        if (_M0L11str__offsetS182 >= 0) {
          _if__result_3284 = _M0L2e2S181 < _M0L4len2S185;
        } else {
          _if__result_3284 = 0;
        }
      } else {
        _if__result_3284 = 0;
      }
    } else {
      _if__result_3284 = 0;
    }
  } else {
    _if__result_3284 = 0;
  }
  if (_if__result_3284) {
    int32_t _M0L16end__str__offsetS187 =
      _M0L11str__offsetS182 + _M0L6lengthS180;
    int32_t _M0L1iS188 = _M0L11str__offsetS182;
    int32_t _M0L1jS189 = _M0L13bytes__offsetS179;
    while (1) {
      if (_M0L1iS188 < _M0L16end__str__offsetS187) {
        int32_t _M0L6_2atmpS1526 = _M0L3strS186[_M0L1iS188];
        int32_t _M0L6_2atmpS1525 = (int32_t)_M0L6_2atmpS1526;
        uint32_t _M0L1cS190 = *(uint32_t*)&_M0L6_2atmpS1525;
        uint32_t _M0L6_2atmpS1521 = _M0L1cS190 & 255u;
        int32_t _M0L6_2atmpS1520;
        int32_t _M0L6_2atmpS1522;
        uint32_t _M0L6_2atmpS1524;
        int32_t _M0L6_2atmpS1523;
        int32_t _M0L6_2atmpS1527;
        int32_t _M0L6_2atmpS1528;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1520 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1521);
        if (
          _M0L1jS189 < 0 || _M0L1jS189 >= Moonbit_array_length(_M0L4selfS184)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS184[_M0L1jS189] = _M0L6_2atmpS1520;
        _M0L6_2atmpS1522 = _M0L1jS189 + 1;
        _M0L6_2atmpS1524 = _M0L1cS190 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1523 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1524);
        if (
          _M0L6_2atmpS1522 < 0
          || _M0L6_2atmpS1522 >= Moonbit_array_length(_M0L4selfS184)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS184[_M0L6_2atmpS1522] = _M0L6_2atmpS1523;
        _M0L6_2atmpS1527 = _M0L1iS188 + 1;
        _M0L6_2atmpS1528 = _M0L1jS189 + 2;
        _M0L1iS188 = _M0L6_2atmpS1527;
        _M0L1jS189 = _M0L6_2atmpS1528;
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
  struct _M0TPB6Logger _M0L6_2atmpS1518;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1518
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS175
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS174, _M0L6_2atmpS1518);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS177,
  struct _M0TPC16string10StringView _M0L3objS176
) {
  struct _M0TPB6Logger _M0L6_2atmpS1519;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1519
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS177
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS176, _M0L6_2atmpS1519);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS120
) {
  int32_t _M0L6_2atmpS1517;
  struct _M0TPC16string10StringView _M0L7_2abindS119;
  moonbit_string_t _M0L7_2adataS121;
  int32_t _M0L8_2astartS122;
  int32_t _M0L6_2atmpS1516;
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
  int32_t _M0L6_2atmpS1474;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1517 = Moonbit_array_length(_M0L4reprS120);
  _M0L7_2abindS119
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1517, _M0L4reprS120
  };
  moonbit_incref(_M0L7_2abindS119.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS121 = _M0MPC16string10StringView4data(_M0L7_2abindS119);
  moonbit_incref(_M0L7_2abindS119.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS122
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS119);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1516 = _M0MPC16string10StringView6length(_M0L7_2abindS119);
  _M0L6_2aendS123 = _M0L8_2astartS122 + _M0L6_2atmpS1516;
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
  _M0L6_2atmpS1474 = _M0Lm9_2acursorS124;
  if (_M0L6_2atmpS1474 < _M0L6_2aendS123) {
    int32_t _M0L6_2atmpS1476 = _M0Lm9_2acursorS124;
    int32_t _M0L6_2atmpS1475;
    moonbit_incref(_M0L7_2adataS121);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1475
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1476);
    if (_M0L6_2atmpS1475 == 64) {
      int32_t _M0L6_2atmpS1477 = _M0Lm9_2acursorS124;
      _M0Lm9_2acursorS124 = _M0L6_2atmpS1477 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1478;
        _M0Lm6tag__0S132 = _M0Lm9_2acursorS124;
        _M0L6_2atmpS1478 = _M0Lm9_2acursorS124;
        if (_M0L6_2atmpS1478 < _M0L6_2aendS123) {
          int32_t _M0L6_2atmpS1515 = _M0Lm9_2acursorS124;
          int32_t _M0L10next__charS147;
          int32_t _M0L6_2atmpS1479;
          moonbit_incref(_M0L7_2adataS121);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS147
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1515);
          _M0L6_2atmpS1479 = _M0Lm9_2acursorS124;
          _M0Lm9_2acursorS124 = _M0L6_2atmpS1479 + 1;
          if (_M0L10next__charS147 == 58) {
            int32_t _M0L6_2atmpS1480 = _M0Lm9_2acursorS124;
            if (_M0L6_2atmpS1480 < _M0L6_2aendS123) {
              int32_t _M0L6_2atmpS1481 = _M0Lm9_2acursorS124;
              int32_t _M0L12dispatch__15S148;
              _M0Lm9_2acursorS124 = _M0L6_2atmpS1481 + 1;
              _M0L12dispatch__15S148 = 0;
              loop__label__15_151:;
              while (1) {
                int32_t _M0L6_2atmpS1482;
                switch (_M0L12dispatch__15S148) {
                  case 3: {
                    int32_t _M0L6_2atmpS1485;
                    _M0Lm9tag__1__2S135 = _M0Lm9tag__1__1S134;
                    _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1485 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1485 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1490 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS155;
                      int32_t _M0L6_2atmpS1486;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS155
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1490);
                      _M0L6_2atmpS1486 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1486 + 1;
                      if (_M0L10next__charS155 < 58) {
                        if (_M0L10next__charS155 < 48) {
                          goto join_154;
                        } else {
                          int32_t _M0L6_2atmpS1487;
                          _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                          _M0Lm9tag__2__1S138 = _M0Lm6tag__2S137;
                          _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                          _M0Lm6tag__3S136 = _M0Lm9_2acursorS124;
                          _M0L6_2atmpS1487 = _M0Lm9_2acursorS124;
                          if (_M0L6_2atmpS1487 < _M0L6_2aendS123) {
                            int32_t _M0L6_2atmpS1489 = _M0Lm9_2acursorS124;
                            int32_t _M0L10next__charS157;
                            int32_t _M0L6_2atmpS1488;
                            moonbit_incref(_M0L7_2adataS121);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS157
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1489);
                            _M0L6_2atmpS1488 = _M0Lm9_2acursorS124;
                            _M0Lm9_2acursorS124 = _M0L6_2atmpS1488 + 1;
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
                    int32_t _M0L6_2atmpS1491;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1491 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1491 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1493 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS159;
                      int32_t _M0L6_2atmpS1492;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS159
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1493);
                      _M0L6_2atmpS1492 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1492 + 1;
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
                    int32_t _M0L6_2atmpS1494;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1494 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1494 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1496 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1495;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1496);
                      _M0L6_2atmpS1495 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1495 + 1;
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
                    int32_t _M0L6_2atmpS1497;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__4S139 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1497 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1497 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1505 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS162;
                      int32_t _M0L6_2atmpS1498;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS162
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1505);
                      _M0L6_2atmpS1498 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1498 + 1;
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
                        int32_t _M0L6_2atmpS1499;
                        _M0Lm9tag__1__2S135 = _M0Lm9tag__1__1S134;
                        _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                        _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                        _M0L6_2atmpS1499 = _M0Lm9_2acursorS124;
                        if (_M0L6_2atmpS1499 < _M0L6_2aendS123) {
                          int32_t _M0L6_2atmpS1504 = _M0Lm9_2acursorS124;
                          int32_t _M0L10next__charS164;
                          int32_t _M0L6_2atmpS1500;
                          moonbit_incref(_M0L7_2adataS121);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS164
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1504);
                          _M0L6_2atmpS1500 = _M0Lm9_2acursorS124;
                          _M0Lm9_2acursorS124 = _M0L6_2atmpS1500 + 1;
                          if (_M0L10next__charS164 < 58) {
                            if (_M0L10next__charS164 < 48) {
                              goto join_163;
                            } else {
                              int32_t _M0L6_2atmpS1501;
                              _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                              _M0Lm9tag__2__1S138 = _M0Lm6tag__2S137;
                              _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                              _M0L6_2atmpS1501 = _M0Lm9_2acursorS124;
                              if (_M0L6_2atmpS1501 < _M0L6_2aendS123) {
                                int32_t _M0L6_2atmpS1503 =
                                  _M0Lm9_2acursorS124;
                                int32_t _M0L10next__charS166;
                                int32_t _M0L6_2atmpS1502;
                                moonbit_incref(_M0L7_2adataS121);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS166
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1503);
                                _M0L6_2atmpS1502 = _M0Lm9_2acursorS124;
                                _M0Lm9_2acursorS124 = _M0L6_2atmpS1502 + 1;
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
                    int32_t _M0L6_2atmpS1506;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1506 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1506 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1508 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1507;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1508);
                      _M0L6_2atmpS1507 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1507 + 1;
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
                    int32_t _M0L6_2atmpS1509;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__3S136 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1509 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1509 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1511 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1510;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1511);
                      _M0L6_2atmpS1510 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1510 + 1;
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
                    int32_t _M0L6_2atmpS1512;
                    _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1512 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1512 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1514 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS172;
                      int32_t _M0L6_2atmpS1513;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS172
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1514);
                      _M0L6_2atmpS1513 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1513 + 1;
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
                _M0L6_2atmpS1482 = _M0Lm9_2acursorS124;
                if (_M0L6_2atmpS1482 < _M0L6_2aendS123) {
                  int32_t _M0L6_2atmpS1484 = _M0Lm9_2acursorS124;
                  int32_t _M0L10next__charS152;
                  int32_t _M0L6_2atmpS1483;
                  moonbit_incref(_M0L7_2adataS121);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS152
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1484);
                  _M0L6_2atmpS1483 = _M0Lm9_2acursorS124;
                  _M0Lm9_2acursorS124 = _M0L6_2atmpS1483 + 1;
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
      int32_t _M0L6_2atmpS1473 = _M0Lm20match__tag__saver__1S128;
      int32_t _M0L6_2atmpS1472 = _M0L6_2atmpS1473 + 1;
      int64_t _M0L6_2atmpS1469 = (int64_t)_M0L6_2atmpS1472;
      int32_t _M0L6_2atmpS1471 = _M0Lm20match__tag__saver__2S129;
      int64_t _M0L6_2atmpS1470 = (int64_t)_M0L6_2atmpS1471;
      struct _M0TPC16string10StringView _M0L11start__lineS141;
      int32_t _M0L6_2atmpS1468;
      int32_t _M0L6_2atmpS1467;
      int64_t _M0L6_2atmpS1464;
      int32_t _M0L6_2atmpS1466;
      int64_t _M0L6_2atmpS1465;
      struct _M0TPC16string10StringView _M0L13start__columnS142;
      int32_t _M0L6_2atmpS1463;
      int64_t _M0L6_2atmpS1460;
      int32_t _M0L6_2atmpS1462;
      int64_t _M0L6_2atmpS1461;
      struct _M0TPC16string10StringView _M0L3pkgS143;
      int32_t _M0L6_2atmpS1459;
      int32_t _M0L6_2atmpS1458;
      int64_t _M0L6_2atmpS1455;
      int32_t _M0L6_2atmpS1457;
      int64_t _M0L6_2atmpS1456;
      struct _M0TPC16string10StringView _M0L8filenameS144;
      int32_t _M0L6_2atmpS1454;
      int32_t _M0L6_2atmpS1453;
      int64_t _M0L6_2atmpS1450;
      int32_t _M0L6_2atmpS1452;
      int64_t _M0L6_2atmpS1451;
      struct _M0TPC16string10StringView _M0L9end__lineS145;
      int32_t _M0L6_2atmpS1449;
      int32_t _M0L6_2atmpS1448;
      int64_t _M0L6_2atmpS1445;
      int32_t _M0L6_2atmpS1447;
      int64_t _M0L6_2atmpS1446;
      struct _M0TPC16string10StringView _M0L11end__columnS146;
      struct _M0TPB13SourceLocRepr* _block_3301;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS141
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1469, _M0L6_2atmpS1470);
      _M0L6_2atmpS1468 = _M0Lm20match__tag__saver__2S129;
      _M0L6_2atmpS1467 = _M0L6_2atmpS1468 + 1;
      _M0L6_2atmpS1464 = (int64_t)_M0L6_2atmpS1467;
      _M0L6_2atmpS1466 = _M0Lm20match__tag__saver__3S130;
      _M0L6_2atmpS1465 = (int64_t)_M0L6_2atmpS1466;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS142
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1464, _M0L6_2atmpS1465);
      _M0L6_2atmpS1463 = _M0L8_2astartS122 + 1;
      _M0L6_2atmpS1460 = (int64_t)_M0L6_2atmpS1463;
      _M0L6_2atmpS1462 = _M0Lm20match__tag__saver__0S127;
      _M0L6_2atmpS1461 = (int64_t)_M0L6_2atmpS1462;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS143
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1460, _M0L6_2atmpS1461);
      _M0L6_2atmpS1459 = _M0Lm20match__tag__saver__0S127;
      _M0L6_2atmpS1458 = _M0L6_2atmpS1459 + 1;
      _M0L6_2atmpS1455 = (int64_t)_M0L6_2atmpS1458;
      _M0L6_2atmpS1457 = _M0Lm20match__tag__saver__1S128;
      _M0L6_2atmpS1456 = (int64_t)_M0L6_2atmpS1457;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS144
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1455, _M0L6_2atmpS1456);
      _M0L6_2atmpS1454 = _M0Lm20match__tag__saver__3S130;
      _M0L6_2atmpS1453 = _M0L6_2atmpS1454 + 1;
      _M0L6_2atmpS1450 = (int64_t)_M0L6_2atmpS1453;
      _M0L6_2atmpS1452 = _M0Lm20match__tag__saver__4S131;
      _M0L6_2atmpS1451 = (int64_t)_M0L6_2atmpS1452;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS145
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1450, _M0L6_2atmpS1451);
      _M0L6_2atmpS1449 = _M0Lm20match__tag__saver__4S131;
      _M0L6_2atmpS1448 = _M0L6_2atmpS1449 + 1;
      _M0L6_2atmpS1445 = (int64_t)_M0L6_2atmpS1448;
      _M0L6_2atmpS1447 = _M0Lm10match__endS126;
      _M0L6_2atmpS1446 = (int64_t)_M0L6_2atmpS1447;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS146
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1445, _M0L6_2atmpS1446);
      _block_3301
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3301)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3301->$0_0 = _M0L3pkgS143.$0;
      _block_3301->$0_1 = _M0L3pkgS143.$1;
      _block_3301->$0_2 = _M0L3pkgS143.$2;
      _block_3301->$1_0 = _M0L8filenameS144.$0;
      _block_3301->$1_1 = _M0L8filenameS144.$1;
      _block_3301->$1_2 = _M0L8filenameS144.$2;
      _block_3301->$2_0 = _M0L11start__lineS141.$0;
      _block_3301->$2_1 = _M0L11start__lineS141.$1;
      _block_3301->$2_2 = _M0L11start__lineS141.$2;
      _block_3301->$3_0 = _M0L13start__columnS142.$0;
      _block_3301->$3_1 = _M0L13start__columnS142.$1;
      _block_3301->$3_2 = _M0L13start__columnS142.$2;
      _block_3301->$4_0 = _M0L9end__lineS145.$0;
      _block_3301->$4_1 = _M0L9end__lineS145.$1;
      _block_3301->$4_2 = _M0L9end__lineS145.$2;
      _block_3301->$5_0 = _M0L11end__columnS146.$0;
      _block_3301->$5_1 = _M0L11end__columnS146.$1;
      _block_3301->$5_2 = _M0L11end__columnS146.$2;
      return _block_3301;
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
  int32_t _if__result_3302;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS116 = _M0L4selfS117->$1;
  if (_M0L5indexS118 >= 0) {
    _if__result_3302 = _M0L5indexS118 < _M0L3lenS116;
  } else {
    _if__result_3302 = 0;
  }
  if (_if__result_3302) {
    moonbit_string_t* _M0L6_2atmpS1444;
    moonbit_string_t _M0L6_2atmpS3024;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1444 = _M0MPC15array5Array6bufferGsE(_M0L4selfS117);
    if (
      _M0L5indexS118 < 0
      || _M0L5indexS118 >= Moonbit_array_length(_M0L6_2atmpS1444)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3024 = (moonbit_string_t)_M0L6_2atmpS1444[_M0L5indexS118];
    moonbit_incref(_M0L6_2atmpS3024);
    moonbit_decref(_M0L6_2atmpS1444);
    return _M0L6_2atmpS3024;
  } else {
    moonbit_decref(_M0L4selfS117);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS113
) {
  moonbit_string_t* _M0L8_2afieldS3025;
  int32_t _M0L6_2acntS3140;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3025 = _M0L4selfS113->$0;
  _M0L6_2acntS3140 = Moonbit_object_header(_M0L4selfS113)->rc;
  if (_M0L6_2acntS3140 > 1) {
    int32_t _M0L11_2anew__cntS3141 = _M0L6_2acntS3140 - 1;
    Moonbit_object_header(_M0L4selfS113)->rc = _M0L11_2anew__cntS3141;
    moonbit_incref(_M0L8_2afieldS3025);
  } else if (_M0L6_2acntS3140 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS113);
  }
  return _M0L8_2afieldS3025;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS114
) {
  struct _M0TUsiE** _M0L8_2afieldS3026;
  int32_t _M0L6_2acntS3142;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3026 = _M0L4selfS114->$0;
  _M0L6_2acntS3142 = Moonbit_object_header(_M0L4selfS114)->rc;
  if (_M0L6_2acntS3142 > 1) {
    int32_t _M0L11_2anew__cntS3143 = _M0L6_2acntS3142 - 1;
    Moonbit_object_header(_M0L4selfS114)->rc = _M0L11_2anew__cntS3143;
    moonbit_incref(_M0L8_2afieldS3026);
  } else if (_M0L6_2acntS3142 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS114);
  }
  return _M0L8_2afieldS3026;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS115
) {
  void** _M0L8_2afieldS3027;
  int32_t _M0L6_2acntS3144;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3027 = _M0L4selfS115->$0;
  _M0L6_2acntS3144 = Moonbit_object_header(_M0L4selfS115)->rc;
  if (_M0L6_2acntS3144 > 1) {
    int32_t _M0L11_2anew__cntS3145 = _M0L6_2acntS3144 - 1;
    Moonbit_object_header(_M0L4selfS115)->rc = _M0L11_2anew__cntS3145;
    moonbit_incref(_M0L8_2afieldS3027);
  } else if (_M0L6_2acntS3144 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS115);
  }
  return _M0L8_2afieldS3027;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS112) {
  struct _M0TPB13StringBuilder* _M0L3bufS111;
  struct _M0TPB6Logger _M0L6_2atmpS1443;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS111 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS111);
  _M0L6_2atmpS1443
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS111
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS112, _M0L6_2atmpS1443);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS111);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS110) {
  int32_t _M0L6_2atmpS1442;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1442 = (int32_t)_M0L4selfS110;
  return _M0L6_2atmpS1442;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS108,
  int32_t _M0L8trailingS109
) {
  int32_t _M0L6_2atmpS1441;
  int32_t _M0L6_2atmpS1440;
  int32_t _M0L6_2atmpS1439;
  int32_t _M0L6_2atmpS1438;
  int32_t _M0L6_2atmpS1437;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1441 = _M0L7leadingS108 - 55296;
  _M0L6_2atmpS1440 = _M0L6_2atmpS1441 * 1024;
  _M0L6_2atmpS1439 = _M0L6_2atmpS1440 + _M0L8trailingS109;
  _M0L6_2atmpS1438 = _M0L6_2atmpS1439 - 56320;
  _M0L6_2atmpS1437 = _M0L6_2atmpS1438 + 65536;
  return _M0L6_2atmpS1437;
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
  int32_t _M0L3lenS1432;
  int32_t _M0L6_2atmpS1431;
  moonbit_bytes_t _M0L8_2afieldS3028;
  moonbit_bytes_t _M0L4dataS1435;
  int32_t _M0L3lenS1436;
  int32_t _M0L3incS104;
  int32_t _M0L3lenS1434;
  int32_t _M0L6_2atmpS1433;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1432 = _M0L4selfS103->$1;
  _M0L6_2atmpS1431 = _M0L3lenS1432 + 4;
  moonbit_incref(_M0L4selfS103);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS103, _M0L6_2atmpS1431);
  _M0L8_2afieldS3028 = _M0L4selfS103->$0;
  _M0L4dataS1435 = _M0L8_2afieldS3028;
  _M0L3lenS1436 = _M0L4selfS103->$1;
  moonbit_incref(_M0L4dataS1435);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS104
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1435, _M0L3lenS1436, _M0L2chS105);
  _M0L3lenS1434 = _M0L4selfS103->$1;
  _M0L6_2atmpS1433 = _M0L3lenS1434 + _M0L3incS104;
  _M0L4selfS103->$1 = _M0L6_2atmpS1433;
  moonbit_decref(_M0L4selfS103);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS98,
  int32_t _M0L8requiredS99
) {
  moonbit_bytes_t _M0L8_2afieldS3032;
  moonbit_bytes_t _M0L4dataS1430;
  int32_t _M0L6_2atmpS3031;
  int32_t _M0L12current__lenS97;
  int32_t _M0Lm13enough__spaceS100;
  int32_t _M0L6_2atmpS1428;
  int32_t _M0L6_2atmpS1429;
  moonbit_bytes_t _M0L9new__dataS102;
  moonbit_bytes_t _M0L8_2afieldS3030;
  moonbit_bytes_t _M0L4dataS1426;
  int32_t _M0L3lenS1427;
  moonbit_bytes_t _M0L6_2aoldS3029;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3032 = _M0L4selfS98->$0;
  _M0L4dataS1430 = _M0L8_2afieldS3032;
  _M0L6_2atmpS3031 = Moonbit_array_length(_M0L4dataS1430);
  _M0L12current__lenS97 = _M0L6_2atmpS3031;
  if (_M0L8requiredS99 <= _M0L12current__lenS97) {
    moonbit_decref(_M0L4selfS98);
    return 0;
  }
  _M0Lm13enough__spaceS100 = _M0L12current__lenS97;
  while (1) {
    int32_t _M0L6_2atmpS1424 = _M0Lm13enough__spaceS100;
    if (_M0L6_2atmpS1424 < _M0L8requiredS99) {
      int32_t _M0L6_2atmpS1425 = _M0Lm13enough__spaceS100;
      _M0Lm13enough__spaceS100 = _M0L6_2atmpS1425 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1428 = _M0Lm13enough__spaceS100;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1429 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS102
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1428, _M0L6_2atmpS1429);
  _M0L8_2afieldS3030 = _M0L4selfS98->$0;
  _M0L4dataS1426 = _M0L8_2afieldS3030;
  _M0L3lenS1427 = _M0L4selfS98->$1;
  moonbit_incref(_M0L4dataS1426);
  moonbit_incref(_M0L9new__dataS102);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS102, 0, _M0L4dataS1426, 0, _M0L3lenS1427);
  _M0L6_2aoldS3029 = _M0L4selfS98->$0;
  moonbit_decref(_M0L6_2aoldS3029);
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
    uint32_t _M0L6_2atmpS1407 = _M0L4codeS90 & 255u;
    int32_t _M0L6_2atmpS1406;
    int32_t _M0L6_2atmpS1408;
    uint32_t _M0L6_2atmpS1410;
    int32_t _M0L6_2atmpS1409;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1406 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1407);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1406;
    _M0L6_2atmpS1408 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1410 = _M0L4codeS90 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1409 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1410);
    if (
      _M0L6_2atmpS1408 < 0
      || _M0L6_2atmpS1408 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1408] = _M0L6_2atmpS1409;
    moonbit_decref(_M0L4selfS92);
    return 2;
  } else if (_M0L4codeS90 < 1114112u) {
    uint32_t _M0L2hiS94 = _M0L4codeS90 - 65536u;
    uint32_t _M0L6_2atmpS1423 = _M0L2hiS94 >> 10;
    uint32_t _M0L2loS95 = _M0L6_2atmpS1423 | 55296u;
    uint32_t _M0L6_2atmpS1422 = _M0L2hiS94 & 1023u;
    uint32_t _M0L2hiS96 = _M0L6_2atmpS1422 | 56320u;
    uint32_t _M0L6_2atmpS1412 = _M0L2loS95 & 255u;
    int32_t _M0L6_2atmpS1411;
    int32_t _M0L6_2atmpS1413;
    uint32_t _M0L6_2atmpS1415;
    int32_t _M0L6_2atmpS1414;
    int32_t _M0L6_2atmpS1416;
    uint32_t _M0L6_2atmpS1418;
    int32_t _M0L6_2atmpS1417;
    int32_t _M0L6_2atmpS1419;
    uint32_t _M0L6_2atmpS1421;
    int32_t _M0L6_2atmpS1420;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1411 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1412);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1411;
    _M0L6_2atmpS1413 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1415 = _M0L2loS95 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1414 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1415);
    if (
      _M0L6_2atmpS1413 < 0
      || _M0L6_2atmpS1413 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1413] = _M0L6_2atmpS1414;
    _M0L6_2atmpS1416 = _M0L6offsetS93 + 2;
    _M0L6_2atmpS1418 = _M0L2hiS96 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1417 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1418);
    if (
      _M0L6_2atmpS1416 < 0
      || _M0L6_2atmpS1416 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1416] = _M0L6_2atmpS1417;
    _M0L6_2atmpS1419 = _M0L6offsetS93 + 3;
    _M0L6_2atmpS1421 = _M0L2hiS96 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1420 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1421);
    if (
      _M0L6_2atmpS1419 < 0
      || _M0L6_2atmpS1419 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1419] = _M0L6_2atmpS1420;
    moonbit_decref(_M0L4selfS92);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS92);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_95.data, (moonbit_string_t)moonbit_string_literal_96.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1405;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1405 = *(int32_t*)&_M0L4selfS89;
  return _M0L6_2atmpS1405 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS88) {
  int32_t _M0L6_2atmpS1404;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1404 = _M0L4selfS88;
  return *(uint32_t*)&_M0L6_2atmpS1404;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS87
) {
  moonbit_bytes_t _M0L8_2afieldS3034;
  moonbit_bytes_t _M0L4dataS1403;
  moonbit_bytes_t _M0L6_2atmpS1400;
  int32_t _M0L8_2afieldS3033;
  int32_t _M0L3lenS1402;
  int64_t _M0L6_2atmpS1401;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3034 = _M0L4selfS87->$0;
  _M0L4dataS1403 = _M0L8_2afieldS3034;
  moonbit_incref(_M0L4dataS1403);
  _M0L6_2atmpS1400 = _M0L4dataS1403;
  _M0L8_2afieldS3033 = _M0L4selfS87->$1;
  moonbit_decref(_M0L4selfS87);
  _M0L3lenS1402 = _M0L8_2afieldS3033;
  _M0L6_2atmpS1401 = (int64_t)_M0L3lenS1402;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1400, 0, _M0L6_2atmpS1401);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS82,
  int32_t _M0L6offsetS86,
  int64_t _M0L6lengthS84
) {
  int32_t _M0L3lenS81;
  int32_t _M0L6lengthS83;
  int32_t _if__result_3304;
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
      int32_t _M0L6_2atmpS1399 = _M0L6offsetS86 + _M0L6lengthS83;
      _if__result_3304 = _M0L6_2atmpS1399 <= _M0L3lenS81;
    } else {
      _if__result_3304 = 0;
    }
  } else {
    _if__result_3304 = 0;
  }
  if (_if__result_3304) {
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
  struct _M0TPB13StringBuilder* _block_3305;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS79 < 1) {
    _M0L7initialS78 = 1;
  } else {
    _M0L7initialS78 = _M0L10size__hintS79;
  }
  _M0L4dataS80 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS78, 0);
  _block_3305
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3305)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3305->$0 = _M0L4dataS80;
  _block_3305->$1 = 0;
  return _block_3305;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS77) {
  int32_t _M0L6_2atmpS1398;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1398 = (int32_t)_M0L4selfS77;
  return _M0L6_2atmpS1398;
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
  int32_t _if__result_3306;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS26 == _M0L3srcS27) {
    _if__result_3306 = _M0L11dst__offsetS28 < _M0L11src__offsetS29;
  } else {
    _if__result_3306 = 0;
  }
  if (_if__result_3306) {
    int32_t _M0L1iS30 = 0;
    while (1) {
      if (_M0L1iS30 < _M0L3lenS31) {
        int32_t _M0L6_2atmpS1362 = _M0L11dst__offsetS28 + _M0L1iS30;
        int32_t _M0L6_2atmpS1364 = _M0L11src__offsetS29 + _M0L1iS30;
        int32_t _M0L6_2atmpS1363;
        int32_t _M0L6_2atmpS1365;
        if (
          _M0L6_2atmpS1364 < 0
          || _M0L6_2atmpS1364 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1363 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1364];
        if (
          _M0L6_2atmpS1362 < 0
          || _M0L6_2atmpS1362 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1362] = _M0L6_2atmpS1363;
        _M0L6_2atmpS1365 = _M0L1iS30 + 1;
        _M0L1iS30 = _M0L6_2atmpS1365;
        continue;
      } else {
        moonbit_decref(_M0L3srcS27);
        moonbit_decref(_M0L3dstS26);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1370 = _M0L3lenS31 - 1;
    int32_t _M0L1iS33 = _M0L6_2atmpS1370;
    while (1) {
      if (_M0L1iS33 >= 0) {
        int32_t _M0L6_2atmpS1366 = _M0L11dst__offsetS28 + _M0L1iS33;
        int32_t _M0L6_2atmpS1368 = _M0L11src__offsetS29 + _M0L1iS33;
        int32_t _M0L6_2atmpS1367;
        int32_t _M0L6_2atmpS1369;
        if (
          _M0L6_2atmpS1368 < 0
          || _M0L6_2atmpS1368 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1367 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1368];
        if (
          _M0L6_2atmpS1366 < 0
          || _M0L6_2atmpS1366 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1366] = _M0L6_2atmpS1367;
        _M0L6_2atmpS1369 = _M0L1iS33 - 1;
        _M0L1iS33 = _M0L6_2atmpS1369;
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
  int32_t _if__result_3309;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS35 == _M0L3srcS36) {
    _if__result_3309 = _M0L11dst__offsetS37 < _M0L11src__offsetS38;
  } else {
    _if__result_3309 = 0;
  }
  if (_if__result_3309) {
    int32_t _M0L1iS39 = 0;
    while (1) {
      if (_M0L1iS39 < _M0L3lenS40) {
        int32_t _M0L6_2atmpS1371 = _M0L11dst__offsetS37 + _M0L1iS39;
        int32_t _M0L6_2atmpS1373 = _M0L11src__offsetS38 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS3036;
        moonbit_string_t _M0L6_2atmpS1372;
        moonbit_string_t _M0L6_2aoldS3035;
        int32_t _M0L6_2atmpS1374;
        if (
          _M0L6_2atmpS1373 < 0
          || _M0L6_2atmpS1373 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3036 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1373];
        _M0L6_2atmpS1372 = _M0L6_2atmpS3036;
        if (
          _M0L6_2atmpS1371 < 0
          || _M0L6_2atmpS1371 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3035 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1371];
        moonbit_incref(_M0L6_2atmpS1372);
        moonbit_decref(_M0L6_2aoldS3035);
        _M0L3dstS35[_M0L6_2atmpS1371] = _M0L6_2atmpS1372;
        _M0L6_2atmpS1374 = _M0L1iS39 + 1;
        _M0L1iS39 = _M0L6_2atmpS1374;
        continue;
      } else {
        moonbit_decref(_M0L3srcS36);
        moonbit_decref(_M0L3dstS35);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1379 = _M0L3lenS40 - 1;
    int32_t _M0L1iS42 = _M0L6_2atmpS1379;
    while (1) {
      if (_M0L1iS42 >= 0) {
        int32_t _M0L6_2atmpS1375 = _M0L11dst__offsetS37 + _M0L1iS42;
        int32_t _M0L6_2atmpS1377 = _M0L11src__offsetS38 + _M0L1iS42;
        moonbit_string_t _M0L6_2atmpS3038;
        moonbit_string_t _M0L6_2atmpS1376;
        moonbit_string_t _M0L6_2aoldS3037;
        int32_t _M0L6_2atmpS1378;
        if (
          _M0L6_2atmpS1377 < 0
          || _M0L6_2atmpS1377 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3038 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1377];
        _M0L6_2atmpS1376 = _M0L6_2atmpS3038;
        if (
          _M0L6_2atmpS1375 < 0
          || _M0L6_2atmpS1375 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3037 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1375];
        moonbit_incref(_M0L6_2atmpS1376);
        moonbit_decref(_M0L6_2aoldS3037);
        _M0L3dstS35[_M0L6_2atmpS1375] = _M0L6_2atmpS1376;
        _M0L6_2atmpS1378 = _M0L1iS42 - 1;
        _M0L1iS42 = _M0L6_2atmpS1378;
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
  int32_t _if__result_3312;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS44 == _M0L3srcS45) {
    _if__result_3312 = _M0L11dst__offsetS46 < _M0L11src__offsetS47;
  } else {
    _if__result_3312 = 0;
  }
  if (_if__result_3312) {
    int32_t _M0L1iS48 = 0;
    while (1) {
      if (_M0L1iS48 < _M0L3lenS49) {
        int32_t _M0L6_2atmpS1380 = _M0L11dst__offsetS46 + _M0L1iS48;
        int32_t _M0L6_2atmpS1382 = _M0L11src__offsetS47 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS3040;
        struct _M0TUsiE* _M0L6_2atmpS1381;
        struct _M0TUsiE* _M0L6_2aoldS3039;
        int32_t _M0L6_2atmpS1383;
        if (
          _M0L6_2atmpS1382 < 0
          || _M0L6_2atmpS1382 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3040 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1382];
        _M0L6_2atmpS1381 = _M0L6_2atmpS3040;
        if (
          _M0L6_2atmpS1380 < 0
          || _M0L6_2atmpS1380 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3039 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1380];
        if (_M0L6_2atmpS1381) {
          moonbit_incref(_M0L6_2atmpS1381);
        }
        if (_M0L6_2aoldS3039) {
          moonbit_decref(_M0L6_2aoldS3039);
        }
        _M0L3dstS44[_M0L6_2atmpS1380] = _M0L6_2atmpS1381;
        _M0L6_2atmpS1383 = _M0L1iS48 + 1;
        _M0L1iS48 = _M0L6_2atmpS1383;
        continue;
      } else {
        moonbit_decref(_M0L3srcS45);
        moonbit_decref(_M0L3dstS44);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1388 = _M0L3lenS49 - 1;
    int32_t _M0L1iS51 = _M0L6_2atmpS1388;
    while (1) {
      if (_M0L1iS51 >= 0) {
        int32_t _M0L6_2atmpS1384 = _M0L11dst__offsetS46 + _M0L1iS51;
        int32_t _M0L6_2atmpS1386 = _M0L11src__offsetS47 + _M0L1iS51;
        struct _M0TUsiE* _M0L6_2atmpS3042;
        struct _M0TUsiE* _M0L6_2atmpS1385;
        struct _M0TUsiE* _M0L6_2aoldS3041;
        int32_t _M0L6_2atmpS1387;
        if (
          _M0L6_2atmpS1386 < 0
          || _M0L6_2atmpS1386 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3042 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1386];
        _M0L6_2atmpS1385 = _M0L6_2atmpS3042;
        if (
          _M0L6_2atmpS1384 < 0
          || _M0L6_2atmpS1384 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3041 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1384];
        if (_M0L6_2atmpS1385) {
          moonbit_incref(_M0L6_2atmpS1385);
        }
        if (_M0L6_2aoldS3041) {
          moonbit_decref(_M0L6_2aoldS3041);
        }
        _M0L3dstS44[_M0L6_2atmpS1384] = _M0L6_2atmpS1385;
        _M0L6_2atmpS1387 = _M0L1iS51 - 1;
        _M0L1iS51 = _M0L6_2atmpS1387;
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
  int32_t _if__result_3315;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS53 == _M0L3srcS54) {
    _if__result_3315 = _M0L11dst__offsetS55 < _M0L11src__offsetS56;
  } else {
    _if__result_3315 = 0;
  }
  if (_if__result_3315) {
    int32_t _M0L1iS57 = 0;
    while (1) {
      if (_M0L1iS57 < _M0L3lenS58) {
        int32_t _M0L6_2atmpS1389 = _M0L11dst__offsetS55 + _M0L1iS57;
        int32_t _M0L6_2atmpS1391 = _M0L11src__offsetS56 + _M0L1iS57;
        void* _M0L6_2atmpS3044;
        void* _M0L6_2atmpS1390;
        void* _M0L6_2aoldS3043;
        int32_t _M0L6_2atmpS1392;
        if (
          _M0L6_2atmpS1391 < 0
          || _M0L6_2atmpS1391 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3044 = (void*)_M0L3srcS54[_M0L6_2atmpS1391];
        _M0L6_2atmpS1390 = _M0L6_2atmpS3044;
        if (
          _M0L6_2atmpS1389 < 0
          || _M0L6_2atmpS1389 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3043 = (void*)_M0L3dstS53[_M0L6_2atmpS1389];
        moonbit_incref(_M0L6_2atmpS1390);
        moonbit_decref(_M0L6_2aoldS3043);
        _M0L3dstS53[_M0L6_2atmpS1389] = _M0L6_2atmpS1390;
        _M0L6_2atmpS1392 = _M0L1iS57 + 1;
        _M0L1iS57 = _M0L6_2atmpS1392;
        continue;
      } else {
        moonbit_decref(_M0L3srcS54);
        moonbit_decref(_M0L3dstS53);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1397 = _M0L3lenS58 - 1;
    int32_t _M0L1iS60 = _M0L6_2atmpS1397;
    while (1) {
      if (_M0L1iS60 >= 0) {
        int32_t _M0L6_2atmpS1393 = _M0L11dst__offsetS55 + _M0L1iS60;
        int32_t _M0L6_2atmpS1395 = _M0L11src__offsetS56 + _M0L1iS60;
        void* _M0L6_2atmpS3046;
        void* _M0L6_2atmpS1394;
        void* _M0L6_2aoldS3045;
        int32_t _M0L6_2atmpS1396;
        if (
          _M0L6_2atmpS1395 < 0
          || _M0L6_2atmpS1395 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3046 = (void*)_M0L3srcS54[_M0L6_2atmpS1395];
        _M0L6_2atmpS1394 = _M0L6_2atmpS3046;
        if (
          _M0L6_2atmpS1393 < 0
          || _M0L6_2atmpS1393 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3045 = (void*)_M0L3dstS53[_M0L6_2atmpS1393];
        moonbit_incref(_M0L6_2atmpS1394);
        moonbit_decref(_M0L6_2aoldS3045);
        _M0L3dstS53[_M0L6_2atmpS1393] = _M0L6_2atmpS1394;
        _M0L6_2atmpS1396 = _M0L1iS60 - 1;
        _M0L1iS60 = _M0L6_2atmpS1396;
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
  moonbit_string_t _M0L6_2atmpS1346;
  moonbit_string_t _M0L6_2atmpS3049;
  moonbit_string_t _M0L6_2atmpS1344;
  moonbit_string_t _M0L6_2atmpS1345;
  moonbit_string_t _M0L6_2atmpS3048;
  moonbit_string_t _M0L6_2atmpS1343;
  moonbit_string_t _M0L6_2atmpS3047;
  moonbit_string_t _M0L6_2atmpS1342;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1346 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3049
  = moonbit_add_string(_M0L6_2atmpS1346, (moonbit_string_t)moonbit_string_literal_97.data);
  moonbit_decref(_M0L6_2atmpS1346);
  _M0L6_2atmpS1344 = _M0L6_2atmpS3049;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1345
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3048 = moonbit_add_string(_M0L6_2atmpS1344, _M0L6_2atmpS1345);
  moonbit_decref(_M0L6_2atmpS1344);
  moonbit_decref(_M0L6_2atmpS1345);
  _M0L6_2atmpS1343 = _M0L6_2atmpS3048;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3047
  = moonbit_add_string(_M0L6_2atmpS1343, (moonbit_string_t)moonbit_string_literal_31.data);
  moonbit_decref(_M0L6_2atmpS1343);
  _M0L6_2atmpS1342 = _M0L6_2atmpS3047;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1342);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1351;
  moonbit_string_t _M0L6_2atmpS3052;
  moonbit_string_t _M0L6_2atmpS1349;
  moonbit_string_t _M0L6_2atmpS1350;
  moonbit_string_t _M0L6_2atmpS3051;
  moonbit_string_t _M0L6_2atmpS1348;
  moonbit_string_t _M0L6_2atmpS3050;
  moonbit_string_t _M0L6_2atmpS1347;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1351 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3052
  = moonbit_add_string(_M0L6_2atmpS1351, (moonbit_string_t)moonbit_string_literal_97.data);
  moonbit_decref(_M0L6_2atmpS1351);
  _M0L6_2atmpS1349 = _M0L6_2atmpS3052;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1350
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3051 = moonbit_add_string(_M0L6_2atmpS1349, _M0L6_2atmpS1350);
  moonbit_decref(_M0L6_2atmpS1349);
  moonbit_decref(_M0L6_2atmpS1350);
  _M0L6_2atmpS1348 = _M0L6_2atmpS3051;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3050
  = moonbit_add_string(_M0L6_2atmpS1348, (moonbit_string_t)moonbit_string_literal_31.data);
  moonbit_decref(_M0L6_2atmpS1348);
  _M0L6_2atmpS1347 = _M0L6_2atmpS3050;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1347);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1356;
  moonbit_string_t _M0L6_2atmpS3055;
  moonbit_string_t _M0L6_2atmpS1354;
  moonbit_string_t _M0L6_2atmpS1355;
  moonbit_string_t _M0L6_2atmpS3054;
  moonbit_string_t _M0L6_2atmpS1353;
  moonbit_string_t _M0L6_2atmpS3053;
  moonbit_string_t _M0L6_2atmpS1352;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1356 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3055
  = moonbit_add_string(_M0L6_2atmpS1356, (moonbit_string_t)moonbit_string_literal_97.data);
  moonbit_decref(_M0L6_2atmpS1356);
  _M0L6_2atmpS1354 = _M0L6_2atmpS3055;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1355
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3054 = moonbit_add_string(_M0L6_2atmpS1354, _M0L6_2atmpS1355);
  moonbit_decref(_M0L6_2atmpS1354);
  moonbit_decref(_M0L6_2atmpS1355);
  _M0L6_2atmpS1353 = _M0L6_2atmpS3054;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3053
  = moonbit_add_string(_M0L6_2atmpS1353, (moonbit_string_t)moonbit_string_literal_31.data);
  moonbit_decref(_M0L6_2atmpS1353);
  _M0L6_2atmpS1352 = _M0L6_2atmpS3053;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1352);
}

moonbit_string_t _M0FPB5abortGsE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1361;
  moonbit_string_t _M0L6_2atmpS3058;
  moonbit_string_t _M0L6_2atmpS1359;
  moonbit_string_t _M0L6_2atmpS1360;
  moonbit_string_t _M0L6_2atmpS3057;
  moonbit_string_t _M0L6_2atmpS1358;
  moonbit_string_t _M0L6_2atmpS3056;
  moonbit_string_t _M0L6_2atmpS1357;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1361 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3058
  = moonbit_add_string(_M0L6_2atmpS1361, (moonbit_string_t)moonbit_string_literal_97.data);
  moonbit_decref(_M0L6_2atmpS1361);
  _M0L6_2atmpS1359 = _M0L6_2atmpS3058;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1360
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3057 = moonbit_add_string(_M0L6_2atmpS1359, _M0L6_2atmpS1360);
  moonbit_decref(_M0L6_2atmpS1359);
  moonbit_decref(_M0L6_2atmpS1360);
  _M0L6_2atmpS1358 = _M0L6_2atmpS3057;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3056
  = moonbit_add_string(_M0L6_2atmpS1358, (moonbit_string_t)moonbit_string_literal_31.data);
  moonbit_decref(_M0L6_2atmpS1358);
  _M0L6_2atmpS1357 = _M0L6_2atmpS3056;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGsE(_M0L6_2atmpS1357);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1341;
  uint32_t _M0L6_2atmpS1340;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1341 = _M0L4selfS16->$0;
  _M0L6_2atmpS1340 = _M0L3accS1341 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1340;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1338;
  uint32_t _M0L6_2atmpS1339;
  uint32_t _M0L6_2atmpS1337;
  uint32_t _M0L6_2atmpS1336;
  uint32_t _M0L6_2atmpS1335;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1338 = _M0L4selfS14->$0;
  _M0L6_2atmpS1339 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1337 = _M0L3accS1338 + _M0L6_2atmpS1339;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1336 = _M0FPB4rotl(_M0L6_2atmpS1337, 17);
  _M0L6_2atmpS1335 = _M0L6_2atmpS1336 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1335;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1332;
  int32_t _M0L6_2atmpS1334;
  uint32_t _M0L6_2atmpS1333;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1332 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1334 = 32 - _M0L1rS13;
  _M0L6_2atmpS1333 = _M0L1xS12 >> (_M0L6_2atmpS1334 & 31);
  return _M0L6_2atmpS1332 | _M0L6_2atmpS1333;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS3059;
  int32_t _M0L6_2acntS3146;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS3059 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS3146 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS3146 > 1) {
    int32_t _M0L11_2anew__cntS3147 = _M0L6_2acntS3146 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS3147;
    moonbit_incref(_M0L8_2afieldS3059);
  } else if (_M0L6_2acntS3146 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS3059;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_98.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_99.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_3318;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3318 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3318)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3318)->$0 = _M0L4selfS7;
  return _block_3318;
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

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t _M0L3msgS4) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1233) {
  switch (Moonbit_object_tag(_M0L4_2aeS1233)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1233);
      return (moonbit_string_t)moonbit_string_literal_100.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1233);
      return (moonbit_string_t)moonbit_string_literal_101.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1233);
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1233);
      return (moonbit_string_t)moonbit_string_literal_102.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1233);
      return (moonbit_string_t)moonbit_string_literal_103.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1270,
  int32_t _M0L8_2aparamS1269
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1268 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1270;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1268, _M0L8_2aparamS1269);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1267,
  struct _M0TPC16string10StringView _M0L8_2aparamS1266
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1265 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1267;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1265, _M0L8_2aparamS1266);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1264,
  moonbit_string_t _M0L8_2aparamS1261,
  int32_t _M0L8_2aparamS1262,
  int32_t _M0L8_2aparamS1263
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1260 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1264;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1260, _M0L8_2aparamS1261, _M0L8_2aparamS1262, _M0L8_2aparamS1263);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1259,
  moonbit_string_t _M0L8_2aparamS1258
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1257 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1259;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1257, _M0L8_2aparamS1258);
  return 0;
}

void* _M0IPC16uint646UInt64PB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1255
) {
  struct _M0Y6UInt64* _M0L14_2aboxed__selfS1256 =
    (struct _M0Y6UInt64*)_M0L11_2aobj__ptrS1255;
  uint64_t _M0L8_2afieldS3060 = _M0L14_2aboxed__selfS1256->$0;
  uint64_t _M0L7_2aselfS1254;
  moonbit_decref(_M0L14_2aboxed__selfS1256);
  _M0L7_2aselfS1254 = _M0L8_2afieldS3060;
  return _M0IPC16uint646UInt64PB6ToJson8to__json(_M0L7_2aselfS1254);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1331 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1330;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1329;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1152;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1328;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1327;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1326;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1278;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1153;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1325;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1324;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1323;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1279;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1154;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1322;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1321;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1320;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1280;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1155;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1319;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1318;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1317;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1281;
  moonbit_string_t* _M0L6_2atmpS1316;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1315;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1314;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1156;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1313;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1312;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1311;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1282;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1157;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1310;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1309;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1308;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1283;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1158;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1307;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1306;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1305;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1284;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1159;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1304;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1303;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1302;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1285;
  moonbit_string_t* _M0L6_2atmpS1301;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1300;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1290;
  moonbit_string_t* _M0L6_2atmpS1299;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1298;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1291;
  moonbit_string_t* _M0L6_2atmpS1297;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1296;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1292;
  moonbit_string_t* _M0L6_2atmpS1295;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1294;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1293;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1160;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1289;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1288;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1287;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1286;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1151;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1277;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1276;
  _M0L6_2atmpS1331[0] = (moonbit_string_t)moonbit_string_literal_104.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__7374726c656e5f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1330
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1330)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1330->$0
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__7374726c656e5f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1330->$1 = _M0L6_2atmpS1331;
  _M0L8_2atupleS1329
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1329)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1329->$0 = 0;
  _M0L8_2atupleS1329->$1 = _M0L8_2atupleS1330;
  _M0L7_2abindS1152
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1152[0] = _M0L8_2atupleS1329;
  _M0L6_2atmpS1328 = _M0L7_2abindS1152;
  _M0L6_2atmpS1327
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1328
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1326
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1327);
  _M0L8_2atupleS1278
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1278)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1278->$0 = (moonbit_string_t)moonbit_string_literal_105.data;
  _M0L8_2atupleS1278->$1 = _M0L6_2atmpS1326;
  _M0L7_2abindS1153
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1325 = _M0L7_2abindS1153;
  _M0L6_2atmpS1324
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1325
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1323
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1324);
  _M0L8_2atupleS1279
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1279)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1279->$0 = (moonbit_string_t)moonbit_string_literal_106.data;
  _M0L8_2atupleS1279->$1 = _M0L6_2atmpS1323;
  _M0L7_2abindS1154
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1322 = _M0L7_2abindS1154;
  _M0L6_2atmpS1321
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1322
  };
  #line 403 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1320
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1321);
  _M0L8_2atupleS1280
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1280)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1280->$0 = (moonbit_string_t)moonbit_string_literal_107.data;
  _M0L8_2atupleS1280->$1 = _M0L6_2atmpS1320;
  _M0L7_2abindS1155
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1319 = _M0L7_2abindS1155;
  _M0L6_2atmpS1318
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1319
  };
  #line 405 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1317
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1318);
  _M0L8_2atupleS1281
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1281)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1281->$0 = (moonbit_string_t)moonbit_string_literal_108.data;
  _M0L8_2atupleS1281->$1 = _M0L6_2atmpS1317;
  _M0L6_2atmpS1316 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1316[0] = (moonbit_string_t)moonbit_string_literal_109.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__6d656d6370795f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1315
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1315)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1315->$0
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test49____test__6d656d6370795f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1315->$1 = _M0L6_2atmpS1316;
  _M0L8_2atupleS1314
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1314)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1314->$0 = 0;
  _M0L8_2atupleS1314->$1 = _M0L8_2atupleS1315;
  _M0L7_2abindS1156
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1156[0] = _M0L8_2atupleS1314;
  _M0L6_2atmpS1313 = _M0L7_2abindS1156;
  _M0L6_2atmpS1312
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1313
  };
  #line 407 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1311
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1312);
  _M0L8_2atupleS1282
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1282)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1282->$0 = (moonbit_string_t)moonbit_string_literal_110.data;
  _M0L8_2atupleS1282->$1 = _M0L6_2atmpS1311;
  _M0L7_2abindS1157
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1310 = _M0L7_2abindS1157;
  _M0L6_2atmpS1309
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1310
  };
  #line 410 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1308
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1309);
  _M0L8_2atupleS1283
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1283)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1283->$0 = (moonbit_string_t)moonbit_string_literal_111.data;
  _M0L8_2atupleS1283->$1 = _M0L6_2atmpS1308;
  _M0L7_2abindS1158
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1307 = _M0L7_2abindS1158;
  _M0L6_2atmpS1306
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1307
  };
  #line 412 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1305
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1306);
  _M0L8_2atupleS1284
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1284)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1284->$0 = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L8_2atupleS1284->$1 = _M0L6_2atmpS1305;
  _M0L7_2abindS1159
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1304 = _M0L7_2abindS1159;
  _M0L6_2atmpS1303
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1304
  };
  #line 414 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1302
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1303);
  _M0L8_2atupleS1285
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1285)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1285->$0 = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L8_2atupleS1285->$1 = _M0L6_2atmpS1302;
  _M0L6_2atmpS1301 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1301[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__0_2eclo);
  _M0L8_2atupleS1300
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1300)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1300->$0
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__0_2eclo;
  _M0L8_2atupleS1300->$1 = _M0L6_2atmpS1301;
  _M0L8_2atupleS1290
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1290)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1290->$0 = 0;
  _M0L8_2atupleS1290->$1 = _M0L8_2atupleS1300;
  _M0L6_2atmpS1299 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1299[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__1_2eclo);
  _M0L8_2atupleS1298
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1298)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1298->$0
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__1_2eclo;
  _M0L8_2atupleS1298->$1 = _M0L6_2atmpS1299;
  _M0L8_2atupleS1291
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1291)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1291->$0 = 1;
  _M0L8_2atupleS1291->$1 = _M0L8_2atupleS1298;
  _M0L6_2atmpS1297 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1297[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__2_2eclo);
  _M0L8_2atupleS1296
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1296)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1296->$0
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__2_2eclo;
  _M0L8_2atupleS1296->$1 = _M0L6_2atmpS1297;
  _M0L8_2atupleS1292
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1292)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1292->$0 = 2;
  _M0L8_2atupleS1292->$1 = _M0L8_2atupleS1296;
  _M0L6_2atmpS1295 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1295[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__3_2eclo);
  _M0L8_2atupleS1294
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1294)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1294->$0
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test45____test__524541444d452e6d62742e6d64__3_2eclo;
  _M0L8_2atupleS1294->$1 = _M0L6_2atmpS1295;
  _M0L8_2atupleS1293
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1293)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1293->$0 = 3;
  _M0L8_2atupleS1293->$1 = _M0L8_2atupleS1294;
  _M0L7_2abindS1160
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1160[0] = _M0L8_2atupleS1290;
  _M0L7_2abindS1160[1] = _M0L8_2atupleS1291;
  _M0L7_2abindS1160[2] = _M0L8_2atupleS1292;
  _M0L7_2abindS1160[3] = _M0L8_2atupleS1293;
  _M0L6_2atmpS1289 = _M0L7_2abindS1160;
  _M0L6_2atmpS1288
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 4, _M0L6_2atmpS1289
  };
  #line 416 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1287
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1288);
  _M0L8_2atupleS1286
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1286)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1286->$0 = (moonbit_string_t)moonbit_string_literal_114.data;
  _M0L8_2atupleS1286->$1 = _M0L6_2atmpS1287;
  _M0L7_2abindS1151
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(9);
  _M0L7_2abindS1151[0] = _M0L8_2atupleS1278;
  _M0L7_2abindS1151[1] = _M0L8_2atupleS1279;
  _M0L7_2abindS1151[2] = _M0L8_2atupleS1280;
  _M0L7_2abindS1151[3] = _M0L8_2atupleS1281;
  _M0L7_2abindS1151[4] = _M0L8_2atupleS1282;
  _M0L7_2abindS1151[5] = _M0L8_2atupleS1283;
  _M0L7_2abindS1151[6] = _M0L8_2atupleS1284;
  _M0L7_2abindS1151[7] = _M0L8_2atupleS1285;
  _M0L7_2abindS1151[8] = _M0L8_2atupleS1286;
  _M0L6_2atmpS1277 = _M0L7_2abindS1151;
  _M0L6_2atmpS1276
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 9, _M0L6_2atmpS1277
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal17c__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1276);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1275;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1227;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1228;
  int32_t _M0L7_2abindS1229;
  int32_t _M0L2__S1230;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1275
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1227
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1227)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1227->$0 = _M0L6_2atmpS1275;
  _M0L12async__testsS1227->$1 = 0;
  #line 459 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1228
  = _M0FP48clawteam8clawteam8internal17c__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1229 = _M0L7_2abindS1228->$1;
  _M0L2__S1230 = 0;
  while (1) {
    if (_M0L2__S1230 < _M0L7_2abindS1229) {
      struct _M0TUsiE** _M0L8_2afieldS3064 = _M0L7_2abindS1228->$0;
      struct _M0TUsiE** _M0L3bufS1274 = _M0L8_2afieldS3064;
      struct _M0TUsiE* _M0L6_2atmpS3063 =
        (struct _M0TUsiE*)_M0L3bufS1274[_M0L2__S1230];
      struct _M0TUsiE* _M0L3argS1231 = _M0L6_2atmpS3063;
      moonbit_string_t _M0L8_2afieldS3062 = _M0L3argS1231->$0;
      moonbit_string_t _M0L6_2atmpS1271 = _M0L8_2afieldS3062;
      int32_t _M0L8_2afieldS3061 = _M0L3argS1231->$1;
      int32_t _M0L6_2atmpS1272 = _M0L8_2afieldS3061;
      int32_t _M0L6_2atmpS1273;
      moonbit_incref(_M0L6_2atmpS1271);
      moonbit_incref(_M0L12async__testsS1227);
      #line 460 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal17c__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1227, _M0L6_2atmpS1271, _M0L6_2atmpS1272);
      _M0L6_2atmpS1273 = _M0L2__S1230 + 1;
      _M0L2__S1230 = _M0L6_2atmpS1273;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1228);
    }
    break;
  }
  #line 462 "E:\\moonbit\\clawteam\\internal\\c\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal17c__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal17c__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1227);
  return 0;
}