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
struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6schema6Schema;

struct _M0TWssbEu;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0R38String_3a_3aiter_2eanon__u2344__l247__;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__;

struct _M0DTPC16option6OptionGORPC111sorted__set4NodeGsEE4Some;

struct _M0TWsERPB4Json;

struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__;

struct _M0TPB6Logger;

struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object;

struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE;

struct _M0DTPB4Json6Number;

struct _M0TWEORPB4Json;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTP48clawteam8clawteam8internal6schema6Schema4Enum;

struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0DTP48clawteam8clawteam8internal6schema6Schema5Array;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6schema33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0TPC111sorted__set4NodeGsE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TUORPC111sorted__set4NodeGsEbE;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGsE;

struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE;

struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE;

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE;

struct _M0TWEOUsRPB3MapGsRPB4JsonEE;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TWRPB4JsonEb;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__;

struct _M0TPB9ArrayViewGRPB4JsonE;

struct _M0TPC111sorted__set9SortedSetGsE;

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0TUsRPB3MapGsRPB4JsonEE;

struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPB4Json6Object;

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6schema33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0DTPC16result6ResultGuRPB7FailureE3Err;

struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__;

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok;

struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE {
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
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

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6schema6Schema {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
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

struct _M0R38String_3a_3aiter_2eanon__u2344__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0DTPC16option6OptionGORPC111sorted__set4NodeGsEE4Some {
  struct _M0TPC111sorted__set4NodeGsE* $0;
  
};

struct _M0TWsERPB4Json {
  void*(* code)(struct _M0TWsERPB4Json*, moonbit_string_t);
  
};

struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__ {
  int32_t(* code)(struct _M0TWRPB4JsonEb*, void*);
  void* $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object {
  int32_t $2;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGsRPB4JsonE* $5;
  
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

struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** $0;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* $5;
  
};

struct _M0DTPB4Json6Number {
  double $0;
  moonbit_string_t $1;
  
};

struct _M0TWEORPB4Json {
  void*(* code)(struct _M0TWEORPB4Json*);
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0DTP48clawteam8clawteam8internal6schema6Schema4Enum {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTP48clawteam8clawteam8internal6schema6Schema5Array {
  void* $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6schema33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TPC111sorted__set4NodeGsE {
  int32_t $3;
  moonbit_string_t $0;
  struct _M0TPC111sorted__set4NodeGsE* $1;
  struct _M0TPC111sorted__set4NodeGsE* $2;
  
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

struct _M0TUORPC111sorted__set4NodeGsEbE {
  int32_t $1;
  struct _M0TPC111sorted__set4NodeGsE* $0;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** $0;
  
};

struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__ {
  struct _M0TUsRPB3MapGsRPB4JsonEE*(* code)(
    struct _M0TWEOUsRPB3MapGsRPB4JsonEE*
  );
  struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE* $0;
  
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

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** $0;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* $5;
  
};

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE {
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*(* code)(
    struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*
  );
  
};

struct _M0TWEOUsRPB3MapGsRPB4JsonEE {
  struct _M0TUsRPB3MapGsRPB4JsonEE*(* code)(
    struct _M0TWEOUsRPB3MapGsRPB4JsonEE*
  );
  
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

struct _M0TWRPB4JsonEb {
  int32_t(* code)(struct _M0TWRPB4JsonEb*, void*);
  
};

struct _M0TPB9ArrayViewGUsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB4JsonE** $0;
  
};

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* $1;
  moonbit_string_t $4;
  void* $5;
  
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

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
};

struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB9ArrayViewGRPB4JsonE {
  int32_t $1;
  int32_t $2;
  void** $0;
  
};

struct _M0TPC111sorted__set9SortedSetGsE {
  int32_t $1;
  struct _M0TPC111sorted__set4NodeGsE* $0;
  
};

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE {
  moonbit_string_t $0;
  void* $1;
  
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

struct _M0TUsRPB3MapGsRPB4JsonEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGsRPB4JsonE* $1;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* $0;
  
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

struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
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

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6schema33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__ {
  void*(* code)(struct _M0TWEORPB4Json*);
  int32_t $0_1;
  int32_t $0_2;
  void** $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok {
  int32_t $0;
  
};

struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__ {
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*(* code)(
    struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*
  );
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE* $0;
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0MPC14json4Json16string_2edyncall(
  struct _M0TWsERPB4Json*,
  moonbit_string_t
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN17error__to__stringS1708(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN14handle__resultS1699(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testC3791l426(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testC3787l427(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal6schema45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1633(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1628(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1615(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal6schema28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6schema34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema33____test__736368656d612e6d6274__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema33____test__736368656d612e6d6274__0(
  
);

int32_t _M0MP48clawteam8clawteam8internal6schema6Schema6verify(void*, void*);

int32_t _M0MP48clawteam8clawteam8internal6schema6Schema6verifyC3379l96(
  struct _M0TWRPB4JsonEb*,
  void*
);

void* _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson8to__json(
  void*
);

struct _M0TPB3MapGsRPB4JsonE* _M0MP48clawteam8clawteam8internal6schema6Schema8to__json(
  void*
);

void* _M0FP48clawteam8clawteam8internal6schema14object_2einner(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  int32_t,
  struct _M0TPB5ArrayGsE*
);

void* _M0FP48clawteam8clawteam8internal6schema6string();

void* _M0FP48clawteam8clawteam8internal6schema7integer();

int32_t _M0MPC111sorted__set9SortedSet8containsGsE(
  struct _M0TPC111sorted__set9SortedSetGsE*,
  moonbit_string_t
);

struct _M0TPC111sorted__set9SortedSetGsE* _M0MPC111sorted__set9SortedSet11from__arrayGsE(
  struct _M0TPB9ArrayViewGsE
);

int32_t _M0MPC111sorted__set9SortedSet3addGsE(
  struct _M0TPC111sorted__set9SortedSetGsE*,
  moonbit_string_t
);

struct _M0TUORPC111sorted__set4NodeGsEbE* _M0FPC111sorted__set9add__nodeGsE(
  struct _M0TPC111sorted__set4NodeGsE*,
  moonbit_string_t
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set9new__nodeGsE(
  moonbit_string_t,
  void*,
  void*,
  int64_t
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set17new__node_2einnerGsE(
  moonbit_string_t,
  struct _M0TPC111sorted__set4NodeGsE*,
  struct _M0TPC111sorted__set4NodeGsE*,
  int32_t
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set7balanceGsE(
  struct _M0TPC111sorted__set4NodeGsE*
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set10rotate__rlGsE(
  struct _M0TPC111sorted__set4NodeGsE*
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set10rotate__lrGsE(
  struct _M0TPC111sorted__set4NodeGsE*
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set9rotate__rGsE(
  struct _M0TPC111sorted__set4NodeGsE*
);

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set9rotate__lGsE(
  struct _M0TPC111sorted__set4NodeGsE*
);

int32_t _M0MPC111sorted__set4Node14update__heightGsE(
  struct _M0TPC111sorted__set4NodeGsE*
);

int32_t _M0FPC111sorted__set10height__geGsE(
  struct _M0TPC111sorted__set4NodeGsE*,
  struct _M0TPC111sorted__set4NodeGsE*
);

struct _M0TPC111sorted__set9SortedSetGsE* _M0MPC111sorted__set9SortedSet3newGsE(
  
);

int32_t _M0FPC111sorted__set6heightGsE(struct _M0TPC111sorted__set4NodeGsE*);

int32_t _M0FPC111sorted__set3max(int32_t, int32_t);

int32_t _M0IPC111sorted__set4NodePB2Eq5equalGsE(
  struct _M0TPC111sorted__set4NodeGsE*,
  struct _M0TPC111sorted__set4NodeGsE*
);

void* _M0IPC14json4JsonPB6ToJson8to__json(void*);

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

int32_t _M0MPC15array5Array8containsGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*,
  void*
);

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

int32_t _M0IPC15array5ArrayPB2Eq5equalGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*,
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

int32_t _M0MPC16double6Double7to__int(double);

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB13assert__false(
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

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*
);

struct _M0TUsRPB4JsonE* _M0MPB5Iter24nextGsRPB4JsonE(
  struct _M0TWEOUsRPB4JsonE*
);

struct _M0TUsRPB3MapGsRPB4JsonEE* _M0MPB5Iter24nextGsRPB3MapGsRPB4JsonEE(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE*
);

int32_t _M0MPB4Iter3allGRPB4JsonE(
  struct _M0TWEORPB4Json*,
  struct _M0TWRPB4JsonEb*
);

void* _M0IPB3MapPB6ToJson8to__jsonGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0IPB3MapPB6ToJson8to__jsonGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGsRPB4JsonE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TWsERPB4Json*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json7boolean(int32_t);

void* _M0MPC14json4Json6string(moonbit_string_t);

int32_t _M0IPC14json4JsonPB2Eq5equal(void*, void*);

int32_t _M0IPB3MapPB2Eq5equalGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*
);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map5iter2GsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0MPB3Map5iter2GsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*
);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*
);

struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0MPB3Map4iterGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*
);

struct _M0TUsRPB3MapGsRPB4JsonEE* _M0MPB3Map4iterGsRPB3MapGsRPB4JsonEEC2814l591(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE*
);

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal6schema6SchemaEC2808l591(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2802l591(
  struct _M0TWEOUsRPB4JsonE*
);

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

int32_t _M0MPB3Map12contains__kvGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  moonbit_string_t,
  void*
);

int32_t _M0MPB3Map8containsGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  moonbit_string_t
);

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

struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE
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

int32_t _M0MPB3Map3setGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  moonbit_string_t,
  void*
);

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  moonbit_string_t,
  void*
);

int32_t _M0MPB3Map3setGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*,
  moonbit_string_t,
  struct _M0TPB3MapGsRPB4JsonE*
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map4growGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*
);

int32_t _M0MPB3Map4growGsRPB4JsonE(struct _M0TPB3MapGsRPB4JsonE*);

int32_t _M0MPB3Map4growGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*
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

int32_t _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  moonbit_string_t,
  void*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  moonbit_string_t,
  void*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*,
  moonbit_string_t,
  struct _M0TPB3MapGsRPB4JsonE*,
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

int32_t _M0MPB3Map10push__awayGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  int32_t,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*
);

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  int32_t,
  struct _M0TPB5EntryGsRPB4JsonE*
);

int32_t _M0MPB3Map10push__awayGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*
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

int32_t _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  struct _M0TPB5EntryGsRPB4JsonE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*,
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*,
  int32_t,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*,
  int32_t,
  struct _M0TPB5EntryGsRPB4JsonE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map11new_2einnerGsRP48clawteam8clawteam8internal6schema6SchemaE(
  int32_t
);

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(int32_t);

struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0MPB3Map11new_2einnerGsRPB3MapGsRPB4JsonEE(
  int32_t
);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TPB5ArrayGsE* _M0MPC16option6Option10unwrap__orGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGsRPB4JsonEEE(
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*
);

struct _M0TPC111sorted__set4NodeGsE* _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(
  struct _M0TPC111sorted__set4NodeGsE*
);

int32_t _M0IPC16option6OptionPB2Eq5equalGRPC111sorted__set4NodeGsEE(
  struct _M0TPC111sorted__set4NodeGsE*,
  struct _M0TPC111sorted__set4NodeGsE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t
);

moonbit_string_t _M0MPC15array9ArrayView2atGsE(
  struct _M0TPB9ArrayViewGsE,
  int32_t
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEORPB4Json* _M0MPC15array5Array4iterGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

struct _M0TWEORPB4Json* _M0MPC15array9ArrayView4iterGRPB4JsonE(
  struct _M0TPB9ArrayViewGRPB4JsonE
);

void* _M0MPC15array9ArrayView4iterGRPB4JsonEC2379l570(
  struct _M0TWEORPB4Json*
);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2367l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC16string6StringPB7Compare7compare(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2344l247(struct _M0TWEOc*);

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

int32_t _M0MPC15array9ArrayView6lengthGUsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE
);

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE
);

int32_t _M0MPC15array9ArrayView6lengthGRPB4JsonE(
  struct _M0TPB9ArrayViewGRPB4JsonE
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

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB4Iter3newGUsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*
);

struct _M0TWEORPB4Json* _M0MPB4Iter3newGRPB4JsonE(struct _M0TWEORPB4Json*);

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc*);

struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0MPB4Iter3newGUsRPB3MapGsRPB4JsonEEE(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE*
);

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

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB4Iter4nextGUsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*
);

void* _M0MPB4Iter4nextGRPB4JsonE(struct _M0TWEORPB4Json*);

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc*);

struct _M0TUsRPB3MapGsRPB4JsonEE* _M0MPB4Iter4nextGUsRPB3MapGsRPB4JsonEEE(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE*
);

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

int32_t _M0IP016_24default__implPB2Eq10not__equalGORPC111sorted__set4NodeGsEE(
  struct _M0TPC111sorted__set4NodeGsE*,
  struct _M0TPC111sorted__set4NodeGsE*
);

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

uint64_t _M0MPC13int3Int10to__uint64(int32_t);

void* _M0MPC14json4Json6number(double, moonbit_string_t);

int32_t _M0MPB6Hasher12combine__int(struct _M0TPB6Hasher*, int32_t);

int32_t _M0IPC16uint166UInt16PB7Compare7compare(int32_t, int32_t);

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

moonbit_string_t _M0FPB5abortGsE(moonbit_string_t, moonbit_string_t);

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

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

void* _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
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
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    118, 101, 114, 105, 102, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 51, 50, 58, 49, 51, 45, 49, 52, 52, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    74, 111, 104, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 56, 49, 58, 51, 45, 49, 56, 53, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    101, 110, 117, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 56, 54, 58, 51, 45, 49, 56, 56, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_66 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 99, 104, 101, 
    109, 97, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    116, 111, 95, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_46 =
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
} const moonbit_string_literal_101 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 51, 53, 
    58, 53, 45, 49, 51, 55, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 121, 112, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[102]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 101), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 99, 
    104, 101, 109, 97, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 52, 54, 58, 51, 45, 49, 54, 57, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    105, 110, 100, 101, 120, 32, 111, 117, 116, 32, 111, 102, 32, 98, 
    111, 117, 110, 100, 115, 58, 32, 116, 104, 101, 32, 108, 101, 110, 
    32, 105, 115, 32, 102, 114, 111, 109, 32, 48, 32, 116, 111, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_56 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_104 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 53, 52, 58, 49, 51, 45, 49, 54, 56, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    111, 98, 106, 101, 99, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[100]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 99), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 99, 
    104, 101, 109, 97, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 116, 114, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    32, 98, 117, 116, 32, 116, 104, 101, 32, 105, 110, 100, 101, 120, 
    32, 105, 115, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    114, 101, 113, 117, 105, 114, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 52, 55, 58, 53, 45, 49, 53, 51, 58, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 50, 52, 58, 51, 45, 49, 52, 53, 58, 52, 0
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
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    96, 32, 105, 115, 32, 110, 111, 116, 32, 102, 97, 108, 115, 101, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 50, 48, 50, 58, 51, 45, 50, 48, 50, 58, 55, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    96, 32, 105, 115, 32, 110, 111, 116, 32, 116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_70 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 57, 54, 58, 51, 45, 50, 48, 49, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    83, 109, 105, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    102, 105, 114, 115, 116, 78, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_73 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    105, 110, 116, 101, 103, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    109, 105, 100, 100, 108, 101, 78, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    99, 104, 101, 109, 97, 58, 115, 99, 104, 101, 109, 97, 46, 109, 98, 
    116, 58, 49, 50, 53, 58, 53, 45, 49, 51, 49, 58, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    97, 100, 100, 105, 116, 105, 111, 110, 97, 108, 80, 114, 111, 112, 
    101, 114, 116, 105, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    105, 116, 101, 109, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    112, 114, 111, 112, 101, 114, 116, 105, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    97, 114, 114, 97, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    68, 111, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    108, 97, 115, 116, 78, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 99, 104, 101, 109, 97, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_43 =
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

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN17error__to__stringS1708$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN17error__to__stringS1708
  };

struct { int32_t rc; uint32_t meta; struct _M0TWsERPB4Json data; 
} const _M0MPC14json4Json16string_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MPC14json4Json16string_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__1_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal6schema39____test__736368656d612e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__1_2edyncall$closure.data;

struct _M0TWsERPB4Json* _M0MPC14json4Json12string_2eclo =
  (struct _M0TWsERPB4Json*)&_M0MPC14json4Json16string_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal6schema39____test__736368656d612e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__0_2edyncall$closure.data;

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
} _M0FP0123clawteam_2fclawteam_2finternal_2fschema_2fSchema_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0123clawteam_2fclawteam_2finternal_2fschema_2fSchema_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0123clawteam_2fclawteam_2finternal_2fschema_2fSchema_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1258$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1258 =
  &_M0FPB31ryu__to__string_2erecord_2f1258$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal6schema48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3824
) {
  return _M0FP48clawteam8clawteam8internal6schema33____test__736368656d612e6d6274__0();
}

void* _M0MPC14json4Json16string_2edyncall(
  struct _M0TWsERPB4Json* _M0L6_2aenvS3823,
  moonbit_string_t _M0L6stringS1000
) {
  return _M0MPC14json4Json6string(_M0L6stringS1000);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema43____test__736368656d612e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3822
) {
  return _M0FP48clawteam8clawteam8internal6schema33____test__736368656d612e6d6274__1();
}

int32_t _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1729,
  moonbit_string_t _M0L8filenameS1704,
  int32_t _M0L5indexS1707
) {
  struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699* _closure_4490;
  struct _M0TWssbEu* _M0L14handle__resultS1699;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1708;
  void* _M0L11_2atry__errS1723;
  struct moonbit_result_0 _tmp_4492;
  int32_t _handle__error__result_4493;
  int32_t _M0L6_2atmpS3810;
  void* _M0L3errS1724;
  moonbit_string_t _M0L4nameS1726;
  struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1727;
  moonbit_string_t _M0L8_2afieldS3825;
  int32_t _M0L6_2acntS4292;
  moonbit_string_t _M0L7_2anameS1728;
  #line 525 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1704);
  _closure_4490
  = (struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699*)moonbit_malloc(sizeof(struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699));
  Moonbit_object_header(_closure_4490)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699, $1) >> 2, 1, 0);
  _closure_4490->code
  = &_M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN14handle__resultS1699;
  _closure_4490->$0 = _M0L5indexS1707;
  _closure_4490->$1 = _M0L8filenameS1704;
  _M0L14handle__resultS1699 = (struct _M0TWssbEu*)_closure_4490;
  _M0L17error__to__stringS1708
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN17error__to__stringS1708$closure.data;
  moonbit_incref(_M0L12async__testsS1729);
  moonbit_incref(_M0L17error__to__stringS1708);
  moonbit_incref(_M0L8filenameS1704);
  moonbit_incref(_M0L14handle__resultS1699);
  #line 559 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _tmp_4492
  = _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__test(_M0L12async__testsS1729, _M0L8filenameS1704, _M0L5indexS1707, _M0L14handle__resultS1699, _M0L17error__to__stringS1708);
  if (_tmp_4492.tag) {
    int32_t const _M0L5_2aokS3819 = _tmp_4492.data.ok;
    _handle__error__result_4493 = _M0L5_2aokS3819;
  } else {
    void* const _M0L6_2aerrS3820 = _tmp_4492.data.err;
    moonbit_decref(_M0L12async__testsS1729);
    moonbit_decref(_M0L17error__to__stringS1708);
    moonbit_decref(_M0L8filenameS1704);
    _M0L11_2atry__errS1723 = _M0L6_2aerrS3820;
    goto join_1722;
  }
  if (_handle__error__result_4493) {
    moonbit_decref(_M0L12async__testsS1729);
    moonbit_decref(_M0L17error__to__stringS1708);
    moonbit_decref(_M0L8filenameS1704);
    _M0L6_2atmpS3810 = 1;
  } else {
    struct moonbit_result_0 _tmp_4494;
    int32_t _handle__error__result_4495;
    moonbit_incref(_M0L12async__testsS1729);
    moonbit_incref(_M0L17error__to__stringS1708);
    moonbit_incref(_M0L8filenameS1704);
    moonbit_incref(_M0L14handle__resultS1699);
    #line 562 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    _tmp_4494
    = _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1729, _M0L8filenameS1704, _M0L5indexS1707, _M0L14handle__resultS1699, _M0L17error__to__stringS1708);
    if (_tmp_4494.tag) {
      int32_t const _M0L5_2aokS3817 = _tmp_4494.data.ok;
      _handle__error__result_4495 = _M0L5_2aokS3817;
    } else {
      void* const _M0L6_2aerrS3818 = _tmp_4494.data.err;
      moonbit_decref(_M0L12async__testsS1729);
      moonbit_decref(_M0L17error__to__stringS1708);
      moonbit_decref(_M0L8filenameS1704);
      _M0L11_2atry__errS1723 = _M0L6_2aerrS3818;
      goto join_1722;
    }
    if (_handle__error__result_4495) {
      moonbit_decref(_M0L12async__testsS1729);
      moonbit_decref(_M0L17error__to__stringS1708);
      moonbit_decref(_M0L8filenameS1704);
      _M0L6_2atmpS3810 = 1;
    } else {
      struct moonbit_result_0 _tmp_4496;
      int32_t _handle__error__result_4497;
      moonbit_incref(_M0L12async__testsS1729);
      moonbit_incref(_M0L17error__to__stringS1708);
      moonbit_incref(_M0L8filenameS1704);
      moonbit_incref(_M0L14handle__resultS1699);
      #line 565 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _tmp_4496
      = _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1729, _M0L8filenameS1704, _M0L5indexS1707, _M0L14handle__resultS1699, _M0L17error__to__stringS1708);
      if (_tmp_4496.tag) {
        int32_t const _M0L5_2aokS3815 = _tmp_4496.data.ok;
        _handle__error__result_4497 = _M0L5_2aokS3815;
      } else {
        void* const _M0L6_2aerrS3816 = _tmp_4496.data.err;
        moonbit_decref(_M0L12async__testsS1729);
        moonbit_decref(_M0L17error__to__stringS1708);
        moonbit_decref(_M0L8filenameS1704);
        _M0L11_2atry__errS1723 = _M0L6_2aerrS3816;
        goto join_1722;
      }
      if (_handle__error__result_4497) {
        moonbit_decref(_M0L12async__testsS1729);
        moonbit_decref(_M0L17error__to__stringS1708);
        moonbit_decref(_M0L8filenameS1704);
        _M0L6_2atmpS3810 = 1;
      } else {
        struct moonbit_result_0 _tmp_4498;
        int32_t _handle__error__result_4499;
        moonbit_incref(_M0L12async__testsS1729);
        moonbit_incref(_M0L17error__to__stringS1708);
        moonbit_incref(_M0L8filenameS1704);
        moonbit_incref(_M0L14handle__resultS1699);
        #line 568 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        _tmp_4498
        = _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1729, _M0L8filenameS1704, _M0L5indexS1707, _M0L14handle__resultS1699, _M0L17error__to__stringS1708);
        if (_tmp_4498.tag) {
          int32_t const _M0L5_2aokS3813 = _tmp_4498.data.ok;
          _handle__error__result_4499 = _M0L5_2aokS3813;
        } else {
          void* const _M0L6_2aerrS3814 = _tmp_4498.data.err;
          moonbit_decref(_M0L12async__testsS1729);
          moonbit_decref(_M0L17error__to__stringS1708);
          moonbit_decref(_M0L8filenameS1704);
          _M0L11_2atry__errS1723 = _M0L6_2aerrS3814;
          goto join_1722;
        }
        if (_handle__error__result_4499) {
          moonbit_decref(_M0L12async__testsS1729);
          moonbit_decref(_M0L17error__to__stringS1708);
          moonbit_decref(_M0L8filenameS1704);
          _M0L6_2atmpS3810 = 1;
        } else {
          struct moonbit_result_0 _tmp_4500;
          moonbit_incref(_M0L14handle__resultS1699);
          #line 571 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
          _tmp_4500
          = _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1729, _M0L8filenameS1704, _M0L5indexS1707, _M0L14handle__resultS1699, _M0L17error__to__stringS1708);
          if (_tmp_4500.tag) {
            int32_t const _M0L5_2aokS3811 = _tmp_4500.data.ok;
            _M0L6_2atmpS3810 = _M0L5_2aokS3811;
          } else {
            void* const _M0L6_2aerrS3812 = _tmp_4500.data.err;
            _M0L11_2atry__errS1723 = _M0L6_2aerrS3812;
            goto join_1722;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3810) {
    void* _M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3821 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3821)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3821)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1723
    = _M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3821;
    goto join_1722;
  } else {
    moonbit_decref(_M0L14handle__resultS1699);
  }
  goto joinlet_4491;
  join_1722:;
  _M0L3errS1724 = _M0L11_2atry__errS1723;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1727
  = (struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1724;
  _M0L8_2afieldS3825 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1727->$0;
  _M0L6_2acntS4292
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1727)->rc;
  if (_M0L6_2acntS4292 > 1) {
    int32_t _M0L11_2anew__cntS4293 = _M0L6_2acntS4292 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1727)->rc
    = _M0L11_2anew__cntS4293;
    moonbit_incref(_M0L8_2afieldS3825);
  } else if (_M0L6_2acntS4292 == 1) {
    #line 578 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1727);
  }
  _M0L7_2anameS1728 = _M0L8_2afieldS3825;
  _M0L4nameS1726 = _M0L7_2anameS1728;
  goto join_1725;
  goto joinlet_4501;
  join_1725:;
  #line 579 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN14handle__resultS1699(_M0L14handle__resultS1699, _M0L4nameS1726, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4501:;
  joinlet_4491:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN17error__to__stringS1708(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3809,
  void* _M0L3errS1709
) {
  void* _M0L1eS1711;
  moonbit_string_t _M0L1eS1713;
  #line 548 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3809);
  switch (Moonbit_object_tag(_M0L3errS1709)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1714 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1709;
      moonbit_string_t _M0L8_2afieldS3826 = _M0L10_2aFailureS1714->$0;
      int32_t _M0L6_2acntS4294 =
        Moonbit_object_header(_M0L10_2aFailureS1714)->rc;
      moonbit_string_t _M0L4_2aeS1715;
      if (_M0L6_2acntS4294 > 1) {
        int32_t _M0L11_2anew__cntS4295 = _M0L6_2acntS4294 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1714)->rc
        = _M0L11_2anew__cntS4295;
        moonbit_incref(_M0L8_2afieldS3826);
      } else if (_M0L6_2acntS4294 == 1) {
        #line 549 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1714);
      }
      _M0L4_2aeS1715 = _M0L8_2afieldS3826;
      _M0L1eS1713 = _M0L4_2aeS1715;
      goto join_1712;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1716 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1709;
      moonbit_string_t _M0L8_2afieldS3827 = _M0L15_2aInspectErrorS1716->$0;
      int32_t _M0L6_2acntS4296 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1716)->rc;
      moonbit_string_t _M0L4_2aeS1717;
      if (_M0L6_2acntS4296 > 1) {
        int32_t _M0L11_2anew__cntS4297 = _M0L6_2acntS4296 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1716)->rc
        = _M0L11_2anew__cntS4297;
        moonbit_incref(_M0L8_2afieldS3827);
      } else if (_M0L6_2acntS4296 == 1) {
        #line 549 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1716);
      }
      _M0L4_2aeS1717 = _M0L8_2afieldS3827;
      _M0L1eS1713 = _M0L4_2aeS1717;
      goto join_1712;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1718 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1709;
      moonbit_string_t _M0L8_2afieldS3828 = _M0L16_2aSnapshotErrorS1718->$0;
      int32_t _M0L6_2acntS4298 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1718)->rc;
      moonbit_string_t _M0L4_2aeS1719;
      if (_M0L6_2acntS4298 > 1) {
        int32_t _M0L11_2anew__cntS4299 = _M0L6_2acntS4298 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1718)->rc
        = _M0L11_2anew__cntS4299;
        moonbit_incref(_M0L8_2afieldS3828);
      } else if (_M0L6_2acntS4298 == 1) {
        #line 549 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1718);
      }
      _M0L4_2aeS1719 = _M0L8_2afieldS3828;
      _M0L1eS1713 = _M0L4_2aeS1719;
      goto join_1712;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1720 =
        (struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1709;
      moonbit_string_t _M0L8_2afieldS3829 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1720->$0;
      int32_t _M0L6_2acntS4300 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1720)->rc;
      moonbit_string_t _M0L4_2aeS1721;
      if (_M0L6_2acntS4300 > 1) {
        int32_t _M0L11_2anew__cntS4301 = _M0L6_2acntS4300 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1720)->rc
        = _M0L11_2anew__cntS4301;
        moonbit_incref(_M0L8_2afieldS3829);
      } else if (_M0L6_2acntS4300 == 1) {
        #line 549 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1720);
      }
      _M0L4_2aeS1721 = _M0L8_2afieldS3829;
      _M0L1eS1713 = _M0L4_2aeS1721;
      goto join_1712;
      break;
    }
    default: {
      _M0L1eS1711 = _M0L3errS1709;
      goto join_1710;
      break;
    }
  }
  join_1712:;
  return _M0L1eS1713;
  join_1710:;
  #line 554 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1711);
}

int32_t _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__executeN14handle__resultS1699(
  struct _M0TWssbEu* _M0L6_2aenvS3795,
  moonbit_string_t _M0L8testnameS1700,
  moonbit_string_t _M0L7messageS1701,
  int32_t _M0L7skippedS1702
) {
  struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699* _M0L14_2acasted__envS3796;
  moonbit_string_t _M0L8_2afieldS3839;
  moonbit_string_t _M0L8filenameS1704;
  int32_t _M0L8_2afieldS3838;
  int32_t _M0L6_2acntS4302;
  int32_t _M0L5indexS1707;
  int32_t _if__result_4504;
  moonbit_string_t _M0L10file__nameS1703;
  moonbit_string_t _M0L10test__nameS1705;
  moonbit_string_t _M0L7messageS1706;
  moonbit_string_t _M0L6_2atmpS3808;
  moonbit_string_t _M0L6_2atmpS3837;
  moonbit_string_t _M0L6_2atmpS3807;
  moonbit_string_t _M0L6_2atmpS3836;
  moonbit_string_t _M0L6_2atmpS3805;
  moonbit_string_t _M0L6_2atmpS3806;
  moonbit_string_t _M0L6_2atmpS3835;
  moonbit_string_t _M0L6_2atmpS3804;
  moonbit_string_t _M0L6_2atmpS3834;
  moonbit_string_t _M0L6_2atmpS3802;
  moonbit_string_t _M0L6_2atmpS3803;
  moonbit_string_t _M0L6_2atmpS3833;
  moonbit_string_t _M0L6_2atmpS3801;
  moonbit_string_t _M0L6_2atmpS3832;
  moonbit_string_t _M0L6_2atmpS3799;
  moonbit_string_t _M0L6_2atmpS3800;
  moonbit_string_t _M0L6_2atmpS3831;
  moonbit_string_t _M0L6_2atmpS3798;
  moonbit_string_t _M0L6_2atmpS3830;
  moonbit_string_t _M0L6_2atmpS3797;
  #line 532 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3796
  = (struct _M0R113_24clawteam_2fclawteam_2finternal_2fschema_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1699*)_M0L6_2aenvS3795;
  _M0L8_2afieldS3839 = _M0L14_2acasted__envS3796->$1;
  _M0L8filenameS1704 = _M0L8_2afieldS3839;
  _M0L8_2afieldS3838 = _M0L14_2acasted__envS3796->$0;
  _M0L6_2acntS4302 = Moonbit_object_header(_M0L14_2acasted__envS3796)->rc;
  if (_M0L6_2acntS4302 > 1) {
    int32_t _M0L11_2anew__cntS4303 = _M0L6_2acntS4302 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3796)->rc
    = _M0L11_2anew__cntS4303;
    moonbit_incref(_M0L8filenameS1704);
  } else if (_M0L6_2acntS4302 == 1) {
    #line 532 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3796);
  }
  _M0L5indexS1707 = _M0L8_2afieldS3838;
  if (!_M0L7skippedS1702) {
    _if__result_4504 = 1;
  } else {
    _if__result_4504 = 0;
  }
  if (_if__result_4504) {
    
  }
  #line 538 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1703 = _M0MPC16string6String6escape(_M0L8filenameS1704);
  #line 539 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1705 = _M0MPC16string6String6escape(_M0L8testnameS1700);
  #line 540 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1706 = _M0MPC16string6String6escape(_M0L7messageS1701);
  #line 541 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 543 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3808
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1703);
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3837
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3808);
  moonbit_decref(_M0L6_2atmpS3808);
  _M0L6_2atmpS3807 = _M0L6_2atmpS3837;
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3836
  = moonbit_add_string(_M0L6_2atmpS3807, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3807);
  _M0L6_2atmpS3805 = _M0L6_2atmpS3836;
  #line 543 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3806
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1707);
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3835 = moonbit_add_string(_M0L6_2atmpS3805, _M0L6_2atmpS3806);
  moonbit_decref(_M0L6_2atmpS3805);
  moonbit_decref(_M0L6_2atmpS3806);
  _M0L6_2atmpS3804 = _M0L6_2atmpS3835;
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3834
  = moonbit_add_string(_M0L6_2atmpS3804, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3804);
  _M0L6_2atmpS3802 = _M0L6_2atmpS3834;
  #line 543 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3803
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1705);
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3833 = moonbit_add_string(_M0L6_2atmpS3802, _M0L6_2atmpS3803);
  moonbit_decref(_M0L6_2atmpS3802);
  moonbit_decref(_M0L6_2atmpS3803);
  _M0L6_2atmpS3801 = _M0L6_2atmpS3833;
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3832
  = moonbit_add_string(_M0L6_2atmpS3801, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3801);
  _M0L6_2atmpS3799 = _M0L6_2atmpS3832;
  #line 543 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3800
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1706);
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3831 = moonbit_add_string(_M0L6_2atmpS3799, _M0L6_2atmpS3800);
  moonbit_decref(_M0L6_2atmpS3799);
  moonbit_decref(_M0L6_2atmpS3800);
  _M0L6_2atmpS3798 = _M0L6_2atmpS3831;
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3830
  = moonbit_add_string(_M0L6_2atmpS3798, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3798);
  _M0L6_2atmpS3797 = _M0L6_2atmpS3830;
  #line 542 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3797);
  #line 545 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1698,
  moonbit_string_t _M0L8filenameS1695,
  int32_t _M0L5indexS1689,
  struct _M0TWssbEu* _M0L14handle__resultS1685,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1687
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1665;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1694;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1667;
  moonbit_string_t* _M0L5attrsS1668;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1688;
  moonbit_string_t _M0L4nameS1671;
  moonbit_string_t _M0L4nameS1669;
  int32_t _M0L6_2atmpS3794;
  struct _M0TWEOs* _M0L5_2aitS1673;
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__* _closure_4513;
  struct _M0TWEOc* _M0L6_2atmpS3785;
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__* _closure_4514;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3786;
  struct moonbit_result_0 _result_4515;
  #line 406 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1698);
  moonbit_incref(_M0FP48clawteam8clawteam8internal6schema48moonbit__test__driver__internal__no__args__tests);
  #line 413 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1694
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal6schema48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1695);
  if (_M0L7_2abindS1694 == 0) {
    struct moonbit_result_0 _result_4506;
    if (_M0L7_2abindS1694) {
      moonbit_decref(_M0L7_2abindS1694);
    }
    moonbit_decref(_M0L17error__to__stringS1687);
    moonbit_decref(_M0L14handle__resultS1685);
    _result_4506.tag = 1;
    _result_4506.data.ok = 0;
    return _result_4506;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1696 =
      _M0L7_2abindS1694;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1697 =
      _M0L7_2aSomeS1696;
    _M0L10index__mapS1665 = _M0L13_2aindex__mapS1697;
    goto join_1664;
  }
  join_1664:;
  #line 415 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1688
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1665, _M0L5indexS1689);
  if (_M0L7_2abindS1688 == 0) {
    struct moonbit_result_0 _result_4508;
    if (_M0L7_2abindS1688) {
      moonbit_decref(_M0L7_2abindS1688);
    }
    moonbit_decref(_M0L17error__to__stringS1687);
    moonbit_decref(_M0L14handle__resultS1685);
    _result_4508.tag = 1;
    _result_4508.data.ok = 0;
    return _result_4508;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1690 =
      _M0L7_2abindS1688;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1691 = _M0L7_2aSomeS1690;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3843 = _M0L4_2axS1691->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1692 = _M0L8_2afieldS3843;
    moonbit_string_t* _M0L8_2afieldS3842 = _M0L4_2axS1691->$1;
    int32_t _M0L6_2acntS4304 = Moonbit_object_header(_M0L4_2axS1691)->rc;
    moonbit_string_t* _M0L8_2aattrsS1693;
    if (_M0L6_2acntS4304 > 1) {
      int32_t _M0L11_2anew__cntS4305 = _M0L6_2acntS4304 - 1;
      Moonbit_object_header(_M0L4_2axS1691)->rc = _M0L11_2anew__cntS4305;
      moonbit_incref(_M0L8_2afieldS3842);
      moonbit_incref(_M0L4_2afS1692);
    } else if (_M0L6_2acntS4304 == 1) {
      #line 413 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1691);
    }
    _M0L8_2aattrsS1693 = _M0L8_2afieldS3842;
    _M0L1fS1667 = _M0L4_2afS1692;
    _M0L5attrsS1668 = _M0L8_2aattrsS1693;
    goto join_1666;
  }
  join_1666:;
  _M0L6_2atmpS3794 = Moonbit_array_length(_M0L5attrsS1668);
  if (_M0L6_2atmpS3794 >= 1) {
    moonbit_string_t _M0L6_2atmpS3841 = (moonbit_string_t)_M0L5attrsS1668[0];
    moonbit_string_t _M0L7_2anameS1672 = _M0L6_2atmpS3841;
    moonbit_incref(_M0L7_2anameS1672);
    _M0L4nameS1671 = _M0L7_2anameS1672;
    goto join_1670;
  } else {
    _M0L4nameS1669 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4509;
  join_1670:;
  _M0L4nameS1669 = _M0L4nameS1671;
  joinlet_4509:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1673 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1668);
  while (1) {
    moonbit_string_t _M0L4attrS1675;
    moonbit_string_t _M0L7_2abindS1682;
    int32_t _M0L6_2atmpS3778;
    int64_t _M0L6_2atmpS3777;
    moonbit_incref(_M0L5_2aitS1673);
    #line 418 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1682 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1673);
    if (_M0L7_2abindS1682 == 0) {
      if (_M0L7_2abindS1682) {
        moonbit_decref(_M0L7_2abindS1682);
      }
      moonbit_decref(_M0L5_2aitS1673);
    } else {
      moonbit_string_t _M0L7_2aSomeS1683 = _M0L7_2abindS1682;
      moonbit_string_t _M0L7_2aattrS1684 = _M0L7_2aSomeS1683;
      _M0L4attrS1675 = _M0L7_2aattrS1684;
      goto join_1674;
    }
    goto joinlet_4511;
    join_1674:;
    _M0L6_2atmpS3778 = Moonbit_array_length(_M0L4attrS1675);
    _M0L6_2atmpS3777 = (int64_t)_M0L6_2atmpS3778;
    moonbit_incref(_M0L4attrS1675);
    #line 419 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1675, 5, 0, _M0L6_2atmpS3777)
    ) {
      int32_t _M0L6_2atmpS3784 = _M0L4attrS1675[0];
      int32_t _M0L4_2axS1676 = _M0L6_2atmpS3784;
      if (_M0L4_2axS1676 == 112) {
        int32_t _M0L6_2atmpS3783 = _M0L4attrS1675[1];
        int32_t _M0L4_2axS1677 = _M0L6_2atmpS3783;
        if (_M0L4_2axS1677 == 97) {
          int32_t _M0L6_2atmpS3782 = _M0L4attrS1675[2];
          int32_t _M0L4_2axS1678 = _M0L6_2atmpS3782;
          if (_M0L4_2axS1678 == 110) {
            int32_t _M0L6_2atmpS3781 = _M0L4attrS1675[3];
            int32_t _M0L4_2axS1679 = _M0L6_2atmpS3781;
            if (_M0L4_2axS1679 == 105) {
              int32_t _M0L6_2atmpS3840 = _M0L4attrS1675[4];
              int32_t _M0L6_2atmpS3780;
              int32_t _M0L4_2axS1680;
              moonbit_decref(_M0L4attrS1675);
              _M0L6_2atmpS3780 = _M0L6_2atmpS3840;
              _M0L4_2axS1680 = _M0L6_2atmpS3780;
              if (_M0L4_2axS1680 == 99) {
                void* _M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3779;
                struct moonbit_result_0 _result_4512;
                moonbit_decref(_M0L17error__to__stringS1687);
                moonbit_decref(_M0L14handle__resultS1685);
                moonbit_decref(_M0L5_2aitS1673);
                moonbit_decref(_M0L1fS1667);
                _M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3779
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3779)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3779)->$0
                = _M0L4nameS1669;
                _result_4512.tag = 0;
                _result_4512.data.err
                = _M0L111clawteam_2fclawteam_2finternal_2fschema_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3779;
                return _result_4512;
              }
            } else {
              moonbit_decref(_M0L4attrS1675);
            }
          } else {
            moonbit_decref(_M0L4attrS1675);
          }
        } else {
          moonbit_decref(_M0L4attrS1675);
        }
      } else {
        moonbit_decref(_M0L4attrS1675);
      }
    } else {
      moonbit_decref(_M0L4attrS1675);
    }
    continue;
    joinlet_4511:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1685);
  moonbit_incref(_M0L4nameS1669);
  _closure_4513
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__*)moonbit_malloc(sizeof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__));
  Moonbit_object_header(_closure_4513)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__, $0) >> 2, 2, 0);
  _closure_4513->code
  = &_M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testC3791l426;
  _closure_4513->$0 = _M0L14handle__resultS1685;
  _closure_4513->$1 = _M0L4nameS1669;
  _M0L6_2atmpS3785 = (struct _M0TWEOc*)_closure_4513;
  _closure_4514
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__*)moonbit_malloc(sizeof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__));
  Moonbit_object_header(_closure_4514)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__, $0) >> 2, 3, 0);
  _closure_4514->code
  = &_M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testC3787l427;
  _closure_4514->$0 = _M0L17error__to__stringS1687;
  _closure_4514->$1 = _M0L14handle__resultS1685;
  _closure_4514->$2 = _M0L4nameS1669;
  _M0L6_2atmpS3786 = (struct _M0TWRPC15error5ErrorEu*)_closure_4514;
  #line 424 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal6schema45moonbit__test__driver__internal__catch__error(_M0L1fS1667, _M0L6_2atmpS3785, _M0L6_2atmpS3786);
  _result_4515.tag = 1;
  _result_4515.data.ok = 1;
  return _result_4515;
}

int32_t _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testC3791l426(
  struct _M0TWEOc* _M0L6_2aenvS3792
) {
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__* _M0L14_2acasted__envS3793;
  moonbit_string_t _M0L8_2afieldS3845;
  moonbit_string_t _M0L4nameS1669;
  struct _M0TWssbEu* _M0L8_2afieldS3844;
  int32_t _M0L6_2acntS4306;
  struct _M0TWssbEu* _M0L14handle__resultS1685;
  #line 426 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3793
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3791__l426__*)_M0L6_2aenvS3792;
  _M0L8_2afieldS3845 = _M0L14_2acasted__envS3793->$1;
  _M0L4nameS1669 = _M0L8_2afieldS3845;
  _M0L8_2afieldS3844 = _M0L14_2acasted__envS3793->$0;
  _M0L6_2acntS4306 = Moonbit_object_header(_M0L14_2acasted__envS3793)->rc;
  if (_M0L6_2acntS4306 > 1) {
    int32_t _M0L11_2anew__cntS4307 = _M0L6_2acntS4306 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3793)->rc
    = _M0L11_2anew__cntS4307;
    moonbit_incref(_M0L4nameS1669);
    moonbit_incref(_M0L8_2afieldS3844);
  } else if (_M0L6_2acntS4306 == 1) {
    #line 426 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3793);
  }
  _M0L14handle__resultS1685 = _M0L8_2afieldS3844;
  #line 426 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1685->code(_M0L14handle__resultS1685, _M0L4nameS1669, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal6schema41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testC3787l427(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3788,
  void* _M0L3errS1686
) {
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__* _M0L14_2acasted__envS3789;
  moonbit_string_t _M0L8_2afieldS3848;
  moonbit_string_t _M0L4nameS1669;
  struct _M0TWssbEu* _M0L8_2afieldS3847;
  struct _M0TWssbEu* _M0L14handle__resultS1685;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3846;
  int32_t _M0L6_2acntS4308;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1687;
  moonbit_string_t _M0L6_2atmpS3790;
  #line 427 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3789
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fschema_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3787__l427__*)_M0L6_2aenvS3788;
  _M0L8_2afieldS3848 = _M0L14_2acasted__envS3789->$2;
  _M0L4nameS1669 = _M0L8_2afieldS3848;
  _M0L8_2afieldS3847 = _M0L14_2acasted__envS3789->$1;
  _M0L14handle__resultS1685 = _M0L8_2afieldS3847;
  _M0L8_2afieldS3846 = _M0L14_2acasted__envS3789->$0;
  _M0L6_2acntS4308 = Moonbit_object_header(_M0L14_2acasted__envS3789)->rc;
  if (_M0L6_2acntS4308 > 1) {
    int32_t _M0L11_2anew__cntS4309 = _M0L6_2acntS4308 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3789)->rc
    = _M0L11_2anew__cntS4309;
    moonbit_incref(_M0L4nameS1669);
    moonbit_incref(_M0L14handle__resultS1685);
    moonbit_incref(_M0L8_2afieldS3846);
  } else if (_M0L6_2acntS4308 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3789);
  }
  _M0L17error__to__stringS1687 = _M0L8_2afieldS3846;
  #line 427 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3790
  = _M0L17error__to__stringS1687->code(_M0L17error__to__stringS1687, _M0L3errS1686);
  #line 427 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1685->code(_M0L14handle__resultS1685, _M0L4nameS1669, _M0L6_2atmpS3790, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal6schema45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1660,
  struct _M0TWEOc* _M0L6on__okS1661,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1658
) {
  void* _M0L11_2atry__errS1656;
  struct moonbit_result_0 _tmp_4517;
  void* _M0L3errS1657;
  #line 375 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _tmp_4517 = _M0L1fS1660->code(_M0L1fS1660);
  if (_tmp_4517.tag) {
    int32_t const _M0L5_2aokS3775 = _tmp_4517.data.ok;
    moonbit_decref(_M0L7on__errS1658);
  } else {
    void* const _M0L6_2aerrS3776 = _tmp_4517.data.err;
    moonbit_decref(_M0L6on__okS1661);
    _M0L11_2atry__errS1656 = _M0L6_2aerrS3776;
    goto join_1655;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1661->code(_M0L6on__okS1661);
  goto joinlet_4516;
  join_1655:;
  _M0L3errS1657 = _M0L11_2atry__errS1656;
  #line 383 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1658->code(_M0L7on__errS1658, _M0L3errS1657);
  joinlet_4516:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1615;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1628;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1633;
  struct _M0TUsiE** _M0L6_2atmpS3774;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1640;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1641;
  moonbit_string_t _M0L6_2atmpS3773;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1642;
  int32_t _M0L7_2abindS1643;
  int32_t _M0L2__S1644;
  #line 193 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1615 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1628
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1633 = 0;
  _M0L6_2atmpS3774 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1640
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1640)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1640->$0 = _M0L6_2atmpS3774;
  _M0L16file__and__indexS1640->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1641
  = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1628(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1628);
  #line 284 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3773 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1641, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1642
  = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1633(_M0L51moonbit__test__driver__internal__split__mbt__stringS1633, _M0L6_2atmpS3773, 47);
  _M0L7_2abindS1643 = _M0L10test__argsS1642->$1;
  _M0L2__S1644 = 0;
  while (1) {
    if (_M0L2__S1644 < _M0L7_2abindS1643) {
      moonbit_string_t* _M0L8_2afieldS3850 = _M0L10test__argsS1642->$0;
      moonbit_string_t* _M0L3bufS3772 = _M0L8_2afieldS3850;
      moonbit_string_t _M0L6_2atmpS3849 =
        (moonbit_string_t)_M0L3bufS3772[_M0L2__S1644];
      moonbit_string_t _M0L3argS1645 = _M0L6_2atmpS3849;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1646;
      moonbit_string_t _M0L4fileS1647;
      moonbit_string_t _M0L5rangeS1648;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1649;
      moonbit_string_t _M0L6_2atmpS3770;
      int32_t _M0L5startS1650;
      moonbit_string_t _M0L6_2atmpS3769;
      int32_t _M0L3endS1651;
      int32_t _M0L1iS1652;
      int32_t _M0L6_2atmpS3771;
      moonbit_incref(_M0L3argS1645);
      #line 288 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1646
      = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1633(_M0L51moonbit__test__driver__internal__split__mbt__stringS1633, _M0L3argS1645, 58);
      moonbit_incref(_M0L16file__and__rangeS1646);
      #line 289 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1647
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1646, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1648
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1646, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1649
      = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1633(_M0L51moonbit__test__driver__internal__split__mbt__stringS1633, _M0L5rangeS1648, 45);
      moonbit_incref(_M0L15start__and__endS1649);
      #line 294 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3770
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1649, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1650
      = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1615(_M0L45moonbit__test__driver__internal__parse__int__S1615, _M0L6_2atmpS3770);
      #line 295 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3769
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1649, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1651
      = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1615(_M0L45moonbit__test__driver__internal__parse__int__S1615, _M0L6_2atmpS3769);
      _M0L1iS1652 = _M0L5startS1650;
      while (1) {
        if (_M0L1iS1652 < _M0L3endS1651) {
          struct _M0TUsiE* _M0L8_2atupleS3767;
          int32_t _M0L6_2atmpS3768;
          moonbit_incref(_M0L4fileS1647);
          _M0L8_2atupleS3767
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3767)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3767->$0 = _M0L4fileS1647;
          _M0L8_2atupleS3767->$1 = _M0L1iS1652;
          moonbit_incref(_M0L16file__and__indexS1640);
          #line 297 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1640, _M0L8_2atupleS3767);
          _M0L6_2atmpS3768 = _M0L1iS1652 + 1;
          _M0L1iS1652 = _M0L6_2atmpS3768;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1647);
        }
        break;
      }
      _M0L6_2atmpS3771 = _M0L2__S1644 + 1;
      _M0L2__S1644 = _M0L6_2atmpS3771;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1642);
    }
    break;
  }
  return _M0L16file__and__indexS1640;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1633(
  int32_t _M0L6_2aenvS3748,
  moonbit_string_t _M0L1sS1634,
  int32_t _M0L3sepS1635
) {
  moonbit_string_t* _M0L6_2atmpS3766;
  struct _M0TPB5ArrayGsE* _M0L3resS1636;
  struct _M0TPC13ref3RefGiE* _M0L1iS1637;
  struct _M0TPC13ref3RefGiE* _M0L5startS1638;
  int32_t _M0L3valS3761;
  int32_t _M0L6_2atmpS3762;
  #line 261 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3766 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1636
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1636)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1636->$0 = _M0L6_2atmpS3766;
  _M0L3resS1636->$1 = 0;
  _M0L1iS1637
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1637)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1637->$0 = 0;
  _M0L5startS1638
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1638)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1638->$0 = 0;
  while (1) {
    int32_t _M0L3valS3749 = _M0L1iS1637->$0;
    int32_t _M0L6_2atmpS3750 = Moonbit_array_length(_M0L1sS1634);
    if (_M0L3valS3749 < _M0L6_2atmpS3750) {
      int32_t _M0L3valS3753 = _M0L1iS1637->$0;
      int32_t _M0L6_2atmpS3752;
      int32_t _M0L6_2atmpS3751;
      int32_t _M0L3valS3760;
      int32_t _M0L6_2atmpS3759;
      if (
        _M0L3valS3753 < 0
        || _M0L3valS3753 >= Moonbit_array_length(_M0L1sS1634)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3752 = _M0L1sS1634[_M0L3valS3753];
      _M0L6_2atmpS3751 = _M0L6_2atmpS3752;
      if (_M0L6_2atmpS3751 == _M0L3sepS1635) {
        int32_t _M0L3valS3755 = _M0L5startS1638->$0;
        int32_t _M0L3valS3756 = _M0L1iS1637->$0;
        moonbit_string_t _M0L6_2atmpS3754;
        int32_t _M0L3valS3758;
        int32_t _M0L6_2atmpS3757;
        moonbit_incref(_M0L1sS1634);
        #line 270 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3754
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1634, _M0L3valS3755, _M0L3valS3756);
        moonbit_incref(_M0L3resS1636);
        #line 270 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1636, _M0L6_2atmpS3754);
        _M0L3valS3758 = _M0L1iS1637->$0;
        _M0L6_2atmpS3757 = _M0L3valS3758 + 1;
        _M0L5startS1638->$0 = _M0L6_2atmpS3757;
      }
      _M0L3valS3760 = _M0L1iS1637->$0;
      _M0L6_2atmpS3759 = _M0L3valS3760 + 1;
      _M0L1iS1637->$0 = _M0L6_2atmpS3759;
      continue;
    } else {
      moonbit_decref(_M0L1iS1637);
    }
    break;
  }
  _M0L3valS3761 = _M0L5startS1638->$0;
  _M0L6_2atmpS3762 = Moonbit_array_length(_M0L1sS1634);
  if (_M0L3valS3761 < _M0L6_2atmpS3762) {
    int32_t _M0L8_2afieldS3851 = _M0L5startS1638->$0;
    int32_t _M0L3valS3764;
    int32_t _M0L6_2atmpS3765;
    moonbit_string_t _M0L6_2atmpS3763;
    moonbit_decref(_M0L5startS1638);
    _M0L3valS3764 = _M0L8_2afieldS3851;
    _M0L6_2atmpS3765 = Moonbit_array_length(_M0L1sS1634);
    #line 276 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3763
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1634, _M0L3valS3764, _M0L6_2atmpS3765);
    moonbit_incref(_M0L3resS1636);
    #line 276 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1636, _M0L6_2atmpS3763);
  } else {
    moonbit_decref(_M0L5startS1638);
    moonbit_decref(_M0L1sS1634);
  }
  return _M0L3resS1636;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1628(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621
) {
  moonbit_bytes_t* _M0L3tmpS1629;
  int32_t _M0L6_2atmpS3747;
  struct _M0TPB5ArrayGsE* _M0L3resS1630;
  int32_t _M0L1iS1631;
  #line 250 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1629
  = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3747 = Moonbit_array_length(_M0L3tmpS1629);
  #line 254 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1630 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3747);
  _M0L1iS1631 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3743 = Moonbit_array_length(_M0L3tmpS1629);
    if (_M0L1iS1631 < _M0L6_2atmpS3743) {
      moonbit_bytes_t _M0L6_2atmpS3852;
      moonbit_bytes_t _M0L6_2atmpS3745;
      moonbit_string_t _M0L6_2atmpS3744;
      int32_t _M0L6_2atmpS3746;
      if (
        _M0L1iS1631 < 0 || _M0L1iS1631 >= Moonbit_array_length(_M0L3tmpS1629)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3852 = (moonbit_bytes_t)_M0L3tmpS1629[_M0L1iS1631];
      _M0L6_2atmpS3745 = _M0L6_2atmpS3852;
      moonbit_incref(_M0L6_2atmpS3745);
      #line 256 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3744
      = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621, _M0L6_2atmpS3745);
      moonbit_incref(_M0L3resS1630);
      #line 256 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1630, _M0L6_2atmpS3744);
      _M0L6_2atmpS3746 = _M0L1iS1631 + 1;
      _M0L1iS1631 = _M0L6_2atmpS3746;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1629);
    }
    break;
  }
  return _M0L3resS1630;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1621(
  int32_t _M0L6_2aenvS3657,
  moonbit_bytes_t _M0L5bytesS1622
) {
  struct _M0TPB13StringBuilder* _M0L3resS1623;
  int32_t _M0L3lenS1624;
  struct _M0TPC13ref3RefGiE* _M0L1iS1625;
  #line 206 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1623 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1624 = Moonbit_array_length(_M0L5bytesS1622);
  _M0L1iS1625
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1625)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1625->$0 = 0;
  while (1) {
    int32_t _M0L3valS3658 = _M0L1iS1625->$0;
    if (_M0L3valS3658 < _M0L3lenS1624) {
      int32_t _M0L3valS3742 = _M0L1iS1625->$0;
      int32_t _M0L6_2atmpS3741;
      int32_t _M0L6_2atmpS3740;
      struct _M0TPC13ref3RefGiE* _M0L1cS1626;
      int32_t _M0L3valS3659;
      if (
        _M0L3valS3742 < 0
        || _M0L3valS3742 >= Moonbit_array_length(_M0L5bytesS1622)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3741 = _M0L5bytesS1622[_M0L3valS3742];
      _M0L6_2atmpS3740 = (int32_t)_M0L6_2atmpS3741;
      _M0L1cS1626
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1626)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1626->$0 = _M0L6_2atmpS3740;
      _M0L3valS3659 = _M0L1cS1626->$0;
      if (_M0L3valS3659 < 128) {
        int32_t _M0L8_2afieldS3853 = _M0L1cS1626->$0;
        int32_t _M0L3valS3661;
        int32_t _M0L6_2atmpS3660;
        int32_t _M0L3valS3663;
        int32_t _M0L6_2atmpS3662;
        moonbit_decref(_M0L1cS1626);
        _M0L3valS3661 = _M0L8_2afieldS3853;
        _M0L6_2atmpS3660 = _M0L3valS3661;
        moonbit_incref(_M0L3resS1623);
        #line 215 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1623, _M0L6_2atmpS3660);
        _M0L3valS3663 = _M0L1iS1625->$0;
        _M0L6_2atmpS3662 = _M0L3valS3663 + 1;
        _M0L1iS1625->$0 = _M0L6_2atmpS3662;
      } else {
        int32_t _M0L3valS3664 = _M0L1cS1626->$0;
        if (_M0L3valS3664 < 224) {
          int32_t _M0L3valS3666 = _M0L1iS1625->$0;
          int32_t _M0L6_2atmpS3665 = _M0L3valS3666 + 1;
          int32_t _M0L3valS3675;
          int32_t _M0L6_2atmpS3674;
          int32_t _M0L6_2atmpS3668;
          int32_t _M0L3valS3673;
          int32_t _M0L6_2atmpS3672;
          int32_t _M0L6_2atmpS3671;
          int32_t _M0L6_2atmpS3670;
          int32_t _M0L6_2atmpS3669;
          int32_t _M0L6_2atmpS3667;
          int32_t _M0L8_2afieldS3854;
          int32_t _M0L3valS3677;
          int32_t _M0L6_2atmpS3676;
          int32_t _M0L3valS3679;
          int32_t _M0L6_2atmpS3678;
          if (_M0L6_2atmpS3665 >= _M0L3lenS1624) {
            moonbit_decref(_M0L1cS1626);
            moonbit_decref(_M0L1iS1625);
            moonbit_decref(_M0L5bytesS1622);
            break;
          }
          _M0L3valS3675 = _M0L1cS1626->$0;
          _M0L6_2atmpS3674 = _M0L3valS3675 & 31;
          _M0L6_2atmpS3668 = _M0L6_2atmpS3674 << 6;
          _M0L3valS3673 = _M0L1iS1625->$0;
          _M0L6_2atmpS3672 = _M0L3valS3673 + 1;
          if (
            _M0L6_2atmpS3672 < 0
            || _M0L6_2atmpS3672 >= Moonbit_array_length(_M0L5bytesS1622)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3671 = _M0L5bytesS1622[_M0L6_2atmpS3672];
          _M0L6_2atmpS3670 = (int32_t)_M0L6_2atmpS3671;
          _M0L6_2atmpS3669 = _M0L6_2atmpS3670 & 63;
          _M0L6_2atmpS3667 = _M0L6_2atmpS3668 | _M0L6_2atmpS3669;
          _M0L1cS1626->$0 = _M0L6_2atmpS3667;
          _M0L8_2afieldS3854 = _M0L1cS1626->$0;
          moonbit_decref(_M0L1cS1626);
          _M0L3valS3677 = _M0L8_2afieldS3854;
          _M0L6_2atmpS3676 = _M0L3valS3677;
          moonbit_incref(_M0L3resS1623);
          #line 222 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1623, _M0L6_2atmpS3676);
          _M0L3valS3679 = _M0L1iS1625->$0;
          _M0L6_2atmpS3678 = _M0L3valS3679 + 2;
          _M0L1iS1625->$0 = _M0L6_2atmpS3678;
        } else {
          int32_t _M0L3valS3680 = _M0L1cS1626->$0;
          if (_M0L3valS3680 < 240) {
            int32_t _M0L3valS3682 = _M0L1iS1625->$0;
            int32_t _M0L6_2atmpS3681 = _M0L3valS3682 + 2;
            int32_t _M0L3valS3698;
            int32_t _M0L6_2atmpS3697;
            int32_t _M0L6_2atmpS3690;
            int32_t _M0L3valS3696;
            int32_t _M0L6_2atmpS3695;
            int32_t _M0L6_2atmpS3694;
            int32_t _M0L6_2atmpS3693;
            int32_t _M0L6_2atmpS3692;
            int32_t _M0L6_2atmpS3691;
            int32_t _M0L6_2atmpS3684;
            int32_t _M0L3valS3689;
            int32_t _M0L6_2atmpS3688;
            int32_t _M0L6_2atmpS3687;
            int32_t _M0L6_2atmpS3686;
            int32_t _M0L6_2atmpS3685;
            int32_t _M0L6_2atmpS3683;
            int32_t _M0L8_2afieldS3855;
            int32_t _M0L3valS3700;
            int32_t _M0L6_2atmpS3699;
            int32_t _M0L3valS3702;
            int32_t _M0L6_2atmpS3701;
            if (_M0L6_2atmpS3681 >= _M0L3lenS1624) {
              moonbit_decref(_M0L1cS1626);
              moonbit_decref(_M0L1iS1625);
              moonbit_decref(_M0L5bytesS1622);
              break;
            }
            _M0L3valS3698 = _M0L1cS1626->$0;
            _M0L6_2atmpS3697 = _M0L3valS3698 & 15;
            _M0L6_2atmpS3690 = _M0L6_2atmpS3697 << 12;
            _M0L3valS3696 = _M0L1iS1625->$0;
            _M0L6_2atmpS3695 = _M0L3valS3696 + 1;
            if (
              _M0L6_2atmpS3695 < 0
              || _M0L6_2atmpS3695 >= Moonbit_array_length(_M0L5bytesS1622)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3694 = _M0L5bytesS1622[_M0L6_2atmpS3695];
            _M0L6_2atmpS3693 = (int32_t)_M0L6_2atmpS3694;
            _M0L6_2atmpS3692 = _M0L6_2atmpS3693 & 63;
            _M0L6_2atmpS3691 = _M0L6_2atmpS3692 << 6;
            _M0L6_2atmpS3684 = _M0L6_2atmpS3690 | _M0L6_2atmpS3691;
            _M0L3valS3689 = _M0L1iS1625->$0;
            _M0L6_2atmpS3688 = _M0L3valS3689 + 2;
            if (
              _M0L6_2atmpS3688 < 0
              || _M0L6_2atmpS3688 >= Moonbit_array_length(_M0L5bytesS1622)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3687 = _M0L5bytesS1622[_M0L6_2atmpS3688];
            _M0L6_2atmpS3686 = (int32_t)_M0L6_2atmpS3687;
            _M0L6_2atmpS3685 = _M0L6_2atmpS3686 & 63;
            _M0L6_2atmpS3683 = _M0L6_2atmpS3684 | _M0L6_2atmpS3685;
            _M0L1cS1626->$0 = _M0L6_2atmpS3683;
            _M0L8_2afieldS3855 = _M0L1cS1626->$0;
            moonbit_decref(_M0L1cS1626);
            _M0L3valS3700 = _M0L8_2afieldS3855;
            _M0L6_2atmpS3699 = _M0L3valS3700;
            moonbit_incref(_M0L3resS1623);
            #line 231 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1623, _M0L6_2atmpS3699);
            _M0L3valS3702 = _M0L1iS1625->$0;
            _M0L6_2atmpS3701 = _M0L3valS3702 + 3;
            _M0L1iS1625->$0 = _M0L6_2atmpS3701;
          } else {
            int32_t _M0L3valS3704 = _M0L1iS1625->$0;
            int32_t _M0L6_2atmpS3703 = _M0L3valS3704 + 3;
            int32_t _M0L3valS3727;
            int32_t _M0L6_2atmpS3726;
            int32_t _M0L6_2atmpS3719;
            int32_t _M0L3valS3725;
            int32_t _M0L6_2atmpS3724;
            int32_t _M0L6_2atmpS3723;
            int32_t _M0L6_2atmpS3722;
            int32_t _M0L6_2atmpS3721;
            int32_t _M0L6_2atmpS3720;
            int32_t _M0L6_2atmpS3712;
            int32_t _M0L3valS3718;
            int32_t _M0L6_2atmpS3717;
            int32_t _M0L6_2atmpS3716;
            int32_t _M0L6_2atmpS3715;
            int32_t _M0L6_2atmpS3714;
            int32_t _M0L6_2atmpS3713;
            int32_t _M0L6_2atmpS3706;
            int32_t _M0L3valS3711;
            int32_t _M0L6_2atmpS3710;
            int32_t _M0L6_2atmpS3709;
            int32_t _M0L6_2atmpS3708;
            int32_t _M0L6_2atmpS3707;
            int32_t _M0L6_2atmpS3705;
            int32_t _M0L3valS3729;
            int32_t _M0L6_2atmpS3728;
            int32_t _M0L3valS3733;
            int32_t _M0L6_2atmpS3732;
            int32_t _M0L6_2atmpS3731;
            int32_t _M0L6_2atmpS3730;
            int32_t _M0L8_2afieldS3856;
            int32_t _M0L3valS3737;
            int32_t _M0L6_2atmpS3736;
            int32_t _M0L6_2atmpS3735;
            int32_t _M0L6_2atmpS3734;
            int32_t _M0L3valS3739;
            int32_t _M0L6_2atmpS3738;
            if (_M0L6_2atmpS3703 >= _M0L3lenS1624) {
              moonbit_decref(_M0L1cS1626);
              moonbit_decref(_M0L1iS1625);
              moonbit_decref(_M0L5bytesS1622);
              break;
            }
            _M0L3valS3727 = _M0L1cS1626->$0;
            _M0L6_2atmpS3726 = _M0L3valS3727 & 7;
            _M0L6_2atmpS3719 = _M0L6_2atmpS3726 << 18;
            _M0L3valS3725 = _M0L1iS1625->$0;
            _M0L6_2atmpS3724 = _M0L3valS3725 + 1;
            if (
              _M0L6_2atmpS3724 < 0
              || _M0L6_2atmpS3724 >= Moonbit_array_length(_M0L5bytesS1622)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3723 = _M0L5bytesS1622[_M0L6_2atmpS3724];
            _M0L6_2atmpS3722 = (int32_t)_M0L6_2atmpS3723;
            _M0L6_2atmpS3721 = _M0L6_2atmpS3722 & 63;
            _M0L6_2atmpS3720 = _M0L6_2atmpS3721 << 12;
            _M0L6_2atmpS3712 = _M0L6_2atmpS3719 | _M0L6_2atmpS3720;
            _M0L3valS3718 = _M0L1iS1625->$0;
            _M0L6_2atmpS3717 = _M0L3valS3718 + 2;
            if (
              _M0L6_2atmpS3717 < 0
              || _M0L6_2atmpS3717 >= Moonbit_array_length(_M0L5bytesS1622)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3716 = _M0L5bytesS1622[_M0L6_2atmpS3717];
            _M0L6_2atmpS3715 = (int32_t)_M0L6_2atmpS3716;
            _M0L6_2atmpS3714 = _M0L6_2atmpS3715 & 63;
            _M0L6_2atmpS3713 = _M0L6_2atmpS3714 << 6;
            _M0L6_2atmpS3706 = _M0L6_2atmpS3712 | _M0L6_2atmpS3713;
            _M0L3valS3711 = _M0L1iS1625->$0;
            _M0L6_2atmpS3710 = _M0L3valS3711 + 3;
            if (
              _M0L6_2atmpS3710 < 0
              || _M0L6_2atmpS3710 >= Moonbit_array_length(_M0L5bytesS1622)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3709 = _M0L5bytesS1622[_M0L6_2atmpS3710];
            _M0L6_2atmpS3708 = (int32_t)_M0L6_2atmpS3709;
            _M0L6_2atmpS3707 = _M0L6_2atmpS3708 & 63;
            _M0L6_2atmpS3705 = _M0L6_2atmpS3706 | _M0L6_2atmpS3707;
            _M0L1cS1626->$0 = _M0L6_2atmpS3705;
            _M0L3valS3729 = _M0L1cS1626->$0;
            _M0L6_2atmpS3728 = _M0L3valS3729 - 65536;
            _M0L1cS1626->$0 = _M0L6_2atmpS3728;
            _M0L3valS3733 = _M0L1cS1626->$0;
            _M0L6_2atmpS3732 = _M0L3valS3733 >> 10;
            _M0L6_2atmpS3731 = _M0L6_2atmpS3732 + 55296;
            _M0L6_2atmpS3730 = _M0L6_2atmpS3731;
            moonbit_incref(_M0L3resS1623);
            #line 242 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1623, _M0L6_2atmpS3730);
            _M0L8_2afieldS3856 = _M0L1cS1626->$0;
            moonbit_decref(_M0L1cS1626);
            _M0L3valS3737 = _M0L8_2afieldS3856;
            _M0L6_2atmpS3736 = _M0L3valS3737 & 1023;
            _M0L6_2atmpS3735 = _M0L6_2atmpS3736 + 56320;
            _M0L6_2atmpS3734 = _M0L6_2atmpS3735;
            moonbit_incref(_M0L3resS1623);
            #line 243 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1623, _M0L6_2atmpS3734);
            _M0L3valS3739 = _M0L1iS1625->$0;
            _M0L6_2atmpS3738 = _M0L3valS3739 + 4;
            _M0L1iS1625->$0 = _M0L6_2atmpS3738;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1625);
      moonbit_decref(_M0L5bytesS1622);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1623);
}

int32_t _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1615(
  int32_t _M0L6_2aenvS3650,
  moonbit_string_t _M0L1sS1616
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1617;
  int32_t _M0L3lenS1618;
  int32_t _M0L1iS1619;
  int32_t _M0L8_2afieldS3857;
  #line 197 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1617
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1617)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1617->$0 = 0;
  _M0L3lenS1618 = Moonbit_array_length(_M0L1sS1616);
  _M0L1iS1619 = 0;
  while (1) {
    if (_M0L1iS1619 < _M0L3lenS1618) {
      int32_t _M0L3valS3655 = _M0L3resS1617->$0;
      int32_t _M0L6_2atmpS3652 = _M0L3valS3655 * 10;
      int32_t _M0L6_2atmpS3654;
      int32_t _M0L6_2atmpS3653;
      int32_t _M0L6_2atmpS3651;
      int32_t _M0L6_2atmpS3656;
      if (
        _M0L1iS1619 < 0 || _M0L1iS1619 >= Moonbit_array_length(_M0L1sS1616)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3654 = _M0L1sS1616[_M0L1iS1619];
      _M0L6_2atmpS3653 = _M0L6_2atmpS3654 - 48;
      _M0L6_2atmpS3651 = _M0L6_2atmpS3652 + _M0L6_2atmpS3653;
      _M0L3resS1617->$0 = _M0L6_2atmpS3651;
      _M0L6_2atmpS3656 = _M0L1iS1619 + 1;
      _M0L1iS1619 = _M0L6_2atmpS3656;
      continue;
    } else {
      moonbit_decref(_M0L1sS1616);
    }
    break;
  }
  _M0L8_2afieldS3857 = _M0L3resS1617->$0;
  moonbit_decref(_M0L3resS1617);
  return _M0L8_2afieldS3857;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1595,
  moonbit_string_t _M0L12_2adiscard__S1596,
  int32_t _M0L12_2adiscard__S1597,
  struct _M0TWssbEu* _M0L12_2adiscard__S1598,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1599
) {
  struct moonbit_result_0 _result_4524;
  #line 34 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1599);
  moonbit_decref(_M0L12_2adiscard__S1598);
  moonbit_decref(_M0L12_2adiscard__S1596);
  moonbit_decref(_M0L12_2adiscard__S1595);
  _result_4524.tag = 1;
  _result_4524.data.ok = 0;
  return _result_4524;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1600,
  moonbit_string_t _M0L12_2adiscard__S1601,
  int32_t _M0L12_2adiscard__S1602,
  struct _M0TWssbEu* _M0L12_2adiscard__S1603,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1604
) {
  struct moonbit_result_0 _result_4525;
  #line 34 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1604);
  moonbit_decref(_M0L12_2adiscard__S1603);
  moonbit_decref(_M0L12_2adiscard__S1601);
  moonbit_decref(_M0L12_2adiscard__S1600);
  _result_4525.tag = 1;
  _result_4525.data.ok = 0;
  return _result_4525;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1605,
  moonbit_string_t _M0L12_2adiscard__S1606,
  int32_t _M0L12_2adiscard__S1607,
  struct _M0TWssbEu* _M0L12_2adiscard__S1608,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1609
) {
  struct moonbit_result_0 _result_4526;
  #line 34 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1609);
  moonbit_decref(_M0L12_2adiscard__S1608);
  moonbit_decref(_M0L12_2adiscard__S1606);
  moonbit_decref(_M0L12_2adiscard__S1605);
  _result_4526.tag = 1;
  _result_4526.data.ok = 0;
  return _result_4526;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6schema21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6schema50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1610,
  moonbit_string_t _M0L12_2adiscard__S1611,
  int32_t _M0L12_2adiscard__S1612,
  struct _M0TWssbEu* _M0L12_2adiscard__S1613,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1614
) {
  struct moonbit_result_0 _result_4527;
  #line 34 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1614);
  moonbit_decref(_M0L12_2adiscard__S1613);
  moonbit_decref(_M0L12_2adiscard__S1611);
  moonbit_decref(_M0L12_2adiscard__S1610);
  _result_4527.tag = 1;
  _result_4527.data.ok = 0;
  return _result_4527;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal6schema28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6schema34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1594
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1594);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema33____test__736368656d612e6d6274__1(
  
) {
  void* _M0L6_2atmpS3649;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3644;
  void* _M0L6_2atmpS3648;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3645;
  void* _M0L6_2atmpS3647;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3646;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1582;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3643;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3642;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3640;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3641;
  void* _M0L6_2atmpS3639;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3638;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1581;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3637;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3636;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3634;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3635;
  void* _M0L6schemaS1580;
  void* _M0L6_2atmpS3557;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3552;
  void* _M0L6_2atmpS3556;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3553;
  void* _M0L6_2atmpS3555;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3554;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1584;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3551;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3550;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3549;
  void* _M0L6_2atmpS3548;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3547;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1583;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3546;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3545;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3544;
  void* _M0L6_2atmpS3543;
  int32_t _M0L6_2atmpS3541;
  moonbit_string_t _M0L6_2atmpS3542;
  struct moonbit_result_0 _tmp_4528;
  void* _M0L6_2atmpS3575;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3571;
  moonbit_string_t _M0L6_2atmpS3574;
  void* _M0L6_2atmpS3573;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3572;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1586;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3570;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3569;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3568;
  void* _M0L6_2atmpS3567;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3566;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1585;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3565;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3564;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3563;
  void* _M0L6_2atmpS3562;
  int32_t _M0L6_2atmpS3560;
  moonbit_string_t _M0L6_2atmpS3561;
  struct moonbit_result_0 _tmp_4530;
  void* _M0L6_2atmpS3633;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3628;
  void* _M0L6_2atmpS3632;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3629;
  void* _M0L6_2atmpS3631;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3630;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1589;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3627;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3626;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3622;
  moonbit_string_t* _M0L6_2atmpS3625;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3624;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3623;
  void* _M0L6_2atmpS3621;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3618;
  void* _M0L6_2atmpS3620;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3619;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1588;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3617;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3616;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3614;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3615;
  void* _M0L6schemaS1587;
  void* _M0L6_2atmpS3595;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3592;
  void* _M0L6_2atmpS3594;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3593;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1591;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3591;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3590;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3589;
  void* _M0L6_2atmpS3588;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3584;
  moonbit_string_t _M0L6_2atmpS3587;
  void* _M0L6_2atmpS3586;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3585;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1590;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3583;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3582;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3581;
  void* _M0L6_2atmpS3580;
  int32_t _M0L6_2atmpS3578;
  moonbit_string_t _M0L6_2atmpS3579;
  struct moonbit_result_0 _tmp_4532;
  void* _M0L6_2atmpS3613;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3612;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1593;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3611;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3610;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3609;
  void* _M0L6_2atmpS3608;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3604;
  moonbit_string_t _M0L6_2atmpS3607;
  void* _M0L6_2atmpS3606;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3605;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1592;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3603;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3602;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3601;
  void* _M0L6_2atmpS3600;
  int32_t _M0L6_2atmpS3598;
  moonbit_string_t _M0L6_2atmpS3599;
  #line 173 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  #line 176 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3649 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3644
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3644)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3644->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3644->$1 = _M0L6_2atmpS3649;
  #line 177 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3648 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3645
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3645)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3645->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3645->$1 = _M0L6_2atmpS3648;
  #line 178 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3647 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3646
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3646)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3646->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3646->$1 = _M0L6_2atmpS3647;
  _M0L7_2abindS1582
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1582[0] = _M0L8_2atupleS3644;
  _M0L7_2abindS1582[1] = _M0L8_2atupleS3645;
  _M0L7_2abindS1582[2] = _M0L8_2atupleS3646;
  _M0L6_2atmpS3643 = _M0L7_2abindS1582;
  _M0L6_2atmpS3642
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 3, _M0L6_2atmpS3643
  };
  #line 175 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3640
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3642);
  _M0L6_2atmpS3641 = 0;
  #line 175 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3639
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3640, 1, _M0L6_2atmpS3641);
  _M0L8_2atupleS3638
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3638)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3638->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3638->$1 = _M0L6_2atmpS3639;
  _M0L7_2abindS1581
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1581[0] = _M0L8_2atupleS3638;
  _M0L6_2atmpS3637 = _M0L7_2abindS1581;
  _M0L6_2atmpS3636
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 1, _M0L6_2atmpS3637
  };
  #line 174 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3634
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3636);
  _M0L6_2atmpS3635 = 0;
  #line 174 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6schemaS1580
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3634, 1, _M0L6_2atmpS3635);
  #line 183 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3557
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3552
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3552)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3552->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3552->$1 = _M0L6_2atmpS3557;
  #line 183 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3556
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS3553
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3553)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3553->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3553->$1 = _M0L6_2atmpS3556;
  #line 183 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3555
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_15.data);
  _M0L8_2atupleS3554
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3554)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3554->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3554->$1 = _M0L6_2atmpS3555;
  _M0L7_2abindS1584 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1584[0] = _M0L8_2atupleS3552;
  _M0L7_2abindS1584[1] = _M0L8_2atupleS3553;
  _M0L7_2abindS1584[2] = _M0L8_2atupleS3554;
  _M0L6_2atmpS3551 = _M0L7_2abindS1584;
  _M0L6_2atmpS3550
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3551
  };
  #line 183 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3549 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3550);
  #line 183 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3548 = _M0MPC14json4Json6object(_M0L6_2atmpS3549);
  _M0L8_2atupleS3547
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3547)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3547->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3547->$1 = _M0L6_2atmpS3548;
  _M0L7_2abindS1583 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1583[0] = _M0L8_2atupleS3547;
  _M0L6_2atmpS3546 = _M0L7_2abindS1583;
  _M0L6_2atmpS3545
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3546
  };
  #line 182 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3544 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3545);
  #line 182 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3543 = _M0MPC14json4Json6object(_M0L6_2atmpS3544);
  moonbit_incref(_M0L6schemaS1580);
  #line 182 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3541
  = _M0MP48clawteam8clawteam8internal6schema6Schema6verify(_M0L6schemaS1580, _M0L6_2atmpS3543);
  _M0L6_2atmpS3542 = 0;
  #line 181 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _tmp_4528
  = _M0FPB12assert__true(_M0L6_2atmpS3541, _M0L6_2atmpS3542, (moonbit_string_t)moonbit_string_literal_16.data);
  if (_tmp_4528.tag) {
    int32_t const _M0L5_2aokS3558 = _tmp_4528.data.ok;
  } else {
    void* const _M0L6_2aerrS3559 = _tmp_4528.data.err;
    struct moonbit_result_0 _result_4529;
    moonbit_decref(_M0L6schemaS1580);
    _result_4529.tag = 0;
    _result_4529.data.err = _M0L6_2aerrS3559;
    return _result_4529;
  }
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3575
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3571
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3571)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3571->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3571->$1 = _M0L6_2atmpS3575;
  _M0L6_2atmpS3574 = 0;
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3573 = _M0MPC14json4Json6number(0x1p+0, _M0L6_2atmpS3574);
  _M0L8_2atupleS3572
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3572)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3572->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3572->$1 = _M0L6_2atmpS3573;
  _M0L7_2abindS1586 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1586[0] = _M0L8_2atupleS3571;
  _M0L7_2abindS1586[1] = _M0L8_2atupleS3572;
  _M0L6_2atmpS3570 = _M0L7_2abindS1586;
  _M0L6_2atmpS3569
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3570
  };
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3568 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3569);
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3567 = _M0MPC14json4Json6object(_M0L6_2atmpS3568);
  _M0L8_2atupleS3566
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3566)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3566->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3566->$1 = _M0L6_2atmpS3567;
  _M0L7_2abindS1585 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1585[0] = _M0L8_2atupleS3566;
  _M0L6_2atmpS3565 = _M0L7_2abindS1585;
  _M0L6_2atmpS3564
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3565
  };
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3563 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3564);
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3562 = _M0MPC14json4Json6object(_M0L6_2atmpS3563);
  #line 187 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3560
  = _M0MP48clawteam8clawteam8internal6schema6Schema6verify(_M0L6schemaS1580, _M0L6_2atmpS3562);
  _M0L6_2atmpS3561 = 0;
  #line 186 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _tmp_4530
  = _M0FPB13assert__false(_M0L6_2atmpS3560, _M0L6_2atmpS3561, (moonbit_string_t)moonbit_string_literal_17.data);
  if (_tmp_4530.tag) {
    int32_t const _M0L5_2aokS3576 = _tmp_4530.data.ok;
  } else {
    void* const _M0L6_2aerrS3577 = _tmp_4530.data.err;
    struct moonbit_result_0 _result_4531;
    _result_4531.tag = 0;
    _result_4531.data.err = _M0L6_2aerrS3577;
    return _result_4531;
  }
  #line 191 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3633 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3628
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3628)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3628->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3628->$1 = _M0L6_2atmpS3633;
  #line 191 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3632 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3629
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3629)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3629->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3629->$1 = _M0L6_2atmpS3632;
  #line 191 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3631 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3630
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3630)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3630->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3630->$1 = _M0L6_2atmpS3631;
  _M0L7_2abindS1589
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1589[0] = _M0L8_2atupleS3628;
  _M0L7_2abindS1589[1] = _M0L8_2atupleS3629;
  _M0L7_2abindS1589[2] = _M0L8_2atupleS3630;
  _M0L6_2atmpS3627 = _M0L7_2abindS1589;
  _M0L6_2atmpS3626
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 3, _M0L6_2atmpS3627
  };
  #line 191 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3622
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3626);
  _M0L6_2atmpS3625 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3625[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3625[1] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3624
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3624)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3624->$0 = _M0L6_2atmpS3625;
  _M0L6_2atmpS3624->$1 = 2;
  _M0L6_2atmpS3623 = _M0L6_2atmpS3624;
  #line 190 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3621
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3622, 1, _M0L6_2atmpS3623);
  _M0L8_2atupleS3618
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3618)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3618->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3618->$1 = _M0L6_2atmpS3621;
  #line 194 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3620 = _M0FP48clawteam8clawteam8internal6schema7integer();
  _M0L8_2atupleS3619
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3619)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3619->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3619->$1 = _M0L6_2atmpS3620;
  _M0L7_2abindS1588
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1588[0] = _M0L8_2atupleS3618;
  _M0L7_2abindS1588[1] = _M0L8_2atupleS3619;
  _M0L6_2atmpS3617 = _M0L7_2abindS1588;
  _M0L6_2atmpS3616
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 2, _M0L6_2atmpS3617
  };
  #line 189 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3614
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3616);
  _M0L6_2atmpS3615 = 0;
  #line 189 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6schemaS1587
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3614, 1, _M0L6_2atmpS3615);
  #line 198 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3595
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3592
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3592)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3592->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3592->$1 = _M0L6_2atmpS3595;
  #line 198 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3594
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS3593
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3593)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3593->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3593->$1 = _M0L6_2atmpS3594;
  _M0L7_2abindS1591 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1591[0] = _M0L8_2atupleS3592;
  _M0L7_2abindS1591[1] = _M0L8_2atupleS3593;
  _M0L6_2atmpS3591 = _M0L7_2abindS1591;
  _M0L6_2atmpS3590
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3591
  };
  #line 198 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3589 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3590);
  #line 198 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3588 = _M0MPC14json4Json6object(_M0L6_2atmpS3589);
  _M0L8_2atupleS3584
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3584)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3584->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3584->$1 = _M0L6_2atmpS3588;
  _M0L6_2atmpS3587 = 0;
  #line 199 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3586 = _M0MPC14json4Json6number(0x1.ep+4, _M0L6_2atmpS3587);
  _M0L8_2atupleS3585
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3585)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3585->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3585->$1 = _M0L6_2atmpS3586;
  _M0L7_2abindS1590 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1590[0] = _M0L8_2atupleS3584;
  _M0L7_2abindS1590[1] = _M0L8_2atupleS3585;
  _M0L6_2atmpS3583 = _M0L7_2abindS1590;
  _M0L6_2atmpS3582
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3583
  };
  #line 197 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3581 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3582);
  #line 197 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3580 = _M0MPC14json4Json6object(_M0L6_2atmpS3581);
  moonbit_incref(_M0L6schemaS1587);
  #line 197 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3578
  = _M0MP48clawteam8clawteam8internal6schema6Schema6verify(_M0L6schemaS1587, _M0L6_2atmpS3580);
  _M0L6_2atmpS3579 = 0;
  #line 196 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _tmp_4532
  = _M0FPB12assert__true(_M0L6_2atmpS3578, _M0L6_2atmpS3579, (moonbit_string_t)moonbit_string_literal_19.data);
  if (_tmp_4532.tag) {
    int32_t const _M0L5_2aokS3596 = _tmp_4532.data.ok;
  } else {
    void* const _M0L6_2aerrS3597 = _tmp_4532.data.err;
    struct moonbit_result_0 _result_4533;
    moonbit_decref(_M0L6schemaS1587);
    _result_4533.tag = 0;
    _result_4533.data.err = _M0L6_2aerrS3597;
    return _result_4533;
  }
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3613
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3612
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3612)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3612->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3612->$1 = _M0L6_2atmpS3613;
  _M0L7_2abindS1593 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1593[0] = _M0L8_2atupleS3612;
  _M0L6_2atmpS3611 = _M0L7_2abindS1593;
  _M0L6_2atmpS3610
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3611
  };
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3609 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3610);
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3608 = _M0MPC14json4Json6object(_M0L6_2atmpS3609);
  _M0L8_2atupleS3604
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3604)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3604->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3604->$1 = _M0L6_2atmpS3608;
  _M0L6_2atmpS3607 = 0;
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3606 = _M0MPC14json4Json6number(0x1.ep+4, _M0L6_2atmpS3607);
  _M0L8_2atupleS3605
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3605)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3605->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3605->$1 = _M0L6_2atmpS3606;
  _M0L7_2abindS1592 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1592[0] = _M0L8_2atupleS3604;
  _M0L7_2abindS1592[1] = _M0L8_2atupleS3605;
  _M0L6_2atmpS3603 = _M0L7_2abindS1592;
  _M0L6_2atmpS3602
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3603
  };
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3601 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3602);
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3600 = _M0MPC14json4Json6object(_M0L6_2atmpS3601);
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3598
  = _M0MP48clawteam8clawteam8internal6schema6Schema6verify(_M0L6schemaS1587, _M0L6_2atmpS3600);
  _M0L6_2atmpS3599 = 0;
  #line 202 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0FPB13assert__false(_M0L6_2atmpS3598, _M0L6_2atmpS3599, (moonbit_string_t)moonbit_string_literal_20.data);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6schema33____test__736368656d612e6d6274__0(
  
) {
  void* _M0L6_2atmpS3452;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3447;
  void* _M0L6_2atmpS3451;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3448;
  void* _M0L6_2atmpS3450;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3449;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1562;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3446;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3445;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3443;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3444;
  void* _M0L6_2atmpS3442;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3441;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1561;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3440;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3439;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3437;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3438;
  void* _M0L6_2atmpS3436;
  struct _M0TPB6ToJson _M0L6_2atmpS3384;
  void* _M0L6_2atmpS3435;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3396;
  void* _M0L6_2atmpS3434;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3407;
  void* _M0L6_2atmpS3433;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3432;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1567;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3431;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3430;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3429;
  void* _M0L6_2atmpS3428;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3413;
  void* _M0L6_2atmpS3427;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3426;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1568;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3425;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3424;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3423;
  void* _M0L6_2atmpS3422;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3414;
  void* _M0L6_2atmpS3421;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3420;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1569;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3419;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3418;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3417;
  void* _M0L6_2atmpS3416;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3415;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1566;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3412;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3411;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3410;
  void* _M0L6_2atmpS3409;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3408;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1565;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3406;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3405;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3404;
  void* _M0L6_2atmpS3403;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3402;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1564;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3401;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3400;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3399;
  void* _M0L6_2atmpS3398;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3397;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1563;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3395;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3394;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3393;
  void* _M0L6_2atmpS3392;
  void* _M0L6_2atmpS3385;
  moonbit_string_t _M0L6_2atmpS3388;
  moonbit_string_t _M0L6_2atmpS3389;
  moonbit_string_t _M0L6_2atmpS3390;
  moonbit_string_t _M0L6_2atmpS3391;
  moonbit_string_t* _M0L6_2atmpS3387;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3386;
  struct moonbit_result_0 _tmp_4534;
  void* _M0L6_2atmpS3540;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3535;
  void* _M0L6_2atmpS3539;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3536;
  void* _M0L6_2atmpS3538;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3537;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1571;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3534;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3533;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3529;
  moonbit_string_t* _M0L6_2atmpS3532;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3531;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3530;
  void* _M0L6_2atmpS3528;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3525;
  void* _M0L6_2atmpS3527;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS3526;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS1570;
  struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS3524;
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L6_2atmpS3523;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS3521;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3522;
  void* _M0L6_2atmpS3520;
  struct _M0TPB6ToJson _M0L6_2atmpS3455;
  void* _M0L6_2atmpS3519;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3467;
  void* _M0L6_2atmpS3518;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3485;
  void* _M0L6_2atmpS3517;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3516;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1576;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3515;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3514;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3513;
  void* _M0L6_2atmpS3512;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3497;
  void* _M0L6_2atmpS3511;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3510;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1577;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3509;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3508;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3507;
  void* _M0L6_2atmpS3506;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3498;
  void* _M0L6_2atmpS3505;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3504;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1578;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3503;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3502;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3501;
  void* _M0L6_2atmpS3500;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3499;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1575;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3496;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3495;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3494;
  void* _M0L6_2atmpS3493;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3486;
  void* _M0L6_2atmpS3491;
  void* _M0L6_2atmpS3492;
  void** _M0L6_2atmpS3490;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3489;
  void* _M0L6_2atmpS3488;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3487;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1574;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3484;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3483;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3482;
  void* _M0L6_2atmpS3481;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3473;
  void* _M0L6_2atmpS3480;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3479;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1579;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3478;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3477;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3476;
  void* _M0L6_2atmpS3475;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3474;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1573;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3472;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3471;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3470;
  void* _M0L6_2atmpS3469;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3468;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1572;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3466;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3465;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3464;
  void* _M0L6_2atmpS3463;
  void* _M0L6_2atmpS3456;
  moonbit_string_t _M0L6_2atmpS3459;
  moonbit_string_t _M0L6_2atmpS3460;
  moonbit_string_t _M0L6_2atmpS3461;
  moonbit_string_t _M0L6_2atmpS3462;
  moonbit_string_t* _M0L6_2atmpS3458;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3457;
  #line 123 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  #line 127 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3452 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3447
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3447)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3447->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3447->$1 = _M0L6_2atmpS3452;
  #line 128 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3451 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3448
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3448)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3448->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3448->$1 = _M0L6_2atmpS3451;
  #line 129 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3450 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3449
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3449)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3449->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3449->$1 = _M0L6_2atmpS3450;
  _M0L7_2abindS1562
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1562[0] = _M0L8_2atupleS3447;
  _M0L7_2abindS1562[1] = _M0L8_2atupleS3448;
  _M0L7_2abindS1562[2] = _M0L8_2atupleS3449;
  _M0L6_2atmpS3446 = _M0L7_2abindS1562;
  _M0L6_2atmpS3445
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 3, _M0L6_2atmpS3446
  };
  #line 126 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3443
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3445);
  _M0L6_2atmpS3444 = 0;
  #line 126 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3442
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3443, 1, _M0L6_2atmpS3444);
  _M0L8_2atupleS3441
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3441)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3441->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3441->$1 = _M0L6_2atmpS3442;
  _M0L7_2abindS1561
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1561[0] = _M0L8_2atupleS3441;
  _M0L6_2atmpS3440 = _M0L7_2abindS1561;
  _M0L6_2atmpS3439
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 1, _M0L6_2atmpS3440
  };
  #line 125 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3437
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3439);
  _M0L6_2atmpS3438 = 0;
  #line 125 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3436
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3437, 1, _M0L6_2atmpS3438);
  _M0L6_2atmpS3384
  = (struct _M0TPB6ToJson){
    _M0FP0123clawteam_2fclawteam_2finternal_2fschema_2fSchema_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3436
  };
  #line 133 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3435
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L8_2atupleS3396
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3396)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3396->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3396->$1 = _M0L6_2atmpS3435;
  #line 136 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3434
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L8_2atupleS3407
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3407)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3407->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3407->$1 = _M0L6_2atmpS3434;
  #line 138 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3433
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3432
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3432)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3432->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3432->$1 = _M0L6_2atmpS3433;
  _M0L7_2abindS1567 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1567[0] = _M0L8_2atupleS3432;
  _M0L6_2atmpS3431 = _M0L7_2abindS1567;
  _M0L6_2atmpS3430
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3431
  };
  #line 138 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3429 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3430);
  #line 138 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3428 = _M0MPC14json4Json6object(_M0L6_2atmpS3429);
  _M0L8_2atupleS3413
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3413)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3413->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3413->$1 = _M0L6_2atmpS3428;
  #line 139 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3427
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3426
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3426)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3426->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3426->$1 = _M0L6_2atmpS3427;
  _M0L7_2abindS1568 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1568[0] = _M0L8_2atupleS3426;
  _M0L6_2atmpS3425 = _M0L7_2abindS1568;
  _M0L6_2atmpS3424
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3425
  };
  #line 139 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3423 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3424);
  #line 139 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3422 = _M0MPC14json4Json6object(_M0L6_2atmpS3423);
  _M0L8_2atupleS3414
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3414)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3414->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3414->$1 = _M0L6_2atmpS3422;
  #line 140 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3421
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3420
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3420)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3420->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3420->$1 = _M0L6_2atmpS3421;
  _M0L7_2abindS1569 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1569[0] = _M0L8_2atupleS3420;
  _M0L6_2atmpS3419 = _M0L7_2abindS1569;
  _M0L6_2atmpS3418
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3419
  };
  #line 140 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3417 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3418);
  #line 140 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3416 = _M0MPC14json4Json6object(_M0L6_2atmpS3417);
  _M0L8_2atupleS3415
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3415)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3415->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3415->$1 = _M0L6_2atmpS3416;
  _M0L7_2abindS1566 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1566[0] = _M0L8_2atupleS3413;
  _M0L7_2abindS1566[1] = _M0L8_2atupleS3414;
  _M0L7_2abindS1566[2] = _M0L8_2atupleS3415;
  _M0L6_2atmpS3412 = _M0L7_2abindS1566;
  _M0L6_2atmpS3411
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3412
  };
  #line 137 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3410 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3411);
  #line 137 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3409 = _M0MPC14json4Json6object(_M0L6_2atmpS3410);
  _M0L8_2atupleS3408
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3408)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3408->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3408->$1 = _M0L6_2atmpS3409;
  _M0L7_2abindS1565 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1565[0] = _M0L8_2atupleS3407;
  _M0L7_2abindS1565[1] = _M0L8_2atupleS3408;
  _M0L6_2atmpS3406 = _M0L7_2abindS1565;
  _M0L6_2atmpS3405
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3406
  };
  #line 135 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3404 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3405);
  #line 135 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3403 = _M0MPC14json4Json6object(_M0L6_2atmpS3404);
  _M0L8_2atupleS3402
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3402)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3402->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3402->$1 = _M0L6_2atmpS3403;
  _M0L7_2abindS1564 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1564[0] = _M0L8_2atupleS3402;
  _M0L6_2atmpS3401 = _M0L7_2abindS1564;
  _M0L6_2atmpS3400
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3401
  };
  #line 134 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3399 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3400);
  #line 134 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3398 = _M0MPC14json4Json6object(_M0L6_2atmpS3399);
  _M0L8_2atupleS3397
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3397)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3397->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3397->$1 = _M0L6_2atmpS3398;
  _M0L7_2abindS1563 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1563[0] = _M0L8_2atupleS3396;
  _M0L7_2abindS1563[1] = _M0L8_2atupleS3397;
  _M0L6_2atmpS3395 = _M0L7_2abindS1563;
  _M0L6_2atmpS3394
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3395
  };
  #line 132 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3393 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3394);
  #line 132 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3392 = _M0MPC14json4Json6object(_M0L6_2atmpS3393);
  _M0L6_2atmpS3385 = _M0L6_2atmpS3392;
  _M0L6_2atmpS3388 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3389 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3390 = 0;
  _M0L6_2atmpS3391 = 0;
  _M0L6_2atmpS3387 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3387[0] = _M0L6_2atmpS3388;
  _M0L6_2atmpS3387[1] = _M0L6_2atmpS3389;
  _M0L6_2atmpS3387[2] = _M0L6_2atmpS3390;
  _M0L6_2atmpS3387[3] = _M0L6_2atmpS3391;
  _M0L6_2atmpS3386
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3386)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3386->$0 = _M0L6_2atmpS3387;
  _M0L6_2atmpS3386->$1 = 4;
  #line 124 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _tmp_4534
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3384, _M0L6_2atmpS3385, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS3386);
  if (_tmp_4534.tag) {
    int32_t const _M0L5_2aokS3453 = _tmp_4534.data.ok;
  } else {
    void* const _M0L6_2aerrS3454 = _tmp_4534.data.err;
    struct moonbit_result_0 _result_4535;
    _result_4535.tag = 0;
    _result_4535.data.err = _M0L6_2aerrS3454;
    return _result_4535;
  }
  #line 149 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3540 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3535
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3535)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3535->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3535->$1 = _M0L6_2atmpS3540;
  #line 149 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3539 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3536
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3536)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3536->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3536->$1 = _M0L6_2atmpS3539;
  #line 149 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3538 = _M0FP48clawteam8clawteam8internal6schema6string();
  _M0L8_2atupleS3537
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3537)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3537->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3537->$1 = _M0L6_2atmpS3538;
  _M0L7_2abindS1571
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1571[0] = _M0L8_2atupleS3535;
  _M0L7_2abindS1571[1] = _M0L8_2atupleS3536;
  _M0L7_2abindS1571[2] = _M0L8_2atupleS3537;
  _M0L6_2atmpS3534 = _M0L7_2abindS1571;
  _M0L6_2atmpS3533
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 3, _M0L6_2atmpS3534
  };
  #line 149 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3529
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3533);
  _M0L6_2atmpS3532 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3532[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3532[1] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3531
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3531)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3531->$0 = _M0L6_2atmpS3532;
  _M0L6_2atmpS3531->$1 = 2;
  _M0L6_2atmpS3530 = _M0L6_2atmpS3531;
  #line 148 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3528
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3529, 1, _M0L6_2atmpS3530);
  _M0L8_2atupleS3525
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3525)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3525->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3525->$1 = _M0L6_2atmpS3528;
  #line 152 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3527 = _M0FP48clawteam8clawteam8internal6schema7integer();
  _M0L8_2atupleS3526
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_M0L8_2atupleS3526)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3526->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3526->$1 = _M0L6_2atmpS3527;
  _M0L7_2abindS1570
  = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1570[0] = _M0L8_2atupleS3525;
  _M0L7_2abindS1570[1] = _M0L8_2atupleS3526;
  _M0L6_2atmpS3524 = _M0L7_2abindS1570;
  _M0L6_2atmpS3523
  = (struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE){
    0, 2, _M0L6_2atmpS3524
  };
  #line 147 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3521
  = _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS3523);
  _M0L6_2atmpS3522 = 0;
  #line 147 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3520
  = _M0FP48clawteam8clawteam8internal6schema14object_2einner(_M0L6_2atmpS3521, 1, _M0L6_2atmpS3522);
  _M0L6_2atmpS3455
  = (struct _M0TPB6ToJson){
    _M0FP0123clawteam_2fclawteam_2finternal_2fschema_2fSchema_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3520
  };
  #line 155 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3519
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L8_2atupleS3467
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3467)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3467->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3467->$1 = _M0L6_2atmpS3519;
  #line 158 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3518
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L8_2atupleS3485
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3485)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3485->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3485->$1 = _M0L6_2atmpS3518;
  #line 160 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3517
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3516
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3516)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3516->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3516->$1 = _M0L6_2atmpS3517;
  _M0L7_2abindS1576 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1576[0] = _M0L8_2atupleS3516;
  _M0L6_2atmpS3515 = _M0L7_2abindS1576;
  _M0L6_2atmpS3514
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3515
  };
  #line 160 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3513 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3514);
  #line 160 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3512 = _M0MPC14json4Json6object(_M0L6_2atmpS3513);
  _M0L8_2atupleS3497
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3497)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3497->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3497->$1 = _M0L6_2atmpS3512;
  #line 161 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3511
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3510
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3510)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3510->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3510->$1 = _M0L6_2atmpS3511;
  _M0L7_2abindS1577 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1577[0] = _M0L8_2atupleS3510;
  _M0L6_2atmpS3509 = _M0L7_2abindS1577;
  _M0L6_2atmpS3508
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3509
  };
  #line 161 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3507 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3508);
  #line 161 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3506 = _M0MPC14json4Json6object(_M0L6_2atmpS3507);
  _M0L8_2atupleS3498
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3498)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3498->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3498->$1 = _M0L6_2atmpS3506;
  #line 162 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3505
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3504
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3504)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3504->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3504->$1 = _M0L6_2atmpS3505;
  _M0L7_2abindS1578 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1578[0] = _M0L8_2atupleS3504;
  _M0L6_2atmpS3503 = _M0L7_2abindS1578;
  _M0L6_2atmpS3502
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3503
  };
  #line 162 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3501 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3502);
  #line 162 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3500 = _M0MPC14json4Json6object(_M0L6_2atmpS3501);
  _M0L8_2atupleS3499
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3499)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3499->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3499->$1 = _M0L6_2atmpS3500;
  _M0L7_2abindS1575 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1575[0] = _M0L8_2atupleS3497;
  _M0L7_2abindS1575[1] = _M0L8_2atupleS3498;
  _M0L7_2abindS1575[2] = _M0L8_2atupleS3499;
  _M0L6_2atmpS3496 = _M0L7_2abindS1575;
  _M0L6_2atmpS3495
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3496
  };
  #line 159 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3494 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3495);
  #line 159 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3493 = _M0MPC14json4Json6object(_M0L6_2atmpS3494);
  _M0L8_2atupleS3486
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3486)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3486->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3486->$1 = _M0L6_2atmpS3493;
  #line 164 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3491
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  #line 164 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3492
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L6_2atmpS3490 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3490[0] = _M0L6_2atmpS3491;
  _M0L6_2atmpS3490[1] = _M0L6_2atmpS3492;
  _M0L6_2atmpS3489
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3489)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3489->$0 = _M0L6_2atmpS3490;
  _M0L6_2atmpS3489->$1 = 2;
  #line 164 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3488 = _M0MPC14json4Json5array(_M0L6_2atmpS3489);
  _M0L8_2atupleS3487
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3487)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3487->$0 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L8_2atupleS3487->$1 = _M0L6_2atmpS3488;
  _M0L7_2abindS1574 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1574[0] = _M0L8_2atupleS3485;
  _M0L7_2abindS1574[1] = _M0L8_2atupleS3486;
  _M0L7_2abindS1574[2] = _M0L8_2atupleS3487;
  _M0L6_2atmpS3484 = _M0L7_2abindS1574;
  _M0L6_2atmpS3483
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3484
  };
  #line 157 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3482 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3483);
  #line 157 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3481 = _M0MPC14json4Json6object(_M0L6_2atmpS3482);
  _M0L8_2atupleS3473
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3473)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3473->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3473->$1 = _M0L6_2atmpS3481;
  #line 166 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3480
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_29.data);
  _M0L8_2atupleS3479
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3479)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3479->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3479->$1 = _M0L6_2atmpS3480;
  _M0L7_2abindS1579 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1579[0] = _M0L8_2atupleS3479;
  _M0L6_2atmpS3478 = _M0L7_2abindS1579;
  _M0L6_2atmpS3477
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3478
  };
  #line 166 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3476 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3477);
  #line 166 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3475 = _M0MPC14json4Json6object(_M0L6_2atmpS3476);
  _M0L8_2atupleS3474
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3474)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3474->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3474->$1 = _M0L6_2atmpS3475;
  _M0L7_2abindS1573 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1573[0] = _M0L8_2atupleS3473;
  _M0L7_2abindS1573[1] = _M0L8_2atupleS3474;
  _M0L6_2atmpS3472 = _M0L7_2abindS1573;
  _M0L6_2atmpS3471
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3472
  };
  #line 156 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3470 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3471);
  #line 156 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3469 = _M0MPC14json4Json6object(_M0L6_2atmpS3470);
  _M0L8_2atupleS3468
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3468)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3468->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3468->$1 = _M0L6_2atmpS3469;
  _M0L7_2abindS1572 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1572[0] = _M0L8_2atupleS3467;
  _M0L7_2abindS1572[1] = _M0L8_2atupleS3468;
  _M0L6_2atmpS3466 = _M0L7_2abindS1572;
  _M0L6_2atmpS3465
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3466
  };
  #line 154 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3464 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3465);
  #line 154 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3463 = _M0MPC14json4Json6object(_M0L6_2atmpS3464);
  _M0L6_2atmpS3456 = _M0L6_2atmpS3463;
  _M0L6_2atmpS3459 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3460 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3461 = 0;
  _M0L6_2atmpS3462 = 0;
  _M0L6_2atmpS3458 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3458[0] = _M0L6_2atmpS3459;
  _M0L6_2atmpS3458[1] = _M0L6_2atmpS3460;
  _M0L6_2atmpS3458[2] = _M0L6_2atmpS3461;
  _M0L6_2atmpS3458[3] = _M0L6_2atmpS3462;
  _M0L6_2atmpS3457
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3457)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3457->$0 = _M0L6_2atmpS3458;
  _M0L6_2atmpS3457->$1 = 4;
  #line 146 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3455, _M0L6_2atmpS3456, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS3457);
}

int32_t _M0MP48clawteam8clawteam8internal6schema6Schema6verify(
  void* _M0L4selfS1546,
  void* _M0L4jsonS1543
) {
  int32_t _M0L20additionalPropertiesS1508;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L3mapS1509;
  struct _M0TPB5ArrayGsE* _M0L8requiredS1510;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1511;
  void* _M0L6schemaS1538;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6valuesS1539;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6valuesS1542;
  double _M0L1nS1545;
  struct _M0TWEORPB4Json* _M0L6_2atmpS3377;
  struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__* _closure_4540;
  struct _M0TWRPB4JsonEb* _M0L6_2atmpS3378;
  moonbit_string_t* _M0L6_2atmpS3376;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3375;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1513;
  moonbit_string_t* _M0L8_2afieldS3862;
  moonbit_string_t* _M0L3bufS3373;
  int32_t _M0L8_2afieldS3861;
  int32_t _M0L6_2acntS4310;
  int32_t _M0L3lenS3374;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3372;
  struct _M0TPC111sorted__set9SortedSetGsE* _M0L8requiredS1512;
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5_2aitS1514;
  struct _M0TWEOUsRPB4JsonE* _M0L5_2aitS1529;
  #line 90 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  switch (Moonbit_object_tag(_M0L4selfS1546)) {
    case 0: {
      switch (Moonbit_object_tag(_M0L4jsonS1543)) {
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1547 =
            (struct _M0DTPB4Json6Number*)_M0L4jsonS1543;
          double _M0L8_2afieldS3863 = _M0L9_2aNumberS1547->$0;
          double _M0L4_2anS1548;
          moonbit_decref(_M0L9_2aNumberS1547);
          _M0L4_2anS1548 = _M0L8_2afieldS3863;
          _M0L1nS1545 = _M0L4_2anS1548;
          goto join_1544;
          break;
        }
        default: {
          moonbit_decref(_M0L4jsonS1543);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 1: {
      switch (Moonbit_object_tag(_M0L4jsonS1543)) {
        case 4: {
          moonbit_decref(_M0L4jsonS1543);
          return 1;
          break;
        }
        default: {
          moonbit_decref(_M0L4jsonS1543);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 4: {
      struct _M0DTP48clawteam8clawteam8internal6schema6Schema4Enum* _M0L7_2aEnumS1549 =
        (struct _M0DTP48clawteam8clawteam8internal6schema6Schema4Enum*)_M0L4selfS1546;
      struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3864 =
        _M0L7_2aEnumS1549->$0;
      int32_t _M0L6_2acntS4317 = Moonbit_object_header(_M0L7_2aEnumS1549)->rc;
      struct _M0TPB5ArrayGRPB4JsonE* _M0L9_2avaluesS1550;
      if (_M0L6_2acntS4317 > 1) {
        int32_t _M0L11_2anew__cntS4318 = _M0L6_2acntS4317 - 1;
        Moonbit_object_header(_M0L7_2aEnumS1549)->rc = _M0L11_2anew__cntS4318;
        moonbit_incref(_M0L8_2afieldS3864);
      } else if (_M0L6_2acntS4317 == 1) {
        #line 91 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L7_2aEnumS1549);
      }
      _M0L9_2avaluesS1550 = _M0L8_2afieldS3864;
      _M0L6valuesS1542 = _M0L9_2avaluesS1550;
      goto join_1541;
      break;
    }
    
    case 3: {
      struct _M0DTP48clawteam8clawteam8internal6schema6Schema5Array* _M0L8_2aArrayS1551 =
        (struct _M0DTP48clawteam8clawteam8internal6schema6Schema5Array*)_M0L4selfS1546;
      void* _M0L8_2afieldS3866 = _M0L8_2aArrayS1551->$0;
      int32_t _M0L6_2acntS4319 =
        Moonbit_object_header(_M0L8_2aArrayS1551)->rc;
      void* _M0L9_2aschemaS1552;
      if (_M0L6_2acntS4319 > 1) {
        int32_t _M0L11_2anew__cntS4320 = _M0L6_2acntS4319 - 1;
        Moonbit_object_header(_M0L8_2aArrayS1551)->rc
        = _M0L11_2anew__cntS4320;
        moonbit_incref(_M0L8_2afieldS3866);
      } else if (_M0L6_2acntS4319 == 1) {
        #line 91 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L8_2aArrayS1551);
      }
      _M0L9_2aschemaS1552 = _M0L8_2afieldS3866;
      switch (Moonbit_object_tag(_M0L4jsonS1543)) {
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1553 =
            (struct _M0DTPB4Json5Array*)_M0L4jsonS1543;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3865 =
            _M0L8_2aArrayS1553->$0;
          int32_t _M0L6_2acntS4321 =
            Moonbit_object_header(_M0L8_2aArrayS1553)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L9_2avaluesS1554;
          if (_M0L6_2acntS4321 > 1) {
            int32_t _M0L11_2anew__cntS4322 = _M0L6_2acntS4321 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1553)->rc
            = _M0L11_2anew__cntS4322;
            moonbit_incref(_M0L8_2afieldS3865);
          } else if (_M0L6_2acntS4321 == 1) {
            #line 91 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
            moonbit_free(_M0L8_2aArrayS1553);
          }
          _M0L9_2avaluesS1554 = _M0L8_2afieldS3865;
          _M0L6schemaS1538 = _M0L9_2aschemaS1552;
          _M0L6valuesS1539 = _M0L9_2avaluesS1554;
          goto join_1537;
          break;
        }
        default: {
          moonbit_decref(_M0L9_2aschemaS1552);
          moonbit_decref(_M0L4jsonS1543);
          return 0;
          break;
        }
      }
      break;
    }
    default: {
      struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object* _M0L9_2aObjectS1555 =
        (struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object*)_M0L4selfS1546;
      struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS3870 =
        _M0L9_2aObjectS1555->$0;
      struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2amapS1556 =
        _M0L8_2afieldS3870;
      struct _M0TPB5ArrayGsE* _M0L8_2afieldS3869 = _M0L9_2aObjectS1555->$1;
      struct _M0TPB5ArrayGsE* _M0L11_2arequiredS1557 = _M0L8_2afieldS3869;
      int32_t _M0L8_2afieldS3868 = _M0L9_2aObjectS1555->$2;
      int32_t _M0L6_2acntS4323 =
        Moonbit_object_header(_M0L9_2aObjectS1555)->rc;
      int32_t _M0L23_2aadditionalPropertiesS1558;
      if (_M0L6_2acntS4323 > 1) {
        int32_t _M0L11_2anew__cntS4324 = _M0L6_2acntS4323 - 1;
        Moonbit_object_header(_M0L9_2aObjectS1555)->rc
        = _M0L11_2anew__cntS4324;
        if (_M0L11_2arequiredS1557) {
          moonbit_incref(_M0L11_2arequiredS1557);
        }
        moonbit_incref(_M0L6_2amapS1556);
      } else if (_M0L6_2acntS4323 == 1) {
        #line 91 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L9_2aObjectS1555);
      }
      _M0L23_2aadditionalPropertiesS1558 = _M0L8_2afieldS3868;
      switch (Moonbit_object_tag(_M0L4jsonS1543)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1559 =
            (struct _M0DTPB4Json6Object*)_M0L4jsonS1543;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3867 =
            _M0L9_2aObjectS1559->$0;
          int32_t _M0L6_2acntS4325 =
            Moonbit_object_header(_M0L9_2aObjectS1559)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L7_2ajsonS1560;
          if (_M0L6_2acntS4325 > 1) {
            int32_t _M0L11_2anew__cntS4326 = _M0L6_2acntS4325 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1559)->rc
            = _M0L11_2anew__cntS4326;
            moonbit_incref(_M0L8_2afieldS3867);
          } else if (_M0L6_2acntS4325 == 1) {
            #line 91 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
            moonbit_free(_M0L9_2aObjectS1559);
          }
          _M0L7_2ajsonS1560 = _M0L8_2afieldS3867;
          _M0L20additionalPropertiesS1508
          = _M0L23_2aadditionalPropertiesS1558;
          _M0L3mapS1509 = _M0L6_2amapS1556;
          _M0L8requiredS1510 = _M0L11_2arequiredS1557;
          _M0L4jsonS1511 = _M0L7_2ajsonS1560;
          goto join_1507;
          break;
        }
        default: {
          if (_M0L11_2arequiredS1557) {
            moonbit_decref(_M0L11_2arequiredS1557);
          }
          moonbit_decref(_M0L6_2amapS1556);
          moonbit_decref(_M0L4jsonS1543);
          return 0;
          break;
        }
      }
      break;
    }
  }
  join_1544:;
  if (_M0L1nS1545 == _M0L1nS1545) {
    int32_t _M0L6_2atmpS3383;
    double _M0L6_2atmpS3382;
    #line 92 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L6_2atmpS3383 = _M0MPC16double6Double7to__int(_M0L1nS1545);
    _M0L6_2atmpS3382 = (double)_M0L6_2atmpS3383;
    return _M0L6_2atmpS3382 == _M0L1nS1545;
  } else {
    return 0;
  }
  join_1541:;
  #line 94 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0MPC15array5Array8containsGRPB4JsonE(_M0L6valuesS1542, _M0L4jsonS1543);
  join_1537:;
  #line 96 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3377 = _M0MPC15array5Array4iterGRPB4JsonE(_M0L6valuesS1539);
  _closure_4540
  = (struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__*)moonbit_malloc(sizeof(struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__));
  Moonbit_object_header(_closure_4540)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__, $0) >> 2, 1, 0);
  _closure_4540->code
  = &_M0MP48clawteam8clawteam8internal6schema6Schema6verifyC3379l96;
  _closure_4540->$0 = _M0L6schemaS1538;
  _M0L6_2atmpS3378 = (struct _M0TWRPB4JsonEb*)_closure_4540;
  #line 96 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0MPB4Iter3allGRPB4JsonE(_M0L6_2atmpS3377, _M0L6_2atmpS3378);
  join_1507:;
  _M0L6_2atmpS3376 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3375
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3375)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3375->$0 = _M0L6_2atmpS3376;
  _M0L6_2atmpS3375->$1 = 0;
  #line 101 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L7_2abindS1513
  = _M0MPC16option6Option10unwrap__orGRPB5ArrayGsEE(_M0L8requiredS1510, _M0L6_2atmpS3375);
  _M0L8_2afieldS3862 = _M0L7_2abindS1513->$0;
  _M0L3bufS3373 = _M0L8_2afieldS3862;
  _M0L8_2afieldS3861 = _M0L7_2abindS1513->$1;
  _M0L6_2acntS4310 = Moonbit_object_header(_M0L7_2abindS1513)->rc;
  if (_M0L6_2acntS4310 > 1) {
    int32_t _M0L11_2anew__cntS4311 = _M0L6_2acntS4310 - 1;
    Moonbit_object_header(_M0L7_2abindS1513)->rc = _M0L11_2anew__cntS4311;
    moonbit_incref(_M0L3bufS3373);
  } else if (_M0L6_2acntS4310 == 1) {
    #line 101 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    moonbit_free(_M0L7_2abindS1513);
  }
  _M0L3lenS3374 = _M0L8_2afieldS3861;
  _M0L6_2atmpS3372
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS3374, _M0L3bufS3373
  };
  #line 101 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L8requiredS1512
  = _M0MPC111sorted__set9SortedSet11from__arrayGsE(_M0L6_2atmpS3372);
  moonbit_incref(_M0L3mapS1509);
  #line 101 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L5_2aitS1514
  = _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L3mapS1509);
  while (1) {
    moonbit_string_t _M0L3keyS1516;
    void* _M0L5valueS1517;
    struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS1524;
    void* _M0L4jsonS1519;
    void* _M0L7_2abindS1520;
    int32_t _M0L6_2atmpS3370;
    moonbit_incref(_M0L5_2aitS1514);
    #line 102 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L7_2abindS1524
    = _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L5_2aitS1514);
    if (_M0L7_2abindS1524 == 0) {
      if (_M0L7_2abindS1524) {
        moonbit_decref(_M0L7_2abindS1524);
      }
      moonbit_decref(_M0L5_2aitS1514);
      moonbit_decref(_M0L8requiredS1512);
    } else {
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS1525 =
        _M0L7_2abindS1524;
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4_2axS1526 =
        _M0L7_2aSomeS1525;
      moonbit_string_t _M0L8_2afieldS3860 = _M0L4_2axS1526->$0;
      moonbit_string_t _M0L6_2akeyS1527 = _M0L8_2afieldS3860;
      void* _M0L8_2afieldS3859 = _M0L4_2axS1526->$1;
      int32_t _M0L6_2acntS4312 = Moonbit_object_header(_M0L4_2axS1526)->rc;
      void* _M0L8_2avalueS1528;
      if (_M0L6_2acntS4312 > 1) {
        int32_t _M0L11_2anew__cntS4313 = _M0L6_2acntS4312 - 1;
        Moonbit_object_header(_M0L4_2axS1526)->rc = _M0L11_2anew__cntS4313;
        moonbit_incref(_M0L8_2afieldS3859);
        moonbit_incref(_M0L6_2akeyS1527);
      } else if (_M0L6_2acntS4312 == 1) {
        #line 102 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L4_2axS1526);
      }
      _M0L8_2avalueS1528 = _M0L8_2afieldS3859;
      _M0L3keyS1516 = _M0L6_2akeyS1527;
      _M0L5valueS1517 = _M0L8_2avalueS1528;
      goto join_1515;
    }
    goto joinlet_4542;
    join_1515:;
    moonbit_incref(_M0L3keyS1516);
    moonbit_incref(_M0L4jsonS1511);
    #line 103 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L7_2abindS1520
    = _M0MPB3Map3getGsRPB4JsonE(_M0L4jsonS1511, _M0L3keyS1516);
    if (_M0L7_2abindS1520 == 0) {
      if (_M0L7_2abindS1520) {
        moonbit_decref(_M0L7_2abindS1520);
      }
      moonbit_decref(_M0L5valueS1517);
      moonbit_incref(_M0L8requiredS1512);
      #line 107 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
      if (
        _M0MPC111sorted__set9SortedSet8containsGsE(_M0L8requiredS1512, _M0L3keyS1516)
      ) {
        moonbit_decref(_M0L5_2aitS1514);
        moonbit_decref(_M0L8requiredS1512);
        moonbit_decref(_M0L4jsonS1511);
        moonbit_decref(_M0L3mapS1509);
        return 0;
      }
    } else {
      void* _M0L7_2aSomeS1521;
      void* _M0L7_2ajsonS1522;
      moonbit_decref(_M0L3keyS1516);
      _M0L7_2aSomeS1521 = _M0L7_2abindS1520;
      _M0L7_2ajsonS1522 = _M0L7_2aSomeS1521;
      _M0L4jsonS1519 = _M0L7_2ajsonS1522;
      goto join_1518;
    }
    goto joinlet_4543;
    join_1518:;
    #line 104 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L6_2atmpS3370
    = _M0MP48clawteam8clawteam8internal6schema6Schema6verify(_M0L5valueS1517, _M0L4jsonS1519);
    if (!_M0L6_2atmpS3370) {
      moonbit_decref(_M0L5_2aitS1514);
      moonbit_decref(_M0L8requiredS1512);
      moonbit_decref(_M0L4jsonS1511);
      moonbit_decref(_M0L3mapS1509);
      return 0;
    }
    joinlet_4543:;
    continue;
    joinlet_4542:;
    break;
  }
  #line 101 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L5_2aitS1529 = _M0MPB3Map5iter2GsRPB4JsonE(_M0L4jsonS1511);
  while (1) {
    moonbit_string_t _M0L3keyS1531;
    struct _M0TUsRPB4JsonE* _M0L7_2abindS1533;
    int32_t _M0L6_2atmpS3371;
    int32_t _if__result_4546;
    moonbit_incref(_M0L5_2aitS1529);
    #line 111 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L7_2abindS1533 = _M0MPB5Iter24nextGsRPB4JsonE(_M0L5_2aitS1529);
    if (_M0L7_2abindS1533 == 0) {
      if (_M0L7_2abindS1533) {
        moonbit_decref(_M0L7_2abindS1533);
      }
      moonbit_decref(_M0L5_2aitS1529);
      moonbit_decref(_M0L3mapS1509);
    } else {
      struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1534 = _M0L7_2abindS1533;
      struct _M0TUsRPB4JsonE* _M0L4_2axS1535 = _M0L7_2aSomeS1534;
      moonbit_string_t _M0L8_2afieldS3858 = _M0L4_2axS1535->$0;
      int32_t _M0L6_2acntS4314 = Moonbit_object_header(_M0L4_2axS1535)->rc;
      moonbit_string_t _M0L6_2akeyS1536;
      if (_M0L6_2acntS4314 > 1) {
        int32_t _M0L11_2anew__cntS4316 = _M0L6_2acntS4314 - 1;
        Moonbit_object_header(_M0L4_2axS1535)->rc = _M0L11_2anew__cntS4316;
        moonbit_incref(_M0L8_2afieldS3858);
      } else if (_M0L6_2acntS4314 == 1) {
        void* _M0L8_2afieldS4315 = _M0L4_2axS1535->$1;
        moonbit_decref(_M0L8_2afieldS4315);
        #line 111 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L4_2axS1535);
      }
      _M0L6_2akeyS1536 = _M0L8_2afieldS3858;
      _M0L3keyS1531 = _M0L6_2akeyS1536;
      goto join_1530;
    }
    goto joinlet_4545;
    join_1530:;
    moonbit_incref(_M0L3mapS1509);
    #line 112 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L6_2atmpS3371
    = _M0MPB3Map8containsGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L3mapS1509, _M0L3keyS1531);
    if (!_M0L6_2atmpS3371) {
      _if__result_4546 = !_M0L20additionalPropertiesS1508;
    } else {
      _if__result_4546 = 0;
    }
    if (_if__result_4546) {
      moonbit_decref(_M0L5_2aitS1529);
      moonbit_decref(_M0L3mapS1509);
      return 0;
    }
    continue;
    joinlet_4545:;
    break;
  }
  return 1;
}

int32_t _M0MP48clawteam8clawteam8internal6schema6Schema6verifyC3379l96(
  struct _M0TWRPB4JsonEb* _M0L6_2aenvS3380,
  void* _M0L5valueS1540
) {
  struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__* _M0L14_2acasted__envS3381;
  void* _M0L8_2afieldS3871;
  int32_t _M0L6_2acntS4327;
  void* _M0L6schemaS1538;
  #line 96 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L14_2acasted__envS3381
  = (struct _M0R84_40clawteam_2fclawteam_2finternal_2fschema_2eSchema_3a_3averify_2eanon__u3379__l96__*)_M0L6_2aenvS3380;
  _M0L8_2afieldS3871 = _M0L14_2acasted__envS3381->$0;
  _M0L6_2acntS4327 = Moonbit_object_header(_M0L14_2acasted__envS3381)->rc;
  if (_M0L6_2acntS4327 > 1) {
    int32_t _M0L11_2anew__cntS4328 = _M0L6_2acntS4327 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3381)->rc
    = _M0L11_2anew__cntS4328;
    moonbit_incref(_M0L8_2afieldS3871);
  } else if (_M0L6_2acntS4327 == 1) {
    #line 96 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    moonbit_free(_M0L14_2acasted__envS3381);
  }
  _M0L6schemaS1538 = _M0L8_2afieldS3871;
  #line 96 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0MP48clawteam8clawteam8internal6schema6Schema6verify(_M0L6schemaS1538, _M0L5valueS1540);
}

void* _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson8to__json(
  void* _M0L4selfS1506
) {
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3369;
  #line 85 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  #line 86 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3369
  = _M0MP48clawteam8clawteam8internal6schema6Schema8to__json(_M0L4selfS1506);
  #line 86 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0IPB3MapPB6ToJson8to__jsonGsRPB4JsonE(_M0L6_2atmpS3369);
}

struct _M0TPB3MapGsRPB4JsonE* _M0MP48clawteam8clawteam8internal6schema6Schema8to__json(
  void* _M0L4selfS1495
) {
  struct _M0TPB5ArrayGsE* _M0L8requiredS1469;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L3mapS1470;
  int32_t _M0L22additional__propertiesS1471;
  void* _M0L6schemaS1490;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6valuesS1493;
  void* _M0L6_2atmpS3360;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3359;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1494;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3358;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3357;
  void* _M0L6_2atmpS3356;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3353;
  void* _M0L6_2atmpS3355;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3354;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1491;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3352;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3351;
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L12json__objectS1472;
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5_2aitS1473;
  void* _M0L6_2atmpS3350;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3347;
  void* _M0L6_2atmpS3349;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3348;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1484;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3346;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3345;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6resultS1483;
  struct _M0TPB5ArrayGsE* _M0L8requiredS1486;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3343;
  void* _M0L6_2atmpS3342;
  #line 58 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  switch (Moonbit_object_tag(_M0L4selfS1495)) {
    case 0: {
      void* _M0L6_2atmpS3368;
      struct _M0TUsRPB4JsonE* _M0L8_2atupleS3367;
      struct _M0TUsRPB4JsonE** _M0L7_2abindS1496;
      struct _M0TUsRPB4JsonE** _M0L6_2atmpS3366;
      struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3365;
      #line 60 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
      _M0L6_2atmpS3368
      = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_29.data);
      _M0L8_2atupleS3367
      = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
      Moonbit_object_header(_M0L8_2atupleS3367)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
      _M0L8_2atupleS3367->$0
      = (moonbit_string_t)moonbit_string_literal_22.data;
      _M0L8_2atupleS3367->$1 = _M0L6_2atmpS3368;
      _M0L7_2abindS1496
      = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
      _M0L7_2abindS1496[0] = _M0L8_2atupleS3367;
      _M0L6_2atmpS3366 = _M0L7_2abindS1496;
      _M0L6_2atmpS3365
      = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
        0, 1, _M0L6_2atmpS3366
      };
      #line 60 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
      return _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3365);
      break;
    }
    
    case 1: {
      void* _M0L6_2atmpS3364;
      struct _M0TUsRPB4JsonE* _M0L8_2atupleS3363;
      struct _M0TUsRPB4JsonE** _M0L7_2abindS1497;
      struct _M0TUsRPB4JsonE** _M0L6_2atmpS3362;
      struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3361;
      #line 61 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
      _M0L6_2atmpS3364
      = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
      _M0L8_2atupleS3363
      = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
      Moonbit_object_header(_M0L8_2atupleS3363)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
      _M0L8_2atupleS3363->$0
      = (moonbit_string_t)moonbit_string_literal_22.data;
      _M0L8_2atupleS3363->$1 = _M0L6_2atmpS3364;
      _M0L7_2abindS1497
      = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
      _M0L7_2abindS1497[0] = _M0L8_2atupleS3363;
      _M0L6_2atmpS3362 = _M0L7_2abindS1497;
      _M0L6_2atmpS3361
      = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
        0, 1, _M0L6_2atmpS3362
      };
      #line 61 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
      return _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3361);
      break;
    }
    
    case 4: {
      struct _M0DTP48clawteam8clawteam8internal6schema6Schema4Enum* _M0L7_2aEnumS1498 =
        (struct _M0DTP48clawteam8clawteam8internal6schema6Schema4Enum*)_M0L4selfS1495;
      struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3874 =
        _M0L7_2aEnumS1498->$0;
      int32_t _M0L6_2acntS4331 = Moonbit_object_header(_M0L7_2aEnumS1498)->rc;
      struct _M0TPB5ArrayGRPB4JsonE* _M0L9_2avaluesS1499;
      if (_M0L6_2acntS4331 > 1) {
        int32_t _M0L11_2anew__cntS4332 = _M0L6_2acntS4331 - 1;
        Moonbit_object_header(_M0L7_2aEnumS1498)->rc = _M0L11_2anew__cntS4332;
        moonbit_incref(_M0L8_2afieldS3874);
      } else if (_M0L6_2acntS4331 == 1) {
        #line 59 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L7_2aEnumS1498);
      }
      _M0L9_2avaluesS1499 = _M0L8_2afieldS3874;
      _M0L6valuesS1493 = _M0L9_2avaluesS1499;
      goto join_1492;
      break;
    }
    
    case 3: {
      struct _M0DTP48clawteam8clawteam8internal6schema6Schema5Array* _M0L8_2aArrayS1500 =
        (struct _M0DTP48clawteam8clawteam8internal6schema6Schema5Array*)_M0L4selfS1495;
      void* _M0L8_2afieldS3875 = _M0L8_2aArrayS1500->$0;
      int32_t _M0L6_2acntS4333 =
        Moonbit_object_header(_M0L8_2aArrayS1500)->rc;
      void* _M0L9_2aschemaS1501;
      if (_M0L6_2acntS4333 > 1) {
        int32_t _M0L11_2anew__cntS4334 = _M0L6_2acntS4333 - 1;
        Moonbit_object_header(_M0L8_2aArrayS1500)->rc
        = _M0L11_2anew__cntS4334;
        moonbit_incref(_M0L8_2afieldS3875);
      } else if (_M0L6_2acntS4333 == 1) {
        #line 59 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L8_2aArrayS1500);
      }
      _M0L9_2aschemaS1501 = _M0L8_2afieldS3875;
      _M0L6schemaS1490 = _M0L9_2aschemaS1501;
      goto join_1489;
      break;
    }
    default: {
      struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object* _M0L9_2aObjectS1502 =
        (struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object*)_M0L4selfS1495;
      struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS3878 =
        _M0L9_2aObjectS1502->$0;
      struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2amapS1503 =
        _M0L8_2afieldS3878;
      struct _M0TPB5ArrayGsE* _M0L8_2afieldS3877 = _M0L9_2aObjectS1502->$1;
      struct _M0TPB5ArrayGsE* _M0L11_2arequiredS1504 = _M0L8_2afieldS3877;
      int32_t _M0L8_2afieldS3876 = _M0L9_2aObjectS1502->$2;
      int32_t _M0L6_2acntS4335 =
        Moonbit_object_header(_M0L9_2aObjectS1502)->rc;
      int32_t _M0L25_2aadditional__propertiesS1505;
      if (_M0L6_2acntS4335 > 1) {
        int32_t _M0L11_2anew__cntS4336 = _M0L6_2acntS4335 - 1;
        Moonbit_object_header(_M0L9_2aObjectS1502)->rc
        = _M0L11_2anew__cntS4336;
        if (_M0L11_2arequiredS1504) {
          moonbit_incref(_M0L11_2arequiredS1504);
        }
        moonbit_incref(_M0L6_2amapS1503);
      } else if (_M0L6_2acntS4335 == 1) {
        #line 59 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L9_2aObjectS1502);
      }
      _M0L25_2aadditional__propertiesS1505 = _M0L8_2afieldS3876;
      _M0L8requiredS1469 = _M0L11_2arequiredS1504;
      _M0L3mapS1470 = _M0L6_2amapS1503;
      _M0L22additional__propertiesS1471
      = _M0L25_2aadditional__propertiesS1505;
      goto join_1468;
      break;
    }
  }
  join_1492:;
  #line 62 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3360 = _M0MPC14json4Json5array(_M0L6valuesS1493);
  _M0L8_2atupleS3359
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3359)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3359->$0 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L8_2atupleS3359->$1 = _M0L6_2atmpS3360;
  _M0L7_2abindS1494 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1494[0] = _M0L8_2atupleS3359;
  _M0L6_2atmpS3358 = _M0L7_2abindS1494;
  _M0L6_2atmpS3357
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3358
  };
  #line 62 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3357);
  join_1489:;
  #line 63 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3356
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_34.data);
  _M0L8_2atupleS3353
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3353)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3353->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3353->$1 = _M0L6_2atmpS3356;
  #line 63 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3355
  = _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson8to__json(_M0L6schemaS1490);
  _M0L8_2atupleS3354
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3354)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3354->$0 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L8_2atupleS3354->$1 = _M0L6_2atmpS3355;
  _M0L7_2abindS1491 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1491[0] = _M0L8_2atupleS3353;
  _M0L7_2abindS1491[1] = _M0L8_2atupleS3354;
  _M0L6_2atmpS3352 = _M0L7_2abindS1491;
  _M0L6_2atmpS3351
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3352
  };
  #line 63 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3351);
  join_1468:;
  #line 65 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L12json__objectS1472 = _M0MPB3Map11new_2einnerGsRPB3MapGsRPB4JsonEE(8);
  #line 65 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L5_2aitS1473
  = _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L3mapS1470);
  while (1) {
    moonbit_string_t _M0L3keyS1475;
    void* _M0L5valueS1476;
    struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS1478;
    struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3341;
    moonbit_incref(_M0L5_2aitS1473);
    #line 66 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L7_2abindS1478
    = _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L5_2aitS1473);
    if (_M0L7_2abindS1478 == 0) {
      if (_M0L7_2abindS1478) {
        moonbit_decref(_M0L7_2abindS1478);
      }
      moonbit_decref(_M0L5_2aitS1473);
    } else {
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS1479 =
        _M0L7_2abindS1478;
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4_2axS1480 =
        _M0L7_2aSomeS1479;
      moonbit_string_t _M0L8_2afieldS3873 = _M0L4_2axS1480->$0;
      moonbit_string_t _M0L6_2akeyS1481 = _M0L8_2afieldS3873;
      void* _M0L8_2afieldS3872 = _M0L4_2axS1480->$1;
      int32_t _M0L6_2acntS4329 = Moonbit_object_header(_M0L4_2axS1480)->rc;
      void* _M0L8_2avalueS1482;
      if (_M0L6_2acntS4329 > 1) {
        int32_t _M0L11_2anew__cntS4330 = _M0L6_2acntS4329 - 1;
        Moonbit_object_header(_M0L4_2axS1480)->rc = _M0L11_2anew__cntS4330;
        moonbit_incref(_M0L8_2afieldS3872);
        moonbit_incref(_M0L6_2akeyS1481);
      } else if (_M0L6_2acntS4329 == 1) {
        #line 66 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
        moonbit_free(_M0L4_2axS1480);
      }
      _M0L8_2avalueS1482 = _M0L8_2afieldS3872;
      _M0L3keyS1475 = _M0L6_2akeyS1481;
      _M0L5valueS1476 = _M0L8_2avalueS1482;
      goto join_1474;
    }
    goto joinlet_4551;
    join_1474:;
    #line 67 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L6_2atmpS3341
    = _M0MP48clawteam8clawteam8internal6schema6Schema8to__json(_M0L5valueS1476);
    moonbit_incref(_M0L12json__objectS1472);
    #line 67 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0MPB3Map3setGsRPB3MapGsRPB4JsonEE(_M0L12json__objectS1472, _M0L3keyS1475, _M0L6_2atmpS3341);
    continue;
    joinlet_4551:;
    break;
  }
  #line 70 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3350
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L8_2atupleS3347
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3347)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3347->$0 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L8_2atupleS3347->$1 = _M0L6_2atmpS3350;
  #line 71 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3349
  = _M0IPB3MapPB6ToJson8to__jsonGsRPB3MapGsRPB4JsonEE(_M0L12json__objectS1472);
  _M0L8_2atupleS3348
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3348)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3348->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3348->$1 = _M0L6_2atmpS3349;
  _M0L7_2abindS1484 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1484[0] = _M0L8_2atupleS3347;
  _M0L7_2abindS1484[1] = _M0L8_2atupleS3348;
  _M0L6_2atmpS3346 = _M0L7_2abindS1484;
  _M0L6_2atmpS3345
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3346
  };
  #line 69 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6resultS1483 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3345);
  if (_M0L8requiredS1469 == 0) {
    if (_M0L8requiredS1469) {
      moonbit_decref(_M0L8requiredS1469);
    }
  } else {
    struct _M0TPB5ArrayGsE* _M0L7_2aSomeS1487 = _M0L8requiredS1469;
    struct _M0TPB5ArrayGsE* _M0L11_2arequiredS1488 = _M0L7_2aSomeS1487;
    _M0L8requiredS1486 = _M0L11_2arequiredS1488;
    goto join_1485;
  }
  goto joinlet_4552;
  join_1485:;
  moonbit_incref(_M0MPC14json4Json12string_2eclo);
  #line 74 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3343
  = _M0MPC15array5Array3mapGsRPB4JsonE(_M0L8requiredS1486, _M0MPC14json4Json12string_2eclo);
  #line 74 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0L6_2atmpS3342 = _M0MPC14json4Json5array(_M0L6_2atmpS3343);
  moonbit_incref(_M0L6resultS1483);
  #line 74 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6resultS1483, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS3342);
  joinlet_4552:;
  if (!_M0L22additional__propertiesS1471) {
    void* _M0L6_2atmpS3344;
    #line 77 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0L6_2atmpS3344 = _M0MPC14json4Json7boolean(0);
    moonbit_incref(_M0L6resultS1483);
    #line 77 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
    _M0MPB3Map3setGsRPB4JsonE(_M0L6resultS1483, (moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS3344);
  }
  return _M0L6resultS1483;
}

void* _M0FP48clawteam8clawteam8internal6schema14object_2einner(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6objectS1465,
  int32_t _M0L22additional__propertiesS1467,
  struct _M0TPB5ArrayGsE* _M0L8requiredS1466
) {
  void* _block_4553;
  #line 39 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  _block_4553
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object));
  Moonbit_object_header(_block_4553)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object, $0) >> 2, 2, 2);
  ((struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object*)_block_4553)->$0
  = _M0L6objectS1465;
  ((struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object*)_block_4553)->$1
  = _M0L8requiredS1466;
  ((struct _M0DTP48clawteam8clawteam8internal6schema6Schema6Object*)_block_4553)->$2
  = _M0L22additional__propertiesS1467;
  return _block_4553;
}

void* _M0FP48clawteam8clawteam8internal6schema6string() {
  #line 34 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return (struct moonbit_object*)&moonbit_constant_constructor_1 + 1;
}

void* _M0FP48clawteam8clawteam8internal6schema7integer() {
  #line 29 "E:\\moonbit\\clawteam\\internal\\schema\\schema.mbt"
  return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
}

int32_t _M0MPC111sorted__set9SortedSet8containsGsE(
  struct _M0TPC111sorted__set9SortedSetGsE* _M0L4selfS1458,
  moonbit_string_t _M0L5valueS1463
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3882;
  int32_t _M0L6_2acntS4337;
  struct _M0TPC111sorted__set4NodeGsE* _M0L7_2abindS1457;
  struct _M0TPC111sorted__set4NodeGsE* _M0L11_2aparam__0S1459;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3882 = _M0L4selfS1458->$0;
  _M0L6_2acntS4337 = Moonbit_object_header(_M0L4selfS1458)->rc;
  if (_M0L6_2acntS4337 > 1) {
    int32_t _M0L11_2anew__cntS4338 = _M0L6_2acntS4337 - 1;
    Moonbit_object_header(_M0L4selfS1458)->rc = _M0L11_2anew__cntS4338;
    if (_M0L8_2afieldS3882) {
      moonbit_incref(_M0L8_2afieldS3882);
    }
  } else if (_M0L6_2acntS4337 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
    moonbit_free(_M0L4selfS1458);
  }
  _M0L7_2abindS1457 = _M0L8_2afieldS3882;
  _M0L11_2aparam__0S1459 = _M0L7_2abindS1457;
  while (1) {
    if (_M0L11_2aparam__0S1459 == 0) {
      moonbit_decref(_M0L5valueS1463);
      if (_M0L11_2aparam__0S1459) {
        moonbit_decref(_M0L11_2aparam__0S1459);
      }
      return 0;
    } else {
      struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS1460 =
        _M0L11_2aparam__0S1459;
      struct _M0TPC111sorted__set4NodeGsE* _M0L7_2anodeS1461 =
        _M0L7_2aSomeS1460;
      moonbit_string_t _M0L8_2afieldS3881 = _M0L7_2anodeS1461->$0;
      moonbit_string_t _M0L5valueS3340 = _M0L8_2afieldS3881;
      int32_t _M0L15compare__resultS1462;
      moonbit_incref(_M0L5valueS3340);
      moonbit_incref(_M0L5valueS1463);
      #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      _M0L15compare__resultS1462
      = _M0IPC16string6StringPB7Compare7compare(_M0L5valueS1463, _M0L5valueS3340);
      if (_M0L15compare__resultS1462 == 0) {
        moonbit_decref(_M0L5valueS1463);
        moonbit_decref(_M0L7_2anodeS1461);
        return 1;
      } else if (_M0L15compare__resultS1462 < 0) {
        struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3879 =
          _M0L7_2anodeS1461->$1;
        int32_t _M0L6_2acntS4339 =
          Moonbit_object_header(_M0L7_2anodeS1461)->rc;
        struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS3338;
        if (_M0L6_2acntS4339 > 1) {
          int32_t _M0L11_2anew__cntS4342 = _M0L6_2acntS4339 - 1;
          Moonbit_object_header(_M0L7_2anodeS1461)->rc
          = _M0L11_2anew__cntS4342;
          if (_M0L8_2afieldS3879) {
            moonbit_incref(_M0L8_2afieldS3879);
          }
        } else if (_M0L6_2acntS4339 == 1) {
          struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS4341 =
            _M0L7_2anodeS1461->$2;
          moonbit_string_t _M0L8_2afieldS4340;
          if (_M0L8_2afieldS4341) {
            moonbit_decref(_M0L8_2afieldS4341);
          }
          _M0L8_2afieldS4340 = _M0L7_2anodeS1461->$0;
          moonbit_decref(_M0L8_2afieldS4340);
          #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
          moonbit_free(_M0L7_2anodeS1461);
        }
        _M0L4leftS3338 = _M0L8_2afieldS3879;
        _M0L11_2aparam__0S1459 = _M0L4leftS3338;
        continue;
      } else {
        struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3880 =
          _M0L7_2anodeS1461->$2;
        int32_t _M0L6_2acntS4343 =
          Moonbit_object_header(_M0L7_2anodeS1461)->rc;
        struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS3339;
        if (_M0L6_2acntS4343 > 1) {
          int32_t _M0L11_2anew__cntS4346 = _M0L6_2acntS4343 - 1;
          Moonbit_object_header(_M0L7_2anodeS1461)->rc
          = _M0L11_2anew__cntS4346;
          if (_M0L8_2afieldS3880) {
            moonbit_incref(_M0L8_2afieldS3880);
          }
        } else if (_M0L6_2acntS4343 == 1) {
          struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS4345 =
            _M0L7_2anodeS1461->$1;
          moonbit_string_t _M0L8_2afieldS4344;
          if (_M0L8_2afieldS4345) {
            moonbit_decref(_M0L8_2afieldS4345);
          }
          _M0L8_2afieldS4344 = _M0L7_2anodeS1461->$0;
          moonbit_decref(_M0L8_2afieldS4344);
          #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
          moonbit_free(_M0L7_2anodeS1461);
        }
        _M0L5rightS3339 = _M0L8_2afieldS3880;
        _M0L11_2aparam__0S1459 = _M0L5rightS3339;
        continue;
      }
    }
    break;
  }
}

struct _M0TPC111sorted__set9SortedSetGsE* _M0MPC111sorted__set9SortedSet11from__arrayGsE(
  struct _M0TPB9ArrayViewGsE _M0L5arrayS1454
) {
  struct _M0TPC111sorted__set9SortedSetGsE* _M0L3setS1452;
  int32_t _M0L7_2abindS1453;
  int32_t _M0L1iS1455;
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L3setS1452 = _M0MPC111sorted__set9SortedSet3newGsE();
  moonbit_incref(_M0L5arrayS1454.$0);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L7_2abindS1453 = _M0MPC15array9ArrayView6lengthGsE(_M0L5arrayS1454);
  _M0L1iS1455 = 0;
  while (1) {
    if (_M0L1iS1455 < _M0L7_2abindS1453) {
      moonbit_string_t _M0L6_2atmpS3336;
      int32_t _M0L6_2atmpS3337;
      moonbit_incref(_M0L5arrayS1454.$0);
      #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      _M0L6_2atmpS3336
      = _M0MPC15array9ArrayView2atGsE(_M0L5arrayS1454, _M0L1iS1455);
      moonbit_incref(_M0L3setS1452);
      #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      _M0MPC111sorted__set9SortedSet3addGsE(_M0L3setS1452, _M0L6_2atmpS3336);
      _M0L6_2atmpS3337 = _M0L1iS1455 + 1;
      _M0L1iS1455 = _M0L6_2atmpS3337;
      continue;
    } else {
      moonbit_decref(_M0L5arrayS1454.$0);
    }
    break;
  }
  return _M0L3setS1452;
}

int32_t _M0MPC111sorted__set9SortedSet3addGsE(
  struct _M0TPC111sorted__set9SortedSetGsE* _M0L4selfS1448,
  moonbit_string_t _M0L5valueS1449
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3887;
  struct _M0TPC111sorted__set4NodeGsE* _M0L4rootS3335;
  struct _M0TUORPC111sorted__set4NodeGsEbE* _M0L7_2abindS1447;
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3886;
  struct _M0TPC111sorted__set4NodeGsE* _M0L12_2anew__rootS1450;
  int32_t _M0L8_2afieldS3885;
  int32_t _M0L6_2acntS4347;
  int32_t _M0L11_2ainsertedS1451;
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3884;
  struct _M0TPC111sorted__set4NodeGsE* _M0L4rootS3332;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3887 = _M0L4selfS1448->$0;
  _M0L4rootS3335 = _M0L8_2afieldS3887;
  if (_M0L4rootS3335) {
    moonbit_incref(_M0L4rootS3335);
  }
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L7_2abindS1447
  = _M0FPC111sorted__set9add__nodeGsE(_M0L4rootS3335, _M0L5valueS1449);
  _M0L8_2afieldS3886 = _M0L7_2abindS1447->$0;
  _M0L12_2anew__rootS1450 = _M0L8_2afieldS3886;
  _M0L8_2afieldS3885 = _M0L7_2abindS1447->$1;
  _M0L6_2acntS4347 = Moonbit_object_header(_M0L7_2abindS1447)->rc;
  if (_M0L6_2acntS4347 > 1) {
    int32_t _M0L11_2anew__cntS4348 = _M0L6_2acntS4347 - 1;
    Moonbit_object_header(_M0L7_2abindS1447)->rc = _M0L11_2anew__cntS4348;
    if (_M0L12_2anew__rootS1450) {
      moonbit_incref(_M0L12_2anew__rootS1450);
    }
  } else if (_M0L6_2acntS4347 == 1) {
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
    moonbit_free(_M0L7_2abindS1447);
  }
  _M0L11_2ainsertedS1451 = _M0L8_2afieldS3885;
  _M0L8_2afieldS3884 = _M0L4selfS1448->$0;
  _M0L4rootS3332 = _M0L8_2afieldS3884;
  if (_M0L4rootS3332) {
    moonbit_incref(_M0L4rootS3332);
  }
  if (_M0L12_2anew__rootS1450) {
    moonbit_incref(_M0L12_2anew__rootS1450);
  }
  #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGORPC111sorted__set4NodeGsEE(_M0L4rootS3332, _M0L12_2anew__rootS1450)
  ) {
    struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3883 =
      _M0L4selfS1448->$0;
    if (_M0L6_2aoldS3883) {
      moonbit_decref(_M0L6_2aoldS3883);
    }
    _M0L4selfS1448->$0 = _M0L12_2anew__rootS1450;
  } else if (_M0L12_2anew__rootS1450) {
    moonbit_decref(_M0L12_2anew__rootS1450);
  }
  if (_M0L11_2ainsertedS1451) {
    int32_t _M0L4sizeS3334 = _M0L4selfS1448->$1;
    int32_t _M0L6_2atmpS3333 = _M0L4sizeS3334 + 1;
    _M0L4selfS1448->$1 = _M0L6_2atmpS3333;
    moonbit_decref(_M0L4selfS1448);
  } else {
    moonbit_decref(_M0L4selfS1448);
  }
  return 0;
}

struct _M0TUORPC111sorted__set4NodeGsEbE* _M0FPC111sorted__set9add__nodeGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4rootS1434,
  moonbit_string_t _M0L5valueS1435
) {
  #line 612 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  if (_M0L4rootS1434 == 0) {
    void* _M0L4NoneS3324;
    void* _M0L4NoneS3325;
    struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3323;
    struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3322;
    struct _M0TUORPC111sorted__set4NodeGsEbE* _block_4556;
    if (_M0L4rootS1434) {
      moonbit_decref(_M0L4rootS1434);
    }
    _M0L4NoneS3324
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4NoneS3325
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    #line 614 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
    _M0L6_2atmpS3323
    = _M0FPC111sorted__set9new__nodeGsE(_M0L5valueS1435, _M0L4NoneS3324, _M0L4NoneS3325, 4294967296ll);
    _M0L6_2atmpS3322 = _M0L6_2atmpS3323;
    _block_4556
    = (struct _M0TUORPC111sorted__set4NodeGsEbE*)moonbit_malloc(sizeof(struct _M0TUORPC111sorted__set4NodeGsEbE));
    Moonbit_object_header(_block_4556)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUORPC111sorted__set4NodeGsEbE, $0) >> 2, 1, 0);
    _block_4556->$0 = _M0L6_2atmpS3322;
    _block_4556->$1 = 1;
    return _block_4556;
  } else {
    struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS1436 = _M0L4rootS1434;
    struct _M0TPC111sorted__set4NodeGsE* _M0L4_2anS1437 = _M0L7_2aSomeS1436;
    moonbit_string_t _M0L8_2afieldS3897 = _M0L4_2anS1437->$0;
    moonbit_string_t _M0L5valueS3331 = _M0L8_2afieldS3897;
    int32_t _M0L4compS1438;
    moonbit_incref(_M0L5valueS3331);
    moonbit_incref(_M0L5valueS1435);
    #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
    _M0L4compS1438
    = _M0IPC16string6StringPB7Compare7compare(_M0L5valueS1435, _M0L5valueS3331);
    if (_M0L4compS1438 == 0) {
      moonbit_string_t _M0L6_2aoldS3888 = _M0L4_2anS1437->$0;
      struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3326;
      struct _M0TUORPC111sorted__set4NodeGsEbE* _block_4557;
      moonbit_decref(_M0L6_2aoldS3888);
      _M0L4_2anS1437->$0 = _M0L5valueS1435;
      _M0L6_2atmpS3326 = _M0L4_2anS1437;
      _block_4557
      = (struct _M0TUORPC111sorted__set4NodeGsEbE*)moonbit_malloc(sizeof(struct _M0TUORPC111sorted__set4NodeGsEbE));
      Moonbit_object_header(_block_4557)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUORPC111sorted__set4NodeGsEbE, $0) >> 2, 1, 0);
      _block_4557->$0 = _M0L6_2atmpS3326;
      _block_4557->$1 = 0;
      return _block_4557;
    } else {
      struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3896 =
        _M0L4_2anS1437->$1;
      struct _M0TPC111sorted__set4NodeGsE* _M0L1lS1439 = _M0L8_2afieldS3896;
      struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3895 =
        _M0L4_2anS1437->$2;
      struct _M0TPC111sorted__set4NodeGsE* _M0L1rS1440 = _M0L8_2afieldS3895;
      if (_M0L4compS1438 < 0) {
        struct _M0TUORPC111sorted__set4NodeGsEbE* _M0L7_2abindS1441;
        struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3891;
        struct _M0TPC111sorted__set4NodeGsE* _M0L5_2anlS1442;
        int32_t _M0L8_2afieldS3890;
        int32_t _M0L6_2acntS4349;
        int32_t _M0L11_2ainsertedS1443;
        struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3889;
        struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3328;
        struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3327;
        struct _M0TUORPC111sorted__set4NodeGsEbE* _block_4558;
        if (_M0L1lS1439) {
          moonbit_incref(_M0L1lS1439);
        }
        #line 623 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        _M0L7_2abindS1441
        = _M0FPC111sorted__set9add__nodeGsE(_M0L1lS1439, _M0L5valueS1435);
        _M0L8_2afieldS3891 = _M0L7_2abindS1441->$0;
        _M0L5_2anlS1442 = _M0L8_2afieldS3891;
        _M0L8_2afieldS3890 = _M0L7_2abindS1441->$1;
        _M0L6_2acntS4349 = Moonbit_object_header(_M0L7_2abindS1441)->rc;
        if (_M0L6_2acntS4349 > 1) {
          int32_t _M0L11_2anew__cntS4350 = _M0L6_2acntS4349 - 1;
          Moonbit_object_header(_M0L7_2abindS1441)->rc
          = _M0L11_2anew__cntS4350;
          if (_M0L5_2anlS1442) {
            moonbit_incref(_M0L5_2anlS1442);
          }
        } else if (_M0L6_2acntS4349 == 1) {
          #line 623 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
          moonbit_free(_M0L7_2abindS1441);
        }
        _M0L11_2ainsertedS1443 = _M0L8_2afieldS3890;
        _M0L6_2aoldS3889 = _M0L4_2anS1437->$1;
        if (_M0L6_2aoldS3889) {
          moonbit_decref(_M0L6_2aoldS3889);
        }
        _M0L4_2anS1437->$1 = _M0L5_2anlS1442;
        #line 625 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        _M0L6_2atmpS3328 = _M0FPC111sorted__set7balanceGsE(_M0L4_2anS1437);
        _M0L6_2atmpS3327 = _M0L6_2atmpS3328;
        _block_4558
        = (struct _M0TUORPC111sorted__set4NodeGsEbE*)moonbit_malloc(sizeof(struct _M0TUORPC111sorted__set4NodeGsEbE));
        Moonbit_object_header(_block_4558)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TUORPC111sorted__set4NodeGsEbE, $0) >> 2, 1, 0);
        _block_4558->$0 = _M0L6_2atmpS3327;
        _block_4558->$1 = _M0L11_2ainsertedS1443;
        return _block_4558;
      } else {
        struct _M0TUORPC111sorted__set4NodeGsEbE* _M0L7_2abindS1444;
        struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3894;
        struct _M0TPC111sorted__set4NodeGsE* _M0L5_2anrS1445;
        int32_t _M0L8_2afieldS3893;
        int32_t _M0L6_2acntS4351;
        int32_t _M0L11_2ainsertedS1446;
        struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3892;
        struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3330;
        struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3329;
        struct _M0TUORPC111sorted__set4NodeGsEbE* _block_4559;
        if (_M0L1rS1440) {
          moonbit_incref(_M0L1rS1440);
        }
        #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        _M0L7_2abindS1444
        = _M0FPC111sorted__set9add__nodeGsE(_M0L1rS1440, _M0L5valueS1435);
        _M0L8_2afieldS3894 = _M0L7_2abindS1444->$0;
        _M0L5_2anrS1445 = _M0L8_2afieldS3894;
        _M0L8_2afieldS3893 = _M0L7_2abindS1444->$1;
        _M0L6_2acntS4351 = Moonbit_object_header(_M0L7_2abindS1444)->rc;
        if (_M0L6_2acntS4351 > 1) {
          int32_t _M0L11_2anew__cntS4352 = _M0L6_2acntS4351 - 1;
          Moonbit_object_header(_M0L7_2abindS1444)->rc
          = _M0L11_2anew__cntS4352;
          if (_M0L5_2anrS1445) {
            moonbit_incref(_M0L5_2anrS1445);
          }
        } else if (_M0L6_2acntS4351 == 1) {
          #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
          moonbit_free(_M0L7_2abindS1444);
        }
        _M0L11_2ainsertedS1446 = _M0L8_2afieldS3893;
        _M0L6_2aoldS3892 = _M0L4_2anS1437->$2;
        if (_M0L6_2aoldS3892) {
          moonbit_decref(_M0L6_2aoldS3892);
        }
        _M0L4_2anS1437->$2 = _M0L5_2anrS1445;
        #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        _M0L6_2atmpS3330 = _M0FPC111sorted__set7balanceGsE(_M0L4_2anS1437);
        _M0L6_2atmpS3329 = _M0L6_2atmpS3330;
        _block_4559
        = (struct _M0TUORPC111sorted__set4NodeGsEbE*)moonbit_malloc(sizeof(struct _M0TUORPC111sorted__set4NodeGsEbE));
        Moonbit_object_header(_block_4559)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TUORPC111sorted__set4NodeGsEbE, $0) >> 2, 1, 0);
        _block_4559->$0 = _M0L6_2atmpS3329;
        _block_4559->$1 = _M0L11_2ainsertedS1446;
        return _block_4559;
      }
    }
  }
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set9new__nodeGsE(
  moonbit_string_t _M0L5valueS1433,
  void* _M0L10left_2eoptS1425,
  void* _M0L11right_2eoptS1428,
  int64_t _M0L12height_2eoptS1431
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS1424;
  struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS1427;
  int32_t _M0L6heightS1430;
  switch (Moonbit_object_tag(_M0L10left_2eoptS1425)) {
    case 1: {
      struct _M0DTPC16option6OptionGORPC111sorted__set4NodeGsEE4Some* _M0L7_2aSomeS1426 =
        (struct _M0DTPC16option6OptionGORPC111sorted__set4NodeGsEE4Some*)_M0L10left_2eoptS1425;
      struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3899 =
        _M0L7_2aSomeS1426->$0;
      int32_t _M0L6_2acntS4353 = Moonbit_object_header(_M0L7_2aSomeS1426)->rc;
      if (_M0L6_2acntS4353 > 1) {
        int32_t _M0L11_2anew__cntS4354 = _M0L6_2acntS4353 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1426)->rc = _M0L11_2anew__cntS4354;
        if (_M0L8_2afieldS3899) {
          moonbit_incref(_M0L8_2afieldS3899);
        }
      } else if (_M0L6_2acntS4353 == 1) {
        moonbit_free(_M0L7_2aSomeS1426);
      }
      _M0L4leftS1424 = _M0L8_2afieldS3899;
      break;
    }
    default: {
      moonbit_decref(_M0L10left_2eoptS1425);
      _M0L4leftS1424 = 0;
      break;
    }
  }
  switch (Moonbit_object_tag(_M0L11right_2eoptS1428)) {
    case 1: {
      struct _M0DTPC16option6OptionGORPC111sorted__set4NodeGsEE4Some* _M0L7_2aSomeS1429 =
        (struct _M0DTPC16option6OptionGORPC111sorted__set4NodeGsEE4Some*)_M0L11right_2eoptS1428;
      struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3898 =
        _M0L7_2aSomeS1429->$0;
      int32_t _M0L6_2acntS4355 = Moonbit_object_header(_M0L7_2aSomeS1429)->rc;
      if (_M0L6_2acntS4355 > 1) {
        int32_t _M0L11_2anew__cntS4356 = _M0L6_2acntS4355 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1429)->rc = _M0L11_2anew__cntS4356;
        if (_M0L8_2afieldS3898) {
          moonbit_incref(_M0L8_2afieldS3898);
        }
      } else if (_M0L6_2acntS4355 == 1) {
        moonbit_free(_M0L7_2aSomeS1429);
      }
      _M0L5rightS1427 = _M0L8_2afieldS3898;
      break;
    }
    default: {
      moonbit_decref(_M0L11right_2eoptS1428);
      _M0L5rightS1427 = 0;
      break;
    }
  }
  if (_M0L12height_2eoptS1431 == 4294967296ll) {
    _M0L6heightS1430 = 1;
  } else {
    int64_t _M0L7_2aSomeS1432 = _M0L12height_2eoptS1431;
    _M0L6heightS1430 = (int32_t)_M0L7_2aSomeS1432;
  }
  return _M0FPC111sorted__set17new__node_2einnerGsE(_M0L5valueS1433, _M0L4leftS1424, _M0L5rightS1427, _M0L6heightS1430);
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set17new__node_2einnerGsE(
  moonbit_string_t _M0L5valueS1420,
  struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS1421,
  struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS1422,
  int32_t _M0L6heightS1423
) {
  struct _M0TPC111sorted__set4NodeGsE* _block_4560;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _block_4560
  = (struct _M0TPC111sorted__set4NodeGsE*)moonbit_malloc(sizeof(struct _M0TPC111sorted__set4NodeGsE));
  Moonbit_object_header(_block_4560)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC111sorted__set4NodeGsE, $0) >> 2, 3, 0);
  _block_4560->$0 = _M0L5valueS1420;
  _block_4560->$1 = _M0L4leftS1421;
  _block_4560->$2 = _M0L5rightS1422;
  _block_4560->$3 = _M0L6heightS1423;
  return _block_4560;
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set7balanceGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4rootS1409
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3905;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1lS1408;
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3904;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1rS1410;
  int32_t _M0L2hlS1411;
  int32_t _M0L2hrS1412;
  int32_t _M0L6_2atmpS3320;
  struct _M0TPC111sorted__set4NodeGsE* _M0L9new__rootS1413;
  #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3905 = _M0L4rootS1409->$1;
  _M0L1lS1408 = _M0L8_2afieldS3905;
  _M0L8_2afieldS3904 = _M0L4rootS1409->$2;
  _M0L1rS1410 = _M0L8_2afieldS3904;
  if (_M0L1lS1408) {
    moonbit_incref(_M0L1lS1408);
  }
  if (_M0L1rS1410) {
    moonbit_incref(_M0L1rS1410);
  }
  if (_M0L1lS1408) {
    moonbit_incref(_M0L1lS1408);
  }
  #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L2hlS1411 = _M0FPC111sorted__set6heightGsE(_M0L1lS1408);
  if (_M0L1rS1410) {
    moonbit_incref(_M0L1rS1410);
  }
  #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L2hrS1412 = _M0FPC111sorted__set6heightGsE(_M0L1rS1410);
  _M0L6_2atmpS3320 = _M0L2hrS1412 + 1;
  if (_M0L2hlS1411 > _M0L6_2atmpS3320) {
    struct _M0TPC111sorted__set4NodeGsE* _M0L7_2abindS1414;
    struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3901;
    struct _M0TPC111sorted__set4NodeGsE* _M0L5_2allS1415;
    struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3900;
    int32_t _M0L6_2acntS4357;
    struct _M0TPC111sorted__set4NodeGsE* _M0L5_2alrS1416;
    if (_M0L1rS1410) {
      moonbit_decref(_M0L1rS1410);
    }
    #line 555 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
    _M0L7_2abindS1414
    = _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(_M0L1lS1408);
    _M0L8_2afieldS3901 = _M0L7_2abindS1414->$1;
    _M0L5_2allS1415 = _M0L8_2afieldS3901;
    _M0L8_2afieldS3900 = _M0L7_2abindS1414->$2;
    _M0L6_2acntS4357 = Moonbit_object_header(_M0L7_2abindS1414)->rc;
    if (_M0L6_2acntS4357 > 1) {
      int32_t _M0L11_2anew__cntS4359 = _M0L6_2acntS4357 - 1;
      Moonbit_object_header(_M0L7_2abindS1414)->rc = _M0L11_2anew__cntS4359;
      if (_M0L8_2afieldS3900) {
        moonbit_incref(_M0L8_2afieldS3900);
      }
      if (_M0L5_2allS1415) {
        moonbit_incref(_M0L5_2allS1415);
      }
    } else if (_M0L6_2acntS4357 == 1) {
      moonbit_string_t _M0L8_2afieldS4358 = _M0L7_2abindS1414->$0;
      moonbit_decref(_M0L8_2afieldS4358);
      #line 555 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      moonbit_free(_M0L7_2abindS1414);
    }
    _M0L5_2alrS1416 = _M0L8_2afieldS3900;
    #line 556 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
    if (
      _M0FPC111sorted__set10height__geGsE(_M0L5_2allS1415, _M0L5_2alrS1416)
    ) {
      #line 557 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      _M0L9new__rootS1413 = _M0FPC111sorted__set9rotate__rGsE(_M0L4rootS1409);
    } else {
      #line 559 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      _M0L9new__rootS1413
      = _M0FPC111sorted__set10rotate__lrGsE(_M0L4rootS1409);
    }
  } else {
    int32_t _M0L6_2atmpS3321;
    if (_M0L1lS1408) {
      moonbit_decref(_M0L1lS1408);
    }
    _M0L6_2atmpS3321 = _M0L2hlS1411 + 1;
    if (_M0L2hrS1412 > _M0L6_2atmpS3321) {
      struct _M0TPC111sorted__set4NodeGsE* _M0L7_2abindS1417;
      struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3903;
      struct _M0TPC111sorted__set4NodeGsE* _M0L5_2arlS1418;
      struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3902;
      int32_t _M0L6_2acntS4360;
      struct _M0TPC111sorted__set4NodeGsE* _M0L5_2arrS1419;
      #line 562 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      _M0L7_2abindS1417
      = _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(_M0L1rS1410);
      _M0L8_2afieldS3903 = _M0L7_2abindS1417->$1;
      _M0L5_2arlS1418 = _M0L8_2afieldS3903;
      _M0L8_2afieldS3902 = _M0L7_2abindS1417->$2;
      _M0L6_2acntS4360 = Moonbit_object_header(_M0L7_2abindS1417)->rc;
      if (_M0L6_2acntS4360 > 1) {
        int32_t _M0L11_2anew__cntS4362 = _M0L6_2acntS4360 - 1;
        Moonbit_object_header(_M0L7_2abindS1417)->rc = _M0L11_2anew__cntS4362;
        if (_M0L8_2afieldS3902) {
          moonbit_incref(_M0L8_2afieldS3902);
        }
        if (_M0L5_2arlS1418) {
          moonbit_incref(_M0L5_2arlS1418);
        }
      } else if (_M0L6_2acntS4360 == 1) {
        moonbit_string_t _M0L8_2afieldS4361 = _M0L7_2abindS1417->$0;
        moonbit_decref(_M0L8_2afieldS4361);
        #line 562 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        moonbit_free(_M0L7_2abindS1417);
      }
      _M0L5_2arrS1419 = _M0L8_2afieldS3902;
      #line 563 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
      if (
        _M0FPC111sorted__set10height__geGsE(_M0L5_2arrS1419, _M0L5_2arlS1418)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        _M0L9new__rootS1413
        = _M0FPC111sorted__set9rotate__lGsE(_M0L4rootS1409);
      } else {
        #line 566 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
        _M0L9new__rootS1413
        = _M0FPC111sorted__set10rotate__rlGsE(_M0L4rootS1409);
      }
    } else {
      if (_M0L1rS1410) {
        moonbit_decref(_M0L1rS1410);
      }
      _M0L9new__rootS1413 = _M0L4rootS1409;
    }
  }
  moonbit_incref(_M0L9new__rootS1413);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0MPC111sorted__set4Node14update__heightGsE(_M0L9new__rootS1413);
  return _M0L9new__rootS1413;
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set10rotate__rlGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L1nS1406
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3907;
  struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS3319;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1rS1405;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1vS1407;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3318;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3906;
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3907 = _M0L1nS1406->$2;
  _M0L5rightS3319 = _M0L8_2afieldS3907;
  if (_M0L5rightS3319) {
    moonbit_incref(_M0L5rightS3319);
  }
  #line 605 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L1rS1405
  = _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(_M0L5rightS3319);
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L1vS1407 = _M0FPC111sorted__set9rotate__rGsE(_M0L1rS1405);
  _M0L6_2atmpS3318 = _M0L1vS1407;
  _M0L6_2aoldS3906 = _M0L1nS1406->$2;
  if (_M0L6_2aoldS3906) {
    moonbit_decref(_M0L6_2aoldS3906);
  }
  _M0L1nS1406->$2 = _M0L6_2atmpS3318;
  #line 608 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  return _M0FPC111sorted__set9rotate__lGsE(_M0L1nS1406);
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set10rotate__lrGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L1nS1403
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3909;
  struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS3317;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1lS1402;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1vS1404;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3316;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3908;
  #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3909 = _M0L1nS1403->$1;
  _M0L4leftS3317 = _M0L8_2afieldS3909;
  if (_M0L4leftS3317) {
    moonbit_incref(_M0L4leftS3317);
  }
  #line 597 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L1lS1402
  = _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(_M0L4leftS3317);
  #line 598 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L1vS1404 = _M0FPC111sorted__set9rotate__lGsE(_M0L1lS1402);
  _M0L6_2atmpS3316 = _M0L1vS1404;
  _M0L6_2aoldS3908 = _M0L1nS1403->$1;
  if (_M0L6_2aoldS3908) {
    moonbit_decref(_M0L6_2aoldS3908);
  }
  _M0L1nS1403->$1 = _M0L6_2atmpS3316;
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  return _M0FPC111sorted__set9rotate__rGsE(_M0L1nS1403);
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set9rotate__rGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L1nS1401
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3913;
  struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS3315;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1lS1400;
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3912;
  struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS3313;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3911;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3314;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3910;
  #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3913 = _M0L1nS1401->$1;
  _M0L4leftS3315 = _M0L8_2afieldS3913;
  if (_M0L4leftS3315) {
    moonbit_incref(_M0L4leftS3315);
  }
  #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L1lS1400
  = _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(_M0L4leftS3315);
  _M0L8_2afieldS3912 = _M0L1lS1400->$2;
  _M0L5rightS3313 = _M0L8_2afieldS3912;
  _M0L6_2aoldS3911 = _M0L1nS1401->$1;
  if (_M0L5rightS3313) {
    moonbit_incref(_M0L5rightS3313);
  }
  if (_M0L6_2aoldS3911) {
    moonbit_decref(_M0L6_2aoldS3911);
  }
  _M0L1nS1401->$1 = _M0L5rightS3313;
  moonbit_incref(_M0L1nS1401);
  _M0L6_2atmpS3314 = _M0L1nS1401;
  _M0L6_2aoldS3910 = _M0L1lS1400->$2;
  if (_M0L6_2aoldS3910) {
    moonbit_decref(_M0L6_2aoldS3910);
  }
  _M0L1lS1400->$2 = _M0L6_2atmpS3314;
  #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0MPC111sorted__set4Node14update__heightGsE(_M0L1nS1401);
  moonbit_incref(_M0L1lS1400);
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0MPC111sorted__set4Node14update__heightGsE(_M0L1lS1400);
  return _M0L1lS1400;
}

struct _M0TPC111sorted__set4NodeGsE* _M0FPC111sorted__set9rotate__lGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L1nS1399
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3917;
  struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS3312;
  struct _M0TPC111sorted__set4NodeGsE* _M0L1rS1398;
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3916;
  struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS3310;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3915;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3311;
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2aoldS3914;
  #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3917 = _M0L1nS1399->$2;
  _M0L5rightS3312 = _M0L8_2afieldS3917;
  if (_M0L5rightS3312) {
    moonbit_incref(_M0L5rightS3312);
  }
  #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L1rS1398
  = _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(_M0L5rightS3312);
  _M0L8_2afieldS3916 = _M0L1rS1398->$1;
  _M0L4leftS3310 = _M0L8_2afieldS3916;
  _M0L6_2aoldS3915 = _M0L1nS1399->$2;
  if (_M0L4leftS3310) {
    moonbit_incref(_M0L4leftS3310);
  }
  if (_M0L6_2aoldS3915) {
    moonbit_decref(_M0L6_2aoldS3915);
  }
  _M0L1nS1399->$2 = _M0L4leftS3310;
  moonbit_incref(_M0L1nS1399);
  _M0L6_2atmpS3311 = _M0L1nS1399;
  _M0L6_2aoldS3914 = _M0L1rS1398->$1;
  if (_M0L6_2aoldS3914) {
    moonbit_decref(_M0L6_2aoldS3914);
  }
  _M0L1rS1398->$1 = _M0L6_2atmpS3311;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0MPC111sorted__set4Node14update__heightGsE(_M0L1nS1399);
  moonbit_incref(_M0L1rS1398);
  #line 581 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0MPC111sorted__set4Node14update__heightGsE(_M0L1rS1398);
  return _M0L1rS1398;
}

int32_t _M0MPC111sorted__set4Node14update__heightGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4selfS1397
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3919;
  struct _M0TPC111sorted__set4NodeGsE* _M0L4leftS3309;
  int32_t _M0L6_2atmpS3306;
  struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS3918;
  struct _M0TPC111sorted__set4NodeGsE* _M0L5rightS3308;
  int32_t _M0L6_2atmpS3307;
  int32_t _M0L6_2atmpS3305;
  int32_t _M0L6_2atmpS3304;
  #line 534 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L8_2afieldS3919 = _M0L4selfS1397->$1;
  _M0L4leftS3309 = _M0L8_2afieldS3919;
  if (_M0L4leftS3309) {
    moonbit_incref(_M0L4leftS3309);
  }
  #line 535 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L6_2atmpS3306 = _M0FPC111sorted__set6heightGsE(_M0L4leftS3309);
  _M0L8_2afieldS3918 = _M0L4selfS1397->$2;
  _M0L5rightS3308 = _M0L8_2afieldS3918;
  if (_M0L5rightS3308) {
    moonbit_incref(_M0L5rightS3308);
  }
  #line 535 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L6_2atmpS3307 = _M0FPC111sorted__set6heightGsE(_M0L5rightS3308);
  #line 535 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L6_2atmpS3305
  = _M0FPC111sorted__set3max(_M0L6_2atmpS3306, _M0L6_2atmpS3307);
  _M0L6_2atmpS3304 = 1 + _M0L6_2atmpS3305;
  _M0L4selfS1397->$3 = _M0L6_2atmpS3304;
  moonbit_decref(_M0L4selfS1397);
  return 0;
}

int32_t _M0FPC111sorted__set10height__geGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L2x1S1394,
  struct _M0TPC111sorted__set4NodeGsE* _M0L2x2S1391
) {
  #line 539 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  if (_M0L2x2S1391 == 0) {
    if (_M0L2x1S1394) {
      moonbit_decref(_M0L2x1S1394);
    }
    if (_M0L2x2S1391) {
      moonbit_decref(_M0L2x2S1391);
    }
    return 1;
  } else {
    struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS1392 = _M0L2x2S1391;
    struct _M0TPC111sorted__set4NodeGsE* _M0L5_2an2S1393 = _M0L7_2aSomeS1392;
    if (_M0L2x1S1394 == 0) {
      if (_M0L2x1S1394) {
        moonbit_decref(_M0L2x1S1394);
      }
      moonbit_decref(_M0L5_2an2S1393);
      return 0;
    } else {
      struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS1395 = _M0L2x1S1394;
      struct _M0TPC111sorted__set4NodeGsE* _M0L5_2an1S1396 =
        _M0L7_2aSomeS1395;
      int32_t _M0L8_2afieldS3921 = _M0L5_2an1S1396->$3;
      int32_t _M0L6heightS3302;
      int32_t _M0L8_2afieldS3920;
      int32_t _M0L6heightS3303;
      moonbit_decref(_M0L5_2an1S1396);
      _M0L6heightS3302 = _M0L8_2afieldS3921;
      _M0L8_2afieldS3920 = _M0L5_2an2S1393->$3;
      moonbit_decref(_M0L5_2an2S1393);
      _M0L6heightS3303 = _M0L8_2afieldS3920;
      return _M0L6heightS3302 >= _M0L6heightS3303;
    }
  }
}

struct _M0TPC111sorted__set9SortedSetGsE* _M0MPC111sorted__set9SortedSet3newGsE(
  
) {
  struct _M0TPC111sorted__set4NodeGsE* _M0L6_2atmpS3301;
  struct _M0TPC111sorted__set9SortedSetGsE* _block_4561;
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\set.mbt"
  _M0L6_2atmpS3301 = 0;
  _block_4561
  = (struct _M0TPC111sorted__set9SortedSetGsE*)moonbit_malloc(sizeof(struct _M0TPC111sorted__set9SortedSetGsE));
  Moonbit_object_header(_block_4561)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC111sorted__set9SortedSetGsE, $0) >> 2, 1, 0);
  _block_4561->$0 = _M0L6_2atmpS3301;
  _block_4561->$1 = 0;
  return _block_4561;
}

int32_t _M0FPC111sorted__set6heightGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4nodeS1388
) {
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\utils.mbt"
  if (_M0L4nodeS1388 == 0) {
    if (_M0L4nodeS1388) {
      moonbit_decref(_M0L4nodeS1388);
    }
    return 0;
  } else {
    struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS1389 = _M0L4nodeS1388;
    struct _M0TPC111sorted__set4NodeGsE* _M0L4_2anS1390 = _M0L7_2aSomeS1389;
    int32_t _M0L8_2afieldS3922 = _M0L4_2anS1390->$3;
    moonbit_decref(_M0L4_2anS1390);
    return _M0L8_2afieldS3922;
  }
}

int32_t _M0FPC111sorted__set3max(int32_t _M0L1xS1386, int32_t _M0L1yS1387) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\utils.mbt"
  if (_M0L1xS1386 > _M0L1yS1387) {
    return _M0L1xS1386;
  } else {
    return _M0L1yS1387;
  }
}

int32_t _M0IPC111sorted__set4NodePB2Eq5equalGsE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4selfS1384,
  struct _M0TPC111sorted__set4NodeGsE* _M0L5otherS1385
) {
  moonbit_string_t _M0L8_2afieldS3925;
  int32_t _M0L6_2acntS4363;
  moonbit_string_t _M0L5valueS3299;
  moonbit_string_t _M0L8_2afieldS3924;
  int32_t _M0L6_2acntS4367;
  moonbit_string_t _M0L5valueS3300;
  int32_t _M0L6_2atmpS3923;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\utils.mbt"
  _M0L8_2afieldS3925 = _M0L4selfS1384->$0;
  _M0L6_2acntS4363 = Moonbit_object_header(_M0L4selfS1384)->rc;
  if (_M0L6_2acntS4363 > 1) {
    int32_t _M0L11_2anew__cntS4366 = _M0L6_2acntS4363 - 1;
    Moonbit_object_header(_M0L4selfS1384)->rc = _M0L11_2anew__cntS4366;
    moonbit_incref(_M0L8_2afieldS3925);
  } else if (_M0L6_2acntS4363 == 1) {
    struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS4365 =
      _M0L4selfS1384->$2;
    struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS4364;
    if (_M0L8_2afieldS4365) {
      moonbit_decref(_M0L8_2afieldS4365);
    }
    _M0L8_2afieldS4364 = _M0L4selfS1384->$1;
    if (_M0L8_2afieldS4364) {
      moonbit_decref(_M0L8_2afieldS4364);
    }
    #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\utils.mbt"
    moonbit_free(_M0L4selfS1384);
  }
  _M0L5valueS3299 = _M0L8_2afieldS3925;
  _M0L8_2afieldS3924 = _M0L5otherS1385->$0;
  _M0L6_2acntS4367 = Moonbit_object_header(_M0L5otherS1385)->rc;
  if (_M0L6_2acntS4367 > 1) {
    int32_t _M0L11_2anew__cntS4370 = _M0L6_2acntS4367 - 1;
    Moonbit_object_header(_M0L5otherS1385)->rc = _M0L11_2anew__cntS4370;
    moonbit_incref(_M0L8_2afieldS3924);
  } else if (_M0L6_2acntS4367 == 1) {
    struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS4369 =
      _M0L5otherS1385->$2;
    struct _M0TPC111sorted__set4NodeGsE* _M0L8_2afieldS4368;
    if (_M0L8_2afieldS4369) {
      moonbit_decref(_M0L8_2afieldS4369);
    }
    _M0L8_2afieldS4368 = _M0L5otherS1385->$1;
    if (_M0L8_2afieldS4368) {
      moonbit_decref(_M0L8_2afieldS4368);
    }
    #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\utils.mbt"
    moonbit_free(_M0L5otherS1385);
  }
  _M0L5valueS3300 = _M0L8_2afieldS3924;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\sorted_set\\utils.mbt"
  _M0L6_2atmpS3923
  = moonbit_val_array_equal(_M0L5valueS3299, _M0L5valueS3300);
  moonbit_decref(_M0L5valueS3299);
  moonbit_decref(_M0L5valueS3300);
  return _M0L6_2atmpS3923;
}

void* _M0IPC14json4JsonPB6ToJson8to__json(void* _M0L4selfS1383) {
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0L4selfS1383;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1378,
  void* _M0L7contentS1380,
  moonbit_string_t _M0L3locS1374,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1376
) {
  moonbit_string_t _M0L3locS1373;
  moonbit_string_t _M0L9args__locS1375;
  void* _M0L6_2atmpS3297;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3298;
  moonbit_string_t _M0L6actualS1377;
  moonbit_string_t _M0L4wantS1379;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1373 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1374);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1375 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1376);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3297 = _M0L3objS1378.$0->$method_0(_M0L3objS1378.$1);
  _M0L6_2atmpS3298 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1377
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3297, 0, 0, _M0L6_2atmpS3298);
  if (_M0L7contentS1380 == 0) {
    void* _M0L6_2atmpS3294;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3295;
    if (_M0L7contentS1380) {
      moonbit_decref(_M0L7contentS1380);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3294
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS3295 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1379
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3294, 0, 0, _M0L6_2atmpS3295);
  } else {
    void* _M0L7_2aSomeS1381 = _M0L7contentS1380;
    void* _M0L4_2axS1382 = _M0L7_2aSomeS1381;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3296 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1379
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1382, 0, 0, _M0L6_2atmpS3296);
  }
  moonbit_incref(_M0L4wantS1379);
  moonbit_incref(_M0L6actualS1377);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1377, _M0L4wantS1379)
  ) {
    moonbit_string_t _M0L6_2atmpS3292;
    moonbit_string_t _M0L6_2atmpS3933;
    moonbit_string_t _M0L6_2atmpS3291;
    moonbit_string_t _M0L6_2atmpS3932;
    moonbit_string_t _M0L6_2atmpS3289;
    moonbit_string_t _M0L6_2atmpS3290;
    moonbit_string_t _M0L6_2atmpS3931;
    moonbit_string_t _M0L6_2atmpS3288;
    moonbit_string_t _M0L6_2atmpS3930;
    moonbit_string_t _M0L6_2atmpS3285;
    moonbit_string_t _M0L6_2atmpS3287;
    moonbit_string_t _M0L6_2atmpS3286;
    moonbit_string_t _M0L6_2atmpS3929;
    moonbit_string_t _M0L6_2atmpS3284;
    moonbit_string_t _M0L6_2atmpS3928;
    moonbit_string_t _M0L6_2atmpS3281;
    moonbit_string_t _M0L6_2atmpS3283;
    moonbit_string_t _M0L6_2atmpS3282;
    moonbit_string_t _M0L6_2atmpS3927;
    moonbit_string_t _M0L6_2atmpS3280;
    moonbit_string_t _M0L6_2atmpS3926;
    moonbit_string_t _M0L6_2atmpS3279;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3278;
    struct moonbit_result_0 _result_4562;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3292
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1373);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3933
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS3292);
    moonbit_decref(_M0L6_2atmpS3292);
    _M0L6_2atmpS3291 = _M0L6_2atmpS3933;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3932
    = moonbit_add_string(_M0L6_2atmpS3291, (moonbit_string_t)moonbit_string_literal_38.data);
    moonbit_decref(_M0L6_2atmpS3291);
    _M0L6_2atmpS3289 = _M0L6_2atmpS3932;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3290
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1375);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3931 = moonbit_add_string(_M0L6_2atmpS3289, _M0L6_2atmpS3290);
    moonbit_decref(_M0L6_2atmpS3289);
    moonbit_decref(_M0L6_2atmpS3290);
    _M0L6_2atmpS3288 = _M0L6_2atmpS3931;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3930
    = moonbit_add_string(_M0L6_2atmpS3288, (moonbit_string_t)moonbit_string_literal_39.data);
    moonbit_decref(_M0L6_2atmpS3288);
    _M0L6_2atmpS3285 = _M0L6_2atmpS3930;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3287 = _M0MPC16string6String6escape(_M0L4wantS1379);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3286
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3287);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3929 = moonbit_add_string(_M0L6_2atmpS3285, _M0L6_2atmpS3286);
    moonbit_decref(_M0L6_2atmpS3285);
    moonbit_decref(_M0L6_2atmpS3286);
    _M0L6_2atmpS3284 = _M0L6_2atmpS3929;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3928
    = moonbit_add_string(_M0L6_2atmpS3284, (moonbit_string_t)moonbit_string_literal_40.data);
    moonbit_decref(_M0L6_2atmpS3284);
    _M0L6_2atmpS3281 = _M0L6_2atmpS3928;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3283 = _M0MPC16string6String6escape(_M0L6actualS1377);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3282
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3283);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3927 = moonbit_add_string(_M0L6_2atmpS3281, _M0L6_2atmpS3282);
    moonbit_decref(_M0L6_2atmpS3281);
    moonbit_decref(_M0L6_2atmpS3282);
    _M0L6_2atmpS3280 = _M0L6_2atmpS3927;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3926
    = moonbit_add_string(_M0L6_2atmpS3280, (moonbit_string_t)moonbit_string_literal_41.data);
    moonbit_decref(_M0L6_2atmpS3280);
    _M0L6_2atmpS3279 = _M0L6_2atmpS3926;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3278
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3278)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3278)->$0
    = _M0L6_2atmpS3279;
    _result_4562.tag = 0;
    _result_4562.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3278;
    return _result_4562;
  } else {
    int32_t _M0L6_2atmpS3293;
    struct moonbit_result_0 _result_4563;
    moonbit_decref(_M0L4wantS1379);
    moonbit_decref(_M0L6actualS1377);
    moonbit_decref(_M0L9args__locS1375);
    moonbit_decref(_M0L3locS1373);
    _M0L6_2atmpS3293 = 0;
    _result_4563.tag = 1;
    _result_4563.data.ok = _M0L6_2atmpS3293;
    return _result_4563;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1372,
  int32_t _M0L13escape__slashS1344,
  int32_t _M0L6indentS1339,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1365
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1331;
  void** _M0L6_2atmpS3277;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1332;
  int32_t _M0Lm5depthS1333;
  void* _M0L6_2atmpS3276;
  void* _M0L8_2aparamS1334;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1331 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3277 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1332
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1332)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1332->$0 = _M0L6_2atmpS3277;
  _M0L5stackS1332->$1 = 0;
  _M0Lm5depthS1333 = 0;
  _M0L6_2atmpS3276 = _M0L4selfS1372;
  _M0L8_2aparamS1334 = _M0L6_2atmpS3276;
  _2aloop_1350:;
  while (1) {
    if (_M0L8_2aparamS1334 == 0) {
      int32_t _M0L3lenS3238;
      if (_M0L8_2aparamS1334) {
        moonbit_decref(_M0L8_2aparamS1334);
      }
      _M0L3lenS3238 = _M0L5stackS1332->$1;
      if (_M0L3lenS3238 == 0) {
        if (_M0L8replacerS1365) {
          moonbit_decref(_M0L8replacerS1365);
        }
        moonbit_decref(_M0L5stackS1332);
        break;
      } else {
        void** _M0L8_2afieldS3941 = _M0L5stackS1332->$0;
        void** _M0L3bufS3262 = _M0L8_2afieldS3941;
        int32_t _M0L3lenS3264 = _M0L5stackS1332->$1;
        int32_t _M0L6_2atmpS3263 = _M0L3lenS3264 - 1;
        void* _M0L6_2atmpS3940 = (void*)_M0L3bufS3262[_M0L6_2atmpS3263];
        void* _M0L4_2axS1351 = _M0L6_2atmpS3940;
        switch (Moonbit_object_tag(_M0L4_2axS1351)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1352 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1351;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3936 =
              _M0L8_2aArrayS1352->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1353 =
              _M0L8_2afieldS3936;
            int32_t _M0L4_2aiS1354 = _M0L8_2aArrayS1352->$1;
            int32_t _M0L3lenS3250 = _M0L6_2aarrS1353->$1;
            if (_M0L4_2aiS1354 < _M0L3lenS3250) {
              int32_t _if__result_4565;
              void** _M0L8_2afieldS3935;
              void** _M0L3bufS3256;
              void* _M0L6_2atmpS3934;
              void* _M0L7elementS1355;
              int32_t _M0L6_2atmpS3251;
              void* _M0L6_2atmpS3254;
              if (_M0L4_2aiS1354 < 0) {
                _if__result_4565 = 1;
              } else {
                int32_t _M0L3lenS3255 = _M0L6_2aarrS1353->$1;
                _if__result_4565 = _M0L4_2aiS1354 >= _M0L3lenS3255;
              }
              if (_if__result_4565) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3935 = _M0L6_2aarrS1353->$0;
              _M0L3bufS3256 = _M0L8_2afieldS3935;
              _M0L6_2atmpS3934 = (void*)_M0L3bufS3256[_M0L4_2aiS1354];
              _M0L7elementS1355 = _M0L6_2atmpS3934;
              _M0L6_2atmpS3251 = _M0L4_2aiS1354 + 1;
              _M0L8_2aArrayS1352->$1 = _M0L6_2atmpS3251;
              if (_M0L4_2aiS1354 > 0) {
                int32_t _M0L6_2atmpS3253;
                moonbit_string_t _M0L6_2atmpS3252;
                moonbit_incref(_M0L7elementS1355);
                moonbit_incref(_M0L3bufS1331);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 44);
                _M0L6_2atmpS3253 = _M0Lm5depthS1333;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3252
                = _M0FPC14json11indent__str(_M0L6_2atmpS3253, _M0L6indentS1339);
                moonbit_incref(_M0L3bufS1331);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3252);
              } else {
                moonbit_incref(_M0L7elementS1355);
              }
              _M0L6_2atmpS3254 = _M0L7elementS1355;
              _M0L8_2aparamS1334 = _M0L6_2atmpS3254;
              goto _2aloop_1350;
            } else {
              int32_t _M0L6_2atmpS3257 = _M0Lm5depthS1333;
              void* _M0L6_2atmpS3258;
              int32_t _M0L6_2atmpS3260;
              moonbit_string_t _M0L6_2atmpS3259;
              void* _M0L6_2atmpS3261;
              _M0Lm5depthS1333 = _M0L6_2atmpS3257 - 1;
              moonbit_incref(_M0L5stackS1332);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3258
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1332);
              if (_M0L6_2atmpS3258) {
                moonbit_decref(_M0L6_2atmpS3258);
              }
              _M0L6_2atmpS3260 = _M0Lm5depthS1333;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3259
              = _M0FPC14json11indent__str(_M0L6_2atmpS3260, _M0L6indentS1339);
              moonbit_incref(_M0L3bufS1331);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3259);
              moonbit_incref(_M0L3bufS1331);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 93);
              _M0L6_2atmpS3261 = 0;
              _M0L8_2aparamS1334 = _M0L6_2atmpS3261;
              goto _2aloop_1350;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1356 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1351;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3939 =
              _M0L9_2aObjectS1356->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1357 =
              _M0L8_2afieldS3939;
            int32_t _M0L8_2afirstS1358 = _M0L9_2aObjectS1356->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1359;
            moonbit_incref(_M0L11_2aiteratorS1357);
            moonbit_incref(_M0L9_2aObjectS1356);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1359
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1357);
            if (_M0L7_2abindS1359 == 0) {
              int32_t _M0L6_2atmpS3239;
              void* _M0L6_2atmpS3240;
              int32_t _M0L6_2atmpS3242;
              moonbit_string_t _M0L6_2atmpS3241;
              void* _M0L6_2atmpS3243;
              if (_M0L7_2abindS1359) {
                moonbit_decref(_M0L7_2abindS1359);
              }
              moonbit_decref(_M0L9_2aObjectS1356);
              _M0L6_2atmpS3239 = _M0Lm5depthS1333;
              _M0Lm5depthS1333 = _M0L6_2atmpS3239 - 1;
              moonbit_incref(_M0L5stackS1332);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3240
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1332);
              if (_M0L6_2atmpS3240) {
                moonbit_decref(_M0L6_2atmpS3240);
              }
              _M0L6_2atmpS3242 = _M0Lm5depthS1333;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3241
              = _M0FPC14json11indent__str(_M0L6_2atmpS3242, _M0L6indentS1339);
              moonbit_incref(_M0L3bufS1331);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3241);
              moonbit_incref(_M0L3bufS1331);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 125);
              _M0L6_2atmpS3243 = 0;
              _M0L8_2aparamS1334 = _M0L6_2atmpS3243;
              goto _2aloop_1350;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1360 = _M0L7_2abindS1359;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1361 = _M0L7_2aSomeS1360;
              moonbit_string_t _M0L8_2afieldS3938 = _M0L4_2axS1361->$0;
              moonbit_string_t _M0L4_2akS1362 = _M0L8_2afieldS3938;
              void* _M0L8_2afieldS3937 = _M0L4_2axS1361->$1;
              int32_t _M0L6_2acntS4371 =
                Moonbit_object_header(_M0L4_2axS1361)->rc;
              void* _M0L4_2avS1363;
              void* _M0Lm2v2S1364;
              moonbit_string_t _M0L6_2atmpS3247;
              void* _M0L6_2atmpS3249;
              void* _M0L6_2atmpS3248;
              if (_M0L6_2acntS4371 > 1) {
                int32_t _M0L11_2anew__cntS4372 = _M0L6_2acntS4371 - 1;
                Moonbit_object_header(_M0L4_2axS1361)->rc
                = _M0L11_2anew__cntS4372;
                moonbit_incref(_M0L8_2afieldS3937);
                moonbit_incref(_M0L4_2akS1362);
              } else if (_M0L6_2acntS4371 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1361);
              }
              _M0L4_2avS1363 = _M0L8_2afieldS3937;
              _M0Lm2v2S1364 = _M0L4_2avS1363;
              if (_M0L8replacerS1365 == 0) {
                moonbit_incref(_M0Lm2v2S1364);
                moonbit_decref(_M0L4_2avS1363);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1366 =
                  _M0L8replacerS1365;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1367 =
                  _M0L7_2aSomeS1366;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1368 =
                  _M0L11_2areplacerS1367;
                void* _M0L7_2abindS1369;
                moonbit_incref(_M0L7_2afuncS1368);
                moonbit_incref(_M0L4_2akS1362);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1369
                = _M0L7_2afuncS1368->code(_M0L7_2afuncS1368, _M0L4_2akS1362, _M0L4_2avS1363);
                if (_M0L7_2abindS1369 == 0) {
                  void* _M0L6_2atmpS3244;
                  if (_M0L7_2abindS1369) {
                    moonbit_decref(_M0L7_2abindS1369);
                  }
                  moonbit_decref(_M0L4_2akS1362);
                  moonbit_decref(_M0L9_2aObjectS1356);
                  _M0L6_2atmpS3244 = 0;
                  _M0L8_2aparamS1334 = _M0L6_2atmpS3244;
                  goto _2aloop_1350;
                } else {
                  void* _M0L7_2aSomeS1370 = _M0L7_2abindS1369;
                  void* _M0L4_2avS1371 = _M0L7_2aSomeS1370;
                  _M0Lm2v2S1364 = _M0L4_2avS1371;
                }
              }
              if (!_M0L8_2afirstS1358) {
                int32_t _M0L6_2atmpS3246;
                moonbit_string_t _M0L6_2atmpS3245;
                moonbit_incref(_M0L3bufS1331);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 44);
                _M0L6_2atmpS3246 = _M0Lm5depthS1333;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3245
                = _M0FPC14json11indent__str(_M0L6_2atmpS3246, _M0L6indentS1339);
                moonbit_incref(_M0L3bufS1331);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3245);
              }
              moonbit_incref(_M0L3bufS1331);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3247
              = _M0FPC14json6escape(_M0L4_2akS1362, _M0L13escape__slashS1344);
              moonbit_incref(_M0L3bufS1331);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3247);
              moonbit_incref(_M0L3bufS1331);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 34);
              moonbit_incref(_M0L3bufS1331);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 58);
              if (_M0L6indentS1339 > 0) {
                moonbit_incref(_M0L3bufS1331);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 32);
              }
              _M0L9_2aObjectS1356->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1356);
              _M0L6_2atmpS3249 = _M0Lm2v2S1364;
              _M0L6_2atmpS3248 = _M0L6_2atmpS3249;
              _M0L8_2aparamS1334 = _M0L6_2atmpS3248;
              goto _2aloop_1350;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1335 = _M0L8_2aparamS1334;
      void* _M0L8_2avalueS1336 = _M0L7_2aSomeS1335;
      void* _M0L6_2atmpS3275;
      switch (Moonbit_object_tag(_M0L8_2avalueS1336)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1337 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1336;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3942 =
            _M0L9_2aObjectS1337->$0;
          int32_t _M0L6_2acntS4373 =
            Moonbit_object_header(_M0L9_2aObjectS1337)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1338;
          if (_M0L6_2acntS4373 > 1) {
            int32_t _M0L11_2anew__cntS4374 = _M0L6_2acntS4373 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1337)->rc
            = _M0L11_2anew__cntS4374;
            moonbit_incref(_M0L8_2afieldS3942);
          } else if (_M0L6_2acntS4373 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1337);
          }
          _M0L10_2amembersS1338 = _M0L8_2afieldS3942;
          moonbit_incref(_M0L10_2amembersS1338);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1338)) {
            moonbit_decref(_M0L10_2amembersS1338);
            moonbit_incref(_M0L3bufS1331);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, (moonbit_string_t)moonbit_string_literal_42.data);
          } else {
            int32_t _M0L6_2atmpS3270 = _M0Lm5depthS1333;
            int32_t _M0L6_2atmpS3272;
            moonbit_string_t _M0L6_2atmpS3271;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3274;
            void* _M0L6ObjectS3273;
            _M0Lm5depthS1333 = _M0L6_2atmpS3270 + 1;
            moonbit_incref(_M0L3bufS1331);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 123);
            _M0L6_2atmpS3272 = _M0Lm5depthS1333;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3271
            = _M0FPC14json11indent__str(_M0L6_2atmpS3272, _M0L6indentS1339);
            moonbit_incref(_M0L3bufS1331);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3271);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3274
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1338);
            _M0L6ObjectS3273
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3273)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3273)->$0
            = _M0L6_2atmpS3274;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3273)->$1
            = 1;
            moonbit_incref(_M0L5stackS1332);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1332, _M0L6ObjectS3273);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1340 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1336;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3943 =
            _M0L8_2aArrayS1340->$0;
          int32_t _M0L6_2acntS4375 =
            Moonbit_object_header(_M0L8_2aArrayS1340)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1341;
          if (_M0L6_2acntS4375 > 1) {
            int32_t _M0L11_2anew__cntS4376 = _M0L6_2acntS4375 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1340)->rc
            = _M0L11_2anew__cntS4376;
            moonbit_incref(_M0L8_2afieldS3943);
          } else if (_M0L6_2acntS4375 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1340);
          }
          _M0L6_2aarrS1341 = _M0L8_2afieldS3943;
          moonbit_incref(_M0L6_2aarrS1341);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1341)) {
            moonbit_decref(_M0L6_2aarrS1341);
            moonbit_incref(_M0L3bufS1331);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, (moonbit_string_t)moonbit_string_literal_43.data);
          } else {
            int32_t _M0L6_2atmpS3266 = _M0Lm5depthS1333;
            int32_t _M0L6_2atmpS3268;
            moonbit_string_t _M0L6_2atmpS3267;
            void* _M0L5ArrayS3269;
            _M0Lm5depthS1333 = _M0L6_2atmpS3266 + 1;
            moonbit_incref(_M0L3bufS1331);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 91);
            _M0L6_2atmpS3268 = _M0Lm5depthS1333;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3267
            = _M0FPC14json11indent__str(_M0L6_2atmpS3268, _M0L6indentS1339);
            moonbit_incref(_M0L3bufS1331);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3267);
            _M0L5ArrayS3269
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3269)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3269)->$0
            = _M0L6_2aarrS1341;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3269)->$1
            = 0;
            moonbit_incref(_M0L5stackS1332);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1332, _M0L5ArrayS3269);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1342 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1336;
          moonbit_string_t _M0L8_2afieldS3944 = _M0L9_2aStringS1342->$0;
          int32_t _M0L6_2acntS4377 =
            Moonbit_object_header(_M0L9_2aStringS1342)->rc;
          moonbit_string_t _M0L4_2asS1343;
          moonbit_string_t _M0L6_2atmpS3265;
          if (_M0L6_2acntS4377 > 1) {
            int32_t _M0L11_2anew__cntS4378 = _M0L6_2acntS4377 - 1;
            Moonbit_object_header(_M0L9_2aStringS1342)->rc
            = _M0L11_2anew__cntS4378;
            moonbit_incref(_M0L8_2afieldS3944);
          } else if (_M0L6_2acntS4377 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1342);
          }
          _M0L4_2asS1343 = _M0L8_2afieldS3944;
          moonbit_incref(_M0L3bufS1331);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3265
          = _M0FPC14json6escape(_M0L4_2asS1343, _M0L13escape__slashS1344);
          moonbit_incref(_M0L3bufS1331);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L6_2atmpS3265);
          moonbit_incref(_M0L3bufS1331);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1331, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1345 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1336;
          double _M0L4_2anS1346 = _M0L9_2aNumberS1345->$0;
          moonbit_string_t _M0L8_2afieldS3945 = _M0L9_2aNumberS1345->$1;
          int32_t _M0L6_2acntS4379 =
            Moonbit_object_header(_M0L9_2aNumberS1345)->rc;
          moonbit_string_t _M0L7_2areprS1347;
          if (_M0L6_2acntS4379 > 1) {
            int32_t _M0L11_2anew__cntS4380 = _M0L6_2acntS4379 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1345)->rc
            = _M0L11_2anew__cntS4380;
            if (_M0L8_2afieldS3945) {
              moonbit_incref(_M0L8_2afieldS3945);
            }
          } else if (_M0L6_2acntS4379 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1345);
          }
          _M0L7_2areprS1347 = _M0L8_2afieldS3945;
          if (_M0L7_2areprS1347 == 0) {
            if (_M0L7_2areprS1347) {
              moonbit_decref(_M0L7_2areprS1347);
            }
            moonbit_incref(_M0L3bufS1331);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1331, _M0L4_2anS1346);
          } else {
            moonbit_string_t _M0L7_2aSomeS1348 = _M0L7_2areprS1347;
            moonbit_string_t _M0L4_2arS1349 = _M0L7_2aSomeS1348;
            moonbit_incref(_M0L3bufS1331);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, _M0L4_2arS1349);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1331);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, (moonbit_string_t)moonbit_string_literal_44.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1331);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, (moonbit_string_t)moonbit_string_literal_45.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1336);
          moonbit_incref(_M0L3bufS1331);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1331, (moonbit_string_t)moonbit_string_literal_46.data);
          break;
        }
      }
      _M0L6_2atmpS3275 = 0;
      _M0L8_2aparamS1334 = _M0L6_2atmpS3275;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1331);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1330,
  int32_t _M0L6indentS1328
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1328 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1329 = _M0L6indentS1328 * _M0L5levelS1330;
    switch (_M0L6spacesS1329) {
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
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_55.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS3237;
        moonbit_string_t _M0L6_2atmpS3946;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3237
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_56.data, _M0L6spacesS1329);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3946
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS3237);
        moonbit_decref(_M0L6_2atmpS3237);
        return _M0L6_2atmpS3946;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1320,
  int32_t _M0L13escape__slashS1325
) {
  int32_t _M0L6_2atmpS3236;
  struct _M0TPB13StringBuilder* _M0L3bufS1319;
  struct _M0TWEOc* _M0L5_2aitS1321;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3236 = Moonbit_array_length(_M0L3strS1320);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1319 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3236);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1321 = _M0MPC16string6String4iter(_M0L3strS1320);
  while (1) {
    int32_t _M0L7_2abindS1322;
    moonbit_incref(_M0L5_2aitS1321);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1322 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1321);
    if (_M0L7_2abindS1322 == -1) {
      moonbit_decref(_M0L5_2aitS1321);
    } else {
      int32_t _M0L7_2aSomeS1323 = _M0L7_2abindS1322;
      int32_t _M0L4_2acS1324 = _M0L7_2aSomeS1323;
      if (_M0L4_2acS1324 == 34) {
        moonbit_incref(_M0L3bufS1319);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_57.data);
      } else if (_M0L4_2acS1324 == 92) {
        moonbit_incref(_M0L3bufS1319);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_58.data);
      } else if (_M0L4_2acS1324 == 47) {
        if (_M0L13escape__slashS1325) {
          moonbit_incref(_M0L3bufS1319);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_59.data);
        } else {
          moonbit_incref(_M0L3bufS1319);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1319, _M0L4_2acS1324);
        }
      } else if (_M0L4_2acS1324 == 10) {
        moonbit_incref(_M0L3bufS1319);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_60.data);
      } else if (_M0L4_2acS1324 == 13) {
        moonbit_incref(_M0L3bufS1319);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_61.data);
      } else if (_M0L4_2acS1324 == 8) {
        moonbit_incref(_M0L3bufS1319);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_62.data);
      } else if (_M0L4_2acS1324 == 9) {
        moonbit_incref(_M0L3bufS1319);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_63.data);
      } else {
        int32_t _M0L4codeS1326 = _M0L4_2acS1324;
        if (_M0L4codeS1326 == 12) {
          moonbit_incref(_M0L3bufS1319);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_64.data);
        } else if (_M0L4codeS1326 < 32) {
          int32_t _M0L6_2atmpS3235;
          moonbit_string_t _M0L6_2atmpS3234;
          moonbit_incref(_M0L3bufS1319);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, (moonbit_string_t)moonbit_string_literal_65.data);
          _M0L6_2atmpS3235 = _M0L4codeS1326 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3234 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3235);
          moonbit_incref(_M0L3bufS1319);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1319, _M0L6_2atmpS3234);
        } else {
          moonbit_incref(_M0L3bufS1319);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1319, _M0L4_2acS1324);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1319);
}

int32_t _M0MPC15array5Array8containsGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1314,
  void* _M0L5valueS1317
) {
  int32_t _M0L7_2abindS1313;
  int32_t _M0L2__S1315;
  #line 838 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L7_2abindS1313 = _M0L4selfS1314->$1;
  _M0L2__S1315 = 0;
  while (1) {
    if (_M0L2__S1315 < _M0L7_2abindS1313) {
      void** _M0L8_2afieldS3948 = _M0L4selfS1314->$0;
      void** _M0L3bufS3233 = _M0L8_2afieldS3948;
      void* _M0L6_2atmpS3947 = (void*)_M0L3bufS3233[_M0L2__S1315];
      void* _M0L1vS1316 = _M0L6_2atmpS3947;
      int32_t _M0L6_2atmpS3232;
      moonbit_incref(_M0L5valueS1317);
      moonbit_incref(_M0L1vS1316);
      #line 840 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      if (_M0IPC14json4JsonPB2Eq5equal(_M0L1vS1316, _M0L5valueS1317)) {
        moonbit_decref(_M0L5valueS1317);
        moonbit_decref(_M0L4selfS1314);
        return 1;
      }
      _M0L6_2atmpS3232 = _M0L2__S1315 + 1;
      _M0L2__S1315 = _M0L6_2atmpS3232;
      continue;
    } else {
      moonbit_decref(_M0L5valueS1317);
      moonbit_decref(_M0L4selfS1314);
      return 0;
    }
    break;
  }
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1312
) {
  int32_t _M0L8_2afieldS3949;
  int32_t _M0L3lenS3231;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3949 = _M0L4selfS1312->$1;
  moonbit_decref(_M0L4selfS1312);
  _M0L3lenS3231 = _M0L8_2afieldS3949;
  return _M0L3lenS3231 == 0;
}

int32_t _M0IPC15array5ArrayPB2Eq5equalGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1307,
  struct _M0TPB5ArrayGRPB4JsonE* _M0L5otherS1309
) {
  int32_t _M0L9self__lenS1306;
  int32_t _M0L10other__lenS1308;
  #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L9self__lenS1306 = _M0L4selfS1307->$1;
  _M0L10other__lenS1308 = _M0L5otherS1309->$1;
  if (_M0L9self__lenS1306 == _M0L10other__lenS1308) {
    int32_t _M0L1iS1310 = 0;
    while (1) {
      if (_M0L1iS1310 < _M0L9self__lenS1306) {
        void** _M0L8_2afieldS3953 = _M0L4selfS1307->$0;
        void** _M0L3bufS3229 = _M0L8_2afieldS3953;
        void* _M0L6_2atmpS3952 = (void*)_M0L3bufS3229[_M0L1iS1310];
        void* _M0L6_2atmpS3226 = _M0L6_2atmpS3952;
        void** _M0L8_2afieldS3951 = _M0L5otherS1309->$0;
        void** _M0L3bufS3228 = _M0L8_2afieldS3951;
        void* _M0L6_2atmpS3950 = (void*)_M0L3bufS3228[_M0L1iS1310];
        void* _M0L6_2atmpS3227 = _M0L6_2atmpS3950;
        int32_t _M0L6_2atmpS3230;
        moonbit_incref(_M0L6_2atmpS3227);
        moonbit_incref(_M0L6_2atmpS3226);
        #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
        if (_M0IPC14json4JsonPB2Eq5equal(_M0L6_2atmpS3226, _M0L6_2atmpS3227)) {
          
        } else {
          moonbit_decref(_M0L5otherS1309);
          moonbit_decref(_M0L4selfS1307);
          return 0;
        }
        _M0L6_2atmpS3230 = _M0L1iS1310 + 1;
        _M0L1iS1310 = _M0L6_2atmpS3230;
        continue;
      } else {
        moonbit_decref(_M0L5otherS1309);
        moonbit_decref(_M0L4selfS1307);
        return 1;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L5otherS1309);
    moonbit_decref(_M0L4selfS1307);
    return 0;
  }
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1303
) {
  int32_t _M0L3lenS1302;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1302 = _M0L4selfS1303->$1;
  if (_M0L3lenS1302 == 0) {
    moonbit_decref(_M0L4selfS1303);
    return 0;
  } else {
    int32_t _M0L5indexS1304 = _M0L3lenS1302 - 1;
    void** _M0L8_2afieldS3957 = _M0L4selfS1303->$0;
    void** _M0L3bufS3225 = _M0L8_2afieldS3957;
    void* _M0L6_2atmpS3956 = (void*)_M0L3bufS3225[_M0L5indexS1304];
    void* _M0L1vS1305 = _M0L6_2atmpS3956;
    void** _M0L8_2afieldS3955 = _M0L4selfS1303->$0;
    void** _M0L3bufS3224 = _M0L8_2afieldS3955;
    void* _M0L6_2aoldS3954;
    if (
      _M0L5indexS1304 < 0
      || _M0L5indexS1304 >= Moonbit_array_length(_M0L3bufS3224)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3954 = (void*)_M0L3bufS3224[_M0L5indexS1304];
    moonbit_incref(_M0L1vS1305);
    moonbit_decref(_M0L6_2aoldS3954);
    if (
      _M0L5indexS1304 < 0
      || _M0L5indexS1304 >= Moonbit_array_length(_M0L3bufS3224)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3224[_M0L5indexS1304]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1303->$1 = _M0L5indexS1304;
    moonbit_decref(_M0L4selfS1303);
    return _M0L1vS1305;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1300,
  struct _M0TPB6Logger _M0L6loggerS1301
) {
  moonbit_string_t _M0L6_2atmpS3223;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS3222;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3223 = _M0L4selfS1300;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3222 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS3223);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS3222, _M0L6loggerS1301);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1277,
  struct _M0TPB6Logger _M0L6loggerS1299
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3966;
  struct _M0TPC16string10StringView _M0L3pkgS1276;
  moonbit_string_t _M0L7_2adataS1278;
  int32_t _M0L8_2astartS1279;
  int32_t _M0L6_2atmpS3221;
  int32_t _M0L6_2aendS1280;
  int32_t _M0Lm9_2acursorS1281;
  int32_t _M0Lm13accept__stateS1282;
  int32_t _M0Lm10match__endS1283;
  int32_t _M0Lm20match__tag__saver__0S1284;
  int32_t _M0Lm6tag__0S1285;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1286;
  struct _M0TPC16string10StringView _M0L8_2afieldS3965;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1295;
  void* _M0L8_2afieldS3964;
  int32_t _M0L6_2acntS4381;
  void* _M0L16_2apackage__nameS1296;
  struct _M0TPC16string10StringView _M0L8_2afieldS3962;
  struct _M0TPC16string10StringView _M0L8filenameS3198;
  struct _M0TPC16string10StringView _M0L8_2afieldS3961;
  struct _M0TPC16string10StringView _M0L11start__lineS3199;
  struct _M0TPC16string10StringView _M0L8_2afieldS3960;
  struct _M0TPC16string10StringView _M0L13start__columnS3200;
  struct _M0TPC16string10StringView _M0L8_2afieldS3959;
  struct _M0TPC16string10StringView _M0L9end__lineS3201;
  struct _M0TPC16string10StringView _M0L8_2afieldS3958;
  int32_t _M0L6_2acntS4385;
  struct _M0TPC16string10StringView _M0L11end__columnS3202;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3966
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1277->$0_1, _M0L4selfS1277->$0_2, _M0L4selfS1277->$0_0
  };
  _M0L3pkgS1276 = _M0L8_2afieldS3966;
  moonbit_incref(_M0L3pkgS1276.$0);
  moonbit_incref(_M0L3pkgS1276.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1278 = _M0MPC16string10StringView4data(_M0L3pkgS1276);
  moonbit_incref(_M0L3pkgS1276.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1279
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1276);
  moonbit_incref(_M0L3pkgS1276.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3221 = _M0MPC16string10StringView6length(_M0L3pkgS1276);
  _M0L6_2aendS1280 = _M0L8_2astartS1279 + _M0L6_2atmpS3221;
  _M0Lm9_2acursorS1281 = _M0L8_2astartS1279;
  _M0Lm13accept__stateS1282 = -1;
  _M0Lm10match__endS1283 = -1;
  _M0Lm20match__tag__saver__0S1284 = -1;
  _M0Lm6tag__0S1285 = -1;
  while (1) {
    int32_t _M0L6_2atmpS3213 = _M0Lm9_2acursorS1281;
    if (_M0L6_2atmpS3213 < _M0L6_2aendS1280) {
      int32_t _M0L6_2atmpS3220 = _M0Lm9_2acursorS1281;
      int32_t _M0L10next__charS1290;
      int32_t _M0L6_2atmpS3214;
      moonbit_incref(_M0L7_2adataS1278);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1290
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1278, _M0L6_2atmpS3220);
      _M0L6_2atmpS3214 = _M0Lm9_2acursorS1281;
      _M0Lm9_2acursorS1281 = _M0L6_2atmpS3214 + 1;
      if (_M0L10next__charS1290 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS3215;
          _M0Lm6tag__0S1285 = _M0Lm9_2acursorS1281;
          _M0L6_2atmpS3215 = _M0Lm9_2acursorS1281;
          if (_M0L6_2atmpS3215 < _M0L6_2aendS1280) {
            int32_t _M0L6_2atmpS3219 = _M0Lm9_2acursorS1281;
            int32_t _M0L10next__charS1291;
            int32_t _M0L6_2atmpS3216;
            moonbit_incref(_M0L7_2adataS1278);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1291
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1278, _M0L6_2atmpS3219);
            _M0L6_2atmpS3216 = _M0Lm9_2acursorS1281;
            _M0Lm9_2acursorS1281 = _M0L6_2atmpS3216 + 1;
            if (_M0L10next__charS1291 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS3217 = _M0Lm9_2acursorS1281;
                if (_M0L6_2atmpS3217 < _M0L6_2aendS1280) {
                  int32_t _M0L6_2atmpS3218 = _M0Lm9_2acursorS1281;
                  _M0Lm9_2acursorS1281 = _M0L6_2atmpS3218 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1284 = _M0Lm6tag__0S1285;
                  _M0Lm13accept__stateS1282 = 0;
                  _M0Lm10match__endS1283 = _M0Lm9_2acursorS1281;
                  goto join_1287;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1287;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1287;
    }
    break;
  }
  goto joinlet_4569;
  join_1287:;
  switch (_M0Lm13accept__stateS1282) {
    case 0: {
      int32_t _M0L6_2atmpS3211;
      int32_t _M0L6_2atmpS3210;
      int64_t _M0L6_2atmpS3207;
      int32_t _M0L6_2atmpS3209;
      int64_t _M0L6_2atmpS3208;
      struct _M0TPC16string10StringView _M0L13package__nameS1288;
      int64_t _M0L6_2atmpS3204;
      int32_t _M0L6_2atmpS3206;
      int64_t _M0L6_2atmpS3205;
      struct _M0TPC16string10StringView _M0L12module__nameS1289;
      void* _M0L4SomeS3203;
      moonbit_decref(_M0L3pkgS1276.$0);
      _M0L6_2atmpS3211 = _M0Lm20match__tag__saver__0S1284;
      _M0L6_2atmpS3210 = _M0L6_2atmpS3211 + 1;
      _M0L6_2atmpS3207 = (int64_t)_M0L6_2atmpS3210;
      _M0L6_2atmpS3209 = _M0Lm10match__endS1283;
      _M0L6_2atmpS3208 = (int64_t)_M0L6_2atmpS3209;
      moonbit_incref(_M0L7_2adataS1278);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1288
      = _M0MPC16string6String4view(_M0L7_2adataS1278, _M0L6_2atmpS3207, _M0L6_2atmpS3208);
      _M0L6_2atmpS3204 = (int64_t)_M0L8_2astartS1279;
      _M0L6_2atmpS3206 = _M0Lm20match__tag__saver__0S1284;
      _M0L6_2atmpS3205 = (int64_t)_M0L6_2atmpS3206;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1289
      = _M0MPC16string6String4view(_M0L7_2adataS1278, _M0L6_2atmpS3204, _M0L6_2atmpS3205);
      _M0L4SomeS3203
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS3203)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3203)->$0_0
      = _M0L13package__nameS1288.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3203)->$0_1
      = _M0L13package__nameS1288.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3203)->$0_2
      = _M0L13package__nameS1288.$2;
      _M0L7_2abindS1286
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1286)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1286->$0_0 = _M0L12module__nameS1289.$0;
      _M0L7_2abindS1286->$0_1 = _M0L12module__nameS1289.$1;
      _M0L7_2abindS1286->$0_2 = _M0L12module__nameS1289.$2;
      _M0L7_2abindS1286->$1 = _M0L4SomeS3203;
      break;
    }
    default: {
      void* _M0L4NoneS3212;
      moonbit_decref(_M0L7_2adataS1278);
      _M0L4NoneS3212
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1286
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1286)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1286->$0_0 = _M0L3pkgS1276.$0;
      _M0L7_2abindS1286->$0_1 = _M0L3pkgS1276.$1;
      _M0L7_2abindS1286->$0_2 = _M0L3pkgS1276.$2;
      _M0L7_2abindS1286->$1 = _M0L4NoneS3212;
      break;
    }
  }
  joinlet_4569:;
  _M0L8_2afieldS3965
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1286->$0_1, _M0L7_2abindS1286->$0_2, _M0L7_2abindS1286->$0_0
  };
  _M0L15_2amodule__nameS1295 = _M0L8_2afieldS3965;
  _M0L8_2afieldS3964 = _M0L7_2abindS1286->$1;
  _M0L6_2acntS4381 = Moonbit_object_header(_M0L7_2abindS1286)->rc;
  if (_M0L6_2acntS4381 > 1) {
    int32_t _M0L11_2anew__cntS4382 = _M0L6_2acntS4381 - 1;
    Moonbit_object_header(_M0L7_2abindS1286)->rc = _M0L11_2anew__cntS4382;
    moonbit_incref(_M0L8_2afieldS3964);
    moonbit_incref(_M0L15_2amodule__nameS1295.$0);
  } else if (_M0L6_2acntS4381 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1286);
  }
  _M0L16_2apackage__nameS1296 = _M0L8_2afieldS3964;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1296)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1297 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1296;
      struct _M0TPC16string10StringView _M0L8_2afieldS3963 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1297->$0_1,
                                              _M0L7_2aSomeS1297->$0_2,
                                              _M0L7_2aSomeS1297->$0_0};
      int32_t _M0L6_2acntS4383 = Moonbit_object_header(_M0L7_2aSomeS1297)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1298;
      if (_M0L6_2acntS4383 > 1) {
        int32_t _M0L11_2anew__cntS4384 = _M0L6_2acntS4383 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1297)->rc = _M0L11_2anew__cntS4384;
        moonbit_incref(_M0L8_2afieldS3963.$0);
      } else if (_M0L6_2acntS4383 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1297);
      }
      _M0L12_2apkg__nameS1298 = _M0L8_2afieldS3963;
      if (_M0L6loggerS1299.$1) {
        moonbit_incref(_M0L6loggerS1299.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L12_2apkg__nameS1298);
      if (_M0L6loggerS1299.$1) {
        moonbit_incref(_M0L6loggerS1299.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1299.$0->$method_3(_M0L6loggerS1299.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1296);
      break;
    }
  }
  _M0L8_2afieldS3962
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1277->$1_1, _M0L4selfS1277->$1_2, _M0L4selfS1277->$1_0
  };
  _M0L8filenameS3198 = _M0L8_2afieldS3962;
  moonbit_incref(_M0L8filenameS3198.$0);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L8filenameS3198);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_3(_M0L6loggerS1299.$1, 58);
  _M0L8_2afieldS3961
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1277->$2_1, _M0L4selfS1277->$2_2, _M0L4selfS1277->$2_0
  };
  _M0L11start__lineS3199 = _M0L8_2afieldS3961;
  moonbit_incref(_M0L11start__lineS3199.$0);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L11start__lineS3199);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_3(_M0L6loggerS1299.$1, 58);
  _M0L8_2afieldS3960
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1277->$3_1, _M0L4selfS1277->$3_2, _M0L4selfS1277->$3_0
  };
  _M0L13start__columnS3200 = _M0L8_2afieldS3960;
  moonbit_incref(_M0L13start__columnS3200.$0);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L13start__columnS3200);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_3(_M0L6loggerS1299.$1, 45);
  _M0L8_2afieldS3959
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1277->$4_1, _M0L4selfS1277->$4_2, _M0L4selfS1277->$4_0
  };
  _M0L9end__lineS3201 = _M0L8_2afieldS3959;
  moonbit_incref(_M0L9end__lineS3201.$0);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L9end__lineS3201);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_3(_M0L6loggerS1299.$1, 58);
  _M0L8_2afieldS3958
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1277->$5_1, _M0L4selfS1277->$5_2, _M0L4selfS1277->$5_0
  };
  _M0L6_2acntS4385 = Moonbit_object_header(_M0L4selfS1277)->rc;
  if (_M0L6_2acntS4385 > 1) {
    int32_t _M0L11_2anew__cntS4391 = _M0L6_2acntS4385 - 1;
    Moonbit_object_header(_M0L4selfS1277)->rc = _M0L11_2anew__cntS4391;
    moonbit_incref(_M0L8_2afieldS3958.$0);
  } else if (_M0L6_2acntS4385 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4390 =
      (struct _M0TPC16string10StringView){_M0L4selfS1277->$4_1,
                                            _M0L4selfS1277->$4_2,
                                            _M0L4selfS1277->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4389;
    struct _M0TPC16string10StringView _M0L8_2afieldS4388;
    struct _M0TPC16string10StringView _M0L8_2afieldS4387;
    struct _M0TPC16string10StringView _M0L8_2afieldS4386;
    moonbit_decref(_M0L8_2afieldS4390.$0);
    _M0L8_2afieldS4389
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1277->$3_1, _M0L4selfS1277->$3_2, _M0L4selfS1277->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4389.$0);
    _M0L8_2afieldS4388
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1277->$2_1, _M0L4selfS1277->$2_2, _M0L4selfS1277->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4388.$0);
    _M0L8_2afieldS4387
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1277->$1_1, _M0L4selfS1277->$1_2, _M0L4selfS1277->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4387.$0);
    _M0L8_2afieldS4386
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1277->$0_1, _M0L4selfS1277->$0_2, _M0L4selfS1277->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4386.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1277);
  }
  _M0L11end__columnS3202 = _M0L8_2afieldS3958;
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L11end__columnS3202);
  if (_M0L6loggerS1299.$1) {
    moonbit_incref(_M0L6loggerS1299.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_3(_M0L6loggerS1299.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1299.$0->$method_2(_M0L6loggerS1299.$1, _M0L15_2amodule__nameS1295);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1275) {
  moonbit_string_t _M0L6_2atmpS3197;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS3197
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1275);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS3197);
  moonbit_decref(_M0L6_2atmpS3197);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1274,
  struct _M0TPB6Logger _M0L6loggerS1273
) {
  moonbit_string_t _M0L6_2atmpS3196;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS3196 = _M0MPC16double6Double10to__string(_M0L4selfS1274);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1273.$0->$method_0(_M0L6loggerS1273.$1, _M0L6_2atmpS3196);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1272) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1272);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1259) {
  uint64_t _M0L4bitsS1260;
  uint64_t _M0L6_2atmpS3195;
  uint64_t _M0L6_2atmpS3194;
  int32_t _M0L8ieeeSignS1261;
  uint64_t _M0L12ieeeMantissaS1262;
  uint64_t _M0L6_2atmpS3193;
  uint64_t _M0L6_2atmpS3192;
  int32_t _M0L12ieeeExponentS1263;
  int32_t _if__result_4573;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1264;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1265;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3191;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1259 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_66.data;
  }
  _M0L4bitsS1260 = *(int64_t*)&_M0L3valS1259;
  _M0L6_2atmpS3195 = _M0L4bitsS1260 >> 63;
  _M0L6_2atmpS3194 = _M0L6_2atmpS3195 & 1ull;
  _M0L8ieeeSignS1261 = _M0L6_2atmpS3194 != 0ull;
  _M0L12ieeeMantissaS1262 = _M0L4bitsS1260 & 4503599627370495ull;
  _M0L6_2atmpS3193 = _M0L4bitsS1260 >> 52;
  _M0L6_2atmpS3192 = _M0L6_2atmpS3193 & 2047ull;
  _M0L12ieeeExponentS1263 = (int32_t)_M0L6_2atmpS3192;
  if (_M0L12ieeeExponentS1263 == 2047) {
    _if__result_4573 = 1;
  } else if (_M0L12ieeeExponentS1263 == 0) {
    _if__result_4573 = _M0L12ieeeMantissaS1262 == 0ull;
  } else {
    _if__result_4573 = 0;
  }
  if (_if__result_4573) {
    int32_t _M0L6_2atmpS3180 = _M0L12ieeeExponentS1263 != 0;
    int32_t _M0L6_2atmpS3181 = _M0L12ieeeMantissaS1262 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1261, _M0L6_2atmpS3180, _M0L6_2atmpS3181);
  }
  _M0Lm1vS1264 = _M0FPB31ryu__to__string_2erecord_2f1258;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1265
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1262, _M0L12ieeeExponentS1263);
  if (_M0L5smallS1265 == 0) {
    uint32_t _M0L6_2atmpS3182;
    if (_M0L5smallS1265) {
      moonbit_decref(_M0L5smallS1265);
    }
    _M0L6_2atmpS3182 = *(uint32_t*)&_M0L12ieeeExponentS1263;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1264 = _M0FPB3d2d(_M0L12ieeeMantissaS1262, _M0L6_2atmpS3182);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1266 = _M0L5smallS1265;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1267 = _M0L7_2aSomeS1266;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1268 = _M0L4_2afS1267;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3190 = _M0Lm1xS1268;
      uint64_t _M0L8_2afieldS3969 = _M0L6_2atmpS3190->$0;
      uint64_t _M0L8mantissaS3189 = _M0L8_2afieldS3969;
      uint64_t _M0L1qS1269 = _M0L8mantissaS3189 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3188 = _M0Lm1xS1268;
      uint64_t _M0L8_2afieldS3968 = _M0L6_2atmpS3188->$0;
      uint64_t _M0L8mantissaS3186 = _M0L8_2afieldS3968;
      uint64_t _M0L6_2atmpS3187 = 10ull * _M0L1qS1269;
      uint64_t _M0L1rS1270 = _M0L8mantissaS3186 - _M0L6_2atmpS3187;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3185;
      int32_t _M0L8_2afieldS3967;
      int32_t _M0L8exponentS3184;
      int32_t _M0L6_2atmpS3183;
      if (_M0L1rS1270 != 0ull) {
        break;
      }
      _M0L6_2atmpS3185 = _M0Lm1xS1268;
      _M0L8_2afieldS3967 = _M0L6_2atmpS3185->$1;
      moonbit_decref(_M0L6_2atmpS3185);
      _M0L8exponentS3184 = _M0L8_2afieldS3967;
      _M0L6_2atmpS3183 = _M0L8exponentS3184 + 1;
      _M0Lm1xS1268
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1268)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1268->$0 = _M0L1qS1269;
      _M0Lm1xS1268->$1 = _M0L6_2atmpS3183;
      continue;
      break;
    }
    _M0Lm1vS1264 = _M0Lm1xS1268;
  }
  _M0L6_2atmpS3191 = _M0Lm1vS1264;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS3191, _M0L8ieeeSignS1261);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1253,
  int32_t _M0L12ieeeExponentS1255
) {
  uint64_t _M0L2m2S1252;
  int32_t _M0L6_2atmpS3179;
  int32_t _M0L2e2S1254;
  int32_t _M0L6_2atmpS3178;
  uint64_t _M0L6_2atmpS3177;
  uint64_t _M0L4maskS1256;
  uint64_t _M0L8fractionS1257;
  int32_t _M0L6_2atmpS3176;
  uint64_t _M0L6_2atmpS3175;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3174;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1252 = 4503599627370496ull | _M0L12ieeeMantissaS1253;
  _M0L6_2atmpS3179 = _M0L12ieeeExponentS1255 - 1023;
  _M0L2e2S1254 = _M0L6_2atmpS3179 - 52;
  if (_M0L2e2S1254 > 0) {
    return 0;
  }
  if (_M0L2e2S1254 < -52) {
    return 0;
  }
  _M0L6_2atmpS3178 = -_M0L2e2S1254;
  _M0L6_2atmpS3177 = 1ull << (_M0L6_2atmpS3178 & 63);
  _M0L4maskS1256 = _M0L6_2atmpS3177 - 1ull;
  _M0L8fractionS1257 = _M0L2m2S1252 & _M0L4maskS1256;
  if (_M0L8fractionS1257 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3176 = -_M0L2e2S1254;
  _M0L6_2atmpS3175 = _M0L2m2S1252 >> (_M0L6_2atmpS3176 & 63);
  _M0L6_2atmpS3174
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3174)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS3174->$0 = _M0L6_2atmpS3175;
  _M0L6_2atmpS3174->$1 = 0;
  return _M0L6_2atmpS3174;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1226,
  int32_t _M0L4signS1224
) {
  int32_t _M0L6_2atmpS3173;
  moonbit_bytes_t _M0L6resultS1222;
  int32_t _M0Lm5indexS1223;
  uint64_t _M0Lm6outputS1225;
  uint64_t _M0L6_2atmpS3172;
  int32_t _M0L7olengthS1227;
  int32_t _M0L8_2afieldS3970;
  int32_t _M0L8exponentS3171;
  int32_t _M0L6_2atmpS3170;
  int32_t _M0Lm3expS1228;
  int32_t _M0L6_2atmpS3169;
  int32_t _M0L6_2atmpS3167;
  int32_t _M0L18scientificNotationS1229;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3173 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1222
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3173);
  _M0Lm5indexS1223 = 0;
  if (_M0L4signS1224) {
    int32_t _M0L6_2atmpS3042 = _M0Lm5indexS1223;
    int32_t _M0L6_2atmpS3043;
    if (
      _M0L6_2atmpS3042 < 0
      || _M0L6_2atmpS3042 >= Moonbit_array_length(_M0L6resultS1222)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1222[_M0L6_2atmpS3042] = 45;
    _M0L6_2atmpS3043 = _M0Lm5indexS1223;
    _M0Lm5indexS1223 = _M0L6_2atmpS3043 + 1;
  }
  _M0Lm6outputS1225 = _M0L1vS1226->$0;
  _M0L6_2atmpS3172 = _M0Lm6outputS1225;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1227 = _M0FPB17decimal__length17(_M0L6_2atmpS3172);
  _M0L8_2afieldS3970 = _M0L1vS1226->$1;
  moonbit_decref(_M0L1vS1226);
  _M0L8exponentS3171 = _M0L8_2afieldS3970;
  _M0L6_2atmpS3170 = _M0L8exponentS3171 + _M0L7olengthS1227;
  _M0Lm3expS1228 = _M0L6_2atmpS3170 - 1;
  _M0L6_2atmpS3169 = _M0Lm3expS1228;
  if (_M0L6_2atmpS3169 >= -6) {
    int32_t _M0L6_2atmpS3168 = _M0Lm3expS1228;
    _M0L6_2atmpS3167 = _M0L6_2atmpS3168 < 21;
  } else {
    _M0L6_2atmpS3167 = 0;
  }
  _M0L18scientificNotationS1229 = !_M0L6_2atmpS3167;
  if (_M0L18scientificNotationS1229) {
    int32_t _M0L7_2abindS1230 = _M0L7olengthS1227 - 1;
    int32_t _M0L1iS1231 = 0;
    int32_t _M0L6_2atmpS3053;
    uint64_t _M0L6_2atmpS3058;
    int32_t _M0L6_2atmpS3057;
    int32_t _M0L6_2atmpS3056;
    int32_t _M0L6_2atmpS3055;
    int32_t _M0L6_2atmpS3054;
    int32_t _M0L6_2atmpS3062;
    int32_t _M0L6_2atmpS3063;
    int32_t _M0L6_2atmpS3064;
    int32_t _M0L6_2atmpS3065;
    int32_t _M0L6_2atmpS3066;
    int32_t _M0L6_2atmpS3072;
    int32_t _M0L6_2atmpS3105;
    while (1) {
      if (_M0L1iS1231 < _M0L7_2abindS1230) {
        uint64_t _M0L6_2atmpS3051 = _M0Lm6outputS1225;
        uint64_t _M0L1cS1232 = _M0L6_2atmpS3051 % 10ull;
        uint64_t _M0L6_2atmpS3044 = _M0Lm6outputS1225;
        int32_t _M0L6_2atmpS3050;
        int32_t _M0L6_2atmpS3049;
        int32_t _M0L6_2atmpS3045;
        int32_t _M0L6_2atmpS3048;
        int32_t _M0L6_2atmpS3047;
        int32_t _M0L6_2atmpS3046;
        int32_t _M0L6_2atmpS3052;
        _M0Lm6outputS1225 = _M0L6_2atmpS3044 / 10ull;
        _M0L6_2atmpS3050 = _M0Lm5indexS1223;
        _M0L6_2atmpS3049 = _M0L6_2atmpS3050 + _M0L7olengthS1227;
        _M0L6_2atmpS3045 = _M0L6_2atmpS3049 - _M0L1iS1231;
        _M0L6_2atmpS3048 = (int32_t)_M0L1cS1232;
        _M0L6_2atmpS3047 = 48 + _M0L6_2atmpS3048;
        _M0L6_2atmpS3046 = _M0L6_2atmpS3047 & 0xff;
        if (
          _M0L6_2atmpS3045 < 0
          || _M0L6_2atmpS3045 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3045] = _M0L6_2atmpS3046;
        _M0L6_2atmpS3052 = _M0L1iS1231 + 1;
        _M0L1iS1231 = _M0L6_2atmpS3052;
        continue;
      }
      break;
    }
    _M0L6_2atmpS3053 = _M0Lm5indexS1223;
    _M0L6_2atmpS3058 = _M0Lm6outputS1225;
    _M0L6_2atmpS3057 = (int32_t)_M0L6_2atmpS3058;
    _M0L6_2atmpS3056 = _M0L6_2atmpS3057 % 10;
    _M0L6_2atmpS3055 = 48 + _M0L6_2atmpS3056;
    _M0L6_2atmpS3054 = _M0L6_2atmpS3055 & 0xff;
    if (
      _M0L6_2atmpS3053 < 0
      || _M0L6_2atmpS3053 >= Moonbit_array_length(_M0L6resultS1222)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1222[_M0L6_2atmpS3053] = _M0L6_2atmpS3054;
    if (_M0L7olengthS1227 > 1) {
      int32_t _M0L6_2atmpS3060 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3059 = _M0L6_2atmpS3060 + 1;
      if (
        _M0L6_2atmpS3059 < 0
        || _M0L6_2atmpS3059 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3059] = 46;
    } else {
      int32_t _M0L6_2atmpS3061 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3061 - 1;
    }
    _M0L6_2atmpS3062 = _M0Lm5indexS1223;
    _M0L6_2atmpS3063 = _M0L7olengthS1227 + 1;
    _M0Lm5indexS1223 = _M0L6_2atmpS3062 + _M0L6_2atmpS3063;
    _M0L6_2atmpS3064 = _M0Lm5indexS1223;
    if (
      _M0L6_2atmpS3064 < 0
      || _M0L6_2atmpS3064 >= Moonbit_array_length(_M0L6resultS1222)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1222[_M0L6_2atmpS3064] = 101;
    _M0L6_2atmpS3065 = _M0Lm5indexS1223;
    _M0Lm5indexS1223 = _M0L6_2atmpS3065 + 1;
    _M0L6_2atmpS3066 = _M0Lm3expS1228;
    if (_M0L6_2atmpS3066 < 0) {
      int32_t _M0L6_2atmpS3067 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3068;
      int32_t _M0L6_2atmpS3069;
      if (
        _M0L6_2atmpS3067 < 0
        || _M0L6_2atmpS3067 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3067] = 45;
      _M0L6_2atmpS3068 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3068 + 1;
      _M0L6_2atmpS3069 = _M0Lm3expS1228;
      _M0Lm3expS1228 = -_M0L6_2atmpS3069;
    } else {
      int32_t _M0L6_2atmpS3070 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3071;
      if (
        _M0L6_2atmpS3070 < 0
        || _M0L6_2atmpS3070 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3070] = 43;
      _M0L6_2atmpS3071 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3071 + 1;
    }
    _M0L6_2atmpS3072 = _M0Lm3expS1228;
    if (_M0L6_2atmpS3072 >= 100) {
      int32_t _M0L6_2atmpS3088 = _M0Lm3expS1228;
      int32_t _M0L1aS1234 = _M0L6_2atmpS3088 / 100;
      int32_t _M0L6_2atmpS3087 = _M0Lm3expS1228;
      int32_t _M0L6_2atmpS3086 = _M0L6_2atmpS3087 / 10;
      int32_t _M0L1bS1235 = _M0L6_2atmpS3086 % 10;
      int32_t _M0L6_2atmpS3085 = _M0Lm3expS1228;
      int32_t _M0L1cS1236 = _M0L6_2atmpS3085 % 10;
      int32_t _M0L6_2atmpS3073 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3075 = 48 + _M0L1aS1234;
      int32_t _M0L6_2atmpS3074 = _M0L6_2atmpS3075 & 0xff;
      int32_t _M0L6_2atmpS3079;
      int32_t _M0L6_2atmpS3076;
      int32_t _M0L6_2atmpS3078;
      int32_t _M0L6_2atmpS3077;
      int32_t _M0L6_2atmpS3083;
      int32_t _M0L6_2atmpS3080;
      int32_t _M0L6_2atmpS3082;
      int32_t _M0L6_2atmpS3081;
      int32_t _M0L6_2atmpS3084;
      if (
        _M0L6_2atmpS3073 < 0
        || _M0L6_2atmpS3073 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3073] = _M0L6_2atmpS3074;
      _M0L6_2atmpS3079 = _M0Lm5indexS1223;
      _M0L6_2atmpS3076 = _M0L6_2atmpS3079 + 1;
      _M0L6_2atmpS3078 = 48 + _M0L1bS1235;
      _M0L6_2atmpS3077 = _M0L6_2atmpS3078 & 0xff;
      if (
        _M0L6_2atmpS3076 < 0
        || _M0L6_2atmpS3076 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3076] = _M0L6_2atmpS3077;
      _M0L6_2atmpS3083 = _M0Lm5indexS1223;
      _M0L6_2atmpS3080 = _M0L6_2atmpS3083 + 2;
      _M0L6_2atmpS3082 = 48 + _M0L1cS1236;
      _M0L6_2atmpS3081 = _M0L6_2atmpS3082 & 0xff;
      if (
        _M0L6_2atmpS3080 < 0
        || _M0L6_2atmpS3080 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3080] = _M0L6_2atmpS3081;
      _M0L6_2atmpS3084 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3084 + 3;
    } else {
      int32_t _M0L6_2atmpS3089 = _M0Lm3expS1228;
      if (_M0L6_2atmpS3089 >= 10) {
        int32_t _M0L6_2atmpS3099 = _M0Lm3expS1228;
        int32_t _M0L1aS1237 = _M0L6_2atmpS3099 / 10;
        int32_t _M0L6_2atmpS3098 = _M0Lm3expS1228;
        int32_t _M0L1bS1238 = _M0L6_2atmpS3098 % 10;
        int32_t _M0L6_2atmpS3090 = _M0Lm5indexS1223;
        int32_t _M0L6_2atmpS3092 = 48 + _M0L1aS1237;
        int32_t _M0L6_2atmpS3091 = _M0L6_2atmpS3092 & 0xff;
        int32_t _M0L6_2atmpS3096;
        int32_t _M0L6_2atmpS3093;
        int32_t _M0L6_2atmpS3095;
        int32_t _M0L6_2atmpS3094;
        int32_t _M0L6_2atmpS3097;
        if (
          _M0L6_2atmpS3090 < 0
          || _M0L6_2atmpS3090 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3090] = _M0L6_2atmpS3091;
        _M0L6_2atmpS3096 = _M0Lm5indexS1223;
        _M0L6_2atmpS3093 = _M0L6_2atmpS3096 + 1;
        _M0L6_2atmpS3095 = 48 + _M0L1bS1238;
        _M0L6_2atmpS3094 = _M0L6_2atmpS3095 & 0xff;
        if (
          _M0L6_2atmpS3093 < 0
          || _M0L6_2atmpS3093 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3093] = _M0L6_2atmpS3094;
        _M0L6_2atmpS3097 = _M0Lm5indexS1223;
        _M0Lm5indexS1223 = _M0L6_2atmpS3097 + 2;
      } else {
        int32_t _M0L6_2atmpS3100 = _M0Lm5indexS1223;
        int32_t _M0L6_2atmpS3103 = _M0Lm3expS1228;
        int32_t _M0L6_2atmpS3102 = 48 + _M0L6_2atmpS3103;
        int32_t _M0L6_2atmpS3101 = _M0L6_2atmpS3102 & 0xff;
        int32_t _M0L6_2atmpS3104;
        if (
          _M0L6_2atmpS3100 < 0
          || _M0L6_2atmpS3100 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3100] = _M0L6_2atmpS3101;
        _M0L6_2atmpS3104 = _M0Lm5indexS1223;
        _M0Lm5indexS1223 = _M0L6_2atmpS3104 + 1;
      }
    }
    _M0L6_2atmpS3105 = _M0Lm5indexS1223;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1222, 0, _M0L6_2atmpS3105);
  } else {
    int32_t _M0L6_2atmpS3106 = _M0Lm3expS1228;
    int32_t _M0L6_2atmpS3166;
    if (_M0L6_2atmpS3106 < 0) {
      int32_t _M0L6_2atmpS3107 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3108;
      int32_t _M0L6_2atmpS3109;
      int32_t _M0L6_2atmpS3110;
      int32_t _M0L1iS1239;
      int32_t _M0L7currentS1241;
      int32_t _M0L1iS1242;
      if (
        _M0L6_2atmpS3107 < 0
        || _M0L6_2atmpS3107 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3107] = 48;
      _M0L6_2atmpS3108 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3108 + 1;
      _M0L6_2atmpS3109 = _M0Lm5indexS1223;
      if (
        _M0L6_2atmpS3109 < 0
        || _M0L6_2atmpS3109 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3109] = 46;
      _M0L6_2atmpS3110 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3110 + 1;
      _M0L1iS1239 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3111 = _M0Lm3expS1228;
        if (_M0L1iS1239 > _M0L6_2atmpS3111) {
          int32_t _M0L6_2atmpS3112 = _M0Lm5indexS1223;
          int32_t _M0L6_2atmpS3113;
          int32_t _M0L6_2atmpS3114;
          if (
            _M0L6_2atmpS3112 < 0
            || _M0L6_2atmpS3112 >= Moonbit_array_length(_M0L6resultS1222)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1222[_M0L6_2atmpS3112] = 48;
          _M0L6_2atmpS3113 = _M0Lm5indexS1223;
          _M0Lm5indexS1223 = _M0L6_2atmpS3113 + 1;
          _M0L6_2atmpS3114 = _M0L1iS1239 - 1;
          _M0L1iS1239 = _M0L6_2atmpS3114;
          continue;
        }
        break;
      }
      _M0L7currentS1241 = _M0Lm5indexS1223;
      _M0L1iS1242 = 0;
      while (1) {
        if (_M0L1iS1242 < _M0L7olengthS1227) {
          int32_t _M0L6_2atmpS3122 = _M0L7currentS1241 + _M0L7olengthS1227;
          int32_t _M0L6_2atmpS3121 = _M0L6_2atmpS3122 - _M0L1iS1242;
          int32_t _M0L6_2atmpS3115 = _M0L6_2atmpS3121 - 1;
          uint64_t _M0L6_2atmpS3120 = _M0Lm6outputS1225;
          uint64_t _M0L6_2atmpS3119 = _M0L6_2atmpS3120 % 10ull;
          int32_t _M0L6_2atmpS3118 = (int32_t)_M0L6_2atmpS3119;
          int32_t _M0L6_2atmpS3117 = 48 + _M0L6_2atmpS3118;
          int32_t _M0L6_2atmpS3116 = _M0L6_2atmpS3117 & 0xff;
          uint64_t _M0L6_2atmpS3123;
          int32_t _M0L6_2atmpS3124;
          int32_t _M0L6_2atmpS3125;
          if (
            _M0L6_2atmpS3115 < 0
            || _M0L6_2atmpS3115 >= Moonbit_array_length(_M0L6resultS1222)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1222[_M0L6_2atmpS3115] = _M0L6_2atmpS3116;
          _M0L6_2atmpS3123 = _M0Lm6outputS1225;
          _M0Lm6outputS1225 = _M0L6_2atmpS3123 / 10ull;
          _M0L6_2atmpS3124 = _M0Lm5indexS1223;
          _M0Lm5indexS1223 = _M0L6_2atmpS3124 + 1;
          _M0L6_2atmpS3125 = _M0L1iS1242 + 1;
          _M0L1iS1242 = _M0L6_2atmpS3125;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS3127 = _M0Lm3expS1228;
      int32_t _M0L6_2atmpS3126 = _M0L6_2atmpS3127 + 1;
      if (_M0L6_2atmpS3126 >= _M0L7olengthS1227) {
        int32_t _M0L1iS1244 = 0;
        int32_t _M0L6_2atmpS3139;
        int32_t _M0L6_2atmpS3143;
        int32_t _M0L7_2abindS1246;
        int32_t _M0L2__S1247;
        while (1) {
          if (_M0L1iS1244 < _M0L7olengthS1227) {
            int32_t _M0L6_2atmpS3136 = _M0Lm5indexS1223;
            int32_t _M0L6_2atmpS3135 = _M0L6_2atmpS3136 + _M0L7olengthS1227;
            int32_t _M0L6_2atmpS3134 = _M0L6_2atmpS3135 - _M0L1iS1244;
            int32_t _M0L6_2atmpS3128 = _M0L6_2atmpS3134 - 1;
            uint64_t _M0L6_2atmpS3133 = _M0Lm6outputS1225;
            uint64_t _M0L6_2atmpS3132 = _M0L6_2atmpS3133 % 10ull;
            int32_t _M0L6_2atmpS3131 = (int32_t)_M0L6_2atmpS3132;
            int32_t _M0L6_2atmpS3130 = 48 + _M0L6_2atmpS3131;
            int32_t _M0L6_2atmpS3129 = _M0L6_2atmpS3130 & 0xff;
            uint64_t _M0L6_2atmpS3137;
            int32_t _M0L6_2atmpS3138;
            if (
              _M0L6_2atmpS3128 < 0
              || _M0L6_2atmpS3128 >= Moonbit_array_length(_M0L6resultS1222)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1222[_M0L6_2atmpS3128] = _M0L6_2atmpS3129;
            _M0L6_2atmpS3137 = _M0Lm6outputS1225;
            _M0Lm6outputS1225 = _M0L6_2atmpS3137 / 10ull;
            _M0L6_2atmpS3138 = _M0L1iS1244 + 1;
            _M0L1iS1244 = _M0L6_2atmpS3138;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3139 = _M0Lm5indexS1223;
        _M0Lm5indexS1223 = _M0L6_2atmpS3139 + _M0L7olengthS1227;
        _M0L6_2atmpS3143 = _M0Lm3expS1228;
        _M0L7_2abindS1246 = _M0L6_2atmpS3143 + 1;
        _M0L2__S1247 = _M0L7olengthS1227;
        while (1) {
          if (_M0L2__S1247 < _M0L7_2abindS1246) {
            int32_t _M0L6_2atmpS3140 = _M0Lm5indexS1223;
            int32_t _M0L6_2atmpS3141;
            int32_t _M0L6_2atmpS3142;
            if (
              _M0L6_2atmpS3140 < 0
              || _M0L6_2atmpS3140 >= Moonbit_array_length(_M0L6resultS1222)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1222[_M0L6_2atmpS3140] = 48;
            _M0L6_2atmpS3141 = _M0Lm5indexS1223;
            _M0Lm5indexS1223 = _M0L6_2atmpS3141 + 1;
            _M0L6_2atmpS3142 = _M0L2__S1247 + 1;
            _M0L2__S1247 = _M0L6_2atmpS3142;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS3165 = _M0Lm5indexS1223;
        int32_t _M0Lm7currentS1249 = _M0L6_2atmpS3165 + 1;
        int32_t _M0L1iS1250 = 0;
        int32_t _M0L6_2atmpS3163;
        int32_t _M0L6_2atmpS3164;
        while (1) {
          if (_M0L1iS1250 < _M0L7olengthS1227) {
            int32_t _M0L6_2atmpS3146 = _M0L7olengthS1227 - _M0L1iS1250;
            int32_t _M0L6_2atmpS3144 = _M0L6_2atmpS3146 - 1;
            int32_t _M0L6_2atmpS3145 = _M0Lm3expS1228;
            int32_t _M0L6_2atmpS3160;
            int32_t _M0L6_2atmpS3159;
            int32_t _M0L6_2atmpS3158;
            int32_t _M0L6_2atmpS3152;
            uint64_t _M0L6_2atmpS3157;
            uint64_t _M0L6_2atmpS3156;
            int32_t _M0L6_2atmpS3155;
            int32_t _M0L6_2atmpS3154;
            int32_t _M0L6_2atmpS3153;
            uint64_t _M0L6_2atmpS3161;
            int32_t _M0L6_2atmpS3162;
            if (_M0L6_2atmpS3144 == _M0L6_2atmpS3145) {
              int32_t _M0L6_2atmpS3150 = _M0Lm7currentS1249;
              int32_t _M0L6_2atmpS3149 = _M0L6_2atmpS3150 + _M0L7olengthS1227;
              int32_t _M0L6_2atmpS3148 = _M0L6_2atmpS3149 - _M0L1iS1250;
              int32_t _M0L6_2atmpS3147 = _M0L6_2atmpS3148 - 1;
              int32_t _M0L6_2atmpS3151;
              if (
                _M0L6_2atmpS3147 < 0
                || _M0L6_2atmpS3147 >= Moonbit_array_length(_M0L6resultS1222)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1222[_M0L6_2atmpS3147] = 46;
              _M0L6_2atmpS3151 = _M0Lm7currentS1249;
              _M0Lm7currentS1249 = _M0L6_2atmpS3151 - 1;
            }
            _M0L6_2atmpS3160 = _M0Lm7currentS1249;
            _M0L6_2atmpS3159 = _M0L6_2atmpS3160 + _M0L7olengthS1227;
            _M0L6_2atmpS3158 = _M0L6_2atmpS3159 - _M0L1iS1250;
            _M0L6_2atmpS3152 = _M0L6_2atmpS3158 - 1;
            _M0L6_2atmpS3157 = _M0Lm6outputS1225;
            _M0L6_2atmpS3156 = _M0L6_2atmpS3157 % 10ull;
            _M0L6_2atmpS3155 = (int32_t)_M0L6_2atmpS3156;
            _M0L6_2atmpS3154 = 48 + _M0L6_2atmpS3155;
            _M0L6_2atmpS3153 = _M0L6_2atmpS3154 & 0xff;
            if (
              _M0L6_2atmpS3152 < 0
              || _M0L6_2atmpS3152 >= Moonbit_array_length(_M0L6resultS1222)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1222[_M0L6_2atmpS3152] = _M0L6_2atmpS3153;
            _M0L6_2atmpS3161 = _M0Lm6outputS1225;
            _M0Lm6outputS1225 = _M0L6_2atmpS3161 / 10ull;
            _M0L6_2atmpS3162 = _M0L1iS1250 + 1;
            _M0L1iS1250 = _M0L6_2atmpS3162;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3163 = _M0Lm5indexS1223;
        _M0L6_2atmpS3164 = _M0L7olengthS1227 + 1;
        _M0Lm5indexS1223 = _M0L6_2atmpS3163 + _M0L6_2atmpS3164;
      }
    }
    _M0L6_2atmpS3166 = _M0Lm5indexS1223;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1222, 0, _M0L6_2atmpS3166);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1168,
  uint32_t _M0L12ieeeExponentS1167
) {
  int32_t _M0Lm2e2S1165;
  uint64_t _M0Lm2m2S1166;
  uint64_t _M0L6_2atmpS3041;
  uint64_t _M0L6_2atmpS3040;
  int32_t _M0L4evenS1169;
  uint64_t _M0L6_2atmpS3039;
  uint64_t _M0L2mvS1170;
  int32_t _M0L7mmShiftS1171;
  uint64_t _M0Lm2vrS1172;
  uint64_t _M0Lm2vpS1173;
  uint64_t _M0Lm2vmS1174;
  int32_t _M0Lm3e10S1175;
  int32_t _M0Lm17vmIsTrailingZerosS1176;
  int32_t _M0Lm17vrIsTrailingZerosS1177;
  int32_t _M0L6_2atmpS2941;
  int32_t _M0Lm7removedS1196;
  int32_t _M0Lm16lastRemovedDigitS1197;
  uint64_t _M0Lm6outputS1198;
  int32_t _M0L6_2atmpS3037;
  int32_t _M0L6_2atmpS3038;
  int32_t _M0L3expS1221;
  uint64_t _M0L6_2atmpS3036;
  struct _M0TPB17FloatingDecimal64* _block_4586;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1165 = 0;
  _M0Lm2m2S1166 = 0ull;
  if (_M0L12ieeeExponentS1167 == 0u) {
    _M0Lm2e2S1165 = -1076;
    _M0Lm2m2S1166 = _M0L12ieeeMantissaS1168;
  } else {
    int32_t _M0L6_2atmpS2940 = *(int32_t*)&_M0L12ieeeExponentS1167;
    int32_t _M0L6_2atmpS2939 = _M0L6_2atmpS2940 - 1023;
    int32_t _M0L6_2atmpS2938 = _M0L6_2atmpS2939 - 52;
    _M0Lm2e2S1165 = _M0L6_2atmpS2938 - 2;
    _M0Lm2m2S1166 = 4503599627370496ull | _M0L12ieeeMantissaS1168;
  }
  _M0L6_2atmpS3041 = _M0Lm2m2S1166;
  _M0L6_2atmpS3040 = _M0L6_2atmpS3041 & 1ull;
  _M0L4evenS1169 = _M0L6_2atmpS3040 == 0ull;
  _M0L6_2atmpS3039 = _M0Lm2m2S1166;
  _M0L2mvS1170 = 4ull * _M0L6_2atmpS3039;
  if (_M0L12ieeeMantissaS1168 != 0ull) {
    _M0L7mmShiftS1171 = 1;
  } else {
    _M0L7mmShiftS1171 = _M0L12ieeeExponentS1167 <= 1u;
  }
  _M0Lm2vrS1172 = 0ull;
  _M0Lm2vpS1173 = 0ull;
  _M0Lm2vmS1174 = 0ull;
  _M0Lm3e10S1175 = 0;
  _M0Lm17vmIsTrailingZerosS1176 = 0;
  _M0Lm17vrIsTrailingZerosS1177 = 0;
  _M0L6_2atmpS2941 = _M0Lm2e2S1165;
  if (_M0L6_2atmpS2941 >= 0) {
    int32_t _M0L6_2atmpS2963 = _M0Lm2e2S1165;
    int32_t _M0L6_2atmpS2959;
    int32_t _M0L6_2atmpS2962;
    int32_t _M0L6_2atmpS2961;
    int32_t _M0L6_2atmpS2960;
    int32_t _M0L1qS1178;
    int32_t _M0L6_2atmpS2958;
    int32_t _M0L6_2atmpS2957;
    int32_t _M0L1kS1179;
    int32_t _M0L6_2atmpS2956;
    int32_t _M0L6_2atmpS2955;
    int32_t _M0L6_2atmpS2954;
    int32_t _M0L1iS1180;
    struct _M0TPB8Pow5Pair _M0L4pow5S1181;
    uint64_t _M0L6_2atmpS2953;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1182;
    uint64_t _M0L8_2avrOutS1183;
    uint64_t _M0L8_2avpOutS1184;
    uint64_t _M0L8_2avmOutS1185;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2959 = _M0FPB9log10Pow2(_M0L6_2atmpS2963);
    _M0L6_2atmpS2962 = _M0Lm2e2S1165;
    _M0L6_2atmpS2961 = _M0L6_2atmpS2962 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2960 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2961);
    _M0L1qS1178 = _M0L6_2atmpS2959 - _M0L6_2atmpS2960;
    _M0Lm3e10S1175 = _M0L1qS1178;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2958 = _M0FPB8pow5bits(_M0L1qS1178);
    _M0L6_2atmpS2957 = 125 + _M0L6_2atmpS2958;
    _M0L1kS1179 = _M0L6_2atmpS2957 - 1;
    _M0L6_2atmpS2956 = _M0Lm2e2S1165;
    _M0L6_2atmpS2955 = -_M0L6_2atmpS2956;
    _M0L6_2atmpS2954 = _M0L6_2atmpS2955 + _M0L1qS1178;
    _M0L1iS1180 = _M0L6_2atmpS2954 + _M0L1kS1179;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1181 = _M0FPB22double__computeInvPow5(_M0L1qS1178);
    _M0L6_2atmpS2953 = _M0Lm2m2S1166;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1182
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2953, _M0L4pow5S1181, _M0L1iS1180, _M0L7mmShiftS1171);
    _M0L8_2avrOutS1183 = _M0L7_2abindS1182.$0;
    _M0L8_2avpOutS1184 = _M0L7_2abindS1182.$1;
    _M0L8_2avmOutS1185 = _M0L7_2abindS1182.$2;
    _M0Lm2vrS1172 = _M0L8_2avrOutS1183;
    _M0Lm2vpS1173 = _M0L8_2avpOutS1184;
    _M0Lm2vmS1174 = _M0L8_2avmOutS1185;
    if (_M0L1qS1178 <= 21) {
      int32_t _M0L6_2atmpS2949 = (int32_t)_M0L2mvS1170;
      uint64_t _M0L6_2atmpS2952 = _M0L2mvS1170 / 5ull;
      int32_t _M0L6_2atmpS2951 = (int32_t)_M0L6_2atmpS2952;
      int32_t _M0L6_2atmpS2950 = 5 * _M0L6_2atmpS2951;
      int32_t _M0L6mvMod5S1186 = _M0L6_2atmpS2949 - _M0L6_2atmpS2950;
      if (_M0L6mvMod5S1186 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1177
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1170, _M0L1qS1178);
      } else if (_M0L4evenS1169) {
        uint64_t _M0L6_2atmpS2943 = _M0L2mvS1170 - 1ull;
        uint64_t _M0L6_2atmpS2944;
        uint64_t _M0L6_2atmpS2942;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2944 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1171);
        _M0L6_2atmpS2942 = _M0L6_2atmpS2943 - _M0L6_2atmpS2944;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1176
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2942, _M0L1qS1178);
      } else {
        uint64_t _M0L6_2atmpS2945 = _M0Lm2vpS1173;
        uint64_t _M0L6_2atmpS2948 = _M0L2mvS1170 + 2ull;
        int32_t _M0L6_2atmpS2947;
        uint64_t _M0L6_2atmpS2946;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2947
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2948, _M0L1qS1178);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2946 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2947);
        _M0Lm2vpS1173 = _M0L6_2atmpS2945 - _M0L6_2atmpS2946;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2977 = _M0Lm2e2S1165;
    int32_t _M0L6_2atmpS2976 = -_M0L6_2atmpS2977;
    int32_t _M0L6_2atmpS2971;
    int32_t _M0L6_2atmpS2975;
    int32_t _M0L6_2atmpS2974;
    int32_t _M0L6_2atmpS2973;
    int32_t _M0L6_2atmpS2972;
    int32_t _M0L1qS1187;
    int32_t _M0L6_2atmpS2964;
    int32_t _M0L6_2atmpS2970;
    int32_t _M0L6_2atmpS2969;
    int32_t _M0L1iS1188;
    int32_t _M0L6_2atmpS2968;
    int32_t _M0L1kS1189;
    int32_t _M0L1jS1190;
    struct _M0TPB8Pow5Pair _M0L4pow5S1191;
    uint64_t _M0L6_2atmpS2967;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1192;
    uint64_t _M0L8_2avrOutS1193;
    uint64_t _M0L8_2avpOutS1194;
    uint64_t _M0L8_2avmOutS1195;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2971 = _M0FPB9log10Pow5(_M0L6_2atmpS2976);
    _M0L6_2atmpS2975 = _M0Lm2e2S1165;
    _M0L6_2atmpS2974 = -_M0L6_2atmpS2975;
    _M0L6_2atmpS2973 = _M0L6_2atmpS2974 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2972 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2973);
    _M0L1qS1187 = _M0L6_2atmpS2971 - _M0L6_2atmpS2972;
    _M0L6_2atmpS2964 = _M0Lm2e2S1165;
    _M0Lm3e10S1175 = _M0L1qS1187 + _M0L6_2atmpS2964;
    _M0L6_2atmpS2970 = _M0Lm2e2S1165;
    _M0L6_2atmpS2969 = -_M0L6_2atmpS2970;
    _M0L1iS1188 = _M0L6_2atmpS2969 - _M0L1qS1187;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2968 = _M0FPB8pow5bits(_M0L1iS1188);
    _M0L1kS1189 = _M0L6_2atmpS2968 - 125;
    _M0L1jS1190 = _M0L1qS1187 - _M0L1kS1189;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1191 = _M0FPB19double__computePow5(_M0L1iS1188);
    _M0L6_2atmpS2967 = _M0Lm2m2S1166;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1192
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2967, _M0L4pow5S1191, _M0L1jS1190, _M0L7mmShiftS1171);
    _M0L8_2avrOutS1193 = _M0L7_2abindS1192.$0;
    _M0L8_2avpOutS1194 = _M0L7_2abindS1192.$1;
    _M0L8_2avmOutS1195 = _M0L7_2abindS1192.$2;
    _M0Lm2vrS1172 = _M0L8_2avrOutS1193;
    _M0Lm2vpS1173 = _M0L8_2avpOutS1194;
    _M0Lm2vmS1174 = _M0L8_2avmOutS1195;
    if (_M0L1qS1187 <= 1) {
      _M0Lm17vrIsTrailingZerosS1177 = 1;
      if (_M0L4evenS1169) {
        int32_t _M0L6_2atmpS2965;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2965 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1171);
        _M0Lm17vmIsTrailingZerosS1176 = _M0L6_2atmpS2965 == 1;
      } else {
        uint64_t _M0L6_2atmpS2966 = _M0Lm2vpS1173;
        _M0Lm2vpS1173 = _M0L6_2atmpS2966 - 1ull;
      }
    } else if (_M0L1qS1187 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1177
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1170, _M0L1qS1187);
    }
  }
  _M0Lm7removedS1196 = 0;
  _M0Lm16lastRemovedDigitS1197 = 0;
  _M0Lm6outputS1198 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1176 || _M0Lm17vrIsTrailingZerosS1177) {
    int32_t _if__result_4583;
    uint64_t _M0L6_2atmpS3007;
    uint64_t _M0L6_2atmpS3013;
    uint64_t _M0L6_2atmpS3014;
    int32_t _if__result_4584;
    int32_t _M0L6_2atmpS3010;
    int64_t _M0L6_2atmpS3009;
    uint64_t _M0L6_2atmpS3008;
    while (1) {
      uint64_t _M0L6_2atmpS2990 = _M0Lm2vpS1173;
      uint64_t _M0L7vpDiv10S1199 = _M0L6_2atmpS2990 / 10ull;
      uint64_t _M0L6_2atmpS2989 = _M0Lm2vmS1174;
      uint64_t _M0L7vmDiv10S1200 = _M0L6_2atmpS2989 / 10ull;
      uint64_t _M0L6_2atmpS2988;
      int32_t _M0L6_2atmpS2985;
      int32_t _M0L6_2atmpS2987;
      int32_t _M0L6_2atmpS2986;
      int32_t _M0L7vmMod10S1202;
      uint64_t _M0L6_2atmpS2984;
      uint64_t _M0L7vrDiv10S1203;
      uint64_t _M0L6_2atmpS2983;
      int32_t _M0L6_2atmpS2980;
      int32_t _M0L6_2atmpS2982;
      int32_t _M0L6_2atmpS2981;
      int32_t _M0L7vrMod10S1204;
      int32_t _M0L6_2atmpS2979;
      if (_M0L7vpDiv10S1199 <= _M0L7vmDiv10S1200) {
        break;
      }
      _M0L6_2atmpS2988 = _M0Lm2vmS1174;
      _M0L6_2atmpS2985 = (int32_t)_M0L6_2atmpS2988;
      _M0L6_2atmpS2987 = (int32_t)_M0L7vmDiv10S1200;
      _M0L6_2atmpS2986 = 10 * _M0L6_2atmpS2987;
      _M0L7vmMod10S1202 = _M0L6_2atmpS2985 - _M0L6_2atmpS2986;
      _M0L6_2atmpS2984 = _M0Lm2vrS1172;
      _M0L7vrDiv10S1203 = _M0L6_2atmpS2984 / 10ull;
      _M0L6_2atmpS2983 = _M0Lm2vrS1172;
      _M0L6_2atmpS2980 = (int32_t)_M0L6_2atmpS2983;
      _M0L6_2atmpS2982 = (int32_t)_M0L7vrDiv10S1203;
      _M0L6_2atmpS2981 = 10 * _M0L6_2atmpS2982;
      _M0L7vrMod10S1204 = _M0L6_2atmpS2980 - _M0L6_2atmpS2981;
      if (_M0Lm17vmIsTrailingZerosS1176) {
        _M0Lm17vmIsTrailingZerosS1176 = _M0L7vmMod10S1202 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1176 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1177) {
        int32_t _M0L6_2atmpS2978 = _M0Lm16lastRemovedDigitS1197;
        _M0Lm17vrIsTrailingZerosS1177 = _M0L6_2atmpS2978 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1177 = 0;
      }
      _M0Lm16lastRemovedDigitS1197 = _M0L7vrMod10S1204;
      _M0Lm2vrS1172 = _M0L7vrDiv10S1203;
      _M0Lm2vpS1173 = _M0L7vpDiv10S1199;
      _M0Lm2vmS1174 = _M0L7vmDiv10S1200;
      _M0L6_2atmpS2979 = _M0Lm7removedS1196;
      _M0Lm7removedS1196 = _M0L6_2atmpS2979 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1176) {
      while (1) {
        uint64_t _M0L6_2atmpS3003 = _M0Lm2vmS1174;
        uint64_t _M0L7vmDiv10S1205 = _M0L6_2atmpS3003 / 10ull;
        uint64_t _M0L6_2atmpS3002 = _M0Lm2vmS1174;
        int32_t _M0L6_2atmpS2999 = (int32_t)_M0L6_2atmpS3002;
        int32_t _M0L6_2atmpS3001 = (int32_t)_M0L7vmDiv10S1205;
        int32_t _M0L6_2atmpS3000 = 10 * _M0L6_2atmpS3001;
        int32_t _M0L7vmMod10S1206 = _M0L6_2atmpS2999 - _M0L6_2atmpS3000;
        uint64_t _M0L6_2atmpS2998;
        uint64_t _M0L7vpDiv10S1208;
        uint64_t _M0L6_2atmpS2997;
        uint64_t _M0L7vrDiv10S1209;
        uint64_t _M0L6_2atmpS2996;
        int32_t _M0L6_2atmpS2993;
        int32_t _M0L6_2atmpS2995;
        int32_t _M0L6_2atmpS2994;
        int32_t _M0L7vrMod10S1210;
        int32_t _M0L6_2atmpS2992;
        if (_M0L7vmMod10S1206 != 0) {
          break;
        }
        _M0L6_2atmpS2998 = _M0Lm2vpS1173;
        _M0L7vpDiv10S1208 = _M0L6_2atmpS2998 / 10ull;
        _M0L6_2atmpS2997 = _M0Lm2vrS1172;
        _M0L7vrDiv10S1209 = _M0L6_2atmpS2997 / 10ull;
        _M0L6_2atmpS2996 = _M0Lm2vrS1172;
        _M0L6_2atmpS2993 = (int32_t)_M0L6_2atmpS2996;
        _M0L6_2atmpS2995 = (int32_t)_M0L7vrDiv10S1209;
        _M0L6_2atmpS2994 = 10 * _M0L6_2atmpS2995;
        _M0L7vrMod10S1210 = _M0L6_2atmpS2993 - _M0L6_2atmpS2994;
        if (_M0Lm17vrIsTrailingZerosS1177) {
          int32_t _M0L6_2atmpS2991 = _M0Lm16lastRemovedDigitS1197;
          _M0Lm17vrIsTrailingZerosS1177 = _M0L6_2atmpS2991 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1177 = 0;
        }
        _M0Lm16lastRemovedDigitS1197 = _M0L7vrMod10S1210;
        _M0Lm2vrS1172 = _M0L7vrDiv10S1209;
        _M0Lm2vpS1173 = _M0L7vpDiv10S1208;
        _M0Lm2vmS1174 = _M0L7vmDiv10S1205;
        _M0L6_2atmpS2992 = _M0Lm7removedS1196;
        _M0Lm7removedS1196 = _M0L6_2atmpS2992 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1177) {
      int32_t _M0L6_2atmpS3006 = _M0Lm16lastRemovedDigitS1197;
      if (_M0L6_2atmpS3006 == 5) {
        uint64_t _M0L6_2atmpS3005 = _M0Lm2vrS1172;
        uint64_t _M0L6_2atmpS3004 = _M0L6_2atmpS3005 % 2ull;
        _if__result_4583 = _M0L6_2atmpS3004 == 0ull;
      } else {
        _if__result_4583 = 0;
      }
    } else {
      _if__result_4583 = 0;
    }
    if (_if__result_4583) {
      _M0Lm16lastRemovedDigitS1197 = 4;
    }
    _M0L6_2atmpS3007 = _M0Lm2vrS1172;
    _M0L6_2atmpS3013 = _M0Lm2vrS1172;
    _M0L6_2atmpS3014 = _M0Lm2vmS1174;
    if (_M0L6_2atmpS3013 == _M0L6_2atmpS3014) {
      if (!_M0L4evenS1169) {
        _if__result_4584 = 1;
      } else {
        int32_t _M0L6_2atmpS3012 = _M0Lm17vmIsTrailingZerosS1176;
        _if__result_4584 = !_M0L6_2atmpS3012;
      }
    } else {
      _if__result_4584 = 0;
    }
    if (_if__result_4584) {
      _M0L6_2atmpS3010 = 1;
    } else {
      int32_t _M0L6_2atmpS3011 = _M0Lm16lastRemovedDigitS1197;
      _M0L6_2atmpS3010 = _M0L6_2atmpS3011 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3009 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3010);
    _M0L6_2atmpS3008 = *(uint64_t*)&_M0L6_2atmpS3009;
    _M0Lm6outputS1198 = _M0L6_2atmpS3007 + _M0L6_2atmpS3008;
  } else {
    int32_t _M0Lm7roundUpS1211 = 0;
    uint64_t _M0L6_2atmpS3035 = _M0Lm2vpS1173;
    uint64_t _M0L8vpDiv100S1212 = _M0L6_2atmpS3035 / 100ull;
    uint64_t _M0L6_2atmpS3034 = _M0Lm2vmS1174;
    uint64_t _M0L8vmDiv100S1213 = _M0L6_2atmpS3034 / 100ull;
    uint64_t _M0L6_2atmpS3029;
    uint64_t _M0L6_2atmpS3032;
    uint64_t _M0L6_2atmpS3033;
    int32_t _M0L6_2atmpS3031;
    uint64_t _M0L6_2atmpS3030;
    if (_M0L8vpDiv100S1212 > _M0L8vmDiv100S1213) {
      uint64_t _M0L6_2atmpS3020 = _M0Lm2vrS1172;
      uint64_t _M0L8vrDiv100S1214 = _M0L6_2atmpS3020 / 100ull;
      uint64_t _M0L6_2atmpS3019 = _M0Lm2vrS1172;
      int32_t _M0L6_2atmpS3016 = (int32_t)_M0L6_2atmpS3019;
      int32_t _M0L6_2atmpS3018 = (int32_t)_M0L8vrDiv100S1214;
      int32_t _M0L6_2atmpS3017 = 100 * _M0L6_2atmpS3018;
      int32_t _M0L8vrMod100S1215 = _M0L6_2atmpS3016 - _M0L6_2atmpS3017;
      int32_t _M0L6_2atmpS3015;
      _M0Lm7roundUpS1211 = _M0L8vrMod100S1215 >= 50;
      _M0Lm2vrS1172 = _M0L8vrDiv100S1214;
      _M0Lm2vpS1173 = _M0L8vpDiv100S1212;
      _M0Lm2vmS1174 = _M0L8vmDiv100S1213;
      _M0L6_2atmpS3015 = _M0Lm7removedS1196;
      _M0Lm7removedS1196 = _M0L6_2atmpS3015 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3028 = _M0Lm2vpS1173;
      uint64_t _M0L7vpDiv10S1216 = _M0L6_2atmpS3028 / 10ull;
      uint64_t _M0L6_2atmpS3027 = _M0Lm2vmS1174;
      uint64_t _M0L7vmDiv10S1217 = _M0L6_2atmpS3027 / 10ull;
      uint64_t _M0L6_2atmpS3026;
      uint64_t _M0L7vrDiv10S1219;
      uint64_t _M0L6_2atmpS3025;
      int32_t _M0L6_2atmpS3022;
      int32_t _M0L6_2atmpS3024;
      int32_t _M0L6_2atmpS3023;
      int32_t _M0L7vrMod10S1220;
      int32_t _M0L6_2atmpS3021;
      if (_M0L7vpDiv10S1216 <= _M0L7vmDiv10S1217) {
        break;
      }
      _M0L6_2atmpS3026 = _M0Lm2vrS1172;
      _M0L7vrDiv10S1219 = _M0L6_2atmpS3026 / 10ull;
      _M0L6_2atmpS3025 = _M0Lm2vrS1172;
      _M0L6_2atmpS3022 = (int32_t)_M0L6_2atmpS3025;
      _M0L6_2atmpS3024 = (int32_t)_M0L7vrDiv10S1219;
      _M0L6_2atmpS3023 = 10 * _M0L6_2atmpS3024;
      _M0L7vrMod10S1220 = _M0L6_2atmpS3022 - _M0L6_2atmpS3023;
      _M0Lm7roundUpS1211 = _M0L7vrMod10S1220 >= 5;
      _M0Lm2vrS1172 = _M0L7vrDiv10S1219;
      _M0Lm2vpS1173 = _M0L7vpDiv10S1216;
      _M0Lm2vmS1174 = _M0L7vmDiv10S1217;
      _M0L6_2atmpS3021 = _M0Lm7removedS1196;
      _M0Lm7removedS1196 = _M0L6_2atmpS3021 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3029 = _M0Lm2vrS1172;
    _M0L6_2atmpS3032 = _M0Lm2vrS1172;
    _M0L6_2atmpS3033 = _M0Lm2vmS1174;
    _M0L6_2atmpS3031
    = _M0L6_2atmpS3032 == _M0L6_2atmpS3033 || _M0Lm7roundUpS1211;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3030 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3031);
    _M0Lm6outputS1198 = _M0L6_2atmpS3029 + _M0L6_2atmpS3030;
  }
  _M0L6_2atmpS3037 = _M0Lm3e10S1175;
  _M0L6_2atmpS3038 = _M0Lm7removedS1196;
  _M0L3expS1221 = _M0L6_2atmpS3037 + _M0L6_2atmpS3038;
  _M0L6_2atmpS3036 = _M0Lm6outputS1198;
  _block_4586
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4586)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4586->$0 = _M0L6_2atmpS3036;
  _block_4586->$1 = _M0L3expS1221;
  return _block_4586;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1164) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1164) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1163) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1163) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1162) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1162) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1161) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS1161 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1161 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1161 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1161 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1161 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1161 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1161 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1161 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1161 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1161 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1161 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1161 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1161 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1161 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1161 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1161 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1144) {
  int32_t _M0L6_2atmpS2937;
  int32_t _M0L6_2atmpS2936;
  int32_t _M0L4baseS1143;
  int32_t _M0L5base2S1145;
  int32_t _M0L6offsetS1146;
  int32_t _M0L6_2atmpS2935;
  uint64_t _M0L4mul0S1147;
  int32_t _M0L6_2atmpS2934;
  int32_t _M0L6_2atmpS2933;
  uint64_t _M0L4mul1S1148;
  uint64_t _M0L1mS1149;
  struct _M0TPB7Umul128 _M0L7_2abindS1150;
  uint64_t _M0L7_2alow1S1151;
  uint64_t _M0L8_2ahigh1S1152;
  struct _M0TPB7Umul128 _M0L7_2abindS1153;
  uint64_t _M0L7_2alow0S1154;
  uint64_t _M0L8_2ahigh0S1155;
  uint64_t _M0L3sumS1156;
  uint64_t _M0Lm5high1S1157;
  int32_t _M0L6_2atmpS2931;
  int32_t _M0L6_2atmpS2932;
  int32_t _M0L5deltaS1158;
  uint64_t _M0L6_2atmpS2930;
  uint64_t _M0L6_2atmpS2922;
  int32_t _M0L6_2atmpS2929;
  uint32_t _M0L6_2atmpS2926;
  int32_t _M0L6_2atmpS2928;
  int32_t _M0L6_2atmpS2927;
  uint32_t _M0L6_2atmpS2925;
  uint32_t _M0L6_2atmpS2924;
  uint64_t _M0L6_2atmpS2923;
  uint64_t _M0L1aS1159;
  uint64_t _M0L6_2atmpS2921;
  uint64_t _M0L1bS1160;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2937 = _M0L1iS1144 + 26;
  _M0L6_2atmpS2936 = _M0L6_2atmpS2937 - 1;
  _M0L4baseS1143 = _M0L6_2atmpS2936 / 26;
  _M0L5base2S1145 = _M0L4baseS1143 * 26;
  _M0L6offsetS1146 = _M0L5base2S1145 - _M0L1iS1144;
  _M0L6_2atmpS2935 = _M0L4baseS1143 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1147
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2935);
  _M0L6_2atmpS2934 = _M0L4baseS1143 * 2;
  _M0L6_2atmpS2933 = _M0L6_2atmpS2934 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1148
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2933);
  if (_M0L6offsetS1146 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1147, _M0L4mul1S1148};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1149
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1146);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1150 = _M0FPB7umul128(_M0L1mS1149, _M0L4mul1S1148);
  _M0L7_2alow1S1151 = _M0L7_2abindS1150.$0;
  _M0L8_2ahigh1S1152 = _M0L7_2abindS1150.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1153 = _M0FPB7umul128(_M0L1mS1149, _M0L4mul0S1147);
  _M0L7_2alow0S1154 = _M0L7_2abindS1153.$0;
  _M0L8_2ahigh0S1155 = _M0L7_2abindS1153.$1;
  _M0L3sumS1156 = _M0L8_2ahigh0S1155 + _M0L7_2alow1S1151;
  _M0Lm5high1S1157 = _M0L8_2ahigh1S1152;
  if (_M0L3sumS1156 < _M0L8_2ahigh0S1155) {
    uint64_t _M0L6_2atmpS2920 = _M0Lm5high1S1157;
    _M0Lm5high1S1157 = _M0L6_2atmpS2920 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2931 = _M0FPB8pow5bits(_M0L5base2S1145);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2932 = _M0FPB8pow5bits(_M0L1iS1144);
  _M0L5deltaS1158 = _M0L6_2atmpS2931 - _M0L6_2atmpS2932;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2930
  = _M0FPB13shiftright128(_M0L7_2alow0S1154, _M0L3sumS1156, _M0L5deltaS1158);
  _M0L6_2atmpS2922 = _M0L6_2atmpS2930 + 1ull;
  _M0L6_2atmpS2929 = _M0L1iS1144 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2926
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2929);
  _M0L6_2atmpS2928 = _M0L1iS1144 % 16;
  _M0L6_2atmpS2927 = _M0L6_2atmpS2928 << 1;
  _M0L6_2atmpS2925 = _M0L6_2atmpS2926 >> (_M0L6_2atmpS2927 & 31);
  _M0L6_2atmpS2924 = _M0L6_2atmpS2925 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2923 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2924);
  _M0L1aS1159 = _M0L6_2atmpS2922 + _M0L6_2atmpS2923;
  _M0L6_2atmpS2921 = _M0Lm5high1S1157;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1160
  = _M0FPB13shiftright128(_M0L3sumS1156, _M0L6_2atmpS2921, _M0L5deltaS1158);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1159, _M0L1bS1160};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1126) {
  int32_t _M0L4baseS1125;
  int32_t _M0L5base2S1127;
  int32_t _M0L6offsetS1128;
  int32_t _M0L6_2atmpS2919;
  uint64_t _M0L4mul0S1129;
  int32_t _M0L6_2atmpS2918;
  int32_t _M0L6_2atmpS2917;
  uint64_t _M0L4mul1S1130;
  uint64_t _M0L1mS1131;
  struct _M0TPB7Umul128 _M0L7_2abindS1132;
  uint64_t _M0L7_2alow1S1133;
  uint64_t _M0L8_2ahigh1S1134;
  struct _M0TPB7Umul128 _M0L7_2abindS1135;
  uint64_t _M0L7_2alow0S1136;
  uint64_t _M0L8_2ahigh0S1137;
  uint64_t _M0L3sumS1138;
  uint64_t _M0Lm5high1S1139;
  int32_t _M0L6_2atmpS2915;
  int32_t _M0L6_2atmpS2916;
  int32_t _M0L5deltaS1140;
  uint64_t _M0L6_2atmpS2907;
  int32_t _M0L6_2atmpS2914;
  uint32_t _M0L6_2atmpS2911;
  int32_t _M0L6_2atmpS2913;
  int32_t _M0L6_2atmpS2912;
  uint32_t _M0L6_2atmpS2910;
  uint32_t _M0L6_2atmpS2909;
  uint64_t _M0L6_2atmpS2908;
  uint64_t _M0L1aS1141;
  uint64_t _M0L6_2atmpS2906;
  uint64_t _M0L1bS1142;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS1125 = _M0L1iS1126 / 26;
  _M0L5base2S1127 = _M0L4baseS1125 * 26;
  _M0L6offsetS1128 = _M0L1iS1126 - _M0L5base2S1127;
  _M0L6_2atmpS2919 = _M0L4baseS1125 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1129
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2919);
  _M0L6_2atmpS2918 = _M0L4baseS1125 * 2;
  _M0L6_2atmpS2917 = _M0L6_2atmpS2918 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1130
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2917);
  if (_M0L6offsetS1128 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1129, _M0L4mul1S1130};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1131
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1128);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1132 = _M0FPB7umul128(_M0L1mS1131, _M0L4mul1S1130);
  _M0L7_2alow1S1133 = _M0L7_2abindS1132.$0;
  _M0L8_2ahigh1S1134 = _M0L7_2abindS1132.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1135 = _M0FPB7umul128(_M0L1mS1131, _M0L4mul0S1129);
  _M0L7_2alow0S1136 = _M0L7_2abindS1135.$0;
  _M0L8_2ahigh0S1137 = _M0L7_2abindS1135.$1;
  _M0L3sumS1138 = _M0L8_2ahigh0S1137 + _M0L7_2alow1S1133;
  _M0Lm5high1S1139 = _M0L8_2ahigh1S1134;
  if (_M0L3sumS1138 < _M0L8_2ahigh0S1137) {
    uint64_t _M0L6_2atmpS2905 = _M0Lm5high1S1139;
    _M0Lm5high1S1139 = _M0L6_2atmpS2905 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2915 = _M0FPB8pow5bits(_M0L1iS1126);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2916 = _M0FPB8pow5bits(_M0L5base2S1127);
  _M0L5deltaS1140 = _M0L6_2atmpS2915 - _M0L6_2atmpS2916;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2907
  = _M0FPB13shiftright128(_M0L7_2alow0S1136, _M0L3sumS1138, _M0L5deltaS1140);
  _M0L6_2atmpS2914 = _M0L1iS1126 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2911
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2914);
  _M0L6_2atmpS2913 = _M0L1iS1126 % 16;
  _M0L6_2atmpS2912 = _M0L6_2atmpS2913 << 1;
  _M0L6_2atmpS2910 = _M0L6_2atmpS2911 >> (_M0L6_2atmpS2912 & 31);
  _M0L6_2atmpS2909 = _M0L6_2atmpS2910 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2908 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2909);
  _M0L1aS1141 = _M0L6_2atmpS2907 + _M0L6_2atmpS2908;
  _M0L6_2atmpS2906 = _M0Lm5high1S1139;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1142
  = _M0FPB13shiftright128(_M0L3sumS1138, _M0L6_2atmpS2906, _M0L5deltaS1140);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1141, _M0L1bS1142};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1099,
  struct _M0TPB8Pow5Pair _M0L3mulS1096,
  int32_t _M0L1jS1112,
  int32_t _M0L7mmShiftS1114
) {
  uint64_t _M0L7_2amul0S1095;
  uint64_t _M0L7_2amul1S1097;
  uint64_t _M0L1mS1098;
  struct _M0TPB7Umul128 _M0L7_2abindS1100;
  uint64_t _M0L5_2aloS1101;
  uint64_t _M0L6_2atmpS1102;
  struct _M0TPB7Umul128 _M0L7_2abindS1103;
  uint64_t _M0L6_2alo2S1104;
  uint64_t _M0L6_2ahi2S1105;
  uint64_t _M0L3midS1106;
  uint64_t _M0L6_2atmpS2904;
  uint64_t _M0L2hiS1107;
  uint64_t _M0L3lo2S1108;
  uint64_t _M0L6_2atmpS2902;
  uint64_t _M0L6_2atmpS2903;
  uint64_t _M0L4mid2S1109;
  uint64_t _M0L6_2atmpS2901;
  uint64_t _M0L3hi2S1110;
  int32_t _M0L6_2atmpS2900;
  int32_t _M0L6_2atmpS2899;
  uint64_t _M0L2vpS1111;
  uint64_t _M0Lm2vmS1113;
  int32_t _M0L6_2atmpS2898;
  int32_t _M0L6_2atmpS2897;
  uint64_t _M0L2vrS1124;
  uint64_t _M0L6_2atmpS2896;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S1095 = _M0L3mulS1096.$0;
  _M0L7_2amul1S1097 = _M0L3mulS1096.$1;
  _M0L1mS1098 = _M0L1mS1099 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1100 = _M0FPB7umul128(_M0L1mS1098, _M0L7_2amul0S1095);
  _M0L5_2aloS1101 = _M0L7_2abindS1100.$0;
  _M0L6_2atmpS1102 = _M0L7_2abindS1100.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1103 = _M0FPB7umul128(_M0L1mS1098, _M0L7_2amul1S1097);
  _M0L6_2alo2S1104 = _M0L7_2abindS1103.$0;
  _M0L6_2ahi2S1105 = _M0L7_2abindS1103.$1;
  _M0L3midS1106 = _M0L6_2atmpS1102 + _M0L6_2alo2S1104;
  if (_M0L3midS1106 < _M0L6_2atmpS1102) {
    _M0L6_2atmpS2904 = 1ull;
  } else {
    _M0L6_2atmpS2904 = 0ull;
  }
  _M0L2hiS1107 = _M0L6_2ahi2S1105 + _M0L6_2atmpS2904;
  _M0L3lo2S1108 = _M0L5_2aloS1101 + _M0L7_2amul0S1095;
  _M0L6_2atmpS2902 = _M0L3midS1106 + _M0L7_2amul1S1097;
  if (_M0L3lo2S1108 < _M0L5_2aloS1101) {
    _M0L6_2atmpS2903 = 1ull;
  } else {
    _M0L6_2atmpS2903 = 0ull;
  }
  _M0L4mid2S1109 = _M0L6_2atmpS2902 + _M0L6_2atmpS2903;
  if (_M0L4mid2S1109 < _M0L3midS1106) {
    _M0L6_2atmpS2901 = 1ull;
  } else {
    _M0L6_2atmpS2901 = 0ull;
  }
  _M0L3hi2S1110 = _M0L2hiS1107 + _M0L6_2atmpS2901;
  _M0L6_2atmpS2900 = _M0L1jS1112 - 64;
  _M0L6_2atmpS2899 = _M0L6_2atmpS2900 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS1111
  = _M0FPB13shiftright128(_M0L4mid2S1109, _M0L3hi2S1110, _M0L6_2atmpS2899);
  _M0Lm2vmS1113 = 0ull;
  if (_M0L7mmShiftS1114) {
    uint64_t _M0L3lo3S1115 = _M0L5_2aloS1101 - _M0L7_2amul0S1095;
    uint64_t _M0L6_2atmpS2886 = _M0L3midS1106 - _M0L7_2amul1S1097;
    uint64_t _M0L6_2atmpS2887;
    uint64_t _M0L4mid3S1116;
    uint64_t _M0L6_2atmpS2885;
    uint64_t _M0L3hi3S1117;
    int32_t _M0L6_2atmpS2884;
    int32_t _M0L6_2atmpS2883;
    if (_M0L5_2aloS1101 < _M0L3lo3S1115) {
      _M0L6_2atmpS2887 = 1ull;
    } else {
      _M0L6_2atmpS2887 = 0ull;
    }
    _M0L4mid3S1116 = _M0L6_2atmpS2886 - _M0L6_2atmpS2887;
    if (_M0L3midS1106 < _M0L4mid3S1116) {
      _M0L6_2atmpS2885 = 1ull;
    } else {
      _M0L6_2atmpS2885 = 0ull;
    }
    _M0L3hi3S1117 = _M0L2hiS1107 - _M0L6_2atmpS2885;
    _M0L6_2atmpS2884 = _M0L1jS1112 - 64;
    _M0L6_2atmpS2883 = _M0L6_2atmpS2884 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS1113
    = _M0FPB13shiftright128(_M0L4mid3S1116, _M0L3hi3S1117, _M0L6_2atmpS2883);
  } else {
    uint64_t _M0L3lo3S1118 = _M0L5_2aloS1101 + _M0L5_2aloS1101;
    uint64_t _M0L6_2atmpS2894 = _M0L3midS1106 + _M0L3midS1106;
    uint64_t _M0L6_2atmpS2895;
    uint64_t _M0L4mid3S1119;
    uint64_t _M0L6_2atmpS2892;
    uint64_t _M0L6_2atmpS2893;
    uint64_t _M0L3hi3S1120;
    uint64_t _M0L3lo4S1121;
    uint64_t _M0L6_2atmpS2890;
    uint64_t _M0L6_2atmpS2891;
    uint64_t _M0L4mid4S1122;
    uint64_t _M0L6_2atmpS2889;
    uint64_t _M0L3hi4S1123;
    int32_t _M0L6_2atmpS2888;
    if (_M0L3lo3S1118 < _M0L5_2aloS1101) {
      _M0L6_2atmpS2895 = 1ull;
    } else {
      _M0L6_2atmpS2895 = 0ull;
    }
    _M0L4mid3S1119 = _M0L6_2atmpS2894 + _M0L6_2atmpS2895;
    _M0L6_2atmpS2892 = _M0L2hiS1107 + _M0L2hiS1107;
    if (_M0L4mid3S1119 < _M0L3midS1106) {
      _M0L6_2atmpS2893 = 1ull;
    } else {
      _M0L6_2atmpS2893 = 0ull;
    }
    _M0L3hi3S1120 = _M0L6_2atmpS2892 + _M0L6_2atmpS2893;
    _M0L3lo4S1121 = _M0L3lo3S1118 - _M0L7_2amul0S1095;
    _M0L6_2atmpS2890 = _M0L4mid3S1119 - _M0L7_2amul1S1097;
    if (_M0L3lo3S1118 < _M0L3lo4S1121) {
      _M0L6_2atmpS2891 = 1ull;
    } else {
      _M0L6_2atmpS2891 = 0ull;
    }
    _M0L4mid4S1122 = _M0L6_2atmpS2890 - _M0L6_2atmpS2891;
    if (_M0L4mid3S1119 < _M0L4mid4S1122) {
      _M0L6_2atmpS2889 = 1ull;
    } else {
      _M0L6_2atmpS2889 = 0ull;
    }
    _M0L3hi4S1123 = _M0L3hi3S1120 - _M0L6_2atmpS2889;
    _M0L6_2atmpS2888 = _M0L1jS1112 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS1113
    = _M0FPB13shiftright128(_M0L4mid4S1122, _M0L3hi4S1123, _M0L6_2atmpS2888);
  }
  _M0L6_2atmpS2898 = _M0L1jS1112 - 64;
  _M0L6_2atmpS2897 = _M0L6_2atmpS2898 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS1124
  = _M0FPB13shiftright128(_M0L3midS1106, _M0L2hiS1107, _M0L6_2atmpS2897);
  _M0L6_2atmpS2896 = _M0Lm2vmS1113;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS1124,
                                                _M0L2vpS1111,
                                                _M0L6_2atmpS2896};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1093,
  int32_t _M0L1pS1094
) {
  uint64_t _M0L6_2atmpS2882;
  uint64_t _M0L6_2atmpS2881;
  uint64_t _M0L6_2atmpS2880;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2882 = 1ull << (_M0L1pS1094 & 63);
  _M0L6_2atmpS2881 = _M0L6_2atmpS2882 - 1ull;
  _M0L6_2atmpS2880 = _M0L5valueS1093 & _M0L6_2atmpS2881;
  return _M0L6_2atmpS2880 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1091,
  int32_t _M0L1pS1092
) {
  int32_t _M0L6_2atmpS2879;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2879 = _M0FPB10pow5Factor(_M0L5valueS1091);
  return _M0L6_2atmpS2879 >= _M0L1pS1092;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1087) {
  uint64_t _M0L6_2atmpS2867;
  uint64_t _M0L6_2atmpS2868;
  uint64_t _M0L6_2atmpS2869;
  uint64_t _M0L6_2atmpS2870;
  int32_t _M0Lm5countS1088;
  uint64_t _M0Lm5valueS1089;
  uint64_t _M0L6_2atmpS2878;
  moonbit_string_t _M0L6_2atmpS2877;
  moonbit_string_t _M0L6_2atmpS3971;
  moonbit_string_t _M0L6_2atmpS2876;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2867 = _M0L5valueS1087 % 5ull;
  if (_M0L6_2atmpS2867 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2868 = _M0L5valueS1087 % 25ull;
  if (_M0L6_2atmpS2868 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2869 = _M0L5valueS1087 % 125ull;
  if (_M0L6_2atmpS2869 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2870 = _M0L5valueS1087 % 625ull;
  if (_M0L6_2atmpS2870 != 0ull) {
    return 3;
  }
  _M0Lm5countS1088 = 4;
  _M0Lm5valueS1089 = _M0L5valueS1087 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2871 = _M0Lm5valueS1089;
    if (_M0L6_2atmpS2871 > 0ull) {
      uint64_t _M0L6_2atmpS2873 = _M0Lm5valueS1089;
      uint64_t _M0L6_2atmpS2872 = _M0L6_2atmpS2873 % 5ull;
      uint64_t _M0L6_2atmpS2874;
      int32_t _M0L6_2atmpS2875;
      if (_M0L6_2atmpS2872 != 0ull) {
        return _M0Lm5countS1088;
      }
      _M0L6_2atmpS2874 = _M0Lm5valueS1089;
      _M0Lm5valueS1089 = _M0L6_2atmpS2874 / 5ull;
      _M0L6_2atmpS2875 = _M0Lm5countS1088;
      _M0Lm5countS1088 = _M0L6_2atmpS2875 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2878 = _M0Lm5valueS1089;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2877
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2878);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3971
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_67.data, _M0L6_2atmpS2877);
  moonbit_decref(_M0L6_2atmpS2877);
  _M0L6_2atmpS2876 = _M0L6_2atmpS3971;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2876, (moonbit_string_t)moonbit_string_literal_68.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1086,
  uint64_t _M0L2hiS1084,
  int32_t _M0L4distS1085
) {
  int32_t _M0L6_2atmpS2866;
  uint64_t _M0L6_2atmpS2864;
  uint64_t _M0L6_2atmpS2865;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2866 = 64 - _M0L4distS1085;
  _M0L6_2atmpS2864 = _M0L2hiS1084 << (_M0L6_2atmpS2866 & 63);
  _M0L6_2atmpS2865 = _M0L2loS1086 >> (_M0L4distS1085 & 63);
  return _M0L6_2atmpS2864 | _M0L6_2atmpS2865;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1074,
  uint64_t _M0L1bS1077
) {
  uint64_t _M0L3aLoS1073;
  uint64_t _M0L3aHiS1075;
  uint64_t _M0L3bLoS1076;
  uint64_t _M0L3bHiS1078;
  uint64_t _M0L1xS1079;
  uint64_t _M0L6_2atmpS2862;
  uint64_t _M0L6_2atmpS2863;
  uint64_t _M0L1yS1080;
  uint64_t _M0L6_2atmpS2860;
  uint64_t _M0L6_2atmpS2861;
  uint64_t _M0L1zS1081;
  uint64_t _M0L6_2atmpS2858;
  uint64_t _M0L6_2atmpS2859;
  uint64_t _M0L6_2atmpS2856;
  uint64_t _M0L6_2atmpS2857;
  uint64_t _M0L1wS1082;
  uint64_t _M0L2loS1083;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS1073 = _M0L1aS1074 & 4294967295ull;
  _M0L3aHiS1075 = _M0L1aS1074 >> 32;
  _M0L3bLoS1076 = _M0L1bS1077 & 4294967295ull;
  _M0L3bHiS1078 = _M0L1bS1077 >> 32;
  _M0L1xS1079 = _M0L3aLoS1073 * _M0L3bLoS1076;
  _M0L6_2atmpS2862 = _M0L3aHiS1075 * _M0L3bLoS1076;
  _M0L6_2atmpS2863 = _M0L1xS1079 >> 32;
  _M0L1yS1080 = _M0L6_2atmpS2862 + _M0L6_2atmpS2863;
  _M0L6_2atmpS2860 = _M0L3aLoS1073 * _M0L3bHiS1078;
  _M0L6_2atmpS2861 = _M0L1yS1080 & 4294967295ull;
  _M0L1zS1081 = _M0L6_2atmpS2860 + _M0L6_2atmpS2861;
  _M0L6_2atmpS2858 = _M0L3aHiS1075 * _M0L3bHiS1078;
  _M0L6_2atmpS2859 = _M0L1yS1080 >> 32;
  _M0L6_2atmpS2856 = _M0L6_2atmpS2858 + _M0L6_2atmpS2859;
  _M0L6_2atmpS2857 = _M0L1zS1081 >> 32;
  _M0L1wS1082 = _M0L6_2atmpS2856 + _M0L6_2atmpS2857;
  _M0L2loS1083 = _M0L1aS1074 * _M0L1bS1077;
  return (struct _M0TPB7Umul128){_M0L2loS1083, _M0L1wS1082};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1068,
  int32_t _M0L4fromS1072,
  int32_t _M0L2toS1070
) {
  int32_t _M0L6_2atmpS2855;
  struct _M0TPB13StringBuilder* _M0L3bufS1067;
  int32_t _M0L1iS1069;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2855 = Moonbit_array_length(_M0L5bytesS1068);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS1067 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2855);
  _M0L1iS1069 = _M0L4fromS1072;
  while (1) {
    if (_M0L1iS1069 < _M0L2toS1070) {
      int32_t _M0L6_2atmpS2853;
      int32_t _M0L6_2atmpS2852;
      int32_t _M0L6_2atmpS2854;
      if (
        _M0L1iS1069 < 0
        || _M0L1iS1069 >= Moonbit_array_length(_M0L5bytesS1068)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2853 = (int32_t)_M0L5bytesS1068[_M0L1iS1069];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2852 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2853);
      moonbit_incref(_M0L3bufS1067);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1067, _M0L6_2atmpS2852);
      _M0L6_2atmpS2854 = _M0L1iS1069 + 1;
      _M0L1iS1069 = _M0L6_2atmpS2854;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS1068);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1067);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1066) {
  int32_t _M0L6_2atmpS2851;
  uint32_t _M0L6_2atmpS2850;
  uint32_t _M0L6_2atmpS2849;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2851 = _M0L1eS1066 * 78913;
  _M0L6_2atmpS2850 = *(uint32_t*)&_M0L6_2atmpS2851;
  _M0L6_2atmpS2849 = _M0L6_2atmpS2850 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2849;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1065) {
  int32_t _M0L6_2atmpS2848;
  uint32_t _M0L6_2atmpS2847;
  uint32_t _M0L6_2atmpS2846;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2848 = _M0L1eS1065 * 732923;
  _M0L6_2atmpS2847 = *(uint32_t*)&_M0L6_2atmpS2848;
  _M0L6_2atmpS2846 = _M0L6_2atmpS2847 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2846;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1063,
  int32_t _M0L8exponentS1064,
  int32_t _M0L8mantissaS1061
) {
  moonbit_string_t _M0L1sS1062;
  moonbit_string_t _M0L6_2atmpS3972;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1061) {
    return (moonbit_string_t)moonbit_string_literal_69.data;
  }
  if (_M0L4signS1063) {
    _M0L1sS1062 = (moonbit_string_t)moonbit_string_literal_70.data;
  } else {
    _M0L1sS1062 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS1064) {
    moonbit_string_t _M0L6_2atmpS3973;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3973
    = moonbit_add_string(_M0L1sS1062, (moonbit_string_t)moonbit_string_literal_71.data);
    moonbit_decref(_M0L1sS1062);
    return _M0L6_2atmpS3973;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3972
  = moonbit_add_string(_M0L1sS1062, (moonbit_string_t)moonbit_string_literal_72.data);
  moonbit_decref(_M0L1sS1062);
  return _M0L6_2atmpS3972;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1060) {
  int32_t _M0L6_2atmpS2845;
  uint32_t _M0L6_2atmpS2844;
  uint32_t _M0L6_2atmpS2843;
  int32_t _M0L6_2atmpS2842;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2845 = _M0L1eS1060 * 1217359;
  _M0L6_2atmpS2844 = *(uint32_t*)&_M0L6_2atmpS2845;
  _M0L6_2atmpS2843 = _M0L6_2atmpS2844 >> 19;
  _M0L6_2atmpS2842 = *(int32_t*)&_M0L6_2atmpS2843;
  return _M0L6_2atmpS2842 + 1;
}

int32_t _M0MPC16double6Double7to__int(double _M0L4selfS1059) {
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_to_int.mbt"
  if (_M0L4selfS1059 != _M0L4selfS1059) {
    return 0;
  } else if (_M0L4selfS1059 >= 0x1.fffffffcp+30) {
    return 2147483647;
  } else if (_M0L4selfS1059 <= -0x1p+31) {
    return (int32_t)0x80000000;
  } else {
    return (int32_t)_M0L4selfS1059;
  }
}

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t _M0L1xS1054,
  moonbit_string_t _M0L3msgS1056,
  moonbit_string_t _M0L3locS1058
) {
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (!_M0L1xS1054) {
    moonbit_string_t _M0L9fail__msgS1055;
    if (_M0L3msgS1056 == 0) {
      moonbit_string_t _M0L6_2atmpS2840;
      moonbit_string_t _M0L6_2atmpS3975;
      moonbit_string_t _M0L6_2atmpS2839;
      moonbit_string_t _M0L6_2atmpS3974;
      if (_M0L3msgS1056) {
        moonbit_decref(_M0L3msgS1056);
      }
      #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2840
      = _M0IP016_24default__implPB4Show10to__stringGbE(_M0L1xS1054);
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS3975
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS2840);
      moonbit_decref(_M0L6_2atmpS2840);
      _M0L6_2atmpS2839 = _M0L6_2atmpS3975;
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS3974
      = moonbit_add_string(_M0L6_2atmpS2839, (moonbit_string_t)moonbit_string_literal_74.data);
      moonbit_decref(_M0L6_2atmpS2839);
      _M0L9fail__msgS1055 = _M0L6_2atmpS3974;
    } else {
      moonbit_string_t _M0L7_2aSomeS1057 = _M0L3msgS1056;
      _M0L9fail__msgS1055 = _M0L7_2aSomeS1057;
    }
    #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS1055, _M0L3locS1058);
  } else {
    int32_t _M0L6_2atmpS2841;
    struct moonbit_result_0 _result_4589;
    moonbit_decref(_M0L3locS1058);
    if (_M0L3msgS1056) {
      moonbit_decref(_M0L3msgS1056);
    }
    _M0L6_2atmpS2841 = 0;
    _result_4589.tag = 1;
    _result_4589.data.ok = _M0L6_2atmpS2841;
    return _result_4589;
  }
}

struct moonbit_result_0 _M0FPB13assert__false(
  int32_t _M0L1xS1049,
  moonbit_string_t _M0L3msgS1051,
  moonbit_string_t _M0L3locS1053
) {
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (_M0L1xS1049) {
    moonbit_string_t _M0L9fail__msgS1050;
    if (_M0L3msgS1051 == 0) {
      moonbit_string_t _M0L6_2atmpS2837;
      moonbit_string_t _M0L6_2atmpS3977;
      moonbit_string_t _M0L6_2atmpS2836;
      moonbit_string_t _M0L6_2atmpS3976;
      if (_M0L3msgS1051) {
        moonbit_decref(_M0L3msgS1051);
      }
      #line 160 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2837
      = _M0IP016_24default__implPB4Show10to__stringGbE(_M0L1xS1049);
      #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS3977
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS2837);
      moonbit_decref(_M0L6_2atmpS2837);
      _M0L6_2atmpS2836 = _M0L6_2atmpS3977;
      #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS3976
      = moonbit_add_string(_M0L6_2atmpS2836, (moonbit_string_t)moonbit_string_literal_75.data);
      moonbit_decref(_M0L6_2atmpS2836);
      _M0L9fail__msgS1050 = _M0L6_2atmpS3976;
    } else {
      moonbit_string_t _M0L7_2aSomeS1052 = _M0L3msgS1051;
      _M0L9fail__msgS1050 = _M0L7_2aSomeS1052;
    }
    #line 162 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS1050, _M0L3locS1053);
  } else {
    int32_t _M0L6_2atmpS2838;
    struct moonbit_result_0 _result_4590;
    moonbit_decref(_M0L3locS1053);
    if (_M0L3msgS1051) {
      moonbit_decref(_M0L3msgS1051);
    }
    _M0L6_2atmpS2838 = 0;
    _result_4590.tag = 1;
    _result_4590.data.ok = _M0L6_2atmpS2838;
    return _result_4590;
  }
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS1048,
  struct _M0TPB6Hasher* _M0L6hasherS1047
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS1047, _M0L4selfS1048);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS1046,
  struct _M0TPB6Hasher* _M0L6hasherS1045
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS1045, _M0L4selfS1046);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS1043,
  moonbit_string_t _M0L5valueS1041
) {
  int32_t _M0L7_2abindS1040;
  int32_t _M0L1iS1042;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS1040 = Moonbit_array_length(_M0L5valueS1041);
  _M0L1iS1042 = 0;
  while (1) {
    if (_M0L1iS1042 < _M0L7_2abindS1040) {
      int32_t _M0L6_2atmpS2834 = _M0L5valueS1041[_M0L1iS1042];
      int32_t _M0L6_2atmpS2833 = (int32_t)_M0L6_2atmpS2834;
      uint32_t _M0L6_2atmpS2832 = *(uint32_t*)&_M0L6_2atmpS2833;
      int32_t _M0L6_2atmpS2835;
      moonbit_incref(_M0L4selfS1043);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS1043, _M0L6_2atmpS2832);
      _M0L6_2atmpS2835 = _M0L1iS1042 + 1;
      _M0L1iS1042 = _M0L6_2atmpS2835;
      continue;
    } else {
      moonbit_decref(_M0L4selfS1043);
      moonbit_decref(_M0L5valueS1041);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS1038,
  int32_t _M0L3idxS1039
) {
  int32_t _M0L6_2atmpS3978;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3978 = _M0L4selfS1038[_M0L3idxS1039];
  moonbit_decref(_M0L4selfS1038);
  return _M0L6_2atmpS3978;
}

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB5Iter24nextGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS1035
) {
  #line 904 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  #line 905 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPB4Iter4nextGUsRP48clawteam8clawteam8internal6schema6SchemaEE(_M0L4selfS1035);
}

struct _M0TUsRPB4JsonE* _M0MPB5Iter24nextGsRPB4JsonE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS1036
) {
  #line 904 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  #line 905 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L4selfS1036);
}

struct _M0TUsRPB3MapGsRPB4JsonEE* _M0MPB5Iter24nextGsRPB3MapGsRPB4JsonEE(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L4selfS1037
) {
  #line 904 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  #line 905 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPB4Iter4nextGUsRPB3MapGsRPB4JsonEEE(_M0L4selfS1037);
}

int32_t _M0MPB4Iter3allGRPB4JsonE(
  struct _M0TWEORPB4Json* _M0L4selfS1030,
  struct _M0TWRPB4JsonEb* _M0L1fS1033
) {
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  while (1) {
    void* _M0L7_2abindS1029;
    moonbit_incref(_M0L4selfS1030);
    #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L7_2abindS1029 = _M0MPB4Iter4nextGRPB4JsonE(_M0L4selfS1030);
    if (_M0L7_2abindS1029 == 0) {
      moonbit_decref(_M0L1fS1033);
      moonbit_decref(_M0L4selfS1030);
      if (_M0L7_2abindS1029) {
        moonbit_decref(_M0L7_2abindS1029);
      }
      return 1;
    } else {
      void* _M0L7_2aSomeS1031 = _M0L7_2abindS1029;
      void* _M0L4_2axS1032 = _M0L7_2aSomeS1031;
      moonbit_incref(_M0L1fS1033);
      #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      if (_M0L1fS1033->code(_M0L1fS1033, _M0L4_2axS1032)) {
        
      } else {
        moonbit_decref(_M0L1fS1033);
        moonbit_decref(_M0L4selfS1030);
        return 0;
      }
      continue;
    }
    break;
  }
}

void* _M0IPB3MapPB6ToJson8to__jsonGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS1012
) {
  int32_t _M0L8capacityS2828;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS1011;
  struct _M0TWEOUsRPB4JsonE* _M0L5_2aitS1013;
  void* _block_4594;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L8capacityS2828 = _M0L4selfS1012->$2;
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6objectS1011 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L8capacityS2828);
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L5_2aitS1013 = _M0MPB3Map5iter2GsRPB4JsonE(_M0L4selfS1012);
  while (1) {
    struct _M0TUsRPB4JsonE* _M0L7_2abindS1014;
    moonbit_incref(_M0L5_2aitS1013);
    #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L7_2abindS1014 = _M0MPB5Iter24nextGsRPB4JsonE(_M0L5_2aitS1013);
    if (_M0L7_2abindS1014 == 0) {
      if (_M0L7_2abindS1014) {
        moonbit_decref(_M0L7_2abindS1014);
      }
      moonbit_decref(_M0L5_2aitS1013);
    } else {
      struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1015 = _M0L7_2abindS1014;
      struct _M0TUsRPB4JsonE* _M0L4_2axS1016 = _M0L7_2aSomeS1015;
      moonbit_string_t _M0L8_2afieldS3980 = _M0L4_2axS1016->$0;
      moonbit_string_t _M0L4_2akS1017 = _M0L8_2afieldS3980;
      void* _M0L8_2afieldS3979 = _M0L4_2axS1016->$1;
      int32_t _M0L6_2acntS4392 = Moonbit_object_header(_M0L4_2axS1016)->rc;
      void* _M0L4_2avS1018;
      moonbit_string_t _M0L6_2atmpS2826;
      void* _M0L6_2atmpS2827;
      if (_M0L6_2acntS4392 > 1) {
        int32_t _M0L11_2anew__cntS4393 = _M0L6_2acntS4392 - 1;
        Moonbit_object_header(_M0L4_2axS1016)->rc = _M0L11_2anew__cntS4393;
        moonbit_incref(_M0L8_2afieldS3979);
        moonbit_incref(_M0L4_2akS1017);
      } else if (_M0L6_2acntS4392 == 1) {
        #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L4_2axS1016);
      }
      _M0L4_2avS1018 = _M0L8_2afieldS3979;
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2826
      = _M0IPC16string6StringPB4Show10to__string(_M0L4_2akS1017);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2827 = _M0IPC14json4JsonPB6ToJson8to__json(_M0L4_2avS1018);
      moonbit_incref(_M0L6objectS1011);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS1011, _M0L6_2atmpS2826, _M0L6_2atmpS2827);
      continue;
    }
    break;
  }
  _block_4594 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_4594)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_4594)->$0 = _M0L6objectS1011;
  return _block_4594;
}

void* _M0IPB3MapPB6ToJson8to__jsonGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS1021
) {
  int32_t _M0L8capacityS2831;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS1020;
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L5_2aitS1022;
  void* _block_4596;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L8capacityS2831 = _M0L4selfS1021->$2;
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6objectS1020 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L8capacityS2831);
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L5_2aitS1022 = _M0MPB3Map5iter2GsRPB3MapGsRPB4JsonEE(_M0L4selfS1021);
  while (1) {
    struct _M0TUsRPB3MapGsRPB4JsonEE* _M0L7_2abindS1023;
    moonbit_incref(_M0L5_2aitS1022);
    #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L7_2abindS1023
    = _M0MPB5Iter24nextGsRPB3MapGsRPB4JsonEE(_M0L5_2aitS1022);
    if (_M0L7_2abindS1023 == 0) {
      if (_M0L7_2abindS1023) {
        moonbit_decref(_M0L7_2abindS1023);
      }
      moonbit_decref(_M0L5_2aitS1022);
    } else {
      struct _M0TUsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS1024 = _M0L7_2abindS1023;
      struct _M0TUsRPB3MapGsRPB4JsonEE* _M0L4_2axS1025 = _M0L7_2aSomeS1024;
      moonbit_string_t _M0L8_2afieldS3982 = _M0L4_2axS1025->$0;
      moonbit_string_t _M0L4_2akS1026 = _M0L8_2afieldS3982;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3981 = _M0L4_2axS1025->$1;
      int32_t _M0L6_2acntS4394 = Moonbit_object_header(_M0L4_2axS1025)->rc;
      struct _M0TPB3MapGsRPB4JsonE* _M0L4_2avS1027;
      moonbit_string_t _M0L6_2atmpS2829;
      void* _M0L6_2atmpS2830;
      if (_M0L6_2acntS4394 > 1) {
        int32_t _M0L11_2anew__cntS4395 = _M0L6_2acntS4394 - 1;
        Moonbit_object_header(_M0L4_2axS1025)->rc = _M0L11_2anew__cntS4395;
        moonbit_incref(_M0L8_2afieldS3981);
        moonbit_incref(_M0L4_2akS1026);
      } else if (_M0L6_2acntS4394 == 1) {
        #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L4_2axS1025);
      }
      _M0L4_2avS1027 = _M0L8_2afieldS3981;
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2829
      = _M0IPC16string6StringPB4Show10to__string(_M0L4_2akS1026);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2830
      = _M0IPB3MapPB6ToJson8to__jsonGsRPB4JsonE(_M0L4_2avS1027);
      moonbit_incref(_M0L6objectS1020);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS1020, _M0L6_2atmpS2829, _M0L6_2atmpS2830);
      continue;
    }
    break;
  }
  _block_4596 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_4596)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_4596)->$0 = _M0L6objectS1020;
  return _block_4596;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGsRPB4JsonE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1005,
  struct _M0TWsERPB4Json* _M0L1fS1009
) {
  int32_t _M0L3lenS2825;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS1004;
  int32_t _M0L7_2abindS1006;
  int32_t _M0L1iS1007;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2825 = _M0L4selfS1005->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS1004 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2825);
  _M0L7_2abindS1006 = _M0L4selfS1005->$1;
  _M0L1iS1007 = 0;
  while (1) {
    if (_M0L1iS1007 < _M0L7_2abindS1006) {
      moonbit_string_t* _M0L8_2afieldS3986 = _M0L4selfS1005->$0;
      moonbit_string_t* _M0L3bufS2824 = _M0L8_2afieldS3986;
      moonbit_string_t _M0L6_2atmpS3985 =
        (moonbit_string_t)_M0L3bufS2824[_M0L1iS1007];
      moonbit_string_t _M0L1vS1008 = _M0L6_2atmpS3985;
      void** _M0L8_2afieldS3984 = _M0L3arrS1004->$0;
      void** _M0L3bufS2821 = _M0L8_2afieldS3984;
      void* _M0L6_2atmpS2822;
      void* _M0L6_2aoldS3983;
      int32_t _M0L6_2atmpS2823;
      moonbit_incref(_M0L3bufS2821);
      moonbit_incref(_M0L1fS1009);
      moonbit_incref(_M0L1vS1008);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2822 = _M0L1fS1009->code(_M0L1fS1009, _M0L1vS1008);
      _M0L6_2aoldS3983 = (void*)_M0L3bufS2821[_M0L1iS1007];
      moonbit_decref(_M0L6_2aoldS3983);
      _M0L3bufS2821[_M0L1iS1007] = _M0L6_2atmpS2822;
      moonbit_decref(_M0L3bufS2821);
      _M0L6_2atmpS2823 = _M0L1iS1007 + 1;
      _M0L1iS1007 = _M0L6_2atmpS2823;
      continue;
    } else {
      moonbit_decref(_M0L1fS1009);
      moonbit_decref(_M0L4selfS1005);
    }
    break;
  }
  return _M0L3arrS1004;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS1003) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS1003;
}

void* _M0MPC14json4Json6object(
  struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS1002
) {
  void* _block_4598;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4598 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_4598)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_4598)->$0 = _M0L6objectS1002;
  return _block_4598;
}

void* _M0MPC14json4Json7boolean(int32_t _M0L7booleanS1001) {
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L7booleanS1001) {
    return (struct moonbit_object*)&moonbit_constant_constructor_1 + 1;
  } else {
    return (struct moonbit_object*)&moonbit_constant_constructor_2 + 1;
  }
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS1000) {
  void* _block_4599;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4599 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4599)->$0 = _M0L6stringS1000;
  return _block_4599;
}

int32_t _M0IPC14json4JsonPB2Eq5equal(void* _M0L1aS982, void* _M0L1bS983) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  switch (Moonbit_object_tag(_M0L1aS982)) {
    case 0: {
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 0: {
          return 1;
          break;
        }
        default: {
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 1: {
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 1: {
          return 1;
          break;
        }
        default: {
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 2: {
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 2: {
          return 1;
          break;
        }
        default: {
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 3: {
      struct _M0DTPB4Json6Number* _M0L9_2aNumberS984 =
        (struct _M0DTPB4Json6Number*)_M0L1aS982;
      double _M0L8_2afieldS3988 = _M0L9_2aNumberS984->$0;
      double _M0L9_2aa__numS985;
      moonbit_decref(_M0L9_2aNumberS984);
      _M0L9_2aa__numS985 = _M0L8_2afieldS3988;
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS986 =
            (struct _M0DTPB4Json6Number*)_M0L1bS983;
          double _M0L8_2afieldS3987 = _M0L9_2aNumberS986->$0;
          double _M0L9_2ab__numS987;
          moonbit_decref(_M0L9_2aNumberS986);
          _M0L9_2ab__numS987 = _M0L8_2afieldS3987;
          return _M0L9_2aa__numS985 == _M0L9_2ab__numS987;
          break;
        }
        default: {
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 4: {
      struct _M0DTPB4Json6String* _M0L9_2aStringS988 =
        (struct _M0DTPB4Json6String*)_M0L1aS982;
      moonbit_string_t _M0L8_2afieldS3991 = _M0L9_2aStringS988->$0;
      int32_t _M0L6_2acntS4396 =
        Moonbit_object_header(_M0L9_2aStringS988)->rc;
      moonbit_string_t _M0L9_2aa__strS989;
      if (_M0L6_2acntS4396 > 1) {
        int32_t _M0L11_2anew__cntS4397 = _M0L6_2acntS4396 - 1;
        Moonbit_object_header(_M0L9_2aStringS988)->rc
        = _M0L11_2anew__cntS4397;
        moonbit_incref(_M0L8_2afieldS3991);
      } else if (_M0L6_2acntS4396 == 1) {
        #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L9_2aStringS988);
      }
      _M0L9_2aa__strS989 = _M0L8_2afieldS3991;
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS990 =
            (struct _M0DTPB4Json6String*)_M0L1bS983;
          moonbit_string_t _M0L8_2afieldS3990 = _M0L9_2aStringS990->$0;
          int32_t _M0L6_2acntS4398 =
            Moonbit_object_header(_M0L9_2aStringS990)->rc;
          moonbit_string_t _M0L9_2ab__strS991;
          int32_t _M0L6_2atmpS3989;
          if (_M0L6_2acntS4398 > 1) {
            int32_t _M0L11_2anew__cntS4399 = _M0L6_2acntS4398 - 1;
            Moonbit_object_header(_M0L9_2aStringS990)->rc
            = _M0L11_2anew__cntS4399;
            moonbit_incref(_M0L8_2afieldS3990);
          } else if (_M0L6_2acntS4398 == 1) {
            #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
            moonbit_free(_M0L9_2aStringS990);
          }
          _M0L9_2ab__strS991 = _M0L8_2afieldS3990;
          #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
          _M0L6_2atmpS3989
          = moonbit_val_array_equal(_M0L9_2aa__strS989, _M0L9_2ab__strS991);
          moonbit_decref(_M0L9_2aa__strS989);
          moonbit_decref(_M0L9_2ab__strS991);
          return _M0L6_2atmpS3989;
          break;
        }
        default: {
          moonbit_decref(_M0L9_2aa__strS989);
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
    
    case 5: {
      struct _M0DTPB4Json5Array* _M0L8_2aArrayS992 =
        (struct _M0DTPB4Json5Array*)_M0L1aS982;
      struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3993 =
        _M0L8_2aArrayS992->$0;
      int32_t _M0L6_2acntS4400 = Moonbit_object_header(_M0L8_2aArrayS992)->rc;
      struct _M0TPB5ArrayGRPB4JsonE* _M0L9_2aa__arrS993;
      if (_M0L6_2acntS4400 > 1) {
        int32_t _M0L11_2anew__cntS4401 = _M0L6_2acntS4400 - 1;
        Moonbit_object_header(_M0L8_2aArrayS992)->rc = _M0L11_2anew__cntS4401;
        moonbit_incref(_M0L8_2afieldS3993);
      } else if (_M0L6_2acntS4400 == 1) {
        #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L8_2aArrayS992);
      }
      _M0L9_2aa__arrS993 = _M0L8_2afieldS3993;
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS994 =
            (struct _M0DTPB4Json5Array*)_M0L1bS983;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3992 =
            _M0L8_2aArrayS994->$0;
          int32_t _M0L6_2acntS4402 =
            Moonbit_object_header(_M0L8_2aArrayS994)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L9_2ab__arrS995;
          if (_M0L6_2acntS4402 > 1) {
            int32_t _M0L11_2anew__cntS4403 = _M0L6_2acntS4402 - 1;
            Moonbit_object_header(_M0L8_2aArrayS994)->rc
            = _M0L11_2anew__cntS4403;
            moonbit_incref(_M0L8_2afieldS3992);
          } else if (_M0L6_2acntS4402 == 1) {
            #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
            moonbit_free(_M0L8_2aArrayS994);
          }
          _M0L9_2ab__arrS995 = _M0L8_2afieldS3992;
          #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
          return _M0IPC15array5ArrayPB2Eq5equalGRPB4JsonE(_M0L9_2aa__arrS993, _M0L9_2ab__arrS995);
          break;
        }
        default: {
          moonbit_decref(_M0L9_2aa__arrS993);
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
    default: {
      struct _M0DTPB4Json6Object* _M0L9_2aObjectS996 =
        (struct _M0DTPB4Json6Object*)_M0L1aS982;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3995 =
        _M0L9_2aObjectS996->$0;
      int32_t _M0L6_2acntS4404 =
        Moonbit_object_header(_M0L9_2aObjectS996)->rc;
      struct _M0TPB3MapGsRPB4JsonE* _M0L9_2aa__objS997;
      if (_M0L6_2acntS4404 > 1) {
        int32_t _M0L11_2anew__cntS4405 = _M0L6_2acntS4404 - 1;
        Moonbit_object_header(_M0L9_2aObjectS996)->rc
        = _M0L11_2anew__cntS4405;
        moonbit_incref(_M0L8_2afieldS3995);
      } else if (_M0L6_2acntS4404 == 1) {
        #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L9_2aObjectS996);
      }
      _M0L9_2aa__objS997 = _M0L8_2afieldS3995;
      switch (Moonbit_object_tag(_M0L1bS983)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS998 =
            (struct _M0DTPB4Json6Object*)_M0L1bS983;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3994 =
            _M0L9_2aObjectS998->$0;
          int32_t _M0L6_2acntS4406 =
            Moonbit_object_header(_M0L9_2aObjectS998)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L9_2ab__objS999;
          if (_M0L6_2acntS4406 > 1) {
            int32_t _M0L11_2anew__cntS4407 = _M0L6_2acntS4406 - 1;
            Moonbit_object_header(_M0L9_2aObjectS998)->rc
            = _M0L11_2anew__cntS4407;
            moonbit_incref(_M0L8_2afieldS3994);
          } else if (_M0L6_2acntS4406 == 1) {
            #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
            moonbit_free(_M0L9_2aObjectS998);
          }
          _M0L9_2ab__objS999 = _M0L8_2afieldS3994;
          #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
          return _M0IPB3MapPB2Eq5equalGsRPB4JsonE(_M0L9_2aa__objS997, _M0L9_2ab__objS999);
          break;
        }
        default: {
          moonbit_decref(_M0L9_2aa__objS997);
          moonbit_decref(_M0L1bS983);
          return 0;
          break;
        }
      }
      break;
    }
  }
}

int32_t _M0IPB3MapPB2Eq5equalGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS973,
  struct _M0TPB3MapGsRPB4JsonE* _M0L4thatS974
) {
  int32_t _M0L4sizeS2819;
  int32_t _M0L4sizeS2820;
  #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4sizeS2819 = _M0L4selfS973->$1;
  _M0L4sizeS2820 = _M0L4thatS974->$1;
  if (_M0L4sizeS2819 == _M0L4sizeS2820) {
    struct _M0TWEOUsRPB4JsonE* _M0L5_2aitS975;
    #line 660 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    _M0L5_2aitS975 = _M0MPB3Map5iter2GsRPB4JsonE(_M0L4selfS973);
    while (1) {
      struct _M0TUsRPB4JsonE* _M0L7_2abindS976;
      moonbit_incref(_M0L5_2aitS975);
      #line 661 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L7_2abindS976 = _M0MPB5Iter24nextGsRPB4JsonE(_M0L5_2aitS975);
      if (_M0L7_2abindS976 == 0) {
        if (_M0L7_2abindS976) {
          moonbit_decref(_M0L7_2abindS976);
        }
        moonbit_decref(_M0L5_2aitS975);
        moonbit_decref(_M0L4thatS974);
        return 1;
      } else {
        struct _M0TUsRPB4JsonE* _M0L7_2aSomeS977 = _M0L7_2abindS976;
        struct _M0TUsRPB4JsonE* _M0L4_2axS978 = _M0L7_2aSomeS977;
        moonbit_string_t _M0L8_2afieldS3997 = _M0L4_2axS978->$0;
        moonbit_string_t _M0L4_2akS979 = _M0L8_2afieldS3997;
        void* _M0L8_2afieldS3996 = _M0L4_2axS978->$1;
        int32_t _M0L6_2acntS4408 = Moonbit_object_header(_M0L4_2axS978)->rc;
        void* _M0L4_2avS980;
        if (_M0L6_2acntS4408 > 1) {
          int32_t _M0L11_2anew__cntS4409 = _M0L6_2acntS4408 - 1;
          Moonbit_object_header(_M0L4_2axS978)->rc = _M0L11_2anew__cntS4409;
          moonbit_incref(_M0L8_2afieldS3996);
          moonbit_incref(_M0L4_2akS979);
        } else if (_M0L6_2acntS4408 == 1) {
          #line 661 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L4_2axS978);
        }
        _M0L4_2avS980 = _M0L8_2afieldS3996;
        moonbit_incref(_M0L4thatS974);
        #line 662 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        if (
          _M0MPB3Map12contains__kvGsRPB4JsonE(_M0L4thatS974, _M0L4_2akS979, _M0L4_2avS980)
        ) {
          
        } else {
          moonbit_decref(_M0L5_2aitS975);
          moonbit_decref(_M0L4thatS974);
          return 0;
        }
        continue;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4thatS974);
    moonbit_decref(_M0L4selfS973);
    return 0;
  }
}

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map5iter2GsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS970
) {
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS970);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map5iter2GsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS971
) {
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRPB4JsonE(_M0L4selfS971);
}

struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0MPB3Map5iter2GsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS972
) {
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRPB3MapGsRPB4JsonEE(_M0L4selfS972);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS947
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3998;
  int32_t _M0L6_2acntS4410;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2806;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS946;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__* _closure_4601;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2801;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3998 = _M0L4selfS947->$5;
  _M0L6_2acntS4410 = Moonbit_object_header(_M0L4selfS947)->rc;
  if (_M0L6_2acntS4410 > 1) {
    int32_t _M0L11_2anew__cntS4412 = _M0L6_2acntS4410 - 1;
    Moonbit_object_header(_M0L4selfS947)->rc = _M0L11_2anew__cntS4412;
    if (_M0L8_2afieldS3998) {
      moonbit_incref(_M0L8_2afieldS3998);
    }
  } else if (_M0L6_2acntS4410 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4411 = _M0L4selfS947->$0;
    moonbit_decref(_M0L8_2afieldS4411);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS947);
  }
  _M0L4headS2806 = _M0L8_2afieldS3998;
  _M0L11curr__entryS946
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS946)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS946->$0 = _M0L4headS2806;
  _closure_4601
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__));
  Moonbit_object_header(_closure_4601)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__, $0) >> 2, 1, 0);
  _closure_4601->code = &_M0MPB3Map4iterGsRPB4JsonEC2802l591;
  _closure_4601->$0 = _M0L11curr__entryS946;
  _M0L6_2atmpS2801 = (struct _M0TWEOUsRPB4JsonE*)_closure_4601;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2801);
}

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS955
) {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS3999;
  int32_t _M0L6_2acntS4413;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4headS2812;
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE* _M0L11curr__entryS954;
  struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__* _closure_4602;
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2807;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3999 = _M0L4selfS955->$5;
  _M0L6_2acntS4413 = Moonbit_object_header(_M0L4selfS955)->rc;
  if (_M0L6_2acntS4413 > 1) {
    int32_t _M0L11_2anew__cntS4415 = _M0L6_2acntS4413 - 1;
    Moonbit_object_header(_M0L4selfS955)->rc = _M0L11_2anew__cntS4415;
    if (_M0L8_2afieldS3999) {
      moonbit_incref(_M0L8_2afieldS3999);
    }
  } else if (_M0L6_2acntS4413 == 1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4414 =
      _M0L4selfS955->$0;
    moonbit_decref(_M0L8_2afieldS4414);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS955);
  }
  _M0L4headS2812 = _M0L8_2afieldS3999;
  _M0L11curr__entryS954
  = (struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE));
  Moonbit_object_header(_M0L11curr__entryS954)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS954->$0 = _M0L4headS2812;
  _closure_4602
  = (struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__*)moonbit_malloc(sizeof(struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__));
  Moonbit_object_header(_closure_4602)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__, $0) >> 2, 1, 0);
  _closure_4602->code
  = &_M0MPB3Map4iterGsRP48clawteam8clawteam8internal6schema6SchemaEC2808l591;
  _closure_4602->$0 = _M0L11curr__entryS954;
  _M0L6_2atmpS2807
  = (struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE*)_closure_4602;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRP48clawteam8clawteam8internal6schema6SchemaEE(_M0L6_2atmpS2807);
}

struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0MPB3Map4iterGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS963
) {
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2afieldS4000;
  int32_t _M0L6_2acntS4416;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L4headS2818;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE* _M0L11curr__entryS962;
  struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__* _closure_4603;
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2813;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4000 = _M0L4selfS963->$5;
  _M0L6_2acntS4416 = Moonbit_object_header(_M0L4selfS963)->rc;
  if (_M0L6_2acntS4416 > 1) {
    int32_t _M0L11_2anew__cntS4418 = _M0L6_2acntS4416 - 1;
    Moonbit_object_header(_M0L4selfS963)->rc = _M0L11_2anew__cntS4418;
    if (_M0L8_2afieldS4000) {
      moonbit_incref(_M0L8_2afieldS4000);
    }
  } else if (_M0L6_2acntS4416 == 1) {
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L8_2afieldS4417 =
      _M0L4selfS963->$0;
    moonbit_decref(_M0L8_2afieldS4417);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS963);
  }
  _M0L4headS2818 = _M0L8_2afieldS4000;
  _M0L11curr__entryS962
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE));
  Moonbit_object_header(_M0L11curr__entryS962)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS962->$0 = _M0L4headS2818;
  _closure_4603
  = (struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__*)moonbit_malloc(sizeof(struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__));
  Moonbit_object_header(_closure_4603)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__, $0) >> 2, 1, 0);
  _closure_4603->code = &_M0MPB3Map4iterGsRPB3MapGsRPB4JsonEEC2814l591;
  _closure_4603->$0 = _M0L11curr__entryS962;
  _M0L6_2atmpS2813 = (struct _M0TWEOUsRPB3MapGsRPB4JsonEE*)_closure_4603;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB3MapGsRPB4JsonEEE(_M0L6_2atmpS2813);
}

struct _M0TUsRPB3MapGsRPB4JsonEE* _M0MPB3Map4iterGsRPB3MapGsRPB4JsonEEC2814l591(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L6_2aenvS2815
) {
  struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__* _M0L14_2acasted__envS2816;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE* _M0L8_2afieldS4006;
  int32_t _M0L6_2acntS4419;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB3MapGsRPB4JsonEEE* _M0L11curr__entryS962;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2afieldS4005;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS964;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2816
  = (struct _M0R146Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_5d_7c_2eanon__u2814__l591__*)_M0L6_2aenvS2815;
  _M0L8_2afieldS4006 = _M0L14_2acasted__envS2816->$0;
  _M0L6_2acntS4419 = Moonbit_object_header(_M0L14_2acasted__envS2816)->rc;
  if (_M0L6_2acntS4419 > 1) {
    int32_t _M0L11_2anew__cntS4420 = _M0L6_2acntS4419 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2816)->rc
    = _M0L11_2anew__cntS4420;
    moonbit_incref(_M0L8_2afieldS4006);
  } else if (_M0L6_2acntS4419 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2816);
  }
  _M0L11curr__entryS962 = _M0L8_2afieldS4006;
  _M0L8_2afieldS4005 = _M0L11curr__entryS962->$0;
  _M0L7_2abindS964 = _M0L8_2afieldS4005;
  if (_M0L7_2abindS964 == 0) {
    moonbit_decref(_M0L11curr__entryS962);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS965 =
      _M0L7_2abindS964;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L4_2axS966 =
      _M0L7_2aSomeS965;
    moonbit_string_t _M0L8_2afieldS4004 = _M0L4_2axS966->$4;
    moonbit_string_t _M0L6_2akeyS967 = _M0L8_2afieldS4004;
    struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS4003 = _M0L4_2axS966->$5;
    struct _M0TPB3MapGsRPB4JsonE* _M0L8_2avalueS968 = _M0L8_2afieldS4003;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2afieldS4002 =
      _M0L4_2axS966->$1;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2anextS969 =
      _M0L8_2afieldS4002;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2aoldS4001 =
      _M0L11curr__entryS962->$0;
    struct _M0TUsRPB3MapGsRPB4JsonEE* _M0L8_2atupleS2817;
    if (_M0L7_2anextS969) {
      moonbit_incref(_M0L7_2anextS969);
    }
    moonbit_incref(_M0L8_2avalueS968);
    moonbit_incref(_M0L6_2akeyS967);
    if (_M0L6_2aoldS4001) {
      moonbit_decref(_M0L6_2aoldS4001);
    }
    _M0L11curr__entryS962->$0 = _M0L7_2anextS969;
    moonbit_decref(_M0L11curr__entryS962);
    _M0L8_2atupleS2817
    = (struct _M0TUsRPB3MapGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGsRPB4JsonEE));
    Moonbit_object_header(_M0L8_2atupleS2817)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGsRPB4JsonEE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2817->$0 = _M0L6_2akeyS967;
    _M0L8_2atupleS2817->$1 = _M0L8_2avalueS968;
    return _M0L8_2atupleS2817;
  }
}

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map4iterGsRP48clawteam8clawteam8internal6schema6SchemaEC2808l591(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aenvS2809
) {
  struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__* _M0L14_2acasted__envS2810;
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE* _M0L8_2afieldS4012;
  int32_t _M0L6_2acntS4421;
  struct _M0TPC13ref3RefGORPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE* _M0L11curr__entryS954;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS4011;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS956;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2810
  = (struct _M0R107Map_3a_3aiter_7c_5bString_2c_20clawteam_2fclawteam_2finternal_2fschema_2fSchema_5d_7c_2eanon__u2808__l591__*)_M0L6_2aenvS2809;
  _M0L8_2afieldS4012 = _M0L14_2acasted__envS2810->$0;
  _M0L6_2acntS4421 = Moonbit_object_header(_M0L14_2acasted__envS2810)->rc;
  if (_M0L6_2acntS4421 > 1) {
    int32_t _M0L11_2anew__cntS4422 = _M0L6_2acntS4421 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2810)->rc
    = _M0L11_2anew__cntS4422;
    moonbit_incref(_M0L8_2afieldS4012);
  } else if (_M0L6_2acntS4421 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2810);
  }
  _M0L11curr__entryS954 = _M0L8_2afieldS4012;
  _M0L8_2afieldS4011 = _M0L11curr__entryS954->$0;
  _M0L7_2abindS956 = _M0L8_2afieldS4011;
  if (_M0L7_2abindS956 == 0) {
    moonbit_decref(_M0L11curr__entryS954);
    return 0;
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS957 =
      _M0L7_2abindS956;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4_2axS958 =
      _M0L7_2aSomeS957;
    moonbit_string_t _M0L8_2afieldS4010 = _M0L4_2axS958->$4;
    moonbit_string_t _M0L6_2akeyS959 = _M0L8_2afieldS4010;
    void* _M0L8_2afieldS4009 = _M0L4_2axS958->$5;
    void* _M0L8_2avalueS960 = _M0L8_2afieldS4009;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS4008 =
      _M0L4_2axS958->$1;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2anextS961 =
      _M0L8_2afieldS4008;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aoldS4007 =
      _M0L11curr__entryS954->$0;
    struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2atupleS2811;
    if (_M0L7_2anextS961) {
      moonbit_incref(_M0L7_2anextS961);
    }
    moonbit_incref(_M0L8_2avalueS960);
    moonbit_incref(_M0L6_2akeyS959);
    if (_M0L6_2aoldS4007) {
      moonbit_decref(_M0L6_2aoldS4007);
    }
    _M0L11curr__entryS954->$0 = _M0L7_2anextS961;
    moonbit_decref(_M0L11curr__entryS954);
    _M0L8_2atupleS2811
    = (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE));
    Moonbit_object_header(_M0L8_2atupleS2811)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2811->$0 = _M0L6_2akeyS959;
    _M0L8_2atupleS2811->$1 = _M0L8_2avalueS960;
    return _M0L8_2atupleS2811;
  }
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2802l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2803
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__* _M0L14_2acasted__envS2804;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS4018;
  int32_t _M0L6_2acntS4423;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS946;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4017;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS948;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2804
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2802__l591__*)_M0L6_2aenvS2803;
  _M0L8_2afieldS4018 = _M0L14_2acasted__envS2804->$0;
  _M0L6_2acntS4423 = Moonbit_object_header(_M0L14_2acasted__envS2804)->rc;
  if (_M0L6_2acntS4423 > 1) {
    int32_t _M0L11_2anew__cntS4424 = _M0L6_2acntS4423 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2804)->rc
    = _M0L11_2anew__cntS4424;
    moonbit_incref(_M0L8_2afieldS4018);
  } else if (_M0L6_2acntS4423 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2804);
  }
  _M0L11curr__entryS946 = _M0L8_2afieldS4018;
  _M0L8_2afieldS4017 = _M0L11curr__entryS946->$0;
  _M0L7_2abindS948 = _M0L8_2afieldS4017;
  if (_M0L7_2abindS948 == 0) {
    moonbit_decref(_M0L11curr__entryS946);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS949 = _M0L7_2abindS948;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS950 = _M0L7_2aSomeS949;
    moonbit_string_t _M0L8_2afieldS4016 = _M0L4_2axS950->$4;
    moonbit_string_t _M0L6_2akeyS951 = _M0L8_2afieldS4016;
    void* _M0L8_2afieldS4015 = _M0L4_2axS950->$5;
    void* _M0L8_2avalueS952 = _M0L8_2afieldS4015;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4014 = _M0L4_2axS950->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS953 = _M0L8_2afieldS4014;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4013 =
      _M0L11curr__entryS946->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2805;
    if (_M0L7_2anextS953) {
      moonbit_incref(_M0L7_2anextS953);
    }
    moonbit_incref(_M0L8_2avalueS952);
    moonbit_incref(_M0L6_2akeyS951);
    if (_M0L6_2aoldS4013) {
      moonbit_decref(_M0L6_2aoldS4013);
    }
    _M0L11curr__entryS946->$0 = _M0L7_2anextS953;
    moonbit_decref(_M0L11curr__entryS946);
    _M0L8_2atupleS2805
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2805)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2805->$0 = _M0L6_2akeyS951;
    _M0L8_2atupleS2805->$1 = _M0L8_2avalueS952;
    return _M0L8_2atupleS2805;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS945
) {
  int32_t _M0L8_2afieldS4019;
  int32_t _M0L4sizeS2800;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4019 = _M0L4selfS945->$1;
  moonbit_decref(_M0L4selfS945);
  _M0L4sizeS2800 = _M0L8_2afieldS4019;
  return _M0L4sizeS2800 == 0;
}

int32_t _M0MPB3Map12contains__kvGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS940,
  moonbit_string_t _M0L3keyS936,
  void* _M0L5valueS943
) {
  int32_t _M0L4hashS935;
  int32_t _M0L14capacity__maskS2799;
  int32_t _M0L6_2atmpS2798;
  int32_t _M0L1iS937;
  int32_t _M0L3idxS938;
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS936);
  #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS935 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS936);
  _M0L14capacity__maskS2799 = _M0L4selfS940->$3;
  _M0L6_2atmpS2798 = _M0L4hashS935 & _M0L14capacity__maskS2799;
  _M0L1iS937 = 0;
  _M0L3idxS938 = _M0L6_2atmpS2798;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4025 = _M0L4selfS940->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2797 = _M0L8_2afieldS4025;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4024;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS939;
    if (
      _M0L3idxS938 < 0
      || _M0L3idxS938 >= Moonbit_array_length(_M0L7entriesS2797)
    ) {
      #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4024
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2797[_M0L3idxS938];
    _M0L7_2abindS939 = _M0L6_2atmpS4024;
    if (_M0L7_2abindS939 == 0) {
      if (_M0L7_2abindS939) {
        moonbit_incref(_M0L7_2abindS939);
      }
      moonbit_decref(_M0L5valueS943);
      moonbit_decref(_M0L4selfS940);
      if (_M0L7_2abindS939) {
        moonbit_decref(_M0L7_2abindS939);
      }
      moonbit_decref(_M0L3keyS936);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS941 = _M0L7_2abindS939;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aentryS942 = _M0L7_2aSomeS941;
      int32_t _M0L4hashS2791 = _M0L8_2aentryS942->$3;
      int32_t _if__result_4605;
      int32_t _M0L8_2afieldS4020;
      int32_t _M0L3pslS2792;
      int32_t _M0L6_2atmpS2793;
      int32_t _M0L6_2atmpS2795;
      int32_t _M0L14capacity__maskS2796;
      int32_t _M0L6_2atmpS2794;
      if (_M0L4hashS2791 == _M0L4hashS935) {
        moonbit_string_t _M0L8_2afieldS4023 = _M0L8_2aentryS942->$4;
        moonbit_string_t _M0L3keyS2790 = _M0L8_2afieldS4023;
        int32_t _M0L6_2atmpS4022;
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4022
        = moonbit_val_array_equal(_M0L3keyS2790, _M0L3keyS936);
        if (_M0L6_2atmpS4022) {
          void* _M0L8_2afieldS4021 = _M0L8_2aentryS942->$5;
          void* _M0L5valueS2789 = _M0L8_2afieldS4021;
          moonbit_incref(_M0L5valueS2789);
          moonbit_incref(_M0L5valueS943);
          moonbit_incref(_M0L8_2aentryS942);
          #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _if__result_4605
          = _M0IPC14json4JsonPB2Eq5equal(_M0L5valueS2789, _M0L5valueS943);
        } else {
          moonbit_incref(_M0L8_2aentryS942);
          _if__result_4605 = 0;
        }
      } else {
        moonbit_incref(_M0L8_2aentryS942);
        _if__result_4605 = 0;
      }
      if (_if__result_4605) {
        moonbit_decref(_M0L5valueS943);
        moonbit_decref(_M0L8_2aentryS942);
        moonbit_decref(_M0L4selfS940);
        moonbit_decref(_M0L3keyS936);
        return 1;
      }
      _M0L8_2afieldS4020 = _M0L8_2aentryS942->$2;
      moonbit_decref(_M0L8_2aentryS942);
      _M0L3pslS2792 = _M0L8_2afieldS4020;
      if (_M0L1iS937 > _M0L3pslS2792) {
        moonbit_decref(_M0L5valueS943);
        moonbit_decref(_M0L4selfS940);
        moonbit_decref(_M0L3keyS936);
        return 0;
      }
      _M0L6_2atmpS2793 = _M0L1iS937 + 1;
      _M0L6_2atmpS2795 = _M0L3idxS938 + 1;
      _M0L14capacity__maskS2796 = _M0L4selfS940->$3;
      _M0L6_2atmpS2794 = _M0L6_2atmpS2795 & _M0L14capacity__maskS2796;
      _M0L1iS937 = _M0L6_2atmpS2793;
      _M0L3idxS938 = _M0L6_2atmpS2794;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS931,
  moonbit_string_t _M0L3keyS927
) {
  int32_t _M0L4hashS926;
  int32_t _M0L14capacity__maskS2788;
  int32_t _M0L6_2atmpS2787;
  int32_t _M0L1iS928;
  int32_t _M0L3idxS929;
  #line 340 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS927);
  #line 342 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS926 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS927);
  _M0L14capacity__maskS2788 = _M0L4selfS931->$3;
  _M0L6_2atmpS2787 = _M0L4hashS926 & _M0L14capacity__maskS2788;
  _M0L1iS928 = 0;
  _M0L3idxS929 = _M0L6_2atmpS2787;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4030 =
      _M0L4selfS931->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7entriesS2786 =
      _M0L8_2afieldS4030;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS4029;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS930;
    if (
      _M0L3idxS929 < 0
      || _M0L3idxS929 >= Moonbit_array_length(_M0L7entriesS2786)
    ) {
      #line 344 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4029
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L7entriesS2786[
        _M0L3idxS929
      ];
    _M0L7_2abindS930 = _M0L6_2atmpS4029;
    if (_M0L7_2abindS930 == 0) {
      if (_M0L7_2abindS930) {
        moonbit_incref(_M0L7_2abindS930);
      }
      moonbit_decref(_M0L4selfS931);
      if (_M0L7_2abindS930) {
        moonbit_decref(_M0L7_2abindS930);
      }
      moonbit_decref(_M0L3keyS927);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS932 =
        _M0L7_2abindS930;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2aentryS933 =
        _M0L7_2aSomeS932;
      int32_t _M0L4hashS2780 = _M0L8_2aentryS933->$3;
      int32_t _if__result_4607;
      int32_t _M0L8_2afieldS4026;
      int32_t _M0L3pslS2781;
      int32_t _M0L6_2atmpS2782;
      int32_t _M0L6_2atmpS2784;
      int32_t _M0L14capacity__maskS2785;
      int32_t _M0L6_2atmpS2783;
      if (_M0L4hashS2780 == _M0L4hashS926) {
        moonbit_string_t _M0L8_2afieldS4028 = _M0L8_2aentryS933->$4;
        moonbit_string_t _M0L3keyS2779 = _M0L8_2afieldS4028;
        int32_t _M0L6_2atmpS4027;
        #line 345 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4027
        = moonbit_val_array_equal(_M0L3keyS2779, _M0L3keyS927);
        _if__result_4607 = _M0L6_2atmpS4027;
      } else {
        _if__result_4607 = 0;
      }
      if (_if__result_4607) {
        moonbit_decref(_M0L4selfS931);
        moonbit_decref(_M0L3keyS927);
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS933);
      }
      _M0L8_2afieldS4026 = _M0L8_2aentryS933->$2;
      moonbit_decref(_M0L8_2aentryS933);
      _M0L3pslS2781 = _M0L8_2afieldS4026;
      if (_M0L1iS928 > _M0L3pslS2781) {
        moonbit_decref(_M0L4selfS931);
        moonbit_decref(_M0L3keyS927);
        return 0;
      }
      _M0L6_2atmpS2782 = _M0L1iS928 + 1;
      _M0L6_2atmpS2784 = _M0L3idxS929 + 1;
      _M0L14capacity__maskS2785 = _M0L4selfS931->$3;
      _M0L6_2atmpS2783 = _M0L6_2atmpS2784 & _M0L14capacity__maskS2785;
      _M0L1iS928 = _M0L6_2atmpS2782;
      _M0L3idxS929 = _M0L6_2atmpS2783;
      continue;
    }
    break;
  }
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS904,
  int32_t _M0L3keyS900
) {
  int32_t _M0L4hashS899;
  int32_t _M0L14capacity__maskS2750;
  int32_t _M0L6_2atmpS2749;
  int32_t _M0L1iS901;
  int32_t _M0L3idxS902;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS899 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS900);
  _M0L14capacity__maskS2750 = _M0L4selfS904->$3;
  _M0L6_2atmpS2749 = _M0L4hashS899 & _M0L14capacity__maskS2750;
  _M0L1iS901 = 0;
  _M0L3idxS902 = _M0L6_2atmpS2749;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4034 =
      _M0L4selfS904->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2748 =
      _M0L8_2afieldS4034;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4033;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS903;
    if (
      _M0L3idxS902 < 0
      || _M0L3idxS902 >= Moonbit_array_length(_M0L7entriesS2748)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4033
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2748[
        _M0L3idxS902
      ];
    _M0L7_2abindS903 = _M0L6_2atmpS4033;
    if (_M0L7_2abindS903 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2737;
      if (_M0L7_2abindS903) {
        moonbit_incref(_M0L7_2abindS903);
      }
      moonbit_decref(_M0L4selfS904);
      if (_M0L7_2abindS903) {
        moonbit_decref(_M0L7_2abindS903);
      }
      _M0L6_2atmpS2737 = 0;
      return _M0L6_2atmpS2737;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS905 =
        _M0L7_2abindS903;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS906 =
        _M0L7_2aSomeS905;
      int32_t _M0L4hashS2739 = _M0L8_2aentryS906->$3;
      int32_t _if__result_4609;
      int32_t _M0L8_2afieldS4031;
      int32_t _M0L3pslS2742;
      int32_t _M0L6_2atmpS2744;
      int32_t _M0L6_2atmpS2746;
      int32_t _M0L14capacity__maskS2747;
      int32_t _M0L6_2atmpS2745;
      if (_M0L4hashS2739 == _M0L4hashS899) {
        int32_t _M0L3keyS2738 = _M0L8_2aentryS906->$4;
        _if__result_4609 = _M0L3keyS2738 == _M0L3keyS900;
      } else {
        _if__result_4609 = 0;
      }
      if (_if__result_4609) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4032;
        int32_t _M0L6_2acntS4425;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2741;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2740;
        moonbit_incref(_M0L8_2aentryS906);
        moonbit_decref(_M0L4selfS904);
        _M0L8_2afieldS4032 = _M0L8_2aentryS906->$5;
        _M0L6_2acntS4425 = Moonbit_object_header(_M0L8_2aentryS906)->rc;
        if (_M0L6_2acntS4425 > 1) {
          int32_t _M0L11_2anew__cntS4427 = _M0L6_2acntS4425 - 1;
          Moonbit_object_header(_M0L8_2aentryS906)->rc
          = _M0L11_2anew__cntS4427;
          moonbit_incref(_M0L8_2afieldS4032);
        } else if (_M0L6_2acntS4425 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4426 =
            _M0L8_2aentryS906->$1;
          if (_M0L8_2afieldS4426) {
            moonbit_decref(_M0L8_2afieldS4426);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS906);
        }
        _M0L5valueS2741 = _M0L8_2afieldS4032;
        _M0L6_2atmpS2740 = _M0L5valueS2741;
        return _M0L6_2atmpS2740;
      } else {
        moonbit_incref(_M0L8_2aentryS906);
      }
      _M0L8_2afieldS4031 = _M0L8_2aentryS906->$2;
      moonbit_decref(_M0L8_2aentryS906);
      _M0L3pslS2742 = _M0L8_2afieldS4031;
      if (_M0L1iS901 > _M0L3pslS2742) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2743;
        moonbit_decref(_M0L4selfS904);
        _M0L6_2atmpS2743 = 0;
        return _M0L6_2atmpS2743;
      }
      _M0L6_2atmpS2744 = _M0L1iS901 + 1;
      _M0L6_2atmpS2746 = _M0L3idxS902 + 1;
      _M0L14capacity__maskS2747 = _M0L4selfS904->$3;
      _M0L6_2atmpS2745 = _M0L6_2atmpS2746 & _M0L14capacity__maskS2747;
      _M0L1iS901 = _M0L6_2atmpS2744;
      _M0L3idxS902 = _M0L6_2atmpS2745;
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
  int32_t _M0L14capacity__maskS2764;
  int32_t _M0L6_2atmpS2763;
  int32_t _M0L1iS910;
  int32_t _M0L3idxS911;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS909);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS908 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS909);
  _M0L14capacity__maskS2764 = _M0L4selfS913->$3;
  _M0L6_2atmpS2763 = _M0L4hashS908 & _M0L14capacity__maskS2764;
  _M0L1iS910 = 0;
  _M0L3idxS911 = _M0L6_2atmpS2763;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4040 =
      _M0L4selfS913->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2762 =
      _M0L8_2afieldS4040;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4039;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS912;
    if (
      _M0L3idxS911 < 0
      || _M0L3idxS911 >= Moonbit_array_length(_M0L7entriesS2762)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4039
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2762[
        _M0L3idxS911
      ];
    _M0L7_2abindS912 = _M0L6_2atmpS4039;
    if (_M0L7_2abindS912 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2751;
      if (_M0L7_2abindS912) {
        moonbit_incref(_M0L7_2abindS912);
      }
      moonbit_decref(_M0L4selfS913);
      if (_M0L7_2abindS912) {
        moonbit_decref(_M0L7_2abindS912);
      }
      moonbit_decref(_M0L3keyS909);
      _M0L6_2atmpS2751 = 0;
      return _M0L6_2atmpS2751;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS914 =
        _M0L7_2abindS912;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS915 =
        _M0L7_2aSomeS914;
      int32_t _M0L4hashS2753 = _M0L8_2aentryS915->$3;
      int32_t _if__result_4611;
      int32_t _M0L8_2afieldS4035;
      int32_t _M0L3pslS2756;
      int32_t _M0L6_2atmpS2758;
      int32_t _M0L6_2atmpS2760;
      int32_t _M0L14capacity__maskS2761;
      int32_t _M0L6_2atmpS2759;
      if (_M0L4hashS2753 == _M0L4hashS908) {
        moonbit_string_t _M0L8_2afieldS4038 = _M0L8_2aentryS915->$4;
        moonbit_string_t _M0L3keyS2752 = _M0L8_2afieldS4038;
        int32_t _M0L6_2atmpS4037;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4037
        = moonbit_val_array_equal(_M0L3keyS2752, _M0L3keyS909);
        _if__result_4611 = _M0L6_2atmpS4037;
      } else {
        _if__result_4611 = 0;
      }
      if (_if__result_4611) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4036;
        int32_t _M0L6_2acntS4428;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2755;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2754;
        moonbit_incref(_M0L8_2aentryS915);
        moonbit_decref(_M0L4selfS913);
        moonbit_decref(_M0L3keyS909);
        _M0L8_2afieldS4036 = _M0L8_2aentryS915->$5;
        _M0L6_2acntS4428 = Moonbit_object_header(_M0L8_2aentryS915)->rc;
        if (_M0L6_2acntS4428 > 1) {
          int32_t _M0L11_2anew__cntS4431 = _M0L6_2acntS4428 - 1;
          Moonbit_object_header(_M0L8_2aentryS915)->rc
          = _M0L11_2anew__cntS4431;
          moonbit_incref(_M0L8_2afieldS4036);
        } else if (_M0L6_2acntS4428 == 1) {
          moonbit_string_t _M0L8_2afieldS4430 = _M0L8_2aentryS915->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4429;
          moonbit_decref(_M0L8_2afieldS4430);
          _M0L8_2afieldS4429 = _M0L8_2aentryS915->$1;
          if (_M0L8_2afieldS4429) {
            moonbit_decref(_M0L8_2afieldS4429);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS915);
        }
        _M0L5valueS2755 = _M0L8_2afieldS4036;
        _M0L6_2atmpS2754 = _M0L5valueS2755;
        return _M0L6_2atmpS2754;
      } else {
        moonbit_incref(_M0L8_2aentryS915);
      }
      _M0L8_2afieldS4035 = _M0L8_2aentryS915->$2;
      moonbit_decref(_M0L8_2aentryS915);
      _M0L3pslS2756 = _M0L8_2afieldS4035;
      if (_M0L1iS910 > _M0L3pslS2756) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2757;
        moonbit_decref(_M0L4selfS913);
        moonbit_decref(_M0L3keyS909);
        _M0L6_2atmpS2757 = 0;
        return _M0L6_2atmpS2757;
      }
      _M0L6_2atmpS2758 = _M0L1iS910 + 1;
      _M0L6_2atmpS2760 = _M0L3idxS911 + 1;
      _M0L14capacity__maskS2761 = _M0L4selfS913->$3;
      _M0L6_2atmpS2759 = _M0L6_2atmpS2760 & _M0L14capacity__maskS2761;
      _M0L1iS910 = _M0L6_2atmpS2758;
      _M0L3idxS911 = _M0L6_2atmpS2759;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS922,
  moonbit_string_t _M0L3keyS918
) {
  int32_t _M0L4hashS917;
  int32_t _M0L14capacity__maskS2778;
  int32_t _M0L6_2atmpS2777;
  int32_t _M0L1iS919;
  int32_t _M0L3idxS920;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS918);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS917 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS918);
  _M0L14capacity__maskS2778 = _M0L4selfS922->$3;
  _M0L6_2atmpS2777 = _M0L4hashS917 & _M0L14capacity__maskS2778;
  _M0L1iS919 = 0;
  _M0L3idxS920 = _M0L6_2atmpS2777;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4046 = _M0L4selfS922->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2776 = _M0L8_2afieldS4046;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4045;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS921;
    if (
      _M0L3idxS920 < 0
      || _M0L3idxS920 >= Moonbit_array_length(_M0L7entriesS2776)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4045
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2776[_M0L3idxS920];
    _M0L7_2abindS921 = _M0L6_2atmpS4045;
    if (_M0L7_2abindS921 == 0) {
      void* _M0L6_2atmpS2765;
      if (_M0L7_2abindS921) {
        moonbit_incref(_M0L7_2abindS921);
      }
      moonbit_decref(_M0L4selfS922);
      if (_M0L7_2abindS921) {
        moonbit_decref(_M0L7_2abindS921);
      }
      moonbit_decref(_M0L3keyS918);
      _M0L6_2atmpS2765 = 0;
      return _M0L6_2atmpS2765;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS923 = _M0L7_2abindS921;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aentryS924 = _M0L7_2aSomeS923;
      int32_t _M0L4hashS2767 = _M0L8_2aentryS924->$3;
      int32_t _if__result_4613;
      int32_t _M0L8_2afieldS4041;
      int32_t _M0L3pslS2770;
      int32_t _M0L6_2atmpS2772;
      int32_t _M0L6_2atmpS2774;
      int32_t _M0L14capacity__maskS2775;
      int32_t _M0L6_2atmpS2773;
      if (_M0L4hashS2767 == _M0L4hashS917) {
        moonbit_string_t _M0L8_2afieldS4044 = _M0L8_2aentryS924->$4;
        moonbit_string_t _M0L3keyS2766 = _M0L8_2afieldS4044;
        int32_t _M0L6_2atmpS4043;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4043
        = moonbit_val_array_equal(_M0L3keyS2766, _M0L3keyS918);
        _if__result_4613 = _M0L6_2atmpS4043;
      } else {
        _if__result_4613 = 0;
      }
      if (_if__result_4613) {
        void* _M0L8_2afieldS4042;
        int32_t _M0L6_2acntS4432;
        void* _M0L5valueS2769;
        void* _M0L6_2atmpS2768;
        moonbit_incref(_M0L8_2aentryS924);
        moonbit_decref(_M0L4selfS922);
        moonbit_decref(_M0L3keyS918);
        _M0L8_2afieldS4042 = _M0L8_2aentryS924->$5;
        _M0L6_2acntS4432 = Moonbit_object_header(_M0L8_2aentryS924)->rc;
        if (_M0L6_2acntS4432 > 1) {
          int32_t _M0L11_2anew__cntS4435 = _M0L6_2acntS4432 - 1;
          Moonbit_object_header(_M0L8_2aentryS924)->rc
          = _M0L11_2anew__cntS4435;
          moonbit_incref(_M0L8_2afieldS4042);
        } else if (_M0L6_2acntS4432 == 1) {
          moonbit_string_t _M0L8_2afieldS4434 = _M0L8_2aentryS924->$4;
          struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4433;
          moonbit_decref(_M0L8_2afieldS4434);
          _M0L8_2afieldS4433 = _M0L8_2aentryS924->$1;
          if (_M0L8_2afieldS4433) {
            moonbit_decref(_M0L8_2afieldS4433);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS924);
        }
        _M0L5valueS2769 = _M0L8_2afieldS4042;
        _M0L6_2atmpS2768 = _M0L5valueS2769;
        return _M0L6_2atmpS2768;
      } else {
        moonbit_incref(_M0L8_2aentryS924);
      }
      _M0L8_2afieldS4041 = _M0L8_2aentryS924->$2;
      moonbit_decref(_M0L8_2aentryS924);
      _M0L3pslS2770 = _M0L8_2afieldS4041;
      if (_M0L1iS919 > _M0L3pslS2770) {
        void* _M0L6_2atmpS2771;
        moonbit_decref(_M0L4selfS922);
        moonbit_decref(_M0L3keyS918);
        _M0L6_2atmpS2771 = 0;
        return _M0L6_2atmpS2771;
      }
      _M0L6_2atmpS2772 = _M0L1iS919 + 1;
      _M0L6_2atmpS2774 = _M0L3idxS920 + 1;
      _M0L14capacity__maskS2775 = _M0L4selfS922->$3;
      _M0L6_2atmpS2773 = _M0L6_2atmpS2774 & _M0L14capacity__maskS2775;
      _M0L1iS919 = _M0L6_2atmpS2772;
      _M0L3idxS920 = _M0L6_2atmpS2773;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS868
) {
  int32_t _M0L6lengthS867;
  int32_t _M0Lm8capacityS869;
  int32_t _M0L6_2atmpS2690;
  int32_t _M0L6_2atmpS2689;
  int32_t _M0L6_2atmpS2700;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS870;
  int32_t _M0L3endS2698;
  int32_t _M0L5startS2699;
  int32_t _M0L7_2abindS871;
  int32_t _M0L2__S872;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS868.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS867
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS868);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS869 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS867);
  _M0L6_2atmpS2690 = _M0Lm8capacityS869;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2689 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2690);
  if (_M0L6lengthS867 > _M0L6_2atmpS2689) {
    int32_t _M0L6_2atmpS2691 = _M0Lm8capacityS869;
    _M0Lm8capacityS869 = _M0L6_2atmpS2691 * 2;
  }
  _M0L6_2atmpS2700 = _M0Lm8capacityS869;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS870
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2700);
  _M0L3endS2698 = _M0L3arrS868.$2;
  _M0L5startS2699 = _M0L3arrS868.$1;
  _M0L7_2abindS871 = _M0L3endS2698 - _M0L5startS2699;
  _M0L2__S872 = 0;
  while (1) {
    if (_M0L2__S872 < _M0L7_2abindS871) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4050 =
        _M0L3arrS868.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2695 =
        _M0L8_2afieldS4050;
      int32_t _M0L5startS2697 = _M0L3arrS868.$1;
      int32_t _M0L6_2atmpS2696 = _M0L5startS2697 + _M0L2__S872;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4049 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2695[
          _M0L6_2atmpS2696
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS873 =
        _M0L6_2atmpS4049;
      moonbit_string_t _M0L8_2afieldS4048 = _M0L1eS873->$0;
      moonbit_string_t _M0L6_2atmpS2692 = _M0L8_2afieldS4048;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4047 =
        _M0L1eS873->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2693 =
        _M0L8_2afieldS4047;
      int32_t _M0L6_2atmpS2694;
      moonbit_incref(_M0L6_2atmpS2693);
      moonbit_incref(_M0L6_2atmpS2692);
      moonbit_incref(_M0L1mS870);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS870, _M0L6_2atmpS2692, _M0L6_2atmpS2693);
      _M0L6_2atmpS2694 = _M0L2__S872 + 1;
      _M0L2__S872 = _M0L6_2atmpS2694;
      continue;
    } else {
      moonbit_decref(_M0L3arrS868.$0);
    }
    break;
  }
  return _M0L1mS870;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS876
) {
  int32_t _M0L6lengthS875;
  int32_t _M0Lm8capacityS877;
  int32_t _M0L6_2atmpS2702;
  int32_t _M0L6_2atmpS2701;
  int32_t _M0L6_2atmpS2712;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS878;
  int32_t _M0L3endS2710;
  int32_t _M0L5startS2711;
  int32_t _M0L7_2abindS879;
  int32_t _M0L2__S880;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS876.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS875
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS876);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS877 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS875);
  _M0L6_2atmpS2702 = _M0Lm8capacityS877;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2701 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2702);
  if (_M0L6lengthS875 > _M0L6_2atmpS2701) {
    int32_t _M0L6_2atmpS2703 = _M0Lm8capacityS877;
    _M0Lm8capacityS877 = _M0L6_2atmpS2703 * 2;
  }
  _M0L6_2atmpS2712 = _M0Lm8capacityS877;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS878
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2712);
  _M0L3endS2710 = _M0L3arrS876.$2;
  _M0L5startS2711 = _M0L3arrS876.$1;
  _M0L7_2abindS879 = _M0L3endS2710 - _M0L5startS2711;
  _M0L2__S880 = 0;
  while (1) {
    if (_M0L2__S880 < _M0L7_2abindS879) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4053 =
        _M0L3arrS876.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2707 =
        _M0L8_2afieldS4053;
      int32_t _M0L5startS2709 = _M0L3arrS876.$1;
      int32_t _M0L6_2atmpS2708 = _M0L5startS2709 + _M0L2__S880;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4052 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2707[
          _M0L6_2atmpS2708
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS881 = _M0L6_2atmpS4052;
      int32_t _M0L6_2atmpS2704 = _M0L1eS881->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4051 =
        _M0L1eS881->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2705 =
        _M0L8_2afieldS4051;
      int32_t _M0L6_2atmpS2706;
      moonbit_incref(_M0L6_2atmpS2705);
      moonbit_incref(_M0L1mS878);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS878, _M0L6_2atmpS2704, _M0L6_2atmpS2705);
      _M0L6_2atmpS2706 = _M0L2__S880 + 1;
      _M0L2__S880 = _M0L6_2atmpS2706;
      continue;
    } else {
      moonbit_decref(_M0L3arrS876.$0);
    }
    break;
  }
  return _M0L1mS878;
}

struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map11from__arrayGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L3arrS884
) {
  int32_t _M0L6lengthS883;
  int32_t _M0Lm8capacityS885;
  int32_t _M0L6_2atmpS2714;
  int32_t _M0L6_2atmpS2713;
  int32_t _M0L6_2atmpS2724;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L1mS886;
  int32_t _M0L3endS2722;
  int32_t _M0L5startS2723;
  int32_t _M0L7_2abindS887;
  int32_t _M0L2__S888;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS884.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS883
  = _M0MPC15array9ArrayView6lengthGUsRP48clawteam8clawteam8internal6schema6SchemaEE(_M0L3arrS884);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS885 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS883);
  _M0L6_2atmpS2714 = _M0Lm8capacityS885;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2713 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2714);
  if (_M0L6lengthS883 > _M0L6_2atmpS2713) {
    int32_t _M0L6_2atmpS2715 = _M0Lm8capacityS885;
    _M0Lm8capacityS885 = _M0L6_2atmpS2715 * 2;
  }
  _M0L6_2atmpS2724 = _M0Lm8capacityS885;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS886
  = _M0MPB3Map11new_2einnerGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L6_2atmpS2724);
  _M0L3endS2722 = _M0L3arrS884.$2;
  _M0L5startS2723 = _M0L3arrS884.$1;
  _M0L7_2abindS887 = _M0L3endS2722 - _M0L5startS2723;
  _M0L2__S888 = 0;
  while (1) {
    if (_M0L2__S888 < _M0L7_2abindS887) {
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4057 =
        _M0L3arrS884.$0;
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L3bufS2719 =
        _M0L8_2afieldS4057;
      int32_t _M0L5startS2721 = _M0L3arrS884.$1;
      int32_t _M0L6_2atmpS2720 = _M0L5startS2721 + _M0L2__S888;
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS4056 =
        (struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L3bufS2719[
          _M0L6_2atmpS2720
        ];
      struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L1eS889 =
        _M0L6_2atmpS4056;
      moonbit_string_t _M0L8_2afieldS4055 = _M0L1eS889->$0;
      moonbit_string_t _M0L6_2atmpS2716 = _M0L8_2afieldS4055;
      void* _M0L8_2afieldS4054 = _M0L1eS889->$1;
      void* _M0L6_2atmpS2717 = _M0L8_2afieldS4054;
      int32_t _M0L6_2atmpS2718;
      moonbit_incref(_M0L6_2atmpS2717);
      moonbit_incref(_M0L6_2atmpS2716);
      moonbit_incref(_M0L1mS886);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L1mS886, _M0L6_2atmpS2716, _M0L6_2atmpS2717);
      _M0L6_2atmpS2718 = _M0L2__S888 + 1;
      _M0L2__S888 = _M0L6_2atmpS2718;
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
  int32_t _M0L6_2atmpS2726;
  int32_t _M0L6_2atmpS2725;
  int32_t _M0L6_2atmpS2736;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS894;
  int32_t _M0L3endS2734;
  int32_t _M0L5startS2735;
  int32_t _M0L7_2abindS895;
  int32_t _M0L2__S896;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS892.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS891 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS892);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS893 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS891);
  _M0L6_2atmpS2726 = _M0Lm8capacityS893;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2725 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2726);
  if (_M0L6lengthS891 > _M0L6_2atmpS2725) {
    int32_t _M0L6_2atmpS2727 = _M0Lm8capacityS893;
    _M0Lm8capacityS893 = _M0L6_2atmpS2727 * 2;
  }
  _M0L6_2atmpS2736 = _M0Lm8capacityS893;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS894 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2736);
  _M0L3endS2734 = _M0L3arrS892.$2;
  _M0L5startS2735 = _M0L3arrS892.$1;
  _M0L7_2abindS895 = _M0L3endS2734 - _M0L5startS2735;
  _M0L2__S896 = 0;
  while (1) {
    if (_M0L2__S896 < _M0L7_2abindS895) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS4061 = _M0L3arrS892.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2731 = _M0L8_2afieldS4061;
      int32_t _M0L5startS2733 = _M0L3arrS892.$1;
      int32_t _M0L6_2atmpS2732 = _M0L5startS2733 + _M0L2__S896;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS4060 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2731[_M0L6_2atmpS2732];
      struct _M0TUsRPB4JsonE* _M0L1eS897 = _M0L6_2atmpS4060;
      moonbit_string_t _M0L8_2afieldS4059 = _M0L1eS897->$0;
      moonbit_string_t _M0L6_2atmpS2728 = _M0L8_2afieldS4059;
      void* _M0L8_2afieldS4058 = _M0L1eS897->$1;
      void* _M0L6_2atmpS2729 = _M0L8_2afieldS4058;
      int32_t _M0L6_2atmpS2730;
      moonbit_incref(_M0L6_2atmpS2729);
      moonbit_incref(_M0L6_2atmpS2728);
      moonbit_incref(_M0L1mS894);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS894, _M0L6_2atmpS2728, _M0L6_2atmpS2729);
      _M0L6_2atmpS2730 = _M0L2__S896 + 1;
      _M0L2__S896 = _M0L6_2atmpS2730;
      continue;
    } else {
      moonbit_decref(_M0L3arrS892.$0);
    }
    break;
  }
  return _M0L1mS894;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS852,
  moonbit_string_t _M0L3keyS853,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS854
) {
  int32_t _M0L6_2atmpS2684;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS853);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2684 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS853);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS852, _M0L3keyS853, _M0L5valueS854, _M0L6_2atmpS2684);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS855,
  int32_t _M0L3keyS856,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS857
) {
  int32_t _M0L6_2atmpS2685;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2685 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS856);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS855, _M0L3keyS856, _M0L5valueS857, _M0L6_2atmpS2685);
  return 0;
}

int32_t _M0MPB3Map3setGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS858,
  moonbit_string_t _M0L3keyS859,
  void* _M0L5valueS860
) {
  int32_t _M0L6_2atmpS2686;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS859);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2686 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS859);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS858, _M0L3keyS859, _M0L5valueS860, _M0L6_2atmpS2686);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS861,
  moonbit_string_t _M0L3keyS862,
  void* _M0L5valueS863
) {
  int32_t _M0L6_2atmpS2687;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS862);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2687 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS862);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS861, _M0L3keyS862, _M0L5valueS863, _M0L6_2atmpS2687);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS864,
  moonbit_string_t _M0L3keyS865,
  struct _M0TPB3MapGsRPB4JsonE* _M0L5valueS866
) {
  int32_t _M0L6_2atmpS2688;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS865);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2688 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS865);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGsRPB4JsonEE(_M0L4selfS864, _M0L3keyS865, _M0L5valueS866, _M0L6_2atmpS2688);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS798
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4068;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS797;
  int32_t _M0L8capacityS2655;
  int32_t _M0L13new__capacityS799;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2650;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2649;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS4067;
  int32_t _M0L6_2atmpS2651;
  int32_t _M0L8capacityS2653;
  int32_t _M0L6_2atmpS2652;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2654;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4066;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS800;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4068 = _M0L4selfS798->$5;
  _M0L9old__headS797 = _M0L8_2afieldS4068;
  _M0L8capacityS2655 = _M0L4selfS798->$2;
  _M0L13new__capacityS799 = _M0L8capacityS2655 << 1;
  _M0L6_2atmpS2650 = 0;
  _M0L6_2atmpS2649
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS799, _M0L6_2atmpS2650);
  _M0L6_2aoldS4067 = _M0L4selfS798->$0;
  if (_M0L9old__headS797) {
    moonbit_incref(_M0L9old__headS797);
  }
  moonbit_decref(_M0L6_2aoldS4067);
  _M0L4selfS798->$0 = _M0L6_2atmpS2649;
  _M0L4selfS798->$2 = _M0L13new__capacityS799;
  _M0L6_2atmpS2651 = _M0L13new__capacityS799 - 1;
  _M0L4selfS798->$3 = _M0L6_2atmpS2651;
  _M0L8capacityS2653 = _M0L4selfS798->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2652 = _M0FPB21calc__grow__threshold(_M0L8capacityS2653);
  _M0L4selfS798->$4 = _M0L6_2atmpS2652;
  _M0L4selfS798->$1 = 0;
  _M0L6_2atmpS2654 = 0;
  _M0L6_2aoldS4066 = _M0L4selfS798->$5;
  if (_M0L6_2aoldS4066) {
    moonbit_decref(_M0L6_2aoldS4066);
  }
  _M0L4selfS798->$5 = _M0L6_2atmpS2654;
  _M0L4selfS798->$6 = -1;
  _M0L8_2aparamS800 = _M0L9old__headS797;
  while (1) {
    if (_M0L8_2aparamS800 == 0) {
      if (_M0L8_2aparamS800) {
        moonbit_decref(_M0L8_2aparamS800);
      }
      moonbit_decref(_M0L4selfS798);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS801 =
        _M0L8_2aparamS800;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS802 =
        _M0L7_2aSomeS801;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4065 =
        _M0L4_2axS802->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS803 =
        _M0L8_2afieldS4065;
      moonbit_string_t _M0L8_2afieldS4064 = _M0L4_2axS802->$4;
      moonbit_string_t _M0L6_2akeyS804 = _M0L8_2afieldS4064;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4063 =
        _M0L4_2axS802->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS805 =
        _M0L8_2afieldS4063;
      int32_t _M0L8_2afieldS4062 = _M0L4_2axS802->$3;
      int32_t _M0L6_2acntS4436 = Moonbit_object_header(_M0L4_2axS802)->rc;
      int32_t _M0L7_2ahashS806;
      if (_M0L6_2acntS4436 > 1) {
        int32_t _M0L11_2anew__cntS4437 = _M0L6_2acntS4436 - 1;
        Moonbit_object_header(_M0L4_2axS802)->rc = _M0L11_2anew__cntS4437;
        moonbit_incref(_M0L8_2avalueS805);
        moonbit_incref(_M0L6_2akeyS804);
        if (_M0L7_2anextS803) {
          moonbit_incref(_M0L7_2anextS803);
        }
      } else if (_M0L6_2acntS4436 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS802);
      }
      _M0L7_2ahashS806 = _M0L8_2afieldS4062;
      moonbit_incref(_M0L4selfS798);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS798, _M0L6_2akeyS804, _M0L8_2avalueS805, _M0L7_2ahashS806);
      _M0L8_2aparamS800 = _M0L7_2anextS803;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS809
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4074;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS808;
  int32_t _M0L8capacityS2662;
  int32_t _M0L13new__capacityS810;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2657;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2656;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4073;
  int32_t _M0L6_2atmpS2658;
  int32_t _M0L8capacityS2660;
  int32_t _M0L6_2atmpS2659;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2661;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4072;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS811;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4074 = _M0L4selfS809->$5;
  _M0L9old__headS808 = _M0L8_2afieldS4074;
  _M0L8capacityS2662 = _M0L4selfS809->$2;
  _M0L13new__capacityS810 = _M0L8capacityS2662 << 1;
  _M0L6_2atmpS2657 = 0;
  _M0L6_2atmpS2656
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS810, _M0L6_2atmpS2657);
  _M0L6_2aoldS4073 = _M0L4selfS809->$0;
  if (_M0L9old__headS808) {
    moonbit_incref(_M0L9old__headS808);
  }
  moonbit_decref(_M0L6_2aoldS4073);
  _M0L4selfS809->$0 = _M0L6_2atmpS2656;
  _M0L4selfS809->$2 = _M0L13new__capacityS810;
  _M0L6_2atmpS2658 = _M0L13new__capacityS810 - 1;
  _M0L4selfS809->$3 = _M0L6_2atmpS2658;
  _M0L8capacityS2660 = _M0L4selfS809->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2659 = _M0FPB21calc__grow__threshold(_M0L8capacityS2660);
  _M0L4selfS809->$4 = _M0L6_2atmpS2659;
  _M0L4selfS809->$1 = 0;
  _M0L6_2atmpS2661 = 0;
  _M0L6_2aoldS4072 = _M0L4selfS809->$5;
  if (_M0L6_2aoldS4072) {
    moonbit_decref(_M0L6_2aoldS4072);
  }
  _M0L4selfS809->$5 = _M0L6_2atmpS2661;
  _M0L4selfS809->$6 = -1;
  _M0L8_2aparamS811 = _M0L9old__headS808;
  while (1) {
    if (_M0L8_2aparamS811 == 0) {
      if (_M0L8_2aparamS811) {
        moonbit_decref(_M0L8_2aparamS811);
      }
      moonbit_decref(_M0L4selfS809);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS812 =
        _M0L8_2aparamS811;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS813 =
        _M0L7_2aSomeS812;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4071 =
        _M0L4_2axS813->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS814 =
        _M0L8_2afieldS4071;
      int32_t _M0L6_2akeyS815 = _M0L4_2axS813->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4070 =
        _M0L4_2axS813->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS816 =
        _M0L8_2afieldS4070;
      int32_t _M0L8_2afieldS4069 = _M0L4_2axS813->$3;
      int32_t _M0L6_2acntS4438 = Moonbit_object_header(_M0L4_2axS813)->rc;
      int32_t _M0L7_2ahashS817;
      if (_M0L6_2acntS4438 > 1) {
        int32_t _M0L11_2anew__cntS4439 = _M0L6_2acntS4438 - 1;
        Moonbit_object_header(_M0L4_2axS813)->rc = _M0L11_2anew__cntS4439;
        moonbit_incref(_M0L8_2avalueS816);
        if (_M0L7_2anextS814) {
          moonbit_incref(_M0L7_2anextS814);
        }
      } else if (_M0L6_2acntS4438 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS813);
      }
      _M0L7_2ahashS817 = _M0L8_2afieldS4069;
      moonbit_incref(_M0L4selfS809);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS809, _M0L6_2akeyS815, _M0L8_2avalueS816, _M0L7_2ahashS817);
      _M0L8_2aparamS811 = _M0L7_2anextS814;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS820
) {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS4081;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L9old__headS819;
  int32_t _M0L8capacityS2669;
  int32_t _M0L13new__capacityS821;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2664;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2atmpS2663;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L6_2aoldS4080;
  int32_t _M0L6_2atmpS2665;
  int32_t _M0L8capacityS2667;
  int32_t _M0L6_2atmpS2666;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2668;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aoldS4079;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2aparamS822;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4081 = _M0L4selfS820->$5;
  _M0L9old__headS819 = _M0L8_2afieldS4081;
  _M0L8capacityS2669 = _M0L4selfS820->$2;
  _M0L13new__capacityS821 = _M0L8capacityS2669 << 1;
  _M0L6_2atmpS2664 = 0;
  _M0L6_2atmpS2663
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array(_M0L13new__capacityS821, _M0L6_2atmpS2664);
  _M0L6_2aoldS4080 = _M0L4selfS820->$0;
  if (_M0L9old__headS819) {
    moonbit_incref(_M0L9old__headS819);
  }
  moonbit_decref(_M0L6_2aoldS4080);
  _M0L4selfS820->$0 = _M0L6_2atmpS2663;
  _M0L4selfS820->$2 = _M0L13new__capacityS821;
  _M0L6_2atmpS2665 = _M0L13new__capacityS821 - 1;
  _M0L4selfS820->$3 = _M0L6_2atmpS2665;
  _M0L8capacityS2667 = _M0L4selfS820->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2666 = _M0FPB21calc__grow__threshold(_M0L8capacityS2667);
  _M0L4selfS820->$4 = _M0L6_2atmpS2666;
  _M0L4selfS820->$1 = 0;
  _M0L6_2atmpS2668 = 0;
  _M0L6_2aoldS4079 = _M0L4selfS820->$5;
  if (_M0L6_2aoldS4079) {
    moonbit_decref(_M0L6_2aoldS4079);
  }
  _M0L4selfS820->$5 = _M0L6_2atmpS2668;
  _M0L4selfS820->$6 = -1;
  _M0L8_2aparamS822 = _M0L9old__headS819;
  while (1) {
    if (_M0L8_2aparamS822 == 0) {
      if (_M0L8_2aparamS822) {
        moonbit_decref(_M0L8_2aparamS822);
      }
      moonbit_decref(_M0L4selfS820);
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS823 =
        _M0L8_2aparamS822;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4_2axS824 =
        _M0L7_2aSomeS823;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS4078 =
        _M0L4_2axS824->$1;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2anextS825 =
        _M0L8_2afieldS4078;
      moonbit_string_t _M0L8_2afieldS4077 = _M0L4_2axS824->$4;
      moonbit_string_t _M0L6_2akeyS826 = _M0L8_2afieldS4077;
      void* _M0L8_2afieldS4076 = _M0L4_2axS824->$5;
      void* _M0L8_2avalueS827 = _M0L8_2afieldS4076;
      int32_t _M0L8_2afieldS4075 = _M0L4_2axS824->$3;
      int32_t _M0L6_2acntS4440 = Moonbit_object_header(_M0L4_2axS824)->rc;
      int32_t _M0L7_2ahashS828;
      if (_M0L6_2acntS4440 > 1) {
        int32_t _M0L11_2anew__cntS4441 = _M0L6_2acntS4440 - 1;
        Moonbit_object_header(_M0L4_2axS824)->rc = _M0L11_2anew__cntS4441;
        moonbit_incref(_M0L8_2avalueS827);
        moonbit_incref(_M0L6_2akeyS826);
        if (_M0L7_2anextS825) {
          moonbit_incref(_M0L7_2anextS825);
        }
      } else if (_M0L6_2acntS4440 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS824);
      }
      _M0L7_2ahashS828 = _M0L8_2afieldS4075;
      moonbit_incref(_M0L4selfS820);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS820, _M0L6_2akeyS826, _M0L8_2avalueS827, _M0L7_2ahashS828);
      _M0L8_2aparamS822 = _M0L7_2anextS825;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS831
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4088;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS830;
  int32_t _M0L8capacityS2676;
  int32_t _M0L13new__capacityS832;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2671;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2670;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS4087;
  int32_t _M0L6_2atmpS2672;
  int32_t _M0L8capacityS2674;
  int32_t _M0L6_2atmpS2673;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2675;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4086;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS833;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4088 = _M0L4selfS831->$5;
  _M0L9old__headS830 = _M0L8_2afieldS4088;
  _M0L8capacityS2676 = _M0L4selfS831->$2;
  _M0L13new__capacityS832 = _M0L8capacityS2676 << 1;
  _M0L6_2atmpS2671 = 0;
  _M0L6_2atmpS2670
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS832, _M0L6_2atmpS2671);
  _M0L6_2aoldS4087 = _M0L4selfS831->$0;
  if (_M0L9old__headS830) {
    moonbit_incref(_M0L9old__headS830);
  }
  moonbit_decref(_M0L6_2aoldS4087);
  _M0L4selfS831->$0 = _M0L6_2atmpS2670;
  _M0L4selfS831->$2 = _M0L13new__capacityS832;
  _M0L6_2atmpS2672 = _M0L13new__capacityS832 - 1;
  _M0L4selfS831->$3 = _M0L6_2atmpS2672;
  _M0L8capacityS2674 = _M0L4selfS831->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2673 = _M0FPB21calc__grow__threshold(_M0L8capacityS2674);
  _M0L4selfS831->$4 = _M0L6_2atmpS2673;
  _M0L4selfS831->$1 = 0;
  _M0L6_2atmpS2675 = 0;
  _M0L6_2aoldS4086 = _M0L4selfS831->$5;
  if (_M0L6_2aoldS4086) {
    moonbit_decref(_M0L6_2aoldS4086);
  }
  _M0L4selfS831->$5 = _M0L6_2atmpS2675;
  _M0L4selfS831->$6 = -1;
  _M0L8_2aparamS833 = _M0L9old__headS830;
  while (1) {
    if (_M0L8_2aparamS833 == 0) {
      if (_M0L8_2aparamS833) {
        moonbit_decref(_M0L8_2aparamS833);
      }
      moonbit_decref(_M0L4selfS831);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS834 = _M0L8_2aparamS833;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS835 = _M0L7_2aSomeS834;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4085 = _M0L4_2axS835->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS836 = _M0L8_2afieldS4085;
      moonbit_string_t _M0L8_2afieldS4084 = _M0L4_2axS835->$4;
      moonbit_string_t _M0L6_2akeyS837 = _M0L8_2afieldS4084;
      void* _M0L8_2afieldS4083 = _M0L4_2axS835->$5;
      void* _M0L8_2avalueS838 = _M0L8_2afieldS4083;
      int32_t _M0L8_2afieldS4082 = _M0L4_2axS835->$3;
      int32_t _M0L6_2acntS4442 = Moonbit_object_header(_M0L4_2axS835)->rc;
      int32_t _M0L7_2ahashS839;
      if (_M0L6_2acntS4442 > 1) {
        int32_t _M0L11_2anew__cntS4443 = _M0L6_2acntS4442 - 1;
        Moonbit_object_header(_M0L4_2axS835)->rc = _M0L11_2anew__cntS4443;
        moonbit_incref(_M0L8_2avalueS838);
        moonbit_incref(_M0L6_2akeyS837);
        if (_M0L7_2anextS836) {
          moonbit_incref(_M0L7_2anextS836);
        }
      } else if (_M0L6_2acntS4442 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS835);
      }
      _M0L7_2ahashS839 = _M0L8_2afieldS4082;
      moonbit_incref(_M0L4selfS831);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS831, _M0L6_2akeyS837, _M0L8_2avalueS838, _M0L7_2ahashS839);
      _M0L8_2aparamS833 = _M0L7_2anextS836;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS842
) {
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2afieldS4095;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L9old__headS841;
  int32_t _M0L8capacityS2683;
  int32_t _M0L13new__capacityS843;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2678;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L6_2atmpS2677;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L6_2aoldS4094;
  int32_t _M0L6_2atmpS2679;
  int32_t _M0L8capacityS2681;
  int32_t _M0L6_2atmpS2680;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2682;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2aoldS4093;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2aparamS844;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4095 = _M0L4selfS842->$5;
  _M0L9old__headS841 = _M0L8_2afieldS4095;
  _M0L8capacityS2683 = _M0L4selfS842->$2;
  _M0L13new__capacityS843 = _M0L8capacityS2683 << 1;
  _M0L6_2atmpS2678 = 0;
  _M0L6_2atmpS2677
  = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE**)moonbit_make_ref_array(_M0L13new__capacityS843, _M0L6_2atmpS2678);
  _M0L6_2aoldS4094 = _M0L4selfS842->$0;
  if (_M0L9old__headS841) {
    moonbit_incref(_M0L9old__headS841);
  }
  moonbit_decref(_M0L6_2aoldS4094);
  _M0L4selfS842->$0 = _M0L6_2atmpS2677;
  _M0L4selfS842->$2 = _M0L13new__capacityS843;
  _M0L6_2atmpS2679 = _M0L13new__capacityS843 - 1;
  _M0L4selfS842->$3 = _M0L6_2atmpS2679;
  _M0L8capacityS2681 = _M0L4selfS842->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2680 = _M0FPB21calc__grow__threshold(_M0L8capacityS2681);
  _M0L4selfS842->$4 = _M0L6_2atmpS2680;
  _M0L4selfS842->$1 = 0;
  _M0L6_2atmpS2682 = 0;
  _M0L6_2aoldS4093 = _M0L4selfS842->$5;
  if (_M0L6_2aoldS4093) {
    moonbit_decref(_M0L6_2aoldS4093);
  }
  _M0L4selfS842->$5 = _M0L6_2atmpS2682;
  _M0L4selfS842->$6 = -1;
  _M0L8_2aparamS844 = _M0L9old__headS841;
  while (1) {
    if (_M0L8_2aparamS844 == 0) {
      if (_M0L8_2aparamS844) {
        moonbit_decref(_M0L8_2aparamS844);
      }
      moonbit_decref(_M0L4selfS842);
    } else {
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS845 =
        _M0L8_2aparamS844;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L4_2axS846 =
        _M0L7_2aSomeS845;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2afieldS4092 =
        _M0L4_2axS846->$1;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2anextS847 =
        _M0L8_2afieldS4092;
      moonbit_string_t _M0L8_2afieldS4091 = _M0L4_2axS846->$4;
      moonbit_string_t _M0L6_2akeyS848 = _M0L8_2afieldS4091;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS4090 = _M0L4_2axS846->$5;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2avalueS849 = _M0L8_2afieldS4090;
      int32_t _M0L8_2afieldS4089 = _M0L4_2axS846->$3;
      int32_t _M0L6_2acntS4444 = Moonbit_object_header(_M0L4_2axS846)->rc;
      int32_t _M0L7_2ahashS850;
      if (_M0L6_2acntS4444 > 1) {
        int32_t _M0L11_2anew__cntS4445 = _M0L6_2acntS4444 - 1;
        Moonbit_object_header(_M0L4_2axS846)->rc = _M0L11_2anew__cntS4445;
        moonbit_incref(_M0L8_2avalueS849);
        moonbit_incref(_M0L6_2akeyS848);
        if (_M0L7_2anextS847) {
          moonbit_incref(_M0L7_2anextS847);
        }
      } else if (_M0L6_2acntS4444 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS846);
      }
      _M0L7_2ahashS850 = _M0L8_2afieldS4089;
      moonbit_incref(_M0L4selfS842);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGsRPB4JsonEE(_M0L4selfS842, _M0L6_2akeyS848, _M0L8_2avalueS849, _M0L7_2ahashS850);
      _M0L8_2aparamS844 = _M0L7_2anextS847;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS720,
  moonbit_string_t _M0L3keyS726,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS727,
  int32_t _M0L4hashS722
) {
  int32_t _M0L14capacity__maskS2576;
  int32_t _M0L6_2atmpS2575;
  int32_t _M0L3pslS717;
  int32_t _M0L3idxS718;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2576 = _M0L4selfS720->$3;
  _M0L6_2atmpS2575 = _M0L4hashS722 & _M0L14capacity__maskS2576;
  _M0L3pslS717 = 0;
  _M0L3idxS718 = _M0L6_2atmpS2575;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4100 =
      _M0L4selfS720->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2574 =
      _M0L8_2afieldS4100;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4099;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS719;
    if (
      _M0L3idxS718 < 0
      || _M0L3idxS718 >= Moonbit_array_length(_M0L7entriesS2574)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4099
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2574[
        _M0L3idxS718
      ];
    _M0L7_2abindS719 = _M0L6_2atmpS4099;
    if (_M0L7_2abindS719 == 0) {
      int32_t _M0L4sizeS2559 = _M0L4selfS720->$1;
      int32_t _M0L8grow__atS2560 = _M0L4selfS720->$4;
      int32_t _M0L7_2abindS723;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS724;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS725;
      if (_M0L4sizeS2559 >= _M0L8grow__atS2560) {
        int32_t _M0L14capacity__maskS2562;
        int32_t _M0L6_2atmpS2561;
        moonbit_incref(_M0L4selfS720);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS720);
        _M0L14capacity__maskS2562 = _M0L4selfS720->$3;
        _M0L6_2atmpS2561 = _M0L4hashS722 & _M0L14capacity__maskS2562;
        _M0L3pslS717 = 0;
        _M0L3idxS718 = _M0L6_2atmpS2561;
        continue;
      }
      _M0L7_2abindS723 = _M0L4selfS720->$6;
      _M0L7_2abindS724 = 0;
      _M0L5entryS725
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS725)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS725->$0 = _M0L7_2abindS723;
      _M0L5entryS725->$1 = _M0L7_2abindS724;
      _M0L5entryS725->$2 = _M0L3pslS717;
      _M0L5entryS725->$3 = _M0L4hashS722;
      _M0L5entryS725->$4 = _M0L3keyS726;
      _M0L5entryS725->$5 = _M0L5valueS727;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS720, _M0L3idxS718, _M0L5entryS725);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS728 =
        _M0L7_2abindS719;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS729 =
        _M0L7_2aSomeS728;
      int32_t _M0L4hashS2564 = _M0L14_2acurr__entryS729->$3;
      int32_t _if__result_4624;
      int32_t _M0L3pslS2565;
      int32_t _M0L6_2atmpS2570;
      int32_t _M0L6_2atmpS2572;
      int32_t _M0L14capacity__maskS2573;
      int32_t _M0L6_2atmpS2571;
      if (_M0L4hashS2564 == _M0L4hashS722) {
        moonbit_string_t _M0L8_2afieldS4098 = _M0L14_2acurr__entryS729->$4;
        moonbit_string_t _M0L3keyS2563 = _M0L8_2afieldS4098;
        int32_t _M0L6_2atmpS4097;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4097
        = moonbit_val_array_equal(_M0L3keyS2563, _M0L3keyS726);
        _if__result_4624 = _M0L6_2atmpS4097;
      } else {
        _if__result_4624 = 0;
      }
      if (_if__result_4624) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4096;
        moonbit_incref(_M0L14_2acurr__entryS729);
        moonbit_decref(_M0L3keyS726);
        moonbit_decref(_M0L4selfS720);
        _M0L6_2aoldS4096 = _M0L14_2acurr__entryS729->$5;
        moonbit_decref(_M0L6_2aoldS4096);
        _M0L14_2acurr__entryS729->$5 = _M0L5valueS727;
        moonbit_decref(_M0L14_2acurr__entryS729);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS729);
      }
      _M0L3pslS2565 = _M0L14_2acurr__entryS729->$2;
      if (_M0L3pslS717 > _M0L3pslS2565) {
        int32_t _M0L4sizeS2566 = _M0L4selfS720->$1;
        int32_t _M0L8grow__atS2567 = _M0L4selfS720->$4;
        int32_t _M0L7_2abindS730;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS731;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS732;
        if (_M0L4sizeS2566 >= _M0L8grow__atS2567) {
          int32_t _M0L14capacity__maskS2569;
          int32_t _M0L6_2atmpS2568;
          moonbit_decref(_M0L14_2acurr__entryS729);
          moonbit_incref(_M0L4selfS720);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS720);
          _M0L14capacity__maskS2569 = _M0L4selfS720->$3;
          _M0L6_2atmpS2568 = _M0L4hashS722 & _M0L14capacity__maskS2569;
          _M0L3pslS717 = 0;
          _M0L3idxS718 = _M0L6_2atmpS2568;
          continue;
        }
        moonbit_incref(_M0L4selfS720);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS720, _M0L3idxS718, _M0L14_2acurr__entryS729);
        _M0L7_2abindS730 = _M0L4selfS720->$6;
        _M0L7_2abindS731 = 0;
        _M0L5entryS732
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS732)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS732->$0 = _M0L7_2abindS730;
        _M0L5entryS732->$1 = _M0L7_2abindS731;
        _M0L5entryS732->$2 = _M0L3pslS717;
        _M0L5entryS732->$3 = _M0L4hashS722;
        _M0L5entryS732->$4 = _M0L3keyS726;
        _M0L5entryS732->$5 = _M0L5valueS727;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS720, _M0L3idxS718, _M0L5entryS732);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS729);
      }
      _M0L6_2atmpS2570 = _M0L3pslS717 + 1;
      _M0L6_2atmpS2572 = _M0L3idxS718 + 1;
      _M0L14capacity__maskS2573 = _M0L4selfS720->$3;
      _M0L6_2atmpS2571 = _M0L6_2atmpS2572 & _M0L14capacity__maskS2573;
      _M0L3pslS717 = _M0L6_2atmpS2570;
      _M0L3idxS718 = _M0L6_2atmpS2571;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS736,
  int32_t _M0L3keyS742,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS743,
  int32_t _M0L4hashS738
) {
  int32_t _M0L14capacity__maskS2594;
  int32_t _M0L6_2atmpS2593;
  int32_t _M0L3pslS733;
  int32_t _M0L3idxS734;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2594 = _M0L4selfS736->$3;
  _M0L6_2atmpS2593 = _M0L4hashS738 & _M0L14capacity__maskS2594;
  _M0L3pslS733 = 0;
  _M0L3idxS734 = _M0L6_2atmpS2593;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4103 =
      _M0L4selfS736->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2592 =
      _M0L8_2afieldS4103;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4102;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS735;
    if (
      _M0L3idxS734 < 0
      || _M0L3idxS734 >= Moonbit_array_length(_M0L7entriesS2592)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4102
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2592[
        _M0L3idxS734
      ];
    _M0L7_2abindS735 = _M0L6_2atmpS4102;
    if (_M0L7_2abindS735 == 0) {
      int32_t _M0L4sizeS2577 = _M0L4selfS736->$1;
      int32_t _M0L8grow__atS2578 = _M0L4selfS736->$4;
      int32_t _M0L7_2abindS739;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS740;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS741;
      if (_M0L4sizeS2577 >= _M0L8grow__atS2578) {
        int32_t _M0L14capacity__maskS2580;
        int32_t _M0L6_2atmpS2579;
        moonbit_incref(_M0L4selfS736);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS736);
        _M0L14capacity__maskS2580 = _M0L4selfS736->$3;
        _M0L6_2atmpS2579 = _M0L4hashS738 & _M0L14capacity__maskS2580;
        _M0L3pslS733 = 0;
        _M0L3idxS734 = _M0L6_2atmpS2579;
        continue;
      }
      _M0L7_2abindS739 = _M0L4selfS736->$6;
      _M0L7_2abindS740 = 0;
      _M0L5entryS741
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS741)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS741->$0 = _M0L7_2abindS739;
      _M0L5entryS741->$1 = _M0L7_2abindS740;
      _M0L5entryS741->$2 = _M0L3pslS733;
      _M0L5entryS741->$3 = _M0L4hashS738;
      _M0L5entryS741->$4 = _M0L3keyS742;
      _M0L5entryS741->$5 = _M0L5valueS743;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS736, _M0L3idxS734, _M0L5entryS741);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS744 =
        _M0L7_2abindS735;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS745 =
        _M0L7_2aSomeS744;
      int32_t _M0L4hashS2582 = _M0L14_2acurr__entryS745->$3;
      int32_t _if__result_4626;
      int32_t _M0L3pslS2583;
      int32_t _M0L6_2atmpS2588;
      int32_t _M0L6_2atmpS2590;
      int32_t _M0L14capacity__maskS2591;
      int32_t _M0L6_2atmpS2589;
      if (_M0L4hashS2582 == _M0L4hashS738) {
        int32_t _M0L3keyS2581 = _M0L14_2acurr__entryS745->$4;
        _if__result_4626 = _M0L3keyS2581 == _M0L3keyS742;
      } else {
        _if__result_4626 = 0;
      }
      if (_if__result_4626) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4101;
        moonbit_incref(_M0L14_2acurr__entryS745);
        moonbit_decref(_M0L4selfS736);
        _M0L6_2aoldS4101 = _M0L14_2acurr__entryS745->$5;
        moonbit_decref(_M0L6_2aoldS4101);
        _M0L14_2acurr__entryS745->$5 = _M0L5valueS743;
        moonbit_decref(_M0L14_2acurr__entryS745);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS745);
      }
      _M0L3pslS2583 = _M0L14_2acurr__entryS745->$2;
      if (_M0L3pslS733 > _M0L3pslS2583) {
        int32_t _M0L4sizeS2584 = _M0L4selfS736->$1;
        int32_t _M0L8grow__atS2585 = _M0L4selfS736->$4;
        int32_t _M0L7_2abindS746;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS747;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS748;
        if (_M0L4sizeS2584 >= _M0L8grow__atS2585) {
          int32_t _M0L14capacity__maskS2587;
          int32_t _M0L6_2atmpS2586;
          moonbit_decref(_M0L14_2acurr__entryS745);
          moonbit_incref(_M0L4selfS736);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS736);
          _M0L14capacity__maskS2587 = _M0L4selfS736->$3;
          _M0L6_2atmpS2586 = _M0L4hashS738 & _M0L14capacity__maskS2587;
          _M0L3pslS733 = 0;
          _M0L3idxS734 = _M0L6_2atmpS2586;
          continue;
        }
        moonbit_incref(_M0L4selfS736);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS736, _M0L3idxS734, _M0L14_2acurr__entryS745);
        _M0L7_2abindS746 = _M0L4selfS736->$6;
        _M0L7_2abindS747 = 0;
        _M0L5entryS748
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS748)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS748->$0 = _M0L7_2abindS746;
        _M0L5entryS748->$1 = _M0L7_2abindS747;
        _M0L5entryS748->$2 = _M0L3pslS733;
        _M0L5entryS748->$3 = _M0L4hashS738;
        _M0L5entryS748->$4 = _M0L3keyS742;
        _M0L5entryS748->$5 = _M0L5valueS743;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS736, _M0L3idxS734, _M0L5entryS748);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS745);
      }
      _M0L6_2atmpS2588 = _M0L3pslS733 + 1;
      _M0L6_2atmpS2590 = _M0L3idxS734 + 1;
      _M0L14capacity__maskS2591 = _M0L4selfS736->$3;
      _M0L6_2atmpS2589 = _M0L6_2atmpS2590 & _M0L14capacity__maskS2591;
      _M0L3pslS733 = _M0L6_2atmpS2588;
      _M0L3idxS734 = _M0L6_2atmpS2589;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS752,
  moonbit_string_t _M0L3keyS758,
  void* _M0L5valueS759,
  int32_t _M0L4hashS754
) {
  int32_t _M0L14capacity__maskS2612;
  int32_t _M0L6_2atmpS2611;
  int32_t _M0L3pslS749;
  int32_t _M0L3idxS750;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2612 = _M0L4selfS752->$3;
  _M0L6_2atmpS2611 = _M0L4hashS754 & _M0L14capacity__maskS2612;
  _M0L3pslS749 = 0;
  _M0L3idxS750 = _M0L6_2atmpS2611;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4108 =
      _M0L4selfS752->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7entriesS2610 =
      _M0L8_2afieldS4108;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS4107;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS751;
    if (
      _M0L3idxS750 < 0
      || _M0L3idxS750 >= Moonbit_array_length(_M0L7entriesS2610)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4107
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L7entriesS2610[
        _M0L3idxS750
      ];
    _M0L7_2abindS751 = _M0L6_2atmpS4107;
    if (_M0L7_2abindS751 == 0) {
      int32_t _M0L4sizeS2595 = _M0L4selfS752->$1;
      int32_t _M0L8grow__atS2596 = _M0L4selfS752->$4;
      int32_t _M0L7_2abindS755;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS756;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5entryS757;
      if (_M0L4sizeS2595 >= _M0L8grow__atS2596) {
        int32_t _M0L14capacity__maskS2598;
        int32_t _M0L6_2atmpS2597;
        moonbit_incref(_M0L4selfS752);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS752);
        _M0L14capacity__maskS2598 = _M0L4selfS752->$3;
        _M0L6_2atmpS2597 = _M0L4hashS754 & _M0L14capacity__maskS2598;
        _M0L3pslS749 = 0;
        _M0L3idxS750 = _M0L6_2atmpS2597;
        continue;
      }
      _M0L7_2abindS755 = _M0L4selfS752->$6;
      _M0L7_2abindS756 = 0;
      _M0L5entryS757
      = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE));
      Moonbit_object_header(_M0L5entryS757)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE, $1) >> 2, 3, 0);
      _M0L5entryS757->$0 = _M0L7_2abindS755;
      _M0L5entryS757->$1 = _M0L7_2abindS756;
      _M0L5entryS757->$2 = _M0L3pslS749;
      _M0L5entryS757->$3 = _M0L4hashS754;
      _M0L5entryS757->$4 = _M0L3keyS758;
      _M0L5entryS757->$5 = _M0L5valueS759;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS752, _M0L3idxS750, _M0L5entryS757);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS760 =
        _M0L7_2abindS751;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L14_2acurr__entryS761 =
        _M0L7_2aSomeS760;
      int32_t _M0L4hashS2600 = _M0L14_2acurr__entryS761->$3;
      int32_t _if__result_4628;
      int32_t _M0L3pslS2601;
      int32_t _M0L6_2atmpS2606;
      int32_t _M0L6_2atmpS2608;
      int32_t _M0L14capacity__maskS2609;
      int32_t _M0L6_2atmpS2607;
      if (_M0L4hashS2600 == _M0L4hashS754) {
        moonbit_string_t _M0L8_2afieldS4106 = _M0L14_2acurr__entryS761->$4;
        moonbit_string_t _M0L3keyS2599 = _M0L8_2afieldS4106;
        int32_t _M0L6_2atmpS4105;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4105
        = moonbit_val_array_equal(_M0L3keyS2599, _M0L3keyS758);
        _if__result_4628 = _M0L6_2atmpS4105;
      } else {
        _if__result_4628 = 0;
      }
      if (_if__result_4628) {
        void* _M0L6_2aoldS4104;
        moonbit_incref(_M0L14_2acurr__entryS761);
        moonbit_decref(_M0L3keyS758);
        moonbit_decref(_M0L4selfS752);
        _M0L6_2aoldS4104 = _M0L14_2acurr__entryS761->$5;
        moonbit_decref(_M0L6_2aoldS4104);
        _M0L14_2acurr__entryS761->$5 = _M0L5valueS759;
        moonbit_decref(_M0L14_2acurr__entryS761);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS761);
      }
      _M0L3pslS2601 = _M0L14_2acurr__entryS761->$2;
      if (_M0L3pslS749 > _M0L3pslS2601) {
        int32_t _M0L4sizeS2602 = _M0L4selfS752->$1;
        int32_t _M0L8grow__atS2603 = _M0L4selfS752->$4;
        int32_t _M0L7_2abindS762;
        struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS763;
        struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5entryS764;
        if (_M0L4sizeS2602 >= _M0L8grow__atS2603) {
          int32_t _M0L14capacity__maskS2605;
          int32_t _M0L6_2atmpS2604;
          moonbit_decref(_M0L14_2acurr__entryS761);
          moonbit_incref(_M0L4selfS752);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS752);
          _M0L14capacity__maskS2605 = _M0L4selfS752->$3;
          _M0L6_2atmpS2604 = _M0L4hashS754 & _M0L14capacity__maskS2605;
          _M0L3pslS749 = 0;
          _M0L3idxS750 = _M0L6_2atmpS2604;
          continue;
        }
        moonbit_incref(_M0L4selfS752);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS752, _M0L3idxS750, _M0L14_2acurr__entryS761);
        _M0L7_2abindS762 = _M0L4selfS752->$6;
        _M0L7_2abindS763 = 0;
        _M0L5entryS764
        = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE));
        Moonbit_object_header(_M0L5entryS764)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE, $1) >> 2, 3, 0);
        _M0L5entryS764->$0 = _M0L7_2abindS762;
        _M0L5entryS764->$1 = _M0L7_2abindS763;
        _M0L5entryS764->$2 = _M0L3pslS749;
        _M0L5entryS764->$3 = _M0L4hashS754;
        _M0L5entryS764->$4 = _M0L3keyS758;
        _M0L5entryS764->$5 = _M0L5valueS759;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS752, _M0L3idxS750, _M0L5entryS764);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS761);
      }
      _M0L6_2atmpS2606 = _M0L3pslS749 + 1;
      _M0L6_2atmpS2608 = _M0L3idxS750 + 1;
      _M0L14capacity__maskS2609 = _M0L4selfS752->$3;
      _M0L6_2atmpS2607 = _M0L6_2atmpS2608 & _M0L14capacity__maskS2609;
      _M0L3pslS749 = _M0L6_2atmpS2606;
      _M0L3idxS750 = _M0L6_2atmpS2607;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS768,
  moonbit_string_t _M0L3keyS774,
  void* _M0L5valueS775,
  int32_t _M0L4hashS770
) {
  int32_t _M0L14capacity__maskS2630;
  int32_t _M0L6_2atmpS2629;
  int32_t _M0L3pslS765;
  int32_t _M0L3idxS766;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2630 = _M0L4selfS768->$3;
  _M0L6_2atmpS2629 = _M0L4hashS770 & _M0L14capacity__maskS2630;
  _M0L3pslS765 = 0;
  _M0L3idxS766 = _M0L6_2atmpS2629;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4113 = _M0L4selfS768->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2628 = _M0L8_2afieldS4113;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4112;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS767;
    if (
      _M0L3idxS766 < 0
      || _M0L3idxS766 >= Moonbit_array_length(_M0L7entriesS2628)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4112
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2628[_M0L3idxS766];
    _M0L7_2abindS767 = _M0L6_2atmpS4112;
    if (_M0L7_2abindS767 == 0) {
      int32_t _M0L4sizeS2613 = _M0L4selfS768->$1;
      int32_t _M0L8grow__atS2614 = _M0L4selfS768->$4;
      int32_t _M0L7_2abindS771;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS772;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS773;
      if (_M0L4sizeS2613 >= _M0L8grow__atS2614) {
        int32_t _M0L14capacity__maskS2616;
        int32_t _M0L6_2atmpS2615;
        moonbit_incref(_M0L4selfS768);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS768);
        _M0L14capacity__maskS2616 = _M0L4selfS768->$3;
        _M0L6_2atmpS2615 = _M0L4hashS770 & _M0L14capacity__maskS2616;
        _M0L3pslS765 = 0;
        _M0L3idxS766 = _M0L6_2atmpS2615;
        continue;
      }
      _M0L7_2abindS771 = _M0L4selfS768->$6;
      _M0L7_2abindS772 = 0;
      _M0L5entryS773
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS773)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS773->$0 = _M0L7_2abindS771;
      _M0L5entryS773->$1 = _M0L7_2abindS772;
      _M0L5entryS773->$2 = _M0L3pslS765;
      _M0L5entryS773->$3 = _M0L4hashS770;
      _M0L5entryS773->$4 = _M0L3keyS774;
      _M0L5entryS773->$5 = _M0L5valueS775;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS768, _M0L3idxS766, _M0L5entryS773);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS776 = _M0L7_2abindS767;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS777 =
        _M0L7_2aSomeS776;
      int32_t _M0L4hashS2618 = _M0L14_2acurr__entryS777->$3;
      int32_t _if__result_4630;
      int32_t _M0L3pslS2619;
      int32_t _M0L6_2atmpS2624;
      int32_t _M0L6_2atmpS2626;
      int32_t _M0L14capacity__maskS2627;
      int32_t _M0L6_2atmpS2625;
      if (_M0L4hashS2618 == _M0L4hashS770) {
        moonbit_string_t _M0L8_2afieldS4111 = _M0L14_2acurr__entryS777->$4;
        moonbit_string_t _M0L3keyS2617 = _M0L8_2afieldS4111;
        int32_t _M0L6_2atmpS4110;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4110
        = moonbit_val_array_equal(_M0L3keyS2617, _M0L3keyS774);
        _if__result_4630 = _M0L6_2atmpS4110;
      } else {
        _if__result_4630 = 0;
      }
      if (_if__result_4630) {
        void* _M0L6_2aoldS4109;
        moonbit_incref(_M0L14_2acurr__entryS777);
        moonbit_decref(_M0L3keyS774);
        moonbit_decref(_M0L4selfS768);
        _M0L6_2aoldS4109 = _M0L14_2acurr__entryS777->$5;
        moonbit_decref(_M0L6_2aoldS4109);
        _M0L14_2acurr__entryS777->$5 = _M0L5valueS775;
        moonbit_decref(_M0L14_2acurr__entryS777);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS777);
      }
      _M0L3pslS2619 = _M0L14_2acurr__entryS777->$2;
      if (_M0L3pslS765 > _M0L3pslS2619) {
        int32_t _M0L4sizeS2620 = _M0L4selfS768->$1;
        int32_t _M0L8grow__atS2621 = _M0L4selfS768->$4;
        int32_t _M0L7_2abindS778;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS779;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS780;
        if (_M0L4sizeS2620 >= _M0L8grow__atS2621) {
          int32_t _M0L14capacity__maskS2623;
          int32_t _M0L6_2atmpS2622;
          moonbit_decref(_M0L14_2acurr__entryS777);
          moonbit_incref(_M0L4selfS768);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS768);
          _M0L14capacity__maskS2623 = _M0L4selfS768->$3;
          _M0L6_2atmpS2622 = _M0L4hashS770 & _M0L14capacity__maskS2623;
          _M0L3pslS765 = 0;
          _M0L3idxS766 = _M0L6_2atmpS2622;
          continue;
        }
        moonbit_incref(_M0L4selfS768);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS768, _M0L3idxS766, _M0L14_2acurr__entryS777);
        _M0L7_2abindS778 = _M0L4selfS768->$6;
        _M0L7_2abindS779 = 0;
        _M0L5entryS780
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS780)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS780->$0 = _M0L7_2abindS778;
        _M0L5entryS780->$1 = _M0L7_2abindS779;
        _M0L5entryS780->$2 = _M0L3pslS765;
        _M0L5entryS780->$3 = _M0L4hashS770;
        _M0L5entryS780->$4 = _M0L3keyS774;
        _M0L5entryS780->$5 = _M0L5valueS775;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS768, _M0L3idxS766, _M0L5entryS780);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS777);
      }
      _M0L6_2atmpS2624 = _M0L3pslS765 + 1;
      _M0L6_2atmpS2626 = _M0L3idxS766 + 1;
      _M0L14capacity__maskS2627 = _M0L4selfS768->$3;
      _M0L6_2atmpS2625 = _M0L6_2atmpS2626 & _M0L14capacity__maskS2627;
      _M0L3pslS765 = _M0L6_2atmpS2624;
      _M0L3idxS766 = _M0L6_2atmpS2625;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS784,
  moonbit_string_t _M0L3keyS790,
  struct _M0TPB3MapGsRPB4JsonE* _M0L5valueS791,
  int32_t _M0L4hashS786
) {
  int32_t _M0L14capacity__maskS2648;
  int32_t _M0L6_2atmpS2647;
  int32_t _M0L3pslS781;
  int32_t _M0L3idxS782;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2648 = _M0L4selfS784->$3;
  _M0L6_2atmpS2647 = _M0L4hashS786 & _M0L14capacity__maskS2648;
  _M0L3pslS781 = 0;
  _M0L3idxS782 = _M0L6_2atmpS2647;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L8_2afieldS4118 =
      _M0L4selfS784->$0;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L7entriesS2646 =
      _M0L8_2afieldS4118;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS4117;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS783;
    if (
      _M0L3idxS782 < 0
      || _M0L3idxS782 >= Moonbit_array_length(_M0L7entriesS2646)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4117
    = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)_M0L7entriesS2646[
        _M0L3idxS782
      ];
    _M0L7_2abindS783 = _M0L6_2atmpS4117;
    if (_M0L7_2abindS783 == 0) {
      int32_t _M0L4sizeS2631 = _M0L4selfS784->$1;
      int32_t _M0L8grow__atS2632 = _M0L4selfS784->$4;
      int32_t _M0L7_2abindS787;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS788;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L5entryS789;
      if (_M0L4sizeS2631 >= _M0L8grow__atS2632) {
        int32_t _M0L14capacity__maskS2634;
        int32_t _M0L6_2atmpS2633;
        moonbit_incref(_M0L4selfS784);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGsRPB4JsonEE(_M0L4selfS784);
        _M0L14capacity__maskS2634 = _M0L4selfS784->$3;
        _M0L6_2atmpS2633 = _M0L4hashS786 & _M0L14capacity__maskS2634;
        _M0L3pslS781 = 0;
        _M0L3idxS782 = _M0L6_2atmpS2633;
        continue;
      }
      _M0L7_2abindS787 = _M0L4selfS784->$6;
      _M0L7_2abindS788 = 0;
      _M0L5entryS789
      = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE));
      Moonbit_object_header(_M0L5entryS789)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE, $1) >> 2, 3, 0);
      _M0L5entryS789->$0 = _M0L7_2abindS787;
      _M0L5entryS789->$1 = _M0L7_2abindS788;
      _M0L5entryS789->$2 = _M0L3pslS781;
      _M0L5entryS789->$3 = _M0L4hashS786;
      _M0L5entryS789->$4 = _M0L3keyS790;
      _M0L5entryS789->$5 = _M0L5valueS791;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGsRPB4JsonEE(_M0L4selfS784, _M0L3idxS782, _M0L5entryS789);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS792 =
        _M0L7_2abindS783;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L14_2acurr__entryS793 =
        _M0L7_2aSomeS792;
      int32_t _M0L4hashS2636 = _M0L14_2acurr__entryS793->$3;
      int32_t _if__result_4632;
      int32_t _M0L3pslS2637;
      int32_t _M0L6_2atmpS2642;
      int32_t _M0L6_2atmpS2644;
      int32_t _M0L14capacity__maskS2645;
      int32_t _M0L6_2atmpS2643;
      if (_M0L4hashS2636 == _M0L4hashS786) {
        moonbit_string_t _M0L8_2afieldS4116 = _M0L14_2acurr__entryS793->$4;
        moonbit_string_t _M0L3keyS2635 = _M0L8_2afieldS4116;
        int32_t _M0L6_2atmpS4115;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4115
        = moonbit_val_array_equal(_M0L3keyS2635, _M0L3keyS790);
        _if__result_4632 = _M0L6_2atmpS4115;
      } else {
        _if__result_4632 = 0;
      }
      if (_if__result_4632) {
        struct _M0TPB3MapGsRPB4JsonE* _M0L6_2aoldS4114;
        moonbit_incref(_M0L14_2acurr__entryS793);
        moonbit_decref(_M0L3keyS790);
        moonbit_decref(_M0L4selfS784);
        _M0L6_2aoldS4114 = _M0L14_2acurr__entryS793->$5;
        moonbit_decref(_M0L6_2aoldS4114);
        _M0L14_2acurr__entryS793->$5 = _M0L5valueS791;
        moonbit_decref(_M0L14_2acurr__entryS793);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS793);
      }
      _M0L3pslS2637 = _M0L14_2acurr__entryS793->$2;
      if (_M0L3pslS781 > _M0L3pslS2637) {
        int32_t _M0L4sizeS2638 = _M0L4selfS784->$1;
        int32_t _M0L8grow__atS2639 = _M0L4selfS784->$4;
        int32_t _M0L7_2abindS794;
        struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS795;
        struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L5entryS796;
        if (_M0L4sizeS2638 >= _M0L8grow__atS2639) {
          int32_t _M0L14capacity__maskS2641;
          int32_t _M0L6_2atmpS2640;
          moonbit_decref(_M0L14_2acurr__entryS793);
          moonbit_incref(_M0L4selfS784);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGsRPB4JsonEE(_M0L4selfS784);
          _M0L14capacity__maskS2641 = _M0L4selfS784->$3;
          _M0L6_2atmpS2640 = _M0L4hashS786 & _M0L14capacity__maskS2641;
          _M0L3pslS781 = 0;
          _M0L3idxS782 = _M0L6_2atmpS2640;
          continue;
        }
        moonbit_incref(_M0L4selfS784);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGsRPB4JsonEE(_M0L4selfS784, _M0L3idxS782, _M0L14_2acurr__entryS793);
        _M0L7_2abindS794 = _M0L4selfS784->$6;
        _M0L7_2abindS795 = 0;
        _M0L5entryS796
        = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE));
        Moonbit_object_header(_M0L5entryS796)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE, $1) >> 2, 3, 0);
        _M0L5entryS796->$0 = _M0L7_2abindS794;
        _M0L5entryS796->$1 = _M0L7_2abindS795;
        _M0L5entryS796->$2 = _M0L3pslS781;
        _M0L5entryS796->$3 = _M0L4hashS786;
        _M0L5entryS796->$4 = _M0L3keyS790;
        _M0L5entryS796->$5 = _M0L5valueS791;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGsRPB4JsonEE(_M0L4selfS784, _M0L3idxS782, _M0L5entryS796);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS793);
      }
      _M0L6_2atmpS2642 = _M0L3pslS781 + 1;
      _M0L6_2atmpS2644 = _M0L3idxS782 + 1;
      _M0L14capacity__maskS2645 = _M0L4selfS784->$3;
      _M0L6_2atmpS2643 = _M0L6_2atmpS2644 & _M0L14capacity__maskS2645;
      _M0L3pslS781 = _M0L6_2atmpS2642;
      _M0L3idxS782 = _M0L6_2atmpS2643;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS671,
  int32_t _M0L3idxS676,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS675
) {
  int32_t _M0L3pslS2494;
  int32_t _M0L6_2atmpS2490;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0L14capacity__maskS2493;
  int32_t _M0L6_2atmpS2491;
  int32_t _M0L3pslS667;
  int32_t _M0L3idxS668;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS669;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2494 = _M0L5entryS675->$2;
  _M0L6_2atmpS2490 = _M0L3pslS2494 + 1;
  _M0L6_2atmpS2492 = _M0L3idxS676 + 1;
  _M0L14capacity__maskS2493 = _M0L4selfS671->$3;
  _M0L6_2atmpS2491 = _M0L6_2atmpS2492 & _M0L14capacity__maskS2493;
  _M0L3pslS667 = _M0L6_2atmpS2490;
  _M0L3idxS668 = _M0L6_2atmpS2491;
  _M0L5entryS669 = _M0L5entryS675;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4120 =
      _M0L4selfS671->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2489 =
      _M0L8_2afieldS4120;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4119;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS670;
    if (
      _M0L3idxS668 < 0
      || _M0L3idxS668 >= Moonbit_array_length(_M0L7entriesS2489)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4119
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2489[
        _M0L3idxS668
      ];
    _M0L7_2abindS670 = _M0L6_2atmpS4119;
    if (_M0L7_2abindS670 == 0) {
      _M0L5entryS669->$2 = _M0L3pslS667;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS671, _M0L5entryS669, _M0L3idxS668);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS673 =
        _M0L7_2abindS670;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS674 =
        _M0L7_2aSomeS673;
      int32_t _M0L3pslS2479 = _M0L14_2acurr__entryS674->$2;
      if (_M0L3pslS667 > _M0L3pslS2479) {
        int32_t _M0L3pslS2484;
        int32_t _M0L6_2atmpS2480;
        int32_t _M0L6_2atmpS2482;
        int32_t _M0L14capacity__maskS2483;
        int32_t _M0L6_2atmpS2481;
        _M0L5entryS669->$2 = _M0L3pslS667;
        moonbit_incref(_M0L14_2acurr__entryS674);
        moonbit_incref(_M0L4selfS671);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS671, _M0L5entryS669, _M0L3idxS668);
        _M0L3pslS2484 = _M0L14_2acurr__entryS674->$2;
        _M0L6_2atmpS2480 = _M0L3pslS2484 + 1;
        _M0L6_2atmpS2482 = _M0L3idxS668 + 1;
        _M0L14capacity__maskS2483 = _M0L4selfS671->$3;
        _M0L6_2atmpS2481 = _M0L6_2atmpS2482 & _M0L14capacity__maskS2483;
        _M0L3pslS667 = _M0L6_2atmpS2480;
        _M0L3idxS668 = _M0L6_2atmpS2481;
        _M0L5entryS669 = _M0L14_2acurr__entryS674;
        continue;
      } else {
        int32_t _M0L6_2atmpS2485 = _M0L3pslS667 + 1;
        int32_t _M0L6_2atmpS2487 = _M0L3idxS668 + 1;
        int32_t _M0L14capacity__maskS2488 = _M0L4selfS671->$3;
        int32_t _M0L6_2atmpS2486 =
          _M0L6_2atmpS2487 & _M0L14capacity__maskS2488;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4634 =
          _M0L5entryS669;
        _M0L3pslS667 = _M0L6_2atmpS2485;
        _M0L3idxS668 = _M0L6_2atmpS2486;
        _M0L5entryS669 = _tmp_4634;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS681,
  int32_t _M0L3idxS686,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS685
) {
  int32_t _M0L3pslS2510;
  int32_t _M0L6_2atmpS2506;
  int32_t _M0L6_2atmpS2508;
  int32_t _M0L14capacity__maskS2509;
  int32_t _M0L6_2atmpS2507;
  int32_t _M0L3pslS677;
  int32_t _M0L3idxS678;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS679;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2510 = _M0L5entryS685->$2;
  _M0L6_2atmpS2506 = _M0L3pslS2510 + 1;
  _M0L6_2atmpS2508 = _M0L3idxS686 + 1;
  _M0L14capacity__maskS2509 = _M0L4selfS681->$3;
  _M0L6_2atmpS2507 = _M0L6_2atmpS2508 & _M0L14capacity__maskS2509;
  _M0L3pslS677 = _M0L6_2atmpS2506;
  _M0L3idxS678 = _M0L6_2atmpS2507;
  _M0L5entryS679 = _M0L5entryS685;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4122 =
      _M0L4selfS681->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2505 =
      _M0L8_2afieldS4122;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4121;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS680;
    if (
      _M0L3idxS678 < 0
      || _M0L3idxS678 >= Moonbit_array_length(_M0L7entriesS2505)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4121
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2505[
        _M0L3idxS678
      ];
    _M0L7_2abindS680 = _M0L6_2atmpS4121;
    if (_M0L7_2abindS680 == 0) {
      _M0L5entryS679->$2 = _M0L3pslS677;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS681, _M0L5entryS679, _M0L3idxS678);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS683 =
        _M0L7_2abindS680;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS684 =
        _M0L7_2aSomeS683;
      int32_t _M0L3pslS2495 = _M0L14_2acurr__entryS684->$2;
      if (_M0L3pslS677 > _M0L3pslS2495) {
        int32_t _M0L3pslS2500;
        int32_t _M0L6_2atmpS2496;
        int32_t _M0L6_2atmpS2498;
        int32_t _M0L14capacity__maskS2499;
        int32_t _M0L6_2atmpS2497;
        _M0L5entryS679->$2 = _M0L3pslS677;
        moonbit_incref(_M0L14_2acurr__entryS684);
        moonbit_incref(_M0L4selfS681);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS681, _M0L5entryS679, _M0L3idxS678);
        _M0L3pslS2500 = _M0L14_2acurr__entryS684->$2;
        _M0L6_2atmpS2496 = _M0L3pslS2500 + 1;
        _M0L6_2atmpS2498 = _M0L3idxS678 + 1;
        _M0L14capacity__maskS2499 = _M0L4selfS681->$3;
        _M0L6_2atmpS2497 = _M0L6_2atmpS2498 & _M0L14capacity__maskS2499;
        _M0L3pslS677 = _M0L6_2atmpS2496;
        _M0L3idxS678 = _M0L6_2atmpS2497;
        _M0L5entryS679 = _M0L14_2acurr__entryS684;
        continue;
      } else {
        int32_t _M0L6_2atmpS2501 = _M0L3pslS677 + 1;
        int32_t _M0L6_2atmpS2503 = _M0L3idxS678 + 1;
        int32_t _M0L14capacity__maskS2504 = _M0L4selfS681->$3;
        int32_t _M0L6_2atmpS2502 =
          _M0L6_2atmpS2503 & _M0L14capacity__maskS2504;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4636 =
          _M0L5entryS679;
        _M0L3pslS677 = _M0L6_2atmpS2501;
        _M0L3idxS678 = _M0L6_2atmpS2502;
        _M0L5entryS679 = _tmp_4636;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS691,
  int32_t _M0L3idxS696,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5entryS695
) {
  int32_t _M0L3pslS2526;
  int32_t _M0L6_2atmpS2522;
  int32_t _M0L6_2atmpS2524;
  int32_t _M0L14capacity__maskS2525;
  int32_t _M0L6_2atmpS2523;
  int32_t _M0L3pslS687;
  int32_t _M0L3idxS688;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5entryS689;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2526 = _M0L5entryS695->$2;
  _M0L6_2atmpS2522 = _M0L3pslS2526 + 1;
  _M0L6_2atmpS2524 = _M0L3idxS696 + 1;
  _M0L14capacity__maskS2525 = _M0L4selfS691->$3;
  _M0L6_2atmpS2523 = _M0L6_2atmpS2524 & _M0L14capacity__maskS2525;
  _M0L3pslS687 = _M0L6_2atmpS2522;
  _M0L3idxS688 = _M0L6_2atmpS2523;
  _M0L5entryS689 = _M0L5entryS695;
  while (1) {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4124 =
      _M0L4selfS691->$0;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7entriesS2521 =
      _M0L8_2afieldS4124;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS4123;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS690;
    if (
      _M0L3idxS688 < 0
      || _M0L3idxS688 >= Moonbit_array_length(_M0L7entriesS2521)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4123
    = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L7entriesS2521[
        _M0L3idxS688
      ];
    _M0L7_2abindS690 = _M0L6_2atmpS4123;
    if (_M0L7_2abindS690 == 0) {
      _M0L5entryS689->$2 = _M0L3pslS687;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS691, _M0L5entryS689, _M0L3idxS688);
      break;
    } else {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS693 =
        _M0L7_2abindS690;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L14_2acurr__entryS694 =
        _M0L7_2aSomeS693;
      int32_t _M0L3pslS2511 = _M0L14_2acurr__entryS694->$2;
      if (_M0L3pslS687 > _M0L3pslS2511) {
        int32_t _M0L3pslS2516;
        int32_t _M0L6_2atmpS2512;
        int32_t _M0L6_2atmpS2514;
        int32_t _M0L14capacity__maskS2515;
        int32_t _M0L6_2atmpS2513;
        _M0L5entryS689->$2 = _M0L3pslS687;
        moonbit_incref(_M0L14_2acurr__entryS694);
        moonbit_incref(_M0L4selfS691);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal6schema6SchemaE(_M0L4selfS691, _M0L5entryS689, _M0L3idxS688);
        _M0L3pslS2516 = _M0L14_2acurr__entryS694->$2;
        _M0L6_2atmpS2512 = _M0L3pslS2516 + 1;
        _M0L6_2atmpS2514 = _M0L3idxS688 + 1;
        _M0L14capacity__maskS2515 = _M0L4selfS691->$3;
        _M0L6_2atmpS2513 = _M0L6_2atmpS2514 & _M0L14capacity__maskS2515;
        _M0L3pslS687 = _M0L6_2atmpS2512;
        _M0L3idxS688 = _M0L6_2atmpS2513;
        _M0L5entryS689 = _M0L14_2acurr__entryS694;
        continue;
      } else {
        int32_t _M0L6_2atmpS2517 = _M0L3pslS687 + 1;
        int32_t _M0L6_2atmpS2519 = _M0L3idxS688 + 1;
        int32_t _M0L14capacity__maskS2520 = _M0L4selfS691->$3;
        int32_t _M0L6_2atmpS2518 =
          _M0L6_2atmpS2519 & _M0L14capacity__maskS2520;
        struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _tmp_4638 =
          _M0L5entryS689;
        _M0L3pslS687 = _M0L6_2atmpS2517;
        _M0L3idxS688 = _M0L6_2atmpS2518;
        _M0L5entryS689 = _tmp_4638;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS701,
  int32_t _M0L3idxS706,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS705
) {
  int32_t _M0L3pslS2542;
  int32_t _M0L6_2atmpS2538;
  int32_t _M0L6_2atmpS2540;
  int32_t _M0L14capacity__maskS2541;
  int32_t _M0L6_2atmpS2539;
  int32_t _M0L3pslS697;
  int32_t _M0L3idxS698;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS699;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2542 = _M0L5entryS705->$2;
  _M0L6_2atmpS2538 = _M0L3pslS2542 + 1;
  _M0L6_2atmpS2540 = _M0L3idxS706 + 1;
  _M0L14capacity__maskS2541 = _M0L4selfS701->$3;
  _M0L6_2atmpS2539 = _M0L6_2atmpS2540 & _M0L14capacity__maskS2541;
  _M0L3pslS697 = _M0L6_2atmpS2538;
  _M0L3idxS698 = _M0L6_2atmpS2539;
  _M0L5entryS699 = _M0L5entryS705;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4126 = _M0L4selfS701->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2537 = _M0L8_2afieldS4126;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4125;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS700;
    if (
      _M0L3idxS698 < 0
      || _M0L3idxS698 >= Moonbit_array_length(_M0L7entriesS2537)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4125
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2537[_M0L3idxS698];
    _M0L7_2abindS700 = _M0L6_2atmpS4125;
    if (_M0L7_2abindS700 == 0) {
      _M0L5entryS699->$2 = _M0L3pslS697;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS701, _M0L5entryS699, _M0L3idxS698);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS703 = _M0L7_2abindS700;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS704 =
        _M0L7_2aSomeS703;
      int32_t _M0L3pslS2527 = _M0L14_2acurr__entryS704->$2;
      if (_M0L3pslS697 > _M0L3pslS2527) {
        int32_t _M0L3pslS2532;
        int32_t _M0L6_2atmpS2528;
        int32_t _M0L6_2atmpS2530;
        int32_t _M0L14capacity__maskS2531;
        int32_t _M0L6_2atmpS2529;
        _M0L5entryS699->$2 = _M0L3pslS697;
        moonbit_incref(_M0L14_2acurr__entryS704);
        moonbit_incref(_M0L4selfS701);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS701, _M0L5entryS699, _M0L3idxS698);
        _M0L3pslS2532 = _M0L14_2acurr__entryS704->$2;
        _M0L6_2atmpS2528 = _M0L3pslS2532 + 1;
        _M0L6_2atmpS2530 = _M0L3idxS698 + 1;
        _M0L14capacity__maskS2531 = _M0L4selfS701->$3;
        _M0L6_2atmpS2529 = _M0L6_2atmpS2530 & _M0L14capacity__maskS2531;
        _M0L3pslS697 = _M0L6_2atmpS2528;
        _M0L3idxS698 = _M0L6_2atmpS2529;
        _M0L5entryS699 = _M0L14_2acurr__entryS704;
        continue;
      } else {
        int32_t _M0L6_2atmpS2533 = _M0L3pslS697 + 1;
        int32_t _M0L6_2atmpS2535 = _M0L3idxS698 + 1;
        int32_t _M0L14capacity__maskS2536 = _M0L4selfS701->$3;
        int32_t _M0L6_2atmpS2534 =
          _M0L6_2atmpS2535 & _M0L14capacity__maskS2536;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_4640 = _M0L5entryS699;
        _M0L3pslS697 = _M0L6_2atmpS2533;
        _M0L3idxS698 = _M0L6_2atmpS2534;
        _M0L5entryS699 = _tmp_4640;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS711,
  int32_t _M0L3idxS716,
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L5entryS715
) {
  int32_t _M0L3pslS2558;
  int32_t _M0L6_2atmpS2554;
  int32_t _M0L6_2atmpS2556;
  int32_t _M0L14capacity__maskS2557;
  int32_t _M0L6_2atmpS2555;
  int32_t _M0L3pslS707;
  int32_t _M0L3idxS708;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L5entryS709;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2558 = _M0L5entryS715->$2;
  _M0L6_2atmpS2554 = _M0L3pslS2558 + 1;
  _M0L6_2atmpS2556 = _M0L3idxS716 + 1;
  _M0L14capacity__maskS2557 = _M0L4selfS711->$3;
  _M0L6_2atmpS2555 = _M0L6_2atmpS2556 & _M0L14capacity__maskS2557;
  _M0L3pslS707 = _M0L6_2atmpS2554;
  _M0L3idxS708 = _M0L6_2atmpS2555;
  _M0L5entryS709 = _M0L5entryS715;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L8_2afieldS4128 =
      _M0L4selfS711->$0;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L7entriesS2553 =
      _M0L8_2afieldS4128;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS4127;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS710;
    if (
      _M0L3idxS708 < 0
      || _M0L3idxS708 >= Moonbit_array_length(_M0L7entriesS2553)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4127
    = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)_M0L7entriesS2553[
        _M0L3idxS708
      ];
    _M0L7_2abindS710 = _M0L6_2atmpS4127;
    if (_M0L7_2abindS710 == 0) {
      _M0L5entryS709->$2 = _M0L3pslS707;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGsRPB4JsonEE(_M0L4selfS711, _M0L5entryS709, _M0L3idxS708);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS713 =
        _M0L7_2abindS710;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L14_2acurr__entryS714 =
        _M0L7_2aSomeS713;
      int32_t _M0L3pslS2543 = _M0L14_2acurr__entryS714->$2;
      if (_M0L3pslS707 > _M0L3pslS2543) {
        int32_t _M0L3pslS2548;
        int32_t _M0L6_2atmpS2544;
        int32_t _M0L6_2atmpS2546;
        int32_t _M0L14capacity__maskS2547;
        int32_t _M0L6_2atmpS2545;
        _M0L5entryS709->$2 = _M0L3pslS707;
        moonbit_incref(_M0L14_2acurr__entryS714);
        moonbit_incref(_M0L4selfS711);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGsRPB4JsonEE(_M0L4selfS711, _M0L5entryS709, _M0L3idxS708);
        _M0L3pslS2548 = _M0L14_2acurr__entryS714->$2;
        _M0L6_2atmpS2544 = _M0L3pslS2548 + 1;
        _M0L6_2atmpS2546 = _M0L3idxS708 + 1;
        _M0L14capacity__maskS2547 = _M0L4selfS711->$3;
        _M0L6_2atmpS2545 = _M0L6_2atmpS2546 & _M0L14capacity__maskS2547;
        _M0L3pslS707 = _M0L6_2atmpS2544;
        _M0L3idxS708 = _M0L6_2atmpS2545;
        _M0L5entryS709 = _M0L14_2acurr__entryS714;
        continue;
      } else {
        int32_t _M0L6_2atmpS2549 = _M0L3pslS707 + 1;
        int32_t _M0L6_2atmpS2551 = _M0L3idxS708 + 1;
        int32_t _M0L14capacity__maskS2552 = _M0L4selfS711->$3;
        int32_t _M0L6_2atmpS2550 =
          _M0L6_2atmpS2551 & _M0L14capacity__maskS2552;
        struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _tmp_4642 = _M0L5entryS709;
        _M0L3pslS707 = _M0L6_2atmpS2549;
        _M0L3idxS708 = _M0L6_2atmpS2550;
        _M0L5entryS709 = _tmp_4642;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS637,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS639,
  int32_t _M0L8new__idxS638
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4131;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2469;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2470;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4130;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4129;
  int32_t _M0L6_2acntS4446;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS640;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4131 = _M0L4selfS637->$0;
  _M0L7entriesS2469 = _M0L8_2afieldS4131;
  moonbit_incref(_M0L5entryS639);
  _M0L6_2atmpS2470 = _M0L5entryS639;
  if (
    _M0L8new__idxS638 < 0
    || _M0L8new__idxS638 >= Moonbit_array_length(_M0L7entriesS2469)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4130
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2469[
      _M0L8new__idxS638
    ];
  if (_M0L6_2aoldS4130) {
    moonbit_decref(_M0L6_2aoldS4130);
  }
  _M0L7entriesS2469[_M0L8new__idxS638] = _M0L6_2atmpS2470;
  _M0L8_2afieldS4129 = _M0L5entryS639->$1;
  _M0L6_2acntS4446 = Moonbit_object_header(_M0L5entryS639)->rc;
  if (_M0L6_2acntS4446 > 1) {
    int32_t _M0L11_2anew__cntS4449 = _M0L6_2acntS4446 - 1;
    Moonbit_object_header(_M0L5entryS639)->rc = _M0L11_2anew__cntS4449;
    if (_M0L8_2afieldS4129) {
      moonbit_incref(_M0L8_2afieldS4129);
    }
  } else if (_M0L6_2acntS4446 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4448 =
      _M0L5entryS639->$5;
    moonbit_string_t _M0L8_2afieldS4447;
    moonbit_decref(_M0L8_2afieldS4448);
    _M0L8_2afieldS4447 = _M0L5entryS639->$4;
    moonbit_decref(_M0L8_2afieldS4447);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS639);
  }
  _M0L7_2abindS640 = _M0L8_2afieldS4129;
  if (_M0L7_2abindS640 == 0) {
    if (_M0L7_2abindS640) {
      moonbit_decref(_M0L7_2abindS640);
    }
    _M0L4selfS637->$6 = _M0L8new__idxS638;
    moonbit_decref(_M0L4selfS637);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS641;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS642;
    moonbit_decref(_M0L4selfS637);
    _M0L7_2aSomeS641 = _M0L7_2abindS640;
    _M0L7_2anextS642 = _M0L7_2aSomeS641;
    _M0L7_2anextS642->$0 = _M0L8new__idxS638;
    moonbit_decref(_M0L7_2anextS642);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS643,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS645,
  int32_t _M0L8new__idxS644
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4134;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2471;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2472;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4133;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4132;
  int32_t _M0L6_2acntS4450;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS646;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4134 = _M0L4selfS643->$0;
  _M0L7entriesS2471 = _M0L8_2afieldS4134;
  moonbit_incref(_M0L5entryS645);
  _M0L6_2atmpS2472 = _M0L5entryS645;
  if (
    _M0L8new__idxS644 < 0
    || _M0L8new__idxS644 >= Moonbit_array_length(_M0L7entriesS2471)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4133
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2471[
      _M0L8new__idxS644
    ];
  if (_M0L6_2aoldS4133) {
    moonbit_decref(_M0L6_2aoldS4133);
  }
  _M0L7entriesS2471[_M0L8new__idxS644] = _M0L6_2atmpS2472;
  _M0L8_2afieldS4132 = _M0L5entryS645->$1;
  _M0L6_2acntS4450 = Moonbit_object_header(_M0L5entryS645)->rc;
  if (_M0L6_2acntS4450 > 1) {
    int32_t _M0L11_2anew__cntS4452 = _M0L6_2acntS4450 - 1;
    Moonbit_object_header(_M0L5entryS645)->rc = _M0L11_2anew__cntS4452;
    if (_M0L8_2afieldS4132) {
      moonbit_incref(_M0L8_2afieldS4132);
    }
  } else if (_M0L6_2acntS4450 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4451 =
      _M0L5entryS645->$5;
    moonbit_decref(_M0L8_2afieldS4451);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS645);
  }
  _M0L7_2abindS646 = _M0L8_2afieldS4132;
  if (_M0L7_2abindS646 == 0) {
    if (_M0L7_2abindS646) {
      moonbit_decref(_M0L7_2abindS646);
    }
    _M0L4selfS643->$6 = _M0L8new__idxS644;
    moonbit_decref(_M0L4selfS643);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS647;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS648;
    moonbit_decref(_M0L4selfS643);
    _M0L7_2aSomeS647 = _M0L7_2abindS646;
    _M0L7_2anextS648 = _M0L7_2aSomeS647;
    _M0L7_2anextS648->$0 = _M0L8new__idxS644;
    moonbit_decref(_M0L7_2anextS648);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS649,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5entryS651,
  int32_t _M0L8new__idxS650
) {
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4137;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7entriesS2473;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2474;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aoldS4136;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L8_2afieldS4135;
  int32_t _M0L6_2acntS4453;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS652;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4137 = _M0L4selfS649->$0;
  _M0L7entriesS2473 = _M0L8_2afieldS4137;
  moonbit_incref(_M0L5entryS651);
  _M0L6_2atmpS2474 = _M0L5entryS651;
  if (
    _M0L8new__idxS650 < 0
    || _M0L8new__idxS650 >= Moonbit_array_length(_M0L7entriesS2473)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4136
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L7entriesS2473[
      _M0L8new__idxS650
    ];
  if (_M0L6_2aoldS4136) {
    moonbit_decref(_M0L6_2aoldS4136);
  }
  _M0L7entriesS2473[_M0L8new__idxS650] = _M0L6_2atmpS2474;
  _M0L8_2afieldS4135 = _M0L5entryS651->$1;
  _M0L6_2acntS4453 = Moonbit_object_header(_M0L5entryS651)->rc;
  if (_M0L6_2acntS4453 > 1) {
    int32_t _M0L11_2anew__cntS4456 = _M0L6_2acntS4453 - 1;
    Moonbit_object_header(_M0L5entryS651)->rc = _M0L11_2anew__cntS4456;
    if (_M0L8_2afieldS4135) {
      moonbit_incref(_M0L8_2afieldS4135);
    }
  } else if (_M0L6_2acntS4453 == 1) {
    void* _M0L8_2afieldS4455 = _M0L5entryS651->$5;
    moonbit_string_t _M0L8_2afieldS4454;
    moonbit_decref(_M0L8_2afieldS4455);
    _M0L8_2afieldS4454 = _M0L5entryS651->$4;
    moonbit_decref(_M0L8_2afieldS4454);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS651);
  }
  _M0L7_2abindS652 = _M0L8_2afieldS4135;
  if (_M0L7_2abindS652 == 0) {
    if (_M0L7_2abindS652) {
      moonbit_decref(_M0L7_2abindS652);
    }
    _M0L4selfS649->$6 = _M0L8new__idxS650;
    moonbit_decref(_M0L4selfS649);
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS653;
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2anextS654;
    moonbit_decref(_M0L4selfS649);
    _M0L7_2aSomeS653 = _M0L7_2abindS652;
    _M0L7_2anextS654 = _M0L7_2aSomeS653;
    _M0L7_2anextS654->$0 = _M0L8new__idxS650;
    moonbit_decref(_M0L7_2anextS654);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS655,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS657,
  int32_t _M0L8new__idxS656
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4140;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2475;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2476;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4139;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4138;
  int32_t _M0L6_2acntS4457;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS658;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4140 = _M0L4selfS655->$0;
  _M0L7entriesS2475 = _M0L8_2afieldS4140;
  moonbit_incref(_M0L5entryS657);
  _M0L6_2atmpS2476 = _M0L5entryS657;
  if (
    _M0L8new__idxS656 < 0
    || _M0L8new__idxS656 >= Moonbit_array_length(_M0L7entriesS2475)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4139
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2475[_M0L8new__idxS656];
  if (_M0L6_2aoldS4139) {
    moonbit_decref(_M0L6_2aoldS4139);
  }
  _M0L7entriesS2475[_M0L8new__idxS656] = _M0L6_2atmpS2476;
  _M0L8_2afieldS4138 = _M0L5entryS657->$1;
  _M0L6_2acntS4457 = Moonbit_object_header(_M0L5entryS657)->rc;
  if (_M0L6_2acntS4457 > 1) {
    int32_t _M0L11_2anew__cntS4460 = _M0L6_2acntS4457 - 1;
    Moonbit_object_header(_M0L5entryS657)->rc = _M0L11_2anew__cntS4460;
    if (_M0L8_2afieldS4138) {
      moonbit_incref(_M0L8_2afieldS4138);
    }
  } else if (_M0L6_2acntS4457 == 1) {
    void* _M0L8_2afieldS4459 = _M0L5entryS657->$5;
    moonbit_string_t _M0L8_2afieldS4458;
    moonbit_decref(_M0L8_2afieldS4459);
    _M0L8_2afieldS4458 = _M0L5entryS657->$4;
    moonbit_decref(_M0L8_2afieldS4458);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS657);
  }
  _M0L7_2abindS658 = _M0L8_2afieldS4138;
  if (_M0L7_2abindS658 == 0) {
    if (_M0L7_2abindS658) {
      moonbit_decref(_M0L7_2abindS658);
    }
    _M0L4selfS655->$6 = _M0L8new__idxS656;
    moonbit_decref(_M0L4selfS655);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS659;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS660;
    moonbit_decref(_M0L4selfS655);
    _M0L7_2aSomeS659 = _M0L7_2abindS658;
    _M0L7_2anextS660 = _M0L7_2aSomeS659;
    _M0L7_2anextS660->$0 = _M0L8new__idxS656;
    moonbit_decref(_M0L7_2anextS660);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS661,
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L5entryS663,
  int32_t _M0L8new__idxS662
) {
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L8_2afieldS4143;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L7entriesS2477;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2478;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2aoldS4142;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L8_2afieldS4141;
  int32_t _M0L6_2acntS4461;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS664;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4143 = _M0L4selfS661->$0;
  _M0L7entriesS2477 = _M0L8_2afieldS4143;
  moonbit_incref(_M0L5entryS663);
  _M0L6_2atmpS2478 = _M0L5entryS663;
  if (
    _M0L8new__idxS662 < 0
    || _M0L8new__idxS662 >= Moonbit_array_length(_M0L7entriesS2477)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4142
  = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)_M0L7entriesS2477[
      _M0L8new__idxS662
    ];
  if (_M0L6_2aoldS4142) {
    moonbit_decref(_M0L6_2aoldS4142);
  }
  _M0L7entriesS2477[_M0L8new__idxS662] = _M0L6_2atmpS2478;
  _M0L8_2afieldS4141 = _M0L5entryS663->$1;
  _M0L6_2acntS4461 = Moonbit_object_header(_M0L5entryS663)->rc;
  if (_M0L6_2acntS4461 > 1) {
    int32_t _M0L11_2anew__cntS4464 = _M0L6_2acntS4461 - 1;
    Moonbit_object_header(_M0L5entryS663)->rc = _M0L11_2anew__cntS4464;
    if (_M0L8_2afieldS4141) {
      moonbit_incref(_M0L8_2afieldS4141);
    }
  } else if (_M0L6_2acntS4461 == 1) {
    struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS4463 = _M0L5entryS663->$5;
    moonbit_string_t _M0L8_2afieldS4462;
    moonbit_decref(_M0L8_2afieldS4463);
    _M0L8_2afieldS4462 = _M0L5entryS663->$4;
    moonbit_decref(_M0L8_2afieldS4462);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS663);
  }
  _M0L7_2abindS664 = _M0L8_2afieldS4141;
  if (_M0L7_2abindS664 == 0) {
    if (_M0L7_2abindS664) {
      moonbit_decref(_M0L7_2abindS664);
    }
    _M0L4selfS661->$6 = _M0L8new__idxS662;
    moonbit_decref(_M0L4selfS661);
  } else {
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS665;
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2anextS666;
    moonbit_decref(_M0L4selfS661);
    _M0L7_2aSomeS665 = _M0L7_2abindS664;
    _M0L7_2anextS666 = _M0L7_2aSomeS665;
    _M0L7_2anextS666->$0 = _M0L8new__idxS662;
    moonbit_decref(_M0L7_2anextS666);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS618,
  int32_t _M0L3idxS620,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS619
) {
  int32_t _M0L7_2abindS617;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4145;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2429;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2430;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4144;
  int32_t _M0L4sizeS2432;
  int32_t _M0L6_2atmpS2431;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS617 = _M0L4selfS618->$6;
  switch (_M0L7_2abindS617) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2424;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4146;
      moonbit_incref(_M0L5entryS619);
      _M0L6_2atmpS2424 = _M0L5entryS619;
      _M0L6_2aoldS4146 = _M0L4selfS618->$5;
      if (_M0L6_2aoldS4146) {
        moonbit_decref(_M0L6_2aoldS4146);
      }
      _M0L4selfS618->$5 = _M0L6_2atmpS2424;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4149 =
        _M0L4selfS618->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2428 =
        _M0L8_2afieldS4149;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4148;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2427;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2425;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2426;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4147;
      if (
        _M0L7_2abindS617 < 0
        || _M0L7_2abindS617 >= Moonbit_array_length(_M0L7entriesS2428)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4148
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2428[
          _M0L7_2abindS617
        ];
      _M0L6_2atmpS2427 = _M0L6_2atmpS4148;
      if (_M0L6_2atmpS2427) {
        moonbit_incref(_M0L6_2atmpS2427);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2425
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2427);
      moonbit_incref(_M0L5entryS619);
      _M0L6_2atmpS2426 = _M0L5entryS619;
      _M0L6_2aoldS4147 = _M0L6_2atmpS2425->$1;
      if (_M0L6_2aoldS4147) {
        moonbit_decref(_M0L6_2aoldS4147);
      }
      _M0L6_2atmpS2425->$1 = _M0L6_2atmpS2426;
      moonbit_decref(_M0L6_2atmpS2425);
      break;
    }
  }
  _M0L4selfS618->$6 = _M0L3idxS620;
  _M0L8_2afieldS4145 = _M0L4selfS618->$0;
  _M0L7entriesS2429 = _M0L8_2afieldS4145;
  _M0L6_2atmpS2430 = _M0L5entryS619;
  if (
    _M0L3idxS620 < 0
    || _M0L3idxS620 >= Moonbit_array_length(_M0L7entriesS2429)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4144
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2429[
      _M0L3idxS620
    ];
  if (_M0L6_2aoldS4144) {
    moonbit_decref(_M0L6_2aoldS4144);
  }
  _M0L7entriesS2429[_M0L3idxS620] = _M0L6_2atmpS2430;
  _M0L4sizeS2432 = _M0L4selfS618->$1;
  _M0L6_2atmpS2431 = _M0L4sizeS2432 + 1;
  _M0L4selfS618->$1 = _M0L6_2atmpS2431;
  moonbit_decref(_M0L4selfS618);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS622,
  int32_t _M0L3idxS624,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS623
) {
  int32_t _M0L7_2abindS621;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4151;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2438;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2439;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4150;
  int32_t _M0L4sizeS2441;
  int32_t _M0L6_2atmpS2440;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS621 = _M0L4selfS622->$6;
  switch (_M0L7_2abindS621) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2433;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4152;
      moonbit_incref(_M0L5entryS623);
      _M0L6_2atmpS2433 = _M0L5entryS623;
      _M0L6_2aoldS4152 = _M0L4selfS622->$5;
      if (_M0L6_2aoldS4152) {
        moonbit_decref(_M0L6_2aoldS4152);
      }
      _M0L4selfS622->$5 = _M0L6_2atmpS2433;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4155 =
        _M0L4selfS622->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2437 =
        _M0L8_2afieldS4155;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4154;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2436;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2434;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2435;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4153;
      if (
        _M0L7_2abindS621 < 0
        || _M0L7_2abindS621 >= Moonbit_array_length(_M0L7entriesS2437)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4154
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2437[
          _M0L7_2abindS621
        ];
      _M0L6_2atmpS2436 = _M0L6_2atmpS4154;
      if (_M0L6_2atmpS2436) {
        moonbit_incref(_M0L6_2atmpS2436);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2434
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2436);
      moonbit_incref(_M0L5entryS623);
      _M0L6_2atmpS2435 = _M0L5entryS623;
      _M0L6_2aoldS4153 = _M0L6_2atmpS2434->$1;
      if (_M0L6_2aoldS4153) {
        moonbit_decref(_M0L6_2aoldS4153);
      }
      _M0L6_2atmpS2434->$1 = _M0L6_2atmpS2435;
      moonbit_decref(_M0L6_2atmpS2434);
      break;
    }
  }
  _M0L4selfS622->$6 = _M0L3idxS624;
  _M0L8_2afieldS4151 = _M0L4selfS622->$0;
  _M0L7entriesS2438 = _M0L8_2afieldS4151;
  _M0L6_2atmpS2439 = _M0L5entryS623;
  if (
    _M0L3idxS624 < 0
    || _M0L3idxS624 >= Moonbit_array_length(_M0L7entriesS2438)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4150
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2438[
      _M0L3idxS624
    ];
  if (_M0L6_2aoldS4150) {
    moonbit_decref(_M0L6_2aoldS4150);
  }
  _M0L7entriesS2438[_M0L3idxS624] = _M0L6_2atmpS2439;
  _M0L4sizeS2441 = _M0L4selfS622->$1;
  _M0L6_2atmpS2440 = _M0L4sizeS2441 + 1;
  _M0L4selfS622->$1 = _M0L6_2atmpS2440;
  moonbit_decref(_M0L4selfS622);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP48clawteam8clawteam8internal6schema6SchemaE(
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS626,
  int32_t _M0L3idxS628,
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L5entryS627
) {
  int32_t _M0L7_2abindS625;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4157;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7entriesS2447;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2448;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aoldS4156;
  int32_t _M0L4sizeS2450;
  int32_t _M0L6_2atmpS2449;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS625 = _M0L4selfS626->$6;
  switch (_M0L7_2abindS625) {
    case -1: {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2442;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aoldS4158;
      moonbit_incref(_M0L5entryS627);
      _M0L6_2atmpS2442 = _M0L5entryS627;
      _M0L6_2aoldS4158 = _M0L4selfS626->$5;
      if (_M0L6_2aoldS4158) {
        moonbit_decref(_M0L6_2aoldS4158);
      }
      _M0L4selfS626->$5 = _M0L6_2atmpS2442;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L8_2afieldS4161 =
        _M0L4selfS626->$0;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7entriesS2446 =
        _M0L8_2afieldS4161;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS4160;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2445;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2443;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2444;
      struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2aoldS4159;
      if (
        _M0L7_2abindS625 < 0
        || _M0L7_2abindS625 >= Moonbit_array_length(_M0L7entriesS2446)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4160
      = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L7entriesS2446[
          _M0L7_2abindS625
        ];
      _M0L6_2atmpS2445 = _M0L6_2atmpS4160;
      if (_M0L6_2atmpS2445) {
        moonbit_incref(_M0L6_2atmpS2445);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2443
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE(_M0L6_2atmpS2445);
      moonbit_incref(_M0L5entryS627);
      _M0L6_2atmpS2444 = _M0L5entryS627;
      _M0L6_2aoldS4159 = _M0L6_2atmpS2443->$1;
      if (_M0L6_2aoldS4159) {
        moonbit_decref(_M0L6_2aoldS4159);
      }
      _M0L6_2atmpS2443->$1 = _M0L6_2atmpS2444;
      moonbit_decref(_M0L6_2atmpS2443);
      break;
    }
  }
  _M0L4selfS626->$6 = _M0L3idxS628;
  _M0L8_2afieldS4157 = _M0L4selfS626->$0;
  _M0L7entriesS2447 = _M0L8_2afieldS4157;
  _M0L6_2atmpS2448 = _M0L5entryS627;
  if (
    _M0L3idxS628 < 0
    || _M0L3idxS628 >= Moonbit_array_length(_M0L7entriesS2447)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4156
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE*)_M0L7entriesS2447[
      _M0L3idxS628
    ];
  if (_M0L6_2aoldS4156) {
    moonbit_decref(_M0L6_2aoldS4156);
  }
  _M0L7entriesS2447[_M0L3idxS628] = _M0L6_2atmpS2448;
  _M0L4sizeS2450 = _M0L4selfS626->$1;
  _M0L6_2atmpS2449 = _M0L4sizeS2450 + 1;
  _M0L4selfS626->$1 = _M0L6_2atmpS2449;
  moonbit_decref(_M0L4selfS626);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS630,
  int32_t _M0L3idxS632,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS631
) {
  int32_t _M0L7_2abindS629;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4163;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2456;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2457;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4162;
  int32_t _M0L4sizeS2459;
  int32_t _M0L6_2atmpS2458;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS629 = _M0L4selfS630->$6;
  switch (_M0L7_2abindS629) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2451;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4164;
      moonbit_incref(_M0L5entryS631);
      _M0L6_2atmpS2451 = _M0L5entryS631;
      _M0L6_2aoldS4164 = _M0L4selfS630->$5;
      if (_M0L6_2aoldS4164) {
        moonbit_decref(_M0L6_2aoldS4164);
      }
      _M0L4selfS630->$5 = _M0L6_2atmpS2451;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4167 = _M0L4selfS630->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2455 = _M0L8_2afieldS4167;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4166;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2454;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2452;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2453;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4165;
      if (
        _M0L7_2abindS629 < 0
        || _M0L7_2abindS629 >= Moonbit_array_length(_M0L7entriesS2455)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4166
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2455[_M0L7_2abindS629];
      _M0L6_2atmpS2454 = _M0L6_2atmpS4166;
      if (_M0L6_2atmpS2454) {
        moonbit_incref(_M0L6_2atmpS2454);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2452
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2454);
      moonbit_incref(_M0L5entryS631);
      _M0L6_2atmpS2453 = _M0L5entryS631;
      _M0L6_2aoldS4165 = _M0L6_2atmpS2452->$1;
      if (_M0L6_2aoldS4165) {
        moonbit_decref(_M0L6_2aoldS4165);
      }
      _M0L6_2atmpS2452->$1 = _M0L6_2atmpS2453;
      moonbit_decref(_M0L6_2atmpS2452);
      break;
    }
  }
  _M0L4selfS630->$6 = _M0L3idxS632;
  _M0L8_2afieldS4163 = _M0L4selfS630->$0;
  _M0L7entriesS2456 = _M0L8_2afieldS4163;
  _M0L6_2atmpS2457 = _M0L5entryS631;
  if (
    _M0L3idxS632 < 0
    || _M0L3idxS632 >= Moonbit_array_length(_M0L7entriesS2456)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4162
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2456[_M0L3idxS632];
  if (_M0L6_2aoldS4162) {
    moonbit_decref(_M0L6_2aoldS4162);
  }
  _M0L7entriesS2456[_M0L3idxS632] = _M0L6_2atmpS2457;
  _M0L4sizeS2459 = _M0L4selfS630->$1;
  _M0L6_2atmpS2458 = _M0L4sizeS2459 + 1;
  _M0L4selfS630->$1 = _M0L6_2atmpS2458;
  moonbit_decref(_M0L4selfS630);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGsRPB4JsonEE(
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0L4selfS634,
  int32_t _M0L3idxS636,
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L5entryS635
) {
  int32_t _M0L7_2abindS633;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L8_2afieldS4169;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L7entriesS2465;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2466;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2aoldS4168;
  int32_t _M0L4sizeS2468;
  int32_t _M0L6_2atmpS2467;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS633 = _M0L4selfS634->$6;
  switch (_M0L7_2abindS633) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2460;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2aoldS4170;
      moonbit_incref(_M0L5entryS635);
      _M0L6_2atmpS2460 = _M0L5entryS635;
      _M0L6_2aoldS4170 = _M0L4selfS634->$5;
      if (_M0L6_2aoldS4170) {
        moonbit_decref(_M0L6_2aoldS4170);
      }
      _M0L4selfS634->$5 = _M0L6_2atmpS2460;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L8_2afieldS4173 =
        _M0L4selfS634->$0;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L7entriesS2464 =
        _M0L8_2afieldS4173;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS4172;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2463;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2461;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2462;
      struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2aoldS4171;
      if (
        _M0L7_2abindS633 < 0
        || _M0L7_2abindS633 >= Moonbit_array_length(_M0L7entriesS2464)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4172
      = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)_M0L7entriesS2464[
          _M0L7_2abindS633
        ];
      _M0L6_2atmpS2463 = _M0L6_2atmpS4172;
      if (_M0L6_2atmpS2463) {
        moonbit_incref(_M0L6_2atmpS2463);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2461
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGsRPB4JsonEEE(_M0L6_2atmpS2463);
      moonbit_incref(_M0L5entryS635);
      _M0L6_2atmpS2462 = _M0L5entryS635;
      _M0L6_2aoldS4171 = _M0L6_2atmpS2461->$1;
      if (_M0L6_2aoldS4171) {
        moonbit_decref(_M0L6_2aoldS4171);
      }
      _M0L6_2atmpS2461->$1 = _M0L6_2atmpS2462;
      moonbit_decref(_M0L6_2atmpS2461);
      break;
    }
  }
  _M0L4selfS634->$6 = _M0L3idxS636;
  _M0L8_2afieldS4169 = _M0L4selfS634->$0;
  _M0L7entriesS2465 = _M0L8_2afieldS4169;
  _M0L6_2atmpS2466 = _M0L5entryS635;
  if (
    _M0L3idxS636 < 0
    || _M0L3idxS636 >= Moonbit_array_length(_M0L7entriesS2465)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4168
  = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE*)_M0L7entriesS2465[
      _M0L3idxS636
    ];
  if (_M0L6_2aoldS4168) {
    moonbit_decref(_M0L6_2aoldS4168);
  }
  _M0L7entriesS2465[_M0L3idxS636] = _M0L6_2atmpS2466;
  _M0L4sizeS2468 = _M0L4selfS634->$1;
  _M0L6_2atmpS2467 = _M0L4sizeS2468 + 1;
  _M0L4selfS634->$1 = _M0L6_2atmpS2467;
  moonbit_decref(_M0L4selfS634);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS588
) {
  int32_t _M0L8capacityS587;
  int32_t _M0L7_2abindS589;
  int32_t _M0L7_2abindS590;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2419;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS591;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS592;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4643;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS587
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS588);
  _M0L7_2abindS589 = _M0L8capacityS587 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS590 = _M0FPB21calc__grow__threshold(_M0L8capacityS587);
  _M0L6_2atmpS2419 = 0;
  _M0L7_2abindS591
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS587, _M0L6_2atmpS2419);
  _M0L7_2abindS592 = 0;
  _block_4643
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4643)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4643->$0 = _M0L7_2abindS591;
  _block_4643->$1 = 0;
  _block_4643->$2 = _M0L8capacityS587;
  _block_4643->$3 = _M0L7_2abindS589;
  _block_4643->$4 = _M0L7_2abindS590;
  _block_4643->$5 = _M0L7_2abindS592;
  _block_4643->$6 = -1;
  return _block_4643;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS594
) {
  int32_t _M0L8capacityS593;
  int32_t _M0L7_2abindS595;
  int32_t _M0L7_2abindS596;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2420;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS597;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS598;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4644;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS593
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS594);
  _M0L7_2abindS595 = _M0L8capacityS593 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS596 = _M0FPB21calc__grow__threshold(_M0L8capacityS593);
  _M0L6_2atmpS2420 = 0;
  _M0L7_2abindS597
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS593, _M0L6_2atmpS2420);
  _M0L7_2abindS598 = 0;
  _block_4644
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4644)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4644->$0 = _M0L7_2abindS597;
  _block_4644->$1 = 0;
  _block_4644->$2 = _M0L8capacityS593;
  _block_4644->$3 = _M0L7_2abindS595;
  _block_4644->$4 = _M0L7_2abindS596;
  _block_4644->$5 = _M0L7_2abindS598;
  _block_4644->$6 = -1;
  return _block_4644;
}

struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB3Map11new_2einnerGsRP48clawteam8clawteam8internal6schema6SchemaE(
  int32_t _M0L8capacityS600
) {
  int32_t _M0L8capacityS599;
  int32_t _M0L7_2abindS601;
  int32_t _M0L7_2abindS602;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L6_2atmpS2421;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE** _M0L7_2abindS603;
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2abindS604;
  struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE* _block_4645;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS599
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS600);
  _M0L7_2abindS601 = _M0L8capacityS599 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS602 = _M0FPB21calc__grow__threshold(_M0L8capacityS599);
  _M0L6_2atmpS2421 = 0;
  _M0L7_2abindS603
  = (struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE**)moonbit_make_ref_array(_M0L8capacityS599, _M0L6_2atmpS2421);
  _M0L7_2abindS604 = 0;
  _block_4645
  = (struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE));
  Moonbit_object_header(_block_4645)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRP48clawteam8clawteam8internal6schema6SchemaE, $0) >> 2, 2, 0);
  _block_4645->$0 = _M0L7_2abindS603;
  _block_4645->$1 = 0;
  _block_4645->$2 = _M0L8capacityS599;
  _block_4645->$3 = _M0L7_2abindS601;
  _block_4645->$4 = _M0L7_2abindS602;
  _block_4645->$5 = _M0L7_2abindS604;
  _block_4645->$6 = -1;
  return _block_4645;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS606
) {
  int32_t _M0L8capacityS605;
  int32_t _M0L7_2abindS607;
  int32_t _M0L7_2abindS608;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2422;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS609;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS610;
  struct _M0TPB3MapGsRPB4JsonE* _block_4646;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS605
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS606);
  _M0L7_2abindS607 = _M0L8capacityS605 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS608 = _M0FPB21calc__grow__threshold(_M0L8capacityS605);
  _M0L6_2atmpS2422 = 0;
  _M0L7_2abindS609
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS605, _M0L6_2atmpS2422);
  _M0L7_2abindS610 = 0;
  _block_4646
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_4646)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_4646->$0 = _M0L7_2abindS609;
  _block_4646->$1 = 0;
  _block_4646->$2 = _M0L8capacityS605;
  _block_4646->$3 = _M0L7_2abindS607;
  _block_4646->$4 = _M0L7_2abindS608;
  _block_4646->$5 = _M0L7_2abindS610;
  _block_4646->$6 = -1;
  return _block_4646;
}

struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _M0MPB3Map11new_2einnerGsRPB3MapGsRPB4JsonEE(
  int32_t _M0L8capacityS612
) {
  int32_t _M0L8capacityS611;
  int32_t _M0L7_2abindS613;
  int32_t _M0L7_2abindS614;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L6_2atmpS2423;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE** _M0L7_2abindS615;
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2abindS616;
  struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE* _block_4647;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS611
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS612);
  _M0L7_2abindS613 = _M0L8capacityS611 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS614 = _M0FPB21calc__grow__threshold(_M0L8capacityS611);
  _M0L6_2atmpS2423 = 0;
  _M0L7_2abindS615
  = (struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE**)moonbit_make_ref_array(_M0L8capacityS611, _M0L6_2atmpS2423);
  _M0L7_2abindS616 = 0;
  _block_4647
  = (struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE));
  Moonbit_object_header(_block_4647)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGsRPB4JsonEE, $0) >> 2, 2, 0);
  _block_4647->$0 = _M0L7_2abindS615;
  _block_4647->$1 = 0;
  _block_4647->$2 = _M0L8capacityS611;
  _block_4647->$3 = _M0L7_2abindS613;
  _block_4647->$4 = _M0L7_2abindS614;
  _block_4647->$5 = _M0L7_2abindS616;
  _block_4647->$6 = -1;
  return _block_4647;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS586) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS586 >= 0) {
    int32_t _M0L6_2atmpS2418;
    int32_t _M0L6_2atmpS2417;
    int32_t _M0L6_2atmpS2416;
    int32_t _M0L6_2atmpS2415;
    if (_M0L4selfS586 <= 1) {
      return 1;
    }
    if (_M0L4selfS586 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2418 = _M0L4selfS586 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2417 = moonbit_clz32(_M0L6_2atmpS2418);
    _M0L6_2atmpS2416 = _M0L6_2atmpS2417 - 1;
    _M0L6_2atmpS2415 = 2147483647 >> (_M0L6_2atmpS2416 & 31);
    return _M0L6_2atmpS2415 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS585) {
  int32_t _M0L6_2atmpS2414;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2414 = _M0L8capacityS585 * 13;
  return _M0L6_2atmpS2414 / 16;
}

struct _M0TPB5ArrayGsE* _M0MPC16option6Option10unwrap__orGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE* _M0L4selfS582,
  struct _M0TPB5ArrayGsE* _M0L7defaultS583
) {
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS582 == 0) {
    if (_M0L4selfS582) {
      moonbit_decref(_M0L4selfS582);
    }
    return _M0L7defaultS583;
  } else {
    struct _M0TPB5ArrayGsE* _M0L7_2aSomeS584;
    moonbit_decref(_M0L7defaultS583);
    _M0L7_2aSomeS584 = _M0L4selfS582;
    return _M0L7_2aSomeS584;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS570
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS570 == 0) {
    if (_M0L4selfS570) {
      moonbit_decref(_M0L4selfS570);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS571 =
      _M0L4selfS570;
    return _M0L7_2aSomeS571;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS572
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS572 == 0) {
    if (_M0L4selfS572) {
      moonbit_decref(_M0L4selfS572);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS573 =
      _M0L4selfS572;
    return _M0L7_2aSomeS573;
  }
}

struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS574
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS574 == 0) {
    if (_M0L4selfS574) {
      moonbit_decref(_M0L4selfS574);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2aSomeS575 =
      _M0L4selfS574;
    return _M0L7_2aSomeS575;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS576
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS576 == 0) {
    if (_M0L4selfS576) {
      moonbit_decref(_M0L4selfS576);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS577 = _M0L4selfS576;
    return _M0L7_2aSomeS577;
  }
}

struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGsRPB4JsonEEE(
  struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L4selfS578
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS578 == 0) {
    if (_M0L4selfS578) {
      moonbit_decref(_M0L4selfS578);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGsRPB4JsonEE* _M0L7_2aSomeS579 =
      _M0L4selfS578;
    return _M0L7_2aSomeS579;
  }
}

struct _M0TPC111sorted__set4NodeGsE* _M0MPC16option6Option6unwrapGRPC111sorted__set4NodeGsEE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4selfS580
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS580 == 0) {
    if (_M0L4selfS580) {
      moonbit_decref(_M0L4selfS580);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS581 = _M0L4selfS580;
    return _M0L7_2aSomeS581;
  }
}

int32_t _M0IPC16option6OptionPB2Eq5equalGRPC111sorted__set4NodeGsEE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L4selfS564,
  struct _M0TPC111sorted__set4NodeGsE* _M0L5otherS565
) {
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS564 == 0) {
    int32_t _M0L6_2atmpS4174;
    if (_M0L4selfS564) {
      moonbit_decref(_M0L4selfS564);
    }
    _M0L6_2atmpS4174 = _M0L5otherS565 == 0;
    if (_M0L5otherS565) {
      moonbit_decref(_M0L5otherS565);
    }
    return _M0L6_2atmpS4174;
  } else {
    struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS566 = _M0L4selfS564;
    struct _M0TPC111sorted__set4NodeGsE* _M0L4_2axS567 = _M0L7_2aSomeS566;
    if (_M0L5otherS565 == 0) {
      moonbit_decref(_M0L4_2axS567);
      if (_M0L5otherS565) {
        moonbit_decref(_M0L5otherS565);
      }
      return 0;
    } else {
      struct _M0TPC111sorted__set4NodeGsE* _M0L7_2aSomeS568 = _M0L5otherS565;
      struct _M0TPC111sorted__set4NodeGsE* _M0L4_2ayS569 = _M0L7_2aSomeS568;
      #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
      return _M0IPC111sorted__set4NodePB2Eq5equalGsE(_M0L4_2axS567, _M0L4_2ayS569);
    }
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS563
) {
  void** _M0L6_2atmpS2413;
  struct _M0TPB5ArrayGRPB4JsonE* _block_4648;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2413
  = (void**)moonbit_make_ref_array(_M0L3lenS563, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_4648
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_4648)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_4648->$0 = _M0L6_2atmpS2413;
  _block_4648->$1 = _M0L3lenS563;
  return _block_4648;
}

moonbit_string_t _M0MPC15array9ArrayView2atGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS562,
  int32_t _M0L5indexS561
) {
  int32_t _if__result_4649;
  #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  if (_M0L5indexS561 >= 0) {
    int32_t _M0L3endS2400 = _M0L4selfS562.$2;
    int32_t _M0L5startS2401 = _M0L4selfS562.$1;
    int32_t _M0L6_2atmpS2399 = _M0L3endS2400 - _M0L5startS2401;
    _if__result_4649 = _M0L5indexS561 < _M0L6_2atmpS2399;
  } else {
    _if__result_4649 = 0;
  }
  if (_if__result_4649) {
    moonbit_string_t* _M0L8_2afieldS4177 = _M0L4selfS562.$0;
    moonbit_string_t* _M0L3bufS2402 = _M0L8_2afieldS4177;
    int32_t _M0L8_2afieldS4176 = _M0L4selfS562.$1;
    int32_t _M0L5startS2404 = _M0L8_2afieldS4176;
    int32_t _M0L6_2atmpS2403 = _M0L5startS2404 + _M0L5indexS561;
    moonbit_string_t _M0L6_2atmpS4175;
    if (
      _M0L6_2atmpS2403 < 0
      || _M0L6_2atmpS2403 >= Moonbit_array_length(_M0L3bufS2402)
    ) {
      #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4175 = (moonbit_string_t)_M0L3bufS2402[_M0L6_2atmpS2403];
    moonbit_incref(_M0L6_2atmpS4175);
    moonbit_decref(_M0L3bufS2402);
    return _M0L6_2atmpS4175;
  } else {
    int32_t _M0L3endS2411 = _M0L4selfS562.$2;
    int32_t _M0L8_2afieldS4181 = _M0L4selfS562.$1;
    int32_t _M0L5startS2412;
    int32_t _M0L6_2atmpS2410;
    moonbit_string_t _M0L6_2atmpS2409;
    moonbit_string_t _M0L6_2atmpS4180;
    moonbit_string_t _M0L6_2atmpS2408;
    moonbit_string_t _M0L6_2atmpS4179;
    moonbit_string_t _M0L6_2atmpS2406;
    moonbit_string_t _M0L6_2atmpS2407;
    moonbit_string_t _M0L6_2atmpS4178;
    moonbit_string_t _M0L6_2atmpS2405;
    moonbit_decref(_M0L4selfS562.$0);
    _M0L5startS2412 = _M0L8_2afieldS4181;
    _M0L6_2atmpS2410 = _M0L3endS2411 - _M0L5startS2412;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2409
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2410);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4180
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_76.data, _M0L6_2atmpS2409);
    moonbit_decref(_M0L6_2atmpS2409);
    _M0L6_2atmpS2408 = _M0L6_2atmpS4180;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4179
    = moonbit_add_string(_M0L6_2atmpS2408, (moonbit_string_t)moonbit_string_literal_77.data);
    moonbit_decref(_M0L6_2atmpS2408);
    _M0L6_2atmpS2406 = _M0L6_2atmpS4179;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2407
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS561);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4178 = moonbit_add_string(_M0L6_2atmpS2406, _M0L6_2atmpS2407);
    moonbit_decref(_M0L6_2atmpS2406);
    moonbit_decref(_M0L6_2atmpS2407);
    _M0L6_2atmpS2405 = _M0L6_2atmpS4178;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGsE(_M0L6_2atmpS2405, (moonbit_string_t)moonbit_string_literal_78.data);
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS560
) {
  moonbit_string_t* _M0L6_2atmpS2398;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2398 = _M0L4selfS560;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2398);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS556,
  int32_t _M0L5indexS557
) {
  uint64_t* _M0L6_2atmpS2396;
  uint64_t _M0L6_2atmpS4182;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2396 = _M0L4selfS556;
  if (
    _M0L5indexS557 < 0
    || _M0L5indexS557 >= Moonbit_array_length(_M0L6_2atmpS2396)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4182 = (uint64_t)_M0L6_2atmpS2396[_M0L5indexS557];
  moonbit_decref(_M0L6_2atmpS2396);
  return _M0L6_2atmpS4182;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS558,
  int32_t _M0L5indexS559
) {
  uint32_t* _M0L6_2atmpS2397;
  uint32_t _M0L6_2atmpS4183;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2397 = _M0L4selfS558;
  if (
    _M0L5indexS559 < 0
    || _M0L5indexS559 >= Moonbit_array_length(_M0L6_2atmpS2397)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4183 = (uint32_t)_M0L6_2atmpS2397[_M0L5indexS559];
  moonbit_decref(_M0L6_2atmpS2397);
  return _M0L6_2atmpS4183;
}

struct _M0TWEORPB4Json* _M0MPC15array5Array4iterGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS555
) {
  void** _M0L8_2afieldS4185;
  void** _M0L3bufS2394;
  int32_t _M0L8_2afieldS4184;
  int32_t _M0L6_2acntS4465;
  int32_t _M0L3lenS2395;
  struct _M0TPB9ArrayViewGRPB4JsonE _M0L6_2atmpS2393;
  #line 1651 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS4185 = _M0L4selfS555->$0;
  _M0L3bufS2394 = _M0L8_2afieldS4185;
  _M0L8_2afieldS4184 = _M0L4selfS555->$1;
  _M0L6_2acntS4465 = Moonbit_object_header(_M0L4selfS555)->rc;
  if (_M0L6_2acntS4465 > 1) {
    int32_t _M0L11_2anew__cntS4466 = _M0L6_2acntS4465 - 1;
    Moonbit_object_header(_M0L4selfS555)->rc = _M0L11_2anew__cntS4466;
    moonbit_incref(_M0L3bufS2394);
  } else if (_M0L6_2acntS4465 == 1) {
    #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_free(_M0L4selfS555);
  }
  _M0L3lenS2395 = _M0L8_2afieldS4184;
  _M0L6_2atmpS2393
  = (struct _M0TPB9ArrayViewGRPB4JsonE){
    0, _M0L3lenS2395, _M0L3bufS2394
  };
  #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  return _M0MPC15array9ArrayView4iterGRPB4JsonE(_M0L6_2atmpS2393);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS554
) {
  moonbit_string_t* _M0L6_2atmpS2391;
  int32_t _M0L6_2atmpS4186;
  int32_t _M0L6_2atmpS2392;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2390;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS554);
  _M0L6_2atmpS2391 = _M0L4selfS554;
  _M0L6_2atmpS4186 = Moonbit_array_length(_M0L4selfS554);
  moonbit_decref(_M0L4selfS554);
  _M0L6_2atmpS2392 = _M0L6_2atmpS4186;
  _M0L6_2atmpS2390
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2392, _M0L6_2atmpS2391
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2390);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS549
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS548;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__* _closure_4650;
  struct _M0TWEOs* _M0L6_2atmpS2366;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS548
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS548)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS548->$0 = 0;
  _closure_4650
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__));
  Moonbit_object_header(_closure_4650)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__, $0_0) >> 2, 2, 0);
  _closure_4650->code = &_M0MPC15array9ArrayView4iterGsEC2367l570;
  _closure_4650->$0_0 = _M0L4selfS549.$0;
  _closure_4650->$0_1 = _M0L4selfS549.$1;
  _closure_4650->$0_2 = _M0L4selfS549.$2;
  _closure_4650->$1 = _M0L1iS548;
  _M0L6_2atmpS2366 = (struct _M0TWEOs*)_closure_4650;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2366);
}

struct _M0TWEORPB4Json* _M0MPC15array9ArrayView4iterGRPB4JsonE(
  struct _M0TPB9ArrayViewGRPB4JsonE _M0L4selfS552
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS551;
  struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__* _closure_4651;
  struct _M0TWEORPB4Json* _M0L6_2atmpS2378;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS551
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS551)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS551->$0 = 0;
  _closure_4651
  = (struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__*)moonbit_malloc(sizeof(struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__));
  Moonbit_object_header(_closure_4651)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__, $0_0) >> 2, 2, 0);
  _closure_4651->code = &_M0MPC15array9ArrayView4iterGRPB4JsonEC2379l570;
  _closure_4651->$0_0 = _M0L4selfS552.$0;
  _closure_4651->$0_1 = _M0L4selfS552.$1;
  _closure_4651->$0_2 = _M0L4selfS552.$2;
  _closure_4651->$1 = _M0L1iS551;
  _M0L6_2atmpS2378 = (struct _M0TWEORPB4Json*)_closure_4651;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGRPB4JsonE(_M0L6_2atmpS2378);
}

void* _M0MPC15array9ArrayView4iterGRPB4JsonEC2379l570(
  struct _M0TWEORPB4Json* _M0L6_2aenvS2380
) {
  struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__* _M0L14_2acasted__envS2381;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4191;
  struct _M0TPC13ref3RefGiE* _M0L1iS551;
  struct _M0TPB9ArrayViewGRPB4JsonE _M0L8_2afieldS4190;
  int32_t _M0L6_2acntS4467;
  struct _M0TPB9ArrayViewGRPB4JsonE _M0L4selfS552;
  int32_t _M0L3valS2382;
  int32_t _M0L6_2atmpS2383;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2381
  = (struct _M0R88ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2379__l570__*)_M0L6_2aenvS2380;
  _M0L8_2afieldS4191 = _M0L14_2acasted__envS2381->$1;
  _M0L1iS551 = _M0L8_2afieldS4191;
  _M0L8_2afieldS4190
  = (struct _M0TPB9ArrayViewGRPB4JsonE){
    _M0L14_2acasted__envS2381->$0_1,
      _M0L14_2acasted__envS2381->$0_2,
      _M0L14_2acasted__envS2381->$0_0
  };
  _M0L6_2acntS4467 = Moonbit_object_header(_M0L14_2acasted__envS2381)->rc;
  if (_M0L6_2acntS4467 > 1) {
    int32_t _M0L11_2anew__cntS4468 = _M0L6_2acntS4467 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2381)->rc
    = _M0L11_2anew__cntS4468;
    moonbit_incref(_M0L1iS551);
    moonbit_incref(_M0L8_2afieldS4190.$0);
  } else if (_M0L6_2acntS4467 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2381);
  }
  _M0L4selfS552 = _M0L8_2afieldS4190;
  _M0L3valS2382 = _M0L1iS551->$0;
  moonbit_incref(_M0L4selfS552.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2383 = _M0MPC15array9ArrayView6lengthGRPB4JsonE(_M0L4selfS552);
  if (_M0L3valS2382 < _M0L6_2atmpS2383) {
    void** _M0L8_2afieldS4189 = _M0L4selfS552.$0;
    void** _M0L3bufS2386 = _M0L8_2afieldS4189;
    int32_t _M0L8_2afieldS4188 = _M0L4selfS552.$1;
    int32_t _M0L5startS2388 = _M0L8_2afieldS4188;
    int32_t _M0L3valS2389 = _M0L1iS551->$0;
    int32_t _M0L6_2atmpS2387 = _M0L5startS2388 + _M0L3valS2389;
    void* _M0L6_2atmpS4187 = (void*)_M0L3bufS2386[_M0L6_2atmpS2387];
    void* _M0L4elemS553;
    int32_t _M0L3valS2385;
    int32_t _M0L6_2atmpS2384;
    moonbit_incref(_M0L6_2atmpS4187);
    moonbit_decref(_M0L3bufS2386);
    _M0L4elemS553 = _M0L6_2atmpS4187;
    _M0L3valS2385 = _M0L1iS551->$0;
    _M0L6_2atmpS2384 = _M0L3valS2385 + 1;
    _M0L1iS551->$0 = _M0L6_2atmpS2384;
    moonbit_decref(_M0L1iS551);
    return _M0L4elemS553;
  } else {
    moonbit_decref(_M0L4selfS552.$0);
    moonbit_decref(_M0L1iS551);
    return 0;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2367l570(
  struct _M0TWEOs* _M0L6_2aenvS2368
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__* _M0L14_2acasted__envS2369;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4196;
  struct _M0TPC13ref3RefGiE* _M0L1iS548;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4195;
  int32_t _M0L6_2acntS4469;
  struct _M0TPB9ArrayViewGsE _M0L4selfS549;
  int32_t _M0L3valS2370;
  int32_t _M0L6_2atmpS2371;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2369
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2367__l570__*)_M0L6_2aenvS2368;
  _M0L8_2afieldS4196 = _M0L14_2acasted__envS2369->$1;
  _M0L1iS548 = _M0L8_2afieldS4196;
  _M0L8_2afieldS4195
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2369->$0_1,
      _M0L14_2acasted__envS2369->$0_2,
      _M0L14_2acasted__envS2369->$0_0
  };
  _M0L6_2acntS4469 = Moonbit_object_header(_M0L14_2acasted__envS2369)->rc;
  if (_M0L6_2acntS4469 > 1) {
    int32_t _M0L11_2anew__cntS4470 = _M0L6_2acntS4469 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2369)->rc
    = _M0L11_2anew__cntS4470;
    moonbit_incref(_M0L1iS548);
    moonbit_incref(_M0L8_2afieldS4195.$0);
  } else if (_M0L6_2acntS4469 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2369);
  }
  _M0L4selfS549 = _M0L8_2afieldS4195;
  _M0L3valS2370 = _M0L1iS548->$0;
  moonbit_incref(_M0L4selfS549.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2371 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS549);
  if (_M0L3valS2370 < _M0L6_2atmpS2371) {
    moonbit_string_t* _M0L8_2afieldS4194 = _M0L4selfS549.$0;
    moonbit_string_t* _M0L3bufS2374 = _M0L8_2afieldS4194;
    int32_t _M0L8_2afieldS4193 = _M0L4selfS549.$1;
    int32_t _M0L5startS2376 = _M0L8_2afieldS4193;
    int32_t _M0L3valS2377 = _M0L1iS548->$0;
    int32_t _M0L6_2atmpS2375 = _M0L5startS2376 + _M0L3valS2377;
    moonbit_string_t _M0L6_2atmpS4192 =
      (moonbit_string_t)_M0L3bufS2374[_M0L6_2atmpS2375];
    moonbit_string_t _M0L4elemS550;
    int32_t _M0L3valS2373;
    int32_t _M0L6_2atmpS2372;
    moonbit_incref(_M0L6_2atmpS4192);
    moonbit_decref(_M0L3bufS2374);
    _M0L4elemS550 = _M0L6_2atmpS4192;
    _M0L3valS2373 = _M0L1iS548->$0;
    _M0L6_2atmpS2372 = _M0L3valS2373 + 1;
    _M0L1iS548->$0 = _M0L6_2atmpS2372;
    moonbit_decref(_M0L1iS548);
    return _M0L4elemS550;
  } else {
    moonbit_decref(_M0L4selfS549.$0);
    moonbit_decref(_M0L1iS548);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS547
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS547;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS546,
  struct _M0TPB6Logger _M0L6loggerS545
) {
  moonbit_string_t _M0L6_2atmpS2365;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2365
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS546, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS545.$0->$method_0(_M0L6loggerS545.$1, _M0L6_2atmpS2365);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS544,
  struct _M0TPB6Logger _M0L6loggerS543
) {
  moonbit_string_t _M0L6_2atmpS2364;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2364 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS544, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS543.$0->$method_0(_M0L6loggerS543.$1, _M0L6_2atmpS2364);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS541,
  struct _M0TPB6Logger _M0L6loggerS542
) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS541) {
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS542.$0->$method_0(_M0L6loggerS542.$1, (moonbit_string_t)moonbit_string_literal_44.data);
  } else {
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS542.$0->$method_0(_M0L6loggerS542.$1, (moonbit_string_t)moonbit_string_literal_45.data);
  }
  return 0;
}

int32_t _M0IPC16string6StringPB7Compare7compare(
  moonbit_string_t _M0L4selfS535,
  moonbit_string_t _M0L5otherS537
) {
  int32_t _M0L3lenS534;
  int32_t _M0L6_2atmpS2363;
  int32_t _M0L7_2abindS536;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS534 = Moonbit_array_length(_M0L4selfS535);
  _M0L6_2atmpS2363 = Moonbit_array_length(_M0L5otherS537);
  _M0L7_2abindS536
  = (_M0L3lenS534 >= _M0L6_2atmpS2363) - (_M0L3lenS534 <= _M0L6_2atmpS2363);
  switch (_M0L7_2abindS536) {
    case 0: {
      int32_t _M0L1iS538 = 0;
      while (1) {
        if (_M0L1iS538 < _M0L3lenS534) {
          int32_t _M0L6_2atmpS2360 = _M0L4selfS535[_M0L1iS538];
          int32_t _M0L6_2atmpS2361 = _M0L5otherS537[_M0L1iS538];
          int32_t _M0L5orderS539;
          int32_t _M0L6_2atmpS2362;
          #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0L5orderS539
          = _M0IPC16uint166UInt16PB7Compare7compare(_M0L6_2atmpS2360, _M0L6_2atmpS2361);
          if (_M0L5orderS539 != 0) {
            moonbit_decref(_M0L5otherS537);
            moonbit_decref(_M0L4selfS535);
            return _M0L5orderS539;
          }
          _M0L6_2atmpS2362 = _M0L1iS538 + 1;
          _M0L1iS538 = _M0L6_2atmpS2362;
          continue;
        } else {
          moonbit_decref(_M0L5otherS537);
          moonbit_decref(_M0L4selfS535);
        }
        break;
      }
      return 0;
      break;
    }
    default: {
      moonbit_decref(_M0L5otherS537);
      moonbit_decref(_M0L4selfS535);
      return _M0L7_2abindS536;
      break;
    }
  }
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS529) {
  int32_t _M0L3lenS528;
  struct _M0TPC13ref3RefGiE* _M0L5indexS530;
  struct _M0R38String_3a_3aiter_2eanon__u2344__l247__* _closure_4653;
  struct _M0TWEOc* _M0L6_2atmpS2343;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS528 = Moonbit_array_length(_M0L4selfS529);
  _M0L5indexS530
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS530)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS530->$0 = 0;
  _closure_4653
  = (struct _M0R38String_3a_3aiter_2eanon__u2344__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2344__l247__));
  Moonbit_object_header(_closure_4653)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2344__l247__, $0) >> 2, 2, 0);
  _closure_4653->code = &_M0MPC16string6String4iterC2344l247;
  _closure_4653->$0 = _M0L5indexS530;
  _closure_4653->$1 = _M0L4selfS529;
  _closure_4653->$2 = _M0L3lenS528;
  _M0L6_2atmpS2343 = (struct _M0TWEOc*)_closure_4653;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2343);
}

int32_t _M0MPC16string6String4iterC2344l247(
  struct _M0TWEOc* _M0L6_2aenvS2345
) {
  struct _M0R38String_3a_3aiter_2eanon__u2344__l247__* _M0L14_2acasted__envS2346;
  int32_t _M0L3lenS528;
  moonbit_string_t _M0L8_2afieldS4199;
  moonbit_string_t _M0L4selfS529;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4198;
  int32_t _M0L6_2acntS4471;
  struct _M0TPC13ref3RefGiE* _M0L5indexS530;
  int32_t _M0L3valS2347;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2346
  = (struct _M0R38String_3a_3aiter_2eanon__u2344__l247__*)_M0L6_2aenvS2345;
  _M0L3lenS528 = _M0L14_2acasted__envS2346->$2;
  _M0L8_2afieldS4199 = _M0L14_2acasted__envS2346->$1;
  _M0L4selfS529 = _M0L8_2afieldS4199;
  _M0L8_2afieldS4198 = _M0L14_2acasted__envS2346->$0;
  _M0L6_2acntS4471 = Moonbit_object_header(_M0L14_2acasted__envS2346)->rc;
  if (_M0L6_2acntS4471 > 1) {
    int32_t _M0L11_2anew__cntS4472 = _M0L6_2acntS4471 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2346)->rc
    = _M0L11_2anew__cntS4472;
    moonbit_incref(_M0L4selfS529);
    moonbit_incref(_M0L8_2afieldS4198);
  } else if (_M0L6_2acntS4471 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2346);
  }
  _M0L5indexS530 = _M0L8_2afieldS4198;
  _M0L3valS2347 = _M0L5indexS530->$0;
  if (_M0L3valS2347 < _M0L3lenS528) {
    int32_t _M0L3valS2359 = _M0L5indexS530->$0;
    int32_t _M0L2c1S531 = _M0L4selfS529[_M0L3valS2359];
    int32_t _if__result_4654;
    int32_t _M0L3valS2357;
    int32_t _M0L6_2atmpS2356;
    int32_t _M0L6_2atmpS2358;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S531)) {
      int32_t _M0L3valS2349 = _M0L5indexS530->$0;
      int32_t _M0L6_2atmpS2348 = _M0L3valS2349 + 1;
      _if__result_4654 = _M0L6_2atmpS2348 < _M0L3lenS528;
    } else {
      _if__result_4654 = 0;
    }
    if (_if__result_4654) {
      int32_t _M0L3valS2355 = _M0L5indexS530->$0;
      int32_t _M0L6_2atmpS2354 = _M0L3valS2355 + 1;
      int32_t _M0L6_2atmpS4197 = _M0L4selfS529[_M0L6_2atmpS2354];
      int32_t _M0L2c2S532;
      moonbit_decref(_M0L4selfS529);
      _M0L2c2S532 = _M0L6_2atmpS4197;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S532)) {
        int32_t _M0L6_2atmpS2352 = (int32_t)_M0L2c1S531;
        int32_t _M0L6_2atmpS2353 = (int32_t)_M0L2c2S532;
        int32_t _M0L1cS533;
        int32_t _M0L3valS2351;
        int32_t _M0L6_2atmpS2350;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS533
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2352, _M0L6_2atmpS2353);
        _M0L3valS2351 = _M0L5indexS530->$0;
        _M0L6_2atmpS2350 = _M0L3valS2351 + 2;
        _M0L5indexS530->$0 = _M0L6_2atmpS2350;
        moonbit_decref(_M0L5indexS530);
        return _M0L1cS533;
      }
    } else {
      moonbit_decref(_M0L4selfS529);
    }
    _M0L3valS2357 = _M0L5indexS530->$0;
    _M0L6_2atmpS2356 = _M0L3valS2357 + 1;
    _M0L5indexS530->$0 = _M0L6_2atmpS2356;
    moonbit_decref(_M0L5indexS530);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2358 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S531);
    return _M0L6_2atmpS2358;
  } else {
    moonbit_decref(_M0L5indexS530);
    moonbit_decref(_M0L4selfS529);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS519,
  moonbit_string_t _M0L5valueS521
) {
  int32_t _M0L3lenS2328;
  moonbit_string_t* _M0L6_2atmpS2330;
  int32_t _M0L6_2atmpS4202;
  int32_t _M0L6_2atmpS2329;
  int32_t _M0L6lengthS520;
  moonbit_string_t* _M0L8_2afieldS4201;
  moonbit_string_t* _M0L3bufS2331;
  moonbit_string_t _M0L6_2aoldS4200;
  int32_t _M0L6_2atmpS2332;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2328 = _M0L4selfS519->$1;
  moonbit_incref(_M0L4selfS519);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2330 = _M0MPC15array5Array6bufferGsE(_M0L4selfS519);
  _M0L6_2atmpS4202 = Moonbit_array_length(_M0L6_2atmpS2330);
  moonbit_decref(_M0L6_2atmpS2330);
  _M0L6_2atmpS2329 = _M0L6_2atmpS4202;
  if (_M0L3lenS2328 == _M0L6_2atmpS2329) {
    moonbit_incref(_M0L4selfS519);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS519);
  }
  _M0L6lengthS520 = _M0L4selfS519->$1;
  _M0L8_2afieldS4201 = _M0L4selfS519->$0;
  _M0L3bufS2331 = _M0L8_2afieldS4201;
  _M0L6_2aoldS4200 = (moonbit_string_t)_M0L3bufS2331[_M0L6lengthS520];
  moonbit_decref(_M0L6_2aoldS4200);
  _M0L3bufS2331[_M0L6lengthS520] = _M0L5valueS521;
  _M0L6_2atmpS2332 = _M0L6lengthS520 + 1;
  _M0L4selfS519->$1 = _M0L6_2atmpS2332;
  moonbit_decref(_M0L4selfS519);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS522,
  struct _M0TUsiE* _M0L5valueS524
) {
  int32_t _M0L3lenS2333;
  struct _M0TUsiE** _M0L6_2atmpS2335;
  int32_t _M0L6_2atmpS4205;
  int32_t _M0L6_2atmpS2334;
  int32_t _M0L6lengthS523;
  struct _M0TUsiE** _M0L8_2afieldS4204;
  struct _M0TUsiE** _M0L3bufS2336;
  struct _M0TUsiE* _M0L6_2aoldS4203;
  int32_t _M0L6_2atmpS2337;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2333 = _M0L4selfS522->$1;
  moonbit_incref(_M0L4selfS522);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2335 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS522);
  _M0L6_2atmpS4205 = Moonbit_array_length(_M0L6_2atmpS2335);
  moonbit_decref(_M0L6_2atmpS2335);
  _M0L6_2atmpS2334 = _M0L6_2atmpS4205;
  if (_M0L3lenS2333 == _M0L6_2atmpS2334) {
    moonbit_incref(_M0L4selfS522);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS522);
  }
  _M0L6lengthS523 = _M0L4selfS522->$1;
  _M0L8_2afieldS4204 = _M0L4selfS522->$0;
  _M0L3bufS2336 = _M0L8_2afieldS4204;
  _M0L6_2aoldS4203 = (struct _M0TUsiE*)_M0L3bufS2336[_M0L6lengthS523];
  if (_M0L6_2aoldS4203) {
    moonbit_decref(_M0L6_2aoldS4203);
  }
  _M0L3bufS2336[_M0L6lengthS523] = _M0L5valueS524;
  _M0L6_2atmpS2337 = _M0L6lengthS523 + 1;
  _M0L4selfS522->$1 = _M0L6_2atmpS2337;
  moonbit_decref(_M0L4selfS522);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS525,
  void* _M0L5valueS527
) {
  int32_t _M0L3lenS2338;
  void** _M0L6_2atmpS2340;
  int32_t _M0L6_2atmpS4208;
  int32_t _M0L6_2atmpS2339;
  int32_t _M0L6lengthS526;
  void** _M0L8_2afieldS4207;
  void** _M0L3bufS2341;
  void* _M0L6_2aoldS4206;
  int32_t _M0L6_2atmpS2342;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2338 = _M0L4selfS525->$1;
  moonbit_incref(_M0L4selfS525);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2340
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS525);
  _M0L6_2atmpS4208 = Moonbit_array_length(_M0L6_2atmpS2340);
  moonbit_decref(_M0L6_2atmpS2340);
  _M0L6_2atmpS2339 = _M0L6_2atmpS4208;
  if (_M0L3lenS2338 == _M0L6_2atmpS2339) {
    moonbit_incref(_M0L4selfS525);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS525);
  }
  _M0L6lengthS526 = _M0L4selfS525->$1;
  _M0L8_2afieldS4207 = _M0L4selfS525->$0;
  _M0L3bufS2341 = _M0L8_2afieldS4207;
  _M0L6_2aoldS4206 = (void*)_M0L3bufS2341[_M0L6lengthS526];
  moonbit_decref(_M0L6_2aoldS4206);
  _M0L3bufS2341[_M0L6lengthS526] = _M0L5valueS527;
  _M0L6_2atmpS2342 = _M0L6lengthS526 + 1;
  _M0L4selfS525->$1 = _M0L6_2atmpS2342;
  moonbit_decref(_M0L4selfS525);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS511) {
  int32_t _M0L8old__capS510;
  int32_t _M0L8new__capS512;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS510 = _M0L4selfS511->$1;
  if (_M0L8old__capS510 == 0) {
    _M0L8new__capS512 = 8;
  } else {
    _M0L8new__capS512 = _M0L8old__capS510 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS511, _M0L8new__capS512);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS514
) {
  int32_t _M0L8old__capS513;
  int32_t _M0L8new__capS515;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS513 = _M0L4selfS514->$1;
  if (_M0L8old__capS513 == 0) {
    _M0L8new__capS515 = 8;
  } else {
    _M0L8new__capS515 = _M0L8old__capS513 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS514, _M0L8new__capS515);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS517
) {
  int32_t _M0L8old__capS516;
  int32_t _M0L8new__capS518;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS516 = _M0L4selfS517->$1;
  if (_M0L8old__capS516 == 0) {
    _M0L8new__capS518 = 8;
  } else {
    _M0L8new__capS518 = _M0L8old__capS516 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS517, _M0L8new__capS518);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS495,
  int32_t _M0L13new__capacityS493
) {
  moonbit_string_t* _M0L8new__bufS492;
  moonbit_string_t* _M0L8_2afieldS4210;
  moonbit_string_t* _M0L8old__bufS494;
  int32_t _M0L8old__capS496;
  int32_t _M0L9copy__lenS497;
  moonbit_string_t* _M0L6_2aoldS4209;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS492
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS493, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4210 = _M0L4selfS495->$0;
  _M0L8old__bufS494 = _M0L8_2afieldS4210;
  _M0L8old__capS496 = Moonbit_array_length(_M0L8old__bufS494);
  if (_M0L8old__capS496 < _M0L13new__capacityS493) {
    _M0L9copy__lenS497 = _M0L8old__capS496;
  } else {
    _M0L9copy__lenS497 = _M0L13new__capacityS493;
  }
  moonbit_incref(_M0L8old__bufS494);
  moonbit_incref(_M0L8new__bufS492);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS492, 0, _M0L8old__bufS494, 0, _M0L9copy__lenS497);
  _M0L6_2aoldS4209 = _M0L4selfS495->$0;
  moonbit_decref(_M0L6_2aoldS4209);
  _M0L4selfS495->$0 = _M0L8new__bufS492;
  moonbit_decref(_M0L4selfS495);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS501,
  int32_t _M0L13new__capacityS499
) {
  struct _M0TUsiE** _M0L8new__bufS498;
  struct _M0TUsiE** _M0L8_2afieldS4212;
  struct _M0TUsiE** _M0L8old__bufS500;
  int32_t _M0L8old__capS502;
  int32_t _M0L9copy__lenS503;
  struct _M0TUsiE** _M0L6_2aoldS4211;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS498
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS499, 0);
  _M0L8_2afieldS4212 = _M0L4selfS501->$0;
  _M0L8old__bufS500 = _M0L8_2afieldS4212;
  _M0L8old__capS502 = Moonbit_array_length(_M0L8old__bufS500);
  if (_M0L8old__capS502 < _M0L13new__capacityS499) {
    _M0L9copy__lenS503 = _M0L8old__capS502;
  } else {
    _M0L9copy__lenS503 = _M0L13new__capacityS499;
  }
  moonbit_incref(_M0L8old__bufS500);
  moonbit_incref(_M0L8new__bufS498);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS498, 0, _M0L8old__bufS500, 0, _M0L9copy__lenS503);
  _M0L6_2aoldS4211 = _M0L4selfS501->$0;
  moonbit_decref(_M0L6_2aoldS4211);
  _M0L4selfS501->$0 = _M0L8new__bufS498;
  moonbit_decref(_M0L4selfS501);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS507,
  int32_t _M0L13new__capacityS505
) {
  void** _M0L8new__bufS504;
  void** _M0L8_2afieldS4214;
  void** _M0L8old__bufS506;
  int32_t _M0L8old__capS508;
  int32_t _M0L9copy__lenS509;
  void** _M0L6_2aoldS4213;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS504
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS505, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4214 = _M0L4selfS507->$0;
  _M0L8old__bufS506 = _M0L8_2afieldS4214;
  _M0L8old__capS508 = Moonbit_array_length(_M0L8old__bufS506);
  if (_M0L8old__capS508 < _M0L13new__capacityS505) {
    _M0L9copy__lenS509 = _M0L8old__capS508;
  } else {
    _M0L9copy__lenS509 = _M0L13new__capacityS505;
  }
  moonbit_incref(_M0L8old__bufS506);
  moonbit_incref(_M0L8new__bufS504);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS504, 0, _M0L8old__bufS506, 0, _M0L9copy__lenS509);
  _M0L6_2aoldS4213 = _M0L4selfS507->$0;
  moonbit_decref(_M0L6_2aoldS4213);
  _M0L4selfS507->$0 = _M0L8new__bufS504;
  moonbit_decref(_M0L4selfS507);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS491
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS491 == 0) {
    moonbit_string_t* _M0L6_2atmpS2326 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4655 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4655)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4655->$0 = _M0L6_2atmpS2326;
    _block_4655->$1 = 0;
    return _block_4655;
  } else {
    moonbit_string_t* _M0L6_2atmpS2327 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS491, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4656 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4656)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4656->$0 = _M0L6_2atmpS2327;
    _block_4656->$1 = 0;
    return _block_4656;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS485,
  int32_t _M0L1nS484
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS484 <= 0) {
    moonbit_decref(_M0L4selfS485);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS484 == 1) {
    return _M0L4selfS485;
  } else {
    int32_t _M0L3lenS486 = Moonbit_array_length(_M0L4selfS485);
    int32_t _M0L6_2atmpS2325 = _M0L3lenS486 * _M0L1nS484;
    struct _M0TPB13StringBuilder* _M0L3bufS487;
    moonbit_string_t _M0L3strS488;
    int32_t _M0L2__S489;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS487 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2325);
    _M0L3strS488 = _M0L4selfS485;
    _M0L2__S489 = 0;
    while (1) {
      if (_M0L2__S489 < _M0L1nS484) {
        int32_t _M0L6_2atmpS2324;
        moonbit_incref(_M0L3strS488);
        moonbit_incref(_M0L3bufS487);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS487, _M0L3strS488);
        _M0L6_2atmpS2324 = _M0L2__S489 + 1;
        _M0L2__S489 = _M0L6_2atmpS2324;
        continue;
      } else {
        moonbit_decref(_M0L3strS488);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS487);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS482,
  struct _M0TPC16string10StringView _M0L3strS483
) {
  int32_t _M0L3lenS2312;
  int32_t _M0L6_2atmpS2314;
  int32_t _M0L6_2atmpS2313;
  int32_t _M0L6_2atmpS2311;
  moonbit_bytes_t _M0L8_2afieldS4215;
  moonbit_bytes_t _M0L4dataS2315;
  int32_t _M0L3lenS2316;
  moonbit_string_t _M0L6_2atmpS2317;
  int32_t _M0L6_2atmpS2318;
  int32_t _M0L6_2atmpS2319;
  int32_t _M0L3lenS2321;
  int32_t _M0L6_2atmpS2323;
  int32_t _M0L6_2atmpS2322;
  int32_t _M0L6_2atmpS2320;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2312 = _M0L4selfS482->$1;
  moonbit_incref(_M0L3strS483.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2314 = _M0MPC16string10StringView6length(_M0L3strS483);
  _M0L6_2atmpS2313 = _M0L6_2atmpS2314 * 2;
  _M0L6_2atmpS2311 = _M0L3lenS2312 + _M0L6_2atmpS2313;
  moonbit_incref(_M0L4selfS482);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS482, _M0L6_2atmpS2311);
  _M0L8_2afieldS4215 = _M0L4selfS482->$0;
  _M0L4dataS2315 = _M0L8_2afieldS4215;
  _M0L3lenS2316 = _M0L4selfS482->$1;
  moonbit_incref(_M0L4dataS2315);
  moonbit_incref(_M0L3strS483.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2317 = _M0MPC16string10StringView4data(_M0L3strS483);
  moonbit_incref(_M0L3strS483.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2318 = _M0MPC16string10StringView13start__offset(_M0L3strS483);
  moonbit_incref(_M0L3strS483.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2319 = _M0MPC16string10StringView6length(_M0L3strS483);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2315, _M0L3lenS2316, _M0L6_2atmpS2317, _M0L6_2atmpS2318, _M0L6_2atmpS2319);
  _M0L3lenS2321 = _M0L4selfS482->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2323 = _M0MPC16string10StringView6length(_M0L3strS483);
  _M0L6_2atmpS2322 = _M0L6_2atmpS2323 * 2;
  _M0L6_2atmpS2320 = _M0L3lenS2321 + _M0L6_2atmpS2322;
  _M0L4selfS482->$1 = _M0L6_2atmpS2320;
  moonbit_decref(_M0L4selfS482);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS474,
  int32_t _M0L3lenS477,
  int32_t _M0L13start__offsetS481,
  int64_t _M0L11end__offsetS472
) {
  int32_t _M0L11end__offsetS471;
  int32_t _M0L5indexS475;
  int32_t _M0L5countS476;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS472 == 4294967296ll) {
    _M0L11end__offsetS471 = Moonbit_array_length(_M0L4selfS474);
  } else {
    int64_t _M0L7_2aSomeS473 = _M0L11end__offsetS472;
    _M0L11end__offsetS471 = (int32_t)_M0L7_2aSomeS473;
  }
  _M0L5indexS475 = _M0L13start__offsetS481;
  _M0L5countS476 = 0;
  while (1) {
    int32_t _if__result_4659;
    if (_M0L5indexS475 < _M0L11end__offsetS471) {
      _if__result_4659 = _M0L5countS476 < _M0L3lenS477;
    } else {
      _if__result_4659 = 0;
    }
    if (_if__result_4659) {
      int32_t _M0L2c1S478 = _M0L4selfS474[_M0L5indexS475];
      int32_t _if__result_4660;
      int32_t _M0L6_2atmpS2309;
      int32_t _M0L6_2atmpS2310;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S478)) {
        int32_t _M0L6_2atmpS2305 = _M0L5indexS475 + 1;
        _if__result_4660 = _M0L6_2atmpS2305 < _M0L11end__offsetS471;
      } else {
        _if__result_4660 = 0;
      }
      if (_if__result_4660) {
        int32_t _M0L6_2atmpS2308 = _M0L5indexS475 + 1;
        int32_t _M0L2c2S479 = _M0L4selfS474[_M0L6_2atmpS2308];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S479)) {
          int32_t _M0L6_2atmpS2306 = _M0L5indexS475 + 2;
          int32_t _M0L6_2atmpS2307 = _M0L5countS476 + 1;
          _M0L5indexS475 = _M0L6_2atmpS2306;
          _M0L5countS476 = _M0L6_2atmpS2307;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_79.data, (moonbit_string_t)moonbit_string_literal_80.data);
        }
      }
      _M0L6_2atmpS2309 = _M0L5indexS475 + 1;
      _M0L6_2atmpS2310 = _M0L5countS476 + 1;
      _M0L5indexS475 = _M0L6_2atmpS2309;
      _M0L5countS476 = _M0L6_2atmpS2310;
      continue;
    } else {
      moonbit_decref(_M0L4selfS474);
      return _M0L5countS476 >= _M0L3lenS477;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS465
) {
  int32_t _M0L3endS2293;
  int32_t _M0L8_2afieldS4216;
  int32_t _M0L5startS2294;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2293 = _M0L4selfS465.$2;
  _M0L8_2afieldS4216 = _M0L4selfS465.$1;
  moonbit_decref(_M0L4selfS465.$0);
  _M0L5startS2294 = _M0L8_2afieldS4216;
  return _M0L3endS2293 - _M0L5startS2294;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS466
) {
  int32_t _M0L3endS2295;
  int32_t _M0L8_2afieldS4217;
  int32_t _M0L5startS2296;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2295 = _M0L4selfS466.$2;
  _M0L8_2afieldS4217 = _M0L4selfS466.$1;
  moonbit_decref(_M0L4selfS466.$0);
  _M0L5startS2296 = _M0L8_2afieldS4217;
  return _M0L3endS2295 - _M0L5startS2296;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS467
) {
  int32_t _M0L3endS2297;
  int32_t _M0L8_2afieldS4218;
  int32_t _M0L5startS2298;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2297 = _M0L4selfS467.$2;
  _M0L8_2afieldS4218 = _M0L4selfS467.$1;
  moonbit_decref(_M0L4selfS467.$0);
  _M0L5startS2298 = _M0L8_2afieldS4218;
  return _M0L3endS2297 - _M0L5startS2298;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TPB9ArrayViewGUsRP48clawteam8clawteam8internal6schema6SchemaEE _M0L4selfS468
) {
  int32_t _M0L3endS2299;
  int32_t _M0L8_2afieldS4219;
  int32_t _M0L5startS2300;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2299 = _M0L4selfS468.$2;
  _M0L8_2afieldS4219 = _M0L4selfS468.$1;
  moonbit_decref(_M0L4selfS468.$0);
  _M0L5startS2300 = _M0L8_2afieldS4219;
  return _M0L3endS2299 - _M0L5startS2300;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS469
) {
  int32_t _M0L3endS2301;
  int32_t _M0L8_2afieldS4220;
  int32_t _M0L5startS2302;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2301 = _M0L4selfS469.$2;
  _M0L8_2afieldS4220 = _M0L4selfS469.$1;
  moonbit_decref(_M0L4selfS469.$0);
  _M0L5startS2302 = _M0L8_2afieldS4220;
  return _M0L3endS2301 - _M0L5startS2302;
}

int32_t _M0MPC15array9ArrayView6lengthGRPB4JsonE(
  struct _M0TPB9ArrayViewGRPB4JsonE _M0L4selfS470
) {
  int32_t _M0L3endS2303;
  int32_t _M0L8_2afieldS4221;
  int32_t _M0L5startS2304;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2303 = _M0L4selfS470.$2;
  _M0L8_2afieldS4221 = _M0L4selfS470.$1;
  moonbit_decref(_M0L4selfS470.$0);
  _M0L5startS2304 = _M0L8_2afieldS4221;
  return _M0L3endS2303 - _M0L5startS2304;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS463,
  int64_t _M0L19start__offset_2eoptS461,
  int64_t _M0L11end__offsetS464
) {
  int32_t _M0L13start__offsetS460;
  if (_M0L19start__offset_2eoptS461 == 4294967296ll) {
    _M0L13start__offsetS460 = 0;
  } else {
    int64_t _M0L7_2aSomeS462 = _M0L19start__offset_2eoptS461;
    _M0L13start__offsetS460 = (int32_t)_M0L7_2aSomeS462;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS463, _M0L13start__offsetS460, _M0L11end__offsetS464);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS458,
  int32_t _M0L13start__offsetS459,
  int64_t _M0L11end__offsetS456
) {
  int32_t _M0L11end__offsetS455;
  int32_t _if__result_4661;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS456 == 4294967296ll) {
    _M0L11end__offsetS455 = Moonbit_array_length(_M0L4selfS458);
  } else {
    int64_t _M0L7_2aSomeS457 = _M0L11end__offsetS456;
    _M0L11end__offsetS455 = (int32_t)_M0L7_2aSomeS457;
  }
  if (_M0L13start__offsetS459 >= 0) {
    if (_M0L13start__offsetS459 <= _M0L11end__offsetS455) {
      int32_t _M0L6_2atmpS2292 = Moonbit_array_length(_M0L4selfS458);
      _if__result_4661 = _M0L11end__offsetS455 <= _M0L6_2atmpS2292;
    } else {
      _if__result_4661 = 0;
    }
  } else {
    _if__result_4661 = 0;
  }
  if (_if__result_4661) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS459,
                                                 _M0L11end__offsetS455,
                                                 _M0L4selfS458};
  } else {
    moonbit_decref(_M0L4selfS458);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_81.data, (moonbit_string_t)moonbit_string_literal_82.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS454
) {
  moonbit_string_t _M0L8_2afieldS4223;
  moonbit_string_t _M0L3strS2289;
  int32_t _M0L5startS2290;
  int32_t _M0L8_2afieldS4222;
  int32_t _M0L3endS2291;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4223 = _M0L4selfS454.$0;
  _M0L3strS2289 = _M0L8_2afieldS4223;
  _M0L5startS2290 = _M0L4selfS454.$1;
  _M0L8_2afieldS4222 = _M0L4selfS454.$2;
  _M0L3endS2291 = _M0L8_2afieldS4222;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2289, _M0L5startS2290, _M0L3endS2291);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS452,
  struct _M0TPB6Logger _M0L6loggerS453
) {
  moonbit_string_t _M0L8_2afieldS4225;
  moonbit_string_t _M0L3strS2286;
  int32_t _M0L5startS2287;
  int32_t _M0L8_2afieldS4224;
  int32_t _M0L3endS2288;
  moonbit_string_t _M0L6substrS451;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4225 = _M0L4selfS452.$0;
  _M0L3strS2286 = _M0L8_2afieldS4225;
  _M0L5startS2287 = _M0L4selfS452.$1;
  _M0L8_2afieldS4224 = _M0L4selfS452.$2;
  _M0L3endS2288 = _M0L8_2afieldS4224;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS451
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2286, _M0L5startS2287, _M0L3endS2288);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS451, _M0L6loggerS453);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS443,
  struct _M0TPB6Logger _M0L6loggerS441
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS442;
  int32_t _M0L3lenS444;
  int32_t _M0L1iS445;
  int32_t _M0L3segS446;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS441.$1) {
    moonbit_incref(_M0L6loggerS441.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS441.$0->$method_3(_M0L6loggerS441.$1, 34);
  moonbit_incref(_M0L4selfS443);
  if (_M0L6loggerS441.$1) {
    moonbit_incref(_M0L6loggerS441.$1);
  }
  _M0L6_2aenvS442
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS442)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS442->$0 = _M0L4selfS443;
  _M0L6_2aenvS442->$1_0 = _M0L6loggerS441.$0;
  _M0L6_2aenvS442->$1_1 = _M0L6loggerS441.$1;
  _M0L3lenS444 = Moonbit_array_length(_M0L4selfS443);
  _M0L1iS445 = 0;
  _M0L3segS446 = 0;
  _2afor_447:;
  while (1) {
    int32_t _M0L4codeS448;
    int32_t _M0L1cS450;
    int32_t _M0L6_2atmpS2270;
    int32_t _M0L6_2atmpS2271;
    int32_t _M0L6_2atmpS2272;
    int32_t _tmp_4665;
    int32_t _tmp_4666;
    if (_M0L1iS445 >= _M0L3lenS444) {
      moonbit_decref(_M0L4selfS443);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
      break;
    }
    _M0L4codeS448 = _M0L4selfS443[_M0L1iS445];
    switch (_M0L4codeS448) {
      case 34: {
        _M0L1cS450 = _M0L4codeS448;
        goto join_449;
        break;
      }
      
      case 92: {
        _M0L1cS450 = _M0L4codeS448;
        goto join_449;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2273;
        int32_t _M0L6_2atmpS2274;
        moonbit_incref(_M0L6_2aenvS442);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
        if (_M0L6loggerS441.$1) {
          moonbit_incref(_M0L6loggerS441.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS441.$0->$method_0(_M0L6loggerS441.$1, (moonbit_string_t)moonbit_string_literal_60.data);
        _M0L6_2atmpS2273 = _M0L1iS445 + 1;
        _M0L6_2atmpS2274 = _M0L1iS445 + 1;
        _M0L1iS445 = _M0L6_2atmpS2273;
        _M0L3segS446 = _M0L6_2atmpS2274;
        goto _2afor_447;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2275;
        int32_t _M0L6_2atmpS2276;
        moonbit_incref(_M0L6_2aenvS442);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
        if (_M0L6loggerS441.$1) {
          moonbit_incref(_M0L6loggerS441.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS441.$0->$method_0(_M0L6loggerS441.$1, (moonbit_string_t)moonbit_string_literal_61.data);
        _M0L6_2atmpS2275 = _M0L1iS445 + 1;
        _M0L6_2atmpS2276 = _M0L1iS445 + 1;
        _M0L1iS445 = _M0L6_2atmpS2275;
        _M0L3segS446 = _M0L6_2atmpS2276;
        goto _2afor_447;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2277;
        int32_t _M0L6_2atmpS2278;
        moonbit_incref(_M0L6_2aenvS442);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
        if (_M0L6loggerS441.$1) {
          moonbit_incref(_M0L6loggerS441.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS441.$0->$method_0(_M0L6loggerS441.$1, (moonbit_string_t)moonbit_string_literal_62.data);
        _M0L6_2atmpS2277 = _M0L1iS445 + 1;
        _M0L6_2atmpS2278 = _M0L1iS445 + 1;
        _M0L1iS445 = _M0L6_2atmpS2277;
        _M0L3segS446 = _M0L6_2atmpS2278;
        goto _2afor_447;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2279;
        int32_t _M0L6_2atmpS2280;
        moonbit_incref(_M0L6_2aenvS442);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
        if (_M0L6loggerS441.$1) {
          moonbit_incref(_M0L6loggerS441.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS441.$0->$method_0(_M0L6loggerS441.$1, (moonbit_string_t)moonbit_string_literal_63.data);
        _M0L6_2atmpS2279 = _M0L1iS445 + 1;
        _M0L6_2atmpS2280 = _M0L1iS445 + 1;
        _M0L1iS445 = _M0L6_2atmpS2279;
        _M0L3segS446 = _M0L6_2atmpS2280;
        goto _2afor_447;
        break;
      }
      default: {
        if (_M0L4codeS448 < 32) {
          int32_t _M0L6_2atmpS2282;
          moonbit_string_t _M0L6_2atmpS2281;
          int32_t _M0L6_2atmpS2283;
          int32_t _M0L6_2atmpS2284;
          moonbit_incref(_M0L6_2aenvS442);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
          if (_M0L6loggerS441.$1) {
            moonbit_incref(_M0L6loggerS441.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS441.$0->$method_0(_M0L6loggerS441.$1, (moonbit_string_t)moonbit_string_literal_83.data);
          _M0L6_2atmpS2282 = _M0L4codeS448 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2281 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2282);
          if (_M0L6loggerS441.$1) {
            moonbit_incref(_M0L6loggerS441.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS441.$0->$method_0(_M0L6loggerS441.$1, _M0L6_2atmpS2281);
          if (_M0L6loggerS441.$1) {
            moonbit_incref(_M0L6loggerS441.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS441.$0->$method_3(_M0L6loggerS441.$1, 125);
          _M0L6_2atmpS2283 = _M0L1iS445 + 1;
          _M0L6_2atmpS2284 = _M0L1iS445 + 1;
          _M0L1iS445 = _M0L6_2atmpS2283;
          _M0L3segS446 = _M0L6_2atmpS2284;
          goto _2afor_447;
        } else {
          int32_t _M0L6_2atmpS2285 = _M0L1iS445 + 1;
          int32_t _tmp_4664 = _M0L3segS446;
          _M0L1iS445 = _M0L6_2atmpS2285;
          _M0L3segS446 = _tmp_4664;
          goto _2afor_447;
        }
        break;
      }
    }
    goto joinlet_4663;
    join_449:;
    moonbit_incref(_M0L6_2aenvS442);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS442, _M0L3segS446, _M0L1iS445);
    if (_M0L6loggerS441.$1) {
      moonbit_incref(_M0L6loggerS441.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS441.$0->$method_3(_M0L6loggerS441.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2270 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS450);
    if (_M0L6loggerS441.$1) {
      moonbit_incref(_M0L6loggerS441.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS441.$0->$method_3(_M0L6loggerS441.$1, _M0L6_2atmpS2270);
    _M0L6_2atmpS2271 = _M0L1iS445 + 1;
    _M0L6_2atmpS2272 = _M0L1iS445 + 1;
    _M0L1iS445 = _M0L6_2atmpS2271;
    _M0L3segS446 = _M0L6_2atmpS2272;
    continue;
    joinlet_4663:;
    _tmp_4665 = _M0L1iS445;
    _tmp_4666 = _M0L3segS446;
    _M0L1iS445 = _tmp_4665;
    _M0L3segS446 = _tmp_4666;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS441.$0->$method_3(_M0L6loggerS441.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS437,
  int32_t _M0L3segS440,
  int32_t _M0L1iS439
) {
  struct _M0TPB6Logger _M0L8_2afieldS4227;
  struct _M0TPB6Logger _M0L6loggerS436;
  moonbit_string_t _M0L8_2afieldS4226;
  int32_t _M0L6_2acntS4473;
  moonbit_string_t _M0L4selfS438;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4227
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS437->$1_0, _M0L6_2aenvS437->$1_1
  };
  _M0L6loggerS436 = _M0L8_2afieldS4227;
  _M0L8_2afieldS4226 = _M0L6_2aenvS437->$0;
  _M0L6_2acntS4473 = Moonbit_object_header(_M0L6_2aenvS437)->rc;
  if (_M0L6_2acntS4473 > 1) {
    int32_t _M0L11_2anew__cntS4474 = _M0L6_2acntS4473 - 1;
    Moonbit_object_header(_M0L6_2aenvS437)->rc = _M0L11_2anew__cntS4474;
    if (_M0L6loggerS436.$1) {
      moonbit_incref(_M0L6loggerS436.$1);
    }
    moonbit_incref(_M0L8_2afieldS4226);
  } else if (_M0L6_2acntS4473 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS437);
  }
  _M0L4selfS438 = _M0L8_2afieldS4226;
  if (_M0L1iS439 > _M0L3segS440) {
    int32_t _M0L6_2atmpS2269 = _M0L1iS439 - _M0L3segS440;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS436.$0->$method_1(_M0L6loggerS436.$1, _M0L4selfS438, _M0L3segS440, _M0L6_2atmpS2269);
  } else {
    moonbit_decref(_M0L4selfS438);
    if (_M0L6loggerS436.$1) {
      moonbit_decref(_M0L6loggerS436.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS435) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS434;
  int32_t _M0L6_2atmpS2266;
  int32_t _M0L6_2atmpS2265;
  int32_t _M0L6_2atmpS2268;
  int32_t _M0L6_2atmpS2267;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2264;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS434 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2266 = _M0IPC14byte4BytePB3Div3div(_M0L1bS435, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2265
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2266);
  moonbit_incref(_M0L7_2aselfS434);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS434, _M0L6_2atmpS2265);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2268 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS435, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2267
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2268);
  moonbit_incref(_M0L7_2aselfS434);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS434, _M0L6_2atmpS2267);
  _M0L6_2atmpS2264 = _M0L7_2aselfS434;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2264);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS433) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS433 < 10) {
    int32_t _M0L6_2atmpS2261;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2261 = _M0IPC14byte4BytePB3Add3add(_M0L1iS433, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2261);
  } else {
    int32_t _M0L6_2atmpS2263;
    int32_t _M0L6_2atmpS2262;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2263 = _M0IPC14byte4BytePB3Add3add(_M0L1iS433, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2262 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2263, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2262);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS431,
  int32_t _M0L4thatS432
) {
  int32_t _M0L6_2atmpS2259;
  int32_t _M0L6_2atmpS2260;
  int32_t _M0L6_2atmpS2258;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2259 = (int32_t)_M0L4selfS431;
  _M0L6_2atmpS2260 = (int32_t)_M0L4thatS432;
  _M0L6_2atmpS2258 = _M0L6_2atmpS2259 - _M0L6_2atmpS2260;
  return _M0L6_2atmpS2258 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS429,
  int32_t _M0L4thatS430
) {
  int32_t _M0L6_2atmpS2256;
  int32_t _M0L6_2atmpS2257;
  int32_t _M0L6_2atmpS2255;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2256 = (int32_t)_M0L4selfS429;
  _M0L6_2atmpS2257 = (int32_t)_M0L4thatS430;
  _M0L6_2atmpS2255 = _M0L6_2atmpS2256 % _M0L6_2atmpS2257;
  return _M0L6_2atmpS2255 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS427,
  int32_t _M0L4thatS428
) {
  int32_t _M0L6_2atmpS2253;
  int32_t _M0L6_2atmpS2254;
  int32_t _M0L6_2atmpS2252;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2253 = (int32_t)_M0L4selfS427;
  _M0L6_2atmpS2254 = (int32_t)_M0L4thatS428;
  _M0L6_2atmpS2252 = _M0L6_2atmpS2253 / _M0L6_2atmpS2254;
  return _M0L6_2atmpS2252 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS425,
  int32_t _M0L4thatS426
) {
  int32_t _M0L6_2atmpS2250;
  int32_t _M0L6_2atmpS2251;
  int32_t _M0L6_2atmpS2249;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2250 = (int32_t)_M0L4selfS425;
  _M0L6_2atmpS2251 = (int32_t)_M0L4thatS426;
  _M0L6_2atmpS2249 = _M0L6_2atmpS2250 + _M0L6_2atmpS2251;
  return _M0L6_2atmpS2249 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS422,
  int32_t _M0L5startS420,
  int32_t _M0L3endS421
) {
  int32_t _if__result_4667;
  int32_t _M0L3lenS423;
  int32_t _M0L6_2atmpS2247;
  int32_t _M0L6_2atmpS2248;
  moonbit_bytes_t _M0L5bytesS424;
  moonbit_bytes_t _M0L6_2atmpS2246;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS420 == 0) {
    int32_t _M0L6_2atmpS2245 = Moonbit_array_length(_M0L3strS422);
    _if__result_4667 = _M0L3endS421 == _M0L6_2atmpS2245;
  } else {
    _if__result_4667 = 0;
  }
  if (_if__result_4667) {
    return _M0L3strS422;
  }
  _M0L3lenS423 = _M0L3endS421 - _M0L5startS420;
  _M0L6_2atmpS2247 = _M0L3lenS423 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2248 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS424
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2247, _M0L6_2atmpS2248);
  moonbit_incref(_M0L5bytesS424);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS424, 0, _M0L3strS422, _M0L5startS420, _M0L3lenS423);
  _M0L6_2atmpS2246 = _M0L5bytesS424;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2246, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS414) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS414;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS415
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS415;
}

struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB4Iter3newGUsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L1fS416
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS416;
}

struct _M0TWEORPB4Json* _M0MPB4Iter3newGRPB4JsonE(
  struct _M0TWEORPB4Json* _M0L1fS417
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS417;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS418) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS418;
}

struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0MPB4Iter3newGUsRPB3MapGsRPB4JsonEEE(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L1fS419
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS419;
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS413,
  moonbit_string_t _M0L3locS412
) {
  moonbit_string_t _M0L6_2atmpS2244;
  moonbit_string_t _M0L6_2atmpS4229;
  moonbit_string_t _M0L6_2atmpS2242;
  moonbit_string_t _M0L6_2atmpS2243;
  moonbit_string_t _M0L6_2atmpS4228;
  moonbit_string_t _M0L6_2atmpS2241;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2240;
  struct moonbit_result_0 _result_4668;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS2244
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS412);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS4229
  = moonbit_add_string(_M0L6_2atmpS2244, (moonbit_string_t)moonbit_string_literal_84.data);
  moonbit_decref(_M0L6_2atmpS2244);
  _M0L6_2atmpS2242 = _M0L6_2atmpS4229;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS2243 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS413);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS4228 = moonbit_add_string(_M0L6_2atmpS2242, _M0L6_2atmpS2243);
  moonbit_decref(_M0L6_2atmpS2242);
  moonbit_decref(_M0L6_2atmpS2243);
  _M0L6_2atmpS2241 = _M0L6_2atmpS4228;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2240
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2240)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2240)->$0
  = _M0L6_2atmpS2241;
  _result_4668.tag = 0;
  _result_4668.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2240;
  return _result_4668;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS404,
  int32_t _M0L5radixS403
) {
  int32_t _if__result_4669;
  uint16_t* _M0L6bufferS405;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS403 < 2) {
    _if__result_4669 = 1;
  } else {
    _if__result_4669 = _M0L5radixS403 > 36;
  }
  if (_if__result_4669) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_85.data, (moonbit_string_t)moonbit_string_literal_86.data);
  }
  if (_M0L4selfS404 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_66.data;
  }
  switch (_M0L5radixS403) {
    case 10: {
      int32_t _M0L3lenS406;
      uint16_t* _M0L6bufferS407;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS406 = _M0FPB12dec__count64(_M0L4selfS404);
      _M0L6bufferS407 = (uint16_t*)moonbit_make_string(_M0L3lenS406, 0);
      moonbit_incref(_M0L6bufferS407);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS407, _M0L4selfS404, 0, _M0L3lenS406);
      _M0L6bufferS405 = _M0L6bufferS407;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS408;
      uint16_t* _M0L6bufferS409;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS408 = _M0FPB12hex__count64(_M0L4selfS404);
      _M0L6bufferS409 = (uint16_t*)moonbit_make_string(_M0L3lenS408, 0);
      moonbit_incref(_M0L6bufferS409);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS409, _M0L4selfS404, 0, _M0L3lenS408);
      _M0L6bufferS405 = _M0L6bufferS409;
      break;
    }
    default: {
      int32_t _M0L3lenS410;
      uint16_t* _M0L6bufferS411;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS410 = _M0FPB14radix__count64(_M0L4selfS404, _M0L5radixS403);
      _M0L6bufferS411 = (uint16_t*)moonbit_make_string(_M0L3lenS410, 0);
      moonbit_incref(_M0L6bufferS411);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS411, _M0L4selfS404, 0, _M0L3lenS410, _M0L5radixS403);
      _M0L6bufferS405 = _M0L6bufferS411;
      break;
    }
  }
  return _M0L6bufferS405;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS393,
  uint64_t _M0L3numS381,
  int32_t _M0L12digit__startS384,
  int32_t _M0L10total__lenS383
) {
  uint64_t _M0Lm3numS380;
  int32_t _M0Lm6offsetS382;
  uint64_t _M0L6_2atmpS2239;
  int32_t _M0Lm9remainingS395;
  int32_t _M0L6_2atmpS2220;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS380 = _M0L3numS381;
  _M0Lm6offsetS382 = _M0L10total__lenS383 - _M0L12digit__startS384;
  while (1) {
    uint64_t _M0L6_2atmpS2183 = _M0Lm3numS380;
    if (_M0L6_2atmpS2183 >= 10000ull) {
      uint64_t _M0L6_2atmpS2206 = _M0Lm3numS380;
      uint64_t _M0L1tS385 = _M0L6_2atmpS2206 / 10000ull;
      uint64_t _M0L6_2atmpS2205 = _M0Lm3numS380;
      uint64_t _M0L6_2atmpS2204 = _M0L6_2atmpS2205 % 10000ull;
      int32_t _M0L1rS386 = (int32_t)_M0L6_2atmpS2204;
      int32_t _M0L2d1S387;
      int32_t _M0L2d2S388;
      int32_t _M0L6_2atmpS2184;
      int32_t _M0L6_2atmpS2203;
      int32_t _M0L6_2atmpS2202;
      int32_t _M0L6d1__hiS389;
      int32_t _M0L6_2atmpS2201;
      int32_t _M0L6_2atmpS2200;
      int32_t _M0L6d1__loS390;
      int32_t _M0L6_2atmpS2199;
      int32_t _M0L6_2atmpS2198;
      int32_t _M0L6d2__hiS391;
      int32_t _M0L6_2atmpS2197;
      int32_t _M0L6_2atmpS2196;
      int32_t _M0L6d2__loS392;
      int32_t _M0L6_2atmpS2186;
      int32_t _M0L6_2atmpS2185;
      int32_t _M0L6_2atmpS2189;
      int32_t _M0L6_2atmpS2188;
      int32_t _M0L6_2atmpS2187;
      int32_t _M0L6_2atmpS2192;
      int32_t _M0L6_2atmpS2191;
      int32_t _M0L6_2atmpS2190;
      int32_t _M0L6_2atmpS2195;
      int32_t _M0L6_2atmpS2194;
      int32_t _M0L6_2atmpS2193;
      _M0Lm3numS380 = _M0L1tS385;
      _M0L2d1S387 = _M0L1rS386 / 100;
      _M0L2d2S388 = _M0L1rS386 % 100;
      _M0L6_2atmpS2184 = _M0Lm6offsetS382;
      _M0Lm6offsetS382 = _M0L6_2atmpS2184 - 4;
      _M0L6_2atmpS2203 = _M0L2d1S387 / 10;
      _M0L6_2atmpS2202 = 48 + _M0L6_2atmpS2203;
      _M0L6d1__hiS389 = (uint16_t)_M0L6_2atmpS2202;
      _M0L6_2atmpS2201 = _M0L2d1S387 % 10;
      _M0L6_2atmpS2200 = 48 + _M0L6_2atmpS2201;
      _M0L6d1__loS390 = (uint16_t)_M0L6_2atmpS2200;
      _M0L6_2atmpS2199 = _M0L2d2S388 / 10;
      _M0L6_2atmpS2198 = 48 + _M0L6_2atmpS2199;
      _M0L6d2__hiS391 = (uint16_t)_M0L6_2atmpS2198;
      _M0L6_2atmpS2197 = _M0L2d2S388 % 10;
      _M0L6_2atmpS2196 = 48 + _M0L6_2atmpS2197;
      _M0L6d2__loS392 = (uint16_t)_M0L6_2atmpS2196;
      _M0L6_2atmpS2186 = _M0Lm6offsetS382;
      _M0L6_2atmpS2185 = _M0L12digit__startS384 + _M0L6_2atmpS2186;
      _M0L6bufferS393[_M0L6_2atmpS2185] = _M0L6d1__hiS389;
      _M0L6_2atmpS2189 = _M0Lm6offsetS382;
      _M0L6_2atmpS2188 = _M0L12digit__startS384 + _M0L6_2atmpS2189;
      _M0L6_2atmpS2187 = _M0L6_2atmpS2188 + 1;
      _M0L6bufferS393[_M0L6_2atmpS2187] = _M0L6d1__loS390;
      _M0L6_2atmpS2192 = _M0Lm6offsetS382;
      _M0L6_2atmpS2191 = _M0L12digit__startS384 + _M0L6_2atmpS2192;
      _M0L6_2atmpS2190 = _M0L6_2atmpS2191 + 2;
      _M0L6bufferS393[_M0L6_2atmpS2190] = _M0L6d2__hiS391;
      _M0L6_2atmpS2195 = _M0Lm6offsetS382;
      _M0L6_2atmpS2194 = _M0L12digit__startS384 + _M0L6_2atmpS2195;
      _M0L6_2atmpS2193 = _M0L6_2atmpS2194 + 3;
      _M0L6bufferS393[_M0L6_2atmpS2193] = _M0L6d2__loS392;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2239 = _M0Lm3numS380;
  _M0Lm9remainingS395 = (int32_t)_M0L6_2atmpS2239;
  while (1) {
    int32_t _M0L6_2atmpS2207 = _M0Lm9remainingS395;
    if (_M0L6_2atmpS2207 >= 100) {
      int32_t _M0L6_2atmpS2219 = _M0Lm9remainingS395;
      int32_t _M0L1tS396 = _M0L6_2atmpS2219 / 100;
      int32_t _M0L6_2atmpS2218 = _M0Lm9remainingS395;
      int32_t _M0L1dS397 = _M0L6_2atmpS2218 % 100;
      int32_t _M0L6_2atmpS2208;
      int32_t _M0L6_2atmpS2217;
      int32_t _M0L6_2atmpS2216;
      int32_t _M0L5d__hiS398;
      int32_t _M0L6_2atmpS2215;
      int32_t _M0L6_2atmpS2214;
      int32_t _M0L5d__loS399;
      int32_t _M0L6_2atmpS2210;
      int32_t _M0L6_2atmpS2209;
      int32_t _M0L6_2atmpS2213;
      int32_t _M0L6_2atmpS2212;
      int32_t _M0L6_2atmpS2211;
      _M0Lm9remainingS395 = _M0L1tS396;
      _M0L6_2atmpS2208 = _M0Lm6offsetS382;
      _M0Lm6offsetS382 = _M0L6_2atmpS2208 - 2;
      _M0L6_2atmpS2217 = _M0L1dS397 / 10;
      _M0L6_2atmpS2216 = 48 + _M0L6_2atmpS2217;
      _M0L5d__hiS398 = (uint16_t)_M0L6_2atmpS2216;
      _M0L6_2atmpS2215 = _M0L1dS397 % 10;
      _M0L6_2atmpS2214 = 48 + _M0L6_2atmpS2215;
      _M0L5d__loS399 = (uint16_t)_M0L6_2atmpS2214;
      _M0L6_2atmpS2210 = _M0Lm6offsetS382;
      _M0L6_2atmpS2209 = _M0L12digit__startS384 + _M0L6_2atmpS2210;
      _M0L6bufferS393[_M0L6_2atmpS2209] = _M0L5d__hiS398;
      _M0L6_2atmpS2213 = _M0Lm6offsetS382;
      _M0L6_2atmpS2212 = _M0L12digit__startS384 + _M0L6_2atmpS2213;
      _M0L6_2atmpS2211 = _M0L6_2atmpS2212 + 1;
      _M0L6bufferS393[_M0L6_2atmpS2211] = _M0L5d__loS399;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2220 = _M0Lm9remainingS395;
  if (_M0L6_2atmpS2220 >= 10) {
    int32_t _M0L6_2atmpS2221 = _M0Lm6offsetS382;
    int32_t _M0L6_2atmpS2232;
    int32_t _M0L6_2atmpS2231;
    int32_t _M0L6_2atmpS2230;
    int32_t _M0L5d__hiS401;
    int32_t _M0L6_2atmpS2229;
    int32_t _M0L6_2atmpS2228;
    int32_t _M0L6_2atmpS2227;
    int32_t _M0L5d__loS402;
    int32_t _M0L6_2atmpS2223;
    int32_t _M0L6_2atmpS2222;
    int32_t _M0L6_2atmpS2226;
    int32_t _M0L6_2atmpS2225;
    int32_t _M0L6_2atmpS2224;
    _M0Lm6offsetS382 = _M0L6_2atmpS2221 - 2;
    _M0L6_2atmpS2232 = _M0Lm9remainingS395;
    _M0L6_2atmpS2231 = _M0L6_2atmpS2232 / 10;
    _M0L6_2atmpS2230 = 48 + _M0L6_2atmpS2231;
    _M0L5d__hiS401 = (uint16_t)_M0L6_2atmpS2230;
    _M0L6_2atmpS2229 = _M0Lm9remainingS395;
    _M0L6_2atmpS2228 = _M0L6_2atmpS2229 % 10;
    _M0L6_2atmpS2227 = 48 + _M0L6_2atmpS2228;
    _M0L5d__loS402 = (uint16_t)_M0L6_2atmpS2227;
    _M0L6_2atmpS2223 = _M0Lm6offsetS382;
    _M0L6_2atmpS2222 = _M0L12digit__startS384 + _M0L6_2atmpS2223;
    _M0L6bufferS393[_M0L6_2atmpS2222] = _M0L5d__hiS401;
    _M0L6_2atmpS2226 = _M0Lm6offsetS382;
    _M0L6_2atmpS2225 = _M0L12digit__startS384 + _M0L6_2atmpS2226;
    _M0L6_2atmpS2224 = _M0L6_2atmpS2225 + 1;
    _M0L6bufferS393[_M0L6_2atmpS2224] = _M0L5d__loS402;
    moonbit_decref(_M0L6bufferS393);
  } else {
    int32_t _M0L6_2atmpS2233 = _M0Lm6offsetS382;
    int32_t _M0L6_2atmpS2238;
    int32_t _M0L6_2atmpS2234;
    int32_t _M0L6_2atmpS2237;
    int32_t _M0L6_2atmpS2236;
    int32_t _M0L6_2atmpS2235;
    _M0Lm6offsetS382 = _M0L6_2atmpS2233 - 1;
    _M0L6_2atmpS2238 = _M0Lm6offsetS382;
    _M0L6_2atmpS2234 = _M0L12digit__startS384 + _M0L6_2atmpS2238;
    _M0L6_2atmpS2237 = _M0Lm9remainingS395;
    _M0L6_2atmpS2236 = 48 + _M0L6_2atmpS2237;
    _M0L6_2atmpS2235 = (uint16_t)_M0L6_2atmpS2236;
    _M0L6bufferS393[_M0L6_2atmpS2234] = _M0L6_2atmpS2235;
    moonbit_decref(_M0L6bufferS393);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS375,
  uint64_t _M0L3numS369,
  int32_t _M0L12digit__startS367,
  int32_t _M0L10total__lenS366,
  int32_t _M0L5radixS371
) {
  int32_t _M0Lm6offsetS365;
  uint64_t _M0Lm1nS368;
  uint64_t _M0L4baseS370;
  int32_t _M0L6_2atmpS2165;
  int32_t _M0L6_2atmpS2164;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS365 = _M0L10total__lenS366 - _M0L12digit__startS367;
  _M0Lm1nS368 = _M0L3numS369;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS370 = _M0MPC13int3Int10to__uint64(_M0L5radixS371);
  _M0L6_2atmpS2165 = _M0L5radixS371 - 1;
  _M0L6_2atmpS2164 = _M0L5radixS371 & _M0L6_2atmpS2165;
  if (_M0L6_2atmpS2164 == 0) {
    int32_t _M0L5shiftS372;
    uint64_t _M0L4maskS373;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS372 = moonbit_ctz32(_M0L5radixS371);
    _M0L4maskS373 = _M0L4baseS370 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS2166 = _M0Lm1nS368;
      if (_M0L6_2atmpS2166 > 0ull) {
        int32_t _M0L6_2atmpS2167 = _M0Lm6offsetS365;
        uint64_t _M0L6_2atmpS2173;
        uint64_t _M0L6_2atmpS2172;
        int32_t _M0L5digitS374;
        int32_t _M0L6_2atmpS2170;
        int32_t _M0L6_2atmpS2168;
        int32_t _M0L6_2atmpS2169;
        uint64_t _M0L6_2atmpS2171;
        _M0Lm6offsetS365 = _M0L6_2atmpS2167 - 1;
        _M0L6_2atmpS2173 = _M0Lm1nS368;
        _M0L6_2atmpS2172 = _M0L6_2atmpS2173 & _M0L4maskS373;
        _M0L5digitS374 = (int32_t)_M0L6_2atmpS2172;
        _M0L6_2atmpS2170 = _M0Lm6offsetS365;
        _M0L6_2atmpS2168 = _M0L12digit__startS367 + _M0L6_2atmpS2170;
        _M0L6_2atmpS2169
        = ((moonbit_string_t)moonbit_string_literal_87.data)[
          _M0L5digitS374
        ];
        _M0L6bufferS375[_M0L6_2atmpS2168] = _M0L6_2atmpS2169;
        _M0L6_2atmpS2171 = _M0Lm1nS368;
        _M0Lm1nS368 = _M0L6_2atmpS2171 >> (_M0L5shiftS372 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS375);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS2174 = _M0Lm1nS368;
      if (_M0L6_2atmpS2174 > 0ull) {
        int32_t _M0L6_2atmpS2175 = _M0Lm6offsetS365;
        uint64_t _M0L6_2atmpS2182;
        uint64_t _M0L1qS377;
        uint64_t _M0L6_2atmpS2180;
        uint64_t _M0L6_2atmpS2181;
        uint64_t _M0L6_2atmpS2179;
        int32_t _M0L5digitS378;
        int32_t _M0L6_2atmpS2178;
        int32_t _M0L6_2atmpS2176;
        int32_t _M0L6_2atmpS2177;
        _M0Lm6offsetS365 = _M0L6_2atmpS2175 - 1;
        _M0L6_2atmpS2182 = _M0Lm1nS368;
        _M0L1qS377 = _M0L6_2atmpS2182 / _M0L4baseS370;
        _M0L6_2atmpS2180 = _M0Lm1nS368;
        _M0L6_2atmpS2181 = _M0L1qS377 * _M0L4baseS370;
        _M0L6_2atmpS2179 = _M0L6_2atmpS2180 - _M0L6_2atmpS2181;
        _M0L5digitS378 = (int32_t)_M0L6_2atmpS2179;
        _M0L6_2atmpS2178 = _M0Lm6offsetS365;
        _M0L6_2atmpS2176 = _M0L12digit__startS367 + _M0L6_2atmpS2178;
        _M0L6_2atmpS2177
        = ((moonbit_string_t)moonbit_string_literal_87.data)[
          _M0L5digitS378
        ];
        _M0L6bufferS375[_M0L6_2atmpS2176] = _M0L6_2atmpS2177;
        _M0Lm1nS368 = _M0L1qS377;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS375);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS362,
  uint64_t _M0L3numS358,
  int32_t _M0L12digit__startS356,
  int32_t _M0L10total__lenS355
) {
  int32_t _M0Lm6offsetS354;
  uint64_t _M0Lm1nS357;
  int32_t _M0L6_2atmpS2160;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS354 = _M0L10total__lenS355 - _M0L12digit__startS356;
  _M0Lm1nS357 = _M0L3numS358;
  while (1) {
    int32_t _M0L6_2atmpS2148 = _M0Lm6offsetS354;
    if (_M0L6_2atmpS2148 >= 2) {
      int32_t _M0L6_2atmpS2149 = _M0Lm6offsetS354;
      uint64_t _M0L6_2atmpS2159;
      uint64_t _M0L6_2atmpS2158;
      int32_t _M0L9byte__valS359;
      int32_t _M0L2hiS360;
      int32_t _M0L2loS361;
      int32_t _M0L6_2atmpS2152;
      int32_t _M0L6_2atmpS2150;
      int32_t _M0L6_2atmpS2151;
      int32_t _M0L6_2atmpS2156;
      int32_t _M0L6_2atmpS2155;
      int32_t _M0L6_2atmpS2153;
      int32_t _M0L6_2atmpS2154;
      uint64_t _M0L6_2atmpS2157;
      _M0Lm6offsetS354 = _M0L6_2atmpS2149 - 2;
      _M0L6_2atmpS2159 = _M0Lm1nS357;
      _M0L6_2atmpS2158 = _M0L6_2atmpS2159 & 255ull;
      _M0L9byte__valS359 = (int32_t)_M0L6_2atmpS2158;
      _M0L2hiS360 = _M0L9byte__valS359 / 16;
      _M0L2loS361 = _M0L9byte__valS359 % 16;
      _M0L6_2atmpS2152 = _M0Lm6offsetS354;
      _M0L6_2atmpS2150 = _M0L12digit__startS356 + _M0L6_2atmpS2152;
      _M0L6_2atmpS2151
      = ((moonbit_string_t)moonbit_string_literal_87.data)[
        _M0L2hiS360
      ];
      _M0L6bufferS362[_M0L6_2atmpS2150] = _M0L6_2atmpS2151;
      _M0L6_2atmpS2156 = _M0Lm6offsetS354;
      _M0L6_2atmpS2155 = _M0L12digit__startS356 + _M0L6_2atmpS2156;
      _M0L6_2atmpS2153 = _M0L6_2atmpS2155 + 1;
      _M0L6_2atmpS2154
      = ((moonbit_string_t)moonbit_string_literal_87.data)[
        _M0L2loS361
      ];
      _M0L6bufferS362[_M0L6_2atmpS2153] = _M0L6_2atmpS2154;
      _M0L6_2atmpS2157 = _M0Lm1nS357;
      _M0Lm1nS357 = _M0L6_2atmpS2157 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2160 = _M0Lm6offsetS354;
  if (_M0L6_2atmpS2160 == 1) {
    uint64_t _M0L6_2atmpS2163 = _M0Lm1nS357;
    uint64_t _M0L6_2atmpS2162 = _M0L6_2atmpS2163 & 15ull;
    int32_t _M0L6nibbleS364 = (int32_t)_M0L6_2atmpS2162;
    int32_t _M0L6_2atmpS2161 =
      ((moonbit_string_t)moonbit_string_literal_87.data)[_M0L6nibbleS364];
    _M0L6bufferS362[_M0L12digit__startS356] = _M0L6_2atmpS2161;
    moonbit_decref(_M0L6bufferS362);
  } else {
    moonbit_decref(_M0L6bufferS362);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS348,
  int32_t _M0L5radixS351
) {
  uint64_t _M0Lm3numS349;
  uint64_t _M0L4baseS350;
  int32_t _M0Lm5countS352;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS348 == 0ull) {
    return 1;
  }
  _M0Lm3numS349 = _M0L5valueS348;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS350 = _M0MPC13int3Int10to__uint64(_M0L5radixS351);
  _M0Lm5countS352 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS2145 = _M0Lm3numS349;
    if (_M0L6_2atmpS2145 > 0ull) {
      int32_t _M0L6_2atmpS2146 = _M0Lm5countS352;
      uint64_t _M0L6_2atmpS2147;
      _M0Lm5countS352 = _M0L6_2atmpS2146 + 1;
      _M0L6_2atmpS2147 = _M0Lm3numS349;
      _M0Lm3numS349 = _M0L6_2atmpS2147 / _M0L4baseS350;
      continue;
    }
    break;
  }
  return _M0Lm5countS352;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS346) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS346 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS347;
    int32_t _M0L6_2atmpS2144;
    int32_t _M0L6_2atmpS2143;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS347 = moonbit_clz64(_M0L5valueS346);
    _M0L6_2atmpS2144 = 63 - _M0L14leading__zerosS347;
    _M0L6_2atmpS2143 = _M0L6_2atmpS2144 / 4;
    return _M0L6_2atmpS2143 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS345) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS345 >= 10000000000ull) {
    if (_M0L5valueS345 >= 100000000000000ull) {
      if (_M0L5valueS345 >= 10000000000000000ull) {
        if (_M0L5valueS345 >= 1000000000000000000ull) {
          if (_M0L5valueS345 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS345 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS345 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS345 >= 1000000000000ull) {
      if (_M0L5valueS345 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS345 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS345 >= 100000ull) {
    if (_M0L5valueS345 >= 10000000ull) {
      if (_M0L5valueS345 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS345 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS345 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS345 >= 1000ull) {
    if (_M0L5valueS345 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS345 >= 100ull) {
    return 3;
  } else if (_M0L5valueS345 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS329,
  int32_t _M0L5radixS328
) {
  int32_t _if__result_4676;
  int32_t _M0L12is__negativeS330;
  uint32_t _M0L3numS331;
  uint16_t* _M0L6bufferS332;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS328 < 2) {
    _if__result_4676 = 1;
  } else {
    _if__result_4676 = _M0L5radixS328 > 36;
  }
  if (_if__result_4676) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_85.data, (moonbit_string_t)moonbit_string_literal_88.data);
  }
  if (_M0L4selfS329 == 0) {
    return (moonbit_string_t)moonbit_string_literal_66.data;
  }
  _M0L12is__negativeS330 = _M0L4selfS329 < 0;
  if (_M0L12is__negativeS330) {
    int32_t _M0L6_2atmpS2142 = -_M0L4selfS329;
    _M0L3numS331 = *(uint32_t*)&_M0L6_2atmpS2142;
  } else {
    _M0L3numS331 = *(uint32_t*)&_M0L4selfS329;
  }
  switch (_M0L5radixS328) {
    case 10: {
      int32_t _M0L10digit__lenS333;
      int32_t _M0L6_2atmpS2139;
      int32_t _M0L10total__lenS334;
      uint16_t* _M0L6bufferS335;
      int32_t _M0L12digit__startS336;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS333 = _M0FPB12dec__count32(_M0L3numS331);
      if (_M0L12is__negativeS330) {
        _M0L6_2atmpS2139 = 1;
      } else {
        _M0L6_2atmpS2139 = 0;
      }
      _M0L10total__lenS334 = _M0L10digit__lenS333 + _M0L6_2atmpS2139;
      _M0L6bufferS335
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS334, 0);
      if (_M0L12is__negativeS330) {
        _M0L12digit__startS336 = 1;
      } else {
        _M0L12digit__startS336 = 0;
      }
      moonbit_incref(_M0L6bufferS335);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS335, _M0L3numS331, _M0L12digit__startS336, _M0L10total__lenS334);
      _M0L6bufferS332 = _M0L6bufferS335;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS337;
      int32_t _M0L6_2atmpS2140;
      int32_t _M0L10total__lenS338;
      uint16_t* _M0L6bufferS339;
      int32_t _M0L12digit__startS340;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS337 = _M0FPB12hex__count32(_M0L3numS331);
      if (_M0L12is__negativeS330) {
        _M0L6_2atmpS2140 = 1;
      } else {
        _M0L6_2atmpS2140 = 0;
      }
      _M0L10total__lenS338 = _M0L10digit__lenS337 + _M0L6_2atmpS2140;
      _M0L6bufferS339
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS338, 0);
      if (_M0L12is__negativeS330) {
        _M0L12digit__startS340 = 1;
      } else {
        _M0L12digit__startS340 = 0;
      }
      moonbit_incref(_M0L6bufferS339);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS339, _M0L3numS331, _M0L12digit__startS340, _M0L10total__lenS338);
      _M0L6bufferS332 = _M0L6bufferS339;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS341;
      int32_t _M0L6_2atmpS2141;
      int32_t _M0L10total__lenS342;
      uint16_t* _M0L6bufferS343;
      int32_t _M0L12digit__startS344;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS341
      = _M0FPB14radix__count32(_M0L3numS331, _M0L5radixS328);
      if (_M0L12is__negativeS330) {
        _M0L6_2atmpS2141 = 1;
      } else {
        _M0L6_2atmpS2141 = 0;
      }
      _M0L10total__lenS342 = _M0L10digit__lenS341 + _M0L6_2atmpS2141;
      _M0L6bufferS343
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS342, 0);
      if (_M0L12is__negativeS330) {
        _M0L12digit__startS344 = 1;
      } else {
        _M0L12digit__startS344 = 0;
      }
      moonbit_incref(_M0L6bufferS343);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS343, _M0L3numS331, _M0L12digit__startS344, _M0L10total__lenS342, _M0L5radixS328);
      _M0L6bufferS332 = _M0L6bufferS343;
      break;
    }
  }
  if (_M0L12is__negativeS330) {
    _M0L6bufferS332[0] = 45;
  }
  return _M0L6bufferS332;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS322,
  int32_t _M0L5radixS325
) {
  uint32_t _M0Lm3numS323;
  uint32_t _M0L4baseS324;
  int32_t _M0Lm5countS326;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS322 == 0u) {
    return 1;
  }
  _M0Lm3numS323 = _M0L5valueS322;
  _M0L4baseS324 = *(uint32_t*)&_M0L5radixS325;
  _M0Lm5countS326 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS2136 = _M0Lm3numS323;
    if (_M0L6_2atmpS2136 > 0u) {
      int32_t _M0L6_2atmpS2137 = _M0Lm5countS326;
      uint32_t _M0L6_2atmpS2138;
      _M0Lm5countS326 = _M0L6_2atmpS2137 + 1;
      _M0L6_2atmpS2138 = _M0Lm3numS323;
      _M0Lm3numS323 = _M0L6_2atmpS2138 / _M0L4baseS324;
      continue;
    }
    break;
  }
  return _M0Lm5countS326;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS320) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS320 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS321;
    int32_t _M0L6_2atmpS2135;
    int32_t _M0L6_2atmpS2134;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS321 = moonbit_clz32(_M0L5valueS320);
    _M0L6_2atmpS2135 = 31 - _M0L14leading__zerosS321;
    _M0L6_2atmpS2134 = _M0L6_2atmpS2135 / 4;
    return _M0L6_2atmpS2134 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS319) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS319 >= 100000u) {
    if (_M0L5valueS319 >= 10000000u) {
      if (_M0L5valueS319 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS319 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS319 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS319 >= 1000u) {
    if (_M0L5valueS319 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS319 >= 100u) {
    return 3;
  } else if (_M0L5valueS319 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS309,
  uint32_t _M0L3numS297,
  int32_t _M0L12digit__startS300,
  int32_t _M0L10total__lenS299
) {
  uint32_t _M0Lm3numS296;
  int32_t _M0Lm6offsetS298;
  uint32_t _M0L6_2atmpS2133;
  int32_t _M0Lm9remainingS311;
  int32_t _M0L6_2atmpS2114;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS296 = _M0L3numS297;
  _M0Lm6offsetS298 = _M0L10total__lenS299 - _M0L12digit__startS300;
  while (1) {
    uint32_t _M0L6_2atmpS2077 = _M0Lm3numS296;
    if (_M0L6_2atmpS2077 >= 10000u) {
      uint32_t _M0L6_2atmpS2100 = _M0Lm3numS296;
      uint32_t _M0L1tS301 = _M0L6_2atmpS2100 / 10000u;
      uint32_t _M0L6_2atmpS2099 = _M0Lm3numS296;
      uint32_t _M0L6_2atmpS2098 = _M0L6_2atmpS2099 % 10000u;
      int32_t _M0L1rS302 = *(int32_t*)&_M0L6_2atmpS2098;
      int32_t _M0L2d1S303;
      int32_t _M0L2d2S304;
      int32_t _M0L6_2atmpS2078;
      int32_t _M0L6_2atmpS2097;
      int32_t _M0L6_2atmpS2096;
      int32_t _M0L6d1__hiS305;
      int32_t _M0L6_2atmpS2095;
      int32_t _M0L6_2atmpS2094;
      int32_t _M0L6d1__loS306;
      int32_t _M0L6_2atmpS2093;
      int32_t _M0L6_2atmpS2092;
      int32_t _M0L6d2__hiS307;
      int32_t _M0L6_2atmpS2091;
      int32_t _M0L6_2atmpS2090;
      int32_t _M0L6d2__loS308;
      int32_t _M0L6_2atmpS2080;
      int32_t _M0L6_2atmpS2079;
      int32_t _M0L6_2atmpS2083;
      int32_t _M0L6_2atmpS2082;
      int32_t _M0L6_2atmpS2081;
      int32_t _M0L6_2atmpS2086;
      int32_t _M0L6_2atmpS2085;
      int32_t _M0L6_2atmpS2084;
      int32_t _M0L6_2atmpS2089;
      int32_t _M0L6_2atmpS2088;
      int32_t _M0L6_2atmpS2087;
      _M0Lm3numS296 = _M0L1tS301;
      _M0L2d1S303 = _M0L1rS302 / 100;
      _M0L2d2S304 = _M0L1rS302 % 100;
      _M0L6_2atmpS2078 = _M0Lm6offsetS298;
      _M0Lm6offsetS298 = _M0L6_2atmpS2078 - 4;
      _M0L6_2atmpS2097 = _M0L2d1S303 / 10;
      _M0L6_2atmpS2096 = 48 + _M0L6_2atmpS2097;
      _M0L6d1__hiS305 = (uint16_t)_M0L6_2atmpS2096;
      _M0L6_2atmpS2095 = _M0L2d1S303 % 10;
      _M0L6_2atmpS2094 = 48 + _M0L6_2atmpS2095;
      _M0L6d1__loS306 = (uint16_t)_M0L6_2atmpS2094;
      _M0L6_2atmpS2093 = _M0L2d2S304 / 10;
      _M0L6_2atmpS2092 = 48 + _M0L6_2atmpS2093;
      _M0L6d2__hiS307 = (uint16_t)_M0L6_2atmpS2092;
      _M0L6_2atmpS2091 = _M0L2d2S304 % 10;
      _M0L6_2atmpS2090 = 48 + _M0L6_2atmpS2091;
      _M0L6d2__loS308 = (uint16_t)_M0L6_2atmpS2090;
      _M0L6_2atmpS2080 = _M0Lm6offsetS298;
      _M0L6_2atmpS2079 = _M0L12digit__startS300 + _M0L6_2atmpS2080;
      _M0L6bufferS309[_M0L6_2atmpS2079] = _M0L6d1__hiS305;
      _M0L6_2atmpS2083 = _M0Lm6offsetS298;
      _M0L6_2atmpS2082 = _M0L12digit__startS300 + _M0L6_2atmpS2083;
      _M0L6_2atmpS2081 = _M0L6_2atmpS2082 + 1;
      _M0L6bufferS309[_M0L6_2atmpS2081] = _M0L6d1__loS306;
      _M0L6_2atmpS2086 = _M0Lm6offsetS298;
      _M0L6_2atmpS2085 = _M0L12digit__startS300 + _M0L6_2atmpS2086;
      _M0L6_2atmpS2084 = _M0L6_2atmpS2085 + 2;
      _M0L6bufferS309[_M0L6_2atmpS2084] = _M0L6d2__hiS307;
      _M0L6_2atmpS2089 = _M0Lm6offsetS298;
      _M0L6_2atmpS2088 = _M0L12digit__startS300 + _M0L6_2atmpS2089;
      _M0L6_2atmpS2087 = _M0L6_2atmpS2088 + 3;
      _M0L6bufferS309[_M0L6_2atmpS2087] = _M0L6d2__loS308;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2133 = _M0Lm3numS296;
  _M0Lm9remainingS311 = *(int32_t*)&_M0L6_2atmpS2133;
  while (1) {
    int32_t _M0L6_2atmpS2101 = _M0Lm9remainingS311;
    if (_M0L6_2atmpS2101 >= 100) {
      int32_t _M0L6_2atmpS2113 = _M0Lm9remainingS311;
      int32_t _M0L1tS312 = _M0L6_2atmpS2113 / 100;
      int32_t _M0L6_2atmpS2112 = _M0Lm9remainingS311;
      int32_t _M0L1dS313 = _M0L6_2atmpS2112 % 100;
      int32_t _M0L6_2atmpS2102;
      int32_t _M0L6_2atmpS2111;
      int32_t _M0L6_2atmpS2110;
      int32_t _M0L5d__hiS314;
      int32_t _M0L6_2atmpS2109;
      int32_t _M0L6_2atmpS2108;
      int32_t _M0L5d__loS315;
      int32_t _M0L6_2atmpS2104;
      int32_t _M0L6_2atmpS2103;
      int32_t _M0L6_2atmpS2107;
      int32_t _M0L6_2atmpS2106;
      int32_t _M0L6_2atmpS2105;
      _M0Lm9remainingS311 = _M0L1tS312;
      _M0L6_2atmpS2102 = _M0Lm6offsetS298;
      _M0Lm6offsetS298 = _M0L6_2atmpS2102 - 2;
      _M0L6_2atmpS2111 = _M0L1dS313 / 10;
      _M0L6_2atmpS2110 = 48 + _M0L6_2atmpS2111;
      _M0L5d__hiS314 = (uint16_t)_M0L6_2atmpS2110;
      _M0L6_2atmpS2109 = _M0L1dS313 % 10;
      _M0L6_2atmpS2108 = 48 + _M0L6_2atmpS2109;
      _M0L5d__loS315 = (uint16_t)_M0L6_2atmpS2108;
      _M0L6_2atmpS2104 = _M0Lm6offsetS298;
      _M0L6_2atmpS2103 = _M0L12digit__startS300 + _M0L6_2atmpS2104;
      _M0L6bufferS309[_M0L6_2atmpS2103] = _M0L5d__hiS314;
      _M0L6_2atmpS2107 = _M0Lm6offsetS298;
      _M0L6_2atmpS2106 = _M0L12digit__startS300 + _M0L6_2atmpS2107;
      _M0L6_2atmpS2105 = _M0L6_2atmpS2106 + 1;
      _M0L6bufferS309[_M0L6_2atmpS2105] = _M0L5d__loS315;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2114 = _M0Lm9remainingS311;
  if (_M0L6_2atmpS2114 >= 10) {
    int32_t _M0L6_2atmpS2115 = _M0Lm6offsetS298;
    int32_t _M0L6_2atmpS2126;
    int32_t _M0L6_2atmpS2125;
    int32_t _M0L6_2atmpS2124;
    int32_t _M0L5d__hiS317;
    int32_t _M0L6_2atmpS2123;
    int32_t _M0L6_2atmpS2122;
    int32_t _M0L6_2atmpS2121;
    int32_t _M0L5d__loS318;
    int32_t _M0L6_2atmpS2117;
    int32_t _M0L6_2atmpS2116;
    int32_t _M0L6_2atmpS2120;
    int32_t _M0L6_2atmpS2119;
    int32_t _M0L6_2atmpS2118;
    _M0Lm6offsetS298 = _M0L6_2atmpS2115 - 2;
    _M0L6_2atmpS2126 = _M0Lm9remainingS311;
    _M0L6_2atmpS2125 = _M0L6_2atmpS2126 / 10;
    _M0L6_2atmpS2124 = 48 + _M0L6_2atmpS2125;
    _M0L5d__hiS317 = (uint16_t)_M0L6_2atmpS2124;
    _M0L6_2atmpS2123 = _M0Lm9remainingS311;
    _M0L6_2atmpS2122 = _M0L6_2atmpS2123 % 10;
    _M0L6_2atmpS2121 = 48 + _M0L6_2atmpS2122;
    _M0L5d__loS318 = (uint16_t)_M0L6_2atmpS2121;
    _M0L6_2atmpS2117 = _M0Lm6offsetS298;
    _M0L6_2atmpS2116 = _M0L12digit__startS300 + _M0L6_2atmpS2117;
    _M0L6bufferS309[_M0L6_2atmpS2116] = _M0L5d__hiS317;
    _M0L6_2atmpS2120 = _M0Lm6offsetS298;
    _M0L6_2atmpS2119 = _M0L12digit__startS300 + _M0L6_2atmpS2120;
    _M0L6_2atmpS2118 = _M0L6_2atmpS2119 + 1;
    _M0L6bufferS309[_M0L6_2atmpS2118] = _M0L5d__loS318;
    moonbit_decref(_M0L6bufferS309);
  } else {
    int32_t _M0L6_2atmpS2127 = _M0Lm6offsetS298;
    int32_t _M0L6_2atmpS2132;
    int32_t _M0L6_2atmpS2128;
    int32_t _M0L6_2atmpS2131;
    int32_t _M0L6_2atmpS2130;
    int32_t _M0L6_2atmpS2129;
    _M0Lm6offsetS298 = _M0L6_2atmpS2127 - 1;
    _M0L6_2atmpS2132 = _M0Lm6offsetS298;
    _M0L6_2atmpS2128 = _M0L12digit__startS300 + _M0L6_2atmpS2132;
    _M0L6_2atmpS2131 = _M0Lm9remainingS311;
    _M0L6_2atmpS2130 = 48 + _M0L6_2atmpS2131;
    _M0L6_2atmpS2129 = (uint16_t)_M0L6_2atmpS2130;
    _M0L6bufferS309[_M0L6_2atmpS2128] = _M0L6_2atmpS2129;
    moonbit_decref(_M0L6bufferS309);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS291,
  uint32_t _M0L3numS285,
  int32_t _M0L12digit__startS283,
  int32_t _M0L10total__lenS282,
  int32_t _M0L5radixS287
) {
  int32_t _M0Lm6offsetS281;
  uint32_t _M0Lm1nS284;
  uint32_t _M0L4baseS286;
  int32_t _M0L6_2atmpS2059;
  int32_t _M0L6_2atmpS2058;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS281 = _M0L10total__lenS282 - _M0L12digit__startS283;
  _M0Lm1nS284 = _M0L3numS285;
  _M0L4baseS286 = *(uint32_t*)&_M0L5radixS287;
  _M0L6_2atmpS2059 = _M0L5radixS287 - 1;
  _M0L6_2atmpS2058 = _M0L5radixS287 & _M0L6_2atmpS2059;
  if (_M0L6_2atmpS2058 == 0) {
    int32_t _M0L5shiftS288;
    uint32_t _M0L4maskS289;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS288 = moonbit_ctz32(_M0L5radixS287);
    _M0L4maskS289 = _M0L4baseS286 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS2060 = _M0Lm1nS284;
      if (_M0L6_2atmpS2060 > 0u) {
        int32_t _M0L6_2atmpS2061 = _M0Lm6offsetS281;
        uint32_t _M0L6_2atmpS2067;
        uint32_t _M0L6_2atmpS2066;
        int32_t _M0L5digitS290;
        int32_t _M0L6_2atmpS2064;
        int32_t _M0L6_2atmpS2062;
        int32_t _M0L6_2atmpS2063;
        uint32_t _M0L6_2atmpS2065;
        _M0Lm6offsetS281 = _M0L6_2atmpS2061 - 1;
        _M0L6_2atmpS2067 = _M0Lm1nS284;
        _M0L6_2atmpS2066 = _M0L6_2atmpS2067 & _M0L4maskS289;
        _M0L5digitS290 = *(int32_t*)&_M0L6_2atmpS2066;
        _M0L6_2atmpS2064 = _M0Lm6offsetS281;
        _M0L6_2atmpS2062 = _M0L12digit__startS283 + _M0L6_2atmpS2064;
        _M0L6_2atmpS2063
        = ((moonbit_string_t)moonbit_string_literal_87.data)[
          _M0L5digitS290
        ];
        _M0L6bufferS291[_M0L6_2atmpS2062] = _M0L6_2atmpS2063;
        _M0L6_2atmpS2065 = _M0Lm1nS284;
        _M0Lm1nS284 = _M0L6_2atmpS2065 >> (_M0L5shiftS288 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS291);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS2068 = _M0Lm1nS284;
      if (_M0L6_2atmpS2068 > 0u) {
        int32_t _M0L6_2atmpS2069 = _M0Lm6offsetS281;
        uint32_t _M0L6_2atmpS2076;
        uint32_t _M0L1qS293;
        uint32_t _M0L6_2atmpS2074;
        uint32_t _M0L6_2atmpS2075;
        uint32_t _M0L6_2atmpS2073;
        int32_t _M0L5digitS294;
        int32_t _M0L6_2atmpS2072;
        int32_t _M0L6_2atmpS2070;
        int32_t _M0L6_2atmpS2071;
        _M0Lm6offsetS281 = _M0L6_2atmpS2069 - 1;
        _M0L6_2atmpS2076 = _M0Lm1nS284;
        _M0L1qS293 = _M0L6_2atmpS2076 / _M0L4baseS286;
        _M0L6_2atmpS2074 = _M0Lm1nS284;
        _M0L6_2atmpS2075 = _M0L1qS293 * _M0L4baseS286;
        _M0L6_2atmpS2073 = _M0L6_2atmpS2074 - _M0L6_2atmpS2075;
        _M0L5digitS294 = *(int32_t*)&_M0L6_2atmpS2073;
        _M0L6_2atmpS2072 = _M0Lm6offsetS281;
        _M0L6_2atmpS2070 = _M0L12digit__startS283 + _M0L6_2atmpS2072;
        _M0L6_2atmpS2071
        = ((moonbit_string_t)moonbit_string_literal_87.data)[
          _M0L5digitS294
        ];
        _M0L6bufferS291[_M0L6_2atmpS2070] = _M0L6_2atmpS2071;
        _M0Lm1nS284 = _M0L1qS293;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS291);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS278,
  uint32_t _M0L3numS274,
  int32_t _M0L12digit__startS272,
  int32_t _M0L10total__lenS271
) {
  int32_t _M0Lm6offsetS270;
  uint32_t _M0Lm1nS273;
  int32_t _M0L6_2atmpS2054;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS270 = _M0L10total__lenS271 - _M0L12digit__startS272;
  _M0Lm1nS273 = _M0L3numS274;
  while (1) {
    int32_t _M0L6_2atmpS2042 = _M0Lm6offsetS270;
    if (_M0L6_2atmpS2042 >= 2) {
      int32_t _M0L6_2atmpS2043 = _M0Lm6offsetS270;
      uint32_t _M0L6_2atmpS2053;
      uint32_t _M0L6_2atmpS2052;
      int32_t _M0L9byte__valS275;
      int32_t _M0L2hiS276;
      int32_t _M0L2loS277;
      int32_t _M0L6_2atmpS2046;
      int32_t _M0L6_2atmpS2044;
      int32_t _M0L6_2atmpS2045;
      int32_t _M0L6_2atmpS2050;
      int32_t _M0L6_2atmpS2049;
      int32_t _M0L6_2atmpS2047;
      int32_t _M0L6_2atmpS2048;
      uint32_t _M0L6_2atmpS2051;
      _M0Lm6offsetS270 = _M0L6_2atmpS2043 - 2;
      _M0L6_2atmpS2053 = _M0Lm1nS273;
      _M0L6_2atmpS2052 = _M0L6_2atmpS2053 & 255u;
      _M0L9byte__valS275 = *(int32_t*)&_M0L6_2atmpS2052;
      _M0L2hiS276 = _M0L9byte__valS275 / 16;
      _M0L2loS277 = _M0L9byte__valS275 % 16;
      _M0L6_2atmpS2046 = _M0Lm6offsetS270;
      _M0L6_2atmpS2044 = _M0L12digit__startS272 + _M0L6_2atmpS2046;
      _M0L6_2atmpS2045
      = ((moonbit_string_t)moonbit_string_literal_87.data)[
        _M0L2hiS276
      ];
      _M0L6bufferS278[_M0L6_2atmpS2044] = _M0L6_2atmpS2045;
      _M0L6_2atmpS2050 = _M0Lm6offsetS270;
      _M0L6_2atmpS2049 = _M0L12digit__startS272 + _M0L6_2atmpS2050;
      _M0L6_2atmpS2047 = _M0L6_2atmpS2049 + 1;
      _M0L6_2atmpS2048
      = ((moonbit_string_t)moonbit_string_literal_87.data)[
        _M0L2loS277
      ];
      _M0L6bufferS278[_M0L6_2atmpS2047] = _M0L6_2atmpS2048;
      _M0L6_2atmpS2051 = _M0Lm1nS273;
      _M0Lm1nS273 = _M0L6_2atmpS2051 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2054 = _M0Lm6offsetS270;
  if (_M0L6_2atmpS2054 == 1) {
    uint32_t _M0L6_2atmpS2057 = _M0Lm1nS273;
    uint32_t _M0L6_2atmpS2056 = _M0L6_2atmpS2057 & 15u;
    int32_t _M0L6nibbleS280 = *(int32_t*)&_M0L6_2atmpS2056;
    int32_t _M0L6_2atmpS2055 =
      ((moonbit_string_t)moonbit_string_literal_87.data)[_M0L6nibbleS280];
    _M0L6bufferS278[_M0L12digit__startS272] = _M0L6_2atmpS2055;
    moonbit_decref(_M0L6bufferS278);
  } else {
    moonbit_decref(_M0L6bufferS278);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS259) {
  struct _M0TWEOs* _M0L7_2afuncS258;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS258 = _M0L4selfS259;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS258->code(_M0L7_2afuncS258);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS261
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS260;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS260 = _M0L4selfS261;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS260->code(_M0L7_2afuncS260);
}

struct _M0TUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0MPB4Iter4nextGUsRP48clawteam8clawteam8internal6schema6SchemaEE(
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L4selfS263
) {
  struct _M0TWEOUsRP48clawteam8clawteam8internal6schema6SchemaE* _M0L7_2afuncS262;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS262 = _M0L4selfS263;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS262->code(_M0L7_2afuncS262);
}

void* _M0MPB4Iter4nextGRPB4JsonE(struct _M0TWEORPB4Json* _M0L4selfS265) {
  struct _M0TWEORPB4Json* _M0L7_2afuncS264;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS264 = _M0L4selfS265;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS264->code(_M0L7_2afuncS264);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS267) {
  struct _M0TWEOc* _M0L7_2afuncS266;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS266 = _M0L4selfS267;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS266->code(_M0L7_2afuncS266);
}

struct _M0TUsRPB3MapGsRPB4JsonEE* _M0MPB4Iter4nextGUsRPB3MapGsRPB4JsonEEE(
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L4selfS269
) {
  struct _M0TWEOUsRPB3MapGsRPB4JsonEE* _M0L7_2afuncS268;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS268 = _M0L4selfS269;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS268->code(_M0L7_2afuncS268);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS249
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS248;
  struct _M0TPB6Logger _M0L6_2atmpS2037;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS248 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS248);
  _M0L6_2atmpS2037
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS248
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS249, _M0L6_2atmpS2037);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS248);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS251
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS250;
  struct _M0TPB6Logger _M0L6_2atmpS2038;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS250 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS250);
  _M0L6_2atmpS2038
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS250
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS251, _M0L6_2atmpS2038);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS250);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(
  int32_t _M0L4selfS253
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS252;
  struct _M0TPB6Logger _M0L6_2atmpS2039;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS252 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS252);
  _M0L6_2atmpS2039
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS252
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14bool4BoolPB4Show6output(_M0L4selfS253, _M0L6_2atmpS2039);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS252);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS255
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS254;
  struct _M0TPB6Logger _M0L6_2atmpS2040;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS254 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS254);
  _M0L6_2atmpS2040
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS254
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS255, _M0L6_2atmpS2040);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS254);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS257
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS256;
  struct _M0TPB6Logger _M0L6_2atmpS2041;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS256 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS256);
  _M0L6_2atmpS2041
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS256
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS257, _M0L6_2atmpS2041);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS256);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS247
) {
  int32_t _M0L8_2afieldS4230;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4230 = _M0L4selfS247.$1;
  moonbit_decref(_M0L4selfS247.$0);
  return _M0L8_2afieldS4230;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS246
) {
  int32_t _M0L3endS2035;
  int32_t _M0L8_2afieldS4231;
  int32_t _M0L5startS2036;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS2035 = _M0L4selfS246.$2;
  _M0L8_2afieldS4231 = _M0L4selfS246.$1;
  moonbit_decref(_M0L4selfS246.$0);
  _M0L5startS2036 = _M0L8_2afieldS4231;
  return _M0L3endS2035 - _M0L5startS2036;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS245
) {
  moonbit_string_t _M0L8_2afieldS4232;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4232 = _M0L4selfS245.$0;
  return _M0L8_2afieldS4232;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS241,
  moonbit_string_t _M0L5valueS242,
  int32_t _M0L5startS243,
  int32_t _M0L3lenS244
) {
  int32_t _M0L6_2atmpS2034;
  int64_t _M0L6_2atmpS2033;
  struct _M0TPC16string10StringView _M0L6_2atmpS2032;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2034 = _M0L5startS243 + _M0L3lenS244;
  _M0L6_2atmpS2033 = (int64_t)_M0L6_2atmpS2034;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2032
  = _M0MPC16string6String11sub_2einner(_M0L5valueS242, _M0L5startS243, _M0L6_2atmpS2033);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS241, _M0L6_2atmpS2032);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS234,
  int32_t _M0L5startS240,
  int64_t _M0L3endS236
) {
  int32_t _M0L3lenS233;
  int32_t _M0L3endS235;
  int32_t _M0L5startS239;
  int32_t _if__result_4683;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS233 = Moonbit_array_length(_M0L4selfS234);
  if (_M0L3endS236 == 4294967296ll) {
    _M0L3endS235 = _M0L3lenS233;
  } else {
    int64_t _M0L7_2aSomeS237 = _M0L3endS236;
    int32_t _M0L6_2aendS238 = (int32_t)_M0L7_2aSomeS237;
    if (_M0L6_2aendS238 < 0) {
      _M0L3endS235 = _M0L3lenS233 + _M0L6_2aendS238;
    } else {
      _M0L3endS235 = _M0L6_2aendS238;
    }
  }
  if (_M0L5startS240 < 0) {
    _M0L5startS239 = _M0L3lenS233 + _M0L5startS240;
  } else {
    _M0L5startS239 = _M0L5startS240;
  }
  if (_M0L5startS239 >= 0) {
    if (_M0L5startS239 <= _M0L3endS235) {
      _if__result_4683 = _M0L3endS235 <= _M0L3lenS233;
    } else {
      _if__result_4683 = 0;
    }
  } else {
    _if__result_4683 = 0;
  }
  if (_if__result_4683) {
    if (_M0L5startS239 < _M0L3lenS233) {
      int32_t _M0L6_2atmpS2029 = _M0L4selfS234[_M0L5startS239];
      int32_t _M0L6_2atmpS2028;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2028
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2029);
      if (!_M0L6_2atmpS2028) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS235 < _M0L3lenS233) {
      int32_t _M0L6_2atmpS2031 = _M0L4selfS234[_M0L3endS235];
      int32_t _M0L6_2atmpS2030;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2030
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2031);
      if (!_M0L6_2atmpS2030) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS239,
                                                 _M0L3endS235,
                                                 _M0L4selfS234};
  } else {
    moonbit_decref(_M0L4selfS234);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS230) {
  struct _M0TPB6Hasher* _M0L1hS229;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS229 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS229);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS229, _M0L4selfS230);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS229);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS232
) {
  struct _M0TPB6Hasher* _M0L1hS231;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS231 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS231);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS231, _M0L4selfS232);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS231);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS227) {
  int32_t _M0L4seedS226;
  if (_M0L10seed_2eoptS227 == 4294967296ll) {
    _M0L4seedS226 = 0;
  } else {
    int64_t _M0L7_2aSomeS228 = _M0L10seed_2eoptS227;
    _M0L4seedS226 = (int32_t)_M0L7_2aSomeS228;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS226);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS225) {
  uint32_t _M0L6_2atmpS2027;
  uint32_t _M0L6_2atmpS2026;
  struct _M0TPB6Hasher* _block_4684;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2027 = *(uint32_t*)&_M0L4seedS225;
  _M0L6_2atmpS2026 = _M0L6_2atmpS2027 + 374761393u;
  _block_4684
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4684)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4684->$0 = _M0L6_2atmpS2026;
  return _block_4684;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS224) {
  uint32_t _M0L6_2atmpS2025;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2025 = _M0MPB6Hasher9avalanche(_M0L4selfS224);
  return *(int32_t*)&_M0L6_2atmpS2025;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS223) {
  uint32_t _M0L8_2afieldS4233;
  uint32_t _M0Lm3accS222;
  uint32_t _M0L6_2atmpS2014;
  uint32_t _M0L6_2atmpS2016;
  uint32_t _M0L6_2atmpS2015;
  uint32_t _M0L6_2atmpS2017;
  uint32_t _M0L6_2atmpS2018;
  uint32_t _M0L6_2atmpS2020;
  uint32_t _M0L6_2atmpS2019;
  uint32_t _M0L6_2atmpS2021;
  uint32_t _M0L6_2atmpS2022;
  uint32_t _M0L6_2atmpS2024;
  uint32_t _M0L6_2atmpS2023;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4233 = _M0L4selfS223->$0;
  moonbit_decref(_M0L4selfS223);
  _M0Lm3accS222 = _M0L8_2afieldS4233;
  _M0L6_2atmpS2014 = _M0Lm3accS222;
  _M0L6_2atmpS2016 = _M0Lm3accS222;
  _M0L6_2atmpS2015 = _M0L6_2atmpS2016 >> 15;
  _M0Lm3accS222 = _M0L6_2atmpS2014 ^ _M0L6_2atmpS2015;
  _M0L6_2atmpS2017 = _M0Lm3accS222;
  _M0Lm3accS222 = _M0L6_2atmpS2017 * 2246822519u;
  _M0L6_2atmpS2018 = _M0Lm3accS222;
  _M0L6_2atmpS2020 = _M0Lm3accS222;
  _M0L6_2atmpS2019 = _M0L6_2atmpS2020 >> 13;
  _M0Lm3accS222 = _M0L6_2atmpS2018 ^ _M0L6_2atmpS2019;
  _M0L6_2atmpS2021 = _M0Lm3accS222;
  _M0Lm3accS222 = _M0L6_2atmpS2021 * 3266489917u;
  _M0L6_2atmpS2022 = _M0Lm3accS222;
  _M0L6_2atmpS2024 = _M0Lm3accS222;
  _M0L6_2atmpS2023 = _M0L6_2atmpS2024 >> 16;
  _M0Lm3accS222 = _M0L6_2atmpS2022 ^ _M0L6_2atmpS2023;
  return _M0Lm3accS222;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS218,
  moonbit_string_t _M0L1yS219
) {
  int32_t _M0L6_2atmpS4234;
  int32_t _M0L6_2atmpS2012;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4234 = moonbit_val_array_equal(_M0L1xS218, _M0L1yS219);
  moonbit_decref(_M0L1xS218);
  moonbit_decref(_M0L1yS219);
  _M0L6_2atmpS2012 = _M0L6_2atmpS4234;
  return !_M0L6_2atmpS2012;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGORPC111sorted__set4NodeGsEE(
  struct _M0TPC111sorted__set4NodeGsE* _M0L1xS220,
  struct _M0TPC111sorted__set4NodeGsE* _M0L1yS221
) {
  int32_t _M0L6_2atmpS2013;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2013
  = _M0IPC16option6OptionPB2Eq5equalGRPC111sorted__set4NodeGsEE(_M0L1xS220, _M0L1yS221);
  return !_M0L6_2atmpS2013;
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
  int64_t _M0L6_2atmpS2011;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2011 = (int64_t)_M0L4selfS213;
  return *(uint64_t*)&_M0L6_2atmpS2011;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS211,
  moonbit_string_t _M0L4reprS212
) {
  void* _block_4685;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4685 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_4685)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_4685)->$0 = _M0L6numberS211;
  ((struct _M0DTPB4Json6Number*)_block_4685)->$1 = _M0L4reprS212;
  return _block_4685;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS209,
  int32_t _M0L5valueS210
) {
  uint32_t _M0L6_2atmpS2010;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2010 = *(uint32_t*)&_M0L5valueS210;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS209, _M0L6_2atmpS2010);
  return 0;
}

int32_t _M0IPC16uint166UInt16PB7Compare7compare(
  int32_t _M0L4selfS207,
  int32_t _M0L4thatS208
) {
  int32_t _M0L6_2atmpS2008;
  int32_t _M0L6_2atmpS2009;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS2008 = (int32_t)_M0L4selfS207;
  _M0L6_2atmpS2009 = (int32_t)_M0L4thatS208;
  return (_M0L6_2atmpS2008 >= _M0L6_2atmpS2009)
         - (_M0L6_2atmpS2008 <= _M0L6_2atmpS2009);
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS200
) {
  struct _M0TPB13StringBuilder* _M0L3bufS198;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS199;
  int32_t _M0L7_2abindS201;
  int32_t _M0L1iS202;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS198 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS199 = _M0L4selfS200;
  moonbit_incref(_M0L3bufS198);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS198, 91);
  _M0L7_2abindS201 = _M0L7_2aselfS199->$1;
  _M0L1iS202 = 0;
  while (1) {
    if (_M0L1iS202 < _M0L7_2abindS201) {
      int32_t _if__result_4687;
      moonbit_string_t* _M0L8_2afieldS4236;
      moonbit_string_t* _M0L3bufS2006;
      moonbit_string_t _M0L6_2atmpS4235;
      moonbit_string_t _M0L4itemS203;
      int32_t _M0L6_2atmpS2007;
      if (_M0L1iS202 != 0) {
        moonbit_incref(_M0L3bufS198);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS198, (moonbit_string_t)moonbit_string_literal_89.data);
      }
      if (_M0L1iS202 < 0) {
        _if__result_4687 = 1;
      } else {
        int32_t _M0L3lenS2005 = _M0L7_2aselfS199->$1;
        _if__result_4687 = _M0L1iS202 >= _M0L3lenS2005;
      }
      if (_if__result_4687) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4236 = _M0L7_2aselfS199->$0;
      _M0L3bufS2006 = _M0L8_2afieldS4236;
      _M0L6_2atmpS4235 = (moonbit_string_t)_M0L3bufS2006[_M0L1iS202];
      _M0L4itemS203 = _M0L6_2atmpS4235;
      if (_M0L4itemS203 == 0) {
        moonbit_incref(_M0L3bufS198);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS198, (moonbit_string_t)moonbit_string_literal_46.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS204 = _M0L4itemS203;
        moonbit_string_t _M0L6_2alocS205 = _M0L7_2aSomeS204;
        moonbit_string_t _M0L6_2atmpS2004;
        moonbit_incref(_M0L6_2alocS205);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS2004
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS205);
        moonbit_incref(_M0L3bufS198);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS198, _M0L6_2atmpS2004);
      }
      _M0L6_2atmpS2007 = _M0L1iS202 + 1;
      _M0L1iS202 = _M0L6_2atmpS2007;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS199);
    }
    break;
  }
  moonbit_incref(_M0L3bufS198);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS198, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS198);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS197
) {
  moonbit_string_t _M0L6_2atmpS2003;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2002;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2003 = _M0L4selfS197;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2002 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2003);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS2002);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS196
) {
  struct _M0TPB13StringBuilder* _M0L2sbS195;
  struct _M0TPC16string10StringView _M0L8_2afieldS4249;
  struct _M0TPC16string10StringView _M0L3pkgS1987;
  moonbit_string_t _M0L6_2atmpS1986;
  moonbit_string_t _M0L6_2atmpS4248;
  moonbit_string_t _M0L6_2atmpS1985;
  moonbit_string_t _M0L6_2atmpS4247;
  moonbit_string_t _M0L6_2atmpS1984;
  struct _M0TPC16string10StringView _M0L8_2afieldS4246;
  struct _M0TPC16string10StringView _M0L8filenameS1988;
  struct _M0TPC16string10StringView _M0L8_2afieldS4245;
  struct _M0TPC16string10StringView _M0L11start__lineS1991;
  moonbit_string_t _M0L6_2atmpS1990;
  moonbit_string_t _M0L6_2atmpS4244;
  moonbit_string_t _M0L6_2atmpS1989;
  struct _M0TPC16string10StringView _M0L8_2afieldS4243;
  struct _M0TPC16string10StringView _M0L13start__columnS1994;
  moonbit_string_t _M0L6_2atmpS1993;
  moonbit_string_t _M0L6_2atmpS4242;
  moonbit_string_t _M0L6_2atmpS1992;
  struct _M0TPC16string10StringView _M0L8_2afieldS4241;
  struct _M0TPC16string10StringView _M0L9end__lineS1997;
  moonbit_string_t _M0L6_2atmpS1996;
  moonbit_string_t _M0L6_2atmpS4240;
  moonbit_string_t _M0L6_2atmpS1995;
  struct _M0TPC16string10StringView _M0L8_2afieldS4239;
  int32_t _M0L6_2acntS4475;
  struct _M0TPC16string10StringView _M0L11end__columnS2001;
  moonbit_string_t _M0L6_2atmpS2000;
  moonbit_string_t _M0L6_2atmpS4238;
  moonbit_string_t _M0L6_2atmpS1999;
  moonbit_string_t _M0L6_2atmpS4237;
  moonbit_string_t _M0L6_2atmpS1998;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS195 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4249
  = (struct _M0TPC16string10StringView){
    _M0L4selfS196->$0_1, _M0L4selfS196->$0_2, _M0L4selfS196->$0_0
  };
  _M0L3pkgS1987 = _M0L8_2afieldS4249;
  moonbit_incref(_M0L3pkgS1987.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1986
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1987);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4248
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_90.data, _M0L6_2atmpS1986);
  moonbit_decref(_M0L6_2atmpS1986);
  _M0L6_2atmpS1985 = _M0L6_2atmpS4248;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4247
  = moonbit_add_string(_M0L6_2atmpS1985, (moonbit_string_t)moonbit_string_literal_91.data);
  moonbit_decref(_M0L6_2atmpS1985);
  _M0L6_2atmpS1984 = _M0L6_2atmpS4247;
  moonbit_incref(_M0L2sbS195);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS195, _M0L6_2atmpS1984);
  moonbit_incref(_M0L2sbS195);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS195, (moonbit_string_t)moonbit_string_literal_92.data);
  _M0L8_2afieldS4246
  = (struct _M0TPC16string10StringView){
    _M0L4selfS196->$1_1, _M0L4selfS196->$1_2, _M0L4selfS196->$1_0
  };
  _M0L8filenameS1988 = _M0L8_2afieldS4246;
  moonbit_incref(_M0L8filenameS1988.$0);
  moonbit_incref(_M0L2sbS195);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS195, _M0L8filenameS1988);
  _M0L8_2afieldS4245
  = (struct _M0TPC16string10StringView){
    _M0L4selfS196->$2_1, _M0L4selfS196->$2_2, _M0L4selfS196->$2_0
  };
  _M0L11start__lineS1991 = _M0L8_2afieldS4245;
  moonbit_incref(_M0L11start__lineS1991.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1990
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1991);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4244
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_93.data, _M0L6_2atmpS1990);
  moonbit_decref(_M0L6_2atmpS1990);
  _M0L6_2atmpS1989 = _M0L6_2atmpS4244;
  moonbit_incref(_M0L2sbS195);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS195, _M0L6_2atmpS1989);
  _M0L8_2afieldS4243
  = (struct _M0TPC16string10StringView){
    _M0L4selfS196->$3_1, _M0L4selfS196->$3_2, _M0L4selfS196->$3_0
  };
  _M0L13start__columnS1994 = _M0L8_2afieldS4243;
  moonbit_incref(_M0L13start__columnS1994.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1993
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1994);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4242
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_94.data, _M0L6_2atmpS1993);
  moonbit_decref(_M0L6_2atmpS1993);
  _M0L6_2atmpS1992 = _M0L6_2atmpS4242;
  moonbit_incref(_M0L2sbS195);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS195, _M0L6_2atmpS1992);
  _M0L8_2afieldS4241
  = (struct _M0TPC16string10StringView){
    _M0L4selfS196->$4_1, _M0L4selfS196->$4_2, _M0L4selfS196->$4_0
  };
  _M0L9end__lineS1997 = _M0L8_2afieldS4241;
  moonbit_incref(_M0L9end__lineS1997.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1996
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1997);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4240
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_95.data, _M0L6_2atmpS1996);
  moonbit_decref(_M0L6_2atmpS1996);
  _M0L6_2atmpS1995 = _M0L6_2atmpS4240;
  moonbit_incref(_M0L2sbS195);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS195, _M0L6_2atmpS1995);
  _M0L8_2afieldS4239
  = (struct _M0TPC16string10StringView){
    _M0L4selfS196->$5_1, _M0L4selfS196->$5_2, _M0L4selfS196->$5_0
  };
  _M0L6_2acntS4475 = Moonbit_object_header(_M0L4selfS196)->rc;
  if (_M0L6_2acntS4475 > 1) {
    int32_t _M0L11_2anew__cntS4481 = _M0L6_2acntS4475 - 1;
    Moonbit_object_header(_M0L4selfS196)->rc = _M0L11_2anew__cntS4481;
    moonbit_incref(_M0L8_2afieldS4239.$0);
  } else if (_M0L6_2acntS4475 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4480 =
      (struct _M0TPC16string10StringView){_M0L4selfS196->$4_1,
                                            _M0L4selfS196->$4_2,
                                            _M0L4selfS196->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4479;
    struct _M0TPC16string10StringView _M0L8_2afieldS4478;
    struct _M0TPC16string10StringView _M0L8_2afieldS4477;
    struct _M0TPC16string10StringView _M0L8_2afieldS4476;
    moonbit_decref(_M0L8_2afieldS4480.$0);
    _M0L8_2afieldS4479
    = (struct _M0TPC16string10StringView){
      _M0L4selfS196->$3_1, _M0L4selfS196->$3_2, _M0L4selfS196->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4479.$0);
    _M0L8_2afieldS4478
    = (struct _M0TPC16string10StringView){
      _M0L4selfS196->$2_1, _M0L4selfS196->$2_2, _M0L4selfS196->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4478.$0);
    _M0L8_2afieldS4477
    = (struct _M0TPC16string10StringView){
      _M0L4selfS196->$1_1, _M0L4selfS196->$1_2, _M0L4selfS196->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4477.$0);
    _M0L8_2afieldS4476
    = (struct _M0TPC16string10StringView){
      _M0L4selfS196->$0_1, _M0L4selfS196->$0_2, _M0L4selfS196->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4476.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS196);
  }
  _M0L11end__columnS2001 = _M0L8_2afieldS4239;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2000
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS2001);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4238
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_96.data, _M0L6_2atmpS2000);
  moonbit_decref(_M0L6_2atmpS2000);
  _M0L6_2atmpS1999 = _M0L6_2atmpS4238;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4237
  = moonbit_add_string(_M0L6_2atmpS1999, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1999);
  _M0L6_2atmpS1998 = _M0L6_2atmpS4237;
  moonbit_incref(_M0L2sbS195);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS195, _M0L6_2atmpS1998);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS195);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS193,
  moonbit_string_t _M0L3strS194
) {
  int32_t _M0L3lenS1974;
  int32_t _M0L6_2atmpS1976;
  int32_t _M0L6_2atmpS1975;
  int32_t _M0L6_2atmpS1973;
  moonbit_bytes_t _M0L8_2afieldS4251;
  moonbit_bytes_t _M0L4dataS1977;
  int32_t _M0L3lenS1978;
  int32_t _M0L6_2atmpS1979;
  int32_t _M0L3lenS1981;
  int32_t _M0L6_2atmpS4250;
  int32_t _M0L6_2atmpS1983;
  int32_t _M0L6_2atmpS1982;
  int32_t _M0L6_2atmpS1980;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1974 = _M0L4selfS193->$1;
  _M0L6_2atmpS1976 = Moonbit_array_length(_M0L3strS194);
  _M0L6_2atmpS1975 = _M0L6_2atmpS1976 * 2;
  _M0L6_2atmpS1973 = _M0L3lenS1974 + _M0L6_2atmpS1975;
  moonbit_incref(_M0L4selfS193);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS193, _M0L6_2atmpS1973);
  _M0L8_2afieldS4251 = _M0L4selfS193->$0;
  _M0L4dataS1977 = _M0L8_2afieldS4251;
  _M0L3lenS1978 = _M0L4selfS193->$1;
  _M0L6_2atmpS1979 = Moonbit_array_length(_M0L3strS194);
  moonbit_incref(_M0L4dataS1977);
  moonbit_incref(_M0L3strS194);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1977, _M0L3lenS1978, _M0L3strS194, 0, _M0L6_2atmpS1979);
  _M0L3lenS1981 = _M0L4selfS193->$1;
  _M0L6_2atmpS4250 = Moonbit_array_length(_M0L3strS194);
  moonbit_decref(_M0L3strS194);
  _M0L6_2atmpS1983 = _M0L6_2atmpS4250;
  _M0L6_2atmpS1982 = _M0L6_2atmpS1983 * 2;
  _M0L6_2atmpS1980 = _M0L3lenS1981 + _M0L6_2atmpS1982;
  _M0L4selfS193->$1 = _M0L6_2atmpS1980;
  moonbit_decref(_M0L4selfS193);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS185,
  int32_t _M0L13bytes__offsetS180,
  moonbit_string_t _M0L3strS187,
  int32_t _M0L11str__offsetS183,
  int32_t _M0L6lengthS181
) {
  int32_t _M0L6_2atmpS1972;
  int32_t _M0L6_2atmpS1971;
  int32_t _M0L2e1S179;
  int32_t _M0L6_2atmpS1970;
  int32_t _M0L2e2S182;
  int32_t _M0L4len1S184;
  int32_t _M0L4len2S186;
  int32_t _if__result_4688;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1972 = _M0L6lengthS181 * 2;
  _M0L6_2atmpS1971 = _M0L13bytes__offsetS180 + _M0L6_2atmpS1972;
  _M0L2e1S179 = _M0L6_2atmpS1971 - 1;
  _M0L6_2atmpS1970 = _M0L11str__offsetS183 + _M0L6lengthS181;
  _M0L2e2S182 = _M0L6_2atmpS1970 - 1;
  _M0L4len1S184 = Moonbit_array_length(_M0L4selfS185);
  _M0L4len2S186 = Moonbit_array_length(_M0L3strS187);
  if (_M0L6lengthS181 >= 0) {
    if (_M0L13bytes__offsetS180 >= 0) {
      if (_M0L2e1S179 < _M0L4len1S184) {
        if (_M0L11str__offsetS183 >= 0) {
          _if__result_4688 = _M0L2e2S182 < _M0L4len2S186;
        } else {
          _if__result_4688 = 0;
        }
      } else {
        _if__result_4688 = 0;
      }
    } else {
      _if__result_4688 = 0;
    }
  } else {
    _if__result_4688 = 0;
  }
  if (_if__result_4688) {
    int32_t _M0L16end__str__offsetS188 =
      _M0L11str__offsetS183 + _M0L6lengthS181;
    int32_t _M0L1iS189 = _M0L11str__offsetS183;
    int32_t _M0L1jS190 = _M0L13bytes__offsetS180;
    while (1) {
      if (_M0L1iS189 < _M0L16end__str__offsetS188) {
        int32_t _M0L6_2atmpS1967 = _M0L3strS187[_M0L1iS189];
        int32_t _M0L6_2atmpS1966 = (int32_t)_M0L6_2atmpS1967;
        uint32_t _M0L1cS191 = *(uint32_t*)&_M0L6_2atmpS1966;
        uint32_t _M0L6_2atmpS1962 = _M0L1cS191 & 255u;
        int32_t _M0L6_2atmpS1961;
        int32_t _M0L6_2atmpS1963;
        uint32_t _M0L6_2atmpS1965;
        int32_t _M0L6_2atmpS1964;
        int32_t _M0L6_2atmpS1968;
        int32_t _M0L6_2atmpS1969;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1961 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1962);
        if (
          _M0L1jS190 < 0 || _M0L1jS190 >= Moonbit_array_length(_M0L4selfS185)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS185[_M0L1jS190] = _M0L6_2atmpS1961;
        _M0L6_2atmpS1963 = _M0L1jS190 + 1;
        _M0L6_2atmpS1965 = _M0L1cS191 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1964 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1965);
        if (
          _M0L6_2atmpS1963 < 0
          || _M0L6_2atmpS1963 >= Moonbit_array_length(_M0L4selfS185)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS185[_M0L6_2atmpS1963] = _M0L6_2atmpS1964;
        _M0L6_2atmpS1968 = _M0L1iS189 + 1;
        _M0L6_2atmpS1969 = _M0L1jS190 + 2;
        _M0L1iS189 = _M0L6_2atmpS1968;
        _M0L1jS190 = _M0L6_2atmpS1969;
        continue;
      } else {
        moonbit_decref(_M0L3strS187);
        moonbit_decref(_M0L4selfS185);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS187);
    moonbit_decref(_M0L4selfS185);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS176,
  double _M0L3objS175
) {
  struct _M0TPB6Logger _M0L6_2atmpS1959;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1959
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS176
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS175, _M0L6_2atmpS1959);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS178,
  struct _M0TPC16string10StringView _M0L3objS177
) {
  struct _M0TPB6Logger _M0L6_2atmpS1960;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1960
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS178
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS177, _M0L6_2atmpS1960);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS121
) {
  int32_t _M0L6_2atmpS1958;
  struct _M0TPC16string10StringView _M0L7_2abindS120;
  moonbit_string_t _M0L7_2adataS122;
  int32_t _M0L8_2astartS123;
  int32_t _M0L6_2atmpS1957;
  int32_t _M0L6_2aendS124;
  int32_t _M0Lm9_2acursorS125;
  int32_t _M0Lm13accept__stateS126;
  int32_t _M0Lm10match__endS127;
  int32_t _M0Lm20match__tag__saver__0S128;
  int32_t _M0Lm20match__tag__saver__1S129;
  int32_t _M0Lm20match__tag__saver__2S130;
  int32_t _M0Lm20match__tag__saver__3S131;
  int32_t _M0Lm20match__tag__saver__4S132;
  int32_t _M0Lm6tag__0S133;
  int32_t _M0Lm6tag__1S134;
  int32_t _M0Lm9tag__1__1S135;
  int32_t _M0Lm9tag__1__2S136;
  int32_t _M0Lm6tag__3S137;
  int32_t _M0Lm6tag__2S138;
  int32_t _M0Lm9tag__2__1S139;
  int32_t _M0Lm6tag__4S140;
  int32_t _M0L6_2atmpS1915;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1958 = Moonbit_array_length(_M0L4reprS121);
  _M0L7_2abindS120
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1958, _M0L4reprS121
  };
  moonbit_incref(_M0L7_2abindS120.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS122 = _M0MPC16string10StringView4data(_M0L7_2abindS120);
  moonbit_incref(_M0L7_2abindS120.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS123
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS120);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1957 = _M0MPC16string10StringView6length(_M0L7_2abindS120);
  _M0L6_2aendS124 = _M0L8_2astartS123 + _M0L6_2atmpS1957;
  _M0Lm9_2acursorS125 = _M0L8_2astartS123;
  _M0Lm13accept__stateS126 = -1;
  _M0Lm10match__endS127 = -1;
  _M0Lm20match__tag__saver__0S128 = -1;
  _M0Lm20match__tag__saver__1S129 = -1;
  _M0Lm20match__tag__saver__2S130 = -1;
  _M0Lm20match__tag__saver__3S131 = -1;
  _M0Lm20match__tag__saver__4S132 = -1;
  _M0Lm6tag__0S133 = -1;
  _M0Lm6tag__1S134 = -1;
  _M0Lm9tag__1__1S135 = -1;
  _M0Lm9tag__1__2S136 = -1;
  _M0Lm6tag__3S137 = -1;
  _M0Lm6tag__2S138 = -1;
  _M0Lm9tag__2__1S139 = -1;
  _M0Lm6tag__4S140 = -1;
  _M0L6_2atmpS1915 = _M0Lm9_2acursorS125;
  if (_M0L6_2atmpS1915 < _M0L6_2aendS124) {
    int32_t _M0L6_2atmpS1917 = _M0Lm9_2acursorS125;
    int32_t _M0L6_2atmpS1916;
    moonbit_incref(_M0L7_2adataS122);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1916
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1917);
    if (_M0L6_2atmpS1916 == 64) {
      int32_t _M0L6_2atmpS1918 = _M0Lm9_2acursorS125;
      _M0Lm9_2acursorS125 = _M0L6_2atmpS1918 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1919;
        _M0Lm6tag__0S133 = _M0Lm9_2acursorS125;
        _M0L6_2atmpS1919 = _M0Lm9_2acursorS125;
        if (_M0L6_2atmpS1919 < _M0L6_2aendS124) {
          int32_t _M0L6_2atmpS1956 = _M0Lm9_2acursorS125;
          int32_t _M0L10next__charS148;
          int32_t _M0L6_2atmpS1920;
          moonbit_incref(_M0L7_2adataS122);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS148
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1956);
          _M0L6_2atmpS1920 = _M0Lm9_2acursorS125;
          _M0Lm9_2acursorS125 = _M0L6_2atmpS1920 + 1;
          if (_M0L10next__charS148 == 58) {
            int32_t _M0L6_2atmpS1921 = _M0Lm9_2acursorS125;
            if (_M0L6_2atmpS1921 < _M0L6_2aendS124) {
              int32_t _M0L6_2atmpS1922 = _M0Lm9_2acursorS125;
              int32_t _M0L12dispatch__15S149;
              _M0Lm9_2acursorS125 = _M0L6_2atmpS1922 + 1;
              _M0L12dispatch__15S149 = 0;
              loop__label__15_152:;
              while (1) {
                int32_t _M0L6_2atmpS1923;
                switch (_M0L12dispatch__15S149) {
                  case 3: {
                    int32_t _M0L6_2atmpS1926;
                    _M0Lm9tag__1__2S136 = _M0Lm9tag__1__1S135;
                    _M0Lm9tag__1__1S135 = _M0Lm6tag__1S134;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1926 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1926 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1931 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS156;
                      int32_t _M0L6_2atmpS1927;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS156
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1931);
                      _M0L6_2atmpS1927 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1927 + 1;
                      if (_M0L10next__charS156 < 58) {
                        if (_M0L10next__charS156 < 48) {
                          goto join_155;
                        } else {
                          int32_t _M0L6_2atmpS1928;
                          _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                          _M0Lm9tag__2__1S139 = _M0Lm6tag__2S138;
                          _M0Lm6tag__2S138 = _M0Lm9_2acursorS125;
                          _M0Lm6tag__3S137 = _M0Lm9_2acursorS125;
                          _M0L6_2atmpS1928 = _M0Lm9_2acursorS125;
                          if (_M0L6_2atmpS1928 < _M0L6_2aendS124) {
                            int32_t _M0L6_2atmpS1930 = _M0Lm9_2acursorS125;
                            int32_t _M0L10next__charS158;
                            int32_t _M0L6_2atmpS1929;
                            moonbit_incref(_M0L7_2adataS122);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS158
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1930);
                            _M0L6_2atmpS1929 = _M0Lm9_2acursorS125;
                            _M0Lm9_2acursorS125 = _M0L6_2atmpS1929 + 1;
                            if (_M0L10next__charS158 < 48) {
                              if (_M0L10next__charS158 == 45) {
                                goto join_150;
                              } else {
                                goto join_157;
                              }
                            } else if (_M0L10next__charS158 > 57) {
                              if (_M0L10next__charS158 < 59) {
                                _M0L12dispatch__15S149 = 3;
                                goto loop__label__15_152;
                              } else {
                                goto join_157;
                              }
                            } else {
                              _M0L12dispatch__15S149 = 6;
                              goto loop__label__15_152;
                            }
                            join_157:;
                            _M0L12dispatch__15S149 = 0;
                            goto loop__label__15_152;
                          } else {
                            goto join_141;
                          }
                        }
                      } else if (_M0L10next__charS156 > 58) {
                        goto join_155;
                      } else {
                        _M0L12dispatch__15S149 = 1;
                        goto loop__label__15_152;
                      }
                      join_155:;
                      _M0L12dispatch__15S149 = 0;
                      goto loop__label__15_152;
                    } else {
                      goto join_141;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1932;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0Lm6tag__2S138 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1932 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1932 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1934 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1933;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1934);
                      _M0L6_2atmpS1933 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1933 + 1;
                      if (_M0L10next__charS160 < 58) {
                        if (_M0L10next__charS160 < 48) {
                          goto join_159;
                        } else {
                          _M0L12dispatch__15S149 = 2;
                          goto loop__label__15_152;
                        }
                      } else if (_M0L10next__charS160 > 58) {
                        goto join_159;
                      } else {
                        _M0L12dispatch__15S149 = 3;
                        goto loop__label__15_152;
                      }
                      join_159:;
                      _M0L12dispatch__15S149 = 0;
                      goto loop__label__15_152;
                    } else {
                      goto join_141;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1935;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1935 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1935 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1937 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS161;
                      int32_t _M0L6_2atmpS1936;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS161
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1937);
                      _M0L6_2atmpS1936 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1936 + 1;
                      if (_M0L10next__charS161 == 58) {
                        _M0L12dispatch__15S149 = 1;
                        goto loop__label__15_152;
                      } else {
                        _M0L12dispatch__15S149 = 0;
                        goto loop__label__15_152;
                      }
                    } else {
                      goto join_141;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1938;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0Lm6tag__4S140 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1938 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1938 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1946 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS163;
                      int32_t _M0L6_2atmpS1939;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS163
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1946);
                      _M0L6_2atmpS1939 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1939 + 1;
                      if (_M0L10next__charS163 < 58) {
                        if (_M0L10next__charS163 < 48) {
                          goto join_162;
                        } else {
                          _M0L12dispatch__15S149 = 4;
                          goto loop__label__15_152;
                        }
                      } else if (_M0L10next__charS163 > 58) {
                        goto join_162;
                      } else {
                        int32_t _M0L6_2atmpS1940;
                        _M0Lm9tag__1__2S136 = _M0Lm9tag__1__1S135;
                        _M0Lm9tag__1__1S135 = _M0Lm6tag__1S134;
                        _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                        _M0L6_2atmpS1940 = _M0Lm9_2acursorS125;
                        if (_M0L6_2atmpS1940 < _M0L6_2aendS124) {
                          int32_t _M0L6_2atmpS1945 = _M0Lm9_2acursorS125;
                          int32_t _M0L10next__charS165;
                          int32_t _M0L6_2atmpS1941;
                          moonbit_incref(_M0L7_2adataS122);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS165
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1945);
                          _M0L6_2atmpS1941 = _M0Lm9_2acursorS125;
                          _M0Lm9_2acursorS125 = _M0L6_2atmpS1941 + 1;
                          if (_M0L10next__charS165 < 58) {
                            if (_M0L10next__charS165 < 48) {
                              goto join_164;
                            } else {
                              int32_t _M0L6_2atmpS1942;
                              _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                              _M0Lm9tag__2__1S139 = _M0Lm6tag__2S138;
                              _M0Lm6tag__2S138 = _M0Lm9_2acursorS125;
                              _M0L6_2atmpS1942 = _M0Lm9_2acursorS125;
                              if (_M0L6_2atmpS1942 < _M0L6_2aendS124) {
                                int32_t _M0L6_2atmpS1944 =
                                  _M0Lm9_2acursorS125;
                                int32_t _M0L10next__charS167;
                                int32_t _M0L6_2atmpS1943;
                                moonbit_incref(_M0L7_2adataS122);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS167
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1944);
                                _M0L6_2atmpS1943 = _M0Lm9_2acursorS125;
                                _M0Lm9_2acursorS125 = _M0L6_2atmpS1943 + 1;
                                if (_M0L10next__charS167 < 58) {
                                  if (_M0L10next__charS167 < 48) {
                                    goto join_166;
                                  } else {
                                    _M0L12dispatch__15S149 = 5;
                                    goto loop__label__15_152;
                                  }
                                } else if (_M0L10next__charS167 > 58) {
                                  goto join_166;
                                } else {
                                  _M0L12dispatch__15S149 = 3;
                                  goto loop__label__15_152;
                                }
                                join_166:;
                                _M0L12dispatch__15S149 = 0;
                                goto loop__label__15_152;
                              } else {
                                goto join_154;
                              }
                            }
                          } else if (_M0L10next__charS165 > 58) {
                            goto join_164;
                          } else {
                            _M0L12dispatch__15S149 = 1;
                            goto loop__label__15_152;
                          }
                          join_164:;
                          _M0L12dispatch__15S149 = 0;
                          goto loop__label__15_152;
                        } else {
                          goto join_141;
                        }
                      }
                      join_162:;
                      _M0L12dispatch__15S149 = 0;
                      goto loop__label__15_152;
                    } else {
                      goto join_141;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1947;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0Lm6tag__2S138 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1947 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1947 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1949 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS169;
                      int32_t _M0L6_2atmpS1948;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS169
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1949);
                      _M0L6_2atmpS1948 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1948 + 1;
                      if (_M0L10next__charS169 < 58) {
                        if (_M0L10next__charS169 < 48) {
                          goto join_168;
                        } else {
                          _M0L12dispatch__15S149 = 5;
                          goto loop__label__15_152;
                        }
                      } else if (_M0L10next__charS169 > 58) {
                        goto join_168;
                      } else {
                        _M0L12dispatch__15S149 = 3;
                        goto loop__label__15_152;
                      }
                      join_168:;
                      _M0L12dispatch__15S149 = 0;
                      goto loop__label__15_152;
                    } else {
                      goto join_154;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1950;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0Lm6tag__2S138 = _M0Lm9_2acursorS125;
                    _M0Lm6tag__3S137 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1950 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1950 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1952 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS171;
                      int32_t _M0L6_2atmpS1951;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS171
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1952);
                      _M0L6_2atmpS1951 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1951 + 1;
                      if (_M0L10next__charS171 < 48) {
                        if (_M0L10next__charS171 == 45) {
                          goto join_150;
                        } else {
                          goto join_170;
                        }
                      } else if (_M0L10next__charS171 > 57) {
                        if (_M0L10next__charS171 < 59) {
                          _M0L12dispatch__15S149 = 3;
                          goto loop__label__15_152;
                        } else {
                          goto join_170;
                        }
                      } else {
                        _M0L12dispatch__15S149 = 6;
                        goto loop__label__15_152;
                      }
                      join_170:;
                      _M0L12dispatch__15S149 = 0;
                      goto loop__label__15_152;
                    } else {
                      goto join_141;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1953;
                    _M0Lm9tag__1__1S135 = _M0Lm6tag__1S134;
                    _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                    _M0L6_2atmpS1953 = _M0Lm9_2acursorS125;
                    if (_M0L6_2atmpS1953 < _M0L6_2aendS124) {
                      int32_t _M0L6_2atmpS1955 = _M0Lm9_2acursorS125;
                      int32_t _M0L10next__charS173;
                      int32_t _M0L6_2atmpS1954;
                      moonbit_incref(_M0L7_2adataS122);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS173
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1955);
                      _M0L6_2atmpS1954 = _M0Lm9_2acursorS125;
                      _M0Lm9_2acursorS125 = _M0L6_2atmpS1954 + 1;
                      if (_M0L10next__charS173 < 58) {
                        if (_M0L10next__charS173 < 48) {
                          goto join_172;
                        } else {
                          _M0L12dispatch__15S149 = 2;
                          goto loop__label__15_152;
                        }
                      } else if (_M0L10next__charS173 > 58) {
                        goto join_172;
                      } else {
                        _M0L12dispatch__15S149 = 1;
                        goto loop__label__15_152;
                      }
                      join_172:;
                      _M0L12dispatch__15S149 = 0;
                      goto loop__label__15_152;
                    } else {
                      goto join_141;
                    }
                    break;
                  }
                  default: {
                    goto join_141;
                    break;
                  }
                }
                join_154:;
                _M0Lm6tag__1S134 = _M0Lm9tag__1__2S136;
                _M0Lm6tag__2S138 = _M0Lm9tag__2__1S139;
                _M0Lm20match__tag__saver__0S128 = _M0Lm6tag__0S133;
                _M0Lm20match__tag__saver__1S129 = _M0Lm6tag__1S134;
                _M0Lm20match__tag__saver__2S130 = _M0Lm6tag__2S138;
                _M0Lm20match__tag__saver__3S131 = _M0Lm6tag__3S137;
                _M0Lm20match__tag__saver__4S132 = _M0Lm6tag__4S140;
                _M0Lm13accept__stateS126 = 0;
                _M0Lm10match__endS127 = _M0Lm9_2acursorS125;
                goto join_141;
                join_150:;
                _M0Lm9tag__1__1S135 = _M0Lm9tag__1__2S136;
                _M0Lm6tag__1S134 = _M0Lm9_2acursorS125;
                _M0Lm6tag__2S138 = _M0Lm9tag__2__1S139;
                _M0L6_2atmpS1923 = _M0Lm9_2acursorS125;
                if (_M0L6_2atmpS1923 < _M0L6_2aendS124) {
                  int32_t _M0L6_2atmpS1925 = _M0Lm9_2acursorS125;
                  int32_t _M0L10next__charS153;
                  int32_t _M0L6_2atmpS1924;
                  moonbit_incref(_M0L7_2adataS122);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS153
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS122, _M0L6_2atmpS1925);
                  _M0L6_2atmpS1924 = _M0Lm9_2acursorS125;
                  _M0Lm9_2acursorS125 = _M0L6_2atmpS1924 + 1;
                  if (_M0L10next__charS153 < 58) {
                    if (_M0L10next__charS153 < 48) {
                      goto join_151;
                    } else {
                      _M0L12dispatch__15S149 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS153 > 58) {
                    goto join_151;
                  } else {
                    _M0L12dispatch__15S149 = 1;
                    continue;
                  }
                  join_151:;
                  _M0L12dispatch__15S149 = 0;
                  continue;
                } else {
                  goto join_141;
                }
                break;
              }
            } else {
              goto join_141;
            }
          } else {
            continue;
          }
        } else {
          goto join_141;
        }
        break;
      }
    } else {
      goto join_141;
    }
  } else {
    goto join_141;
  }
  join_141:;
  switch (_M0Lm13accept__stateS126) {
    case 0: {
      int32_t _M0L6_2atmpS1914 = _M0Lm20match__tag__saver__1S129;
      int32_t _M0L6_2atmpS1913 = _M0L6_2atmpS1914 + 1;
      int64_t _M0L6_2atmpS1910 = (int64_t)_M0L6_2atmpS1913;
      int32_t _M0L6_2atmpS1912 = _M0Lm20match__tag__saver__2S130;
      int64_t _M0L6_2atmpS1911 = (int64_t)_M0L6_2atmpS1912;
      struct _M0TPC16string10StringView _M0L11start__lineS142;
      int32_t _M0L6_2atmpS1909;
      int32_t _M0L6_2atmpS1908;
      int64_t _M0L6_2atmpS1905;
      int32_t _M0L6_2atmpS1907;
      int64_t _M0L6_2atmpS1906;
      struct _M0TPC16string10StringView _M0L13start__columnS143;
      int32_t _M0L6_2atmpS1904;
      int64_t _M0L6_2atmpS1901;
      int32_t _M0L6_2atmpS1903;
      int64_t _M0L6_2atmpS1902;
      struct _M0TPC16string10StringView _M0L3pkgS144;
      int32_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1899;
      int64_t _M0L6_2atmpS1896;
      int32_t _M0L6_2atmpS1898;
      int64_t _M0L6_2atmpS1897;
      struct _M0TPC16string10StringView _M0L8filenameS145;
      int32_t _M0L6_2atmpS1895;
      int32_t _M0L6_2atmpS1894;
      int64_t _M0L6_2atmpS1891;
      int32_t _M0L6_2atmpS1893;
      int64_t _M0L6_2atmpS1892;
      struct _M0TPC16string10StringView _M0L9end__lineS146;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1889;
      int64_t _M0L6_2atmpS1886;
      int32_t _M0L6_2atmpS1888;
      int64_t _M0L6_2atmpS1887;
      struct _M0TPC16string10StringView _M0L11end__columnS147;
      struct _M0TPB13SourceLocRepr* _block_4705;
      moonbit_incref(_M0L7_2adataS122);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS142
      = _M0MPC16string6String4view(_M0L7_2adataS122, _M0L6_2atmpS1910, _M0L6_2atmpS1911);
      _M0L6_2atmpS1909 = _M0Lm20match__tag__saver__2S130;
      _M0L6_2atmpS1908 = _M0L6_2atmpS1909 + 1;
      _M0L6_2atmpS1905 = (int64_t)_M0L6_2atmpS1908;
      _M0L6_2atmpS1907 = _M0Lm20match__tag__saver__3S131;
      _M0L6_2atmpS1906 = (int64_t)_M0L6_2atmpS1907;
      moonbit_incref(_M0L7_2adataS122);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS143
      = _M0MPC16string6String4view(_M0L7_2adataS122, _M0L6_2atmpS1905, _M0L6_2atmpS1906);
      _M0L6_2atmpS1904 = _M0L8_2astartS123 + 1;
      _M0L6_2atmpS1901 = (int64_t)_M0L6_2atmpS1904;
      _M0L6_2atmpS1903 = _M0Lm20match__tag__saver__0S128;
      _M0L6_2atmpS1902 = (int64_t)_M0L6_2atmpS1903;
      moonbit_incref(_M0L7_2adataS122);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS144
      = _M0MPC16string6String4view(_M0L7_2adataS122, _M0L6_2atmpS1901, _M0L6_2atmpS1902);
      _M0L6_2atmpS1900 = _M0Lm20match__tag__saver__0S128;
      _M0L6_2atmpS1899 = _M0L6_2atmpS1900 + 1;
      _M0L6_2atmpS1896 = (int64_t)_M0L6_2atmpS1899;
      _M0L6_2atmpS1898 = _M0Lm20match__tag__saver__1S129;
      _M0L6_2atmpS1897 = (int64_t)_M0L6_2atmpS1898;
      moonbit_incref(_M0L7_2adataS122);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS145
      = _M0MPC16string6String4view(_M0L7_2adataS122, _M0L6_2atmpS1896, _M0L6_2atmpS1897);
      _M0L6_2atmpS1895 = _M0Lm20match__tag__saver__3S131;
      _M0L6_2atmpS1894 = _M0L6_2atmpS1895 + 1;
      _M0L6_2atmpS1891 = (int64_t)_M0L6_2atmpS1894;
      _M0L6_2atmpS1893 = _M0Lm20match__tag__saver__4S132;
      _M0L6_2atmpS1892 = (int64_t)_M0L6_2atmpS1893;
      moonbit_incref(_M0L7_2adataS122);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS146
      = _M0MPC16string6String4view(_M0L7_2adataS122, _M0L6_2atmpS1891, _M0L6_2atmpS1892);
      _M0L6_2atmpS1890 = _M0Lm20match__tag__saver__4S132;
      _M0L6_2atmpS1889 = _M0L6_2atmpS1890 + 1;
      _M0L6_2atmpS1886 = (int64_t)_M0L6_2atmpS1889;
      _M0L6_2atmpS1888 = _M0Lm10match__endS127;
      _M0L6_2atmpS1887 = (int64_t)_M0L6_2atmpS1888;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS147
      = _M0MPC16string6String4view(_M0L7_2adataS122, _M0L6_2atmpS1886, _M0L6_2atmpS1887);
      _block_4705
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4705)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4705->$0_0 = _M0L3pkgS144.$0;
      _block_4705->$0_1 = _M0L3pkgS144.$1;
      _block_4705->$0_2 = _M0L3pkgS144.$2;
      _block_4705->$1_0 = _M0L8filenameS145.$0;
      _block_4705->$1_1 = _M0L8filenameS145.$1;
      _block_4705->$1_2 = _M0L8filenameS145.$2;
      _block_4705->$2_0 = _M0L11start__lineS142.$0;
      _block_4705->$2_1 = _M0L11start__lineS142.$1;
      _block_4705->$2_2 = _M0L11start__lineS142.$2;
      _block_4705->$3_0 = _M0L13start__columnS143.$0;
      _block_4705->$3_1 = _M0L13start__columnS143.$1;
      _block_4705->$3_2 = _M0L13start__columnS143.$2;
      _block_4705->$4_0 = _M0L9end__lineS146.$0;
      _block_4705->$4_1 = _M0L9end__lineS146.$1;
      _block_4705->$4_2 = _M0L9end__lineS146.$2;
      _block_4705->$5_0 = _M0L11end__columnS147.$0;
      _block_4705->$5_1 = _M0L11end__columnS147.$1;
      _block_4705->$5_2 = _M0L11end__columnS147.$2;
      return _block_4705;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS122);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS118,
  int32_t _M0L5indexS119
) {
  int32_t _M0L3lenS117;
  int32_t _if__result_4706;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS117 = _M0L4selfS118->$1;
  if (_M0L5indexS119 >= 0) {
    _if__result_4706 = _M0L5indexS119 < _M0L3lenS117;
  } else {
    _if__result_4706 = 0;
  }
  if (_if__result_4706) {
    moonbit_string_t* _M0L6_2atmpS1885;
    moonbit_string_t _M0L6_2atmpS4252;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1885 = _M0MPC15array5Array6bufferGsE(_M0L4selfS118);
    if (
      _M0L5indexS119 < 0
      || _M0L5indexS119 >= Moonbit_array_length(_M0L6_2atmpS1885)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4252 = (moonbit_string_t)_M0L6_2atmpS1885[_M0L5indexS119];
    moonbit_incref(_M0L6_2atmpS4252);
    moonbit_decref(_M0L6_2atmpS1885);
    return _M0L6_2atmpS4252;
  } else {
    moonbit_decref(_M0L4selfS118);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS114
) {
  moonbit_string_t* _M0L8_2afieldS4253;
  int32_t _M0L6_2acntS4482;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4253 = _M0L4selfS114->$0;
  _M0L6_2acntS4482 = Moonbit_object_header(_M0L4selfS114)->rc;
  if (_M0L6_2acntS4482 > 1) {
    int32_t _M0L11_2anew__cntS4483 = _M0L6_2acntS4482 - 1;
    Moonbit_object_header(_M0L4selfS114)->rc = _M0L11_2anew__cntS4483;
    moonbit_incref(_M0L8_2afieldS4253);
  } else if (_M0L6_2acntS4482 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS114);
  }
  return _M0L8_2afieldS4253;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS115
) {
  struct _M0TUsiE** _M0L8_2afieldS4254;
  int32_t _M0L6_2acntS4484;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4254 = _M0L4selfS115->$0;
  _M0L6_2acntS4484 = Moonbit_object_header(_M0L4selfS115)->rc;
  if (_M0L6_2acntS4484 > 1) {
    int32_t _M0L11_2anew__cntS4485 = _M0L6_2acntS4484 - 1;
    Moonbit_object_header(_M0L4selfS115)->rc = _M0L11_2anew__cntS4485;
    moonbit_incref(_M0L8_2afieldS4254);
  } else if (_M0L6_2acntS4484 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS115);
  }
  return _M0L8_2afieldS4254;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS116
) {
  void** _M0L8_2afieldS4255;
  int32_t _M0L6_2acntS4486;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4255 = _M0L4selfS116->$0;
  _M0L6_2acntS4486 = Moonbit_object_header(_M0L4selfS116)->rc;
  if (_M0L6_2acntS4486 > 1) {
    int32_t _M0L11_2anew__cntS4487 = _M0L6_2acntS4486 - 1;
    Moonbit_object_header(_M0L4selfS116)->rc = _M0L11_2anew__cntS4487;
    moonbit_incref(_M0L8_2afieldS4255);
  } else if (_M0L6_2acntS4486 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS116);
  }
  return _M0L8_2afieldS4255;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS113) {
  struct _M0TPB13StringBuilder* _M0L3bufS112;
  struct _M0TPB6Logger _M0L6_2atmpS1884;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS112 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS112);
  _M0L6_2atmpS1884
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS112
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS113, _M0L6_2atmpS1884);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS112);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS111) {
  int32_t _M0L6_2atmpS1883;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1883 = (int32_t)_M0L4selfS111;
  return _M0L6_2atmpS1883;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS109,
  int32_t _M0L8trailingS110
) {
  int32_t _M0L6_2atmpS1882;
  int32_t _M0L6_2atmpS1881;
  int32_t _M0L6_2atmpS1880;
  int32_t _M0L6_2atmpS1879;
  int32_t _M0L6_2atmpS1878;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1882 = _M0L7leadingS109 - 55296;
  _M0L6_2atmpS1881 = _M0L6_2atmpS1882 * 1024;
  _M0L6_2atmpS1880 = _M0L6_2atmpS1881 + _M0L8trailingS110;
  _M0L6_2atmpS1879 = _M0L6_2atmpS1880 - 56320;
  _M0L6_2atmpS1878 = _M0L6_2atmpS1879 + 65536;
  return _M0L6_2atmpS1878;
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
  int32_t _M0L3lenS1873;
  int32_t _M0L6_2atmpS1872;
  moonbit_bytes_t _M0L8_2afieldS4256;
  moonbit_bytes_t _M0L4dataS1876;
  int32_t _M0L3lenS1877;
  int32_t _M0L3incS105;
  int32_t _M0L3lenS1875;
  int32_t _M0L6_2atmpS1874;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1873 = _M0L4selfS104->$1;
  _M0L6_2atmpS1872 = _M0L3lenS1873 + 4;
  moonbit_incref(_M0L4selfS104);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS104, _M0L6_2atmpS1872);
  _M0L8_2afieldS4256 = _M0L4selfS104->$0;
  _M0L4dataS1876 = _M0L8_2afieldS4256;
  _M0L3lenS1877 = _M0L4selfS104->$1;
  moonbit_incref(_M0L4dataS1876);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS105
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1876, _M0L3lenS1877, _M0L2chS106);
  _M0L3lenS1875 = _M0L4selfS104->$1;
  _M0L6_2atmpS1874 = _M0L3lenS1875 + _M0L3incS105;
  _M0L4selfS104->$1 = _M0L6_2atmpS1874;
  moonbit_decref(_M0L4selfS104);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS99,
  int32_t _M0L8requiredS100
) {
  moonbit_bytes_t _M0L8_2afieldS4260;
  moonbit_bytes_t _M0L4dataS1871;
  int32_t _M0L6_2atmpS4259;
  int32_t _M0L12current__lenS98;
  int32_t _M0Lm13enough__spaceS101;
  int32_t _M0L6_2atmpS1869;
  int32_t _M0L6_2atmpS1870;
  moonbit_bytes_t _M0L9new__dataS103;
  moonbit_bytes_t _M0L8_2afieldS4258;
  moonbit_bytes_t _M0L4dataS1867;
  int32_t _M0L3lenS1868;
  moonbit_bytes_t _M0L6_2aoldS4257;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4260 = _M0L4selfS99->$0;
  _M0L4dataS1871 = _M0L8_2afieldS4260;
  _M0L6_2atmpS4259 = Moonbit_array_length(_M0L4dataS1871);
  _M0L12current__lenS98 = _M0L6_2atmpS4259;
  if (_M0L8requiredS100 <= _M0L12current__lenS98) {
    moonbit_decref(_M0L4selfS99);
    return 0;
  }
  _M0Lm13enough__spaceS101 = _M0L12current__lenS98;
  while (1) {
    int32_t _M0L6_2atmpS1865 = _M0Lm13enough__spaceS101;
    if (_M0L6_2atmpS1865 < _M0L8requiredS100) {
      int32_t _M0L6_2atmpS1866 = _M0Lm13enough__spaceS101;
      _M0Lm13enough__spaceS101 = _M0L6_2atmpS1866 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1869 = _M0Lm13enough__spaceS101;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1870 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS103
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1869, _M0L6_2atmpS1870);
  _M0L8_2afieldS4258 = _M0L4selfS99->$0;
  _M0L4dataS1867 = _M0L8_2afieldS4258;
  _M0L3lenS1868 = _M0L4selfS99->$1;
  moonbit_incref(_M0L4dataS1867);
  moonbit_incref(_M0L9new__dataS103);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS103, 0, _M0L4dataS1867, 0, _M0L3lenS1868);
  _M0L6_2aoldS4257 = _M0L4selfS99->$0;
  moonbit_decref(_M0L6_2aoldS4257);
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
    uint32_t _M0L6_2atmpS1848 = _M0L4codeS91 & 255u;
    int32_t _M0L6_2atmpS1847;
    int32_t _M0L6_2atmpS1849;
    uint32_t _M0L6_2atmpS1851;
    int32_t _M0L6_2atmpS1850;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1847 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1848);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1847;
    _M0L6_2atmpS1849 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1851 = _M0L4codeS91 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1850 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1851);
    if (
      _M0L6_2atmpS1849 < 0
      || _M0L6_2atmpS1849 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1849] = _M0L6_2atmpS1850;
    moonbit_decref(_M0L4selfS93);
    return 2;
  } else if (_M0L4codeS91 < 1114112u) {
    uint32_t _M0L2hiS95 = _M0L4codeS91 - 65536u;
    uint32_t _M0L6_2atmpS1864 = _M0L2hiS95 >> 10;
    uint32_t _M0L2loS96 = _M0L6_2atmpS1864 | 55296u;
    uint32_t _M0L6_2atmpS1863 = _M0L2hiS95 & 1023u;
    uint32_t _M0L2hiS97 = _M0L6_2atmpS1863 | 56320u;
    uint32_t _M0L6_2atmpS1853 = _M0L2loS96 & 255u;
    int32_t _M0L6_2atmpS1852;
    int32_t _M0L6_2atmpS1854;
    uint32_t _M0L6_2atmpS1856;
    int32_t _M0L6_2atmpS1855;
    int32_t _M0L6_2atmpS1857;
    uint32_t _M0L6_2atmpS1859;
    int32_t _M0L6_2atmpS1858;
    int32_t _M0L6_2atmpS1860;
    uint32_t _M0L6_2atmpS1862;
    int32_t _M0L6_2atmpS1861;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1852 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1853);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1852;
    _M0L6_2atmpS1854 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1856 = _M0L2loS96 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1855 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1856);
    if (
      _M0L6_2atmpS1854 < 0
      || _M0L6_2atmpS1854 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1854] = _M0L6_2atmpS1855;
    _M0L6_2atmpS1857 = _M0L6offsetS94 + 2;
    _M0L6_2atmpS1859 = _M0L2hiS97 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1858 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1859);
    if (
      _M0L6_2atmpS1857 < 0
      || _M0L6_2atmpS1857 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1857] = _M0L6_2atmpS1858;
    _M0L6_2atmpS1860 = _M0L6offsetS94 + 3;
    _M0L6_2atmpS1862 = _M0L2hiS97 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1861 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1862);
    if (
      _M0L6_2atmpS1860 < 0
      || _M0L6_2atmpS1860 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1860] = _M0L6_2atmpS1861;
    moonbit_decref(_M0L4selfS93);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS93);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_97.data, (moonbit_string_t)moonbit_string_literal_98.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS90) {
  int32_t _M0L6_2atmpS1846;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1846 = *(int32_t*)&_M0L4selfS90;
  return _M0L6_2atmpS1846 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1845;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1845 = _M0L4selfS89;
  return *(uint32_t*)&_M0L6_2atmpS1845;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS88
) {
  moonbit_bytes_t _M0L8_2afieldS4262;
  moonbit_bytes_t _M0L4dataS1844;
  moonbit_bytes_t _M0L6_2atmpS1841;
  int32_t _M0L8_2afieldS4261;
  int32_t _M0L3lenS1843;
  int64_t _M0L6_2atmpS1842;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4262 = _M0L4selfS88->$0;
  _M0L4dataS1844 = _M0L8_2afieldS4262;
  moonbit_incref(_M0L4dataS1844);
  _M0L6_2atmpS1841 = _M0L4dataS1844;
  _M0L8_2afieldS4261 = _M0L4selfS88->$1;
  moonbit_decref(_M0L4selfS88);
  _M0L3lenS1843 = _M0L8_2afieldS4261;
  _M0L6_2atmpS1842 = (int64_t)_M0L3lenS1843;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1841, 0, _M0L6_2atmpS1842);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS83,
  int32_t _M0L6offsetS87,
  int64_t _M0L6lengthS85
) {
  int32_t _M0L3lenS82;
  int32_t _M0L6lengthS84;
  int32_t _if__result_4708;
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
      int32_t _M0L6_2atmpS1840 = _M0L6offsetS87 + _M0L6lengthS84;
      _if__result_4708 = _M0L6_2atmpS1840 <= _M0L3lenS82;
    } else {
      _if__result_4708 = 0;
    }
  } else {
    _if__result_4708 = 0;
  }
  if (_if__result_4708) {
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
  struct _M0TPB13StringBuilder* _block_4709;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS80 < 1) {
    _M0L7initialS79 = 1;
  } else {
    _M0L7initialS79 = _M0L10size__hintS80;
  }
  _M0L4dataS81 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS79, 0);
  _block_4709
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4709)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4709->$0 = _M0L4dataS81;
  _block_4709->$1 = 0;
  return _block_4709;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS78) {
  int32_t _M0L6_2atmpS1839;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1839 = (int32_t)_M0L4selfS78;
  return _M0L6_2atmpS1839;
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
  int32_t _if__result_4710;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS27 == _M0L3srcS28) {
    _if__result_4710 = _M0L11dst__offsetS29 < _M0L11src__offsetS30;
  } else {
    _if__result_4710 = 0;
  }
  if (_if__result_4710) {
    int32_t _M0L1iS31 = 0;
    while (1) {
      if (_M0L1iS31 < _M0L3lenS32) {
        int32_t _M0L6_2atmpS1803 = _M0L11dst__offsetS29 + _M0L1iS31;
        int32_t _M0L6_2atmpS1805 = _M0L11src__offsetS30 + _M0L1iS31;
        int32_t _M0L6_2atmpS1804;
        int32_t _M0L6_2atmpS1806;
        if (
          _M0L6_2atmpS1805 < 0
          || _M0L6_2atmpS1805 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1804 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1805];
        if (
          _M0L6_2atmpS1803 < 0
          || _M0L6_2atmpS1803 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1803] = _M0L6_2atmpS1804;
        _M0L6_2atmpS1806 = _M0L1iS31 + 1;
        _M0L1iS31 = _M0L6_2atmpS1806;
        continue;
      } else {
        moonbit_decref(_M0L3srcS28);
        moonbit_decref(_M0L3dstS27);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1811 = _M0L3lenS32 - 1;
    int32_t _M0L1iS34 = _M0L6_2atmpS1811;
    while (1) {
      if (_M0L1iS34 >= 0) {
        int32_t _M0L6_2atmpS1807 = _M0L11dst__offsetS29 + _M0L1iS34;
        int32_t _M0L6_2atmpS1809 = _M0L11src__offsetS30 + _M0L1iS34;
        int32_t _M0L6_2atmpS1808;
        int32_t _M0L6_2atmpS1810;
        if (
          _M0L6_2atmpS1809 < 0
          || _M0L6_2atmpS1809 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1808 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1809];
        if (
          _M0L6_2atmpS1807 < 0
          || _M0L6_2atmpS1807 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1807] = _M0L6_2atmpS1808;
        _M0L6_2atmpS1810 = _M0L1iS34 - 1;
        _M0L1iS34 = _M0L6_2atmpS1810;
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
  int32_t _if__result_4713;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS36 == _M0L3srcS37) {
    _if__result_4713 = _M0L11dst__offsetS38 < _M0L11src__offsetS39;
  } else {
    _if__result_4713 = 0;
  }
  if (_if__result_4713) {
    int32_t _M0L1iS40 = 0;
    while (1) {
      if (_M0L1iS40 < _M0L3lenS41) {
        int32_t _M0L6_2atmpS1812 = _M0L11dst__offsetS38 + _M0L1iS40;
        int32_t _M0L6_2atmpS1814 = _M0L11src__offsetS39 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS4264;
        moonbit_string_t _M0L6_2atmpS1813;
        moonbit_string_t _M0L6_2aoldS4263;
        int32_t _M0L6_2atmpS1815;
        if (
          _M0L6_2atmpS1814 < 0
          || _M0L6_2atmpS1814 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4264 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1814];
        _M0L6_2atmpS1813 = _M0L6_2atmpS4264;
        if (
          _M0L6_2atmpS1812 < 0
          || _M0L6_2atmpS1812 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4263 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1812];
        moonbit_incref(_M0L6_2atmpS1813);
        moonbit_decref(_M0L6_2aoldS4263);
        _M0L3dstS36[_M0L6_2atmpS1812] = _M0L6_2atmpS1813;
        _M0L6_2atmpS1815 = _M0L1iS40 + 1;
        _M0L1iS40 = _M0L6_2atmpS1815;
        continue;
      } else {
        moonbit_decref(_M0L3srcS37);
        moonbit_decref(_M0L3dstS36);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1820 = _M0L3lenS41 - 1;
    int32_t _M0L1iS43 = _M0L6_2atmpS1820;
    while (1) {
      if (_M0L1iS43 >= 0) {
        int32_t _M0L6_2atmpS1816 = _M0L11dst__offsetS38 + _M0L1iS43;
        int32_t _M0L6_2atmpS1818 = _M0L11src__offsetS39 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS4266;
        moonbit_string_t _M0L6_2atmpS1817;
        moonbit_string_t _M0L6_2aoldS4265;
        int32_t _M0L6_2atmpS1819;
        if (
          _M0L6_2atmpS1818 < 0
          || _M0L6_2atmpS1818 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4266 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1818];
        _M0L6_2atmpS1817 = _M0L6_2atmpS4266;
        if (
          _M0L6_2atmpS1816 < 0
          || _M0L6_2atmpS1816 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4265 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1816];
        moonbit_incref(_M0L6_2atmpS1817);
        moonbit_decref(_M0L6_2aoldS4265);
        _M0L3dstS36[_M0L6_2atmpS1816] = _M0L6_2atmpS1817;
        _M0L6_2atmpS1819 = _M0L1iS43 - 1;
        _M0L1iS43 = _M0L6_2atmpS1819;
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
  int32_t _if__result_4716;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS45 == _M0L3srcS46) {
    _if__result_4716 = _M0L11dst__offsetS47 < _M0L11src__offsetS48;
  } else {
    _if__result_4716 = 0;
  }
  if (_if__result_4716) {
    int32_t _M0L1iS49 = 0;
    while (1) {
      if (_M0L1iS49 < _M0L3lenS50) {
        int32_t _M0L6_2atmpS1821 = _M0L11dst__offsetS47 + _M0L1iS49;
        int32_t _M0L6_2atmpS1823 = _M0L11src__offsetS48 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS4268;
        struct _M0TUsiE* _M0L6_2atmpS1822;
        struct _M0TUsiE* _M0L6_2aoldS4267;
        int32_t _M0L6_2atmpS1824;
        if (
          _M0L6_2atmpS1823 < 0
          || _M0L6_2atmpS1823 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4268 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1823];
        _M0L6_2atmpS1822 = _M0L6_2atmpS4268;
        if (
          _M0L6_2atmpS1821 < 0
          || _M0L6_2atmpS1821 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4267 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1821];
        if (_M0L6_2atmpS1822) {
          moonbit_incref(_M0L6_2atmpS1822);
        }
        if (_M0L6_2aoldS4267) {
          moonbit_decref(_M0L6_2aoldS4267);
        }
        _M0L3dstS45[_M0L6_2atmpS1821] = _M0L6_2atmpS1822;
        _M0L6_2atmpS1824 = _M0L1iS49 + 1;
        _M0L1iS49 = _M0L6_2atmpS1824;
        continue;
      } else {
        moonbit_decref(_M0L3srcS46);
        moonbit_decref(_M0L3dstS45);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1829 = _M0L3lenS50 - 1;
    int32_t _M0L1iS52 = _M0L6_2atmpS1829;
    while (1) {
      if (_M0L1iS52 >= 0) {
        int32_t _M0L6_2atmpS1825 = _M0L11dst__offsetS47 + _M0L1iS52;
        int32_t _M0L6_2atmpS1827 = _M0L11src__offsetS48 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS4270;
        struct _M0TUsiE* _M0L6_2atmpS1826;
        struct _M0TUsiE* _M0L6_2aoldS4269;
        int32_t _M0L6_2atmpS1828;
        if (
          _M0L6_2atmpS1827 < 0
          || _M0L6_2atmpS1827 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4270 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1827];
        _M0L6_2atmpS1826 = _M0L6_2atmpS4270;
        if (
          _M0L6_2atmpS1825 < 0
          || _M0L6_2atmpS1825 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4269 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1825];
        if (_M0L6_2atmpS1826) {
          moonbit_incref(_M0L6_2atmpS1826);
        }
        if (_M0L6_2aoldS4269) {
          moonbit_decref(_M0L6_2aoldS4269);
        }
        _M0L3dstS45[_M0L6_2atmpS1825] = _M0L6_2atmpS1826;
        _M0L6_2atmpS1828 = _M0L1iS52 - 1;
        _M0L1iS52 = _M0L6_2atmpS1828;
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
  int32_t _if__result_4719;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS54 == _M0L3srcS55) {
    _if__result_4719 = _M0L11dst__offsetS56 < _M0L11src__offsetS57;
  } else {
    _if__result_4719 = 0;
  }
  if (_if__result_4719) {
    int32_t _M0L1iS58 = 0;
    while (1) {
      if (_M0L1iS58 < _M0L3lenS59) {
        int32_t _M0L6_2atmpS1830 = _M0L11dst__offsetS56 + _M0L1iS58;
        int32_t _M0L6_2atmpS1832 = _M0L11src__offsetS57 + _M0L1iS58;
        void* _M0L6_2atmpS4272;
        void* _M0L6_2atmpS1831;
        void* _M0L6_2aoldS4271;
        int32_t _M0L6_2atmpS1833;
        if (
          _M0L6_2atmpS1832 < 0
          || _M0L6_2atmpS1832 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4272 = (void*)_M0L3srcS55[_M0L6_2atmpS1832];
        _M0L6_2atmpS1831 = _M0L6_2atmpS4272;
        if (
          _M0L6_2atmpS1830 < 0
          || _M0L6_2atmpS1830 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4271 = (void*)_M0L3dstS54[_M0L6_2atmpS1830];
        moonbit_incref(_M0L6_2atmpS1831);
        moonbit_decref(_M0L6_2aoldS4271);
        _M0L3dstS54[_M0L6_2atmpS1830] = _M0L6_2atmpS1831;
        _M0L6_2atmpS1833 = _M0L1iS58 + 1;
        _M0L1iS58 = _M0L6_2atmpS1833;
        continue;
      } else {
        moonbit_decref(_M0L3srcS55);
        moonbit_decref(_M0L3dstS54);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1838 = _M0L3lenS59 - 1;
    int32_t _M0L1iS61 = _M0L6_2atmpS1838;
    while (1) {
      if (_M0L1iS61 >= 0) {
        int32_t _M0L6_2atmpS1834 = _M0L11dst__offsetS56 + _M0L1iS61;
        int32_t _M0L6_2atmpS1836 = _M0L11src__offsetS57 + _M0L1iS61;
        void* _M0L6_2atmpS4274;
        void* _M0L6_2atmpS1835;
        void* _M0L6_2aoldS4273;
        int32_t _M0L6_2atmpS1837;
        if (
          _M0L6_2atmpS1836 < 0
          || _M0L6_2atmpS1836 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4274 = (void*)_M0L3srcS55[_M0L6_2atmpS1836];
        _M0L6_2atmpS1835 = _M0L6_2atmpS4274;
        if (
          _M0L6_2atmpS1834 < 0
          || _M0L6_2atmpS1834 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4273 = (void*)_M0L3dstS54[_M0L6_2atmpS1834];
        moonbit_incref(_M0L6_2atmpS1835);
        moonbit_decref(_M0L6_2aoldS4273);
        _M0L3dstS54[_M0L6_2atmpS1834] = _M0L6_2atmpS1835;
        _M0L6_2atmpS1837 = _M0L1iS61 - 1;
        _M0L1iS61 = _M0L6_2atmpS1837;
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
  moonbit_string_t _M0L6_2atmpS1787;
  moonbit_string_t _M0L6_2atmpS4277;
  moonbit_string_t _M0L6_2atmpS1785;
  moonbit_string_t _M0L6_2atmpS1786;
  moonbit_string_t _M0L6_2atmpS4276;
  moonbit_string_t _M0L6_2atmpS1784;
  moonbit_string_t _M0L6_2atmpS4275;
  moonbit_string_t _M0L6_2atmpS1783;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1787 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4277
  = moonbit_add_string(_M0L6_2atmpS1787, (moonbit_string_t)moonbit_string_literal_99.data);
  moonbit_decref(_M0L6_2atmpS1787);
  _M0L6_2atmpS1785 = _M0L6_2atmpS4277;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1786
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4276 = moonbit_add_string(_M0L6_2atmpS1785, _M0L6_2atmpS1786);
  moonbit_decref(_M0L6_2atmpS1785);
  moonbit_decref(_M0L6_2atmpS1786);
  _M0L6_2atmpS1784 = _M0L6_2atmpS4276;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4275
  = moonbit_add_string(_M0L6_2atmpS1784, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS1784);
  _M0L6_2atmpS1783 = _M0L6_2atmpS4275;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1783);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1792;
  moonbit_string_t _M0L6_2atmpS4280;
  moonbit_string_t _M0L6_2atmpS1790;
  moonbit_string_t _M0L6_2atmpS1791;
  moonbit_string_t _M0L6_2atmpS4279;
  moonbit_string_t _M0L6_2atmpS1789;
  moonbit_string_t _M0L6_2atmpS4278;
  moonbit_string_t _M0L6_2atmpS1788;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1792 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4280
  = moonbit_add_string(_M0L6_2atmpS1792, (moonbit_string_t)moonbit_string_literal_99.data);
  moonbit_decref(_M0L6_2atmpS1792);
  _M0L6_2atmpS1790 = _M0L6_2atmpS4280;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1791
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4279 = moonbit_add_string(_M0L6_2atmpS1790, _M0L6_2atmpS1791);
  moonbit_decref(_M0L6_2atmpS1790);
  moonbit_decref(_M0L6_2atmpS1791);
  _M0L6_2atmpS1789 = _M0L6_2atmpS4279;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4278
  = moonbit_add_string(_M0L6_2atmpS1789, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS1789);
  _M0L6_2atmpS1788 = _M0L6_2atmpS4278;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1788);
  return 0;
}

moonbit_string_t _M0FPB5abortGsE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1797;
  moonbit_string_t _M0L6_2atmpS4283;
  moonbit_string_t _M0L6_2atmpS1795;
  moonbit_string_t _M0L6_2atmpS1796;
  moonbit_string_t _M0L6_2atmpS4282;
  moonbit_string_t _M0L6_2atmpS1794;
  moonbit_string_t _M0L6_2atmpS4281;
  moonbit_string_t _M0L6_2atmpS1793;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1797 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4283
  = moonbit_add_string(_M0L6_2atmpS1797, (moonbit_string_t)moonbit_string_literal_99.data);
  moonbit_decref(_M0L6_2atmpS1797);
  _M0L6_2atmpS1795 = _M0L6_2atmpS4283;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1796
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4282 = moonbit_add_string(_M0L6_2atmpS1795, _M0L6_2atmpS1796);
  moonbit_decref(_M0L6_2atmpS1795);
  moonbit_decref(_M0L6_2atmpS1796);
  _M0L6_2atmpS1794 = _M0L6_2atmpS4282;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4281
  = moonbit_add_string(_M0L6_2atmpS1794, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS1794);
  _M0L6_2atmpS1793 = _M0L6_2atmpS4281;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGsE(_M0L6_2atmpS1793);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1802;
  moonbit_string_t _M0L6_2atmpS4286;
  moonbit_string_t _M0L6_2atmpS1800;
  moonbit_string_t _M0L6_2atmpS1801;
  moonbit_string_t _M0L6_2atmpS4285;
  moonbit_string_t _M0L6_2atmpS1799;
  moonbit_string_t _M0L6_2atmpS4284;
  moonbit_string_t _M0L6_2atmpS1798;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1802 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4286
  = moonbit_add_string(_M0L6_2atmpS1802, (moonbit_string_t)moonbit_string_literal_99.data);
  moonbit_decref(_M0L6_2atmpS1802);
  _M0L6_2atmpS1800 = _M0L6_2atmpS4286;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1801
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4285 = moonbit_add_string(_M0L6_2atmpS1800, _M0L6_2atmpS1801);
  moonbit_decref(_M0L6_2atmpS1800);
  moonbit_decref(_M0L6_2atmpS1801);
  _M0L6_2atmpS1799 = _M0L6_2atmpS4285;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4284
  = moonbit_add_string(_M0L6_2atmpS1799, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS1799);
  _M0L6_2atmpS1798 = _M0L6_2atmpS4284;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1798);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5valueS18
) {
  uint32_t _M0L3accS1782;
  uint32_t _M0L6_2atmpS1781;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1782 = _M0L4selfS17->$0;
  _M0L6_2atmpS1781 = _M0L3accS1782 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1781;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1779;
  uint32_t _M0L6_2atmpS1780;
  uint32_t _M0L6_2atmpS1778;
  uint32_t _M0L6_2atmpS1777;
  uint32_t _M0L6_2atmpS1776;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1779 = _M0L4selfS15->$0;
  _M0L6_2atmpS1780 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1778 = _M0L3accS1779 + _M0L6_2atmpS1780;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1777 = _M0FPB4rotl(_M0L6_2atmpS1778, 17);
  _M0L6_2atmpS1776 = _M0L6_2atmpS1777 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1776;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1773;
  int32_t _M0L6_2atmpS1775;
  uint32_t _M0L6_2atmpS1774;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1773 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1775 = 32 - _M0L1rS14;
  _M0L6_2atmpS1774 = _M0L1xS13 >> (_M0L6_2atmpS1775 & 31);
  return _M0L6_2atmpS1773 | _M0L6_2atmpS1774;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S9,
  struct _M0TPB6Logger _M0L10_2ax__4934S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS4287;
  int32_t _M0L6_2acntS4488;
  moonbit_string_t _M0L15_2a_2aarg__4935S11;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S9;
  _M0L8_2afieldS4287 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS4488 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS4488 > 1) {
    int32_t _M0L11_2anew__cntS4489 = _M0L6_2acntS4488 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS4489;
    moonbit_incref(_M0L8_2afieldS4287);
  } else if (_M0L6_2acntS4488 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__4935S11 = _M0L8_2afieldS4287;
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_100.data);
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S12, _M0L15_2a_2aarg__4935S11);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_101.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS8) {
  void* _block_4722;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4722 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4722)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4722)->$0 = _M0L4selfS8;
  return _block_4722;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS7) {
  void* _block_4723;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4723 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4723)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4723)->$0 = _M0L5arrayS7;
  return _block_4723;
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

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t _M0L3msgS3) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1736) {
  switch (Moonbit_object_tag(_M0L4_2aeS1736)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1736);
      return (moonbit_string_t)moonbit_string_literal_102.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1736);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1736);
      return (moonbit_string_t)moonbit_string_literal_103.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1736);
      return (moonbit_string_t)moonbit_string_literal_104.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1736);
      return (moonbit_string_t)moonbit_string_literal_105.data;
      break;
    }
  }
}

void* _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1755
) {
  void* _M0L7_2aselfS1754 = (void*)_M0L11_2aobj__ptrS1755;
  return _M0IP48clawteam8clawteam8internal6schema6SchemaPB6ToJson8to__json(_M0L7_2aselfS1754);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1753,
  int32_t _M0L8_2aparamS1752
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1751 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1753;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1751, _M0L8_2aparamS1752);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1750,
  struct _M0TPC16string10StringView _M0L8_2aparamS1749
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1748 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1750;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1748, _M0L8_2aparamS1749);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1747,
  moonbit_string_t _M0L8_2aparamS1744,
  int32_t _M0L8_2aparamS1745,
  int32_t _M0L8_2aparamS1746
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1743 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1747;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1743, _M0L8_2aparamS1744, _M0L8_2aparamS1745, _M0L8_2aparamS1746);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1742,
  moonbit_string_t _M0L8_2aparamS1741
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1740 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1742;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1740, _M0L8_2aparamS1741);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1772 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1771;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1767;
  moonbit_string_t* _M0L6_2atmpS1770;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1769;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1768;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1663;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1766;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1765;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1764;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1763;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1662;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1762;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1761;
  _M0L6_2atmpS1772[0] = (moonbit_string_t)moonbit_string_literal_106.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal6schema39____test__736368656d612e6d6274__0_2eclo);
  _M0L8_2atupleS1771
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1771)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1771->$0
  = _M0FP48clawteam8clawteam8internal6schema39____test__736368656d612e6d6274__0_2eclo;
  _M0L8_2atupleS1771->$1 = _M0L6_2atmpS1772;
  _M0L8_2atupleS1767
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1767)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1767->$0 = 0;
  _M0L8_2atupleS1767->$1 = _M0L8_2atupleS1771;
  _M0L6_2atmpS1770 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1770[0] = (moonbit_string_t)moonbit_string_literal_107.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal6schema39____test__736368656d612e6d6274__1_2eclo);
  _M0L8_2atupleS1769
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1769)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1769->$0
  = _M0FP48clawteam8clawteam8internal6schema39____test__736368656d612e6d6274__1_2eclo;
  _M0L8_2atupleS1769->$1 = _M0L6_2atmpS1770;
  _M0L8_2atupleS1768
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1768)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1768->$0 = 1;
  _M0L8_2atupleS1768->$1 = _M0L8_2atupleS1769;
  _M0L7_2abindS1663
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1663[0] = _M0L8_2atupleS1767;
  _M0L7_2abindS1663[1] = _M0L8_2atupleS1768;
  _M0L6_2atmpS1766 = _M0L7_2abindS1663;
  _M0L6_2atmpS1765
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 2, _M0L6_2atmpS1766
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1764
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1765);
  _M0L8_2atupleS1763
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1763)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1763->$0 = (moonbit_string_t)moonbit_string_literal_108.data;
  _M0L8_2atupleS1763->$1 = _M0L6_2atmpS1764;
  _M0L7_2abindS1662
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1662[0] = _M0L8_2atupleS1763;
  _M0L6_2atmpS1762 = _M0L7_2abindS1662;
  _M0L6_2atmpS1761
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS1762
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal6schema48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1761);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1760;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1730;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1731;
  int32_t _M0L7_2abindS1732;
  int32_t _M0L2__S1733;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1760
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1730
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1730)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1730->$0 = _M0L6_2atmpS1760;
  _M0L12async__testsS1730->$1 = 0;
  #line 439 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1731
  = _M0FP48clawteam8clawteam8internal6schema52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1732 = _M0L7_2abindS1731->$1;
  _M0L2__S1733 = 0;
  while (1) {
    if (_M0L2__S1733 < _M0L7_2abindS1732) {
      struct _M0TUsiE** _M0L8_2afieldS4291 = _M0L7_2abindS1731->$0;
      struct _M0TUsiE** _M0L3bufS1759 = _M0L8_2afieldS4291;
      struct _M0TUsiE* _M0L6_2atmpS4290 =
        (struct _M0TUsiE*)_M0L3bufS1759[_M0L2__S1733];
      struct _M0TUsiE* _M0L3argS1734 = _M0L6_2atmpS4290;
      moonbit_string_t _M0L8_2afieldS4289 = _M0L3argS1734->$0;
      moonbit_string_t _M0L6_2atmpS1756 = _M0L8_2afieldS4289;
      int32_t _M0L8_2afieldS4288 = _M0L3argS1734->$1;
      int32_t _M0L6_2atmpS1757 = _M0L8_2afieldS4288;
      int32_t _M0L6_2atmpS1758;
      moonbit_incref(_M0L6_2atmpS1756);
      moonbit_incref(_M0L12async__testsS1730);
      #line 440 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal6schema44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1730, _M0L6_2atmpS1756, _M0L6_2atmpS1757);
      _M0L6_2atmpS1758 = _M0L2__S1733 + 1;
      _M0L2__S1733 = _M0L6_2atmpS1758;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1731);
    }
    break;
  }
  #line 442 "E:\\moonbit\\clawteam\\internal\\schema\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal6schema28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6schema34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1730);
  return 0;
}