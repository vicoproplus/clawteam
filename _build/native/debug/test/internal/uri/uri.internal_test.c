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
struct _M0DTPC16result6ResultGRPC16string10StringViewRP48clawteam8clawteam8internal3uri10ParseErrorE2Ok;

struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6Logger;

struct _M0R38String_3a_3aiter_2eanon__u2560__l247__;

struct _M0TPB19MulShiftAll64Result;

struct _M0TPB5ArrayGRPC16string10StringViewE;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPC13ref3RefGRPC16string10StringViewE;

struct _M0TPB6ToJson;

struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474;

struct _M0TWEOs;

struct _M0KTPB6ToJsonTPC16string10StringView;

struct _M0TPB4Show;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0DTPC15error5Error64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eExtraContent;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC16result6ResultGRPC16string10StringViewRP48clawteam8clawteam8internal3uri10ParseErrorE3Err;

struct _M0TPC13ref3RefGORPC16string10StringViewE;

struct _M0TP48clawteam8clawteam8internal3uri3Uri;

struct _M0TPB9ArrayViewGsE;

struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__;

struct _M0TP48clawteam8clawteam8internal3uri9Authority;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE2Ok;

struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TPC16buffer6Buffer;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__;

struct _M0KTPB4ShowTPC16string10StringView;

struct _M0DTPC15error5Error106clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TUiRPC16string10StringViewE;

struct _M0TWEuQRPC15error5Error;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal3uri33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__;

struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE3Err;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal3uri33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB9ArrayViewGRPC16string10StringViewE;

struct _M0DTPB4Json6Object;

struct _M0BTPB4Show;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0DTPC16result6ResultGuRPB7FailureE3Err;

struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376;

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0DTPC16result6ResultGRPC16string10StringViewRP48clawteam8clawteam8internal3uri10ParseErrorE2Ok {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
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

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2560__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPB4Json6Number {
  double $0;
  moonbit_string_t $1;
  
};

struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352 {
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* $0;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* $1;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPC13ref3RefGRPC16string10StringViewE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474 {
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* $0;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0KTPB6ToJsonTPC16string10StringView {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eExtraContent {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
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

struct _M0DTPC16result6ResultGRPC16string10StringViewRP48clawteam8clawteam8internal3uri10ParseErrorE3Err {
  void* $0;
  
};

struct _M0TPC13ref3RefGORPC16string10StringViewE {
  void* $0;
  
};

struct _M0TP48clawteam8clawteam8internal3uri3Uri {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* $1;
  struct _M0TPB5ArrayGRPC16string10StringViewE* $2;
  void* $3;
  void* $4;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TP48clawteam8clawteam8internal3uri9Authority {
  int32_t $1_1;
  int32_t $1_2;
  int64_t $2;
  void* $0;
  moonbit_string_t $1_0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE2Ok {
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* $0;
  
};

struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  int32_t $0_1;
  int32_t $0_2;
  struct _M0TPC16string10StringView* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0TPC16buffer6Buffer {
  int32_t $1;
  moonbit_bytes_t $0;
  
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

struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0KTPB4ShowTPC16string10StringView {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTPC15error5Error106clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TUiRPC16string10StringViewE {
  int32_t $0;
  int32_t $1_1;
  int32_t $1_2;
  moonbit_string_t $1_0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal3uri33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPB4Json6String {
  moonbit_string_t $0;
  
};

struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal3uri33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB9ArrayViewGRPC16string10StringViewE {
  int32_t $1;
  int32_t $2;
  struct _M0TPC16string10StringView* $0;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
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

struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376 {
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352* $0;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* $1;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* $2;
  
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

struct moonbit_result_2 {
  int tag;
  union {
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* ok;
    void* err;
    
  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_1 {
  int tag;
  union { struct _M0TPC16string10StringView ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN17error__to__stringS1656(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN14handle__resultS1647(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testC4145l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testC4141l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal3uri45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1581(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1576(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1563(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal3uri28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal3uri34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__2(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__1(
  
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri10parse__uri(
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*,
  struct _M0TPC16string10StringView
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri10parse__uriN13parse__schemeS1474(
  struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474*,
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri10parse__uriN17parse__hier__partS1463(
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*,
  struct _M0TPC16string10StringView
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri16parse__authority(
  struct _M0TPC16string10StringView,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__hostS1376(
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376*,
  struct _M0TPC16string10StringView
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__portS1352(
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352*,
  struct _M0TPC16string10StringView
);

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam8internal3uri20parse__path__abempty(
  struct _M0TPC16string10StringView,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*
);

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam8internal3uri12parse__query(
  struct _M0TPC16string10StringView,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*
);

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam8internal3uri15parse__fragment(
  struct _M0TPC16string10StringView,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*
);

struct _M0TPC16string10StringView _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__0(
  
);

struct moonbit_result_1 _M0MP48clawteam8clawteam8internal3uri3Uri13parse__scheme(
  struct _M0TP48clawteam8clawteam8internal3uri3Uri*,
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

int32_t _M0IPC16buffer6BufferPB6Logger11write__char(
  struct _M0TPC16buffer6Buffer*,
  int32_t
);

struct _M0TPC16buffer6Buffer* _M0FPC16buffer11new_2einner(int32_t);

int32_t _M0MPC16buffer6Buffer11write__byte(
  struct _M0TPC16buffer6Buffer*,
  int32_t
);

int32_t _M0MPC16buffer6Buffer19grow__if__necessary(
  struct _M0TPC16buffer6Buffer*,
  int32_t
);

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

int32_t _M0IPC15array5ArrayPB2Eq5equalGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPB5ArrayGRPC16string10StringViewE*
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

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2779l591(
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

struct moonbit_result_2 _M0MPC16option6Option17unwrap__or__errorGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE(
  struct _M0TP48clawteam8clawteam8internal3uri9Authority*,
  void*
);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0IPC16option6OptionPB2Eq5equalGRPC16string10StringViewE(
  void*,
  void*
);

int32_t _M0IPC16option6OptionPB2Eq5equalGiE(int64_t, int64_t);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

int32_t _M0IPC15array5ArrayPB4Show6outputGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPB6Logger
);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC15array5Array4iterGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC15array9ArrayView4iterGRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGRPC16string10StringViewE
);

void* _M0MPC15array9ArrayView4iterGRPC16string10StringViewEC2591l570(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2579l570(struct _M0TWEOs*);

int32_t _M0IPC16option6OptionPB4Show6outputGRPC16string10StringViewE(
  void*,
  struct _M0TPB6Logger
);

int32_t _M0IPC16option6OptionPB4Show6outputGiE(int64_t, struct _M0TPB6Logger);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2560l247(struct _M0TWEOc*);

int32_t _M0MPC16string10StringView9is__empty(
  struct _M0TPC16string10StringView
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

int32_t _M0MPC15array9ArrayView6lengthGRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGRPC16string10StringViewE
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

struct moonbit_result_0 _M0FPB10assert__eqGRPC16string10StringViewE(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB10assert__eqGORPC16string10StringViewE(
  void*,
  void*,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB10assert__eqGOiE(
  int64_t,
  int64_t,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB10assert__eqGRPB5ArrayGRPC16string10StringViewEE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB4failGuE(moonbit_string_t, moonbit_string_t);

moonbit_string_t _M0FPB13debug__stringGRPC16string10StringViewE(
  struct _M0TPC16string10StringView
);

moonbit_string_t _M0FPB13debug__stringGORPC16string10StringViewE(void*);

moonbit_string_t _M0FPB13debug__stringGOiE(int64_t);

moonbit_string_t _M0FPB13debug__stringGRPB5ArrayGRPC16string10StringViewEE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
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

int32_t _M0MPB6Logger19write__iter_2einnerGRPC16string10StringViewE(
  struct _M0TPB6Logger,
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

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

int32_t _M0IP016_24default__implPB2Eq10not__equalGRPC16string10StringViewE(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB2Eq10not__equalGORPC16string10StringViewE(
  void*,
  void*
);

int32_t _M0IP016_24default__implPB2Eq10not__equalGOiE(int64_t, int64_t);

int32_t _M0IP016_24default__implPB2Eq10not__equalGRPB5ArrayGRPC16string10StringViewEE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*,
  struct _M0TPB5ArrayGRPC16string10StringViewE*
);

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

uint64_t _M0MPC13int3Int10to__uint64(int32_t);

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

struct _M0TPC16string10StringView* _M0MPC15array5Array6bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE*
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

int32_t _M0MPB6Logger13write__objectGRPC16string10StringViewE(
  struct _M0TPB6Logger,
  struct _M0TPC16string10StringView
);

int32_t _M0MPB6Logger13write__objectGiE(struct _M0TPB6Logger, int32_t);

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

moonbit_string_t _M0IPC16string10StringViewPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC16string10StringViewPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
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

void* _M0IPC16string10StringViewPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    112, 97, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 48, 54, 58, 
    51, 45, 51, 48, 54, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 73, 110, 
    118, 97, 108, 105, 100, 83, 99, 104, 101, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 54, 55, 58, 51, 
    45, 55, 48, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    78, 111, 110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_77 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 50, 57, 58, 
    49, 51, 45, 51, 50, 57, 58, 49, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    117, 115, 101, 114, 110, 97, 109, 101, 58, 112, 97, 115, 115, 119, 
    111, 114, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 50, 54, 58, 57, 45, 
    52, 50, 54, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_57 =
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
} const moonbit_string_literal_89 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 55, 49, 58, 49, 
    54, 45, 55, 49, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    104, 116, 116, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_67 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 50, 56, 58, 
    53, 45, 51, 50, 56, 58, 56, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    83, 111, 109, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 49, 49, 58, 
    51, 45, 51, 49, 49, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    77, 105, 115, 115, 105, 110, 103, 32, 97, 117, 116, 104, 111, 114, 
    105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 69, 120, 
    116, 114, 97, 67, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 55, 48, 58, 53, 45, 55, 
    48, 58, 54, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 55, 49, 58, 51, 
    45, 55, 49, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    114, 101, 115, 111, 117, 114, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 77, 105, 
    115, 115, 105, 110, 103, 83, 99, 104, 101, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    102, 114, 97, 103, 109, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 48, 48, 58, 
    51, 45, 51, 48, 53, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_81 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 51, 55, 58, 
    51, 45, 51, 51, 55, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 54, 57, 58, 49, 
    51, 45, 54, 57, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 48, 49, 58, 
    53, 45, 51, 48, 51, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 49, 50, 58, 
    51, 45, 51, 49, 50, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 50, 55, 58, 
    51, 45, 51, 51, 48, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 51, 49, 58, 
    51, 45, 51, 51, 49, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 105, 34, 
    44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 49, 53, 58, 
    51, 45, 51, 49, 53, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 73, 110, 
    118, 97, 108, 105, 100, 80, 101, 114, 99, 101, 110, 116, 69, 110, 
    99, 111, 100, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[97]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 96), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    32, 33, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 49, 48, 58, 
    51, 45, 51, 49, 48, 58, 53, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    117, 114, 105, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 45), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 69, 111, 
    102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 54, 56, 58, 53, 
    45, 54, 56, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 77, 105, 
    115, 115, 105, 110, 103, 67, 111, 108, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 51, 54, 58, 
    51, 45, 51, 51, 54, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 
    49, 51, 58, 53, 45, 49, 49, 51, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_84 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[99]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 98), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 
    107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 51, 57, 58, 
    51, 45, 51, 51, 57, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 49, 52, 58, 
    51, 45, 51, 49, 52, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_58 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[39]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 38), 
    104, 116, 116, 112, 58, 47, 47, 101, 120, 97, 109, 112, 108, 101, 
    46, 99, 111, 109, 47, 112, 97, 116, 104, 63, 113, 117, 101, 114, 
    121, 35, 102, 114, 97, 103, 109, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 55, 49, 58, 51, 
    54, 45, 55, 49, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 51, 53, 58, 
    51, 45, 51, 51, 53, 58, 53, 57, 0
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
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    116, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 52, 48, 58, 
    51, 45, 51, 52, 48, 58, 51, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 51, 56, 58, 
    51, 45, 51, 51, 56, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_99 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    104, 116, 116, 112, 58, 47, 47, 117, 115, 101, 114, 110, 97, 109, 
    101, 58, 112, 97, 115, 115, 119, 111, 114, 100, 64, 101, 120, 97, 
    109, 112, 108, 101, 46, 99, 111, 109, 58, 56, 52, 52, 51, 47, 112, 
    97, 116, 104, 47, 116, 111, 47, 114, 101, 115, 111, 117, 114, 99, 
    101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 48, 52, 58, 
    49, 51, 45, 51, 48, 52, 58, 49, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[34]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 33), 
    47, 47, 101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, 47, 112, 
    97, 116, 104, 63, 113, 117, 101, 114, 121, 35, 102, 114, 97, 103, 
    109, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[84]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 83), 
    104, 116, 116, 112, 58, 47, 47, 117, 115, 101, 114, 110, 97, 109, 
    101, 58, 112, 97, 115, 115, 119, 111, 114, 100, 64, 101, 120, 97, 
    109, 112, 108, 101, 46, 99, 111, 109, 58, 56, 52, 52, 51, 47, 112, 
    97, 116, 104, 47, 116, 111, 47, 114, 101, 115, 111, 117, 114, 99, 
    101, 63, 113, 117, 101, 114, 121, 61, 112, 97, 114, 97, 109, 101, 
    116, 101, 114, 35, 102, 114, 97, 103, 109, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_106 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 56, 49, 58, 57, 45, 56, 
    49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    113, 117, 101, 114, 121, 61, 112, 97, 114, 97, 109, 101, 116, 101, 
    114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 114, 
    105, 46, 80, 97, 114, 115, 101, 69, 114, 114, 111, 114, 46, 73, 110, 
    118, 97, 108, 105, 100, 80, 111, 114, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 117, 
    114, 105, 58, 117, 114, 105, 46, 109, 98, 116, 58, 51, 49, 51, 58, 
    51, 45, 51, 49, 51, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
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

struct moonbit_object const moonbit_constant_constructor_5 =
  { -1, Moonbit_make_regular_object_header(2, 0, 5)};

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct moonbit_object const moonbit_constant_constructor_4 =
  { -1, Moonbit_make_regular_object_header(2, 0, 4)};

struct moonbit_object const moonbit_constant_constructor_2 =
  { -1, Moonbit_make_regular_object_header(2, 0, 2)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN17error__to__stringS1656$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN17error__to__stringS1656
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__1_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0115moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC16string10StringViewPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0115moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0115moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FP0113moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC16string10StringViewPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC16string10StringViewPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP0113moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP0113moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1657 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1119$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1119 =
  &_M0FPB31ryu__to__string_2erecord_2f1119$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal3uri48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS4178
) {
  return _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__1();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS4177
) {
  return _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri37____test__7572692e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS4176
) {
  return _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__2();
}

int32_t _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1677,
  moonbit_string_t _M0L8filenameS1652,
  int32_t _M0L5indexS1655
) {
  struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647* _closure_4863;
  struct _M0TWssbEu* _M0L14handle__resultS1647;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1656;
  void* _M0L11_2atry__errS1671;
  struct moonbit_result_0 _tmp_4865;
  int32_t _handle__error__result_4866;
  int32_t _M0L6_2atmpS4164;
  void* _M0L3errS1672;
  moonbit_string_t _M0L4nameS1674;
  struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1675;
  moonbit_string_t _M0L8_2afieldS4179;
  int32_t _M0L6_2acntS4714;
  moonbit_string_t _M0L7_2anameS1676;
  #line 526 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1652);
  _closure_4863
  = (struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647*)moonbit_malloc(sizeof(struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647));
  Moonbit_object_header(_closure_4863)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647, $1) >> 2, 1, 0);
  _closure_4863->code
  = &_M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN14handle__resultS1647;
  _closure_4863->$0 = _M0L5indexS1655;
  _closure_4863->$1 = _M0L8filenameS1652;
  _M0L14handle__resultS1647 = (struct _M0TWssbEu*)_closure_4863;
  _M0L17error__to__stringS1656
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN17error__to__stringS1656$closure.data;
  moonbit_incref(_M0L12async__testsS1677);
  moonbit_incref(_M0L17error__to__stringS1656);
  moonbit_incref(_M0L8filenameS1652);
  moonbit_incref(_M0L14handle__resultS1647);
  #line 560 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _tmp_4865
  = _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__test(_M0L12async__testsS1677, _M0L8filenameS1652, _M0L5indexS1655, _M0L14handle__resultS1647, _M0L17error__to__stringS1656);
  if (_tmp_4865.tag) {
    int32_t const _M0L5_2aokS4173 = _tmp_4865.data.ok;
    _handle__error__result_4866 = _M0L5_2aokS4173;
  } else {
    void* const _M0L6_2aerrS4174 = _tmp_4865.data.err;
    moonbit_decref(_M0L12async__testsS1677);
    moonbit_decref(_M0L17error__to__stringS1656);
    moonbit_decref(_M0L8filenameS1652);
    _M0L11_2atry__errS1671 = _M0L6_2aerrS4174;
    goto join_1670;
  }
  if (_handle__error__result_4866) {
    moonbit_decref(_M0L12async__testsS1677);
    moonbit_decref(_M0L17error__to__stringS1656);
    moonbit_decref(_M0L8filenameS1652);
    _M0L6_2atmpS4164 = 1;
  } else {
    struct moonbit_result_0 _tmp_4867;
    int32_t _handle__error__result_4868;
    moonbit_incref(_M0L12async__testsS1677);
    moonbit_incref(_M0L17error__to__stringS1656);
    moonbit_incref(_M0L8filenameS1652);
    moonbit_incref(_M0L14handle__resultS1647);
    #line 563 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    _tmp_4867
    = _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1677, _M0L8filenameS1652, _M0L5indexS1655, _M0L14handle__resultS1647, _M0L17error__to__stringS1656);
    if (_tmp_4867.tag) {
      int32_t const _M0L5_2aokS4171 = _tmp_4867.data.ok;
      _handle__error__result_4868 = _M0L5_2aokS4171;
    } else {
      void* const _M0L6_2aerrS4172 = _tmp_4867.data.err;
      moonbit_decref(_M0L12async__testsS1677);
      moonbit_decref(_M0L17error__to__stringS1656);
      moonbit_decref(_M0L8filenameS1652);
      _M0L11_2atry__errS1671 = _M0L6_2aerrS4172;
      goto join_1670;
    }
    if (_handle__error__result_4868) {
      moonbit_decref(_M0L12async__testsS1677);
      moonbit_decref(_M0L17error__to__stringS1656);
      moonbit_decref(_M0L8filenameS1652);
      _M0L6_2atmpS4164 = 1;
    } else {
      struct moonbit_result_0 _tmp_4869;
      int32_t _handle__error__result_4870;
      moonbit_incref(_M0L12async__testsS1677);
      moonbit_incref(_M0L17error__to__stringS1656);
      moonbit_incref(_M0L8filenameS1652);
      moonbit_incref(_M0L14handle__resultS1647);
      #line 566 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _tmp_4869
      = _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1677, _M0L8filenameS1652, _M0L5indexS1655, _M0L14handle__resultS1647, _M0L17error__to__stringS1656);
      if (_tmp_4869.tag) {
        int32_t const _M0L5_2aokS4169 = _tmp_4869.data.ok;
        _handle__error__result_4870 = _M0L5_2aokS4169;
      } else {
        void* const _M0L6_2aerrS4170 = _tmp_4869.data.err;
        moonbit_decref(_M0L12async__testsS1677);
        moonbit_decref(_M0L17error__to__stringS1656);
        moonbit_decref(_M0L8filenameS1652);
        _M0L11_2atry__errS1671 = _M0L6_2aerrS4170;
        goto join_1670;
      }
      if (_handle__error__result_4870) {
        moonbit_decref(_M0L12async__testsS1677);
        moonbit_decref(_M0L17error__to__stringS1656);
        moonbit_decref(_M0L8filenameS1652);
        _M0L6_2atmpS4164 = 1;
      } else {
        struct moonbit_result_0 _tmp_4871;
        int32_t _handle__error__result_4872;
        moonbit_incref(_M0L12async__testsS1677);
        moonbit_incref(_M0L17error__to__stringS1656);
        moonbit_incref(_M0L8filenameS1652);
        moonbit_incref(_M0L14handle__resultS1647);
        #line 569 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        _tmp_4871
        = _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1677, _M0L8filenameS1652, _M0L5indexS1655, _M0L14handle__resultS1647, _M0L17error__to__stringS1656);
        if (_tmp_4871.tag) {
          int32_t const _M0L5_2aokS4167 = _tmp_4871.data.ok;
          _handle__error__result_4872 = _M0L5_2aokS4167;
        } else {
          void* const _M0L6_2aerrS4168 = _tmp_4871.data.err;
          moonbit_decref(_M0L12async__testsS1677);
          moonbit_decref(_M0L17error__to__stringS1656);
          moonbit_decref(_M0L8filenameS1652);
          _M0L11_2atry__errS1671 = _M0L6_2aerrS4168;
          goto join_1670;
        }
        if (_handle__error__result_4872) {
          moonbit_decref(_M0L12async__testsS1677);
          moonbit_decref(_M0L17error__to__stringS1656);
          moonbit_decref(_M0L8filenameS1652);
          _M0L6_2atmpS4164 = 1;
        } else {
          struct moonbit_result_0 _tmp_4873;
          moonbit_incref(_M0L14handle__resultS1647);
          #line 572 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
          _tmp_4873
          = _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1677, _M0L8filenameS1652, _M0L5indexS1655, _M0L14handle__resultS1647, _M0L17error__to__stringS1656);
          if (_tmp_4873.tag) {
            int32_t const _M0L5_2aokS4165 = _tmp_4873.data.ok;
            _M0L6_2atmpS4164 = _M0L5_2aokS4165;
          } else {
            void* const _M0L6_2aerrS4166 = _tmp_4873.data.err;
            _M0L11_2atry__errS1671 = _M0L6_2aerrS4166;
            goto join_1670;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS4164) {
    void* _M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4175 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4175)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 9);
    ((struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4175)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1671
    = _M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4175;
    goto join_1670;
  } else {
    moonbit_decref(_M0L14handle__resultS1647);
  }
  goto joinlet_4864;
  join_1670:;
  _M0L3errS1672 = _M0L11_2atry__errS1671;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1675
  = (struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1672;
  _M0L8_2afieldS4179 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1675->$0;
  _M0L6_2acntS4714
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1675)->rc;
  if (_M0L6_2acntS4714 > 1) {
    int32_t _M0L11_2anew__cntS4715 = _M0L6_2acntS4714 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1675)->rc
    = _M0L11_2anew__cntS4715;
    moonbit_incref(_M0L8_2afieldS4179);
  } else if (_M0L6_2acntS4714 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1675);
  }
  _M0L7_2anameS1676 = _M0L8_2afieldS4179;
  _M0L4nameS1674 = _M0L7_2anameS1676;
  goto join_1673;
  goto joinlet_4874;
  join_1673:;
  #line 580 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN14handle__resultS1647(_M0L14handle__resultS1647, _M0L4nameS1674, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4874:;
  joinlet_4864:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN17error__to__stringS1656(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS4163,
  void* _M0L3errS1657
) {
  void* _M0L1eS1659;
  moonbit_string_t _M0L1eS1661;
  #line 549 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS4163);
  switch (Moonbit_object_tag(_M0L3errS1657)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1662 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1657;
      moonbit_string_t _M0L8_2afieldS4180 = _M0L10_2aFailureS1662->$0;
      int32_t _M0L6_2acntS4716 =
        Moonbit_object_header(_M0L10_2aFailureS1662)->rc;
      moonbit_string_t _M0L4_2aeS1663;
      if (_M0L6_2acntS4716 > 1) {
        int32_t _M0L11_2anew__cntS4717 = _M0L6_2acntS4716 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1662)->rc
        = _M0L11_2anew__cntS4717;
        moonbit_incref(_M0L8_2afieldS4180);
      } else if (_M0L6_2acntS4716 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1662);
      }
      _M0L4_2aeS1663 = _M0L8_2afieldS4180;
      _M0L1eS1661 = _M0L4_2aeS1663;
      goto join_1660;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1664 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1657;
      moonbit_string_t _M0L8_2afieldS4181 = _M0L15_2aInspectErrorS1664->$0;
      int32_t _M0L6_2acntS4718 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1664)->rc;
      moonbit_string_t _M0L4_2aeS1665;
      if (_M0L6_2acntS4718 > 1) {
        int32_t _M0L11_2anew__cntS4719 = _M0L6_2acntS4718 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1664)->rc
        = _M0L11_2anew__cntS4719;
        moonbit_incref(_M0L8_2afieldS4181);
      } else if (_M0L6_2acntS4718 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1664);
      }
      _M0L4_2aeS1665 = _M0L8_2afieldS4181;
      _M0L1eS1661 = _M0L4_2aeS1665;
      goto join_1660;
      break;
    }
    
    case 10: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1666 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1657;
      moonbit_string_t _M0L8_2afieldS4182 = _M0L16_2aSnapshotErrorS1666->$0;
      int32_t _M0L6_2acntS4720 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1666)->rc;
      moonbit_string_t _M0L4_2aeS1667;
      if (_M0L6_2acntS4720 > 1) {
        int32_t _M0L11_2anew__cntS4721 = _M0L6_2acntS4720 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1666)->rc
        = _M0L11_2anew__cntS4721;
        moonbit_incref(_M0L8_2afieldS4182);
      } else if (_M0L6_2acntS4720 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1666);
      }
      _M0L4_2aeS1667 = _M0L8_2afieldS4182;
      _M0L1eS1661 = _M0L4_2aeS1667;
      goto join_1660;
      break;
    }
    
    case 11: {
      struct _M0DTPC15error5Error106clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1668 =
        (struct _M0DTPC15error5Error106clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1657;
      moonbit_string_t _M0L8_2afieldS4183 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1668->$0;
      int32_t _M0L6_2acntS4722 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1668)->rc;
      moonbit_string_t _M0L4_2aeS1669;
      if (_M0L6_2acntS4722 > 1) {
        int32_t _M0L11_2anew__cntS4723 = _M0L6_2acntS4722 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1668)->rc
        = _M0L11_2anew__cntS4723;
        moonbit_incref(_M0L8_2afieldS4183);
      } else if (_M0L6_2acntS4722 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1668);
      }
      _M0L4_2aeS1669 = _M0L8_2afieldS4183;
      _M0L1eS1661 = _M0L4_2aeS1669;
      goto join_1660;
      break;
    }
    default: {
      _M0L1eS1659 = _M0L3errS1657;
      goto join_1658;
      break;
    }
  }
  join_1660:;
  return _M0L1eS1661;
  join_1658:;
  #line 555 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1659);
}

int32_t _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__executeN14handle__resultS1647(
  struct _M0TWssbEu* _M0L6_2aenvS4149,
  moonbit_string_t _M0L8testnameS1648,
  moonbit_string_t _M0L7messageS1649,
  int32_t _M0L7skippedS1650
) {
  struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647* _M0L14_2acasted__envS4150;
  moonbit_string_t _M0L8_2afieldS4193;
  moonbit_string_t _M0L8filenameS1652;
  int32_t _M0L8_2afieldS4192;
  int32_t _M0L6_2acntS4724;
  int32_t _M0L5indexS1655;
  int32_t _if__result_4877;
  moonbit_string_t _M0L10file__nameS1651;
  moonbit_string_t _M0L10test__nameS1653;
  moonbit_string_t _M0L7messageS1654;
  moonbit_string_t _M0L6_2atmpS4162;
  moonbit_string_t _M0L6_2atmpS4191;
  moonbit_string_t _M0L6_2atmpS4161;
  moonbit_string_t _M0L6_2atmpS4190;
  moonbit_string_t _M0L6_2atmpS4159;
  moonbit_string_t _M0L6_2atmpS4160;
  moonbit_string_t _M0L6_2atmpS4189;
  moonbit_string_t _M0L6_2atmpS4158;
  moonbit_string_t _M0L6_2atmpS4188;
  moonbit_string_t _M0L6_2atmpS4156;
  moonbit_string_t _M0L6_2atmpS4157;
  moonbit_string_t _M0L6_2atmpS4187;
  moonbit_string_t _M0L6_2atmpS4155;
  moonbit_string_t _M0L6_2atmpS4186;
  moonbit_string_t _M0L6_2atmpS4153;
  moonbit_string_t _M0L6_2atmpS4154;
  moonbit_string_t _M0L6_2atmpS4185;
  moonbit_string_t _M0L6_2atmpS4152;
  moonbit_string_t _M0L6_2atmpS4184;
  moonbit_string_t _M0L6_2atmpS4151;
  #line 533 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS4150
  = (struct _M0R110_24clawteam_2fclawteam_2finternal_2furi_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1647*)_M0L6_2aenvS4149;
  _M0L8_2afieldS4193 = _M0L14_2acasted__envS4150->$1;
  _M0L8filenameS1652 = _M0L8_2afieldS4193;
  _M0L8_2afieldS4192 = _M0L14_2acasted__envS4150->$0;
  _M0L6_2acntS4724 = Moonbit_object_header(_M0L14_2acasted__envS4150)->rc;
  if (_M0L6_2acntS4724 > 1) {
    int32_t _M0L11_2anew__cntS4725 = _M0L6_2acntS4724 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS4150)->rc
    = _M0L11_2anew__cntS4725;
    moonbit_incref(_M0L8filenameS1652);
  } else if (_M0L6_2acntS4724 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS4150);
  }
  _M0L5indexS1655 = _M0L8_2afieldS4192;
  if (!_M0L7skippedS1650) {
    _if__result_4877 = 1;
  } else {
    _if__result_4877 = 0;
  }
  if (_if__result_4877) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1651 = _M0MPC16string6String6escape(_M0L8filenameS1652);
  #line 540 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1653 = _M0MPC16string6String6escape(_M0L8testnameS1648);
  #line 541 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1654 = _M0MPC16string6String6escape(_M0L7messageS1649);
  #line 542 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4162
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1651);
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4191
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS4162);
  moonbit_decref(_M0L6_2atmpS4162);
  _M0L6_2atmpS4161 = _M0L6_2atmpS4191;
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4190
  = moonbit_add_string(_M0L6_2atmpS4161, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS4161);
  _M0L6_2atmpS4159 = _M0L6_2atmpS4190;
  #line 544 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4160
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1655);
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4189 = moonbit_add_string(_M0L6_2atmpS4159, _M0L6_2atmpS4160);
  moonbit_decref(_M0L6_2atmpS4159);
  moonbit_decref(_M0L6_2atmpS4160);
  _M0L6_2atmpS4158 = _M0L6_2atmpS4189;
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4188
  = moonbit_add_string(_M0L6_2atmpS4158, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS4158);
  _M0L6_2atmpS4156 = _M0L6_2atmpS4188;
  #line 544 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4157
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1653);
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4187 = moonbit_add_string(_M0L6_2atmpS4156, _M0L6_2atmpS4157);
  moonbit_decref(_M0L6_2atmpS4156);
  moonbit_decref(_M0L6_2atmpS4157);
  _M0L6_2atmpS4155 = _M0L6_2atmpS4187;
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4186
  = moonbit_add_string(_M0L6_2atmpS4155, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS4155);
  _M0L6_2atmpS4153 = _M0L6_2atmpS4186;
  #line 544 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4154
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1654);
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4185 = moonbit_add_string(_M0L6_2atmpS4153, _M0L6_2atmpS4154);
  moonbit_decref(_M0L6_2atmpS4153);
  moonbit_decref(_M0L6_2atmpS4154);
  _M0L6_2atmpS4152 = _M0L6_2atmpS4185;
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4184
  = moonbit_add_string(_M0L6_2atmpS4152, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS4152);
  _M0L6_2atmpS4151 = _M0L6_2atmpS4184;
  #line 543 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS4151);
  #line 546 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1646,
  moonbit_string_t _M0L8filenameS1643,
  int32_t _M0L5indexS1637,
  struct _M0TWssbEu* _M0L14handle__resultS1633,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1635
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1613;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1642;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1615;
  moonbit_string_t* _M0L5attrsS1616;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1636;
  moonbit_string_t _M0L4nameS1619;
  moonbit_string_t _M0L4nameS1617;
  int32_t _M0L6_2atmpS4148;
  struct _M0TWEOs* _M0L5_2aitS1621;
  struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__* _closure_4886;
  struct _M0TWEOc* _M0L6_2atmpS4139;
  struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__* _closure_4887;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS4140;
  struct moonbit_result_0 _result_4888;
  #line 407 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1646);
  moonbit_incref(_M0FP48clawteam8clawteam8internal3uri48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1642
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal3uri48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1643);
  if (_M0L7_2abindS1642 == 0) {
    struct moonbit_result_0 _result_4879;
    if (_M0L7_2abindS1642) {
      moonbit_decref(_M0L7_2abindS1642);
    }
    moonbit_decref(_M0L17error__to__stringS1635);
    moonbit_decref(_M0L14handle__resultS1633);
    _result_4879.tag = 1;
    _result_4879.data.ok = 0;
    return _result_4879;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1644 =
      _M0L7_2abindS1642;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1645 =
      _M0L7_2aSomeS1644;
    _M0L10index__mapS1613 = _M0L13_2aindex__mapS1645;
    goto join_1612;
  }
  join_1612:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1636
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1613, _M0L5indexS1637);
  if (_M0L7_2abindS1636 == 0) {
    struct moonbit_result_0 _result_4881;
    if (_M0L7_2abindS1636) {
      moonbit_decref(_M0L7_2abindS1636);
    }
    moonbit_decref(_M0L17error__to__stringS1635);
    moonbit_decref(_M0L14handle__resultS1633);
    _result_4881.tag = 1;
    _result_4881.data.ok = 0;
    return _result_4881;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1638 =
      _M0L7_2abindS1636;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1639 = _M0L7_2aSomeS1638;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS4197 = _M0L4_2axS1639->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1640 = _M0L8_2afieldS4197;
    moonbit_string_t* _M0L8_2afieldS4196 = _M0L4_2axS1639->$1;
    int32_t _M0L6_2acntS4726 = Moonbit_object_header(_M0L4_2axS1639)->rc;
    moonbit_string_t* _M0L8_2aattrsS1641;
    if (_M0L6_2acntS4726 > 1) {
      int32_t _M0L11_2anew__cntS4727 = _M0L6_2acntS4726 - 1;
      Moonbit_object_header(_M0L4_2axS1639)->rc = _M0L11_2anew__cntS4727;
      moonbit_incref(_M0L8_2afieldS4196);
      moonbit_incref(_M0L4_2afS1640);
    } else if (_M0L6_2acntS4726 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1639);
    }
    _M0L8_2aattrsS1641 = _M0L8_2afieldS4196;
    _M0L1fS1615 = _M0L4_2afS1640;
    _M0L5attrsS1616 = _M0L8_2aattrsS1641;
    goto join_1614;
  }
  join_1614:;
  _M0L6_2atmpS4148 = Moonbit_array_length(_M0L5attrsS1616);
  if (_M0L6_2atmpS4148 >= 1) {
    moonbit_string_t _M0L6_2atmpS4195 = (moonbit_string_t)_M0L5attrsS1616[0];
    moonbit_string_t _M0L7_2anameS1620 = _M0L6_2atmpS4195;
    moonbit_incref(_M0L7_2anameS1620);
    _M0L4nameS1619 = _M0L7_2anameS1620;
    goto join_1618;
  } else {
    _M0L4nameS1617 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4882;
  join_1618:;
  _M0L4nameS1617 = _M0L4nameS1619;
  joinlet_4882:;
  #line 417 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1621 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1616);
  while (1) {
    moonbit_string_t _M0L4attrS1623;
    moonbit_string_t _M0L7_2abindS1630;
    int32_t _M0L6_2atmpS4132;
    int64_t _M0L6_2atmpS4131;
    moonbit_incref(_M0L5_2aitS1621);
    #line 419 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1630 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1621);
    if (_M0L7_2abindS1630 == 0) {
      if (_M0L7_2abindS1630) {
        moonbit_decref(_M0L7_2abindS1630);
      }
      moonbit_decref(_M0L5_2aitS1621);
    } else {
      moonbit_string_t _M0L7_2aSomeS1631 = _M0L7_2abindS1630;
      moonbit_string_t _M0L7_2aattrS1632 = _M0L7_2aSomeS1631;
      _M0L4attrS1623 = _M0L7_2aattrS1632;
      goto join_1622;
    }
    goto joinlet_4884;
    join_1622:;
    _M0L6_2atmpS4132 = Moonbit_array_length(_M0L4attrS1623);
    _M0L6_2atmpS4131 = (int64_t)_M0L6_2atmpS4132;
    moonbit_incref(_M0L4attrS1623);
    #line 420 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1623, 5, 0, _M0L6_2atmpS4131)
    ) {
      int32_t _M0L6_2atmpS4138 = _M0L4attrS1623[0];
      int32_t _M0L4_2axS1624 = _M0L6_2atmpS4138;
      if (_M0L4_2axS1624 == 112) {
        int32_t _M0L6_2atmpS4137 = _M0L4attrS1623[1];
        int32_t _M0L4_2axS1625 = _M0L6_2atmpS4137;
        if (_M0L4_2axS1625 == 97) {
          int32_t _M0L6_2atmpS4136 = _M0L4attrS1623[2];
          int32_t _M0L4_2axS1626 = _M0L6_2atmpS4136;
          if (_M0L4_2axS1626 == 110) {
            int32_t _M0L6_2atmpS4135 = _M0L4attrS1623[3];
            int32_t _M0L4_2axS1627 = _M0L6_2atmpS4135;
            if (_M0L4_2axS1627 == 105) {
              int32_t _M0L6_2atmpS4194 = _M0L4attrS1623[4];
              int32_t _M0L6_2atmpS4134;
              int32_t _M0L4_2axS1628;
              moonbit_decref(_M0L4attrS1623);
              _M0L6_2atmpS4134 = _M0L6_2atmpS4194;
              _M0L4_2axS1628 = _M0L6_2atmpS4134;
              if (_M0L4_2axS1628 == 99) {
                void* _M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4133;
                struct moonbit_result_0 _result_4885;
                moonbit_decref(_M0L17error__to__stringS1635);
                moonbit_decref(_M0L14handle__resultS1633);
                moonbit_decref(_M0L5_2aitS1621);
                moonbit_decref(_M0L1fS1615);
                _M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4133
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4133)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 9);
                ((struct _M0DTPC15error5Error108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4133)->$0
                = _M0L4nameS1617;
                _result_4885.tag = 0;
                _result_4885.data.err
                = _M0L108clawteam_2fclawteam_2finternal_2furi_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS4133;
                return _result_4885;
              }
            } else {
              moonbit_decref(_M0L4attrS1623);
            }
          } else {
            moonbit_decref(_M0L4attrS1623);
          }
        } else {
          moonbit_decref(_M0L4attrS1623);
        }
      } else {
        moonbit_decref(_M0L4attrS1623);
      }
    } else {
      moonbit_decref(_M0L4attrS1623);
    }
    continue;
    joinlet_4884:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1633);
  moonbit_incref(_M0L4nameS1617);
  _closure_4886
  = (struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__*)moonbit_malloc(sizeof(struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__));
  Moonbit_object_header(_closure_4886)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__, $0) >> 2, 2, 0);
  _closure_4886->code
  = &_M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testC4145l427;
  _closure_4886->$0 = _M0L14handle__resultS1633;
  _closure_4886->$1 = _M0L4nameS1617;
  _M0L6_2atmpS4139 = (struct _M0TWEOc*)_closure_4886;
  _closure_4887
  = (struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__*)moonbit_malloc(sizeof(struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__));
  Moonbit_object_header(_closure_4887)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__, $0) >> 2, 3, 0);
  _closure_4887->code
  = &_M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testC4141l428;
  _closure_4887->$0 = _M0L17error__to__stringS1635;
  _closure_4887->$1 = _M0L14handle__resultS1633;
  _closure_4887->$2 = _M0L4nameS1617;
  _M0L6_2atmpS4140 = (struct _M0TWRPC15error5ErrorEu*)_closure_4887;
  #line 425 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal3uri45moonbit__test__driver__internal__catch__error(_M0L1fS1615, _M0L6_2atmpS4139, _M0L6_2atmpS4140);
  _result_4888.tag = 1;
  _result_4888.data.ok = 1;
  return _result_4888;
}

int32_t _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testC4145l427(
  struct _M0TWEOc* _M0L6_2aenvS4146
) {
  struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__* _M0L14_2acasted__envS4147;
  moonbit_string_t _M0L8_2afieldS4199;
  moonbit_string_t _M0L4nameS1617;
  struct _M0TWssbEu* _M0L8_2afieldS4198;
  int32_t _M0L6_2acntS4728;
  struct _M0TWssbEu* _M0L14handle__resultS1633;
  #line 427 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS4147
  = (struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4145__l427__*)_M0L6_2aenvS4146;
  _M0L8_2afieldS4199 = _M0L14_2acasted__envS4147->$1;
  _M0L4nameS1617 = _M0L8_2afieldS4199;
  _M0L8_2afieldS4198 = _M0L14_2acasted__envS4147->$0;
  _M0L6_2acntS4728 = Moonbit_object_header(_M0L14_2acasted__envS4147)->rc;
  if (_M0L6_2acntS4728 > 1) {
    int32_t _M0L11_2anew__cntS4729 = _M0L6_2acntS4728 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS4147)->rc
    = _M0L11_2anew__cntS4729;
    moonbit_incref(_M0L4nameS1617);
    moonbit_incref(_M0L8_2afieldS4198);
  } else if (_M0L6_2acntS4728 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS4147);
  }
  _M0L14handle__resultS1633 = _M0L8_2afieldS4198;
  #line 427 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1633->code(_M0L14handle__resultS1633, _M0L4nameS1617, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal3uri41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testC4141l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS4142,
  void* _M0L3errS1634
) {
  struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__* _M0L14_2acasted__envS4143;
  moonbit_string_t _M0L8_2afieldS4202;
  moonbit_string_t _M0L4nameS1617;
  struct _M0TWssbEu* _M0L8_2afieldS4201;
  struct _M0TWssbEu* _M0L14handle__resultS1633;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS4200;
  int32_t _M0L6_2acntS4730;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1635;
  moonbit_string_t _M0L6_2atmpS4144;
  #line 428 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS4143
  = (struct _M0R189_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2furi_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u4141__l428__*)_M0L6_2aenvS4142;
  _M0L8_2afieldS4202 = _M0L14_2acasted__envS4143->$2;
  _M0L4nameS1617 = _M0L8_2afieldS4202;
  _M0L8_2afieldS4201 = _M0L14_2acasted__envS4143->$1;
  _M0L14handle__resultS1633 = _M0L8_2afieldS4201;
  _M0L8_2afieldS4200 = _M0L14_2acasted__envS4143->$0;
  _M0L6_2acntS4730 = Moonbit_object_header(_M0L14_2acasted__envS4143)->rc;
  if (_M0L6_2acntS4730 > 1) {
    int32_t _M0L11_2anew__cntS4731 = _M0L6_2acntS4730 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS4143)->rc
    = _M0L11_2anew__cntS4731;
    moonbit_incref(_M0L4nameS1617);
    moonbit_incref(_M0L14handle__resultS1633);
    moonbit_incref(_M0L8_2afieldS4200);
  } else if (_M0L6_2acntS4730 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS4143);
  }
  _M0L17error__to__stringS1635 = _M0L8_2afieldS4200;
  #line 428 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4144
  = _M0L17error__to__stringS1635->code(_M0L17error__to__stringS1635, _M0L3errS1634);
  #line 428 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1633->code(_M0L14handle__resultS1633, _M0L4nameS1617, _M0L6_2atmpS4144, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal3uri45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1608,
  struct _M0TWEOc* _M0L6on__okS1609,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1606
) {
  void* _M0L11_2atry__errS1604;
  struct moonbit_result_0 _tmp_4890;
  void* _M0L3errS1605;
  #line 375 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _tmp_4890 = _M0L1fS1608->code(_M0L1fS1608);
  if (_tmp_4890.tag) {
    int32_t const _M0L5_2aokS4129 = _tmp_4890.data.ok;
    moonbit_decref(_M0L7on__errS1606);
  } else {
    void* const _M0L6_2aerrS4130 = _tmp_4890.data.err;
    moonbit_decref(_M0L6on__okS1609);
    _M0L11_2atry__errS1604 = _M0L6_2aerrS4130;
    goto join_1603;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1609->code(_M0L6on__okS1609);
  goto joinlet_4889;
  join_1603:;
  _M0L3errS1605 = _M0L11_2atry__errS1604;
  #line 383 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1606->code(_M0L7on__errS1606, _M0L3errS1605);
  joinlet_4889:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1563;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1576;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1581;
  struct _M0TUsiE** _M0L6_2atmpS4128;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1588;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1589;
  moonbit_string_t _M0L6_2atmpS4127;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1590;
  int32_t _M0L7_2abindS1591;
  int32_t _M0L2__S1592;
  #line 193 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1563 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1576
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1581 = 0;
  _M0L6_2atmpS4128 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1588
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1588)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1588->$0 = _M0L6_2atmpS4128;
  _M0L16file__and__indexS1588->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1589
  = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1576(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1576);
  #line 284 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4127 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1589, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1590
  = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1581(_M0L51moonbit__test__driver__internal__split__mbt__stringS1581, _M0L6_2atmpS4127, 47);
  _M0L7_2abindS1591 = _M0L10test__argsS1590->$1;
  _M0L2__S1592 = 0;
  while (1) {
    if (_M0L2__S1592 < _M0L7_2abindS1591) {
      moonbit_string_t* _M0L8_2afieldS4204 = _M0L10test__argsS1590->$0;
      moonbit_string_t* _M0L3bufS4126 = _M0L8_2afieldS4204;
      moonbit_string_t _M0L6_2atmpS4203 =
        (moonbit_string_t)_M0L3bufS4126[_M0L2__S1592];
      moonbit_string_t _M0L3argS1593 = _M0L6_2atmpS4203;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1594;
      moonbit_string_t _M0L4fileS1595;
      moonbit_string_t _M0L5rangeS1596;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1597;
      moonbit_string_t _M0L6_2atmpS4124;
      int32_t _M0L5startS1598;
      moonbit_string_t _M0L6_2atmpS4123;
      int32_t _M0L3endS1599;
      int32_t _M0L1iS1600;
      int32_t _M0L6_2atmpS4125;
      moonbit_incref(_M0L3argS1593);
      #line 288 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1594
      = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1581(_M0L51moonbit__test__driver__internal__split__mbt__stringS1581, _M0L3argS1593, 58);
      moonbit_incref(_M0L16file__and__rangeS1594);
      #line 289 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1595
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1594, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1596
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1594, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1597
      = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1581(_M0L51moonbit__test__driver__internal__split__mbt__stringS1581, _M0L5rangeS1596, 45);
      moonbit_incref(_M0L15start__and__endS1597);
      #line 294 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS4124
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1597, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1598
      = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1563(_M0L45moonbit__test__driver__internal__parse__int__S1563, _M0L6_2atmpS4124);
      #line 295 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS4123
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1597, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1599
      = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1563(_M0L45moonbit__test__driver__internal__parse__int__S1563, _M0L6_2atmpS4123);
      _M0L1iS1600 = _M0L5startS1598;
      while (1) {
        if (_M0L1iS1600 < _M0L3endS1599) {
          struct _M0TUsiE* _M0L8_2atupleS4121;
          int32_t _M0L6_2atmpS4122;
          moonbit_incref(_M0L4fileS1595);
          _M0L8_2atupleS4121
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS4121)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS4121->$0 = _M0L4fileS1595;
          _M0L8_2atupleS4121->$1 = _M0L1iS1600;
          moonbit_incref(_M0L16file__and__indexS1588);
          #line 297 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1588, _M0L8_2atupleS4121);
          _M0L6_2atmpS4122 = _M0L1iS1600 + 1;
          _M0L1iS1600 = _M0L6_2atmpS4122;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1595);
        }
        break;
      }
      _M0L6_2atmpS4125 = _M0L2__S1592 + 1;
      _M0L2__S1592 = _M0L6_2atmpS4125;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1590);
    }
    break;
  }
  return _M0L16file__and__indexS1588;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1581(
  int32_t _M0L6_2aenvS4102,
  moonbit_string_t _M0L1sS1582,
  int32_t _M0L3sepS1583
) {
  moonbit_string_t* _M0L6_2atmpS4120;
  struct _M0TPB5ArrayGsE* _M0L3resS1584;
  struct _M0TPC13ref3RefGiE* _M0L1iS1585;
  struct _M0TPC13ref3RefGiE* _M0L5startS1586;
  int32_t _M0L3valS4115;
  int32_t _M0L6_2atmpS4116;
  #line 261 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS4120 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1584
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1584)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1584->$0 = _M0L6_2atmpS4120;
  _M0L3resS1584->$1 = 0;
  _M0L1iS1585
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1585)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1585->$0 = 0;
  _M0L5startS1586
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1586)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1586->$0 = 0;
  while (1) {
    int32_t _M0L3valS4103 = _M0L1iS1585->$0;
    int32_t _M0L6_2atmpS4104 = Moonbit_array_length(_M0L1sS1582);
    if (_M0L3valS4103 < _M0L6_2atmpS4104) {
      int32_t _M0L3valS4107 = _M0L1iS1585->$0;
      int32_t _M0L6_2atmpS4106;
      int32_t _M0L6_2atmpS4105;
      int32_t _M0L3valS4114;
      int32_t _M0L6_2atmpS4113;
      if (
        _M0L3valS4107 < 0
        || _M0L3valS4107 >= Moonbit_array_length(_M0L1sS1582)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4106 = _M0L1sS1582[_M0L3valS4107];
      _M0L6_2atmpS4105 = _M0L6_2atmpS4106;
      if (_M0L6_2atmpS4105 == _M0L3sepS1583) {
        int32_t _M0L3valS4109 = _M0L5startS1586->$0;
        int32_t _M0L3valS4110 = _M0L1iS1585->$0;
        moonbit_string_t _M0L6_2atmpS4108;
        int32_t _M0L3valS4112;
        int32_t _M0L6_2atmpS4111;
        moonbit_incref(_M0L1sS1582);
        #line 270 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS4108
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1582, _M0L3valS4109, _M0L3valS4110);
        moonbit_incref(_M0L3resS1584);
        #line 270 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1584, _M0L6_2atmpS4108);
        _M0L3valS4112 = _M0L1iS1585->$0;
        _M0L6_2atmpS4111 = _M0L3valS4112 + 1;
        _M0L5startS1586->$0 = _M0L6_2atmpS4111;
      }
      _M0L3valS4114 = _M0L1iS1585->$0;
      _M0L6_2atmpS4113 = _M0L3valS4114 + 1;
      _M0L1iS1585->$0 = _M0L6_2atmpS4113;
      continue;
    } else {
      moonbit_decref(_M0L1iS1585);
    }
    break;
  }
  _M0L3valS4115 = _M0L5startS1586->$0;
  _M0L6_2atmpS4116 = Moonbit_array_length(_M0L1sS1582);
  if (_M0L3valS4115 < _M0L6_2atmpS4116) {
    int32_t _M0L8_2afieldS4205 = _M0L5startS1586->$0;
    int32_t _M0L3valS4118;
    int32_t _M0L6_2atmpS4119;
    moonbit_string_t _M0L6_2atmpS4117;
    moonbit_decref(_M0L5startS1586);
    _M0L3valS4118 = _M0L8_2afieldS4205;
    _M0L6_2atmpS4119 = Moonbit_array_length(_M0L1sS1582);
    #line 276 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS4117
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1582, _M0L3valS4118, _M0L6_2atmpS4119);
    moonbit_incref(_M0L3resS1584);
    #line 276 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1584, _M0L6_2atmpS4117);
  } else {
    moonbit_decref(_M0L5startS1586);
    moonbit_decref(_M0L1sS1582);
  }
  return _M0L3resS1584;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1576(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569
) {
  moonbit_bytes_t* _M0L3tmpS1577;
  int32_t _M0L6_2atmpS4101;
  struct _M0TPB5ArrayGsE* _M0L3resS1578;
  int32_t _M0L1iS1579;
  #line 250 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1577
  = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS4101 = Moonbit_array_length(_M0L3tmpS1577);
  #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1578 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS4101);
  _M0L1iS1579 = 0;
  while (1) {
    int32_t _M0L6_2atmpS4097 = Moonbit_array_length(_M0L3tmpS1577);
    if (_M0L1iS1579 < _M0L6_2atmpS4097) {
      moonbit_bytes_t _M0L6_2atmpS4206;
      moonbit_bytes_t _M0L6_2atmpS4099;
      moonbit_string_t _M0L6_2atmpS4098;
      int32_t _M0L6_2atmpS4100;
      if (
        _M0L1iS1579 < 0 || _M0L1iS1579 >= Moonbit_array_length(_M0L3tmpS1577)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4206 = (moonbit_bytes_t)_M0L3tmpS1577[_M0L1iS1579];
      _M0L6_2atmpS4099 = _M0L6_2atmpS4206;
      moonbit_incref(_M0L6_2atmpS4099);
      #line 256 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS4098
      = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569, _M0L6_2atmpS4099);
      moonbit_incref(_M0L3resS1578);
      #line 256 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1578, _M0L6_2atmpS4098);
      _M0L6_2atmpS4100 = _M0L1iS1579 + 1;
      _M0L1iS1579 = _M0L6_2atmpS4100;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1577);
    }
    break;
  }
  return _M0L3resS1578;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1569(
  int32_t _M0L6_2aenvS4011,
  moonbit_bytes_t _M0L5bytesS1570
) {
  struct _M0TPB13StringBuilder* _M0L3resS1571;
  int32_t _M0L3lenS1572;
  struct _M0TPC13ref3RefGiE* _M0L1iS1573;
  #line 206 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1571 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1572 = Moonbit_array_length(_M0L5bytesS1570);
  _M0L1iS1573
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1573)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1573->$0 = 0;
  while (1) {
    int32_t _M0L3valS4012 = _M0L1iS1573->$0;
    if (_M0L3valS4012 < _M0L3lenS1572) {
      int32_t _M0L3valS4096 = _M0L1iS1573->$0;
      int32_t _M0L6_2atmpS4095;
      int32_t _M0L6_2atmpS4094;
      struct _M0TPC13ref3RefGiE* _M0L1cS1574;
      int32_t _M0L3valS4013;
      if (
        _M0L3valS4096 < 0
        || _M0L3valS4096 >= Moonbit_array_length(_M0L5bytesS1570)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4095 = _M0L5bytesS1570[_M0L3valS4096];
      _M0L6_2atmpS4094 = (int32_t)_M0L6_2atmpS4095;
      _M0L1cS1574
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1574)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1574->$0 = _M0L6_2atmpS4094;
      _M0L3valS4013 = _M0L1cS1574->$0;
      if (_M0L3valS4013 < 128) {
        int32_t _M0L8_2afieldS4207 = _M0L1cS1574->$0;
        int32_t _M0L3valS4015;
        int32_t _M0L6_2atmpS4014;
        int32_t _M0L3valS4017;
        int32_t _M0L6_2atmpS4016;
        moonbit_decref(_M0L1cS1574);
        _M0L3valS4015 = _M0L8_2afieldS4207;
        _M0L6_2atmpS4014 = _M0L3valS4015;
        moonbit_incref(_M0L3resS1571);
        #line 215 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1571, _M0L6_2atmpS4014);
        _M0L3valS4017 = _M0L1iS1573->$0;
        _M0L6_2atmpS4016 = _M0L3valS4017 + 1;
        _M0L1iS1573->$0 = _M0L6_2atmpS4016;
      } else {
        int32_t _M0L3valS4018 = _M0L1cS1574->$0;
        if (_M0L3valS4018 < 224) {
          int32_t _M0L3valS4020 = _M0L1iS1573->$0;
          int32_t _M0L6_2atmpS4019 = _M0L3valS4020 + 1;
          int32_t _M0L3valS4029;
          int32_t _M0L6_2atmpS4028;
          int32_t _M0L6_2atmpS4022;
          int32_t _M0L3valS4027;
          int32_t _M0L6_2atmpS4026;
          int32_t _M0L6_2atmpS4025;
          int32_t _M0L6_2atmpS4024;
          int32_t _M0L6_2atmpS4023;
          int32_t _M0L6_2atmpS4021;
          int32_t _M0L8_2afieldS4208;
          int32_t _M0L3valS4031;
          int32_t _M0L6_2atmpS4030;
          int32_t _M0L3valS4033;
          int32_t _M0L6_2atmpS4032;
          if (_M0L6_2atmpS4019 >= _M0L3lenS1572) {
            moonbit_decref(_M0L1cS1574);
            moonbit_decref(_M0L1iS1573);
            moonbit_decref(_M0L5bytesS1570);
            break;
          }
          _M0L3valS4029 = _M0L1cS1574->$0;
          _M0L6_2atmpS4028 = _M0L3valS4029 & 31;
          _M0L6_2atmpS4022 = _M0L6_2atmpS4028 << 6;
          _M0L3valS4027 = _M0L1iS1573->$0;
          _M0L6_2atmpS4026 = _M0L3valS4027 + 1;
          if (
            _M0L6_2atmpS4026 < 0
            || _M0L6_2atmpS4026 >= Moonbit_array_length(_M0L5bytesS1570)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS4025 = _M0L5bytesS1570[_M0L6_2atmpS4026];
          _M0L6_2atmpS4024 = (int32_t)_M0L6_2atmpS4025;
          _M0L6_2atmpS4023 = _M0L6_2atmpS4024 & 63;
          _M0L6_2atmpS4021 = _M0L6_2atmpS4022 | _M0L6_2atmpS4023;
          _M0L1cS1574->$0 = _M0L6_2atmpS4021;
          _M0L8_2afieldS4208 = _M0L1cS1574->$0;
          moonbit_decref(_M0L1cS1574);
          _M0L3valS4031 = _M0L8_2afieldS4208;
          _M0L6_2atmpS4030 = _M0L3valS4031;
          moonbit_incref(_M0L3resS1571);
          #line 222 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1571, _M0L6_2atmpS4030);
          _M0L3valS4033 = _M0L1iS1573->$0;
          _M0L6_2atmpS4032 = _M0L3valS4033 + 2;
          _M0L1iS1573->$0 = _M0L6_2atmpS4032;
        } else {
          int32_t _M0L3valS4034 = _M0L1cS1574->$0;
          if (_M0L3valS4034 < 240) {
            int32_t _M0L3valS4036 = _M0L1iS1573->$0;
            int32_t _M0L6_2atmpS4035 = _M0L3valS4036 + 2;
            int32_t _M0L3valS4052;
            int32_t _M0L6_2atmpS4051;
            int32_t _M0L6_2atmpS4044;
            int32_t _M0L3valS4050;
            int32_t _M0L6_2atmpS4049;
            int32_t _M0L6_2atmpS4048;
            int32_t _M0L6_2atmpS4047;
            int32_t _M0L6_2atmpS4046;
            int32_t _M0L6_2atmpS4045;
            int32_t _M0L6_2atmpS4038;
            int32_t _M0L3valS4043;
            int32_t _M0L6_2atmpS4042;
            int32_t _M0L6_2atmpS4041;
            int32_t _M0L6_2atmpS4040;
            int32_t _M0L6_2atmpS4039;
            int32_t _M0L6_2atmpS4037;
            int32_t _M0L8_2afieldS4209;
            int32_t _M0L3valS4054;
            int32_t _M0L6_2atmpS4053;
            int32_t _M0L3valS4056;
            int32_t _M0L6_2atmpS4055;
            if (_M0L6_2atmpS4035 >= _M0L3lenS1572) {
              moonbit_decref(_M0L1cS1574);
              moonbit_decref(_M0L1iS1573);
              moonbit_decref(_M0L5bytesS1570);
              break;
            }
            _M0L3valS4052 = _M0L1cS1574->$0;
            _M0L6_2atmpS4051 = _M0L3valS4052 & 15;
            _M0L6_2atmpS4044 = _M0L6_2atmpS4051 << 12;
            _M0L3valS4050 = _M0L1iS1573->$0;
            _M0L6_2atmpS4049 = _M0L3valS4050 + 1;
            if (
              _M0L6_2atmpS4049 < 0
              || _M0L6_2atmpS4049 >= Moonbit_array_length(_M0L5bytesS1570)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS4048 = _M0L5bytesS1570[_M0L6_2atmpS4049];
            _M0L6_2atmpS4047 = (int32_t)_M0L6_2atmpS4048;
            _M0L6_2atmpS4046 = _M0L6_2atmpS4047 & 63;
            _M0L6_2atmpS4045 = _M0L6_2atmpS4046 << 6;
            _M0L6_2atmpS4038 = _M0L6_2atmpS4044 | _M0L6_2atmpS4045;
            _M0L3valS4043 = _M0L1iS1573->$0;
            _M0L6_2atmpS4042 = _M0L3valS4043 + 2;
            if (
              _M0L6_2atmpS4042 < 0
              || _M0L6_2atmpS4042 >= Moonbit_array_length(_M0L5bytesS1570)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS4041 = _M0L5bytesS1570[_M0L6_2atmpS4042];
            _M0L6_2atmpS4040 = (int32_t)_M0L6_2atmpS4041;
            _M0L6_2atmpS4039 = _M0L6_2atmpS4040 & 63;
            _M0L6_2atmpS4037 = _M0L6_2atmpS4038 | _M0L6_2atmpS4039;
            _M0L1cS1574->$0 = _M0L6_2atmpS4037;
            _M0L8_2afieldS4209 = _M0L1cS1574->$0;
            moonbit_decref(_M0L1cS1574);
            _M0L3valS4054 = _M0L8_2afieldS4209;
            _M0L6_2atmpS4053 = _M0L3valS4054;
            moonbit_incref(_M0L3resS1571);
            #line 231 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1571, _M0L6_2atmpS4053);
            _M0L3valS4056 = _M0L1iS1573->$0;
            _M0L6_2atmpS4055 = _M0L3valS4056 + 3;
            _M0L1iS1573->$0 = _M0L6_2atmpS4055;
          } else {
            int32_t _M0L3valS4058 = _M0L1iS1573->$0;
            int32_t _M0L6_2atmpS4057 = _M0L3valS4058 + 3;
            int32_t _M0L3valS4081;
            int32_t _M0L6_2atmpS4080;
            int32_t _M0L6_2atmpS4073;
            int32_t _M0L3valS4079;
            int32_t _M0L6_2atmpS4078;
            int32_t _M0L6_2atmpS4077;
            int32_t _M0L6_2atmpS4076;
            int32_t _M0L6_2atmpS4075;
            int32_t _M0L6_2atmpS4074;
            int32_t _M0L6_2atmpS4066;
            int32_t _M0L3valS4072;
            int32_t _M0L6_2atmpS4071;
            int32_t _M0L6_2atmpS4070;
            int32_t _M0L6_2atmpS4069;
            int32_t _M0L6_2atmpS4068;
            int32_t _M0L6_2atmpS4067;
            int32_t _M0L6_2atmpS4060;
            int32_t _M0L3valS4065;
            int32_t _M0L6_2atmpS4064;
            int32_t _M0L6_2atmpS4063;
            int32_t _M0L6_2atmpS4062;
            int32_t _M0L6_2atmpS4061;
            int32_t _M0L6_2atmpS4059;
            int32_t _M0L3valS4083;
            int32_t _M0L6_2atmpS4082;
            int32_t _M0L3valS4087;
            int32_t _M0L6_2atmpS4086;
            int32_t _M0L6_2atmpS4085;
            int32_t _M0L6_2atmpS4084;
            int32_t _M0L8_2afieldS4210;
            int32_t _M0L3valS4091;
            int32_t _M0L6_2atmpS4090;
            int32_t _M0L6_2atmpS4089;
            int32_t _M0L6_2atmpS4088;
            int32_t _M0L3valS4093;
            int32_t _M0L6_2atmpS4092;
            if (_M0L6_2atmpS4057 >= _M0L3lenS1572) {
              moonbit_decref(_M0L1cS1574);
              moonbit_decref(_M0L1iS1573);
              moonbit_decref(_M0L5bytesS1570);
              break;
            }
            _M0L3valS4081 = _M0L1cS1574->$0;
            _M0L6_2atmpS4080 = _M0L3valS4081 & 7;
            _M0L6_2atmpS4073 = _M0L6_2atmpS4080 << 18;
            _M0L3valS4079 = _M0L1iS1573->$0;
            _M0L6_2atmpS4078 = _M0L3valS4079 + 1;
            if (
              _M0L6_2atmpS4078 < 0
              || _M0L6_2atmpS4078 >= Moonbit_array_length(_M0L5bytesS1570)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS4077 = _M0L5bytesS1570[_M0L6_2atmpS4078];
            _M0L6_2atmpS4076 = (int32_t)_M0L6_2atmpS4077;
            _M0L6_2atmpS4075 = _M0L6_2atmpS4076 & 63;
            _M0L6_2atmpS4074 = _M0L6_2atmpS4075 << 12;
            _M0L6_2atmpS4066 = _M0L6_2atmpS4073 | _M0L6_2atmpS4074;
            _M0L3valS4072 = _M0L1iS1573->$0;
            _M0L6_2atmpS4071 = _M0L3valS4072 + 2;
            if (
              _M0L6_2atmpS4071 < 0
              || _M0L6_2atmpS4071 >= Moonbit_array_length(_M0L5bytesS1570)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS4070 = _M0L5bytesS1570[_M0L6_2atmpS4071];
            _M0L6_2atmpS4069 = (int32_t)_M0L6_2atmpS4070;
            _M0L6_2atmpS4068 = _M0L6_2atmpS4069 & 63;
            _M0L6_2atmpS4067 = _M0L6_2atmpS4068 << 6;
            _M0L6_2atmpS4060 = _M0L6_2atmpS4066 | _M0L6_2atmpS4067;
            _M0L3valS4065 = _M0L1iS1573->$0;
            _M0L6_2atmpS4064 = _M0L3valS4065 + 3;
            if (
              _M0L6_2atmpS4064 < 0
              || _M0L6_2atmpS4064 >= Moonbit_array_length(_M0L5bytesS1570)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS4063 = _M0L5bytesS1570[_M0L6_2atmpS4064];
            _M0L6_2atmpS4062 = (int32_t)_M0L6_2atmpS4063;
            _M0L6_2atmpS4061 = _M0L6_2atmpS4062 & 63;
            _M0L6_2atmpS4059 = _M0L6_2atmpS4060 | _M0L6_2atmpS4061;
            _M0L1cS1574->$0 = _M0L6_2atmpS4059;
            _M0L3valS4083 = _M0L1cS1574->$0;
            _M0L6_2atmpS4082 = _M0L3valS4083 - 65536;
            _M0L1cS1574->$0 = _M0L6_2atmpS4082;
            _M0L3valS4087 = _M0L1cS1574->$0;
            _M0L6_2atmpS4086 = _M0L3valS4087 >> 10;
            _M0L6_2atmpS4085 = _M0L6_2atmpS4086 + 55296;
            _M0L6_2atmpS4084 = _M0L6_2atmpS4085;
            moonbit_incref(_M0L3resS1571);
            #line 242 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1571, _M0L6_2atmpS4084);
            _M0L8_2afieldS4210 = _M0L1cS1574->$0;
            moonbit_decref(_M0L1cS1574);
            _M0L3valS4091 = _M0L8_2afieldS4210;
            _M0L6_2atmpS4090 = _M0L3valS4091 & 1023;
            _M0L6_2atmpS4089 = _M0L6_2atmpS4090 + 56320;
            _M0L6_2atmpS4088 = _M0L6_2atmpS4089;
            moonbit_incref(_M0L3resS1571);
            #line 243 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1571, _M0L6_2atmpS4088);
            _M0L3valS4093 = _M0L1iS1573->$0;
            _M0L6_2atmpS4092 = _M0L3valS4093 + 4;
            _M0L1iS1573->$0 = _M0L6_2atmpS4092;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1573);
      moonbit_decref(_M0L5bytesS1570);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1571);
}

int32_t _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1563(
  int32_t _M0L6_2aenvS4004,
  moonbit_string_t _M0L1sS1564
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1565;
  int32_t _M0L3lenS1566;
  int32_t _M0L1iS1567;
  int32_t _M0L8_2afieldS4211;
  #line 197 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1565
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1565)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1565->$0 = 0;
  _M0L3lenS1566 = Moonbit_array_length(_M0L1sS1564);
  _M0L1iS1567 = 0;
  while (1) {
    if (_M0L1iS1567 < _M0L3lenS1566) {
      int32_t _M0L3valS4009 = _M0L3resS1565->$0;
      int32_t _M0L6_2atmpS4006 = _M0L3valS4009 * 10;
      int32_t _M0L6_2atmpS4008;
      int32_t _M0L6_2atmpS4007;
      int32_t _M0L6_2atmpS4005;
      int32_t _M0L6_2atmpS4010;
      if (
        _M0L1iS1567 < 0 || _M0L1iS1567 >= Moonbit_array_length(_M0L1sS1564)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4008 = _M0L1sS1564[_M0L1iS1567];
      _M0L6_2atmpS4007 = _M0L6_2atmpS4008 - 48;
      _M0L6_2atmpS4005 = _M0L6_2atmpS4006 + _M0L6_2atmpS4007;
      _M0L3resS1565->$0 = _M0L6_2atmpS4005;
      _M0L6_2atmpS4010 = _M0L1iS1567 + 1;
      _M0L1iS1567 = _M0L6_2atmpS4010;
      continue;
    } else {
      moonbit_decref(_M0L1sS1564);
    }
    break;
  }
  _M0L8_2afieldS4211 = _M0L3resS1565->$0;
  moonbit_decref(_M0L3resS1565);
  return _M0L8_2afieldS4211;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1543,
  moonbit_string_t _M0L12_2adiscard__S1544,
  int32_t _M0L12_2adiscard__S1545,
  struct _M0TWssbEu* _M0L12_2adiscard__S1546,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1547
) {
  struct moonbit_result_0 _result_4897;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1547);
  moonbit_decref(_M0L12_2adiscard__S1546);
  moonbit_decref(_M0L12_2adiscard__S1544);
  moonbit_decref(_M0L12_2adiscard__S1543);
  _result_4897.tag = 1;
  _result_4897.data.ok = 0;
  return _result_4897;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1548,
  moonbit_string_t _M0L12_2adiscard__S1549,
  int32_t _M0L12_2adiscard__S1550,
  struct _M0TWssbEu* _M0L12_2adiscard__S1551,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1552
) {
  struct moonbit_result_0 _result_4898;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1552);
  moonbit_decref(_M0L12_2adiscard__S1551);
  moonbit_decref(_M0L12_2adiscard__S1549);
  moonbit_decref(_M0L12_2adiscard__S1548);
  _result_4898.tag = 1;
  _result_4898.data.ok = 0;
  return _result_4898;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1553,
  moonbit_string_t _M0L12_2adiscard__S1554,
  int32_t _M0L12_2adiscard__S1555,
  struct _M0TWssbEu* _M0L12_2adiscard__S1556,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1557
) {
  struct moonbit_result_0 _result_4899;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1557);
  moonbit_decref(_M0L12_2adiscard__S1556);
  moonbit_decref(_M0L12_2adiscard__S1554);
  moonbit_decref(_M0L12_2adiscard__S1553);
  _result_4899.tag = 1;
  _result_4899.data.ok = 0;
  return _result_4899;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal3uri21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal3uri50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1558,
  moonbit_string_t _M0L12_2adiscard__S1559,
  int32_t _M0L12_2adiscard__S1560,
  struct _M0TWssbEu* _M0L12_2adiscard__S1561,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1562
) {
  struct moonbit_result_0 _result_4900;
  #line 34 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1562);
  moonbit_decref(_M0L12_2adiscard__S1561);
  moonbit_decref(_M0L12_2adiscard__S1559);
  moonbit_decref(_M0L12_2adiscard__S1558);
  _result_4900.tag = 1;
  _result_4900.data.ok = 0;
  return _result_4900;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal3uri28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal3uri34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1542
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1542);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__2(
  
) {
  moonbit_string_t _M0L7_2abindS1533;
  int32_t _M0L6_2atmpS4003;
  struct _M0TPC16string10StringView _M0L6_2atmpS3997;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2atmpS3998;
  struct _M0TPC16string10StringView* _M0L6_2atmpS4002;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3999;
  void* _M0L4NoneS4000;
  void* _M0L4NoneS4001;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1532;
  moonbit_string_t _M0L7_2abindS1534;
  int32_t _M0L6_2atmpS3945;
  struct _M0TPC16string10StringView _M0L6_2atmpS3944;
  struct moonbit_result_1 _tmp_4902;
  struct _M0TPC16string10StringView _M0L6_2atmpS3942;
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS3943;
  struct _M0TPB4Show _M0L6_2atmpS3935;
  moonbit_string_t _M0L6_2atmpS3938;
  moonbit_string_t _M0L6_2atmpS3939;
  moonbit_string_t _M0L6_2atmpS3940;
  moonbit_string_t _M0L6_2atmpS3941;
  moonbit_string_t* _M0L6_2atmpS3937;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3936;
  struct moonbit_result_0 _tmp_4901;
  struct _M0TPC16string10StringView _M0L8_2afieldS4219;
  struct _M0TPC16string10StringView _M0L6schemeS3950;
  moonbit_string_t _M0L7_2abindS1535;
  int32_t _M0L6_2atmpS3953;
  struct _M0TPC16string10StringView _M0L6_2atmpS3951;
  moonbit_string_t _M0L6_2atmpS3952;
  struct moonbit_result_0 _tmp_4905;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4218;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS3993;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3994;
  struct moonbit_result_2 _tmp_4907;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS1536;
  void* _M0L8_2afieldS4217;
  void* _M0L8userinfoS3956;
  moonbit_string_t _M0L7_2abindS1537;
  int32_t _M0L6_2atmpS3960;
  struct _M0TPC16string10StringView _M0L6_2atmpS3959;
  void* _M0L4SomeS3957;
  moonbit_string_t _M0L6_2atmpS3958;
  struct moonbit_result_0 _tmp_4909;
  struct _M0TPC16string10StringView _M0L8_2afieldS4216;
  struct _M0TPC16string10StringView _M0L4hostS3963;
  moonbit_string_t _M0L7_2abindS1538;
  int32_t _M0L6_2atmpS3966;
  struct _M0TPC16string10StringView _M0L6_2atmpS3964;
  moonbit_string_t _M0L6_2atmpS3965;
  struct moonbit_result_0 _tmp_4911;
  int64_t _M0L8_2afieldS4215;
  int64_t _M0L4portS3969;
  moonbit_string_t _M0L6_2atmpS3970;
  struct moonbit_result_0 _tmp_4913;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4214;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4pathS3973;
  moonbit_string_t _M0L7_2abindS1539;
  int32_t _M0L6_2atmpS3982;
  struct _M0TPC16string10StringView _M0L6_2atmpS3977;
  moonbit_string_t _M0L7_2abindS1540;
  int32_t _M0L6_2atmpS3981;
  struct _M0TPC16string10StringView _M0L6_2atmpS3978;
  moonbit_string_t _M0L7_2abindS1541;
  int32_t _M0L6_2atmpS3980;
  struct _M0TPC16string10StringView _M0L6_2atmpS3979;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3976;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3974;
  moonbit_string_t _M0L6_2atmpS3975;
  struct moonbit_result_0 _tmp_4915;
  void* _M0L8_2afieldS4213;
  void* _M0L5queryS3985;
  void* _M0L4NoneS3986;
  moonbit_string_t _M0L6_2atmpS3987;
  struct moonbit_result_0 _tmp_4917;
  void* _M0L8_2afieldS4212;
  int32_t _M0L6_2acntS4732;
  void* _M0L8fragmentS3990;
  void* _M0L4NoneS3991;
  moonbit_string_t _M0L6_2atmpS3992;
  #line 319 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L7_2abindS1533 = (moonbit_string_t)moonbit_string_literal_0.data;
  _M0L6_2atmpS4003 = Moonbit_array_length(_M0L7_2abindS1533);
  _M0L6_2atmpS3997
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS4003, _M0L7_2abindS1533
  };
  _M0L6_2atmpS3998 = 0;
  _M0L6_2atmpS4002
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L6_2atmpS3999
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3999)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3999->$0 = _M0L6_2atmpS4002;
  _M0L6_2atmpS3999->$1 = 0;
  _M0L4NoneS4000
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L4NoneS4001
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L3uriS1532
  = (struct _M0TP48clawteam8clawteam8internal3uri3Uri*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3uri3Uri));
  Moonbit_object_header(_M0L3uriS1532)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal3uri3Uri, $0_0) >> 2, 5, 0);
  _M0L3uriS1532->$0_0 = _M0L6_2atmpS3997.$0;
  _M0L3uriS1532->$0_1 = _M0L6_2atmpS3997.$1;
  _M0L3uriS1532->$0_2 = _M0L6_2atmpS3997.$2;
  _M0L3uriS1532->$1 = _M0L6_2atmpS3998;
  _M0L3uriS1532->$2 = _M0L6_2atmpS3999;
  _M0L3uriS1532->$3 = _M0L4NoneS4000;
  _M0L3uriS1532->$4 = _M0L4NoneS4001;
  _M0L7_2abindS1534 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3945 = Moonbit_array_length(_M0L7_2abindS1534);
  _M0L6_2atmpS3944
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3945, _M0L7_2abindS1534
  };
  moonbit_incref(_M0L3uriS1532);
  #line 328 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4902
  = _M0FP48clawteam8clawteam8internal3uri10parse__uri(_M0L3uriS1532, _M0L6_2atmpS3944);
  if (_tmp_4902.tag) {
    struct _M0TPC16string10StringView const _M0L5_2aokS3946 =
      _tmp_4902.data.ok;
    _M0L6_2atmpS3942 = _M0L5_2aokS3946;
  } else {
    void* const _M0L6_2aerrS3947 = _tmp_4902.data.err;
    struct moonbit_result_0 _result_4903;
    moonbit_decref(_M0L3uriS1532);
    _result_4903.tag = 0;
    _result_4903.data.err = _M0L6_2aerrS3947;
    return _result_4903;
  }
  _M0L14_2aboxed__selfS3943
  = (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)moonbit_malloc(sizeof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView));
  Moonbit_object_header(_M0L14_2aboxed__selfS3943)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView, $0_0) >> 2, 1, 0);
  _M0L14_2aboxed__selfS3943->$0_0 = _M0L6_2atmpS3942.$0;
  _M0L14_2aboxed__selfS3943->$0_1 = _M0L6_2atmpS3942.$1;
  _M0L14_2aboxed__selfS3943->$0_2 = _M0L6_2atmpS3942.$2;
  _M0L6_2atmpS3935
  = (struct _M0TPB4Show){
    _M0FP0113moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3943
  };
  _M0L6_2atmpS3938 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3939 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3940 = 0;
  _M0L6_2atmpS3941 = 0;
  _M0L6_2atmpS3937 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3937[0] = _M0L6_2atmpS3938;
  _M0L6_2atmpS3937[1] = _M0L6_2atmpS3939;
  _M0L6_2atmpS3937[2] = _M0L6_2atmpS3940;
  _M0L6_2atmpS3937[3] = _M0L6_2atmpS3941;
  _M0L6_2atmpS3936
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3936)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3936->$0 = _M0L6_2atmpS3937;
  _M0L6_2atmpS3936->$1 = 4;
  #line 327 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4901
  = _M0FPB15inspect_2einner(_M0L6_2atmpS3935, (moonbit_string_t)moonbit_string_literal_0.data, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3936);
  if (_tmp_4901.tag) {
    int32_t const _M0L5_2aokS3948 = _tmp_4901.data.ok;
  } else {
    void* const _M0L6_2aerrS3949 = _tmp_4901.data.err;
    struct moonbit_result_0 _result_4904;
    moonbit_decref(_M0L3uriS1532);
    _result_4904.tag = 0;
    _result_4904.data.err = _M0L6_2aerrS3949;
    return _result_4904;
  }
  _M0L8_2afieldS4219
  = (struct _M0TPC16string10StringView){
    _M0L3uriS1532->$0_1, _M0L3uriS1532->$0_2, _M0L3uriS1532->$0_0
  };
  _M0L6schemeS3950 = _M0L8_2afieldS4219;
  _M0L7_2abindS1535 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3953 = Moonbit_array_length(_M0L7_2abindS1535);
  _M0L6_2atmpS3951
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3953, _M0L7_2abindS1535
  };
  _M0L6_2atmpS3952 = 0;
  moonbit_incref(_M0L6schemeS3950.$0);
  #line 331 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4905
  = _M0FPB10assert__eqGRPC16string10StringViewE(_M0L6schemeS3950, _M0L6_2atmpS3951, _M0L6_2atmpS3952, (moonbit_string_t)moonbit_string_literal_14.data);
  if (_tmp_4905.tag) {
    int32_t const _M0L5_2aokS3954 = _tmp_4905.data.ok;
  } else {
    void* const _M0L6_2aerrS3955 = _tmp_4905.data.err;
    struct moonbit_result_0 _result_4906;
    moonbit_decref(_M0L3uriS1532);
    _result_4906.tag = 0;
    _result_4906.data.err = _M0L6_2aerrS3955;
    return _result_4906;
  }
  _M0L8_2afieldS4218 = _M0L3uriS1532->$1;
  _M0L9authorityS3993 = _M0L8_2afieldS4218;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3994
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3994)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3994)->$0
  = (moonbit_string_t)moonbit_string_literal_15.data;
  if (_M0L9authorityS3993) {
    moonbit_incref(_M0L9authorityS3993);
  }
  #line 332 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4907
  = _M0MPC16option6Option17unwrap__or__errorGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE(_M0L9authorityS3993, _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3994);
  if (_tmp_4907.tag) {
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* const _M0L5_2aokS3995 =
      _tmp_4907.data.ok;
    _M0L9authorityS1536 = _M0L5_2aokS3995;
  } else {
    void* const _M0L6_2aerrS3996 = _tmp_4907.data.err;
    struct moonbit_result_0 _result_4908;
    moonbit_decref(_M0L3uriS1532);
    _result_4908.tag = 0;
    _result_4908.data.err = _M0L6_2aerrS3996;
    return _result_4908;
  }
  _M0L8_2afieldS4217 = _M0L9authorityS1536->$0;
  _M0L8userinfoS3956 = _M0L8_2afieldS4217;
  _M0L7_2abindS1537 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3960 = Moonbit_array_length(_M0L7_2abindS1537);
  _M0L6_2atmpS3959
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3960, _M0L7_2abindS1537
  };
  _M0L4SomeS3957
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS3957)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3957)->$0_0
  = _M0L6_2atmpS3959.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3957)->$0_1
  = _M0L6_2atmpS3959.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3957)->$0_2
  = _M0L6_2atmpS3959.$2;
  _M0L6_2atmpS3958 = 0;
  moonbit_incref(_M0L8userinfoS3956);
  #line 335 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4909
  = _M0FPB10assert__eqGORPC16string10StringViewE(_M0L8userinfoS3956, _M0L4SomeS3957, _M0L6_2atmpS3958, (moonbit_string_t)moonbit_string_literal_17.data);
  if (_tmp_4909.tag) {
    int32_t const _M0L5_2aokS3961 = _tmp_4909.data.ok;
  } else {
    void* const _M0L6_2aerrS3962 = _tmp_4909.data.err;
    struct moonbit_result_0 _result_4910;
    moonbit_decref(_M0L9authorityS1536);
    moonbit_decref(_M0L3uriS1532);
    _result_4910.tag = 0;
    _result_4910.data.err = _M0L6_2aerrS3962;
    return _result_4910;
  }
  _M0L8_2afieldS4216
  = (struct _M0TPC16string10StringView){
    _M0L9authorityS1536->$1_1,
      _M0L9authorityS1536->$1_2,
      _M0L9authorityS1536->$1_0
  };
  _M0L4hostS3963 = _M0L8_2afieldS4216;
  _M0L7_2abindS1538 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3966 = Moonbit_array_length(_M0L7_2abindS1538);
  _M0L6_2atmpS3964
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3966, _M0L7_2abindS1538
  };
  _M0L6_2atmpS3965 = 0;
  moonbit_incref(_M0L4hostS3963.$0);
  #line 336 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4911
  = _M0FPB10assert__eqGRPC16string10StringViewE(_M0L4hostS3963, _M0L6_2atmpS3964, _M0L6_2atmpS3965, (moonbit_string_t)moonbit_string_literal_19.data);
  if (_tmp_4911.tag) {
    int32_t const _M0L5_2aokS3967 = _tmp_4911.data.ok;
  } else {
    void* const _M0L6_2aerrS3968 = _tmp_4911.data.err;
    struct moonbit_result_0 _result_4912;
    moonbit_decref(_M0L9authorityS1536);
    moonbit_decref(_M0L3uriS1532);
    _result_4912.tag = 0;
    _result_4912.data.err = _M0L6_2aerrS3968;
    return _result_4912;
  }
  _M0L8_2afieldS4215 = _M0L9authorityS1536->$2;
  moonbit_decref(_M0L9authorityS1536);
  _M0L4portS3969 = _M0L8_2afieldS4215;
  _M0L6_2atmpS3970 = 0;
  #line 337 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4913
  = _M0FPB10assert__eqGOiE(_M0L4portS3969, 8443ll, _M0L6_2atmpS3970, (moonbit_string_t)moonbit_string_literal_20.data);
  if (_tmp_4913.tag) {
    int32_t const _M0L5_2aokS3971 = _tmp_4913.data.ok;
  } else {
    void* const _M0L6_2aerrS3972 = _tmp_4913.data.err;
    struct moonbit_result_0 _result_4914;
    moonbit_decref(_M0L3uriS1532);
    _result_4914.tag = 0;
    _result_4914.data.err = _M0L6_2aerrS3972;
    return _result_4914;
  }
  _M0L8_2afieldS4214 = _M0L3uriS1532->$2;
  _M0L4pathS3973 = _M0L8_2afieldS4214;
  _M0L7_2abindS1539 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3982 = Moonbit_array_length(_M0L7_2abindS1539);
  _M0L6_2atmpS3977
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3982, _M0L7_2abindS1539
  };
  _M0L7_2abindS1540 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3981 = Moonbit_array_length(_M0L7_2abindS1540);
  _M0L6_2atmpS3978
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3981, _M0L7_2abindS1540
  };
  _M0L7_2abindS1541 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3980 = Moonbit_array_length(_M0L7_2abindS1541);
  _M0L6_2atmpS3979
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3980, _M0L7_2abindS1541
  };
  _M0L6_2atmpS3976
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(3, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L6_2atmpS3976[0] = _M0L6_2atmpS3977;
  _M0L6_2atmpS3976[1] = _M0L6_2atmpS3978;
  _M0L6_2atmpS3976[2] = _M0L6_2atmpS3979;
  _M0L6_2atmpS3974
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3974)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3974->$0 = _M0L6_2atmpS3976;
  _M0L6_2atmpS3974->$1 = 3;
  _M0L6_2atmpS3975 = 0;
  moonbit_incref(_M0L4pathS3973);
  #line 338 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4915
  = _M0FPB10assert__eqGRPB5ArrayGRPC16string10StringViewEE(_M0L4pathS3973, _M0L6_2atmpS3974, _M0L6_2atmpS3975, (moonbit_string_t)moonbit_string_literal_24.data);
  if (_tmp_4915.tag) {
    int32_t const _M0L5_2aokS3983 = _tmp_4915.data.ok;
  } else {
    void* const _M0L6_2aerrS3984 = _tmp_4915.data.err;
    struct moonbit_result_0 _result_4916;
    moonbit_decref(_M0L3uriS1532);
    _result_4916.tag = 0;
    _result_4916.data.err = _M0L6_2aerrS3984;
    return _result_4916;
  }
  _M0L8_2afieldS4213 = _M0L3uriS1532->$3;
  _M0L5queryS3985 = _M0L8_2afieldS4213;
  _M0L4NoneS3986
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L6_2atmpS3987 = 0;
  moonbit_incref(_M0L5queryS3985);
  #line 339 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4917
  = _M0FPB10assert__eqGORPC16string10StringViewE(_M0L5queryS3985, _M0L4NoneS3986, _M0L6_2atmpS3987, (moonbit_string_t)moonbit_string_literal_25.data);
  if (_tmp_4917.tag) {
    int32_t const _M0L5_2aokS3988 = _tmp_4917.data.ok;
  } else {
    void* const _M0L6_2aerrS3989 = _tmp_4917.data.err;
    struct moonbit_result_0 _result_4918;
    moonbit_decref(_M0L3uriS1532);
    _result_4918.tag = 0;
    _result_4918.data.err = _M0L6_2aerrS3989;
    return _result_4918;
  }
  _M0L8_2afieldS4212 = _M0L3uriS1532->$4;
  _M0L6_2acntS4732 = Moonbit_object_header(_M0L3uriS1532)->rc;
  if (_M0L6_2acntS4732 > 1) {
    int32_t _M0L11_2anew__cntS4737 = _M0L6_2acntS4732 - 1;
    Moonbit_object_header(_M0L3uriS1532)->rc = _M0L11_2anew__cntS4737;
    moonbit_incref(_M0L8_2afieldS4212);
  } else if (_M0L6_2acntS4732 == 1) {
    void* _M0L8_2afieldS4736 = _M0L3uriS1532->$3;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4735;
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4734;
    struct _M0TPC16string10StringView _M0L8_2afieldS4733;
    moonbit_decref(_M0L8_2afieldS4736);
    _M0L8_2afieldS4735 = _M0L3uriS1532->$2;
    moonbit_decref(_M0L8_2afieldS4735);
    _M0L8_2afieldS4734 = _M0L3uriS1532->$1;
    if (_M0L8_2afieldS4734) {
      moonbit_decref(_M0L8_2afieldS4734);
    }
    _M0L8_2afieldS4733
    = (struct _M0TPC16string10StringView){
      _M0L3uriS1532->$0_1, _M0L3uriS1532->$0_2, _M0L3uriS1532->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4733.$0);
    #line 340 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    moonbit_free(_M0L3uriS1532);
  }
  _M0L8fragmentS3990 = _M0L8_2afieldS4212;
  _M0L4NoneS3991
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L6_2atmpS3992 = 0;
  #line 340 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  return _M0FPB10assert__eqGORPC16string10StringViewE(_M0L8fragmentS3990, _M0L4NoneS3991, _M0L6_2atmpS3992, (moonbit_string_t)moonbit_string_literal_26.data);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__1(
  
) {
  moonbit_string_t _M0L7_2abindS1521;
  int32_t _M0L6_2atmpS3934;
  struct _M0TPC16string10StringView _M0L6_2atmpS3928;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2atmpS3929;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3933;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3930;
  void* _M0L4NoneS3931;
  void* _M0L4NoneS3932;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1520;
  moonbit_string_t _M0L7_2abindS1522;
  int32_t _M0L6_2atmpS3872;
  struct _M0TPC16string10StringView _M0L6_2atmpS3871;
  struct moonbit_result_1 _tmp_4920;
  struct _M0TPC16string10StringView _M0L6_2atmpS3869;
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS3870;
  struct _M0TPB4Show _M0L6_2atmpS3862;
  moonbit_string_t _M0L6_2atmpS3865;
  moonbit_string_t _M0L6_2atmpS3866;
  moonbit_string_t _M0L6_2atmpS3867;
  moonbit_string_t _M0L6_2atmpS3868;
  moonbit_string_t* _M0L6_2atmpS3864;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3863;
  struct moonbit_result_0 _tmp_4919;
  struct _M0TPC16string10StringView _M0L8_2afieldS4227;
  struct _M0TPC16string10StringView _M0L6schemeS3877;
  moonbit_string_t _M0L7_2abindS1523;
  int32_t _M0L6_2atmpS3880;
  struct _M0TPC16string10StringView _M0L6_2atmpS3878;
  moonbit_string_t _M0L6_2atmpS3879;
  struct moonbit_result_0 _tmp_4923;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4226;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS3924;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3925;
  struct moonbit_result_2 _tmp_4925;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS1524;
  void* _M0L8_2afieldS4225;
  void* _M0L8userinfoS3883;
  moonbit_string_t _M0L7_2abindS1525;
  int32_t _M0L6_2atmpS3887;
  struct _M0TPC16string10StringView _M0L6_2atmpS3886;
  void* _M0L4SomeS3884;
  moonbit_string_t _M0L6_2atmpS3885;
  struct moonbit_result_0 _tmp_4927;
  struct _M0TPC16string10StringView _M0L8_2afieldS4224;
  struct _M0TPC16string10StringView _M0L4hostS3890;
  moonbit_string_t _M0L7_2abindS1526;
  int32_t _M0L6_2atmpS3893;
  struct _M0TPC16string10StringView _M0L6_2atmpS3891;
  moonbit_string_t _M0L6_2atmpS3892;
  struct moonbit_result_0 _tmp_4929;
  int64_t _M0L8_2afieldS4223;
  int64_t _M0L4portS3896;
  moonbit_string_t _M0L6_2atmpS3897;
  struct moonbit_result_0 _tmp_4931;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4222;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4pathS3900;
  moonbit_string_t _M0L7_2abindS1527;
  int32_t _M0L6_2atmpS3909;
  struct _M0TPC16string10StringView _M0L6_2atmpS3904;
  moonbit_string_t _M0L7_2abindS1528;
  int32_t _M0L6_2atmpS3908;
  struct _M0TPC16string10StringView _M0L6_2atmpS3905;
  moonbit_string_t _M0L7_2abindS1529;
  int32_t _M0L6_2atmpS3907;
  struct _M0TPC16string10StringView _M0L6_2atmpS3906;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3903;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3901;
  moonbit_string_t _M0L6_2atmpS3902;
  struct moonbit_result_0 _tmp_4933;
  void* _M0L8_2afieldS4221;
  void* _M0L5queryS3912;
  moonbit_string_t _M0L7_2abindS1530;
  int32_t _M0L6_2atmpS3916;
  struct _M0TPC16string10StringView _M0L6_2atmpS3915;
  void* _M0L4SomeS3913;
  moonbit_string_t _M0L6_2atmpS3914;
  struct moonbit_result_0 _tmp_4935;
  void* _M0L8_2afieldS4220;
  int32_t _M0L6_2acntS4738;
  void* _M0L8fragmentS3919;
  moonbit_string_t _M0L7_2abindS1531;
  int32_t _M0L6_2atmpS3923;
  struct _M0TPC16string10StringView _M0L6_2atmpS3922;
  void* _M0L4SomeS3920;
  moonbit_string_t _M0L6_2atmpS3921;
  #line 292 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L7_2abindS1521 = (moonbit_string_t)moonbit_string_literal_0.data;
  _M0L6_2atmpS3934 = Moonbit_array_length(_M0L7_2abindS1521);
  _M0L6_2atmpS3928
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3934, _M0L7_2abindS1521
  };
  _M0L6_2atmpS3929 = 0;
  _M0L6_2atmpS3933
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L6_2atmpS3930
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3930)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3930->$0 = _M0L6_2atmpS3933;
  _M0L6_2atmpS3930->$1 = 0;
  _M0L4NoneS3931
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L4NoneS3932
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L3uriS1520
  = (struct _M0TP48clawteam8clawteam8internal3uri3Uri*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3uri3Uri));
  Moonbit_object_header(_M0L3uriS1520)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal3uri3Uri, $0_0) >> 2, 5, 0);
  _M0L3uriS1520->$0_0 = _M0L6_2atmpS3928.$0;
  _M0L3uriS1520->$0_1 = _M0L6_2atmpS3928.$1;
  _M0L3uriS1520->$0_2 = _M0L6_2atmpS3928.$2;
  _M0L3uriS1520->$1 = _M0L6_2atmpS3929;
  _M0L3uriS1520->$2 = _M0L6_2atmpS3930;
  _M0L3uriS1520->$3 = _M0L4NoneS3931;
  _M0L3uriS1520->$4 = _M0L4NoneS3932;
  _M0L7_2abindS1522 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3872 = Moonbit_array_length(_M0L7_2abindS1522);
  _M0L6_2atmpS3871
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3872, _M0L7_2abindS1522
  };
  moonbit_incref(_M0L3uriS1520);
  #line 301 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4920
  = _M0FP48clawteam8clawteam8internal3uri10parse__uri(_M0L3uriS1520, _M0L6_2atmpS3871);
  if (_tmp_4920.tag) {
    struct _M0TPC16string10StringView const _M0L5_2aokS3873 =
      _tmp_4920.data.ok;
    _M0L6_2atmpS3869 = _M0L5_2aokS3873;
  } else {
    void* const _M0L6_2aerrS3874 = _tmp_4920.data.err;
    struct moonbit_result_0 _result_4921;
    moonbit_decref(_M0L3uriS1520);
    _result_4921.tag = 0;
    _result_4921.data.err = _M0L6_2aerrS3874;
    return _result_4921;
  }
  _M0L14_2aboxed__selfS3870
  = (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)moonbit_malloc(sizeof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView));
  Moonbit_object_header(_M0L14_2aboxed__selfS3870)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView, $0_0) >> 2, 1, 0);
  _M0L14_2aboxed__selfS3870->$0_0 = _M0L6_2atmpS3869.$0;
  _M0L14_2aboxed__selfS3870->$0_1 = _M0L6_2atmpS3869.$1;
  _M0L14_2aboxed__selfS3870->$0_2 = _M0L6_2atmpS3869.$2;
  _M0L6_2atmpS3862
  = (struct _M0TPB4Show){
    _M0FP0113moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3870
  };
  _M0L6_2atmpS3865 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3866 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3867 = 0;
  _M0L6_2atmpS3868 = 0;
  _M0L6_2atmpS3864 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3864[0] = _M0L6_2atmpS3865;
  _M0L6_2atmpS3864[1] = _M0L6_2atmpS3866;
  _M0L6_2atmpS3864[2] = _M0L6_2atmpS3867;
  _M0L6_2atmpS3864[3] = _M0L6_2atmpS3868;
  _M0L6_2atmpS3863
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3863)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3863->$0 = _M0L6_2atmpS3864;
  _M0L6_2atmpS3863->$1 = 4;
  #line 300 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4919
  = _M0FPB15inspect_2einner(_M0L6_2atmpS3862, (moonbit_string_t)moonbit_string_literal_0.data, (moonbit_string_t)moonbit_string_literal_30.data, _M0L6_2atmpS3863);
  if (_tmp_4919.tag) {
    int32_t const _M0L5_2aokS3875 = _tmp_4919.data.ok;
  } else {
    void* const _M0L6_2aerrS3876 = _tmp_4919.data.err;
    struct moonbit_result_0 _result_4922;
    moonbit_decref(_M0L3uriS1520);
    _result_4922.tag = 0;
    _result_4922.data.err = _M0L6_2aerrS3876;
    return _result_4922;
  }
  _M0L8_2afieldS4227
  = (struct _M0TPC16string10StringView){
    _M0L3uriS1520->$0_1, _M0L3uriS1520->$0_2, _M0L3uriS1520->$0_0
  };
  _M0L6schemeS3877 = _M0L8_2afieldS4227;
  _M0L7_2abindS1523 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3880 = Moonbit_array_length(_M0L7_2abindS1523);
  _M0L6_2atmpS3878
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3880, _M0L7_2abindS1523
  };
  _M0L6_2atmpS3879 = 0;
  moonbit_incref(_M0L6schemeS3877.$0);
  #line 306 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4923
  = _M0FPB10assert__eqGRPC16string10StringViewE(_M0L6schemeS3877, _M0L6_2atmpS3878, _M0L6_2atmpS3879, (moonbit_string_t)moonbit_string_literal_31.data);
  if (_tmp_4923.tag) {
    int32_t const _M0L5_2aokS3881 = _tmp_4923.data.ok;
  } else {
    void* const _M0L6_2aerrS3882 = _tmp_4923.data.err;
    struct moonbit_result_0 _result_4924;
    moonbit_decref(_M0L3uriS1520);
    _result_4924.tag = 0;
    _result_4924.data.err = _M0L6_2aerrS3882;
    return _result_4924;
  }
  _M0L8_2afieldS4226 = _M0L3uriS1520->$1;
  _M0L9authorityS3924 = _M0L8_2afieldS4226;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3925
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3925)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3925)->$0
  = (moonbit_string_t)moonbit_string_literal_15.data;
  if (_M0L9authorityS3924) {
    moonbit_incref(_M0L9authorityS3924);
  }
  #line 307 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4925
  = _M0MPC16option6Option17unwrap__or__errorGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE(_M0L9authorityS3924, _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS3925);
  if (_tmp_4925.tag) {
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* const _M0L5_2aokS3926 =
      _tmp_4925.data.ok;
    _M0L9authorityS1524 = _M0L5_2aokS3926;
  } else {
    void* const _M0L6_2aerrS3927 = _tmp_4925.data.err;
    struct moonbit_result_0 _result_4926;
    moonbit_decref(_M0L3uriS1520);
    _result_4926.tag = 0;
    _result_4926.data.err = _M0L6_2aerrS3927;
    return _result_4926;
  }
  _M0L8_2afieldS4225 = _M0L9authorityS1524->$0;
  _M0L8userinfoS3883 = _M0L8_2afieldS4225;
  _M0L7_2abindS1525 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3887 = Moonbit_array_length(_M0L7_2abindS1525);
  _M0L6_2atmpS3886
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3887, _M0L7_2abindS1525
  };
  _M0L4SomeS3884
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS3884)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3884)->$0_0
  = _M0L6_2atmpS3886.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3884)->$0_1
  = _M0L6_2atmpS3886.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3884)->$0_2
  = _M0L6_2atmpS3886.$2;
  _M0L6_2atmpS3885 = 0;
  moonbit_incref(_M0L8userinfoS3883);
  #line 310 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4927
  = _M0FPB10assert__eqGORPC16string10StringViewE(_M0L8userinfoS3883, _M0L4SomeS3884, _M0L6_2atmpS3885, (moonbit_string_t)moonbit_string_literal_32.data);
  if (_tmp_4927.tag) {
    int32_t const _M0L5_2aokS3888 = _tmp_4927.data.ok;
  } else {
    void* const _M0L6_2aerrS3889 = _tmp_4927.data.err;
    struct moonbit_result_0 _result_4928;
    moonbit_decref(_M0L9authorityS1524);
    moonbit_decref(_M0L3uriS1520);
    _result_4928.tag = 0;
    _result_4928.data.err = _M0L6_2aerrS3889;
    return _result_4928;
  }
  _M0L8_2afieldS4224
  = (struct _M0TPC16string10StringView){
    _M0L9authorityS1524->$1_1,
      _M0L9authorityS1524->$1_2,
      _M0L9authorityS1524->$1_0
  };
  _M0L4hostS3890 = _M0L8_2afieldS4224;
  _M0L7_2abindS1526 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3893 = Moonbit_array_length(_M0L7_2abindS1526);
  _M0L6_2atmpS3891
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3893, _M0L7_2abindS1526
  };
  _M0L6_2atmpS3892 = 0;
  moonbit_incref(_M0L4hostS3890.$0);
  #line 311 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4929
  = _M0FPB10assert__eqGRPC16string10StringViewE(_M0L4hostS3890, _M0L6_2atmpS3891, _M0L6_2atmpS3892, (moonbit_string_t)moonbit_string_literal_33.data);
  if (_tmp_4929.tag) {
    int32_t const _M0L5_2aokS3894 = _tmp_4929.data.ok;
  } else {
    void* const _M0L6_2aerrS3895 = _tmp_4929.data.err;
    struct moonbit_result_0 _result_4930;
    moonbit_decref(_M0L9authorityS1524);
    moonbit_decref(_M0L3uriS1520);
    _result_4930.tag = 0;
    _result_4930.data.err = _M0L6_2aerrS3895;
    return _result_4930;
  }
  _M0L8_2afieldS4223 = _M0L9authorityS1524->$2;
  moonbit_decref(_M0L9authorityS1524);
  _M0L4portS3896 = _M0L8_2afieldS4223;
  _M0L6_2atmpS3897 = 0;
  #line 312 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4931
  = _M0FPB10assert__eqGOiE(_M0L4portS3896, 8443ll, _M0L6_2atmpS3897, (moonbit_string_t)moonbit_string_literal_34.data);
  if (_tmp_4931.tag) {
    int32_t const _M0L5_2aokS3898 = _tmp_4931.data.ok;
  } else {
    void* const _M0L6_2aerrS3899 = _tmp_4931.data.err;
    struct moonbit_result_0 _result_4932;
    moonbit_decref(_M0L3uriS1520);
    _result_4932.tag = 0;
    _result_4932.data.err = _M0L6_2aerrS3899;
    return _result_4932;
  }
  _M0L8_2afieldS4222 = _M0L3uriS1520->$2;
  _M0L4pathS3900 = _M0L8_2afieldS4222;
  _M0L7_2abindS1527 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3909 = Moonbit_array_length(_M0L7_2abindS1527);
  _M0L6_2atmpS3904
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3909, _M0L7_2abindS1527
  };
  _M0L7_2abindS1528 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3908 = Moonbit_array_length(_M0L7_2abindS1528);
  _M0L6_2atmpS3905
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3908, _M0L7_2abindS1528
  };
  _M0L7_2abindS1529 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3907 = Moonbit_array_length(_M0L7_2abindS1529);
  _M0L6_2atmpS3906
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3907, _M0L7_2abindS1529
  };
  _M0L6_2atmpS3903
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array_raw(3, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0));
  _M0L6_2atmpS3903[0] = _M0L6_2atmpS3904;
  _M0L6_2atmpS3903[1] = _M0L6_2atmpS3905;
  _M0L6_2atmpS3903[2] = _M0L6_2atmpS3906;
  _M0L6_2atmpS3901
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3901)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3901->$0 = _M0L6_2atmpS3903;
  _M0L6_2atmpS3901->$1 = 3;
  _M0L6_2atmpS3902 = 0;
  moonbit_incref(_M0L4pathS3900);
  #line 313 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4933
  = _M0FPB10assert__eqGRPB5ArrayGRPC16string10StringViewEE(_M0L4pathS3900, _M0L6_2atmpS3901, _M0L6_2atmpS3902, (moonbit_string_t)moonbit_string_literal_35.data);
  if (_tmp_4933.tag) {
    int32_t const _M0L5_2aokS3910 = _tmp_4933.data.ok;
  } else {
    void* const _M0L6_2aerrS3911 = _tmp_4933.data.err;
    struct moonbit_result_0 _result_4934;
    moonbit_decref(_M0L3uriS1520);
    _result_4934.tag = 0;
    _result_4934.data.err = _M0L6_2aerrS3911;
    return _result_4934;
  }
  _M0L8_2afieldS4221 = _M0L3uriS1520->$3;
  _M0L5queryS3912 = _M0L8_2afieldS4221;
  _M0L7_2abindS1530 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3916 = Moonbit_array_length(_M0L7_2abindS1530);
  _M0L6_2atmpS3915
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3916, _M0L7_2abindS1530
  };
  _M0L4SomeS3913
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS3913)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3913)->$0_0
  = _M0L6_2atmpS3915.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3913)->$0_1
  = _M0L6_2atmpS3915.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3913)->$0_2
  = _M0L6_2atmpS3915.$2;
  _M0L6_2atmpS3914 = 0;
  moonbit_incref(_M0L5queryS3912);
  #line 314 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4935
  = _M0FPB10assert__eqGORPC16string10StringViewE(_M0L5queryS3912, _M0L4SomeS3913, _M0L6_2atmpS3914, (moonbit_string_t)moonbit_string_literal_37.data);
  if (_tmp_4935.tag) {
    int32_t const _M0L5_2aokS3917 = _tmp_4935.data.ok;
  } else {
    void* const _M0L6_2aerrS3918 = _tmp_4935.data.err;
    struct moonbit_result_0 _result_4936;
    moonbit_decref(_M0L3uriS1520);
    _result_4936.tag = 0;
    _result_4936.data.err = _M0L6_2aerrS3918;
    return _result_4936;
  }
  _M0L8_2afieldS4220 = _M0L3uriS1520->$4;
  _M0L6_2acntS4738 = Moonbit_object_header(_M0L3uriS1520)->rc;
  if (_M0L6_2acntS4738 > 1) {
    int32_t _M0L11_2anew__cntS4743 = _M0L6_2acntS4738 - 1;
    Moonbit_object_header(_M0L3uriS1520)->rc = _M0L11_2anew__cntS4743;
    moonbit_incref(_M0L8_2afieldS4220);
  } else if (_M0L6_2acntS4738 == 1) {
    void* _M0L8_2afieldS4742 = _M0L3uriS1520->$3;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4741;
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4740;
    struct _M0TPC16string10StringView _M0L8_2afieldS4739;
    moonbit_decref(_M0L8_2afieldS4742);
    _M0L8_2afieldS4741 = _M0L3uriS1520->$2;
    moonbit_decref(_M0L8_2afieldS4741);
    _M0L8_2afieldS4740 = _M0L3uriS1520->$1;
    if (_M0L8_2afieldS4740) {
      moonbit_decref(_M0L8_2afieldS4740);
    }
    _M0L8_2afieldS4739
    = (struct _M0TPC16string10StringView){
      _M0L3uriS1520->$0_1, _M0L3uriS1520->$0_2, _M0L3uriS1520->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4739.$0);
    #line 315 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    moonbit_free(_M0L3uriS1520);
  }
  _M0L8fragmentS3919 = _M0L8_2afieldS4220;
  _M0L7_2abindS1531 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3923 = Moonbit_array_length(_M0L7_2abindS1531);
  _M0L6_2atmpS3922
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3923, _M0L7_2abindS1531
  };
  _M0L4SomeS3920
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS3920)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3920)->$0_0
  = _M0L6_2atmpS3922.$0;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3920)->$0_1
  = _M0L6_2atmpS3922.$1;
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3920)->$0_2
  = _M0L6_2atmpS3922.$2;
  _M0L6_2atmpS3921 = 0;
  #line 315 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  return _M0FPB10assert__eqGORPC16string10StringViewE(_M0L8fragmentS3919, _M0L4SomeS3920, _M0L6_2atmpS3921, (moonbit_string_t)moonbit_string_literal_39.data);
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri10parse__uri(
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1467,
  struct _M0TPC16string10StringView _M0L6sourceS1514
) {
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L17parse__hier__partS1463;
  struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474* _M0L13parse__schemeS1474;
  struct _M0TPC16string10StringView _M0L1sS1511;
  struct _M0TPC16string10StringView _M0L1sS1513;
  moonbit_string_t _M0L8_2afieldS4234;
  moonbit_string_t _M0L3strS3836;
  int32_t _M0L5startS3837;
  int32_t _M0L3endS3839;
  int64_t _M0L6_2atmpS3838;
  struct moonbit_result_1 _tmp_4940;
  struct _M0TPC16string10StringView _M0L6_2atmpS3833;
  struct moonbit_result_1 _result_4942;
  void* _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832;
  struct moonbit_result_1 _result_4943;
  #line 252 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L17parse__hier__partS1463 = _M0L3uriS1467;
  moonbit_incref(_M0L17parse__hier__partS1463);
  _M0L13parse__schemeS1474
  = (struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474*)moonbit_malloc(sizeof(struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474));
  Moonbit_object_header(_M0L13parse__schemeS1474)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474, $0) >> 2, 2, 0);
  _M0L13parse__schemeS1474->$0 = _M0L3uriS1467;
  _M0L13parse__schemeS1474->$1 = _M0L17parse__hier__partS1463;
  _M0L8_2afieldS4234 = _M0L6sourceS1514.$0;
  _M0L3strS3836 = _M0L8_2afieldS4234;
  _M0L5startS3837 = _M0L6sourceS1514.$1;
  _M0L3endS3839 = _M0L6sourceS1514.$2;
  _M0L6_2atmpS3838 = (int64_t)_M0L3endS3839;
  moonbit_incref(_M0L3strS3836);
  #line 284 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  if (
    _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3836, 1, _M0L5startS3837, _M0L6_2atmpS3838)
  ) {
    moonbit_string_t _M0L8_2afieldS4233 = _M0L6sourceS1514.$0;
    moonbit_string_t _M0L3strS3854 = _M0L8_2afieldS4233;
    moonbit_string_t _M0L8_2afieldS4232 = _M0L6sourceS1514.$0;
    moonbit_string_t _M0L3strS3857 = _M0L8_2afieldS4232;
    int32_t _M0L5startS3858 = _M0L6sourceS1514.$1;
    int32_t _M0L3endS3860 = _M0L6sourceS1514.$2;
    int64_t _M0L6_2atmpS3859 = (int64_t)_M0L3endS3860;
    int64_t _M0L6_2atmpS3856;
    int32_t _M0L6_2atmpS3855;
    int32_t _M0L4_2axS1515;
    moonbit_incref(_M0L3strS3857);
    moonbit_incref(_M0L3strS3854);
    #line 284 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3856
    = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3857, 0, _M0L5startS3858, _M0L6_2atmpS3859);
    _M0L6_2atmpS3855 = (int32_t)_M0L6_2atmpS3856;
    #line 284 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L4_2axS1515
    = _M0MPC16string6String16unsafe__char__at(_M0L3strS3854, _M0L6_2atmpS3855);
    if (_M0L4_2axS1515 >= 97 && _M0L4_2axS1515 <= 122) {
      moonbit_string_t _M0L8_2afieldS4229 = _M0L6sourceS1514.$0;
      moonbit_string_t _M0L3strS3847 = _M0L8_2afieldS4229;
      moonbit_string_t _M0L8_2afieldS4228 = _M0L6sourceS1514.$0;
      moonbit_string_t _M0L3strS3850 = _M0L8_2afieldS4228;
      int32_t _M0L5startS3851 = _M0L6sourceS1514.$1;
      int32_t _M0L3endS3853 = _M0L6sourceS1514.$2;
      int64_t _M0L6_2atmpS3852 = (int64_t)_M0L3endS3853;
      int64_t _M0L7_2abindS1725;
      int32_t _M0L6_2atmpS3848;
      int32_t _M0L3endS3849;
      struct _M0TPC16string10StringView _M0L4_2axS1516;
      moonbit_incref(_M0L3strS3850);
      moonbit_incref(_M0L3strS3847);
      #line 284 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1725
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3850, 1, _M0L5startS3851, _M0L6_2atmpS3852);
      if (_M0L7_2abindS1725 == 4294967296ll) {
        _M0L6_2atmpS3848 = _M0L6sourceS1514.$2;
      } else {
        int64_t _M0L7_2aSomeS1517 = _M0L7_2abindS1725;
        _M0L6_2atmpS3848 = (int32_t)_M0L7_2aSomeS1517;
      }
      _M0L3endS3849 = _M0L6sourceS1514.$2;
      _M0L4_2axS1516
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3848, _M0L3endS3849, _M0L3strS3847
      };
      _M0L1sS1513 = _M0L4_2axS1516;
      goto join_1512;
    } else if (_M0L4_2axS1515 >= 65 && _M0L4_2axS1515 <= 90) {
      moonbit_string_t _M0L8_2afieldS4231 = _M0L6sourceS1514.$0;
      moonbit_string_t _M0L3strS3840 = _M0L8_2afieldS4231;
      moonbit_string_t _M0L8_2afieldS4230 = _M0L6sourceS1514.$0;
      moonbit_string_t _M0L3strS3843 = _M0L8_2afieldS4230;
      int32_t _M0L5startS3844 = _M0L6sourceS1514.$1;
      int32_t _M0L3endS3846 = _M0L6sourceS1514.$2;
      int64_t _M0L6_2atmpS3845 = (int64_t)_M0L3endS3846;
      int64_t _M0L7_2abindS1726;
      int32_t _M0L6_2atmpS3841;
      int32_t _M0L3endS3842;
      struct _M0TPC16string10StringView _M0L4_2axS1518;
      moonbit_incref(_M0L3strS3843);
      moonbit_incref(_M0L3strS3840);
      #line 284 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1726
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3843, 1, _M0L5startS3844, _M0L6_2atmpS3845);
      if (_M0L7_2abindS1726 == 4294967296ll) {
        _M0L6_2atmpS3841 = _M0L6sourceS1514.$2;
      } else {
        int64_t _M0L7_2aSomeS1519 = _M0L7_2abindS1726;
        _M0L6_2atmpS3841 = (int32_t)_M0L7_2aSomeS1519;
      }
      _M0L3endS3842 = _M0L6sourceS1514.$2;
      _M0L4_2axS1518
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3841, _M0L3endS3842, _M0L3strS3840
      };
      _M0L1sS1513 = _M0L4_2axS1518;
      goto join_1512;
    } else {
      moonbit_decref(_M0L13parse__schemeS1474);
      _M0L1sS1511 = _M0L6sourceS1514;
      goto join_1510;
    }
  } else {
    void* _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingSchemeS3861;
    struct moonbit_result_1 _result_4939;
    moonbit_decref(_M0L6sourceS1514.$0);
    moonbit_decref(_M0L13parse__schemeS1474);
    _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingSchemeS3861
    = (struct moonbit_object*)&moonbit_constant_constructor_4 + 1;
    _result_4939.tag = 0;
    _result_4939.data.err
    = _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingSchemeS3861;
    return _result_4939;
  }
  join_1512:;
  #line 285 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4940
  = _M0FP48clawteam8clawteam8internal3uri10parse__uriN13parse__schemeS1474(_M0L13parse__schemeS1474, _M0L6sourceS1514, _M0L1sS1513);
  if (_tmp_4940.tag) {
    struct _M0TPC16string10StringView const _M0L5_2aokS3834 =
      _tmp_4940.data.ok;
    _M0L6_2atmpS3833 = _M0L5_2aokS3834;
  } else {
    void* const _M0L6_2aerrS3835 = _tmp_4940.data.err;
    struct moonbit_result_1 _result_4941;
    _result_4941.tag = 0;
    _result_4941.data.err = _M0L6_2aerrS3835;
    return _result_4941;
  }
  _result_4942.tag = 1;
  _result_4942.data.ok = _M0L6_2atmpS3833;
  return _result_4942;
  join_1510:;
  _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme));
  Moonbit_object_header(_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme, $0_0) >> 2, 1, 8);
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832)->$0_0
  = _M0L1sS1511.$0;
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832)->$0_1
  = _M0L1sS1511.$1;
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832)->$0_2
  = _M0L1sS1511.$2;
  _result_4943.tag = 0;
  _result_4943.data.err
  = _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3832;
  return _result_4943;
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri10parse__uriN13parse__schemeS1474(
  struct _M0R75_24clawteam_2fclawteam_2finternal_2furi_2eparse__uri_2eparse__scheme_7c1474* _M0L6_2aenvS3737,
  struct _M0TPC16string10StringView _M0L6sourceS1475,
  struct _M0TPC16string10StringView _M0L1kS1476
) {
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L8_2afieldS4269;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L17parse__hier__partS1463;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L8_2afieldS4268;
  int32_t _M0L6_2acntS4744;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1467;
  struct _M0TPC16string10StringView _M0L1sS1478;
  struct _M0TPC16string10StringView _M0L1sS1480;
  moonbit_string_t _M0L8_2afieldS4267;
  moonbit_string_t _M0L3strS3806;
  int32_t _M0L5startS3807;
  int32_t _M0L3endS3809;
  int64_t _M0L6_2atmpS3808;
  struct _M0TPC16string10StringView _M0L6_2atmpS3739;
  struct _M0TPC16string10StringView _M0L8_2aparamS1481;
  struct moonbit_result_1 _result_4956;
  void* _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738;
  struct moonbit_result_1 _result_4957;
  #line 263 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L8_2afieldS4269 = _M0L6_2aenvS3737->$1;
  _M0L17parse__hier__partS1463 = _M0L8_2afieldS4269;
  _M0L8_2afieldS4268 = _M0L6_2aenvS3737->$0;
  _M0L6_2acntS4744 = Moonbit_object_header(_M0L6_2aenvS3737)->rc;
  if (_M0L6_2acntS4744 > 1) {
    int32_t _M0L11_2anew__cntS4745 = _M0L6_2acntS4744 - 1;
    Moonbit_object_header(_M0L6_2aenvS3737)->rc = _M0L11_2anew__cntS4745;
    moonbit_incref(_M0L17parse__hier__partS1463);
    moonbit_incref(_M0L8_2afieldS4268);
  } else if (_M0L6_2acntS4744 == 1) {
    #line 263 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    moonbit_free(_M0L6_2aenvS3737);
  }
  _M0L3uriS1467 = _M0L8_2afieldS4268;
  _M0L8_2afieldS4267 = _M0L1kS1476.$0;
  _M0L3strS3806 = _M0L8_2afieldS4267;
  _M0L5startS3807 = _M0L1kS1476.$1;
  _M0L3endS3809 = _M0L1kS1476.$2;
  _M0L6_2atmpS3808 = (int64_t)_M0L3endS3809;
  moonbit_incref(_M0L3strS3806);
  #line 267 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  if (
    _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3806, 1, _M0L5startS3807, _M0L6_2atmpS3808)
  ) {
    moonbit_string_t _M0L8_2afieldS4266 = _M0L1kS1476.$0;
    moonbit_string_t _M0L3strS3824 = _M0L8_2afieldS4266;
    moonbit_string_t _M0L8_2afieldS4265 = _M0L1kS1476.$0;
    moonbit_string_t _M0L3strS3827 = _M0L8_2afieldS4265;
    int32_t _M0L5startS3828 = _M0L1kS1476.$1;
    int32_t _M0L3endS3830 = _M0L1kS1476.$2;
    int64_t _M0L6_2atmpS3829 = (int64_t)_M0L3endS3830;
    int64_t _M0L6_2atmpS3826;
    int32_t _M0L6_2atmpS3825;
    int32_t _M0L4_2axS1505;
    moonbit_incref(_M0L3strS3827);
    moonbit_incref(_M0L3strS3824);
    #line 267 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3826
    = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3827, 0, _M0L5startS3828, _M0L6_2atmpS3829);
    _M0L6_2atmpS3825 = (int32_t)_M0L6_2atmpS3826;
    #line 267 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L4_2axS1505
    = _M0MPC16string6String16unsafe__char__at(_M0L3strS3824, _M0L6_2atmpS3825);
    if (_M0L4_2axS1505 >= 97 && _M0L4_2axS1505 <= 122) {
      moonbit_string_t _M0L8_2afieldS4261 = _M0L1kS1476.$0;
      moonbit_string_t _M0L3strS3817 = _M0L8_2afieldS4261;
      moonbit_string_t _M0L8_2afieldS4260 = _M0L1kS1476.$0;
      moonbit_string_t _M0L3strS3820 = _M0L8_2afieldS4260;
      int32_t _M0L5startS3821 = _M0L1kS1476.$1;
      int32_t _M0L3endS3823 = _M0L1kS1476.$2;
      int64_t _M0L6_2atmpS3822 = (int64_t)_M0L3endS3823;
      int64_t _M0L7_2abindS1723;
      int32_t _M0L6_2atmpS3818;
      int32_t _M0L8_2afieldS4259;
      int32_t _M0L3endS3819;
      struct _M0TPC16string10StringView _M0L4_2axS1506;
      moonbit_incref(_M0L3strS3820);
      moonbit_incref(_M0L3strS3817);
      #line 267 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1723
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3820, 1, _M0L5startS3821, _M0L6_2atmpS3822);
      if (_M0L7_2abindS1723 == 4294967296ll) {
        _M0L6_2atmpS3818 = _M0L1kS1476.$2;
      } else {
        int64_t _M0L7_2aSomeS1507 = _M0L7_2abindS1723;
        _M0L6_2atmpS3818 = (int32_t)_M0L7_2aSomeS1507;
      }
      _M0L8_2afieldS4259 = _M0L1kS1476.$2;
      moonbit_decref(_M0L1kS1476.$0);
      _M0L3endS3819 = _M0L8_2afieldS4259;
      _M0L4_2axS1506
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3818, _M0L3endS3819, _M0L3strS3817
      };
      _M0L1sS1480 = _M0L4_2axS1506;
      goto join_1479;
    } else if (_M0L4_2axS1505 >= 65 && _M0L4_2axS1505 <= 90) {
      moonbit_string_t _M0L8_2afieldS4264 = _M0L1kS1476.$0;
      moonbit_string_t _M0L3strS3810 = _M0L8_2afieldS4264;
      moonbit_string_t _M0L8_2afieldS4263 = _M0L1kS1476.$0;
      moonbit_string_t _M0L3strS3813 = _M0L8_2afieldS4263;
      int32_t _M0L5startS3814 = _M0L1kS1476.$1;
      int32_t _M0L3endS3816 = _M0L1kS1476.$2;
      int64_t _M0L6_2atmpS3815 = (int64_t)_M0L3endS3816;
      int64_t _M0L7_2abindS1724;
      int32_t _M0L6_2atmpS3811;
      int32_t _M0L8_2afieldS4262;
      int32_t _M0L3endS3812;
      struct _M0TPC16string10StringView _M0L4_2axS1508;
      moonbit_incref(_M0L3strS3813);
      moonbit_incref(_M0L3strS3810);
      #line 267 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1724
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3813, 1, _M0L5startS3814, _M0L6_2atmpS3815);
      if (_M0L7_2abindS1724 == 4294967296ll) {
        _M0L6_2atmpS3811 = _M0L1kS1476.$2;
      } else {
        int64_t _M0L7_2aSomeS1509 = _M0L7_2abindS1724;
        _M0L6_2atmpS3811 = (int32_t)_M0L7_2aSomeS1509;
      }
      _M0L8_2afieldS4262 = _M0L1kS1476.$2;
      moonbit_decref(_M0L1kS1476.$0);
      _M0L3endS3812 = _M0L8_2afieldS4262;
      _M0L4_2axS1508
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3811, _M0L3endS3812, _M0L3strS3810
      };
      _M0L1sS1480 = _M0L4_2axS1508;
      goto join_1479;
    } else {
      moonbit_decref(_M0L6sourceS1475.$0);
      moonbit_decref(_M0L3uriS1467);
      moonbit_decref(_M0L17parse__hier__partS1463);
      _M0L1sS1478 = _M0L1kS1476;
      goto join_1477;
    }
  } else {
    void* _M0L55clawteam_2fclawteam_2finternal_2furi_2eParseError_2eEofS3831;
    struct moonbit_result_1 _result_4946;
    moonbit_decref(_M0L1kS1476.$0);
    moonbit_decref(_M0L6sourceS1475.$0);
    moonbit_decref(_M0L3uriS1467);
    moonbit_decref(_M0L17parse__hier__partS1463);
    _M0L55clawteam_2fclawteam_2finternal_2furi_2eParseError_2eEofS3831
    = (struct moonbit_object*)&moonbit_constant_constructor_2 + 1;
    _result_4946.tag = 0;
    _result_4946.data.err
    = _M0L55clawteam_2fclawteam_2finternal_2furi_2eParseError_2eEofS3831;
    return _result_4946;
  }
  join_1479:;
  _M0L8_2aparamS1481 = _M0L1sS1480;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1483;
    struct _M0TPC16string10StringView _M0L1sS1485;
    struct _M0TPC16string10StringView _M0L1kS1486;
    struct _M0TPC16string10StringView _M0L1sS1488;
    moonbit_string_t _M0L8_2afieldS4258 = _M0L8_2aparamS1481.$0;
    moonbit_string_t _M0L3strS3745 = _M0L8_2afieldS4258;
    int32_t _M0L5startS3746 = _M0L8_2aparamS1481.$1;
    int32_t _M0L3endS3748 = _M0L8_2aparamS1481.$2;
    int64_t _M0L6_2atmpS3747 = (int64_t)_M0L3endS3748;
    struct _M0TPC16string10StringView _M0L6_2atmpS3741;
    struct _M0TPC16string10StringView _M0L6_2aoldS4235;
    struct moonbit_result_1 _tmp_4952;
    struct _M0TPC16string10StringView _M0L6_2atmpS3742;
    struct moonbit_result_1 _result_4954;
    void* _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740;
    struct moonbit_result_1 _result_4955;
    moonbit_incref(_M0L3strS3745);
    #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3745, 1, _M0L5startS3746, _M0L6_2atmpS3747)
    ) {
      moonbit_string_t _M0L8_2afieldS4257 = _M0L8_2aparamS1481.$0;
      moonbit_string_t _M0L3strS3798 = _M0L8_2afieldS4257;
      moonbit_string_t _M0L8_2afieldS4256 = _M0L8_2aparamS1481.$0;
      moonbit_string_t _M0L3strS3801 = _M0L8_2afieldS4256;
      int32_t _M0L5startS3802 = _M0L8_2aparamS1481.$1;
      int32_t _M0L3endS3804 = _M0L8_2aparamS1481.$2;
      int64_t _M0L6_2atmpS3803 = (int64_t)_M0L3endS3804;
      int64_t _M0L6_2atmpS3800;
      int32_t _M0L6_2atmpS3799;
      int32_t _M0L4_2axS1490;
      moonbit_incref(_M0L3strS3801);
      moonbit_incref(_M0L3strS3798);
      #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3800
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3801, 0, _M0L5startS3802, _M0L6_2atmpS3803);
      _M0L6_2atmpS3799 = (int32_t)_M0L6_2atmpS3800;
      #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1490
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3798, _M0L6_2atmpS3799);
      if (_M0L4_2axS1490 >= 97 && _M0L4_2axS1490 <= 122) {
        moonbit_string_t _M0L8_2afieldS4238 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3791 = _M0L8_2afieldS4238;
        moonbit_string_t _M0L8_2afieldS4237 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3794 = _M0L8_2afieldS4237;
        int32_t _M0L5startS3795 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3797 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3796 = (int64_t)_M0L3endS3797;
        int64_t _M0L7_2abindS1716;
        int32_t _M0L6_2atmpS3792;
        int32_t _M0L8_2afieldS4236;
        int32_t _M0L3endS3793;
        struct _M0TPC16string10StringView _M0L4_2axS1491;
        moonbit_incref(_M0L3strS3794);
        moonbit_incref(_M0L3strS3791);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1716
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3794, 1, _M0L5startS3795, _M0L6_2atmpS3796);
        if (_M0L7_2abindS1716 == 4294967296ll) {
          _M0L6_2atmpS3792 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1492 = _M0L7_2abindS1716;
          _M0L6_2atmpS3792 = (int32_t)_M0L7_2aSomeS1492;
        }
        _M0L8_2afieldS4236 = _M0L8_2aparamS1481.$2;
        moonbit_decref(_M0L8_2aparamS1481.$0);
        _M0L3endS3793 = _M0L8_2afieldS4236;
        _M0L4_2axS1491
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3792, _M0L3endS3793, _M0L3strS3791
        };
        _M0L1sS1488 = _M0L4_2axS1491;
        goto join_1487;
      } else if (_M0L4_2axS1490 >= 65 && _M0L4_2axS1490 <= 90) {
        moonbit_string_t _M0L8_2afieldS4241 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3784 = _M0L8_2afieldS4241;
        moonbit_string_t _M0L8_2afieldS4240 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3787 = _M0L8_2afieldS4240;
        int32_t _M0L5startS3788 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3790 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3789 = (int64_t)_M0L3endS3790;
        int64_t _M0L7_2abindS1717;
        int32_t _M0L6_2atmpS3785;
        int32_t _M0L8_2afieldS4239;
        int32_t _M0L3endS3786;
        struct _M0TPC16string10StringView _M0L4_2axS1493;
        moonbit_incref(_M0L3strS3787);
        moonbit_incref(_M0L3strS3784);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1717
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3787, 1, _M0L5startS3788, _M0L6_2atmpS3789);
        if (_M0L7_2abindS1717 == 4294967296ll) {
          _M0L6_2atmpS3785 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1494 = _M0L7_2abindS1717;
          _M0L6_2atmpS3785 = (int32_t)_M0L7_2aSomeS1494;
        }
        _M0L8_2afieldS4239 = _M0L8_2aparamS1481.$2;
        moonbit_decref(_M0L8_2aparamS1481.$0);
        _M0L3endS3786 = _M0L8_2afieldS4239;
        _M0L4_2axS1493
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3785, _M0L3endS3786, _M0L3strS3784
        };
        _M0L1sS1488 = _M0L4_2axS1493;
        goto join_1487;
      } else if (_M0L4_2axS1490 >= 48 && _M0L4_2axS1490 <= 57) {
        moonbit_string_t _M0L8_2afieldS4244 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3777 = _M0L8_2afieldS4244;
        moonbit_string_t _M0L8_2afieldS4243 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3780 = _M0L8_2afieldS4243;
        int32_t _M0L5startS3781 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3783 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3782 = (int64_t)_M0L3endS3783;
        int64_t _M0L7_2abindS1718;
        int32_t _M0L6_2atmpS3778;
        int32_t _M0L8_2afieldS4242;
        int32_t _M0L3endS3779;
        struct _M0TPC16string10StringView _M0L4_2axS1495;
        moonbit_incref(_M0L3strS3780);
        moonbit_incref(_M0L3strS3777);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1718
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3780, 1, _M0L5startS3781, _M0L6_2atmpS3782);
        if (_M0L7_2abindS1718 == 4294967296ll) {
          _M0L6_2atmpS3778 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1496 = _M0L7_2abindS1718;
          _M0L6_2atmpS3778 = (int32_t)_M0L7_2aSomeS1496;
        }
        _M0L8_2afieldS4242 = _M0L8_2aparamS1481.$2;
        moonbit_decref(_M0L8_2aparamS1481.$0);
        _M0L3endS3779 = _M0L8_2afieldS4242;
        _M0L4_2axS1495
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3778, _M0L3endS3779, _M0L3strS3777
        };
        _M0L1sS1488 = _M0L4_2axS1495;
        goto join_1487;
      } else if (_M0L4_2axS1490 == 43) {
        moonbit_string_t _M0L8_2afieldS4247 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3770 = _M0L8_2afieldS4247;
        moonbit_string_t _M0L8_2afieldS4246 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3773 = _M0L8_2afieldS4246;
        int32_t _M0L5startS3774 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3776 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3775 = (int64_t)_M0L3endS3776;
        int64_t _M0L7_2abindS1719;
        int32_t _M0L6_2atmpS3771;
        int32_t _M0L8_2afieldS4245;
        int32_t _M0L3endS3772;
        struct _M0TPC16string10StringView _M0L4_2axS1497;
        moonbit_incref(_M0L3strS3773);
        moonbit_incref(_M0L3strS3770);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1719
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3773, 1, _M0L5startS3774, _M0L6_2atmpS3775);
        if (_M0L7_2abindS1719 == 4294967296ll) {
          _M0L6_2atmpS3771 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1498 = _M0L7_2abindS1719;
          _M0L6_2atmpS3771 = (int32_t)_M0L7_2aSomeS1498;
        }
        _M0L8_2afieldS4245 = _M0L8_2aparamS1481.$2;
        moonbit_decref(_M0L8_2aparamS1481.$0);
        _M0L3endS3772 = _M0L8_2afieldS4245;
        _M0L4_2axS1497
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3771, _M0L3endS3772, _M0L3strS3770
        };
        _M0L1sS1488 = _M0L4_2axS1497;
        goto join_1487;
      } else if (_M0L4_2axS1490 == 45) {
        moonbit_string_t _M0L8_2afieldS4250 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3763 = _M0L8_2afieldS4250;
        moonbit_string_t _M0L8_2afieldS4249 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3766 = _M0L8_2afieldS4249;
        int32_t _M0L5startS3767 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3769 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3768 = (int64_t)_M0L3endS3769;
        int64_t _M0L7_2abindS1720;
        int32_t _M0L6_2atmpS3764;
        int32_t _M0L8_2afieldS4248;
        int32_t _M0L3endS3765;
        struct _M0TPC16string10StringView _M0L4_2axS1499;
        moonbit_incref(_M0L3strS3766);
        moonbit_incref(_M0L3strS3763);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1720
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3766, 1, _M0L5startS3767, _M0L6_2atmpS3768);
        if (_M0L7_2abindS1720 == 4294967296ll) {
          _M0L6_2atmpS3764 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1500 = _M0L7_2abindS1720;
          _M0L6_2atmpS3764 = (int32_t)_M0L7_2aSomeS1500;
        }
        _M0L8_2afieldS4248 = _M0L8_2aparamS1481.$2;
        moonbit_decref(_M0L8_2aparamS1481.$0);
        _M0L3endS3765 = _M0L8_2afieldS4248;
        _M0L4_2axS1499
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3764, _M0L3endS3765, _M0L3strS3763
        };
        _M0L1sS1488 = _M0L4_2axS1499;
        goto join_1487;
      } else if (_M0L4_2axS1490 == 46) {
        moonbit_string_t _M0L8_2afieldS4253 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3756 = _M0L8_2afieldS4253;
        moonbit_string_t _M0L8_2afieldS4252 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3759 = _M0L8_2afieldS4252;
        int32_t _M0L5startS3760 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3762 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3761 = (int64_t)_M0L3endS3762;
        int64_t _M0L7_2abindS1721;
        int32_t _M0L6_2atmpS3757;
        int32_t _M0L8_2afieldS4251;
        int32_t _M0L3endS3758;
        struct _M0TPC16string10StringView _M0L4_2axS1501;
        moonbit_incref(_M0L3strS3759);
        moonbit_incref(_M0L3strS3756);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1721
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3759, 1, _M0L5startS3760, _M0L6_2atmpS3761);
        if (_M0L7_2abindS1721 == 4294967296ll) {
          _M0L6_2atmpS3757 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1502 = _M0L7_2abindS1721;
          _M0L6_2atmpS3757 = (int32_t)_M0L7_2aSomeS1502;
        }
        _M0L8_2afieldS4251 = _M0L8_2aparamS1481.$2;
        moonbit_decref(_M0L8_2aparamS1481.$0);
        _M0L3endS3758 = _M0L8_2afieldS4251;
        _M0L4_2axS1501
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3757, _M0L3endS3758, _M0L3strS3756
        };
        _M0L1sS1488 = _M0L4_2axS1501;
        goto join_1487;
      } else if (_M0L4_2axS1490 == 58) {
        moonbit_string_t _M0L8_2afieldS4255 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3749 = _M0L8_2afieldS4255;
        moonbit_string_t _M0L8_2afieldS4254 = _M0L8_2aparamS1481.$0;
        moonbit_string_t _M0L3strS3752 = _M0L8_2afieldS4254;
        int32_t _M0L5startS3753 = _M0L8_2aparamS1481.$1;
        int32_t _M0L3endS3755 = _M0L8_2aparamS1481.$2;
        int64_t _M0L6_2atmpS3754 = (int64_t)_M0L3endS3755;
        int64_t _M0L7_2abindS1722;
        int32_t _M0L6_2atmpS3750;
        int32_t _M0L3endS3751;
        struct _M0TPC16string10StringView _M0L4_2axS1503;
        moonbit_incref(_M0L3strS3752);
        moonbit_incref(_M0L3strS3749);
        #line 269 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1722
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3752, 1, _M0L5startS3753, _M0L6_2atmpS3754);
        if (_M0L7_2abindS1722 == 4294967296ll) {
          _M0L6_2atmpS3750 = _M0L8_2aparamS1481.$2;
        } else {
          int64_t _M0L7_2aSomeS1504 = _M0L7_2abindS1722;
          _M0L6_2atmpS3750 = (int32_t)_M0L7_2aSomeS1504;
        }
        _M0L3endS3751 = _M0L8_2aparamS1481.$2;
        _M0L4_2axS1503
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3750, _M0L3endS3751, _M0L3strS3749
        };
        _M0L1sS1485 = _M0L8_2aparamS1481;
        _M0L1kS1486 = _M0L4_2axS1503;
        goto join_1484;
      } else {
        moonbit_decref(_M0L6sourceS1475.$0);
        moonbit_decref(_M0L3uriS1467);
        moonbit_decref(_M0L17parse__hier__partS1463);
        _M0L1sS1483 = _M0L8_2aparamS1481;
        goto join_1482;
      }
    } else {
      void* _M0L64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingColonS3805;
      struct moonbit_result_1 _result_4951;
      moonbit_decref(_M0L8_2aparamS1481.$0);
      moonbit_decref(_M0L6sourceS1475.$0);
      moonbit_decref(_M0L3uriS1467);
      moonbit_decref(_M0L17parse__hier__partS1463);
      _M0L64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingColonS3805
      = (struct moonbit_object*)&moonbit_constant_constructor_5 + 1;
      _result_4951.tag = 0;
      _result_4951.data.err
      = _M0L64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingColonS3805;
      return _result_4951;
    }
    goto joinlet_4950;
    join_1487:;
    _M0L8_2aparamS1481 = _M0L1sS1488;
    continue;
    joinlet_4950:;
    goto joinlet_4949;
    join_1484:;
    #line 273 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3741
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1475, _M0L1sS1485);
    _M0L6_2aoldS4235
    = (struct _M0TPC16string10StringView){
      _M0L3uriS1467->$0_1, _M0L3uriS1467->$0_2, _M0L3uriS1467->$0_0
    };
    moonbit_decref(_M0L6_2aoldS4235.$0);
    _M0L3uriS1467->$0_0 = _M0L6_2atmpS3741.$0;
    _M0L3uriS1467->$0_1 = _M0L6_2atmpS3741.$1;
    _M0L3uriS1467->$0_2 = _M0L6_2atmpS3741.$2;
    moonbit_decref(_M0L3uriS1467);
    #line 274 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _tmp_4952
    = _M0FP48clawteam8clawteam8internal3uri10parse__uriN17parse__hier__partS1463(_M0L17parse__hier__partS1463, _M0L1kS1486);
    if (_tmp_4952.tag) {
      struct _M0TPC16string10StringView const _M0L5_2aokS3743 =
        _tmp_4952.data.ok;
      _M0L6_2atmpS3742 = _M0L5_2aokS3743;
    } else {
      void* const _M0L6_2aerrS3744 = _tmp_4952.data.err;
      struct moonbit_result_1 _result_4953;
      _result_4953.tag = 0;
      _result_4953.data.err = _M0L6_2aerrS3744;
      return _result_4953;
    }
    _result_4954.tag = 1;
    _result_4954.data.ok = _M0L6_2atmpS3742;
    return _result_4954;
    joinlet_4949:;
    goto joinlet_4948;
    join_1482:;
    _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme));
    Moonbit_object_header(_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme, $0_0) >> 2, 1, 8);
    ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740)->$0_0
    = _M0L1sS1483.$0;
    ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740)->$0_1
    = _M0L1sS1483.$1;
    ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740)->$0_2
    = _M0L1sS1483.$2;
    _result_4955.tag = 0;
    _result_4955.data.err
    = _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3740;
    return _result_4955;
    joinlet_4948:;
    break;
  }
  _result_4956.tag = 1;
  _result_4956.data.ok = _M0L6_2atmpS3739;
  return _result_4956;
  join_1477:;
  _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme));
  Moonbit_object_header(_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme, $0_0) >> 2, 1, 8);
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738)->$0_0
  = _M0L1sS1478.$0;
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738)->$0_1
  = _M0L1sS1478.$1;
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738)->$0_2
  = _M0L1sS1478.$2;
  _result_4957.tag = 0;
  _result_4957.data.err
  = _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3738;
  return _result_4957;
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri10parse__uriN17parse__hier__partS1463(
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1467,
  struct _M0TPC16string10StringView _M0L6sourceS1464
) {
  struct _M0TPC16string10StringView _M0L1sS1466;
  struct _M0TPC16string10StringView _M0L1sS1469;
  moonbit_string_t _M0L8_2afieldS4278;
  moonbit_string_t _M0L3strS3712;
  int32_t _M0L5startS3713;
  int32_t _M0L3endS3715;
  int64_t _M0L6_2atmpS3714;
  struct moonbit_result_1 _tmp_4960;
  struct _M0TPC16string10StringView _M0L6_2atmpS3709;
  struct moonbit_result_1 _result_4962;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2atmpS3707;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2aoldS4270;
  struct _M0TPC16string10StringView _M0L6_2atmpS3708;
  struct moonbit_result_1 _result_4963;
  #line 253 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L8_2afieldS4278 = _M0L6sourceS1464.$0;
  _M0L3strS3712 = _M0L8_2afieldS4278;
  _M0L5startS3713 = _M0L6sourceS1464.$1;
  _M0L3endS3715 = _M0L6sourceS1464.$2;
  _M0L6_2atmpS3714 = (int64_t)_M0L3endS3715;
  moonbit_incref(_M0L3strS3712);
  #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  if (
    _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3712, 2, _M0L5startS3713, _M0L6_2atmpS3714)
  ) {
    moonbit_string_t _M0L8_2afieldS4277 = _M0L6sourceS1464.$0;
    moonbit_string_t _M0L3strS3730 = _M0L8_2afieldS4277;
    moonbit_string_t _M0L8_2afieldS4276 = _M0L6sourceS1464.$0;
    moonbit_string_t _M0L3strS3733 = _M0L8_2afieldS4276;
    int32_t _M0L5startS3734 = _M0L6sourceS1464.$1;
    int32_t _M0L3endS3736 = _M0L6sourceS1464.$2;
    int64_t _M0L6_2atmpS3735 = (int64_t)_M0L3endS3736;
    int64_t _M0L6_2atmpS3732;
    int32_t _M0L6_2atmpS3731;
    int32_t _M0L4_2axS1470;
    moonbit_incref(_M0L3strS3733);
    moonbit_incref(_M0L3strS3730);
    #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3732
    = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3733, 0, _M0L5startS3734, _M0L6_2atmpS3735);
    _M0L6_2atmpS3731 = (int32_t)_M0L6_2atmpS3732;
    #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L4_2axS1470
    = _M0MPC16string6String16unsafe__char__at(_M0L3strS3730, _M0L6_2atmpS3731);
    if (_M0L4_2axS1470 == 47) {
      moonbit_string_t _M0L8_2afieldS4275 = _M0L6sourceS1464.$0;
      moonbit_string_t _M0L3strS3723 = _M0L8_2afieldS4275;
      moonbit_string_t _M0L8_2afieldS4274 = _M0L6sourceS1464.$0;
      moonbit_string_t _M0L3strS3726 = _M0L8_2afieldS4274;
      int32_t _M0L5startS3727 = _M0L6sourceS1464.$1;
      int32_t _M0L3endS3729 = _M0L6sourceS1464.$2;
      int64_t _M0L6_2atmpS3728 = (int64_t)_M0L3endS3729;
      int64_t _M0L6_2atmpS3725;
      int32_t _M0L6_2atmpS3724;
      int32_t _M0L4_2axS1471;
      moonbit_incref(_M0L3strS3726);
      moonbit_incref(_M0L3strS3723);
      #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3725
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3726, 1, _M0L5startS3727, _M0L6_2atmpS3728);
      _M0L6_2atmpS3724 = (int32_t)_M0L6_2atmpS3725;
      #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1471
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3723, _M0L6_2atmpS3724);
      if (_M0L4_2axS1471 == 47) {
        moonbit_string_t _M0L8_2afieldS4273 = _M0L6sourceS1464.$0;
        moonbit_string_t _M0L3strS3716 = _M0L8_2afieldS4273;
        moonbit_string_t _M0L8_2afieldS4272 = _M0L6sourceS1464.$0;
        moonbit_string_t _M0L3strS3719 = _M0L8_2afieldS4272;
        int32_t _M0L5startS3720 = _M0L6sourceS1464.$1;
        int32_t _M0L3endS3722 = _M0L6sourceS1464.$2;
        int64_t _M0L6_2atmpS3721 = (int64_t)_M0L3endS3722;
        int64_t _M0L7_2abindS1715;
        int32_t _M0L6_2atmpS3717;
        int32_t _M0L8_2afieldS4271;
        int32_t _M0L3endS3718;
        struct _M0TPC16string10StringView _M0L4_2axS1472;
        moonbit_incref(_M0L3strS3719);
        moonbit_incref(_M0L3strS3716);
        #line 254 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1715
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3719, 2, _M0L5startS3720, _M0L6_2atmpS3721);
        if (_M0L7_2abindS1715 == 4294967296ll) {
          _M0L6_2atmpS3717 = _M0L6sourceS1464.$2;
        } else {
          int64_t _M0L7_2aSomeS1473 = _M0L7_2abindS1715;
          _M0L6_2atmpS3717 = (int32_t)_M0L7_2aSomeS1473;
        }
        _M0L8_2afieldS4271 = _M0L6sourceS1464.$2;
        moonbit_decref(_M0L6sourceS1464.$0);
        _M0L3endS3718 = _M0L8_2afieldS4271;
        _M0L4_2axS1472
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3717, _M0L3endS3718, _M0L3strS3716
        };
        _M0L1sS1469 = _M0L4_2axS1472;
        goto join_1468;
      } else {
        _M0L1sS1466 = _M0L6sourceS1464;
        goto join_1465;
      }
    } else {
      _M0L1sS1466 = _M0L6sourceS1464;
      goto join_1465;
    }
  } else {
    _M0L1sS1466 = _M0L6sourceS1464;
    goto join_1465;
  }
  join_1468:;
  #line 255 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_4960
  = _M0FP48clawteam8clawteam8internal3uri16parse__authority(_M0L1sS1469, _M0L3uriS1467);
  if (_tmp_4960.tag) {
    struct _M0TPC16string10StringView const _M0L5_2aokS3710 =
      _tmp_4960.data.ok;
    _M0L6_2atmpS3709 = _M0L5_2aokS3710;
  } else {
    void* const _M0L6_2aerrS3711 = _tmp_4960.data.err;
    struct moonbit_result_1 _result_4961;
    _result_4961.tag = 0;
    _result_4961.data.err = _M0L6_2aerrS3711;
    return _result_4961;
  }
  _result_4962.tag = 1;
  _result_4962.data.ok = _M0L6_2atmpS3709;
  return _result_4962;
  join_1465:;
  _M0L6_2atmpS3707 = 0;
  _M0L6_2aoldS4270 = _M0L3uriS1467->$1;
  if (_M0L6_2aoldS4270) {
    moonbit_decref(_M0L6_2aoldS4270);
  }
  _M0L3uriS1467->$1 = _M0L6_2atmpS3707;
  #line 258 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L6_2atmpS3708
  = _M0FP48clawteam8clawteam8internal3uri20parse__path__abempty(_M0L1sS1466, _M0L3uriS1467);
  _result_4963.tag = 1;
  _result_4963.data.ok = _M0L6_2atmpS3708;
  return _result_4963;
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri16parse__authority(
  struct _M0TPC16string10StringView _M0L6sourceS1399,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1351
) {
  void* _M0L4NoneS3704;
  moonbit_string_t _M0L7_2abindS1350;
  int32_t _M0L6_2atmpS3706;
  struct _M0TPC16string10StringView _M0L6_2atmpS3705;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS1349;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2atmpS3490;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2aoldS4320;
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352* _M0L11parse__portS1352;
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376* _M0L11parse__hostS1376;
  struct _M0TPC16string10StringView _M0L1sS1398;
  struct _M0TPC16buffer6Buffer* _M0L4hostS1400;
  void* _M0L4NoneS3703;
  struct _M0TPC13ref3RefGORPC16string10StringViewE* _M0L7port__kS1401;
  struct _M0TPC16string10StringView _M0L6_2atmpS3573;
  struct _M0TPC16string10StringView _M0L8_2aparamS1402;
  struct moonbit_result_1 _result_4988;
  #line 80 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L4NoneS3704
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L7_2abindS1350 = (moonbit_string_t)moonbit_string_literal_0.data;
  _M0L6_2atmpS3706 = Moonbit_array_length(_M0L7_2abindS1350);
  _M0L6_2atmpS3705
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3706, _M0L7_2abindS1350
  };
  _M0L9authorityS1349
  = (struct _M0TP48clawteam8clawteam8internal3uri9Authority*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3uri9Authority));
  Moonbit_object_header(_M0L9authorityS1349)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal3uri9Authority, $0) >> 2, 2, 0);
  _M0L9authorityS1349->$0 = _M0L4NoneS3704;
  _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3705.$0;
  _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3705.$1;
  _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3705.$2;
  _M0L9authorityS1349->$2 = 4294967296ll;
  moonbit_incref(_M0L9authorityS1349);
  _M0L6_2atmpS3490 = _M0L9authorityS1349;
  _M0L6_2aoldS4320 = _M0L3uriS1351->$1;
  if (_M0L6_2aoldS4320) {
    moonbit_decref(_M0L6_2aoldS4320);
  }
  _M0L3uriS1351->$1 = _M0L6_2atmpS3490;
  moonbit_incref(_M0L3uriS1351);
  moonbit_incref(_M0L9authorityS1349);
  _M0L11parse__portS1352
  = (struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352*)moonbit_malloc(sizeof(struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352));
  Moonbit_object_header(_M0L11parse__portS1352)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352, $0) >> 2, 2, 0);
  _M0L11parse__portS1352->$0 = _M0L3uriS1351;
  _M0L11parse__portS1352->$1 = _M0L9authorityS1349;
  moonbit_incref(_M0L11parse__portS1352);
  moonbit_incref(_M0L3uriS1351);
  moonbit_incref(_M0L9authorityS1349);
  _M0L11parse__hostS1376
  = (struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376*)moonbit_malloc(sizeof(struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376));
  Moonbit_object_header(_M0L11parse__hostS1376)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376, $0) >> 2, 3, 0);
  _M0L11parse__hostS1376->$0 = _M0L11parse__portS1352;
  _M0L11parse__hostS1376->$1 = _M0L3uriS1351;
  _M0L11parse__hostS1376->$2 = _M0L9authorityS1349;
  _M0L1sS1398 = _M0L6sourceS1399;
  moonbit_incref(_M0L1sS1398.$0);
  #line 126 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L4hostS1400 = _M0FPC16buffer11new_2einner(0);
  _M0L4NoneS3703
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L7port__kS1401
  = (struct _M0TPC13ref3RefGORPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPC16string10StringViewE));
  Moonbit_object_header(_M0L7port__kS1401)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L7port__kS1401->$0 = _M0L4NoneS3703;
  _M0L8_2aparamS1402 = _M0L1sS1398;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1404;
    int32_t _M0L1cS1405;
    struct _M0TPC16string10StringView _M0L1kS1408;
    struct _M0TPC16string10StringView _M0L1sS1437;
    struct _M0TPC16string10StringView _M0L1sS1439;
    struct _M0TPC16string10StringView _M0L1kS1440;
    struct _M0TPC16string10StringView _M0L1sS1442;
    struct _M0TPC16string10StringView _M0L1kS1443;
    struct _M0TPC16string10StringView _M0L1sS1450;
    struct _M0TPC16string10StringView _M0L1kS1451;
    moonbit_string_t _M0L8_2afieldS4319 = _M0L8_2aparamS1402.$0;
    moonbit_string_t _M0L3strS3657 = _M0L8_2afieldS4319;
    int32_t _M0L5startS3658 = _M0L8_2aparamS1402.$1;
    int32_t _M0L3endS3660 = _M0L8_2aparamS1402.$2;
    int64_t _M0L6_2atmpS3659 = (int64_t)_M0L3endS3660;
    struct _M0TPC16string10StringView _M0L6_2atmpS3653;
    void* _M0L4SomeS3652;
    void* _M0L6_2aoldS4304;
    struct moonbit_result_1 _tmp_4971;
    struct _M0TPC16string10StringView _M0L6_2atmpS3654;
    struct moonbit_result_1 _result_4973;
    struct _M0TPC16string10StringView _M0L7port__kS1445;
    void* _M0L8_2afieldS4303;
    int32_t _M0L6_2acntS4746;
    void* _M0L7_2abindS1446;
    struct moonbit_result_1 _tmp_4976;
    struct _M0TPC16string10StringView _M0L6_2atmpS3647;
    struct moonbit_result_1 _result_4978;
    struct _M0TPC16string10StringView _M0L6_2atmpS3645;
    struct _M0TPC16string10StringView _M0L6_2aoldS4300;
    void* _M0L4SomeS3646;
    void* _M0L6_2aoldS4299;
    struct _M0TPC16string10StringView _M0L6_2atmpS3644;
    struct _M0TPC16string10StringView _M0L6_2aoldS4298;
    struct moonbit_result_1 _result_4979;
    struct _M0TPC13ref3RefGiE* _M0L1bS1409;
    struct _M0TPC16string10StringView _M0L1kS1410;
    struct _M0TPC16string10StringView _M0L1kS1413;
    int32_t _M0L2c0S1414;
    struct _M0TPC16string10StringView _M0L1kS1416;
    int32_t _M0L2c0S1417;
    moonbit_string_t _M0L8_2afieldS4297;
    moonbit_string_t _M0L3strS3619;
    int32_t _M0L5startS3620;
    int32_t _M0L3endS3622;
    int64_t _M0L6_2atmpS3621;
    int32_t _M0L6_2atmpS3618;
    int32_t _M0L6_2atmpS3617;
    int32_t _M0L6_2atmpS3616;
    int32_t _M0L6_2atmpS3615;
    int32_t _M0L6_2atmpS3614;
    int32_t _M0L6_2atmpS3613;
    int32_t _M0L6_2atmpS3612;
    void* _M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611;
    struct moonbit_result_1 _result_4983;
    struct _M0TPC16string10StringView _M0L1kS1423;
    struct _M0TPC16string10StringView _M0L1kS1426;
    int32_t _M0L2c1S1427;
    struct _M0TPC16string10StringView _M0L1kS1429;
    int32_t _M0L2c1S1430;
    moonbit_string_t _M0L8_2afieldS4288;
    moonbit_string_t _M0L3strS3586;
    int32_t _M0L5startS3587;
    int32_t _M0L3endS3589;
    int64_t _M0L6_2atmpS3588;
    int32_t _M0L3valS3583;
    int32_t _M0L6_2atmpS3585;
    int32_t _M0L6_2atmpS3584;
    int32_t _M0L6_2atmpS3582;
    int32_t _M0L3valS3578;
    int32_t _M0L6_2atmpS3581;
    int32_t _M0L6_2atmpS3580;
    int32_t _M0L6_2atmpS3579;
    int32_t _M0L6_2atmpS3577;
    void* _M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576;
    struct moonbit_result_1 _result_4987;
    int32_t _M0L8_2afieldS4279;
    int32_t _M0L3valS3575;
    int32_t _M0L6_2atmpS3574;
    moonbit_incref(_M0L3strS3657);
    #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3657, 1, _M0L5startS3658, _M0L6_2atmpS3659)
    ) {
      moonbit_string_t _M0L8_2afieldS4318 = _M0L8_2aparamS1402.$0;
      moonbit_string_t _M0L3strS3696 = _M0L8_2afieldS4318;
      moonbit_string_t _M0L8_2afieldS4317 = _M0L8_2aparamS1402.$0;
      moonbit_string_t _M0L3strS3699 = _M0L8_2afieldS4317;
      int32_t _M0L5startS3700 = _M0L8_2aparamS1402.$1;
      int32_t _M0L3endS3702 = _M0L8_2aparamS1402.$2;
      int64_t _M0L6_2atmpS3701 = (int64_t)_M0L3endS3702;
      int64_t _M0L6_2atmpS3698;
      int32_t _M0L6_2atmpS3697;
      int32_t _M0L4_2axS1452;
      moonbit_incref(_M0L3strS3699);
      moonbit_incref(_M0L3strS3696);
      #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3698
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3699, 0, _M0L5startS3700, _M0L6_2atmpS3701);
      _M0L6_2atmpS3697 = (int32_t)_M0L6_2atmpS3698;
      #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1452
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3696, _M0L6_2atmpS3697);
      if (_M0L4_2axS1452 == 64) {
        moonbit_string_t _M0L8_2afieldS4306;
        moonbit_string_t _M0L3strS3689;
        moonbit_string_t _M0L8_2afieldS4305;
        moonbit_string_t _M0L3strS3692;
        int32_t _M0L5startS3693;
        int32_t _M0L3endS3695;
        int64_t _M0L6_2atmpS3694;
        int64_t _M0L7_2abindS1710;
        int32_t _M0L6_2atmpS3690;
        int32_t _M0L3endS3691;
        struct _M0TPC16string10StringView _M0L4_2axS1453;
        moonbit_decref(_M0L7port__kS1401);
        moonbit_decref(_M0L4hostS1400);
        moonbit_decref(_M0L11parse__portS1352);
        moonbit_decref(_M0L3uriS1351);
        _M0L8_2afieldS4306 = _M0L8_2aparamS1402.$0;
        _M0L3strS3689 = _M0L8_2afieldS4306;
        _M0L8_2afieldS4305 = _M0L8_2aparamS1402.$0;
        _M0L3strS3692 = _M0L8_2afieldS4305;
        _M0L5startS3693 = _M0L8_2aparamS1402.$1;
        _M0L3endS3695 = _M0L8_2aparamS1402.$2;
        _M0L6_2atmpS3694 = (int64_t)_M0L3endS3695;
        moonbit_incref(_M0L3strS3692);
        moonbit_incref(_M0L3strS3689);
        #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1710
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3692, 1, _M0L5startS3693, _M0L6_2atmpS3694);
        if (_M0L7_2abindS1710 == 4294967296ll) {
          _M0L6_2atmpS3690 = _M0L8_2aparamS1402.$2;
        } else {
          int64_t _M0L7_2aSomeS1454 = _M0L7_2abindS1710;
          _M0L6_2atmpS3690 = (int32_t)_M0L7_2aSomeS1454;
        }
        _M0L3endS3691 = _M0L8_2aparamS1402.$2;
        _M0L4_2axS1453
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3690, _M0L3endS3691, _M0L3strS3689
        };
        _M0L1sS1450 = _M0L8_2aparamS1402;
        _M0L1kS1451 = _M0L4_2axS1453;
        goto join_1449;
      } else if (_M0L4_2axS1452 == 47) {
        moonbit_string_t _M0L8_2afieldS4308;
        moonbit_string_t _M0L3strS3682;
        moonbit_string_t _M0L8_2afieldS4307;
        moonbit_string_t _M0L3strS3685;
        int32_t _M0L5startS3686;
        int32_t _M0L3endS3688;
        int64_t _M0L6_2atmpS3687;
        int64_t _M0L7_2abindS1711;
        int32_t _M0L6_2atmpS3683;
        int32_t _M0L3endS3684;
        struct _M0TPC16string10StringView _M0L4_2axS1455;
        moonbit_decref(_M0L4hostS1400);
        moonbit_decref(_M0L11parse__hostS1376);
        _M0L8_2afieldS4308 = _M0L8_2aparamS1402.$0;
        _M0L3strS3682 = _M0L8_2afieldS4308;
        _M0L8_2afieldS4307 = _M0L8_2aparamS1402.$0;
        _M0L3strS3685 = _M0L8_2afieldS4307;
        _M0L5startS3686 = _M0L8_2aparamS1402.$1;
        _M0L3endS3688 = _M0L8_2aparamS1402.$2;
        _M0L6_2atmpS3687 = (int64_t)_M0L3endS3688;
        moonbit_incref(_M0L3strS3685);
        moonbit_incref(_M0L3strS3682);
        #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1711
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3685, 1, _M0L5startS3686, _M0L6_2atmpS3687);
        if (_M0L7_2abindS1711 == 4294967296ll) {
          _M0L6_2atmpS3683 = _M0L8_2aparamS1402.$2;
        } else {
          int64_t _M0L7_2aSomeS1456 = _M0L7_2abindS1711;
          _M0L6_2atmpS3683 = (int32_t)_M0L7_2aSomeS1456;
        }
        _M0L3endS3684 = _M0L8_2aparamS1402.$2;
        _M0L4_2axS1455
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3683, _M0L3endS3684, _M0L3strS3682
        };
        _M0L1sS1442 = _M0L8_2aparamS1402;
        _M0L1kS1443 = _M0L4_2axS1455;
        goto join_1441;
      } else if (_M0L4_2axS1452 == 58) {
        moonbit_string_t _M0L8_2afieldS4310 = _M0L8_2aparamS1402.$0;
        moonbit_string_t _M0L3strS3675 = _M0L8_2afieldS4310;
        moonbit_string_t _M0L8_2afieldS4309 = _M0L8_2aparamS1402.$0;
        moonbit_string_t _M0L3strS3678 = _M0L8_2afieldS4309;
        int32_t _M0L5startS3679 = _M0L8_2aparamS1402.$1;
        int32_t _M0L3endS3681 = _M0L8_2aparamS1402.$2;
        int64_t _M0L6_2atmpS3680 = (int64_t)_M0L3endS3681;
        int64_t _M0L7_2abindS1712;
        int32_t _M0L6_2atmpS3676;
        int32_t _M0L3endS3677;
        struct _M0TPC16string10StringView _M0L4_2axS1457;
        moonbit_incref(_M0L3strS3678);
        moonbit_incref(_M0L3strS3675);
        #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1712
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3678, 1, _M0L5startS3679, _M0L6_2atmpS3680);
        if (_M0L7_2abindS1712 == 4294967296ll) {
          _M0L6_2atmpS3676 = _M0L8_2aparamS1402.$2;
        } else {
          int64_t _M0L7_2aSomeS1458 = _M0L7_2abindS1712;
          _M0L6_2atmpS3676 = (int32_t)_M0L7_2aSomeS1458;
        }
        _M0L3endS3677 = _M0L8_2aparamS1402.$2;
        _M0L4_2axS1457
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3676, _M0L3endS3677, _M0L3strS3675
        };
        _M0L1sS1439 = _M0L8_2aparamS1402;
        _M0L1kS1440 = _M0L4_2axS1457;
        goto join_1438;
      } else if (_M0L4_2axS1452 == 37) {
        moonbit_string_t _M0L8_2afieldS4313 = _M0L8_2aparamS1402.$0;
        moonbit_string_t _M0L3strS3668 = _M0L8_2afieldS4313;
        moonbit_string_t _M0L8_2afieldS4312 = _M0L8_2aparamS1402.$0;
        moonbit_string_t _M0L3strS3671 = _M0L8_2afieldS4312;
        int32_t _M0L5startS3672 = _M0L8_2aparamS1402.$1;
        int32_t _M0L3endS3674 = _M0L8_2aparamS1402.$2;
        int64_t _M0L6_2atmpS3673 = (int64_t)_M0L3endS3674;
        int64_t _M0L7_2abindS1713;
        int32_t _M0L6_2atmpS3669;
        int32_t _M0L8_2afieldS4311;
        int32_t _M0L3endS3670;
        struct _M0TPC16string10StringView _M0L4_2axS1459;
        moonbit_incref(_M0L3strS3671);
        moonbit_incref(_M0L3strS3668);
        #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1713
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3671, 1, _M0L5startS3672, _M0L6_2atmpS3673);
        if (_M0L7_2abindS1713 == 4294967296ll) {
          _M0L6_2atmpS3669 = _M0L8_2aparamS1402.$2;
        } else {
          int64_t _M0L7_2aSomeS1460 = _M0L7_2abindS1713;
          _M0L6_2atmpS3669 = (int32_t)_M0L7_2aSomeS1460;
        }
        _M0L8_2afieldS4311 = _M0L8_2aparamS1402.$2;
        moonbit_decref(_M0L8_2aparamS1402.$0);
        _M0L3endS3670 = _M0L8_2afieldS4311;
        _M0L4_2axS1459
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3669, _M0L3endS3670, _M0L3strS3668
        };
        _M0L1kS1408 = _M0L4_2axS1459;
        goto join_1407;
      } else {
        moonbit_string_t _M0L8_2afieldS4316 = _M0L8_2aparamS1402.$0;
        moonbit_string_t _M0L3strS3661 = _M0L8_2afieldS4316;
        moonbit_string_t _M0L8_2afieldS4315 = _M0L8_2aparamS1402.$0;
        moonbit_string_t _M0L3strS3664 = _M0L8_2afieldS4315;
        int32_t _M0L5startS3665 = _M0L8_2aparamS1402.$1;
        int32_t _M0L3endS3667 = _M0L8_2aparamS1402.$2;
        int64_t _M0L6_2atmpS3666 = (int64_t)_M0L3endS3667;
        int64_t _M0L7_2abindS1714;
        int32_t _M0L6_2atmpS3662;
        int32_t _M0L8_2afieldS4314;
        int32_t _M0L3endS3663;
        struct _M0TPC16string10StringView _M0L4_2axS1461;
        moonbit_incref(_M0L3strS3664);
        moonbit_incref(_M0L3strS3661);
        #line 128 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1714
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3664, 1, _M0L5startS3665, _M0L6_2atmpS3666);
        if (_M0L7_2abindS1714 == 4294967296ll) {
          _M0L6_2atmpS3662 = _M0L8_2aparamS1402.$2;
        } else {
          int64_t _M0L7_2aSomeS1462 = _M0L7_2abindS1714;
          _M0L6_2atmpS3662 = (int32_t)_M0L7_2aSomeS1462;
        }
        _M0L8_2afieldS4314 = _M0L8_2aparamS1402.$2;
        moonbit_decref(_M0L8_2aparamS1402.$0);
        _M0L3endS3663 = _M0L8_2afieldS4314;
        _M0L4_2axS1461
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3662, _M0L3endS3663, _M0L3strS3661
        };
        _M0L1sS1404 = _M0L4_2axS1461;
        _M0L1cS1405 = _M0L4_2axS1452;
        goto join_1403;
      }
    } else {
      moonbit_decref(_M0L7port__kS1401);
      moonbit_decref(_M0L4hostS1400);
      moonbit_decref(_M0L11parse__hostS1376);
      moonbit_decref(_M0L11parse__portS1352);
      moonbit_decref(_M0L3uriS1351);
      _M0L1sS1437 = _M0L8_2aparamS1402;
      goto join_1436;
    }
    goto joinlet_4970;
    join_1449:;
    #line 130 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3653
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1399, _M0L1sS1450);
    _M0L4SomeS3652
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_M0L4SomeS3652)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3652)->$0_0
    = _M0L6_2atmpS3653.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3652)->$0_1
    = _M0L6_2atmpS3653.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3652)->$0_2
    = _M0L6_2atmpS3653.$2;
    _M0L6_2aoldS4304 = _M0L9authorityS1349->$0;
    moonbit_decref(_M0L6_2aoldS4304);
    _M0L9authorityS1349->$0 = _M0L4SomeS3652;
    moonbit_decref(_M0L9authorityS1349);
    #line 131 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _tmp_4971
    = _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__hostS1376(_M0L11parse__hostS1376, _M0L1kS1451);
    if (_tmp_4971.tag) {
      struct _M0TPC16string10StringView const _M0L5_2aokS3655 =
        _tmp_4971.data.ok;
      _M0L6_2atmpS3654 = _M0L5_2aokS3655;
    } else {
      void* const _M0L6_2aerrS3656 = _tmp_4971.data.err;
      struct moonbit_result_1 _result_4972;
      _result_4972.tag = 0;
      _result_4972.data.err = _M0L6_2aerrS3656;
      return _result_4972;
    }
    _result_4973.tag = 1;
    _result_4973.data.ok = _M0L6_2atmpS3654;
    return _result_4973;
    joinlet_4970:;
    goto joinlet_4969;
    join_1441:;
    _M0L8_2afieldS4303 = _M0L7port__kS1401->$0;
    _M0L6_2acntS4746 = Moonbit_object_header(_M0L7port__kS1401)->rc;
    if (_M0L6_2acntS4746 > 1) {
      int32_t _M0L11_2anew__cntS4747 = _M0L6_2acntS4746 - 1;
      Moonbit_object_header(_M0L7port__kS1401)->rc = _M0L11_2anew__cntS4747;
      moonbit_incref(_M0L8_2afieldS4303);
    } else if (_M0L6_2acntS4746 == 1) {
      #line 134 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      moonbit_free(_M0L7port__kS1401);
    }
    _M0L7_2abindS1446 = _M0L8_2afieldS4303;
    switch (Moonbit_object_tag(_M0L7_2abindS1446)) {
      case 1: {
        struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1447;
        struct _M0TPC16string10StringView _M0L8_2afieldS4301;
        int32_t _M0L6_2acntS4748;
        struct _M0TPC16string10StringView _M0L10_2aport__kS1448;
        moonbit_decref(_M0L1kS1443.$0);
        moonbit_decref(_M0L1sS1442.$0);
        moonbit_decref(_M0L6sourceS1399.$0);
        moonbit_decref(_M0L3uriS1351);
        moonbit_decref(_M0L9authorityS1349);
        _M0L7_2aSomeS1447
        = (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS1446;
        _M0L8_2afieldS4301
        = (struct _M0TPC16string10StringView){
          _M0L7_2aSomeS1447->$0_1,
            _M0L7_2aSomeS1447->$0_2,
            _M0L7_2aSomeS1447->$0_0
        };
        _M0L6_2acntS4748 = Moonbit_object_header(_M0L7_2aSomeS1447)->rc;
        if (_M0L6_2acntS4748 > 1) {
          int32_t _M0L11_2anew__cntS4749 = _M0L6_2acntS4748 - 1;
          Moonbit_object_header(_M0L7_2aSomeS1447)->rc
          = _M0L11_2anew__cntS4749;
          moonbit_incref(_M0L8_2afieldS4301.$0);
        } else if (_M0L6_2acntS4748 == 1) {
          #line 134 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
          moonbit_free(_M0L7_2aSomeS1447);
        }
        _M0L10_2aport__kS1448 = _M0L8_2afieldS4301;
        _M0L7port__kS1445 = _M0L10_2aport__kS1448;
        goto join_1444;
        break;
      }
      default: {
        struct _M0TPC16string10StringView _M0L6_2atmpS3650;
        struct _M0TPC16string10StringView _M0L6_2aoldS4302;
        struct _M0TPC16string10StringView _M0L6_2atmpS3651;
        struct moonbit_result_1 _result_4975;
        moonbit_decref(_M0L7_2abindS1446);
        moonbit_decref(_M0L11parse__portS1352);
        #line 137 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L6_2atmpS3650
        = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1399, _M0L1sS1442);
        _M0L6_2aoldS4302
        = (struct _M0TPC16string10StringView){
          _M0L9authorityS1349->$1_1,
            _M0L9authorityS1349->$1_2,
            _M0L9authorityS1349->$1_0
        };
        moonbit_decref(_M0L6_2aoldS4302.$0);
        _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3650.$0;
        _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3650.$1;
        _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3650.$2;
        moonbit_decref(_M0L9authorityS1349);
        #line 138 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L6_2atmpS3651
        = _M0FP48clawteam8clawteam8internal3uri20parse__path__abempty(_M0L1kS1443, _M0L3uriS1351);
        _result_4975.tag = 1;
        _result_4975.data.ok = _M0L6_2atmpS3651;
        return _result_4975;
        break;
      }
    }
    goto joinlet_4974;
    join_1444:;
    #line 135 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _tmp_4976
    = _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__portS1352(_M0L11parse__portS1352, _M0L7port__kS1445);
    if (_tmp_4976.tag) {
      struct _M0TPC16string10StringView const _M0L5_2aokS3648 =
        _tmp_4976.data.ok;
      _M0L6_2atmpS3647 = _M0L5_2aokS3648;
    } else {
      void* const _M0L6_2aerrS3649 = _tmp_4976.data.err;
      struct moonbit_result_1 _result_4977;
      _result_4977.tag = 0;
      _result_4977.data.err = _M0L6_2aerrS3649;
      return _result_4977;
    }
    _result_4978.tag = 1;
    _result_4978.data.ok = _M0L6_2atmpS3647;
    return _result_4978;
    joinlet_4974:;
    joinlet_4969:;
    goto joinlet_4968;
    join_1438:;
    moonbit_incref(_M0L6sourceS1399.$0);
    #line 141 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3645
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1399, _M0L1sS1439);
    _M0L6_2aoldS4300
    = (struct _M0TPC16string10StringView){
      _M0L9authorityS1349->$1_1,
        _M0L9authorityS1349->$1_2,
        _M0L9authorityS1349->$1_0
    };
    moonbit_decref(_M0L6_2aoldS4300.$0);
    _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3645.$0;
    _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3645.$1;
    _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3645.$2;
    moonbit_incref(_M0L1kS1440.$0);
    _M0L4SomeS3646
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_M0L4SomeS3646)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3646)->$0_0
    = _M0L1kS1440.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3646)->$0_1
    = _M0L1kS1440.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3646)->$0_2
    = _M0L1kS1440.$2;
    _M0L6_2aoldS4299 = _M0L7port__kS1401->$0;
    moonbit_decref(_M0L6_2aoldS4299);
    _M0L7port__kS1401->$0 = _M0L4SomeS3646;
    _M0L8_2aparamS1402 = _M0L1kS1440;
    continue;
    joinlet_4968:;
    goto joinlet_4967;
    join_1436:;
    moonbit_incref(_M0L1sS1437.$0);
    #line 146 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3644
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1399, _M0L1sS1437);
    _M0L6_2aoldS4298
    = (struct _M0TPC16string10StringView){
      _M0L9authorityS1349->$1_1,
        _M0L9authorityS1349->$1_2,
        _M0L9authorityS1349->$1_0
    };
    moonbit_decref(_M0L6_2aoldS4298.$0);
    _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3644.$0;
    _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3644.$1;
    _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3644.$2;
    moonbit_decref(_M0L9authorityS1349);
    _result_4979.tag = 1;
    _result_4979.data.ok = _M0L1sS1437;
    return _result_4979;
    joinlet_4967:;
    goto joinlet_4966;
    join_1407:;
    _M0L1bS1409
    = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
    Moonbit_object_header(_M0L1bS1409)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
    _M0L1bS1409->$0 = 0;
    _M0L8_2afieldS4297 = _M0L1kS1408.$0;
    _M0L3strS3619 = _M0L8_2afieldS4297;
    _M0L5startS3620 = _M0L1kS1408.$1;
    _M0L3endS3622 = _M0L1kS1408.$2;
    _M0L6_2atmpS3621 = (int64_t)_M0L3endS3622;
    moonbit_incref(_M0L3strS3619);
    #line 151 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3619, 1, _M0L5startS3620, _M0L6_2atmpS3621)
    ) {
      moonbit_string_t _M0L8_2afieldS4296 = _M0L1kS1408.$0;
      moonbit_string_t _M0L3strS3637 = _M0L8_2afieldS4296;
      moonbit_string_t _M0L8_2afieldS4295 = _M0L1kS1408.$0;
      moonbit_string_t _M0L3strS3640 = _M0L8_2afieldS4295;
      int32_t _M0L5startS3641 = _M0L1kS1408.$1;
      int32_t _M0L3endS3643 = _M0L1kS1408.$2;
      int64_t _M0L6_2atmpS3642 = (int64_t)_M0L3endS3643;
      int64_t _M0L6_2atmpS3639;
      int32_t _M0L6_2atmpS3638;
      int32_t _M0L4_2axS1418;
      moonbit_incref(_M0L3strS3640);
      moonbit_incref(_M0L3strS3637);
      #line 151 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3639
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3640, 0, _M0L5startS3641, _M0L6_2atmpS3642);
      _M0L6_2atmpS3638 = (int32_t)_M0L6_2atmpS3639;
      #line 151 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1418
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3637, _M0L6_2atmpS3638);
      if (_M0L4_2axS1418 >= 48 && _M0L4_2axS1418 <= 57) {
        moonbit_string_t _M0L8_2afieldS4291 = _M0L1kS1408.$0;
        moonbit_string_t _M0L3strS3630 = _M0L8_2afieldS4291;
        moonbit_string_t _M0L8_2afieldS4290 = _M0L1kS1408.$0;
        moonbit_string_t _M0L3strS3633 = _M0L8_2afieldS4290;
        int32_t _M0L5startS3634 = _M0L1kS1408.$1;
        int32_t _M0L3endS3636 = _M0L1kS1408.$2;
        int64_t _M0L6_2atmpS3635 = (int64_t)_M0L3endS3636;
        int64_t _M0L7_2abindS1706;
        int32_t _M0L6_2atmpS3631;
        int32_t _M0L8_2afieldS4289;
        int32_t _M0L3endS3632;
        struct _M0TPC16string10StringView _M0L4_2axS1419;
        moonbit_incref(_M0L3strS3633);
        moonbit_incref(_M0L3strS3630);
        #line 151 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1706
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3633, 1, _M0L5startS3634, _M0L6_2atmpS3635);
        if (_M0L7_2abindS1706 == 4294967296ll) {
          _M0L6_2atmpS3631 = _M0L1kS1408.$2;
        } else {
          int64_t _M0L7_2aSomeS1420 = _M0L7_2abindS1706;
          _M0L6_2atmpS3631 = (int32_t)_M0L7_2aSomeS1420;
        }
        _M0L8_2afieldS4289 = _M0L1kS1408.$2;
        moonbit_decref(_M0L1kS1408.$0);
        _M0L3endS3632 = _M0L8_2afieldS4289;
        _M0L4_2axS1419
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3631, _M0L3endS3632, _M0L3strS3630
        };
        _M0L1kS1416 = _M0L4_2axS1419;
        _M0L2c0S1417 = _M0L4_2axS1418;
        goto join_1415;
      } else if (_M0L4_2axS1418 >= 65 && _M0L4_2axS1418 <= 70) {
        moonbit_string_t _M0L8_2afieldS4294 = _M0L1kS1408.$0;
        moonbit_string_t _M0L3strS3623 = _M0L8_2afieldS4294;
        moonbit_string_t _M0L8_2afieldS4293 = _M0L1kS1408.$0;
        moonbit_string_t _M0L3strS3626 = _M0L8_2afieldS4293;
        int32_t _M0L5startS3627 = _M0L1kS1408.$1;
        int32_t _M0L3endS3629 = _M0L1kS1408.$2;
        int64_t _M0L6_2atmpS3628 = (int64_t)_M0L3endS3629;
        int64_t _M0L7_2abindS1707;
        int32_t _M0L6_2atmpS3624;
        int32_t _M0L8_2afieldS4292;
        int32_t _M0L3endS3625;
        struct _M0TPC16string10StringView _M0L4_2axS1421;
        moonbit_incref(_M0L3strS3626);
        moonbit_incref(_M0L3strS3623);
        #line 151 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1707
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3626, 1, _M0L5startS3627, _M0L6_2atmpS3628);
        if (_M0L7_2abindS1707 == 4294967296ll) {
          _M0L6_2atmpS3624 = _M0L1kS1408.$2;
        } else {
          int64_t _M0L7_2aSomeS1422 = _M0L7_2abindS1707;
          _M0L6_2atmpS3624 = (int32_t)_M0L7_2aSomeS1422;
        }
        _M0L8_2afieldS4292 = _M0L1kS1408.$2;
        moonbit_decref(_M0L1kS1408.$0);
        _M0L3endS3625 = _M0L8_2afieldS4292;
        _M0L4_2axS1421
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3624, _M0L3endS3625, _M0L3strS3623
        };
        _M0L1kS1413 = _M0L4_2axS1421;
        _M0L2c0S1414 = _M0L4_2axS1418;
        goto join_1412;
      } else {
        moonbit_decref(_M0L1bS1409);
        moonbit_decref(_M0L7port__kS1401);
        moonbit_decref(_M0L4hostS1400);
        moonbit_decref(_M0L6sourceS1399.$0);
        moonbit_decref(_M0L11parse__hostS1376);
        moonbit_decref(_M0L11parse__portS1352);
        moonbit_decref(_M0L3uriS1351);
        moonbit_decref(_M0L9authorityS1349);
        goto join_1411;
      }
    } else {
      moonbit_decref(_M0L1bS1409);
      moonbit_decref(_M0L7port__kS1401);
      moonbit_decref(_M0L4hostS1400);
      moonbit_decref(_M0L6sourceS1399.$0);
      moonbit_decref(_M0L11parse__hostS1376);
      moonbit_decref(_M0L11parse__portS1352);
      moonbit_decref(_M0L3uriS1351);
      moonbit_decref(_M0L9authorityS1349);
      goto join_1411;
    }
    goto joinlet_4982;
    join_1415:;
    _M0L6_2atmpS3618 = _M0L2c0S1417;
    _M0L6_2atmpS3617 = _M0L6_2atmpS3618 - 48;
    _M0L6_2atmpS3616 = _M0L6_2atmpS3617 * 16;
    _M0L1bS1409->$0 = _M0L6_2atmpS3616;
    _M0L1kS1410 = _M0L1kS1416;
    joinlet_4982:;
    goto joinlet_4981;
    join_1412:;
    _M0L6_2atmpS3615 = _M0L2c0S1414;
    _M0L6_2atmpS3614 = _M0L6_2atmpS3615 - 65;
    _M0L6_2atmpS3613 = _M0L6_2atmpS3614 + 10;
    _M0L6_2atmpS3612 = _M0L6_2atmpS3613 * 16;
    _M0L1bS1409->$0 = _M0L6_2atmpS3612;
    _M0L1kS1410 = _M0L1kS1413;
    joinlet_4981:;
    goto joinlet_4980;
    join_1411:;
    _M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding));
    Moonbit_object_header(_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding, $0_0) >> 2, 1, 6);
    ((struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding*)_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611)->$0_0
    = _M0L1kS1408.$0;
    ((struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding*)_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611)->$0_1
    = _M0L1kS1408.$1;
    ((struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding*)_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611)->$0_2
    = _M0L1kS1408.$2;
    _result_4983.tag = 0;
    _result_4983.data.err
    = _M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3611;
    return _result_4983;
    joinlet_4980:;
    _M0L8_2afieldS4288 = _M0L1kS1410.$0;
    _M0L3strS3586 = _M0L8_2afieldS4288;
    _M0L5startS3587 = _M0L1kS1410.$1;
    _M0L3endS3589 = _M0L1kS1410.$2;
    _M0L6_2atmpS3588 = (int64_t)_M0L3endS3589;
    moonbit_incref(_M0L3strS3586);
    #line 162 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3586, 1, _M0L5startS3587, _M0L6_2atmpS3588)
    ) {
      moonbit_string_t _M0L8_2afieldS4287 = _M0L1kS1410.$0;
      moonbit_string_t _M0L3strS3604 = _M0L8_2afieldS4287;
      moonbit_string_t _M0L8_2afieldS4286 = _M0L1kS1410.$0;
      moonbit_string_t _M0L3strS3607 = _M0L8_2afieldS4286;
      int32_t _M0L5startS3608 = _M0L1kS1410.$1;
      int32_t _M0L3endS3610 = _M0L1kS1410.$2;
      int64_t _M0L6_2atmpS3609 = (int64_t)_M0L3endS3610;
      int64_t _M0L6_2atmpS3606;
      int32_t _M0L6_2atmpS3605;
      int32_t _M0L4_2axS1431;
      moonbit_incref(_M0L3strS3607);
      moonbit_incref(_M0L3strS3604);
      #line 162 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3606
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3607, 0, _M0L5startS3608, _M0L6_2atmpS3609);
      _M0L6_2atmpS3605 = (int32_t)_M0L6_2atmpS3606;
      #line 162 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1431
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3604, _M0L6_2atmpS3605);
      if (_M0L4_2axS1431 >= 48 && _M0L4_2axS1431 <= 57) {
        moonbit_string_t _M0L8_2afieldS4282 = _M0L1kS1410.$0;
        moonbit_string_t _M0L3strS3597 = _M0L8_2afieldS4282;
        moonbit_string_t _M0L8_2afieldS4281 = _M0L1kS1410.$0;
        moonbit_string_t _M0L3strS3600 = _M0L8_2afieldS4281;
        int32_t _M0L5startS3601 = _M0L1kS1410.$1;
        int32_t _M0L3endS3603 = _M0L1kS1410.$2;
        int64_t _M0L6_2atmpS3602 = (int64_t)_M0L3endS3603;
        int64_t _M0L7_2abindS1708;
        int32_t _M0L6_2atmpS3598;
        int32_t _M0L8_2afieldS4280;
        int32_t _M0L3endS3599;
        struct _M0TPC16string10StringView _M0L4_2axS1432;
        moonbit_incref(_M0L3strS3600);
        moonbit_incref(_M0L3strS3597);
        #line 162 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1708
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3600, 1, _M0L5startS3601, _M0L6_2atmpS3602);
        if (_M0L7_2abindS1708 == 4294967296ll) {
          _M0L6_2atmpS3598 = _M0L1kS1410.$2;
        } else {
          int64_t _M0L7_2aSomeS1433 = _M0L7_2abindS1708;
          _M0L6_2atmpS3598 = (int32_t)_M0L7_2aSomeS1433;
        }
        _M0L8_2afieldS4280 = _M0L1kS1410.$2;
        moonbit_decref(_M0L1kS1410.$0);
        _M0L3endS3599 = _M0L8_2afieldS4280;
        _M0L4_2axS1432
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3598, _M0L3endS3599, _M0L3strS3597
        };
        _M0L1kS1429 = _M0L4_2axS1432;
        _M0L2c1S1430 = _M0L4_2axS1431;
        goto join_1428;
      } else if (_M0L4_2axS1431 >= 65 && _M0L4_2axS1431 <= 70) {
        moonbit_string_t _M0L8_2afieldS4285 = _M0L1kS1410.$0;
        moonbit_string_t _M0L3strS3590 = _M0L8_2afieldS4285;
        moonbit_string_t _M0L8_2afieldS4284 = _M0L1kS1410.$0;
        moonbit_string_t _M0L3strS3593 = _M0L8_2afieldS4284;
        int32_t _M0L5startS3594 = _M0L1kS1410.$1;
        int32_t _M0L3endS3596 = _M0L1kS1410.$2;
        int64_t _M0L6_2atmpS3595 = (int64_t)_M0L3endS3596;
        int64_t _M0L7_2abindS1709;
        int32_t _M0L6_2atmpS3591;
        int32_t _M0L8_2afieldS4283;
        int32_t _M0L3endS3592;
        struct _M0TPC16string10StringView _M0L4_2axS1434;
        moonbit_incref(_M0L3strS3593);
        moonbit_incref(_M0L3strS3590);
        #line 162 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1709
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3593, 1, _M0L5startS3594, _M0L6_2atmpS3595);
        if (_M0L7_2abindS1709 == 4294967296ll) {
          _M0L6_2atmpS3591 = _M0L1kS1410.$2;
        } else {
          int64_t _M0L7_2aSomeS1435 = _M0L7_2abindS1709;
          _M0L6_2atmpS3591 = (int32_t)_M0L7_2aSomeS1435;
        }
        _M0L8_2afieldS4283 = _M0L1kS1410.$2;
        moonbit_decref(_M0L1kS1410.$0);
        _M0L3endS3592 = _M0L8_2afieldS4283;
        _M0L4_2axS1434
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3591, _M0L3endS3592, _M0L3strS3590
        };
        _M0L1kS1426 = _M0L4_2axS1434;
        _M0L2c1S1427 = _M0L4_2axS1431;
        goto join_1425;
      } else {
        moonbit_decref(_M0L1bS1409);
        moonbit_decref(_M0L7port__kS1401);
        moonbit_decref(_M0L4hostS1400);
        moonbit_decref(_M0L6sourceS1399.$0);
        moonbit_decref(_M0L11parse__hostS1376);
        moonbit_decref(_M0L11parse__portS1352);
        moonbit_decref(_M0L3uriS1351);
        moonbit_decref(_M0L9authorityS1349);
        goto join_1424;
      }
    } else {
      moonbit_decref(_M0L1bS1409);
      moonbit_decref(_M0L7port__kS1401);
      moonbit_decref(_M0L4hostS1400);
      moonbit_decref(_M0L6sourceS1399.$0);
      moonbit_decref(_M0L11parse__hostS1376);
      moonbit_decref(_M0L11parse__portS1352);
      moonbit_decref(_M0L3uriS1351);
      moonbit_decref(_M0L9authorityS1349);
      goto join_1424;
    }
    goto joinlet_4986;
    join_1428:;
    _M0L3valS3583 = _M0L1bS1409->$0;
    _M0L6_2atmpS3585 = _M0L2c1S1430;
    _M0L6_2atmpS3584 = _M0L6_2atmpS3585 - 48;
    _M0L6_2atmpS3582 = _M0L3valS3583 + _M0L6_2atmpS3584;
    _M0L1bS1409->$0 = _M0L6_2atmpS3582;
    _M0L1kS1423 = _M0L1kS1429;
    joinlet_4986:;
    goto joinlet_4985;
    join_1425:;
    _M0L3valS3578 = _M0L1bS1409->$0;
    _M0L6_2atmpS3581 = _M0L2c1S1427;
    _M0L6_2atmpS3580 = _M0L6_2atmpS3581 - 65;
    _M0L6_2atmpS3579 = _M0L6_2atmpS3580 + 10;
    _M0L6_2atmpS3577 = _M0L3valS3578 + _M0L6_2atmpS3579;
    _M0L1bS1409->$0 = _M0L6_2atmpS3577;
    _M0L1kS1423 = _M0L1kS1426;
    joinlet_4985:;
    goto joinlet_4984;
    join_1424:;
    _M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding));
    Moonbit_object_header(_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding, $0_0) >> 2, 1, 6);
    ((struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding*)_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576)->$0_0
    = _M0L1kS1410.$0;
    ((struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding*)_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576)->$0_1
    = _M0L1kS1410.$1;
    ((struct _M0DTPC15error5Error74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncoding*)_M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576)->$0_2
    = _M0L1kS1410.$2;
    _result_4987.tag = 0;
    _result_4987.data.err
    = _M0L74clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPercentEncodingS3576;
    return _result_4987;
    joinlet_4984:;
    _M0L8_2afieldS4279 = _M0L1bS1409->$0;
    moonbit_decref(_M0L1bS1409);
    _M0L3valS3575 = _M0L8_2afieldS4279;
    _M0L6_2atmpS3574 = _M0L3valS3575 & 0xff;
    moonbit_incref(_M0L4hostS1400);
    #line 173 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0MPC16buffer6Buffer11write__byte(_M0L4hostS1400, _M0L6_2atmpS3574);
    _M0L8_2aparamS1402 = _M0L1kS1423;
    continue;
    joinlet_4966:;
    goto joinlet_4965;
    join_1403:;
    moonbit_incref(_M0L4hostS1400);
    #line 177 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0IPC16buffer6BufferPB6Logger11write__char(_M0L4hostS1400, _M0L1cS1405);
    _M0L8_2aparamS1402 = _M0L1sS1404;
    continue;
    joinlet_4965:;
    break;
  }
  _result_4988.tag = 1;
  _result_4988.data.ok = _M0L6_2atmpS3573;
  return _result_4988;
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__hostS1376(
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__host_7c1376* _M0L6_2aenvS3532,
  struct _M0TPC16string10StringView _M0L6sourceS1377
) {
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4336;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS1349;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L8_2afieldS4335;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1351;
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352* _M0L8_2afieldS4334;
  int32_t _M0L6_2acntS4750;
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352* _M0L11parse__portS1352;
  struct _M0TPC16string10StringView _M0L1sS1378;
  struct _M0TPC16string10StringView _M0L6_2atmpS3533;
  struct _M0TPC16string10StringView _M0L8_2aparamS1379;
  struct moonbit_result_1 _result_4999;
  #line 106 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L8_2afieldS4336 = _M0L6_2aenvS3532->$2;
  _M0L9authorityS1349 = _M0L8_2afieldS4336;
  _M0L8_2afieldS4335 = _M0L6_2aenvS3532->$1;
  _M0L3uriS1351 = _M0L8_2afieldS4335;
  _M0L8_2afieldS4334 = _M0L6_2aenvS3532->$0;
  _M0L6_2acntS4750 = Moonbit_object_header(_M0L6_2aenvS3532)->rc;
  if (_M0L6_2acntS4750 > 1) {
    int32_t _M0L11_2anew__cntS4751 = _M0L6_2acntS4750 - 1;
    Moonbit_object_header(_M0L6_2aenvS3532)->rc = _M0L11_2anew__cntS4751;
    moonbit_incref(_M0L9authorityS1349);
    moonbit_incref(_M0L3uriS1351);
    moonbit_incref(_M0L8_2afieldS4334);
  } else if (_M0L6_2acntS4750 == 1) {
    #line 106 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    moonbit_free(_M0L6_2aenvS3532);
  }
  _M0L11parse__portS1352 = _M0L8_2afieldS4334;
  _M0L1sS1378 = _M0L6sourceS1377;
  moonbit_incref(_M0L1sS1378.$0);
  _M0L8_2aparamS1379 = _M0L1sS1378;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1381;
    struct _M0TPC16string10StringView _M0L1sS1384;
    struct _M0TPC16string10StringView _M0L1sS1386;
    struct _M0TPC16string10StringView _M0L1kS1387;
    struct _M0TPC16string10StringView _M0L1sS1389;
    struct _M0TPC16string10StringView _M0L1kS1390;
    moonbit_string_t _M0L8_2afieldS4333 = _M0L8_2aparamS1379.$0;
    moonbit_string_t _M0L3strS3541 = _M0L8_2afieldS4333;
    int32_t _M0L5startS3542 = _M0L8_2aparamS1379.$1;
    int32_t _M0L3endS3544 = _M0L8_2aparamS1379.$2;
    int64_t _M0L6_2atmpS3543 = (int64_t)_M0L3endS3544;
    struct _M0TPC16string10StringView _M0L6_2atmpS3537;
    struct _M0TPC16string10StringView _M0L6_2aoldS4323;
    struct moonbit_result_1 _tmp_4994;
    struct _M0TPC16string10StringView _M0L6_2atmpS3538;
    struct moonbit_result_1 _result_4996;
    struct _M0TPC16string10StringView _M0L6_2atmpS3535;
    struct _M0TPC16string10StringView _M0L6_2aoldS4322;
    struct _M0TPC16string10StringView _M0L6_2atmpS3536;
    struct moonbit_result_1 _result_4997;
    struct _M0TPC16string10StringView _M0L6_2atmpS3534;
    struct _M0TPC16string10StringView _M0L6_2aoldS4321;
    struct moonbit_result_1 _result_4998;
    moonbit_incref(_M0L3strS3541);
    #line 108 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3541, 1, _M0L5startS3542, _M0L6_2atmpS3543)
    ) {
      moonbit_string_t _M0L8_2afieldS4332 = _M0L8_2aparamS1379.$0;
      moonbit_string_t _M0L3strS3566 = _M0L8_2afieldS4332;
      moonbit_string_t _M0L8_2afieldS4331 = _M0L8_2aparamS1379.$0;
      moonbit_string_t _M0L3strS3569 = _M0L8_2afieldS4331;
      int32_t _M0L5startS3570 = _M0L8_2aparamS1379.$1;
      int32_t _M0L3endS3572 = _M0L8_2aparamS1379.$2;
      int64_t _M0L6_2atmpS3571 = (int64_t)_M0L3endS3572;
      int64_t _M0L6_2atmpS3568;
      int32_t _M0L6_2atmpS3567;
      int32_t _M0L4_2axS1391;
      moonbit_incref(_M0L3strS3569);
      moonbit_incref(_M0L3strS3566);
      #line 108 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3568
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3569, 0, _M0L5startS3570, _M0L6_2atmpS3571);
      _M0L6_2atmpS3567 = (int32_t)_M0L6_2atmpS3568;
      #line 108 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1391
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3566, _M0L6_2atmpS3567);
      if (_M0L4_2axS1391 == 58) {
        moonbit_string_t _M0L8_2afieldS4325;
        moonbit_string_t _M0L3strS3559;
        moonbit_string_t _M0L8_2afieldS4324;
        moonbit_string_t _M0L3strS3562;
        int32_t _M0L5startS3563;
        int32_t _M0L3endS3565;
        int64_t _M0L6_2atmpS3564;
        int64_t _M0L7_2abindS1703;
        int32_t _M0L6_2atmpS3560;
        int32_t _M0L3endS3561;
        struct _M0TPC16string10StringView _M0L4_2axS1392;
        moonbit_decref(_M0L3uriS1351);
        _M0L8_2afieldS4325 = _M0L8_2aparamS1379.$0;
        _M0L3strS3559 = _M0L8_2afieldS4325;
        _M0L8_2afieldS4324 = _M0L8_2aparamS1379.$0;
        _M0L3strS3562 = _M0L8_2afieldS4324;
        _M0L5startS3563 = _M0L8_2aparamS1379.$1;
        _M0L3endS3565 = _M0L8_2aparamS1379.$2;
        _M0L6_2atmpS3564 = (int64_t)_M0L3endS3565;
        moonbit_incref(_M0L3strS3562);
        moonbit_incref(_M0L3strS3559);
        #line 108 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1703
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3562, 1, _M0L5startS3563, _M0L6_2atmpS3564);
        if (_M0L7_2abindS1703 == 4294967296ll) {
          _M0L6_2atmpS3560 = _M0L8_2aparamS1379.$2;
        } else {
          int64_t _M0L7_2aSomeS1393 = _M0L7_2abindS1703;
          _M0L6_2atmpS3560 = (int32_t)_M0L7_2aSomeS1393;
        }
        _M0L3endS3561 = _M0L8_2aparamS1379.$2;
        _M0L4_2axS1392
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3560, _M0L3endS3561, _M0L3strS3559
        };
        _M0L1sS1389 = _M0L8_2aparamS1379;
        _M0L1kS1390 = _M0L4_2axS1392;
        goto join_1388;
      } else if (_M0L4_2axS1391 == 47) {
        moonbit_string_t _M0L8_2afieldS4327;
        moonbit_string_t _M0L3strS3552;
        moonbit_string_t _M0L8_2afieldS4326;
        moonbit_string_t _M0L3strS3555;
        int32_t _M0L5startS3556;
        int32_t _M0L3endS3558;
        int64_t _M0L6_2atmpS3557;
        int64_t _M0L7_2abindS1704;
        int32_t _M0L6_2atmpS3553;
        int32_t _M0L3endS3554;
        struct _M0TPC16string10StringView _M0L4_2axS1394;
        moonbit_decref(_M0L11parse__portS1352);
        _M0L8_2afieldS4327 = _M0L8_2aparamS1379.$0;
        _M0L3strS3552 = _M0L8_2afieldS4327;
        _M0L8_2afieldS4326 = _M0L8_2aparamS1379.$0;
        _M0L3strS3555 = _M0L8_2afieldS4326;
        _M0L5startS3556 = _M0L8_2aparamS1379.$1;
        _M0L3endS3558 = _M0L8_2aparamS1379.$2;
        _M0L6_2atmpS3557 = (int64_t)_M0L3endS3558;
        moonbit_incref(_M0L3strS3555);
        moonbit_incref(_M0L3strS3552);
        #line 108 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1704
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3555, 1, _M0L5startS3556, _M0L6_2atmpS3557);
        if (_M0L7_2abindS1704 == 4294967296ll) {
          _M0L6_2atmpS3553 = _M0L8_2aparamS1379.$2;
        } else {
          int64_t _M0L7_2aSomeS1395 = _M0L7_2abindS1704;
          _M0L6_2atmpS3553 = (int32_t)_M0L7_2aSomeS1395;
        }
        _M0L3endS3554 = _M0L8_2aparamS1379.$2;
        _M0L4_2axS1394
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3553, _M0L3endS3554, _M0L3strS3552
        };
        _M0L1sS1386 = _M0L8_2aparamS1379;
        _M0L1kS1387 = _M0L4_2axS1394;
        goto join_1385;
      } else {
        moonbit_string_t _M0L8_2afieldS4330 = _M0L8_2aparamS1379.$0;
        moonbit_string_t _M0L3strS3545 = _M0L8_2afieldS4330;
        moonbit_string_t _M0L8_2afieldS4329 = _M0L8_2aparamS1379.$0;
        moonbit_string_t _M0L3strS3548 = _M0L8_2afieldS4329;
        int32_t _M0L5startS3549 = _M0L8_2aparamS1379.$1;
        int32_t _M0L3endS3551 = _M0L8_2aparamS1379.$2;
        int64_t _M0L6_2atmpS3550 = (int64_t)_M0L3endS3551;
        int64_t _M0L7_2abindS1705;
        int32_t _M0L6_2atmpS3546;
        int32_t _M0L8_2afieldS4328;
        int32_t _M0L3endS3547;
        struct _M0TPC16string10StringView _M0L4_2axS1396;
        moonbit_incref(_M0L3strS3548);
        moonbit_incref(_M0L3strS3545);
        #line 108 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1705
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3548, 1, _M0L5startS3549, _M0L6_2atmpS3550);
        if (_M0L7_2abindS1705 == 4294967296ll) {
          _M0L6_2atmpS3546 = _M0L8_2aparamS1379.$2;
        } else {
          int64_t _M0L7_2aSomeS1397 = _M0L7_2abindS1705;
          _M0L6_2atmpS3546 = (int32_t)_M0L7_2aSomeS1397;
        }
        _M0L8_2afieldS4328 = _M0L8_2aparamS1379.$2;
        moonbit_decref(_M0L8_2aparamS1379.$0);
        _M0L3endS3547 = _M0L8_2afieldS4328;
        _M0L4_2axS1396
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3546, _M0L3endS3547, _M0L3strS3545
        };
        _M0L1sS1381 = _M0L4_2axS1396;
        goto join_1380;
      }
    } else {
      moonbit_decref(_M0L11parse__portS1352);
      moonbit_decref(_M0L3uriS1351);
      _M0L1sS1384 = _M0L8_2aparamS1379;
      goto join_1383;
    }
    goto joinlet_4993;
    join_1388:;
    #line 110 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3537
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1377, _M0L1sS1389);
    _M0L6_2aoldS4323
    = (struct _M0TPC16string10StringView){
      _M0L9authorityS1349->$1_1,
        _M0L9authorityS1349->$1_2,
        _M0L9authorityS1349->$1_0
    };
    moonbit_decref(_M0L6_2aoldS4323.$0);
    _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3537.$0;
    _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3537.$1;
    _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3537.$2;
    moonbit_decref(_M0L9authorityS1349);
    #line 111 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _tmp_4994
    = _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__portS1352(_M0L11parse__portS1352, _M0L1kS1390);
    if (_tmp_4994.tag) {
      struct _M0TPC16string10StringView const _M0L5_2aokS3539 =
        _tmp_4994.data.ok;
      _M0L6_2atmpS3538 = _M0L5_2aokS3539;
    } else {
      void* const _M0L6_2aerrS3540 = _tmp_4994.data.err;
      struct moonbit_result_1 _result_4995;
      _result_4995.tag = 0;
      _result_4995.data.err = _M0L6_2aerrS3540;
      return _result_4995;
    }
    _result_4996.tag = 1;
    _result_4996.data.ok = _M0L6_2atmpS3538;
    return _result_4996;
    joinlet_4993:;
    goto joinlet_4992;
    join_1385:;
    #line 114 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3535
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1377, _M0L1sS1386);
    _M0L6_2aoldS4322
    = (struct _M0TPC16string10StringView){
      _M0L9authorityS1349->$1_1,
        _M0L9authorityS1349->$1_2,
        _M0L9authorityS1349->$1_0
    };
    moonbit_decref(_M0L6_2aoldS4322.$0);
    _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3535.$0;
    _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3535.$1;
    _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3535.$2;
    moonbit_decref(_M0L9authorityS1349);
    #line 115 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3536
    = _M0FP48clawteam8clawteam8internal3uri20parse__path__abempty(_M0L1kS1387, _M0L3uriS1351);
    _result_4997.tag = 1;
    _result_4997.data.ok = _M0L6_2atmpS3536;
    return _result_4997;
    joinlet_4992:;
    goto joinlet_4991;
    join_1383:;
    moonbit_incref(_M0L1sS1384.$0);
    #line 118 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3534
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1377, _M0L1sS1384);
    _M0L6_2aoldS4321
    = (struct _M0TPC16string10StringView){
      _M0L9authorityS1349->$1_1,
        _M0L9authorityS1349->$1_2,
        _M0L9authorityS1349->$1_0
    };
    moonbit_decref(_M0L6_2aoldS4321.$0);
    _M0L9authorityS1349->$1_0 = _M0L6_2atmpS3534.$0;
    _M0L9authorityS1349->$1_1 = _M0L6_2atmpS3534.$1;
    _M0L9authorityS1349->$1_2 = _M0L6_2atmpS3534.$2;
    moonbit_decref(_M0L9authorityS1349);
    _result_4998.tag = 1;
    _result_4998.data.ok = _M0L1sS1384;
    return _result_4998;
    joinlet_4991:;
    goto joinlet_4990;
    join_1380:;
    _M0L8_2aparamS1379 = _M0L1sS1381;
    continue;
    joinlet_4990:;
    break;
  }
  _result_4999.tag = 1;
  _result_4999.data.ok = _M0L6_2atmpS3533;
  return _result_4999;
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3uri16parse__authorityN11parse__portS1352(
  struct _M0R79_24clawteam_2fclawteam_2finternal_2furi_2eparse__authority_2eparse__port_7c1352* _M0L6_2aenvS3491,
  struct _M0TPC16string10StringView _M0L6sourceS1353
) {
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4349;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L9authorityS1349;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L8_2afieldS4348;
  int32_t _M0L6_2acntS4752;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1351;
  moonbit_string_t _M0L8_2afieldS4347;
  moonbit_string_t _M0L3strS3492;
  int32_t _M0L5startS3493;
  int32_t _M0L3endS3495;
  int64_t _M0L6_2atmpS3494;
  struct _M0TPC16string10StringView _M0L1sS1354;
  struct _M0TUiRPC16string10StringViewE* _M0L8_2atupleS3531;
  struct _M0TPC16string10StringView _M0L6_2atmpS3496;
  struct _M0TUiRPC16string10StringViewE* _M0L8_2aparamS1355;
  struct moonbit_result_1 _result_5009;
  #line 86 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L8_2afieldS4349 = _M0L6_2aenvS3491->$1;
  _M0L9authorityS1349 = _M0L8_2afieldS4349;
  _M0L8_2afieldS4348 = _M0L6_2aenvS3491->$0;
  _M0L6_2acntS4752 = Moonbit_object_header(_M0L6_2aenvS3491)->rc;
  if (_M0L6_2acntS4752 > 1) {
    int32_t _M0L11_2anew__cntS4753 = _M0L6_2acntS4752 - 1;
    Moonbit_object_header(_M0L6_2aenvS3491)->rc = _M0L11_2anew__cntS4753;
    moonbit_incref(_M0L9authorityS1349);
    moonbit_incref(_M0L8_2afieldS4348);
  } else if (_M0L6_2acntS4752 == 1) {
    #line 86 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    moonbit_free(_M0L6_2aenvS3491);
  }
  _M0L3uriS1351 = _M0L8_2afieldS4348;
  _M0L8_2afieldS4347 = _M0L6sourceS1353.$0;
  _M0L3strS3492 = _M0L8_2afieldS4347;
  _M0L5startS3493 = _M0L6sourceS1353.$1;
  _M0L3endS3495 = _M0L6sourceS1353.$2;
  _M0L6_2atmpS3494 = (int64_t)_M0L3endS3495;
  moonbit_incref(_M0L3strS3492);
  #line 87 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  if (
    _M0MPC16string6String24char__length__eq_2einner(_M0L3strS3492, 0, _M0L5startS3493, _M0L6_2atmpS3494)
  ) {
    struct moonbit_result_1 _result_5000;
    moonbit_decref(_M0L3uriS1351);
    moonbit_decref(_M0L9authorityS1349);
    _result_5000.tag = 1;
    _result_5000.data.ok = _M0L6sourceS1353;
    return _result_5000;
  }
  _M0L1sS1354 = _M0L6sourceS1353;
  _M0L8_2atupleS3531
  = (struct _M0TUiRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewE));
  Moonbit_object_header(_M0L8_2atupleS3531)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewE, $1_0) >> 2, 1, 0);
  _M0L8_2atupleS3531->$0 = 0;
  _M0L8_2atupleS3531->$1_0 = _M0L1sS1354.$0;
  _M0L8_2atupleS3531->$1_1 = _M0L1sS1354.$1;
  _M0L8_2atupleS3531->$1_2 = _M0L1sS1354.$2;
  _M0L8_2aparamS1355 = _M0L8_2atupleS3531;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1357;
    int32_t _M0L4portS1359;
    struct _M0TPC16string10StringView _M0L1sS1360;
    int32_t _M0L4portS1362;
    struct _M0TPC16string10StringView _M0L1kS1363;
    int32_t _M0L1cS1364;
    int32_t _M0L4portS1367;
    struct _M0TPC16string10StringView _M0L1kS1368;
    int32_t _M0L7_2aportS1369 = _M0L8_2aparamS1355->$0;
    struct _M0TPC16string10StringView _M0L8_2afieldS4346 =
      (struct _M0TPC16string10StringView){_M0L8_2aparamS1355->$1_1,
                                            _M0L8_2aparamS1355->$1_2,
                                            _M0L8_2aparamS1355->$1_0};
    int32_t _M0L6_2acntS4754 = Moonbit_object_header(_M0L8_2aparamS1355)->rc;
    struct _M0TPC16string10StringView _M0L4_2axS1370;
    moonbit_string_t _M0L8_2afieldS4345;
    moonbit_string_t _M0L3strS3506;
    int32_t _M0L5startS3507;
    int32_t _M0L3endS3509;
    int64_t _M0L6_2atmpS3508;
    int64_t _M0L6_2atmpS3504;
    struct _M0TPC16string10StringView _M0L6_2atmpS3505;
    struct moonbit_result_1 _result_5006;
    int32_t _M0L6_2atmpS3502;
    int32_t _M0L6_2atmpS3503;
    int32_t _M0L6_2atmpS3501;
    int32_t _M0L6_2atmpS3500;
    struct _M0TUiRPC16string10StringViewE* _M0L8_2atupleS3499;
    int64_t _M0L6_2atmpS3498;
    struct moonbit_result_1 _result_5007;
    void* _M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497;
    struct moonbit_result_1 _result_5008;
    if (_M0L6_2acntS4754 > 1) {
      int32_t _M0L11_2anew__cntS4755 = _M0L6_2acntS4754 - 1;
      Moonbit_object_header(_M0L8_2aparamS1355)->rc = _M0L11_2anew__cntS4755;
      moonbit_incref(_M0L8_2afieldS4346.$0);
    } else if (_M0L6_2acntS4754 == 1) {
      #line 91 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      moonbit_free(_M0L8_2aparamS1355);
    }
    _M0L4_2axS1370 = _M0L8_2afieldS4346;
    _M0L8_2afieldS4345 = _M0L4_2axS1370.$0;
    _M0L3strS3506 = _M0L8_2afieldS4345;
    _M0L5startS3507 = _M0L4_2axS1370.$1;
    _M0L3endS3509 = _M0L4_2axS1370.$2;
    _M0L6_2atmpS3508 = (int64_t)_M0L3endS3509;
    moonbit_incref(_M0L3strS3506);
    #line 91 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3506, 1, _M0L5startS3507, _M0L6_2atmpS3508)
    ) {
      moonbit_string_t _M0L8_2afieldS4344 = _M0L4_2axS1370.$0;
      moonbit_string_t _M0L3strS3524 = _M0L8_2afieldS4344;
      moonbit_string_t _M0L8_2afieldS4343 = _M0L4_2axS1370.$0;
      moonbit_string_t _M0L3strS3527 = _M0L8_2afieldS4343;
      int32_t _M0L5startS3528 = _M0L4_2axS1370.$1;
      int32_t _M0L3endS3530 = _M0L4_2axS1370.$2;
      int64_t _M0L6_2atmpS3529 = (int64_t)_M0L3endS3530;
      int64_t _M0L6_2atmpS3526;
      int32_t _M0L6_2atmpS3525;
      int32_t _M0L4_2axS1371;
      moonbit_incref(_M0L3strS3527);
      moonbit_incref(_M0L3strS3524);
      #line 91 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3526
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3527, 0, _M0L5startS3528, _M0L6_2atmpS3529);
      _M0L6_2atmpS3525 = (int32_t)_M0L6_2atmpS3526;
      #line 91 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1371
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3524, _M0L6_2atmpS3525);
      if (_M0L4_2axS1371 == 47) {
        moonbit_string_t _M0L8_2afieldS4339 = _M0L4_2axS1370.$0;
        moonbit_string_t _M0L3strS3517 = _M0L8_2afieldS4339;
        moonbit_string_t _M0L8_2afieldS4338 = _M0L4_2axS1370.$0;
        moonbit_string_t _M0L3strS3520 = _M0L8_2afieldS4338;
        int32_t _M0L5startS3521 = _M0L4_2axS1370.$1;
        int32_t _M0L3endS3523 = _M0L4_2axS1370.$2;
        int64_t _M0L6_2atmpS3522 = (int64_t)_M0L3endS3523;
        int64_t _M0L7_2abindS1701;
        int32_t _M0L6_2atmpS3518;
        int32_t _M0L8_2afieldS4337;
        int32_t _M0L3endS3519;
        struct _M0TPC16string10StringView _M0L4_2axS1372;
        moonbit_incref(_M0L3strS3520);
        moonbit_incref(_M0L3strS3517);
        #line 91 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1701
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3520, 1, _M0L5startS3521, _M0L6_2atmpS3522);
        if (_M0L7_2abindS1701 == 4294967296ll) {
          _M0L6_2atmpS3518 = _M0L4_2axS1370.$2;
        } else {
          int64_t _M0L7_2aSomeS1373 = _M0L7_2abindS1701;
          _M0L6_2atmpS3518 = (int32_t)_M0L7_2aSomeS1373;
        }
        _M0L8_2afieldS4337 = _M0L4_2axS1370.$2;
        moonbit_decref(_M0L4_2axS1370.$0);
        _M0L3endS3519 = _M0L8_2afieldS4337;
        _M0L4_2axS1372
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3518, _M0L3endS3519, _M0L3strS3517
        };
        _M0L4portS1367 = _M0L7_2aportS1369;
        _M0L1kS1368 = _M0L4_2axS1372;
        goto join_1366;
      } else if (_M0L4_2axS1371 >= 48 && _M0L4_2axS1371 <= 57) {
        moonbit_string_t _M0L8_2afieldS4342 = _M0L4_2axS1370.$0;
        moonbit_string_t _M0L3strS3510 = _M0L8_2afieldS4342;
        moonbit_string_t _M0L8_2afieldS4341 = _M0L4_2axS1370.$0;
        moonbit_string_t _M0L3strS3513 = _M0L8_2afieldS4341;
        int32_t _M0L5startS3514 = _M0L4_2axS1370.$1;
        int32_t _M0L3endS3516 = _M0L4_2axS1370.$2;
        int64_t _M0L6_2atmpS3515 = (int64_t)_M0L3endS3516;
        int64_t _M0L7_2abindS1702;
        int32_t _M0L6_2atmpS3511;
        int32_t _M0L8_2afieldS4340;
        int32_t _M0L3endS3512;
        struct _M0TPC16string10StringView _M0L4_2axS1374;
        moonbit_incref(_M0L3strS3513);
        moonbit_incref(_M0L3strS3510);
        #line 91 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1702
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3513, 1, _M0L5startS3514, _M0L6_2atmpS3515);
        if (_M0L7_2abindS1702 == 4294967296ll) {
          _M0L6_2atmpS3511 = _M0L4_2axS1370.$2;
        } else {
          int64_t _M0L7_2aSomeS1375 = _M0L7_2abindS1702;
          _M0L6_2atmpS3511 = (int32_t)_M0L7_2aSomeS1375;
        }
        _M0L8_2afieldS4340 = _M0L4_2axS1370.$2;
        moonbit_decref(_M0L4_2axS1370.$0);
        _M0L3endS3512 = _M0L8_2afieldS4340;
        _M0L4_2axS1374
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3511, _M0L3endS3512, _M0L3strS3510
        };
        _M0L4portS1362 = _M0L7_2aportS1369;
        _M0L1kS1363 = _M0L4_2axS1374;
        _M0L1cS1364 = _M0L4_2axS1371;
        goto join_1361;
      } else {
        moonbit_decref(_M0L3uriS1351);
        moonbit_decref(_M0L9authorityS1349);
        _M0L1sS1357 = _M0L4_2axS1370;
        goto join_1356;
      }
    } else {
      moonbit_decref(_M0L3uriS1351);
      _M0L4portS1359 = _M0L7_2aportS1369;
      _M0L1sS1360 = _M0L4_2axS1370;
      goto join_1358;
    }
    goto joinlet_5005;
    join_1366:;
    _M0L6_2atmpS3504 = (int64_t)_M0L4portS1367;
    _M0L9authorityS1349->$2 = _M0L6_2atmpS3504;
    moonbit_decref(_M0L9authorityS1349);
    #line 94 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3505
    = _M0FP48clawteam8clawteam8internal3uri20parse__path__abempty(_M0L1kS1368, _M0L3uriS1351);
    _result_5006.tag = 1;
    _result_5006.data.ok = _M0L6_2atmpS3505;
    return _result_5006;
    joinlet_5005:;
    goto joinlet_5004;
    join_1361:;
    _M0L6_2atmpS3502 = _M0L4portS1362 * 10;
    _M0L6_2atmpS3503 = _M0L1cS1364;
    _M0L6_2atmpS3501 = _M0L6_2atmpS3502 + _M0L6_2atmpS3503;
    _M0L6_2atmpS3500 = _M0L6_2atmpS3501 - 48;
    _M0L8_2atupleS3499
    = (struct _M0TUiRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TUiRPC16string10StringViewE));
    Moonbit_object_header(_M0L8_2atupleS3499)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUiRPC16string10StringViewE, $1_0) >> 2, 1, 0);
    _M0L8_2atupleS3499->$0 = _M0L6_2atmpS3500;
    _M0L8_2atupleS3499->$1_0 = _M0L1kS1363.$0;
    _M0L8_2atupleS3499->$1_1 = _M0L1kS1363.$1;
    _M0L8_2atupleS3499->$1_2 = _M0L1kS1363.$2;
    _M0L8_2aparamS1355 = _M0L8_2atupleS3499;
    continue;
    joinlet_5004:;
    goto joinlet_5003;
    join_1358:;
    _M0L6_2atmpS3498 = (int64_t)_M0L4portS1359;
    _M0L9authorityS1349->$2 = _M0L6_2atmpS3498;
    moonbit_decref(_M0L9authorityS1349);
    _result_5007.tag = 1;
    _result_5007.data.ok = _M0L1sS1360;
    return _result_5007;
    joinlet_5003:;
    goto joinlet_5002;
    join_1356:;
    _M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort));
    Moonbit_object_header(_M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort, $0_0) >> 2, 1, 7);
    ((struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort*)_M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497)->$0_0
    = _M0L1sS1357.$0;
    ((struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort*)_M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497)->$0_1
    = _M0L1sS1357.$1;
    ((struct _M0DTPC15error5Error63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPort*)_M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497)->$0_2
    = _M0L1sS1357.$2;
    _result_5008.tag = 0;
    _result_5008.data.err
    = _M0L63clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidPortS3497;
    return _result_5008;
    joinlet_5002:;
    break;
  }
  _result_5009.tag = 1;
  _result_5009.data.ok = _M0L6_2atmpS3496;
  return _result_5009;
}

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam8internal3uri20parse__path__abempty(
  struct _M0TPC16string10StringView _M0L6sourceS1322,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1330
) {
  struct _M0TPC13ref3RefGRPC16string10StringViewE* _M0L4pathS1321;
  struct _M0TPC16string10StringView _M0L8_2aparamS1323;
  #line 209 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  moonbit_incref(_M0L6sourceS1322.$0);
  _M0L4pathS1321
  = (struct _M0TPC13ref3RefGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGRPC16string10StringViewE));
  Moonbit_object_header(_M0L4pathS1321)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGRPC16string10StringViewE, $0_0) >> 2, 1, 0);
  _M0L4pathS1321->$0_0 = _M0L6sourceS1322.$0;
  _M0L4pathS1321->$0_1 = _M0L6sourceS1322.$1;
  _M0L4pathS1321->$0_2 = _M0L6sourceS1322.$2;
  _M0L8_2aparamS1323 = _M0L6sourceS1322;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1325;
    struct _M0TPC16string10StringView _M0L1sS1328;
    struct _M0TPC16string10StringView _M0L1sS1332;
    struct _M0TPC16string10StringView _M0L1kS1333;
    struct _M0TPC16string10StringView _M0L1sS1335;
    struct _M0TPC16string10StringView _M0L1kS1336;
    struct _M0TPC16string10StringView _M0L1sS1338;
    struct _M0TPC16string10StringView _M0L1kS1339;
    moonbit_string_t _M0L8_2afieldS4371 = _M0L8_2aparamS1323.$0;
    moonbit_string_t _M0L3strS3451 = _M0L8_2afieldS4371;
    int32_t _M0L5startS3452 = _M0L8_2aparamS1323.$1;
    int32_t _M0L3endS3454 = _M0L8_2aparamS1323.$2;
    int64_t _M0L6_2atmpS3453 = (int64_t)_M0L3endS3454;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4359;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4pathS3448;
    struct _M0TPC16string10StringView _M0L8_2afieldS4358;
    struct _M0TPC16string10StringView _M0L3valS3450;
    struct _M0TPC16string10StringView _M0L6_2atmpS3449;
    struct _M0TPC16string10StringView _M0L6_2aoldS4357;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4356;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4pathS3445;
    struct _M0TPC16string10StringView _M0L8_2afieldS4355;
    int32_t _M0L6_2acntS4766;
    struct _M0TPC16string10StringView _M0L3valS3447;
    struct _M0TPC16string10StringView _M0L6_2atmpS3446;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4354;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4pathS3441;
    struct _M0TPC16string10StringView _M0L8_2afieldS4353;
    int32_t _M0L6_2acntS4764;
    struct _M0TPC16string10StringView _M0L3valS3443;
    struct _M0TPC16string10StringView _M0L6_2atmpS3442;
    void* _M0L4SomeS3444;
    void* _M0L6_2aoldS4352;
    struct _M0TPC16string10StringView _M0L8_2afieldS4351;
    int32_t _M0L6_2acntS4756;
    struct _M0TPC16string10StringView _M0L3valS3440;
    struct _M0TPC16string10StringView _M0L4partS1329;
    int32_t _M0L6_2atmpS3438;
    moonbit_incref(_M0L3strS3451);
    #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3451, 1, _M0L5startS3452, _M0L6_2atmpS3453)
    ) {
      moonbit_string_t _M0L8_2afieldS4370 = _M0L8_2aparamS1323.$0;
      moonbit_string_t _M0L3strS3483 = _M0L8_2afieldS4370;
      moonbit_string_t _M0L8_2afieldS4369 = _M0L8_2aparamS1323.$0;
      moonbit_string_t _M0L3strS3486 = _M0L8_2afieldS4369;
      int32_t _M0L5startS3487 = _M0L8_2aparamS1323.$1;
      int32_t _M0L3endS3489 = _M0L8_2aparamS1323.$2;
      int64_t _M0L6_2atmpS3488 = (int64_t)_M0L3endS3489;
      int64_t _M0L6_2atmpS3485;
      int32_t _M0L6_2atmpS3484;
      int32_t _M0L4_2axS1340;
      moonbit_incref(_M0L3strS3486);
      moonbit_incref(_M0L3strS3483);
      #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3485
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3486, 0, _M0L5startS3487, _M0L6_2atmpS3488);
      _M0L6_2atmpS3484 = (int32_t)_M0L6_2atmpS3485;
      #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1340
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3483, _M0L6_2atmpS3484);
      if (_M0L4_2axS1340 == 47) {
        moonbit_string_t _M0L8_2afieldS4361 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3476 = _M0L8_2afieldS4361;
        moonbit_string_t _M0L8_2afieldS4360 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3479 = _M0L8_2afieldS4360;
        int32_t _M0L5startS3480 = _M0L8_2aparamS1323.$1;
        int32_t _M0L3endS3482 = _M0L8_2aparamS1323.$2;
        int64_t _M0L6_2atmpS3481 = (int64_t)_M0L3endS3482;
        int64_t _M0L7_2abindS1697;
        int32_t _M0L6_2atmpS3477;
        int32_t _M0L3endS3478;
        struct _M0TPC16string10StringView _M0L4_2axS1341;
        moonbit_incref(_M0L3strS3479);
        moonbit_incref(_M0L3strS3476);
        #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1697
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3479, 1, _M0L5startS3480, _M0L6_2atmpS3481);
        if (_M0L7_2abindS1697 == 4294967296ll) {
          _M0L6_2atmpS3477 = _M0L8_2aparamS1323.$2;
        } else {
          int64_t _M0L7_2aSomeS1342 = _M0L7_2abindS1697;
          _M0L6_2atmpS3477 = (int32_t)_M0L7_2aSomeS1342;
        }
        _M0L3endS3478 = _M0L8_2aparamS1323.$2;
        _M0L4_2axS1341
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3477, _M0L3endS3478, _M0L3strS3476
        };
        _M0L1sS1338 = _M0L8_2aparamS1323;
        _M0L1kS1339 = _M0L4_2axS1341;
        goto join_1337;
      } else if (_M0L4_2axS1340 == 63) {
        moonbit_string_t _M0L8_2afieldS4363 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3469 = _M0L8_2afieldS4363;
        moonbit_string_t _M0L8_2afieldS4362 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3472 = _M0L8_2afieldS4362;
        int32_t _M0L5startS3473 = _M0L8_2aparamS1323.$1;
        int32_t _M0L3endS3475 = _M0L8_2aparamS1323.$2;
        int64_t _M0L6_2atmpS3474 = (int64_t)_M0L3endS3475;
        int64_t _M0L7_2abindS1698;
        int32_t _M0L6_2atmpS3470;
        int32_t _M0L3endS3471;
        struct _M0TPC16string10StringView _M0L4_2axS1343;
        moonbit_incref(_M0L3strS3472);
        moonbit_incref(_M0L3strS3469);
        #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1698
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3472, 1, _M0L5startS3473, _M0L6_2atmpS3474);
        if (_M0L7_2abindS1698 == 4294967296ll) {
          _M0L6_2atmpS3470 = _M0L8_2aparamS1323.$2;
        } else {
          int64_t _M0L7_2aSomeS1344 = _M0L7_2abindS1698;
          _M0L6_2atmpS3470 = (int32_t)_M0L7_2aSomeS1344;
        }
        _M0L3endS3471 = _M0L8_2aparamS1323.$2;
        _M0L4_2axS1343
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3470, _M0L3endS3471, _M0L3strS3469
        };
        _M0L1sS1335 = _M0L8_2aparamS1323;
        _M0L1kS1336 = _M0L4_2axS1343;
        goto join_1334;
      } else if (_M0L4_2axS1340 == 35) {
        moonbit_string_t _M0L8_2afieldS4365 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3462 = _M0L8_2afieldS4365;
        moonbit_string_t _M0L8_2afieldS4364 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3465 = _M0L8_2afieldS4364;
        int32_t _M0L5startS3466 = _M0L8_2aparamS1323.$1;
        int32_t _M0L3endS3468 = _M0L8_2aparamS1323.$2;
        int64_t _M0L6_2atmpS3467 = (int64_t)_M0L3endS3468;
        int64_t _M0L7_2abindS1699;
        int32_t _M0L6_2atmpS3463;
        int32_t _M0L3endS3464;
        struct _M0TPC16string10StringView _M0L4_2axS1345;
        moonbit_incref(_M0L3strS3465);
        moonbit_incref(_M0L3strS3462);
        #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1699
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3465, 1, _M0L5startS3466, _M0L6_2atmpS3467);
        if (_M0L7_2abindS1699 == 4294967296ll) {
          _M0L6_2atmpS3463 = _M0L8_2aparamS1323.$2;
        } else {
          int64_t _M0L7_2aSomeS1346 = _M0L7_2abindS1699;
          _M0L6_2atmpS3463 = (int32_t)_M0L7_2aSomeS1346;
        }
        _M0L3endS3464 = _M0L8_2aparamS1323.$2;
        _M0L4_2axS1345
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3463, _M0L3endS3464, _M0L3strS3462
        };
        _M0L1sS1332 = _M0L8_2aparamS1323;
        _M0L1kS1333 = _M0L4_2axS1345;
        goto join_1331;
      } else {
        moonbit_string_t _M0L8_2afieldS4368 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3455 = _M0L8_2afieldS4368;
        moonbit_string_t _M0L8_2afieldS4367 = _M0L8_2aparamS1323.$0;
        moonbit_string_t _M0L3strS3458 = _M0L8_2afieldS4367;
        int32_t _M0L5startS3459 = _M0L8_2aparamS1323.$1;
        int32_t _M0L3endS3461 = _M0L8_2aparamS1323.$2;
        int64_t _M0L6_2atmpS3460 = (int64_t)_M0L3endS3461;
        int64_t _M0L7_2abindS1700;
        int32_t _M0L6_2atmpS3456;
        int32_t _M0L8_2afieldS4366;
        int32_t _M0L3endS3457;
        struct _M0TPC16string10StringView _M0L4_2axS1347;
        moonbit_incref(_M0L3strS3458);
        moonbit_incref(_M0L3strS3455);
        #line 211 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1700
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3458, 1, _M0L5startS3459, _M0L6_2atmpS3460);
        if (_M0L7_2abindS1700 == 4294967296ll) {
          _M0L6_2atmpS3456 = _M0L8_2aparamS1323.$2;
        } else {
          int64_t _M0L7_2aSomeS1348 = _M0L7_2abindS1700;
          _M0L6_2atmpS3456 = (int32_t)_M0L7_2aSomeS1348;
        }
        _M0L8_2afieldS4366 = _M0L8_2aparamS1323.$2;
        moonbit_decref(_M0L8_2aparamS1323.$0);
        _M0L3endS3457 = _M0L8_2afieldS4366;
        _M0L4_2axS1347
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3456, _M0L3endS3457, _M0L3strS3455
        };
        _M0L1sS1325 = _M0L4_2axS1347;
        goto join_1324;
      }
    } else {
      _M0L1sS1328 = _M0L8_2aparamS1323;
      goto join_1327;
    }
    join_1337:;
    _M0L8_2afieldS4359 = _M0L3uriS1330->$2;
    _M0L4pathS3448 = _M0L8_2afieldS4359;
    _M0L8_2afieldS4358
    = (struct _M0TPC16string10StringView){
      _M0L4pathS1321->$0_1, _M0L4pathS1321->$0_2, _M0L4pathS1321->$0_0
    };
    _M0L3valS3450 = _M0L8_2afieldS4358;
    moonbit_incref(_M0L3valS3450.$0);
    moonbit_incref(_M0L4pathS3448);
    #line 213 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3449
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L3valS3450, _M0L1sS1338);
    #line 213 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L4pathS3448, _M0L6_2atmpS3449);
    _M0L6_2aoldS4357
    = (struct _M0TPC16string10StringView){
      _M0L4pathS1321->$0_1, _M0L4pathS1321->$0_2, _M0L4pathS1321->$0_0
    };
    moonbit_incref(_M0L1kS1339.$0);
    moonbit_decref(_M0L6_2aoldS4357.$0);
    _M0L4pathS1321->$0_0 = _M0L1kS1339.$0;
    _M0L4pathS1321->$0_1 = _M0L1kS1339.$1;
    _M0L4pathS1321->$0_2 = _M0L1kS1339.$2;
    _M0L8_2aparamS1323 = _M0L1kS1339;
    continue;
    join_1334:;
    _M0L8_2afieldS4356 = _M0L3uriS1330->$2;
    _M0L4pathS3445 = _M0L8_2afieldS4356;
    _M0L8_2afieldS4355
    = (struct _M0TPC16string10StringView){
      _M0L4pathS1321->$0_1, _M0L4pathS1321->$0_2, _M0L4pathS1321->$0_0
    };
    moonbit_incref(_M0L4pathS3445);
    _M0L6_2acntS4766 = Moonbit_object_header(_M0L4pathS1321)->rc;
    if (_M0L6_2acntS4766 > 1) {
      int32_t _M0L11_2anew__cntS4767 = _M0L6_2acntS4766 - 1;
      Moonbit_object_header(_M0L4pathS1321)->rc = _M0L11_2anew__cntS4767;
      moonbit_incref(_M0L8_2afieldS4355.$0);
    } else if (_M0L6_2acntS4766 == 1) {
      #line 218 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      moonbit_free(_M0L4pathS1321);
    }
    _M0L3valS3447 = _M0L8_2afieldS4355;
    #line 218 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3446
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L3valS3447, _M0L1sS1335);
    #line 218 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L4pathS3445, _M0L6_2atmpS3446);
    #line 219 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    return _M0FP48clawteam8clawteam8internal3uri12parse__query(_M0L1kS1336, _M0L3uriS1330);
    join_1331:;
    _M0L8_2afieldS4354 = _M0L3uriS1330->$2;
    _M0L4pathS3441 = _M0L8_2afieldS4354;
    _M0L8_2afieldS4353
    = (struct _M0TPC16string10StringView){
      _M0L4pathS1321->$0_1, _M0L4pathS1321->$0_2, _M0L4pathS1321->$0_0
    };
    moonbit_incref(_M0L4pathS3441);
    _M0L6_2acntS4764 = Moonbit_object_header(_M0L4pathS1321)->rc;
    if (_M0L6_2acntS4764 > 1) {
      int32_t _M0L11_2anew__cntS4765 = _M0L6_2acntS4764 - 1;
      Moonbit_object_header(_M0L4pathS1321)->rc = _M0L11_2anew__cntS4765;
      moonbit_incref(_M0L8_2afieldS4353.$0);
    } else if (_M0L6_2acntS4764 == 1) {
      #line 222 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      moonbit_free(_M0L4pathS1321);
    }
    _M0L3valS3443 = _M0L8_2afieldS4353;
    #line 222 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3442
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L3valS3443, _M0L1sS1332);
    #line 222 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L4pathS3441, _M0L6_2atmpS3442);
    moonbit_incref(_M0L1kS1333.$0);
    _M0L4SomeS3444
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_M0L4SomeS3444)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3444)->$0_0
    = _M0L1kS1333.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3444)->$0_1
    = _M0L1kS1333.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3444)->$0_2
    = _M0L1kS1333.$2;
    _M0L6_2aoldS4352 = _M0L3uriS1330->$4;
    moonbit_decref(_M0L6_2aoldS4352);
    _M0L3uriS1330->$4 = _M0L4SomeS3444;
    moonbit_decref(_M0L3uriS1330);
    return _M0L1kS1333;
    join_1327:;
    _M0L8_2afieldS4351
    = (struct _M0TPC16string10StringView){
      _M0L4pathS1321->$0_1, _M0L4pathS1321->$0_2, _M0L4pathS1321->$0_0
    };
    _M0L6_2acntS4756 = Moonbit_object_header(_M0L4pathS1321)->rc;
    if (_M0L6_2acntS4756 > 1) {
      int32_t _M0L11_2anew__cntS4757 = _M0L6_2acntS4756 - 1;
      Moonbit_object_header(_M0L4pathS1321)->rc = _M0L11_2anew__cntS4757;
      moonbit_incref(_M0L8_2afieldS4351.$0);
    } else if (_M0L6_2acntS4756 == 1) {
      #line 227 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      moonbit_free(_M0L4pathS1321);
    }
    _M0L3valS3440 = _M0L8_2afieldS4351;
    moonbit_incref(_M0L1sS1328.$0);
    #line 227 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L4partS1329
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L3valS3440, _M0L1sS1328);
    moonbit_incref(_M0L4partS1329.$0);
    #line 228 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3438 = _M0MPC16string10StringView9is__empty(_M0L4partS1329);
    if (!_M0L6_2atmpS3438) {
      struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4350 =
        _M0L3uriS1330->$2;
      int32_t _M0L6_2acntS4758 = Moonbit_object_header(_M0L3uriS1330)->rc;
      struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4pathS3439;
      if (_M0L6_2acntS4758 > 1) {
        int32_t _M0L11_2anew__cntS4763 = _M0L6_2acntS4758 - 1;
        Moonbit_object_header(_M0L3uriS1330)->rc = _M0L11_2anew__cntS4763;
        moonbit_incref(_M0L8_2afieldS4350);
      } else if (_M0L6_2acntS4758 == 1) {
        void* _M0L8_2afieldS4762 = _M0L3uriS1330->$4;
        void* _M0L8_2afieldS4761;
        struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4760;
        struct _M0TPC16string10StringView _M0L8_2afieldS4759;
        moonbit_decref(_M0L8_2afieldS4762);
        _M0L8_2afieldS4761 = _M0L3uriS1330->$3;
        moonbit_decref(_M0L8_2afieldS4761);
        _M0L8_2afieldS4760 = _M0L3uriS1330->$1;
        if (_M0L8_2afieldS4760) {
          moonbit_decref(_M0L8_2afieldS4760);
        }
        _M0L8_2afieldS4759
        = (struct _M0TPC16string10StringView){
          _M0L3uriS1330->$0_1, _M0L3uriS1330->$0_2, _M0L3uriS1330->$0_0
        };
        moonbit_decref(_M0L8_2afieldS4759.$0);
        #line 229 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        moonbit_free(_M0L3uriS1330);
      }
      _M0L4pathS3439 = _M0L8_2afieldS4350;
      #line 229 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0MPC15array5Array4pushGRPC16string10StringViewE(_M0L4pathS3439, _M0L4partS1329);
    } else {
      moonbit_decref(_M0L3uriS1330);
      moonbit_decref(_M0L4partS1329.$0);
    }
    return _M0L1sS1328;
    join_1324:;
    _M0L8_2aparamS1323 = _M0L1sS1325;
    continue;
    break;
  }
}

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam8internal3uri12parse__query(
  struct _M0TPC16string10StringView _M0L6sourceS1305,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1315
) {
  struct _M0TPC16string10StringView _M0L1sS1304;
  struct _M0TPC16string10StringView _M0L8_2aparamS1306;
  #line 196 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L1sS1304 = _M0L6sourceS1305;
  moonbit_incref(_M0L1sS1304.$0);
  _M0L8_2aparamS1306 = _M0L1sS1304;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1308;
    struct _M0TPC16string10StringView _M0L1sS1311;
    struct _M0TPC16string10StringView _M0L1sS1313;
    struct _M0TPC16string10StringView _M0L1kS1314;
    moonbit_string_t _M0L8_2afieldS4380 = _M0L8_2aparamS1306.$0;
    moonbit_string_t _M0L3strS3413 = _M0L8_2afieldS4380;
    int32_t _M0L5startS3414 = _M0L8_2aparamS1306.$1;
    int32_t _M0L3endS3416 = _M0L8_2aparamS1306.$2;
    int64_t _M0L6_2atmpS3415 = (int64_t)_M0L3endS3416;
    struct _M0TPC16string10StringView _M0L6_2atmpS3412;
    void* _M0L4SomeS3411;
    void* _M0L6_2aoldS4372;
    moonbit_incref(_M0L3strS3413);
    #line 198 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3413, 1, _M0L5startS3414, _M0L6_2atmpS3415)
    ) {
      moonbit_string_t _M0L8_2afieldS4379 = _M0L8_2aparamS1306.$0;
      moonbit_string_t _M0L3strS3431 = _M0L8_2afieldS4379;
      moonbit_string_t _M0L8_2afieldS4378 = _M0L8_2aparamS1306.$0;
      moonbit_string_t _M0L3strS3434 = _M0L8_2afieldS4378;
      int32_t _M0L5startS3435 = _M0L8_2aparamS1306.$1;
      int32_t _M0L3endS3437 = _M0L8_2aparamS1306.$2;
      int64_t _M0L6_2atmpS3436 = (int64_t)_M0L3endS3437;
      int64_t _M0L6_2atmpS3433;
      int32_t _M0L6_2atmpS3432;
      int32_t _M0L4_2axS1316;
      moonbit_incref(_M0L3strS3434);
      moonbit_incref(_M0L3strS3431);
      #line 198 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3433
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3434, 0, _M0L5startS3435, _M0L6_2atmpS3436);
      _M0L6_2atmpS3432 = (int32_t)_M0L6_2atmpS3433;
      #line 198 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1316
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3431, _M0L6_2atmpS3432);
      if (_M0L4_2axS1316 == 35) {
        moonbit_string_t _M0L8_2afieldS4374 = _M0L8_2aparamS1306.$0;
        moonbit_string_t _M0L3strS3424 = _M0L8_2afieldS4374;
        moonbit_string_t _M0L8_2afieldS4373 = _M0L8_2aparamS1306.$0;
        moonbit_string_t _M0L3strS3427 = _M0L8_2afieldS4373;
        int32_t _M0L5startS3428 = _M0L8_2aparamS1306.$1;
        int32_t _M0L3endS3430 = _M0L8_2aparamS1306.$2;
        int64_t _M0L6_2atmpS3429 = (int64_t)_M0L3endS3430;
        int64_t _M0L7_2abindS1695;
        int32_t _M0L6_2atmpS3425;
        int32_t _M0L3endS3426;
        struct _M0TPC16string10StringView _M0L4_2axS1317;
        moonbit_incref(_M0L3strS3427);
        moonbit_incref(_M0L3strS3424);
        #line 198 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1695
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3427, 1, _M0L5startS3428, _M0L6_2atmpS3429);
        if (_M0L7_2abindS1695 == 4294967296ll) {
          _M0L6_2atmpS3425 = _M0L8_2aparamS1306.$2;
        } else {
          int64_t _M0L7_2aSomeS1318 = _M0L7_2abindS1695;
          _M0L6_2atmpS3425 = (int32_t)_M0L7_2aSomeS1318;
        }
        _M0L3endS3426 = _M0L8_2aparamS1306.$2;
        _M0L4_2axS1317
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3425, _M0L3endS3426, _M0L3strS3424
        };
        _M0L1sS1313 = _M0L8_2aparamS1306;
        _M0L1kS1314 = _M0L4_2axS1317;
        goto join_1312;
      } else {
        moonbit_string_t _M0L8_2afieldS4377 = _M0L8_2aparamS1306.$0;
        moonbit_string_t _M0L3strS3417 = _M0L8_2afieldS4377;
        moonbit_string_t _M0L8_2afieldS4376 = _M0L8_2aparamS1306.$0;
        moonbit_string_t _M0L3strS3420 = _M0L8_2afieldS4376;
        int32_t _M0L5startS3421 = _M0L8_2aparamS1306.$1;
        int32_t _M0L3endS3423 = _M0L8_2aparamS1306.$2;
        int64_t _M0L6_2atmpS3422 = (int64_t)_M0L3endS3423;
        int64_t _M0L7_2abindS1696;
        int32_t _M0L6_2atmpS3418;
        int32_t _M0L8_2afieldS4375;
        int32_t _M0L3endS3419;
        struct _M0TPC16string10StringView _M0L4_2axS1319;
        moonbit_incref(_M0L3strS3420);
        moonbit_incref(_M0L3strS3417);
        #line 198 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1696
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3420, 1, _M0L5startS3421, _M0L6_2atmpS3422);
        if (_M0L7_2abindS1696 == 4294967296ll) {
          _M0L6_2atmpS3418 = _M0L8_2aparamS1306.$2;
        } else {
          int64_t _M0L7_2aSomeS1320 = _M0L7_2abindS1696;
          _M0L6_2atmpS3418 = (int32_t)_M0L7_2aSomeS1320;
        }
        _M0L8_2afieldS4375 = _M0L8_2aparamS1306.$2;
        moonbit_decref(_M0L8_2aparamS1306.$0);
        _M0L3endS3419 = _M0L8_2afieldS4375;
        _M0L4_2axS1319
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3418, _M0L3endS3419, _M0L3strS3417
        };
        _M0L1sS1308 = _M0L4_2axS1319;
        goto join_1307;
      }
    } else {
      moonbit_decref(_M0L3uriS1315);
      moonbit_decref(_M0L6sourceS1305.$0);
      _M0L1sS1311 = _M0L8_2aparamS1306;
      goto join_1310;
    }
    join_1312:;
    #line 200 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3412
    = _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(_M0L6sourceS1305, _M0L1sS1313);
    _M0L4SomeS3411
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_M0L4SomeS3411)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3411)->$0_0
    = _M0L6_2atmpS3412.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3411)->$0_1
    = _M0L6_2atmpS3412.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3411)->$0_2
    = _M0L6_2atmpS3412.$2;
    _M0L6_2aoldS4372 = _M0L3uriS1315->$3;
    moonbit_decref(_M0L6_2aoldS4372);
    _M0L3uriS1315->$3 = _M0L4SomeS3411;
    #line 201 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    return _M0FP48clawteam8clawteam8internal3uri15parse__fragment(_M0L1kS1314, _M0L3uriS1315);
    join_1310:;
    return _M0L1sS1311;
    join_1307:;
    _M0L8_2aparamS1306 = _M0L1sS1308;
    continue;
    break;
  }
}

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam8internal3uri15parse__fragment(
  struct _M0TPC16string10StringView _M0L6sourceS1294,
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1301
) {
  struct _M0TPC16string10StringView _M0L1sS1293;
  struct _M0TPC16string10StringView _M0L8_2aparamS1295;
  #line 184 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L1sS1293 = _M0L6sourceS1294;
  moonbit_incref(_M0L1sS1293.$0);
  _M0L8_2aparamS1295 = _M0L1sS1293;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1297;
    struct _M0TPC16string10StringView _M0L1sS1300;
    moonbit_string_t _M0L8_2afieldS4385 = _M0L8_2aparamS1295.$0;
    moonbit_string_t _M0L3strS3400 = _M0L8_2afieldS4385;
    int32_t _M0L5startS3401 = _M0L8_2aparamS1295.$1;
    int32_t _M0L3endS3403 = _M0L8_2aparamS1295.$2;
    int64_t _M0L6_2atmpS3402 = (int64_t)_M0L3endS3403;
    void* _M0L4SomeS3399;
    void* _M0L6_2aoldS4381;
    moonbit_incref(_M0L3strS3400);
    #line 186 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS3400, 0, _M0L5startS3401, _M0L6_2atmpS3402)
    ) {
      _M0L1sS1300 = _M0L8_2aparamS1295;
      goto join_1299;
    } else {
      moonbit_string_t _M0L8_2afieldS4384 = _M0L8_2aparamS1295.$0;
      moonbit_string_t _M0L3strS3404 = _M0L8_2afieldS4384;
      moonbit_string_t _M0L8_2afieldS4383 = _M0L8_2aparamS1295.$0;
      moonbit_string_t _M0L3strS3407 = _M0L8_2afieldS4383;
      int32_t _M0L5startS3408 = _M0L8_2aparamS1295.$1;
      int32_t _M0L3endS3410 = _M0L8_2aparamS1295.$2;
      int64_t _M0L6_2atmpS3409 = (int64_t)_M0L3endS3410;
      int64_t _M0L7_2abindS1694;
      int32_t _M0L6_2atmpS3405;
      int32_t _M0L8_2afieldS4382;
      int32_t _M0L3endS3406;
      struct _M0TPC16string10StringView _M0L4_2axS1302;
      moonbit_incref(_M0L3strS3407);
      moonbit_incref(_M0L3strS3404);
      #line 186 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1694
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3407, 1, _M0L5startS3408, _M0L6_2atmpS3409);
      if (_M0L7_2abindS1694 == 4294967296ll) {
        _M0L6_2atmpS3405 = _M0L8_2aparamS1295.$2;
      } else {
        int64_t _M0L7_2aSomeS1303 = _M0L7_2abindS1694;
        _M0L6_2atmpS3405 = (int32_t)_M0L7_2aSomeS1303;
      }
      _M0L8_2afieldS4382 = _M0L8_2aparamS1295.$2;
      moonbit_decref(_M0L8_2aparamS1295.$0);
      _M0L3endS3406 = _M0L8_2afieldS4382;
      _M0L4_2axS1302
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3405, _M0L3endS3406, _M0L3strS3404
      };
      _M0L1sS1297 = _M0L4_2axS1302;
      goto join_1296;
    }
    join_1299:;
    _M0L4SomeS3399
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_M0L4SomeS3399)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3399)->$0_0
    = _M0L6sourceS1294.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3399)->$0_1
    = _M0L6sourceS1294.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3399)->$0_2
    = _M0L6sourceS1294.$2;
    _M0L6_2aoldS4381 = _M0L3uriS1301->$4;
    moonbit_decref(_M0L6_2aoldS4381);
    _M0L3uriS1301->$4 = _M0L4SomeS3399;
    moonbit_decref(_M0L3uriS1301);
    return _M0L1sS1300;
    join_1296:;
    _M0L8_2aparamS1295 = _M0L1sS1297;
    continue;
    break;
  }
}

struct _M0TPC16string10StringView _M0EPC16string10StringViewP48clawteam8clawteam8internal3uri12slice__until(
  struct _M0TPC16string10StringView _M0L4selfS1291,
  struct _M0TPC16string10StringView _M0L5otherS1292
) {
  int32_t _M0L6_2atmpS3397;
  int32_t _M0L6_2atmpS3398;
  int32_t _M0L6_2atmpS3396;
  int64_t _M0L6_2atmpS3395;
  #line 75 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  #line 76 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L6_2atmpS3397
  = _M0MPC16string10StringView13start__offset(_M0L5otherS1292);
  moonbit_incref(_M0L4selfS1291.$0);
  #line 76 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L6_2atmpS3398
  = _M0MPC16string10StringView13start__offset(_M0L4selfS1291);
  _M0L6_2atmpS3396 = _M0L6_2atmpS3397 - _M0L6_2atmpS3398;
  _M0L6_2atmpS3395 = (int64_t)_M0L6_2atmpS3396;
  #line 76 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  return _M0MPC16string10StringView12view_2einner(_M0L4selfS1291, 0, _M0L6_2atmpS3395);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal3uri27____test__7572692e6d6274__0(
  
) {
  moonbit_string_t _M0L7_2abindS1289;
  int32_t _M0L6_2atmpS3394;
  struct _M0TPC16string10StringView _M0L6_2atmpS3388;
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L6_2atmpS3389;
  struct _M0TPC16string10StringView* _M0L6_2atmpS3393;
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L6_2atmpS3390;
  void* _M0L4NoneS3391;
  void* _M0L4NoneS3392;
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1288;
  moonbit_string_t _M0L6sourceS1290;
  int32_t _M0L6_2atmpS3372;
  struct _M0TPC16string10StringView _M0L6_2atmpS3371;
  struct moonbit_result_1 _tmp_5024;
  struct _M0TPC16string10StringView _M0L6_2atmpS3369;
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS3370;
  struct _M0TPB6ToJson _M0L6_2atmpS3360;
  void* _M0L6_2atmpS3368;
  void* _M0L6_2atmpS3361;
  moonbit_string_t _M0L6_2atmpS3364;
  moonbit_string_t _M0L6_2atmpS3365;
  moonbit_string_t _M0L6_2atmpS3366;
  moonbit_string_t _M0L6_2atmpS3367;
  moonbit_string_t* _M0L6_2atmpS3363;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3362;
  struct moonbit_result_0 _tmp_5023;
  struct _M0TPC16string10StringView _M0L8_2afieldS4386;
  int32_t _M0L6_2acntS4768;
  struct _M0TPC16string10StringView _M0L6schemeS3386;
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS3387;
  struct _M0TPB6ToJson _M0L6_2atmpS3377;
  void* _M0L6_2atmpS3385;
  void* _M0L6_2atmpS3378;
  moonbit_string_t _M0L6_2atmpS3381;
  moonbit_string_t _M0L6_2atmpS3382;
  moonbit_string_t _M0L6_2atmpS3383;
  moonbit_string_t _M0L6_2atmpS3384;
  moonbit_string_t* _M0L6_2atmpS3380;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3379;
  #line 58 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L7_2abindS1289 = (moonbit_string_t)moonbit_string_literal_0.data;
  _M0L6_2atmpS3394 = Moonbit_array_length(_M0L7_2abindS1289);
  _M0L6_2atmpS3388
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3394, _M0L7_2abindS1289
  };
  _M0L6_2atmpS3389 = 0;
  _M0L6_2atmpS3393
  = (struct _M0TPC16string10StringView*)moonbit_empty_ref_valtype_array;
  _M0L6_2atmpS3390
  = (struct _M0TPB5ArrayGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC16string10StringViewE));
  Moonbit_object_header(_M0L6_2atmpS3390)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC16string10StringViewE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3390->$0 = _M0L6_2atmpS3393;
  _M0L6_2atmpS3390->$1 = 0;
  _M0L4NoneS3391
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L4NoneS3392
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L3uriS1288
  = (struct _M0TP48clawteam8clawteam8internal3uri3Uri*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3uri3Uri));
  Moonbit_object_header(_M0L3uriS1288)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal3uri3Uri, $0_0) >> 2, 5, 0);
  _M0L3uriS1288->$0_0 = _M0L6_2atmpS3388.$0;
  _M0L3uriS1288->$0_1 = _M0L6_2atmpS3388.$1;
  _M0L3uriS1288->$0_2 = _M0L6_2atmpS3388.$2;
  _M0L3uriS1288->$1 = _M0L6_2atmpS3389;
  _M0L3uriS1288->$2 = _M0L6_2atmpS3390;
  _M0L3uriS1288->$3 = _M0L4NoneS3391;
  _M0L3uriS1288->$4 = _M0L4NoneS3392;
  _M0L6sourceS1290 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3372 = Moonbit_array_length(_M0L6sourceS1290);
  _M0L6_2atmpS3371
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3372, _M0L6sourceS1290
  };
  moonbit_incref(_M0L3uriS1288);
  #line 68 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_5024
  = _M0MP48clawteam8clawteam8internal3uri3Uri13parse__scheme(_M0L3uriS1288, _M0L6_2atmpS3371);
  if (_tmp_5024.tag) {
    struct _M0TPC16string10StringView const _M0L5_2aokS3373 =
      _tmp_5024.data.ok;
    _M0L6_2atmpS3369 = _M0L5_2aokS3373;
  } else {
    void* const _M0L6_2aerrS3374 = _tmp_5024.data.err;
    struct moonbit_result_0 _result_5025;
    moonbit_decref(_M0L3uriS1288);
    _result_5025.tag = 0;
    _result_5025.data.err = _M0L6_2aerrS3374;
    return _result_5025;
  }
  _M0L14_2aboxed__selfS3370
  = (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)moonbit_malloc(sizeof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView));
  Moonbit_object_header(_M0L14_2aboxed__selfS3370)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView, $0_0) >> 2, 1, 0);
  _M0L14_2aboxed__selfS3370->$0_0 = _M0L6_2atmpS3369.$0;
  _M0L14_2aboxed__selfS3370->$0_1 = _M0L6_2atmpS3369.$1;
  _M0L14_2aboxed__selfS3370->$0_2 = _M0L6_2atmpS3369.$2;
  _M0L6_2atmpS3360
  = (struct _M0TPB6ToJson){
    _M0FP0115moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3370
  };
  #line 69 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L6_2atmpS3368
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_41.data);
  _M0L6_2atmpS3361 = _M0L6_2atmpS3368;
  _M0L6_2atmpS3364 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3365 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3366 = 0;
  _M0L6_2atmpS3367 = 0;
  _M0L6_2atmpS3363 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3363[0] = _M0L6_2atmpS3364;
  _M0L6_2atmpS3363[1] = _M0L6_2atmpS3365;
  _M0L6_2atmpS3363[2] = _M0L6_2atmpS3366;
  _M0L6_2atmpS3363[3] = _M0L6_2atmpS3367;
  _M0L6_2atmpS3362
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3362)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3362->$0 = _M0L6_2atmpS3363;
  _M0L6_2atmpS3362->$1 = 4;
  #line 67 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _tmp_5023
  = _M0FPC14json13json__inspect(_M0L6_2atmpS3360, _M0L6_2atmpS3361, (moonbit_string_t)moonbit_string_literal_44.data, _M0L6_2atmpS3362);
  if (_tmp_5023.tag) {
    int32_t const _M0L5_2aokS3375 = _tmp_5023.data.ok;
  } else {
    void* const _M0L6_2aerrS3376 = _tmp_5023.data.err;
    struct moonbit_result_0 _result_5026;
    moonbit_decref(_M0L3uriS1288);
    _result_5026.tag = 0;
    _result_5026.data.err = _M0L6_2aerrS3376;
    return _result_5026;
  }
  _M0L8_2afieldS4386
  = (struct _M0TPC16string10StringView){
    _M0L3uriS1288->$0_1, _M0L3uriS1288->$0_2, _M0L3uriS1288->$0_0
  };
  _M0L6_2acntS4768 = Moonbit_object_header(_M0L3uriS1288)->rc;
  if (_M0L6_2acntS4768 > 1) {
    int32_t _M0L11_2anew__cntS4773 = _M0L6_2acntS4768 - 1;
    Moonbit_object_header(_M0L3uriS1288)->rc = _M0L11_2anew__cntS4773;
    moonbit_incref(_M0L8_2afieldS4386.$0);
  } else if (_M0L6_2acntS4768 == 1) {
    void* _M0L8_2afieldS4772 = _M0L3uriS1288->$4;
    void* _M0L8_2afieldS4771;
    struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L8_2afieldS4770;
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L8_2afieldS4769;
    moonbit_decref(_M0L8_2afieldS4772);
    _M0L8_2afieldS4771 = _M0L3uriS1288->$3;
    moonbit_decref(_M0L8_2afieldS4771);
    _M0L8_2afieldS4770 = _M0L3uriS1288->$2;
    moonbit_decref(_M0L8_2afieldS4770);
    _M0L8_2afieldS4769 = _M0L3uriS1288->$1;
    if (_M0L8_2afieldS4769) {
      moonbit_decref(_M0L8_2afieldS4769);
    }
    #line 71 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    moonbit_free(_M0L3uriS1288);
  }
  _M0L6schemeS3386 = _M0L8_2afieldS4386;
  _M0L14_2aboxed__selfS3387
  = (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)moonbit_malloc(sizeof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView));
  Moonbit_object_header(_M0L14_2aboxed__selfS3387)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView, $0_0) >> 2, 1, 0);
  _M0L14_2aboxed__selfS3387->$0_0 = _M0L6schemeS3386.$0;
  _M0L14_2aboxed__selfS3387->$0_1 = _M0L6schemeS3386.$1;
  _M0L14_2aboxed__selfS3387->$0_2 = _M0L6schemeS3386.$2;
  _M0L6_2atmpS3377
  = (struct _M0TPB6ToJson){
    _M0FP0115moonbitlang_2fcore_2fstring_2fStringView_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3387
  };
  #line 71 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L6_2atmpS3385
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L6_2atmpS3378 = _M0L6_2atmpS3385;
  _M0L6_2atmpS3381 = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3382 = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3383 = 0;
  _M0L6_2atmpS3384 = 0;
  _M0L6_2atmpS3380 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3380[0] = _M0L6_2atmpS3381;
  _M0L6_2atmpS3380[1] = _M0L6_2atmpS3382;
  _M0L6_2atmpS3380[2] = _M0L6_2atmpS3383;
  _M0L6_2atmpS3380[3] = _M0L6_2atmpS3384;
  _M0L6_2atmpS3379
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3379)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3379->$0 = _M0L6_2atmpS3380;
  _M0L6_2atmpS3379->$1 = 4;
  #line 71 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3377, _M0L6_2atmpS3378, (moonbit_string_t)moonbit_string_literal_47.data, _M0L6_2atmpS3379);
}

struct moonbit_result_1 _M0MP48clawteam8clawteam8internal3uri3Uri13parse__scheme(
  struct _M0TP48clawteam8clawteam8internal3uri3Uri* _M0L3uriS1263,
  struct _M0TPC16string10StringView _M0L6sourceS1264
) {
  struct _M0TPC16string10StringView _M0L1sS1254;
  struct _M0TPC16string10StringView _M0L1sS1256;
  moonbit_string_t _M0L8_2afieldS4417;
  moonbit_string_t _M0L3strS3334;
  int32_t _M0L5startS3335;
  int32_t _M0L3endS3337;
  int64_t _M0L6_2atmpS3336;
  struct _M0TPC16string10StringView _M0L6_2atmpS3266;
  struct _M0TPC16string10StringView _M0L8_2aparamS1257;
  struct moonbit_result_1 _result_5037;
  void* _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265;
  struct moonbit_result_1 _result_5038;
  #line 31 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  _M0L8_2afieldS4417 = _M0L6sourceS1264.$0;
  _M0L3strS3334 = _M0L8_2afieldS4417;
  _M0L5startS3335 = _M0L6sourceS1264.$1;
  _M0L3endS3337 = _M0L6sourceS1264.$2;
  _M0L6_2atmpS3336 = (int64_t)_M0L3endS3337;
  moonbit_incref(_M0L3strS3334);
  #line 35 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
  if (
    _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3334, 1, _M0L5startS3335, _M0L6_2atmpS3336)
  ) {
    moonbit_string_t _M0L8_2afieldS4416 = _M0L6sourceS1264.$0;
    moonbit_string_t _M0L3strS3352 = _M0L8_2afieldS4416;
    moonbit_string_t _M0L8_2afieldS4415 = _M0L6sourceS1264.$0;
    moonbit_string_t _M0L3strS3355 = _M0L8_2afieldS4415;
    int32_t _M0L5startS3356 = _M0L6sourceS1264.$1;
    int32_t _M0L3endS3358 = _M0L6sourceS1264.$2;
    int64_t _M0L6_2atmpS3357 = (int64_t)_M0L3endS3358;
    int64_t _M0L6_2atmpS3354;
    int32_t _M0L6_2atmpS3353;
    int32_t _M0L4_2axS1283;
    moonbit_incref(_M0L3strS3355);
    moonbit_incref(_M0L3strS3352);
    #line 35 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3354
    = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3355, 0, _M0L5startS3356, _M0L6_2atmpS3357);
    _M0L6_2atmpS3353 = (int32_t)_M0L6_2atmpS3354;
    #line 35 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L4_2axS1283
    = _M0MPC16string6String16unsafe__char__at(_M0L3strS3352, _M0L6_2atmpS3353);
    if (_M0L4_2axS1283 >= 97 && _M0L4_2axS1283 <= 122) {
      moonbit_string_t _M0L8_2afieldS4412 = _M0L6sourceS1264.$0;
      moonbit_string_t _M0L3strS3345 = _M0L8_2afieldS4412;
      moonbit_string_t _M0L8_2afieldS4411 = _M0L6sourceS1264.$0;
      moonbit_string_t _M0L3strS3348 = _M0L8_2afieldS4411;
      int32_t _M0L5startS3349 = _M0L6sourceS1264.$1;
      int32_t _M0L3endS3351 = _M0L6sourceS1264.$2;
      int64_t _M0L6_2atmpS3350 = (int64_t)_M0L3endS3351;
      int64_t _M0L7_2abindS1692;
      int32_t _M0L6_2atmpS3346;
      int32_t _M0L3endS3347;
      struct _M0TPC16string10StringView _M0L4_2axS1284;
      moonbit_incref(_M0L3strS3348);
      moonbit_incref(_M0L3strS3345);
      #line 35 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1692
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3348, 1, _M0L5startS3349, _M0L6_2atmpS3350);
      if (_M0L7_2abindS1692 == 4294967296ll) {
        _M0L6_2atmpS3346 = _M0L6sourceS1264.$2;
      } else {
        int64_t _M0L7_2aSomeS1285 = _M0L7_2abindS1692;
        _M0L6_2atmpS3346 = (int32_t)_M0L7_2aSomeS1285;
      }
      _M0L3endS3347 = _M0L6sourceS1264.$2;
      _M0L4_2axS1284
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3346, _M0L3endS3347, _M0L3strS3345
      };
      _M0L1sS1256 = _M0L4_2axS1284;
      goto join_1255;
    } else if (_M0L4_2axS1283 >= 65 && _M0L4_2axS1283 <= 90) {
      moonbit_string_t _M0L8_2afieldS4414 = _M0L6sourceS1264.$0;
      moonbit_string_t _M0L3strS3338 = _M0L8_2afieldS4414;
      moonbit_string_t _M0L8_2afieldS4413 = _M0L6sourceS1264.$0;
      moonbit_string_t _M0L3strS3341 = _M0L8_2afieldS4413;
      int32_t _M0L5startS3342 = _M0L6sourceS1264.$1;
      int32_t _M0L3endS3344 = _M0L6sourceS1264.$2;
      int64_t _M0L6_2atmpS3343 = (int64_t)_M0L3endS3344;
      int64_t _M0L7_2abindS1693;
      int32_t _M0L6_2atmpS3339;
      int32_t _M0L3endS3340;
      struct _M0TPC16string10StringView _M0L4_2axS1286;
      moonbit_incref(_M0L3strS3341);
      moonbit_incref(_M0L3strS3338);
      #line 35 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L7_2abindS1693
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3341, 1, _M0L5startS3342, _M0L6_2atmpS3343);
      if (_M0L7_2abindS1693 == 4294967296ll) {
        _M0L6_2atmpS3339 = _M0L6sourceS1264.$2;
      } else {
        int64_t _M0L7_2aSomeS1287 = _M0L7_2abindS1693;
        _M0L6_2atmpS3339 = (int32_t)_M0L7_2aSomeS1287;
      }
      _M0L3endS3340 = _M0L6sourceS1264.$2;
      _M0L4_2axS1286
      = (struct _M0TPC16string10StringView){
        _M0L6_2atmpS3339, _M0L3endS3340, _M0L3strS3338
      };
      _M0L1sS1256 = _M0L4_2axS1286;
      goto join_1255;
    } else {
      moonbit_decref(_M0L3uriS1263);
      _M0L1sS1254 = _M0L6sourceS1264;
      goto join_1253;
    }
  } else {
    void* _M0L55clawteam_2fclawteam_2finternal_2furi_2eParseError_2eEofS3359;
    struct moonbit_result_1 _result_5029;
    moonbit_decref(_M0L6sourceS1264.$0);
    moonbit_decref(_M0L3uriS1263);
    _M0L55clawteam_2fclawteam_2finternal_2furi_2eParseError_2eEofS3359
    = (struct moonbit_object*)&moonbit_constant_constructor_2 + 1;
    _result_5029.tag = 0;
    _result_5029.data.err
    = _M0L55clawteam_2fclawteam_2finternal_2furi_2eParseError_2eEofS3359;
    return _result_5029;
  }
  join_1255:;
  _M0L8_2aparamS1257 = _M0L1sS1256;
  while (1) {
    struct _M0TPC16string10StringView _M0L1sS1259;
    struct _M0TPC16string10StringView _M0L4viewS1261;
    struct _M0TPC16string10StringView _M0L4restS1262;
    struct _M0TPC16string10StringView _M0L1sS1266;
    moonbit_string_t _M0L8_2afieldS4410 = _M0L8_2aparamS1257.$0;
    moonbit_string_t _M0L3strS3273 = _M0L8_2afieldS4410;
    int32_t _M0L5startS3274 = _M0L8_2aparamS1257.$1;
    int32_t _M0L3endS3276 = _M0L8_2aparamS1257.$2;
    int64_t _M0L6_2atmpS3275 = (int64_t)_M0L3endS3276;
    moonbit_string_t _M0L6_2atmpS3269;
    int32_t _M0L6_2atmpS3270;
    int32_t _M0L6_2atmpS3272;
    int64_t _M0L6_2atmpS3271;
    struct _M0TPC16string10StringView _M0L6_2atmpS3268;
    struct _M0TPC16string10StringView _M0L6_2aoldS4387;
    struct moonbit_result_1 _result_5035;
    void* _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267;
    struct moonbit_result_1 _result_5036;
    moonbit_incref(_M0L3strS3273);
    #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3273, 1, _M0L5startS3274, _M0L6_2atmpS3275)
    ) {
      moonbit_string_t _M0L8_2afieldS4409 = _M0L8_2aparamS1257.$0;
      moonbit_string_t _M0L3strS3326 = _M0L8_2afieldS4409;
      moonbit_string_t _M0L8_2afieldS4408 = _M0L8_2aparamS1257.$0;
      moonbit_string_t _M0L3strS3329 = _M0L8_2afieldS4408;
      int32_t _M0L5startS3330 = _M0L8_2aparamS1257.$1;
      int32_t _M0L3endS3332 = _M0L8_2aparamS1257.$2;
      int64_t _M0L6_2atmpS3331 = (int64_t)_M0L3endS3332;
      int64_t _M0L6_2atmpS3328;
      int32_t _M0L6_2atmpS3327;
      int32_t _M0L4_2axS1268;
      moonbit_incref(_M0L3strS3329);
      moonbit_incref(_M0L3strS3326);
      #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L6_2atmpS3328
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3329, 0, _M0L5startS3330, _M0L6_2atmpS3331);
      _M0L6_2atmpS3327 = (int32_t)_M0L6_2atmpS3328;
      #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
      _M0L4_2axS1268
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3326, _M0L6_2atmpS3327);
      if (_M0L4_2axS1268 >= 97 && _M0L4_2axS1268 <= 122) {
        moonbit_string_t _M0L8_2afieldS4390 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3319 = _M0L8_2afieldS4390;
        moonbit_string_t _M0L8_2afieldS4389 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3322 = _M0L8_2afieldS4389;
        int32_t _M0L5startS3323 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3325 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3324 = (int64_t)_M0L3endS3325;
        int64_t _M0L7_2abindS1685;
        int32_t _M0L6_2atmpS3320;
        int32_t _M0L8_2afieldS4388;
        int32_t _M0L3endS3321;
        struct _M0TPC16string10StringView _M0L4_2axS1269;
        moonbit_incref(_M0L3strS3322);
        moonbit_incref(_M0L3strS3319);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1685
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3322, 1, _M0L5startS3323, _M0L6_2atmpS3324);
        if (_M0L7_2abindS1685 == 4294967296ll) {
          _M0L6_2atmpS3320 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1270 = _M0L7_2abindS1685;
          _M0L6_2atmpS3320 = (int32_t)_M0L7_2aSomeS1270;
        }
        _M0L8_2afieldS4388 = _M0L8_2aparamS1257.$2;
        moonbit_decref(_M0L8_2aparamS1257.$0);
        _M0L3endS3321 = _M0L8_2afieldS4388;
        _M0L4_2axS1269
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3320, _M0L3endS3321, _M0L3strS3319
        };
        _M0L1sS1266 = _M0L4_2axS1269;
        goto join_1265;
      } else if (_M0L4_2axS1268 >= 65 && _M0L4_2axS1268 <= 90) {
        moonbit_string_t _M0L8_2afieldS4393 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3312 = _M0L8_2afieldS4393;
        moonbit_string_t _M0L8_2afieldS4392 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3315 = _M0L8_2afieldS4392;
        int32_t _M0L5startS3316 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3318 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3317 = (int64_t)_M0L3endS3318;
        int64_t _M0L7_2abindS1686;
        int32_t _M0L6_2atmpS3313;
        int32_t _M0L8_2afieldS4391;
        int32_t _M0L3endS3314;
        struct _M0TPC16string10StringView _M0L4_2axS1271;
        moonbit_incref(_M0L3strS3315);
        moonbit_incref(_M0L3strS3312);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1686
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3315, 1, _M0L5startS3316, _M0L6_2atmpS3317);
        if (_M0L7_2abindS1686 == 4294967296ll) {
          _M0L6_2atmpS3313 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1272 = _M0L7_2abindS1686;
          _M0L6_2atmpS3313 = (int32_t)_M0L7_2aSomeS1272;
        }
        _M0L8_2afieldS4391 = _M0L8_2aparamS1257.$2;
        moonbit_decref(_M0L8_2aparamS1257.$0);
        _M0L3endS3314 = _M0L8_2afieldS4391;
        _M0L4_2axS1271
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3313, _M0L3endS3314, _M0L3strS3312
        };
        _M0L1sS1266 = _M0L4_2axS1271;
        goto join_1265;
      } else if (_M0L4_2axS1268 >= 48 && _M0L4_2axS1268 <= 57) {
        moonbit_string_t _M0L8_2afieldS4396 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3305 = _M0L8_2afieldS4396;
        moonbit_string_t _M0L8_2afieldS4395 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3308 = _M0L8_2afieldS4395;
        int32_t _M0L5startS3309 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3311 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3310 = (int64_t)_M0L3endS3311;
        int64_t _M0L7_2abindS1687;
        int32_t _M0L6_2atmpS3306;
        int32_t _M0L8_2afieldS4394;
        int32_t _M0L3endS3307;
        struct _M0TPC16string10StringView _M0L4_2axS1273;
        moonbit_incref(_M0L3strS3308);
        moonbit_incref(_M0L3strS3305);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1687
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3308, 1, _M0L5startS3309, _M0L6_2atmpS3310);
        if (_M0L7_2abindS1687 == 4294967296ll) {
          _M0L6_2atmpS3306 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1274 = _M0L7_2abindS1687;
          _M0L6_2atmpS3306 = (int32_t)_M0L7_2aSomeS1274;
        }
        _M0L8_2afieldS4394 = _M0L8_2aparamS1257.$2;
        moonbit_decref(_M0L8_2aparamS1257.$0);
        _M0L3endS3307 = _M0L8_2afieldS4394;
        _M0L4_2axS1273
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3306, _M0L3endS3307, _M0L3strS3305
        };
        _M0L1sS1266 = _M0L4_2axS1273;
        goto join_1265;
      } else if (_M0L4_2axS1268 == 43) {
        moonbit_string_t _M0L8_2afieldS4399 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3298 = _M0L8_2afieldS4399;
        moonbit_string_t _M0L8_2afieldS4398 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3301 = _M0L8_2afieldS4398;
        int32_t _M0L5startS3302 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3304 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3303 = (int64_t)_M0L3endS3304;
        int64_t _M0L7_2abindS1688;
        int32_t _M0L6_2atmpS3299;
        int32_t _M0L8_2afieldS4397;
        int32_t _M0L3endS3300;
        struct _M0TPC16string10StringView _M0L4_2axS1275;
        moonbit_incref(_M0L3strS3301);
        moonbit_incref(_M0L3strS3298);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1688
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3301, 1, _M0L5startS3302, _M0L6_2atmpS3303);
        if (_M0L7_2abindS1688 == 4294967296ll) {
          _M0L6_2atmpS3299 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1276 = _M0L7_2abindS1688;
          _M0L6_2atmpS3299 = (int32_t)_M0L7_2aSomeS1276;
        }
        _M0L8_2afieldS4397 = _M0L8_2aparamS1257.$2;
        moonbit_decref(_M0L8_2aparamS1257.$0);
        _M0L3endS3300 = _M0L8_2afieldS4397;
        _M0L4_2axS1275
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3299, _M0L3endS3300, _M0L3strS3298
        };
        _M0L1sS1266 = _M0L4_2axS1275;
        goto join_1265;
      } else if (_M0L4_2axS1268 == 45) {
        moonbit_string_t _M0L8_2afieldS4402 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3291 = _M0L8_2afieldS4402;
        moonbit_string_t _M0L8_2afieldS4401 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3294 = _M0L8_2afieldS4401;
        int32_t _M0L5startS3295 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3297 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3296 = (int64_t)_M0L3endS3297;
        int64_t _M0L7_2abindS1689;
        int32_t _M0L6_2atmpS3292;
        int32_t _M0L8_2afieldS4400;
        int32_t _M0L3endS3293;
        struct _M0TPC16string10StringView _M0L4_2axS1277;
        moonbit_incref(_M0L3strS3294);
        moonbit_incref(_M0L3strS3291);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1689
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3294, 1, _M0L5startS3295, _M0L6_2atmpS3296);
        if (_M0L7_2abindS1689 == 4294967296ll) {
          _M0L6_2atmpS3292 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1278 = _M0L7_2abindS1689;
          _M0L6_2atmpS3292 = (int32_t)_M0L7_2aSomeS1278;
        }
        _M0L8_2afieldS4400 = _M0L8_2aparamS1257.$2;
        moonbit_decref(_M0L8_2aparamS1257.$0);
        _M0L3endS3293 = _M0L8_2afieldS4400;
        _M0L4_2axS1277
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3292, _M0L3endS3293, _M0L3strS3291
        };
        _M0L1sS1266 = _M0L4_2axS1277;
        goto join_1265;
      } else if (_M0L4_2axS1268 == 46) {
        moonbit_string_t _M0L8_2afieldS4405 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3284 = _M0L8_2afieldS4405;
        moonbit_string_t _M0L8_2afieldS4404 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3287 = _M0L8_2afieldS4404;
        int32_t _M0L5startS3288 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3290 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3289 = (int64_t)_M0L3endS3290;
        int64_t _M0L7_2abindS1690;
        int32_t _M0L6_2atmpS3285;
        int32_t _M0L8_2afieldS4403;
        int32_t _M0L3endS3286;
        struct _M0TPC16string10StringView _M0L4_2axS1279;
        moonbit_incref(_M0L3strS3287);
        moonbit_incref(_M0L3strS3284);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1690
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3287, 1, _M0L5startS3288, _M0L6_2atmpS3289);
        if (_M0L7_2abindS1690 == 4294967296ll) {
          _M0L6_2atmpS3285 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1280 = _M0L7_2abindS1690;
          _M0L6_2atmpS3285 = (int32_t)_M0L7_2aSomeS1280;
        }
        _M0L8_2afieldS4403 = _M0L8_2aparamS1257.$2;
        moonbit_decref(_M0L8_2aparamS1257.$0);
        _M0L3endS3286 = _M0L8_2afieldS4403;
        _M0L4_2axS1279
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3285, _M0L3endS3286, _M0L3strS3284
        };
        _M0L1sS1266 = _M0L4_2axS1279;
        goto join_1265;
      } else if (_M0L4_2axS1268 == 58) {
        moonbit_string_t _M0L8_2afieldS4407 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3277 = _M0L8_2afieldS4407;
        moonbit_string_t _M0L8_2afieldS4406 = _M0L8_2aparamS1257.$0;
        moonbit_string_t _M0L3strS3280 = _M0L8_2afieldS4406;
        int32_t _M0L5startS3281 = _M0L8_2aparamS1257.$1;
        int32_t _M0L3endS3283 = _M0L8_2aparamS1257.$2;
        int64_t _M0L6_2atmpS3282 = (int64_t)_M0L3endS3283;
        int64_t _M0L7_2abindS1691;
        int32_t _M0L6_2atmpS3278;
        int32_t _M0L3endS3279;
        struct _M0TPC16string10StringView _M0L4_2axS1281;
        moonbit_incref(_M0L3strS3280);
        moonbit_incref(_M0L3strS3277);
        #line 37 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
        _M0L7_2abindS1691
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3280, 1, _M0L5startS3281, _M0L6_2atmpS3282);
        if (_M0L7_2abindS1691 == 4294967296ll) {
          _M0L6_2atmpS3278 = _M0L8_2aparamS1257.$2;
        } else {
          int64_t _M0L7_2aSomeS1282 = _M0L7_2abindS1691;
          _M0L6_2atmpS3278 = (int32_t)_M0L7_2aSomeS1282;
        }
        _M0L3endS3279 = _M0L8_2aparamS1257.$2;
        _M0L4_2axS1281
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3278, _M0L3endS3279, _M0L3strS3277
        };
        _M0L4viewS1261 = _M0L8_2aparamS1257;
        _M0L4restS1262 = _M0L4_2axS1281;
        goto join_1260;
      } else {
        moonbit_decref(_M0L6sourceS1264.$0);
        moonbit_decref(_M0L3uriS1263);
        _M0L1sS1259 = _M0L8_2aparamS1257;
        goto join_1258;
      }
    } else {
      void* _M0L64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingColonS3333;
      struct moonbit_result_1 _result_5034;
      moonbit_decref(_M0L6sourceS1264.$0);
      moonbit_decref(_M0L3uriS1263);
      moonbit_decref(_M0L8_2aparamS1257.$0);
      _M0L64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingColonS3333
      = (struct moonbit_object*)&moonbit_constant_constructor_5 + 1;
      _result_5034.tag = 0;
      _result_5034.data.err
      = _M0L64clawteam_2fclawteam_2finternal_2furi_2eParseError_2eMissingColonS3333;
      return _result_5034;
    }
    goto joinlet_5033;
    join_1265:;
    _M0L8_2aparamS1257 = _M0L1sS1266;
    continue;
    joinlet_5033:;
    goto joinlet_5032;
    join_1260:;
    moonbit_incref(_M0L6sourceS1264.$0);
    #line 41 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3269 = _M0MPC16string10StringView4data(_M0L6sourceS1264);
    #line 44 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3270
    = _M0MPC16string10StringView13start__offset(_M0L6sourceS1264);
    #line 45 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3272
    = _M0MPC16string10StringView13start__offset(_M0L4viewS1261);
    _M0L6_2atmpS3271 = (int64_t)_M0L6_2atmpS3272;
    #line 41 "E:\\moonbit\\clawteam\\internal\\uri\\uri.mbt"
    _M0L6_2atmpS3268
    = _M0MPC16string6String12view_2einner(_M0L6_2atmpS3269, _M0L6_2atmpS3270, _M0L6_2atmpS3271);
    _M0L6_2aoldS4387
    = (struct _M0TPC16string10StringView){
      _M0L3uriS1263->$0_1, _M0L3uriS1263->$0_2, _M0L3uriS1263->$0_0
    };
    moonbit_decref(_M0L6_2aoldS4387.$0);
    _M0L3uriS1263->$0_0 = _M0L6_2atmpS3268.$0;
    _M0L3uriS1263->$0_1 = _M0L6_2atmpS3268.$1;
    _M0L3uriS1263->$0_2 = _M0L6_2atmpS3268.$2;
    moonbit_decref(_M0L3uriS1263);
    _result_5035.tag = 1;
    _result_5035.data.ok = _M0L4restS1262;
    return _result_5035;
    joinlet_5032:;
    goto joinlet_5031;
    join_1258:;
    _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme));
    Moonbit_object_header(_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme, $0_0) >> 2, 1, 8);
    ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267)->$0_0
    = _M0L1sS1259.$0;
    ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267)->$0_1
    = _M0L1sS1259.$1;
    ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267)->$0_2
    = _M0L1sS1259.$2;
    _result_5036.tag = 0;
    _result_5036.data.err
    = _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3267;
    return _result_5036;
    joinlet_5031:;
    break;
  }
  _result_5037.tag = 1;
  _result_5037.data.ok = _M0L6_2atmpS3266;
  return _result_5037;
  join_1253:;
  _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme));
  Moonbit_object_header(_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme, $0_0) >> 2, 1, 8);
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265)->$0_0
  = _M0L1sS1254.$0;
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265)->$0_1
  = _M0L1sS1254.$1;
  ((struct _M0DTPC15error5Error65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidScheme*)_M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265)->$0_2
  = _M0L1sS1254.$2;
  _result_5038.tag = 0;
  _result_5038.data.err
  = _M0L65clawteam_2fclawteam_2finternal_2furi_2eParseError_2eInvalidSchemeS3265;
  return _result_5038;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1248,
  void* _M0L7contentS1250,
  moonbit_string_t _M0L3locS1244,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1246
) {
  moonbit_string_t _M0L3locS1243;
  moonbit_string_t _M0L9args__locS1245;
  void* _M0L6_2atmpS3263;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3264;
  moonbit_string_t _M0L6actualS1247;
  moonbit_string_t _M0L4wantS1249;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1243 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1244);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1245 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1246);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3263 = _M0L3objS1248.$0->$method_0(_M0L3objS1248.$1);
  _M0L6_2atmpS3264 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1247
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3263, 0, 0, _M0L6_2atmpS3264);
  if (_M0L7contentS1250 == 0) {
    void* _M0L6_2atmpS3260;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3261;
    if (_M0L7contentS1250) {
      moonbit_decref(_M0L7contentS1250);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3260
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS3261 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1249
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3260, 0, 0, _M0L6_2atmpS3261);
  } else {
    void* _M0L7_2aSomeS1251 = _M0L7contentS1250;
    void* _M0L4_2axS1252 = _M0L7_2aSomeS1251;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3262 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1249
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1252, 0, 0, _M0L6_2atmpS3262);
  }
  moonbit_incref(_M0L4wantS1249);
  moonbit_incref(_M0L6actualS1247);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1247, _M0L4wantS1249)
  ) {
    moonbit_string_t _M0L6_2atmpS3258;
    moonbit_string_t _M0L6_2atmpS4425;
    moonbit_string_t _M0L6_2atmpS3257;
    moonbit_string_t _M0L6_2atmpS4424;
    moonbit_string_t _M0L6_2atmpS3255;
    moonbit_string_t _M0L6_2atmpS3256;
    moonbit_string_t _M0L6_2atmpS4423;
    moonbit_string_t _M0L6_2atmpS3254;
    moonbit_string_t _M0L6_2atmpS4422;
    moonbit_string_t _M0L6_2atmpS3251;
    moonbit_string_t _M0L6_2atmpS3253;
    moonbit_string_t _M0L6_2atmpS3252;
    moonbit_string_t _M0L6_2atmpS4421;
    moonbit_string_t _M0L6_2atmpS3250;
    moonbit_string_t _M0L6_2atmpS4420;
    moonbit_string_t _M0L6_2atmpS3247;
    moonbit_string_t _M0L6_2atmpS3249;
    moonbit_string_t _M0L6_2atmpS3248;
    moonbit_string_t _M0L6_2atmpS4419;
    moonbit_string_t _M0L6_2atmpS3246;
    moonbit_string_t _M0L6_2atmpS4418;
    moonbit_string_t _M0L6_2atmpS3245;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3244;
    struct moonbit_result_0 _result_5039;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3258
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1243);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4425
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_48.data, _M0L6_2atmpS3258);
    moonbit_decref(_M0L6_2atmpS3258);
    _M0L6_2atmpS3257 = _M0L6_2atmpS4425;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4424
    = moonbit_add_string(_M0L6_2atmpS3257, (moonbit_string_t)moonbit_string_literal_49.data);
    moonbit_decref(_M0L6_2atmpS3257);
    _M0L6_2atmpS3255 = _M0L6_2atmpS4424;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3256
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1245);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4423 = moonbit_add_string(_M0L6_2atmpS3255, _M0L6_2atmpS3256);
    moonbit_decref(_M0L6_2atmpS3255);
    moonbit_decref(_M0L6_2atmpS3256);
    _M0L6_2atmpS3254 = _M0L6_2atmpS4423;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4422
    = moonbit_add_string(_M0L6_2atmpS3254, (moonbit_string_t)moonbit_string_literal_50.data);
    moonbit_decref(_M0L6_2atmpS3254);
    _M0L6_2atmpS3251 = _M0L6_2atmpS4422;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3253 = _M0MPC16string6String6escape(_M0L4wantS1249);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3252
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3253);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4421 = moonbit_add_string(_M0L6_2atmpS3251, _M0L6_2atmpS3252);
    moonbit_decref(_M0L6_2atmpS3251);
    moonbit_decref(_M0L6_2atmpS3252);
    _M0L6_2atmpS3250 = _M0L6_2atmpS4421;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4420
    = moonbit_add_string(_M0L6_2atmpS3250, (moonbit_string_t)moonbit_string_literal_51.data);
    moonbit_decref(_M0L6_2atmpS3250);
    _M0L6_2atmpS3247 = _M0L6_2atmpS4420;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3249 = _M0MPC16string6String6escape(_M0L6actualS1247);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3248
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3249);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4419 = moonbit_add_string(_M0L6_2atmpS3247, _M0L6_2atmpS3248);
    moonbit_decref(_M0L6_2atmpS3247);
    moonbit_decref(_M0L6_2atmpS3248);
    _M0L6_2atmpS3246 = _M0L6_2atmpS4419;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4418
    = moonbit_add_string(_M0L6_2atmpS3246, (moonbit_string_t)moonbit_string_literal_52.data);
    moonbit_decref(_M0L6_2atmpS3246);
    _M0L6_2atmpS3245 = _M0L6_2atmpS4418;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3244
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3244)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3244)->$0
    = _M0L6_2atmpS3245;
    _result_5039.tag = 0;
    _result_5039.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3244;
    return _result_5039;
  } else {
    int32_t _M0L6_2atmpS3259;
    struct moonbit_result_0 _result_5040;
    moonbit_decref(_M0L4wantS1249);
    moonbit_decref(_M0L6actualS1247);
    moonbit_decref(_M0L9args__locS1245);
    moonbit_decref(_M0L3locS1243);
    _M0L6_2atmpS3259 = 0;
    _result_5040.tag = 1;
    _result_5040.data.ok = _M0L6_2atmpS3259;
    return _result_5040;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1242,
  int32_t _M0L13escape__slashS1214,
  int32_t _M0L6indentS1209,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1235
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1201;
  void** _M0L6_2atmpS3243;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1202;
  int32_t _M0Lm5depthS1203;
  void* _M0L6_2atmpS3242;
  void* _M0L8_2aparamS1204;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1201 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3243 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1202
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1202)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1202->$0 = _M0L6_2atmpS3243;
  _M0L5stackS1202->$1 = 0;
  _M0Lm5depthS1203 = 0;
  _M0L6_2atmpS3242 = _M0L4selfS1242;
  _M0L8_2aparamS1204 = _M0L6_2atmpS3242;
  _2aloop_1220:;
  while (1) {
    if (_M0L8_2aparamS1204 == 0) {
      int32_t _M0L3lenS3204;
      if (_M0L8_2aparamS1204) {
        moonbit_decref(_M0L8_2aparamS1204);
      }
      _M0L3lenS3204 = _M0L5stackS1202->$1;
      if (_M0L3lenS3204 == 0) {
        if (_M0L8replacerS1235) {
          moonbit_decref(_M0L8replacerS1235);
        }
        moonbit_decref(_M0L5stackS1202);
        break;
      } else {
        void** _M0L8_2afieldS4433 = _M0L5stackS1202->$0;
        void** _M0L3bufS3228 = _M0L8_2afieldS4433;
        int32_t _M0L3lenS3230 = _M0L5stackS1202->$1;
        int32_t _M0L6_2atmpS3229 = _M0L3lenS3230 - 1;
        void* _M0L6_2atmpS4432 = (void*)_M0L3bufS3228[_M0L6_2atmpS3229];
        void* _M0L4_2axS1221 = _M0L6_2atmpS4432;
        switch (Moonbit_object_tag(_M0L4_2axS1221)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1222 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1221;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS4428 =
              _M0L8_2aArrayS1222->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1223 =
              _M0L8_2afieldS4428;
            int32_t _M0L4_2aiS1224 = _M0L8_2aArrayS1222->$1;
            int32_t _M0L3lenS3216 = _M0L6_2aarrS1223->$1;
            if (_M0L4_2aiS1224 < _M0L3lenS3216) {
              int32_t _if__result_5042;
              void** _M0L8_2afieldS4427;
              void** _M0L3bufS3222;
              void* _M0L6_2atmpS4426;
              void* _M0L7elementS1225;
              int32_t _M0L6_2atmpS3217;
              void* _M0L6_2atmpS3220;
              if (_M0L4_2aiS1224 < 0) {
                _if__result_5042 = 1;
              } else {
                int32_t _M0L3lenS3221 = _M0L6_2aarrS1223->$1;
                _if__result_5042 = _M0L4_2aiS1224 >= _M0L3lenS3221;
              }
              if (_if__result_5042) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS4427 = _M0L6_2aarrS1223->$0;
              _M0L3bufS3222 = _M0L8_2afieldS4427;
              _M0L6_2atmpS4426 = (void*)_M0L3bufS3222[_M0L4_2aiS1224];
              _M0L7elementS1225 = _M0L6_2atmpS4426;
              _M0L6_2atmpS3217 = _M0L4_2aiS1224 + 1;
              _M0L8_2aArrayS1222->$1 = _M0L6_2atmpS3217;
              if (_M0L4_2aiS1224 > 0) {
                int32_t _M0L6_2atmpS3219;
                moonbit_string_t _M0L6_2atmpS3218;
                moonbit_incref(_M0L7elementS1225);
                moonbit_incref(_M0L3bufS1201);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 44);
                _M0L6_2atmpS3219 = _M0Lm5depthS1203;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3218
                = _M0FPC14json11indent__str(_M0L6_2atmpS3219, _M0L6indentS1209);
                moonbit_incref(_M0L3bufS1201);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3218);
              } else {
                moonbit_incref(_M0L7elementS1225);
              }
              _M0L6_2atmpS3220 = _M0L7elementS1225;
              _M0L8_2aparamS1204 = _M0L6_2atmpS3220;
              goto _2aloop_1220;
            } else {
              int32_t _M0L6_2atmpS3223 = _M0Lm5depthS1203;
              void* _M0L6_2atmpS3224;
              int32_t _M0L6_2atmpS3226;
              moonbit_string_t _M0L6_2atmpS3225;
              void* _M0L6_2atmpS3227;
              _M0Lm5depthS1203 = _M0L6_2atmpS3223 - 1;
              moonbit_incref(_M0L5stackS1202);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3224
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1202);
              if (_M0L6_2atmpS3224) {
                moonbit_decref(_M0L6_2atmpS3224);
              }
              _M0L6_2atmpS3226 = _M0Lm5depthS1203;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3225
              = _M0FPC14json11indent__str(_M0L6_2atmpS3226, _M0L6indentS1209);
              moonbit_incref(_M0L3bufS1201);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3225);
              moonbit_incref(_M0L3bufS1201);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 93);
              _M0L6_2atmpS3227 = 0;
              _M0L8_2aparamS1204 = _M0L6_2atmpS3227;
              goto _2aloop_1220;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1226 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1221;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS4431 =
              _M0L9_2aObjectS1226->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1227 =
              _M0L8_2afieldS4431;
            int32_t _M0L8_2afirstS1228 = _M0L9_2aObjectS1226->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1229;
            moonbit_incref(_M0L11_2aiteratorS1227);
            moonbit_incref(_M0L9_2aObjectS1226);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1229
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1227);
            if (_M0L7_2abindS1229 == 0) {
              int32_t _M0L6_2atmpS3205;
              void* _M0L6_2atmpS3206;
              int32_t _M0L6_2atmpS3208;
              moonbit_string_t _M0L6_2atmpS3207;
              void* _M0L6_2atmpS3209;
              if (_M0L7_2abindS1229) {
                moonbit_decref(_M0L7_2abindS1229);
              }
              moonbit_decref(_M0L9_2aObjectS1226);
              _M0L6_2atmpS3205 = _M0Lm5depthS1203;
              _M0Lm5depthS1203 = _M0L6_2atmpS3205 - 1;
              moonbit_incref(_M0L5stackS1202);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3206
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1202);
              if (_M0L6_2atmpS3206) {
                moonbit_decref(_M0L6_2atmpS3206);
              }
              _M0L6_2atmpS3208 = _M0Lm5depthS1203;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3207
              = _M0FPC14json11indent__str(_M0L6_2atmpS3208, _M0L6indentS1209);
              moonbit_incref(_M0L3bufS1201);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3207);
              moonbit_incref(_M0L3bufS1201);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 125);
              _M0L6_2atmpS3209 = 0;
              _M0L8_2aparamS1204 = _M0L6_2atmpS3209;
              goto _2aloop_1220;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1230 = _M0L7_2abindS1229;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1231 = _M0L7_2aSomeS1230;
              moonbit_string_t _M0L8_2afieldS4430 = _M0L4_2axS1231->$0;
              moonbit_string_t _M0L4_2akS1232 = _M0L8_2afieldS4430;
              void* _M0L8_2afieldS4429 = _M0L4_2axS1231->$1;
              int32_t _M0L6_2acntS4774 =
                Moonbit_object_header(_M0L4_2axS1231)->rc;
              void* _M0L4_2avS1233;
              void* _M0Lm2v2S1234;
              moonbit_string_t _M0L6_2atmpS3213;
              void* _M0L6_2atmpS3215;
              void* _M0L6_2atmpS3214;
              if (_M0L6_2acntS4774 > 1) {
                int32_t _M0L11_2anew__cntS4775 = _M0L6_2acntS4774 - 1;
                Moonbit_object_header(_M0L4_2axS1231)->rc
                = _M0L11_2anew__cntS4775;
                moonbit_incref(_M0L8_2afieldS4429);
                moonbit_incref(_M0L4_2akS1232);
              } else if (_M0L6_2acntS4774 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1231);
              }
              _M0L4_2avS1233 = _M0L8_2afieldS4429;
              _M0Lm2v2S1234 = _M0L4_2avS1233;
              if (_M0L8replacerS1235 == 0) {
                moonbit_incref(_M0Lm2v2S1234);
                moonbit_decref(_M0L4_2avS1233);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1236 =
                  _M0L8replacerS1235;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1237 =
                  _M0L7_2aSomeS1236;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1238 =
                  _M0L11_2areplacerS1237;
                void* _M0L7_2abindS1239;
                moonbit_incref(_M0L7_2afuncS1238);
                moonbit_incref(_M0L4_2akS1232);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1239
                = _M0L7_2afuncS1238->code(_M0L7_2afuncS1238, _M0L4_2akS1232, _M0L4_2avS1233);
                if (_M0L7_2abindS1239 == 0) {
                  void* _M0L6_2atmpS3210;
                  if (_M0L7_2abindS1239) {
                    moonbit_decref(_M0L7_2abindS1239);
                  }
                  moonbit_decref(_M0L4_2akS1232);
                  moonbit_decref(_M0L9_2aObjectS1226);
                  _M0L6_2atmpS3210 = 0;
                  _M0L8_2aparamS1204 = _M0L6_2atmpS3210;
                  goto _2aloop_1220;
                } else {
                  void* _M0L7_2aSomeS1240 = _M0L7_2abindS1239;
                  void* _M0L4_2avS1241 = _M0L7_2aSomeS1240;
                  _M0Lm2v2S1234 = _M0L4_2avS1241;
                }
              }
              if (!_M0L8_2afirstS1228) {
                int32_t _M0L6_2atmpS3212;
                moonbit_string_t _M0L6_2atmpS3211;
                moonbit_incref(_M0L3bufS1201);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 44);
                _M0L6_2atmpS3212 = _M0Lm5depthS1203;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3211
                = _M0FPC14json11indent__str(_M0L6_2atmpS3212, _M0L6indentS1209);
                moonbit_incref(_M0L3bufS1201);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3211);
              }
              moonbit_incref(_M0L3bufS1201);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3213
              = _M0FPC14json6escape(_M0L4_2akS1232, _M0L13escape__slashS1214);
              moonbit_incref(_M0L3bufS1201);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3213);
              moonbit_incref(_M0L3bufS1201);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 34);
              moonbit_incref(_M0L3bufS1201);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 58);
              if (_M0L6indentS1209 > 0) {
                moonbit_incref(_M0L3bufS1201);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 32);
              }
              _M0L9_2aObjectS1226->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1226);
              _M0L6_2atmpS3215 = _M0Lm2v2S1234;
              _M0L6_2atmpS3214 = _M0L6_2atmpS3215;
              _M0L8_2aparamS1204 = _M0L6_2atmpS3214;
              goto _2aloop_1220;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1205 = _M0L8_2aparamS1204;
      void* _M0L8_2avalueS1206 = _M0L7_2aSomeS1205;
      void* _M0L6_2atmpS3241;
      switch (Moonbit_object_tag(_M0L8_2avalueS1206)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1207 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1206;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS4434 =
            _M0L9_2aObjectS1207->$0;
          int32_t _M0L6_2acntS4776 =
            Moonbit_object_header(_M0L9_2aObjectS1207)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1208;
          if (_M0L6_2acntS4776 > 1) {
            int32_t _M0L11_2anew__cntS4777 = _M0L6_2acntS4776 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1207)->rc
            = _M0L11_2anew__cntS4777;
            moonbit_incref(_M0L8_2afieldS4434);
          } else if (_M0L6_2acntS4776 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1207);
          }
          _M0L10_2amembersS1208 = _M0L8_2afieldS4434;
          moonbit_incref(_M0L10_2amembersS1208);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1208)) {
            moonbit_decref(_M0L10_2amembersS1208);
            moonbit_incref(_M0L3bufS1201);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, (moonbit_string_t)moonbit_string_literal_53.data);
          } else {
            int32_t _M0L6_2atmpS3236 = _M0Lm5depthS1203;
            int32_t _M0L6_2atmpS3238;
            moonbit_string_t _M0L6_2atmpS3237;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3240;
            void* _M0L6ObjectS3239;
            _M0Lm5depthS1203 = _M0L6_2atmpS3236 + 1;
            moonbit_incref(_M0L3bufS1201);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 123);
            _M0L6_2atmpS3238 = _M0Lm5depthS1203;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3237
            = _M0FPC14json11indent__str(_M0L6_2atmpS3238, _M0L6indentS1209);
            moonbit_incref(_M0L3bufS1201);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3237);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3240
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1208);
            _M0L6ObjectS3239
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3239)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3239)->$0
            = _M0L6_2atmpS3240;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3239)->$1
            = 1;
            moonbit_incref(_M0L5stackS1202);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1202, _M0L6ObjectS3239);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1210 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1206;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS4435 =
            _M0L8_2aArrayS1210->$0;
          int32_t _M0L6_2acntS4778 =
            Moonbit_object_header(_M0L8_2aArrayS1210)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1211;
          if (_M0L6_2acntS4778 > 1) {
            int32_t _M0L11_2anew__cntS4779 = _M0L6_2acntS4778 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1210)->rc
            = _M0L11_2anew__cntS4779;
            moonbit_incref(_M0L8_2afieldS4435);
          } else if (_M0L6_2acntS4778 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1210);
          }
          _M0L6_2aarrS1211 = _M0L8_2afieldS4435;
          moonbit_incref(_M0L6_2aarrS1211);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1211)) {
            moonbit_decref(_M0L6_2aarrS1211);
            moonbit_incref(_M0L3bufS1201);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, (moonbit_string_t)moonbit_string_literal_54.data);
          } else {
            int32_t _M0L6_2atmpS3232 = _M0Lm5depthS1203;
            int32_t _M0L6_2atmpS3234;
            moonbit_string_t _M0L6_2atmpS3233;
            void* _M0L5ArrayS3235;
            _M0Lm5depthS1203 = _M0L6_2atmpS3232 + 1;
            moonbit_incref(_M0L3bufS1201);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 91);
            _M0L6_2atmpS3234 = _M0Lm5depthS1203;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3233
            = _M0FPC14json11indent__str(_M0L6_2atmpS3234, _M0L6indentS1209);
            moonbit_incref(_M0L3bufS1201);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3233);
            _M0L5ArrayS3235
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3235)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3235)->$0
            = _M0L6_2aarrS1211;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3235)->$1
            = 0;
            moonbit_incref(_M0L5stackS1202);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1202, _M0L5ArrayS3235);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1212 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1206;
          moonbit_string_t _M0L8_2afieldS4436 = _M0L9_2aStringS1212->$0;
          int32_t _M0L6_2acntS4780 =
            Moonbit_object_header(_M0L9_2aStringS1212)->rc;
          moonbit_string_t _M0L4_2asS1213;
          moonbit_string_t _M0L6_2atmpS3231;
          if (_M0L6_2acntS4780 > 1) {
            int32_t _M0L11_2anew__cntS4781 = _M0L6_2acntS4780 - 1;
            Moonbit_object_header(_M0L9_2aStringS1212)->rc
            = _M0L11_2anew__cntS4781;
            moonbit_incref(_M0L8_2afieldS4436);
          } else if (_M0L6_2acntS4780 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1212);
          }
          _M0L4_2asS1213 = _M0L8_2afieldS4436;
          moonbit_incref(_M0L3bufS1201);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3231
          = _M0FPC14json6escape(_M0L4_2asS1213, _M0L13escape__slashS1214);
          moonbit_incref(_M0L3bufS1201);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L6_2atmpS3231);
          moonbit_incref(_M0L3bufS1201);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1201, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1215 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1206;
          double _M0L4_2anS1216 = _M0L9_2aNumberS1215->$0;
          moonbit_string_t _M0L8_2afieldS4437 = _M0L9_2aNumberS1215->$1;
          int32_t _M0L6_2acntS4782 =
            Moonbit_object_header(_M0L9_2aNumberS1215)->rc;
          moonbit_string_t _M0L7_2areprS1217;
          if (_M0L6_2acntS4782 > 1) {
            int32_t _M0L11_2anew__cntS4783 = _M0L6_2acntS4782 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1215)->rc
            = _M0L11_2anew__cntS4783;
            if (_M0L8_2afieldS4437) {
              moonbit_incref(_M0L8_2afieldS4437);
            }
          } else if (_M0L6_2acntS4782 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1215);
          }
          _M0L7_2areprS1217 = _M0L8_2afieldS4437;
          if (_M0L7_2areprS1217 == 0) {
            if (_M0L7_2areprS1217) {
              moonbit_decref(_M0L7_2areprS1217);
            }
            moonbit_incref(_M0L3bufS1201);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1201, _M0L4_2anS1216);
          } else {
            moonbit_string_t _M0L7_2aSomeS1218 = _M0L7_2areprS1217;
            moonbit_string_t _M0L4_2arS1219 = _M0L7_2aSomeS1218;
            moonbit_incref(_M0L3bufS1201);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, _M0L4_2arS1219);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1201);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, (moonbit_string_t)moonbit_string_literal_55.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1201);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, (moonbit_string_t)moonbit_string_literal_56.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1206);
          moonbit_incref(_M0L3bufS1201);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1201, (moonbit_string_t)moonbit_string_literal_57.data);
          break;
        }
      }
      _M0L6_2atmpS3241 = 0;
      _M0L8_2aparamS1204 = _M0L6_2atmpS3241;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1201);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1200,
  int32_t _M0L6indentS1198
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1198 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1199 = _M0L6indentS1198 * _M0L5levelS1200;
    switch (_M0L6spacesS1199) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_58.data;
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
        return (moonbit_string_t)moonbit_string_literal_63.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_64.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_65.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_66.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS3203;
        moonbit_string_t _M0L6_2atmpS4438;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3203
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_67.data, _M0L6spacesS1199);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS4438
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_58.data, _M0L6_2atmpS3203);
        moonbit_decref(_M0L6_2atmpS3203);
        return _M0L6_2atmpS4438;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1190,
  int32_t _M0L13escape__slashS1195
) {
  int32_t _M0L6_2atmpS3202;
  struct _M0TPB13StringBuilder* _M0L3bufS1189;
  struct _M0TWEOc* _M0L5_2aitS1191;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3202 = Moonbit_array_length(_M0L3strS1190);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1189 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3202);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1191 = _M0MPC16string6String4iter(_M0L3strS1190);
  while (1) {
    int32_t _M0L7_2abindS1192;
    moonbit_incref(_M0L5_2aitS1191);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1192 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1191);
    if (_M0L7_2abindS1192 == -1) {
      moonbit_decref(_M0L5_2aitS1191);
    } else {
      int32_t _M0L7_2aSomeS1193 = _M0L7_2abindS1192;
      int32_t _M0L4_2acS1194 = _M0L7_2aSomeS1193;
      if (_M0L4_2acS1194 == 34) {
        moonbit_incref(_M0L3bufS1189);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_68.data);
      } else if (_M0L4_2acS1194 == 92) {
        moonbit_incref(_M0L3bufS1189);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_69.data);
      } else if (_M0L4_2acS1194 == 47) {
        if (_M0L13escape__slashS1195) {
          moonbit_incref(_M0L3bufS1189);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_70.data);
        } else {
          moonbit_incref(_M0L3bufS1189);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1189, _M0L4_2acS1194);
        }
      } else if (_M0L4_2acS1194 == 10) {
        moonbit_incref(_M0L3bufS1189);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_71.data);
      } else if (_M0L4_2acS1194 == 13) {
        moonbit_incref(_M0L3bufS1189);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_72.data);
      } else if (_M0L4_2acS1194 == 8) {
        moonbit_incref(_M0L3bufS1189);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_73.data);
      } else if (_M0L4_2acS1194 == 9) {
        moonbit_incref(_M0L3bufS1189);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_74.data);
      } else {
        int32_t _M0L4codeS1196 = _M0L4_2acS1194;
        if (_M0L4codeS1196 == 12) {
          moonbit_incref(_M0L3bufS1189);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_75.data);
        } else if (_M0L4codeS1196 < 32) {
          int32_t _M0L6_2atmpS3201;
          moonbit_string_t _M0L6_2atmpS3200;
          moonbit_incref(_M0L3bufS1189);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, (moonbit_string_t)moonbit_string_literal_76.data);
          _M0L6_2atmpS3201 = _M0L4codeS1196 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3200 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3201);
          moonbit_incref(_M0L3bufS1189);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1189, _M0L6_2atmpS3200);
        } else {
          moonbit_incref(_M0L3bufS1189);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1189, _M0L4_2acS1194);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1189);
}

int32_t _M0IPC16buffer6BufferPB6Logger11write__char(
  struct _M0TPC16buffer6Buffer* _M0L4selfS1186,
  int32_t _M0L5valueS1188
) {
  int32_t _M0L3lenS3195;
  int32_t _M0L6_2atmpS3194;
  moonbit_bytes_t _M0L8_2afieldS4439;
  moonbit_bytes_t _M0L4dataS3198;
  int32_t _M0L3lenS3199;
  int32_t _M0L3incS1187;
  int32_t _M0L3lenS3197;
  int32_t _M0L6_2atmpS3196;
  #line 1003 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L3lenS3195 = _M0L4selfS1186->$1;
  _M0L6_2atmpS3194 = _M0L3lenS3195 + 4;
  moonbit_incref(_M0L4selfS1186);
  #line 1004 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0MPC16buffer6Buffer19grow__if__necessary(_M0L4selfS1186, _M0L6_2atmpS3194);
  _M0L8_2afieldS4439 = _M0L4selfS1186->$0;
  _M0L4dataS3198 = _M0L8_2afieldS4439;
  _M0L3lenS3199 = _M0L4selfS1186->$1;
  moonbit_incref(_M0L4dataS3198);
  #line 1005 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L3incS1187
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS3198, _M0L3lenS3199, _M0L5valueS1188);
  _M0L3lenS3197 = _M0L4selfS1186->$1;
  _M0L6_2atmpS3196 = _M0L3lenS3197 + _M0L3incS1187;
  _M0L4selfS1186->$1 = _M0L6_2atmpS3196;
  moonbit_decref(_M0L4selfS1186);
  return 0;
}

struct _M0TPC16buffer6Buffer* _M0FPC16buffer11new_2einner(
  int32_t _M0L10size__hintS1184
) {
  int32_t _M0L7initialS1183;
  int32_t _M0L6_2atmpS3193;
  moonbit_bytes_t _M0L4dataS1185;
  struct _M0TPC16buffer6Buffer* _block_5044;
  #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  if (_M0L10size__hintS1184 < 1) {
    _M0L7initialS1183 = 1;
  } else {
    _M0L7initialS1183 = _M0L10size__hintS1184;
  }
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS3193 = _M0IPC14byte4BytePB7Default7default();
  _M0L4dataS1185
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS1183, _M0L6_2atmpS3193);
  _block_5044
  = (struct _M0TPC16buffer6Buffer*)moonbit_malloc(sizeof(struct _M0TPC16buffer6Buffer));
  Moonbit_object_header(_block_5044)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC16buffer6Buffer, $0) >> 2, 1, 0);
  _block_5044->$0 = _M0L4dataS1185;
  _block_5044->$1 = 0;
  return _block_5044;
}

int32_t _M0MPC16buffer6Buffer11write__byte(
  struct _M0TPC16buffer6Buffer* _M0L4selfS1181,
  int32_t _M0L5valueS1182
) {
  int32_t _M0L3lenS3188;
  int32_t _M0L6_2atmpS3187;
  moonbit_bytes_t _M0L8_2afieldS4440;
  moonbit_bytes_t _M0L4dataS3189;
  int32_t _M0L3lenS3190;
  int32_t _M0L3lenS3192;
  int32_t _M0L6_2atmpS3191;
  #line 1032 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L3lenS3188 = _M0L4selfS1181->$1;
  _M0L6_2atmpS3187 = _M0L3lenS3188 + 1;
  moonbit_incref(_M0L4selfS1181);
  #line 1033 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0MPC16buffer6Buffer19grow__if__necessary(_M0L4selfS1181, _M0L6_2atmpS3187);
  _M0L8_2afieldS4440 = _M0L4selfS1181->$0;
  _M0L4dataS3189 = _M0L8_2afieldS4440;
  _M0L3lenS3190 = _M0L4selfS1181->$1;
  if (
    _M0L3lenS3190 < 0
    || _M0L3lenS3190 >= Moonbit_array_length(_M0L4dataS3189)
  ) {
    #line 1034 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    moonbit_panic();
  }
  _M0L4dataS3189[_M0L3lenS3190] = _M0L5valueS1182;
  _M0L3lenS3192 = _M0L4selfS1181->$1;
  _M0L6_2atmpS3191 = _M0L3lenS3192 + 1;
  _M0L4selfS1181->$1 = _M0L6_2atmpS3191;
  moonbit_decref(_M0L4selfS1181);
  return 0;
}

int32_t _M0MPC16buffer6Buffer19grow__if__necessary(
  struct _M0TPC16buffer6Buffer* _M0L4selfS1175,
  int32_t _M0L8requiredS1178
) {
  moonbit_bytes_t _M0L8_2afieldS4448;
  moonbit_bytes_t _M0L4dataS3185;
  int32_t _M0L6_2atmpS4447;
  int32_t _M0L6_2atmpS3184;
  int32_t _M0L5startS1174;
  int32_t _M0L13enough__spaceS1176;
  int32_t _M0L5spaceS1177;
  moonbit_bytes_t _M0L8_2afieldS4444;
  moonbit_bytes_t _M0L4dataS3179;
  int32_t _M0L6_2atmpS4443;
  int32_t _M0L6_2atmpS3178;
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS4448 = _M0L4selfS1175->$0;
  _M0L4dataS3185 = _M0L8_2afieldS4448;
  _M0L6_2atmpS4447 = Moonbit_array_length(_M0L4dataS3185);
  _M0L6_2atmpS3184 = _M0L6_2atmpS4447;
  if (_M0L6_2atmpS3184 <= 0) {
    _M0L5startS1174 = 1;
  } else {
    moonbit_bytes_t _M0L8_2afieldS4446 = _M0L4selfS1175->$0;
    moonbit_bytes_t _M0L4dataS3186 = _M0L8_2afieldS4446;
    int32_t _M0L6_2atmpS4445 = Moonbit_array_length(_M0L4dataS3186);
    _M0L5startS1174 = _M0L6_2atmpS4445;
  }
  _M0L5spaceS1177 = _M0L5startS1174;
  while (1) {
    int32_t _M0L6_2atmpS3183;
    if (_M0L5spaceS1177 >= _M0L8requiredS1178) {
      _M0L13enough__spaceS1176 = _M0L5spaceS1177;
      break;
    }
    _M0L6_2atmpS3183 = _M0L5spaceS1177 * 2;
    _M0L5spaceS1177 = _M0L6_2atmpS3183;
    continue;
    break;
  }
  _M0L8_2afieldS4444 = _M0L4selfS1175->$0;
  _M0L4dataS3179 = _M0L8_2afieldS4444;
  _M0L6_2atmpS4443 = Moonbit_array_length(_M0L4dataS3179);
  _M0L6_2atmpS3178 = _M0L6_2atmpS4443;
  if (_M0L13enough__spaceS1176 != _M0L6_2atmpS3178) {
    int32_t _M0L6_2atmpS3182;
    moonbit_bytes_t _M0L9new__dataS1180;
    moonbit_bytes_t _M0L8_2afieldS4442;
    moonbit_bytes_t _M0L4dataS3180;
    int32_t _M0L3lenS3181;
    moonbit_bytes_t _M0L6_2aoldS4441;
    #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS3182 = _M0IPC14byte4BytePB7Default7default();
    _M0L9new__dataS1180
    = (moonbit_bytes_t)moonbit_make_bytes(_M0L13enough__spaceS1176, _M0L6_2atmpS3182);
    _M0L8_2afieldS4442 = _M0L4selfS1175->$0;
    _M0L4dataS3180 = _M0L8_2afieldS4442;
    _M0L3lenS3181 = _M0L4selfS1175->$1;
    moonbit_incref(_M0L4dataS3180);
    moonbit_incref(_M0L9new__dataS1180);
    #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS1180, 0, _M0L4dataS3180, 0, _M0L3lenS3181);
    _M0L6_2aoldS4441 = _M0L4selfS1175->$0;
    moonbit_decref(_M0L6_2aoldS4441);
    _M0L4selfS1175->$0 = _M0L9new__dataS1180;
    moonbit_decref(_M0L4selfS1175);
  } else {
    moonbit_decref(_M0L4selfS1175);
  }
  return 0;
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1173
) {
  int32_t _M0L8_2afieldS4449;
  int32_t _M0L3lenS3177;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS4449 = _M0L4selfS1173->$1;
  moonbit_decref(_M0L4selfS1173);
  _M0L3lenS3177 = _M0L8_2afieldS4449;
  return _M0L3lenS3177 == 0;
}

int32_t _M0IPC15array5ArrayPB2Eq5equalGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS1168,
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L5otherS1170
) {
  int32_t _M0L9self__lenS1167;
  int32_t _M0L10other__lenS1169;
  #line 290 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L9self__lenS1167 = _M0L4selfS1168->$1;
  _M0L10other__lenS1169 = _M0L5otherS1170->$1;
  if (_M0L9self__lenS1167 == _M0L10other__lenS1169) {
    int32_t _M0L1iS1171 = 0;
    while (1) {
      if (_M0L1iS1171 < _M0L9self__lenS1167) {
        struct _M0TPC16string10StringView* _M0L8_2afieldS4453 =
          _M0L4selfS1168->$0;
        struct _M0TPC16string10StringView* _M0L3bufS3175 = _M0L8_2afieldS4453;
        struct _M0TPC16string10StringView _M0L6_2atmpS4452 =
          _M0L3bufS3175[_M0L1iS1171];
        struct _M0TPC16string10StringView _M0L6_2atmpS3172 = _M0L6_2atmpS4452;
        struct _M0TPC16string10StringView* _M0L8_2afieldS4451 =
          _M0L5otherS1170->$0;
        struct _M0TPC16string10StringView* _M0L3bufS3174 = _M0L8_2afieldS4451;
        struct _M0TPC16string10StringView _M0L6_2atmpS4450 =
          _M0L3bufS3174[_M0L1iS1171];
        struct _M0TPC16string10StringView _M0L6_2atmpS3173 = _M0L6_2atmpS4450;
        int32_t _M0L6_2atmpS3176;
        moonbit_incref(_M0L6_2atmpS3173.$0);
        moonbit_incref(_M0L6_2atmpS3172.$0);
        #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
        if (
          _M0IPC16string10StringViewPB2Eq5equal(_M0L6_2atmpS3172, _M0L6_2atmpS3173)
        ) {
          
        } else {
          moonbit_decref(_M0L5otherS1170);
          moonbit_decref(_M0L4selfS1168);
          return 0;
        }
        _M0L6_2atmpS3176 = _M0L1iS1171 + 1;
        _M0L1iS1171 = _M0L6_2atmpS3176;
        continue;
      } else {
        moonbit_decref(_M0L5otherS1170);
        moonbit_decref(_M0L4selfS1168);
        return 1;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L5otherS1170);
    moonbit_decref(_M0L4selfS1168);
    return 0;
  }
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1164
) {
  int32_t _M0L3lenS1163;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1163 = _M0L4selfS1164->$1;
  if (_M0L3lenS1163 == 0) {
    moonbit_decref(_M0L4selfS1164);
    return 0;
  } else {
    int32_t _M0L5indexS1165 = _M0L3lenS1163 - 1;
    void** _M0L8_2afieldS4457 = _M0L4selfS1164->$0;
    void** _M0L3bufS3171 = _M0L8_2afieldS4457;
    void* _M0L6_2atmpS4456 = (void*)_M0L3bufS3171[_M0L5indexS1165];
    void* _M0L1vS1166 = _M0L6_2atmpS4456;
    void** _M0L8_2afieldS4455 = _M0L4selfS1164->$0;
    void** _M0L3bufS3170 = _M0L8_2afieldS4455;
    void* _M0L6_2aoldS4454;
    if (
      _M0L5indexS1165 < 0
      || _M0L5indexS1165 >= Moonbit_array_length(_M0L3bufS3170)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4454 = (void*)_M0L3bufS3170[_M0L5indexS1165];
    moonbit_incref(_M0L1vS1166);
    moonbit_decref(_M0L6_2aoldS4454);
    if (
      _M0L5indexS1165 < 0
      || _M0L5indexS1165 >= Moonbit_array_length(_M0L3bufS3170)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3170[_M0L5indexS1165]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1164->$1 = _M0L5indexS1165;
    moonbit_decref(_M0L4selfS1164);
    return _M0L1vS1166;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1161,
  struct _M0TPB6Logger _M0L6loggerS1162
) {
  moonbit_string_t _M0L6_2atmpS3169;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS3168;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3169 = _M0L4selfS1161;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3168 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS3169);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS3168, _M0L6loggerS1162);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1138,
  struct _M0TPB6Logger _M0L6loggerS1160
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS4466;
  struct _M0TPC16string10StringView _M0L3pkgS1137;
  moonbit_string_t _M0L7_2adataS1139;
  int32_t _M0L8_2astartS1140;
  int32_t _M0L6_2atmpS3167;
  int32_t _M0L6_2aendS1141;
  int32_t _M0Lm9_2acursorS1142;
  int32_t _M0Lm13accept__stateS1143;
  int32_t _M0Lm10match__endS1144;
  int32_t _M0Lm20match__tag__saver__0S1145;
  int32_t _M0Lm6tag__0S1146;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1147;
  struct _M0TPC16string10StringView _M0L8_2afieldS4465;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1156;
  void* _M0L8_2afieldS4464;
  int32_t _M0L6_2acntS4784;
  void* _M0L16_2apackage__nameS1157;
  struct _M0TPC16string10StringView _M0L8_2afieldS4462;
  struct _M0TPC16string10StringView _M0L8filenameS3144;
  struct _M0TPC16string10StringView _M0L8_2afieldS4461;
  struct _M0TPC16string10StringView _M0L11start__lineS3145;
  struct _M0TPC16string10StringView _M0L8_2afieldS4460;
  struct _M0TPC16string10StringView _M0L13start__columnS3146;
  struct _M0TPC16string10StringView _M0L8_2afieldS4459;
  struct _M0TPC16string10StringView _M0L9end__lineS3147;
  struct _M0TPC16string10StringView _M0L8_2afieldS4458;
  int32_t _M0L6_2acntS4788;
  struct _M0TPC16string10StringView _M0L11end__columnS3148;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS4466
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1138->$0_1, _M0L4selfS1138->$0_2, _M0L4selfS1138->$0_0
  };
  _M0L3pkgS1137 = _M0L8_2afieldS4466;
  moonbit_incref(_M0L3pkgS1137.$0);
  moonbit_incref(_M0L3pkgS1137.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1139 = _M0MPC16string10StringView4data(_M0L3pkgS1137);
  moonbit_incref(_M0L3pkgS1137.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1140
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1137);
  moonbit_incref(_M0L3pkgS1137.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3167 = _M0MPC16string10StringView6length(_M0L3pkgS1137);
  _M0L6_2aendS1141 = _M0L8_2astartS1140 + _M0L6_2atmpS3167;
  _M0Lm9_2acursorS1142 = _M0L8_2astartS1140;
  _M0Lm13accept__stateS1143 = -1;
  _M0Lm10match__endS1144 = -1;
  _M0Lm20match__tag__saver__0S1145 = -1;
  _M0Lm6tag__0S1146 = -1;
  while (1) {
    int32_t _M0L6_2atmpS3159 = _M0Lm9_2acursorS1142;
    if (_M0L6_2atmpS3159 < _M0L6_2aendS1141) {
      int32_t _M0L6_2atmpS3166 = _M0Lm9_2acursorS1142;
      int32_t _M0L10next__charS1151;
      int32_t _M0L6_2atmpS3160;
      moonbit_incref(_M0L7_2adataS1139);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1151
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1139, _M0L6_2atmpS3166);
      _M0L6_2atmpS3160 = _M0Lm9_2acursorS1142;
      _M0Lm9_2acursorS1142 = _M0L6_2atmpS3160 + 1;
      if (_M0L10next__charS1151 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS3161;
          _M0Lm6tag__0S1146 = _M0Lm9_2acursorS1142;
          _M0L6_2atmpS3161 = _M0Lm9_2acursorS1142;
          if (_M0L6_2atmpS3161 < _M0L6_2aendS1141) {
            int32_t _M0L6_2atmpS3165 = _M0Lm9_2acursorS1142;
            int32_t _M0L10next__charS1152;
            int32_t _M0L6_2atmpS3162;
            moonbit_incref(_M0L7_2adataS1139);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1152
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1139, _M0L6_2atmpS3165);
            _M0L6_2atmpS3162 = _M0Lm9_2acursorS1142;
            _M0Lm9_2acursorS1142 = _M0L6_2atmpS3162 + 1;
            if (_M0L10next__charS1152 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS3163 = _M0Lm9_2acursorS1142;
                if (_M0L6_2atmpS3163 < _M0L6_2aendS1141) {
                  int32_t _M0L6_2atmpS3164 = _M0Lm9_2acursorS1142;
                  _M0Lm9_2acursorS1142 = _M0L6_2atmpS3164 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1145 = _M0Lm6tag__0S1146;
                  _M0Lm13accept__stateS1143 = 0;
                  _M0Lm10match__endS1144 = _M0Lm9_2acursorS1142;
                  goto join_1148;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1148;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1148;
    }
    break;
  }
  goto joinlet_5047;
  join_1148:;
  switch (_M0Lm13accept__stateS1143) {
    case 0: {
      int32_t _M0L6_2atmpS3157;
      int32_t _M0L6_2atmpS3156;
      int64_t _M0L6_2atmpS3153;
      int32_t _M0L6_2atmpS3155;
      int64_t _M0L6_2atmpS3154;
      struct _M0TPC16string10StringView _M0L13package__nameS1149;
      int64_t _M0L6_2atmpS3150;
      int32_t _M0L6_2atmpS3152;
      int64_t _M0L6_2atmpS3151;
      struct _M0TPC16string10StringView _M0L12module__nameS1150;
      void* _M0L4SomeS3149;
      moonbit_decref(_M0L3pkgS1137.$0);
      _M0L6_2atmpS3157 = _M0Lm20match__tag__saver__0S1145;
      _M0L6_2atmpS3156 = _M0L6_2atmpS3157 + 1;
      _M0L6_2atmpS3153 = (int64_t)_M0L6_2atmpS3156;
      _M0L6_2atmpS3155 = _M0Lm10match__endS1144;
      _M0L6_2atmpS3154 = (int64_t)_M0L6_2atmpS3155;
      moonbit_incref(_M0L7_2adataS1139);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1149
      = _M0MPC16string6String4view(_M0L7_2adataS1139, _M0L6_2atmpS3153, _M0L6_2atmpS3154);
      _M0L6_2atmpS3150 = (int64_t)_M0L8_2astartS1140;
      _M0L6_2atmpS3152 = _M0Lm20match__tag__saver__0S1145;
      _M0L6_2atmpS3151 = (int64_t)_M0L6_2atmpS3152;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1150
      = _M0MPC16string6String4view(_M0L7_2adataS1139, _M0L6_2atmpS3150, _M0L6_2atmpS3151);
      _M0L4SomeS3149
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS3149)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3149)->$0_0
      = _M0L13package__nameS1149.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3149)->$0_1
      = _M0L13package__nameS1149.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3149)->$0_2
      = _M0L13package__nameS1149.$2;
      _M0L7_2abindS1147
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1147)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1147->$0_0 = _M0L12module__nameS1150.$0;
      _M0L7_2abindS1147->$0_1 = _M0L12module__nameS1150.$1;
      _M0L7_2abindS1147->$0_2 = _M0L12module__nameS1150.$2;
      _M0L7_2abindS1147->$1 = _M0L4SomeS3149;
      break;
    }
    default: {
      void* _M0L4NoneS3158;
      moonbit_decref(_M0L7_2adataS1139);
      _M0L4NoneS3158
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1147
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1147)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1147->$0_0 = _M0L3pkgS1137.$0;
      _M0L7_2abindS1147->$0_1 = _M0L3pkgS1137.$1;
      _M0L7_2abindS1147->$0_2 = _M0L3pkgS1137.$2;
      _M0L7_2abindS1147->$1 = _M0L4NoneS3158;
      break;
    }
  }
  joinlet_5047:;
  _M0L8_2afieldS4465
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1147->$0_1, _M0L7_2abindS1147->$0_2, _M0L7_2abindS1147->$0_0
  };
  _M0L15_2amodule__nameS1156 = _M0L8_2afieldS4465;
  _M0L8_2afieldS4464 = _M0L7_2abindS1147->$1;
  _M0L6_2acntS4784 = Moonbit_object_header(_M0L7_2abindS1147)->rc;
  if (_M0L6_2acntS4784 > 1) {
    int32_t _M0L11_2anew__cntS4785 = _M0L6_2acntS4784 - 1;
    Moonbit_object_header(_M0L7_2abindS1147)->rc = _M0L11_2anew__cntS4785;
    moonbit_incref(_M0L8_2afieldS4464);
    moonbit_incref(_M0L15_2amodule__nameS1156.$0);
  } else if (_M0L6_2acntS4784 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1147);
  }
  _M0L16_2apackage__nameS1157 = _M0L8_2afieldS4464;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1157)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1158 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1157;
      struct _M0TPC16string10StringView _M0L8_2afieldS4463 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1158->$0_1,
                                              _M0L7_2aSomeS1158->$0_2,
                                              _M0L7_2aSomeS1158->$0_0};
      int32_t _M0L6_2acntS4786 = Moonbit_object_header(_M0L7_2aSomeS1158)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1159;
      if (_M0L6_2acntS4786 > 1) {
        int32_t _M0L11_2anew__cntS4787 = _M0L6_2acntS4786 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1158)->rc = _M0L11_2anew__cntS4787;
        moonbit_incref(_M0L8_2afieldS4463.$0);
      } else if (_M0L6_2acntS4786 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1158);
      }
      _M0L12_2apkg__nameS1159 = _M0L8_2afieldS4463;
      if (_M0L6loggerS1160.$1) {
        moonbit_incref(_M0L6loggerS1160.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L12_2apkg__nameS1159);
      if (_M0L6loggerS1160.$1) {
        moonbit_incref(_M0L6loggerS1160.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1160.$0->$method_3(_M0L6loggerS1160.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1157);
      break;
    }
  }
  _M0L8_2afieldS4462
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1138->$1_1, _M0L4selfS1138->$1_2, _M0L4selfS1138->$1_0
  };
  _M0L8filenameS3144 = _M0L8_2afieldS4462;
  moonbit_incref(_M0L8filenameS3144.$0);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L8filenameS3144);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_3(_M0L6loggerS1160.$1, 58);
  _M0L8_2afieldS4461
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1138->$2_1, _M0L4selfS1138->$2_2, _M0L4selfS1138->$2_0
  };
  _M0L11start__lineS3145 = _M0L8_2afieldS4461;
  moonbit_incref(_M0L11start__lineS3145.$0);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L11start__lineS3145);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_3(_M0L6loggerS1160.$1, 58);
  _M0L8_2afieldS4460
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1138->$3_1, _M0L4selfS1138->$3_2, _M0L4selfS1138->$3_0
  };
  _M0L13start__columnS3146 = _M0L8_2afieldS4460;
  moonbit_incref(_M0L13start__columnS3146.$0);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L13start__columnS3146);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_3(_M0L6loggerS1160.$1, 45);
  _M0L8_2afieldS4459
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1138->$4_1, _M0L4selfS1138->$4_2, _M0L4selfS1138->$4_0
  };
  _M0L9end__lineS3147 = _M0L8_2afieldS4459;
  moonbit_incref(_M0L9end__lineS3147.$0);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L9end__lineS3147);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_3(_M0L6loggerS1160.$1, 58);
  _M0L8_2afieldS4458
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1138->$5_1, _M0L4selfS1138->$5_2, _M0L4selfS1138->$5_0
  };
  _M0L6_2acntS4788 = Moonbit_object_header(_M0L4selfS1138)->rc;
  if (_M0L6_2acntS4788 > 1) {
    int32_t _M0L11_2anew__cntS4794 = _M0L6_2acntS4788 - 1;
    Moonbit_object_header(_M0L4selfS1138)->rc = _M0L11_2anew__cntS4794;
    moonbit_incref(_M0L8_2afieldS4458.$0);
  } else if (_M0L6_2acntS4788 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4793 =
      (struct _M0TPC16string10StringView){_M0L4selfS1138->$4_1,
                                            _M0L4selfS1138->$4_2,
                                            _M0L4selfS1138->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4792;
    struct _M0TPC16string10StringView _M0L8_2afieldS4791;
    struct _M0TPC16string10StringView _M0L8_2afieldS4790;
    struct _M0TPC16string10StringView _M0L8_2afieldS4789;
    moonbit_decref(_M0L8_2afieldS4793.$0);
    _M0L8_2afieldS4792
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1138->$3_1, _M0L4selfS1138->$3_2, _M0L4selfS1138->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4792.$0);
    _M0L8_2afieldS4791
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1138->$2_1, _M0L4selfS1138->$2_2, _M0L4selfS1138->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4791.$0);
    _M0L8_2afieldS4790
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1138->$1_1, _M0L4selfS1138->$1_2, _M0L4selfS1138->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4790.$0);
    _M0L8_2afieldS4789
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1138->$0_1, _M0L4selfS1138->$0_2, _M0L4selfS1138->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4789.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1138);
  }
  _M0L11end__columnS3148 = _M0L8_2afieldS4458;
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L11end__columnS3148);
  if (_M0L6loggerS1160.$1) {
    moonbit_incref(_M0L6loggerS1160.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_3(_M0L6loggerS1160.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1160.$0->$method_2(_M0L6loggerS1160.$1, _M0L15_2amodule__nameS1156);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1136) {
  moonbit_string_t _M0L6_2atmpS3143;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS3143
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1136);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS3143);
  moonbit_decref(_M0L6_2atmpS3143);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1135,
  struct _M0TPB6Logger _M0L6loggerS1134
) {
  moonbit_string_t _M0L6_2atmpS3142;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS3142 = _M0MPC16double6Double10to__string(_M0L4selfS1135);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1134.$0->$method_0(_M0L6loggerS1134.$1, _M0L6_2atmpS3142);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1133) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1133);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1120) {
  uint64_t _M0L4bitsS1121;
  uint64_t _M0L6_2atmpS3141;
  uint64_t _M0L6_2atmpS3140;
  int32_t _M0L8ieeeSignS1122;
  uint64_t _M0L12ieeeMantissaS1123;
  uint64_t _M0L6_2atmpS3139;
  uint64_t _M0L6_2atmpS3138;
  int32_t _M0L12ieeeExponentS1124;
  int32_t _if__result_5051;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1125;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1126;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3137;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1120 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_77.data;
  }
  _M0L4bitsS1121 = *(int64_t*)&_M0L3valS1120;
  _M0L6_2atmpS3141 = _M0L4bitsS1121 >> 63;
  _M0L6_2atmpS3140 = _M0L6_2atmpS3141 & 1ull;
  _M0L8ieeeSignS1122 = _M0L6_2atmpS3140 != 0ull;
  _M0L12ieeeMantissaS1123 = _M0L4bitsS1121 & 4503599627370495ull;
  _M0L6_2atmpS3139 = _M0L4bitsS1121 >> 52;
  _M0L6_2atmpS3138 = _M0L6_2atmpS3139 & 2047ull;
  _M0L12ieeeExponentS1124 = (int32_t)_M0L6_2atmpS3138;
  if (_M0L12ieeeExponentS1124 == 2047) {
    _if__result_5051 = 1;
  } else if (_M0L12ieeeExponentS1124 == 0) {
    _if__result_5051 = _M0L12ieeeMantissaS1123 == 0ull;
  } else {
    _if__result_5051 = 0;
  }
  if (_if__result_5051) {
    int32_t _M0L6_2atmpS3126 = _M0L12ieeeExponentS1124 != 0;
    int32_t _M0L6_2atmpS3127 = _M0L12ieeeMantissaS1123 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1122, _M0L6_2atmpS3126, _M0L6_2atmpS3127);
  }
  _M0Lm1vS1125 = _M0FPB31ryu__to__string_2erecord_2f1119;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1126
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1123, _M0L12ieeeExponentS1124);
  if (_M0L5smallS1126 == 0) {
    uint32_t _M0L6_2atmpS3128;
    if (_M0L5smallS1126) {
      moonbit_decref(_M0L5smallS1126);
    }
    _M0L6_2atmpS3128 = *(uint32_t*)&_M0L12ieeeExponentS1124;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1125 = _M0FPB3d2d(_M0L12ieeeMantissaS1123, _M0L6_2atmpS3128);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1127 = _M0L5smallS1126;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1128 = _M0L7_2aSomeS1127;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1129 = _M0L4_2afS1128;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3136 = _M0Lm1xS1129;
      uint64_t _M0L8_2afieldS4469 = _M0L6_2atmpS3136->$0;
      uint64_t _M0L8mantissaS3135 = _M0L8_2afieldS4469;
      uint64_t _M0L1qS1130 = _M0L8mantissaS3135 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3134 = _M0Lm1xS1129;
      uint64_t _M0L8_2afieldS4468 = _M0L6_2atmpS3134->$0;
      uint64_t _M0L8mantissaS3132 = _M0L8_2afieldS4468;
      uint64_t _M0L6_2atmpS3133 = 10ull * _M0L1qS1130;
      uint64_t _M0L1rS1131 = _M0L8mantissaS3132 - _M0L6_2atmpS3133;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3131;
      int32_t _M0L8_2afieldS4467;
      int32_t _M0L8exponentS3130;
      int32_t _M0L6_2atmpS3129;
      if (_M0L1rS1131 != 0ull) {
        break;
      }
      _M0L6_2atmpS3131 = _M0Lm1xS1129;
      _M0L8_2afieldS4467 = _M0L6_2atmpS3131->$1;
      moonbit_decref(_M0L6_2atmpS3131);
      _M0L8exponentS3130 = _M0L8_2afieldS4467;
      _M0L6_2atmpS3129 = _M0L8exponentS3130 + 1;
      _M0Lm1xS1129
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1129)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1129->$0 = _M0L1qS1130;
      _M0Lm1xS1129->$1 = _M0L6_2atmpS3129;
      continue;
      break;
    }
    _M0Lm1vS1125 = _M0Lm1xS1129;
  }
  _M0L6_2atmpS3137 = _M0Lm1vS1125;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS3137, _M0L8ieeeSignS1122);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1114,
  int32_t _M0L12ieeeExponentS1116
) {
  uint64_t _M0L2m2S1113;
  int32_t _M0L6_2atmpS3125;
  int32_t _M0L2e2S1115;
  int32_t _M0L6_2atmpS3124;
  uint64_t _M0L6_2atmpS3123;
  uint64_t _M0L4maskS1117;
  uint64_t _M0L8fractionS1118;
  int32_t _M0L6_2atmpS3122;
  uint64_t _M0L6_2atmpS3121;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3120;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1113 = 4503599627370496ull | _M0L12ieeeMantissaS1114;
  _M0L6_2atmpS3125 = _M0L12ieeeExponentS1116 - 1023;
  _M0L2e2S1115 = _M0L6_2atmpS3125 - 52;
  if (_M0L2e2S1115 > 0) {
    return 0;
  }
  if (_M0L2e2S1115 < -52) {
    return 0;
  }
  _M0L6_2atmpS3124 = -_M0L2e2S1115;
  _M0L6_2atmpS3123 = 1ull << (_M0L6_2atmpS3124 & 63);
  _M0L4maskS1117 = _M0L6_2atmpS3123 - 1ull;
  _M0L8fractionS1118 = _M0L2m2S1113 & _M0L4maskS1117;
  if (_M0L8fractionS1118 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3122 = -_M0L2e2S1115;
  _M0L6_2atmpS3121 = _M0L2m2S1113 >> (_M0L6_2atmpS3122 & 63);
  _M0L6_2atmpS3120
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3120)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS3120->$0 = _M0L6_2atmpS3121;
  _M0L6_2atmpS3120->$1 = 0;
  return _M0L6_2atmpS3120;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1087,
  int32_t _M0L4signS1085
) {
  int32_t _M0L6_2atmpS3119;
  moonbit_bytes_t _M0L6resultS1083;
  int32_t _M0Lm5indexS1084;
  uint64_t _M0Lm6outputS1086;
  uint64_t _M0L6_2atmpS3118;
  int32_t _M0L7olengthS1088;
  int32_t _M0L8_2afieldS4470;
  int32_t _M0L8exponentS3117;
  int32_t _M0L6_2atmpS3116;
  int32_t _M0Lm3expS1089;
  int32_t _M0L6_2atmpS3115;
  int32_t _M0L6_2atmpS3113;
  int32_t _M0L18scientificNotationS1090;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3119 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1083
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3119);
  _M0Lm5indexS1084 = 0;
  if (_M0L4signS1085) {
    int32_t _M0L6_2atmpS2988 = _M0Lm5indexS1084;
    int32_t _M0L6_2atmpS2989;
    if (
      _M0L6_2atmpS2988 < 0
      || _M0L6_2atmpS2988 >= Moonbit_array_length(_M0L6resultS1083)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1083[_M0L6_2atmpS2988] = 45;
    _M0L6_2atmpS2989 = _M0Lm5indexS1084;
    _M0Lm5indexS1084 = _M0L6_2atmpS2989 + 1;
  }
  _M0Lm6outputS1086 = _M0L1vS1087->$0;
  _M0L6_2atmpS3118 = _M0Lm6outputS1086;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1088 = _M0FPB17decimal__length17(_M0L6_2atmpS3118);
  _M0L8_2afieldS4470 = _M0L1vS1087->$1;
  moonbit_decref(_M0L1vS1087);
  _M0L8exponentS3117 = _M0L8_2afieldS4470;
  _M0L6_2atmpS3116 = _M0L8exponentS3117 + _M0L7olengthS1088;
  _M0Lm3expS1089 = _M0L6_2atmpS3116 - 1;
  _M0L6_2atmpS3115 = _M0Lm3expS1089;
  if (_M0L6_2atmpS3115 >= -6) {
    int32_t _M0L6_2atmpS3114 = _M0Lm3expS1089;
    _M0L6_2atmpS3113 = _M0L6_2atmpS3114 < 21;
  } else {
    _M0L6_2atmpS3113 = 0;
  }
  _M0L18scientificNotationS1090 = !_M0L6_2atmpS3113;
  if (_M0L18scientificNotationS1090) {
    int32_t _M0L7_2abindS1091 = _M0L7olengthS1088 - 1;
    int32_t _M0L1iS1092 = 0;
    int32_t _M0L6_2atmpS2999;
    uint64_t _M0L6_2atmpS3004;
    int32_t _M0L6_2atmpS3003;
    int32_t _M0L6_2atmpS3002;
    int32_t _M0L6_2atmpS3001;
    int32_t _M0L6_2atmpS3000;
    int32_t _M0L6_2atmpS3008;
    int32_t _M0L6_2atmpS3009;
    int32_t _M0L6_2atmpS3010;
    int32_t _M0L6_2atmpS3011;
    int32_t _M0L6_2atmpS3012;
    int32_t _M0L6_2atmpS3018;
    int32_t _M0L6_2atmpS3051;
    while (1) {
      if (_M0L1iS1092 < _M0L7_2abindS1091) {
        uint64_t _M0L6_2atmpS2997 = _M0Lm6outputS1086;
        uint64_t _M0L1cS1093 = _M0L6_2atmpS2997 % 10ull;
        uint64_t _M0L6_2atmpS2990 = _M0Lm6outputS1086;
        int32_t _M0L6_2atmpS2996;
        int32_t _M0L6_2atmpS2995;
        int32_t _M0L6_2atmpS2991;
        int32_t _M0L6_2atmpS2994;
        int32_t _M0L6_2atmpS2993;
        int32_t _M0L6_2atmpS2992;
        int32_t _M0L6_2atmpS2998;
        _M0Lm6outputS1086 = _M0L6_2atmpS2990 / 10ull;
        _M0L6_2atmpS2996 = _M0Lm5indexS1084;
        _M0L6_2atmpS2995 = _M0L6_2atmpS2996 + _M0L7olengthS1088;
        _M0L6_2atmpS2991 = _M0L6_2atmpS2995 - _M0L1iS1092;
        _M0L6_2atmpS2994 = (int32_t)_M0L1cS1093;
        _M0L6_2atmpS2993 = 48 + _M0L6_2atmpS2994;
        _M0L6_2atmpS2992 = _M0L6_2atmpS2993 & 0xff;
        if (
          _M0L6_2atmpS2991 < 0
          || _M0L6_2atmpS2991 >= Moonbit_array_length(_M0L6resultS1083)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1083[_M0L6_2atmpS2991] = _M0L6_2atmpS2992;
        _M0L6_2atmpS2998 = _M0L1iS1092 + 1;
        _M0L1iS1092 = _M0L6_2atmpS2998;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2999 = _M0Lm5indexS1084;
    _M0L6_2atmpS3004 = _M0Lm6outputS1086;
    _M0L6_2atmpS3003 = (int32_t)_M0L6_2atmpS3004;
    _M0L6_2atmpS3002 = _M0L6_2atmpS3003 % 10;
    _M0L6_2atmpS3001 = 48 + _M0L6_2atmpS3002;
    _M0L6_2atmpS3000 = _M0L6_2atmpS3001 & 0xff;
    if (
      _M0L6_2atmpS2999 < 0
      || _M0L6_2atmpS2999 >= Moonbit_array_length(_M0L6resultS1083)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1083[_M0L6_2atmpS2999] = _M0L6_2atmpS3000;
    if (_M0L7olengthS1088 > 1) {
      int32_t _M0L6_2atmpS3006 = _M0Lm5indexS1084;
      int32_t _M0L6_2atmpS3005 = _M0L6_2atmpS3006 + 1;
      if (
        _M0L6_2atmpS3005 < 0
        || _M0L6_2atmpS3005 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3005] = 46;
    } else {
      int32_t _M0L6_2atmpS3007 = _M0Lm5indexS1084;
      _M0Lm5indexS1084 = _M0L6_2atmpS3007 - 1;
    }
    _M0L6_2atmpS3008 = _M0Lm5indexS1084;
    _M0L6_2atmpS3009 = _M0L7olengthS1088 + 1;
    _M0Lm5indexS1084 = _M0L6_2atmpS3008 + _M0L6_2atmpS3009;
    _M0L6_2atmpS3010 = _M0Lm5indexS1084;
    if (
      _M0L6_2atmpS3010 < 0
      || _M0L6_2atmpS3010 >= Moonbit_array_length(_M0L6resultS1083)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1083[_M0L6_2atmpS3010] = 101;
    _M0L6_2atmpS3011 = _M0Lm5indexS1084;
    _M0Lm5indexS1084 = _M0L6_2atmpS3011 + 1;
    _M0L6_2atmpS3012 = _M0Lm3expS1089;
    if (_M0L6_2atmpS3012 < 0) {
      int32_t _M0L6_2atmpS3013 = _M0Lm5indexS1084;
      int32_t _M0L6_2atmpS3014;
      int32_t _M0L6_2atmpS3015;
      if (
        _M0L6_2atmpS3013 < 0
        || _M0L6_2atmpS3013 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3013] = 45;
      _M0L6_2atmpS3014 = _M0Lm5indexS1084;
      _M0Lm5indexS1084 = _M0L6_2atmpS3014 + 1;
      _M0L6_2atmpS3015 = _M0Lm3expS1089;
      _M0Lm3expS1089 = -_M0L6_2atmpS3015;
    } else {
      int32_t _M0L6_2atmpS3016 = _M0Lm5indexS1084;
      int32_t _M0L6_2atmpS3017;
      if (
        _M0L6_2atmpS3016 < 0
        || _M0L6_2atmpS3016 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3016] = 43;
      _M0L6_2atmpS3017 = _M0Lm5indexS1084;
      _M0Lm5indexS1084 = _M0L6_2atmpS3017 + 1;
    }
    _M0L6_2atmpS3018 = _M0Lm3expS1089;
    if (_M0L6_2atmpS3018 >= 100) {
      int32_t _M0L6_2atmpS3034 = _M0Lm3expS1089;
      int32_t _M0L1aS1095 = _M0L6_2atmpS3034 / 100;
      int32_t _M0L6_2atmpS3033 = _M0Lm3expS1089;
      int32_t _M0L6_2atmpS3032 = _M0L6_2atmpS3033 / 10;
      int32_t _M0L1bS1096 = _M0L6_2atmpS3032 % 10;
      int32_t _M0L6_2atmpS3031 = _M0Lm3expS1089;
      int32_t _M0L1cS1097 = _M0L6_2atmpS3031 % 10;
      int32_t _M0L6_2atmpS3019 = _M0Lm5indexS1084;
      int32_t _M0L6_2atmpS3021 = 48 + _M0L1aS1095;
      int32_t _M0L6_2atmpS3020 = _M0L6_2atmpS3021 & 0xff;
      int32_t _M0L6_2atmpS3025;
      int32_t _M0L6_2atmpS3022;
      int32_t _M0L6_2atmpS3024;
      int32_t _M0L6_2atmpS3023;
      int32_t _M0L6_2atmpS3029;
      int32_t _M0L6_2atmpS3026;
      int32_t _M0L6_2atmpS3028;
      int32_t _M0L6_2atmpS3027;
      int32_t _M0L6_2atmpS3030;
      if (
        _M0L6_2atmpS3019 < 0
        || _M0L6_2atmpS3019 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3019] = _M0L6_2atmpS3020;
      _M0L6_2atmpS3025 = _M0Lm5indexS1084;
      _M0L6_2atmpS3022 = _M0L6_2atmpS3025 + 1;
      _M0L6_2atmpS3024 = 48 + _M0L1bS1096;
      _M0L6_2atmpS3023 = _M0L6_2atmpS3024 & 0xff;
      if (
        _M0L6_2atmpS3022 < 0
        || _M0L6_2atmpS3022 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3022] = _M0L6_2atmpS3023;
      _M0L6_2atmpS3029 = _M0Lm5indexS1084;
      _M0L6_2atmpS3026 = _M0L6_2atmpS3029 + 2;
      _M0L6_2atmpS3028 = 48 + _M0L1cS1097;
      _M0L6_2atmpS3027 = _M0L6_2atmpS3028 & 0xff;
      if (
        _M0L6_2atmpS3026 < 0
        || _M0L6_2atmpS3026 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3026] = _M0L6_2atmpS3027;
      _M0L6_2atmpS3030 = _M0Lm5indexS1084;
      _M0Lm5indexS1084 = _M0L6_2atmpS3030 + 3;
    } else {
      int32_t _M0L6_2atmpS3035 = _M0Lm3expS1089;
      if (_M0L6_2atmpS3035 >= 10) {
        int32_t _M0L6_2atmpS3045 = _M0Lm3expS1089;
        int32_t _M0L1aS1098 = _M0L6_2atmpS3045 / 10;
        int32_t _M0L6_2atmpS3044 = _M0Lm3expS1089;
        int32_t _M0L1bS1099 = _M0L6_2atmpS3044 % 10;
        int32_t _M0L6_2atmpS3036 = _M0Lm5indexS1084;
        int32_t _M0L6_2atmpS3038 = 48 + _M0L1aS1098;
        int32_t _M0L6_2atmpS3037 = _M0L6_2atmpS3038 & 0xff;
        int32_t _M0L6_2atmpS3042;
        int32_t _M0L6_2atmpS3039;
        int32_t _M0L6_2atmpS3041;
        int32_t _M0L6_2atmpS3040;
        int32_t _M0L6_2atmpS3043;
        if (
          _M0L6_2atmpS3036 < 0
          || _M0L6_2atmpS3036 >= Moonbit_array_length(_M0L6resultS1083)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1083[_M0L6_2atmpS3036] = _M0L6_2atmpS3037;
        _M0L6_2atmpS3042 = _M0Lm5indexS1084;
        _M0L6_2atmpS3039 = _M0L6_2atmpS3042 + 1;
        _M0L6_2atmpS3041 = 48 + _M0L1bS1099;
        _M0L6_2atmpS3040 = _M0L6_2atmpS3041 & 0xff;
        if (
          _M0L6_2atmpS3039 < 0
          || _M0L6_2atmpS3039 >= Moonbit_array_length(_M0L6resultS1083)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1083[_M0L6_2atmpS3039] = _M0L6_2atmpS3040;
        _M0L6_2atmpS3043 = _M0Lm5indexS1084;
        _M0Lm5indexS1084 = _M0L6_2atmpS3043 + 2;
      } else {
        int32_t _M0L6_2atmpS3046 = _M0Lm5indexS1084;
        int32_t _M0L6_2atmpS3049 = _M0Lm3expS1089;
        int32_t _M0L6_2atmpS3048 = 48 + _M0L6_2atmpS3049;
        int32_t _M0L6_2atmpS3047 = _M0L6_2atmpS3048 & 0xff;
        int32_t _M0L6_2atmpS3050;
        if (
          _M0L6_2atmpS3046 < 0
          || _M0L6_2atmpS3046 >= Moonbit_array_length(_M0L6resultS1083)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1083[_M0L6_2atmpS3046] = _M0L6_2atmpS3047;
        _M0L6_2atmpS3050 = _M0Lm5indexS1084;
        _M0Lm5indexS1084 = _M0L6_2atmpS3050 + 1;
      }
    }
    _M0L6_2atmpS3051 = _M0Lm5indexS1084;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1083, 0, _M0L6_2atmpS3051);
  } else {
    int32_t _M0L6_2atmpS3052 = _M0Lm3expS1089;
    int32_t _M0L6_2atmpS3112;
    if (_M0L6_2atmpS3052 < 0) {
      int32_t _M0L6_2atmpS3053 = _M0Lm5indexS1084;
      int32_t _M0L6_2atmpS3054;
      int32_t _M0L6_2atmpS3055;
      int32_t _M0L6_2atmpS3056;
      int32_t _M0L1iS1100;
      int32_t _M0L7currentS1102;
      int32_t _M0L1iS1103;
      if (
        _M0L6_2atmpS3053 < 0
        || _M0L6_2atmpS3053 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3053] = 48;
      _M0L6_2atmpS3054 = _M0Lm5indexS1084;
      _M0Lm5indexS1084 = _M0L6_2atmpS3054 + 1;
      _M0L6_2atmpS3055 = _M0Lm5indexS1084;
      if (
        _M0L6_2atmpS3055 < 0
        || _M0L6_2atmpS3055 >= Moonbit_array_length(_M0L6resultS1083)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1083[_M0L6_2atmpS3055] = 46;
      _M0L6_2atmpS3056 = _M0Lm5indexS1084;
      _M0Lm5indexS1084 = _M0L6_2atmpS3056 + 1;
      _M0L1iS1100 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3057 = _M0Lm3expS1089;
        if (_M0L1iS1100 > _M0L6_2atmpS3057) {
          int32_t _M0L6_2atmpS3058 = _M0Lm5indexS1084;
          int32_t _M0L6_2atmpS3059;
          int32_t _M0L6_2atmpS3060;
          if (
            _M0L6_2atmpS3058 < 0
            || _M0L6_2atmpS3058 >= Moonbit_array_length(_M0L6resultS1083)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1083[_M0L6_2atmpS3058] = 48;
          _M0L6_2atmpS3059 = _M0Lm5indexS1084;
          _M0Lm5indexS1084 = _M0L6_2atmpS3059 + 1;
          _M0L6_2atmpS3060 = _M0L1iS1100 - 1;
          _M0L1iS1100 = _M0L6_2atmpS3060;
          continue;
        }
        break;
      }
      _M0L7currentS1102 = _M0Lm5indexS1084;
      _M0L1iS1103 = 0;
      while (1) {
        if (_M0L1iS1103 < _M0L7olengthS1088) {
          int32_t _M0L6_2atmpS3068 = _M0L7currentS1102 + _M0L7olengthS1088;
          int32_t _M0L6_2atmpS3067 = _M0L6_2atmpS3068 - _M0L1iS1103;
          int32_t _M0L6_2atmpS3061 = _M0L6_2atmpS3067 - 1;
          uint64_t _M0L6_2atmpS3066 = _M0Lm6outputS1086;
          uint64_t _M0L6_2atmpS3065 = _M0L6_2atmpS3066 % 10ull;
          int32_t _M0L6_2atmpS3064 = (int32_t)_M0L6_2atmpS3065;
          int32_t _M0L6_2atmpS3063 = 48 + _M0L6_2atmpS3064;
          int32_t _M0L6_2atmpS3062 = _M0L6_2atmpS3063 & 0xff;
          uint64_t _M0L6_2atmpS3069;
          int32_t _M0L6_2atmpS3070;
          int32_t _M0L6_2atmpS3071;
          if (
            _M0L6_2atmpS3061 < 0
            || _M0L6_2atmpS3061 >= Moonbit_array_length(_M0L6resultS1083)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1083[_M0L6_2atmpS3061] = _M0L6_2atmpS3062;
          _M0L6_2atmpS3069 = _M0Lm6outputS1086;
          _M0Lm6outputS1086 = _M0L6_2atmpS3069 / 10ull;
          _M0L6_2atmpS3070 = _M0Lm5indexS1084;
          _M0Lm5indexS1084 = _M0L6_2atmpS3070 + 1;
          _M0L6_2atmpS3071 = _M0L1iS1103 + 1;
          _M0L1iS1103 = _M0L6_2atmpS3071;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS3073 = _M0Lm3expS1089;
      int32_t _M0L6_2atmpS3072 = _M0L6_2atmpS3073 + 1;
      if (_M0L6_2atmpS3072 >= _M0L7olengthS1088) {
        int32_t _M0L1iS1105 = 0;
        int32_t _M0L6_2atmpS3085;
        int32_t _M0L6_2atmpS3089;
        int32_t _M0L7_2abindS1107;
        int32_t _M0L2__S1108;
        while (1) {
          if (_M0L1iS1105 < _M0L7olengthS1088) {
            int32_t _M0L6_2atmpS3082 = _M0Lm5indexS1084;
            int32_t _M0L6_2atmpS3081 = _M0L6_2atmpS3082 + _M0L7olengthS1088;
            int32_t _M0L6_2atmpS3080 = _M0L6_2atmpS3081 - _M0L1iS1105;
            int32_t _M0L6_2atmpS3074 = _M0L6_2atmpS3080 - 1;
            uint64_t _M0L6_2atmpS3079 = _M0Lm6outputS1086;
            uint64_t _M0L6_2atmpS3078 = _M0L6_2atmpS3079 % 10ull;
            int32_t _M0L6_2atmpS3077 = (int32_t)_M0L6_2atmpS3078;
            int32_t _M0L6_2atmpS3076 = 48 + _M0L6_2atmpS3077;
            int32_t _M0L6_2atmpS3075 = _M0L6_2atmpS3076 & 0xff;
            uint64_t _M0L6_2atmpS3083;
            int32_t _M0L6_2atmpS3084;
            if (
              _M0L6_2atmpS3074 < 0
              || _M0L6_2atmpS3074 >= Moonbit_array_length(_M0L6resultS1083)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1083[_M0L6_2atmpS3074] = _M0L6_2atmpS3075;
            _M0L6_2atmpS3083 = _M0Lm6outputS1086;
            _M0Lm6outputS1086 = _M0L6_2atmpS3083 / 10ull;
            _M0L6_2atmpS3084 = _M0L1iS1105 + 1;
            _M0L1iS1105 = _M0L6_2atmpS3084;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3085 = _M0Lm5indexS1084;
        _M0Lm5indexS1084 = _M0L6_2atmpS3085 + _M0L7olengthS1088;
        _M0L6_2atmpS3089 = _M0Lm3expS1089;
        _M0L7_2abindS1107 = _M0L6_2atmpS3089 + 1;
        _M0L2__S1108 = _M0L7olengthS1088;
        while (1) {
          if (_M0L2__S1108 < _M0L7_2abindS1107) {
            int32_t _M0L6_2atmpS3086 = _M0Lm5indexS1084;
            int32_t _M0L6_2atmpS3087;
            int32_t _M0L6_2atmpS3088;
            if (
              _M0L6_2atmpS3086 < 0
              || _M0L6_2atmpS3086 >= Moonbit_array_length(_M0L6resultS1083)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1083[_M0L6_2atmpS3086] = 48;
            _M0L6_2atmpS3087 = _M0Lm5indexS1084;
            _M0Lm5indexS1084 = _M0L6_2atmpS3087 + 1;
            _M0L6_2atmpS3088 = _M0L2__S1108 + 1;
            _M0L2__S1108 = _M0L6_2atmpS3088;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS3111 = _M0Lm5indexS1084;
        int32_t _M0Lm7currentS1110 = _M0L6_2atmpS3111 + 1;
        int32_t _M0L1iS1111 = 0;
        int32_t _M0L6_2atmpS3109;
        int32_t _M0L6_2atmpS3110;
        while (1) {
          if (_M0L1iS1111 < _M0L7olengthS1088) {
            int32_t _M0L6_2atmpS3092 = _M0L7olengthS1088 - _M0L1iS1111;
            int32_t _M0L6_2atmpS3090 = _M0L6_2atmpS3092 - 1;
            int32_t _M0L6_2atmpS3091 = _M0Lm3expS1089;
            int32_t _M0L6_2atmpS3106;
            int32_t _M0L6_2atmpS3105;
            int32_t _M0L6_2atmpS3104;
            int32_t _M0L6_2atmpS3098;
            uint64_t _M0L6_2atmpS3103;
            uint64_t _M0L6_2atmpS3102;
            int32_t _M0L6_2atmpS3101;
            int32_t _M0L6_2atmpS3100;
            int32_t _M0L6_2atmpS3099;
            uint64_t _M0L6_2atmpS3107;
            int32_t _M0L6_2atmpS3108;
            if (_M0L6_2atmpS3090 == _M0L6_2atmpS3091) {
              int32_t _M0L6_2atmpS3096 = _M0Lm7currentS1110;
              int32_t _M0L6_2atmpS3095 = _M0L6_2atmpS3096 + _M0L7olengthS1088;
              int32_t _M0L6_2atmpS3094 = _M0L6_2atmpS3095 - _M0L1iS1111;
              int32_t _M0L6_2atmpS3093 = _M0L6_2atmpS3094 - 1;
              int32_t _M0L6_2atmpS3097;
              if (
                _M0L6_2atmpS3093 < 0
                || _M0L6_2atmpS3093 >= Moonbit_array_length(_M0L6resultS1083)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1083[_M0L6_2atmpS3093] = 46;
              _M0L6_2atmpS3097 = _M0Lm7currentS1110;
              _M0Lm7currentS1110 = _M0L6_2atmpS3097 - 1;
            }
            _M0L6_2atmpS3106 = _M0Lm7currentS1110;
            _M0L6_2atmpS3105 = _M0L6_2atmpS3106 + _M0L7olengthS1088;
            _M0L6_2atmpS3104 = _M0L6_2atmpS3105 - _M0L1iS1111;
            _M0L6_2atmpS3098 = _M0L6_2atmpS3104 - 1;
            _M0L6_2atmpS3103 = _M0Lm6outputS1086;
            _M0L6_2atmpS3102 = _M0L6_2atmpS3103 % 10ull;
            _M0L6_2atmpS3101 = (int32_t)_M0L6_2atmpS3102;
            _M0L6_2atmpS3100 = 48 + _M0L6_2atmpS3101;
            _M0L6_2atmpS3099 = _M0L6_2atmpS3100 & 0xff;
            if (
              _M0L6_2atmpS3098 < 0
              || _M0L6_2atmpS3098 >= Moonbit_array_length(_M0L6resultS1083)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1083[_M0L6_2atmpS3098] = _M0L6_2atmpS3099;
            _M0L6_2atmpS3107 = _M0Lm6outputS1086;
            _M0Lm6outputS1086 = _M0L6_2atmpS3107 / 10ull;
            _M0L6_2atmpS3108 = _M0L1iS1111 + 1;
            _M0L1iS1111 = _M0L6_2atmpS3108;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3109 = _M0Lm5indexS1084;
        _M0L6_2atmpS3110 = _M0L7olengthS1088 + 1;
        _M0Lm5indexS1084 = _M0L6_2atmpS3109 + _M0L6_2atmpS3110;
      }
    }
    _M0L6_2atmpS3112 = _M0Lm5indexS1084;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1083, 0, _M0L6_2atmpS3112);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1029,
  uint32_t _M0L12ieeeExponentS1028
) {
  int32_t _M0Lm2e2S1026;
  uint64_t _M0Lm2m2S1027;
  uint64_t _M0L6_2atmpS2987;
  uint64_t _M0L6_2atmpS2986;
  int32_t _M0L4evenS1030;
  uint64_t _M0L6_2atmpS2985;
  uint64_t _M0L2mvS1031;
  int32_t _M0L7mmShiftS1032;
  uint64_t _M0Lm2vrS1033;
  uint64_t _M0Lm2vpS1034;
  uint64_t _M0Lm2vmS1035;
  int32_t _M0Lm3e10S1036;
  int32_t _M0Lm17vmIsTrailingZerosS1037;
  int32_t _M0Lm17vrIsTrailingZerosS1038;
  int32_t _M0L6_2atmpS2887;
  int32_t _M0Lm7removedS1057;
  int32_t _M0Lm16lastRemovedDigitS1058;
  uint64_t _M0Lm6outputS1059;
  int32_t _M0L6_2atmpS2983;
  int32_t _M0L6_2atmpS2984;
  int32_t _M0L3expS1082;
  uint64_t _M0L6_2atmpS2982;
  struct _M0TPB17FloatingDecimal64* _block_5064;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1026 = 0;
  _M0Lm2m2S1027 = 0ull;
  if (_M0L12ieeeExponentS1028 == 0u) {
    _M0Lm2e2S1026 = -1076;
    _M0Lm2m2S1027 = _M0L12ieeeMantissaS1029;
  } else {
    int32_t _M0L6_2atmpS2886 = *(int32_t*)&_M0L12ieeeExponentS1028;
    int32_t _M0L6_2atmpS2885 = _M0L6_2atmpS2886 - 1023;
    int32_t _M0L6_2atmpS2884 = _M0L6_2atmpS2885 - 52;
    _M0Lm2e2S1026 = _M0L6_2atmpS2884 - 2;
    _M0Lm2m2S1027 = 4503599627370496ull | _M0L12ieeeMantissaS1029;
  }
  _M0L6_2atmpS2987 = _M0Lm2m2S1027;
  _M0L6_2atmpS2986 = _M0L6_2atmpS2987 & 1ull;
  _M0L4evenS1030 = _M0L6_2atmpS2986 == 0ull;
  _M0L6_2atmpS2985 = _M0Lm2m2S1027;
  _M0L2mvS1031 = 4ull * _M0L6_2atmpS2985;
  if (_M0L12ieeeMantissaS1029 != 0ull) {
    _M0L7mmShiftS1032 = 1;
  } else {
    _M0L7mmShiftS1032 = _M0L12ieeeExponentS1028 <= 1u;
  }
  _M0Lm2vrS1033 = 0ull;
  _M0Lm2vpS1034 = 0ull;
  _M0Lm2vmS1035 = 0ull;
  _M0Lm3e10S1036 = 0;
  _M0Lm17vmIsTrailingZerosS1037 = 0;
  _M0Lm17vrIsTrailingZerosS1038 = 0;
  _M0L6_2atmpS2887 = _M0Lm2e2S1026;
  if (_M0L6_2atmpS2887 >= 0) {
    int32_t _M0L6_2atmpS2909 = _M0Lm2e2S1026;
    int32_t _M0L6_2atmpS2905;
    int32_t _M0L6_2atmpS2908;
    int32_t _M0L6_2atmpS2907;
    int32_t _M0L6_2atmpS2906;
    int32_t _M0L1qS1039;
    int32_t _M0L6_2atmpS2904;
    int32_t _M0L6_2atmpS2903;
    int32_t _M0L1kS1040;
    int32_t _M0L6_2atmpS2902;
    int32_t _M0L6_2atmpS2901;
    int32_t _M0L6_2atmpS2900;
    int32_t _M0L1iS1041;
    struct _M0TPB8Pow5Pair _M0L4pow5S1042;
    uint64_t _M0L6_2atmpS2899;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1043;
    uint64_t _M0L8_2avrOutS1044;
    uint64_t _M0L8_2avpOutS1045;
    uint64_t _M0L8_2avmOutS1046;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2905 = _M0FPB9log10Pow2(_M0L6_2atmpS2909);
    _M0L6_2atmpS2908 = _M0Lm2e2S1026;
    _M0L6_2atmpS2907 = _M0L6_2atmpS2908 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2906 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2907);
    _M0L1qS1039 = _M0L6_2atmpS2905 - _M0L6_2atmpS2906;
    _M0Lm3e10S1036 = _M0L1qS1039;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2904 = _M0FPB8pow5bits(_M0L1qS1039);
    _M0L6_2atmpS2903 = 125 + _M0L6_2atmpS2904;
    _M0L1kS1040 = _M0L6_2atmpS2903 - 1;
    _M0L6_2atmpS2902 = _M0Lm2e2S1026;
    _M0L6_2atmpS2901 = -_M0L6_2atmpS2902;
    _M0L6_2atmpS2900 = _M0L6_2atmpS2901 + _M0L1qS1039;
    _M0L1iS1041 = _M0L6_2atmpS2900 + _M0L1kS1040;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1042 = _M0FPB22double__computeInvPow5(_M0L1qS1039);
    _M0L6_2atmpS2899 = _M0Lm2m2S1027;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1043
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2899, _M0L4pow5S1042, _M0L1iS1041, _M0L7mmShiftS1032);
    _M0L8_2avrOutS1044 = _M0L7_2abindS1043.$0;
    _M0L8_2avpOutS1045 = _M0L7_2abindS1043.$1;
    _M0L8_2avmOutS1046 = _M0L7_2abindS1043.$2;
    _M0Lm2vrS1033 = _M0L8_2avrOutS1044;
    _M0Lm2vpS1034 = _M0L8_2avpOutS1045;
    _M0Lm2vmS1035 = _M0L8_2avmOutS1046;
    if (_M0L1qS1039 <= 21) {
      int32_t _M0L6_2atmpS2895 = (int32_t)_M0L2mvS1031;
      uint64_t _M0L6_2atmpS2898 = _M0L2mvS1031 / 5ull;
      int32_t _M0L6_2atmpS2897 = (int32_t)_M0L6_2atmpS2898;
      int32_t _M0L6_2atmpS2896 = 5 * _M0L6_2atmpS2897;
      int32_t _M0L6mvMod5S1047 = _M0L6_2atmpS2895 - _M0L6_2atmpS2896;
      if (_M0L6mvMod5S1047 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1038
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1031, _M0L1qS1039);
      } else if (_M0L4evenS1030) {
        uint64_t _M0L6_2atmpS2889 = _M0L2mvS1031 - 1ull;
        uint64_t _M0L6_2atmpS2890;
        uint64_t _M0L6_2atmpS2888;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2890 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1032);
        _M0L6_2atmpS2888 = _M0L6_2atmpS2889 - _M0L6_2atmpS2890;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1037
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2888, _M0L1qS1039);
      } else {
        uint64_t _M0L6_2atmpS2891 = _M0Lm2vpS1034;
        uint64_t _M0L6_2atmpS2894 = _M0L2mvS1031 + 2ull;
        int32_t _M0L6_2atmpS2893;
        uint64_t _M0L6_2atmpS2892;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2893
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2894, _M0L1qS1039);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2892 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2893);
        _M0Lm2vpS1034 = _M0L6_2atmpS2891 - _M0L6_2atmpS2892;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2923 = _M0Lm2e2S1026;
    int32_t _M0L6_2atmpS2922 = -_M0L6_2atmpS2923;
    int32_t _M0L6_2atmpS2917;
    int32_t _M0L6_2atmpS2921;
    int32_t _M0L6_2atmpS2920;
    int32_t _M0L6_2atmpS2919;
    int32_t _M0L6_2atmpS2918;
    int32_t _M0L1qS1048;
    int32_t _M0L6_2atmpS2910;
    int32_t _M0L6_2atmpS2916;
    int32_t _M0L6_2atmpS2915;
    int32_t _M0L1iS1049;
    int32_t _M0L6_2atmpS2914;
    int32_t _M0L1kS1050;
    int32_t _M0L1jS1051;
    struct _M0TPB8Pow5Pair _M0L4pow5S1052;
    uint64_t _M0L6_2atmpS2913;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1053;
    uint64_t _M0L8_2avrOutS1054;
    uint64_t _M0L8_2avpOutS1055;
    uint64_t _M0L8_2avmOutS1056;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2917 = _M0FPB9log10Pow5(_M0L6_2atmpS2922);
    _M0L6_2atmpS2921 = _M0Lm2e2S1026;
    _M0L6_2atmpS2920 = -_M0L6_2atmpS2921;
    _M0L6_2atmpS2919 = _M0L6_2atmpS2920 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2918 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2919);
    _M0L1qS1048 = _M0L6_2atmpS2917 - _M0L6_2atmpS2918;
    _M0L6_2atmpS2910 = _M0Lm2e2S1026;
    _M0Lm3e10S1036 = _M0L1qS1048 + _M0L6_2atmpS2910;
    _M0L6_2atmpS2916 = _M0Lm2e2S1026;
    _M0L6_2atmpS2915 = -_M0L6_2atmpS2916;
    _M0L1iS1049 = _M0L6_2atmpS2915 - _M0L1qS1048;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2914 = _M0FPB8pow5bits(_M0L1iS1049);
    _M0L1kS1050 = _M0L6_2atmpS2914 - 125;
    _M0L1jS1051 = _M0L1qS1048 - _M0L1kS1050;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1052 = _M0FPB19double__computePow5(_M0L1iS1049);
    _M0L6_2atmpS2913 = _M0Lm2m2S1027;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1053
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2913, _M0L4pow5S1052, _M0L1jS1051, _M0L7mmShiftS1032);
    _M0L8_2avrOutS1054 = _M0L7_2abindS1053.$0;
    _M0L8_2avpOutS1055 = _M0L7_2abindS1053.$1;
    _M0L8_2avmOutS1056 = _M0L7_2abindS1053.$2;
    _M0Lm2vrS1033 = _M0L8_2avrOutS1054;
    _M0Lm2vpS1034 = _M0L8_2avpOutS1055;
    _M0Lm2vmS1035 = _M0L8_2avmOutS1056;
    if (_M0L1qS1048 <= 1) {
      _M0Lm17vrIsTrailingZerosS1038 = 1;
      if (_M0L4evenS1030) {
        int32_t _M0L6_2atmpS2911;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2911 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1032);
        _M0Lm17vmIsTrailingZerosS1037 = _M0L6_2atmpS2911 == 1;
      } else {
        uint64_t _M0L6_2atmpS2912 = _M0Lm2vpS1034;
        _M0Lm2vpS1034 = _M0L6_2atmpS2912 - 1ull;
      }
    } else if (_M0L1qS1048 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1038
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1031, _M0L1qS1048);
    }
  }
  _M0Lm7removedS1057 = 0;
  _M0Lm16lastRemovedDigitS1058 = 0;
  _M0Lm6outputS1059 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1037 || _M0Lm17vrIsTrailingZerosS1038) {
    int32_t _if__result_5061;
    uint64_t _M0L6_2atmpS2953;
    uint64_t _M0L6_2atmpS2959;
    uint64_t _M0L6_2atmpS2960;
    int32_t _if__result_5062;
    int32_t _M0L6_2atmpS2956;
    int64_t _M0L6_2atmpS2955;
    uint64_t _M0L6_2atmpS2954;
    while (1) {
      uint64_t _M0L6_2atmpS2936 = _M0Lm2vpS1034;
      uint64_t _M0L7vpDiv10S1060 = _M0L6_2atmpS2936 / 10ull;
      uint64_t _M0L6_2atmpS2935 = _M0Lm2vmS1035;
      uint64_t _M0L7vmDiv10S1061 = _M0L6_2atmpS2935 / 10ull;
      uint64_t _M0L6_2atmpS2934;
      int32_t _M0L6_2atmpS2931;
      int32_t _M0L6_2atmpS2933;
      int32_t _M0L6_2atmpS2932;
      int32_t _M0L7vmMod10S1063;
      uint64_t _M0L6_2atmpS2930;
      uint64_t _M0L7vrDiv10S1064;
      uint64_t _M0L6_2atmpS2929;
      int32_t _M0L6_2atmpS2926;
      int32_t _M0L6_2atmpS2928;
      int32_t _M0L6_2atmpS2927;
      int32_t _M0L7vrMod10S1065;
      int32_t _M0L6_2atmpS2925;
      if (_M0L7vpDiv10S1060 <= _M0L7vmDiv10S1061) {
        break;
      }
      _M0L6_2atmpS2934 = _M0Lm2vmS1035;
      _M0L6_2atmpS2931 = (int32_t)_M0L6_2atmpS2934;
      _M0L6_2atmpS2933 = (int32_t)_M0L7vmDiv10S1061;
      _M0L6_2atmpS2932 = 10 * _M0L6_2atmpS2933;
      _M0L7vmMod10S1063 = _M0L6_2atmpS2931 - _M0L6_2atmpS2932;
      _M0L6_2atmpS2930 = _M0Lm2vrS1033;
      _M0L7vrDiv10S1064 = _M0L6_2atmpS2930 / 10ull;
      _M0L6_2atmpS2929 = _M0Lm2vrS1033;
      _M0L6_2atmpS2926 = (int32_t)_M0L6_2atmpS2929;
      _M0L6_2atmpS2928 = (int32_t)_M0L7vrDiv10S1064;
      _M0L6_2atmpS2927 = 10 * _M0L6_2atmpS2928;
      _M0L7vrMod10S1065 = _M0L6_2atmpS2926 - _M0L6_2atmpS2927;
      if (_M0Lm17vmIsTrailingZerosS1037) {
        _M0Lm17vmIsTrailingZerosS1037 = _M0L7vmMod10S1063 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1037 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1038) {
        int32_t _M0L6_2atmpS2924 = _M0Lm16lastRemovedDigitS1058;
        _M0Lm17vrIsTrailingZerosS1038 = _M0L6_2atmpS2924 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1038 = 0;
      }
      _M0Lm16lastRemovedDigitS1058 = _M0L7vrMod10S1065;
      _M0Lm2vrS1033 = _M0L7vrDiv10S1064;
      _M0Lm2vpS1034 = _M0L7vpDiv10S1060;
      _M0Lm2vmS1035 = _M0L7vmDiv10S1061;
      _M0L6_2atmpS2925 = _M0Lm7removedS1057;
      _M0Lm7removedS1057 = _M0L6_2atmpS2925 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1037) {
      while (1) {
        uint64_t _M0L6_2atmpS2949 = _M0Lm2vmS1035;
        uint64_t _M0L7vmDiv10S1066 = _M0L6_2atmpS2949 / 10ull;
        uint64_t _M0L6_2atmpS2948 = _M0Lm2vmS1035;
        int32_t _M0L6_2atmpS2945 = (int32_t)_M0L6_2atmpS2948;
        int32_t _M0L6_2atmpS2947 = (int32_t)_M0L7vmDiv10S1066;
        int32_t _M0L6_2atmpS2946 = 10 * _M0L6_2atmpS2947;
        int32_t _M0L7vmMod10S1067 = _M0L6_2atmpS2945 - _M0L6_2atmpS2946;
        uint64_t _M0L6_2atmpS2944;
        uint64_t _M0L7vpDiv10S1069;
        uint64_t _M0L6_2atmpS2943;
        uint64_t _M0L7vrDiv10S1070;
        uint64_t _M0L6_2atmpS2942;
        int32_t _M0L6_2atmpS2939;
        int32_t _M0L6_2atmpS2941;
        int32_t _M0L6_2atmpS2940;
        int32_t _M0L7vrMod10S1071;
        int32_t _M0L6_2atmpS2938;
        if (_M0L7vmMod10S1067 != 0) {
          break;
        }
        _M0L6_2atmpS2944 = _M0Lm2vpS1034;
        _M0L7vpDiv10S1069 = _M0L6_2atmpS2944 / 10ull;
        _M0L6_2atmpS2943 = _M0Lm2vrS1033;
        _M0L7vrDiv10S1070 = _M0L6_2atmpS2943 / 10ull;
        _M0L6_2atmpS2942 = _M0Lm2vrS1033;
        _M0L6_2atmpS2939 = (int32_t)_M0L6_2atmpS2942;
        _M0L6_2atmpS2941 = (int32_t)_M0L7vrDiv10S1070;
        _M0L6_2atmpS2940 = 10 * _M0L6_2atmpS2941;
        _M0L7vrMod10S1071 = _M0L6_2atmpS2939 - _M0L6_2atmpS2940;
        if (_M0Lm17vrIsTrailingZerosS1038) {
          int32_t _M0L6_2atmpS2937 = _M0Lm16lastRemovedDigitS1058;
          _M0Lm17vrIsTrailingZerosS1038 = _M0L6_2atmpS2937 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1038 = 0;
        }
        _M0Lm16lastRemovedDigitS1058 = _M0L7vrMod10S1071;
        _M0Lm2vrS1033 = _M0L7vrDiv10S1070;
        _M0Lm2vpS1034 = _M0L7vpDiv10S1069;
        _M0Lm2vmS1035 = _M0L7vmDiv10S1066;
        _M0L6_2atmpS2938 = _M0Lm7removedS1057;
        _M0Lm7removedS1057 = _M0L6_2atmpS2938 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1038) {
      int32_t _M0L6_2atmpS2952 = _M0Lm16lastRemovedDigitS1058;
      if (_M0L6_2atmpS2952 == 5) {
        uint64_t _M0L6_2atmpS2951 = _M0Lm2vrS1033;
        uint64_t _M0L6_2atmpS2950 = _M0L6_2atmpS2951 % 2ull;
        _if__result_5061 = _M0L6_2atmpS2950 == 0ull;
      } else {
        _if__result_5061 = 0;
      }
    } else {
      _if__result_5061 = 0;
    }
    if (_if__result_5061) {
      _M0Lm16lastRemovedDigitS1058 = 4;
    }
    _M0L6_2atmpS2953 = _M0Lm2vrS1033;
    _M0L6_2atmpS2959 = _M0Lm2vrS1033;
    _M0L6_2atmpS2960 = _M0Lm2vmS1035;
    if (_M0L6_2atmpS2959 == _M0L6_2atmpS2960) {
      if (!_M0L4evenS1030) {
        _if__result_5062 = 1;
      } else {
        int32_t _M0L6_2atmpS2958 = _M0Lm17vmIsTrailingZerosS1037;
        _if__result_5062 = !_M0L6_2atmpS2958;
      }
    } else {
      _if__result_5062 = 0;
    }
    if (_if__result_5062) {
      _M0L6_2atmpS2956 = 1;
    } else {
      int32_t _M0L6_2atmpS2957 = _M0Lm16lastRemovedDigitS1058;
      _M0L6_2atmpS2956 = _M0L6_2atmpS2957 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2955 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2956);
    _M0L6_2atmpS2954 = *(uint64_t*)&_M0L6_2atmpS2955;
    _M0Lm6outputS1059 = _M0L6_2atmpS2953 + _M0L6_2atmpS2954;
  } else {
    int32_t _M0Lm7roundUpS1072 = 0;
    uint64_t _M0L6_2atmpS2981 = _M0Lm2vpS1034;
    uint64_t _M0L8vpDiv100S1073 = _M0L6_2atmpS2981 / 100ull;
    uint64_t _M0L6_2atmpS2980 = _M0Lm2vmS1035;
    uint64_t _M0L8vmDiv100S1074 = _M0L6_2atmpS2980 / 100ull;
    uint64_t _M0L6_2atmpS2975;
    uint64_t _M0L6_2atmpS2978;
    uint64_t _M0L6_2atmpS2979;
    int32_t _M0L6_2atmpS2977;
    uint64_t _M0L6_2atmpS2976;
    if (_M0L8vpDiv100S1073 > _M0L8vmDiv100S1074) {
      uint64_t _M0L6_2atmpS2966 = _M0Lm2vrS1033;
      uint64_t _M0L8vrDiv100S1075 = _M0L6_2atmpS2966 / 100ull;
      uint64_t _M0L6_2atmpS2965 = _M0Lm2vrS1033;
      int32_t _M0L6_2atmpS2962 = (int32_t)_M0L6_2atmpS2965;
      int32_t _M0L6_2atmpS2964 = (int32_t)_M0L8vrDiv100S1075;
      int32_t _M0L6_2atmpS2963 = 100 * _M0L6_2atmpS2964;
      int32_t _M0L8vrMod100S1076 = _M0L6_2atmpS2962 - _M0L6_2atmpS2963;
      int32_t _M0L6_2atmpS2961;
      _M0Lm7roundUpS1072 = _M0L8vrMod100S1076 >= 50;
      _M0Lm2vrS1033 = _M0L8vrDiv100S1075;
      _M0Lm2vpS1034 = _M0L8vpDiv100S1073;
      _M0Lm2vmS1035 = _M0L8vmDiv100S1074;
      _M0L6_2atmpS2961 = _M0Lm7removedS1057;
      _M0Lm7removedS1057 = _M0L6_2atmpS2961 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2974 = _M0Lm2vpS1034;
      uint64_t _M0L7vpDiv10S1077 = _M0L6_2atmpS2974 / 10ull;
      uint64_t _M0L6_2atmpS2973 = _M0Lm2vmS1035;
      uint64_t _M0L7vmDiv10S1078 = _M0L6_2atmpS2973 / 10ull;
      uint64_t _M0L6_2atmpS2972;
      uint64_t _M0L7vrDiv10S1080;
      uint64_t _M0L6_2atmpS2971;
      int32_t _M0L6_2atmpS2968;
      int32_t _M0L6_2atmpS2970;
      int32_t _M0L6_2atmpS2969;
      int32_t _M0L7vrMod10S1081;
      int32_t _M0L6_2atmpS2967;
      if (_M0L7vpDiv10S1077 <= _M0L7vmDiv10S1078) {
        break;
      }
      _M0L6_2atmpS2972 = _M0Lm2vrS1033;
      _M0L7vrDiv10S1080 = _M0L6_2atmpS2972 / 10ull;
      _M0L6_2atmpS2971 = _M0Lm2vrS1033;
      _M0L6_2atmpS2968 = (int32_t)_M0L6_2atmpS2971;
      _M0L6_2atmpS2970 = (int32_t)_M0L7vrDiv10S1080;
      _M0L6_2atmpS2969 = 10 * _M0L6_2atmpS2970;
      _M0L7vrMod10S1081 = _M0L6_2atmpS2968 - _M0L6_2atmpS2969;
      _M0Lm7roundUpS1072 = _M0L7vrMod10S1081 >= 5;
      _M0Lm2vrS1033 = _M0L7vrDiv10S1080;
      _M0Lm2vpS1034 = _M0L7vpDiv10S1077;
      _M0Lm2vmS1035 = _M0L7vmDiv10S1078;
      _M0L6_2atmpS2967 = _M0Lm7removedS1057;
      _M0Lm7removedS1057 = _M0L6_2atmpS2967 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2975 = _M0Lm2vrS1033;
    _M0L6_2atmpS2978 = _M0Lm2vrS1033;
    _M0L6_2atmpS2979 = _M0Lm2vmS1035;
    _M0L6_2atmpS2977
    = _M0L6_2atmpS2978 == _M0L6_2atmpS2979 || _M0Lm7roundUpS1072;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2976 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2977);
    _M0Lm6outputS1059 = _M0L6_2atmpS2975 + _M0L6_2atmpS2976;
  }
  _M0L6_2atmpS2983 = _M0Lm3e10S1036;
  _M0L6_2atmpS2984 = _M0Lm7removedS1057;
  _M0L3expS1082 = _M0L6_2atmpS2983 + _M0L6_2atmpS2984;
  _M0L6_2atmpS2982 = _M0Lm6outputS1059;
  _block_5064
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_5064)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_5064->$0 = _M0L6_2atmpS2982;
  _block_5064->$1 = _M0L3expS1082;
  return _block_5064;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1025) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1025) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1024) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1024) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1023) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1023) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1022) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS1022 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1022 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1022 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1022 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1022 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1022 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1022 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1022 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1022 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1022 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1022 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1022 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1022 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1022 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1022 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1022 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1005) {
  int32_t _M0L6_2atmpS2883;
  int32_t _M0L6_2atmpS2882;
  int32_t _M0L4baseS1004;
  int32_t _M0L5base2S1006;
  int32_t _M0L6offsetS1007;
  int32_t _M0L6_2atmpS2881;
  uint64_t _M0L4mul0S1008;
  int32_t _M0L6_2atmpS2880;
  int32_t _M0L6_2atmpS2879;
  uint64_t _M0L4mul1S1009;
  uint64_t _M0L1mS1010;
  struct _M0TPB7Umul128 _M0L7_2abindS1011;
  uint64_t _M0L7_2alow1S1012;
  uint64_t _M0L8_2ahigh1S1013;
  struct _M0TPB7Umul128 _M0L7_2abindS1014;
  uint64_t _M0L7_2alow0S1015;
  uint64_t _M0L8_2ahigh0S1016;
  uint64_t _M0L3sumS1017;
  uint64_t _M0Lm5high1S1018;
  int32_t _M0L6_2atmpS2877;
  int32_t _M0L6_2atmpS2878;
  int32_t _M0L5deltaS1019;
  uint64_t _M0L6_2atmpS2876;
  uint64_t _M0L6_2atmpS2868;
  int32_t _M0L6_2atmpS2875;
  uint32_t _M0L6_2atmpS2872;
  int32_t _M0L6_2atmpS2874;
  int32_t _M0L6_2atmpS2873;
  uint32_t _M0L6_2atmpS2871;
  uint32_t _M0L6_2atmpS2870;
  uint64_t _M0L6_2atmpS2869;
  uint64_t _M0L1aS1020;
  uint64_t _M0L6_2atmpS2867;
  uint64_t _M0L1bS1021;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2883 = _M0L1iS1005 + 26;
  _M0L6_2atmpS2882 = _M0L6_2atmpS2883 - 1;
  _M0L4baseS1004 = _M0L6_2atmpS2882 / 26;
  _M0L5base2S1006 = _M0L4baseS1004 * 26;
  _M0L6offsetS1007 = _M0L5base2S1006 - _M0L1iS1005;
  _M0L6_2atmpS2881 = _M0L4baseS1004 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1008
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2881);
  _M0L6_2atmpS2880 = _M0L4baseS1004 * 2;
  _M0L6_2atmpS2879 = _M0L6_2atmpS2880 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1009
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2879);
  if (_M0L6offsetS1007 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1008, _M0L4mul1S1009};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1010
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1007);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1011 = _M0FPB7umul128(_M0L1mS1010, _M0L4mul1S1009);
  _M0L7_2alow1S1012 = _M0L7_2abindS1011.$0;
  _M0L8_2ahigh1S1013 = _M0L7_2abindS1011.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1014 = _M0FPB7umul128(_M0L1mS1010, _M0L4mul0S1008);
  _M0L7_2alow0S1015 = _M0L7_2abindS1014.$0;
  _M0L8_2ahigh0S1016 = _M0L7_2abindS1014.$1;
  _M0L3sumS1017 = _M0L8_2ahigh0S1016 + _M0L7_2alow1S1012;
  _M0Lm5high1S1018 = _M0L8_2ahigh1S1013;
  if (_M0L3sumS1017 < _M0L8_2ahigh0S1016) {
    uint64_t _M0L6_2atmpS2866 = _M0Lm5high1S1018;
    _M0Lm5high1S1018 = _M0L6_2atmpS2866 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2877 = _M0FPB8pow5bits(_M0L5base2S1006);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2878 = _M0FPB8pow5bits(_M0L1iS1005);
  _M0L5deltaS1019 = _M0L6_2atmpS2877 - _M0L6_2atmpS2878;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2876
  = _M0FPB13shiftright128(_M0L7_2alow0S1015, _M0L3sumS1017, _M0L5deltaS1019);
  _M0L6_2atmpS2868 = _M0L6_2atmpS2876 + 1ull;
  _M0L6_2atmpS2875 = _M0L1iS1005 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2872
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2875);
  _M0L6_2atmpS2874 = _M0L1iS1005 % 16;
  _M0L6_2atmpS2873 = _M0L6_2atmpS2874 << 1;
  _M0L6_2atmpS2871 = _M0L6_2atmpS2872 >> (_M0L6_2atmpS2873 & 31);
  _M0L6_2atmpS2870 = _M0L6_2atmpS2871 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2869 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2870);
  _M0L1aS1020 = _M0L6_2atmpS2868 + _M0L6_2atmpS2869;
  _M0L6_2atmpS2867 = _M0Lm5high1S1018;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1021
  = _M0FPB13shiftright128(_M0L3sumS1017, _M0L6_2atmpS2867, _M0L5deltaS1019);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1020, _M0L1bS1021};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS987) {
  int32_t _M0L4baseS986;
  int32_t _M0L5base2S988;
  int32_t _M0L6offsetS989;
  int32_t _M0L6_2atmpS2865;
  uint64_t _M0L4mul0S990;
  int32_t _M0L6_2atmpS2864;
  int32_t _M0L6_2atmpS2863;
  uint64_t _M0L4mul1S991;
  uint64_t _M0L1mS992;
  struct _M0TPB7Umul128 _M0L7_2abindS993;
  uint64_t _M0L7_2alow1S994;
  uint64_t _M0L8_2ahigh1S995;
  struct _M0TPB7Umul128 _M0L7_2abindS996;
  uint64_t _M0L7_2alow0S997;
  uint64_t _M0L8_2ahigh0S998;
  uint64_t _M0L3sumS999;
  uint64_t _M0Lm5high1S1000;
  int32_t _M0L6_2atmpS2861;
  int32_t _M0L6_2atmpS2862;
  int32_t _M0L5deltaS1001;
  uint64_t _M0L6_2atmpS2853;
  int32_t _M0L6_2atmpS2860;
  uint32_t _M0L6_2atmpS2857;
  int32_t _M0L6_2atmpS2859;
  int32_t _M0L6_2atmpS2858;
  uint32_t _M0L6_2atmpS2856;
  uint32_t _M0L6_2atmpS2855;
  uint64_t _M0L6_2atmpS2854;
  uint64_t _M0L1aS1002;
  uint64_t _M0L6_2atmpS2852;
  uint64_t _M0L1bS1003;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS986 = _M0L1iS987 / 26;
  _M0L5base2S988 = _M0L4baseS986 * 26;
  _M0L6offsetS989 = _M0L1iS987 - _M0L5base2S988;
  _M0L6_2atmpS2865 = _M0L4baseS986 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S990
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2865);
  _M0L6_2atmpS2864 = _M0L4baseS986 * 2;
  _M0L6_2atmpS2863 = _M0L6_2atmpS2864 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S991
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2863);
  if (_M0L6offsetS989 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S990, _M0L4mul1S991};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS992
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS989);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS993 = _M0FPB7umul128(_M0L1mS992, _M0L4mul1S991);
  _M0L7_2alow1S994 = _M0L7_2abindS993.$0;
  _M0L8_2ahigh1S995 = _M0L7_2abindS993.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS996 = _M0FPB7umul128(_M0L1mS992, _M0L4mul0S990);
  _M0L7_2alow0S997 = _M0L7_2abindS996.$0;
  _M0L8_2ahigh0S998 = _M0L7_2abindS996.$1;
  _M0L3sumS999 = _M0L8_2ahigh0S998 + _M0L7_2alow1S994;
  _M0Lm5high1S1000 = _M0L8_2ahigh1S995;
  if (_M0L3sumS999 < _M0L8_2ahigh0S998) {
    uint64_t _M0L6_2atmpS2851 = _M0Lm5high1S1000;
    _M0Lm5high1S1000 = _M0L6_2atmpS2851 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2861 = _M0FPB8pow5bits(_M0L1iS987);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2862 = _M0FPB8pow5bits(_M0L5base2S988);
  _M0L5deltaS1001 = _M0L6_2atmpS2861 - _M0L6_2atmpS2862;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2853
  = _M0FPB13shiftright128(_M0L7_2alow0S997, _M0L3sumS999, _M0L5deltaS1001);
  _M0L6_2atmpS2860 = _M0L1iS987 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2857
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2860);
  _M0L6_2atmpS2859 = _M0L1iS987 % 16;
  _M0L6_2atmpS2858 = _M0L6_2atmpS2859 << 1;
  _M0L6_2atmpS2856 = _M0L6_2atmpS2857 >> (_M0L6_2atmpS2858 & 31);
  _M0L6_2atmpS2855 = _M0L6_2atmpS2856 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2854 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2855);
  _M0L1aS1002 = _M0L6_2atmpS2853 + _M0L6_2atmpS2854;
  _M0L6_2atmpS2852 = _M0Lm5high1S1000;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1003
  = _M0FPB13shiftright128(_M0L3sumS999, _M0L6_2atmpS2852, _M0L5deltaS1001);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1002, _M0L1bS1003};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS960,
  struct _M0TPB8Pow5Pair _M0L3mulS957,
  int32_t _M0L1jS973,
  int32_t _M0L7mmShiftS975
) {
  uint64_t _M0L7_2amul0S956;
  uint64_t _M0L7_2amul1S958;
  uint64_t _M0L1mS959;
  struct _M0TPB7Umul128 _M0L7_2abindS961;
  uint64_t _M0L5_2aloS962;
  uint64_t _M0L6_2atmpS963;
  struct _M0TPB7Umul128 _M0L7_2abindS964;
  uint64_t _M0L6_2alo2S965;
  uint64_t _M0L6_2ahi2S966;
  uint64_t _M0L3midS967;
  uint64_t _M0L6_2atmpS2850;
  uint64_t _M0L2hiS968;
  uint64_t _M0L3lo2S969;
  uint64_t _M0L6_2atmpS2848;
  uint64_t _M0L6_2atmpS2849;
  uint64_t _M0L4mid2S970;
  uint64_t _M0L6_2atmpS2847;
  uint64_t _M0L3hi2S971;
  int32_t _M0L6_2atmpS2846;
  int32_t _M0L6_2atmpS2845;
  uint64_t _M0L2vpS972;
  uint64_t _M0Lm2vmS974;
  int32_t _M0L6_2atmpS2844;
  int32_t _M0L6_2atmpS2843;
  uint64_t _M0L2vrS985;
  uint64_t _M0L6_2atmpS2842;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S956 = _M0L3mulS957.$0;
  _M0L7_2amul1S958 = _M0L3mulS957.$1;
  _M0L1mS959 = _M0L1mS960 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS961 = _M0FPB7umul128(_M0L1mS959, _M0L7_2amul0S956);
  _M0L5_2aloS962 = _M0L7_2abindS961.$0;
  _M0L6_2atmpS963 = _M0L7_2abindS961.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS964 = _M0FPB7umul128(_M0L1mS959, _M0L7_2amul1S958);
  _M0L6_2alo2S965 = _M0L7_2abindS964.$0;
  _M0L6_2ahi2S966 = _M0L7_2abindS964.$1;
  _M0L3midS967 = _M0L6_2atmpS963 + _M0L6_2alo2S965;
  if (_M0L3midS967 < _M0L6_2atmpS963) {
    _M0L6_2atmpS2850 = 1ull;
  } else {
    _M0L6_2atmpS2850 = 0ull;
  }
  _M0L2hiS968 = _M0L6_2ahi2S966 + _M0L6_2atmpS2850;
  _M0L3lo2S969 = _M0L5_2aloS962 + _M0L7_2amul0S956;
  _M0L6_2atmpS2848 = _M0L3midS967 + _M0L7_2amul1S958;
  if (_M0L3lo2S969 < _M0L5_2aloS962) {
    _M0L6_2atmpS2849 = 1ull;
  } else {
    _M0L6_2atmpS2849 = 0ull;
  }
  _M0L4mid2S970 = _M0L6_2atmpS2848 + _M0L6_2atmpS2849;
  if (_M0L4mid2S970 < _M0L3midS967) {
    _M0L6_2atmpS2847 = 1ull;
  } else {
    _M0L6_2atmpS2847 = 0ull;
  }
  _M0L3hi2S971 = _M0L2hiS968 + _M0L6_2atmpS2847;
  _M0L6_2atmpS2846 = _M0L1jS973 - 64;
  _M0L6_2atmpS2845 = _M0L6_2atmpS2846 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS972
  = _M0FPB13shiftright128(_M0L4mid2S970, _M0L3hi2S971, _M0L6_2atmpS2845);
  _M0Lm2vmS974 = 0ull;
  if (_M0L7mmShiftS975) {
    uint64_t _M0L3lo3S976 = _M0L5_2aloS962 - _M0L7_2amul0S956;
    uint64_t _M0L6_2atmpS2832 = _M0L3midS967 - _M0L7_2amul1S958;
    uint64_t _M0L6_2atmpS2833;
    uint64_t _M0L4mid3S977;
    uint64_t _M0L6_2atmpS2831;
    uint64_t _M0L3hi3S978;
    int32_t _M0L6_2atmpS2830;
    int32_t _M0L6_2atmpS2829;
    if (_M0L5_2aloS962 < _M0L3lo3S976) {
      _M0L6_2atmpS2833 = 1ull;
    } else {
      _M0L6_2atmpS2833 = 0ull;
    }
    _M0L4mid3S977 = _M0L6_2atmpS2832 - _M0L6_2atmpS2833;
    if (_M0L3midS967 < _M0L4mid3S977) {
      _M0L6_2atmpS2831 = 1ull;
    } else {
      _M0L6_2atmpS2831 = 0ull;
    }
    _M0L3hi3S978 = _M0L2hiS968 - _M0L6_2atmpS2831;
    _M0L6_2atmpS2830 = _M0L1jS973 - 64;
    _M0L6_2atmpS2829 = _M0L6_2atmpS2830 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS974
    = _M0FPB13shiftright128(_M0L4mid3S977, _M0L3hi3S978, _M0L6_2atmpS2829);
  } else {
    uint64_t _M0L3lo3S979 = _M0L5_2aloS962 + _M0L5_2aloS962;
    uint64_t _M0L6_2atmpS2840 = _M0L3midS967 + _M0L3midS967;
    uint64_t _M0L6_2atmpS2841;
    uint64_t _M0L4mid3S980;
    uint64_t _M0L6_2atmpS2838;
    uint64_t _M0L6_2atmpS2839;
    uint64_t _M0L3hi3S981;
    uint64_t _M0L3lo4S982;
    uint64_t _M0L6_2atmpS2836;
    uint64_t _M0L6_2atmpS2837;
    uint64_t _M0L4mid4S983;
    uint64_t _M0L6_2atmpS2835;
    uint64_t _M0L3hi4S984;
    int32_t _M0L6_2atmpS2834;
    if (_M0L3lo3S979 < _M0L5_2aloS962) {
      _M0L6_2atmpS2841 = 1ull;
    } else {
      _M0L6_2atmpS2841 = 0ull;
    }
    _M0L4mid3S980 = _M0L6_2atmpS2840 + _M0L6_2atmpS2841;
    _M0L6_2atmpS2838 = _M0L2hiS968 + _M0L2hiS968;
    if (_M0L4mid3S980 < _M0L3midS967) {
      _M0L6_2atmpS2839 = 1ull;
    } else {
      _M0L6_2atmpS2839 = 0ull;
    }
    _M0L3hi3S981 = _M0L6_2atmpS2838 + _M0L6_2atmpS2839;
    _M0L3lo4S982 = _M0L3lo3S979 - _M0L7_2amul0S956;
    _M0L6_2atmpS2836 = _M0L4mid3S980 - _M0L7_2amul1S958;
    if (_M0L3lo3S979 < _M0L3lo4S982) {
      _M0L6_2atmpS2837 = 1ull;
    } else {
      _M0L6_2atmpS2837 = 0ull;
    }
    _M0L4mid4S983 = _M0L6_2atmpS2836 - _M0L6_2atmpS2837;
    if (_M0L4mid3S980 < _M0L4mid4S983) {
      _M0L6_2atmpS2835 = 1ull;
    } else {
      _M0L6_2atmpS2835 = 0ull;
    }
    _M0L3hi4S984 = _M0L3hi3S981 - _M0L6_2atmpS2835;
    _M0L6_2atmpS2834 = _M0L1jS973 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS974
    = _M0FPB13shiftright128(_M0L4mid4S983, _M0L3hi4S984, _M0L6_2atmpS2834);
  }
  _M0L6_2atmpS2844 = _M0L1jS973 - 64;
  _M0L6_2atmpS2843 = _M0L6_2atmpS2844 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS985
  = _M0FPB13shiftright128(_M0L3midS967, _M0L2hiS968, _M0L6_2atmpS2843);
  _M0L6_2atmpS2842 = _M0Lm2vmS974;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS985,
                                                _M0L2vpS972,
                                                _M0L6_2atmpS2842};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS954,
  int32_t _M0L1pS955
) {
  uint64_t _M0L6_2atmpS2828;
  uint64_t _M0L6_2atmpS2827;
  uint64_t _M0L6_2atmpS2826;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2828 = 1ull << (_M0L1pS955 & 63);
  _M0L6_2atmpS2827 = _M0L6_2atmpS2828 - 1ull;
  _M0L6_2atmpS2826 = _M0L5valueS954 & _M0L6_2atmpS2827;
  return _M0L6_2atmpS2826 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS952,
  int32_t _M0L1pS953
) {
  int32_t _M0L6_2atmpS2825;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2825 = _M0FPB10pow5Factor(_M0L5valueS952);
  return _M0L6_2atmpS2825 >= _M0L1pS953;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS948) {
  uint64_t _M0L6_2atmpS2813;
  uint64_t _M0L6_2atmpS2814;
  uint64_t _M0L6_2atmpS2815;
  uint64_t _M0L6_2atmpS2816;
  int32_t _M0Lm5countS949;
  uint64_t _M0Lm5valueS950;
  uint64_t _M0L6_2atmpS2824;
  moonbit_string_t _M0L6_2atmpS2823;
  moonbit_string_t _M0L6_2atmpS4471;
  moonbit_string_t _M0L6_2atmpS2822;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2813 = _M0L5valueS948 % 5ull;
  if (_M0L6_2atmpS2813 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2814 = _M0L5valueS948 % 25ull;
  if (_M0L6_2atmpS2814 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2815 = _M0L5valueS948 % 125ull;
  if (_M0L6_2atmpS2815 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2816 = _M0L5valueS948 % 625ull;
  if (_M0L6_2atmpS2816 != 0ull) {
    return 3;
  }
  _M0Lm5countS949 = 4;
  _M0Lm5valueS950 = _M0L5valueS948 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2817 = _M0Lm5valueS950;
    if (_M0L6_2atmpS2817 > 0ull) {
      uint64_t _M0L6_2atmpS2819 = _M0Lm5valueS950;
      uint64_t _M0L6_2atmpS2818 = _M0L6_2atmpS2819 % 5ull;
      uint64_t _M0L6_2atmpS2820;
      int32_t _M0L6_2atmpS2821;
      if (_M0L6_2atmpS2818 != 0ull) {
        return _M0Lm5countS949;
      }
      _M0L6_2atmpS2820 = _M0Lm5valueS950;
      _M0Lm5valueS950 = _M0L6_2atmpS2820 / 5ull;
      _M0L6_2atmpS2821 = _M0Lm5countS949;
      _M0Lm5countS949 = _M0L6_2atmpS2821 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2824 = _M0Lm5valueS950;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2823
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2824);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS4471
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_78.data, _M0L6_2atmpS2823);
  moonbit_decref(_M0L6_2atmpS2823);
  _M0L6_2atmpS2822 = _M0L6_2atmpS4471;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2822, (moonbit_string_t)moonbit_string_literal_79.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS947,
  uint64_t _M0L2hiS945,
  int32_t _M0L4distS946
) {
  int32_t _M0L6_2atmpS2812;
  uint64_t _M0L6_2atmpS2810;
  uint64_t _M0L6_2atmpS2811;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2812 = 64 - _M0L4distS946;
  _M0L6_2atmpS2810 = _M0L2hiS945 << (_M0L6_2atmpS2812 & 63);
  _M0L6_2atmpS2811 = _M0L2loS947 >> (_M0L4distS946 & 63);
  return _M0L6_2atmpS2810 | _M0L6_2atmpS2811;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS935,
  uint64_t _M0L1bS938
) {
  uint64_t _M0L3aLoS934;
  uint64_t _M0L3aHiS936;
  uint64_t _M0L3bLoS937;
  uint64_t _M0L3bHiS939;
  uint64_t _M0L1xS940;
  uint64_t _M0L6_2atmpS2808;
  uint64_t _M0L6_2atmpS2809;
  uint64_t _M0L1yS941;
  uint64_t _M0L6_2atmpS2806;
  uint64_t _M0L6_2atmpS2807;
  uint64_t _M0L1zS942;
  uint64_t _M0L6_2atmpS2804;
  uint64_t _M0L6_2atmpS2805;
  uint64_t _M0L6_2atmpS2802;
  uint64_t _M0L6_2atmpS2803;
  uint64_t _M0L1wS943;
  uint64_t _M0L2loS944;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS934 = _M0L1aS935 & 4294967295ull;
  _M0L3aHiS936 = _M0L1aS935 >> 32;
  _M0L3bLoS937 = _M0L1bS938 & 4294967295ull;
  _M0L3bHiS939 = _M0L1bS938 >> 32;
  _M0L1xS940 = _M0L3aLoS934 * _M0L3bLoS937;
  _M0L6_2atmpS2808 = _M0L3aHiS936 * _M0L3bLoS937;
  _M0L6_2atmpS2809 = _M0L1xS940 >> 32;
  _M0L1yS941 = _M0L6_2atmpS2808 + _M0L6_2atmpS2809;
  _M0L6_2atmpS2806 = _M0L3aLoS934 * _M0L3bHiS939;
  _M0L6_2atmpS2807 = _M0L1yS941 & 4294967295ull;
  _M0L1zS942 = _M0L6_2atmpS2806 + _M0L6_2atmpS2807;
  _M0L6_2atmpS2804 = _M0L3aHiS936 * _M0L3bHiS939;
  _M0L6_2atmpS2805 = _M0L1yS941 >> 32;
  _M0L6_2atmpS2802 = _M0L6_2atmpS2804 + _M0L6_2atmpS2805;
  _M0L6_2atmpS2803 = _M0L1zS942 >> 32;
  _M0L1wS943 = _M0L6_2atmpS2802 + _M0L6_2atmpS2803;
  _M0L2loS944 = _M0L1aS935 * _M0L1bS938;
  return (struct _M0TPB7Umul128){_M0L2loS944, _M0L1wS943};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS929,
  int32_t _M0L4fromS933,
  int32_t _M0L2toS931
) {
  int32_t _M0L6_2atmpS2801;
  struct _M0TPB13StringBuilder* _M0L3bufS928;
  int32_t _M0L1iS930;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2801 = Moonbit_array_length(_M0L5bytesS929);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS928 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2801);
  _M0L1iS930 = _M0L4fromS933;
  while (1) {
    if (_M0L1iS930 < _M0L2toS931) {
      int32_t _M0L6_2atmpS2799;
      int32_t _M0L6_2atmpS2798;
      int32_t _M0L6_2atmpS2800;
      if (
        _M0L1iS930 < 0 || _M0L1iS930 >= Moonbit_array_length(_M0L5bytesS929)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2799 = (int32_t)_M0L5bytesS929[_M0L1iS930];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2798 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2799);
      moonbit_incref(_M0L3bufS928);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS928, _M0L6_2atmpS2798);
      _M0L6_2atmpS2800 = _M0L1iS930 + 1;
      _M0L1iS930 = _M0L6_2atmpS2800;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS929);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS928);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS927) {
  int32_t _M0L6_2atmpS2797;
  uint32_t _M0L6_2atmpS2796;
  uint32_t _M0L6_2atmpS2795;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2797 = _M0L1eS927 * 78913;
  _M0L6_2atmpS2796 = *(uint32_t*)&_M0L6_2atmpS2797;
  _M0L6_2atmpS2795 = _M0L6_2atmpS2796 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2795;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS926) {
  int32_t _M0L6_2atmpS2794;
  uint32_t _M0L6_2atmpS2793;
  uint32_t _M0L6_2atmpS2792;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2794 = _M0L1eS926 * 732923;
  _M0L6_2atmpS2793 = *(uint32_t*)&_M0L6_2atmpS2794;
  _M0L6_2atmpS2792 = _M0L6_2atmpS2793 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2792;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS924,
  int32_t _M0L8exponentS925,
  int32_t _M0L8mantissaS922
) {
  moonbit_string_t _M0L1sS923;
  moonbit_string_t _M0L6_2atmpS4472;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS922) {
    return (moonbit_string_t)moonbit_string_literal_80.data;
  }
  if (_M0L4signS924) {
    _M0L1sS923 = (moonbit_string_t)moonbit_string_literal_81.data;
  } else {
    _M0L1sS923 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS925) {
    moonbit_string_t _M0L6_2atmpS4473;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS4473
    = moonbit_add_string(_M0L1sS923, (moonbit_string_t)moonbit_string_literal_82.data);
    moonbit_decref(_M0L1sS923);
    return _M0L6_2atmpS4473;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS4472
  = moonbit_add_string(_M0L1sS923, (moonbit_string_t)moonbit_string_literal_83.data);
  moonbit_decref(_M0L1sS923);
  return _M0L6_2atmpS4472;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS921) {
  int32_t _M0L6_2atmpS2791;
  uint32_t _M0L6_2atmpS2790;
  uint32_t _M0L6_2atmpS2789;
  int32_t _M0L6_2atmpS2788;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2791 = _M0L1eS921 * 1217359;
  _M0L6_2atmpS2790 = *(uint32_t*)&_M0L6_2atmpS2791;
  _M0L6_2atmpS2789 = _M0L6_2atmpS2790 >> 19;
  _M0L6_2atmpS2788 = *(int32_t*)&_M0L6_2atmpS2789;
  return _M0L6_2atmpS2788 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS920,
  struct _M0TPB6Hasher* _M0L6hasherS919
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS919, _M0L4selfS920);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS918,
  struct _M0TPB6Hasher* _M0L6hasherS917
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS917, _M0L4selfS918);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS915,
  moonbit_string_t _M0L5valueS913
) {
  int32_t _M0L7_2abindS912;
  int32_t _M0L1iS914;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS912 = Moonbit_array_length(_M0L5valueS913);
  _M0L1iS914 = 0;
  while (1) {
    if (_M0L1iS914 < _M0L7_2abindS912) {
      int32_t _M0L6_2atmpS2786 = _M0L5valueS913[_M0L1iS914];
      int32_t _M0L6_2atmpS2785 = (int32_t)_M0L6_2atmpS2786;
      uint32_t _M0L6_2atmpS2784 = *(uint32_t*)&_M0L6_2atmpS2785;
      int32_t _M0L6_2atmpS2787;
      moonbit_incref(_M0L4selfS915);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS915, _M0L6_2atmpS2784);
      _M0L6_2atmpS2787 = _M0L1iS914 + 1;
      _M0L1iS914 = _M0L6_2atmpS2787;
      continue;
    } else {
      moonbit_decref(_M0L4selfS915);
      moonbit_decref(_M0L5valueS913);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS910,
  int32_t _M0L3idxS911
) {
  int32_t _M0L6_2atmpS4474;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4474 = _M0L4selfS910[_M0L3idxS911];
  moonbit_decref(_M0L4selfS910);
  return _M0L6_2atmpS4474;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS909) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS909;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS908) {
  void* _block_5068;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_5068 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_5068)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_5068)->$0 = _M0L6stringS908;
  return _block_5068;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS901
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4475;
  int32_t _M0L6_2acntS4795;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2783;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS900;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__* _closure_5069;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2778;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4475 = _M0L4selfS901->$5;
  _M0L6_2acntS4795 = Moonbit_object_header(_M0L4selfS901)->rc;
  if (_M0L6_2acntS4795 > 1) {
    int32_t _M0L11_2anew__cntS4797 = _M0L6_2acntS4795 - 1;
    Moonbit_object_header(_M0L4selfS901)->rc = _M0L11_2anew__cntS4797;
    if (_M0L8_2afieldS4475) {
      moonbit_incref(_M0L8_2afieldS4475);
    }
  } else if (_M0L6_2acntS4795 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4796 = _M0L4selfS901->$0;
    moonbit_decref(_M0L8_2afieldS4796);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS901);
  }
  _M0L4headS2783 = _M0L8_2afieldS4475;
  _M0L11curr__entryS900
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS900)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS900->$0 = _M0L4headS2783;
  _closure_5069
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__));
  Moonbit_object_header(_closure_5069)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__, $0) >> 2, 1, 0);
  _closure_5069->code = &_M0MPB3Map4iterGsRPB4JsonEC2779l591;
  _closure_5069->$0 = _M0L11curr__entryS900;
  _M0L6_2atmpS2778 = (struct _M0TWEOUsRPB4JsonE*)_closure_5069;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2778);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2779l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2780
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__* _M0L14_2acasted__envS2781;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS4481;
  int32_t _M0L6_2acntS4798;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS900;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4480;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS902;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2781
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2779__l591__*)_M0L6_2aenvS2780;
  _M0L8_2afieldS4481 = _M0L14_2acasted__envS2781->$0;
  _M0L6_2acntS4798 = Moonbit_object_header(_M0L14_2acasted__envS2781)->rc;
  if (_M0L6_2acntS4798 > 1) {
    int32_t _M0L11_2anew__cntS4799 = _M0L6_2acntS4798 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2781)->rc
    = _M0L11_2anew__cntS4799;
    moonbit_incref(_M0L8_2afieldS4481);
  } else if (_M0L6_2acntS4798 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2781);
  }
  _M0L11curr__entryS900 = _M0L8_2afieldS4481;
  _M0L8_2afieldS4480 = _M0L11curr__entryS900->$0;
  _M0L7_2abindS902 = _M0L8_2afieldS4480;
  if (_M0L7_2abindS902 == 0) {
    moonbit_decref(_M0L11curr__entryS900);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS903 = _M0L7_2abindS902;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS904 = _M0L7_2aSomeS903;
    moonbit_string_t _M0L8_2afieldS4479 = _M0L4_2axS904->$4;
    moonbit_string_t _M0L6_2akeyS905 = _M0L8_2afieldS4479;
    void* _M0L8_2afieldS4478 = _M0L4_2axS904->$5;
    void* _M0L8_2avalueS906 = _M0L8_2afieldS4478;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4477 = _M0L4_2axS904->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS907 = _M0L8_2afieldS4477;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4476 =
      _M0L11curr__entryS900->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2782;
    if (_M0L7_2anextS907) {
      moonbit_incref(_M0L7_2anextS907);
    }
    moonbit_incref(_M0L8_2avalueS906);
    moonbit_incref(_M0L6_2akeyS905);
    if (_M0L6_2aoldS4476) {
      moonbit_decref(_M0L6_2aoldS4476);
    }
    _M0L11curr__entryS900->$0 = _M0L7_2anextS907;
    moonbit_decref(_M0L11curr__entryS900);
    _M0L8_2atupleS2782
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2782)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2782->$0 = _M0L6_2akeyS905;
    _M0L8_2atupleS2782->$1 = _M0L8_2avalueS906;
    return _M0L8_2atupleS2782;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS899
) {
  int32_t _M0L8_2afieldS4482;
  int32_t _M0L4sizeS2777;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4482 = _M0L4selfS899->$1;
  moonbit_decref(_M0L4selfS899);
  _M0L4sizeS2777 = _M0L8_2afieldS4482;
  return _M0L4sizeS2777 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS886,
  int32_t _M0L3keyS882
) {
  int32_t _M0L4hashS881;
  int32_t _M0L14capacity__maskS2762;
  int32_t _M0L6_2atmpS2761;
  int32_t _M0L1iS883;
  int32_t _M0L3idxS884;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS881 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS882);
  _M0L14capacity__maskS2762 = _M0L4selfS886->$3;
  _M0L6_2atmpS2761 = _M0L4hashS881 & _M0L14capacity__maskS2762;
  _M0L1iS883 = 0;
  _M0L3idxS884 = _M0L6_2atmpS2761;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4486 =
      _M0L4selfS886->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2760 =
      _M0L8_2afieldS4486;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4485;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS885;
    if (
      _M0L3idxS884 < 0
      || _M0L3idxS884 >= Moonbit_array_length(_M0L7entriesS2760)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4485
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2760[
        _M0L3idxS884
      ];
    _M0L7_2abindS885 = _M0L6_2atmpS4485;
    if (_M0L7_2abindS885 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2749;
      if (_M0L7_2abindS885) {
        moonbit_incref(_M0L7_2abindS885);
      }
      moonbit_decref(_M0L4selfS886);
      if (_M0L7_2abindS885) {
        moonbit_decref(_M0L7_2abindS885);
      }
      _M0L6_2atmpS2749 = 0;
      return _M0L6_2atmpS2749;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS887 =
        _M0L7_2abindS885;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS888 =
        _M0L7_2aSomeS887;
      int32_t _M0L4hashS2751 = _M0L8_2aentryS888->$3;
      int32_t _if__result_5071;
      int32_t _M0L8_2afieldS4483;
      int32_t _M0L3pslS2754;
      int32_t _M0L6_2atmpS2756;
      int32_t _M0L6_2atmpS2758;
      int32_t _M0L14capacity__maskS2759;
      int32_t _M0L6_2atmpS2757;
      if (_M0L4hashS2751 == _M0L4hashS881) {
        int32_t _M0L3keyS2750 = _M0L8_2aentryS888->$4;
        _if__result_5071 = _M0L3keyS2750 == _M0L3keyS882;
      } else {
        _if__result_5071 = 0;
      }
      if (_if__result_5071) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4484;
        int32_t _M0L6_2acntS4800;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2753;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2752;
        moonbit_incref(_M0L8_2aentryS888);
        moonbit_decref(_M0L4selfS886);
        _M0L8_2afieldS4484 = _M0L8_2aentryS888->$5;
        _M0L6_2acntS4800 = Moonbit_object_header(_M0L8_2aentryS888)->rc;
        if (_M0L6_2acntS4800 > 1) {
          int32_t _M0L11_2anew__cntS4802 = _M0L6_2acntS4800 - 1;
          Moonbit_object_header(_M0L8_2aentryS888)->rc
          = _M0L11_2anew__cntS4802;
          moonbit_incref(_M0L8_2afieldS4484);
        } else if (_M0L6_2acntS4800 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4801 =
            _M0L8_2aentryS888->$1;
          if (_M0L8_2afieldS4801) {
            moonbit_decref(_M0L8_2afieldS4801);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS888);
        }
        _M0L5valueS2753 = _M0L8_2afieldS4484;
        _M0L6_2atmpS2752 = _M0L5valueS2753;
        return _M0L6_2atmpS2752;
      } else {
        moonbit_incref(_M0L8_2aentryS888);
      }
      _M0L8_2afieldS4483 = _M0L8_2aentryS888->$2;
      moonbit_decref(_M0L8_2aentryS888);
      _M0L3pslS2754 = _M0L8_2afieldS4483;
      if (_M0L1iS883 > _M0L3pslS2754) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2755;
        moonbit_decref(_M0L4selfS886);
        _M0L6_2atmpS2755 = 0;
        return _M0L6_2atmpS2755;
      }
      _M0L6_2atmpS2756 = _M0L1iS883 + 1;
      _M0L6_2atmpS2758 = _M0L3idxS884 + 1;
      _M0L14capacity__maskS2759 = _M0L4selfS886->$3;
      _M0L6_2atmpS2757 = _M0L6_2atmpS2758 & _M0L14capacity__maskS2759;
      _M0L1iS883 = _M0L6_2atmpS2756;
      _M0L3idxS884 = _M0L6_2atmpS2757;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS895,
  moonbit_string_t _M0L3keyS891
) {
  int32_t _M0L4hashS890;
  int32_t _M0L14capacity__maskS2776;
  int32_t _M0L6_2atmpS2775;
  int32_t _M0L1iS892;
  int32_t _M0L3idxS893;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS891);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS890 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS891);
  _M0L14capacity__maskS2776 = _M0L4selfS895->$3;
  _M0L6_2atmpS2775 = _M0L4hashS890 & _M0L14capacity__maskS2776;
  _M0L1iS892 = 0;
  _M0L3idxS893 = _M0L6_2atmpS2775;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4492 =
      _M0L4selfS895->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2774 =
      _M0L8_2afieldS4492;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4491;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS894;
    if (
      _M0L3idxS893 < 0
      || _M0L3idxS893 >= Moonbit_array_length(_M0L7entriesS2774)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4491
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2774[
        _M0L3idxS893
      ];
    _M0L7_2abindS894 = _M0L6_2atmpS4491;
    if (_M0L7_2abindS894 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2763;
      if (_M0L7_2abindS894) {
        moonbit_incref(_M0L7_2abindS894);
      }
      moonbit_decref(_M0L4selfS895);
      if (_M0L7_2abindS894) {
        moonbit_decref(_M0L7_2abindS894);
      }
      moonbit_decref(_M0L3keyS891);
      _M0L6_2atmpS2763 = 0;
      return _M0L6_2atmpS2763;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS896 =
        _M0L7_2abindS894;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS897 =
        _M0L7_2aSomeS896;
      int32_t _M0L4hashS2765 = _M0L8_2aentryS897->$3;
      int32_t _if__result_5073;
      int32_t _M0L8_2afieldS4487;
      int32_t _M0L3pslS2768;
      int32_t _M0L6_2atmpS2770;
      int32_t _M0L6_2atmpS2772;
      int32_t _M0L14capacity__maskS2773;
      int32_t _M0L6_2atmpS2771;
      if (_M0L4hashS2765 == _M0L4hashS890) {
        moonbit_string_t _M0L8_2afieldS4490 = _M0L8_2aentryS897->$4;
        moonbit_string_t _M0L3keyS2764 = _M0L8_2afieldS4490;
        int32_t _M0L6_2atmpS4489;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4489
        = moonbit_val_array_equal(_M0L3keyS2764, _M0L3keyS891);
        _if__result_5073 = _M0L6_2atmpS4489;
      } else {
        _if__result_5073 = 0;
      }
      if (_if__result_5073) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4488;
        int32_t _M0L6_2acntS4803;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2767;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2766;
        moonbit_incref(_M0L8_2aentryS897);
        moonbit_decref(_M0L4selfS895);
        moonbit_decref(_M0L3keyS891);
        _M0L8_2afieldS4488 = _M0L8_2aentryS897->$5;
        _M0L6_2acntS4803 = Moonbit_object_header(_M0L8_2aentryS897)->rc;
        if (_M0L6_2acntS4803 > 1) {
          int32_t _M0L11_2anew__cntS4806 = _M0L6_2acntS4803 - 1;
          Moonbit_object_header(_M0L8_2aentryS897)->rc
          = _M0L11_2anew__cntS4806;
          moonbit_incref(_M0L8_2afieldS4488);
        } else if (_M0L6_2acntS4803 == 1) {
          moonbit_string_t _M0L8_2afieldS4805 = _M0L8_2aentryS897->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4804;
          moonbit_decref(_M0L8_2afieldS4805);
          _M0L8_2afieldS4804 = _M0L8_2aentryS897->$1;
          if (_M0L8_2afieldS4804) {
            moonbit_decref(_M0L8_2afieldS4804);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS897);
        }
        _M0L5valueS2767 = _M0L8_2afieldS4488;
        _M0L6_2atmpS2766 = _M0L5valueS2767;
        return _M0L6_2atmpS2766;
      } else {
        moonbit_incref(_M0L8_2aentryS897);
      }
      _M0L8_2afieldS4487 = _M0L8_2aentryS897->$2;
      moonbit_decref(_M0L8_2aentryS897);
      _M0L3pslS2768 = _M0L8_2afieldS4487;
      if (_M0L1iS892 > _M0L3pslS2768) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2769;
        moonbit_decref(_M0L4selfS895);
        moonbit_decref(_M0L3keyS891);
        _M0L6_2atmpS2769 = 0;
        return _M0L6_2atmpS2769;
      }
      _M0L6_2atmpS2770 = _M0L1iS892 + 1;
      _M0L6_2atmpS2772 = _M0L3idxS893 + 1;
      _M0L14capacity__maskS2773 = _M0L4selfS895->$3;
      _M0L6_2atmpS2771 = _M0L6_2atmpS2772 & _M0L14capacity__maskS2773;
      _M0L1iS892 = _M0L6_2atmpS2770;
      _M0L3idxS893 = _M0L6_2atmpS2771;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS866
) {
  int32_t _M0L6lengthS865;
  int32_t _M0Lm8capacityS867;
  int32_t _M0L6_2atmpS2726;
  int32_t _M0L6_2atmpS2725;
  int32_t _M0L6_2atmpS2736;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS868;
  int32_t _M0L3endS2734;
  int32_t _M0L5startS2735;
  int32_t _M0L7_2abindS869;
  int32_t _M0L2__S870;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS866.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS865
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS866);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS867 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS865);
  _M0L6_2atmpS2726 = _M0Lm8capacityS867;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2725 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2726);
  if (_M0L6lengthS865 > _M0L6_2atmpS2725) {
    int32_t _M0L6_2atmpS2727 = _M0Lm8capacityS867;
    _M0Lm8capacityS867 = _M0L6_2atmpS2727 * 2;
  }
  _M0L6_2atmpS2736 = _M0Lm8capacityS867;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS868
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2736);
  _M0L3endS2734 = _M0L3arrS866.$2;
  _M0L5startS2735 = _M0L3arrS866.$1;
  _M0L7_2abindS869 = _M0L3endS2734 - _M0L5startS2735;
  _M0L2__S870 = 0;
  while (1) {
    if (_M0L2__S870 < _M0L7_2abindS869) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4496 =
        _M0L3arrS866.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2731 =
        _M0L8_2afieldS4496;
      int32_t _M0L5startS2733 = _M0L3arrS866.$1;
      int32_t _M0L6_2atmpS2732 = _M0L5startS2733 + _M0L2__S870;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4495 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2731[
          _M0L6_2atmpS2732
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS871 =
        _M0L6_2atmpS4495;
      moonbit_string_t _M0L8_2afieldS4494 = _M0L1eS871->$0;
      moonbit_string_t _M0L6_2atmpS2728 = _M0L8_2afieldS4494;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4493 =
        _M0L1eS871->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2729 =
        _M0L8_2afieldS4493;
      int32_t _M0L6_2atmpS2730;
      moonbit_incref(_M0L6_2atmpS2729);
      moonbit_incref(_M0L6_2atmpS2728);
      moonbit_incref(_M0L1mS868);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS868, _M0L6_2atmpS2728, _M0L6_2atmpS2729);
      _M0L6_2atmpS2730 = _M0L2__S870 + 1;
      _M0L2__S870 = _M0L6_2atmpS2730;
      continue;
    } else {
      moonbit_decref(_M0L3arrS866.$0);
    }
    break;
  }
  return _M0L1mS868;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS874
) {
  int32_t _M0L6lengthS873;
  int32_t _M0Lm8capacityS875;
  int32_t _M0L6_2atmpS2738;
  int32_t _M0L6_2atmpS2737;
  int32_t _M0L6_2atmpS2748;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS876;
  int32_t _M0L3endS2746;
  int32_t _M0L5startS2747;
  int32_t _M0L7_2abindS877;
  int32_t _M0L2__S878;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS874.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS873
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS874);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS875 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS873);
  _M0L6_2atmpS2738 = _M0Lm8capacityS875;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2737 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2738);
  if (_M0L6lengthS873 > _M0L6_2atmpS2737) {
    int32_t _M0L6_2atmpS2739 = _M0Lm8capacityS875;
    _M0Lm8capacityS875 = _M0L6_2atmpS2739 * 2;
  }
  _M0L6_2atmpS2748 = _M0Lm8capacityS875;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS876
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2748);
  _M0L3endS2746 = _M0L3arrS874.$2;
  _M0L5startS2747 = _M0L3arrS874.$1;
  _M0L7_2abindS877 = _M0L3endS2746 - _M0L5startS2747;
  _M0L2__S878 = 0;
  while (1) {
    if (_M0L2__S878 < _M0L7_2abindS877) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4499 =
        _M0L3arrS874.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2743 =
        _M0L8_2afieldS4499;
      int32_t _M0L5startS2745 = _M0L3arrS874.$1;
      int32_t _M0L6_2atmpS2744 = _M0L5startS2745 + _M0L2__S878;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4498 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2743[
          _M0L6_2atmpS2744
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS879 = _M0L6_2atmpS4498;
      int32_t _M0L6_2atmpS2740 = _M0L1eS879->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4497 =
        _M0L1eS879->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2741 =
        _M0L8_2afieldS4497;
      int32_t _M0L6_2atmpS2742;
      moonbit_incref(_M0L6_2atmpS2741);
      moonbit_incref(_M0L1mS876);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS876, _M0L6_2atmpS2740, _M0L6_2atmpS2741);
      _M0L6_2atmpS2742 = _M0L2__S878 + 1;
      _M0L2__S878 = _M0L6_2atmpS2742;
      continue;
    } else {
      moonbit_decref(_M0L3arrS874.$0);
    }
    break;
  }
  return _M0L1mS876;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS859,
  moonbit_string_t _M0L3keyS860,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS861
) {
  int32_t _M0L6_2atmpS2723;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS860);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2723 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS860);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS859, _M0L3keyS860, _M0L5valueS861, _M0L6_2atmpS2723);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS862,
  int32_t _M0L3keyS863,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS864
) {
  int32_t _M0L6_2atmpS2724;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2724 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS863);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS862, _M0L3keyS863, _M0L5valueS864, _M0L6_2atmpS2724);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS838
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4506;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS837;
  int32_t _M0L8capacityS2715;
  int32_t _M0L13new__capacityS839;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2710;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2709;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS4505;
  int32_t _M0L6_2atmpS2711;
  int32_t _M0L8capacityS2713;
  int32_t _M0L6_2atmpS2712;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2714;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4504;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS840;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4506 = _M0L4selfS838->$5;
  _M0L9old__headS837 = _M0L8_2afieldS4506;
  _M0L8capacityS2715 = _M0L4selfS838->$2;
  _M0L13new__capacityS839 = _M0L8capacityS2715 << 1;
  _M0L6_2atmpS2710 = 0;
  _M0L6_2atmpS2709
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS839, _M0L6_2atmpS2710);
  _M0L6_2aoldS4505 = _M0L4selfS838->$0;
  if (_M0L9old__headS837) {
    moonbit_incref(_M0L9old__headS837);
  }
  moonbit_decref(_M0L6_2aoldS4505);
  _M0L4selfS838->$0 = _M0L6_2atmpS2709;
  _M0L4selfS838->$2 = _M0L13new__capacityS839;
  _M0L6_2atmpS2711 = _M0L13new__capacityS839 - 1;
  _M0L4selfS838->$3 = _M0L6_2atmpS2711;
  _M0L8capacityS2713 = _M0L4selfS838->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2712 = _M0FPB21calc__grow__threshold(_M0L8capacityS2713);
  _M0L4selfS838->$4 = _M0L6_2atmpS2712;
  _M0L4selfS838->$1 = 0;
  _M0L6_2atmpS2714 = 0;
  _M0L6_2aoldS4504 = _M0L4selfS838->$5;
  if (_M0L6_2aoldS4504) {
    moonbit_decref(_M0L6_2aoldS4504);
  }
  _M0L4selfS838->$5 = _M0L6_2atmpS2714;
  _M0L4selfS838->$6 = -1;
  _M0L8_2aparamS840 = _M0L9old__headS837;
  while (1) {
    if (_M0L8_2aparamS840 == 0) {
      if (_M0L8_2aparamS840) {
        moonbit_decref(_M0L8_2aparamS840);
      }
      moonbit_decref(_M0L4selfS838);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS841 =
        _M0L8_2aparamS840;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS842 =
        _M0L7_2aSomeS841;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4503 =
        _M0L4_2axS842->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS843 =
        _M0L8_2afieldS4503;
      moonbit_string_t _M0L8_2afieldS4502 = _M0L4_2axS842->$4;
      moonbit_string_t _M0L6_2akeyS844 = _M0L8_2afieldS4502;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4501 =
        _M0L4_2axS842->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS845 =
        _M0L8_2afieldS4501;
      int32_t _M0L8_2afieldS4500 = _M0L4_2axS842->$3;
      int32_t _M0L6_2acntS4807 = Moonbit_object_header(_M0L4_2axS842)->rc;
      int32_t _M0L7_2ahashS846;
      if (_M0L6_2acntS4807 > 1) {
        int32_t _M0L11_2anew__cntS4808 = _M0L6_2acntS4807 - 1;
        Moonbit_object_header(_M0L4_2axS842)->rc = _M0L11_2anew__cntS4808;
        moonbit_incref(_M0L8_2avalueS845);
        moonbit_incref(_M0L6_2akeyS844);
        if (_M0L7_2anextS843) {
          moonbit_incref(_M0L7_2anextS843);
        }
      } else if (_M0L6_2acntS4807 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS842);
      }
      _M0L7_2ahashS846 = _M0L8_2afieldS4500;
      moonbit_incref(_M0L4selfS838);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS838, _M0L6_2akeyS844, _M0L8_2avalueS845, _M0L7_2ahashS846);
      _M0L8_2aparamS840 = _M0L7_2anextS843;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS849
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4512;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS848;
  int32_t _M0L8capacityS2722;
  int32_t _M0L13new__capacityS850;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2717;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2716;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4511;
  int32_t _M0L6_2atmpS2718;
  int32_t _M0L8capacityS2720;
  int32_t _M0L6_2atmpS2719;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2721;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4510;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS851;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4512 = _M0L4selfS849->$5;
  _M0L9old__headS848 = _M0L8_2afieldS4512;
  _M0L8capacityS2722 = _M0L4selfS849->$2;
  _M0L13new__capacityS850 = _M0L8capacityS2722 << 1;
  _M0L6_2atmpS2717 = 0;
  _M0L6_2atmpS2716
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS850, _M0L6_2atmpS2717);
  _M0L6_2aoldS4511 = _M0L4selfS849->$0;
  if (_M0L9old__headS848) {
    moonbit_incref(_M0L9old__headS848);
  }
  moonbit_decref(_M0L6_2aoldS4511);
  _M0L4selfS849->$0 = _M0L6_2atmpS2716;
  _M0L4selfS849->$2 = _M0L13new__capacityS850;
  _M0L6_2atmpS2718 = _M0L13new__capacityS850 - 1;
  _M0L4selfS849->$3 = _M0L6_2atmpS2718;
  _M0L8capacityS2720 = _M0L4selfS849->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2719 = _M0FPB21calc__grow__threshold(_M0L8capacityS2720);
  _M0L4selfS849->$4 = _M0L6_2atmpS2719;
  _M0L4selfS849->$1 = 0;
  _M0L6_2atmpS2721 = 0;
  _M0L6_2aoldS4510 = _M0L4selfS849->$5;
  if (_M0L6_2aoldS4510) {
    moonbit_decref(_M0L6_2aoldS4510);
  }
  _M0L4selfS849->$5 = _M0L6_2atmpS2721;
  _M0L4selfS849->$6 = -1;
  _M0L8_2aparamS851 = _M0L9old__headS848;
  while (1) {
    if (_M0L8_2aparamS851 == 0) {
      if (_M0L8_2aparamS851) {
        moonbit_decref(_M0L8_2aparamS851);
      }
      moonbit_decref(_M0L4selfS849);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS852 =
        _M0L8_2aparamS851;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS853 =
        _M0L7_2aSomeS852;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4509 =
        _M0L4_2axS853->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS854 =
        _M0L8_2afieldS4509;
      int32_t _M0L6_2akeyS855 = _M0L4_2axS853->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4508 =
        _M0L4_2axS853->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS856 =
        _M0L8_2afieldS4508;
      int32_t _M0L8_2afieldS4507 = _M0L4_2axS853->$3;
      int32_t _M0L6_2acntS4809 = Moonbit_object_header(_M0L4_2axS853)->rc;
      int32_t _M0L7_2ahashS857;
      if (_M0L6_2acntS4809 > 1) {
        int32_t _M0L11_2anew__cntS4810 = _M0L6_2acntS4809 - 1;
        Moonbit_object_header(_M0L4_2axS853)->rc = _M0L11_2anew__cntS4810;
        moonbit_incref(_M0L8_2avalueS856);
        if (_M0L7_2anextS854) {
          moonbit_incref(_M0L7_2anextS854);
        }
      } else if (_M0L6_2acntS4809 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS853);
      }
      _M0L7_2ahashS857 = _M0L8_2afieldS4507;
      moonbit_incref(_M0L4selfS849);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS849, _M0L6_2akeyS855, _M0L8_2avalueS856, _M0L7_2ahashS857);
      _M0L8_2aparamS851 = _M0L7_2anextS854;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS808,
  moonbit_string_t _M0L3keyS814,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS815,
  int32_t _M0L4hashS810
) {
  int32_t _M0L14capacity__maskS2690;
  int32_t _M0L6_2atmpS2689;
  int32_t _M0L3pslS805;
  int32_t _M0L3idxS806;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2690 = _M0L4selfS808->$3;
  _M0L6_2atmpS2689 = _M0L4hashS810 & _M0L14capacity__maskS2690;
  _M0L3pslS805 = 0;
  _M0L3idxS806 = _M0L6_2atmpS2689;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4517 =
      _M0L4selfS808->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2688 =
      _M0L8_2afieldS4517;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4516;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS807;
    if (
      _M0L3idxS806 < 0
      || _M0L3idxS806 >= Moonbit_array_length(_M0L7entriesS2688)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4516
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2688[
        _M0L3idxS806
      ];
    _M0L7_2abindS807 = _M0L6_2atmpS4516;
    if (_M0L7_2abindS807 == 0) {
      int32_t _M0L4sizeS2673 = _M0L4selfS808->$1;
      int32_t _M0L8grow__atS2674 = _M0L4selfS808->$4;
      int32_t _M0L7_2abindS811;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS812;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS813;
      if (_M0L4sizeS2673 >= _M0L8grow__atS2674) {
        int32_t _M0L14capacity__maskS2676;
        int32_t _M0L6_2atmpS2675;
        moonbit_incref(_M0L4selfS808);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS808);
        _M0L14capacity__maskS2676 = _M0L4selfS808->$3;
        _M0L6_2atmpS2675 = _M0L4hashS810 & _M0L14capacity__maskS2676;
        _M0L3pslS805 = 0;
        _M0L3idxS806 = _M0L6_2atmpS2675;
        continue;
      }
      _M0L7_2abindS811 = _M0L4selfS808->$6;
      _M0L7_2abindS812 = 0;
      _M0L5entryS813
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS813)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS813->$0 = _M0L7_2abindS811;
      _M0L5entryS813->$1 = _M0L7_2abindS812;
      _M0L5entryS813->$2 = _M0L3pslS805;
      _M0L5entryS813->$3 = _M0L4hashS810;
      _M0L5entryS813->$4 = _M0L3keyS814;
      _M0L5entryS813->$5 = _M0L5valueS815;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS808, _M0L3idxS806, _M0L5entryS813);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS816 =
        _M0L7_2abindS807;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS817 =
        _M0L7_2aSomeS816;
      int32_t _M0L4hashS2678 = _M0L14_2acurr__entryS817->$3;
      int32_t _if__result_5079;
      int32_t _M0L3pslS2679;
      int32_t _M0L6_2atmpS2684;
      int32_t _M0L6_2atmpS2686;
      int32_t _M0L14capacity__maskS2687;
      int32_t _M0L6_2atmpS2685;
      if (_M0L4hashS2678 == _M0L4hashS810) {
        moonbit_string_t _M0L8_2afieldS4515 = _M0L14_2acurr__entryS817->$4;
        moonbit_string_t _M0L3keyS2677 = _M0L8_2afieldS4515;
        int32_t _M0L6_2atmpS4514;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4514
        = moonbit_val_array_equal(_M0L3keyS2677, _M0L3keyS814);
        _if__result_5079 = _M0L6_2atmpS4514;
      } else {
        _if__result_5079 = 0;
      }
      if (_if__result_5079) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4513;
        moonbit_incref(_M0L14_2acurr__entryS817);
        moonbit_decref(_M0L3keyS814);
        moonbit_decref(_M0L4selfS808);
        _M0L6_2aoldS4513 = _M0L14_2acurr__entryS817->$5;
        moonbit_decref(_M0L6_2aoldS4513);
        _M0L14_2acurr__entryS817->$5 = _M0L5valueS815;
        moonbit_decref(_M0L14_2acurr__entryS817);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS817);
      }
      _M0L3pslS2679 = _M0L14_2acurr__entryS817->$2;
      if (_M0L3pslS805 > _M0L3pslS2679) {
        int32_t _M0L4sizeS2680 = _M0L4selfS808->$1;
        int32_t _M0L8grow__atS2681 = _M0L4selfS808->$4;
        int32_t _M0L7_2abindS818;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS819;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS820;
        if (_M0L4sizeS2680 >= _M0L8grow__atS2681) {
          int32_t _M0L14capacity__maskS2683;
          int32_t _M0L6_2atmpS2682;
          moonbit_decref(_M0L14_2acurr__entryS817);
          moonbit_incref(_M0L4selfS808);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS808);
          _M0L14capacity__maskS2683 = _M0L4selfS808->$3;
          _M0L6_2atmpS2682 = _M0L4hashS810 & _M0L14capacity__maskS2683;
          _M0L3pslS805 = 0;
          _M0L3idxS806 = _M0L6_2atmpS2682;
          continue;
        }
        moonbit_incref(_M0L4selfS808);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS808, _M0L3idxS806, _M0L14_2acurr__entryS817);
        _M0L7_2abindS818 = _M0L4selfS808->$6;
        _M0L7_2abindS819 = 0;
        _M0L5entryS820
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS820)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS820->$0 = _M0L7_2abindS818;
        _M0L5entryS820->$1 = _M0L7_2abindS819;
        _M0L5entryS820->$2 = _M0L3pslS805;
        _M0L5entryS820->$3 = _M0L4hashS810;
        _M0L5entryS820->$4 = _M0L3keyS814;
        _M0L5entryS820->$5 = _M0L5valueS815;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS808, _M0L3idxS806, _M0L5entryS820);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS817);
      }
      _M0L6_2atmpS2684 = _M0L3pslS805 + 1;
      _M0L6_2atmpS2686 = _M0L3idxS806 + 1;
      _M0L14capacity__maskS2687 = _M0L4selfS808->$3;
      _M0L6_2atmpS2685 = _M0L6_2atmpS2686 & _M0L14capacity__maskS2687;
      _M0L3pslS805 = _M0L6_2atmpS2684;
      _M0L3idxS806 = _M0L6_2atmpS2685;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS824,
  int32_t _M0L3keyS830,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS831,
  int32_t _M0L4hashS826
) {
  int32_t _M0L14capacity__maskS2708;
  int32_t _M0L6_2atmpS2707;
  int32_t _M0L3pslS821;
  int32_t _M0L3idxS822;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2708 = _M0L4selfS824->$3;
  _M0L6_2atmpS2707 = _M0L4hashS826 & _M0L14capacity__maskS2708;
  _M0L3pslS821 = 0;
  _M0L3idxS822 = _M0L6_2atmpS2707;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4520 =
      _M0L4selfS824->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2706 =
      _M0L8_2afieldS4520;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4519;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS823;
    if (
      _M0L3idxS822 < 0
      || _M0L3idxS822 >= Moonbit_array_length(_M0L7entriesS2706)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4519
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2706[
        _M0L3idxS822
      ];
    _M0L7_2abindS823 = _M0L6_2atmpS4519;
    if (_M0L7_2abindS823 == 0) {
      int32_t _M0L4sizeS2691 = _M0L4selfS824->$1;
      int32_t _M0L8grow__atS2692 = _M0L4selfS824->$4;
      int32_t _M0L7_2abindS827;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS828;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS829;
      if (_M0L4sizeS2691 >= _M0L8grow__atS2692) {
        int32_t _M0L14capacity__maskS2694;
        int32_t _M0L6_2atmpS2693;
        moonbit_incref(_M0L4selfS824);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS824);
        _M0L14capacity__maskS2694 = _M0L4selfS824->$3;
        _M0L6_2atmpS2693 = _M0L4hashS826 & _M0L14capacity__maskS2694;
        _M0L3pslS821 = 0;
        _M0L3idxS822 = _M0L6_2atmpS2693;
        continue;
      }
      _M0L7_2abindS827 = _M0L4selfS824->$6;
      _M0L7_2abindS828 = 0;
      _M0L5entryS829
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS829)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS829->$0 = _M0L7_2abindS827;
      _M0L5entryS829->$1 = _M0L7_2abindS828;
      _M0L5entryS829->$2 = _M0L3pslS821;
      _M0L5entryS829->$3 = _M0L4hashS826;
      _M0L5entryS829->$4 = _M0L3keyS830;
      _M0L5entryS829->$5 = _M0L5valueS831;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS824, _M0L3idxS822, _M0L5entryS829);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS832 =
        _M0L7_2abindS823;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS833 =
        _M0L7_2aSomeS832;
      int32_t _M0L4hashS2696 = _M0L14_2acurr__entryS833->$3;
      int32_t _if__result_5081;
      int32_t _M0L3pslS2697;
      int32_t _M0L6_2atmpS2702;
      int32_t _M0L6_2atmpS2704;
      int32_t _M0L14capacity__maskS2705;
      int32_t _M0L6_2atmpS2703;
      if (_M0L4hashS2696 == _M0L4hashS826) {
        int32_t _M0L3keyS2695 = _M0L14_2acurr__entryS833->$4;
        _if__result_5081 = _M0L3keyS2695 == _M0L3keyS830;
      } else {
        _if__result_5081 = 0;
      }
      if (_if__result_5081) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4518;
        moonbit_incref(_M0L14_2acurr__entryS833);
        moonbit_decref(_M0L4selfS824);
        _M0L6_2aoldS4518 = _M0L14_2acurr__entryS833->$5;
        moonbit_decref(_M0L6_2aoldS4518);
        _M0L14_2acurr__entryS833->$5 = _M0L5valueS831;
        moonbit_decref(_M0L14_2acurr__entryS833);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS833);
      }
      _M0L3pslS2697 = _M0L14_2acurr__entryS833->$2;
      if (_M0L3pslS821 > _M0L3pslS2697) {
        int32_t _M0L4sizeS2698 = _M0L4selfS824->$1;
        int32_t _M0L8grow__atS2699 = _M0L4selfS824->$4;
        int32_t _M0L7_2abindS834;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS835;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS836;
        if (_M0L4sizeS2698 >= _M0L8grow__atS2699) {
          int32_t _M0L14capacity__maskS2701;
          int32_t _M0L6_2atmpS2700;
          moonbit_decref(_M0L14_2acurr__entryS833);
          moonbit_incref(_M0L4selfS824);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS824);
          _M0L14capacity__maskS2701 = _M0L4selfS824->$3;
          _M0L6_2atmpS2700 = _M0L4hashS826 & _M0L14capacity__maskS2701;
          _M0L3pslS821 = 0;
          _M0L3idxS822 = _M0L6_2atmpS2700;
          continue;
        }
        moonbit_incref(_M0L4selfS824);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS824, _M0L3idxS822, _M0L14_2acurr__entryS833);
        _M0L7_2abindS834 = _M0L4selfS824->$6;
        _M0L7_2abindS835 = 0;
        _M0L5entryS836
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS836)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS836->$0 = _M0L7_2abindS834;
        _M0L5entryS836->$1 = _M0L7_2abindS835;
        _M0L5entryS836->$2 = _M0L3pslS821;
        _M0L5entryS836->$3 = _M0L4hashS826;
        _M0L5entryS836->$4 = _M0L3keyS830;
        _M0L5entryS836->$5 = _M0L5valueS831;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS824, _M0L3idxS822, _M0L5entryS836);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS833);
      }
      _M0L6_2atmpS2702 = _M0L3pslS821 + 1;
      _M0L6_2atmpS2704 = _M0L3idxS822 + 1;
      _M0L14capacity__maskS2705 = _M0L4selfS824->$3;
      _M0L6_2atmpS2703 = _M0L6_2atmpS2704 & _M0L14capacity__maskS2705;
      _M0L3pslS821 = _M0L6_2atmpS2702;
      _M0L3idxS822 = _M0L6_2atmpS2703;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS789,
  int32_t _M0L3idxS794,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS793
) {
  int32_t _M0L3pslS2656;
  int32_t _M0L6_2atmpS2652;
  int32_t _M0L6_2atmpS2654;
  int32_t _M0L14capacity__maskS2655;
  int32_t _M0L6_2atmpS2653;
  int32_t _M0L3pslS785;
  int32_t _M0L3idxS786;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS787;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2656 = _M0L5entryS793->$2;
  _M0L6_2atmpS2652 = _M0L3pslS2656 + 1;
  _M0L6_2atmpS2654 = _M0L3idxS794 + 1;
  _M0L14capacity__maskS2655 = _M0L4selfS789->$3;
  _M0L6_2atmpS2653 = _M0L6_2atmpS2654 & _M0L14capacity__maskS2655;
  _M0L3pslS785 = _M0L6_2atmpS2652;
  _M0L3idxS786 = _M0L6_2atmpS2653;
  _M0L5entryS787 = _M0L5entryS793;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4522 =
      _M0L4selfS789->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2651 =
      _M0L8_2afieldS4522;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4521;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS788;
    if (
      _M0L3idxS786 < 0
      || _M0L3idxS786 >= Moonbit_array_length(_M0L7entriesS2651)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4521
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2651[
        _M0L3idxS786
      ];
    _M0L7_2abindS788 = _M0L6_2atmpS4521;
    if (_M0L7_2abindS788 == 0) {
      _M0L5entryS787->$2 = _M0L3pslS785;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS789, _M0L5entryS787, _M0L3idxS786);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS791 =
        _M0L7_2abindS788;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS792 =
        _M0L7_2aSomeS791;
      int32_t _M0L3pslS2641 = _M0L14_2acurr__entryS792->$2;
      if (_M0L3pslS785 > _M0L3pslS2641) {
        int32_t _M0L3pslS2646;
        int32_t _M0L6_2atmpS2642;
        int32_t _M0L6_2atmpS2644;
        int32_t _M0L14capacity__maskS2645;
        int32_t _M0L6_2atmpS2643;
        _M0L5entryS787->$2 = _M0L3pslS785;
        moonbit_incref(_M0L14_2acurr__entryS792);
        moonbit_incref(_M0L4selfS789);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS789, _M0L5entryS787, _M0L3idxS786);
        _M0L3pslS2646 = _M0L14_2acurr__entryS792->$2;
        _M0L6_2atmpS2642 = _M0L3pslS2646 + 1;
        _M0L6_2atmpS2644 = _M0L3idxS786 + 1;
        _M0L14capacity__maskS2645 = _M0L4selfS789->$3;
        _M0L6_2atmpS2643 = _M0L6_2atmpS2644 & _M0L14capacity__maskS2645;
        _M0L3pslS785 = _M0L6_2atmpS2642;
        _M0L3idxS786 = _M0L6_2atmpS2643;
        _M0L5entryS787 = _M0L14_2acurr__entryS792;
        continue;
      } else {
        int32_t _M0L6_2atmpS2647 = _M0L3pslS785 + 1;
        int32_t _M0L6_2atmpS2649 = _M0L3idxS786 + 1;
        int32_t _M0L14capacity__maskS2650 = _M0L4selfS789->$3;
        int32_t _M0L6_2atmpS2648 =
          _M0L6_2atmpS2649 & _M0L14capacity__maskS2650;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_5083 =
          _M0L5entryS787;
        _M0L3pslS785 = _M0L6_2atmpS2647;
        _M0L3idxS786 = _M0L6_2atmpS2648;
        _M0L5entryS787 = _tmp_5083;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS799,
  int32_t _M0L3idxS804,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS803
) {
  int32_t _M0L3pslS2672;
  int32_t _M0L6_2atmpS2668;
  int32_t _M0L6_2atmpS2670;
  int32_t _M0L14capacity__maskS2671;
  int32_t _M0L6_2atmpS2669;
  int32_t _M0L3pslS795;
  int32_t _M0L3idxS796;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS797;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2672 = _M0L5entryS803->$2;
  _M0L6_2atmpS2668 = _M0L3pslS2672 + 1;
  _M0L6_2atmpS2670 = _M0L3idxS804 + 1;
  _M0L14capacity__maskS2671 = _M0L4selfS799->$3;
  _M0L6_2atmpS2669 = _M0L6_2atmpS2670 & _M0L14capacity__maskS2671;
  _M0L3pslS795 = _M0L6_2atmpS2668;
  _M0L3idxS796 = _M0L6_2atmpS2669;
  _M0L5entryS797 = _M0L5entryS803;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4524 =
      _M0L4selfS799->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2667 =
      _M0L8_2afieldS4524;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4523;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS798;
    if (
      _M0L3idxS796 < 0
      || _M0L3idxS796 >= Moonbit_array_length(_M0L7entriesS2667)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4523
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2667[
        _M0L3idxS796
      ];
    _M0L7_2abindS798 = _M0L6_2atmpS4523;
    if (_M0L7_2abindS798 == 0) {
      _M0L5entryS797->$2 = _M0L3pslS795;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS799, _M0L5entryS797, _M0L3idxS796);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS801 =
        _M0L7_2abindS798;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS802 =
        _M0L7_2aSomeS801;
      int32_t _M0L3pslS2657 = _M0L14_2acurr__entryS802->$2;
      if (_M0L3pslS795 > _M0L3pslS2657) {
        int32_t _M0L3pslS2662;
        int32_t _M0L6_2atmpS2658;
        int32_t _M0L6_2atmpS2660;
        int32_t _M0L14capacity__maskS2661;
        int32_t _M0L6_2atmpS2659;
        _M0L5entryS797->$2 = _M0L3pslS795;
        moonbit_incref(_M0L14_2acurr__entryS802);
        moonbit_incref(_M0L4selfS799);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS799, _M0L5entryS797, _M0L3idxS796);
        _M0L3pslS2662 = _M0L14_2acurr__entryS802->$2;
        _M0L6_2atmpS2658 = _M0L3pslS2662 + 1;
        _M0L6_2atmpS2660 = _M0L3idxS796 + 1;
        _M0L14capacity__maskS2661 = _M0L4selfS799->$3;
        _M0L6_2atmpS2659 = _M0L6_2atmpS2660 & _M0L14capacity__maskS2661;
        _M0L3pslS795 = _M0L6_2atmpS2658;
        _M0L3idxS796 = _M0L6_2atmpS2659;
        _M0L5entryS797 = _M0L14_2acurr__entryS802;
        continue;
      } else {
        int32_t _M0L6_2atmpS2663 = _M0L3pslS795 + 1;
        int32_t _M0L6_2atmpS2665 = _M0L3idxS796 + 1;
        int32_t _M0L14capacity__maskS2666 = _M0L4selfS799->$3;
        int32_t _M0L6_2atmpS2664 =
          _M0L6_2atmpS2665 & _M0L14capacity__maskS2666;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_5085 =
          _M0L5entryS797;
        _M0L3pslS795 = _M0L6_2atmpS2663;
        _M0L3idxS796 = _M0L6_2atmpS2664;
        _M0L5entryS797 = _tmp_5085;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS773,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS775,
  int32_t _M0L8new__idxS774
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4527;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2637;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2638;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4526;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4525;
  int32_t _M0L6_2acntS4811;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS776;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4527 = _M0L4selfS773->$0;
  _M0L7entriesS2637 = _M0L8_2afieldS4527;
  moonbit_incref(_M0L5entryS775);
  _M0L6_2atmpS2638 = _M0L5entryS775;
  if (
    _M0L8new__idxS774 < 0
    || _M0L8new__idxS774 >= Moonbit_array_length(_M0L7entriesS2637)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4526
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2637[
      _M0L8new__idxS774
    ];
  if (_M0L6_2aoldS4526) {
    moonbit_decref(_M0L6_2aoldS4526);
  }
  _M0L7entriesS2637[_M0L8new__idxS774] = _M0L6_2atmpS2638;
  _M0L8_2afieldS4525 = _M0L5entryS775->$1;
  _M0L6_2acntS4811 = Moonbit_object_header(_M0L5entryS775)->rc;
  if (_M0L6_2acntS4811 > 1) {
    int32_t _M0L11_2anew__cntS4814 = _M0L6_2acntS4811 - 1;
    Moonbit_object_header(_M0L5entryS775)->rc = _M0L11_2anew__cntS4814;
    if (_M0L8_2afieldS4525) {
      moonbit_incref(_M0L8_2afieldS4525);
    }
  } else if (_M0L6_2acntS4811 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4813 =
      _M0L5entryS775->$5;
    moonbit_string_t _M0L8_2afieldS4812;
    moonbit_decref(_M0L8_2afieldS4813);
    _M0L8_2afieldS4812 = _M0L5entryS775->$4;
    moonbit_decref(_M0L8_2afieldS4812);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS775);
  }
  _M0L7_2abindS776 = _M0L8_2afieldS4525;
  if (_M0L7_2abindS776 == 0) {
    if (_M0L7_2abindS776) {
      moonbit_decref(_M0L7_2abindS776);
    }
    _M0L4selfS773->$6 = _M0L8new__idxS774;
    moonbit_decref(_M0L4selfS773);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS777;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS778;
    moonbit_decref(_M0L4selfS773);
    _M0L7_2aSomeS777 = _M0L7_2abindS776;
    _M0L7_2anextS778 = _M0L7_2aSomeS777;
    _M0L7_2anextS778->$0 = _M0L8new__idxS774;
    moonbit_decref(_M0L7_2anextS778);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS779,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS781,
  int32_t _M0L8new__idxS780
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4530;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2639;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2640;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4529;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4528;
  int32_t _M0L6_2acntS4815;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS782;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4530 = _M0L4selfS779->$0;
  _M0L7entriesS2639 = _M0L8_2afieldS4530;
  moonbit_incref(_M0L5entryS781);
  _M0L6_2atmpS2640 = _M0L5entryS781;
  if (
    _M0L8new__idxS780 < 0
    || _M0L8new__idxS780 >= Moonbit_array_length(_M0L7entriesS2639)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4529
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2639[
      _M0L8new__idxS780
    ];
  if (_M0L6_2aoldS4529) {
    moonbit_decref(_M0L6_2aoldS4529);
  }
  _M0L7entriesS2639[_M0L8new__idxS780] = _M0L6_2atmpS2640;
  _M0L8_2afieldS4528 = _M0L5entryS781->$1;
  _M0L6_2acntS4815 = Moonbit_object_header(_M0L5entryS781)->rc;
  if (_M0L6_2acntS4815 > 1) {
    int32_t _M0L11_2anew__cntS4817 = _M0L6_2acntS4815 - 1;
    Moonbit_object_header(_M0L5entryS781)->rc = _M0L11_2anew__cntS4817;
    if (_M0L8_2afieldS4528) {
      moonbit_incref(_M0L8_2afieldS4528);
    }
  } else if (_M0L6_2acntS4815 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4816 =
      _M0L5entryS781->$5;
    moonbit_decref(_M0L8_2afieldS4816);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS781);
  }
  _M0L7_2abindS782 = _M0L8_2afieldS4528;
  if (_M0L7_2abindS782 == 0) {
    if (_M0L7_2abindS782) {
      moonbit_decref(_M0L7_2abindS782);
    }
    _M0L4selfS779->$6 = _M0L8new__idxS780;
    moonbit_decref(_M0L4selfS779);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS783;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS784;
    moonbit_decref(_M0L4selfS779);
    _M0L7_2aSomeS783 = _M0L7_2abindS782;
    _M0L7_2anextS784 = _M0L7_2aSomeS783;
    _M0L7_2anextS784->$0 = _M0L8new__idxS780;
    moonbit_decref(_M0L7_2anextS784);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS766,
  int32_t _M0L3idxS768,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS767
) {
  int32_t _M0L7_2abindS765;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4532;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2624;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2625;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4531;
  int32_t _M0L4sizeS2627;
  int32_t _M0L6_2atmpS2626;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS765 = _M0L4selfS766->$6;
  switch (_M0L7_2abindS765) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2619;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4533;
      moonbit_incref(_M0L5entryS767);
      _M0L6_2atmpS2619 = _M0L5entryS767;
      _M0L6_2aoldS4533 = _M0L4selfS766->$5;
      if (_M0L6_2aoldS4533) {
        moonbit_decref(_M0L6_2aoldS4533);
      }
      _M0L4selfS766->$5 = _M0L6_2atmpS2619;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4536 =
        _M0L4selfS766->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2623 =
        _M0L8_2afieldS4536;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4535;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2622;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2620;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2621;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4534;
      if (
        _M0L7_2abindS765 < 0
        || _M0L7_2abindS765 >= Moonbit_array_length(_M0L7entriesS2623)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4535
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2623[
          _M0L7_2abindS765
        ];
      _M0L6_2atmpS2622 = _M0L6_2atmpS4535;
      if (_M0L6_2atmpS2622) {
        moonbit_incref(_M0L6_2atmpS2622);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2620
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2622);
      moonbit_incref(_M0L5entryS767);
      _M0L6_2atmpS2621 = _M0L5entryS767;
      _M0L6_2aoldS4534 = _M0L6_2atmpS2620->$1;
      if (_M0L6_2aoldS4534) {
        moonbit_decref(_M0L6_2aoldS4534);
      }
      _M0L6_2atmpS2620->$1 = _M0L6_2atmpS2621;
      moonbit_decref(_M0L6_2atmpS2620);
      break;
    }
  }
  _M0L4selfS766->$6 = _M0L3idxS768;
  _M0L8_2afieldS4532 = _M0L4selfS766->$0;
  _M0L7entriesS2624 = _M0L8_2afieldS4532;
  _M0L6_2atmpS2625 = _M0L5entryS767;
  if (
    _M0L3idxS768 < 0
    || _M0L3idxS768 >= Moonbit_array_length(_M0L7entriesS2624)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4531
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2624[
      _M0L3idxS768
    ];
  if (_M0L6_2aoldS4531) {
    moonbit_decref(_M0L6_2aoldS4531);
  }
  _M0L7entriesS2624[_M0L3idxS768] = _M0L6_2atmpS2625;
  _M0L4sizeS2627 = _M0L4selfS766->$1;
  _M0L6_2atmpS2626 = _M0L4sizeS2627 + 1;
  _M0L4selfS766->$1 = _M0L6_2atmpS2626;
  moonbit_decref(_M0L4selfS766);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS770,
  int32_t _M0L3idxS772,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS771
) {
  int32_t _M0L7_2abindS769;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4538;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2633;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2634;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4537;
  int32_t _M0L4sizeS2636;
  int32_t _M0L6_2atmpS2635;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS769 = _M0L4selfS770->$6;
  switch (_M0L7_2abindS769) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2628;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4539;
      moonbit_incref(_M0L5entryS771);
      _M0L6_2atmpS2628 = _M0L5entryS771;
      _M0L6_2aoldS4539 = _M0L4selfS770->$5;
      if (_M0L6_2aoldS4539) {
        moonbit_decref(_M0L6_2aoldS4539);
      }
      _M0L4selfS770->$5 = _M0L6_2atmpS2628;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4542 =
        _M0L4selfS770->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2632 =
        _M0L8_2afieldS4542;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4541;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2631;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2629;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2630;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4540;
      if (
        _M0L7_2abindS769 < 0
        || _M0L7_2abindS769 >= Moonbit_array_length(_M0L7entriesS2632)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4541
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2632[
          _M0L7_2abindS769
        ];
      _M0L6_2atmpS2631 = _M0L6_2atmpS4541;
      if (_M0L6_2atmpS2631) {
        moonbit_incref(_M0L6_2atmpS2631);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2629
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2631);
      moonbit_incref(_M0L5entryS771);
      _M0L6_2atmpS2630 = _M0L5entryS771;
      _M0L6_2aoldS4540 = _M0L6_2atmpS2629->$1;
      if (_M0L6_2aoldS4540) {
        moonbit_decref(_M0L6_2aoldS4540);
      }
      _M0L6_2atmpS2629->$1 = _M0L6_2atmpS2630;
      moonbit_decref(_M0L6_2atmpS2629);
      break;
    }
  }
  _M0L4selfS770->$6 = _M0L3idxS772;
  _M0L8_2afieldS4538 = _M0L4selfS770->$0;
  _M0L7entriesS2633 = _M0L8_2afieldS4538;
  _M0L6_2atmpS2634 = _M0L5entryS771;
  if (
    _M0L3idxS772 < 0
    || _M0L3idxS772 >= Moonbit_array_length(_M0L7entriesS2633)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4537
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2633[
      _M0L3idxS772
    ];
  if (_M0L6_2aoldS4537) {
    moonbit_decref(_M0L6_2aoldS4537);
  }
  _M0L7entriesS2633[_M0L3idxS772] = _M0L6_2atmpS2634;
  _M0L4sizeS2636 = _M0L4selfS770->$1;
  _M0L6_2atmpS2635 = _M0L4sizeS2636 + 1;
  _M0L4selfS770->$1 = _M0L6_2atmpS2635;
  moonbit_decref(_M0L4selfS770);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS754
) {
  int32_t _M0L8capacityS753;
  int32_t _M0L7_2abindS755;
  int32_t _M0L7_2abindS756;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2617;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS757;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS758;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_5086;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS753
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS754);
  _M0L7_2abindS755 = _M0L8capacityS753 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS756 = _M0FPB21calc__grow__threshold(_M0L8capacityS753);
  _M0L6_2atmpS2617 = 0;
  _M0L7_2abindS757
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS753, _M0L6_2atmpS2617);
  _M0L7_2abindS758 = 0;
  _block_5086
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_5086)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_5086->$0 = _M0L7_2abindS757;
  _block_5086->$1 = 0;
  _block_5086->$2 = _M0L8capacityS753;
  _block_5086->$3 = _M0L7_2abindS755;
  _block_5086->$4 = _M0L7_2abindS756;
  _block_5086->$5 = _M0L7_2abindS758;
  _block_5086->$6 = -1;
  return _block_5086;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS760
) {
  int32_t _M0L8capacityS759;
  int32_t _M0L7_2abindS761;
  int32_t _M0L7_2abindS762;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2618;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS763;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS764;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_5087;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS759
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS760);
  _M0L7_2abindS761 = _M0L8capacityS759 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS762 = _M0FPB21calc__grow__threshold(_M0L8capacityS759);
  _M0L6_2atmpS2618 = 0;
  _M0L7_2abindS763
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS759, _M0L6_2atmpS2618);
  _M0L7_2abindS764 = 0;
  _block_5087
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_5087)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_5087->$0 = _M0L7_2abindS763;
  _block_5087->$1 = 0;
  _block_5087->$2 = _M0L8capacityS759;
  _block_5087->$3 = _M0L7_2abindS761;
  _block_5087->$4 = _M0L7_2abindS762;
  _block_5087->$5 = _M0L7_2abindS764;
  _block_5087->$6 = -1;
  return _block_5087;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS752) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS752 >= 0) {
    int32_t _M0L6_2atmpS2616;
    int32_t _M0L6_2atmpS2615;
    int32_t _M0L6_2atmpS2614;
    int32_t _M0L6_2atmpS2613;
    if (_M0L4selfS752 <= 1) {
      return 1;
    }
    if (_M0L4selfS752 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2616 = _M0L4selfS752 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2615 = moonbit_clz32(_M0L6_2atmpS2616);
    _M0L6_2atmpS2614 = _M0L6_2atmpS2615 - 1;
    _M0L6_2atmpS2613 = 2147483647 >> (_M0L6_2atmpS2614 & 31);
    return _M0L6_2atmpS2613 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS751) {
  int32_t _M0L6_2atmpS2612;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2612 = _M0L8capacityS751 * 13;
  return _M0L6_2atmpS2612 / 16;
}

struct moonbit_result_2 _M0MPC16option6Option17unwrap__or__errorGRP48clawteam8clawteam8internal3uri9AuthorityRPB7FailureE(
  struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L4selfS747,
  void* _M0L3errS750
) {
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS747 == 0) {
    struct moonbit_result_2 _result_5088;
    if (_M0L4selfS747) {
      moonbit_decref(_M0L4selfS747);
    }
    _result_5088.tag = 0;
    _result_5088.data.err = _M0L3errS750;
    return _result_5088;
  } else {
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L7_2aSomeS748;
    struct _M0TP48clawteam8clawteam8internal3uri9Authority* _M0L4_2avS749;
    struct moonbit_result_2 _result_5089;
    moonbit_decref(_M0L3errS750);
    _M0L7_2aSomeS748 = _M0L4selfS747;
    _M0L4_2avS749 = _M0L7_2aSomeS748;
    _result_5089.tag = 1;
    _result_5089.data.ok = _M0L4_2avS749;
    return _result_5089;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS743
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS743 == 0) {
    if (_M0L4selfS743) {
      moonbit_decref(_M0L4selfS743);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS744 =
      _M0L4selfS743;
    return _M0L7_2aSomeS744;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS745
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS745 == 0) {
    if (_M0L4selfS745) {
      moonbit_decref(_M0L4selfS745);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS746 =
      _M0L4selfS745;
    return _M0L7_2aSomeS746;
  }
}

int32_t _M0IPC16option6OptionPB2Eq5equalGRPC16string10StringViewE(
  void* _M0L4selfS731,
  void* _M0L5otherS732
) {
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  switch (Moonbit_object_tag(_M0L4selfS731)) {
    case 0: {
      switch (Moonbit_object_tag(_M0L5otherS732)) {
        case 0: {
          return 1;
          break;
        }
        default: {
          moonbit_decref(_M0L5otherS732);
          return 0;
          break;
        }
      }
      break;
    }
    default: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS733 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4selfS731;
      struct _M0TPC16string10StringView _M0L8_2afieldS4544 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS733->$0_1,
                                              _M0L7_2aSomeS733->$0_2,
                                              _M0L7_2aSomeS733->$0_0};
      int32_t _M0L6_2acntS4818 = Moonbit_object_header(_M0L7_2aSomeS733)->rc;
      struct _M0TPC16string10StringView _M0L4_2axS734;
      if (_M0L6_2acntS4818 > 1) {
        int32_t _M0L11_2anew__cntS4819 = _M0L6_2acntS4818 - 1;
        Moonbit_object_header(_M0L7_2aSomeS733)->rc = _M0L11_2anew__cntS4819;
        moonbit_incref(_M0L8_2afieldS4544.$0);
      } else if (_M0L6_2acntS4818 == 1) {
        #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
        moonbit_free(_M0L7_2aSomeS733);
      }
      _M0L4_2axS734 = _M0L8_2afieldS4544;
      switch (Moonbit_object_tag(_M0L5otherS732)) {
        case 1: {
          struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS735 =
            (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L5otherS732;
          struct _M0TPC16string10StringView _M0L8_2afieldS4543 =
            (struct _M0TPC16string10StringView){_M0L7_2aSomeS735->$0_1,
                                                  _M0L7_2aSomeS735->$0_2,
                                                  _M0L7_2aSomeS735->$0_0};
          int32_t _M0L6_2acntS4820 =
            Moonbit_object_header(_M0L7_2aSomeS735)->rc;
          struct _M0TPC16string10StringView _M0L4_2ayS736;
          if (_M0L6_2acntS4820 > 1) {
            int32_t _M0L11_2anew__cntS4821 = _M0L6_2acntS4820 - 1;
            Moonbit_object_header(_M0L7_2aSomeS735)->rc
            = _M0L11_2anew__cntS4821;
            moonbit_incref(_M0L8_2afieldS4543.$0);
          } else if (_M0L6_2acntS4820 == 1) {
            #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
            moonbit_free(_M0L7_2aSomeS735);
          }
          _M0L4_2ayS736 = _M0L8_2afieldS4543;
          #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
          return _M0IPC16string10StringViewPB2Eq5equal(_M0L4_2axS734, _M0L4_2ayS736);
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS734.$0);
          moonbit_decref(_M0L5otherS732);
          return 0;
          break;
        }
      }
      break;
    }
  }
}

int32_t _M0IPC16option6OptionPB2Eq5equalGiE(
  int64_t _M0L4selfS737,
  int64_t _M0L5otherS738
) {
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS737 == 4294967296ll) {
    return _M0L5otherS738 == 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS739 = _M0L4selfS737;
    int32_t _M0L4_2axS740 = (int32_t)_M0L7_2aSomeS739;
    if (_M0L5otherS738 == 4294967296ll) {
      return 0;
    } else {
      int64_t _M0L7_2aSomeS741 = _M0L5otherS738;
      int32_t _M0L4_2ayS742 = (int32_t)_M0L7_2aSomeS741;
      return _M0L4_2axS740 == _M0L4_2ayS742;
    }
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS730
) {
  moonbit_string_t* _M0L6_2atmpS2611;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2611 = _M0L4selfS730;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2611);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS726,
  int32_t _M0L5indexS727
) {
  uint64_t* _M0L6_2atmpS2609;
  uint64_t _M0L6_2atmpS4545;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2609 = _M0L4selfS726;
  if (
    _M0L5indexS727 < 0
    || _M0L5indexS727 >= Moonbit_array_length(_M0L6_2atmpS2609)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4545 = (uint64_t)_M0L6_2atmpS2609[_M0L5indexS727];
  moonbit_decref(_M0L6_2atmpS2609);
  return _M0L6_2atmpS4545;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS728,
  int32_t _M0L5indexS729
) {
  uint32_t* _M0L6_2atmpS2610;
  uint32_t _M0L6_2atmpS4546;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2610 = _M0L4selfS728;
  if (
    _M0L5indexS729 < 0
    || _M0L5indexS729 >= Moonbit_array_length(_M0L6_2atmpS2610)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4546 = (uint32_t)_M0L6_2atmpS2610[_M0L5indexS729];
  moonbit_decref(_M0L6_2atmpS2610);
  return _M0L6_2atmpS4546;
}

int32_t _M0IPC15array5ArrayPB4Show6outputGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS725,
  struct _M0TPB6Logger _M0L6loggerS724
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2608;
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2608
  = _M0MPC15array5Array4iterGRPC16string10StringViewE(_M0L4selfS725);
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0MPB6Logger19write__iter_2einnerGRPC16string10StringViewE(_M0L6loggerS724, _M0L6_2atmpS2608, (moonbit_string_t)moonbit_string_literal_84.data, (moonbit_string_t)moonbit_string_literal_85.data, (moonbit_string_t)moonbit_string_literal_86.data, 0);
  return 0;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC15array5Array4iterGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS723
) {
  struct _M0TPC16string10StringView* _M0L8_2afieldS4548;
  struct _M0TPC16string10StringView* _M0L3bufS2606;
  int32_t _M0L8_2afieldS4547;
  int32_t _M0L6_2acntS4822;
  int32_t _M0L3lenS2607;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L6_2atmpS2605;
  #line 1651 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS4548 = _M0L4selfS723->$0;
  _M0L3bufS2606 = _M0L8_2afieldS4548;
  _M0L8_2afieldS4547 = _M0L4selfS723->$1;
  _M0L6_2acntS4822 = Moonbit_object_header(_M0L4selfS723)->rc;
  if (_M0L6_2acntS4822 > 1) {
    int32_t _M0L11_2anew__cntS4823 = _M0L6_2acntS4822 - 1;
    Moonbit_object_header(_M0L4selfS723)->rc = _M0L11_2anew__cntS4823;
    moonbit_incref(_M0L3bufS2606);
  } else if (_M0L6_2acntS4822 == 1) {
    #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_free(_M0L4selfS723);
  }
  _M0L3lenS2607 = _M0L8_2afieldS4547;
  _M0L6_2atmpS2605
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    0, _M0L3lenS2607, _M0L3bufS2606
  };
  #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  return _M0MPC15array9ArrayView4iterGRPC16string10StringViewE(_M0L6_2atmpS2605);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS722
) {
  moonbit_string_t* _M0L6_2atmpS2603;
  int32_t _M0L6_2atmpS4549;
  int32_t _M0L6_2atmpS2604;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2602;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS722);
  _M0L6_2atmpS2603 = _M0L4selfS722;
  _M0L6_2atmpS4549 = Moonbit_array_length(_M0L4selfS722);
  moonbit_decref(_M0L4selfS722);
  _M0L6_2atmpS2604 = _M0L6_2atmpS4549;
  _M0L6_2atmpS2602
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2604, _M0L6_2atmpS2603
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2602);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS717
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS716;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__* _closure_5090;
  struct _M0TWEOs* _M0L6_2atmpS2578;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS716
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS716)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS716->$0 = 0;
  _closure_5090
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__));
  Moonbit_object_header(_closure_5090)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__, $0_0) >> 2, 2, 0);
  _closure_5090->code = &_M0MPC15array9ArrayView4iterGsEC2579l570;
  _closure_5090->$0_0 = _M0L4selfS717.$0;
  _closure_5090->$0_1 = _M0L4selfS717.$1;
  _closure_5090->$0_2 = _M0L4selfS717.$2;
  _closure_5090->$1 = _M0L1iS716;
  _M0L6_2atmpS2578 = (struct _M0TWEOs*)_closure_5090;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2578);
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPC15array9ArrayView4iterGRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4selfS720
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS719;
  struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__* _closure_5091;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2590;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS719
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS719)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS719->$0 = 0;
  _closure_5091
  = (struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__*)moonbit_malloc(sizeof(struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__));
  Moonbit_object_header(_closure_5091)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__, $0_0) >> 2, 2, 0);
  _closure_5091->code
  = &_M0MPC15array9ArrayView4iterGRPC16string10StringViewEC2591l570;
  _closure_5091->$0_0 = _M0L4selfS720.$0;
  _closure_5091->$0_1 = _M0L4selfS720.$1;
  _closure_5091->$0_2 = _M0L4selfS720.$2;
  _closure_5091->$1 = _M0L1iS719;
  _M0L6_2atmpS2590
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_5091;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS2590);
}

void* _M0MPC15array9ArrayView4iterGRPC16string10StringViewEC2591l570(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2592
) {
  struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__* _M0L14_2acasted__envS2593;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4554;
  struct _M0TPC13ref3RefGiE* _M0L1iS719;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L8_2afieldS4553;
  int32_t _M0L6_2acntS4824;
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4selfS720;
  int32_t _M0L3valS2594;
  int32_t _M0L6_2atmpS2595;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2593
  = (struct _M0R93ArrayView_3a_3aiter_7c_5bmoonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2591__l570__*)_M0L6_2aenvS2592;
  _M0L8_2afieldS4554 = _M0L14_2acasted__envS2593->$1;
  _M0L1iS719 = _M0L8_2afieldS4554;
  _M0L8_2afieldS4553
  = (struct _M0TPB9ArrayViewGRPC16string10StringViewE){
    _M0L14_2acasted__envS2593->$0_1,
      _M0L14_2acasted__envS2593->$0_2,
      _M0L14_2acasted__envS2593->$0_0
  };
  _M0L6_2acntS4824 = Moonbit_object_header(_M0L14_2acasted__envS2593)->rc;
  if (_M0L6_2acntS4824 > 1) {
    int32_t _M0L11_2anew__cntS4825 = _M0L6_2acntS4824 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2593)->rc
    = _M0L11_2anew__cntS4825;
    moonbit_incref(_M0L1iS719);
    moonbit_incref(_M0L8_2afieldS4553.$0);
  } else if (_M0L6_2acntS4824 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2593);
  }
  _M0L4selfS720 = _M0L8_2afieldS4553;
  _M0L3valS2594 = _M0L1iS719->$0;
  moonbit_incref(_M0L4selfS720.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2595
  = _M0MPC15array9ArrayView6lengthGRPC16string10StringViewE(_M0L4selfS720);
  if (_M0L3valS2594 < _M0L6_2atmpS2595) {
    struct _M0TPC16string10StringView* _M0L8_2afieldS4552 = _M0L4selfS720.$0;
    struct _M0TPC16string10StringView* _M0L3bufS2598 = _M0L8_2afieldS4552;
    int32_t _M0L8_2afieldS4551 = _M0L4selfS720.$1;
    int32_t _M0L5startS2600 = _M0L8_2afieldS4551;
    int32_t _M0L3valS2601 = _M0L1iS719->$0;
    int32_t _M0L6_2atmpS2599 = _M0L5startS2600 + _M0L3valS2601;
    struct _M0TPC16string10StringView _M0L6_2atmpS4550 =
      _M0L3bufS2598[_M0L6_2atmpS2599];
    struct _M0TPC16string10StringView _M0L4elemS721;
    int32_t _M0L3valS2597;
    int32_t _M0L6_2atmpS2596;
    void* _block_5092;
    moonbit_incref(_M0L6_2atmpS4550.$0);
    moonbit_decref(_M0L3bufS2598);
    _M0L4elemS721 = _M0L6_2atmpS4550;
    _M0L3valS2597 = _M0L1iS719->$0;
    _M0L6_2atmpS2596 = _M0L3valS2597 + 1;
    _M0L1iS719->$0 = _M0L6_2atmpS2596;
    moonbit_decref(_M0L1iS719);
    _block_5092
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_5092)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5092)->$0_0
    = _M0L4elemS721.$0;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5092)->$0_1
    = _M0L4elemS721.$1;
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5092)->$0_2
    = _M0L4elemS721.$2;
    return _block_5092;
  } else {
    moonbit_decref(_M0L4selfS720.$0);
    moonbit_decref(_M0L1iS719);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2579l570(
  struct _M0TWEOs* _M0L6_2aenvS2580
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__* _M0L14_2acasted__envS2581;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4559;
  struct _M0TPC13ref3RefGiE* _M0L1iS716;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4558;
  int32_t _M0L6_2acntS4826;
  struct _M0TPB9ArrayViewGsE _M0L4selfS717;
  int32_t _M0L3valS2582;
  int32_t _M0L6_2atmpS2583;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2581
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2579__l570__*)_M0L6_2aenvS2580;
  _M0L8_2afieldS4559 = _M0L14_2acasted__envS2581->$1;
  _M0L1iS716 = _M0L8_2afieldS4559;
  _M0L8_2afieldS4558
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2581->$0_1,
      _M0L14_2acasted__envS2581->$0_2,
      _M0L14_2acasted__envS2581->$0_0
  };
  _M0L6_2acntS4826 = Moonbit_object_header(_M0L14_2acasted__envS2581)->rc;
  if (_M0L6_2acntS4826 > 1) {
    int32_t _M0L11_2anew__cntS4827 = _M0L6_2acntS4826 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2581)->rc
    = _M0L11_2anew__cntS4827;
    moonbit_incref(_M0L1iS716);
    moonbit_incref(_M0L8_2afieldS4558.$0);
  } else if (_M0L6_2acntS4826 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2581);
  }
  _M0L4selfS717 = _M0L8_2afieldS4558;
  _M0L3valS2582 = _M0L1iS716->$0;
  moonbit_incref(_M0L4selfS717.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2583 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS717);
  if (_M0L3valS2582 < _M0L6_2atmpS2583) {
    moonbit_string_t* _M0L8_2afieldS4557 = _M0L4selfS717.$0;
    moonbit_string_t* _M0L3bufS2586 = _M0L8_2afieldS4557;
    int32_t _M0L8_2afieldS4556 = _M0L4selfS717.$1;
    int32_t _M0L5startS2588 = _M0L8_2afieldS4556;
    int32_t _M0L3valS2589 = _M0L1iS716->$0;
    int32_t _M0L6_2atmpS2587 = _M0L5startS2588 + _M0L3valS2589;
    moonbit_string_t _M0L6_2atmpS4555 =
      (moonbit_string_t)_M0L3bufS2586[_M0L6_2atmpS2587];
    moonbit_string_t _M0L4elemS718;
    int32_t _M0L3valS2585;
    int32_t _M0L6_2atmpS2584;
    moonbit_incref(_M0L6_2atmpS4555);
    moonbit_decref(_M0L3bufS2586);
    _M0L4elemS718 = _M0L6_2atmpS4555;
    _M0L3valS2585 = _M0L1iS716->$0;
    _M0L6_2atmpS2584 = _M0L3valS2585 + 1;
    _M0L1iS716->$0 = _M0L6_2atmpS2584;
    moonbit_decref(_M0L1iS716);
    return _M0L4elemS718;
  } else {
    moonbit_decref(_M0L4selfS717.$0);
    moonbit_decref(_M0L1iS716);
    return 0;
  }
}

int32_t _M0IPC16option6OptionPB4Show6outputGRPC16string10StringViewE(
  void* _M0L4selfS708,
  struct _M0TPB6Logger _M0L6loggerS709
) {
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  switch (Moonbit_object_tag(_M0L4selfS708)) {
    case 0: {
      #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0L6loggerS709.$0->$method_0(_M0L6loggerS709.$1, (moonbit_string_t)moonbit_string_literal_87.data);
      break;
    }
    default: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS710 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4selfS708;
      struct _M0TPC16string10StringView _M0L8_2afieldS4560 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS710->$0_1,
                                              _M0L7_2aSomeS710->$0_2,
                                              _M0L7_2aSomeS710->$0_0};
      int32_t _M0L6_2acntS4828 = Moonbit_object_header(_M0L7_2aSomeS710)->rc;
      struct _M0TPC16string10StringView _M0L6_2aargS711;
      if (_M0L6_2acntS4828 > 1) {
        int32_t _M0L11_2anew__cntS4829 = _M0L6_2acntS4828 - 1;
        Moonbit_object_header(_M0L7_2aSomeS710)->rc = _M0L11_2anew__cntS4829;
        moonbit_incref(_M0L8_2afieldS4560.$0);
      } else if (_M0L6_2acntS4828 == 1) {
        #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        moonbit_free(_M0L7_2aSomeS710);
      }
      _M0L6_2aargS711 = _M0L8_2afieldS4560;
      if (_M0L6loggerS709.$1) {
        moonbit_incref(_M0L6loggerS709.$1);
      }
      #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0L6loggerS709.$0->$method_0(_M0L6loggerS709.$1, (moonbit_string_t)moonbit_string_literal_88.data);
      if (_M0L6loggerS709.$1) {
        moonbit_incref(_M0L6loggerS709.$1);
      }
      #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0MPB6Logger13write__objectGRPC16string10StringViewE(_M0L6loggerS709, _M0L6_2aargS711);
      #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0L6loggerS709.$0->$method_0(_M0L6loggerS709.$1, (moonbit_string_t)moonbit_string_literal_89.data);
      break;
    }
  }
  return 0;
}

int32_t _M0IPC16option6OptionPB4Show6outputGiE(
  int64_t _M0L4selfS712,
  struct _M0TPB6Logger _M0L6loggerS713
) {
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS712 == 4294967296ll) {
    #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS713.$0->$method_0(_M0L6loggerS713.$1, (moonbit_string_t)moonbit_string_literal_87.data);
  } else {
    int64_t _M0L7_2aSomeS714 = _M0L4selfS712;
    int32_t _M0L6_2aargS715 = (int32_t)_M0L7_2aSomeS714;
    if (_M0L6loggerS713.$1) {
      moonbit_incref(_M0L6loggerS713.$1);
    }
    #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS713.$0->$method_0(_M0L6loggerS713.$1, (moonbit_string_t)moonbit_string_literal_88.data);
    if (_M0L6loggerS713.$1) {
      moonbit_incref(_M0L6loggerS713.$1);
    }
    #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0MPB6Logger13write__objectGiE(_M0L6loggerS713, _M0L6_2aargS715);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS713.$0->$method_0(_M0L6loggerS713.$1, (moonbit_string_t)moonbit_string_literal_89.data);
  }
  return 0;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS707
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS707;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS706,
  struct _M0TPB6Logger _M0L6loggerS705
) {
  moonbit_string_t _M0L6_2atmpS2577;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2577
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS706, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS705.$0->$method_0(_M0L6loggerS705.$1, _M0L6_2atmpS2577);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS704,
  struct _M0TPB6Logger _M0L6loggerS703
) {
  moonbit_string_t _M0L6_2atmpS2576;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2576 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS704, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS703.$0->$method_0(_M0L6loggerS703.$1, _M0L6_2atmpS2576);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS698) {
  int32_t _M0L3lenS697;
  struct _M0TPC13ref3RefGiE* _M0L5indexS699;
  struct _M0R38String_3a_3aiter_2eanon__u2560__l247__* _closure_5093;
  struct _M0TWEOc* _M0L6_2atmpS2559;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS697 = Moonbit_array_length(_M0L4selfS698);
  _M0L5indexS699
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS699)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS699->$0 = 0;
  _closure_5093
  = (struct _M0R38String_3a_3aiter_2eanon__u2560__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2560__l247__));
  Moonbit_object_header(_closure_5093)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2560__l247__, $0) >> 2, 2, 0);
  _closure_5093->code = &_M0MPC16string6String4iterC2560l247;
  _closure_5093->$0 = _M0L5indexS699;
  _closure_5093->$1 = _M0L4selfS698;
  _closure_5093->$2 = _M0L3lenS697;
  _M0L6_2atmpS2559 = (struct _M0TWEOc*)_closure_5093;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2559);
}

int32_t _M0MPC16string6String4iterC2560l247(
  struct _M0TWEOc* _M0L6_2aenvS2561
) {
  struct _M0R38String_3a_3aiter_2eanon__u2560__l247__* _M0L14_2acasted__envS2562;
  int32_t _M0L3lenS697;
  moonbit_string_t _M0L8_2afieldS4563;
  moonbit_string_t _M0L4selfS698;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4562;
  int32_t _M0L6_2acntS4830;
  struct _M0TPC13ref3RefGiE* _M0L5indexS699;
  int32_t _M0L3valS2563;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2562
  = (struct _M0R38String_3a_3aiter_2eanon__u2560__l247__*)_M0L6_2aenvS2561;
  _M0L3lenS697 = _M0L14_2acasted__envS2562->$2;
  _M0L8_2afieldS4563 = _M0L14_2acasted__envS2562->$1;
  _M0L4selfS698 = _M0L8_2afieldS4563;
  _M0L8_2afieldS4562 = _M0L14_2acasted__envS2562->$0;
  _M0L6_2acntS4830 = Moonbit_object_header(_M0L14_2acasted__envS2562)->rc;
  if (_M0L6_2acntS4830 > 1) {
    int32_t _M0L11_2anew__cntS4831 = _M0L6_2acntS4830 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2562)->rc
    = _M0L11_2anew__cntS4831;
    moonbit_incref(_M0L4selfS698);
    moonbit_incref(_M0L8_2afieldS4562);
  } else if (_M0L6_2acntS4830 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2562);
  }
  _M0L5indexS699 = _M0L8_2afieldS4562;
  _M0L3valS2563 = _M0L5indexS699->$0;
  if (_M0L3valS2563 < _M0L3lenS697) {
    int32_t _M0L3valS2575 = _M0L5indexS699->$0;
    int32_t _M0L2c1S700 = _M0L4selfS698[_M0L3valS2575];
    int32_t _if__result_5094;
    int32_t _M0L3valS2573;
    int32_t _M0L6_2atmpS2572;
    int32_t _M0L6_2atmpS2574;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S700)) {
      int32_t _M0L3valS2565 = _M0L5indexS699->$0;
      int32_t _M0L6_2atmpS2564 = _M0L3valS2565 + 1;
      _if__result_5094 = _M0L6_2atmpS2564 < _M0L3lenS697;
    } else {
      _if__result_5094 = 0;
    }
    if (_if__result_5094) {
      int32_t _M0L3valS2571 = _M0L5indexS699->$0;
      int32_t _M0L6_2atmpS2570 = _M0L3valS2571 + 1;
      int32_t _M0L6_2atmpS4561 = _M0L4selfS698[_M0L6_2atmpS2570];
      int32_t _M0L2c2S701;
      moonbit_decref(_M0L4selfS698);
      _M0L2c2S701 = _M0L6_2atmpS4561;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S701)) {
        int32_t _M0L6_2atmpS2568 = (int32_t)_M0L2c1S700;
        int32_t _M0L6_2atmpS2569 = (int32_t)_M0L2c2S701;
        int32_t _M0L1cS702;
        int32_t _M0L3valS2567;
        int32_t _M0L6_2atmpS2566;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS702
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2568, _M0L6_2atmpS2569);
        _M0L3valS2567 = _M0L5indexS699->$0;
        _M0L6_2atmpS2566 = _M0L3valS2567 + 2;
        _M0L5indexS699->$0 = _M0L6_2atmpS2566;
        moonbit_decref(_M0L5indexS699);
        return _M0L1cS702;
      }
    } else {
      moonbit_decref(_M0L4selfS698);
    }
    _M0L3valS2573 = _M0L5indexS699->$0;
    _M0L6_2atmpS2572 = _M0L3valS2573 + 1;
    _M0L5indexS699->$0 = _M0L6_2atmpS2572;
    moonbit_decref(_M0L5indexS699);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2574 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S700);
    return _M0L6_2atmpS2574;
  } else {
    moonbit_decref(_M0L5indexS699);
    moonbit_decref(_M0L4selfS698);
    return -1;
  }
}

int32_t _M0MPC16string10StringView9is__empty(
  struct _M0TPC16string10StringView _M0L4selfS696
) {
  int32_t _M0L6_2atmpS2558;
  #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2558 = _M0MPC16string10StringView6length(_M0L4selfS696);
  return _M0L6_2atmpS2558 == 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS684,
  moonbit_string_t _M0L5valueS686
) {
  int32_t _M0L3lenS2538;
  moonbit_string_t* _M0L6_2atmpS2540;
  int32_t _M0L6_2atmpS4566;
  int32_t _M0L6_2atmpS2539;
  int32_t _M0L6lengthS685;
  moonbit_string_t* _M0L8_2afieldS4565;
  moonbit_string_t* _M0L3bufS2541;
  moonbit_string_t _M0L6_2aoldS4564;
  int32_t _M0L6_2atmpS2542;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2538 = _M0L4selfS684->$1;
  moonbit_incref(_M0L4selfS684);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2540 = _M0MPC15array5Array6bufferGsE(_M0L4selfS684);
  _M0L6_2atmpS4566 = Moonbit_array_length(_M0L6_2atmpS2540);
  moonbit_decref(_M0L6_2atmpS2540);
  _M0L6_2atmpS2539 = _M0L6_2atmpS4566;
  if (_M0L3lenS2538 == _M0L6_2atmpS2539) {
    moonbit_incref(_M0L4selfS684);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS684);
  }
  _M0L6lengthS685 = _M0L4selfS684->$1;
  _M0L8_2afieldS4565 = _M0L4selfS684->$0;
  _M0L3bufS2541 = _M0L8_2afieldS4565;
  _M0L6_2aoldS4564 = (moonbit_string_t)_M0L3bufS2541[_M0L6lengthS685];
  moonbit_decref(_M0L6_2aoldS4564);
  _M0L3bufS2541[_M0L6lengthS685] = _M0L5valueS686;
  _M0L6_2atmpS2542 = _M0L6lengthS685 + 1;
  _M0L4selfS684->$1 = _M0L6_2atmpS2542;
  moonbit_decref(_M0L4selfS684);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS687,
  struct _M0TUsiE* _M0L5valueS689
) {
  int32_t _M0L3lenS2543;
  struct _M0TUsiE** _M0L6_2atmpS2545;
  int32_t _M0L6_2atmpS4569;
  int32_t _M0L6_2atmpS2544;
  int32_t _M0L6lengthS688;
  struct _M0TUsiE** _M0L8_2afieldS4568;
  struct _M0TUsiE** _M0L3bufS2546;
  struct _M0TUsiE* _M0L6_2aoldS4567;
  int32_t _M0L6_2atmpS2547;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2543 = _M0L4selfS687->$1;
  moonbit_incref(_M0L4selfS687);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2545 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS687);
  _M0L6_2atmpS4569 = Moonbit_array_length(_M0L6_2atmpS2545);
  moonbit_decref(_M0L6_2atmpS2545);
  _M0L6_2atmpS2544 = _M0L6_2atmpS4569;
  if (_M0L3lenS2543 == _M0L6_2atmpS2544) {
    moonbit_incref(_M0L4selfS687);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS687);
  }
  _M0L6lengthS688 = _M0L4selfS687->$1;
  _M0L8_2afieldS4568 = _M0L4selfS687->$0;
  _M0L3bufS2546 = _M0L8_2afieldS4568;
  _M0L6_2aoldS4567 = (struct _M0TUsiE*)_M0L3bufS2546[_M0L6lengthS688];
  if (_M0L6_2aoldS4567) {
    moonbit_decref(_M0L6_2aoldS4567);
  }
  _M0L3bufS2546[_M0L6lengthS688] = _M0L5valueS689;
  _M0L6_2atmpS2547 = _M0L6lengthS688 + 1;
  _M0L4selfS687->$1 = _M0L6_2atmpS2547;
  moonbit_decref(_M0L4selfS687);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS690,
  void* _M0L5valueS692
) {
  int32_t _M0L3lenS2548;
  void** _M0L6_2atmpS2550;
  int32_t _M0L6_2atmpS4572;
  int32_t _M0L6_2atmpS2549;
  int32_t _M0L6lengthS691;
  void** _M0L8_2afieldS4571;
  void** _M0L3bufS2551;
  void* _M0L6_2aoldS4570;
  int32_t _M0L6_2atmpS2552;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2548 = _M0L4selfS690->$1;
  moonbit_incref(_M0L4selfS690);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2550
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS690);
  _M0L6_2atmpS4572 = Moonbit_array_length(_M0L6_2atmpS2550);
  moonbit_decref(_M0L6_2atmpS2550);
  _M0L6_2atmpS2549 = _M0L6_2atmpS4572;
  if (_M0L3lenS2548 == _M0L6_2atmpS2549) {
    moonbit_incref(_M0L4selfS690);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS690);
  }
  _M0L6lengthS691 = _M0L4selfS690->$1;
  _M0L8_2afieldS4571 = _M0L4selfS690->$0;
  _M0L3bufS2551 = _M0L8_2afieldS4571;
  _M0L6_2aoldS4570 = (void*)_M0L3bufS2551[_M0L6lengthS691];
  moonbit_decref(_M0L6_2aoldS4570);
  _M0L3bufS2551[_M0L6lengthS691] = _M0L5valueS692;
  _M0L6_2atmpS2552 = _M0L6lengthS691 + 1;
  _M0L4selfS690->$1 = _M0L6_2atmpS2552;
  moonbit_decref(_M0L4selfS690);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS693,
  struct _M0TPC16string10StringView _M0L5valueS695
) {
  int32_t _M0L3lenS2553;
  struct _M0TPC16string10StringView* _M0L6_2atmpS2555;
  int32_t _M0L6_2atmpS4575;
  int32_t _M0L6_2atmpS2554;
  int32_t _M0L6lengthS694;
  struct _M0TPC16string10StringView* _M0L8_2afieldS4574;
  struct _M0TPC16string10StringView* _M0L3bufS2556;
  struct _M0TPC16string10StringView _M0L6_2aoldS4573;
  int32_t _M0L6_2atmpS2557;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2553 = _M0L4selfS693->$1;
  moonbit_incref(_M0L4selfS693);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2555
  = _M0MPC15array5Array6bufferGRPC16string10StringViewE(_M0L4selfS693);
  _M0L6_2atmpS4575 = Moonbit_array_length(_M0L6_2atmpS2555);
  moonbit_decref(_M0L6_2atmpS2555);
  _M0L6_2atmpS2554 = _M0L6_2atmpS4575;
  if (_M0L3lenS2553 == _M0L6_2atmpS2554) {
    moonbit_incref(_M0L4selfS693);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC16string10StringViewE(_M0L4selfS693);
  }
  _M0L6lengthS694 = _M0L4selfS693->$1;
  _M0L8_2afieldS4574 = _M0L4selfS693->$0;
  _M0L3bufS2556 = _M0L8_2afieldS4574;
  _M0L6_2aoldS4573 = _M0L3bufS2556[_M0L6lengthS694];
  moonbit_decref(_M0L6_2aoldS4573.$0);
  _M0L3bufS2556[_M0L6lengthS694] = _M0L5valueS695;
  _M0L6_2atmpS2557 = _M0L6lengthS694 + 1;
  _M0L4selfS693->$1 = _M0L6_2atmpS2557;
  moonbit_decref(_M0L4selfS693);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS673) {
  int32_t _M0L8old__capS672;
  int32_t _M0L8new__capS674;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS672 = _M0L4selfS673->$1;
  if (_M0L8old__capS672 == 0) {
    _M0L8new__capS674 = 8;
  } else {
    _M0L8new__capS674 = _M0L8old__capS672 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS673, _M0L8new__capS674);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS676
) {
  int32_t _M0L8old__capS675;
  int32_t _M0L8new__capS677;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS675 = _M0L4selfS676->$1;
  if (_M0L8old__capS675 == 0) {
    _M0L8new__capS677 = 8;
  } else {
    _M0L8new__capS677 = _M0L8old__capS675 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS676, _M0L8new__capS677);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS679
) {
  int32_t _M0L8old__capS678;
  int32_t _M0L8new__capS680;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS678 = _M0L4selfS679->$1;
  if (_M0L8old__capS678 == 0) {
    _M0L8new__capS680 = 8;
  } else {
    _M0L8new__capS680 = _M0L8old__capS678 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS679, _M0L8new__capS680);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS682
) {
  int32_t _M0L8old__capS681;
  int32_t _M0L8new__capS683;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS681 = _M0L4selfS682->$1;
  if (_M0L8old__capS681 == 0) {
    _M0L8new__capS683 = 8;
  } else {
    _M0L8new__capS683 = _M0L8old__capS681 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(_M0L4selfS682, _M0L8new__capS683);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS651,
  int32_t _M0L13new__capacityS649
) {
  moonbit_string_t* _M0L8new__bufS648;
  moonbit_string_t* _M0L8_2afieldS4577;
  moonbit_string_t* _M0L8old__bufS650;
  int32_t _M0L8old__capS652;
  int32_t _M0L9copy__lenS653;
  moonbit_string_t* _M0L6_2aoldS4576;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS648
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS649, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4577 = _M0L4selfS651->$0;
  _M0L8old__bufS650 = _M0L8_2afieldS4577;
  _M0L8old__capS652 = Moonbit_array_length(_M0L8old__bufS650);
  if (_M0L8old__capS652 < _M0L13new__capacityS649) {
    _M0L9copy__lenS653 = _M0L8old__capS652;
  } else {
    _M0L9copy__lenS653 = _M0L13new__capacityS649;
  }
  moonbit_incref(_M0L8old__bufS650);
  moonbit_incref(_M0L8new__bufS648);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS648, 0, _M0L8old__bufS650, 0, _M0L9copy__lenS653);
  _M0L6_2aoldS4576 = _M0L4selfS651->$0;
  moonbit_decref(_M0L6_2aoldS4576);
  _M0L4selfS651->$0 = _M0L8new__bufS648;
  moonbit_decref(_M0L4selfS651);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS657,
  int32_t _M0L13new__capacityS655
) {
  struct _M0TUsiE** _M0L8new__bufS654;
  struct _M0TUsiE** _M0L8_2afieldS4579;
  struct _M0TUsiE** _M0L8old__bufS656;
  int32_t _M0L8old__capS658;
  int32_t _M0L9copy__lenS659;
  struct _M0TUsiE** _M0L6_2aoldS4578;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS654
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS655, 0);
  _M0L8_2afieldS4579 = _M0L4selfS657->$0;
  _M0L8old__bufS656 = _M0L8_2afieldS4579;
  _M0L8old__capS658 = Moonbit_array_length(_M0L8old__bufS656);
  if (_M0L8old__capS658 < _M0L13new__capacityS655) {
    _M0L9copy__lenS659 = _M0L8old__capS658;
  } else {
    _M0L9copy__lenS659 = _M0L13new__capacityS655;
  }
  moonbit_incref(_M0L8old__bufS656);
  moonbit_incref(_M0L8new__bufS654);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS654, 0, _M0L8old__bufS656, 0, _M0L9copy__lenS659);
  _M0L6_2aoldS4578 = _M0L4selfS657->$0;
  moonbit_decref(_M0L6_2aoldS4578);
  _M0L4selfS657->$0 = _M0L8new__bufS654;
  moonbit_decref(_M0L4selfS657);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS663,
  int32_t _M0L13new__capacityS661
) {
  void** _M0L8new__bufS660;
  void** _M0L8_2afieldS4581;
  void** _M0L8old__bufS662;
  int32_t _M0L8old__capS664;
  int32_t _M0L9copy__lenS665;
  void** _M0L6_2aoldS4580;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS660
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS661, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4581 = _M0L4selfS663->$0;
  _M0L8old__bufS662 = _M0L8_2afieldS4581;
  _M0L8old__capS664 = Moonbit_array_length(_M0L8old__bufS662);
  if (_M0L8old__capS664 < _M0L13new__capacityS661) {
    _M0L9copy__lenS665 = _M0L8old__capS664;
  } else {
    _M0L9copy__lenS665 = _M0L13new__capacityS661;
  }
  moonbit_incref(_M0L8old__bufS662);
  moonbit_incref(_M0L8new__bufS660);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS660, 0, _M0L8old__bufS662, 0, _M0L9copy__lenS665);
  _M0L6_2aoldS4580 = _M0L4selfS663->$0;
  moonbit_decref(_M0L6_2aoldS4580);
  _M0L4selfS663->$0 = _M0L8new__bufS660;
  moonbit_decref(_M0L4selfS663);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS669,
  int32_t _M0L13new__capacityS667
) {
  struct _M0TPC16string10StringView* _M0L8new__bufS666;
  struct _M0TPC16string10StringView* _M0L8_2afieldS4583;
  struct _M0TPC16string10StringView* _M0L8old__bufS668;
  int32_t _M0L8old__capS670;
  int32_t _M0L9copy__lenS671;
  struct _M0TPC16string10StringView* _M0L6_2aoldS4582;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS666
  = (struct _M0TPC16string10StringView*)moonbit_make_ref_valtype_array(_M0L13new__capacityS667, sizeof(struct _M0TPC16string10StringView), Moonbit_make_regular_object_header(offsetof(struct _M0TPC16string10StringView, $0) >> 2, 1, 0), &(struct _M0TPC16string10StringView){0, 0, (moonbit_string_t)moonbit_string_literal_0.data});
  _M0L8_2afieldS4583 = _M0L4selfS669->$0;
  _M0L8old__bufS668 = _M0L8_2afieldS4583;
  _M0L8old__capS670 = Moonbit_array_length(_M0L8old__bufS668);
  if (_M0L8old__capS670 < _M0L13new__capacityS667) {
    _M0L9copy__lenS671 = _M0L8old__capS670;
  } else {
    _M0L9copy__lenS671 = _M0L13new__capacityS667;
  }
  moonbit_incref(_M0L8old__bufS668);
  moonbit_incref(_M0L8new__bufS666);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(_M0L8new__bufS666, 0, _M0L8old__bufS668, 0, _M0L9copy__lenS671);
  _M0L6_2aoldS4582 = _M0L4selfS669->$0;
  moonbit_decref(_M0L6_2aoldS4582);
  _M0L4selfS669->$0 = _M0L8new__bufS666;
  moonbit_decref(_M0L4selfS669);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS647
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS647 == 0) {
    moonbit_string_t* _M0L6_2atmpS2536 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5095 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5095)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_5095->$0 = _M0L6_2atmpS2536;
    _block_5095->$1 = 0;
    return _block_5095;
  } else {
    moonbit_string_t* _M0L6_2atmpS2537 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS647, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_5096 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5096)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_5096->$0 = _M0L6_2atmpS2537;
    _block_5096->$1 = 0;
    return _block_5096;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS641,
  int32_t _M0L1nS640
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS640 <= 0) {
    moonbit_decref(_M0L4selfS641);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS640 == 1) {
    return _M0L4selfS641;
  } else {
    int32_t _M0L3lenS642 = Moonbit_array_length(_M0L4selfS641);
    int32_t _M0L6_2atmpS2535 = _M0L3lenS642 * _M0L1nS640;
    struct _M0TPB13StringBuilder* _M0L3bufS643;
    moonbit_string_t _M0L3strS644;
    int32_t _M0L2__S645;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS643 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2535);
    _M0L3strS644 = _M0L4selfS641;
    _M0L2__S645 = 0;
    while (1) {
      if (_M0L2__S645 < _M0L1nS640) {
        int32_t _M0L6_2atmpS2534;
        moonbit_incref(_M0L3strS644);
        moonbit_incref(_M0L3bufS643);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS643, _M0L3strS644);
        _M0L6_2atmpS2534 = _M0L2__S645 + 1;
        _M0L2__S645 = _M0L6_2atmpS2534;
        continue;
      } else {
        moonbit_decref(_M0L3strS644);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS643);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS638,
  struct _M0TPC16string10StringView _M0L3strS639
) {
  int32_t _M0L3lenS2522;
  int32_t _M0L6_2atmpS2524;
  int32_t _M0L6_2atmpS2523;
  int32_t _M0L6_2atmpS2521;
  moonbit_bytes_t _M0L8_2afieldS4584;
  moonbit_bytes_t _M0L4dataS2525;
  int32_t _M0L3lenS2526;
  moonbit_string_t _M0L6_2atmpS2527;
  int32_t _M0L6_2atmpS2528;
  int32_t _M0L6_2atmpS2529;
  int32_t _M0L3lenS2531;
  int32_t _M0L6_2atmpS2533;
  int32_t _M0L6_2atmpS2532;
  int32_t _M0L6_2atmpS2530;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2522 = _M0L4selfS638->$1;
  moonbit_incref(_M0L3strS639.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2524 = _M0MPC16string10StringView6length(_M0L3strS639);
  _M0L6_2atmpS2523 = _M0L6_2atmpS2524 * 2;
  _M0L6_2atmpS2521 = _M0L3lenS2522 + _M0L6_2atmpS2523;
  moonbit_incref(_M0L4selfS638);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS638, _M0L6_2atmpS2521);
  _M0L8_2afieldS4584 = _M0L4selfS638->$0;
  _M0L4dataS2525 = _M0L8_2afieldS4584;
  _M0L3lenS2526 = _M0L4selfS638->$1;
  moonbit_incref(_M0L4dataS2525);
  moonbit_incref(_M0L3strS639.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2527 = _M0MPC16string10StringView4data(_M0L3strS639);
  moonbit_incref(_M0L3strS639.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2528 = _M0MPC16string10StringView13start__offset(_M0L3strS639);
  moonbit_incref(_M0L3strS639.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2529 = _M0MPC16string10StringView6length(_M0L3strS639);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2525, _M0L3lenS2526, _M0L6_2atmpS2527, _M0L6_2atmpS2528, _M0L6_2atmpS2529);
  _M0L3lenS2531 = _M0L4selfS638->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2533 = _M0MPC16string10StringView6length(_M0L3strS639);
  _M0L6_2atmpS2532 = _M0L6_2atmpS2533 * 2;
  _M0L6_2atmpS2530 = _M0L3lenS2531 + _M0L6_2atmpS2532;
  _M0L4selfS638->$1 = _M0L6_2atmpS2530;
  moonbit_decref(_M0L4selfS638);
  return 0;
}

void* _M0IPC16string10StringViewPB6ToJson8to__json(
  struct _M0TPC16string10StringView _M0L4selfS637
) {
  moonbit_string_t _M0L6_2atmpS2520;
  #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6_2atmpS2520
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L4selfS637);
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS2520);
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS634,
  int32_t _M0L1iS635,
  int32_t _M0L13start__offsetS636,
  int64_t _M0L11end__offsetS632
) {
  int32_t _M0L11end__offsetS631;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS632 == 4294967296ll) {
    _M0L11end__offsetS631 = Moonbit_array_length(_M0L4selfS634);
  } else {
    int64_t _M0L7_2aSomeS633 = _M0L11end__offsetS632;
    _M0L11end__offsetS631 = (int32_t)_M0L7_2aSomeS633;
  }
  if (_M0L1iS635 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS634, _M0L1iS635, _M0L13start__offsetS636, _M0L11end__offsetS631);
  } else {
    int32_t _M0L6_2atmpS2519 = -_M0L1iS635;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS634, _M0L6_2atmpS2519, _M0L13start__offsetS636, _M0L11end__offsetS631);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS629,
  int32_t _M0L1nS627,
  int32_t _M0L13start__offsetS623,
  int32_t _M0L11end__offsetS624
) {
  int32_t _if__result_5098;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS623 >= 0) {
    _if__result_5098 = _M0L13start__offsetS623 <= _M0L11end__offsetS624;
  } else {
    _if__result_5098 = 0;
  }
  if (_if__result_5098) {
    int32_t _M0Lm13utf16__offsetS625 = _M0L13start__offsetS623;
    int32_t _M0Lm11char__countS626 = 0;
    int32_t _M0L6_2atmpS2517;
    int32_t _if__result_5101;
    while (1) {
      int32_t _M0L6_2atmpS2511 = _M0Lm13utf16__offsetS625;
      int32_t _if__result_5100;
      if (_M0L6_2atmpS2511 < _M0L11end__offsetS624) {
        int32_t _M0L6_2atmpS2510 = _M0Lm11char__countS626;
        _if__result_5100 = _M0L6_2atmpS2510 < _M0L1nS627;
      } else {
        _if__result_5100 = 0;
      }
      if (_if__result_5100) {
        int32_t _M0L6_2atmpS2515 = _M0Lm13utf16__offsetS625;
        int32_t _M0L1cS628 = _M0L4selfS629[_M0L6_2atmpS2515];
        int32_t _M0L6_2atmpS2514;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS628)) {
          int32_t _M0L6_2atmpS2512 = _M0Lm13utf16__offsetS625;
          _M0Lm13utf16__offsetS625 = _M0L6_2atmpS2512 + 2;
        } else {
          int32_t _M0L6_2atmpS2513 = _M0Lm13utf16__offsetS625;
          _M0Lm13utf16__offsetS625 = _M0L6_2atmpS2513 + 1;
        }
        _M0L6_2atmpS2514 = _M0Lm11char__countS626;
        _M0Lm11char__countS626 = _M0L6_2atmpS2514 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS629);
      }
      break;
    }
    _M0L6_2atmpS2517 = _M0Lm11char__countS626;
    if (_M0L6_2atmpS2517 < _M0L1nS627) {
      _if__result_5101 = 1;
    } else {
      int32_t _M0L6_2atmpS2516 = _M0Lm13utf16__offsetS625;
      _if__result_5101 = _M0L6_2atmpS2516 >= _M0L11end__offsetS624;
    }
    if (_if__result_5101) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2518 = _M0Lm13utf16__offsetS625;
      return (int64_t)_M0L6_2atmpS2518;
    }
  } else {
    moonbit_decref(_M0L4selfS629);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_90.data, (moonbit_string_t)moonbit_string_literal_91.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS621,
  int32_t _M0L1nS619,
  int32_t _M0L13start__offsetS618,
  int32_t _M0L11end__offsetS617
) {
  int32_t _M0Lm11char__countS615;
  int32_t _M0Lm13utf16__offsetS616;
  int32_t _M0L6_2atmpS2508;
  int32_t _if__result_5104;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS615 = 0;
  _M0Lm13utf16__offsetS616 = _M0L11end__offsetS617;
  while (1) {
    int32_t _M0L6_2atmpS2501 = _M0Lm13utf16__offsetS616;
    int32_t _M0L6_2atmpS2500 = _M0L6_2atmpS2501 - 1;
    int32_t _if__result_5103;
    if (_M0L6_2atmpS2500 >= _M0L13start__offsetS618) {
      int32_t _M0L6_2atmpS2499 = _M0Lm11char__countS615;
      _if__result_5103 = _M0L6_2atmpS2499 < _M0L1nS619;
    } else {
      _if__result_5103 = 0;
    }
    if (_if__result_5103) {
      int32_t _M0L6_2atmpS2506 = _M0Lm13utf16__offsetS616;
      int32_t _M0L6_2atmpS2505 = _M0L6_2atmpS2506 - 1;
      int32_t _M0L1cS620 = _M0L4selfS621[_M0L6_2atmpS2505];
      int32_t _M0L6_2atmpS2504;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS620)) {
        int32_t _M0L6_2atmpS2502 = _M0Lm13utf16__offsetS616;
        _M0Lm13utf16__offsetS616 = _M0L6_2atmpS2502 - 2;
      } else {
        int32_t _M0L6_2atmpS2503 = _M0Lm13utf16__offsetS616;
        _M0Lm13utf16__offsetS616 = _M0L6_2atmpS2503 - 1;
      }
      _M0L6_2atmpS2504 = _M0Lm11char__countS615;
      _M0Lm11char__countS615 = _M0L6_2atmpS2504 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS621);
    }
    break;
  }
  _M0L6_2atmpS2508 = _M0Lm11char__countS615;
  if (_M0L6_2atmpS2508 < _M0L1nS619) {
    _if__result_5104 = 1;
  } else {
    int32_t _M0L6_2atmpS2507 = _M0Lm13utf16__offsetS616;
    _if__result_5104 = _M0L6_2atmpS2507 < _M0L13start__offsetS618;
  }
  if (_if__result_5104) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2509 = _M0Lm13utf16__offsetS616;
    return (int64_t)_M0L6_2atmpS2509;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS607,
  int32_t _M0L3lenS610,
  int32_t _M0L13start__offsetS614,
  int64_t _M0L11end__offsetS605
) {
  int32_t _M0L11end__offsetS604;
  int32_t _M0L5indexS608;
  int32_t _M0L5countS609;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS605 == 4294967296ll) {
    _M0L11end__offsetS604 = Moonbit_array_length(_M0L4selfS607);
  } else {
    int64_t _M0L7_2aSomeS606 = _M0L11end__offsetS605;
    _M0L11end__offsetS604 = (int32_t)_M0L7_2aSomeS606;
  }
  _M0L5indexS608 = _M0L13start__offsetS614;
  _M0L5countS609 = 0;
  while (1) {
    int32_t _if__result_5106;
    if (_M0L5indexS608 < _M0L11end__offsetS604) {
      _if__result_5106 = _M0L5countS609 < _M0L3lenS610;
    } else {
      _if__result_5106 = 0;
    }
    if (_if__result_5106) {
      int32_t _M0L2c1S611 = _M0L4selfS607[_M0L5indexS608];
      int32_t _if__result_5107;
      int32_t _M0L6_2atmpS2497;
      int32_t _M0L6_2atmpS2498;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S611)) {
        int32_t _M0L6_2atmpS2493 = _M0L5indexS608 + 1;
        _if__result_5107 = _M0L6_2atmpS2493 < _M0L11end__offsetS604;
      } else {
        _if__result_5107 = 0;
      }
      if (_if__result_5107) {
        int32_t _M0L6_2atmpS2496 = _M0L5indexS608 + 1;
        int32_t _M0L2c2S612 = _M0L4selfS607[_M0L6_2atmpS2496];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S612)) {
          int32_t _M0L6_2atmpS2494 = _M0L5indexS608 + 2;
          int32_t _M0L6_2atmpS2495 = _M0L5countS609 + 1;
          _M0L5indexS608 = _M0L6_2atmpS2494;
          _M0L5countS609 = _M0L6_2atmpS2495;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_92.data, (moonbit_string_t)moonbit_string_literal_93.data);
        }
      }
      _M0L6_2atmpS2497 = _M0L5indexS608 + 1;
      _M0L6_2atmpS2498 = _M0L5countS609 + 1;
      _M0L5indexS608 = _M0L6_2atmpS2497;
      _M0L5countS609 = _M0L6_2atmpS2498;
      continue;
    } else {
      moonbit_decref(_M0L4selfS607);
      return _M0L5countS609 >= _M0L3lenS610;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS596,
  int32_t _M0L3lenS599,
  int32_t _M0L13start__offsetS603,
  int64_t _M0L11end__offsetS594
) {
  int32_t _M0L11end__offsetS593;
  int32_t _M0L5indexS597;
  int32_t _M0L5countS598;
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS594 == 4294967296ll) {
    _M0L11end__offsetS593 = Moonbit_array_length(_M0L4selfS596);
  } else {
    int64_t _M0L7_2aSomeS595 = _M0L11end__offsetS594;
    _M0L11end__offsetS593 = (int32_t)_M0L7_2aSomeS595;
  }
  _M0L5indexS597 = _M0L13start__offsetS603;
  _M0L5countS598 = 0;
  while (1) {
    int32_t _if__result_5109;
    if (_M0L5indexS597 < _M0L11end__offsetS593) {
      _if__result_5109 = _M0L5countS598 < _M0L3lenS599;
    } else {
      _if__result_5109 = 0;
    }
    if (_if__result_5109) {
      int32_t _M0L2c1S600 = _M0L4selfS596[_M0L5indexS597];
      int32_t _if__result_5110;
      int32_t _M0L6_2atmpS2491;
      int32_t _M0L6_2atmpS2492;
      #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S600)) {
        int32_t _M0L6_2atmpS2487 = _M0L5indexS597 + 1;
        _if__result_5110 = _M0L6_2atmpS2487 < _M0L11end__offsetS593;
      } else {
        _if__result_5110 = 0;
      }
      if (_if__result_5110) {
        int32_t _M0L6_2atmpS2490 = _M0L5indexS597 + 1;
        int32_t _M0L2c2S601 = _M0L4selfS596[_M0L6_2atmpS2490];
        #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S601)) {
          int32_t _M0L6_2atmpS2488 = _M0L5indexS597 + 2;
          int32_t _M0L6_2atmpS2489 = _M0L5countS598 + 1;
          _M0L5indexS597 = _M0L6_2atmpS2488;
          _M0L5countS598 = _M0L6_2atmpS2489;
          continue;
        } else {
          #line 426 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_92.data, (moonbit_string_t)moonbit_string_literal_94.data);
        }
      }
      _M0L6_2atmpS2491 = _M0L5indexS597 + 1;
      _M0L6_2atmpS2492 = _M0L5countS598 + 1;
      _M0L5indexS597 = _M0L6_2atmpS2491;
      _M0L5countS598 = _M0L6_2atmpS2492;
      continue;
    } else {
      moonbit_decref(_M0L4selfS596);
      if (_M0L5countS598 == _M0L3lenS599) {
        return _M0L5indexS597 == _M0L11end__offsetS593;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS589
) {
  int32_t _M0L3endS2479;
  int32_t _M0L8_2afieldS4585;
  int32_t _M0L5startS2480;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2479 = _M0L4selfS589.$2;
  _M0L8_2afieldS4585 = _M0L4selfS589.$1;
  moonbit_decref(_M0L4selfS589.$0);
  _M0L5startS2480 = _M0L8_2afieldS4585;
  return _M0L3endS2479 - _M0L5startS2480;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS590
) {
  int32_t _M0L3endS2481;
  int32_t _M0L8_2afieldS4586;
  int32_t _M0L5startS2482;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2481 = _M0L4selfS590.$2;
  _M0L8_2afieldS4586 = _M0L4selfS590.$1;
  moonbit_decref(_M0L4selfS590.$0);
  _M0L5startS2482 = _M0L8_2afieldS4586;
  return _M0L3endS2481 - _M0L5startS2482;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS591
) {
  int32_t _M0L3endS2483;
  int32_t _M0L8_2afieldS4587;
  int32_t _M0L5startS2484;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2483 = _M0L4selfS591.$2;
  _M0L8_2afieldS4587 = _M0L4selfS591.$1;
  moonbit_decref(_M0L4selfS591.$0);
  _M0L5startS2484 = _M0L8_2afieldS4587;
  return _M0L3endS2483 - _M0L5startS2484;
}

int32_t _M0MPC15array9ArrayView6lengthGRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGRPC16string10StringViewE _M0L4selfS592
) {
  int32_t _M0L3endS2485;
  int32_t _M0L8_2afieldS4588;
  int32_t _M0L5startS2486;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2485 = _M0L4selfS592.$2;
  _M0L8_2afieldS4588 = _M0L4selfS592.$1;
  moonbit_decref(_M0L4selfS592.$0);
  _M0L5startS2486 = _M0L8_2afieldS4588;
  return _M0L3endS2485 - _M0L5startS2486;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS587,
  int64_t _M0L19start__offset_2eoptS585,
  int64_t _M0L11end__offsetS588
) {
  int32_t _M0L13start__offsetS584;
  if (_M0L19start__offset_2eoptS585 == 4294967296ll) {
    _M0L13start__offsetS584 = 0;
  } else {
    int64_t _M0L7_2aSomeS586 = _M0L19start__offset_2eoptS585;
    _M0L13start__offsetS584 = (int32_t)_M0L7_2aSomeS586;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS587, _M0L13start__offsetS584, _M0L11end__offsetS588);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS582,
  int32_t _M0L13start__offsetS583,
  int64_t _M0L11end__offsetS580
) {
  int32_t _M0L11end__offsetS579;
  int32_t _if__result_5111;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS580 == 4294967296ll) {
    _M0L11end__offsetS579 = Moonbit_array_length(_M0L4selfS582);
  } else {
    int64_t _M0L7_2aSomeS581 = _M0L11end__offsetS580;
    _M0L11end__offsetS579 = (int32_t)_M0L7_2aSomeS581;
  }
  if (_M0L13start__offsetS583 >= 0) {
    if (_M0L13start__offsetS583 <= _M0L11end__offsetS579) {
      int32_t _M0L6_2atmpS2478 = Moonbit_array_length(_M0L4selfS582);
      _if__result_5111 = _M0L11end__offsetS579 <= _M0L6_2atmpS2478;
    } else {
      _if__result_5111 = 0;
    }
  } else {
    _if__result_5111 = 0;
  }
  if (_if__result_5111) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS583,
                                                 _M0L11end__offsetS579,
                                                 _M0L4selfS582};
  } else {
    moonbit_decref(_M0L4selfS582);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_95.data, (moonbit_string_t)moonbit_string_literal_96.data);
  }
}

int32_t _M0IPC16string10StringViewPB2Eq5equal(
  struct _M0TPC16string10StringView _M0L4selfS575,
  struct _M0TPC16string10StringView _M0L5otherS576
) {
  int32_t _M0L3lenS574;
  int32_t _M0L6_2atmpS2464;
  #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  moonbit_incref(_M0L4selfS575.$0);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS574 = _M0MPC16string10StringView6length(_M0L4selfS575);
  moonbit_incref(_M0L5otherS576.$0);
  #line 271 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6_2atmpS2464 = _M0MPC16string10StringView6length(_M0L5otherS576);
  if (_M0L3lenS574 == _M0L6_2atmpS2464) {
    moonbit_string_t _M0L8_2afieldS4595 = _M0L4selfS575.$0;
    moonbit_string_t _M0L3strS2467 = _M0L8_2afieldS4595;
    moonbit_string_t _M0L8_2afieldS4594 = _M0L5otherS576.$0;
    moonbit_string_t _M0L3strS2468 = _M0L8_2afieldS4594;
    int32_t _M0L6_2atmpS4593 = _M0L3strS2467 == _M0L3strS2468;
    int32_t _if__result_5112;
    int32_t _M0L1iS577;
    if (_M0L6_2atmpS4593) {
      int32_t _M0L5startS2465 = _M0L4selfS575.$1;
      int32_t _M0L5startS2466 = _M0L5otherS576.$1;
      _if__result_5112 = _M0L5startS2465 == _M0L5startS2466;
    } else {
      _if__result_5112 = 0;
    }
    if (_if__result_5112) {
      moonbit_decref(_M0L5otherS576.$0);
      moonbit_decref(_M0L4selfS575.$0);
      return 1;
    }
    _M0L1iS577 = 0;
    while (1) {
      if (_M0L1iS577 < _M0L3lenS574) {
        moonbit_string_t _M0L8_2afieldS4592 = _M0L4selfS575.$0;
        moonbit_string_t _M0L3strS2474 = _M0L8_2afieldS4592;
        int32_t _M0L5startS2476 = _M0L4selfS575.$1;
        int32_t _M0L6_2atmpS2475 = _M0L5startS2476 + _M0L1iS577;
        int32_t _M0L6_2atmpS4591 = _M0L3strS2474[_M0L6_2atmpS2475];
        int32_t _M0L6_2atmpS2469 = _M0L6_2atmpS4591;
        moonbit_string_t _M0L8_2afieldS4590 = _M0L5otherS576.$0;
        moonbit_string_t _M0L3strS2471 = _M0L8_2afieldS4590;
        int32_t _M0L5startS2473 = _M0L5otherS576.$1;
        int32_t _M0L6_2atmpS2472 = _M0L5startS2473 + _M0L1iS577;
        int32_t _M0L6_2atmpS4589 = _M0L3strS2471[_M0L6_2atmpS2472];
        int32_t _M0L6_2atmpS2470 = _M0L6_2atmpS4589;
        int32_t _M0L6_2atmpS2477;
        if (_M0L6_2atmpS2469 == _M0L6_2atmpS2470) {
          
        } else {
          moonbit_decref(_M0L5otherS576.$0);
          moonbit_decref(_M0L4selfS575.$0);
          return 0;
        }
        _M0L6_2atmpS2477 = _M0L1iS577 + 1;
        _M0L1iS577 = _M0L6_2atmpS2477;
        continue;
      } else {
        moonbit_decref(_M0L5otherS576.$0);
        moonbit_decref(_M0L4selfS575.$0);
      }
      break;
    }
    return 1;
  } else {
    moonbit_decref(_M0L5otherS576.$0);
    moonbit_decref(_M0L4selfS575.$0);
    return 0;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS573
) {
  moonbit_string_t _M0L8_2afieldS4597;
  moonbit_string_t _M0L3strS2461;
  int32_t _M0L5startS2462;
  int32_t _M0L8_2afieldS4596;
  int32_t _M0L3endS2463;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4597 = _M0L4selfS573.$0;
  _M0L3strS2461 = _M0L8_2afieldS4597;
  _M0L5startS2462 = _M0L4selfS573.$1;
  _M0L8_2afieldS4596 = _M0L4selfS573.$2;
  _M0L3endS2463 = _M0L8_2afieldS4596;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2461, _M0L5startS2462, _M0L3endS2463);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS571,
  struct _M0TPB6Logger _M0L6loggerS572
) {
  moonbit_string_t _M0L8_2afieldS4599;
  moonbit_string_t _M0L3strS2458;
  int32_t _M0L5startS2459;
  int32_t _M0L8_2afieldS4598;
  int32_t _M0L3endS2460;
  moonbit_string_t _M0L6substrS570;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4599 = _M0L4selfS571.$0;
  _M0L3strS2458 = _M0L8_2afieldS4599;
  _M0L5startS2459 = _M0L4selfS571.$1;
  _M0L8_2afieldS4598 = _M0L4selfS571.$2;
  _M0L3endS2460 = _M0L8_2afieldS4598;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS570
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2458, _M0L5startS2459, _M0L3endS2460);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS570, _M0L6loggerS572);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS562,
  struct _M0TPB6Logger _M0L6loggerS560
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS561;
  int32_t _M0L3lenS563;
  int32_t _M0L1iS564;
  int32_t _M0L3segS565;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS560.$1) {
    moonbit_incref(_M0L6loggerS560.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS560.$0->$method_3(_M0L6loggerS560.$1, 34);
  moonbit_incref(_M0L4selfS562);
  if (_M0L6loggerS560.$1) {
    moonbit_incref(_M0L6loggerS560.$1);
  }
  _M0L6_2aenvS561
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS561)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS561->$0 = _M0L4selfS562;
  _M0L6_2aenvS561->$1_0 = _M0L6loggerS560.$0;
  _M0L6_2aenvS561->$1_1 = _M0L6loggerS560.$1;
  _M0L3lenS563 = Moonbit_array_length(_M0L4selfS562);
  _M0L1iS564 = 0;
  _M0L3segS565 = 0;
  _2afor_566:;
  while (1) {
    int32_t _M0L4codeS567;
    int32_t _M0L1cS569;
    int32_t _M0L6_2atmpS2442;
    int32_t _M0L6_2atmpS2443;
    int32_t _M0L6_2atmpS2444;
    int32_t _tmp_5117;
    int32_t _tmp_5118;
    if (_M0L1iS564 >= _M0L3lenS563) {
      moonbit_decref(_M0L4selfS562);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
      break;
    }
    _M0L4codeS567 = _M0L4selfS562[_M0L1iS564];
    switch (_M0L4codeS567) {
      case 34: {
        _M0L1cS569 = _M0L4codeS567;
        goto join_568;
        break;
      }
      
      case 92: {
        _M0L1cS569 = _M0L4codeS567;
        goto join_568;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2445;
        int32_t _M0L6_2atmpS2446;
        moonbit_incref(_M0L6_2aenvS561);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
        if (_M0L6loggerS560.$1) {
          moonbit_incref(_M0L6loggerS560.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS560.$0->$method_0(_M0L6loggerS560.$1, (moonbit_string_t)moonbit_string_literal_71.data);
        _M0L6_2atmpS2445 = _M0L1iS564 + 1;
        _M0L6_2atmpS2446 = _M0L1iS564 + 1;
        _M0L1iS564 = _M0L6_2atmpS2445;
        _M0L3segS565 = _M0L6_2atmpS2446;
        goto _2afor_566;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2447;
        int32_t _M0L6_2atmpS2448;
        moonbit_incref(_M0L6_2aenvS561);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
        if (_M0L6loggerS560.$1) {
          moonbit_incref(_M0L6loggerS560.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS560.$0->$method_0(_M0L6loggerS560.$1, (moonbit_string_t)moonbit_string_literal_72.data);
        _M0L6_2atmpS2447 = _M0L1iS564 + 1;
        _M0L6_2atmpS2448 = _M0L1iS564 + 1;
        _M0L1iS564 = _M0L6_2atmpS2447;
        _M0L3segS565 = _M0L6_2atmpS2448;
        goto _2afor_566;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2449;
        int32_t _M0L6_2atmpS2450;
        moonbit_incref(_M0L6_2aenvS561);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
        if (_M0L6loggerS560.$1) {
          moonbit_incref(_M0L6loggerS560.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS560.$0->$method_0(_M0L6loggerS560.$1, (moonbit_string_t)moonbit_string_literal_73.data);
        _M0L6_2atmpS2449 = _M0L1iS564 + 1;
        _M0L6_2atmpS2450 = _M0L1iS564 + 1;
        _M0L1iS564 = _M0L6_2atmpS2449;
        _M0L3segS565 = _M0L6_2atmpS2450;
        goto _2afor_566;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2451;
        int32_t _M0L6_2atmpS2452;
        moonbit_incref(_M0L6_2aenvS561);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
        if (_M0L6loggerS560.$1) {
          moonbit_incref(_M0L6loggerS560.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS560.$0->$method_0(_M0L6loggerS560.$1, (moonbit_string_t)moonbit_string_literal_74.data);
        _M0L6_2atmpS2451 = _M0L1iS564 + 1;
        _M0L6_2atmpS2452 = _M0L1iS564 + 1;
        _M0L1iS564 = _M0L6_2atmpS2451;
        _M0L3segS565 = _M0L6_2atmpS2452;
        goto _2afor_566;
        break;
      }
      default: {
        if (_M0L4codeS567 < 32) {
          int32_t _M0L6_2atmpS2454;
          moonbit_string_t _M0L6_2atmpS2453;
          int32_t _M0L6_2atmpS2455;
          int32_t _M0L6_2atmpS2456;
          moonbit_incref(_M0L6_2aenvS561);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
          if (_M0L6loggerS560.$1) {
            moonbit_incref(_M0L6loggerS560.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS560.$0->$method_0(_M0L6loggerS560.$1, (moonbit_string_t)moonbit_string_literal_97.data);
          _M0L6_2atmpS2454 = _M0L4codeS567 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2453 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2454);
          if (_M0L6loggerS560.$1) {
            moonbit_incref(_M0L6loggerS560.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS560.$0->$method_0(_M0L6loggerS560.$1, _M0L6_2atmpS2453);
          if (_M0L6loggerS560.$1) {
            moonbit_incref(_M0L6loggerS560.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS560.$0->$method_3(_M0L6loggerS560.$1, 125);
          _M0L6_2atmpS2455 = _M0L1iS564 + 1;
          _M0L6_2atmpS2456 = _M0L1iS564 + 1;
          _M0L1iS564 = _M0L6_2atmpS2455;
          _M0L3segS565 = _M0L6_2atmpS2456;
          goto _2afor_566;
        } else {
          int32_t _M0L6_2atmpS2457 = _M0L1iS564 + 1;
          int32_t _tmp_5116 = _M0L3segS565;
          _M0L1iS564 = _M0L6_2atmpS2457;
          _M0L3segS565 = _tmp_5116;
          goto _2afor_566;
        }
        break;
      }
    }
    goto joinlet_5115;
    join_568:;
    moonbit_incref(_M0L6_2aenvS561);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS561, _M0L3segS565, _M0L1iS564);
    if (_M0L6loggerS560.$1) {
      moonbit_incref(_M0L6loggerS560.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS560.$0->$method_3(_M0L6loggerS560.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2442 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS569);
    if (_M0L6loggerS560.$1) {
      moonbit_incref(_M0L6loggerS560.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS560.$0->$method_3(_M0L6loggerS560.$1, _M0L6_2atmpS2442);
    _M0L6_2atmpS2443 = _M0L1iS564 + 1;
    _M0L6_2atmpS2444 = _M0L1iS564 + 1;
    _M0L1iS564 = _M0L6_2atmpS2443;
    _M0L3segS565 = _M0L6_2atmpS2444;
    continue;
    joinlet_5115:;
    _tmp_5117 = _M0L1iS564;
    _tmp_5118 = _M0L3segS565;
    _M0L1iS564 = _tmp_5117;
    _M0L3segS565 = _tmp_5118;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS560.$0->$method_3(_M0L6loggerS560.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS556,
  int32_t _M0L3segS559,
  int32_t _M0L1iS558
) {
  struct _M0TPB6Logger _M0L8_2afieldS4601;
  struct _M0TPB6Logger _M0L6loggerS555;
  moonbit_string_t _M0L8_2afieldS4600;
  int32_t _M0L6_2acntS4832;
  moonbit_string_t _M0L4selfS557;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4601
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS556->$1_0, _M0L6_2aenvS556->$1_1
  };
  _M0L6loggerS555 = _M0L8_2afieldS4601;
  _M0L8_2afieldS4600 = _M0L6_2aenvS556->$0;
  _M0L6_2acntS4832 = Moonbit_object_header(_M0L6_2aenvS556)->rc;
  if (_M0L6_2acntS4832 > 1) {
    int32_t _M0L11_2anew__cntS4833 = _M0L6_2acntS4832 - 1;
    Moonbit_object_header(_M0L6_2aenvS556)->rc = _M0L11_2anew__cntS4833;
    if (_M0L6loggerS555.$1) {
      moonbit_incref(_M0L6loggerS555.$1);
    }
    moonbit_incref(_M0L8_2afieldS4600);
  } else if (_M0L6_2acntS4832 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS556);
  }
  _M0L4selfS557 = _M0L8_2afieldS4600;
  if (_M0L1iS558 > _M0L3segS559) {
    int32_t _M0L6_2atmpS2441 = _M0L1iS558 - _M0L3segS559;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS555.$0->$method_1(_M0L6loggerS555.$1, _M0L4selfS557, _M0L3segS559, _M0L6_2atmpS2441);
  } else {
    moonbit_decref(_M0L4selfS557);
    if (_M0L6loggerS555.$1) {
      moonbit_decref(_M0L6loggerS555.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS554) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS553;
  int32_t _M0L6_2atmpS2438;
  int32_t _M0L6_2atmpS2437;
  int32_t _M0L6_2atmpS2440;
  int32_t _M0L6_2atmpS2439;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2436;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS553 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2438 = _M0IPC14byte4BytePB3Div3div(_M0L1bS554, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2437
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2438);
  moonbit_incref(_M0L7_2aselfS553);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS553, _M0L6_2atmpS2437);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2440 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS554, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2439
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2440);
  moonbit_incref(_M0L7_2aselfS553);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS553, _M0L6_2atmpS2439);
  _M0L6_2atmpS2436 = _M0L7_2aselfS553;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2436);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS552) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS552 < 10) {
    int32_t _M0L6_2atmpS2433;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2433 = _M0IPC14byte4BytePB3Add3add(_M0L1iS552, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2433);
  } else {
    int32_t _M0L6_2atmpS2435;
    int32_t _M0L6_2atmpS2434;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2435 = _M0IPC14byte4BytePB3Add3add(_M0L1iS552, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2434 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2435, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2434);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS550,
  int32_t _M0L4thatS551
) {
  int32_t _M0L6_2atmpS2431;
  int32_t _M0L6_2atmpS2432;
  int32_t _M0L6_2atmpS2430;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2431 = (int32_t)_M0L4selfS550;
  _M0L6_2atmpS2432 = (int32_t)_M0L4thatS551;
  _M0L6_2atmpS2430 = _M0L6_2atmpS2431 - _M0L6_2atmpS2432;
  return _M0L6_2atmpS2430 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS548,
  int32_t _M0L4thatS549
) {
  int32_t _M0L6_2atmpS2428;
  int32_t _M0L6_2atmpS2429;
  int32_t _M0L6_2atmpS2427;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2428 = (int32_t)_M0L4selfS548;
  _M0L6_2atmpS2429 = (int32_t)_M0L4thatS549;
  _M0L6_2atmpS2427 = _M0L6_2atmpS2428 % _M0L6_2atmpS2429;
  return _M0L6_2atmpS2427 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS546,
  int32_t _M0L4thatS547
) {
  int32_t _M0L6_2atmpS2425;
  int32_t _M0L6_2atmpS2426;
  int32_t _M0L6_2atmpS2424;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2425 = (int32_t)_M0L4selfS546;
  _M0L6_2atmpS2426 = (int32_t)_M0L4thatS547;
  _M0L6_2atmpS2424 = _M0L6_2atmpS2425 / _M0L6_2atmpS2426;
  return _M0L6_2atmpS2424 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS544,
  int32_t _M0L4thatS545
) {
  int32_t _M0L6_2atmpS2422;
  int32_t _M0L6_2atmpS2423;
  int32_t _M0L6_2atmpS2421;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2422 = (int32_t)_M0L4selfS544;
  _M0L6_2atmpS2423 = (int32_t)_M0L4thatS545;
  _M0L6_2atmpS2421 = _M0L6_2atmpS2422 + _M0L6_2atmpS2423;
  return _M0L6_2atmpS2421 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS541,
  int32_t _M0L5startS539,
  int32_t _M0L3endS540
) {
  int32_t _if__result_5119;
  int32_t _M0L3lenS542;
  int32_t _M0L6_2atmpS2419;
  int32_t _M0L6_2atmpS2420;
  moonbit_bytes_t _M0L5bytesS543;
  moonbit_bytes_t _M0L6_2atmpS2418;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS539 == 0) {
    int32_t _M0L6_2atmpS2417 = Moonbit_array_length(_M0L3strS541);
    _if__result_5119 = _M0L3endS540 == _M0L6_2atmpS2417;
  } else {
    _if__result_5119 = 0;
  }
  if (_if__result_5119) {
    return _M0L3strS541;
  }
  _M0L3lenS542 = _M0L3endS540 - _M0L5startS539;
  _M0L6_2atmpS2419 = _M0L3lenS542 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2420 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS543
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2419, _M0L6_2atmpS2420);
  moonbit_incref(_M0L5bytesS543);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS543, 0, _M0L3strS541, _M0L5startS539, _M0L3lenS542);
  _M0L6_2atmpS2418 = _M0L5bytesS543;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2418, 0, 4294967296ll);
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS537,
  int32_t _M0L13start__offsetS538,
  int64_t _M0L11end__offsetS535
) {
  int32_t _M0L11end__offsetS534;
  int32_t _if__result_5120;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS535 == 4294967296ll) {
    moonbit_incref(_M0L4selfS537.$0);
    #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L11end__offsetS534 = _M0MPC16string10StringView6length(_M0L4selfS537);
  } else {
    int64_t _M0L7_2aSomeS536 = _M0L11end__offsetS535;
    _M0L11end__offsetS534 = (int32_t)_M0L7_2aSomeS536;
  }
  if (_M0L13start__offsetS538 >= 0) {
    if (_M0L13start__offsetS538 <= _M0L11end__offsetS534) {
      int32_t _M0L6_2atmpS2411;
      moonbit_incref(_M0L4selfS537.$0);
      #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2411 = _M0MPC16string10StringView6length(_M0L4selfS537);
      _if__result_5120 = _M0L11end__offsetS534 <= _M0L6_2atmpS2411;
    } else {
      _if__result_5120 = 0;
    }
  } else {
    _if__result_5120 = 0;
  }
  if (_if__result_5120) {
    moonbit_string_t _M0L8_2afieldS4603 = _M0L4selfS537.$0;
    moonbit_string_t _M0L3strS2412 = _M0L8_2afieldS4603;
    int32_t _M0L5startS2416 = _M0L4selfS537.$1;
    int32_t _M0L6_2atmpS2413 = _M0L5startS2416 + _M0L13start__offsetS538;
    int32_t _M0L8_2afieldS4602 = _M0L4selfS537.$1;
    int32_t _M0L5startS2415 = _M0L8_2afieldS4602;
    int32_t _M0L6_2atmpS2414 = _M0L5startS2415 + _M0L11end__offsetS534;
    return (struct _M0TPC16string10StringView){_M0L6_2atmpS2413,
                                                 _M0L6_2atmpS2414,
                                                 _M0L3strS2412};
  } else {
    moonbit_decref(_M0L4selfS537.$0);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_95.data, (moonbit_string_t)moonbit_string_literal_98.data);
  }
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS530) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS530;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS531
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS531;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS532) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS532;
}

struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS533
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS533;
}

struct moonbit_result_0 _M0FPB10assert__eqGRPC16string10StringViewE(
  struct _M0TPC16string10StringView _M0L1aS506,
  struct _M0TPC16string10StringView _M0L1bS507,
  moonbit_string_t _M0L3msgS509,
  moonbit_string_t _M0L3locS511
) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  moonbit_incref(_M0L1bS507.$0);
  moonbit_incref(_M0L1aS506.$0);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGRPC16string10StringViewE(_M0L1aS506, _M0L1bS507)
  ) {
    moonbit_string_t _M0L9fail__msgS508;
    if (_M0L3msgS509 == 0) {
      moonbit_string_t _M0L6_2atmpS2385;
      moonbit_string_t _M0L6_2atmpS2384;
      moonbit_string_t _M0L6_2atmpS4607;
      moonbit_string_t _M0L6_2atmpS2383;
      moonbit_string_t _M0L6_2atmpS4606;
      moonbit_string_t _M0L6_2atmpS2380;
      moonbit_string_t _M0L6_2atmpS2382;
      moonbit_string_t _M0L6_2atmpS2381;
      moonbit_string_t _M0L6_2atmpS4605;
      moonbit_string_t _M0L6_2atmpS2379;
      moonbit_string_t _M0L6_2atmpS4604;
      if (_M0L3msgS509) {
        moonbit_decref(_M0L3msgS509);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2385
      = _M0FPB13debug__stringGRPC16string10StringViewE(_M0L1aS506);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2384
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2385);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4607
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS2384);
      moonbit_decref(_M0L6_2atmpS2384);
      _M0L6_2atmpS2383 = _M0L6_2atmpS4607;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4606
      = moonbit_add_string(_M0L6_2atmpS2383, (moonbit_string_t)moonbit_string_literal_100.data);
      moonbit_decref(_M0L6_2atmpS2383);
      _M0L6_2atmpS2380 = _M0L6_2atmpS4606;
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2382
      = _M0FPB13debug__stringGRPC16string10StringViewE(_M0L1bS507);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2381
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2382);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4605
      = moonbit_add_string(_M0L6_2atmpS2380, _M0L6_2atmpS2381);
      moonbit_decref(_M0L6_2atmpS2380);
      moonbit_decref(_M0L6_2atmpS2381);
      _M0L6_2atmpS2379 = _M0L6_2atmpS4605;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4604
      = moonbit_add_string(_M0L6_2atmpS2379, (moonbit_string_t)moonbit_string_literal_99.data);
      moonbit_decref(_M0L6_2atmpS2379);
      _M0L9fail__msgS508 = _M0L6_2atmpS4604;
    } else {
      moonbit_string_t _M0L7_2aSomeS510;
      moonbit_decref(_M0L1bS507.$0);
      moonbit_decref(_M0L1aS506.$0);
      _M0L7_2aSomeS510 = _M0L3msgS509;
      _M0L9fail__msgS508 = _M0L7_2aSomeS510;
    }
    #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS508, _M0L3locS511);
  } else {
    int32_t _M0L6_2atmpS2386;
    struct moonbit_result_0 _result_5121;
    moonbit_decref(_M0L3locS511);
    if (_M0L3msgS509) {
      moonbit_decref(_M0L3msgS509);
    }
    moonbit_decref(_M0L1bS507.$0);
    moonbit_decref(_M0L1aS506.$0);
    _M0L6_2atmpS2386 = 0;
    _result_5121.tag = 1;
    _result_5121.data.ok = _M0L6_2atmpS2386;
    return _result_5121;
  }
}

struct moonbit_result_0 _M0FPB10assert__eqGORPC16string10StringViewE(
  void* _M0L1aS512,
  void* _M0L1bS513,
  moonbit_string_t _M0L3msgS515,
  moonbit_string_t _M0L3locS517
) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  moonbit_incref(_M0L1bS513);
  moonbit_incref(_M0L1aS512);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGORPC16string10StringViewE(_M0L1aS512, _M0L1bS513)
  ) {
    moonbit_string_t _M0L9fail__msgS514;
    if (_M0L3msgS515 == 0) {
      moonbit_string_t _M0L6_2atmpS2393;
      moonbit_string_t _M0L6_2atmpS2392;
      moonbit_string_t _M0L6_2atmpS4611;
      moonbit_string_t _M0L6_2atmpS2391;
      moonbit_string_t _M0L6_2atmpS4610;
      moonbit_string_t _M0L6_2atmpS2388;
      moonbit_string_t _M0L6_2atmpS2390;
      moonbit_string_t _M0L6_2atmpS2389;
      moonbit_string_t _M0L6_2atmpS4609;
      moonbit_string_t _M0L6_2atmpS2387;
      moonbit_string_t _M0L6_2atmpS4608;
      if (_M0L3msgS515) {
        moonbit_decref(_M0L3msgS515);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2393
      = _M0FPB13debug__stringGORPC16string10StringViewE(_M0L1aS512);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2392
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2393);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4611
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS2392);
      moonbit_decref(_M0L6_2atmpS2392);
      _M0L6_2atmpS2391 = _M0L6_2atmpS4611;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4610
      = moonbit_add_string(_M0L6_2atmpS2391, (moonbit_string_t)moonbit_string_literal_100.data);
      moonbit_decref(_M0L6_2atmpS2391);
      _M0L6_2atmpS2388 = _M0L6_2atmpS4610;
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2390
      = _M0FPB13debug__stringGORPC16string10StringViewE(_M0L1bS513);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2389
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2390);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4609
      = moonbit_add_string(_M0L6_2atmpS2388, _M0L6_2atmpS2389);
      moonbit_decref(_M0L6_2atmpS2388);
      moonbit_decref(_M0L6_2atmpS2389);
      _M0L6_2atmpS2387 = _M0L6_2atmpS4609;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4608
      = moonbit_add_string(_M0L6_2atmpS2387, (moonbit_string_t)moonbit_string_literal_99.data);
      moonbit_decref(_M0L6_2atmpS2387);
      _M0L9fail__msgS514 = _M0L6_2atmpS4608;
    } else {
      moonbit_string_t _M0L7_2aSomeS516;
      moonbit_decref(_M0L1bS513);
      moonbit_decref(_M0L1aS512);
      _M0L7_2aSomeS516 = _M0L3msgS515;
      _M0L9fail__msgS514 = _M0L7_2aSomeS516;
    }
    #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS514, _M0L3locS517);
  } else {
    int32_t _M0L6_2atmpS2394;
    struct moonbit_result_0 _result_5122;
    moonbit_decref(_M0L3locS517);
    if (_M0L3msgS515) {
      moonbit_decref(_M0L3msgS515);
    }
    moonbit_decref(_M0L1bS513);
    moonbit_decref(_M0L1aS512);
    _M0L6_2atmpS2394 = 0;
    _result_5122.tag = 1;
    _result_5122.data.ok = _M0L6_2atmpS2394;
    return _result_5122;
  }
}

struct moonbit_result_0 _M0FPB10assert__eqGOiE(
  int64_t _M0L1aS518,
  int64_t _M0L1bS519,
  moonbit_string_t _M0L3msgS521,
  moonbit_string_t _M0L3locS523
) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (_M0IP016_24default__implPB2Eq10not__equalGOiE(_M0L1aS518, _M0L1bS519)) {
    moonbit_string_t _M0L9fail__msgS520;
    if (_M0L3msgS521 == 0) {
      moonbit_string_t _M0L6_2atmpS2401;
      moonbit_string_t _M0L6_2atmpS2400;
      moonbit_string_t _M0L6_2atmpS4615;
      moonbit_string_t _M0L6_2atmpS2399;
      moonbit_string_t _M0L6_2atmpS4614;
      moonbit_string_t _M0L6_2atmpS2396;
      moonbit_string_t _M0L6_2atmpS2398;
      moonbit_string_t _M0L6_2atmpS2397;
      moonbit_string_t _M0L6_2atmpS4613;
      moonbit_string_t _M0L6_2atmpS2395;
      moonbit_string_t _M0L6_2atmpS4612;
      if (_M0L3msgS521) {
        moonbit_decref(_M0L3msgS521);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2401 = _M0FPB13debug__stringGOiE(_M0L1aS518);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2400
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2401);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4615
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS2400);
      moonbit_decref(_M0L6_2atmpS2400);
      _M0L6_2atmpS2399 = _M0L6_2atmpS4615;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4614
      = moonbit_add_string(_M0L6_2atmpS2399, (moonbit_string_t)moonbit_string_literal_100.data);
      moonbit_decref(_M0L6_2atmpS2399);
      _M0L6_2atmpS2396 = _M0L6_2atmpS4614;
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2398 = _M0FPB13debug__stringGOiE(_M0L1bS519);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2397
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2398);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4613
      = moonbit_add_string(_M0L6_2atmpS2396, _M0L6_2atmpS2397);
      moonbit_decref(_M0L6_2atmpS2396);
      moonbit_decref(_M0L6_2atmpS2397);
      _M0L6_2atmpS2395 = _M0L6_2atmpS4613;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4612
      = moonbit_add_string(_M0L6_2atmpS2395, (moonbit_string_t)moonbit_string_literal_99.data);
      moonbit_decref(_M0L6_2atmpS2395);
      _M0L9fail__msgS520 = _M0L6_2atmpS4612;
    } else {
      moonbit_string_t _M0L7_2aSomeS522 = _M0L3msgS521;
      _M0L9fail__msgS520 = _M0L7_2aSomeS522;
    }
    #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS520, _M0L3locS523);
  } else {
    int32_t _M0L6_2atmpS2402;
    struct moonbit_result_0 _result_5123;
    moonbit_decref(_M0L3locS523);
    if (_M0L3msgS521) {
      moonbit_decref(_M0L3msgS521);
    }
    _M0L6_2atmpS2402 = 0;
    _result_5123.tag = 1;
    _result_5123.data.ok = _M0L6_2atmpS2402;
    return _result_5123;
  }
}

struct moonbit_result_0 _M0FPB10assert__eqGRPB5ArrayGRPC16string10StringViewEE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L1aS524,
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L1bS525,
  moonbit_string_t _M0L3msgS527,
  moonbit_string_t _M0L3locS529
) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  moonbit_incref(_M0L1bS525);
  moonbit_incref(_M0L1aS524);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGRPB5ArrayGRPC16string10StringViewEE(_M0L1aS524, _M0L1bS525)
  ) {
    moonbit_string_t _M0L9fail__msgS526;
    if (_M0L3msgS527 == 0) {
      moonbit_string_t _M0L6_2atmpS2409;
      moonbit_string_t _M0L6_2atmpS2408;
      moonbit_string_t _M0L6_2atmpS4619;
      moonbit_string_t _M0L6_2atmpS2407;
      moonbit_string_t _M0L6_2atmpS4618;
      moonbit_string_t _M0L6_2atmpS2404;
      moonbit_string_t _M0L6_2atmpS2406;
      moonbit_string_t _M0L6_2atmpS2405;
      moonbit_string_t _M0L6_2atmpS4617;
      moonbit_string_t _M0L6_2atmpS2403;
      moonbit_string_t _M0L6_2atmpS4616;
      if (_M0L3msgS527) {
        moonbit_decref(_M0L3msgS527);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2409
      = _M0FPB13debug__stringGRPB5ArrayGRPC16string10StringViewEE(_M0L1aS524);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2408
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2409);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4619
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS2408);
      moonbit_decref(_M0L6_2atmpS2408);
      _M0L6_2atmpS2407 = _M0L6_2atmpS4619;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4618
      = moonbit_add_string(_M0L6_2atmpS2407, (moonbit_string_t)moonbit_string_literal_100.data);
      moonbit_decref(_M0L6_2atmpS2407);
      _M0L6_2atmpS2404 = _M0L6_2atmpS4618;
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2406
      = _M0FPB13debug__stringGRPB5ArrayGRPC16string10StringViewEE(_M0L1bS525);
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2405
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2406);
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4617
      = moonbit_add_string(_M0L6_2atmpS2404, _M0L6_2atmpS2405);
      moonbit_decref(_M0L6_2atmpS2404);
      moonbit_decref(_M0L6_2atmpS2405);
      _M0L6_2atmpS2403 = _M0L6_2atmpS4617;
      #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4616
      = moonbit_add_string(_M0L6_2atmpS2403, (moonbit_string_t)moonbit_string_literal_99.data);
      moonbit_decref(_M0L6_2atmpS2403);
      _M0L9fail__msgS526 = _M0L6_2atmpS4616;
    } else {
      moonbit_string_t _M0L7_2aSomeS528;
      moonbit_decref(_M0L1bS525);
      moonbit_decref(_M0L1aS524);
      _M0L7_2aSomeS528 = _M0L3msgS527;
      _M0L9fail__msgS526 = _M0L7_2aSomeS528;
    }
    #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS526, _M0L3locS529);
  } else {
    int32_t _M0L6_2atmpS2410;
    struct moonbit_result_0 _result_5124;
    moonbit_decref(_M0L3locS529);
    if (_M0L3msgS527) {
      moonbit_decref(_M0L3msgS527);
    }
    moonbit_decref(_M0L1bS525);
    moonbit_decref(_M0L1aS524);
    _M0L6_2atmpS2410 = 0;
    _result_5124.tag = 1;
    _result_5124.data.ok = _M0L6_2atmpS2410;
    return _result_5124;
  }
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS505,
  moonbit_string_t _M0L3locS504
) {
  moonbit_string_t _M0L6_2atmpS2378;
  moonbit_string_t _M0L6_2atmpS4621;
  moonbit_string_t _M0L6_2atmpS2376;
  moonbit_string_t _M0L6_2atmpS2377;
  moonbit_string_t _M0L6_2atmpS4620;
  moonbit_string_t _M0L6_2atmpS2375;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2374;
  struct moonbit_result_0 _result_5125;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS2378
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS504);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS4621
  = moonbit_add_string(_M0L6_2atmpS2378, (moonbit_string_t)moonbit_string_literal_101.data);
  moonbit_decref(_M0L6_2atmpS2378);
  _M0L6_2atmpS2376 = _M0L6_2atmpS4621;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS2377 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS505);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS4620 = moonbit_add_string(_M0L6_2atmpS2376, _M0L6_2atmpS2377);
  moonbit_decref(_M0L6_2atmpS2376);
  moonbit_decref(_M0L6_2atmpS2377);
  _M0L6_2atmpS2375 = _M0L6_2atmpS4620;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2374
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2374)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2374)->$0
  = _M0L6_2atmpS2375;
  _result_5125.tag = 0;
  _result_5125.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS2374;
  return _result_5125;
}

moonbit_string_t _M0FPB13debug__stringGRPC16string10StringViewE(
  struct _M0TPC16string10StringView _M0L1tS497
) {
  struct _M0TPB13StringBuilder* _M0L3bufS496;
  struct _M0TPB6Logger _M0L6_2atmpS2370;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0L3bufS496 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS496);
  _M0L6_2atmpS2370
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS496
  };
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L1tS497, _M0L6_2atmpS2370);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS496);
}

moonbit_string_t _M0FPB13debug__stringGORPC16string10StringViewE(
  void* _M0L1tS499
) {
  struct _M0TPB13StringBuilder* _M0L3bufS498;
  struct _M0TPB6Logger _M0L6_2atmpS2371;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0L3bufS498 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS498);
  _M0L6_2atmpS2371
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS498
  };
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0IPC16option6OptionPB4Show6outputGRPC16string10StringViewE(_M0L1tS499, _M0L6_2atmpS2371);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS498);
}

moonbit_string_t _M0FPB13debug__stringGOiE(int64_t _M0L1tS501) {
  struct _M0TPB13StringBuilder* _M0L3bufS500;
  struct _M0TPB6Logger _M0L6_2atmpS2372;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0L3bufS500 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS500);
  _M0L6_2atmpS2372
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS500
  };
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0IPC16option6OptionPB4Show6outputGiE(_M0L1tS501, _M0L6_2atmpS2372);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS500);
}

moonbit_string_t _M0FPB13debug__stringGRPB5ArrayGRPC16string10StringViewEE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L1tS503
) {
  struct _M0TPB13StringBuilder* _M0L3bufS502;
  struct _M0TPB6Logger _M0L6_2atmpS2373;
  #line 16 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0L3bufS502 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS502);
  _M0L6_2atmpS2373
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS502
  };
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  _M0IPC15array5ArrayPB4Show6outputGRPC16string10StringViewE(_M0L1tS503, _M0L6_2atmpS2373);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS502);
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS488,
  int32_t _M0L5radixS487
) {
  int32_t _if__result_5126;
  uint16_t* _M0L6bufferS489;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS487 < 2) {
    _if__result_5126 = 1;
  } else {
    _if__result_5126 = _M0L5radixS487 > 36;
  }
  if (_if__result_5126) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_102.data, (moonbit_string_t)moonbit_string_literal_103.data);
  }
  if (_M0L4selfS488 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_77.data;
  }
  switch (_M0L5radixS487) {
    case 10: {
      int32_t _M0L3lenS490;
      uint16_t* _M0L6bufferS491;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS490 = _M0FPB12dec__count64(_M0L4selfS488);
      _M0L6bufferS491 = (uint16_t*)moonbit_make_string(_M0L3lenS490, 0);
      moonbit_incref(_M0L6bufferS491);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS491, _M0L4selfS488, 0, _M0L3lenS490);
      _M0L6bufferS489 = _M0L6bufferS491;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS492;
      uint16_t* _M0L6bufferS493;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS492 = _M0FPB12hex__count64(_M0L4selfS488);
      _M0L6bufferS493 = (uint16_t*)moonbit_make_string(_M0L3lenS492, 0);
      moonbit_incref(_M0L6bufferS493);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS493, _M0L4selfS488, 0, _M0L3lenS492);
      _M0L6bufferS489 = _M0L6bufferS493;
      break;
    }
    default: {
      int32_t _M0L3lenS494;
      uint16_t* _M0L6bufferS495;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS494 = _M0FPB14radix__count64(_M0L4selfS488, _M0L5radixS487);
      _M0L6bufferS495 = (uint16_t*)moonbit_make_string(_M0L3lenS494, 0);
      moonbit_incref(_M0L6bufferS495);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS495, _M0L4selfS488, 0, _M0L3lenS494, _M0L5radixS487);
      _M0L6bufferS489 = _M0L6bufferS495;
      break;
    }
  }
  return _M0L6bufferS489;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS477,
  uint64_t _M0L3numS465,
  int32_t _M0L12digit__startS468,
  int32_t _M0L10total__lenS467
) {
  uint64_t _M0Lm3numS464;
  int32_t _M0Lm6offsetS466;
  uint64_t _M0L6_2atmpS2369;
  int32_t _M0Lm9remainingS479;
  int32_t _M0L6_2atmpS2350;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS464 = _M0L3numS465;
  _M0Lm6offsetS466 = _M0L10total__lenS467 - _M0L12digit__startS468;
  while (1) {
    uint64_t _M0L6_2atmpS2313 = _M0Lm3numS464;
    if (_M0L6_2atmpS2313 >= 10000ull) {
      uint64_t _M0L6_2atmpS2336 = _M0Lm3numS464;
      uint64_t _M0L1tS469 = _M0L6_2atmpS2336 / 10000ull;
      uint64_t _M0L6_2atmpS2335 = _M0Lm3numS464;
      uint64_t _M0L6_2atmpS2334 = _M0L6_2atmpS2335 % 10000ull;
      int32_t _M0L1rS470 = (int32_t)_M0L6_2atmpS2334;
      int32_t _M0L2d1S471;
      int32_t _M0L2d2S472;
      int32_t _M0L6_2atmpS2314;
      int32_t _M0L6_2atmpS2333;
      int32_t _M0L6_2atmpS2332;
      int32_t _M0L6d1__hiS473;
      int32_t _M0L6_2atmpS2331;
      int32_t _M0L6_2atmpS2330;
      int32_t _M0L6d1__loS474;
      int32_t _M0L6_2atmpS2329;
      int32_t _M0L6_2atmpS2328;
      int32_t _M0L6d2__hiS475;
      int32_t _M0L6_2atmpS2327;
      int32_t _M0L6_2atmpS2326;
      int32_t _M0L6d2__loS476;
      int32_t _M0L6_2atmpS2316;
      int32_t _M0L6_2atmpS2315;
      int32_t _M0L6_2atmpS2319;
      int32_t _M0L6_2atmpS2318;
      int32_t _M0L6_2atmpS2317;
      int32_t _M0L6_2atmpS2322;
      int32_t _M0L6_2atmpS2321;
      int32_t _M0L6_2atmpS2320;
      int32_t _M0L6_2atmpS2325;
      int32_t _M0L6_2atmpS2324;
      int32_t _M0L6_2atmpS2323;
      _M0Lm3numS464 = _M0L1tS469;
      _M0L2d1S471 = _M0L1rS470 / 100;
      _M0L2d2S472 = _M0L1rS470 % 100;
      _M0L6_2atmpS2314 = _M0Lm6offsetS466;
      _M0Lm6offsetS466 = _M0L6_2atmpS2314 - 4;
      _M0L6_2atmpS2333 = _M0L2d1S471 / 10;
      _M0L6_2atmpS2332 = 48 + _M0L6_2atmpS2333;
      _M0L6d1__hiS473 = (uint16_t)_M0L6_2atmpS2332;
      _M0L6_2atmpS2331 = _M0L2d1S471 % 10;
      _M0L6_2atmpS2330 = 48 + _M0L6_2atmpS2331;
      _M0L6d1__loS474 = (uint16_t)_M0L6_2atmpS2330;
      _M0L6_2atmpS2329 = _M0L2d2S472 / 10;
      _M0L6_2atmpS2328 = 48 + _M0L6_2atmpS2329;
      _M0L6d2__hiS475 = (uint16_t)_M0L6_2atmpS2328;
      _M0L6_2atmpS2327 = _M0L2d2S472 % 10;
      _M0L6_2atmpS2326 = 48 + _M0L6_2atmpS2327;
      _M0L6d2__loS476 = (uint16_t)_M0L6_2atmpS2326;
      _M0L6_2atmpS2316 = _M0Lm6offsetS466;
      _M0L6_2atmpS2315 = _M0L12digit__startS468 + _M0L6_2atmpS2316;
      _M0L6bufferS477[_M0L6_2atmpS2315] = _M0L6d1__hiS473;
      _M0L6_2atmpS2319 = _M0Lm6offsetS466;
      _M0L6_2atmpS2318 = _M0L12digit__startS468 + _M0L6_2atmpS2319;
      _M0L6_2atmpS2317 = _M0L6_2atmpS2318 + 1;
      _M0L6bufferS477[_M0L6_2atmpS2317] = _M0L6d1__loS474;
      _M0L6_2atmpS2322 = _M0Lm6offsetS466;
      _M0L6_2atmpS2321 = _M0L12digit__startS468 + _M0L6_2atmpS2322;
      _M0L6_2atmpS2320 = _M0L6_2atmpS2321 + 2;
      _M0L6bufferS477[_M0L6_2atmpS2320] = _M0L6d2__hiS475;
      _M0L6_2atmpS2325 = _M0Lm6offsetS466;
      _M0L6_2atmpS2324 = _M0L12digit__startS468 + _M0L6_2atmpS2325;
      _M0L6_2atmpS2323 = _M0L6_2atmpS2324 + 3;
      _M0L6bufferS477[_M0L6_2atmpS2323] = _M0L6d2__loS476;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2369 = _M0Lm3numS464;
  _M0Lm9remainingS479 = (int32_t)_M0L6_2atmpS2369;
  while (1) {
    int32_t _M0L6_2atmpS2337 = _M0Lm9remainingS479;
    if (_M0L6_2atmpS2337 >= 100) {
      int32_t _M0L6_2atmpS2349 = _M0Lm9remainingS479;
      int32_t _M0L1tS480 = _M0L6_2atmpS2349 / 100;
      int32_t _M0L6_2atmpS2348 = _M0Lm9remainingS479;
      int32_t _M0L1dS481 = _M0L6_2atmpS2348 % 100;
      int32_t _M0L6_2atmpS2338;
      int32_t _M0L6_2atmpS2347;
      int32_t _M0L6_2atmpS2346;
      int32_t _M0L5d__hiS482;
      int32_t _M0L6_2atmpS2345;
      int32_t _M0L6_2atmpS2344;
      int32_t _M0L5d__loS483;
      int32_t _M0L6_2atmpS2340;
      int32_t _M0L6_2atmpS2339;
      int32_t _M0L6_2atmpS2343;
      int32_t _M0L6_2atmpS2342;
      int32_t _M0L6_2atmpS2341;
      _M0Lm9remainingS479 = _M0L1tS480;
      _M0L6_2atmpS2338 = _M0Lm6offsetS466;
      _M0Lm6offsetS466 = _M0L6_2atmpS2338 - 2;
      _M0L6_2atmpS2347 = _M0L1dS481 / 10;
      _M0L6_2atmpS2346 = 48 + _M0L6_2atmpS2347;
      _M0L5d__hiS482 = (uint16_t)_M0L6_2atmpS2346;
      _M0L6_2atmpS2345 = _M0L1dS481 % 10;
      _M0L6_2atmpS2344 = 48 + _M0L6_2atmpS2345;
      _M0L5d__loS483 = (uint16_t)_M0L6_2atmpS2344;
      _M0L6_2atmpS2340 = _M0Lm6offsetS466;
      _M0L6_2atmpS2339 = _M0L12digit__startS468 + _M0L6_2atmpS2340;
      _M0L6bufferS477[_M0L6_2atmpS2339] = _M0L5d__hiS482;
      _M0L6_2atmpS2343 = _M0Lm6offsetS466;
      _M0L6_2atmpS2342 = _M0L12digit__startS468 + _M0L6_2atmpS2343;
      _M0L6_2atmpS2341 = _M0L6_2atmpS2342 + 1;
      _M0L6bufferS477[_M0L6_2atmpS2341] = _M0L5d__loS483;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2350 = _M0Lm9remainingS479;
  if (_M0L6_2atmpS2350 >= 10) {
    int32_t _M0L6_2atmpS2351 = _M0Lm6offsetS466;
    int32_t _M0L6_2atmpS2362;
    int32_t _M0L6_2atmpS2361;
    int32_t _M0L6_2atmpS2360;
    int32_t _M0L5d__hiS485;
    int32_t _M0L6_2atmpS2359;
    int32_t _M0L6_2atmpS2358;
    int32_t _M0L6_2atmpS2357;
    int32_t _M0L5d__loS486;
    int32_t _M0L6_2atmpS2353;
    int32_t _M0L6_2atmpS2352;
    int32_t _M0L6_2atmpS2356;
    int32_t _M0L6_2atmpS2355;
    int32_t _M0L6_2atmpS2354;
    _M0Lm6offsetS466 = _M0L6_2atmpS2351 - 2;
    _M0L6_2atmpS2362 = _M0Lm9remainingS479;
    _M0L6_2atmpS2361 = _M0L6_2atmpS2362 / 10;
    _M0L6_2atmpS2360 = 48 + _M0L6_2atmpS2361;
    _M0L5d__hiS485 = (uint16_t)_M0L6_2atmpS2360;
    _M0L6_2atmpS2359 = _M0Lm9remainingS479;
    _M0L6_2atmpS2358 = _M0L6_2atmpS2359 % 10;
    _M0L6_2atmpS2357 = 48 + _M0L6_2atmpS2358;
    _M0L5d__loS486 = (uint16_t)_M0L6_2atmpS2357;
    _M0L6_2atmpS2353 = _M0Lm6offsetS466;
    _M0L6_2atmpS2352 = _M0L12digit__startS468 + _M0L6_2atmpS2353;
    _M0L6bufferS477[_M0L6_2atmpS2352] = _M0L5d__hiS485;
    _M0L6_2atmpS2356 = _M0Lm6offsetS466;
    _M0L6_2atmpS2355 = _M0L12digit__startS468 + _M0L6_2atmpS2356;
    _M0L6_2atmpS2354 = _M0L6_2atmpS2355 + 1;
    _M0L6bufferS477[_M0L6_2atmpS2354] = _M0L5d__loS486;
    moonbit_decref(_M0L6bufferS477);
  } else {
    int32_t _M0L6_2atmpS2363 = _M0Lm6offsetS466;
    int32_t _M0L6_2atmpS2368;
    int32_t _M0L6_2atmpS2364;
    int32_t _M0L6_2atmpS2367;
    int32_t _M0L6_2atmpS2366;
    int32_t _M0L6_2atmpS2365;
    _M0Lm6offsetS466 = _M0L6_2atmpS2363 - 1;
    _M0L6_2atmpS2368 = _M0Lm6offsetS466;
    _M0L6_2atmpS2364 = _M0L12digit__startS468 + _M0L6_2atmpS2368;
    _M0L6_2atmpS2367 = _M0Lm9remainingS479;
    _M0L6_2atmpS2366 = 48 + _M0L6_2atmpS2367;
    _M0L6_2atmpS2365 = (uint16_t)_M0L6_2atmpS2366;
    _M0L6bufferS477[_M0L6_2atmpS2364] = _M0L6_2atmpS2365;
    moonbit_decref(_M0L6bufferS477);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS459,
  uint64_t _M0L3numS453,
  int32_t _M0L12digit__startS451,
  int32_t _M0L10total__lenS450,
  int32_t _M0L5radixS455
) {
  int32_t _M0Lm6offsetS449;
  uint64_t _M0Lm1nS452;
  uint64_t _M0L4baseS454;
  int32_t _M0L6_2atmpS2295;
  int32_t _M0L6_2atmpS2294;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS449 = _M0L10total__lenS450 - _M0L12digit__startS451;
  _M0Lm1nS452 = _M0L3numS453;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS454 = _M0MPC13int3Int10to__uint64(_M0L5radixS455);
  _M0L6_2atmpS2295 = _M0L5radixS455 - 1;
  _M0L6_2atmpS2294 = _M0L5radixS455 & _M0L6_2atmpS2295;
  if (_M0L6_2atmpS2294 == 0) {
    int32_t _M0L5shiftS456;
    uint64_t _M0L4maskS457;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS456 = moonbit_ctz32(_M0L5radixS455);
    _M0L4maskS457 = _M0L4baseS454 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS2296 = _M0Lm1nS452;
      if (_M0L6_2atmpS2296 > 0ull) {
        int32_t _M0L6_2atmpS2297 = _M0Lm6offsetS449;
        uint64_t _M0L6_2atmpS2303;
        uint64_t _M0L6_2atmpS2302;
        int32_t _M0L5digitS458;
        int32_t _M0L6_2atmpS2300;
        int32_t _M0L6_2atmpS2298;
        int32_t _M0L6_2atmpS2299;
        uint64_t _M0L6_2atmpS2301;
        _M0Lm6offsetS449 = _M0L6_2atmpS2297 - 1;
        _M0L6_2atmpS2303 = _M0Lm1nS452;
        _M0L6_2atmpS2302 = _M0L6_2atmpS2303 & _M0L4maskS457;
        _M0L5digitS458 = (int32_t)_M0L6_2atmpS2302;
        _M0L6_2atmpS2300 = _M0Lm6offsetS449;
        _M0L6_2atmpS2298 = _M0L12digit__startS451 + _M0L6_2atmpS2300;
        _M0L6_2atmpS2299
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS458
        ];
        _M0L6bufferS459[_M0L6_2atmpS2298] = _M0L6_2atmpS2299;
        _M0L6_2atmpS2301 = _M0Lm1nS452;
        _M0Lm1nS452 = _M0L6_2atmpS2301 >> (_M0L5shiftS456 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS459);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS2304 = _M0Lm1nS452;
      if (_M0L6_2atmpS2304 > 0ull) {
        int32_t _M0L6_2atmpS2305 = _M0Lm6offsetS449;
        uint64_t _M0L6_2atmpS2312;
        uint64_t _M0L1qS461;
        uint64_t _M0L6_2atmpS2310;
        uint64_t _M0L6_2atmpS2311;
        uint64_t _M0L6_2atmpS2309;
        int32_t _M0L5digitS462;
        int32_t _M0L6_2atmpS2308;
        int32_t _M0L6_2atmpS2306;
        int32_t _M0L6_2atmpS2307;
        _M0Lm6offsetS449 = _M0L6_2atmpS2305 - 1;
        _M0L6_2atmpS2312 = _M0Lm1nS452;
        _M0L1qS461 = _M0L6_2atmpS2312 / _M0L4baseS454;
        _M0L6_2atmpS2310 = _M0Lm1nS452;
        _M0L6_2atmpS2311 = _M0L1qS461 * _M0L4baseS454;
        _M0L6_2atmpS2309 = _M0L6_2atmpS2310 - _M0L6_2atmpS2311;
        _M0L5digitS462 = (int32_t)_M0L6_2atmpS2309;
        _M0L6_2atmpS2308 = _M0Lm6offsetS449;
        _M0L6_2atmpS2306 = _M0L12digit__startS451 + _M0L6_2atmpS2308;
        _M0L6_2atmpS2307
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS462
        ];
        _M0L6bufferS459[_M0L6_2atmpS2306] = _M0L6_2atmpS2307;
        _M0Lm1nS452 = _M0L1qS461;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS459);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS446,
  uint64_t _M0L3numS442,
  int32_t _M0L12digit__startS440,
  int32_t _M0L10total__lenS439
) {
  int32_t _M0Lm6offsetS438;
  uint64_t _M0Lm1nS441;
  int32_t _M0L6_2atmpS2290;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS438 = _M0L10total__lenS439 - _M0L12digit__startS440;
  _M0Lm1nS441 = _M0L3numS442;
  while (1) {
    int32_t _M0L6_2atmpS2278 = _M0Lm6offsetS438;
    if (_M0L6_2atmpS2278 >= 2) {
      int32_t _M0L6_2atmpS2279 = _M0Lm6offsetS438;
      uint64_t _M0L6_2atmpS2289;
      uint64_t _M0L6_2atmpS2288;
      int32_t _M0L9byte__valS443;
      int32_t _M0L2hiS444;
      int32_t _M0L2loS445;
      int32_t _M0L6_2atmpS2282;
      int32_t _M0L6_2atmpS2280;
      int32_t _M0L6_2atmpS2281;
      int32_t _M0L6_2atmpS2286;
      int32_t _M0L6_2atmpS2285;
      int32_t _M0L6_2atmpS2283;
      int32_t _M0L6_2atmpS2284;
      uint64_t _M0L6_2atmpS2287;
      _M0Lm6offsetS438 = _M0L6_2atmpS2279 - 2;
      _M0L6_2atmpS2289 = _M0Lm1nS441;
      _M0L6_2atmpS2288 = _M0L6_2atmpS2289 & 255ull;
      _M0L9byte__valS443 = (int32_t)_M0L6_2atmpS2288;
      _M0L2hiS444 = _M0L9byte__valS443 / 16;
      _M0L2loS445 = _M0L9byte__valS443 % 16;
      _M0L6_2atmpS2282 = _M0Lm6offsetS438;
      _M0L6_2atmpS2280 = _M0L12digit__startS440 + _M0L6_2atmpS2282;
      _M0L6_2atmpS2281
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2hiS444
      ];
      _M0L6bufferS446[_M0L6_2atmpS2280] = _M0L6_2atmpS2281;
      _M0L6_2atmpS2286 = _M0Lm6offsetS438;
      _M0L6_2atmpS2285 = _M0L12digit__startS440 + _M0L6_2atmpS2286;
      _M0L6_2atmpS2283 = _M0L6_2atmpS2285 + 1;
      _M0L6_2atmpS2284
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS445
      ];
      _M0L6bufferS446[_M0L6_2atmpS2283] = _M0L6_2atmpS2284;
      _M0L6_2atmpS2287 = _M0Lm1nS441;
      _M0Lm1nS441 = _M0L6_2atmpS2287 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2290 = _M0Lm6offsetS438;
  if (_M0L6_2atmpS2290 == 1) {
    uint64_t _M0L6_2atmpS2293 = _M0Lm1nS441;
    uint64_t _M0L6_2atmpS2292 = _M0L6_2atmpS2293 & 15ull;
    int32_t _M0L6nibbleS448 = (int32_t)_M0L6_2atmpS2292;
    int32_t _M0L6_2atmpS2291 =
      ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS448];
    _M0L6bufferS446[_M0L12digit__startS440] = _M0L6_2atmpS2291;
    moonbit_decref(_M0L6bufferS446);
  } else {
    moonbit_decref(_M0L6bufferS446);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS432,
  int32_t _M0L5radixS435
) {
  uint64_t _M0Lm3numS433;
  uint64_t _M0L4baseS434;
  int32_t _M0Lm5countS436;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS432 == 0ull) {
    return 1;
  }
  _M0Lm3numS433 = _M0L5valueS432;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS434 = _M0MPC13int3Int10to__uint64(_M0L5radixS435);
  _M0Lm5countS436 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS2275 = _M0Lm3numS433;
    if (_M0L6_2atmpS2275 > 0ull) {
      int32_t _M0L6_2atmpS2276 = _M0Lm5countS436;
      uint64_t _M0L6_2atmpS2277;
      _M0Lm5countS436 = _M0L6_2atmpS2276 + 1;
      _M0L6_2atmpS2277 = _M0Lm3numS433;
      _M0Lm3numS433 = _M0L6_2atmpS2277 / _M0L4baseS434;
      continue;
    }
    break;
  }
  return _M0Lm5countS436;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS430) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS430 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS431;
    int32_t _M0L6_2atmpS2274;
    int32_t _M0L6_2atmpS2273;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS431 = moonbit_clz64(_M0L5valueS430);
    _M0L6_2atmpS2274 = 63 - _M0L14leading__zerosS431;
    _M0L6_2atmpS2273 = _M0L6_2atmpS2274 / 4;
    return _M0L6_2atmpS2273 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS429) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS429 >= 10000000000ull) {
    if (_M0L5valueS429 >= 100000000000000ull) {
      if (_M0L5valueS429 >= 10000000000000000ull) {
        if (_M0L5valueS429 >= 1000000000000000000ull) {
          if (_M0L5valueS429 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS429 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS429 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS429 >= 1000000000000ull) {
      if (_M0L5valueS429 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS429 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS429 >= 100000ull) {
    if (_M0L5valueS429 >= 10000000ull) {
      if (_M0L5valueS429 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS429 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS429 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS429 >= 1000ull) {
    if (_M0L5valueS429 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS429 >= 100ull) {
    return 3;
  } else if (_M0L5valueS429 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS413,
  int32_t _M0L5radixS412
) {
  int32_t _if__result_5133;
  int32_t _M0L12is__negativeS414;
  uint32_t _M0L3numS415;
  uint16_t* _M0L6bufferS416;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS412 < 2) {
    _if__result_5133 = 1;
  } else {
    _if__result_5133 = _M0L5radixS412 > 36;
  }
  if (_if__result_5133) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_102.data, (moonbit_string_t)moonbit_string_literal_105.data);
  }
  if (_M0L4selfS413 == 0) {
    return (moonbit_string_t)moonbit_string_literal_77.data;
  }
  _M0L12is__negativeS414 = _M0L4selfS413 < 0;
  if (_M0L12is__negativeS414) {
    int32_t _M0L6_2atmpS2272 = -_M0L4selfS413;
    _M0L3numS415 = *(uint32_t*)&_M0L6_2atmpS2272;
  } else {
    _M0L3numS415 = *(uint32_t*)&_M0L4selfS413;
  }
  switch (_M0L5radixS412) {
    case 10: {
      int32_t _M0L10digit__lenS417;
      int32_t _M0L6_2atmpS2269;
      int32_t _M0L10total__lenS418;
      uint16_t* _M0L6bufferS419;
      int32_t _M0L12digit__startS420;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS417 = _M0FPB12dec__count32(_M0L3numS415);
      if (_M0L12is__negativeS414) {
        _M0L6_2atmpS2269 = 1;
      } else {
        _M0L6_2atmpS2269 = 0;
      }
      _M0L10total__lenS418 = _M0L10digit__lenS417 + _M0L6_2atmpS2269;
      _M0L6bufferS419
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS418, 0);
      if (_M0L12is__negativeS414) {
        _M0L12digit__startS420 = 1;
      } else {
        _M0L12digit__startS420 = 0;
      }
      moonbit_incref(_M0L6bufferS419);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS419, _M0L3numS415, _M0L12digit__startS420, _M0L10total__lenS418);
      _M0L6bufferS416 = _M0L6bufferS419;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS421;
      int32_t _M0L6_2atmpS2270;
      int32_t _M0L10total__lenS422;
      uint16_t* _M0L6bufferS423;
      int32_t _M0L12digit__startS424;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS421 = _M0FPB12hex__count32(_M0L3numS415);
      if (_M0L12is__negativeS414) {
        _M0L6_2atmpS2270 = 1;
      } else {
        _M0L6_2atmpS2270 = 0;
      }
      _M0L10total__lenS422 = _M0L10digit__lenS421 + _M0L6_2atmpS2270;
      _M0L6bufferS423
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS422, 0);
      if (_M0L12is__negativeS414) {
        _M0L12digit__startS424 = 1;
      } else {
        _M0L12digit__startS424 = 0;
      }
      moonbit_incref(_M0L6bufferS423);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS423, _M0L3numS415, _M0L12digit__startS424, _M0L10total__lenS422);
      _M0L6bufferS416 = _M0L6bufferS423;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS425;
      int32_t _M0L6_2atmpS2271;
      int32_t _M0L10total__lenS426;
      uint16_t* _M0L6bufferS427;
      int32_t _M0L12digit__startS428;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS425
      = _M0FPB14radix__count32(_M0L3numS415, _M0L5radixS412);
      if (_M0L12is__negativeS414) {
        _M0L6_2atmpS2271 = 1;
      } else {
        _M0L6_2atmpS2271 = 0;
      }
      _M0L10total__lenS426 = _M0L10digit__lenS425 + _M0L6_2atmpS2271;
      _M0L6bufferS427
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS426, 0);
      if (_M0L12is__negativeS414) {
        _M0L12digit__startS428 = 1;
      } else {
        _M0L12digit__startS428 = 0;
      }
      moonbit_incref(_M0L6bufferS427);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS427, _M0L3numS415, _M0L12digit__startS428, _M0L10total__lenS426, _M0L5radixS412);
      _M0L6bufferS416 = _M0L6bufferS427;
      break;
    }
  }
  if (_M0L12is__negativeS414) {
    _M0L6bufferS416[0] = 45;
  }
  return _M0L6bufferS416;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS406,
  int32_t _M0L5radixS409
) {
  uint32_t _M0Lm3numS407;
  uint32_t _M0L4baseS408;
  int32_t _M0Lm5countS410;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS406 == 0u) {
    return 1;
  }
  _M0Lm3numS407 = _M0L5valueS406;
  _M0L4baseS408 = *(uint32_t*)&_M0L5radixS409;
  _M0Lm5countS410 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS2266 = _M0Lm3numS407;
    if (_M0L6_2atmpS2266 > 0u) {
      int32_t _M0L6_2atmpS2267 = _M0Lm5countS410;
      uint32_t _M0L6_2atmpS2268;
      _M0Lm5countS410 = _M0L6_2atmpS2267 + 1;
      _M0L6_2atmpS2268 = _M0Lm3numS407;
      _M0Lm3numS407 = _M0L6_2atmpS2268 / _M0L4baseS408;
      continue;
    }
    break;
  }
  return _M0Lm5countS410;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS404) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS404 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS405;
    int32_t _M0L6_2atmpS2265;
    int32_t _M0L6_2atmpS2264;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS405 = moonbit_clz32(_M0L5valueS404);
    _M0L6_2atmpS2265 = 31 - _M0L14leading__zerosS405;
    _M0L6_2atmpS2264 = _M0L6_2atmpS2265 / 4;
    return _M0L6_2atmpS2264 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS403) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS403 >= 100000u) {
    if (_M0L5valueS403 >= 10000000u) {
      if (_M0L5valueS403 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS403 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS403 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS403 >= 1000u) {
    if (_M0L5valueS403 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS403 >= 100u) {
    return 3;
  } else if (_M0L5valueS403 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS393,
  uint32_t _M0L3numS381,
  int32_t _M0L12digit__startS384,
  int32_t _M0L10total__lenS383
) {
  uint32_t _M0Lm3numS380;
  int32_t _M0Lm6offsetS382;
  uint32_t _M0L6_2atmpS2263;
  int32_t _M0Lm9remainingS395;
  int32_t _M0L6_2atmpS2244;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS380 = _M0L3numS381;
  _M0Lm6offsetS382 = _M0L10total__lenS383 - _M0L12digit__startS384;
  while (1) {
    uint32_t _M0L6_2atmpS2207 = _M0Lm3numS380;
    if (_M0L6_2atmpS2207 >= 10000u) {
      uint32_t _M0L6_2atmpS2230 = _M0Lm3numS380;
      uint32_t _M0L1tS385 = _M0L6_2atmpS2230 / 10000u;
      uint32_t _M0L6_2atmpS2229 = _M0Lm3numS380;
      uint32_t _M0L6_2atmpS2228 = _M0L6_2atmpS2229 % 10000u;
      int32_t _M0L1rS386 = *(int32_t*)&_M0L6_2atmpS2228;
      int32_t _M0L2d1S387;
      int32_t _M0L2d2S388;
      int32_t _M0L6_2atmpS2208;
      int32_t _M0L6_2atmpS2227;
      int32_t _M0L6_2atmpS2226;
      int32_t _M0L6d1__hiS389;
      int32_t _M0L6_2atmpS2225;
      int32_t _M0L6_2atmpS2224;
      int32_t _M0L6d1__loS390;
      int32_t _M0L6_2atmpS2223;
      int32_t _M0L6_2atmpS2222;
      int32_t _M0L6d2__hiS391;
      int32_t _M0L6_2atmpS2221;
      int32_t _M0L6_2atmpS2220;
      int32_t _M0L6d2__loS392;
      int32_t _M0L6_2atmpS2210;
      int32_t _M0L6_2atmpS2209;
      int32_t _M0L6_2atmpS2213;
      int32_t _M0L6_2atmpS2212;
      int32_t _M0L6_2atmpS2211;
      int32_t _M0L6_2atmpS2216;
      int32_t _M0L6_2atmpS2215;
      int32_t _M0L6_2atmpS2214;
      int32_t _M0L6_2atmpS2219;
      int32_t _M0L6_2atmpS2218;
      int32_t _M0L6_2atmpS2217;
      _M0Lm3numS380 = _M0L1tS385;
      _M0L2d1S387 = _M0L1rS386 / 100;
      _M0L2d2S388 = _M0L1rS386 % 100;
      _M0L6_2atmpS2208 = _M0Lm6offsetS382;
      _M0Lm6offsetS382 = _M0L6_2atmpS2208 - 4;
      _M0L6_2atmpS2227 = _M0L2d1S387 / 10;
      _M0L6_2atmpS2226 = 48 + _M0L6_2atmpS2227;
      _M0L6d1__hiS389 = (uint16_t)_M0L6_2atmpS2226;
      _M0L6_2atmpS2225 = _M0L2d1S387 % 10;
      _M0L6_2atmpS2224 = 48 + _M0L6_2atmpS2225;
      _M0L6d1__loS390 = (uint16_t)_M0L6_2atmpS2224;
      _M0L6_2atmpS2223 = _M0L2d2S388 / 10;
      _M0L6_2atmpS2222 = 48 + _M0L6_2atmpS2223;
      _M0L6d2__hiS391 = (uint16_t)_M0L6_2atmpS2222;
      _M0L6_2atmpS2221 = _M0L2d2S388 % 10;
      _M0L6_2atmpS2220 = 48 + _M0L6_2atmpS2221;
      _M0L6d2__loS392 = (uint16_t)_M0L6_2atmpS2220;
      _M0L6_2atmpS2210 = _M0Lm6offsetS382;
      _M0L6_2atmpS2209 = _M0L12digit__startS384 + _M0L6_2atmpS2210;
      _M0L6bufferS393[_M0L6_2atmpS2209] = _M0L6d1__hiS389;
      _M0L6_2atmpS2213 = _M0Lm6offsetS382;
      _M0L6_2atmpS2212 = _M0L12digit__startS384 + _M0L6_2atmpS2213;
      _M0L6_2atmpS2211 = _M0L6_2atmpS2212 + 1;
      _M0L6bufferS393[_M0L6_2atmpS2211] = _M0L6d1__loS390;
      _M0L6_2atmpS2216 = _M0Lm6offsetS382;
      _M0L6_2atmpS2215 = _M0L12digit__startS384 + _M0L6_2atmpS2216;
      _M0L6_2atmpS2214 = _M0L6_2atmpS2215 + 2;
      _M0L6bufferS393[_M0L6_2atmpS2214] = _M0L6d2__hiS391;
      _M0L6_2atmpS2219 = _M0Lm6offsetS382;
      _M0L6_2atmpS2218 = _M0L12digit__startS384 + _M0L6_2atmpS2219;
      _M0L6_2atmpS2217 = _M0L6_2atmpS2218 + 3;
      _M0L6bufferS393[_M0L6_2atmpS2217] = _M0L6d2__loS392;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2263 = _M0Lm3numS380;
  _M0Lm9remainingS395 = *(int32_t*)&_M0L6_2atmpS2263;
  while (1) {
    int32_t _M0L6_2atmpS2231 = _M0Lm9remainingS395;
    if (_M0L6_2atmpS2231 >= 100) {
      int32_t _M0L6_2atmpS2243 = _M0Lm9remainingS395;
      int32_t _M0L1tS396 = _M0L6_2atmpS2243 / 100;
      int32_t _M0L6_2atmpS2242 = _M0Lm9remainingS395;
      int32_t _M0L1dS397 = _M0L6_2atmpS2242 % 100;
      int32_t _M0L6_2atmpS2232;
      int32_t _M0L6_2atmpS2241;
      int32_t _M0L6_2atmpS2240;
      int32_t _M0L5d__hiS398;
      int32_t _M0L6_2atmpS2239;
      int32_t _M0L6_2atmpS2238;
      int32_t _M0L5d__loS399;
      int32_t _M0L6_2atmpS2234;
      int32_t _M0L6_2atmpS2233;
      int32_t _M0L6_2atmpS2237;
      int32_t _M0L6_2atmpS2236;
      int32_t _M0L6_2atmpS2235;
      _M0Lm9remainingS395 = _M0L1tS396;
      _M0L6_2atmpS2232 = _M0Lm6offsetS382;
      _M0Lm6offsetS382 = _M0L6_2atmpS2232 - 2;
      _M0L6_2atmpS2241 = _M0L1dS397 / 10;
      _M0L6_2atmpS2240 = 48 + _M0L6_2atmpS2241;
      _M0L5d__hiS398 = (uint16_t)_M0L6_2atmpS2240;
      _M0L6_2atmpS2239 = _M0L1dS397 % 10;
      _M0L6_2atmpS2238 = 48 + _M0L6_2atmpS2239;
      _M0L5d__loS399 = (uint16_t)_M0L6_2atmpS2238;
      _M0L6_2atmpS2234 = _M0Lm6offsetS382;
      _M0L6_2atmpS2233 = _M0L12digit__startS384 + _M0L6_2atmpS2234;
      _M0L6bufferS393[_M0L6_2atmpS2233] = _M0L5d__hiS398;
      _M0L6_2atmpS2237 = _M0Lm6offsetS382;
      _M0L6_2atmpS2236 = _M0L12digit__startS384 + _M0L6_2atmpS2237;
      _M0L6_2atmpS2235 = _M0L6_2atmpS2236 + 1;
      _M0L6bufferS393[_M0L6_2atmpS2235] = _M0L5d__loS399;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2244 = _M0Lm9remainingS395;
  if (_M0L6_2atmpS2244 >= 10) {
    int32_t _M0L6_2atmpS2245 = _M0Lm6offsetS382;
    int32_t _M0L6_2atmpS2256;
    int32_t _M0L6_2atmpS2255;
    int32_t _M0L6_2atmpS2254;
    int32_t _M0L5d__hiS401;
    int32_t _M0L6_2atmpS2253;
    int32_t _M0L6_2atmpS2252;
    int32_t _M0L6_2atmpS2251;
    int32_t _M0L5d__loS402;
    int32_t _M0L6_2atmpS2247;
    int32_t _M0L6_2atmpS2246;
    int32_t _M0L6_2atmpS2250;
    int32_t _M0L6_2atmpS2249;
    int32_t _M0L6_2atmpS2248;
    _M0Lm6offsetS382 = _M0L6_2atmpS2245 - 2;
    _M0L6_2atmpS2256 = _M0Lm9remainingS395;
    _M0L6_2atmpS2255 = _M0L6_2atmpS2256 / 10;
    _M0L6_2atmpS2254 = 48 + _M0L6_2atmpS2255;
    _M0L5d__hiS401 = (uint16_t)_M0L6_2atmpS2254;
    _M0L6_2atmpS2253 = _M0Lm9remainingS395;
    _M0L6_2atmpS2252 = _M0L6_2atmpS2253 % 10;
    _M0L6_2atmpS2251 = 48 + _M0L6_2atmpS2252;
    _M0L5d__loS402 = (uint16_t)_M0L6_2atmpS2251;
    _M0L6_2atmpS2247 = _M0Lm6offsetS382;
    _M0L6_2atmpS2246 = _M0L12digit__startS384 + _M0L6_2atmpS2247;
    _M0L6bufferS393[_M0L6_2atmpS2246] = _M0L5d__hiS401;
    _M0L6_2atmpS2250 = _M0Lm6offsetS382;
    _M0L6_2atmpS2249 = _M0L12digit__startS384 + _M0L6_2atmpS2250;
    _M0L6_2atmpS2248 = _M0L6_2atmpS2249 + 1;
    _M0L6bufferS393[_M0L6_2atmpS2248] = _M0L5d__loS402;
    moonbit_decref(_M0L6bufferS393);
  } else {
    int32_t _M0L6_2atmpS2257 = _M0Lm6offsetS382;
    int32_t _M0L6_2atmpS2262;
    int32_t _M0L6_2atmpS2258;
    int32_t _M0L6_2atmpS2261;
    int32_t _M0L6_2atmpS2260;
    int32_t _M0L6_2atmpS2259;
    _M0Lm6offsetS382 = _M0L6_2atmpS2257 - 1;
    _M0L6_2atmpS2262 = _M0Lm6offsetS382;
    _M0L6_2atmpS2258 = _M0L12digit__startS384 + _M0L6_2atmpS2262;
    _M0L6_2atmpS2261 = _M0Lm9remainingS395;
    _M0L6_2atmpS2260 = 48 + _M0L6_2atmpS2261;
    _M0L6_2atmpS2259 = (uint16_t)_M0L6_2atmpS2260;
    _M0L6bufferS393[_M0L6_2atmpS2258] = _M0L6_2atmpS2259;
    moonbit_decref(_M0L6bufferS393);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS375,
  uint32_t _M0L3numS369,
  int32_t _M0L12digit__startS367,
  int32_t _M0L10total__lenS366,
  int32_t _M0L5radixS371
) {
  int32_t _M0Lm6offsetS365;
  uint32_t _M0Lm1nS368;
  uint32_t _M0L4baseS370;
  int32_t _M0L6_2atmpS2189;
  int32_t _M0L6_2atmpS2188;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS365 = _M0L10total__lenS366 - _M0L12digit__startS367;
  _M0Lm1nS368 = _M0L3numS369;
  _M0L4baseS370 = *(uint32_t*)&_M0L5radixS371;
  _M0L6_2atmpS2189 = _M0L5radixS371 - 1;
  _M0L6_2atmpS2188 = _M0L5radixS371 & _M0L6_2atmpS2189;
  if (_M0L6_2atmpS2188 == 0) {
    int32_t _M0L5shiftS372;
    uint32_t _M0L4maskS373;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS372 = moonbit_ctz32(_M0L5radixS371);
    _M0L4maskS373 = _M0L4baseS370 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS2190 = _M0Lm1nS368;
      if (_M0L6_2atmpS2190 > 0u) {
        int32_t _M0L6_2atmpS2191 = _M0Lm6offsetS365;
        uint32_t _M0L6_2atmpS2197;
        uint32_t _M0L6_2atmpS2196;
        int32_t _M0L5digitS374;
        int32_t _M0L6_2atmpS2194;
        int32_t _M0L6_2atmpS2192;
        int32_t _M0L6_2atmpS2193;
        uint32_t _M0L6_2atmpS2195;
        _M0Lm6offsetS365 = _M0L6_2atmpS2191 - 1;
        _M0L6_2atmpS2197 = _M0Lm1nS368;
        _M0L6_2atmpS2196 = _M0L6_2atmpS2197 & _M0L4maskS373;
        _M0L5digitS374 = *(int32_t*)&_M0L6_2atmpS2196;
        _M0L6_2atmpS2194 = _M0Lm6offsetS365;
        _M0L6_2atmpS2192 = _M0L12digit__startS367 + _M0L6_2atmpS2194;
        _M0L6_2atmpS2193
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS374
        ];
        _M0L6bufferS375[_M0L6_2atmpS2192] = _M0L6_2atmpS2193;
        _M0L6_2atmpS2195 = _M0Lm1nS368;
        _M0Lm1nS368 = _M0L6_2atmpS2195 >> (_M0L5shiftS372 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS375);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS2198 = _M0Lm1nS368;
      if (_M0L6_2atmpS2198 > 0u) {
        int32_t _M0L6_2atmpS2199 = _M0Lm6offsetS365;
        uint32_t _M0L6_2atmpS2206;
        uint32_t _M0L1qS377;
        uint32_t _M0L6_2atmpS2204;
        uint32_t _M0L6_2atmpS2205;
        uint32_t _M0L6_2atmpS2203;
        int32_t _M0L5digitS378;
        int32_t _M0L6_2atmpS2202;
        int32_t _M0L6_2atmpS2200;
        int32_t _M0L6_2atmpS2201;
        _M0Lm6offsetS365 = _M0L6_2atmpS2199 - 1;
        _M0L6_2atmpS2206 = _M0Lm1nS368;
        _M0L1qS377 = _M0L6_2atmpS2206 / _M0L4baseS370;
        _M0L6_2atmpS2204 = _M0Lm1nS368;
        _M0L6_2atmpS2205 = _M0L1qS377 * _M0L4baseS370;
        _M0L6_2atmpS2203 = _M0L6_2atmpS2204 - _M0L6_2atmpS2205;
        _M0L5digitS378 = *(int32_t*)&_M0L6_2atmpS2203;
        _M0L6_2atmpS2202 = _M0Lm6offsetS365;
        _M0L6_2atmpS2200 = _M0L12digit__startS367 + _M0L6_2atmpS2202;
        _M0L6_2atmpS2201
        = ((moonbit_string_t)moonbit_string_literal_104.data)[
          _M0L5digitS378
        ];
        _M0L6bufferS375[_M0L6_2atmpS2200] = _M0L6_2atmpS2201;
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

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS362,
  uint32_t _M0L3numS358,
  int32_t _M0L12digit__startS356,
  int32_t _M0L10total__lenS355
) {
  int32_t _M0Lm6offsetS354;
  uint32_t _M0Lm1nS357;
  int32_t _M0L6_2atmpS2184;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS354 = _M0L10total__lenS355 - _M0L12digit__startS356;
  _M0Lm1nS357 = _M0L3numS358;
  while (1) {
    int32_t _M0L6_2atmpS2172 = _M0Lm6offsetS354;
    if (_M0L6_2atmpS2172 >= 2) {
      int32_t _M0L6_2atmpS2173 = _M0Lm6offsetS354;
      uint32_t _M0L6_2atmpS2183;
      uint32_t _M0L6_2atmpS2182;
      int32_t _M0L9byte__valS359;
      int32_t _M0L2hiS360;
      int32_t _M0L2loS361;
      int32_t _M0L6_2atmpS2176;
      int32_t _M0L6_2atmpS2174;
      int32_t _M0L6_2atmpS2175;
      int32_t _M0L6_2atmpS2180;
      int32_t _M0L6_2atmpS2179;
      int32_t _M0L6_2atmpS2177;
      int32_t _M0L6_2atmpS2178;
      uint32_t _M0L6_2atmpS2181;
      _M0Lm6offsetS354 = _M0L6_2atmpS2173 - 2;
      _M0L6_2atmpS2183 = _M0Lm1nS357;
      _M0L6_2atmpS2182 = _M0L6_2atmpS2183 & 255u;
      _M0L9byte__valS359 = *(int32_t*)&_M0L6_2atmpS2182;
      _M0L2hiS360 = _M0L9byte__valS359 / 16;
      _M0L2loS361 = _M0L9byte__valS359 % 16;
      _M0L6_2atmpS2176 = _M0Lm6offsetS354;
      _M0L6_2atmpS2174 = _M0L12digit__startS356 + _M0L6_2atmpS2176;
      _M0L6_2atmpS2175
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2hiS360
      ];
      _M0L6bufferS362[_M0L6_2atmpS2174] = _M0L6_2atmpS2175;
      _M0L6_2atmpS2180 = _M0Lm6offsetS354;
      _M0L6_2atmpS2179 = _M0L12digit__startS356 + _M0L6_2atmpS2180;
      _M0L6_2atmpS2177 = _M0L6_2atmpS2179 + 1;
      _M0L6_2atmpS2178
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS361
      ];
      _M0L6bufferS362[_M0L6_2atmpS2177] = _M0L6_2atmpS2178;
      _M0L6_2atmpS2181 = _M0Lm1nS357;
      _M0Lm1nS357 = _M0L6_2atmpS2181 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2184 = _M0Lm6offsetS354;
  if (_M0L6_2atmpS2184 == 1) {
    uint32_t _M0L6_2atmpS2187 = _M0Lm1nS357;
    uint32_t _M0L6_2atmpS2186 = _M0L6_2atmpS2187 & 15u;
    int32_t _M0L6nibbleS364 = *(int32_t*)&_M0L6_2atmpS2186;
    int32_t _M0L6_2atmpS2185 =
      ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS364];
    _M0L6bufferS362[_M0L12digit__startS356] = _M0L6_2atmpS2185;
    moonbit_decref(_M0L6bufferS362);
  } else {
    moonbit_decref(_M0L6bufferS362);
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGRPC16string10StringViewE(
  struct _M0TPB6Logger _M0L4selfS337,
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4iterS341,
  moonbit_string_t _M0L6prefixS338,
  moonbit_string_t _M0L6suffixS353,
  moonbit_string_t _M0L3sepS344,
  int32_t _M0L8trailingS339
) {
  #line 156 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  if (_M0L4selfS337.$1) {
    moonbit_incref(_M0L4selfS337.$1);
  }
  #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L4selfS337.$0->$method_0(_M0L4selfS337.$1, _M0L6prefixS338);
  if (_M0L8trailingS339) {
    _2afor_345:;
    while (1) {
      void* _M0L7_2abindS340;
      moonbit_incref(_M0L4iterS341);
      #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
      _M0L7_2abindS340
      = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4iterS341);
      switch (Moonbit_object_tag(_M0L7_2abindS340)) {
        case 1: {
          struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS342 =
            (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS340;
          struct _M0TPC16string10StringView _M0L8_2afieldS4622 =
            (struct _M0TPC16string10StringView){_M0L7_2aSomeS342->$0_1,
                                                  _M0L7_2aSomeS342->$0_2,
                                                  _M0L7_2aSomeS342->$0_0};
          int32_t _M0L6_2acntS4834 =
            Moonbit_object_header(_M0L7_2aSomeS342)->rc;
          struct _M0TPC16string10StringView _M0L4_2axS343;
          if (_M0L6_2acntS4834 > 1) {
            int32_t _M0L11_2anew__cntS4835 = _M0L6_2acntS4834 - 1;
            Moonbit_object_header(_M0L7_2aSomeS342)->rc
            = _M0L11_2anew__cntS4835;
            moonbit_incref(_M0L8_2afieldS4622.$0);
          } else if (_M0L6_2acntS4834 == 1) {
            #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
            moonbit_free(_M0L7_2aSomeS342);
          }
          _M0L4_2axS343 = _M0L8_2afieldS4622;
          if (_M0L4selfS337.$1) {
            moonbit_incref(_M0L4selfS337.$1);
          }
          #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0MPB6Logger13write__objectGRPC16string10StringViewE(_M0L4selfS337, _M0L4_2axS343);
          moonbit_incref(_M0L3sepS344);
          if (_M0L4selfS337.$1) {
            moonbit_incref(_M0L4selfS337.$1);
          }
          #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0L4selfS337.$0->$method_0(_M0L4selfS337.$1, _M0L3sepS344);
          goto _2afor_345;
          break;
        }
        default: {
          moonbit_decref(_M0L3sepS344);
          moonbit_decref(_M0L4iterS341);
          moonbit_decref(_M0L7_2abindS340);
          break;
        }
      }
      break;
    }
  } else {
    void* _M0L7_2abindS346;
    moonbit_incref(_M0L4iterS341);
    #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
    _M0L7_2abindS346
    = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4iterS341);
    switch (Moonbit_object_tag(_M0L7_2abindS346)) {
      case 1: {
        struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS347 =
          (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS346;
        struct _M0TPC16string10StringView _M0L8_2afieldS4624 =
          (struct _M0TPC16string10StringView){_M0L7_2aSomeS347->$0_1,
                                                _M0L7_2aSomeS347->$0_2,
                                                _M0L7_2aSomeS347->$0_0};
        int32_t _M0L6_2acntS4836 =
          Moonbit_object_header(_M0L7_2aSomeS347)->rc;
        struct _M0TPC16string10StringView _M0L4_2axS348;
        if (_M0L6_2acntS4836 > 1) {
          int32_t _M0L11_2anew__cntS4837 = _M0L6_2acntS4836 - 1;
          Moonbit_object_header(_M0L7_2aSomeS347)->rc
          = _M0L11_2anew__cntS4837;
          moonbit_incref(_M0L8_2afieldS4624.$0);
        } else if (_M0L6_2acntS4836 == 1) {
          #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          moonbit_free(_M0L7_2aSomeS347);
        }
        _M0L4_2axS348 = _M0L8_2afieldS4624;
        if (_M0L4selfS337.$1) {
          moonbit_incref(_M0L4selfS337.$1);
        }
        #line 171 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0MPB6Logger13write__objectGRPC16string10StringViewE(_M0L4selfS337, _M0L4_2axS348);
        _2afor_352:;
        while (1) {
          void* _M0L7_2abindS349;
          moonbit_incref(_M0L4iterS341);
          #line 172 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0L7_2abindS349
          = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L4iterS341);
          switch (Moonbit_object_tag(_M0L7_2abindS349)) {
            case 1: {
              struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS350 =
                (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS349;
              struct _M0TPC16string10StringView _M0L8_2afieldS4623 =
                (struct _M0TPC16string10StringView){_M0L7_2aSomeS350->$0_1,
                                                      _M0L7_2aSomeS350->$0_2,
                                                      _M0L7_2aSomeS350->$0_0};
              int32_t _M0L6_2acntS4838 =
                Moonbit_object_header(_M0L7_2aSomeS350)->rc;
              struct _M0TPC16string10StringView _M0L4_2axS351;
              if (_M0L6_2acntS4838 > 1) {
                int32_t _M0L11_2anew__cntS4839 = _M0L6_2acntS4838 - 1;
                Moonbit_object_header(_M0L7_2aSomeS350)->rc
                = _M0L11_2anew__cntS4839;
                moonbit_incref(_M0L8_2afieldS4623.$0);
              } else if (_M0L6_2acntS4838 == 1) {
                #line 172 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
                moonbit_free(_M0L7_2aSomeS350);
              }
              _M0L4_2axS351 = _M0L8_2afieldS4623;
              moonbit_incref(_M0L3sepS344);
              if (_M0L4selfS337.$1) {
                moonbit_incref(_M0L4selfS337.$1);
              }
              #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
              _M0L4selfS337.$0->$method_0(_M0L4selfS337.$1, _M0L3sepS344);
              if (_M0L4selfS337.$1) {
                moonbit_incref(_M0L4selfS337.$1);
              }
              #line 174 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
              _M0MPB6Logger13write__objectGRPC16string10StringViewE(_M0L4selfS337, _M0L4_2axS351);
              goto _2afor_352;
              break;
            }
            default: {
              moonbit_decref(_M0L7_2abindS349);
              moonbit_decref(_M0L3sepS344);
              moonbit_decref(_M0L4iterS341);
              break;
            }
          }
          break;
        }
        break;
      }
      default: {
        moonbit_decref(_M0L7_2abindS346);
        moonbit_decref(_M0L3sepS344);
        moonbit_decref(_M0L4iterS341);
        break;
      }
    }
  }
  #line 177 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L4selfS337.$0->$method_0(_M0L4selfS337.$1, _M0L6suffixS353);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS330) {
  struct _M0TWEOs* _M0L7_2afuncS329;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS329 = _M0L4selfS330;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS329->code(_M0L7_2afuncS329);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS332
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS331;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS331 = _M0L4selfS332;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS331->code(_M0L7_2afuncS331);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS334) {
  struct _M0TWEOc* _M0L7_2afuncS333;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS333 = _M0L4selfS334;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS333->code(_M0L7_2afuncS333);
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L4selfS336
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS335;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS335 = _M0L4selfS336;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS335->code(_M0L7_2afuncS335);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS322
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS321;
  struct _M0TPB6Logger _M0L6_2atmpS2168;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS321 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS321);
  _M0L6_2atmpS2168
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS321
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS322, _M0L6_2atmpS2168);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS321);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS324
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS323;
  struct _M0TPB6Logger _M0L6_2atmpS2169;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS323 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS323);
  _M0L6_2atmpS2169
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS323
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS324, _M0L6_2atmpS2169);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS323);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS326
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS325;
  struct _M0TPB6Logger _M0L6_2atmpS2170;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS325 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS325);
  _M0L6_2atmpS2170
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS325
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS326, _M0L6_2atmpS2170);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS325);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS328
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS327;
  struct _M0TPB6Logger _M0L6_2atmpS2171;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS327 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS327);
  _M0L6_2atmpS2171
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS327
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS328, _M0L6_2atmpS2171);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS327);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS320
) {
  int32_t _M0L8_2afieldS4625;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4625 = _M0L4selfS320.$1;
  moonbit_decref(_M0L4selfS320.$0);
  return _M0L8_2afieldS4625;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS319
) {
  int32_t _M0L3endS2166;
  int32_t _M0L8_2afieldS4626;
  int32_t _M0L5startS2167;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS2166 = _M0L4selfS319.$2;
  _M0L8_2afieldS4626 = _M0L4selfS319.$1;
  moonbit_decref(_M0L4selfS319.$0);
  _M0L5startS2167 = _M0L8_2afieldS4626;
  return _M0L3endS2166 - _M0L5startS2167;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS318
) {
  moonbit_string_t _M0L8_2afieldS4627;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4627 = _M0L4selfS318.$0;
  return _M0L8_2afieldS4627;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS314,
  moonbit_string_t _M0L5valueS315,
  int32_t _M0L5startS316,
  int32_t _M0L3lenS317
) {
  int32_t _M0L6_2atmpS2165;
  int64_t _M0L6_2atmpS2164;
  struct _M0TPC16string10StringView _M0L6_2atmpS2163;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2165 = _M0L5startS316 + _M0L3lenS317;
  _M0L6_2atmpS2164 = (int64_t)_M0L6_2atmpS2165;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2163
  = _M0MPC16string6String11sub_2einner(_M0L5valueS315, _M0L5startS316, _M0L6_2atmpS2164);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS314, _M0L6_2atmpS2163);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS307,
  int32_t _M0L5startS313,
  int64_t _M0L3endS309
) {
  int32_t _M0L3lenS306;
  int32_t _M0L3endS308;
  int32_t _M0L5startS312;
  int32_t _if__result_5142;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS306 = Moonbit_array_length(_M0L4selfS307);
  if (_M0L3endS309 == 4294967296ll) {
    _M0L3endS308 = _M0L3lenS306;
  } else {
    int64_t _M0L7_2aSomeS310 = _M0L3endS309;
    int32_t _M0L6_2aendS311 = (int32_t)_M0L7_2aSomeS310;
    if (_M0L6_2aendS311 < 0) {
      _M0L3endS308 = _M0L3lenS306 + _M0L6_2aendS311;
    } else {
      _M0L3endS308 = _M0L6_2aendS311;
    }
  }
  if (_M0L5startS313 < 0) {
    _M0L5startS312 = _M0L3lenS306 + _M0L5startS313;
  } else {
    _M0L5startS312 = _M0L5startS313;
  }
  if (_M0L5startS312 >= 0) {
    if (_M0L5startS312 <= _M0L3endS308) {
      _if__result_5142 = _M0L3endS308 <= _M0L3lenS306;
    } else {
      _if__result_5142 = 0;
    }
  } else {
    _if__result_5142 = 0;
  }
  if (_if__result_5142) {
    if (_M0L5startS312 < _M0L3lenS306) {
      int32_t _M0L6_2atmpS2160 = _M0L4selfS307[_M0L5startS312];
      int32_t _M0L6_2atmpS2159;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2159
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2160);
      if (!_M0L6_2atmpS2159) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS308 < _M0L3lenS306) {
      int32_t _M0L6_2atmpS2162 = _M0L4selfS307[_M0L3endS308];
      int32_t _M0L6_2atmpS2161;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2161
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2162);
      if (!_M0L6_2atmpS2161) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS312,
                                                 _M0L3endS308,
                                                 _M0L4selfS307};
  } else {
    moonbit_decref(_M0L4selfS307);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS303) {
  struct _M0TPB6Hasher* _M0L1hS302;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS302 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS302);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS302, _M0L4selfS303);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS302);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS305
) {
  struct _M0TPB6Hasher* _M0L1hS304;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS304 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS304);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS304, _M0L4selfS305);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS304);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS300) {
  int32_t _M0L4seedS299;
  if (_M0L10seed_2eoptS300 == 4294967296ll) {
    _M0L4seedS299 = 0;
  } else {
    int64_t _M0L7_2aSomeS301 = _M0L10seed_2eoptS300;
    _M0L4seedS299 = (int32_t)_M0L7_2aSomeS301;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS299);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS298) {
  uint32_t _M0L6_2atmpS2158;
  uint32_t _M0L6_2atmpS2157;
  struct _M0TPB6Hasher* _block_5143;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2158 = *(uint32_t*)&_M0L4seedS298;
  _M0L6_2atmpS2157 = _M0L6_2atmpS2158 + 374761393u;
  _block_5143
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_5143)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_5143->$0 = _M0L6_2atmpS2157;
  return _block_5143;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS297) {
  uint32_t _M0L6_2atmpS2156;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2156 = _M0MPB6Hasher9avalanche(_M0L4selfS297);
  return *(int32_t*)&_M0L6_2atmpS2156;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS296) {
  uint32_t _M0L8_2afieldS4628;
  uint32_t _M0Lm3accS295;
  uint32_t _M0L6_2atmpS2145;
  uint32_t _M0L6_2atmpS2147;
  uint32_t _M0L6_2atmpS2146;
  uint32_t _M0L6_2atmpS2148;
  uint32_t _M0L6_2atmpS2149;
  uint32_t _M0L6_2atmpS2151;
  uint32_t _M0L6_2atmpS2150;
  uint32_t _M0L6_2atmpS2152;
  uint32_t _M0L6_2atmpS2153;
  uint32_t _M0L6_2atmpS2155;
  uint32_t _M0L6_2atmpS2154;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4628 = _M0L4selfS296->$0;
  moonbit_decref(_M0L4selfS296);
  _M0Lm3accS295 = _M0L8_2afieldS4628;
  _M0L6_2atmpS2145 = _M0Lm3accS295;
  _M0L6_2atmpS2147 = _M0Lm3accS295;
  _M0L6_2atmpS2146 = _M0L6_2atmpS2147 >> 15;
  _M0Lm3accS295 = _M0L6_2atmpS2145 ^ _M0L6_2atmpS2146;
  _M0L6_2atmpS2148 = _M0Lm3accS295;
  _M0Lm3accS295 = _M0L6_2atmpS2148 * 2246822519u;
  _M0L6_2atmpS2149 = _M0Lm3accS295;
  _M0L6_2atmpS2151 = _M0Lm3accS295;
  _M0L6_2atmpS2150 = _M0L6_2atmpS2151 >> 13;
  _M0Lm3accS295 = _M0L6_2atmpS2149 ^ _M0L6_2atmpS2150;
  _M0L6_2atmpS2152 = _M0Lm3accS295;
  _M0Lm3accS295 = _M0L6_2atmpS2152 * 3266489917u;
  _M0L6_2atmpS2153 = _M0Lm3accS295;
  _M0L6_2atmpS2155 = _M0Lm3accS295;
  _M0L6_2atmpS2154 = _M0L6_2atmpS2155 >> 16;
  _M0Lm3accS295 = _M0L6_2atmpS2153 ^ _M0L6_2atmpS2154;
  return _M0Lm3accS295;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS285,
  moonbit_string_t _M0L1yS286
) {
  int32_t _M0L6_2atmpS4629;
  int32_t _M0L6_2atmpS2140;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4629 = moonbit_val_array_equal(_M0L1xS285, _M0L1yS286);
  moonbit_decref(_M0L1xS285);
  moonbit_decref(_M0L1yS286);
  _M0L6_2atmpS2140 = _M0L6_2atmpS4629;
  return !_M0L6_2atmpS2140;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGRPC16string10StringViewE(
  struct _M0TPC16string10StringView _M0L1xS287,
  struct _M0TPC16string10StringView _M0L1yS288
) {
  int32_t _M0L6_2atmpS2141;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2141
  = _M0IPC16string10StringViewPB2Eq5equal(_M0L1xS287, _M0L1yS288);
  return !_M0L6_2atmpS2141;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGORPC16string10StringViewE(
  void* _M0L1xS289,
  void* _M0L1yS290
) {
  int32_t _M0L6_2atmpS2142;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2142
  = _M0IPC16option6OptionPB2Eq5equalGRPC16string10StringViewE(_M0L1xS289, _M0L1yS290);
  return !_M0L6_2atmpS2142;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGOiE(
  int64_t _M0L1xS291,
  int64_t _M0L1yS292
) {
  int32_t _M0L6_2atmpS2143;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2143
  = _M0IPC16option6OptionPB2Eq5equalGiE(_M0L1xS291, _M0L1yS292);
  return !_M0L6_2atmpS2143;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGRPB5ArrayGRPC16string10StringViewEE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L1xS293,
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L1yS294
) {
  int32_t _M0L6_2atmpS2144;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2144
  = _M0IPC15array5ArrayPB2Eq5equalGRPC16string10StringViewE(_M0L1xS293, _M0L1yS294);
  return !_M0L6_2atmpS2144;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS282,
  int32_t _M0L5valueS281
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS281, _M0L4selfS282);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS284,
  moonbit_string_t _M0L5valueS283
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS283, _M0L4selfS284);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS280) {
  int64_t _M0L6_2atmpS2139;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2139 = (int64_t)_M0L4selfS280;
  return *(uint64_t*)&_M0L6_2atmpS2139;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS278,
  int32_t _M0L5valueS279
) {
  uint32_t _M0L6_2atmpS2138;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2138 = *(uint32_t*)&_M0L5valueS279;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS278, _M0L6_2atmpS2138);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS268,
  moonbit_string_t _M0L7contentS269,
  moonbit_string_t _M0L3locS271,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS273
) {
  moonbit_string_t _M0L6actualS267;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6actualS267 = _M0L3objS268.$0->$method_1(_M0L3objS268.$1);
  moonbit_incref(_M0L7contentS269);
  moonbit_incref(_M0L6actualS267);
  #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS267, _M0L7contentS269)
  ) {
    moonbit_string_t _M0L3locS270;
    moonbit_string_t _M0L9args__locS272;
    moonbit_string_t _M0L15expect__escapedS274;
    moonbit_string_t _M0L15actual__escapedS275;
    moonbit_string_t _M0L6_2atmpS2136;
    moonbit_string_t _M0L6_2atmpS2135;
    moonbit_string_t _M0L6_2atmpS4645;
    moonbit_string_t _M0L6_2atmpS2134;
    moonbit_string_t _M0L6_2atmpS4644;
    moonbit_string_t _M0L14expect__base64S276;
    moonbit_string_t _M0L6_2atmpS2133;
    moonbit_string_t _M0L6_2atmpS2132;
    moonbit_string_t _M0L6_2atmpS4643;
    moonbit_string_t _M0L6_2atmpS2131;
    moonbit_string_t _M0L6_2atmpS4642;
    moonbit_string_t _M0L14actual__base64S277;
    moonbit_string_t _M0L6_2atmpS2130;
    moonbit_string_t _M0L6_2atmpS4641;
    moonbit_string_t _M0L6_2atmpS2129;
    moonbit_string_t _M0L6_2atmpS4640;
    moonbit_string_t _M0L6_2atmpS2127;
    moonbit_string_t _M0L6_2atmpS2128;
    moonbit_string_t _M0L6_2atmpS4639;
    moonbit_string_t _M0L6_2atmpS2126;
    moonbit_string_t _M0L6_2atmpS4638;
    moonbit_string_t _M0L6_2atmpS2124;
    moonbit_string_t _M0L6_2atmpS2125;
    moonbit_string_t _M0L6_2atmpS4637;
    moonbit_string_t _M0L6_2atmpS2123;
    moonbit_string_t _M0L6_2atmpS4636;
    moonbit_string_t _M0L6_2atmpS2121;
    moonbit_string_t _M0L6_2atmpS2122;
    moonbit_string_t _M0L6_2atmpS4635;
    moonbit_string_t _M0L6_2atmpS2120;
    moonbit_string_t _M0L6_2atmpS4634;
    moonbit_string_t _M0L6_2atmpS2118;
    moonbit_string_t _M0L6_2atmpS2119;
    moonbit_string_t _M0L6_2atmpS4633;
    moonbit_string_t _M0L6_2atmpS2117;
    moonbit_string_t _M0L6_2atmpS4632;
    moonbit_string_t _M0L6_2atmpS2115;
    moonbit_string_t _M0L6_2atmpS2116;
    moonbit_string_t _M0L6_2atmpS4631;
    moonbit_string_t _M0L6_2atmpS2114;
    moonbit_string_t _M0L6_2atmpS4630;
    moonbit_string_t _M0L6_2atmpS2113;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2112;
    struct moonbit_result_0 _result_5144;
    #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L3locS270 = _M0MPB9SourceLoc16to__json__string(_M0L3locS271);
    #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L9args__locS272 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS273);
    moonbit_incref(_M0L7contentS269);
    #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15expect__escapedS274
    = _M0MPC16string6String6escape(_M0L7contentS269);
    moonbit_incref(_M0L6actualS267);
    #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15actual__escapedS275 = _M0MPC16string6String6escape(_M0L6actualS267);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2136
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS269);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2135
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2136);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4645
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_106.data, _M0L6_2atmpS2135);
    moonbit_decref(_M0L6_2atmpS2135);
    _M0L6_2atmpS2134 = _M0L6_2atmpS4645;
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4644
    = moonbit_add_string(_M0L6_2atmpS2134, (moonbit_string_t)moonbit_string_literal_106.data);
    moonbit_decref(_M0L6_2atmpS2134);
    _M0L14expect__base64S276 = _M0L6_2atmpS4644;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2133
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS267);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2132
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2133);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4643
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_106.data, _M0L6_2atmpS2132);
    moonbit_decref(_M0L6_2atmpS2132);
    _M0L6_2atmpS2131 = _M0L6_2atmpS4643;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4642
    = moonbit_add_string(_M0L6_2atmpS2131, (moonbit_string_t)moonbit_string_literal_106.data);
    moonbit_decref(_M0L6_2atmpS2131);
    _M0L14actual__base64S277 = _M0L6_2atmpS4642;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2130 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS270);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4641
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_48.data, _M0L6_2atmpS2130);
    moonbit_decref(_M0L6_2atmpS2130);
    _M0L6_2atmpS2129 = _M0L6_2atmpS4641;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4640
    = moonbit_add_string(_M0L6_2atmpS2129, (moonbit_string_t)moonbit_string_literal_49.data);
    moonbit_decref(_M0L6_2atmpS2129);
    _M0L6_2atmpS2127 = _M0L6_2atmpS4640;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2128
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS272);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4639 = moonbit_add_string(_M0L6_2atmpS2127, _M0L6_2atmpS2128);
    moonbit_decref(_M0L6_2atmpS2127);
    moonbit_decref(_M0L6_2atmpS2128);
    _M0L6_2atmpS2126 = _M0L6_2atmpS4639;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4638
    = moonbit_add_string(_M0L6_2atmpS2126, (moonbit_string_t)moonbit_string_literal_50.data);
    moonbit_decref(_M0L6_2atmpS2126);
    _M0L6_2atmpS2124 = _M0L6_2atmpS4638;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2125
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS274);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4637 = moonbit_add_string(_M0L6_2atmpS2124, _M0L6_2atmpS2125);
    moonbit_decref(_M0L6_2atmpS2124);
    moonbit_decref(_M0L6_2atmpS2125);
    _M0L6_2atmpS2123 = _M0L6_2atmpS4637;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4636
    = moonbit_add_string(_M0L6_2atmpS2123, (moonbit_string_t)moonbit_string_literal_51.data);
    moonbit_decref(_M0L6_2atmpS2123);
    _M0L6_2atmpS2121 = _M0L6_2atmpS4636;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2122
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS275);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4635 = moonbit_add_string(_M0L6_2atmpS2121, _M0L6_2atmpS2122);
    moonbit_decref(_M0L6_2atmpS2121);
    moonbit_decref(_M0L6_2atmpS2122);
    _M0L6_2atmpS2120 = _M0L6_2atmpS4635;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4634
    = moonbit_add_string(_M0L6_2atmpS2120, (moonbit_string_t)moonbit_string_literal_107.data);
    moonbit_decref(_M0L6_2atmpS2120);
    _M0L6_2atmpS2118 = _M0L6_2atmpS4634;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2119
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S276);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4633 = moonbit_add_string(_M0L6_2atmpS2118, _M0L6_2atmpS2119);
    moonbit_decref(_M0L6_2atmpS2118);
    moonbit_decref(_M0L6_2atmpS2119);
    _M0L6_2atmpS2117 = _M0L6_2atmpS4633;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4632
    = moonbit_add_string(_M0L6_2atmpS2117, (moonbit_string_t)moonbit_string_literal_108.data);
    moonbit_decref(_M0L6_2atmpS2117);
    _M0L6_2atmpS2115 = _M0L6_2atmpS4632;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2116
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S277);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4631 = moonbit_add_string(_M0L6_2atmpS2115, _M0L6_2atmpS2116);
    moonbit_decref(_M0L6_2atmpS2115);
    moonbit_decref(_M0L6_2atmpS2116);
    _M0L6_2atmpS2114 = _M0L6_2atmpS4631;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS4630
    = moonbit_add_string(_M0L6_2atmpS2114, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS2114);
    _M0L6_2atmpS2113 = _M0L6_2atmpS4630;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2112
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2112)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2112)->$0
    = _M0L6_2atmpS2113;
    _result_5144.tag = 0;
    _result_5144.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2112;
    return _result_5144;
  } else {
    int32_t _M0L6_2atmpS2137;
    struct moonbit_result_0 _result_5145;
    moonbit_decref(_M0L9args__locS273);
    moonbit_decref(_M0L3locS271);
    moonbit_decref(_M0L7contentS269);
    moonbit_decref(_M0L6actualS267);
    _M0L6_2atmpS2137 = 0;
    _result_5145.tag = 1;
    _result_5145.data.ok = _M0L6_2atmpS2137;
    return _result_5145;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS260
) {
  struct _M0TPB13StringBuilder* _M0L3bufS258;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS259;
  int32_t _M0L7_2abindS261;
  int32_t _M0L1iS262;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS258 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS259 = _M0L4selfS260;
  moonbit_incref(_M0L3bufS258);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS258, 91);
  _M0L7_2abindS261 = _M0L7_2aselfS259->$1;
  _M0L1iS262 = 0;
  while (1) {
    if (_M0L1iS262 < _M0L7_2abindS261) {
      int32_t _if__result_5147;
      moonbit_string_t* _M0L8_2afieldS4647;
      moonbit_string_t* _M0L3bufS2110;
      moonbit_string_t _M0L6_2atmpS4646;
      moonbit_string_t _M0L4itemS263;
      int32_t _M0L6_2atmpS2111;
      if (_M0L1iS262 != 0) {
        moonbit_incref(_M0L3bufS258);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS258, (moonbit_string_t)moonbit_string_literal_86.data);
      }
      if (_M0L1iS262 < 0) {
        _if__result_5147 = 1;
      } else {
        int32_t _M0L3lenS2109 = _M0L7_2aselfS259->$1;
        _if__result_5147 = _M0L1iS262 >= _M0L3lenS2109;
      }
      if (_if__result_5147) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4647 = _M0L7_2aselfS259->$0;
      _M0L3bufS2110 = _M0L8_2afieldS4647;
      _M0L6_2atmpS4646 = (moonbit_string_t)_M0L3bufS2110[_M0L1iS262];
      _M0L4itemS263 = _M0L6_2atmpS4646;
      if (_M0L4itemS263 == 0) {
        moonbit_incref(_M0L3bufS258);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS258, (moonbit_string_t)moonbit_string_literal_57.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS264 = _M0L4itemS263;
        moonbit_string_t _M0L6_2alocS265 = _M0L7_2aSomeS264;
        moonbit_string_t _M0L6_2atmpS2108;
        moonbit_incref(_M0L6_2alocS265);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS2108
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS265);
        moonbit_incref(_M0L3bufS258);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS258, _M0L6_2atmpS2108);
      }
      _M0L6_2atmpS2111 = _M0L1iS262 + 1;
      _M0L1iS262 = _M0L6_2atmpS2111;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS259);
    }
    break;
  }
  moonbit_incref(_M0L3bufS258);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS258, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS258);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS257
) {
  moonbit_string_t _M0L6_2atmpS2107;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2106;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2107 = _M0L4selfS257;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2106 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2107);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS2106);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS256
) {
  struct _M0TPB13StringBuilder* _M0L2sbS255;
  struct _M0TPC16string10StringView _M0L8_2afieldS4660;
  struct _M0TPC16string10StringView _M0L3pkgS2091;
  moonbit_string_t _M0L6_2atmpS2090;
  moonbit_string_t _M0L6_2atmpS4659;
  moonbit_string_t _M0L6_2atmpS2089;
  moonbit_string_t _M0L6_2atmpS4658;
  moonbit_string_t _M0L6_2atmpS2088;
  struct _M0TPC16string10StringView _M0L8_2afieldS4657;
  struct _M0TPC16string10StringView _M0L8filenameS2092;
  struct _M0TPC16string10StringView _M0L8_2afieldS4656;
  struct _M0TPC16string10StringView _M0L11start__lineS2095;
  moonbit_string_t _M0L6_2atmpS2094;
  moonbit_string_t _M0L6_2atmpS4655;
  moonbit_string_t _M0L6_2atmpS2093;
  struct _M0TPC16string10StringView _M0L8_2afieldS4654;
  struct _M0TPC16string10StringView _M0L13start__columnS2098;
  moonbit_string_t _M0L6_2atmpS2097;
  moonbit_string_t _M0L6_2atmpS4653;
  moonbit_string_t _M0L6_2atmpS2096;
  struct _M0TPC16string10StringView _M0L8_2afieldS4652;
  struct _M0TPC16string10StringView _M0L9end__lineS2101;
  moonbit_string_t _M0L6_2atmpS2100;
  moonbit_string_t _M0L6_2atmpS4651;
  moonbit_string_t _M0L6_2atmpS2099;
  struct _M0TPC16string10StringView _M0L8_2afieldS4650;
  int32_t _M0L6_2acntS4840;
  struct _M0TPC16string10StringView _M0L11end__columnS2105;
  moonbit_string_t _M0L6_2atmpS2104;
  moonbit_string_t _M0L6_2atmpS4649;
  moonbit_string_t _M0L6_2atmpS2103;
  moonbit_string_t _M0L6_2atmpS4648;
  moonbit_string_t _M0L6_2atmpS2102;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS255 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4660
  = (struct _M0TPC16string10StringView){
    _M0L4selfS256->$0_1, _M0L4selfS256->$0_2, _M0L4selfS256->$0_0
  };
  _M0L3pkgS2091 = _M0L8_2afieldS4660;
  moonbit_incref(_M0L3pkgS2091.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2090
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS2091);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4659
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_109.data, _M0L6_2atmpS2090);
  moonbit_decref(_M0L6_2atmpS2090);
  _M0L6_2atmpS2089 = _M0L6_2atmpS4659;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4658
  = moonbit_add_string(_M0L6_2atmpS2089, (moonbit_string_t)moonbit_string_literal_106.data);
  moonbit_decref(_M0L6_2atmpS2089);
  _M0L6_2atmpS2088 = _M0L6_2atmpS4658;
  moonbit_incref(_M0L2sbS255);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS255, _M0L6_2atmpS2088);
  moonbit_incref(_M0L2sbS255);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS255, (moonbit_string_t)moonbit_string_literal_110.data);
  _M0L8_2afieldS4657
  = (struct _M0TPC16string10StringView){
    _M0L4selfS256->$1_1, _M0L4selfS256->$1_2, _M0L4selfS256->$1_0
  };
  _M0L8filenameS2092 = _M0L8_2afieldS4657;
  moonbit_incref(_M0L8filenameS2092.$0);
  moonbit_incref(_M0L2sbS255);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS255, _M0L8filenameS2092);
  _M0L8_2afieldS4656
  = (struct _M0TPC16string10StringView){
    _M0L4selfS256->$2_1, _M0L4selfS256->$2_2, _M0L4selfS256->$2_0
  };
  _M0L11start__lineS2095 = _M0L8_2afieldS4656;
  moonbit_incref(_M0L11start__lineS2095.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2094
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS2095);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4655
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_111.data, _M0L6_2atmpS2094);
  moonbit_decref(_M0L6_2atmpS2094);
  _M0L6_2atmpS2093 = _M0L6_2atmpS4655;
  moonbit_incref(_M0L2sbS255);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS255, _M0L6_2atmpS2093);
  _M0L8_2afieldS4654
  = (struct _M0TPC16string10StringView){
    _M0L4selfS256->$3_1, _M0L4selfS256->$3_2, _M0L4selfS256->$3_0
  };
  _M0L13start__columnS2098 = _M0L8_2afieldS4654;
  moonbit_incref(_M0L13start__columnS2098.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2097
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS2098);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4653
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_112.data, _M0L6_2atmpS2097);
  moonbit_decref(_M0L6_2atmpS2097);
  _M0L6_2atmpS2096 = _M0L6_2atmpS4653;
  moonbit_incref(_M0L2sbS255);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS255, _M0L6_2atmpS2096);
  _M0L8_2afieldS4652
  = (struct _M0TPC16string10StringView){
    _M0L4selfS256->$4_1, _M0L4selfS256->$4_2, _M0L4selfS256->$4_0
  };
  _M0L9end__lineS2101 = _M0L8_2afieldS4652;
  moonbit_incref(_M0L9end__lineS2101.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2100
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS2101);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4651
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_113.data, _M0L6_2atmpS2100);
  moonbit_decref(_M0L6_2atmpS2100);
  _M0L6_2atmpS2099 = _M0L6_2atmpS4651;
  moonbit_incref(_M0L2sbS255);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS255, _M0L6_2atmpS2099);
  _M0L8_2afieldS4650
  = (struct _M0TPC16string10StringView){
    _M0L4selfS256->$5_1, _M0L4selfS256->$5_2, _M0L4selfS256->$5_0
  };
  _M0L6_2acntS4840 = Moonbit_object_header(_M0L4selfS256)->rc;
  if (_M0L6_2acntS4840 > 1) {
    int32_t _M0L11_2anew__cntS4846 = _M0L6_2acntS4840 - 1;
    Moonbit_object_header(_M0L4selfS256)->rc = _M0L11_2anew__cntS4846;
    moonbit_incref(_M0L8_2afieldS4650.$0);
  } else if (_M0L6_2acntS4840 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4845 =
      (struct _M0TPC16string10StringView){_M0L4selfS256->$4_1,
                                            _M0L4selfS256->$4_2,
                                            _M0L4selfS256->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4844;
    struct _M0TPC16string10StringView _M0L8_2afieldS4843;
    struct _M0TPC16string10StringView _M0L8_2afieldS4842;
    struct _M0TPC16string10StringView _M0L8_2afieldS4841;
    moonbit_decref(_M0L8_2afieldS4845.$0);
    _M0L8_2afieldS4844
    = (struct _M0TPC16string10StringView){
      _M0L4selfS256->$3_1, _M0L4selfS256->$3_2, _M0L4selfS256->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4844.$0);
    _M0L8_2afieldS4843
    = (struct _M0TPC16string10StringView){
      _M0L4selfS256->$2_1, _M0L4selfS256->$2_2, _M0L4selfS256->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4843.$0);
    _M0L8_2afieldS4842
    = (struct _M0TPC16string10StringView){
      _M0L4selfS256->$1_1, _M0L4selfS256->$1_2, _M0L4selfS256->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4842.$0);
    _M0L8_2afieldS4841
    = (struct _M0TPC16string10StringView){
      _M0L4selfS256->$0_1, _M0L4selfS256->$0_2, _M0L4selfS256->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4841.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS256);
  }
  _M0L11end__columnS2105 = _M0L8_2afieldS4650;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2104
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS2105);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4649
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_114.data, _M0L6_2atmpS2104);
  moonbit_decref(_M0L6_2atmpS2104);
  _M0L6_2atmpS2103 = _M0L6_2atmpS4649;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4648
  = moonbit_add_string(_M0L6_2atmpS2103, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2103);
  _M0L6_2atmpS2102 = _M0L6_2atmpS4648;
  moonbit_incref(_M0L2sbS255);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS255, _M0L6_2atmpS2102);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS255);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS253,
  moonbit_string_t _M0L3strS254
) {
  int32_t _M0L3lenS2078;
  int32_t _M0L6_2atmpS2080;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L6_2atmpS2077;
  moonbit_bytes_t _M0L8_2afieldS4662;
  moonbit_bytes_t _M0L4dataS2081;
  int32_t _M0L3lenS2082;
  int32_t _M0L6_2atmpS2083;
  int32_t _M0L3lenS2085;
  int32_t _M0L6_2atmpS4661;
  int32_t _M0L6_2atmpS2087;
  int32_t _M0L6_2atmpS2086;
  int32_t _M0L6_2atmpS2084;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2078 = _M0L4selfS253->$1;
  _M0L6_2atmpS2080 = Moonbit_array_length(_M0L3strS254);
  _M0L6_2atmpS2079 = _M0L6_2atmpS2080 * 2;
  _M0L6_2atmpS2077 = _M0L3lenS2078 + _M0L6_2atmpS2079;
  moonbit_incref(_M0L4selfS253);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS253, _M0L6_2atmpS2077);
  _M0L8_2afieldS4662 = _M0L4selfS253->$0;
  _M0L4dataS2081 = _M0L8_2afieldS4662;
  _M0L3lenS2082 = _M0L4selfS253->$1;
  _M0L6_2atmpS2083 = Moonbit_array_length(_M0L3strS254);
  moonbit_incref(_M0L4dataS2081);
  moonbit_incref(_M0L3strS254);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2081, _M0L3lenS2082, _M0L3strS254, 0, _M0L6_2atmpS2083);
  _M0L3lenS2085 = _M0L4selfS253->$1;
  _M0L6_2atmpS4661 = Moonbit_array_length(_M0L3strS254);
  moonbit_decref(_M0L3strS254);
  _M0L6_2atmpS2087 = _M0L6_2atmpS4661;
  _M0L6_2atmpS2086 = _M0L6_2atmpS2087 * 2;
  _M0L6_2atmpS2084 = _M0L3lenS2085 + _M0L6_2atmpS2086;
  _M0L4selfS253->$1 = _M0L6_2atmpS2084;
  moonbit_decref(_M0L4selfS253);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS245,
  int32_t _M0L13bytes__offsetS240,
  moonbit_string_t _M0L3strS247,
  int32_t _M0L11str__offsetS243,
  int32_t _M0L6lengthS241
) {
  int32_t _M0L6_2atmpS2076;
  int32_t _M0L6_2atmpS2075;
  int32_t _M0L2e1S239;
  int32_t _M0L6_2atmpS2074;
  int32_t _M0L2e2S242;
  int32_t _M0L4len1S244;
  int32_t _M0L4len2S246;
  int32_t _if__result_5148;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS2076 = _M0L6lengthS241 * 2;
  _M0L6_2atmpS2075 = _M0L13bytes__offsetS240 + _M0L6_2atmpS2076;
  _M0L2e1S239 = _M0L6_2atmpS2075 - 1;
  _M0L6_2atmpS2074 = _M0L11str__offsetS243 + _M0L6lengthS241;
  _M0L2e2S242 = _M0L6_2atmpS2074 - 1;
  _M0L4len1S244 = Moonbit_array_length(_M0L4selfS245);
  _M0L4len2S246 = Moonbit_array_length(_M0L3strS247);
  if (_M0L6lengthS241 >= 0) {
    if (_M0L13bytes__offsetS240 >= 0) {
      if (_M0L2e1S239 < _M0L4len1S244) {
        if (_M0L11str__offsetS243 >= 0) {
          _if__result_5148 = _M0L2e2S242 < _M0L4len2S246;
        } else {
          _if__result_5148 = 0;
        }
      } else {
        _if__result_5148 = 0;
      }
    } else {
      _if__result_5148 = 0;
    }
  } else {
    _if__result_5148 = 0;
  }
  if (_if__result_5148) {
    int32_t _M0L16end__str__offsetS248 =
      _M0L11str__offsetS243 + _M0L6lengthS241;
    int32_t _M0L1iS249 = _M0L11str__offsetS243;
    int32_t _M0L1jS250 = _M0L13bytes__offsetS240;
    while (1) {
      if (_M0L1iS249 < _M0L16end__str__offsetS248) {
        int32_t _M0L6_2atmpS2071 = _M0L3strS247[_M0L1iS249];
        int32_t _M0L6_2atmpS2070 = (int32_t)_M0L6_2atmpS2071;
        uint32_t _M0L1cS251 = *(uint32_t*)&_M0L6_2atmpS2070;
        uint32_t _M0L6_2atmpS2066 = _M0L1cS251 & 255u;
        int32_t _M0L6_2atmpS2065;
        int32_t _M0L6_2atmpS2067;
        uint32_t _M0L6_2atmpS2069;
        int32_t _M0L6_2atmpS2068;
        int32_t _M0L6_2atmpS2072;
        int32_t _M0L6_2atmpS2073;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS2065 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2066);
        if (
          _M0L1jS250 < 0 || _M0L1jS250 >= Moonbit_array_length(_M0L4selfS245)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS245[_M0L1jS250] = _M0L6_2atmpS2065;
        _M0L6_2atmpS2067 = _M0L1jS250 + 1;
        _M0L6_2atmpS2069 = _M0L1cS251 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS2068 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2069);
        if (
          _M0L6_2atmpS2067 < 0
          || _M0L6_2atmpS2067 >= Moonbit_array_length(_M0L4selfS245)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS245[_M0L6_2atmpS2067] = _M0L6_2atmpS2068;
        _M0L6_2atmpS2072 = _M0L1iS249 + 1;
        _M0L6_2atmpS2073 = _M0L1jS250 + 2;
        _M0L1iS249 = _M0L6_2atmpS2072;
        _M0L1jS250 = _M0L6_2atmpS2073;
        continue;
      } else {
        moonbit_decref(_M0L3strS247);
        moonbit_decref(_M0L4selfS245);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS247);
    moonbit_decref(_M0L4selfS245);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS236,
  double _M0L3objS235
) {
  struct _M0TPB6Logger _M0L6_2atmpS2063;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS2063
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS236
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS235, _M0L6_2atmpS2063);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS238,
  struct _M0TPC16string10StringView _M0L3objS237
) {
  struct _M0TPB6Logger _M0L6_2atmpS2064;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS2064
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS238
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS237, _M0L6_2atmpS2064);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS181
) {
  int32_t _M0L6_2atmpS2062;
  struct _M0TPC16string10StringView _M0L7_2abindS180;
  moonbit_string_t _M0L7_2adataS182;
  int32_t _M0L8_2astartS183;
  int32_t _M0L6_2atmpS2061;
  int32_t _M0L6_2aendS184;
  int32_t _M0Lm9_2acursorS185;
  int32_t _M0Lm13accept__stateS186;
  int32_t _M0Lm10match__endS187;
  int32_t _M0Lm20match__tag__saver__0S188;
  int32_t _M0Lm20match__tag__saver__1S189;
  int32_t _M0Lm20match__tag__saver__2S190;
  int32_t _M0Lm20match__tag__saver__3S191;
  int32_t _M0Lm20match__tag__saver__4S192;
  int32_t _M0Lm6tag__0S193;
  int32_t _M0Lm6tag__1S194;
  int32_t _M0Lm9tag__1__1S195;
  int32_t _M0Lm9tag__1__2S196;
  int32_t _M0Lm6tag__3S197;
  int32_t _M0Lm6tag__2S198;
  int32_t _M0Lm9tag__2__1S199;
  int32_t _M0Lm6tag__4S200;
  int32_t _M0L6_2atmpS2019;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2062 = Moonbit_array_length(_M0L4reprS181);
  _M0L7_2abindS180
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2062, _M0L4reprS181
  };
  moonbit_incref(_M0L7_2abindS180.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS182 = _M0MPC16string10StringView4data(_M0L7_2abindS180);
  moonbit_incref(_M0L7_2abindS180.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS183
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS180);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2061 = _M0MPC16string10StringView6length(_M0L7_2abindS180);
  _M0L6_2aendS184 = _M0L8_2astartS183 + _M0L6_2atmpS2061;
  _M0Lm9_2acursorS185 = _M0L8_2astartS183;
  _M0Lm13accept__stateS186 = -1;
  _M0Lm10match__endS187 = -1;
  _M0Lm20match__tag__saver__0S188 = -1;
  _M0Lm20match__tag__saver__1S189 = -1;
  _M0Lm20match__tag__saver__2S190 = -1;
  _M0Lm20match__tag__saver__3S191 = -1;
  _M0Lm20match__tag__saver__4S192 = -1;
  _M0Lm6tag__0S193 = -1;
  _M0Lm6tag__1S194 = -1;
  _M0Lm9tag__1__1S195 = -1;
  _M0Lm9tag__1__2S196 = -1;
  _M0Lm6tag__3S197 = -1;
  _M0Lm6tag__2S198 = -1;
  _M0Lm9tag__2__1S199 = -1;
  _M0Lm6tag__4S200 = -1;
  _M0L6_2atmpS2019 = _M0Lm9_2acursorS185;
  if (_M0L6_2atmpS2019 < _M0L6_2aendS184) {
    int32_t _M0L6_2atmpS2021 = _M0Lm9_2acursorS185;
    int32_t _M0L6_2atmpS2020;
    moonbit_incref(_M0L7_2adataS182);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS2020
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2021);
    if (_M0L6_2atmpS2020 == 64) {
      int32_t _M0L6_2atmpS2022 = _M0Lm9_2acursorS185;
      _M0Lm9_2acursorS185 = _M0L6_2atmpS2022 + 1;
      while (1) {
        int32_t _M0L6_2atmpS2023;
        _M0Lm6tag__0S193 = _M0Lm9_2acursorS185;
        _M0L6_2atmpS2023 = _M0Lm9_2acursorS185;
        if (_M0L6_2atmpS2023 < _M0L6_2aendS184) {
          int32_t _M0L6_2atmpS2060 = _M0Lm9_2acursorS185;
          int32_t _M0L10next__charS208;
          int32_t _M0L6_2atmpS2024;
          moonbit_incref(_M0L7_2adataS182);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS208
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2060);
          _M0L6_2atmpS2024 = _M0Lm9_2acursorS185;
          _M0Lm9_2acursorS185 = _M0L6_2atmpS2024 + 1;
          if (_M0L10next__charS208 == 58) {
            int32_t _M0L6_2atmpS2025 = _M0Lm9_2acursorS185;
            if (_M0L6_2atmpS2025 < _M0L6_2aendS184) {
              int32_t _M0L6_2atmpS2026 = _M0Lm9_2acursorS185;
              int32_t _M0L12dispatch__15S209;
              _M0Lm9_2acursorS185 = _M0L6_2atmpS2026 + 1;
              _M0L12dispatch__15S209 = 0;
              loop__label__15_212:;
              while (1) {
                int32_t _M0L6_2atmpS2027;
                switch (_M0L12dispatch__15S209) {
                  case 3: {
                    int32_t _M0L6_2atmpS2030;
                    _M0Lm9tag__1__2S196 = _M0Lm9tag__1__1S195;
                    _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2030 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2030 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2035 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS216;
                      int32_t _M0L6_2atmpS2031;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS216
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2035);
                      _M0L6_2atmpS2031 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2031 + 1;
                      if (_M0L10next__charS216 < 58) {
                        if (_M0L10next__charS216 < 48) {
                          goto join_215;
                        } else {
                          int32_t _M0L6_2atmpS2032;
                          _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                          _M0Lm9tag__2__1S199 = _M0Lm6tag__2S198;
                          _M0Lm6tag__2S198 = _M0Lm9_2acursorS185;
                          _M0Lm6tag__3S197 = _M0Lm9_2acursorS185;
                          _M0L6_2atmpS2032 = _M0Lm9_2acursorS185;
                          if (_M0L6_2atmpS2032 < _M0L6_2aendS184) {
                            int32_t _M0L6_2atmpS2034 = _M0Lm9_2acursorS185;
                            int32_t _M0L10next__charS218;
                            int32_t _M0L6_2atmpS2033;
                            moonbit_incref(_M0L7_2adataS182);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS218
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2034);
                            _M0L6_2atmpS2033 = _M0Lm9_2acursorS185;
                            _M0Lm9_2acursorS185 = _M0L6_2atmpS2033 + 1;
                            if (_M0L10next__charS218 < 48) {
                              if (_M0L10next__charS218 == 45) {
                                goto join_210;
                              } else {
                                goto join_217;
                              }
                            } else if (_M0L10next__charS218 > 57) {
                              if (_M0L10next__charS218 < 59) {
                                _M0L12dispatch__15S209 = 3;
                                goto loop__label__15_212;
                              } else {
                                goto join_217;
                              }
                            } else {
                              _M0L12dispatch__15S209 = 6;
                              goto loop__label__15_212;
                            }
                            join_217:;
                            _M0L12dispatch__15S209 = 0;
                            goto loop__label__15_212;
                          } else {
                            goto join_201;
                          }
                        }
                      } else if (_M0L10next__charS216 > 58) {
                        goto join_215;
                      } else {
                        _M0L12dispatch__15S209 = 1;
                        goto loop__label__15_212;
                      }
                      join_215:;
                      _M0L12dispatch__15S209 = 0;
                      goto loop__label__15_212;
                    } else {
                      goto join_201;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS2036;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0Lm6tag__2S198 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2036 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2036 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2038 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS220;
                      int32_t _M0L6_2atmpS2037;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS220
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2038);
                      _M0L6_2atmpS2037 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2037 + 1;
                      if (_M0L10next__charS220 < 58) {
                        if (_M0L10next__charS220 < 48) {
                          goto join_219;
                        } else {
                          _M0L12dispatch__15S209 = 2;
                          goto loop__label__15_212;
                        }
                      } else if (_M0L10next__charS220 > 58) {
                        goto join_219;
                      } else {
                        _M0L12dispatch__15S209 = 3;
                        goto loop__label__15_212;
                      }
                      join_219:;
                      _M0L12dispatch__15S209 = 0;
                      goto loop__label__15_212;
                    } else {
                      goto join_201;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS2039;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2039 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2039 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2041 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS221;
                      int32_t _M0L6_2atmpS2040;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS221
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2041);
                      _M0L6_2atmpS2040 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2040 + 1;
                      if (_M0L10next__charS221 == 58) {
                        _M0L12dispatch__15S209 = 1;
                        goto loop__label__15_212;
                      } else {
                        _M0L12dispatch__15S209 = 0;
                        goto loop__label__15_212;
                      }
                    } else {
                      goto join_201;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS2042;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0Lm6tag__4S200 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2042 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2042 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2050 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS223;
                      int32_t _M0L6_2atmpS2043;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS223
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2050);
                      _M0L6_2atmpS2043 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2043 + 1;
                      if (_M0L10next__charS223 < 58) {
                        if (_M0L10next__charS223 < 48) {
                          goto join_222;
                        } else {
                          _M0L12dispatch__15S209 = 4;
                          goto loop__label__15_212;
                        }
                      } else if (_M0L10next__charS223 > 58) {
                        goto join_222;
                      } else {
                        int32_t _M0L6_2atmpS2044;
                        _M0Lm9tag__1__2S196 = _M0Lm9tag__1__1S195;
                        _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                        _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                        _M0L6_2atmpS2044 = _M0Lm9_2acursorS185;
                        if (_M0L6_2atmpS2044 < _M0L6_2aendS184) {
                          int32_t _M0L6_2atmpS2049 = _M0Lm9_2acursorS185;
                          int32_t _M0L10next__charS225;
                          int32_t _M0L6_2atmpS2045;
                          moonbit_incref(_M0L7_2adataS182);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS225
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2049);
                          _M0L6_2atmpS2045 = _M0Lm9_2acursorS185;
                          _M0Lm9_2acursorS185 = _M0L6_2atmpS2045 + 1;
                          if (_M0L10next__charS225 < 58) {
                            if (_M0L10next__charS225 < 48) {
                              goto join_224;
                            } else {
                              int32_t _M0L6_2atmpS2046;
                              _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                              _M0Lm9tag__2__1S199 = _M0Lm6tag__2S198;
                              _M0Lm6tag__2S198 = _M0Lm9_2acursorS185;
                              _M0L6_2atmpS2046 = _M0Lm9_2acursorS185;
                              if (_M0L6_2atmpS2046 < _M0L6_2aendS184) {
                                int32_t _M0L6_2atmpS2048 =
                                  _M0Lm9_2acursorS185;
                                int32_t _M0L10next__charS227;
                                int32_t _M0L6_2atmpS2047;
                                moonbit_incref(_M0L7_2adataS182);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS227
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2048);
                                _M0L6_2atmpS2047 = _M0Lm9_2acursorS185;
                                _M0Lm9_2acursorS185 = _M0L6_2atmpS2047 + 1;
                                if (_M0L10next__charS227 < 58) {
                                  if (_M0L10next__charS227 < 48) {
                                    goto join_226;
                                  } else {
                                    _M0L12dispatch__15S209 = 5;
                                    goto loop__label__15_212;
                                  }
                                } else if (_M0L10next__charS227 > 58) {
                                  goto join_226;
                                } else {
                                  _M0L12dispatch__15S209 = 3;
                                  goto loop__label__15_212;
                                }
                                join_226:;
                                _M0L12dispatch__15S209 = 0;
                                goto loop__label__15_212;
                              } else {
                                goto join_214;
                              }
                            }
                          } else if (_M0L10next__charS225 > 58) {
                            goto join_224;
                          } else {
                            _M0L12dispatch__15S209 = 1;
                            goto loop__label__15_212;
                          }
                          join_224:;
                          _M0L12dispatch__15S209 = 0;
                          goto loop__label__15_212;
                        } else {
                          goto join_201;
                        }
                      }
                      join_222:;
                      _M0L12dispatch__15S209 = 0;
                      goto loop__label__15_212;
                    } else {
                      goto join_201;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS2051;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0Lm6tag__2S198 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2051 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2051 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2053 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS229;
                      int32_t _M0L6_2atmpS2052;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS229
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2053);
                      _M0L6_2atmpS2052 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2052 + 1;
                      if (_M0L10next__charS229 < 58) {
                        if (_M0L10next__charS229 < 48) {
                          goto join_228;
                        } else {
                          _M0L12dispatch__15S209 = 5;
                          goto loop__label__15_212;
                        }
                      } else if (_M0L10next__charS229 > 58) {
                        goto join_228;
                      } else {
                        _M0L12dispatch__15S209 = 3;
                        goto loop__label__15_212;
                      }
                      join_228:;
                      _M0L12dispatch__15S209 = 0;
                      goto loop__label__15_212;
                    } else {
                      goto join_214;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS2054;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0Lm6tag__2S198 = _M0Lm9_2acursorS185;
                    _M0Lm6tag__3S197 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2054 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2054 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2056 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS231;
                      int32_t _M0L6_2atmpS2055;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS231
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2056);
                      _M0L6_2atmpS2055 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2055 + 1;
                      if (_M0L10next__charS231 < 48) {
                        if (_M0L10next__charS231 == 45) {
                          goto join_210;
                        } else {
                          goto join_230;
                        }
                      } else if (_M0L10next__charS231 > 57) {
                        if (_M0L10next__charS231 < 59) {
                          _M0L12dispatch__15S209 = 3;
                          goto loop__label__15_212;
                        } else {
                          goto join_230;
                        }
                      } else {
                        _M0L12dispatch__15S209 = 6;
                        goto loop__label__15_212;
                      }
                      join_230:;
                      _M0L12dispatch__15S209 = 0;
                      goto loop__label__15_212;
                    } else {
                      goto join_201;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS2057;
                    _M0Lm9tag__1__1S195 = _M0Lm6tag__1S194;
                    _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                    _M0L6_2atmpS2057 = _M0Lm9_2acursorS185;
                    if (_M0L6_2atmpS2057 < _M0L6_2aendS184) {
                      int32_t _M0L6_2atmpS2059 = _M0Lm9_2acursorS185;
                      int32_t _M0L10next__charS233;
                      int32_t _M0L6_2atmpS2058;
                      moonbit_incref(_M0L7_2adataS182);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS233
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2059);
                      _M0L6_2atmpS2058 = _M0Lm9_2acursorS185;
                      _M0Lm9_2acursorS185 = _M0L6_2atmpS2058 + 1;
                      if (_M0L10next__charS233 < 58) {
                        if (_M0L10next__charS233 < 48) {
                          goto join_232;
                        } else {
                          _M0L12dispatch__15S209 = 2;
                          goto loop__label__15_212;
                        }
                      } else if (_M0L10next__charS233 > 58) {
                        goto join_232;
                      } else {
                        _M0L12dispatch__15S209 = 1;
                        goto loop__label__15_212;
                      }
                      join_232:;
                      _M0L12dispatch__15S209 = 0;
                      goto loop__label__15_212;
                    } else {
                      goto join_201;
                    }
                    break;
                  }
                  default: {
                    goto join_201;
                    break;
                  }
                }
                join_214:;
                _M0Lm6tag__1S194 = _M0Lm9tag__1__2S196;
                _M0Lm6tag__2S198 = _M0Lm9tag__2__1S199;
                _M0Lm20match__tag__saver__0S188 = _M0Lm6tag__0S193;
                _M0Lm20match__tag__saver__1S189 = _M0Lm6tag__1S194;
                _M0Lm20match__tag__saver__2S190 = _M0Lm6tag__2S198;
                _M0Lm20match__tag__saver__3S191 = _M0Lm6tag__3S197;
                _M0Lm20match__tag__saver__4S192 = _M0Lm6tag__4S200;
                _M0Lm13accept__stateS186 = 0;
                _M0Lm10match__endS187 = _M0Lm9_2acursorS185;
                goto join_201;
                join_210:;
                _M0Lm9tag__1__1S195 = _M0Lm9tag__1__2S196;
                _M0Lm6tag__1S194 = _M0Lm9_2acursorS185;
                _M0Lm6tag__2S198 = _M0Lm9tag__2__1S199;
                _M0L6_2atmpS2027 = _M0Lm9_2acursorS185;
                if (_M0L6_2atmpS2027 < _M0L6_2aendS184) {
                  int32_t _M0L6_2atmpS2029 = _M0Lm9_2acursorS185;
                  int32_t _M0L10next__charS213;
                  int32_t _M0L6_2atmpS2028;
                  moonbit_incref(_M0L7_2adataS182);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS213
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS182, _M0L6_2atmpS2029);
                  _M0L6_2atmpS2028 = _M0Lm9_2acursorS185;
                  _M0Lm9_2acursorS185 = _M0L6_2atmpS2028 + 1;
                  if (_M0L10next__charS213 < 58) {
                    if (_M0L10next__charS213 < 48) {
                      goto join_211;
                    } else {
                      _M0L12dispatch__15S209 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS213 > 58) {
                    goto join_211;
                  } else {
                    _M0L12dispatch__15S209 = 1;
                    continue;
                  }
                  join_211:;
                  _M0L12dispatch__15S209 = 0;
                  continue;
                } else {
                  goto join_201;
                }
                break;
              }
            } else {
              goto join_201;
            }
          } else {
            continue;
          }
        } else {
          goto join_201;
        }
        break;
      }
    } else {
      goto join_201;
    }
  } else {
    goto join_201;
  }
  join_201:;
  switch (_M0Lm13accept__stateS186) {
    case 0: {
      int32_t _M0L6_2atmpS2018 = _M0Lm20match__tag__saver__1S189;
      int32_t _M0L6_2atmpS2017 = _M0L6_2atmpS2018 + 1;
      int64_t _M0L6_2atmpS2014 = (int64_t)_M0L6_2atmpS2017;
      int32_t _M0L6_2atmpS2016 = _M0Lm20match__tag__saver__2S190;
      int64_t _M0L6_2atmpS2015 = (int64_t)_M0L6_2atmpS2016;
      struct _M0TPC16string10StringView _M0L11start__lineS202;
      int32_t _M0L6_2atmpS2013;
      int32_t _M0L6_2atmpS2012;
      int64_t _M0L6_2atmpS2009;
      int32_t _M0L6_2atmpS2011;
      int64_t _M0L6_2atmpS2010;
      struct _M0TPC16string10StringView _M0L13start__columnS203;
      int32_t _M0L6_2atmpS2008;
      int64_t _M0L6_2atmpS2005;
      int32_t _M0L6_2atmpS2007;
      int64_t _M0L6_2atmpS2006;
      struct _M0TPC16string10StringView _M0L3pkgS204;
      int32_t _M0L6_2atmpS2004;
      int32_t _M0L6_2atmpS2003;
      int64_t _M0L6_2atmpS2000;
      int32_t _M0L6_2atmpS2002;
      int64_t _M0L6_2atmpS2001;
      struct _M0TPC16string10StringView _M0L8filenameS205;
      int32_t _M0L6_2atmpS1999;
      int32_t _M0L6_2atmpS1998;
      int64_t _M0L6_2atmpS1995;
      int32_t _M0L6_2atmpS1997;
      int64_t _M0L6_2atmpS1996;
      struct _M0TPC16string10StringView _M0L9end__lineS206;
      int32_t _M0L6_2atmpS1994;
      int32_t _M0L6_2atmpS1993;
      int64_t _M0L6_2atmpS1990;
      int32_t _M0L6_2atmpS1992;
      int64_t _M0L6_2atmpS1991;
      struct _M0TPC16string10StringView _M0L11end__columnS207;
      struct _M0TPB13SourceLocRepr* _block_5165;
      moonbit_incref(_M0L7_2adataS182);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS202
      = _M0MPC16string6String4view(_M0L7_2adataS182, _M0L6_2atmpS2014, _M0L6_2atmpS2015);
      _M0L6_2atmpS2013 = _M0Lm20match__tag__saver__2S190;
      _M0L6_2atmpS2012 = _M0L6_2atmpS2013 + 1;
      _M0L6_2atmpS2009 = (int64_t)_M0L6_2atmpS2012;
      _M0L6_2atmpS2011 = _M0Lm20match__tag__saver__3S191;
      _M0L6_2atmpS2010 = (int64_t)_M0L6_2atmpS2011;
      moonbit_incref(_M0L7_2adataS182);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS203
      = _M0MPC16string6String4view(_M0L7_2adataS182, _M0L6_2atmpS2009, _M0L6_2atmpS2010);
      _M0L6_2atmpS2008 = _M0L8_2astartS183 + 1;
      _M0L6_2atmpS2005 = (int64_t)_M0L6_2atmpS2008;
      _M0L6_2atmpS2007 = _M0Lm20match__tag__saver__0S188;
      _M0L6_2atmpS2006 = (int64_t)_M0L6_2atmpS2007;
      moonbit_incref(_M0L7_2adataS182);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS204
      = _M0MPC16string6String4view(_M0L7_2adataS182, _M0L6_2atmpS2005, _M0L6_2atmpS2006);
      _M0L6_2atmpS2004 = _M0Lm20match__tag__saver__0S188;
      _M0L6_2atmpS2003 = _M0L6_2atmpS2004 + 1;
      _M0L6_2atmpS2000 = (int64_t)_M0L6_2atmpS2003;
      _M0L6_2atmpS2002 = _M0Lm20match__tag__saver__1S189;
      _M0L6_2atmpS2001 = (int64_t)_M0L6_2atmpS2002;
      moonbit_incref(_M0L7_2adataS182);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS205
      = _M0MPC16string6String4view(_M0L7_2adataS182, _M0L6_2atmpS2000, _M0L6_2atmpS2001);
      _M0L6_2atmpS1999 = _M0Lm20match__tag__saver__3S191;
      _M0L6_2atmpS1998 = _M0L6_2atmpS1999 + 1;
      _M0L6_2atmpS1995 = (int64_t)_M0L6_2atmpS1998;
      _M0L6_2atmpS1997 = _M0Lm20match__tag__saver__4S192;
      _M0L6_2atmpS1996 = (int64_t)_M0L6_2atmpS1997;
      moonbit_incref(_M0L7_2adataS182);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS206
      = _M0MPC16string6String4view(_M0L7_2adataS182, _M0L6_2atmpS1995, _M0L6_2atmpS1996);
      _M0L6_2atmpS1994 = _M0Lm20match__tag__saver__4S192;
      _M0L6_2atmpS1993 = _M0L6_2atmpS1994 + 1;
      _M0L6_2atmpS1990 = (int64_t)_M0L6_2atmpS1993;
      _M0L6_2atmpS1992 = _M0Lm10match__endS187;
      _M0L6_2atmpS1991 = (int64_t)_M0L6_2atmpS1992;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS207
      = _M0MPC16string6String4view(_M0L7_2adataS182, _M0L6_2atmpS1990, _M0L6_2atmpS1991);
      _block_5165
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_5165)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_5165->$0_0 = _M0L3pkgS204.$0;
      _block_5165->$0_1 = _M0L3pkgS204.$1;
      _block_5165->$0_2 = _M0L3pkgS204.$2;
      _block_5165->$1_0 = _M0L8filenameS205.$0;
      _block_5165->$1_1 = _M0L8filenameS205.$1;
      _block_5165->$1_2 = _M0L8filenameS205.$2;
      _block_5165->$2_0 = _M0L11start__lineS202.$0;
      _block_5165->$2_1 = _M0L11start__lineS202.$1;
      _block_5165->$2_2 = _M0L11start__lineS202.$2;
      _block_5165->$3_0 = _M0L13start__columnS203.$0;
      _block_5165->$3_1 = _M0L13start__columnS203.$1;
      _block_5165->$3_2 = _M0L13start__columnS203.$2;
      _block_5165->$4_0 = _M0L9end__lineS206.$0;
      _block_5165->$4_1 = _M0L9end__lineS206.$1;
      _block_5165->$4_2 = _M0L9end__lineS206.$2;
      _block_5165->$5_0 = _M0L11end__columnS207.$0;
      _block_5165->$5_1 = _M0L11end__columnS207.$1;
      _block_5165->$5_2 = _M0L11end__columnS207.$2;
      return _block_5165;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS182);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS178,
  int32_t _M0L5indexS179
) {
  int32_t _M0L3lenS177;
  int32_t _if__result_5166;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS177 = _M0L4selfS178->$1;
  if (_M0L5indexS179 >= 0) {
    _if__result_5166 = _M0L5indexS179 < _M0L3lenS177;
  } else {
    _if__result_5166 = 0;
  }
  if (_if__result_5166) {
    moonbit_string_t* _M0L6_2atmpS1989;
    moonbit_string_t _M0L6_2atmpS4663;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1989 = _M0MPC15array5Array6bufferGsE(_M0L4selfS178);
    if (
      _M0L5indexS179 < 0
      || _M0L5indexS179 >= Moonbit_array_length(_M0L6_2atmpS1989)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4663 = (moonbit_string_t)_M0L6_2atmpS1989[_M0L5indexS179];
    moonbit_incref(_M0L6_2atmpS4663);
    moonbit_decref(_M0L6_2atmpS1989);
    return _M0L6_2atmpS4663;
  } else {
    moonbit_decref(_M0L4selfS178);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS173
) {
  moonbit_string_t* _M0L8_2afieldS4664;
  int32_t _M0L6_2acntS4847;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4664 = _M0L4selfS173->$0;
  _M0L6_2acntS4847 = Moonbit_object_header(_M0L4selfS173)->rc;
  if (_M0L6_2acntS4847 > 1) {
    int32_t _M0L11_2anew__cntS4848 = _M0L6_2acntS4847 - 1;
    Moonbit_object_header(_M0L4selfS173)->rc = _M0L11_2anew__cntS4848;
    moonbit_incref(_M0L8_2afieldS4664);
  } else if (_M0L6_2acntS4847 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS173);
  }
  return _M0L8_2afieldS4664;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS174
) {
  struct _M0TUsiE** _M0L8_2afieldS4665;
  int32_t _M0L6_2acntS4849;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4665 = _M0L4selfS174->$0;
  _M0L6_2acntS4849 = Moonbit_object_header(_M0L4selfS174)->rc;
  if (_M0L6_2acntS4849 > 1) {
    int32_t _M0L11_2anew__cntS4850 = _M0L6_2acntS4849 - 1;
    Moonbit_object_header(_M0L4selfS174)->rc = _M0L11_2anew__cntS4850;
    moonbit_incref(_M0L8_2afieldS4665);
  } else if (_M0L6_2acntS4849 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS174);
  }
  return _M0L8_2afieldS4665;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS175
) {
  void** _M0L8_2afieldS4666;
  int32_t _M0L6_2acntS4851;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4666 = _M0L4selfS175->$0;
  _M0L6_2acntS4851 = Moonbit_object_header(_M0L4selfS175)->rc;
  if (_M0L6_2acntS4851 > 1) {
    int32_t _M0L11_2anew__cntS4852 = _M0L6_2acntS4851 - 1;
    Moonbit_object_header(_M0L4selfS175)->rc = _M0L11_2anew__cntS4852;
    moonbit_incref(_M0L8_2afieldS4666);
  } else if (_M0L6_2acntS4851 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS175);
  }
  return _M0L8_2afieldS4666;
}

struct _M0TPC16string10StringView* _M0MPC15array5Array6bufferGRPC16string10StringViewE(
  struct _M0TPB5ArrayGRPC16string10StringViewE* _M0L4selfS176
) {
  struct _M0TPC16string10StringView* _M0L8_2afieldS4667;
  int32_t _M0L6_2acntS4853;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4667 = _M0L4selfS176->$0;
  _M0L6_2acntS4853 = Moonbit_object_header(_M0L4selfS176)->rc;
  if (_M0L6_2acntS4853 > 1) {
    int32_t _M0L11_2anew__cntS4854 = _M0L6_2acntS4853 - 1;
    Moonbit_object_header(_M0L4selfS176)->rc = _M0L11_2anew__cntS4854;
    moonbit_incref(_M0L8_2afieldS4667);
  } else if (_M0L6_2acntS4853 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS176);
  }
  return _M0L8_2afieldS4667;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS172) {
  struct _M0TPB13StringBuilder* _M0L3bufS171;
  struct _M0TPB6Logger _M0L6_2atmpS1988;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS171 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS171);
  _M0L6_2atmpS1988
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS171
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS172, _M0L6_2atmpS1988);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS171);
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS165
) {
  int32_t _M0L17codepoint__lengthS164;
  int32_t _M0L6_2atmpS1987;
  moonbit_bytes_t _M0L4dataS166;
  int32_t _M0L1iS167;
  int32_t _M0L12utf16__indexS168;
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_incref(_M0L1sS165);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L17codepoint__lengthS164
  = _M0MPC16string6String20char__length_2einner(_M0L1sS165, 0, 4294967296ll);
  _M0L6_2atmpS1987 = _M0L17codepoint__lengthS164 * 4;
  _M0L4dataS166 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1987, 0);
  _M0L1iS167 = 0;
  _M0L12utf16__indexS168 = 0;
  while (1) {
    if (_M0L1iS167 < _M0L17codepoint__lengthS164) {
      int32_t _M0L6_2atmpS1984;
      int32_t _M0L1cS169;
      int32_t _M0L6_2atmpS1985;
      int32_t _M0L6_2atmpS1986;
      moonbit_incref(_M0L1sS165);
      #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1984
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS165, _M0L12utf16__indexS168);
      _M0L1cS169 = _M0L6_2atmpS1984;
      if (_M0L1cS169 > 65535) {
        int32_t _M0L6_2atmpS1952 = _M0L1iS167 * 4;
        int32_t _M0L6_2atmpS1954 = _M0L1cS169 & 255;
        int32_t _M0L6_2atmpS1953 = _M0L6_2atmpS1954 & 0xff;
        int32_t _M0L6_2atmpS1959;
        int32_t _M0L6_2atmpS1955;
        int32_t _M0L6_2atmpS1958;
        int32_t _M0L6_2atmpS1957;
        int32_t _M0L6_2atmpS1956;
        int32_t _M0L6_2atmpS1964;
        int32_t _M0L6_2atmpS1960;
        int32_t _M0L6_2atmpS1963;
        int32_t _M0L6_2atmpS1962;
        int32_t _M0L6_2atmpS1961;
        int32_t _M0L6_2atmpS1969;
        int32_t _M0L6_2atmpS1965;
        int32_t _M0L6_2atmpS1968;
        int32_t _M0L6_2atmpS1967;
        int32_t _M0L6_2atmpS1966;
        int32_t _M0L6_2atmpS1970;
        int32_t _M0L6_2atmpS1971;
        if (
          _M0L6_2atmpS1952 < 0
          || _M0L6_2atmpS1952 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1952] = _M0L6_2atmpS1953;
        _M0L6_2atmpS1959 = _M0L1iS167 * 4;
        _M0L6_2atmpS1955 = _M0L6_2atmpS1959 + 1;
        _M0L6_2atmpS1958 = _M0L1cS169 >> 8;
        _M0L6_2atmpS1957 = _M0L6_2atmpS1958 & 255;
        _M0L6_2atmpS1956 = _M0L6_2atmpS1957 & 0xff;
        if (
          _M0L6_2atmpS1955 < 0
          || _M0L6_2atmpS1955 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1955] = _M0L6_2atmpS1956;
        _M0L6_2atmpS1964 = _M0L1iS167 * 4;
        _M0L6_2atmpS1960 = _M0L6_2atmpS1964 + 2;
        _M0L6_2atmpS1963 = _M0L1cS169 >> 16;
        _M0L6_2atmpS1962 = _M0L6_2atmpS1963 & 255;
        _M0L6_2atmpS1961 = _M0L6_2atmpS1962 & 0xff;
        if (
          _M0L6_2atmpS1960 < 0
          || _M0L6_2atmpS1960 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1960] = _M0L6_2atmpS1961;
        _M0L6_2atmpS1969 = _M0L1iS167 * 4;
        _M0L6_2atmpS1965 = _M0L6_2atmpS1969 + 3;
        _M0L6_2atmpS1968 = _M0L1cS169 >> 24;
        _M0L6_2atmpS1967 = _M0L6_2atmpS1968 & 255;
        _M0L6_2atmpS1966 = _M0L6_2atmpS1967 & 0xff;
        if (
          _M0L6_2atmpS1965 < 0
          || _M0L6_2atmpS1965 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1965] = _M0L6_2atmpS1966;
        _M0L6_2atmpS1970 = _M0L1iS167 + 1;
        _M0L6_2atmpS1971 = _M0L12utf16__indexS168 + 2;
        _M0L1iS167 = _M0L6_2atmpS1970;
        _M0L12utf16__indexS168 = _M0L6_2atmpS1971;
        continue;
      } else {
        int32_t _M0L6_2atmpS1972 = _M0L1iS167 * 4;
        int32_t _M0L6_2atmpS1974 = _M0L1cS169 & 255;
        int32_t _M0L6_2atmpS1973 = _M0L6_2atmpS1974 & 0xff;
        int32_t _M0L6_2atmpS1979;
        int32_t _M0L6_2atmpS1975;
        int32_t _M0L6_2atmpS1978;
        int32_t _M0L6_2atmpS1977;
        int32_t _M0L6_2atmpS1976;
        int32_t _M0L6_2atmpS1981;
        int32_t _M0L6_2atmpS1980;
        int32_t _M0L6_2atmpS1983;
        int32_t _M0L6_2atmpS1982;
        if (
          _M0L6_2atmpS1972 < 0
          || _M0L6_2atmpS1972 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1972] = _M0L6_2atmpS1973;
        _M0L6_2atmpS1979 = _M0L1iS167 * 4;
        _M0L6_2atmpS1975 = _M0L6_2atmpS1979 + 1;
        _M0L6_2atmpS1978 = _M0L1cS169 >> 8;
        _M0L6_2atmpS1977 = _M0L6_2atmpS1978 & 255;
        _M0L6_2atmpS1976 = _M0L6_2atmpS1977 & 0xff;
        if (
          _M0L6_2atmpS1975 < 0
          || _M0L6_2atmpS1975 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1975] = _M0L6_2atmpS1976;
        _M0L6_2atmpS1981 = _M0L1iS167 * 4;
        _M0L6_2atmpS1980 = _M0L6_2atmpS1981 + 2;
        if (
          _M0L6_2atmpS1980 < 0
          || _M0L6_2atmpS1980 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1980] = 0;
        _M0L6_2atmpS1983 = _M0L1iS167 * 4;
        _M0L6_2atmpS1982 = _M0L6_2atmpS1983 + 3;
        if (
          _M0L6_2atmpS1982 < 0
          || _M0L6_2atmpS1982 >= Moonbit_array_length(_M0L4dataS166)
        ) {
          #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS166[_M0L6_2atmpS1982] = 0;
      }
      _M0L6_2atmpS1985 = _M0L1iS167 + 1;
      _M0L6_2atmpS1986 = _M0L12utf16__indexS168 + 1;
      _M0L1iS167 = _M0L6_2atmpS1985;
      _M0L12utf16__indexS168 = _M0L6_2atmpS1986;
      continue;
    } else {
      moonbit_decref(_M0L1sS165);
    }
    break;
  }
  #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS166);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS161,
  int32_t _M0L5indexS162
) {
  int32_t _M0L2c1S160;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S160 = _M0L4selfS161[_M0L5indexS162];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S160)) {
    int32_t _M0L6_2atmpS1951 = _M0L5indexS162 + 1;
    int32_t _M0L6_2atmpS4668 = _M0L4selfS161[_M0L6_2atmpS1951];
    int32_t _M0L2c2S163;
    int32_t _M0L6_2atmpS1949;
    int32_t _M0L6_2atmpS1950;
    moonbit_decref(_M0L4selfS161);
    _M0L2c2S163 = _M0L6_2atmpS4668;
    _M0L6_2atmpS1949 = (int32_t)_M0L2c1S160;
    _M0L6_2atmpS1950 = (int32_t)_M0L2c2S163;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1949, _M0L6_2atmpS1950);
  } else {
    moonbit_decref(_M0L4selfS161);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S160);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS159) {
  int32_t _M0L6_2atmpS1948;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1948 = (int32_t)_M0L4selfS159;
  return _M0L6_2atmpS1948;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS157,
  int32_t _M0L8trailingS158
) {
  int32_t _M0L6_2atmpS1947;
  int32_t _M0L6_2atmpS1946;
  int32_t _M0L6_2atmpS1945;
  int32_t _M0L6_2atmpS1944;
  int32_t _M0L6_2atmpS1943;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1947 = _M0L7leadingS157 - 55296;
  _M0L6_2atmpS1946 = _M0L6_2atmpS1947 * 1024;
  _M0L6_2atmpS1945 = _M0L6_2atmpS1946 + _M0L8trailingS158;
  _M0L6_2atmpS1944 = _M0L6_2atmpS1945 - 56320;
  _M0L6_2atmpS1943 = _M0L6_2atmpS1944 + 65536;
  return _M0L6_2atmpS1943;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS150,
  int32_t _M0L13start__offsetS151,
  int64_t _M0L11end__offsetS148
) {
  int32_t _M0L11end__offsetS147;
  int32_t _if__result_5168;
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS148 == 4294967296ll) {
    _M0L11end__offsetS147 = Moonbit_array_length(_M0L4selfS150);
  } else {
    int64_t _M0L7_2aSomeS149 = _M0L11end__offsetS148;
    _M0L11end__offsetS147 = (int32_t)_M0L7_2aSomeS149;
  }
  if (_M0L13start__offsetS151 >= 0) {
    if (_M0L13start__offsetS151 <= _M0L11end__offsetS147) {
      int32_t _M0L6_2atmpS1936 = Moonbit_array_length(_M0L4selfS150);
      _if__result_5168 = _M0L11end__offsetS147 <= _M0L6_2atmpS1936;
    } else {
      _if__result_5168 = 0;
    }
  } else {
    _if__result_5168 = 0;
  }
  if (_if__result_5168) {
    int32_t _M0L12utf16__indexS152 = _M0L13start__offsetS151;
    int32_t _M0L11char__countS153 = 0;
    while (1) {
      if (_M0L12utf16__indexS152 < _M0L11end__offsetS147) {
        int32_t _M0L2c1S154 = _M0L4selfS150[_M0L12utf16__indexS152];
        int32_t _if__result_5170;
        int32_t _M0L6_2atmpS1941;
        int32_t _M0L6_2atmpS1942;
        #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S154)) {
          int32_t _M0L6_2atmpS1937 = _M0L12utf16__indexS152 + 1;
          _if__result_5170 = _M0L6_2atmpS1937 < _M0L11end__offsetS147;
        } else {
          _if__result_5170 = 0;
        }
        if (_if__result_5170) {
          int32_t _M0L6_2atmpS1940 = _M0L12utf16__indexS152 + 1;
          int32_t _M0L2c2S155 = _M0L4selfS150[_M0L6_2atmpS1940];
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S155)) {
            int32_t _M0L6_2atmpS1938 = _M0L12utf16__indexS152 + 2;
            int32_t _M0L6_2atmpS1939 = _M0L11char__countS153 + 1;
            _M0L12utf16__indexS152 = _M0L6_2atmpS1938;
            _M0L11char__countS153 = _M0L6_2atmpS1939;
            continue;
          } else {
            #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
            _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_92.data, (moonbit_string_t)moonbit_string_literal_115.data);
          }
        }
        _M0L6_2atmpS1941 = _M0L12utf16__indexS152 + 1;
        _M0L6_2atmpS1942 = _M0L11char__countS153 + 1;
        _M0L12utf16__indexS152 = _M0L6_2atmpS1941;
        _M0L11char__countS153 = _M0L6_2atmpS1942;
        continue;
      } else {
        moonbit_decref(_M0L4selfS150);
        return _M0L11char__countS153;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS150);
    #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_116.data, (moonbit_string_t)moonbit_string_literal_117.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS146) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS146 >= 56320) {
    return _M0L4selfS146 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS145) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS145 >= 55296) {
    return _M0L4selfS145 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS126) {
  struct _M0TPB13StringBuilder* _M0L3bufS124;
  int32_t _M0L3lenS125;
  int32_t _M0L3remS127;
  int32_t _M0L1iS128;
  #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L3bufS124 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS125 = Moonbit_array_length(_M0L4dataS126);
  _M0L3remS127 = _M0L3lenS125 % 3;
  _M0L1iS128 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1888 = _M0L3lenS125 - _M0L3remS127;
    if (_M0L1iS128 < _M0L6_2atmpS1888) {
      int32_t _M0L6_2atmpS1910;
      int32_t _M0L2b0S129;
      int32_t _M0L6_2atmpS1909;
      int32_t _M0L6_2atmpS1908;
      int32_t _M0L2b1S130;
      int32_t _M0L6_2atmpS1907;
      int32_t _M0L6_2atmpS1906;
      int32_t _M0L2b2S131;
      int32_t _M0L6_2atmpS1905;
      int32_t _M0L6_2atmpS1904;
      int32_t _M0L2x0S132;
      int32_t _M0L6_2atmpS1903;
      int32_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1902;
      int32_t _M0L6_2atmpS1901;
      int32_t _M0L6_2atmpS1899;
      int32_t _M0L2x1S133;
      int32_t _M0L6_2atmpS1898;
      int32_t _M0L6_2atmpS1895;
      int32_t _M0L6_2atmpS1897;
      int32_t _M0L6_2atmpS1896;
      int32_t _M0L6_2atmpS1894;
      int32_t _M0L2x2S134;
      int32_t _M0L6_2atmpS1893;
      int32_t _M0L2x3S135;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1891;
      int32_t _M0L6_2atmpS1892;
      int32_t _M0L6_2atmpS1911;
      if (
        _M0L1iS128 < 0 || _M0L1iS128 >= Moonbit_array_length(_M0L4dataS126)
      ) {
        #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1910 = (int32_t)_M0L4dataS126[_M0L1iS128];
      _M0L2b0S129 = (int32_t)_M0L6_2atmpS1910;
      _M0L6_2atmpS1909 = _M0L1iS128 + 1;
      if (
        _M0L6_2atmpS1909 < 0
        || _M0L6_2atmpS1909 >= Moonbit_array_length(_M0L4dataS126)
      ) {
        #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1908 = (int32_t)_M0L4dataS126[_M0L6_2atmpS1909];
      _M0L2b1S130 = (int32_t)_M0L6_2atmpS1908;
      _M0L6_2atmpS1907 = _M0L1iS128 + 2;
      if (
        _M0L6_2atmpS1907 < 0
        || _M0L6_2atmpS1907 >= Moonbit_array_length(_M0L4dataS126)
      ) {
        #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1906 = (int32_t)_M0L4dataS126[_M0L6_2atmpS1907];
      _M0L2b2S131 = (int32_t)_M0L6_2atmpS1906;
      _M0L6_2atmpS1905 = _M0L2b0S129 & 252;
      _M0L6_2atmpS1904 = _M0L6_2atmpS1905 >> 2;
      if (
        _M0L6_2atmpS1904 < 0
        || _M0L6_2atmpS1904
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x0S132 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1904];
      _M0L6_2atmpS1903 = _M0L2b0S129 & 3;
      _M0L6_2atmpS1900 = _M0L6_2atmpS1903 << 4;
      _M0L6_2atmpS1902 = _M0L2b1S130 & 240;
      _M0L6_2atmpS1901 = _M0L6_2atmpS1902 >> 4;
      _M0L6_2atmpS1899 = _M0L6_2atmpS1900 | _M0L6_2atmpS1901;
      if (
        _M0L6_2atmpS1899 < 0
        || _M0L6_2atmpS1899
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x1S133 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1899];
      _M0L6_2atmpS1898 = _M0L2b1S130 & 15;
      _M0L6_2atmpS1895 = _M0L6_2atmpS1898 << 2;
      _M0L6_2atmpS1897 = _M0L2b2S131 & 192;
      _M0L6_2atmpS1896 = _M0L6_2atmpS1897 >> 6;
      _M0L6_2atmpS1894 = _M0L6_2atmpS1895 | _M0L6_2atmpS1896;
      if (
        _M0L6_2atmpS1894 < 0
        || _M0L6_2atmpS1894
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x2S134 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1894];
      _M0L6_2atmpS1893 = _M0L2b2S131 & 63;
      if (
        _M0L6_2atmpS1893 < 0
        || _M0L6_2atmpS1893
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x3S135 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1893];
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1889 = _M0MPC14byte4Byte8to__char(_M0L2x0S132);
      moonbit_incref(_M0L3bufS124);
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1889);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1890 = _M0MPC14byte4Byte8to__char(_M0L2x1S133);
      moonbit_incref(_M0L3bufS124);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1890);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1891 = _M0MPC14byte4Byte8to__char(_M0L2x2S134);
      moonbit_incref(_M0L3bufS124);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1891);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1892 = _M0MPC14byte4Byte8to__char(_M0L2x3S135);
      moonbit_incref(_M0L3bufS124);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1892);
      _M0L6_2atmpS1911 = _M0L1iS128 + 3;
      _M0L1iS128 = _M0L6_2atmpS1911;
      continue;
    }
    break;
  }
  if (_M0L3remS127 == 1) {
    int32_t _M0L6_2atmpS1919 = _M0L3lenS125 - 1;
    int32_t _M0L6_2atmpS4669;
    int32_t _M0L6_2atmpS1918;
    int32_t _M0L2b0S137;
    int32_t _M0L6_2atmpS1917;
    int32_t _M0L6_2atmpS1916;
    int32_t _M0L2x0S138;
    int32_t _M0L6_2atmpS1915;
    int32_t _M0L6_2atmpS1914;
    int32_t _M0L2x1S139;
    int32_t _M0L6_2atmpS1912;
    int32_t _M0L6_2atmpS1913;
    if (
      _M0L6_2atmpS1919 < 0
      || _M0L6_2atmpS1919 >= Moonbit_array_length(_M0L4dataS126)
    ) {
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4669 = (int32_t)_M0L4dataS126[_M0L6_2atmpS1919];
    moonbit_decref(_M0L4dataS126);
    _M0L6_2atmpS1918 = _M0L6_2atmpS4669;
    _M0L2b0S137 = (int32_t)_M0L6_2atmpS1918;
    _M0L6_2atmpS1917 = _M0L2b0S137 & 252;
    _M0L6_2atmpS1916 = _M0L6_2atmpS1917 >> 2;
    if (
      _M0L6_2atmpS1916 < 0
      || _M0L6_2atmpS1916
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S138 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1916];
    _M0L6_2atmpS1915 = _M0L2b0S137 & 3;
    _M0L6_2atmpS1914 = _M0L6_2atmpS1915 << 4;
    if (
      _M0L6_2atmpS1914 < 0
      || _M0L6_2atmpS1914
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S139 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1914];
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1912 = _M0MPC14byte4Byte8to__char(_M0L2x0S138);
    moonbit_incref(_M0L3bufS124);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1912);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1913 = _M0MPC14byte4Byte8to__char(_M0L2x1S139);
    moonbit_incref(_M0L3bufS124);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1913);
    moonbit_incref(_M0L3bufS124);
    #line 85 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, 61);
    moonbit_incref(_M0L3bufS124);
    #line 86 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, 61);
  } else if (_M0L3remS127 == 2) {
    int32_t _M0L6_2atmpS1935 = _M0L3lenS125 - 2;
    int32_t _M0L6_2atmpS1934;
    int32_t _M0L2b0S140;
    int32_t _M0L6_2atmpS1933;
    int32_t _M0L6_2atmpS4670;
    int32_t _M0L6_2atmpS1932;
    int32_t _M0L2b1S141;
    int32_t _M0L6_2atmpS1931;
    int32_t _M0L6_2atmpS1930;
    int32_t _M0L2x0S142;
    int32_t _M0L6_2atmpS1929;
    int32_t _M0L6_2atmpS1926;
    int32_t _M0L6_2atmpS1928;
    int32_t _M0L6_2atmpS1927;
    int32_t _M0L6_2atmpS1925;
    int32_t _M0L2x1S143;
    int32_t _M0L6_2atmpS1924;
    int32_t _M0L6_2atmpS1923;
    int32_t _M0L2x2S144;
    int32_t _M0L6_2atmpS1920;
    int32_t _M0L6_2atmpS1921;
    int32_t _M0L6_2atmpS1922;
    if (
      _M0L6_2atmpS1935 < 0
      || _M0L6_2atmpS1935 >= Moonbit_array_length(_M0L4dataS126)
    ) {
      #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1934 = (int32_t)_M0L4dataS126[_M0L6_2atmpS1935];
    _M0L2b0S140 = (int32_t)_M0L6_2atmpS1934;
    _M0L6_2atmpS1933 = _M0L3lenS125 - 1;
    if (
      _M0L6_2atmpS1933 < 0
      || _M0L6_2atmpS1933 >= Moonbit_array_length(_M0L4dataS126)
    ) {
      #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4670 = (int32_t)_M0L4dataS126[_M0L6_2atmpS1933];
    moonbit_decref(_M0L4dataS126);
    _M0L6_2atmpS1932 = _M0L6_2atmpS4670;
    _M0L2b1S141 = (int32_t)_M0L6_2atmpS1932;
    _M0L6_2atmpS1931 = _M0L2b0S140 & 252;
    _M0L6_2atmpS1930 = _M0L6_2atmpS1931 >> 2;
    if (
      _M0L6_2atmpS1930 < 0
      || _M0L6_2atmpS1930
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S142 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1930];
    _M0L6_2atmpS1929 = _M0L2b0S140 & 3;
    _M0L6_2atmpS1926 = _M0L6_2atmpS1929 << 4;
    _M0L6_2atmpS1928 = _M0L2b1S141 & 240;
    _M0L6_2atmpS1927 = _M0L6_2atmpS1928 >> 4;
    _M0L6_2atmpS1925 = _M0L6_2atmpS1926 | _M0L6_2atmpS1927;
    if (
      _M0L6_2atmpS1925 < 0
      || _M0L6_2atmpS1925
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S143 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1925];
    _M0L6_2atmpS1924 = _M0L2b1S141 & 15;
    _M0L6_2atmpS1923 = _M0L6_2atmpS1924 << 2;
    if (
      _M0L6_2atmpS1923 < 0
      || _M0L6_2atmpS1923
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x2S144 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1923];
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1920 = _M0MPC14byte4Byte8to__char(_M0L2x0S142);
    moonbit_incref(_M0L3bufS124);
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1920);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1921 = _M0MPC14byte4Byte8to__char(_M0L2x1S143);
    moonbit_incref(_M0L3bufS124);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1921);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1922 = _M0MPC14byte4Byte8to__char(_M0L2x2S144);
    moonbit_incref(_M0L3bufS124);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, _M0L6_2atmpS1922);
    moonbit_incref(_M0L3bufS124);
    #line 96 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS124, 61);
  } else {
    moonbit_decref(_M0L4dataS126);
  }
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS124);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS121,
  int32_t _M0L2chS123
) {
  int32_t _M0L3lenS1883;
  int32_t _M0L6_2atmpS1882;
  moonbit_bytes_t _M0L8_2afieldS4671;
  moonbit_bytes_t _M0L4dataS1886;
  int32_t _M0L3lenS1887;
  int32_t _M0L3incS122;
  int32_t _M0L3lenS1885;
  int32_t _M0L6_2atmpS1884;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1883 = _M0L4selfS121->$1;
  _M0L6_2atmpS1882 = _M0L3lenS1883 + 4;
  moonbit_incref(_M0L4selfS121);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS121, _M0L6_2atmpS1882);
  _M0L8_2afieldS4671 = _M0L4selfS121->$0;
  _M0L4dataS1886 = _M0L8_2afieldS4671;
  _M0L3lenS1887 = _M0L4selfS121->$1;
  moonbit_incref(_M0L4dataS1886);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS122
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1886, _M0L3lenS1887, _M0L2chS123);
  _M0L3lenS1885 = _M0L4selfS121->$1;
  _M0L6_2atmpS1884 = _M0L3lenS1885 + _M0L3incS122;
  _M0L4selfS121->$1 = _M0L6_2atmpS1884;
  moonbit_decref(_M0L4selfS121);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS116,
  int32_t _M0L8requiredS117
) {
  moonbit_bytes_t _M0L8_2afieldS4675;
  moonbit_bytes_t _M0L4dataS1881;
  int32_t _M0L6_2atmpS4674;
  int32_t _M0L12current__lenS115;
  int32_t _M0Lm13enough__spaceS118;
  int32_t _M0L6_2atmpS1879;
  int32_t _M0L6_2atmpS1880;
  moonbit_bytes_t _M0L9new__dataS120;
  moonbit_bytes_t _M0L8_2afieldS4673;
  moonbit_bytes_t _M0L4dataS1877;
  int32_t _M0L3lenS1878;
  moonbit_bytes_t _M0L6_2aoldS4672;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4675 = _M0L4selfS116->$0;
  _M0L4dataS1881 = _M0L8_2afieldS4675;
  _M0L6_2atmpS4674 = Moonbit_array_length(_M0L4dataS1881);
  _M0L12current__lenS115 = _M0L6_2atmpS4674;
  if (_M0L8requiredS117 <= _M0L12current__lenS115) {
    moonbit_decref(_M0L4selfS116);
    return 0;
  }
  _M0Lm13enough__spaceS118 = _M0L12current__lenS115;
  while (1) {
    int32_t _M0L6_2atmpS1875 = _M0Lm13enough__spaceS118;
    if (_M0L6_2atmpS1875 < _M0L8requiredS117) {
      int32_t _M0L6_2atmpS1876 = _M0Lm13enough__spaceS118;
      _M0Lm13enough__spaceS118 = _M0L6_2atmpS1876 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1879 = _M0Lm13enough__spaceS118;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1880 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS120
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1879, _M0L6_2atmpS1880);
  _M0L8_2afieldS4673 = _M0L4selfS116->$0;
  _M0L4dataS1877 = _M0L8_2afieldS4673;
  _M0L3lenS1878 = _M0L4selfS116->$1;
  moonbit_incref(_M0L4dataS1877);
  moonbit_incref(_M0L9new__dataS120);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS120, 0, _M0L4dataS1877, 0, _M0L3lenS1878);
  _M0L6_2aoldS4672 = _M0L4selfS116->$0;
  moonbit_decref(_M0L6_2aoldS4672);
  _M0L4selfS116->$0 = _M0L9new__dataS120;
  moonbit_decref(_M0L4selfS116);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS110,
  int32_t _M0L6offsetS111,
  int32_t _M0L5valueS109
) {
  uint32_t _M0L4codeS108;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS108 = _M0MPC14char4Char8to__uint(_M0L5valueS109);
  if (_M0L4codeS108 < 65536u) {
    uint32_t _M0L6_2atmpS1858 = _M0L4codeS108 & 255u;
    int32_t _M0L6_2atmpS1857;
    int32_t _M0L6_2atmpS1859;
    uint32_t _M0L6_2atmpS1861;
    int32_t _M0L6_2atmpS1860;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1857 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1858);
    if (
      _M0L6offsetS111 < 0
      || _M0L6offsetS111 >= Moonbit_array_length(_M0L4selfS110)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS110[_M0L6offsetS111] = _M0L6_2atmpS1857;
    _M0L6_2atmpS1859 = _M0L6offsetS111 + 1;
    _M0L6_2atmpS1861 = _M0L4codeS108 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1860 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1861);
    if (
      _M0L6_2atmpS1859 < 0
      || _M0L6_2atmpS1859 >= Moonbit_array_length(_M0L4selfS110)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS110[_M0L6_2atmpS1859] = _M0L6_2atmpS1860;
    moonbit_decref(_M0L4selfS110);
    return 2;
  } else if (_M0L4codeS108 < 1114112u) {
    uint32_t _M0L2hiS112 = _M0L4codeS108 - 65536u;
    uint32_t _M0L6_2atmpS1874 = _M0L2hiS112 >> 10;
    uint32_t _M0L2loS113 = _M0L6_2atmpS1874 | 55296u;
    uint32_t _M0L6_2atmpS1873 = _M0L2hiS112 & 1023u;
    uint32_t _M0L2hiS114 = _M0L6_2atmpS1873 | 56320u;
    uint32_t _M0L6_2atmpS1863 = _M0L2loS113 & 255u;
    int32_t _M0L6_2atmpS1862;
    int32_t _M0L6_2atmpS1864;
    uint32_t _M0L6_2atmpS1866;
    int32_t _M0L6_2atmpS1865;
    int32_t _M0L6_2atmpS1867;
    uint32_t _M0L6_2atmpS1869;
    int32_t _M0L6_2atmpS1868;
    int32_t _M0L6_2atmpS1870;
    uint32_t _M0L6_2atmpS1872;
    int32_t _M0L6_2atmpS1871;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1862 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1863);
    if (
      _M0L6offsetS111 < 0
      || _M0L6offsetS111 >= Moonbit_array_length(_M0L4selfS110)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS110[_M0L6offsetS111] = _M0L6_2atmpS1862;
    _M0L6_2atmpS1864 = _M0L6offsetS111 + 1;
    _M0L6_2atmpS1866 = _M0L2loS113 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1865 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1866);
    if (
      _M0L6_2atmpS1864 < 0
      || _M0L6_2atmpS1864 >= Moonbit_array_length(_M0L4selfS110)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS110[_M0L6_2atmpS1864] = _M0L6_2atmpS1865;
    _M0L6_2atmpS1867 = _M0L6offsetS111 + 2;
    _M0L6_2atmpS1869 = _M0L2hiS114 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1868 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1869);
    if (
      _M0L6_2atmpS1867 < 0
      || _M0L6_2atmpS1867 >= Moonbit_array_length(_M0L4selfS110)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS110[_M0L6_2atmpS1867] = _M0L6_2atmpS1868;
    _M0L6_2atmpS1870 = _M0L6offsetS111 + 3;
    _M0L6_2atmpS1872 = _M0L2hiS114 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1871 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1872);
    if (
      _M0L6_2atmpS1870 < 0
      || _M0L6_2atmpS1870 >= Moonbit_array_length(_M0L4selfS110)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS110[_M0L6_2atmpS1870] = _M0L6_2atmpS1871;
    moonbit_decref(_M0L4selfS110);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS110);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_118.data, (moonbit_string_t)moonbit_string_literal_119.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS107) {
  int32_t _M0L6_2atmpS1856;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1856 = *(int32_t*)&_M0L4selfS107;
  return _M0L6_2atmpS1856 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS106) {
  int32_t _M0L6_2atmpS1855;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1855 = _M0L4selfS106;
  return *(uint32_t*)&_M0L6_2atmpS1855;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS105
) {
  moonbit_bytes_t _M0L8_2afieldS4677;
  moonbit_bytes_t _M0L4dataS1854;
  moonbit_bytes_t _M0L6_2atmpS1851;
  int32_t _M0L8_2afieldS4676;
  int32_t _M0L3lenS1853;
  int64_t _M0L6_2atmpS1852;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4677 = _M0L4selfS105->$0;
  _M0L4dataS1854 = _M0L8_2afieldS4677;
  moonbit_incref(_M0L4dataS1854);
  _M0L6_2atmpS1851 = _M0L4dataS1854;
  _M0L8_2afieldS4676 = _M0L4selfS105->$1;
  moonbit_decref(_M0L4selfS105);
  _M0L3lenS1853 = _M0L8_2afieldS4676;
  _M0L6_2atmpS1852 = (int64_t)_M0L3lenS1853;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1851, 0, _M0L6_2atmpS1852);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS100,
  int32_t _M0L6offsetS104,
  int64_t _M0L6lengthS102
) {
  int32_t _M0L3lenS99;
  int32_t _M0L6lengthS101;
  int32_t _if__result_5173;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS99 = Moonbit_array_length(_M0L4selfS100);
  if (_M0L6lengthS102 == 4294967296ll) {
    _M0L6lengthS101 = _M0L3lenS99 - _M0L6offsetS104;
  } else {
    int64_t _M0L7_2aSomeS103 = _M0L6lengthS102;
    _M0L6lengthS101 = (int32_t)_M0L7_2aSomeS103;
  }
  if (_M0L6offsetS104 >= 0) {
    if (_M0L6lengthS101 >= 0) {
      int32_t _M0L6_2atmpS1850 = _M0L6offsetS104 + _M0L6lengthS101;
      _if__result_5173 = _M0L6_2atmpS1850 <= _M0L3lenS99;
    } else {
      _if__result_5173 = 0;
    }
  } else {
    _if__result_5173 = 0;
  }
  if (_if__result_5173) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS100, _M0L6offsetS104, _M0L6lengthS101);
  } else {
    moonbit_decref(_M0L4selfS100);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS97
) {
  int32_t _M0L7initialS96;
  moonbit_bytes_t _M0L4dataS98;
  struct _M0TPB13StringBuilder* _block_5174;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS97 < 1) {
    _M0L7initialS96 = 1;
  } else {
    _M0L7initialS96 = _M0L10size__hintS97;
  }
  _M0L4dataS98 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS96, 0);
  _block_5174
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_5174)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_5174->$0 = _M0L4dataS98;
  _block_5174->$1 = 0;
  return _block_5174;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS95) {
  int32_t _M0L6_2atmpS1849;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1849 = (int32_t)_M0L4selfS95;
  return _M0L6_2atmpS1849;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS75,
  int32_t _M0L11dst__offsetS76,
  moonbit_string_t* _M0L3srcS77,
  int32_t _M0L11src__offsetS78,
  int32_t _M0L3lenS79
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS75, _M0L11dst__offsetS76, _M0L3srcS77, _M0L11src__offsetS78, _M0L3lenS79);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS80,
  int32_t _M0L11dst__offsetS81,
  struct _M0TUsiE** _M0L3srcS82,
  int32_t _M0L11src__offsetS83,
  int32_t _M0L3lenS84
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS80, _M0L11dst__offsetS81, _M0L3srcS82, _M0L11src__offsetS83, _M0L3lenS84);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS85,
  int32_t _M0L11dst__offsetS86,
  void** _M0L3srcS87,
  int32_t _M0L11src__offsetS88,
  int32_t _M0L3lenS89
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS85, _M0L11dst__offsetS86, _M0L3srcS87, _M0L11src__offsetS88, _M0L3lenS89);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC16string10StringViewE(
  struct _M0TPC16string10StringView* _M0L3dstS90,
  int32_t _M0L11dst__offsetS91,
  struct _M0TPC16string10StringView* _M0L3srcS92,
  int32_t _M0L11src__offsetS93,
  int32_t _M0L3lenS94
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(_M0L3dstS90, _M0L11dst__offsetS91, _M0L3srcS92, _M0L11src__offsetS93, _M0L3lenS94);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS30,
  int32_t _M0L11dst__offsetS32,
  moonbit_bytes_t _M0L3srcS31,
  int32_t _M0L11src__offsetS33,
  int32_t _M0L3lenS35
) {
  int32_t _if__result_5175;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS30 == _M0L3srcS31) {
    _if__result_5175 = _M0L11dst__offsetS32 < _M0L11src__offsetS33;
  } else {
    _if__result_5175 = 0;
  }
  if (_if__result_5175) {
    int32_t _M0L1iS34 = 0;
    while (1) {
      if (_M0L1iS34 < _M0L3lenS35) {
        int32_t _M0L6_2atmpS1804 = _M0L11dst__offsetS32 + _M0L1iS34;
        int32_t _M0L6_2atmpS1806 = _M0L11src__offsetS33 + _M0L1iS34;
        int32_t _M0L6_2atmpS1805;
        int32_t _M0L6_2atmpS1807;
        if (
          _M0L6_2atmpS1806 < 0
          || _M0L6_2atmpS1806 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1805 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1806];
        if (
          _M0L6_2atmpS1804 < 0
          || _M0L6_2atmpS1804 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1804] = _M0L6_2atmpS1805;
        _M0L6_2atmpS1807 = _M0L1iS34 + 1;
        _M0L1iS34 = _M0L6_2atmpS1807;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1812 = _M0L3lenS35 - 1;
    int32_t _M0L1iS37 = _M0L6_2atmpS1812;
    while (1) {
      if (_M0L1iS37 >= 0) {
        int32_t _M0L6_2atmpS1808 = _M0L11dst__offsetS32 + _M0L1iS37;
        int32_t _M0L6_2atmpS1810 = _M0L11src__offsetS33 + _M0L1iS37;
        int32_t _M0L6_2atmpS1809;
        int32_t _M0L6_2atmpS1811;
        if (
          _M0L6_2atmpS1810 < 0
          || _M0L6_2atmpS1810 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1809 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1810];
        if (
          _M0L6_2atmpS1808 < 0
          || _M0L6_2atmpS1808 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1808] = _M0L6_2atmpS1809;
        _M0L6_2atmpS1811 = _M0L1iS37 - 1;
        _M0L1iS37 = _M0L6_2atmpS1811;
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
  int32_t _if__result_5178;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS39 == _M0L3srcS40) {
    _if__result_5178 = _M0L11dst__offsetS41 < _M0L11src__offsetS42;
  } else {
    _if__result_5178 = 0;
  }
  if (_if__result_5178) {
    int32_t _M0L1iS43 = 0;
    while (1) {
      if (_M0L1iS43 < _M0L3lenS44) {
        int32_t _M0L6_2atmpS1813 = _M0L11dst__offsetS41 + _M0L1iS43;
        int32_t _M0L6_2atmpS1815 = _M0L11src__offsetS42 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS4679;
        moonbit_string_t _M0L6_2atmpS1814;
        moonbit_string_t _M0L6_2aoldS4678;
        int32_t _M0L6_2atmpS1816;
        if (
          _M0L6_2atmpS1815 < 0
          || _M0L6_2atmpS1815 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4679 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1815];
        _M0L6_2atmpS1814 = _M0L6_2atmpS4679;
        if (
          _M0L6_2atmpS1813 < 0
          || _M0L6_2atmpS1813 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4678 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1813];
        moonbit_incref(_M0L6_2atmpS1814);
        moonbit_decref(_M0L6_2aoldS4678);
        _M0L3dstS39[_M0L6_2atmpS1813] = _M0L6_2atmpS1814;
        _M0L6_2atmpS1816 = _M0L1iS43 + 1;
        _M0L1iS43 = _M0L6_2atmpS1816;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1821 = _M0L3lenS44 - 1;
    int32_t _M0L1iS46 = _M0L6_2atmpS1821;
    while (1) {
      if (_M0L1iS46 >= 0) {
        int32_t _M0L6_2atmpS1817 = _M0L11dst__offsetS41 + _M0L1iS46;
        int32_t _M0L6_2atmpS1819 = _M0L11src__offsetS42 + _M0L1iS46;
        moonbit_string_t _M0L6_2atmpS4681;
        moonbit_string_t _M0L6_2atmpS1818;
        moonbit_string_t _M0L6_2aoldS4680;
        int32_t _M0L6_2atmpS1820;
        if (
          _M0L6_2atmpS1819 < 0
          || _M0L6_2atmpS1819 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4681 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1819];
        _M0L6_2atmpS1818 = _M0L6_2atmpS4681;
        if (
          _M0L6_2atmpS1817 < 0
          || _M0L6_2atmpS1817 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4680 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1817];
        moonbit_incref(_M0L6_2atmpS1818);
        moonbit_decref(_M0L6_2aoldS4680);
        _M0L3dstS39[_M0L6_2atmpS1817] = _M0L6_2atmpS1818;
        _M0L6_2atmpS1820 = _M0L1iS46 - 1;
        _M0L1iS46 = _M0L6_2atmpS1820;
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
  int32_t _if__result_5181;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS48 == _M0L3srcS49) {
    _if__result_5181 = _M0L11dst__offsetS50 < _M0L11src__offsetS51;
  } else {
    _if__result_5181 = 0;
  }
  if (_if__result_5181) {
    int32_t _M0L1iS52 = 0;
    while (1) {
      if (_M0L1iS52 < _M0L3lenS53) {
        int32_t _M0L6_2atmpS1822 = _M0L11dst__offsetS50 + _M0L1iS52;
        int32_t _M0L6_2atmpS1824 = _M0L11src__offsetS51 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS4683;
        struct _M0TUsiE* _M0L6_2atmpS1823;
        struct _M0TUsiE* _M0L6_2aoldS4682;
        int32_t _M0L6_2atmpS1825;
        if (
          _M0L6_2atmpS1824 < 0
          || _M0L6_2atmpS1824 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4683 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1824];
        _M0L6_2atmpS1823 = _M0L6_2atmpS4683;
        if (
          _M0L6_2atmpS1822 < 0
          || _M0L6_2atmpS1822 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4682 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1822];
        if (_M0L6_2atmpS1823) {
          moonbit_incref(_M0L6_2atmpS1823);
        }
        if (_M0L6_2aoldS4682) {
          moonbit_decref(_M0L6_2aoldS4682);
        }
        _M0L3dstS48[_M0L6_2atmpS1822] = _M0L6_2atmpS1823;
        _M0L6_2atmpS1825 = _M0L1iS52 + 1;
        _M0L1iS52 = _M0L6_2atmpS1825;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1830 = _M0L3lenS53 - 1;
    int32_t _M0L1iS55 = _M0L6_2atmpS1830;
    while (1) {
      if (_M0L1iS55 >= 0) {
        int32_t _M0L6_2atmpS1826 = _M0L11dst__offsetS50 + _M0L1iS55;
        int32_t _M0L6_2atmpS1828 = _M0L11src__offsetS51 + _M0L1iS55;
        struct _M0TUsiE* _M0L6_2atmpS4685;
        struct _M0TUsiE* _M0L6_2atmpS1827;
        struct _M0TUsiE* _M0L6_2aoldS4684;
        int32_t _M0L6_2atmpS1829;
        if (
          _M0L6_2atmpS1828 < 0
          || _M0L6_2atmpS1828 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4685 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1828];
        _M0L6_2atmpS1827 = _M0L6_2atmpS4685;
        if (
          _M0L6_2atmpS1826 < 0
          || _M0L6_2atmpS1826 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4684 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1826];
        if (_M0L6_2atmpS1827) {
          moonbit_incref(_M0L6_2atmpS1827);
        }
        if (_M0L6_2aoldS4684) {
          moonbit_decref(_M0L6_2aoldS4684);
        }
        _M0L3dstS48[_M0L6_2atmpS1826] = _M0L6_2atmpS1827;
        _M0L6_2atmpS1829 = _M0L1iS55 - 1;
        _M0L1iS55 = _M0L6_2atmpS1829;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS57,
  int32_t _M0L11dst__offsetS59,
  void** _M0L3srcS58,
  int32_t _M0L11src__offsetS60,
  int32_t _M0L3lenS62
) {
  int32_t _if__result_5184;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS57 == _M0L3srcS58) {
    _if__result_5184 = _M0L11dst__offsetS59 < _M0L11src__offsetS60;
  } else {
    _if__result_5184 = 0;
  }
  if (_if__result_5184) {
    int32_t _M0L1iS61 = 0;
    while (1) {
      if (_M0L1iS61 < _M0L3lenS62) {
        int32_t _M0L6_2atmpS1831 = _M0L11dst__offsetS59 + _M0L1iS61;
        int32_t _M0L6_2atmpS1833 = _M0L11src__offsetS60 + _M0L1iS61;
        void* _M0L6_2atmpS4687;
        void* _M0L6_2atmpS1832;
        void* _M0L6_2aoldS4686;
        int32_t _M0L6_2atmpS1834;
        if (
          _M0L6_2atmpS1833 < 0
          || _M0L6_2atmpS1833 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4687 = (void*)_M0L3srcS58[_M0L6_2atmpS1833];
        _M0L6_2atmpS1832 = _M0L6_2atmpS4687;
        if (
          _M0L6_2atmpS1831 < 0
          || _M0L6_2atmpS1831 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4686 = (void*)_M0L3dstS57[_M0L6_2atmpS1831];
        moonbit_incref(_M0L6_2atmpS1832);
        moonbit_decref(_M0L6_2aoldS4686);
        _M0L3dstS57[_M0L6_2atmpS1831] = _M0L6_2atmpS1832;
        _M0L6_2atmpS1834 = _M0L1iS61 + 1;
        _M0L1iS61 = _M0L6_2atmpS1834;
        continue;
      } else {
        moonbit_decref(_M0L3srcS58);
        moonbit_decref(_M0L3dstS57);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1839 = _M0L3lenS62 - 1;
    int32_t _M0L1iS64 = _M0L6_2atmpS1839;
    while (1) {
      if (_M0L1iS64 >= 0) {
        int32_t _M0L6_2atmpS1835 = _M0L11dst__offsetS59 + _M0L1iS64;
        int32_t _M0L6_2atmpS1837 = _M0L11src__offsetS60 + _M0L1iS64;
        void* _M0L6_2atmpS4689;
        void* _M0L6_2atmpS1836;
        void* _M0L6_2aoldS4688;
        int32_t _M0L6_2atmpS1838;
        if (
          _M0L6_2atmpS1837 < 0
          || _M0L6_2atmpS1837 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4689 = (void*)_M0L3srcS58[_M0L6_2atmpS1837];
        _M0L6_2atmpS1836 = _M0L6_2atmpS4689;
        if (
          _M0L6_2atmpS1835 < 0
          || _M0L6_2atmpS1835 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4688 = (void*)_M0L3dstS57[_M0L6_2atmpS1835];
        moonbit_incref(_M0L6_2atmpS1836);
        moonbit_decref(_M0L6_2aoldS4688);
        _M0L3dstS57[_M0L6_2atmpS1835] = _M0L6_2atmpS1836;
        _M0L6_2atmpS1838 = _M0L1iS64 - 1;
        _M0L1iS64 = _M0L6_2atmpS1838;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC16string10StringViewEE(
  struct _M0TPC16string10StringView* _M0L3dstS66,
  int32_t _M0L11dst__offsetS68,
  struct _M0TPC16string10StringView* _M0L3srcS67,
  int32_t _M0L11src__offsetS69,
  int32_t _M0L3lenS71
) {
  int32_t _if__result_5187;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS66 == _M0L3srcS67) {
    _if__result_5187 = _M0L11dst__offsetS68 < _M0L11src__offsetS69;
  } else {
    _if__result_5187 = 0;
  }
  if (_if__result_5187) {
    int32_t _M0L1iS70 = 0;
    while (1) {
      if (_M0L1iS70 < _M0L3lenS71) {
        int32_t _M0L6_2atmpS1840 = _M0L11dst__offsetS68 + _M0L1iS70;
        int32_t _M0L6_2atmpS1842 = _M0L11src__offsetS69 + _M0L1iS70;
        struct _M0TPC16string10StringView _M0L6_2atmpS4691;
        struct _M0TPC16string10StringView _M0L6_2atmpS1841;
        struct _M0TPC16string10StringView _M0L6_2aoldS4690;
        int32_t _M0L6_2atmpS1843;
        if (
          _M0L6_2atmpS1842 < 0
          || _M0L6_2atmpS1842 >= Moonbit_array_length(_M0L3srcS67)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4691 = _M0L3srcS67[_M0L6_2atmpS1842];
        _M0L6_2atmpS1841 = _M0L6_2atmpS4691;
        if (
          _M0L6_2atmpS1840 < 0
          || _M0L6_2atmpS1840 >= Moonbit_array_length(_M0L3dstS66)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4690 = _M0L3dstS66[_M0L6_2atmpS1840];
        moonbit_incref(_M0L6_2atmpS1841.$0);
        moonbit_decref(_M0L6_2aoldS4690.$0);
        _M0L3dstS66[_M0L6_2atmpS1840] = _M0L6_2atmpS1841;
        _M0L6_2atmpS1843 = _M0L1iS70 + 1;
        _M0L1iS70 = _M0L6_2atmpS1843;
        continue;
      } else {
        moonbit_decref(_M0L3srcS67);
        moonbit_decref(_M0L3dstS66);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1848 = _M0L3lenS71 - 1;
    int32_t _M0L1iS73 = _M0L6_2atmpS1848;
    while (1) {
      if (_M0L1iS73 >= 0) {
        int32_t _M0L6_2atmpS1844 = _M0L11dst__offsetS68 + _M0L1iS73;
        int32_t _M0L6_2atmpS1846 = _M0L11src__offsetS69 + _M0L1iS73;
        struct _M0TPC16string10StringView _M0L6_2atmpS4693;
        struct _M0TPC16string10StringView _M0L6_2atmpS1845;
        struct _M0TPC16string10StringView _M0L6_2aoldS4692;
        int32_t _M0L6_2atmpS1847;
        if (
          _M0L6_2atmpS1846 < 0
          || _M0L6_2atmpS1846 >= Moonbit_array_length(_M0L3srcS67)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4693 = _M0L3srcS67[_M0L6_2atmpS1846];
        _M0L6_2atmpS1845 = _M0L6_2atmpS4693;
        if (
          _M0L6_2atmpS1844 < 0
          || _M0L6_2atmpS1844 >= Moonbit_array_length(_M0L3dstS66)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4692 = _M0L3dstS66[_M0L6_2atmpS1844];
        moonbit_incref(_M0L6_2atmpS1845.$0);
        moonbit_decref(_M0L6_2aoldS4692.$0);
        _M0L3dstS66[_M0L6_2atmpS1844] = _M0L6_2atmpS1845;
        _M0L6_2atmpS1847 = _M0L1iS73 - 1;
        _M0L1iS73 = _M0L6_2atmpS1847;
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

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1788;
  moonbit_string_t _M0L6_2atmpS4696;
  moonbit_string_t _M0L6_2atmpS1786;
  moonbit_string_t _M0L6_2atmpS1787;
  moonbit_string_t _M0L6_2atmpS4695;
  moonbit_string_t _M0L6_2atmpS1785;
  moonbit_string_t _M0L6_2atmpS4694;
  moonbit_string_t _M0L6_2atmpS1784;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1788 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4696
  = moonbit_add_string(_M0L6_2atmpS1788, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1788);
  _M0L6_2atmpS1786 = _M0L6_2atmpS4696;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1787
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4695 = moonbit_add_string(_M0L6_2atmpS1786, _M0L6_2atmpS1787);
  moonbit_decref(_M0L6_2atmpS1786);
  moonbit_decref(_M0L6_2atmpS1787);
  _M0L6_2atmpS1785 = _M0L6_2atmpS4695;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4694
  = moonbit_add_string(_M0L6_2atmpS1785, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1785);
  _M0L6_2atmpS1784 = _M0L6_2atmpS4694;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1784);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1793;
  moonbit_string_t _M0L6_2atmpS4699;
  moonbit_string_t _M0L6_2atmpS1791;
  moonbit_string_t _M0L6_2atmpS1792;
  moonbit_string_t _M0L6_2atmpS4698;
  moonbit_string_t _M0L6_2atmpS1790;
  moonbit_string_t _M0L6_2atmpS4697;
  moonbit_string_t _M0L6_2atmpS1789;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1793 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4699
  = moonbit_add_string(_M0L6_2atmpS1793, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1793);
  _M0L6_2atmpS1791 = _M0L6_2atmpS4699;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1792
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4698 = moonbit_add_string(_M0L6_2atmpS1791, _M0L6_2atmpS1792);
  moonbit_decref(_M0L6_2atmpS1791);
  moonbit_decref(_M0L6_2atmpS1792);
  _M0L6_2atmpS1790 = _M0L6_2atmpS4698;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4697
  = moonbit_add_string(_M0L6_2atmpS1790, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1790);
  _M0L6_2atmpS1789 = _M0L6_2atmpS4697;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1789);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS26,
  moonbit_string_t _M0L3locS27
) {
  moonbit_string_t _M0L6_2atmpS1798;
  moonbit_string_t _M0L6_2atmpS4702;
  moonbit_string_t _M0L6_2atmpS1796;
  moonbit_string_t _M0L6_2atmpS1797;
  moonbit_string_t _M0L6_2atmpS4701;
  moonbit_string_t _M0L6_2atmpS1795;
  moonbit_string_t _M0L6_2atmpS4700;
  moonbit_string_t _M0L6_2atmpS1794;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1798 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4702
  = moonbit_add_string(_M0L6_2atmpS1798, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1798);
  _M0L6_2atmpS1796 = _M0L6_2atmpS4702;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1797
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS27);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4701 = moonbit_add_string(_M0L6_2atmpS1796, _M0L6_2atmpS1797);
  moonbit_decref(_M0L6_2atmpS1796);
  moonbit_decref(_M0L6_2atmpS1797);
  _M0L6_2atmpS1795 = _M0L6_2atmpS4701;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4700
  = moonbit_add_string(_M0L6_2atmpS1795, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1795);
  _M0L6_2atmpS1794 = _M0L6_2atmpS4700;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1794);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS28,
  moonbit_string_t _M0L3locS29
) {
  moonbit_string_t _M0L6_2atmpS1803;
  moonbit_string_t _M0L6_2atmpS4705;
  moonbit_string_t _M0L6_2atmpS1801;
  moonbit_string_t _M0L6_2atmpS1802;
  moonbit_string_t _M0L6_2atmpS4704;
  moonbit_string_t _M0L6_2atmpS1800;
  moonbit_string_t _M0L6_2atmpS4703;
  moonbit_string_t _M0L6_2atmpS1799;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1803 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS28);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4705
  = moonbit_add_string(_M0L6_2atmpS1803, (moonbit_string_t)moonbit_string_literal_120.data);
  moonbit_decref(_M0L6_2atmpS1803);
  _M0L6_2atmpS1801 = _M0L6_2atmpS4705;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1802
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS29);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4704 = moonbit_add_string(_M0L6_2atmpS1801, _M0L6_2atmpS1802);
  moonbit_decref(_M0L6_2atmpS1801);
  moonbit_decref(_M0L6_2atmpS1802);
  _M0L6_2atmpS1800 = _M0L6_2atmpS4704;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4703
  = moonbit_add_string(_M0L6_2atmpS1800, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1800);
  _M0L6_2atmpS1799 = _M0L6_2atmpS4703;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1799);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS20,
  uint32_t _M0L5valueS21
) {
  uint32_t _M0L3accS1783;
  uint32_t _M0L6_2atmpS1782;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1783 = _M0L4selfS20->$0;
  _M0L6_2atmpS1782 = _M0L3accS1783 + 4u;
  _M0L4selfS20->$0 = _M0L6_2atmpS1782;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS20, _M0L5valueS21);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS18,
  uint32_t _M0L5inputS19
) {
  uint32_t _M0L3accS1780;
  uint32_t _M0L6_2atmpS1781;
  uint32_t _M0L6_2atmpS1779;
  uint32_t _M0L6_2atmpS1778;
  uint32_t _M0L6_2atmpS1777;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1780 = _M0L4selfS18->$0;
  _M0L6_2atmpS1781 = _M0L5inputS19 * 3266489917u;
  _M0L6_2atmpS1779 = _M0L3accS1780 + _M0L6_2atmpS1781;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1778 = _M0FPB4rotl(_M0L6_2atmpS1779, 17);
  _M0L6_2atmpS1777 = _M0L6_2atmpS1778 * 668265263u;
  _M0L4selfS18->$0 = _M0L6_2atmpS1777;
  moonbit_decref(_M0L4selfS18);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS16, int32_t _M0L1rS17) {
  uint32_t _M0L6_2atmpS1774;
  int32_t _M0L6_2atmpS1776;
  uint32_t _M0L6_2atmpS1775;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1774 = _M0L1xS16 << (_M0L1rS17 & 31);
  _M0L6_2atmpS1776 = 32 - _M0L1rS17;
  _M0L6_2atmpS1775 = _M0L1xS16 >> (_M0L6_2atmpS1776 & 31);
  return _M0L6_2atmpS1774 | _M0L6_2atmpS1775;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S12,
  struct _M0TPB6Logger _M0L10_2ax__4934S15
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS13;
  moonbit_string_t _M0L8_2afieldS4706;
  int32_t _M0L6_2acntS4855;
  moonbit_string_t _M0L15_2a_2aarg__4935S14;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS13
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S12;
  _M0L8_2afieldS4706 = _M0L10_2aFailureS13->$0;
  _M0L6_2acntS4855 = Moonbit_object_header(_M0L10_2aFailureS13)->rc;
  if (_M0L6_2acntS4855 > 1) {
    int32_t _M0L11_2anew__cntS4856 = _M0L6_2acntS4855 - 1;
    Moonbit_object_header(_M0L10_2aFailureS13)->rc = _M0L11_2anew__cntS4856;
    moonbit_incref(_M0L8_2afieldS4706);
  } else if (_M0L6_2acntS4855 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS13);
  }
  _M0L15_2a_2aarg__4935S14 = _M0L8_2afieldS4706;
  if (_M0L10_2ax__4934S15.$1) {
    moonbit_incref(_M0L10_2ax__4934S15.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S15.$0->$method_0(_M0L10_2ax__4934S15.$1, (moonbit_string_t)moonbit_string_literal_121.data);
  if (_M0L10_2ax__4934S15.$1) {
    moonbit_incref(_M0L10_2ax__4934S15.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S15, _M0L15_2a_2aarg__4935S14);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S15.$0->$method_0(_M0L10_2ax__4934S15.$1, (moonbit_string_t)moonbit_string_literal_89.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS11) {
  void* _block_5190;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_5190 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_5190)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_5190)->$0 = _M0L4selfS11;
  return _block_5190;
}

int32_t _M0MPB6Logger13write__objectGRPC16string10StringViewE(
  struct _M0TPB6Logger _M0L4selfS6,
  struct _M0TPC16string10StringView _M0L3objS5
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS5, _M0L4selfS6);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGiE(
  struct _M0TPB6Logger _M0L4selfS8,
  int32_t _M0L3objS7
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L3objS7, _M0L4selfS8);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS10,
  moonbit_string_t _M0L3objS9
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS9, _M0L4selfS10);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1684) {
  switch (Moonbit_object_tag(_M0L4_2aeS1684)) {
    case 4: {
      return (moonbit_string_t)moonbit_string_literal_122.data;
      break;
    }
    
    case 2: {
      return (moonbit_string_t)moonbit_string_literal_123.data;
      break;
    }
    
    case 10: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_124.data;
      break;
    }
    
    case 5: {
      return (moonbit_string_t)moonbit_string_literal_125.data;
      break;
    }
    
    case 8: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_126.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1684);
      break;
    }
    
    case 9: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_127.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_128.data;
      break;
    }
    
    case 7: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_129.data;
      break;
    }
    
    case 11: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_130.data;
      break;
    }
    
    case 6: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_131.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1684);
      return (moonbit_string_t)moonbit_string_literal_132.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1752
) {
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS1753 =
    (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)_M0L11_2aobj__ptrS1752;
  struct _M0TPC16string10StringView _M0L8_2afieldS4707 =
    (struct _M0TPC16string10StringView){_M0L14_2aboxed__selfS1753->$0_1,
                                          _M0L14_2aboxed__selfS1753->$0_2,
                                          _M0L14_2aboxed__selfS1753->$0_0};
  int32_t _M0L6_2acntS4857 =
    Moonbit_object_header(_M0L14_2aboxed__selfS1753)->rc;
  struct _M0TPC16string10StringView _M0L7_2aselfS1751;
  if (_M0L6_2acntS4857 > 1) {
    int32_t _M0L11_2anew__cntS4858 = _M0L6_2acntS4857 - 1;
    Moonbit_object_header(_M0L14_2aboxed__selfS1753)->rc
    = _M0L11_2anew__cntS4858;
    moonbit_incref(_M0L8_2afieldS4707.$0);
  } else if (_M0L6_2acntS4857 == 1) {
    moonbit_free(_M0L14_2aboxed__selfS1753);
  }
  _M0L7_2aselfS1751 = _M0L8_2afieldS4707;
  return _M0IPC16string10StringViewPB4Show10to__string(_M0L7_2aselfS1751);
}

int32_t _M0IPC16string10StringViewPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1749,
  struct _M0TPB6Logger _M0L8_2aparamS1748
) {
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS1750 =
    (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)_M0L11_2aobj__ptrS1749;
  struct _M0TPC16string10StringView _M0L8_2afieldS4708 =
    (struct _M0TPC16string10StringView){_M0L14_2aboxed__selfS1750->$0_1,
                                          _M0L14_2aboxed__selfS1750->$0_2,
                                          _M0L14_2aboxed__selfS1750->$0_0};
  int32_t _M0L6_2acntS4859 =
    Moonbit_object_header(_M0L14_2aboxed__selfS1750)->rc;
  struct _M0TPC16string10StringView _M0L7_2aselfS1747;
  if (_M0L6_2acntS4859 > 1) {
    int32_t _M0L11_2anew__cntS4860 = _M0L6_2acntS4859 - 1;
    Moonbit_object_header(_M0L14_2aboxed__selfS1750)->rc
    = _M0L11_2anew__cntS4860;
    moonbit_incref(_M0L8_2afieldS4708.$0);
  } else if (_M0L6_2acntS4859 == 1) {
    moonbit_free(_M0L14_2aboxed__selfS1750);
  }
  _M0L7_2aselfS1747 = _M0L8_2afieldS4708;
  _M0IPC16string10StringViewPB4Show6output(_M0L7_2aselfS1747, _M0L8_2aparamS1748);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1746,
  int32_t _M0L8_2aparamS1745
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1744 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1746;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1744, _M0L8_2aparamS1745);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1743,
  struct _M0TPC16string10StringView _M0L8_2aparamS1742
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1741 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1743;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1741, _M0L8_2aparamS1742);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1740,
  moonbit_string_t _M0L8_2aparamS1737,
  int32_t _M0L8_2aparamS1738,
  int32_t _M0L8_2aparamS1739
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1736 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1740;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1736, _M0L8_2aparamS1737, _M0L8_2aparamS1738, _M0L8_2aparamS1739);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1735,
  moonbit_string_t _M0L8_2aparamS1734
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1733 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1735;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1733, _M0L8_2aparamS1734);
  return 0;
}

void* _M0IPC16string10StringViewPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1731
) {
  struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView* _M0L14_2aboxed__selfS1732 =
    (struct _M0Y40moonbitlang_2fcore_2fstring_2fStringView*)_M0L11_2aobj__ptrS1731;
  struct _M0TPC16string10StringView _M0L8_2afieldS4709 =
    (struct _M0TPC16string10StringView){_M0L14_2aboxed__selfS1732->$0_1,
                                          _M0L14_2aboxed__selfS1732->$0_2,
                                          _M0L14_2aboxed__selfS1732->$0_0};
  int32_t _M0L6_2acntS4861 =
    Moonbit_object_header(_M0L14_2aboxed__selfS1732)->rc;
  struct _M0TPC16string10StringView _M0L7_2aselfS1730;
  if (_M0L6_2acntS4861 > 1) {
    int32_t _M0L11_2anew__cntS4862 = _M0L6_2acntS4861 - 1;
    Moonbit_object_header(_M0L14_2aboxed__selfS1732)->rc
    = _M0L11_2anew__cntS4862;
    moonbit_incref(_M0L8_2afieldS4709.$0);
  } else if (_M0L6_2acntS4861 == 1) {
    moonbit_free(_M0L14_2aboxed__selfS1732);
  }
  _M0L7_2aselfS1730 = _M0L8_2afieldS4709;
  return _M0IPC16string10StringViewPB6ToJson8to__json(_M0L7_2aselfS1730);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1773 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1772;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1765;
  moonbit_string_t* _M0L6_2atmpS1771;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1770;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1766;
  moonbit_string_t* _M0L6_2atmpS1769;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1768;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1767;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1611;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1764;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1763;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1762;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1761;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1610;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1760;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1759;
  _M0L6_2atmpS1773[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__0_2eclo);
  _M0L8_2atupleS1772
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1772)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1772->$0
  = _M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__0_2eclo;
  _M0L8_2atupleS1772->$1 = _M0L6_2atmpS1773;
  _M0L8_2atupleS1765
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1765)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1765->$0 = 0;
  _M0L8_2atupleS1765->$1 = _M0L8_2atupleS1772;
  _M0L6_2atmpS1771 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1771[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__1_2eclo);
  _M0L8_2atupleS1770
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1770)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1770->$0
  = _M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__1_2eclo;
  _M0L8_2atupleS1770->$1 = _M0L6_2atmpS1771;
  _M0L8_2atupleS1766
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1766)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1766->$0 = 1;
  _M0L8_2atupleS1766->$1 = _M0L8_2atupleS1770;
  _M0L6_2atmpS1769 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1769[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__2_2eclo);
  _M0L8_2atupleS1768
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1768)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1768->$0
  = _M0FP48clawteam8clawteam8internal3uri33____test__7572692e6d6274__2_2eclo;
  _M0L8_2atupleS1768->$1 = _M0L6_2atmpS1769;
  _M0L8_2atupleS1767
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1767)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1767->$0 = 2;
  _M0L8_2atupleS1767->$1 = _M0L8_2atupleS1768;
  _M0L7_2abindS1611
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1611[0] = _M0L8_2atupleS1765;
  _M0L7_2abindS1611[1] = _M0L8_2atupleS1766;
  _M0L7_2abindS1611[2] = _M0L8_2atupleS1767;
  _M0L6_2atmpS1764 = _M0L7_2abindS1611;
  _M0L6_2atmpS1763
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 3, _M0L6_2atmpS1764
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1762
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1763);
  _M0L8_2atupleS1761
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1761)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1761->$0 = (moonbit_string_t)moonbit_string_literal_133.data;
  _M0L8_2atupleS1761->$1 = _M0L6_2atmpS1762;
  _M0L7_2abindS1610
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1610[0] = _M0L8_2atupleS1761;
  _M0L6_2atmpS1760 = _M0L7_2abindS1610;
  _M0L6_2atmpS1759
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS1760
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal3uri48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1759);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1758;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1678;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1679;
  int32_t _M0L7_2abindS1680;
  int32_t _M0L2__S1681;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1758
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1678
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1678)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1678->$0 = _M0L6_2atmpS1758;
  _M0L12async__testsS1678->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1679
  = _M0FP48clawteam8clawteam8internal3uri52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1680 = _M0L7_2abindS1679->$1;
  _M0L2__S1681 = 0;
  while (1) {
    if (_M0L2__S1681 < _M0L7_2abindS1680) {
      struct _M0TUsiE** _M0L8_2afieldS4713 = _M0L7_2abindS1679->$0;
      struct _M0TUsiE** _M0L3bufS1757 = _M0L8_2afieldS4713;
      struct _M0TUsiE* _M0L6_2atmpS4712 =
        (struct _M0TUsiE*)_M0L3bufS1757[_M0L2__S1681];
      struct _M0TUsiE* _M0L3argS1682 = _M0L6_2atmpS4712;
      moonbit_string_t _M0L8_2afieldS4711 = _M0L3argS1682->$0;
      moonbit_string_t _M0L6_2atmpS1754 = _M0L8_2afieldS4711;
      int32_t _M0L8_2afieldS4710 = _M0L3argS1682->$1;
      int32_t _M0L6_2atmpS1755 = _M0L8_2afieldS4710;
      int32_t _M0L6_2atmpS1756;
      moonbit_incref(_M0L6_2atmpS1754);
      moonbit_incref(_M0L12async__testsS1678);
      #line 441 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal3uri44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1678, _M0L6_2atmpS1754, _M0L6_2atmpS1755);
      _M0L6_2atmpS1756 = _M0L2__S1681 + 1;
      _M0L2__S1681 = _M0L6_2atmpS1756;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1679);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\internal\\uri\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal3uri28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal3uri34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1678);
  return 0;
}