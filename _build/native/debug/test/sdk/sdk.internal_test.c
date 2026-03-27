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
struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TWzEsQRPC15error5Error;

struct _M0TPC16buffer6Buffer;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGAsRPC15error5ErrorE2Ok;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC16result6ResultGzRPB7NoErrorE2Ok;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam3sdk33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGAsRPC28encoding4utf89MalformedE2Ok;

struct _M0DTPC16result6ResultGzRPB7NoErrorE3Err;

struct _M0DTPC16result6ResultGAsRPC15error5ErrorE3Err;

struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE3Err;

struct _M0DTPC16result6ResultGAsRPC28encoding4utf89MalformedE3Err;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGyRPB7NoErrorE3Err;

struct _M0TPB13SourceLocRepr;

struct _M0DTPC15error5Error95clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TWRPC15error5ErrorEu;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__;

struct _M0TPC15bytes9BytesView;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__;

struct _M0TPB9ArrayViewGyE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE2Ok;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam3sdk33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0DTPC16result6ResultGyRPB7NoErrorE2Ok;

struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__ {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_bytes_t $0_0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0TWzEsQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWzEsQRPC15error5Error*,
    moonbit_bytes_t
  );
  
};

struct _M0TPC16buffer6Buffer {
  int32_t $1;
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

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGAsRPC15error5ErrorE2Ok {
  moonbit_string_t* $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_1(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC16result6ResultGzRPB7NoErrorE2Ok {
  moonbit_bytes_t $0;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam3sdk33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGAsRPC28encoding4utf89MalformedE2Ok {
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGzRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGAsRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGAsRPC28encoding4utf89MalformedE3Err {
  void* $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGyRPB7NoErrorE3Err {
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

struct _M0DTPC15error5Error95clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
};

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
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

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_bytes_t $0_0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB9ArrayViewGyE {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE2Ok {
  moonbit_string_t $0;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam3sdk33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPC13ref3RefGiE {
  int32_t $0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0DTPC16result6ResultGyRPB7NoErrorE2Ok {
  int32_t $0;
  
};

struct moonbit_result_3 {
  int tag;
  union { moonbit_string_t ok; void* err;  } data;
  
};

struct moonbit_result_2 {
  int tag;
  union { moonbit_string_t* ok; void* err;  } data;
  
};

struct moonbit_result_1 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { moonbit_string_t ok; void* err;  } data;
  
};

int32_t _M0IP016_24default__implP38clawteam8clawteam3sdk28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam3sdk34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS725(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS720(
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S707(
  int32_t,
  moonbit_string_t
);

#define _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

int32_t _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN17error__to__stringS685(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN14handle__resultS676(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk41MoonBit__Test__Driver__Internal__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPB9ArrayViewGyE _M0FPC15abort5abortGRPB9ArrayViewGyEE(
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGyE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

struct moonbit_result_2 _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(
  int32_t
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einnerC1853l6(
  struct _M0TWzEsQRPC15error5Error*,
  moonbit_bytes_t
);

#define _M0FP48clawteam8clawteam8internal9backtrace12c__backtrace moonbit_moonclaw_backtrace

int32_t _M0FP48clawteam8clawteam8internal2os6atexit(void(*)());

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal2os4args();

struct moonbit_result_3 _M0FP48clawteam8clawteam8internal2os6getenv(
  struct _M0TPC16string10StringView
);

#define _M0FP48clawteam8clawteam8internal2os10os__getenv moonbit_moonclaw_os_getenv

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

struct moonbit_result_0 _M0FPC28encoding4utf814decode_2einner(
  struct _M0TPC15bytes9BytesView,
  int32_t
);

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FPC13env4args();

struct _M0TPB5ArrayGsE* _M0FPC13env24get__cli__args__internal();

moonbit_string_t _M0FPC13env28utf8__bytes__to__mbt__string(moonbit_bytes_t);

#define _M0FPC13env19get__cli__args__ffi moonbit_get_cli_args

moonbit_bytes_t _M0MPC16buffer6Buffer9to__bytes(
  struct _M0TPC16buffer6Buffer*
);

int32_t _M0MPC16buffer6Buffer19write__string__utf8(
  struct _M0TPC16buffer6Buffer*,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16buffer6Buffer17write__char__utf8(
  struct _M0TPC16buffer6Buffer*,
  int32_t
);

struct _M0TPC16buffer6Buffer* _M0FPC16buffer11new_2einner(int32_t);

int32_t _M0MPC16buffer6Buffer19grow__if__necessary(
  struct _M0TPC16buffer6Buffer*,
  int32_t
);

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr*,
  struct _M0TPB6Logger
);

moonbit_bytes_t _M0MPC15bytes5Bytes11from__array(struct _M0TPB9ArrayViewGyE);

int32_t _M0MPC15bytes5Bytes11from__arrayC1268l455(struct _M0TWuEu*, int32_t);

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15bytes9BytesView6length(struct _M0TPC15bytes9BytesView);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

moonbit_bytes_t _M0MPC15bytes5Bytes5makei(int32_t, struct _M0TWuEu*);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0MPC15array10FixedArray4join(
  moonbit_string_t*,
  struct _M0TPC16string10StringView
);

struct _M0TPB9ArrayViewGyE _M0MPC15array10FixedArray12view_2einnerGyE(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array9ArrayView2atGyE(struct _M0TPB9ArrayViewGyE, int32_t);

struct moonbit_result_2 _M0MPC15array10FixedArray3mapGzsEHRPC28encoding4utf89Malformed(
  moonbit_bytes_t*,
  struct _M0TWzEsQRPC15error5Error*
);

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(struct _M0TPB5ArrayGsE*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1201l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

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

int32_t _M0MPC15array9ArrayView6lengthGsE(struct _M0TPB9ArrayViewGsE);

int32_t _M0MPC15array9ArrayView6lengthGyE(struct _M0TPB9ArrayViewGyE);

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

int32_t _M0MPC16string10StringView4iterC1149l198(struct _M0TWEOc*);

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

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc*);

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

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger,
  struct _M0TWEOs*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs*);

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc*);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE*
);

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

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(moonbit_string_t);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
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

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0FPB5abortGiE(moonbit_string_t, moonbit_string_t);

int32_t _M0FPB5abortGuE(moonbit_string_t, moonbit_string_t);

struct _M0TPB9ArrayViewGyE _M0FPB5abortGRPB9ArrayViewGyEE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0FPB5abortGyE(moonbit_string_t, moonbit_string_t);

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0IPB7FailurePB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

void _M0FP017____moonbit__initC785l332();

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

moonbit_bytes_t* moonbit_moonclaw_backtrace(int32_t);

int32_t moonbit_moonclaw_c_load_byte(void*, int32_t);

void* moonbit_moonclaw_os_getenv(moonbit_bytes_t);

int32_t moonbit_moonclaw_c_is_null(void*);

void atexit(void(*)());

uint64_t moonbit_moonclaw_c_strlen(void*);

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    86, 105, 101, 119, 32, 105, 110, 100, 101, 120, 32, 111, 117, 116, 
    32, 111, 102, 32, 98, 111, 117, 110, 100, 115, 0
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
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 52, 53, 49, 
    58, 53, 45, 52, 53, 49, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 102, 102, 101, 114, 58, 98, 117, 102, 
    102, 101, 114, 46, 109, 98, 116, 58, 56, 49, 49, 58, 49, 48, 45, 
    56, 49, 49, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 101, 110, 99, 111, 100, 105, 110, 103, 47, 117, 116, 
    102, 56, 46, 77, 97, 108, 102, 111, 114, 109, 101, 100, 46, 77, 97, 
    108, 102, 111, 114, 109, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[88]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 87), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 115, 100, 107, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 
    111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 
    114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 
    111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_45 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_31 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    65, 98, 111, 114, 116, 101, 100, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 115, 100, 107, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 
    101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[90]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 89), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 115, 100, 107, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 
    84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    67, 73, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_21 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    32, 101, 120, 105, 116, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_36 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_20 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 51, 53, 
    58, 53, 45, 49, 51, 55, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 56, 
    48, 58, 53, 45, 49, 56, 48, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    105, 110, 100, 101, 120, 32, 111, 117, 116, 32, 111, 102, 32, 98, 
    111, 117, 110, 100, 115, 58, 32, 116, 104, 101, 32, 108, 101, 110, 
    32, 105, 115, 32, 102, 114, 111, 109, 32, 48, 32, 116, 111, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_10 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    32, 98, 117, 116, 32, 116, 104, 101, 32, 105, 110, 100, 101, 120, 
    32, 105, 115, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_35 =
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

struct { int32_t rc; uint32_t meta; uint8_t const data[1]; 
} const moonbit_bytes_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 0), 0};

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN17error__to__stringS685$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN17error__to__stringS685
  };

struct { int32_t rc; uint32_t meta; struct _M0TWzEsQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einnerC1853l6$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einnerC1853l6
  };

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

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4mock8os__args;

int32_t _M0IP016_24default__implP38clawteam8clawteam3sdk28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam3sdk34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S747
) {
  #line 12 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S747);
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S707;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS720;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS725;
  struct _M0TUsiE** _M0L6_2atmpS2051;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS732;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS733;
  moonbit_string_t _M0L6_2atmpS2050;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS734;
  int32_t _M0L7_2abindS735;
  int32_t _M0L2__S736;
  #line 193 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S707 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS720
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS725 = 0;
  _M0L6_2atmpS2051 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS732
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS732)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS732->$0 = _M0L6_2atmpS2051;
  _M0L16file__and__indexS732->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS733
  = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS720(_M0L57moonbit__test__driver__internal__get__cli__args__internalS720);
  #line 284 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2050 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS733, 1);
  #line 283 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS734
  = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS725(_M0L51moonbit__test__driver__internal__split__mbt__stringS725, _M0L6_2atmpS2050, 47);
  _M0L7_2abindS735 = _M0L10test__argsS734->$1;
  _M0L2__S736 = 0;
  while (1) {
    if (_M0L2__S736 < _M0L7_2abindS735) {
      moonbit_string_t* _M0L8_2afieldS2053 = _M0L10test__argsS734->$0;
      moonbit_string_t* _M0L3bufS2049 = _M0L8_2afieldS2053;
      moonbit_string_t _M0L6_2atmpS2052 =
        (moonbit_string_t)_M0L3bufS2049[_M0L2__S736];
      moonbit_string_t _M0L3argS737 = _M0L6_2atmpS2052;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS738;
      moonbit_string_t _M0L4fileS739;
      moonbit_string_t _M0L5rangeS740;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS741;
      moonbit_string_t _M0L6_2atmpS2047;
      int32_t _M0L5startS742;
      moonbit_string_t _M0L6_2atmpS2046;
      int32_t _M0L3endS743;
      int32_t _M0L1iS744;
      int32_t _M0L6_2atmpS2048;
      moonbit_incref(_M0L3argS737);
      #line 288 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS738
      = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS725(_M0L51moonbit__test__driver__internal__split__mbt__stringS725, _M0L3argS737, 58);
      moonbit_incref(_M0L16file__and__rangeS738);
      #line 289 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS739
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS738, 0);
      #line 290 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS740
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS738, 1);
      #line 291 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS741
      = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS725(_M0L51moonbit__test__driver__internal__split__mbt__stringS725, _M0L5rangeS740, 45);
      moonbit_incref(_M0L15start__and__endS741);
      #line 294 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS2047
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS741, 0);
      #line 294 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L5startS742
      = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S707(_M0L45moonbit__test__driver__internal__parse__int__S707, _M0L6_2atmpS2047);
      #line 295 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS2046
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS741, 1);
      #line 295 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L3endS743
      = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S707(_M0L45moonbit__test__driver__internal__parse__int__S707, _M0L6_2atmpS2046);
      _M0L1iS744 = _M0L5startS742;
      while (1) {
        if (_M0L1iS744 < _M0L3endS743) {
          struct _M0TUsiE* _M0L8_2atupleS2044;
          int32_t _M0L6_2atmpS2045;
          moonbit_incref(_M0L4fileS739);
          _M0L8_2atupleS2044
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2044)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2044->$0 = _M0L4fileS739;
          _M0L8_2atupleS2044->$1 = _M0L1iS744;
          moonbit_incref(_M0L16file__and__indexS732);
          #line 297 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS732, _M0L8_2atupleS2044);
          _M0L6_2atmpS2045 = _M0L1iS744 + 1;
          _M0L1iS744 = _M0L6_2atmpS2045;
          continue;
        } else {
          moonbit_decref(_M0L4fileS739);
        }
        break;
      }
      _M0L6_2atmpS2048 = _M0L2__S736 + 1;
      _M0L2__S736 = _M0L6_2atmpS2048;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS734);
    }
    break;
  }
  return _M0L16file__and__indexS732;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS725(
  int32_t _M0L6_2aenvS2025,
  moonbit_string_t _M0L1sS726,
  int32_t _M0L3sepS727
) {
  moonbit_string_t* _M0L6_2atmpS2043;
  struct _M0TPB5ArrayGsE* _M0L3resS728;
  struct _M0TPC13ref3RefGiE* _M0L1iS729;
  struct _M0TPC13ref3RefGiE* _M0L5startS730;
  int32_t _M0L3valS2038;
  int32_t _M0L6_2atmpS2039;
  #line 261 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2043 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS728
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS728)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS728->$0 = _M0L6_2atmpS2043;
  _M0L3resS728->$1 = 0;
  _M0L1iS729
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS729)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS729->$0 = 0;
  _M0L5startS730
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS730)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS730->$0 = 0;
  while (1) {
    int32_t _M0L3valS2026 = _M0L1iS729->$0;
    int32_t _M0L6_2atmpS2027 = Moonbit_array_length(_M0L1sS726);
    if (_M0L3valS2026 < _M0L6_2atmpS2027) {
      int32_t _M0L3valS2030 = _M0L1iS729->$0;
      int32_t _M0L6_2atmpS2029;
      int32_t _M0L6_2atmpS2028;
      int32_t _M0L3valS2037;
      int32_t _M0L6_2atmpS2036;
      if (
        _M0L3valS2030 < 0
        || _M0L3valS2030 >= Moonbit_array_length(_M0L1sS726)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2029 = _M0L1sS726[_M0L3valS2030];
      _M0L6_2atmpS2028 = _M0L6_2atmpS2029;
      if (_M0L6_2atmpS2028 == _M0L3sepS727) {
        int32_t _M0L3valS2032 = _M0L5startS730->$0;
        int32_t _M0L3valS2033 = _M0L1iS729->$0;
        moonbit_string_t _M0L6_2atmpS2031;
        int32_t _M0L3valS2035;
        int32_t _M0L6_2atmpS2034;
        moonbit_incref(_M0L1sS726);
        #line 270 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS2031
        = _M0MPC16string6String17unsafe__substring(_M0L1sS726, _M0L3valS2032, _M0L3valS2033);
        moonbit_incref(_M0L3resS728);
        #line 270 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS728, _M0L6_2atmpS2031);
        _M0L3valS2035 = _M0L1iS729->$0;
        _M0L6_2atmpS2034 = _M0L3valS2035 + 1;
        _M0L5startS730->$0 = _M0L6_2atmpS2034;
      }
      _M0L3valS2037 = _M0L1iS729->$0;
      _M0L6_2atmpS2036 = _M0L3valS2037 + 1;
      _M0L1iS729->$0 = _M0L6_2atmpS2036;
      continue;
    } else {
      moonbit_decref(_M0L1iS729);
    }
    break;
  }
  _M0L3valS2038 = _M0L5startS730->$0;
  _M0L6_2atmpS2039 = Moonbit_array_length(_M0L1sS726);
  if (_M0L3valS2038 < _M0L6_2atmpS2039) {
    int32_t _M0L8_2afieldS2054 = _M0L5startS730->$0;
    int32_t _M0L3valS2041;
    int32_t _M0L6_2atmpS2042;
    moonbit_string_t _M0L6_2atmpS2040;
    moonbit_decref(_M0L5startS730);
    _M0L3valS2041 = _M0L8_2afieldS2054;
    _M0L6_2atmpS2042 = Moonbit_array_length(_M0L1sS726);
    #line 276 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS2040
    = _M0MPC16string6String17unsafe__substring(_M0L1sS726, _M0L3valS2041, _M0L6_2atmpS2042);
    moonbit_incref(_M0L3resS728);
    #line 276 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS728, _M0L6_2atmpS2040);
  } else {
    moonbit_decref(_M0L5startS730);
    moonbit_decref(_M0L1sS726);
  }
  return _M0L3resS728;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS720(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713
) {
  moonbit_bytes_t* _M0L3tmpS721;
  int32_t _M0L6_2atmpS2024;
  struct _M0TPB5ArrayGsE* _M0L3resS722;
  int32_t _M0L1iS723;
  #line 250 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS721
  = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2024 = Moonbit_array_length(_M0L3tmpS721);
  #line 254 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L3resS722 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2024);
  _M0L1iS723 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2020 = Moonbit_array_length(_M0L3tmpS721);
    if (_M0L1iS723 < _M0L6_2atmpS2020) {
      moonbit_bytes_t _M0L6_2atmpS2055;
      moonbit_bytes_t _M0L6_2atmpS2022;
      moonbit_string_t _M0L6_2atmpS2021;
      int32_t _M0L6_2atmpS2023;
      if (_M0L1iS723 < 0 || _M0L1iS723 >= Moonbit_array_length(_M0L3tmpS721)) {
        #line 256 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2055 = (moonbit_bytes_t)_M0L3tmpS721[_M0L1iS723];
      _M0L6_2atmpS2022 = _M0L6_2atmpS2055;
      moonbit_incref(_M0L6_2atmpS2022);
      #line 256 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS2021
      = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713, _M0L6_2atmpS2022);
      moonbit_incref(_M0L3resS722);
      #line 256 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS722, _M0L6_2atmpS2021);
      _M0L6_2atmpS2023 = _M0L1iS723 + 1;
      _M0L1iS723 = _M0L6_2atmpS2023;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS721);
    }
    break;
  }
  return _M0L3resS722;
}

moonbit_string_t _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS713(
  int32_t _M0L6_2aenvS1934,
  moonbit_bytes_t _M0L5bytesS714
) {
  struct _M0TPB13StringBuilder* _M0L3resS715;
  int32_t _M0L3lenS716;
  struct _M0TPC13ref3RefGiE* _M0L1iS717;
  #line 206 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L3resS715 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS716 = Moonbit_array_length(_M0L5bytesS714);
  _M0L1iS717
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS717)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS717->$0 = 0;
  while (1) {
    int32_t _M0L3valS1935 = _M0L1iS717->$0;
    if (_M0L3valS1935 < _M0L3lenS716) {
      int32_t _M0L3valS2019 = _M0L1iS717->$0;
      int32_t _M0L6_2atmpS2018;
      int32_t _M0L6_2atmpS2017;
      struct _M0TPC13ref3RefGiE* _M0L1cS718;
      int32_t _M0L3valS1936;
      if (
        _M0L3valS2019 < 0
        || _M0L3valS2019 >= Moonbit_array_length(_M0L5bytesS714)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2018 = _M0L5bytesS714[_M0L3valS2019];
      _M0L6_2atmpS2017 = (int32_t)_M0L6_2atmpS2018;
      _M0L1cS718
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS718)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS718->$0 = _M0L6_2atmpS2017;
      _M0L3valS1936 = _M0L1cS718->$0;
      if (_M0L3valS1936 < 128) {
        int32_t _M0L8_2afieldS2056 = _M0L1cS718->$0;
        int32_t _M0L3valS1938;
        int32_t _M0L6_2atmpS1937;
        int32_t _M0L3valS1940;
        int32_t _M0L6_2atmpS1939;
        moonbit_decref(_M0L1cS718);
        _M0L3valS1938 = _M0L8_2afieldS2056;
        _M0L6_2atmpS1937 = _M0L3valS1938;
        moonbit_incref(_M0L3resS715);
        #line 215 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS715, _M0L6_2atmpS1937);
        _M0L3valS1940 = _M0L1iS717->$0;
        _M0L6_2atmpS1939 = _M0L3valS1940 + 1;
        _M0L1iS717->$0 = _M0L6_2atmpS1939;
      } else {
        int32_t _M0L3valS1941 = _M0L1cS718->$0;
        if (_M0L3valS1941 < 224) {
          int32_t _M0L3valS1943 = _M0L1iS717->$0;
          int32_t _M0L6_2atmpS1942 = _M0L3valS1943 + 1;
          int32_t _M0L3valS1952;
          int32_t _M0L6_2atmpS1951;
          int32_t _M0L6_2atmpS1945;
          int32_t _M0L3valS1950;
          int32_t _M0L6_2atmpS1949;
          int32_t _M0L6_2atmpS1948;
          int32_t _M0L6_2atmpS1947;
          int32_t _M0L6_2atmpS1946;
          int32_t _M0L6_2atmpS1944;
          int32_t _M0L8_2afieldS2057;
          int32_t _M0L3valS1954;
          int32_t _M0L6_2atmpS1953;
          int32_t _M0L3valS1956;
          int32_t _M0L6_2atmpS1955;
          if (_M0L6_2atmpS1942 >= _M0L3lenS716) {
            moonbit_decref(_M0L1cS718);
            moonbit_decref(_M0L1iS717);
            moonbit_decref(_M0L5bytesS714);
            break;
          }
          _M0L3valS1952 = _M0L1cS718->$0;
          _M0L6_2atmpS1951 = _M0L3valS1952 & 31;
          _M0L6_2atmpS1945 = _M0L6_2atmpS1951 << 6;
          _M0L3valS1950 = _M0L1iS717->$0;
          _M0L6_2atmpS1949 = _M0L3valS1950 + 1;
          if (
            _M0L6_2atmpS1949 < 0
            || _M0L6_2atmpS1949 >= Moonbit_array_length(_M0L5bytesS714)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1948 = _M0L5bytesS714[_M0L6_2atmpS1949];
          _M0L6_2atmpS1947 = (int32_t)_M0L6_2atmpS1948;
          _M0L6_2atmpS1946 = _M0L6_2atmpS1947 & 63;
          _M0L6_2atmpS1944 = _M0L6_2atmpS1945 | _M0L6_2atmpS1946;
          _M0L1cS718->$0 = _M0L6_2atmpS1944;
          _M0L8_2afieldS2057 = _M0L1cS718->$0;
          moonbit_decref(_M0L1cS718);
          _M0L3valS1954 = _M0L8_2afieldS2057;
          _M0L6_2atmpS1953 = _M0L3valS1954;
          moonbit_incref(_M0L3resS715);
          #line 222 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS715, _M0L6_2atmpS1953);
          _M0L3valS1956 = _M0L1iS717->$0;
          _M0L6_2atmpS1955 = _M0L3valS1956 + 2;
          _M0L1iS717->$0 = _M0L6_2atmpS1955;
        } else {
          int32_t _M0L3valS1957 = _M0L1cS718->$0;
          if (_M0L3valS1957 < 240) {
            int32_t _M0L3valS1959 = _M0L1iS717->$0;
            int32_t _M0L6_2atmpS1958 = _M0L3valS1959 + 2;
            int32_t _M0L3valS1975;
            int32_t _M0L6_2atmpS1974;
            int32_t _M0L6_2atmpS1967;
            int32_t _M0L3valS1973;
            int32_t _M0L6_2atmpS1972;
            int32_t _M0L6_2atmpS1971;
            int32_t _M0L6_2atmpS1970;
            int32_t _M0L6_2atmpS1969;
            int32_t _M0L6_2atmpS1968;
            int32_t _M0L6_2atmpS1961;
            int32_t _M0L3valS1966;
            int32_t _M0L6_2atmpS1965;
            int32_t _M0L6_2atmpS1964;
            int32_t _M0L6_2atmpS1963;
            int32_t _M0L6_2atmpS1962;
            int32_t _M0L6_2atmpS1960;
            int32_t _M0L8_2afieldS2058;
            int32_t _M0L3valS1977;
            int32_t _M0L6_2atmpS1976;
            int32_t _M0L3valS1979;
            int32_t _M0L6_2atmpS1978;
            if (_M0L6_2atmpS1958 >= _M0L3lenS716) {
              moonbit_decref(_M0L1cS718);
              moonbit_decref(_M0L1iS717);
              moonbit_decref(_M0L5bytesS714);
              break;
            }
            _M0L3valS1975 = _M0L1cS718->$0;
            _M0L6_2atmpS1974 = _M0L3valS1975 & 15;
            _M0L6_2atmpS1967 = _M0L6_2atmpS1974 << 12;
            _M0L3valS1973 = _M0L1iS717->$0;
            _M0L6_2atmpS1972 = _M0L3valS1973 + 1;
            if (
              _M0L6_2atmpS1972 < 0
              || _M0L6_2atmpS1972 >= Moonbit_array_length(_M0L5bytesS714)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1971 = _M0L5bytesS714[_M0L6_2atmpS1972];
            _M0L6_2atmpS1970 = (int32_t)_M0L6_2atmpS1971;
            _M0L6_2atmpS1969 = _M0L6_2atmpS1970 & 63;
            _M0L6_2atmpS1968 = _M0L6_2atmpS1969 << 6;
            _M0L6_2atmpS1961 = _M0L6_2atmpS1967 | _M0L6_2atmpS1968;
            _M0L3valS1966 = _M0L1iS717->$0;
            _M0L6_2atmpS1965 = _M0L3valS1966 + 2;
            if (
              _M0L6_2atmpS1965 < 0
              || _M0L6_2atmpS1965 >= Moonbit_array_length(_M0L5bytesS714)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1964 = _M0L5bytesS714[_M0L6_2atmpS1965];
            _M0L6_2atmpS1963 = (int32_t)_M0L6_2atmpS1964;
            _M0L6_2atmpS1962 = _M0L6_2atmpS1963 & 63;
            _M0L6_2atmpS1960 = _M0L6_2atmpS1961 | _M0L6_2atmpS1962;
            _M0L1cS718->$0 = _M0L6_2atmpS1960;
            _M0L8_2afieldS2058 = _M0L1cS718->$0;
            moonbit_decref(_M0L1cS718);
            _M0L3valS1977 = _M0L8_2afieldS2058;
            _M0L6_2atmpS1976 = _M0L3valS1977;
            moonbit_incref(_M0L3resS715);
            #line 231 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS715, _M0L6_2atmpS1976);
            _M0L3valS1979 = _M0L1iS717->$0;
            _M0L6_2atmpS1978 = _M0L3valS1979 + 3;
            _M0L1iS717->$0 = _M0L6_2atmpS1978;
          } else {
            int32_t _M0L3valS1981 = _M0L1iS717->$0;
            int32_t _M0L6_2atmpS1980 = _M0L3valS1981 + 3;
            int32_t _M0L3valS2004;
            int32_t _M0L6_2atmpS2003;
            int32_t _M0L6_2atmpS1996;
            int32_t _M0L3valS2002;
            int32_t _M0L6_2atmpS2001;
            int32_t _M0L6_2atmpS2000;
            int32_t _M0L6_2atmpS1999;
            int32_t _M0L6_2atmpS1998;
            int32_t _M0L6_2atmpS1997;
            int32_t _M0L6_2atmpS1989;
            int32_t _M0L3valS1995;
            int32_t _M0L6_2atmpS1994;
            int32_t _M0L6_2atmpS1993;
            int32_t _M0L6_2atmpS1992;
            int32_t _M0L6_2atmpS1991;
            int32_t _M0L6_2atmpS1990;
            int32_t _M0L6_2atmpS1983;
            int32_t _M0L3valS1988;
            int32_t _M0L6_2atmpS1987;
            int32_t _M0L6_2atmpS1986;
            int32_t _M0L6_2atmpS1985;
            int32_t _M0L6_2atmpS1984;
            int32_t _M0L6_2atmpS1982;
            int32_t _M0L3valS2006;
            int32_t _M0L6_2atmpS2005;
            int32_t _M0L3valS2010;
            int32_t _M0L6_2atmpS2009;
            int32_t _M0L6_2atmpS2008;
            int32_t _M0L6_2atmpS2007;
            int32_t _M0L8_2afieldS2059;
            int32_t _M0L3valS2014;
            int32_t _M0L6_2atmpS2013;
            int32_t _M0L6_2atmpS2012;
            int32_t _M0L6_2atmpS2011;
            int32_t _M0L3valS2016;
            int32_t _M0L6_2atmpS2015;
            if (_M0L6_2atmpS1980 >= _M0L3lenS716) {
              moonbit_decref(_M0L1cS718);
              moonbit_decref(_M0L1iS717);
              moonbit_decref(_M0L5bytesS714);
              break;
            }
            _M0L3valS2004 = _M0L1cS718->$0;
            _M0L6_2atmpS2003 = _M0L3valS2004 & 7;
            _M0L6_2atmpS1996 = _M0L6_2atmpS2003 << 18;
            _M0L3valS2002 = _M0L1iS717->$0;
            _M0L6_2atmpS2001 = _M0L3valS2002 + 1;
            if (
              _M0L6_2atmpS2001 < 0
              || _M0L6_2atmpS2001 >= Moonbit_array_length(_M0L5bytesS714)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2000 = _M0L5bytesS714[_M0L6_2atmpS2001];
            _M0L6_2atmpS1999 = (int32_t)_M0L6_2atmpS2000;
            _M0L6_2atmpS1998 = _M0L6_2atmpS1999 & 63;
            _M0L6_2atmpS1997 = _M0L6_2atmpS1998 << 12;
            _M0L6_2atmpS1989 = _M0L6_2atmpS1996 | _M0L6_2atmpS1997;
            _M0L3valS1995 = _M0L1iS717->$0;
            _M0L6_2atmpS1994 = _M0L3valS1995 + 2;
            if (
              _M0L6_2atmpS1994 < 0
              || _M0L6_2atmpS1994 >= Moonbit_array_length(_M0L5bytesS714)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1993 = _M0L5bytesS714[_M0L6_2atmpS1994];
            _M0L6_2atmpS1992 = (int32_t)_M0L6_2atmpS1993;
            _M0L6_2atmpS1991 = _M0L6_2atmpS1992 & 63;
            _M0L6_2atmpS1990 = _M0L6_2atmpS1991 << 6;
            _M0L6_2atmpS1983 = _M0L6_2atmpS1989 | _M0L6_2atmpS1990;
            _M0L3valS1988 = _M0L1iS717->$0;
            _M0L6_2atmpS1987 = _M0L3valS1988 + 3;
            if (
              _M0L6_2atmpS1987 < 0
              || _M0L6_2atmpS1987 >= Moonbit_array_length(_M0L5bytesS714)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1986 = _M0L5bytesS714[_M0L6_2atmpS1987];
            _M0L6_2atmpS1985 = (int32_t)_M0L6_2atmpS1986;
            _M0L6_2atmpS1984 = _M0L6_2atmpS1985 & 63;
            _M0L6_2atmpS1982 = _M0L6_2atmpS1983 | _M0L6_2atmpS1984;
            _M0L1cS718->$0 = _M0L6_2atmpS1982;
            _M0L3valS2006 = _M0L1cS718->$0;
            _M0L6_2atmpS2005 = _M0L3valS2006 - 65536;
            _M0L1cS718->$0 = _M0L6_2atmpS2005;
            _M0L3valS2010 = _M0L1cS718->$0;
            _M0L6_2atmpS2009 = _M0L3valS2010 >> 10;
            _M0L6_2atmpS2008 = _M0L6_2atmpS2009 + 55296;
            _M0L6_2atmpS2007 = _M0L6_2atmpS2008;
            moonbit_incref(_M0L3resS715);
            #line 242 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS715, _M0L6_2atmpS2007);
            _M0L8_2afieldS2059 = _M0L1cS718->$0;
            moonbit_decref(_M0L1cS718);
            _M0L3valS2014 = _M0L8_2afieldS2059;
            _M0L6_2atmpS2013 = _M0L3valS2014 & 1023;
            _M0L6_2atmpS2012 = _M0L6_2atmpS2013 + 56320;
            _M0L6_2atmpS2011 = _M0L6_2atmpS2012;
            moonbit_incref(_M0L3resS715);
            #line 243 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS715, _M0L6_2atmpS2011);
            _M0L3valS2016 = _M0L1iS717->$0;
            _M0L6_2atmpS2015 = _M0L3valS2016 + 4;
            _M0L1iS717->$0 = _M0L6_2atmpS2015;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS717);
      moonbit_decref(_M0L5bytesS714);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS715);
}

int32_t _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S707(
  int32_t _M0L6_2aenvS1927,
  moonbit_string_t _M0L1sS708
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS709;
  int32_t _M0L3lenS710;
  int32_t _M0L1iS711;
  int32_t _M0L8_2afieldS2060;
  #line 197 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L3resS709
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS709)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS709->$0 = 0;
  _M0L3lenS710 = Moonbit_array_length(_M0L1sS708);
  _M0L1iS711 = 0;
  while (1) {
    if (_M0L1iS711 < _M0L3lenS710) {
      int32_t _M0L3valS1932 = _M0L3resS709->$0;
      int32_t _M0L6_2atmpS1929 = _M0L3valS1932 * 10;
      int32_t _M0L6_2atmpS1931;
      int32_t _M0L6_2atmpS1930;
      int32_t _M0L6_2atmpS1928;
      int32_t _M0L6_2atmpS1933;
      if (_M0L1iS711 < 0 || _M0L1iS711 >= Moonbit_array_length(_M0L1sS708)) {
        #line 201 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1931 = _M0L1sS708[_M0L1iS711];
      _M0L6_2atmpS1930 = _M0L6_2atmpS1931 - 48;
      _M0L6_2atmpS1928 = _M0L6_2atmpS1929 + _M0L6_2atmpS1930;
      _M0L3resS709->$0 = _M0L6_2atmpS1928;
      _M0L6_2atmpS1933 = _M0L1iS711 + 1;
      _M0L1iS711 = _M0L6_2atmpS1933;
      continue;
    } else {
      moonbit_decref(_M0L1sS708);
    }
    break;
  }
  _M0L8_2afieldS2060 = _M0L3resS709->$0;
  moonbit_decref(_M0L3resS709);
  return _M0L8_2afieldS2060;
}

int32_t _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS706,
  moonbit_string_t _M0L8filenameS681,
  int32_t _M0L5indexS684
) {
  struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676* _closure_2406;
  struct _M0TWssbEu* _M0L14handle__resultS676;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS685;
  void* _M0L11_2atry__errS700;
  struct moonbit_result_1 _tmp_2408;
  int32_t _handle__error__result_2409;
  int32_t _M0L6_2atmpS1915;
  void* _M0L3errS701;
  moonbit_string_t _M0L4nameS703;
  struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS704;
  moonbit_string_t _M0L8_2afieldS2061;
  int32_t _M0L6_2acntS2357;
  moonbit_string_t _M0L7_2anameS705;
  #line 483 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS681);
  _closure_2406
  = (struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676*)moonbit_malloc(sizeof(struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676));
  Moonbit_object_header(_closure_2406)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676, $1) >> 2, 1, 0);
  _closure_2406->code
  = &_M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN14handle__resultS676;
  _closure_2406->$0 = _M0L5indexS684;
  _closure_2406->$1 = _M0L8filenameS681;
  _M0L14handle__resultS676 = (struct _M0TWssbEu*)_closure_2406;
  _M0L17error__to__stringS685
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN17error__to__stringS685$closure.data;
  moonbit_incref(_M0L12async__testsS706);
  moonbit_incref(_M0L17error__to__stringS685);
  moonbit_incref(_M0L8filenameS681);
  moonbit_incref(_M0L14handle__resultS676);
  #line 517 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _tmp_2408
  = _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk41MoonBit__Test__Driver__Internal__No__ArgsE(_M0L12async__testsS706, _M0L8filenameS681, _M0L5indexS684, _M0L14handle__resultS676, _M0L17error__to__stringS685);
  if (_tmp_2408.tag) {
    int32_t const _M0L5_2aokS1924 = _tmp_2408.data.ok;
    _handle__error__result_2409 = _M0L5_2aokS1924;
  } else {
    void* const _M0L6_2aerrS1925 = _tmp_2408.data.err;
    moonbit_decref(_M0L12async__testsS706);
    moonbit_decref(_M0L17error__to__stringS685);
    moonbit_decref(_M0L8filenameS681);
    _M0L11_2atry__errS700 = _M0L6_2aerrS1925;
    goto join_699;
  }
  if (_handle__error__result_2409) {
    moonbit_decref(_M0L12async__testsS706);
    moonbit_decref(_M0L17error__to__stringS685);
    moonbit_decref(_M0L8filenameS681);
    _M0L6_2atmpS1915 = 1;
  } else {
    struct moonbit_result_1 _tmp_2410;
    int32_t _handle__error__result_2411;
    moonbit_incref(_M0L12async__testsS706);
    moonbit_incref(_M0L17error__to__stringS685);
    moonbit_incref(_M0L8filenameS681);
    moonbit_incref(_M0L14handle__resultS676);
    #line 520 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
    _tmp_2410
    = _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS706, _M0L8filenameS681, _M0L5indexS684, _M0L14handle__resultS676, _M0L17error__to__stringS685);
    if (_tmp_2410.tag) {
      int32_t const _M0L5_2aokS1922 = _tmp_2410.data.ok;
      _handle__error__result_2411 = _M0L5_2aokS1922;
    } else {
      void* const _M0L6_2aerrS1923 = _tmp_2410.data.err;
      moonbit_decref(_M0L12async__testsS706);
      moonbit_decref(_M0L17error__to__stringS685);
      moonbit_decref(_M0L8filenameS681);
      _M0L11_2atry__errS700 = _M0L6_2aerrS1923;
      goto join_699;
    }
    if (_handle__error__result_2411) {
      moonbit_decref(_M0L12async__testsS706);
      moonbit_decref(_M0L17error__to__stringS685);
      moonbit_decref(_M0L8filenameS681);
      _M0L6_2atmpS1915 = 1;
    } else {
      struct moonbit_result_1 _tmp_2412;
      int32_t _handle__error__result_2413;
      moonbit_incref(_M0L12async__testsS706);
      moonbit_incref(_M0L17error__to__stringS685);
      moonbit_incref(_M0L8filenameS681);
      moonbit_incref(_M0L14handle__resultS676);
      #line 523 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _tmp_2412
      = _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS706, _M0L8filenameS681, _M0L5indexS684, _M0L14handle__resultS676, _M0L17error__to__stringS685);
      if (_tmp_2412.tag) {
        int32_t const _M0L5_2aokS1920 = _tmp_2412.data.ok;
        _handle__error__result_2413 = _M0L5_2aokS1920;
      } else {
        void* const _M0L6_2aerrS1921 = _tmp_2412.data.err;
        moonbit_decref(_M0L12async__testsS706);
        moonbit_decref(_M0L17error__to__stringS685);
        moonbit_decref(_M0L8filenameS681);
        _M0L11_2atry__errS700 = _M0L6_2aerrS1921;
        goto join_699;
      }
      if (_handle__error__result_2413) {
        moonbit_decref(_M0L12async__testsS706);
        moonbit_decref(_M0L17error__to__stringS685);
        moonbit_decref(_M0L8filenameS681);
        _M0L6_2atmpS1915 = 1;
      } else {
        struct moonbit_result_1 _tmp_2414;
        int32_t _handle__error__result_2415;
        moonbit_incref(_M0L12async__testsS706);
        moonbit_incref(_M0L17error__to__stringS685);
        moonbit_incref(_M0L8filenameS681);
        moonbit_incref(_M0L14handle__resultS676);
        #line 526 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        _tmp_2414
        = _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS706, _M0L8filenameS681, _M0L5indexS684, _M0L14handle__resultS676, _M0L17error__to__stringS685);
        if (_tmp_2414.tag) {
          int32_t const _M0L5_2aokS1918 = _tmp_2414.data.ok;
          _handle__error__result_2415 = _M0L5_2aokS1918;
        } else {
          void* const _M0L6_2aerrS1919 = _tmp_2414.data.err;
          moonbit_decref(_M0L12async__testsS706);
          moonbit_decref(_M0L17error__to__stringS685);
          moonbit_decref(_M0L8filenameS681);
          _M0L11_2atry__errS700 = _M0L6_2aerrS1919;
          goto join_699;
        }
        if (_handle__error__result_2415) {
          moonbit_decref(_M0L12async__testsS706);
          moonbit_decref(_M0L17error__to__stringS685);
          moonbit_decref(_M0L8filenameS681);
          _M0L6_2atmpS1915 = 1;
        } else {
          struct moonbit_result_1 _tmp_2416;
          moonbit_incref(_M0L14handle__resultS676);
          #line 529 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
          _tmp_2416
          = _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS706, _M0L8filenameS681, _M0L5indexS684, _M0L14handle__resultS676, _M0L17error__to__stringS685);
          if (_tmp_2416.tag) {
            int32_t const _M0L5_2aokS1916 = _tmp_2416.data.ok;
            _M0L6_2atmpS1915 = _M0L5_2aokS1916;
          } else {
            void* const _M0L6_2aerrS1917 = _tmp_2416.data.err;
            _M0L11_2atry__errS700 = _M0L6_2aerrS1917;
            goto join_699;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1915) {
    void* _M0L97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1926 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1926)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1926)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS700
    = _M0L97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1926;
    goto join_699;
  } else {
    moonbit_decref(_M0L14handle__resultS676);
  }
  goto joinlet_2407;
  join_699:;
  _M0L3errS701 = _M0L11_2atry__errS700;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS704
  = (struct _M0DTPC15error5Error97clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS701;
  _M0L8_2afieldS2061 = _M0L36_2aMoonBitTestDriverInternalSkipTestS704->$0;
  _M0L6_2acntS2357
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS704)->rc;
  if (_M0L6_2acntS2357 > 1) {
    int32_t _M0L11_2anew__cntS2358 = _M0L6_2acntS2357 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS704)->rc
    = _M0L11_2anew__cntS2358;
    moonbit_incref(_M0L8_2afieldS2061);
  } else if (_M0L6_2acntS2357 == 1) {
    #line 536 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS704);
  }
  _M0L7_2anameS705 = _M0L8_2afieldS2061;
  _M0L4nameS703 = _M0L7_2anameS705;
  goto join_702;
  goto joinlet_2417;
  join_702:;
  #line 537 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN14handle__resultS676(_M0L14handle__resultS676, _M0L4nameS703, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2417:;
  joinlet_2407:;
  return 0;
}

moonbit_string_t _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN17error__to__stringS685(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1914,
  void* _M0L3errS686
) {
  void* _M0L1eS688;
  moonbit_string_t _M0L1eS690;
  #line 506 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS1914);
  switch (Moonbit_object_tag(_M0L3errS686)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS691 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS686;
      moonbit_string_t _M0L8_2afieldS2062 = _M0L10_2aFailureS691->$0;
      int32_t _M0L6_2acntS2359 =
        Moonbit_object_header(_M0L10_2aFailureS691)->rc;
      moonbit_string_t _M0L4_2aeS692;
      if (_M0L6_2acntS2359 > 1) {
        int32_t _M0L11_2anew__cntS2360 = _M0L6_2acntS2359 - 1;
        Moonbit_object_header(_M0L10_2aFailureS691)->rc
        = _M0L11_2anew__cntS2360;
        moonbit_incref(_M0L8_2afieldS2062);
      } else if (_M0L6_2acntS2359 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS691);
      }
      _M0L4_2aeS692 = _M0L8_2afieldS2062;
      _M0L1eS690 = _M0L4_2aeS692;
      goto join_689;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS693 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS686;
      moonbit_string_t _M0L8_2afieldS2063 = _M0L15_2aInspectErrorS693->$0;
      int32_t _M0L6_2acntS2361 =
        Moonbit_object_header(_M0L15_2aInspectErrorS693)->rc;
      moonbit_string_t _M0L4_2aeS694;
      if (_M0L6_2acntS2361 > 1) {
        int32_t _M0L11_2anew__cntS2362 = _M0L6_2acntS2361 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS693)->rc
        = _M0L11_2anew__cntS2362;
        moonbit_incref(_M0L8_2afieldS2063);
      } else if (_M0L6_2acntS2361 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS693);
      }
      _M0L4_2aeS694 = _M0L8_2afieldS2063;
      _M0L1eS690 = _M0L4_2aeS694;
      goto join_689;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS695 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS686;
      moonbit_string_t _M0L8_2afieldS2064 = _M0L16_2aSnapshotErrorS695->$0;
      int32_t _M0L6_2acntS2363 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS695)->rc;
      moonbit_string_t _M0L4_2aeS696;
      if (_M0L6_2acntS2363 > 1) {
        int32_t _M0L11_2anew__cntS2364 = _M0L6_2acntS2363 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS695)->rc
        = _M0L11_2anew__cntS2364;
        moonbit_incref(_M0L8_2afieldS2064);
      } else if (_M0L6_2acntS2363 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS695);
      }
      _M0L4_2aeS696 = _M0L8_2afieldS2064;
      _M0L1eS690 = _M0L4_2aeS696;
      goto join_689;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error95clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS697 =
        (struct _M0DTPC15error5Error95clawteam_2fclawteam_2fsdk_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS686;
      moonbit_string_t _M0L8_2afieldS2065 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS697->$0;
      int32_t _M0L6_2acntS2365 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS697)->rc;
      moonbit_string_t _M0L4_2aeS698;
      if (_M0L6_2acntS2365 > 1) {
        int32_t _M0L11_2anew__cntS2366 = _M0L6_2acntS2365 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS697)->rc
        = _M0L11_2anew__cntS2366;
        moonbit_incref(_M0L8_2afieldS2065);
      } else if (_M0L6_2acntS2365 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS697);
      }
      _M0L4_2aeS698 = _M0L8_2afieldS2065;
      _M0L1eS690 = _M0L4_2aeS698;
      goto join_689;
      break;
    }
    default: {
      _M0L1eS688 = _M0L3errS686;
      goto join_687;
      break;
    }
  }
  join_689:;
  return _M0L1eS690;
  join_687:;
  #line 512 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS688);
}

int32_t _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__executeN14handle__resultS676(
  struct _M0TWssbEu* _M0L6_2aenvS1900,
  moonbit_string_t _M0L8testnameS677,
  moonbit_string_t _M0L7messageS678,
  int32_t _M0L7skippedS679
) {
  struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676* _M0L14_2acasted__envS1901;
  moonbit_string_t _M0L8_2afieldS2075;
  moonbit_string_t _M0L8filenameS681;
  int32_t _M0L8_2afieldS2074;
  int32_t _M0L6_2acntS2367;
  int32_t _M0L5indexS684;
  int32_t _if__result_2420;
  moonbit_string_t _M0L10file__nameS680;
  moonbit_string_t _M0L10test__nameS682;
  moonbit_string_t _M0L7messageS683;
  moonbit_string_t _M0L6_2atmpS1913;
  moonbit_string_t _M0L6_2atmpS2073;
  moonbit_string_t _M0L6_2atmpS1912;
  moonbit_string_t _M0L6_2atmpS2072;
  moonbit_string_t _M0L6_2atmpS1910;
  moonbit_string_t _M0L6_2atmpS1911;
  moonbit_string_t _M0L6_2atmpS2071;
  moonbit_string_t _M0L6_2atmpS1909;
  moonbit_string_t _M0L6_2atmpS2070;
  moonbit_string_t _M0L6_2atmpS1907;
  moonbit_string_t _M0L6_2atmpS1908;
  moonbit_string_t _M0L6_2atmpS2069;
  moonbit_string_t _M0L6_2atmpS1906;
  moonbit_string_t _M0L6_2atmpS2068;
  moonbit_string_t _M0L6_2atmpS1904;
  moonbit_string_t _M0L6_2atmpS1905;
  moonbit_string_t _M0L6_2atmpS2067;
  moonbit_string_t _M0L6_2atmpS1903;
  moonbit_string_t _M0L6_2atmpS2066;
  moonbit_string_t _M0L6_2atmpS1902;
  #line 490 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1901
  = (struct _M0R98_24clawteam_2fclawteam_2fsdk_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c676*)_M0L6_2aenvS1900;
  _M0L8_2afieldS2075 = _M0L14_2acasted__envS1901->$1;
  _M0L8filenameS681 = _M0L8_2afieldS2075;
  _M0L8_2afieldS2074 = _M0L14_2acasted__envS1901->$0;
  _M0L6_2acntS2367 = Moonbit_object_header(_M0L14_2acasted__envS1901)->rc;
  if (_M0L6_2acntS2367 > 1) {
    int32_t _M0L11_2anew__cntS2368 = _M0L6_2acntS2367 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1901)->rc
    = _M0L11_2anew__cntS2368;
    moonbit_incref(_M0L8filenameS681);
  } else if (_M0L6_2acntS2367 == 1) {
    #line 490 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1901);
  }
  _M0L5indexS684 = _M0L8_2afieldS2074;
  if (!_M0L7skippedS679) {
    _if__result_2420 = 1;
  } else {
    _if__result_2420 = 0;
  }
  if (_if__result_2420) {
    
  }
  #line 496 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS680 = _M0MPC16string6String6escape(_M0L8filenameS681);
  #line 497 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS682 = _M0MPC16string6String6escape(_M0L8testnameS677);
  #line 498 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS683 = _M0MPC16string6String6escape(_M0L7messageS678);
  #line 499 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 501 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1913
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS680);
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2073
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1913);
  moonbit_decref(_M0L6_2atmpS1913);
  _M0L6_2atmpS1912 = _M0L6_2atmpS2073;
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2072
  = moonbit_add_string(_M0L6_2atmpS1912, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1912);
  _M0L6_2atmpS1910 = _M0L6_2atmpS2072;
  #line 501 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1911
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS684);
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2071 = moonbit_add_string(_M0L6_2atmpS1910, _M0L6_2atmpS1911);
  moonbit_decref(_M0L6_2atmpS1910);
  moonbit_decref(_M0L6_2atmpS1911);
  _M0L6_2atmpS1909 = _M0L6_2atmpS2071;
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2070
  = moonbit_add_string(_M0L6_2atmpS1909, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1909);
  _M0L6_2atmpS1907 = _M0L6_2atmpS2070;
  #line 501 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1908
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS682);
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2069 = moonbit_add_string(_M0L6_2atmpS1907, _M0L6_2atmpS1908);
  moonbit_decref(_M0L6_2atmpS1907);
  moonbit_decref(_M0L6_2atmpS1908);
  _M0L6_2atmpS1906 = _M0L6_2atmpS2069;
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2068
  = moonbit_add_string(_M0L6_2atmpS1906, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1906);
  _M0L6_2atmpS1904 = _M0L6_2atmpS2068;
  #line 501 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1905
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS683);
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2067 = moonbit_add_string(_M0L6_2atmpS1904, _M0L6_2atmpS1905);
  moonbit_decref(_M0L6_2atmpS1904);
  moonbit_decref(_M0L6_2atmpS1905);
  _M0L6_2atmpS1903 = _M0L6_2atmpS2067;
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS2066
  = moonbit_add_string(_M0L6_2atmpS1903, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1903);
  _M0L6_2atmpS1902 = _M0L6_2atmpS2066;
  #line 500 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1902);
  #line 503 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk41MoonBit__Test__Driver__Internal__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S651,
  moonbit_string_t _M0L12_2adiscard__S652,
  int32_t _M0L12_2adiscard__S653,
  struct _M0TWssbEu* _M0L12_2adiscard__S654,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S655
) {
  struct moonbit_result_1 _result_2421;
  #line 34 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S655);
  moonbit_decref(_M0L12_2adiscard__S654);
  moonbit_decref(_M0L12_2adiscard__S652);
  moonbit_decref(_M0L12_2adiscard__S651);
  _result_2421.tag = 1;
  _result_2421.data.ok = 0;
  return _result_2421;
}

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S656,
  moonbit_string_t _M0L12_2adiscard__S657,
  int32_t _M0L12_2adiscard__S658,
  struct _M0TWssbEu* _M0L12_2adiscard__S659,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S660
) {
  struct moonbit_result_1 _result_2422;
  #line 34 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S660);
  moonbit_decref(_M0L12_2adiscard__S659);
  moonbit_decref(_M0L12_2adiscard__S657);
  moonbit_decref(_M0L12_2adiscard__S656);
  _result_2422.tag = 1;
  _result_2422.data.ok = 0;
  return _result_2422;
}

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S661,
  moonbit_string_t _M0L12_2adiscard__S662,
  int32_t _M0L12_2adiscard__S663,
  struct _M0TWssbEu* _M0L12_2adiscard__S664,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S665
) {
  struct moonbit_result_1 _result_2423;
  #line 34 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S665);
  moonbit_decref(_M0L12_2adiscard__S664);
  moonbit_decref(_M0L12_2adiscard__S662);
  moonbit_decref(_M0L12_2adiscard__S661);
  _result_2423.tag = 1;
  _result_2423.data.ok = 0;
  return _result_2423;
}

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S666,
  moonbit_string_t _M0L12_2adiscard__S667,
  int32_t _M0L12_2adiscard__S668,
  struct _M0TWssbEu* _M0L12_2adiscard__S669,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S670
) {
  struct moonbit_result_1 _result_2424;
  #line 34 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S670);
  moonbit_decref(_M0L12_2adiscard__S669);
  moonbit_decref(_M0L12_2adiscard__S667);
  moonbit_decref(_M0L12_2adiscard__S666);
  _result_2424.tag = 1;
  _result_2424.data.ok = 0;
  return _result_2424;
}

struct moonbit_result_1 _M0IP016_24default__implP38clawteam8clawteam3sdk21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam3sdk50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S671,
  moonbit_string_t _M0L12_2adiscard__S672,
  int32_t _M0L12_2adiscard__S673,
  struct _M0TWssbEu* _M0L12_2adiscard__S674,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S675
) {
  struct moonbit_result_1 _result_2425;
  #line 34 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S675);
  moonbit_decref(_M0L12_2adiscard__S674);
  moonbit_decref(_M0L12_2adiscard__S672);
  moonbit_decref(_M0L12_2adiscard__S671);
  _result_2425.tag = 1;
  _result_2425.data.ok = 0;
  return _result_2425;
}

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L3msgS621
) {
  moonbit_string_t _M0L6_2atmpS1859;
  moonbit_string_t _M0L6_2atmpS2076;
  moonbit_string_t _M0L6_2atmpS1858;
  void* _M0L11_2atry__errS624;
  moonbit_string_t* _M0L9backtraceS622;
  struct moonbit_result_2 _tmp_2427;
  moonbit_string_t _M0L7_2abindS625;
  int32_t _M0L6_2atmpS1862;
  struct _M0TPC16string10StringView _M0L6_2atmpS1861;
  moonbit_string_t _M0L6_2atmpS1860;
  #line 16 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1859 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS621);
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS2076
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS1859);
  moonbit_decref(_M0L6_2atmpS1859);
  _M0L6_2atmpS1858 = _M0L6_2atmpS2076;
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1858);
  #line 18 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _tmp_2427
  = _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(32);
  if (_tmp_2427.tag) {
    moonbit_string_t* const _M0L5_2aokS1863 = _tmp_2427.data.ok;
    _M0L9backtraceS622 = _M0L5_2aokS1863;
  } else {
    void* const _M0L6_2aerrS1864 = _tmp_2427.data.err;
    _M0L11_2atry__errS624 = _M0L6_2aerrS1864;
    goto join_623;
  }
  goto joinlet_2426;
  join_623:;
  moonbit_decref(_M0L11_2atry__errS624);
  _M0L9backtraceS622 = (moonbit_string_t*)moonbit_empty_ref_array;
  joinlet_2426:;
  _M0L7_2abindS625 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1862 = Moonbit_array_length(_M0L7_2abindS625);
  _M0L6_2atmpS1861
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1862, _M0L7_2abindS625
  };
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1860
  = _M0MPC15array10FixedArray4join(_M0L9backtraceS622, _M0L6_2atmpS1861);
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1860);
  #line 20 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS626) {
  moonbit_string_t _M0L6_2atmpS1866;
  moonbit_string_t _M0L6_2atmpS2077;
  moonbit_string_t _M0L6_2atmpS1865;
  void* _M0L11_2atry__errS629;
  moonbit_string_t* _M0L9backtraceS627;
  struct moonbit_result_2 _tmp_2429;
  moonbit_string_t _M0L7_2abindS630;
  int32_t _M0L6_2atmpS1869;
  struct _M0TPC16string10StringView _M0L6_2atmpS1868;
  moonbit_string_t _M0L6_2atmpS1867;
  #line 16 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1866 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS626);
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS2077
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS1866);
  moonbit_decref(_M0L6_2atmpS1866);
  _M0L6_2atmpS1865 = _M0L6_2atmpS2077;
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1865);
  #line 18 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _tmp_2429
  = _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(32);
  if (_tmp_2429.tag) {
    moonbit_string_t* const _M0L5_2aokS1870 = _tmp_2429.data.ok;
    _M0L9backtraceS627 = _M0L5_2aokS1870;
  } else {
    void* const _M0L6_2aerrS1871 = _tmp_2429.data.err;
    _M0L11_2atry__errS629 = _M0L6_2aerrS1871;
    goto join_628;
  }
  goto joinlet_2428;
  join_628:;
  moonbit_decref(_M0L11_2atry__errS629);
  _M0L9backtraceS627 = (moonbit_string_t*)moonbit_empty_ref_array;
  joinlet_2428:;
  _M0L7_2abindS630 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1869 = Moonbit_array_length(_M0L7_2abindS630);
  _M0L6_2atmpS1868
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1869, _M0L7_2abindS630
  };
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1867
  = _M0MPC15array10FixedArray4join(_M0L9backtraceS627, _M0L6_2atmpS1868);
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1867);
  #line 20 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGuE(moonbit_string_t _M0L3msgS631) {
  moonbit_string_t _M0L6_2atmpS1873;
  moonbit_string_t _M0L6_2atmpS2078;
  moonbit_string_t _M0L6_2atmpS1872;
  void* _M0L11_2atry__errS634;
  moonbit_string_t* _M0L9backtraceS632;
  struct moonbit_result_2 _tmp_2431;
  moonbit_string_t _M0L7_2abindS635;
  int32_t _M0L6_2atmpS1876;
  struct _M0TPC16string10StringView _M0L6_2atmpS1875;
  moonbit_string_t _M0L6_2atmpS1874;
  #line 16 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1873 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS631);
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS2078
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS1873);
  moonbit_decref(_M0L6_2atmpS1873);
  _M0L6_2atmpS1872 = _M0L6_2atmpS2078;
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1872);
  #line 18 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _tmp_2431
  = _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(32);
  if (_tmp_2431.tag) {
    moonbit_string_t* const _M0L5_2aokS1877 = _tmp_2431.data.ok;
    _M0L9backtraceS632 = _M0L5_2aokS1877;
  } else {
    void* const _M0L6_2aerrS1878 = _tmp_2431.data.err;
    _M0L11_2atry__errS634 = _M0L6_2aerrS1878;
    goto join_633;
  }
  goto joinlet_2430;
  join_633:;
  moonbit_decref(_M0L11_2atry__errS634);
  _M0L9backtraceS632 = (moonbit_string_t*)moonbit_empty_ref_array;
  joinlet_2430:;
  _M0L7_2abindS635 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1876 = Moonbit_array_length(_M0L7_2abindS635);
  _M0L6_2atmpS1875
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1876, _M0L7_2abindS635
  };
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1874
  = _M0MPC15array10FixedArray4join(_M0L9backtraceS632, _M0L6_2atmpS1875);
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1874);
  #line 20 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  moonbit_panic();
  return 0;
}

struct _M0TPB9ArrayViewGyE _M0FPC15abort5abortGRPB9ArrayViewGyEE(
  moonbit_string_t _M0L3msgS636
) {
  moonbit_string_t _M0L6_2atmpS1880;
  moonbit_string_t _M0L6_2atmpS2079;
  moonbit_string_t _M0L6_2atmpS1879;
  void* _M0L11_2atry__errS639;
  moonbit_string_t* _M0L9backtraceS637;
  struct moonbit_result_2 _tmp_2433;
  moonbit_string_t _M0L7_2abindS640;
  int32_t _M0L6_2atmpS1883;
  struct _M0TPC16string10StringView _M0L6_2atmpS1882;
  moonbit_string_t _M0L6_2atmpS1881;
  #line 16 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1880 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS636);
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS2079
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS1880);
  moonbit_decref(_M0L6_2atmpS1880);
  _M0L6_2atmpS1879 = _M0L6_2atmpS2079;
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1879);
  #line 18 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _tmp_2433
  = _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(32);
  if (_tmp_2433.tag) {
    moonbit_string_t* const _M0L5_2aokS1884 = _tmp_2433.data.ok;
    _M0L9backtraceS637 = _M0L5_2aokS1884;
  } else {
    void* const _M0L6_2aerrS1885 = _tmp_2433.data.err;
    _M0L11_2atry__errS639 = _M0L6_2aerrS1885;
    goto join_638;
  }
  goto joinlet_2432;
  join_638:;
  moonbit_decref(_M0L11_2atry__errS639);
  _M0L9backtraceS637 = (moonbit_string_t*)moonbit_empty_ref_array;
  joinlet_2432:;
  _M0L7_2abindS640 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1883 = Moonbit_array_length(_M0L7_2abindS640);
  _M0L6_2atmpS1882
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1883, _M0L7_2abindS640
  };
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1881
  = _M0MPC15array10FixedArray4join(_M0L9backtraceS637, _M0L6_2atmpS1882);
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1881);
  #line 20 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGyE(moonbit_string_t _M0L3msgS641) {
  moonbit_string_t _M0L6_2atmpS1887;
  moonbit_string_t _M0L6_2atmpS2080;
  moonbit_string_t _M0L6_2atmpS1886;
  void* _M0L11_2atry__errS644;
  moonbit_string_t* _M0L9backtraceS642;
  struct moonbit_result_2 _tmp_2435;
  moonbit_string_t _M0L7_2abindS645;
  int32_t _M0L6_2atmpS1890;
  struct _M0TPC16string10StringView _M0L6_2atmpS1889;
  moonbit_string_t _M0L6_2atmpS1888;
  #line 16 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1887 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS641);
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS2080
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS1887);
  moonbit_decref(_M0L6_2atmpS1887);
  _M0L6_2atmpS1886 = _M0L6_2atmpS2080;
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1886);
  #line 18 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _tmp_2435
  = _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(32);
  if (_tmp_2435.tag) {
    moonbit_string_t* const _M0L5_2aokS1891 = _tmp_2435.data.ok;
    _M0L9backtraceS642 = _M0L5_2aokS1891;
  } else {
    void* const _M0L6_2aerrS1892 = _tmp_2435.data.err;
    _M0L11_2atry__errS644 = _M0L6_2aerrS1892;
    goto join_643;
  }
  goto joinlet_2434;
  join_643:;
  moonbit_decref(_M0L11_2atry__errS644);
  _M0L9backtraceS642 = (moonbit_string_t*)moonbit_empty_ref_array;
  joinlet_2434:;
  _M0L7_2abindS645 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1890 = Moonbit_array_length(_M0L7_2abindS645);
  _M0L6_2atmpS1889
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1890, _M0L7_2abindS645
  };
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1888
  = _M0MPC15array10FixedArray4join(_M0L9backtraceS642, _M0L6_2atmpS1889);
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1888);
  #line 20 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS646
) {
  moonbit_string_t _M0L6_2atmpS1894;
  moonbit_string_t _M0L6_2atmpS2081;
  moonbit_string_t _M0L6_2atmpS1893;
  void* _M0L11_2atry__errS649;
  moonbit_string_t* _M0L9backtraceS647;
  struct moonbit_result_2 _tmp_2437;
  moonbit_string_t _M0L7_2abindS650;
  int32_t _M0L6_2atmpS1897;
  struct _M0TPC16string10StringView _M0L6_2atmpS1896;
  moonbit_string_t _M0L6_2atmpS1895;
  #line 16 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1894 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS646);
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS2081
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS1894);
  moonbit_decref(_M0L6_2atmpS1894);
  _M0L6_2atmpS1893 = _M0L6_2atmpS2081;
  #line 17 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1893);
  #line 18 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _tmp_2437
  = _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(32);
  if (_tmp_2437.tag) {
    moonbit_string_t* const _M0L5_2aokS1898 = _tmp_2437.data.ok;
    _M0L9backtraceS647 = _M0L5_2aokS1898;
  } else {
    void* const _M0L6_2aerrS1899 = _tmp_2437.data.err;
    _M0L11_2atry__errS649 = _M0L6_2aerrS1899;
    goto join_648;
  }
  goto joinlet_2436;
  join_648:;
  moonbit_decref(_M0L11_2atry__errS649);
  _M0L9backtraceS647 = (moonbit_string_t*)moonbit_empty_ref_array;
  joinlet_2436:;
  _M0L7_2abindS650 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1897 = Moonbit_array_length(_M0L7_2abindS650);
  _M0L6_2atmpS1896
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1897, _M0L7_2abindS650
  };
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0L6_2atmpS1895
  = _M0MPC15array10FixedArray4join(_M0L9backtraceS647, _M0L6_2atmpS1896);
  #line 19 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1895);
  #line 20 "E:\\moonbit\\clawteam\\internal\\abort\\abort.mbt"
  moonbit_panic();
}

struct moonbit_result_2 _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einner(
  int32_t _M0L1nS619
) {
  moonbit_bytes_t* _M0L6_2atmpS1851;
  struct _M0TWzEsQRPC15error5Error* _M0L6_2atmpS1852;
  #line 5 "E:\\moonbit\\clawteam\\internal\\backtrace\\backtrace.mbt"
  #line 6 "E:\\moonbit\\clawteam\\internal\\backtrace\\backtrace.mbt"
  _M0L6_2atmpS1851
  = _M0FP48clawteam8clawteam8internal9backtrace12c__backtrace(_M0L1nS619);
  _M0L6_2atmpS1852
  = (struct _M0TWzEsQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einnerC1853l6$closure.data;
  #line 6 "E:\\moonbit\\clawteam\\internal\\backtrace\\backtrace.mbt"
  return _M0MPC15array10FixedArray3mapGzsEHRPC28encoding4utf89Malformed(_M0L6_2atmpS1851, _M0L6_2atmpS1852);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal9backtrace17backtrace_2einnerC1853l6(
  struct _M0TWzEsQRPC15error5Error* _M0L6_2aenvS1854,
  moonbit_bytes_t _M0L9backtraceS620
) {
  int32_t _M0L6_2atmpS1857;
  int64_t _M0L6_2atmpS1856;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1855;
  #line 6 "E:\\moonbit\\clawteam\\internal\\backtrace\\backtrace.mbt"
  moonbit_decref(_M0L6_2aenvS1854);
  _M0L6_2atmpS1857 = Moonbit_array_length(_M0L9backtraceS620);
  _M0L6_2atmpS1856 = (int64_t)_M0L6_2atmpS1857;
  #line 6 "E:\\moonbit\\clawteam\\internal\\backtrace\\backtrace.mbt"
  _M0L6_2atmpS1855
  = _M0MPC15bytes5Bytes12view_2einner(_M0L9backtraceS620, 0, _M0L6_2atmpS1856);
  #line 6 "E:\\moonbit\\clawteam\\internal\\backtrace\\backtrace.mbt"
  return _M0FPC28encoding4utf814decode_2einner(_M0L6_2atmpS1855, 0);
}

int32_t _M0FP48clawteam8clawteam8internal2os6atexit(
  void(* _M0L8_2aparamS763)()
) {
  atexit(_M0L8_2aparamS763);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal2os4args() {
  #line 66 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  #line 67 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  return _M0FPC13env4args();
}

struct moonbit_result_3 _M0FP48clawteam8clawteam8internal2os6getenv(
  struct _M0TPC16string10StringView _M0L3keyS604
) {
  moonbit_bytes_t _M0L6_2atmpS1850;
  void* _M0L6_2atmpS2082;
  void* _M0L6c__strS603;
  uint64_t _M0L6_2atmpS1849;
  int32_t _M0L3lenS605;
  moonbit_bytes_t _M0L3bufS606;
  int32_t _M0L1iS607;
  moonbit_bytes_t _M0L7_2abindS609;
  int32_t _M0L6_2atmpS1846;
  int64_t _M0L6_2atmpS1845;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1844;
  struct moonbit_result_0 _tmp_2440;
  moonbit_string_t _M0L6_2atmpS1843;
  moonbit_string_t _M0L6_2atmpS1842;
  struct moonbit_result_3 _result_2442;
  #line 16 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1850 = _M0FPC28encoding4utf814encode_2einner(_M0L3keyS604, 0);
  #line 17 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS2082
  = _M0FP48clawteam8clawteam8internal2os10os__getenv(_M0L6_2atmpS1850);
  moonbit_decref(_M0L6_2atmpS1850);
  _M0L6c__strS603 = _M0L6_2atmpS2082;
  #line 18 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  if (
    _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(_M0L6c__strS603)
  ) {
    moonbit_string_t _M0L6_2atmpS1839 = 0;
    struct moonbit_result_3 _result_2438;
    _result_2438.tag = 1;
    _result_2438.data.ok = _M0L6_2atmpS1839;
    return _result_2438;
  }
  #line 21 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1849
  = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L6c__strS603);
  _M0L3lenS605 = (int32_t)_M0L6_2atmpS1849;
  _M0L3bufS606 = (moonbit_bytes_t)moonbit_make_bytes(_M0L3lenS605, 0);
  _M0L1iS607 = 0;
  while (1) {
    if (_M0L1iS607 < _M0L3lenS605) {
      int32_t _M0L6_2atmpS1840;
      int32_t _M0L6_2atmpS1841;
      #line 24 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
      _M0L6_2atmpS1840
      = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L6c__strS603, _M0L1iS607);
      if (_M0L1iS607 < 0 || _M0L1iS607 >= Moonbit_array_length(_M0L3bufS606)) {
        #line 24 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
        moonbit_panic();
      }
      _M0L3bufS606[_M0L1iS607] = _M0L6_2atmpS1840;
      _M0L6_2atmpS1841 = _M0L1iS607 + 1;
      _M0L1iS607 = _M0L6_2atmpS1841;
      continue;
    }
    break;
  }
  _M0L7_2abindS609 = _M0L3bufS606;
  _M0L6_2atmpS1846 = Moonbit_array_length(_M0L7_2abindS609);
  _M0L6_2atmpS1845 = (int64_t)_M0L6_2atmpS1846;
  #line 26 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1844
  = _M0MPC15bytes5Bytes12view_2einner(_M0L7_2abindS609, 0, _M0L6_2atmpS1845);
  #line 26 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _tmp_2440 = _M0FPC28encoding4utf814decode_2einner(_M0L6_2atmpS1844, 0);
  if (_tmp_2440.tag) {
    moonbit_string_t const _M0L5_2aokS1847 = _tmp_2440.data.ok;
    _M0L6_2atmpS1843 = _M0L5_2aokS1847;
  } else {
    void* const _M0L6_2aerrS1848 = _tmp_2440.data.err;
    struct moonbit_result_3 _result_2441;
    _result_2441.tag = 0;
    _result_2441.data.err = _M0L6_2aerrS1848;
    return _result_2441;
  }
  _M0L6_2atmpS1842 = _M0L6_2atmpS1843;
  _result_2442.tag = 1;
  _result_2442.data.ok = _M0L6_2atmpS1842;
  return _result_2442;
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS601,
  int32_t _M0L6offsetS602
) {
  #line 145 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte(_M0L7pointerS601, _M0L6offsetS602);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void* _M0L4selfS599,
  int32_t _M0L5indexS600
) {
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS599, _M0L5indexS600);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(
  void* _M0L4selfS598
) {
  void* _M0L6_2atmpS1838;
  #line 24 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0L6_2atmpS1838 = _M0L4selfS598;
  #line 25 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0MP48clawteam8clawteam8internal1c7Pointer10__is__null(_M0L6_2atmpS1838);
}

struct moonbit_result_0 _M0FPC28encoding4utf814decode_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS487,
  int32_t _M0L11ignore__bomS488
) {
  struct _M0TPC15bytes9BytesView _M0L5bytesS485;
  int32_t _M0L6_2atmpS1822;
  int32_t _M0L6_2atmpS1821;
  moonbit_bytes_t _M0L1tS493;
  int32_t _M0L4tlenS494;
  int32_t _M0L11_2aparam__0S495;
  struct _M0TPC15bytes9BytesView _M0L11_2aparam__1S496;
  moonbit_bytes_t _M0L6_2atmpS1473;
  int64_t _M0L6_2atmpS1474;
  moonbit_string_t _M0L6_2atmpS1472;
  struct moonbit_result_0 _result_2452;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  if (_M0L11ignore__bomS488) {
    int32_t _M0L3endS1824 = _M0L5bytesS487.$2;
    int32_t _M0L5startS1825 = _M0L5bytesS487.$1;
    int32_t _M0L6_2atmpS1823 = _M0L3endS1824 - _M0L5startS1825;
    if (_M0L6_2atmpS1823 >= 3) {
      moonbit_bytes_t _M0L8_2afieldS2230 = _M0L5bytesS487.$0;
      moonbit_bytes_t _M0L5bytesS1836 = _M0L8_2afieldS2230;
      int32_t _M0L5startS1837 = _M0L5bytesS487.$1;
      int32_t _M0L6_2atmpS2229 = _M0L5bytesS1836[_M0L5startS1837];
      int32_t _M0L4_2axS490 = _M0L6_2atmpS2229;
      if (_M0L4_2axS490 == 239) {
        moonbit_bytes_t _M0L8_2afieldS2228 = _M0L5bytesS487.$0;
        moonbit_bytes_t _M0L5bytesS1833 = _M0L8_2afieldS2228;
        int32_t _M0L5startS1835 = _M0L5bytesS487.$1;
        int32_t _M0L6_2atmpS1834 = _M0L5startS1835 + 1;
        int32_t _M0L6_2atmpS2227 = _M0L5bytesS1833[_M0L6_2atmpS1834];
        int32_t _M0L4_2axS491 = _M0L6_2atmpS2227;
        if (_M0L4_2axS491 == 187) {
          moonbit_bytes_t _M0L8_2afieldS2226 = _M0L5bytesS487.$0;
          moonbit_bytes_t _M0L5bytesS1830 = _M0L8_2afieldS2226;
          int32_t _M0L5startS1832 = _M0L5bytesS487.$1;
          int32_t _M0L6_2atmpS1831 = _M0L5startS1832 + 2;
          int32_t _M0L6_2atmpS2225 = _M0L5bytesS1830[_M0L6_2atmpS1831];
          int32_t _M0L4_2axS492 = _M0L6_2atmpS2225;
          if (_M0L4_2axS492 == 191) {
            moonbit_bytes_t _M0L8_2afieldS2224 = _M0L5bytesS487.$0;
            moonbit_bytes_t _M0L5bytesS1826 = _M0L8_2afieldS2224;
            int32_t _M0L5startS1829 = _M0L5bytesS487.$1;
            int32_t _M0L6_2atmpS1827 = _M0L5startS1829 + 3;
            int32_t _M0L8_2afieldS2223 = _M0L5bytesS487.$2;
            int32_t _M0L3endS1828 = _M0L8_2afieldS2223;
            _M0L5bytesS485
            = (struct _M0TPC15bytes9BytesView){
              _M0L6_2atmpS1827, _M0L3endS1828, _M0L5bytesS1826
            };
          } else {
            goto join_489;
          }
        } else {
          goto join_489;
        }
      } else {
        goto join_489;
      }
    } else {
      goto join_489;
    }
    goto joinlet_2444;
    join_489:;
    goto join_486;
    joinlet_2444:;
  } else {
    goto join_486;
  }
  goto joinlet_2443;
  join_486:;
  _M0L5bytesS485 = _M0L5bytesS487;
  joinlet_2443:;
  moonbit_incref(_M0L5bytesS485.$0);
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS1822 = _M0MPC15bytes9BytesView6length(_M0L5bytesS485);
  _M0L6_2atmpS1821 = _M0L6_2atmpS1822 * 2;
  _M0L1tS493 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1821, 0);
  _M0L11_2aparam__0S495 = 0;
  _M0L11_2aparam__1S496 = _M0L5bytesS485;
  while (1) {
    struct _M0TPC15bytes9BytesView _M0L5bytesS498;
    int32_t _M0L4tlenS500;
    int32_t _M0L2b0S501;
    int32_t _M0L2b1S502;
    int32_t _M0L2b2S503;
    int32_t _M0L2b3S504;
    struct _M0TPC15bytes9BytesView _M0L4restS505;
    int32_t _M0L4tlenS512;
    int32_t _M0L2b0S513;
    int32_t _M0L2b1S514;
    int32_t _M0L2b2S515;
    struct _M0TPC15bytes9BytesView _M0L4restS516;
    int32_t _M0L4tlenS519;
    struct _M0TPC15bytes9BytesView _M0L4restS520;
    int32_t _M0L2b0S521;
    int32_t _M0L2b1S522;
    int32_t _M0L4tlenS525;
    struct _M0TPC15bytes9BytesView _M0L4restS526;
    int32_t _M0L1bS527;
    int32_t _M0L3endS1527 = _M0L11_2aparam__1S496.$2;
    int32_t _M0L5startS1528 = _M0L11_2aparam__1S496.$1;
    int32_t _M0L6_2atmpS1526 = _M0L3endS1527 - _M0L5startS1528;
    int32_t _M0L6_2atmpS1525;
    int32_t _M0L6_2atmpS1524;
    int32_t _M0L6_2atmpS1523;
    int32_t _M0L6_2atmpS1520;
    int32_t _M0L6_2atmpS1522;
    int32_t _M0L6_2atmpS1521;
    int32_t _M0L2chS523;
    int32_t _M0L6_2atmpS1515;
    int32_t _M0L6_2atmpS1516;
    int32_t _M0L6_2atmpS1518;
    int32_t _M0L6_2atmpS1517;
    int32_t _M0L6_2atmpS1519;
    int32_t _M0L6_2atmpS1514;
    int32_t _M0L6_2atmpS1513;
    int32_t _M0L6_2atmpS1509;
    int32_t _M0L6_2atmpS1512;
    int32_t _M0L6_2atmpS1511;
    int32_t _M0L6_2atmpS1510;
    int32_t _M0L6_2atmpS1506;
    int32_t _M0L6_2atmpS1508;
    int32_t _M0L6_2atmpS1507;
    int32_t _M0L2chS517;
    int32_t _M0L6_2atmpS1501;
    int32_t _M0L6_2atmpS1502;
    int32_t _M0L6_2atmpS1504;
    int32_t _M0L6_2atmpS1503;
    int32_t _M0L6_2atmpS1505;
    int32_t _M0L6_2atmpS1500;
    int32_t _M0L6_2atmpS1499;
    int32_t _M0L6_2atmpS1495;
    int32_t _M0L6_2atmpS1498;
    int32_t _M0L6_2atmpS1497;
    int32_t _M0L6_2atmpS1496;
    int32_t _M0L6_2atmpS1491;
    int32_t _M0L6_2atmpS1494;
    int32_t _M0L6_2atmpS1493;
    int32_t _M0L6_2atmpS1492;
    int32_t _M0L6_2atmpS1488;
    int32_t _M0L6_2atmpS1490;
    int32_t _M0L6_2atmpS1489;
    int32_t _M0L2chS506;
    int32_t _M0L3chmS507;
    int32_t _M0L6_2atmpS1487;
    int32_t _M0L3ch1S508;
    int32_t _M0L6_2atmpS1486;
    int32_t _M0L3ch2S509;
    int32_t _M0L6_2atmpS1476;
    int32_t _M0L6_2atmpS1477;
    int32_t _M0L6_2atmpS1479;
    int32_t _M0L6_2atmpS1478;
    int32_t _M0L6_2atmpS1480;
    int32_t _M0L6_2atmpS1481;
    int32_t _M0L6_2atmpS1482;
    int32_t _M0L6_2atmpS1484;
    int32_t _M0L6_2atmpS1483;
    int32_t _M0L6_2atmpS1485;
    void* _M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475;
    struct moonbit_result_0 _result_2451;
    if (_M0L6_2atmpS1526 == 0) {
      moonbit_decref(_M0L11_2aparam__1S496.$0);
      _M0L4tlenS494 = _M0L11_2aparam__0S495;
    } else {
      int32_t _M0L3endS1530 = _M0L11_2aparam__1S496.$2;
      int32_t _M0L5startS1531 = _M0L11_2aparam__1S496.$1;
      int32_t _M0L6_2atmpS1529 = _M0L3endS1530 - _M0L5startS1531;
      if (_M0L6_2atmpS1529 >= 8) {
        moonbit_bytes_t _M0L8_2afieldS2166 = _M0L11_2aparam__1S496.$0;
        moonbit_bytes_t _M0L5bytesS1679 = _M0L8_2afieldS2166;
        int32_t _M0L5startS1680 = _M0L11_2aparam__1S496.$1;
        int32_t _M0L6_2atmpS2165 = _M0L5bytesS1679[_M0L5startS1680];
        int32_t _M0L4_2axS528 = _M0L6_2atmpS2165;
        if (_M0L4_2axS528 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS2112 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1676 = _M0L8_2afieldS2112;
          int32_t _M0L5startS1678 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1677 = _M0L5startS1678 + 1;
          int32_t _M0L6_2atmpS2111 = _M0L5bytesS1676[_M0L6_2atmpS1677];
          int32_t _M0L4_2axS529 = _M0L6_2atmpS2111;
          if (_M0L4_2axS529 <= 127) {
            moonbit_bytes_t _M0L8_2afieldS2108 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1673 = _M0L8_2afieldS2108;
            int32_t _M0L5startS1675 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1674 = _M0L5startS1675 + 2;
            int32_t _M0L6_2atmpS2107 = _M0L5bytesS1673[_M0L6_2atmpS1674];
            int32_t _M0L4_2axS530 = _M0L6_2atmpS2107;
            if (_M0L4_2axS530 <= 127) {
              moonbit_bytes_t _M0L8_2afieldS2104 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1670 = _M0L8_2afieldS2104;
              int32_t _M0L5startS1672 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1671 = _M0L5startS1672 + 3;
              int32_t _M0L6_2atmpS2103 = _M0L5bytesS1670[_M0L6_2atmpS1671];
              int32_t _M0L4_2axS531 = _M0L6_2atmpS2103;
              if (_M0L4_2axS531 <= 127) {
                moonbit_bytes_t _M0L8_2afieldS2100 = _M0L11_2aparam__1S496.$0;
                moonbit_bytes_t _M0L5bytesS1667 = _M0L8_2afieldS2100;
                int32_t _M0L5startS1669 = _M0L11_2aparam__1S496.$1;
                int32_t _M0L6_2atmpS1668 = _M0L5startS1669 + 4;
                int32_t _M0L6_2atmpS2099 = _M0L5bytesS1667[_M0L6_2atmpS1668];
                int32_t _M0L4_2axS532 = _M0L6_2atmpS2099;
                if (_M0L4_2axS532 <= 127) {
                  moonbit_bytes_t _M0L8_2afieldS2096 =
                    _M0L11_2aparam__1S496.$0;
                  moonbit_bytes_t _M0L5bytesS1664 = _M0L8_2afieldS2096;
                  int32_t _M0L5startS1666 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1665 = _M0L5startS1666 + 5;
                  int32_t _M0L6_2atmpS2095 =
                    _M0L5bytesS1664[_M0L6_2atmpS1665];
                  int32_t _M0L4_2axS533 = _M0L6_2atmpS2095;
                  if (_M0L4_2axS533 <= 127) {
                    moonbit_bytes_t _M0L8_2afieldS2092 =
                      _M0L11_2aparam__1S496.$0;
                    moonbit_bytes_t _M0L5bytesS1661 = _M0L8_2afieldS2092;
                    int32_t _M0L5startS1663 = _M0L11_2aparam__1S496.$1;
                    int32_t _M0L6_2atmpS1662 = _M0L5startS1663 + 6;
                    int32_t _M0L6_2atmpS2091 =
                      _M0L5bytesS1661[_M0L6_2atmpS1662];
                    int32_t _M0L4_2axS534 = _M0L6_2atmpS2091;
                    if (_M0L4_2axS534 <= 127) {
                      moonbit_bytes_t _M0L8_2afieldS2088 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1658 = _M0L8_2afieldS2088;
                      int32_t _M0L5startS1660 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1659 = _M0L5startS1660 + 7;
                      int32_t _M0L6_2atmpS2087 =
                        _M0L5bytesS1658[_M0L6_2atmpS1659];
                      int32_t _M0L4_2axS535 = _M0L6_2atmpS2087;
                      if (_M0L4_2axS535 <= 127) {
                        moonbit_bytes_t _M0L8_2afieldS2084 =
                          _M0L11_2aparam__1S496.$0;
                        moonbit_bytes_t _M0L5bytesS1654 = _M0L8_2afieldS2084;
                        int32_t _M0L5startS1657 = _M0L11_2aparam__1S496.$1;
                        int32_t _M0L6_2atmpS1655 = _M0L5startS1657 + 8;
                        int32_t _M0L8_2afieldS2083 = _M0L11_2aparam__1S496.$2;
                        int32_t _M0L3endS1656 = _M0L8_2afieldS2083;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS536 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1655,
                                                             _M0L3endS1656,
                                                             _M0L5bytesS1654};
                        int32_t _M0L6_2atmpS1646;
                        int32_t _M0L6_2atmpS1647;
                        int32_t _M0L6_2atmpS1648;
                        int32_t _M0L6_2atmpS1649;
                        int32_t _M0L6_2atmpS1650;
                        int32_t _M0L6_2atmpS1651;
                        int32_t _M0L6_2atmpS1652;
                        int32_t _M0L6_2atmpS1653;
                        _M0L1tS493[_M0L11_2aparam__0S495] = _M0L4_2axS528;
                        _M0L6_2atmpS1646 = _M0L11_2aparam__0S495 + 2;
                        _M0L1tS493[_M0L6_2atmpS1646] = _M0L4_2axS529;
                        _M0L6_2atmpS1647 = _M0L11_2aparam__0S495 + 4;
                        _M0L1tS493[_M0L6_2atmpS1647] = _M0L4_2axS530;
                        _M0L6_2atmpS1648 = _M0L11_2aparam__0S495 + 6;
                        _M0L1tS493[_M0L6_2atmpS1648] = _M0L4_2axS531;
                        _M0L6_2atmpS1649 = _M0L11_2aparam__0S495 + 8;
                        _M0L1tS493[_M0L6_2atmpS1649] = _M0L4_2axS532;
                        _M0L6_2atmpS1650 = _M0L11_2aparam__0S495 + 10;
                        _M0L1tS493[_M0L6_2atmpS1650] = _M0L4_2axS533;
                        _M0L6_2atmpS1651 = _M0L11_2aparam__0S495 + 12;
                        _M0L1tS493[_M0L6_2atmpS1651] = _M0L4_2axS534;
                        _M0L6_2atmpS1652 = _M0L11_2aparam__0S495 + 14;
                        _M0L1tS493[_M0L6_2atmpS1652] = _M0L4_2axS535;
                        _M0L6_2atmpS1653 = _M0L11_2aparam__0S495 + 16;
                        _M0L11_2aparam__0S495 = _M0L6_2atmpS1653;
                        _M0L11_2aparam__1S496 = _M0L4_2axS536;
                        continue;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS2086 =
                          _M0L11_2aparam__1S496.$0;
                        moonbit_bytes_t _M0L5bytesS1642 = _M0L8_2afieldS2086;
                        int32_t _M0L5startS1645 = _M0L11_2aparam__1S496.$1;
                        int32_t _M0L6_2atmpS1643 = _M0L5startS1645 + 1;
                        int32_t _M0L8_2afieldS2085 = _M0L11_2aparam__1S496.$2;
                        int32_t _M0L3endS1644 = _M0L8_2afieldS2085;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS537 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1643,
                                                             _M0L3endS1644,
                                                             _M0L5bytesS1642};
                        _M0L4tlenS525 = _M0L11_2aparam__0S495;
                        _M0L4restS526 = _M0L4_2axS537;
                        _M0L1bS527 = _M0L4_2axS528;
                        goto join_524;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS2090 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1638 = _M0L8_2afieldS2090;
                      int32_t _M0L5startS1641 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1639 = _M0L5startS1641 + 1;
                      int32_t _M0L8_2afieldS2089 = _M0L11_2aparam__1S496.$2;
                      int32_t _M0L3endS1640 = _M0L8_2afieldS2089;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS538 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1639,
                                                           _M0L3endS1640,
                                                           _M0L5bytesS1638};
                      _M0L4tlenS525 = _M0L11_2aparam__0S495;
                      _M0L4restS526 = _M0L4_2axS538;
                      _M0L1bS527 = _M0L4_2axS528;
                      goto join_524;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS2094 =
                      _M0L11_2aparam__1S496.$0;
                    moonbit_bytes_t _M0L5bytesS1634 = _M0L8_2afieldS2094;
                    int32_t _M0L5startS1637 = _M0L11_2aparam__1S496.$1;
                    int32_t _M0L6_2atmpS1635 = _M0L5startS1637 + 1;
                    int32_t _M0L8_2afieldS2093 = _M0L11_2aparam__1S496.$2;
                    int32_t _M0L3endS1636 = _M0L8_2afieldS2093;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS539 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1635,
                                                         _M0L3endS1636,
                                                         _M0L5bytesS1634};
                    _M0L4tlenS525 = _M0L11_2aparam__0S495;
                    _M0L4restS526 = _M0L4_2axS539;
                    _M0L1bS527 = _M0L4_2axS528;
                    goto join_524;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS2098 =
                    _M0L11_2aparam__1S496.$0;
                  moonbit_bytes_t _M0L5bytesS1630 = _M0L8_2afieldS2098;
                  int32_t _M0L5startS1633 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1631 = _M0L5startS1633 + 1;
                  int32_t _M0L8_2afieldS2097 = _M0L11_2aparam__1S496.$2;
                  int32_t _M0L3endS1632 = _M0L8_2afieldS2097;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS540 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1631,
                                                       _M0L3endS1632,
                                                       _M0L5bytesS1630};
                  _M0L4tlenS525 = _M0L11_2aparam__0S495;
                  _M0L4restS526 = _M0L4_2axS540;
                  _M0L1bS527 = _M0L4_2axS528;
                  goto join_524;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS2102 = _M0L11_2aparam__1S496.$0;
                moonbit_bytes_t _M0L5bytesS1626 = _M0L8_2afieldS2102;
                int32_t _M0L5startS1629 = _M0L11_2aparam__1S496.$1;
                int32_t _M0L6_2atmpS1627 = _M0L5startS1629 + 1;
                int32_t _M0L8_2afieldS2101 = _M0L11_2aparam__1S496.$2;
                int32_t _M0L3endS1628 = _M0L8_2afieldS2101;
                struct _M0TPC15bytes9BytesView _M0L4_2axS541 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1627,
                                                     _M0L3endS1628,
                                                     _M0L5bytesS1626};
                _M0L4tlenS525 = _M0L11_2aparam__0S495;
                _M0L4restS526 = _M0L4_2axS541;
                _M0L1bS527 = _M0L4_2axS528;
                goto join_524;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS2106 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1622 = _M0L8_2afieldS2106;
              int32_t _M0L5startS1625 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1623 = _M0L5startS1625 + 1;
              int32_t _M0L8_2afieldS2105 = _M0L11_2aparam__1S496.$2;
              int32_t _M0L3endS1624 = _M0L8_2afieldS2105;
              struct _M0TPC15bytes9BytesView _M0L4_2axS542 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1623,
                                                   _M0L3endS1624,
                                                   _M0L5bytesS1622};
              _M0L4tlenS525 = _M0L11_2aparam__0S495;
              _M0L4restS526 = _M0L4_2axS542;
              _M0L1bS527 = _M0L4_2axS528;
              goto join_524;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS2110 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1618 = _M0L8_2afieldS2110;
            int32_t _M0L5startS1621 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1619 = _M0L5startS1621 + 1;
            int32_t _M0L8_2afieldS2109 = _M0L11_2aparam__1S496.$2;
            int32_t _M0L3endS1620 = _M0L8_2afieldS2109;
            struct _M0TPC15bytes9BytesView _M0L4_2axS543 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1619,
                                                 _M0L3endS1620,
                                                 _M0L5bytesS1618};
            _M0L4tlenS525 = _M0L11_2aparam__0S495;
            _M0L4restS526 = _M0L4_2axS543;
            _M0L1bS527 = _M0L4_2axS528;
            goto join_524;
          }
        } else if (_M0L4_2axS528 >= 194 && _M0L4_2axS528 <= 223) {
          moonbit_bytes_t _M0L8_2afieldS2116 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1615 = _M0L8_2afieldS2116;
          int32_t _M0L5startS1617 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1616 = _M0L5startS1617 + 1;
          int32_t _M0L6_2atmpS2115 = _M0L5bytesS1615[_M0L6_2atmpS1616];
          int32_t _M0L4_2axS544 = _M0L6_2atmpS2115;
          if (_M0L4_2axS544 >= 128 && _M0L4_2axS544 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2114 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1611 = _M0L8_2afieldS2114;
            int32_t _M0L5startS1614 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1612 = _M0L5startS1614 + 2;
            int32_t _M0L8_2afieldS2113 = _M0L11_2aparam__1S496.$2;
            int32_t _M0L3endS1613 = _M0L8_2afieldS2113;
            struct _M0TPC15bytes9BytesView _M0L4_2axS545 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1612,
                                                 _M0L3endS1613,
                                                 _M0L5bytesS1611};
            _M0L4tlenS519 = _M0L11_2aparam__0S495;
            _M0L4restS520 = _M0L4_2axS545;
            _M0L2b0S521 = _M0L4_2axS528;
            _M0L2b1S522 = _M0L4_2axS544;
            goto join_518;
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 == 224) {
          moonbit_bytes_t _M0L8_2afieldS2122 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1608 = _M0L8_2afieldS2122;
          int32_t _M0L5startS1610 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1609 = _M0L5startS1610 + 1;
          int32_t _M0L6_2atmpS2121 = _M0L5bytesS1608[_M0L6_2atmpS1609];
          int32_t _M0L4_2axS546 = _M0L6_2atmpS2121;
          if (_M0L4_2axS546 >= 160 && _M0L4_2axS546 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2120 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1605 = _M0L8_2afieldS2120;
            int32_t _M0L5startS1607 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1606 = _M0L5startS1607 + 2;
            int32_t _M0L6_2atmpS2119 = _M0L5bytesS1605[_M0L6_2atmpS1606];
            int32_t _M0L4_2axS547 = _M0L6_2atmpS2119;
            if (_M0L4_2axS547 >= 128 && _M0L4_2axS547 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2118 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1601 = _M0L8_2afieldS2118;
              int32_t _M0L5startS1604 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1602 = _M0L5startS1604 + 3;
              int32_t _M0L8_2afieldS2117 = _M0L11_2aparam__1S496.$2;
              int32_t _M0L3endS1603 = _M0L8_2afieldS2117;
              struct _M0TPC15bytes9BytesView _M0L4_2axS548 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1602,
                                                   _M0L3endS1603,
                                                   _M0L5bytesS1601};
              _M0L4tlenS512 = _M0L11_2aparam__0S495;
              _M0L2b0S513 = _M0L4_2axS528;
              _M0L2b1S514 = _M0L4_2axS546;
              _M0L2b2S515 = _M0L4_2axS547;
              _M0L4restS516 = _M0L4_2axS548;
              goto join_511;
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 >= 225 && _M0L4_2axS528 <= 236) {
          moonbit_bytes_t _M0L8_2afieldS2128 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1598 = _M0L8_2afieldS2128;
          int32_t _M0L5startS1600 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1599 = _M0L5startS1600 + 1;
          int32_t _M0L6_2atmpS2127 = _M0L5bytesS1598[_M0L6_2atmpS1599];
          int32_t _M0L4_2axS549 = _M0L6_2atmpS2127;
          if (_M0L4_2axS549 >= 128 && _M0L4_2axS549 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2126 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1595 = _M0L8_2afieldS2126;
            int32_t _M0L5startS1597 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1596 = _M0L5startS1597 + 2;
            int32_t _M0L6_2atmpS2125 = _M0L5bytesS1595[_M0L6_2atmpS1596];
            int32_t _M0L4_2axS550 = _M0L6_2atmpS2125;
            if (_M0L4_2axS550 >= 128 && _M0L4_2axS550 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2124 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1591 = _M0L8_2afieldS2124;
              int32_t _M0L5startS1594 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1592 = _M0L5startS1594 + 3;
              int32_t _M0L8_2afieldS2123 = _M0L11_2aparam__1S496.$2;
              int32_t _M0L3endS1593 = _M0L8_2afieldS2123;
              struct _M0TPC15bytes9BytesView _M0L4_2axS551 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1592,
                                                   _M0L3endS1593,
                                                   _M0L5bytesS1591};
              _M0L4tlenS512 = _M0L11_2aparam__0S495;
              _M0L2b0S513 = _M0L4_2axS528;
              _M0L2b1S514 = _M0L4_2axS549;
              _M0L2b2S515 = _M0L4_2axS550;
              _M0L4restS516 = _M0L4_2axS551;
              goto join_511;
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 == 237) {
          moonbit_bytes_t _M0L8_2afieldS2134 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1588 = _M0L8_2afieldS2134;
          int32_t _M0L5startS1590 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1589 = _M0L5startS1590 + 1;
          int32_t _M0L6_2atmpS2133 = _M0L5bytesS1588[_M0L6_2atmpS1589];
          int32_t _M0L4_2axS552 = _M0L6_2atmpS2133;
          if (_M0L4_2axS552 >= 128 && _M0L4_2axS552 <= 159) {
            moonbit_bytes_t _M0L8_2afieldS2132 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1585 = _M0L8_2afieldS2132;
            int32_t _M0L5startS1587 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1586 = _M0L5startS1587 + 2;
            int32_t _M0L6_2atmpS2131 = _M0L5bytesS1585[_M0L6_2atmpS1586];
            int32_t _M0L4_2axS553 = _M0L6_2atmpS2131;
            if (_M0L4_2axS553 >= 128 && _M0L4_2axS553 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2130 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1581 = _M0L8_2afieldS2130;
              int32_t _M0L5startS1584 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1582 = _M0L5startS1584 + 3;
              int32_t _M0L8_2afieldS2129 = _M0L11_2aparam__1S496.$2;
              int32_t _M0L3endS1583 = _M0L8_2afieldS2129;
              struct _M0TPC15bytes9BytesView _M0L4_2axS554 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1582,
                                                   _M0L3endS1583,
                                                   _M0L5bytesS1581};
              _M0L4tlenS512 = _M0L11_2aparam__0S495;
              _M0L2b0S513 = _M0L4_2axS528;
              _M0L2b1S514 = _M0L4_2axS552;
              _M0L2b2S515 = _M0L4_2axS553;
              _M0L4restS516 = _M0L4_2axS554;
              goto join_511;
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 >= 238 && _M0L4_2axS528 <= 239) {
          moonbit_bytes_t _M0L8_2afieldS2140 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1578 = _M0L8_2afieldS2140;
          int32_t _M0L5startS1580 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1579 = _M0L5startS1580 + 1;
          int32_t _M0L6_2atmpS2139 = _M0L5bytesS1578[_M0L6_2atmpS1579];
          int32_t _M0L4_2axS555 = _M0L6_2atmpS2139;
          if (_M0L4_2axS555 >= 128 && _M0L4_2axS555 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2138 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1575 = _M0L8_2afieldS2138;
            int32_t _M0L5startS1577 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1576 = _M0L5startS1577 + 2;
            int32_t _M0L6_2atmpS2137 = _M0L5bytesS1575[_M0L6_2atmpS1576];
            int32_t _M0L4_2axS556 = _M0L6_2atmpS2137;
            if (_M0L4_2axS556 >= 128 && _M0L4_2axS556 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2136 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1571 = _M0L8_2afieldS2136;
              int32_t _M0L5startS1574 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1572 = _M0L5startS1574 + 3;
              int32_t _M0L8_2afieldS2135 = _M0L11_2aparam__1S496.$2;
              int32_t _M0L3endS1573 = _M0L8_2afieldS2135;
              struct _M0TPC15bytes9BytesView _M0L4_2axS557 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1572,
                                                   _M0L3endS1573,
                                                   _M0L5bytesS1571};
              _M0L4tlenS512 = _M0L11_2aparam__0S495;
              _M0L2b0S513 = _M0L4_2axS528;
              _M0L2b1S514 = _M0L4_2axS555;
              _M0L2b2S515 = _M0L4_2axS556;
              _M0L4restS516 = _M0L4_2axS557;
              goto join_511;
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 == 240) {
          moonbit_bytes_t _M0L8_2afieldS2148 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1568 = _M0L8_2afieldS2148;
          int32_t _M0L5startS1570 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1569 = _M0L5startS1570 + 1;
          int32_t _M0L6_2atmpS2147 = _M0L5bytesS1568[_M0L6_2atmpS1569];
          int32_t _M0L4_2axS558 = _M0L6_2atmpS2147;
          if (_M0L4_2axS558 >= 144 && _M0L4_2axS558 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2146 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1565 = _M0L8_2afieldS2146;
            int32_t _M0L5startS1567 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1566 = _M0L5startS1567 + 2;
            int32_t _M0L6_2atmpS2145 = _M0L5bytesS1565[_M0L6_2atmpS1566];
            int32_t _M0L4_2axS559 = _M0L6_2atmpS2145;
            if (_M0L4_2axS559 >= 128 && _M0L4_2axS559 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2144 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1562 = _M0L8_2afieldS2144;
              int32_t _M0L5startS1564 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1563 = _M0L5startS1564 + 3;
              int32_t _M0L6_2atmpS2143 = _M0L5bytesS1562[_M0L6_2atmpS1563];
              int32_t _M0L4_2axS560 = _M0L6_2atmpS2143;
              if (_M0L4_2axS560 >= 128 && _M0L4_2axS560 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2142 = _M0L11_2aparam__1S496.$0;
                moonbit_bytes_t _M0L5bytesS1558 = _M0L8_2afieldS2142;
                int32_t _M0L5startS1561 = _M0L11_2aparam__1S496.$1;
                int32_t _M0L6_2atmpS1559 = _M0L5startS1561 + 4;
                int32_t _M0L8_2afieldS2141 = _M0L11_2aparam__1S496.$2;
                int32_t _M0L3endS1560 = _M0L8_2afieldS2141;
                struct _M0TPC15bytes9BytesView _M0L4_2axS561 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1559,
                                                     _M0L3endS1560,
                                                     _M0L5bytesS1558};
                _M0L4tlenS500 = _M0L11_2aparam__0S495;
                _M0L2b0S501 = _M0L4_2axS528;
                _M0L2b1S502 = _M0L4_2axS558;
                _M0L2b2S503 = _M0L4_2axS559;
                _M0L2b3S504 = _M0L4_2axS560;
                _M0L4restS505 = _M0L4_2axS561;
                goto join_499;
              } else {
                moonbit_decref(_M0L1tS493);
                _M0L5bytesS498 = _M0L11_2aparam__1S496;
                goto join_497;
              }
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 >= 241 && _M0L4_2axS528 <= 243) {
          moonbit_bytes_t _M0L8_2afieldS2156 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1555 = _M0L8_2afieldS2156;
          int32_t _M0L5startS1557 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1556 = _M0L5startS1557 + 1;
          int32_t _M0L6_2atmpS2155 = _M0L5bytesS1555[_M0L6_2atmpS1556];
          int32_t _M0L4_2axS562 = _M0L6_2atmpS2155;
          if (_M0L4_2axS562 >= 128 && _M0L4_2axS562 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2154 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1552 = _M0L8_2afieldS2154;
            int32_t _M0L5startS1554 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1553 = _M0L5startS1554 + 2;
            int32_t _M0L6_2atmpS2153 = _M0L5bytesS1552[_M0L6_2atmpS1553];
            int32_t _M0L4_2axS563 = _M0L6_2atmpS2153;
            if (_M0L4_2axS563 >= 128 && _M0L4_2axS563 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2152 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1549 = _M0L8_2afieldS2152;
              int32_t _M0L5startS1551 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1550 = _M0L5startS1551 + 3;
              int32_t _M0L6_2atmpS2151 = _M0L5bytesS1549[_M0L6_2atmpS1550];
              int32_t _M0L4_2axS564 = _M0L6_2atmpS2151;
              if (_M0L4_2axS564 >= 128 && _M0L4_2axS564 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2150 = _M0L11_2aparam__1S496.$0;
                moonbit_bytes_t _M0L5bytesS1545 = _M0L8_2afieldS2150;
                int32_t _M0L5startS1548 = _M0L11_2aparam__1S496.$1;
                int32_t _M0L6_2atmpS1546 = _M0L5startS1548 + 4;
                int32_t _M0L8_2afieldS2149 = _M0L11_2aparam__1S496.$2;
                int32_t _M0L3endS1547 = _M0L8_2afieldS2149;
                struct _M0TPC15bytes9BytesView _M0L4_2axS565 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1546,
                                                     _M0L3endS1547,
                                                     _M0L5bytesS1545};
                _M0L4tlenS500 = _M0L11_2aparam__0S495;
                _M0L2b0S501 = _M0L4_2axS528;
                _M0L2b1S502 = _M0L4_2axS562;
                _M0L2b2S503 = _M0L4_2axS563;
                _M0L2b3S504 = _M0L4_2axS564;
                _M0L4restS505 = _M0L4_2axS565;
                goto join_499;
              } else {
                moonbit_decref(_M0L1tS493);
                _M0L5bytesS498 = _M0L11_2aparam__1S496;
                goto join_497;
              }
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else if (_M0L4_2axS528 == 244) {
          moonbit_bytes_t _M0L8_2afieldS2164 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1542 = _M0L8_2afieldS2164;
          int32_t _M0L5startS1544 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1543 = _M0L5startS1544 + 1;
          int32_t _M0L6_2atmpS2163 = _M0L5bytesS1542[_M0L6_2atmpS1543];
          int32_t _M0L4_2axS566 = _M0L6_2atmpS2163;
          if (_M0L4_2axS566 >= 128 && _M0L4_2axS566 <= 143) {
            moonbit_bytes_t _M0L8_2afieldS2162 = _M0L11_2aparam__1S496.$0;
            moonbit_bytes_t _M0L5bytesS1539 = _M0L8_2afieldS2162;
            int32_t _M0L5startS1541 = _M0L11_2aparam__1S496.$1;
            int32_t _M0L6_2atmpS1540 = _M0L5startS1541 + 2;
            int32_t _M0L6_2atmpS2161 = _M0L5bytesS1539[_M0L6_2atmpS1540];
            int32_t _M0L4_2axS567 = _M0L6_2atmpS2161;
            if (_M0L4_2axS567 >= 128 && _M0L4_2axS567 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2160 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1536 = _M0L8_2afieldS2160;
              int32_t _M0L5startS1538 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1537 = _M0L5startS1538 + 3;
              int32_t _M0L6_2atmpS2159 = _M0L5bytesS1536[_M0L6_2atmpS1537];
              int32_t _M0L4_2axS568 = _M0L6_2atmpS2159;
              if (_M0L4_2axS568 >= 128 && _M0L4_2axS568 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2158 = _M0L11_2aparam__1S496.$0;
                moonbit_bytes_t _M0L5bytesS1532 = _M0L8_2afieldS2158;
                int32_t _M0L5startS1535 = _M0L11_2aparam__1S496.$1;
                int32_t _M0L6_2atmpS1533 = _M0L5startS1535 + 4;
                int32_t _M0L8_2afieldS2157 = _M0L11_2aparam__1S496.$2;
                int32_t _M0L3endS1534 = _M0L8_2afieldS2157;
                struct _M0TPC15bytes9BytesView _M0L4_2axS569 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1533,
                                                     _M0L3endS1534,
                                                     _M0L5bytesS1532};
                _M0L4tlenS500 = _M0L11_2aparam__0S495;
                _M0L2b0S501 = _M0L4_2axS528;
                _M0L2b1S502 = _M0L4_2axS566;
                _M0L2b2S503 = _M0L4_2axS567;
                _M0L2b3S504 = _M0L4_2axS568;
                _M0L4restS505 = _M0L4_2axS569;
                goto join_499;
              } else {
                moonbit_decref(_M0L1tS493);
                _M0L5bytesS498 = _M0L11_2aparam__1S496;
                goto join_497;
              }
            } else {
              moonbit_decref(_M0L1tS493);
              _M0L5bytesS498 = _M0L11_2aparam__1S496;
              goto join_497;
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        } else {
          moonbit_decref(_M0L1tS493);
          _M0L5bytesS498 = _M0L11_2aparam__1S496;
          goto join_497;
        }
      } else {
        moonbit_bytes_t _M0L8_2afieldS2222 = _M0L11_2aparam__1S496.$0;
        moonbit_bytes_t _M0L5bytesS1819 = _M0L8_2afieldS2222;
        int32_t _M0L5startS1820 = _M0L11_2aparam__1S496.$1;
        int32_t _M0L6_2atmpS2221 = _M0L5bytesS1819[_M0L5startS1820];
        int32_t _M0L4_2axS570 = _M0L6_2atmpS2221;
        if (_M0L4_2axS570 >= 0 && _M0L4_2axS570 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS2168 = _M0L11_2aparam__1S496.$0;
          moonbit_bytes_t _M0L5bytesS1815 = _M0L8_2afieldS2168;
          int32_t _M0L5startS1818 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1816 = _M0L5startS1818 + 1;
          int32_t _M0L8_2afieldS2167 = _M0L11_2aparam__1S496.$2;
          int32_t _M0L3endS1817 = _M0L8_2afieldS2167;
          struct _M0TPC15bytes9BytesView _M0L4_2axS571 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1816,
                                               _M0L3endS1817,
                                               _M0L5bytesS1815};
          _M0L4tlenS525 = _M0L11_2aparam__0S495;
          _M0L4restS526 = _M0L4_2axS571;
          _M0L1bS527 = _M0L4_2axS570;
          goto join_524;
        } else {
          int32_t _M0L3endS1682 = _M0L11_2aparam__1S496.$2;
          int32_t _M0L5startS1683 = _M0L11_2aparam__1S496.$1;
          int32_t _M0L6_2atmpS1681 = _M0L3endS1682 - _M0L5startS1683;
          if (_M0L6_2atmpS1681 >= 2) {
            if (_M0L4_2axS570 >= 194 && _M0L4_2axS570 <= 223) {
              moonbit_bytes_t _M0L8_2afieldS2172 = _M0L11_2aparam__1S496.$0;
              moonbit_bytes_t _M0L5bytesS1812 = _M0L8_2afieldS2172;
              int32_t _M0L5startS1814 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1813 = _M0L5startS1814 + 1;
              int32_t _M0L6_2atmpS2171 = _M0L5bytesS1812[_M0L6_2atmpS1813];
              int32_t _M0L4_2axS572 = _M0L6_2atmpS2171;
              if (_M0L4_2axS572 >= 128 && _M0L4_2axS572 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2170 = _M0L11_2aparam__1S496.$0;
                moonbit_bytes_t _M0L5bytesS1808 = _M0L8_2afieldS2170;
                int32_t _M0L5startS1811 = _M0L11_2aparam__1S496.$1;
                int32_t _M0L6_2atmpS1809 = _M0L5startS1811 + 2;
                int32_t _M0L8_2afieldS2169 = _M0L11_2aparam__1S496.$2;
                int32_t _M0L3endS1810 = _M0L8_2afieldS2169;
                struct _M0TPC15bytes9BytesView _M0L4_2axS573 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1809,
                                                     _M0L3endS1810,
                                                     _M0L5bytesS1808};
                _M0L4tlenS519 = _M0L11_2aparam__0S495;
                _M0L4restS520 = _M0L4_2axS573;
                _M0L2b0S521 = _M0L4_2axS570;
                _M0L2b1S522 = _M0L4_2axS572;
                goto join_518;
              } else {
                int32_t _M0L3endS1802;
                int32_t _M0L5startS1803;
                int32_t _M0L6_2atmpS1801;
                moonbit_decref(_M0L1tS493);
                _M0L3endS1802 = _M0L11_2aparam__1S496.$2;
                _M0L5startS1803 = _M0L11_2aparam__1S496.$1;
                _M0L6_2atmpS1801 = _M0L3endS1802 - _M0L5startS1803;
                if (_M0L6_2atmpS1801 >= 3) {
                  int32_t _M0L3endS1806 = _M0L11_2aparam__1S496.$2;
                  int32_t _M0L5startS1807 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1805 = _M0L3endS1806 - _M0L5startS1807;
                  int32_t _M0L6_2atmpS1804 = _M0L6_2atmpS1805 >= 4;
                  _M0L5bytesS498 = _M0L11_2aparam__1S496;
                  goto join_497;
                } else {
                  _M0L5bytesS498 = _M0L11_2aparam__1S496;
                  goto join_497;
                }
              }
            } else {
              int32_t _M0L3endS1685 = _M0L11_2aparam__1S496.$2;
              int32_t _M0L5startS1686 = _M0L11_2aparam__1S496.$1;
              int32_t _M0L6_2atmpS1684 = _M0L3endS1685 - _M0L5startS1686;
              if (_M0L6_2atmpS1684 >= 3) {
                if (_M0L4_2axS570 == 224) {
                  moonbit_bytes_t _M0L8_2afieldS2178 =
                    _M0L11_2aparam__1S496.$0;
                  moonbit_bytes_t _M0L5bytesS1798 = _M0L8_2afieldS2178;
                  int32_t _M0L5startS1800 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1799 = _M0L5startS1800 + 1;
                  int32_t _M0L6_2atmpS2177 =
                    _M0L5bytesS1798[_M0L6_2atmpS1799];
                  int32_t _M0L4_2axS574 = _M0L6_2atmpS2177;
                  if (_M0L4_2axS574 >= 160 && _M0L4_2axS574 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS2176 =
                      _M0L11_2aparam__1S496.$0;
                    moonbit_bytes_t _M0L5bytesS1795 = _M0L8_2afieldS2176;
                    int32_t _M0L5startS1797 = _M0L11_2aparam__1S496.$1;
                    int32_t _M0L6_2atmpS1796 = _M0L5startS1797 + 2;
                    int32_t _M0L6_2atmpS2175 =
                      _M0L5bytesS1795[_M0L6_2atmpS1796];
                    int32_t _M0L4_2axS575 = _M0L6_2atmpS2175;
                    if (_M0L4_2axS575 >= 128 && _M0L4_2axS575 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2174 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1791 = _M0L8_2afieldS2174;
                      int32_t _M0L5startS1794 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1792 = _M0L5startS1794 + 3;
                      int32_t _M0L8_2afieldS2173 = _M0L11_2aparam__1S496.$2;
                      int32_t _M0L3endS1793 = _M0L8_2afieldS2173;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS576 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1792,
                                                           _M0L3endS1793,
                                                           _M0L5bytesS1791};
                      _M0L4tlenS512 = _M0L11_2aparam__0S495;
                      _M0L2b0S513 = _M0L4_2axS570;
                      _M0L2b1S514 = _M0L4_2axS574;
                      _M0L2b2S515 = _M0L4_2axS575;
                      _M0L4restS516 = _M0L4_2axS576;
                      goto join_511;
                    } else {
                      int32_t _M0L3endS1789;
                      int32_t _M0L5startS1790;
                      int32_t _M0L6_2atmpS1788;
                      int32_t _M0L6_2atmpS1787;
                      moonbit_decref(_M0L1tS493);
                      _M0L3endS1789 = _M0L11_2aparam__1S496.$2;
                      _M0L5startS1790 = _M0L11_2aparam__1S496.$1;
                      _M0L6_2atmpS1788 = _M0L3endS1789 - _M0L5startS1790;
                      _M0L6_2atmpS1787 = _M0L6_2atmpS1788 >= 4;
                      _M0L5bytesS498 = _M0L11_2aparam__1S496;
                      goto join_497;
                    }
                  } else {
                    int32_t _M0L3endS1785;
                    int32_t _M0L5startS1786;
                    int32_t _M0L6_2atmpS1784;
                    int32_t _M0L6_2atmpS1783;
                    moonbit_decref(_M0L1tS493);
                    _M0L3endS1785 = _M0L11_2aparam__1S496.$2;
                    _M0L5startS1786 = _M0L11_2aparam__1S496.$1;
                    _M0L6_2atmpS1784 = _M0L3endS1785 - _M0L5startS1786;
                    _M0L6_2atmpS1783 = _M0L6_2atmpS1784 >= 4;
                    _M0L5bytesS498 = _M0L11_2aparam__1S496;
                    goto join_497;
                  }
                } else if (_M0L4_2axS570 >= 225 && _M0L4_2axS570 <= 236) {
                  moonbit_bytes_t _M0L8_2afieldS2184 =
                    _M0L11_2aparam__1S496.$0;
                  moonbit_bytes_t _M0L5bytesS1780 = _M0L8_2afieldS2184;
                  int32_t _M0L5startS1782 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1781 = _M0L5startS1782 + 1;
                  int32_t _M0L6_2atmpS2183 =
                    _M0L5bytesS1780[_M0L6_2atmpS1781];
                  int32_t _M0L4_2axS577 = _M0L6_2atmpS2183;
                  if (_M0L4_2axS577 >= 128 && _M0L4_2axS577 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS2182 =
                      _M0L11_2aparam__1S496.$0;
                    moonbit_bytes_t _M0L5bytesS1777 = _M0L8_2afieldS2182;
                    int32_t _M0L5startS1779 = _M0L11_2aparam__1S496.$1;
                    int32_t _M0L6_2atmpS1778 = _M0L5startS1779 + 2;
                    int32_t _M0L6_2atmpS2181 =
                      _M0L5bytesS1777[_M0L6_2atmpS1778];
                    int32_t _M0L4_2axS578 = _M0L6_2atmpS2181;
                    if (_M0L4_2axS578 >= 128 && _M0L4_2axS578 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2180 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1773 = _M0L8_2afieldS2180;
                      int32_t _M0L5startS1776 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1774 = _M0L5startS1776 + 3;
                      int32_t _M0L8_2afieldS2179 = _M0L11_2aparam__1S496.$2;
                      int32_t _M0L3endS1775 = _M0L8_2afieldS2179;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS579 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1774,
                                                           _M0L3endS1775,
                                                           _M0L5bytesS1773};
                      _M0L4tlenS512 = _M0L11_2aparam__0S495;
                      _M0L2b0S513 = _M0L4_2axS570;
                      _M0L2b1S514 = _M0L4_2axS577;
                      _M0L2b2S515 = _M0L4_2axS578;
                      _M0L4restS516 = _M0L4_2axS579;
                      goto join_511;
                    } else {
                      int32_t _M0L3endS1771;
                      int32_t _M0L5startS1772;
                      int32_t _M0L6_2atmpS1770;
                      int32_t _M0L6_2atmpS1769;
                      moonbit_decref(_M0L1tS493);
                      _M0L3endS1771 = _M0L11_2aparam__1S496.$2;
                      _M0L5startS1772 = _M0L11_2aparam__1S496.$1;
                      _M0L6_2atmpS1770 = _M0L3endS1771 - _M0L5startS1772;
                      _M0L6_2atmpS1769 = _M0L6_2atmpS1770 >= 4;
                      _M0L5bytesS498 = _M0L11_2aparam__1S496;
                      goto join_497;
                    }
                  } else {
                    int32_t _M0L3endS1767;
                    int32_t _M0L5startS1768;
                    int32_t _M0L6_2atmpS1766;
                    int32_t _M0L6_2atmpS1765;
                    moonbit_decref(_M0L1tS493);
                    _M0L3endS1767 = _M0L11_2aparam__1S496.$2;
                    _M0L5startS1768 = _M0L11_2aparam__1S496.$1;
                    _M0L6_2atmpS1766 = _M0L3endS1767 - _M0L5startS1768;
                    _M0L6_2atmpS1765 = _M0L6_2atmpS1766 >= 4;
                    _M0L5bytesS498 = _M0L11_2aparam__1S496;
                    goto join_497;
                  }
                } else if (_M0L4_2axS570 == 237) {
                  moonbit_bytes_t _M0L8_2afieldS2190 =
                    _M0L11_2aparam__1S496.$0;
                  moonbit_bytes_t _M0L5bytesS1762 = _M0L8_2afieldS2190;
                  int32_t _M0L5startS1764 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1763 = _M0L5startS1764 + 1;
                  int32_t _M0L6_2atmpS2189 =
                    _M0L5bytesS1762[_M0L6_2atmpS1763];
                  int32_t _M0L4_2axS580 = _M0L6_2atmpS2189;
                  if (_M0L4_2axS580 >= 128 && _M0L4_2axS580 <= 159) {
                    moonbit_bytes_t _M0L8_2afieldS2188 =
                      _M0L11_2aparam__1S496.$0;
                    moonbit_bytes_t _M0L5bytesS1759 = _M0L8_2afieldS2188;
                    int32_t _M0L5startS1761 = _M0L11_2aparam__1S496.$1;
                    int32_t _M0L6_2atmpS1760 = _M0L5startS1761 + 2;
                    int32_t _M0L6_2atmpS2187 =
                      _M0L5bytesS1759[_M0L6_2atmpS1760];
                    int32_t _M0L4_2axS581 = _M0L6_2atmpS2187;
                    if (_M0L4_2axS581 >= 128 && _M0L4_2axS581 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2186 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1755 = _M0L8_2afieldS2186;
                      int32_t _M0L5startS1758 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1756 = _M0L5startS1758 + 3;
                      int32_t _M0L8_2afieldS2185 = _M0L11_2aparam__1S496.$2;
                      int32_t _M0L3endS1757 = _M0L8_2afieldS2185;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS582 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1756,
                                                           _M0L3endS1757,
                                                           _M0L5bytesS1755};
                      _M0L4tlenS512 = _M0L11_2aparam__0S495;
                      _M0L2b0S513 = _M0L4_2axS570;
                      _M0L2b1S514 = _M0L4_2axS580;
                      _M0L2b2S515 = _M0L4_2axS581;
                      _M0L4restS516 = _M0L4_2axS582;
                      goto join_511;
                    } else {
                      int32_t _M0L3endS1753;
                      int32_t _M0L5startS1754;
                      int32_t _M0L6_2atmpS1752;
                      int32_t _M0L6_2atmpS1751;
                      moonbit_decref(_M0L1tS493);
                      _M0L3endS1753 = _M0L11_2aparam__1S496.$2;
                      _M0L5startS1754 = _M0L11_2aparam__1S496.$1;
                      _M0L6_2atmpS1752 = _M0L3endS1753 - _M0L5startS1754;
                      _M0L6_2atmpS1751 = _M0L6_2atmpS1752 >= 4;
                      _M0L5bytesS498 = _M0L11_2aparam__1S496;
                      goto join_497;
                    }
                  } else {
                    int32_t _M0L3endS1749;
                    int32_t _M0L5startS1750;
                    int32_t _M0L6_2atmpS1748;
                    int32_t _M0L6_2atmpS1747;
                    moonbit_decref(_M0L1tS493);
                    _M0L3endS1749 = _M0L11_2aparam__1S496.$2;
                    _M0L5startS1750 = _M0L11_2aparam__1S496.$1;
                    _M0L6_2atmpS1748 = _M0L3endS1749 - _M0L5startS1750;
                    _M0L6_2atmpS1747 = _M0L6_2atmpS1748 >= 4;
                    _M0L5bytesS498 = _M0L11_2aparam__1S496;
                    goto join_497;
                  }
                } else if (_M0L4_2axS570 >= 238 && _M0L4_2axS570 <= 239) {
                  moonbit_bytes_t _M0L8_2afieldS2196 =
                    _M0L11_2aparam__1S496.$0;
                  moonbit_bytes_t _M0L5bytesS1744 = _M0L8_2afieldS2196;
                  int32_t _M0L5startS1746 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1745 = _M0L5startS1746 + 1;
                  int32_t _M0L6_2atmpS2195 =
                    _M0L5bytesS1744[_M0L6_2atmpS1745];
                  int32_t _M0L4_2axS583 = _M0L6_2atmpS2195;
                  if (_M0L4_2axS583 >= 128 && _M0L4_2axS583 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS2194 =
                      _M0L11_2aparam__1S496.$0;
                    moonbit_bytes_t _M0L5bytesS1741 = _M0L8_2afieldS2194;
                    int32_t _M0L5startS1743 = _M0L11_2aparam__1S496.$1;
                    int32_t _M0L6_2atmpS1742 = _M0L5startS1743 + 2;
                    int32_t _M0L6_2atmpS2193 =
                      _M0L5bytesS1741[_M0L6_2atmpS1742];
                    int32_t _M0L4_2axS584 = _M0L6_2atmpS2193;
                    if (_M0L4_2axS584 >= 128 && _M0L4_2axS584 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2192 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1737 = _M0L8_2afieldS2192;
                      int32_t _M0L5startS1740 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1738 = _M0L5startS1740 + 3;
                      int32_t _M0L8_2afieldS2191 = _M0L11_2aparam__1S496.$2;
                      int32_t _M0L3endS1739 = _M0L8_2afieldS2191;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS585 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1738,
                                                           _M0L3endS1739,
                                                           _M0L5bytesS1737};
                      _M0L4tlenS512 = _M0L11_2aparam__0S495;
                      _M0L2b0S513 = _M0L4_2axS570;
                      _M0L2b1S514 = _M0L4_2axS583;
                      _M0L2b2S515 = _M0L4_2axS584;
                      _M0L4restS516 = _M0L4_2axS585;
                      goto join_511;
                    } else {
                      int32_t _M0L3endS1735;
                      int32_t _M0L5startS1736;
                      int32_t _M0L6_2atmpS1734;
                      int32_t _M0L6_2atmpS1733;
                      moonbit_decref(_M0L1tS493);
                      _M0L3endS1735 = _M0L11_2aparam__1S496.$2;
                      _M0L5startS1736 = _M0L11_2aparam__1S496.$1;
                      _M0L6_2atmpS1734 = _M0L3endS1735 - _M0L5startS1736;
                      _M0L6_2atmpS1733 = _M0L6_2atmpS1734 >= 4;
                      _M0L5bytesS498 = _M0L11_2aparam__1S496;
                      goto join_497;
                    }
                  } else {
                    int32_t _M0L3endS1731;
                    int32_t _M0L5startS1732;
                    int32_t _M0L6_2atmpS1730;
                    int32_t _M0L6_2atmpS1729;
                    moonbit_decref(_M0L1tS493);
                    _M0L3endS1731 = _M0L11_2aparam__1S496.$2;
                    _M0L5startS1732 = _M0L11_2aparam__1S496.$1;
                    _M0L6_2atmpS1730 = _M0L3endS1731 - _M0L5startS1732;
                    _M0L6_2atmpS1729 = _M0L6_2atmpS1730 >= 4;
                    _M0L5bytesS498 = _M0L11_2aparam__1S496;
                    goto join_497;
                  }
                } else {
                  int32_t _M0L3endS1688 = _M0L11_2aparam__1S496.$2;
                  int32_t _M0L5startS1689 = _M0L11_2aparam__1S496.$1;
                  int32_t _M0L6_2atmpS1687 = _M0L3endS1688 - _M0L5startS1689;
                  if (_M0L6_2atmpS1687 >= 4) {
                    if (_M0L4_2axS570 == 240) {
                      moonbit_bytes_t _M0L8_2afieldS2204 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1726 = _M0L8_2afieldS2204;
                      int32_t _M0L5startS1728 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1727 = _M0L5startS1728 + 1;
                      int32_t _M0L6_2atmpS2203 =
                        _M0L5bytesS1726[_M0L6_2atmpS1727];
                      int32_t _M0L4_2axS586 = _M0L6_2atmpS2203;
                      if (_M0L4_2axS586 >= 144 && _M0L4_2axS586 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS2202 =
                          _M0L11_2aparam__1S496.$0;
                        moonbit_bytes_t _M0L5bytesS1723 = _M0L8_2afieldS2202;
                        int32_t _M0L5startS1725 = _M0L11_2aparam__1S496.$1;
                        int32_t _M0L6_2atmpS1724 = _M0L5startS1725 + 2;
                        int32_t _M0L6_2atmpS2201 =
                          _M0L5bytesS1723[_M0L6_2atmpS1724];
                        int32_t _M0L4_2axS587 = _M0L6_2atmpS2201;
                        if (_M0L4_2axS587 >= 128 && _M0L4_2axS587 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS2200 =
                            _M0L11_2aparam__1S496.$0;
                          moonbit_bytes_t _M0L5bytesS1720 =
                            _M0L8_2afieldS2200;
                          int32_t _M0L5startS1722 = _M0L11_2aparam__1S496.$1;
                          int32_t _M0L6_2atmpS1721 = _M0L5startS1722 + 3;
                          int32_t _M0L6_2atmpS2199 =
                            _M0L5bytesS1720[_M0L6_2atmpS1721];
                          int32_t _M0L4_2axS588 = _M0L6_2atmpS2199;
                          if (_M0L4_2axS588 >= 128 && _M0L4_2axS588 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS2198 =
                              _M0L11_2aparam__1S496.$0;
                            moonbit_bytes_t _M0L5bytesS1716 =
                              _M0L8_2afieldS2198;
                            int32_t _M0L5startS1719 =
                              _M0L11_2aparam__1S496.$1;
                            int32_t _M0L6_2atmpS1717 = _M0L5startS1719 + 4;
                            int32_t _M0L8_2afieldS2197 =
                              _M0L11_2aparam__1S496.$2;
                            int32_t _M0L3endS1718 = _M0L8_2afieldS2197;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS589 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1717,
                                                                 _M0L3endS1718,
                                                                 _M0L5bytesS1716};
                            _M0L4tlenS500 = _M0L11_2aparam__0S495;
                            _M0L2b0S501 = _M0L4_2axS570;
                            _M0L2b1S502 = _M0L4_2axS586;
                            _M0L2b2S503 = _M0L4_2axS587;
                            _M0L2b3S504 = _M0L4_2axS588;
                            _M0L4restS505 = _M0L4_2axS589;
                            goto join_499;
                          } else {
                            moonbit_decref(_M0L1tS493);
                            _M0L5bytesS498 = _M0L11_2aparam__1S496;
                            goto join_497;
                          }
                        } else {
                          moonbit_decref(_M0L1tS493);
                          _M0L5bytesS498 = _M0L11_2aparam__1S496;
                          goto join_497;
                        }
                      } else {
                        moonbit_decref(_M0L1tS493);
                        _M0L5bytesS498 = _M0L11_2aparam__1S496;
                        goto join_497;
                      }
                    } else if (_M0L4_2axS570 >= 241 && _M0L4_2axS570 <= 243) {
                      moonbit_bytes_t _M0L8_2afieldS2212 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1713 = _M0L8_2afieldS2212;
                      int32_t _M0L5startS1715 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1714 = _M0L5startS1715 + 1;
                      int32_t _M0L6_2atmpS2211 =
                        _M0L5bytesS1713[_M0L6_2atmpS1714];
                      int32_t _M0L4_2axS590 = _M0L6_2atmpS2211;
                      if (_M0L4_2axS590 >= 128 && _M0L4_2axS590 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS2210 =
                          _M0L11_2aparam__1S496.$0;
                        moonbit_bytes_t _M0L5bytesS1710 = _M0L8_2afieldS2210;
                        int32_t _M0L5startS1712 = _M0L11_2aparam__1S496.$1;
                        int32_t _M0L6_2atmpS1711 = _M0L5startS1712 + 2;
                        int32_t _M0L6_2atmpS2209 =
                          _M0L5bytesS1710[_M0L6_2atmpS1711];
                        int32_t _M0L4_2axS591 = _M0L6_2atmpS2209;
                        if (_M0L4_2axS591 >= 128 && _M0L4_2axS591 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS2208 =
                            _M0L11_2aparam__1S496.$0;
                          moonbit_bytes_t _M0L5bytesS1707 =
                            _M0L8_2afieldS2208;
                          int32_t _M0L5startS1709 = _M0L11_2aparam__1S496.$1;
                          int32_t _M0L6_2atmpS1708 = _M0L5startS1709 + 3;
                          int32_t _M0L6_2atmpS2207 =
                            _M0L5bytesS1707[_M0L6_2atmpS1708];
                          int32_t _M0L4_2axS592 = _M0L6_2atmpS2207;
                          if (_M0L4_2axS592 >= 128 && _M0L4_2axS592 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS2206 =
                              _M0L11_2aparam__1S496.$0;
                            moonbit_bytes_t _M0L5bytesS1703 =
                              _M0L8_2afieldS2206;
                            int32_t _M0L5startS1706 =
                              _M0L11_2aparam__1S496.$1;
                            int32_t _M0L6_2atmpS1704 = _M0L5startS1706 + 4;
                            int32_t _M0L8_2afieldS2205 =
                              _M0L11_2aparam__1S496.$2;
                            int32_t _M0L3endS1705 = _M0L8_2afieldS2205;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS593 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1704,
                                                                 _M0L3endS1705,
                                                                 _M0L5bytesS1703};
                            _M0L4tlenS500 = _M0L11_2aparam__0S495;
                            _M0L2b0S501 = _M0L4_2axS570;
                            _M0L2b1S502 = _M0L4_2axS590;
                            _M0L2b2S503 = _M0L4_2axS591;
                            _M0L2b3S504 = _M0L4_2axS592;
                            _M0L4restS505 = _M0L4_2axS593;
                            goto join_499;
                          } else {
                            moonbit_decref(_M0L1tS493);
                            _M0L5bytesS498 = _M0L11_2aparam__1S496;
                            goto join_497;
                          }
                        } else {
                          moonbit_decref(_M0L1tS493);
                          _M0L5bytesS498 = _M0L11_2aparam__1S496;
                          goto join_497;
                        }
                      } else {
                        moonbit_decref(_M0L1tS493);
                        _M0L5bytesS498 = _M0L11_2aparam__1S496;
                        goto join_497;
                      }
                    } else if (_M0L4_2axS570 == 244) {
                      moonbit_bytes_t _M0L8_2afieldS2220 =
                        _M0L11_2aparam__1S496.$0;
                      moonbit_bytes_t _M0L5bytesS1700 = _M0L8_2afieldS2220;
                      int32_t _M0L5startS1702 = _M0L11_2aparam__1S496.$1;
                      int32_t _M0L6_2atmpS1701 = _M0L5startS1702 + 1;
                      int32_t _M0L6_2atmpS2219 =
                        _M0L5bytesS1700[_M0L6_2atmpS1701];
                      int32_t _M0L4_2axS594 = _M0L6_2atmpS2219;
                      if (_M0L4_2axS594 >= 128 && _M0L4_2axS594 <= 143) {
                        moonbit_bytes_t _M0L8_2afieldS2218 =
                          _M0L11_2aparam__1S496.$0;
                        moonbit_bytes_t _M0L5bytesS1697 = _M0L8_2afieldS2218;
                        int32_t _M0L5startS1699 = _M0L11_2aparam__1S496.$1;
                        int32_t _M0L6_2atmpS1698 = _M0L5startS1699 + 2;
                        int32_t _M0L6_2atmpS2217 =
                          _M0L5bytesS1697[_M0L6_2atmpS1698];
                        int32_t _M0L4_2axS595 = _M0L6_2atmpS2217;
                        if (_M0L4_2axS595 >= 128 && _M0L4_2axS595 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS2216 =
                            _M0L11_2aparam__1S496.$0;
                          moonbit_bytes_t _M0L5bytesS1694 =
                            _M0L8_2afieldS2216;
                          int32_t _M0L5startS1696 = _M0L11_2aparam__1S496.$1;
                          int32_t _M0L6_2atmpS1695 = _M0L5startS1696 + 3;
                          int32_t _M0L6_2atmpS2215 =
                            _M0L5bytesS1694[_M0L6_2atmpS1695];
                          int32_t _M0L4_2axS596 = _M0L6_2atmpS2215;
                          if (_M0L4_2axS596 >= 128 && _M0L4_2axS596 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS2214 =
                              _M0L11_2aparam__1S496.$0;
                            moonbit_bytes_t _M0L5bytesS1690 =
                              _M0L8_2afieldS2214;
                            int32_t _M0L5startS1693 =
                              _M0L11_2aparam__1S496.$1;
                            int32_t _M0L6_2atmpS1691 = _M0L5startS1693 + 4;
                            int32_t _M0L8_2afieldS2213 =
                              _M0L11_2aparam__1S496.$2;
                            int32_t _M0L3endS1692 = _M0L8_2afieldS2213;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS597 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1691,
                                                                 _M0L3endS1692,
                                                                 _M0L5bytesS1690};
                            _M0L4tlenS500 = _M0L11_2aparam__0S495;
                            _M0L2b0S501 = _M0L4_2axS570;
                            _M0L2b1S502 = _M0L4_2axS594;
                            _M0L2b2S503 = _M0L4_2axS595;
                            _M0L2b3S504 = _M0L4_2axS596;
                            _M0L4restS505 = _M0L4_2axS597;
                            goto join_499;
                          } else {
                            moonbit_decref(_M0L1tS493);
                            _M0L5bytesS498 = _M0L11_2aparam__1S496;
                            goto join_497;
                          }
                        } else {
                          moonbit_decref(_M0L1tS493);
                          _M0L5bytesS498 = _M0L11_2aparam__1S496;
                          goto join_497;
                        }
                      } else {
                        moonbit_decref(_M0L1tS493);
                        _M0L5bytesS498 = _M0L11_2aparam__1S496;
                        goto join_497;
                      }
                    } else {
                      moonbit_decref(_M0L1tS493);
                      _M0L5bytesS498 = _M0L11_2aparam__1S496;
                      goto join_497;
                    }
                  } else {
                    moonbit_decref(_M0L1tS493);
                    _M0L5bytesS498 = _M0L11_2aparam__1S496;
                    goto join_497;
                  }
                }
              } else {
                moonbit_decref(_M0L1tS493);
                _M0L5bytesS498 = _M0L11_2aparam__1S496;
                goto join_497;
              }
            }
          } else {
            moonbit_decref(_M0L1tS493);
            _M0L5bytesS498 = _M0L11_2aparam__1S496;
            goto join_497;
          }
        }
      }
    }
    goto joinlet_2450;
    join_524:;
    _M0L1tS493[_M0L4tlenS525] = _M0L1bS527;
    _M0L6_2atmpS1525 = _M0L4tlenS525 + 2;
    _M0L11_2aparam__0S495 = _M0L6_2atmpS1525;
    _M0L11_2aparam__1S496 = _M0L4restS526;
    continue;
    joinlet_2450:;
    goto joinlet_2449;
    join_518:;
    _M0L6_2atmpS1524 = (int32_t)_M0L2b0S521;
    _M0L6_2atmpS1523 = _M0L6_2atmpS1524 & 31;
    _M0L6_2atmpS1520 = _M0L6_2atmpS1523 << 6;
    _M0L6_2atmpS1522 = (int32_t)_M0L2b1S522;
    _M0L6_2atmpS1521 = _M0L6_2atmpS1522 & 63;
    _M0L2chS523 = _M0L6_2atmpS1520 | _M0L6_2atmpS1521;
    _M0L6_2atmpS1515 = _M0L2chS523 & 0xff;
    _M0L1tS493[_M0L4tlenS519] = _M0L6_2atmpS1515;
    _M0L6_2atmpS1516 = _M0L4tlenS519 + 1;
    _M0L6_2atmpS1518 = _M0L2chS523 >> 8;
    _M0L6_2atmpS1517 = _M0L6_2atmpS1518 & 0xff;
    _M0L1tS493[_M0L6_2atmpS1516] = _M0L6_2atmpS1517;
    _M0L6_2atmpS1519 = _M0L4tlenS519 + 2;
    _M0L11_2aparam__0S495 = _M0L6_2atmpS1519;
    _M0L11_2aparam__1S496 = _M0L4restS520;
    continue;
    joinlet_2449:;
    goto joinlet_2448;
    join_511:;
    _M0L6_2atmpS1514 = (int32_t)_M0L2b0S513;
    _M0L6_2atmpS1513 = _M0L6_2atmpS1514 & 15;
    _M0L6_2atmpS1509 = _M0L6_2atmpS1513 << 12;
    _M0L6_2atmpS1512 = (int32_t)_M0L2b1S514;
    _M0L6_2atmpS1511 = _M0L6_2atmpS1512 & 63;
    _M0L6_2atmpS1510 = _M0L6_2atmpS1511 << 6;
    _M0L6_2atmpS1506 = _M0L6_2atmpS1509 | _M0L6_2atmpS1510;
    _M0L6_2atmpS1508 = (int32_t)_M0L2b2S515;
    _M0L6_2atmpS1507 = _M0L6_2atmpS1508 & 63;
    _M0L2chS517 = _M0L6_2atmpS1506 | _M0L6_2atmpS1507;
    _M0L6_2atmpS1501 = _M0L2chS517 & 0xff;
    _M0L1tS493[_M0L4tlenS512] = _M0L6_2atmpS1501;
    _M0L6_2atmpS1502 = _M0L4tlenS512 + 1;
    _M0L6_2atmpS1504 = _M0L2chS517 >> 8;
    _M0L6_2atmpS1503 = _M0L6_2atmpS1504 & 0xff;
    _M0L1tS493[_M0L6_2atmpS1502] = _M0L6_2atmpS1503;
    _M0L6_2atmpS1505 = _M0L4tlenS512 + 2;
    _M0L11_2aparam__0S495 = _M0L6_2atmpS1505;
    _M0L11_2aparam__1S496 = _M0L4restS516;
    continue;
    joinlet_2448:;
    goto joinlet_2447;
    join_499:;
    _M0L6_2atmpS1500 = (int32_t)_M0L2b0S501;
    _M0L6_2atmpS1499 = _M0L6_2atmpS1500 & 7;
    _M0L6_2atmpS1495 = _M0L6_2atmpS1499 << 18;
    _M0L6_2atmpS1498 = (int32_t)_M0L2b1S502;
    _M0L6_2atmpS1497 = _M0L6_2atmpS1498 & 63;
    _M0L6_2atmpS1496 = _M0L6_2atmpS1497 << 12;
    _M0L6_2atmpS1491 = _M0L6_2atmpS1495 | _M0L6_2atmpS1496;
    _M0L6_2atmpS1494 = (int32_t)_M0L2b2S503;
    _M0L6_2atmpS1493 = _M0L6_2atmpS1494 & 63;
    _M0L6_2atmpS1492 = _M0L6_2atmpS1493 << 6;
    _M0L6_2atmpS1488 = _M0L6_2atmpS1491 | _M0L6_2atmpS1492;
    _M0L6_2atmpS1490 = (int32_t)_M0L2b3S504;
    _M0L6_2atmpS1489 = _M0L6_2atmpS1490 & 63;
    _M0L2chS506 = _M0L6_2atmpS1488 | _M0L6_2atmpS1489;
    _M0L3chmS507 = _M0L2chS506 - 65536;
    _M0L6_2atmpS1487 = _M0L3chmS507 >> 10;
    _M0L3ch1S508 = _M0L6_2atmpS1487 + 55296;
    _M0L6_2atmpS1486 = _M0L3chmS507 & 1023;
    _M0L3ch2S509 = _M0L6_2atmpS1486 + 56320;
    _M0L6_2atmpS1476 = _M0L3ch1S508 & 0xff;
    _M0L1tS493[_M0L4tlenS500] = _M0L6_2atmpS1476;
    _M0L6_2atmpS1477 = _M0L4tlenS500 + 1;
    _M0L6_2atmpS1479 = _M0L3ch1S508 >> 8;
    _M0L6_2atmpS1478 = _M0L6_2atmpS1479 & 0xff;
    _M0L1tS493[_M0L6_2atmpS1477] = _M0L6_2atmpS1478;
    _M0L6_2atmpS1480 = _M0L4tlenS500 + 2;
    _M0L6_2atmpS1481 = _M0L3ch2S509 & 0xff;
    _M0L1tS493[_M0L6_2atmpS1480] = _M0L6_2atmpS1481;
    _M0L6_2atmpS1482 = _M0L4tlenS500 + 3;
    _M0L6_2atmpS1484 = _M0L3ch2S509 >> 8;
    _M0L6_2atmpS1483 = _M0L6_2atmpS1484 & 0xff;
    _M0L1tS493[_M0L6_2atmpS1482] = _M0L6_2atmpS1483;
    _M0L6_2atmpS1485 = _M0L4tlenS500 + 4;
    _M0L11_2aparam__0S495 = _M0L6_2atmpS1485;
    _M0L11_2aparam__1S496 = _M0L4restS505;
    continue;
    joinlet_2447:;
    goto joinlet_2446;
    join_497:;
    _M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed));
    Moonbit_object_header(_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed*)_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475)->$0_0
    = _M0L5bytesS498.$0;
    ((struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed*)_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475)->$0_1
    = _M0L5bytesS498.$1;
    ((struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed*)_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475)->$0_2
    = _M0L5bytesS498.$2;
    _result_2451.tag = 0;
    _result_2451.data.err
    = _M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1475;
    return _result_2451;
    joinlet_2446:;
    break;
  }
  _M0L6_2atmpS1473 = _M0L1tS493;
  _M0L6_2atmpS1474 = (int64_t)_M0L4tlenS494;
  #line 122 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS1472
  = _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1473, 0, _M0L6_2atmpS1474);
  _result_2452.tag = 1;
  _result_2452.data.ok = _M0L6_2atmpS1472;
  return _result_2452;
}

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView _M0L3strS483,
  int32_t _M0L3bomS484
) {
  int32_t _M0L6_2atmpS1471;
  int32_t _M0L6_2atmpS1470;
  struct _M0TPC16buffer6Buffer* _M0L6bufferS482;
  #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  moonbit_incref(_M0L3strS483.$0);
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0L6_2atmpS1471 = _M0MPC16string10StringView6length(_M0L3strS483);
  _M0L6_2atmpS1470 = _M0L6_2atmpS1471 * 4;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0L6bufferS482 = _M0FPC16buffer11new_2einner(_M0L6_2atmpS1470);
  if (_M0L3bomS484 == 1) {
    moonbit_incref(_M0L6bufferS482);
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
    _M0MPC16buffer6Buffer17write__char__utf8(_M0L6bufferS482, 65279);
  }
  moonbit_incref(_M0L6bufferS482);
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0MPC16buffer6Buffer19write__string__utf8(_M0L6bufferS482, _M0L3strS483);
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  return _M0MPC16buffer6Buffer9to__bytes(_M0L6bufferS482);
}

struct _M0TPB5ArrayGsE* _M0FPC13env4args() {
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env.mbt"
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env.mbt"
  return _M0FPC13env24get__cli__args__internal();
}

struct _M0TPB5ArrayGsE* _M0FPC13env24get__cli__args__internal() {
  moonbit_bytes_t* _M0L3tmpS477;
  int32_t _M0L6_2atmpS1469;
  struct _M0TPB5ArrayGsE* _M0L3resS478;
  int32_t _M0L7_2abindS479;
  int32_t _M0L1iS480;
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  _M0L3tmpS477 = _M0FPC13env19get__cli__args__ffi();
  _M0L6_2atmpS1469 = Moonbit_array_length(_M0L3tmpS477);
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  _M0L3resS478 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1469);
  _M0L7_2abindS479 = Moonbit_array_length(_M0L3tmpS477);
  _M0L1iS480 = 0;
  while (1) {
    if (_M0L1iS480 < _M0L7_2abindS479) {
      moonbit_bytes_t _M0L6_2atmpS2231;
      moonbit_bytes_t _M0L6_2atmpS1467;
      moonbit_string_t _M0L6_2atmpS1466;
      int32_t _M0L6_2atmpS1468;
      if (_M0L1iS480 < 0 || _M0L1iS480 >= Moonbit_array_length(_M0L3tmpS477)) {
        #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2231 = (moonbit_bytes_t)_M0L3tmpS477[_M0L1iS480];
      _M0L6_2atmpS1467 = _M0L6_2atmpS2231;
      moonbit_incref(_M0L6_2atmpS1467);
      #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
      _M0L6_2atmpS1466
      = _M0FPC13env28utf8__bytes__to__mbt__string(_M0L6_2atmpS1467);
      moonbit_incref(_M0L3resS478);
      #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS478, _M0L6_2atmpS1466);
      _M0L6_2atmpS1468 = _M0L1iS480 + 1;
      _M0L1iS480 = _M0L6_2atmpS1468;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS477);
    }
    break;
  }
  return _M0L3resS478;
}

moonbit_string_t _M0FPC13env28utf8__bytes__to__mbt__string(
  moonbit_bytes_t _M0L5bytesS473
) {
  struct _M0TPB13StringBuilder* _M0L3resS471;
  int32_t _M0L3lenS472;
  int32_t _M0Lm1iS474;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  _M0L3resS471 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS472 = Moonbit_array_length(_M0L5bytesS473);
  _M0Lm1iS474 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1389 = _M0Lm1iS474;
    if (_M0L6_2atmpS1389 < _M0L3lenS472) {
      int32_t _M0L6_2atmpS1465 = _M0Lm1iS474;
      int32_t _M0L6_2atmpS1464;
      int32_t _M0Lm1cS475;
      int32_t _M0L6_2atmpS1390;
      if (
        _M0L6_2atmpS1465 < 0
        || _M0L6_2atmpS1465 >= Moonbit_array_length(_M0L5bytesS473)
      ) {
        #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1464 = _M0L5bytesS473[_M0L6_2atmpS1465];
      _M0Lm1cS475 = (int32_t)_M0L6_2atmpS1464;
      _M0L6_2atmpS1390 = _M0Lm1cS475;
      if (_M0L6_2atmpS1390 == 0) {
        moonbit_decref(_M0L5bytesS473);
        break;
      } else {
        int32_t _M0L6_2atmpS1391 = _M0Lm1cS475;
        if (_M0L6_2atmpS1391 < 128) {
          int32_t _M0L6_2atmpS1393 = _M0Lm1cS475;
          int32_t _M0L6_2atmpS1392 = _M0L6_2atmpS1393;
          int32_t _M0L6_2atmpS1394;
          moonbit_incref(_M0L3resS471);
          #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS471, _M0L6_2atmpS1392);
          _M0L6_2atmpS1394 = _M0Lm1iS474;
          _M0Lm1iS474 = _M0L6_2atmpS1394 + 1;
        } else {
          int32_t _M0L6_2atmpS1395 = _M0Lm1cS475;
          if (_M0L6_2atmpS1395 < 224) {
            int32_t _M0L6_2atmpS1397 = _M0Lm1iS474;
            int32_t _M0L6_2atmpS1396 = _M0L6_2atmpS1397 + 1;
            int32_t _M0L6_2atmpS1405;
            int32_t _M0L6_2atmpS1404;
            int32_t _M0L6_2atmpS1398;
            int32_t _M0L6_2atmpS1403;
            int32_t _M0L6_2atmpS1402;
            int32_t _M0L6_2atmpS1401;
            int32_t _M0L6_2atmpS1400;
            int32_t _M0L6_2atmpS1399;
            int32_t _M0L6_2atmpS1407;
            int32_t _M0L6_2atmpS1406;
            int32_t _M0L6_2atmpS1408;
            if (_M0L6_2atmpS1396 >= _M0L3lenS472) {
              moonbit_decref(_M0L5bytesS473);
              break;
            }
            _M0L6_2atmpS1405 = _M0Lm1cS475;
            _M0L6_2atmpS1404 = _M0L6_2atmpS1405 & 31;
            _M0L6_2atmpS1398 = _M0L6_2atmpS1404 << 6;
            _M0L6_2atmpS1403 = _M0Lm1iS474;
            _M0L6_2atmpS1402 = _M0L6_2atmpS1403 + 1;
            if (
              _M0L6_2atmpS1402 < 0
              || _M0L6_2atmpS1402 >= Moonbit_array_length(_M0L5bytesS473)
            ) {
              #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1401 = _M0L5bytesS473[_M0L6_2atmpS1402];
            _M0L6_2atmpS1400 = (int32_t)_M0L6_2atmpS1401;
            _M0L6_2atmpS1399 = _M0L6_2atmpS1400 & 63;
            _M0Lm1cS475 = _M0L6_2atmpS1398 | _M0L6_2atmpS1399;
            _M0L6_2atmpS1407 = _M0Lm1cS475;
            _M0L6_2atmpS1406 = _M0L6_2atmpS1407;
            moonbit_incref(_M0L3resS471);
            #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS471, _M0L6_2atmpS1406);
            _M0L6_2atmpS1408 = _M0Lm1iS474;
            _M0Lm1iS474 = _M0L6_2atmpS1408 + 2;
          } else {
            int32_t _M0L6_2atmpS1409 = _M0Lm1cS475;
            if (_M0L6_2atmpS1409 < 240) {
              int32_t _M0L6_2atmpS1411 = _M0Lm1iS474;
              int32_t _M0L6_2atmpS1410 = _M0L6_2atmpS1411 + 2;
              int32_t _M0L6_2atmpS1426;
              int32_t _M0L6_2atmpS1425;
              int32_t _M0L6_2atmpS1418;
              int32_t _M0L6_2atmpS1424;
              int32_t _M0L6_2atmpS1423;
              int32_t _M0L6_2atmpS1422;
              int32_t _M0L6_2atmpS1421;
              int32_t _M0L6_2atmpS1420;
              int32_t _M0L6_2atmpS1419;
              int32_t _M0L6_2atmpS1412;
              int32_t _M0L6_2atmpS1417;
              int32_t _M0L6_2atmpS1416;
              int32_t _M0L6_2atmpS1415;
              int32_t _M0L6_2atmpS1414;
              int32_t _M0L6_2atmpS1413;
              int32_t _M0L6_2atmpS1428;
              int32_t _M0L6_2atmpS1427;
              int32_t _M0L6_2atmpS1429;
              if (_M0L6_2atmpS1410 >= _M0L3lenS472) {
                moonbit_decref(_M0L5bytesS473);
                break;
              }
              _M0L6_2atmpS1426 = _M0Lm1cS475;
              _M0L6_2atmpS1425 = _M0L6_2atmpS1426 & 15;
              _M0L6_2atmpS1418 = _M0L6_2atmpS1425 << 12;
              _M0L6_2atmpS1424 = _M0Lm1iS474;
              _M0L6_2atmpS1423 = _M0L6_2atmpS1424 + 1;
              if (
                _M0L6_2atmpS1423 < 0
                || _M0L6_2atmpS1423 >= Moonbit_array_length(_M0L5bytesS473)
              ) {
                #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1422 = _M0L5bytesS473[_M0L6_2atmpS1423];
              _M0L6_2atmpS1421 = (int32_t)_M0L6_2atmpS1422;
              _M0L6_2atmpS1420 = _M0L6_2atmpS1421 & 63;
              _M0L6_2atmpS1419 = _M0L6_2atmpS1420 << 6;
              _M0L6_2atmpS1412 = _M0L6_2atmpS1418 | _M0L6_2atmpS1419;
              _M0L6_2atmpS1417 = _M0Lm1iS474;
              _M0L6_2atmpS1416 = _M0L6_2atmpS1417 + 2;
              if (
                _M0L6_2atmpS1416 < 0
                || _M0L6_2atmpS1416 >= Moonbit_array_length(_M0L5bytesS473)
              ) {
                #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1415 = _M0L5bytesS473[_M0L6_2atmpS1416];
              _M0L6_2atmpS1414 = (int32_t)_M0L6_2atmpS1415;
              _M0L6_2atmpS1413 = _M0L6_2atmpS1414 & 63;
              _M0Lm1cS475 = _M0L6_2atmpS1412 | _M0L6_2atmpS1413;
              _M0L6_2atmpS1428 = _M0Lm1cS475;
              _M0L6_2atmpS1427 = _M0L6_2atmpS1428;
              moonbit_incref(_M0L3resS471);
              #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS471, _M0L6_2atmpS1427);
              _M0L6_2atmpS1429 = _M0Lm1iS474;
              _M0Lm1iS474 = _M0L6_2atmpS1429 + 3;
            } else {
              int32_t _M0L6_2atmpS1431 = _M0Lm1iS474;
              int32_t _M0L6_2atmpS1430 = _M0L6_2atmpS1431 + 3;
              int32_t _M0L6_2atmpS1453;
              int32_t _M0L6_2atmpS1452;
              int32_t _M0L6_2atmpS1445;
              int32_t _M0L6_2atmpS1451;
              int32_t _M0L6_2atmpS1450;
              int32_t _M0L6_2atmpS1449;
              int32_t _M0L6_2atmpS1448;
              int32_t _M0L6_2atmpS1447;
              int32_t _M0L6_2atmpS1446;
              int32_t _M0L6_2atmpS1438;
              int32_t _M0L6_2atmpS1444;
              int32_t _M0L6_2atmpS1443;
              int32_t _M0L6_2atmpS1442;
              int32_t _M0L6_2atmpS1441;
              int32_t _M0L6_2atmpS1440;
              int32_t _M0L6_2atmpS1439;
              int32_t _M0L6_2atmpS1432;
              int32_t _M0L6_2atmpS1437;
              int32_t _M0L6_2atmpS1436;
              int32_t _M0L6_2atmpS1435;
              int32_t _M0L6_2atmpS1434;
              int32_t _M0L6_2atmpS1433;
              int32_t _M0L6_2atmpS1454;
              int32_t _M0L6_2atmpS1458;
              int32_t _M0L6_2atmpS1457;
              int32_t _M0L6_2atmpS1456;
              int32_t _M0L6_2atmpS1455;
              int32_t _M0L6_2atmpS1462;
              int32_t _M0L6_2atmpS1461;
              int32_t _M0L6_2atmpS1460;
              int32_t _M0L6_2atmpS1459;
              int32_t _M0L6_2atmpS1463;
              if (_M0L6_2atmpS1430 >= _M0L3lenS472) {
                moonbit_decref(_M0L5bytesS473);
                break;
              }
              _M0L6_2atmpS1453 = _M0Lm1cS475;
              _M0L6_2atmpS1452 = _M0L6_2atmpS1453 & 7;
              _M0L6_2atmpS1445 = _M0L6_2atmpS1452 << 18;
              _M0L6_2atmpS1451 = _M0Lm1iS474;
              _M0L6_2atmpS1450 = _M0L6_2atmpS1451 + 1;
              if (
                _M0L6_2atmpS1450 < 0
                || _M0L6_2atmpS1450 >= Moonbit_array_length(_M0L5bytesS473)
              ) {
                #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1449 = _M0L5bytesS473[_M0L6_2atmpS1450];
              _M0L6_2atmpS1448 = (int32_t)_M0L6_2atmpS1449;
              _M0L6_2atmpS1447 = _M0L6_2atmpS1448 & 63;
              _M0L6_2atmpS1446 = _M0L6_2atmpS1447 << 12;
              _M0L6_2atmpS1438 = _M0L6_2atmpS1445 | _M0L6_2atmpS1446;
              _M0L6_2atmpS1444 = _M0Lm1iS474;
              _M0L6_2atmpS1443 = _M0L6_2atmpS1444 + 2;
              if (
                _M0L6_2atmpS1443 < 0
                || _M0L6_2atmpS1443 >= Moonbit_array_length(_M0L5bytesS473)
              ) {
                #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1442 = _M0L5bytesS473[_M0L6_2atmpS1443];
              _M0L6_2atmpS1441 = (int32_t)_M0L6_2atmpS1442;
              _M0L6_2atmpS1440 = _M0L6_2atmpS1441 & 63;
              _M0L6_2atmpS1439 = _M0L6_2atmpS1440 << 6;
              _M0L6_2atmpS1432 = _M0L6_2atmpS1438 | _M0L6_2atmpS1439;
              _M0L6_2atmpS1437 = _M0Lm1iS474;
              _M0L6_2atmpS1436 = _M0L6_2atmpS1437 + 3;
              if (
                _M0L6_2atmpS1436 < 0
                || _M0L6_2atmpS1436 >= Moonbit_array_length(_M0L5bytesS473)
              ) {
                #line 64 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1435 = _M0L5bytesS473[_M0L6_2atmpS1436];
              _M0L6_2atmpS1434 = (int32_t)_M0L6_2atmpS1435;
              _M0L6_2atmpS1433 = _M0L6_2atmpS1434 & 63;
              _M0Lm1cS475 = _M0L6_2atmpS1432 | _M0L6_2atmpS1433;
              _M0L6_2atmpS1454 = _M0Lm1cS475;
              _M0Lm1cS475 = _M0L6_2atmpS1454 - 65536;
              _M0L6_2atmpS1458 = _M0Lm1cS475;
              _M0L6_2atmpS1457 = _M0L6_2atmpS1458 >> 10;
              _M0L6_2atmpS1456 = _M0L6_2atmpS1457 + 55296;
              _M0L6_2atmpS1455 = _M0L6_2atmpS1456;
              moonbit_incref(_M0L3resS471);
              #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS471, _M0L6_2atmpS1455);
              _M0L6_2atmpS1462 = _M0Lm1cS475;
              _M0L6_2atmpS1461 = _M0L6_2atmpS1462 & 1023;
              _M0L6_2atmpS1460 = _M0L6_2atmpS1461 + 56320;
              _M0L6_2atmpS1459 = _M0L6_2atmpS1460;
              moonbit_incref(_M0L3resS471);
              #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS471, _M0L6_2atmpS1459);
              _M0L6_2atmpS1463 = _M0Lm1iS474;
              _M0Lm1iS474 = _M0L6_2atmpS1463 + 4;
            }
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L5bytesS473);
    }
    break;
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS471);
}

moonbit_bytes_t _M0MPC16buffer6Buffer9to__bytes(
  struct _M0TPC16buffer6Buffer* _M0L4selfS470
) {
  moonbit_bytes_t _M0L8_2afieldS2233;
  moonbit_bytes_t _M0L4dataS1386;
  int32_t _M0L8_2afieldS2232;
  int32_t _M0L6_2acntS2369;
  int32_t _M0L3lenS1388;
  int64_t _M0L6_2atmpS1387;
  struct _M0TPB9ArrayViewGyE _M0L6_2atmpS1385;
  #line 1112 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS2233 = _M0L4selfS470->$0;
  _M0L4dataS1386 = _M0L8_2afieldS2233;
  _M0L8_2afieldS2232 = _M0L4selfS470->$1;
  _M0L6_2acntS2369 = Moonbit_object_header(_M0L4selfS470)->rc;
  if (_M0L6_2acntS2369 > 1) {
    int32_t _M0L11_2anew__cntS2370 = _M0L6_2acntS2369 - 1;
    Moonbit_object_header(_M0L4selfS470)->rc = _M0L11_2anew__cntS2370;
    moonbit_incref(_M0L4dataS1386);
  } else if (_M0L6_2acntS2369 == 1) {
    #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    moonbit_free(_M0L4selfS470);
  }
  _M0L3lenS1388 = _M0L8_2afieldS2232;
  _M0L6_2atmpS1387 = (int64_t)_M0L3lenS1388;
  #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS1385
  = _M0MPC15array10FixedArray12view_2einnerGyE(_M0L4dataS1386, 0, _M0L6_2atmpS1387);
  #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  return _M0MPC15bytes5Bytes11from__array(_M0L6_2atmpS1385);
}

int32_t _M0MPC16buffer6Buffer19write__string__utf8(
  struct _M0TPC16buffer6Buffer* _M0L3bufS468,
  struct _M0TPC16string10StringView _M0L6stringS464
) {
  struct _M0TWEOc* _M0L5_2aitS463;
  #line 880 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  #line 880 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L5_2aitS463 = _M0MPC16string10StringView4iter(_M0L6stringS464);
  while (1) {
    int32_t _M0L7_2abindS465;
    moonbit_incref(_M0L5_2aitS463);
    #line 881 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L7_2abindS465 = _M0MPB4Iter4nextGcE(_M0L5_2aitS463);
    if (_M0L7_2abindS465 == -1) {
      moonbit_decref(_M0L3bufS468);
      moonbit_decref(_M0L5_2aitS463);
    } else {
      int32_t _M0L7_2aSomeS466 = _M0L7_2abindS465;
      int32_t _M0L5_2achS467 = _M0L7_2aSomeS466;
      moonbit_incref(_M0L3bufS468);
      #line 882 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      _M0MPC16buffer6Buffer17write__char__utf8(_M0L3bufS468, _M0L5_2achS467);
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16buffer6Buffer17write__char__utf8(
  struct _M0TPC16buffer6Buffer* _M0L3bufS462,
  int32_t _M0L5valueS461
) {
  uint32_t _M0L4codeS460;
  #line 782 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  #line 783 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L4codeS460 = _M0MPC14char4Char8to__uint(_M0L5valueS461);
  if (_M0L4codeS460 < 128u) {
    int32_t _M0L3lenS1377 = _M0L3bufS462->$1;
    int32_t _M0L6_2atmpS1376 = _M0L3lenS1377 + 1;
    moonbit_bytes_t _M0L8_2afieldS2234;
    moonbit_bytes_t _M0L4dataS1378;
    int32_t _M0L3lenS1379;
    uint32_t _M0L6_2atmpS1382;
    uint32_t _M0L6_2atmpS1381;
    int32_t _M0L6_2atmpS1380;
    int32_t _M0L3lenS1384;
    int32_t _M0L6_2atmpS1383;
    moonbit_incref(_M0L3bufS462);
    #line 786 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS462, _M0L6_2atmpS1376);
    _M0L8_2afieldS2234 = _M0L3bufS462->$0;
    _M0L4dataS1378 = _M0L8_2afieldS2234;
    _M0L3lenS1379 = _M0L3bufS462->$1;
    _M0L6_2atmpS1382 = _M0L4codeS460 & 127u;
    _M0L6_2atmpS1381 = _M0L6_2atmpS1382 | 0u;
    moonbit_incref(_M0L4dataS1378);
    #line 787 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1380 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1381);
    if (
      _M0L3lenS1379 < 0
      || _M0L3lenS1379 >= Moonbit_array_length(_M0L4dataS1378)
    ) {
      #line 787 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1378[_M0L3lenS1379] = _M0L6_2atmpS1380;
    moonbit_decref(_M0L4dataS1378);
    _M0L3lenS1384 = _M0L3bufS462->$1;
    _M0L6_2atmpS1383 = _M0L3lenS1384 + 1;
    _M0L3bufS462->$1 = _M0L6_2atmpS1383;
    moonbit_decref(_M0L3bufS462);
  } else if (_M0L4codeS460 < 2048u) {
    int32_t _M0L3lenS1361 = _M0L3bufS462->$1;
    int32_t _M0L6_2atmpS1360 = _M0L3lenS1361 + 2;
    moonbit_bytes_t _M0L8_2afieldS2236;
    moonbit_bytes_t _M0L4dataS1362;
    int32_t _M0L3lenS1363;
    uint32_t _M0L6_2atmpS1367;
    uint32_t _M0L6_2atmpS1366;
    uint32_t _M0L6_2atmpS1365;
    int32_t _M0L6_2atmpS1364;
    moonbit_bytes_t _M0L8_2afieldS2235;
    moonbit_bytes_t _M0L4dataS1368;
    int32_t _M0L3lenS1373;
    int32_t _M0L6_2atmpS1369;
    uint32_t _M0L6_2atmpS1372;
    uint32_t _M0L6_2atmpS1371;
    int32_t _M0L6_2atmpS1370;
    int32_t _M0L3lenS1375;
    int32_t _M0L6_2atmpS1374;
    moonbit_incref(_M0L3bufS462);
    #line 791 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS462, _M0L6_2atmpS1360);
    _M0L8_2afieldS2236 = _M0L3bufS462->$0;
    _M0L4dataS1362 = _M0L8_2afieldS2236;
    _M0L3lenS1363 = _M0L3bufS462->$1;
    _M0L6_2atmpS1367 = _M0L4codeS460 >> 6;
    _M0L6_2atmpS1366 = _M0L6_2atmpS1367 & 31u;
    _M0L6_2atmpS1365 = _M0L6_2atmpS1366 | 192u;
    moonbit_incref(_M0L4dataS1362);
    #line 792 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1364 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1365);
    if (
      _M0L3lenS1363 < 0
      || _M0L3lenS1363 >= Moonbit_array_length(_M0L4dataS1362)
    ) {
      #line 792 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1362[_M0L3lenS1363] = _M0L6_2atmpS1364;
    moonbit_decref(_M0L4dataS1362);
    _M0L8_2afieldS2235 = _M0L3bufS462->$0;
    _M0L4dataS1368 = _M0L8_2afieldS2235;
    _M0L3lenS1373 = _M0L3bufS462->$1;
    _M0L6_2atmpS1369 = _M0L3lenS1373 + 1;
    _M0L6_2atmpS1372 = _M0L4codeS460 & 63u;
    _M0L6_2atmpS1371 = _M0L6_2atmpS1372 | 128u;
    moonbit_incref(_M0L4dataS1368);
    #line 793 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1370 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1371);
    if (
      _M0L6_2atmpS1369 < 0
      || _M0L6_2atmpS1369 >= Moonbit_array_length(_M0L4dataS1368)
    ) {
      #line 793 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1368[_M0L6_2atmpS1369] = _M0L6_2atmpS1370;
    moonbit_decref(_M0L4dataS1368);
    _M0L3lenS1375 = _M0L3bufS462->$1;
    _M0L6_2atmpS1374 = _M0L3lenS1375 + 2;
    _M0L3bufS462->$1 = _M0L6_2atmpS1374;
    moonbit_decref(_M0L3bufS462);
  } else if (_M0L4codeS460 < 65536u) {
    int32_t _M0L3lenS1338 = _M0L3bufS462->$1;
    int32_t _M0L6_2atmpS1337 = _M0L3lenS1338 + 3;
    moonbit_bytes_t _M0L8_2afieldS2239;
    moonbit_bytes_t _M0L4dataS1339;
    int32_t _M0L3lenS1340;
    uint32_t _M0L6_2atmpS1344;
    uint32_t _M0L6_2atmpS1343;
    uint32_t _M0L6_2atmpS1342;
    int32_t _M0L6_2atmpS1341;
    moonbit_bytes_t _M0L8_2afieldS2238;
    moonbit_bytes_t _M0L4dataS1345;
    int32_t _M0L3lenS1351;
    int32_t _M0L6_2atmpS1346;
    uint32_t _M0L6_2atmpS1350;
    uint32_t _M0L6_2atmpS1349;
    uint32_t _M0L6_2atmpS1348;
    int32_t _M0L6_2atmpS1347;
    moonbit_bytes_t _M0L8_2afieldS2237;
    moonbit_bytes_t _M0L4dataS1352;
    int32_t _M0L3lenS1357;
    int32_t _M0L6_2atmpS1353;
    uint32_t _M0L6_2atmpS1356;
    uint32_t _M0L6_2atmpS1355;
    int32_t _M0L6_2atmpS1354;
    int32_t _M0L3lenS1359;
    int32_t _M0L6_2atmpS1358;
    moonbit_incref(_M0L3bufS462);
    #line 797 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS462, _M0L6_2atmpS1337);
    _M0L8_2afieldS2239 = _M0L3bufS462->$0;
    _M0L4dataS1339 = _M0L8_2afieldS2239;
    _M0L3lenS1340 = _M0L3bufS462->$1;
    _M0L6_2atmpS1344 = _M0L4codeS460 >> 12;
    _M0L6_2atmpS1343 = _M0L6_2atmpS1344 & 15u;
    _M0L6_2atmpS1342 = _M0L6_2atmpS1343 | 224u;
    moonbit_incref(_M0L4dataS1339);
    #line 798 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1341 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1342);
    if (
      _M0L3lenS1340 < 0
      || _M0L3lenS1340 >= Moonbit_array_length(_M0L4dataS1339)
    ) {
      #line 798 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1339[_M0L3lenS1340] = _M0L6_2atmpS1341;
    moonbit_decref(_M0L4dataS1339);
    _M0L8_2afieldS2238 = _M0L3bufS462->$0;
    _M0L4dataS1345 = _M0L8_2afieldS2238;
    _M0L3lenS1351 = _M0L3bufS462->$1;
    _M0L6_2atmpS1346 = _M0L3lenS1351 + 1;
    _M0L6_2atmpS1350 = _M0L4codeS460 >> 6;
    _M0L6_2atmpS1349 = _M0L6_2atmpS1350 & 63u;
    _M0L6_2atmpS1348 = _M0L6_2atmpS1349 | 128u;
    moonbit_incref(_M0L4dataS1345);
    #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1347 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1348);
    if (
      _M0L6_2atmpS1346 < 0
      || _M0L6_2atmpS1346 >= Moonbit_array_length(_M0L4dataS1345)
    ) {
      #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1345[_M0L6_2atmpS1346] = _M0L6_2atmpS1347;
    moonbit_decref(_M0L4dataS1345);
    _M0L8_2afieldS2237 = _M0L3bufS462->$0;
    _M0L4dataS1352 = _M0L8_2afieldS2237;
    _M0L3lenS1357 = _M0L3bufS462->$1;
    _M0L6_2atmpS1353 = _M0L3lenS1357 + 2;
    _M0L6_2atmpS1356 = _M0L4codeS460 & 63u;
    _M0L6_2atmpS1355 = _M0L6_2atmpS1356 | 128u;
    moonbit_incref(_M0L4dataS1352);
    #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1354 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1355);
    if (
      _M0L6_2atmpS1353 < 0
      || _M0L6_2atmpS1353 >= Moonbit_array_length(_M0L4dataS1352)
    ) {
      #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1352[_M0L6_2atmpS1353] = _M0L6_2atmpS1354;
    moonbit_decref(_M0L4dataS1352);
    _M0L3lenS1359 = _M0L3bufS462->$1;
    _M0L6_2atmpS1358 = _M0L3lenS1359 + 3;
    _M0L3bufS462->$1 = _M0L6_2atmpS1358;
    moonbit_decref(_M0L3bufS462);
  } else if (_M0L4codeS460 < 1114112u) {
    int32_t _M0L3lenS1308 = _M0L3bufS462->$1;
    int32_t _M0L6_2atmpS1307 = _M0L3lenS1308 + 4;
    moonbit_bytes_t _M0L8_2afieldS2243;
    moonbit_bytes_t _M0L4dataS1309;
    int32_t _M0L3lenS1310;
    uint32_t _M0L6_2atmpS1314;
    uint32_t _M0L6_2atmpS1313;
    uint32_t _M0L6_2atmpS1312;
    int32_t _M0L6_2atmpS1311;
    moonbit_bytes_t _M0L8_2afieldS2242;
    moonbit_bytes_t _M0L4dataS1315;
    int32_t _M0L3lenS1321;
    int32_t _M0L6_2atmpS1316;
    uint32_t _M0L6_2atmpS1320;
    uint32_t _M0L6_2atmpS1319;
    uint32_t _M0L6_2atmpS1318;
    int32_t _M0L6_2atmpS1317;
    moonbit_bytes_t _M0L8_2afieldS2241;
    moonbit_bytes_t _M0L4dataS1322;
    int32_t _M0L3lenS1328;
    int32_t _M0L6_2atmpS1323;
    uint32_t _M0L6_2atmpS1327;
    uint32_t _M0L6_2atmpS1326;
    uint32_t _M0L6_2atmpS1325;
    int32_t _M0L6_2atmpS1324;
    moonbit_bytes_t _M0L8_2afieldS2240;
    moonbit_bytes_t _M0L4dataS1329;
    int32_t _M0L3lenS1334;
    int32_t _M0L6_2atmpS1330;
    uint32_t _M0L6_2atmpS1333;
    uint32_t _M0L6_2atmpS1332;
    int32_t _M0L6_2atmpS1331;
    int32_t _M0L3lenS1336;
    int32_t _M0L6_2atmpS1335;
    moonbit_incref(_M0L3bufS462);
    #line 804 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS462, _M0L6_2atmpS1307);
    _M0L8_2afieldS2243 = _M0L3bufS462->$0;
    _M0L4dataS1309 = _M0L8_2afieldS2243;
    _M0L3lenS1310 = _M0L3bufS462->$1;
    _M0L6_2atmpS1314 = _M0L4codeS460 >> 18;
    _M0L6_2atmpS1313 = _M0L6_2atmpS1314 & 7u;
    _M0L6_2atmpS1312 = _M0L6_2atmpS1313 | 240u;
    moonbit_incref(_M0L4dataS1309);
    #line 805 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1311 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1312);
    if (
      _M0L3lenS1310 < 0
      || _M0L3lenS1310 >= Moonbit_array_length(_M0L4dataS1309)
    ) {
      #line 805 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1309[_M0L3lenS1310] = _M0L6_2atmpS1311;
    moonbit_decref(_M0L4dataS1309);
    _M0L8_2afieldS2242 = _M0L3bufS462->$0;
    _M0L4dataS1315 = _M0L8_2afieldS2242;
    _M0L3lenS1321 = _M0L3bufS462->$1;
    _M0L6_2atmpS1316 = _M0L3lenS1321 + 1;
    _M0L6_2atmpS1320 = _M0L4codeS460 >> 12;
    _M0L6_2atmpS1319 = _M0L6_2atmpS1320 & 63u;
    _M0L6_2atmpS1318 = _M0L6_2atmpS1319 | 128u;
    moonbit_incref(_M0L4dataS1315);
    #line 806 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1317 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1318);
    if (
      _M0L6_2atmpS1316 < 0
      || _M0L6_2atmpS1316 >= Moonbit_array_length(_M0L4dataS1315)
    ) {
      #line 806 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1315[_M0L6_2atmpS1316] = _M0L6_2atmpS1317;
    moonbit_decref(_M0L4dataS1315);
    _M0L8_2afieldS2241 = _M0L3bufS462->$0;
    _M0L4dataS1322 = _M0L8_2afieldS2241;
    _M0L3lenS1328 = _M0L3bufS462->$1;
    _M0L6_2atmpS1323 = _M0L3lenS1328 + 2;
    _M0L6_2atmpS1327 = _M0L4codeS460 >> 6;
    _M0L6_2atmpS1326 = _M0L6_2atmpS1327 & 63u;
    _M0L6_2atmpS1325 = _M0L6_2atmpS1326 | 128u;
    moonbit_incref(_M0L4dataS1322);
    #line 807 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1324 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1325);
    if (
      _M0L6_2atmpS1323 < 0
      || _M0L6_2atmpS1323 >= Moonbit_array_length(_M0L4dataS1322)
    ) {
      #line 807 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1322[_M0L6_2atmpS1323] = _M0L6_2atmpS1324;
    moonbit_decref(_M0L4dataS1322);
    _M0L8_2afieldS2240 = _M0L3bufS462->$0;
    _M0L4dataS1329 = _M0L8_2afieldS2240;
    _M0L3lenS1334 = _M0L3bufS462->$1;
    _M0L6_2atmpS1330 = _M0L3lenS1334 + 3;
    _M0L6_2atmpS1333 = _M0L4codeS460 & 63u;
    _M0L6_2atmpS1332 = _M0L6_2atmpS1333 | 128u;
    moonbit_incref(_M0L4dataS1329);
    #line 808 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1331 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1332);
    if (
      _M0L6_2atmpS1330 < 0
      || _M0L6_2atmpS1330 >= Moonbit_array_length(_M0L4dataS1329)
    ) {
      #line 808 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1329[_M0L6_2atmpS1330] = _M0L6_2atmpS1331;
    moonbit_decref(_M0L4dataS1329);
    _M0L3lenS1336 = _M0L3bufS462->$1;
    _M0L6_2atmpS1335 = _M0L3lenS1336 + 4;
    _M0L3bufS462->$1 = _M0L6_2atmpS1335;
    moonbit_decref(_M0L3bufS462);
  } else {
    moonbit_decref(_M0L3bufS462);
    #line 811 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data);
  }
  return 0;
}

struct _M0TPC16buffer6Buffer* _M0FPC16buffer11new_2einner(
  int32_t _M0L10size__hintS458
) {
  int32_t _M0L7initialS457;
  int32_t _M0L6_2atmpS1306;
  moonbit_bytes_t _M0L4dataS459;
  struct _M0TPC16buffer6Buffer* _block_2456;
  #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  if (_M0L10size__hintS458 < 1) {
    _M0L7initialS457 = 1;
  } else {
    _M0L7initialS457 = _M0L10size__hintS458;
  }
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS1306 = _M0IPC14byte4BytePB7Default7default();
  _M0L4dataS459
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS457, _M0L6_2atmpS1306);
  _block_2456
  = (struct _M0TPC16buffer6Buffer*)moonbit_malloc(sizeof(struct _M0TPC16buffer6Buffer));
  Moonbit_object_header(_block_2456)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC16buffer6Buffer, $0) >> 2, 1, 0);
  _block_2456->$0 = _M0L4dataS459;
  _block_2456->$1 = 0;
  return _block_2456;
}

int32_t _M0MPC16buffer6Buffer19grow__if__necessary(
  struct _M0TPC16buffer6Buffer* _M0L4selfS451,
  int32_t _M0L8requiredS454
) {
  moonbit_bytes_t _M0L8_2afieldS2251;
  moonbit_bytes_t _M0L4dataS1304;
  int32_t _M0L6_2atmpS2250;
  int32_t _M0L6_2atmpS1303;
  int32_t _M0L5startS450;
  int32_t _M0L13enough__spaceS452;
  int32_t _M0L5spaceS453;
  moonbit_bytes_t _M0L8_2afieldS2247;
  moonbit_bytes_t _M0L4dataS1298;
  int32_t _M0L6_2atmpS2246;
  int32_t _M0L6_2atmpS1297;
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS2251 = _M0L4selfS451->$0;
  _M0L4dataS1304 = _M0L8_2afieldS2251;
  _M0L6_2atmpS2250 = Moonbit_array_length(_M0L4dataS1304);
  _M0L6_2atmpS1303 = _M0L6_2atmpS2250;
  if (_M0L6_2atmpS1303 <= 0) {
    _M0L5startS450 = 1;
  } else {
    moonbit_bytes_t _M0L8_2afieldS2249 = _M0L4selfS451->$0;
    moonbit_bytes_t _M0L4dataS1305 = _M0L8_2afieldS2249;
    int32_t _M0L6_2atmpS2248 = Moonbit_array_length(_M0L4dataS1305);
    _M0L5startS450 = _M0L6_2atmpS2248;
  }
  _M0L5spaceS453 = _M0L5startS450;
  while (1) {
    int32_t _M0L6_2atmpS1302;
    if (_M0L5spaceS453 >= _M0L8requiredS454) {
      _M0L13enough__spaceS452 = _M0L5spaceS453;
      break;
    }
    _M0L6_2atmpS1302 = _M0L5spaceS453 * 2;
    _M0L5spaceS453 = _M0L6_2atmpS1302;
    continue;
    break;
  }
  _M0L8_2afieldS2247 = _M0L4selfS451->$0;
  _M0L4dataS1298 = _M0L8_2afieldS2247;
  _M0L6_2atmpS2246 = Moonbit_array_length(_M0L4dataS1298);
  _M0L6_2atmpS1297 = _M0L6_2atmpS2246;
  if (_M0L13enough__spaceS452 != _M0L6_2atmpS1297) {
    int32_t _M0L6_2atmpS1301;
    moonbit_bytes_t _M0L9new__dataS456;
    moonbit_bytes_t _M0L8_2afieldS2245;
    moonbit_bytes_t _M0L4dataS1299;
    int32_t _M0L3lenS1300;
    moonbit_bytes_t _M0L6_2aoldS2244;
    #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1301 = _M0IPC14byte4BytePB7Default7default();
    _M0L9new__dataS456
    = (moonbit_bytes_t)moonbit_make_bytes(_M0L13enough__spaceS452, _M0L6_2atmpS1301);
    _M0L8_2afieldS2245 = _M0L4selfS451->$0;
    _M0L4dataS1299 = _M0L8_2afieldS2245;
    _M0L3lenS1300 = _M0L4selfS451->$1;
    moonbit_incref(_M0L4dataS1299);
    moonbit_incref(_M0L9new__dataS456);
    #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS456, 0, _M0L4dataS1299, 0, _M0L3lenS1300);
    _M0L6_2aoldS2244 = _M0L4selfS451->$0;
    moonbit_decref(_M0L6_2aoldS2244);
    _M0L4selfS451->$0 = _M0L9new__dataS456;
    moonbit_decref(_M0L4selfS451);
  } else {
    moonbit_decref(_M0L4selfS451);
  }
  return 0;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS448,
  struct _M0TPB6Logger _M0L6loggerS449
) {
  moonbit_string_t _M0L6_2atmpS1296;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1295;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1296 = _M0L4selfS448;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1295 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1296);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1295, _M0L6loggerS449);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS425,
  struct _M0TPB6Logger _M0L6loggerS447
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2260;
  struct _M0TPC16string10StringView _M0L3pkgS424;
  moonbit_string_t _M0L7_2adataS426;
  int32_t _M0L8_2astartS427;
  int32_t _M0L6_2atmpS1294;
  int32_t _M0L6_2aendS428;
  int32_t _M0Lm9_2acursorS429;
  int32_t _M0Lm13accept__stateS430;
  int32_t _M0Lm10match__endS431;
  int32_t _M0Lm20match__tag__saver__0S432;
  int32_t _M0Lm6tag__0S433;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS434;
  struct _M0TPC16string10StringView _M0L8_2afieldS2259;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS443;
  void* _M0L8_2afieldS2258;
  int32_t _M0L6_2acntS2371;
  void* _M0L16_2apackage__nameS444;
  struct _M0TPC16string10StringView _M0L8_2afieldS2256;
  struct _M0TPC16string10StringView _M0L8filenameS1271;
  struct _M0TPC16string10StringView _M0L8_2afieldS2255;
  struct _M0TPC16string10StringView _M0L11start__lineS1272;
  struct _M0TPC16string10StringView _M0L8_2afieldS2254;
  struct _M0TPC16string10StringView _M0L13start__columnS1273;
  struct _M0TPC16string10StringView _M0L8_2afieldS2253;
  struct _M0TPC16string10StringView _M0L9end__lineS1274;
  struct _M0TPC16string10StringView _M0L8_2afieldS2252;
  int32_t _M0L6_2acntS2375;
  struct _M0TPC16string10StringView _M0L11end__columnS1275;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2260
  = (struct _M0TPC16string10StringView){
    _M0L4selfS425->$0_1, _M0L4selfS425->$0_2, _M0L4selfS425->$0_0
  };
  _M0L3pkgS424 = _M0L8_2afieldS2260;
  moonbit_incref(_M0L3pkgS424.$0);
  moonbit_incref(_M0L3pkgS424.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS426 = _M0MPC16string10StringView4data(_M0L3pkgS424);
  moonbit_incref(_M0L3pkgS424.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS427 = _M0MPC16string10StringView13start__offset(_M0L3pkgS424);
  moonbit_incref(_M0L3pkgS424.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1294 = _M0MPC16string10StringView6length(_M0L3pkgS424);
  _M0L6_2aendS428 = _M0L8_2astartS427 + _M0L6_2atmpS1294;
  _M0Lm9_2acursorS429 = _M0L8_2astartS427;
  _M0Lm13accept__stateS430 = -1;
  _M0Lm10match__endS431 = -1;
  _M0Lm20match__tag__saver__0S432 = -1;
  _M0Lm6tag__0S433 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1286 = _M0Lm9_2acursorS429;
    if (_M0L6_2atmpS1286 < _M0L6_2aendS428) {
      int32_t _M0L6_2atmpS1293 = _M0Lm9_2acursorS429;
      int32_t _M0L10next__charS438;
      int32_t _M0L6_2atmpS1287;
      moonbit_incref(_M0L7_2adataS426);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS438
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS426, _M0L6_2atmpS1293);
      _M0L6_2atmpS1287 = _M0Lm9_2acursorS429;
      _M0Lm9_2acursorS429 = _M0L6_2atmpS1287 + 1;
      if (_M0L10next__charS438 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1288;
          _M0Lm6tag__0S433 = _M0Lm9_2acursorS429;
          _M0L6_2atmpS1288 = _M0Lm9_2acursorS429;
          if (_M0L6_2atmpS1288 < _M0L6_2aendS428) {
            int32_t _M0L6_2atmpS1292 = _M0Lm9_2acursorS429;
            int32_t _M0L10next__charS439;
            int32_t _M0L6_2atmpS1289;
            moonbit_incref(_M0L7_2adataS426);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS439
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS426, _M0L6_2atmpS1292);
            _M0L6_2atmpS1289 = _M0Lm9_2acursorS429;
            _M0Lm9_2acursorS429 = _M0L6_2atmpS1289 + 1;
            if (_M0L10next__charS439 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1290 = _M0Lm9_2acursorS429;
                if (_M0L6_2atmpS1290 < _M0L6_2aendS428) {
                  int32_t _M0L6_2atmpS1291 = _M0Lm9_2acursorS429;
                  _M0Lm9_2acursorS429 = _M0L6_2atmpS1291 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S432 = _M0Lm6tag__0S433;
                  _M0Lm13accept__stateS430 = 0;
                  _M0Lm10match__endS431 = _M0Lm9_2acursorS429;
                  goto join_435;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_435;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_435;
    }
    break;
  }
  goto joinlet_2458;
  join_435:;
  switch (_M0Lm13accept__stateS430) {
    case 0: {
      int32_t _M0L6_2atmpS1284;
      int32_t _M0L6_2atmpS1283;
      int64_t _M0L6_2atmpS1280;
      int32_t _M0L6_2atmpS1282;
      int64_t _M0L6_2atmpS1281;
      struct _M0TPC16string10StringView _M0L13package__nameS436;
      int64_t _M0L6_2atmpS1277;
      int32_t _M0L6_2atmpS1279;
      int64_t _M0L6_2atmpS1278;
      struct _M0TPC16string10StringView _M0L12module__nameS437;
      void* _M0L4SomeS1276;
      moonbit_decref(_M0L3pkgS424.$0);
      _M0L6_2atmpS1284 = _M0Lm20match__tag__saver__0S432;
      _M0L6_2atmpS1283 = _M0L6_2atmpS1284 + 1;
      _M0L6_2atmpS1280 = (int64_t)_M0L6_2atmpS1283;
      _M0L6_2atmpS1282 = _M0Lm10match__endS431;
      _M0L6_2atmpS1281 = (int64_t)_M0L6_2atmpS1282;
      moonbit_incref(_M0L7_2adataS426);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS436
      = _M0MPC16string6String4view(_M0L7_2adataS426, _M0L6_2atmpS1280, _M0L6_2atmpS1281);
      _M0L6_2atmpS1277 = (int64_t)_M0L8_2astartS427;
      _M0L6_2atmpS1279 = _M0Lm20match__tag__saver__0S432;
      _M0L6_2atmpS1278 = (int64_t)_M0L6_2atmpS1279;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS437
      = _M0MPC16string6String4view(_M0L7_2adataS426, _M0L6_2atmpS1277, _M0L6_2atmpS1278);
      _M0L4SomeS1276
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1276)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1276)->$0_0
      = _M0L13package__nameS436.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1276)->$0_1
      = _M0L13package__nameS436.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1276)->$0_2
      = _M0L13package__nameS436.$2;
      _M0L7_2abindS434
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS434)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS434->$0_0 = _M0L12module__nameS437.$0;
      _M0L7_2abindS434->$0_1 = _M0L12module__nameS437.$1;
      _M0L7_2abindS434->$0_2 = _M0L12module__nameS437.$2;
      _M0L7_2abindS434->$1 = _M0L4SomeS1276;
      break;
    }
    default: {
      void* _M0L4NoneS1285;
      moonbit_decref(_M0L7_2adataS426);
      _M0L4NoneS1285
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS434
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS434)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS434->$0_0 = _M0L3pkgS424.$0;
      _M0L7_2abindS434->$0_1 = _M0L3pkgS424.$1;
      _M0L7_2abindS434->$0_2 = _M0L3pkgS424.$2;
      _M0L7_2abindS434->$1 = _M0L4NoneS1285;
      break;
    }
  }
  joinlet_2458:;
  _M0L8_2afieldS2259
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS434->$0_1, _M0L7_2abindS434->$0_2, _M0L7_2abindS434->$0_0
  };
  _M0L15_2amodule__nameS443 = _M0L8_2afieldS2259;
  _M0L8_2afieldS2258 = _M0L7_2abindS434->$1;
  _M0L6_2acntS2371 = Moonbit_object_header(_M0L7_2abindS434)->rc;
  if (_M0L6_2acntS2371 > 1) {
    int32_t _M0L11_2anew__cntS2372 = _M0L6_2acntS2371 - 1;
    Moonbit_object_header(_M0L7_2abindS434)->rc = _M0L11_2anew__cntS2372;
    moonbit_incref(_M0L8_2afieldS2258);
    moonbit_incref(_M0L15_2amodule__nameS443.$0);
  } else if (_M0L6_2acntS2371 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS434);
  }
  _M0L16_2apackage__nameS444 = _M0L8_2afieldS2258;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS444)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS445 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS444;
      struct _M0TPC16string10StringView _M0L8_2afieldS2257 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS445->$0_1,
                                              _M0L7_2aSomeS445->$0_2,
                                              _M0L7_2aSomeS445->$0_0};
      int32_t _M0L6_2acntS2373 = Moonbit_object_header(_M0L7_2aSomeS445)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS446;
      if (_M0L6_2acntS2373 > 1) {
        int32_t _M0L11_2anew__cntS2374 = _M0L6_2acntS2373 - 1;
        Moonbit_object_header(_M0L7_2aSomeS445)->rc = _M0L11_2anew__cntS2374;
        moonbit_incref(_M0L8_2afieldS2257.$0);
      } else if (_M0L6_2acntS2373 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS445);
      }
      _M0L12_2apkg__nameS446 = _M0L8_2afieldS2257;
      if (_M0L6loggerS447.$1) {
        moonbit_incref(_M0L6loggerS447.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L12_2apkg__nameS446);
      if (_M0L6loggerS447.$1) {
        moonbit_incref(_M0L6loggerS447.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS447.$0->$method_3(_M0L6loggerS447.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS444);
      break;
    }
  }
  _M0L8_2afieldS2256
  = (struct _M0TPC16string10StringView){
    _M0L4selfS425->$1_1, _M0L4selfS425->$1_2, _M0L4selfS425->$1_0
  };
  _M0L8filenameS1271 = _M0L8_2afieldS2256;
  moonbit_incref(_M0L8filenameS1271.$0);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L8filenameS1271);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_3(_M0L6loggerS447.$1, 58);
  _M0L8_2afieldS2255
  = (struct _M0TPC16string10StringView){
    _M0L4selfS425->$2_1, _M0L4selfS425->$2_2, _M0L4selfS425->$2_0
  };
  _M0L11start__lineS1272 = _M0L8_2afieldS2255;
  moonbit_incref(_M0L11start__lineS1272.$0);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L11start__lineS1272);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_3(_M0L6loggerS447.$1, 58);
  _M0L8_2afieldS2254
  = (struct _M0TPC16string10StringView){
    _M0L4selfS425->$3_1, _M0L4selfS425->$3_2, _M0L4selfS425->$3_0
  };
  _M0L13start__columnS1273 = _M0L8_2afieldS2254;
  moonbit_incref(_M0L13start__columnS1273.$0);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L13start__columnS1273);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_3(_M0L6loggerS447.$1, 45);
  _M0L8_2afieldS2253
  = (struct _M0TPC16string10StringView){
    _M0L4selfS425->$4_1, _M0L4selfS425->$4_2, _M0L4selfS425->$4_0
  };
  _M0L9end__lineS1274 = _M0L8_2afieldS2253;
  moonbit_incref(_M0L9end__lineS1274.$0);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L9end__lineS1274);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_3(_M0L6loggerS447.$1, 58);
  _M0L8_2afieldS2252
  = (struct _M0TPC16string10StringView){
    _M0L4selfS425->$5_1, _M0L4selfS425->$5_2, _M0L4selfS425->$5_0
  };
  _M0L6_2acntS2375 = Moonbit_object_header(_M0L4selfS425)->rc;
  if (_M0L6_2acntS2375 > 1) {
    int32_t _M0L11_2anew__cntS2381 = _M0L6_2acntS2375 - 1;
    Moonbit_object_header(_M0L4selfS425)->rc = _M0L11_2anew__cntS2381;
    moonbit_incref(_M0L8_2afieldS2252.$0);
  } else if (_M0L6_2acntS2375 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2380 =
      (struct _M0TPC16string10StringView){_M0L4selfS425->$4_1,
                                            _M0L4selfS425->$4_2,
                                            _M0L4selfS425->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2379;
    struct _M0TPC16string10StringView _M0L8_2afieldS2378;
    struct _M0TPC16string10StringView _M0L8_2afieldS2377;
    struct _M0TPC16string10StringView _M0L8_2afieldS2376;
    moonbit_decref(_M0L8_2afieldS2380.$0);
    _M0L8_2afieldS2379
    = (struct _M0TPC16string10StringView){
      _M0L4selfS425->$3_1, _M0L4selfS425->$3_2, _M0L4selfS425->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2379.$0);
    _M0L8_2afieldS2378
    = (struct _M0TPC16string10StringView){
      _M0L4selfS425->$2_1, _M0L4selfS425->$2_2, _M0L4selfS425->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2378.$0);
    _M0L8_2afieldS2377
    = (struct _M0TPC16string10StringView){
      _M0L4selfS425->$1_1, _M0L4selfS425->$1_2, _M0L4selfS425->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2377.$0);
    _M0L8_2afieldS2376
    = (struct _M0TPC16string10StringView){
      _M0L4selfS425->$0_1, _M0L4selfS425->$0_2, _M0L4selfS425->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2376.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS425);
  }
  _M0L11end__columnS1275 = _M0L8_2afieldS2252;
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L11end__columnS1275);
  if (_M0L6loggerS447.$1) {
    moonbit_incref(_M0L6loggerS447.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_3(_M0L6loggerS447.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS447.$0->$method_2(_M0L6loggerS447.$1, _M0L15_2amodule__nameS443);
  return 0;
}

moonbit_bytes_t _M0MPC15bytes5Bytes11from__array(
  struct _M0TPB9ArrayViewGyE _M0L3arrS422
) {
  int32_t _M0L6_2atmpS1266;
  struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__* _closure_2462;
  struct _M0TWuEu* _M0L6_2atmpS1267;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  moonbit_incref(_M0L3arrS422.$0);
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1266 = _M0MPC15array9ArrayView6lengthGyE(_M0L3arrS422);
  _closure_2462
  = (struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__*)moonbit_malloc(sizeof(struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__));
  Moonbit_object_header(_closure_2462)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__, $0_0) >> 2, 1, 0);
  _closure_2462->code = &_M0MPC15bytes5Bytes11from__arrayC1268l455;
  _closure_2462->$0_0 = _M0L3arrS422.$0;
  _closure_2462->$0_1 = _M0L3arrS422.$1;
  _closure_2462->$0_2 = _M0L3arrS422.$2;
  _M0L6_2atmpS1267 = (struct _M0TWuEu*)_closure_2462;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  return _M0MPC15bytes5Bytes5makei(_M0L6_2atmpS1266, _M0L6_2atmpS1267);
}

int32_t _M0MPC15bytes5Bytes11from__arrayC1268l455(
  struct _M0TWuEu* _M0L6_2aenvS1269,
  int32_t _M0L1iS423
) {
  struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__* _M0L14_2acasted__envS1270;
  struct _M0TPB9ArrayViewGyE _M0L8_2afieldS2261;
  int32_t _M0L6_2acntS2382;
  struct _M0TPB9ArrayViewGyE _M0L3arrS422;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L14_2acasted__envS1270
  = (struct _M0R44Bytes_3a_3afrom__array_2eanon__u1268__l455__*)_M0L6_2aenvS1269;
  _M0L8_2afieldS2261
  = (struct _M0TPB9ArrayViewGyE){
    _M0L14_2acasted__envS1270->$0_1,
      _M0L14_2acasted__envS1270->$0_2,
      _M0L14_2acasted__envS1270->$0_0
  };
  _M0L6_2acntS2382 = Moonbit_object_header(_M0L14_2acasted__envS1270)->rc;
  if (_M0L6_2acntS2382 > 1) {
    int32_t _M0L11_2anew__cntS2383 = _M0L6_2acntS2382 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1270)->rc
    = _M0L11_2anew__cntS2383;
    moonbit_incref(_M0L8_2afieldS2261.$0);
  } else if (_M0L6_2acntS2382 == 1) {
    #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_free(_M0L14_2acasted__envS1270);
  }
  _M0L3arrS422 = _M0L8_2afieldS2261;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  return _M0MPC15array9ArrayView2atGyE(_M0L3arrS422, _M0L1iS423);
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS414,
  int32_t _M0L5startS420,
  int64_t _M0L3endS416
) {
  int32_t _M0L3lenS413;
  int32_t _M0L3endS415;
  int32_t _M0L5startS419;
  int32_t _if__result_2463;
  #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3lenS413 = Moonbit_array_length(_M0L4selfS414);
  if (_M0L3endS416 == 4294967296ll) {
    _M0L3endS415 = _M0L3lenS413;
  } else {
    int64_t _M0L7_2aSomeS417 = _M0L3endS416;
    int32_t _M0L6_2aendS418 = (int32_t)_M0L7_2aSomeS417;
    if (_M0L6_2aendS418 < 0) {
      _M0L3endS415 = _M0L3lenS413 + _M0L6_2aendS418;
    } else {
      _M0L3endS415 = _M0L6_2aendS418;
    }
  }
  if (_M0L5startS420 < 0) {
    _M0L5startS419 = _M0L3lenS413 + _M0L5startS420;
  } else {
    _M0L5startS419 = _M0L5startS420;
  }
  if (_M0L5startS419 >= 0) {
    if (_M0L5startS419 <= _M0L3endS415) {
      _if__result_2463 = _M0L3endS415 <= _M0L3lenS413;
    } else {
      _if__result_2463 = 0;
    }
  } else {
    _if__result_2463 = 0;
  }
  if (_if__result_2463) {
    int32_t _M0L7_2abindS421 = _M0L3endS415 - _M0L5startS419;
    int32_t _M0L6_2atmpS1265 = _M0L5startS419 + _M0L7_2abindS421;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS419,
                                              _M0L6_2atmpS1265,
                                              _M0L4selfS414};
  } else {
    moonbit_decref(_M0L4selfS414);
    #line 180 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    return _M0FPB5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_13.data, (moonbit_string_t)moonbit_string_literal_14.data);
  }
}

int32_t _M0MPC15bytes9BytesView6length(
  struct _M0TPC15bytes9BytesView _M0L4selfS412
) {
  int32_t _M0L3endS1263;
  int32_t _M0L8_2afieldS2262;
  int32_t _M0L5startS1264;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3endS1263 = _M0L4selfS412.$2;
  _M0L8_2afieldS2262 = _M0L4selfS412.$1;
  moonbit_decref(_M0L4selfS412.$0);
  _M0L5startS1264 = _M0L8_2afieldS2262;
  return _M0L3endS1263 - _M0L5startS1264;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS411) {
  moonbit_string_t _M0L6_2atmpS1262;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS1262 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS411);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS1262);
  moonbit_decref(_M0L6_2atmpS1262);
  return 0;
}

moonbit_bytes_t _M0MPC15bytes5Bytes5makei(
  int32_t _M0L6lengthS406,
  struct _M0TWuEu* _M0L5valueS408
) {
  int32_t _M0L6_2atmpS1261;
  moonbit_bytes_t _M0L3arrS407;
  int32_t _M0L1iS409;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  if (_M0L6lengthS406 <= 0) {
    moonbit_decref(_M0L5valueS408);
    return (moonbit_bytes_t)moonbit_bytes_literal_0.data;
  }
  moonbit_incref(_M0L5valueS408);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1261 = _M0L5valueS408->code(_M0L5valueS408, 0);
  _M0L3arrS407
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6lengthS406, _M0L6_2atmpS1261);
  _M0L1iS409 = 1;
  while (1) {
    if (_M0L1iS409 < _M0L6lengthS406) {
      int32_t _M0L6_2atmpS1259;
      int32_t _M0L6_2atmpS1260;
      moonbit_incref(_M0L5valueS408);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      _M0L6_2atmpS1259 = _M0L5valueS408->code(_M0L5valueS408, _M0L1iS409);
      if (_M0L1iS409 < 0 || _M0L1iS409 >= Moonbit_array_length(_M0L3arrS407)) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        moonbit_panic();
      }
      _M0L3arrS407[_M0L1iS409] = _M0L6_2atmpS1259;
      _M0L6_2atmpS1260 = _M0L1iS409 + 1;
      _M0L1iS409 = _M0L6_2atmpS1260;
      continue;
    } else {
      moonbit_decref(_M0L5valueS408);
    }
    break;
  }
  return _M0L3arrS407;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS404,
  int32_t _M0L3idxS405
) {
  int32_t _M0L6_2atmpS2263;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2263 = _M0L4selfS404[_M0L3idxS405];
  moonbit_decref(_M0L4selfS404);
  return _M0L6_2atmpS2263;
}

moonbit_string_t _M0MPC15array10FixedArray4join(
  moonbit_string_t* _M0L4selfS393,
  struct _M0TPC16string10StringView _M0L9separatorS397
) {
  int32_t _M0L3lenS392;
  moonbit_string_t _M0L6_2atmpS2270;
  moonbit_string_t _M0L5firstS394;
  int32_t _M0L6_2atmpS2269;
  int32_t _M0Lm10size__hintS395;
  int32_t _M0L1iS396;
  int32_t _M0L6_2atmpS1258;
  struct _M0TPB13StringBuilder* _M0L6stringS399;
  int32_t _M0L6_2atmpS1249;
  #line 1422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L3lenS392 = Moonbit_array_length(_M0L4selfS393);
  if (_M0L3lenS392 == 0) {
    moonbit_decref(_M0L9separatorS397.$0);
    moonbit_decref(_M0L4selfS393);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L4selfS393)) {
    #line 1430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2270 = (moonbit_string_t)_M0L4selfS393[0];
  _M0L5firstS394 = _M0L6_2atmpS2270;
  _M0L6_2atmpS2269 = Moonbit_array_length(_M0L5firstS394);
  _M0Lm10size__hintS395 = _M0L6_2atmpS2269;
  _M0L1iS396 = 1;
  while (1) {
    if (_M0L1iS396 < _M0L3lenS392) {
      int32_t _M0L6_2atmpS1243 = _M0Lm10size__hintS395;
      int32_t _M0L6_2atmpS1245;
      moonbit_string_t _M0L6_2atmpS2268;
      moonbit_string_t _M0L6_2atmpS1247;
      int32_t _M0L6_2atmpS2267;
      int32_t _M0L6_2atmpS1246;
      int32_t _M0L6_2atmpS1244;
      int32_t _M0L6_2atmpS1248;
      moonbit_incref(_M0L9separatorS397.$0);
      #line 1433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
      _M0L6_2atmpS1245
      = _M0MPC16string10StringView6length(_M0L9separatorS397);
      if (
        _M0L1iS396 < 0 || _M0L1iS396 >= Moonbit_array_length(_M0L4selfS393)
      ) {
        #line 1433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2268 = (moonbit_string_t)_M0L4selfS393[_M0L1iS396];
      _M0L6_2atmpS1247 = _M0L6_2atmpS2268;
      _M0L6_2atmpS2267 = Moonbit_array_length(_M0L6_2atmpS1247);
      _M0L6_2atmpS1246 = _M0L6_2atmpS2267;
      _M0L6_2atmpS1244 = _M0L6_2atmpS1245 + _M0L6_2atmpS1246;
      _M0Lm10size__hintS395 = _M0L6_2atmpS1243 + _M0L6_2atmpS1244;
      _M0L6_2atmpS1248 = _M0L1iS396 + 1;
      _M0L1iS396 = _M0L6_2atmpS1248;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1258 = _M0Lm10size__hintS395;
  #line 1435 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L6stringS399 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1258);
  moonbit_incref(_M0L9separatorS397.$0);
  #line 1436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L6_2atmpS1249 = _M0MPC16string10StringView6length(_M0L9separatorS397);
  if (_M0L6_2atmpS1249 == 0) {
    int32_t _M0L1iS400;
    moonbit_decref(_M0L9separatorS397.$0);
    _M0L1iS400 = 0;
    while (1) {
      if (_M0L1iS400 < _M0L3lenS392) {
        moonbit_string_t _M0L6_2atmpS2264;
        moonbit_string_t _M0L6_2atmpS1250;
        int32_t _M0L6_2atmpS1251;
        if (
          _M0L1iS400 < 0 || _M0L1iS400 >= Moonbit_array_length(_M0L4selfS393)
        ) {
          #line 1438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2264 = (moonbit_string_t)_M0L4selfS393[_M0L1iS400];
        _M0L6_2atmpS1250 = _M0L6_2atmpS2264;
        moonbit_incref(_M0L6_2atmpS1250);
        moonbit_incref(_M0L6stringS399);
        #line 1438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L6stringS399, _M0L6_2atmpS1250);
        _M0L6_2atmpS1251 = _M0L1iS400 + 1;
        _M0L1iS400 = _M0L6_2atmpS1251;
        continue;
      } else {
        moonbit_decref(_M0L4selfS393);
      }
      break;
    }
  } else {
    moonbit_string_t _M0L6_2atmpS2266;
    moonbit_string_t _M0L6_2atmpS1252;
    int32_t _M0L1iS402;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L4selfS393)) {
      #line 1441 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2266 = (moonbit_string_t)_M0L4selfS393[0];
    _M0L6_2atmpS1252 = _M0L6_2atmpS2266;
    moonbit_incref(_M0L6_2atmpS1252);
    moonbit_incref(_M0L6stringS399);
    #line 1441 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L6stringS399, _M0L6_2atmpS1252);
    _M0L1iS402 = 1;
    while (1) {
      if (_M0L1iS402 < _M0L3lenS392) {
        moonbit_string_t _M0L6_2atmpS1253;
        int32_t _M0L6_2atmpS1254;
        int32_t _M0L6_2atmpS1255;
        moonbit_string_t _M0L6_2atmpS2265;
        moonbit_string_t _M0L6_2atmpS1256;
        int32_t _M0L6_2atmpS1257;
        moonbit_incref(_M0L9separatorS397.$0);
        #line 1444 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0L6_2atmpS1253
        = _M0MPC16string10StringView4data(_M0L9separatorS397);
        moonbit_incref(_M0L9separatorS397.$0);
        #line 1445 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0L6_2atmpS1254
        = _M0MPC16string10StringView13start__offset(_M0L9separatorS397);
        moonbit_incref(_M0L9separatorS397.$0);
        #line 1446 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0L6_2atmpS1255
        = _M0MPC16string10StringView6length(_M0L9separatorS397);
        moonbit_incref(_M0L6stringS399);
        #line 1443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L6stringS399, _M0L6_2atmpS1253, _M0L6_2atmpS1254, _M0L6_2atmpS1255);
        if (
          _M0L1iS402 < 0 || _M0L1iS402 >= Moonbit_array_length(_M0L4selfS393)
        ) {
          #line 1448 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2265 = (moonbit_string_t)_M0L4selfS393[_M0L1iS402];
        _M0L6_2atmpS1256 = _M0L6_2atmpS2265;
        moonbit_incref(_M0L6_2atmpS1256);
        moonbit_incref(_M0L6stringS399);
        #line 1448 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L6stringS399, _M0L6_2atmpS1256);
        _M0L6_2atmpS1257 = _M0L1iS402 + 1;
        _M0L1iS402 = _M0L6_2atmpS1257;
        continue;
      } else {
        moonbit_decref(_M0L9separatorS397.$0);
        moonbit_decref(_M0L4selfS393);
      }
      break;
    }
  }
  #line 1451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6stringS399);
}

struct _M0TPB9ArrayViewGyE _M0MPC15array10FixedArray12view_2einnerGyE(
  moonbit_bytes_t _M0L4selfS383,
  int32_t _M0L5startS389,
  int64_t _M0L3endS385
) {
  int32_t _M0L3lenS382;
  int32_t _M0L3endS384;
  int32_t _M0L5startS388;
  int32_t _if__result_2468;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3lenS382 = Moonbit_array_length(_M0L4selfS383);
  if (_M0L3endS385 == 4294967296ll) {
    _M0L3endS384 = _M0L3lenS382;
  } else {
    int64_t _M0L7_2aSomeS386 = _M0L3endS385;
    int32_t _M0L6_2aendS387 = (int32_t)_M0L7_2aSomeS386;
    if (_M0L6_2aendS387 < 0) {
      _M0L3endS384 = _M0L3lenS382 + _M0L6_2aendS387;
    } else {
      _M0L3endS384 = _M0L6_2aendS387;
    }
  }
  if (_M0L5startS389 < 0) {
    _M0L5startS388 = _M0L3lenS382 + _M0L5startS389;
  } else {
    _M0L5startS388 = _M0L5startS389;
  }
  if (_M0L5startS388 >= 0) {
    if (_M0L5startS388 <= _M0L3endS384) {
      _if__result_2468 = _M0L3endS384 <= _M0L3lenS382;
    } else {
      _if__result_2468 = 0;
    }
  } else {
    _if__result_2468 = 0;
  }
  if (_if__result_2468) {
    moonbit_bytes_t _M0L7_2abindS390 = _M0L4selfS383;
    int32_t _M0L7_2abindS391 = _M0L3endS384 - _M0L5startS388;
    int32_t _M0L6_2atmpS1242 = _M0L5startS388 + _M0L7_2abindS391;
    return (struct _M0TPB9ArrayViewGyE){_M0L5startS388,
                                          _M0L6_2atmpS1242,
                                          _M0L7_2abindS390};
  } else {
    moonbit_decref(_M0L4selfS383);
    #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGRPB9ArrayViewGyEE((moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_16.data);
  }
}

int32_t _M0MPC15array9ArrayView2atGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS381,
  int32_t _M0L5indexS380
) {
  int32_t _if__result_2469;
  #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  if (_M0L5indexS380 >= 0) {
    int32_t _M0L3endS1229 = _M0L4selfS381.$2;
    int32_t _M0L5startS1230 = _M0L4selfS381.$1;
    int32_t _M0L6_2atmpS1228 = _M0L3endS1229 - _M0L5startS1230;
    _if__result_2469 = _M0L5indexS380 < _M0L6_2atmpS1228;
  } else {
    _if__result_2469 = 0;
  }
  if (_if__result_2469) {
    moonbit_bytes_t _M0L8_2afieldS2273 = _M0L4selfS381.$0;
    moonbit_bytes_t _M0L3bufS1231 = _M0L8_2afieldS2273;
    int32_t _M0L8_2afieldS2272 = _M0L4selfS381.$1;
    int32_t _M0L5startS1233 = _M0L8_2afieldS2272;
    int32_t _M0L6_2atmpS1232 = _M0L5startS1233 + _M0L5indexS380;
    int32_t _M0L6_2atmpS2271;
    if (
      _M0L6_2atmpS1232 < 0
      || _M0L6_2atmpS1232 >= Moonbit_array_length(_M0L3bufS1231)
    ) {
      #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2271 = (int32_t)_M0L3bufS1231[_M0L6_2atmpS1232];
    moonbit_decref(_M0L3bufS1231);
    return _M0L6_2atmpS2271;
  } else {
    int32_t _M0L3endS1240 = _M0L4selfS381.$2;
    int32_t _M0L8_2afieldS2277 = _M0L4selfS381.$1;
    int32_t _M0L5startS1241;
    int32_t _M0L6_2atmpS1239;
    moonbit_string_t _M0L6_2atmpS1238;
    moonbit_string_t _M0L6_2atmpS2276;
    moonbit_string_t _M0L6_2atmpS1237;
    moonbit_string_t _M0L6_2atmpS2275;
    moonbit_string_t _M0L6_2atmpS1235;
    moonbit_string_t _M0L6_2atmpS1236;
    moonbit_string_t _M0L6_2atmpS2274;
    moonbit_string_t _M0L6_2atmpS1234;
    moonbit_decref(_M0L4selfS381.$0);
    _M0L5startS1241 = _M0L8_2afieldS2277;
    _M0L6_2atmpS1239 = _M0L3endS1240 - _M0L5startS1241;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS1238
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS1239);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2276
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_17.data, _M0L6_2atmpS1238);
    moonbit_decref(_M0L6_2atmpS1238);
    _M0L6_2atmpS1237 = _M0L6_2atmpS2276;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2275
    = moonbit_add_string(_M0L6_2atmpS1237, (moonbit_string_t)moonbit_string_literal_18.data);
    moonbit_decref(_M0L6_2atmpS1237);
    _M0L6_2atmpS1235 = _M0L6_2atmpS2275;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS1236
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS380);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2274 = moonbit_add_string(_M0L6_2atmpS1235, _M0L6_2atmpS1236);
    moonbit_decref(_M0L6_2atmpS1235);
    moonbit_decref(_M0L6_2atmpS1236);
    _M0L6_2atmpS1234 = _M0L6_2atmpS2274;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGyE(_M0L6_2atmpS1234, (moonbit_string_t)moonbit_string_literal_19.data);
  }
}

struct moonbit_result_2 _M0MPC15array10FixedArray3mapGzsEHRPC28encoding4utf89Malformed(
  moonbit_bytes_t* _M0L4selfS374,
  struct _M0TWzEsQRPC15error5Error* _M0L1fS376
) {
  int32_t _M0L6_2atmpS1216;
  int32_t _M0L6_2atmpS1223;
  moonbit_bytes_t _M0L6_2atmpS2280;
  moonbit_bytes_t _M0L6_2atmpS1225;
  struct moonbit_result_0 _tmp_2471;
  moonbit_string_t _M0L6_2atmpS1224;
  moonbit_string_t* _M0L3resS375;
  int32_t _M0L7_2abindS377;
  int32_t _M0L1iS378;
  struct moonbit_result_2 _result_2476;
  #line 479 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L6_2atmpS1216 = Moonbit_array_length(_M0L4selfS374);
  if (_M0L6_2atmpS1216 == 0) {
    moonbit_string_t* _M0L6_2atmpS1217;
    struct moonbit_result_2 _result_2470;
    moonbit_decref(_M0L1fS376);
    moonbit_decref(_M0L4selfS374);
    _M0L6_2atmpS1217 = (moonbit_string_t*)moonbit_empty_ref_array;
    _result_2470.tag = 1;
    _result_2470.data.ok = _M0L6_2atmpS1217;
    return _result_2470;
  }
  _M0L6_2atmpS1223 = Moonbit_array_length(_M0L4selfS374);
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L4selfS374)) {
    #line 486 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS2280 = (moonbit_bytes_t)_M0L4selfS374[0];
  _M0L6_2atmpS1225 = _M0L6_2atmpS2280;
  moonbit_incref(_M0L6_2atmpS1225);
  moonbit_incref(_M0L1fS376);
  #line 486 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _tmp_2471 = _M0L1fS376->code(_M0L1fS376, _M0L6_2atmpS1225);
  if (_tmp_2471.tag) {
    moonbit_string_t const _M0L5_2aokS1226 = _tmp_2471.data.ok;
    _M0L6_2atmpS1224 = _M0L5_2aokS1226;
  } else {
    void* const _M0L6_2aerrS1227 = _tmp_2471.data.err;
    struct moonbit_result_2 _result_2472;
    moonbit_decref(_M0L1fS376);
    moonbit_decref(_M0L4selfS374);
    _result_2472.tag = 0;
    _result_2472.data.err = _M0L6_2aerrS1227;
    return _result_2472;
  }
  _M0L3resS375
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L6_2atmpS1223, _M0L6_2atmpS1224);
  _M0L7_2abindS377 = Moonbit_array_length(_M0L4selfS374);
  _M0L1iS378 = 1;
  while (1) {
    if (_M0L1iS378 < _M0L7_2abindS377) {
      moonbit_bytes_t _M0L6_2atmpS2279;
      moonbit_bytes_t _M0L6_2atmpS1219;
      struct moonbit_result_0 _tmp_2474;
      moonbit_string_t _M0L6_2atmpS1218;
      moonbit_string_t _M0L6_2aoldS2278;
      int32_t _M0L6_2atmpS1222;
      if (
        _M0L1iS378 < 0 || _M0L1iS378 >= Moonbit_array_length(_M0L4selfS374)
      ) {
        #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2279 = (moonbit_bytes_t)_M0L4selfS374[_M0L1iS378];
      _M0L6_2atmpS1219 = _M0L6_2atmpS2279;
      moonbit_incref(_M0L6_2atmpS1219);
      moonbit_incref(_M0L1fS376);
      #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
      _tmp_2474 = _M0L1fS376->code(_M0L1fS376, _M0L6_2atmpS1219);
      if (_tmp_2474.tag) {
        moonbit_string_t const _M0L5_2aokS1220 = _tmp_2474.data.ok;
        _M0L6_2atmpS1218 = _M0L5_2aokS1220;
      } else {
        void* const _M0L6_2aerrS1221 = _tmp_2474.data.err;
        struct moonbit_result_2 _result_2475;
        moonbit_decref(_M0L1fS376);
        moonbit_decref(_M0L3resS375);
        moonbit_decref(_M0L4selfS374);
        _result_2475.tag = 0;
        _result_2475.data.err = _M0L6_2aerrS1221;
        return _result_2475;
      }
      if (_M0L1iS378 < 0 || _M0L1iS378 >= Moonbit_array_length(_M0L3resS375)) {
        #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        moonbit_panic();
      }
      _M0L6_2aoldS2278 = (moonbit_string_t)_M0L3resS375[_M0L1iS378];
      moonbit_decref(_M0L6_2aoldS2278);
      _M0L3resS375[_M0L1iS378] = _M0L6_2atmpS1218;
      _M0L6_2atmpS1222 = _M0L1iS378 + 1;
      _M0L1iS378 = _M0L6_2atmpS1222;
      continue;
    } else {
      moonbit_decref(_M0L1fS376);
      moonbit_decref(_M0L4selfS374);
    }
    break;
  }
  _result_2476.tag = 1;
  _result_2476.data.ok = _M0L3resS375;
  return _result_2476;
}

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS373,
  struct _M0TPB6Logger _M0L6loggerS372
) {
  struct _M0TWEOs* _M0L6_2atmpS1215;
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1215 = _M0MPC15array5Array4iterGsE(_M0L4selfS373);
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0MPB6Logger19write__iter_2einnerGsE(_M0L6loggerS372, _M0L6_2atmpS1215, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, (moonbit_string_t)moonbit_string_literal_22.data, 0);
  return 0;
}

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS371
) {
  moonbit_string_t* _M0L8_2afieldS2282;
  moonbit_string_t* _M0L3bufS1213;
  int32_t _M0L8_2afieldS2281;
  int32_t _M0L6_2acntS2384;
  int32_t _M0L3lenS1214;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1212;
  #line 1651 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS2282 = _M0L4selfS371->$0;
  _M0L3bufS1213 = _M0L8_2afieldS2282;
  _M0L8_2afieldS2281 = _M0L4selfS371->$1;
  _M0L6_2acntS2384 = Moonbit_object_header(_M0L4selfS371)->rc;
  if (_M0L6_2acntS2384 > 1) {
    int32_t _M0L11_2anew__cntS2385 = _M0L6_2acntS2384 - 1;
    Moonbit_object_header(_M0L4selfS371)->rc = _M0L11_2anew__cntS2385;
    moonbit_incref(_M0L3bufS1213);
  } else if (_M0L6_2acntS2384 == 1) {
    #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_free(_M0L4selfS371);
  }
  _M0L3lenS1214 = _M0L8_2afieldS2281;
  _M0L6_2atmpS1212
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1214, _M0L3bufS1213
  };
  #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1212);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS369
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS368;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__* _closure_2477;
  struct _M0TWEOs* _M0L6_2atmpS1200;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS368
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS368)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS368->$0 = 0;
  _closure_2477
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__));
  Moonbit_object_header(_closure_2477)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__, $0_0) >> 2, 2, 0);
  _closure_2477->code = &_M0MPC15array9ArrayView4iterGsEC1201l570;
  _closure_2477->$0_0 = _M0L4selfS369.$0;
  _closure_2477->$0_1 = _M0L4selfS369.$1;
  _closure_2477->$0_2 = _M0L4selfS369.$2;
  _closure_2477->$1 = _M0L1iS368;
  _M0L6_2atmpS1200 = (struct _M0TWEOs*)_closure_2477;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1200);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1201l570(
  struct _M0TWEOs* _M0L6_2aenvS1202
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__* _M0L14_2acasted__envS1203;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2287;
  struct _M0TPC13ref3RefGiE* _M0L1iS368;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2286;
  int32_t _M0L6_2acntS2386;
  struct _M0TPB9ArrayViewGsE _M0L4selfS369;
  int32_t _M0L3valS1204;
  int32_t _M0L6_2atmpS1205;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1203
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1201__l570__*)_M0L6_2aenvS1202;
  _M0L8_2afieldS2287 = _M0L14_2acasted__envS1203->$1;
  _M0L1iS368 = _M0L8_2afieldS2287;
  _M0L8_2afieldS2286
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1203->$0_1,
      _M0L14_2acasted__envS1203->$0_2,
      _M0L14_2acasted__envS1203->$0_0
  };
  _M0L6_2acntS2386 = Moonbit_object_header(_M0L14_2acasted__envS1203)->rc;
  if (_M0L6_2acntS2386 > 1) {
    int32_t _M0L11_2anew__cntS2387 = _M0L6_2acntS2386 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1203)->rc
    = _M0L11_2anew__cntS2387;
    moonbit_incref(_M0L1iS368);
    moonbit_incref(_M0L8_2afieldS2286.$0);
  } else if (_M0L6_2acntS2386 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1203);
  }
  _M0L4selfS369 = _M0L8_2afieldS2286;
  _M0L3valS1204 = _M0L1iS368->$0;
  moonbit_incref(_M0L4selfS369.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1205 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS369);
  if (_M0L3valS1204 < _M0L6_2atmpS1205) {
    moonbit_string_t* _M0L8_2afieldS2285 = _M0L4selfS369.$0;
    moonbit_string_t* _M0L3bufS1208 = _M0L8_2afieldS2285;
    int32_t _M0L8_2afieldS2284 = _M0L4selfS369.$1;
    int32_t _M0L5startS1210 = _M0L8_2afieldS2284;
    int32_t _M0L3valS1211 = _M0L1iS368->$0;
    int32_t _M0L6_2atmpS1209 = _M0L5startS1210 + _M0L3valS1211;
    moonbit_string_t _M0L6_2atmpS2283 =
      (moonbit_string_t)_M0L3bufS1208[_M0L6_2atmpS1209];
    moonbit_string_t _M0L4elemS370;
    int32_t _M0L3valS1207;
    int32_t _M0L6_2atmpS1206;
    moonbit_incref(_M0L6_2atmpS2283);
    moonbit_decref(_M0L3bufS1208);
    _M0L4elemS370 = _M0L6_2atmpS2283;
    _M0L3valS1207 = _M0L1iS368->$0;
    _M0L6_2atmpS1206 = _M0L3valS1207 + 1;
    _M0L1iS368->$0 = _M0L6_2atmpS1206;
    moonbit_decref(_M0L1iS368);
    return _M0L4elemS370;
  } else {
    moonbit_decref(_M0L4selfS369.$0);
    moonbit_decref(_M0L1iS368);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS367
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS367;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS366,
  struct _M0TPB6Logger _M0L6loggerS365
) {
  moonbit_string_t _M0L6_2atmpS1199;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1199 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS366, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS365.$0->$method_0(_M0L6loggerS365.$1, _M0L6_2atmpS1199);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS359,
  moonbit_string_t _M0L5valueS361
) {
  int32_t _M0L3lenS1189;
  moonbit_string_t* _M0L6_2atmpS1191;
  int32_t _M0L6_2atmpS2290;
  int32_t _M0L6_2atmpS1190;
  int32_t _M0L6lengthS360;
  moonbit_string_t* _M0L8_2afieldS2289;
  moonbit_string_t* _M0L3bufS1192;
  moonbit_string_t _M0L6_2aoldS2288;
  int32_t _M0L6_2atmpS1193;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1189 = _M0L4selfS359->$1;
  moonbit_incref(_M0L4selfS359);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1191 = _M0MPC15array5Array6bufferGsE(_M0L4selfS359);
  _M0L6_2atmpS2290 = Moonbit_array_length(_M0L6_2atmpS1191);
  moonbit_decref(_M0L6_2atmpS1191);
  _M0L6_2atmpS1190 = _M0L6_2atmpS2290;
  if (_M0L3lenS1189 == _M0L6_2atmpS1190) {
    moonbit_incref(_M0L4selfS359);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS359);
  }
  _M0L6lengthS360 = _M0L4selfS359->$1;
  _M0L8_2afieldS2289 = _M0L4selfS359->$0;
  _M0L3bufS1192 = _M0L8_2afieldS2289;
  _M0L6_2aoldS2288 = (moonbit_string_t)_M0L3bufS1192[_M0L6lengthS360];
  moonbit_decref(_M0L6_2aoldS2288);
  _M0L3bufS1192[_M0L6lengthS360] = _M0L5valueS361;
  _M0L6_2atmpS1193 = _M0L6lengthS360 + 1;
  _M0L4selfS359->$1 = _M0L6_2atmpS1193;
  moonbit_decref(_M0L4selfS359);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS362,
  struct _M0TUsiE* _M0L5valueS364
) {
  int32_t _M0L3lenS1194;
  struct _M0TUsiE** _M0L6_2atmpS1196;
  int32_t _M0L6_2atmpS2293;
  int32_t _M0L6_2atmpS1195;
  int32_t _M0L6lengthS363;
  struct _M0TUsiE** _M0L8_2afieldS2292;
  struct _M0TUsiE** _M0L3bufS1197;
  struct _M0TUsiE* _M0L6_2aoldS2291;
  int32_t _M0L6_2atmpS1198;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1194 = _M0L4selfS362->$1;
  moonbit_incref(_M0L4selfS362);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1196 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS362);
  _M0L6_2atmpS2293 = Moonbit_array_length(_M0L6_2atmpS1196);
  moonbit_decref(_M0L6_2atmpS1196);
  _M0L6_2atmpS1195 = _M0L6_2atmpS2293;
  if (_M0L3lenS1194 == _M0L6_2atmpS1195) {
    moonbit_incref(_M0L4selfS362);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS362);
  }
  _M0L6lengthS363 = _M0L4selfS362->$1;
  _M0L8_2afieldS2292 = _M0L4selfS362->$0;
  _M0L3bufS1197 = _M0L8_2afieldS2292;
  _M0L6_2aoldS2291 = (struct _M0TUsiE*)_M0L3bufS1197[_M0L6lengthS363];
  if (_M0L6_2aoldS2291) {
    moonbit_decref(_M0L6_2aoldS2291);
  }
  _M0L3bufS1197[_M0L6lengthS363] = _M0L5valueS364;
  _M0L6_2atmpS1198 = _M0L6lengthS363 + 1;
  _M0L4selfS362->$1 = _M0L6_2atmpS1198;
  moonbit_decref(_M0L4selfS362);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS354) {
  int32_t _M0L8old__capS353;
  int32_t _M0L8new__capS355;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS353 = _M0L4selfS354->$1;
  if (_M0L8old__capS353 == 0) {
    _M0L8new__capS355 = 8;
  } else {
    _M0L8new__capS355 = _M0L8old__capS353 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS354, _M0L8new__capS355);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS357
) {
  int32_t _M0L8old__capS356;
  int32_t _M0L8new__capS358;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS356 = _M0L4selfS357->$1;
  if (_M0L8old__capS356 == 0) {
    _M0L8new__capS358 = 8;
  } else {
    _M0L8new__capS358 = _M0L8old__capS356 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS357, _M0L8new__capS358);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS344,
  int32_t _M0L13new__capacityS342
) {
  moonbit_string_t* _M0L8new__bufS341;
  moonbit_string_t* _M0L8_2afieldS2295;
  moonbit_string_t* _M0L8old__bufS343;
  int32_t _M0L8old__capS345;
  int32_t _M0L9copy__lenS346;
  moonbit_string_t* _M0L6_2aoldS2294;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS341
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS342, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2295 = _M0L4selfS344->$0;
  _M0L8old__bufS343 = _M0L8_2afieldS2295;
  _M0L8old__capS345 = Moonbit_array_length(_M0L8old__bufS343);
  if (_M0L8old__capS345 < _M0L13new__capacityS342) {
    _M0L9copy__lenS346 = _M0L8old__capS345;
  } else {
    _M0L9copy__lenS346 = _M0L13new__capacityS342;
  }
  moonbit_incref(_M0L8old__bufS343);
  moonbit_incref(_M0L8new__bufS341);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS341, 0, _M0L8old__bufS343, 0, _M0L9copy__lenS346);
  _M0L6_2aoldS2294 = _M0L4selfS344->$0;
  moonbit_decref(_M0L6_2aoldS2294);
  _M0L4selfS344->$0 = _M0L8new__bufS341;
  moonbit_decref(_M0L4selfS344);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS350,
  int32_t _M0L13new__capacityS348
) {
  struct _M0TUsiE** _M0L8new__bufS347;
  struct _M0TUsiE** _M0L8_2afieldS2297;
  struct _M0TUsiE** _M0L8old__bufS349;
  int32_t _M0L8old__capS351;
  int32_t _M0L9copy__lenS352;
  struct _M0TUsiE** _M0L6_2aoldS2296;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS347
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS348, 0);
  _M0L8_2afieldS2297 = _M0L4selfS350->$0;
  _M0L8old__bufS349 = _M0L8_2afieldS2297;
  _M0L8old__capS351 = Moonbit_array_length(_M0L8old__bufS349);
  if (_M0L8old__capS351 < _M0L13new__capacityS348) {
    _M0L9copy__lenS352 = _M0L8old__capS351;
  } else {
    _M0L9copy__lenS352 = _M0L13new__capacityS348;
  }
  moonbit_incref(_M0L8old__bufS349);
  moonbit_incref(_M0L8new__bufS347);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS347, 0, _M0L8old__bufS349, 0, _M0L9copy__lenS352);
  _M0L6_2aoldS2296 = _M0L4selfS350->$0;
  moonbit_decref(_M0L6_2aoldS2296);
  _M0L4selfS350->$0 = _M0L8new__bufS347;
  moonbit_decref(_M0L4selfS350);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS340
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS340 == 0) {
    moonbit_string_t* _M0L6_2atmpS1187 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2478 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2478)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2478->$0 = _M0L6_2atmpS1187;
    _block_2478->$1 = 0;
    return _block_2478;
  } else {
    moonbit_string_t* _M0L6_2atmpS1188 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS340, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2479 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2479)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2479->$0 = _M0L6_2atmpS1188;
    _block_2479->$1 = 0;
    return _block_2479;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS338,
  struct _M0TPC16string10StringView _M0L3strS339
) {
  int32_t _M0L3lenS1175;
  int32_t _M0L6_2atmpS1177;
  int32_t _M0L6_2atmpS1176;
  int32_t _M0L6_2atmpS1174;
  moonbit_bytes_t _M0L8_2afieldS2298;
  moonbit_bytes_t _M0L4dataS1178;
  int32_t _M0L3lenS1179;
  moonbit_string_t _M0L6_2atmpS1180;
  int32_t _M0L6_2atmpS1181;
  int32_t _M0L6_2atmpS1182;
  int32_t _M0L3lenS1184;
  int32_t _M0L6_2atmpS1186;
  int32_t _M0L6_2atmpS1185;
  int32_t _M0L6_2atmpS1183;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1175 = _M0L4selfS338->$1;
  moonbit_incref(_M0L3strS339.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1177 = _M0MPC16string10StringView6length(_M0L3strS339);
  _M0L6_2atmpS1176 = _M0L6_2atmpS1177 * 2;
  _M0L6_2atmpS1174 = _M0L3lenS1175 + _M0L6_2atmpS1176;
  moonbit_incref(_M0L4selfS338);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS338, _M0L6_2atmpS1174);
  _M0L8_2afieldS2298 = _M0L4selfS338->$0;
  _M0L4dataS1178 = _M0L8_2afieldS2298;
  _M0L3lenS1179 = _M0L4selfS338->$1;
  moonbit_incref(_M0L4dataS1178);
  moonbit_incref(_M0L3strS339.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1180 = _M0MPC16string10StringView4data(_M0L3strS339);
  moonbit_incref(_M0L3strS339.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1181 = _M0MPC16string10StringView13start__offset(_M0L3strS339);
  moonbit_incref(_M0L3strS339.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1182 = _M0MPC16string10StringView6length(_M0L3strS339);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1178, _M0L3lenS1179, _M0L6_2atmpS1180, _M0L6_2atmpS1181, _M0L6_2atmpS1182);
  _M0L3lenS1184 = _M0L4selfS338->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1186 = _M0MPC16string10StringView6length(_M0L3strS339);
  _M0L6_2atmpS1185 = _M0L6_2atmpS1186 * 2;
  _M0L6_2atmpS1183 = _M0L3lenS1184 + _M0L6_2atmpS1185;
  _M0L4selfS338->$1 = _M0L6_2atmpS1183;
  moonbit_decref(_M0L4selfS338);
  return 0;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS336
) {
  int32_t _M0L3endS1170;
  int32_t _M0L8_2afieldS2299;
  int32_t _M0L5startS1171;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1170 = _M0L4selfS336.$2;
  _M0L8_2afieldS2299 = _M0L4selfS336.$1;
  moonbit_decref(_M0L4selfS336.$0);
  _M0L5startS1171 = _M0L8_2afieldS2299;
  return _M0L3endS1170 - _M0L5startS1171;
}

int32_t _M0MPC15array9ArrayView6lengthGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS337
) {
  int32_t _M0L3endS1172;
  int32_t _M0L8_2afieldS2300;
  int32_t _M0L5startS1173;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1172 = _M0L4selfS337.$2;
  _M0L8_2afieldS2300 = _M0L4selfS337.$1;
  moonbit_decref(_M0L4selfS337.$0);
  _M0L5startS1173 = _M0L8_2afieldS2300;
  return _M0L3endS1172 - _M0L5startS1173;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS334,
  int64_t _M0L19start__offset_2eoptS332,
  int64_t _M0L11end__offsetS335
) {
  int32_t _M0L13start__offsetS331;
  if (_M0L19start__offset_2eoptS332 == 4294967296ll) {
    _M0L13start__offsetS331 = 0;
  } else {
    int64_t _M0L7_2aSomeS333 = _M0L19start__offset_2eoptS332;
    _M0L13start__offsetS331 = (int32_t)_M0L7_2aSomeS333;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS334, _M0L13start__offsetS331, _M0L11end__offsetS335);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS329,
  int32_t _M0L13start__offsetS330,
  int64_t _M0L11end__offsetS327
) {
  int32_t _M0L11end__offsetS326;
  int32_t _if__result_2480;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS327 == 4294967296ll) {
    _M0L11end__offsetS326 = Moonbit_array_length(_M0L4selfS329);
  } else {
    int64_t _M0L7_2aSomeS328 = _M0L11end__offsetS327;
    _M0L11end__offsetS326 = (int32_t)_M0L7_2aSomeS328;
  }
  if (_M0L13start__offsetS330 >= 0) {
    if (_M0L13start__offsetS330 <= _M0L11end__offsetS326) {
      int32_t _M0L6_2atmpS1169 = Moonbit_array_length(_M0L4selfS329);
      _if__result_2480 = _M0L11end__offsetS326 <= _M0L6_2atmpS1169;
    } else {
      _if__result_2480 = 0;
    }
  } else {
    _if__result_2480 = 0;
  }
  if (_if__result_2480) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS330,
                                                 _M0L11end__offsetS326,
                                                 _M0L4selfS329};
  } else {
    moonbit_decref(_M0L4selfS329);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_13.data, (moonbit_string_t)moonbit_string_literal_23.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS321
) {
  int32_t _M0L5startS320;
  int32_t _M0L3endS322;
  struct _M0TPC13ref3RefGiE* _M0L5indexS323;
  struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__* _closure_2481;
  struct _M0TWEOc* _M0L6_2atmpS1148;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS320 = _M0L4selfS321.$1;
  _M0L3endS322 = _M0L4selfS321.$2;
  _M0L5indexS323
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS323)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS323->$0 = _M0L5startS320;
  _closure_2481
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__));
  Moonbit_object_header(_closure_2481)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__, $0) >> 2, 2, 0);
  _closure_2481->code = &_M0MPC16string10StringView4iterC1149l198;
  _closure_2481->$0 = _M0L5indexS323;
  _closure_2481->$1 = _M0L3endS322;
  _closure_2481->$2_0 = _M0L4selfS321.$0;
  _closure_2481->$2_1 = _M0L4selfS321.$1;
  _closure_2481->$2_2 = _M0L4selfS321.$2;
  _M0L6_2atmpS1148 = (struct _M0TWEOc*)_closure_2481;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1148);
}

int32_t _M0MPC16string10StringView4iterC1149l198(
  struct _M0TWEOc* _M0L6_2aenvS1150
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__* _M0L14_2acasted__envS1151;
  struct _M0TPC16string10StringView _M0L8_2afieldS2306;
  struct _M0TPC16string10StringView _M0L4selfS321;
  int32_t _M0L3endS322;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2305;
  int32_t _M0L6_2acntS2388;
  struct _M0TPC13ref3RefGiE* _M0L5indexS323;
  int32_t _M0L3valS1152;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS1151
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1149__l198__*)_M0L6_2aenvS1150;
  _M0L8_2afieldS2306
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1151->$2_1,
      _M0L14_2acasted__envS1151->$2_2,
      _M0L14_2acasted__envS1151->$2_0
  };
  _M0L4selfS321 = _M0L8_2afieldS2306;
  _M0L3endS322 = _M0L14_2acasted__envS1151->$1;
  _M0L8_2afieldS2305 = _M0L14_2acasted__envS1151->$0;
  _M0L6_2acntS2388 = Moonbit_object_header(_M0L14_2acasted__envS1151)->rc;
  if (_M0L6_2acntS2388 > 1) {
    int32_t _M0L11_2anew__cntS2389 = _M0L6_2acntS2388 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1151)->rc
    = _M0L11_2anew__cntS2389;
    moonbit_incref(_M0L4selfS321.$0);
    moonbit_incref(_M0L8_2afieldS2305);
  } else if (_M0L6_2acntS2388 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS1151);
  }
  _M0L5indexS323 = _M0L8_2afieldS2305;
  _M0L3valS1152 = _M0L5indexS323->$0;
  if (_M0L3valS1152 < _M0L3endS322) {
    moonbit_string_t _M0L8_2afieldS2304 = _M0L4selfS321.$0;
    moonbit_string_t _M0L3strS1167 = _M0L8_2afieldS2304;
    int32_t _M0L3valS1168 = _M0L5indexS323->$0;
    int32_t _M0L6_2atmpS2303 = _M0L3strS1167[_M0L3valS1168];
    int32_t _M0L2c1S324 = _M0L6_2atmpS2303;
    int32_t _if__result_2482;
    int32_t _M0L3valS1165;
    int32_t _M0L6_2atmpS1164;
    int32_t _M0L6_2atmpS1166;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S324)) {
      int32_t _M0L3valS1155 = _M0L5indexS323->$0;
      int32_t _M0L6_2atmpS1153 = _M0L3valS1155 + 1;
      int32_t _M0L3endS1154 = _M0L4selfS321.$2;
      _if__result_2482 = _M0L6_2atmpS1153 < _M0L3endS1154;
    } else {
      _if__result_2482 = 0;
    }
    if (_if__result_2482) {
      moonbit_string_t _M0L8_2afieldS2302 = _M0L4selfS321.$0;
      moonbit_string_t _M0L3strS1161 = _M0L8_2afieldS2302;
      int32_t _M0L3valS1163 = _M0L5indexS323->$0;
      int32_t _M0L6_2atmpS1162 = _M0L3valS1163 + 1;
      int32_t _M0L6_2atmpS2301 = _M0L3strS1161[_M0L6_2atmpS1162];
      int32_t _M0L2c2S325;
      moonbit_decref(_M0L3strS1161);
      _M0L2c2S325 = _M0L6_2atmpS2301;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S325)) {
        int32_t _M0L3valS1157 = _M0L5indexS323->$0;
        int32_t _M0L6_2atmpS1156 = _M0L3valS1157 + 2;
        int32_t _M0L6_2atmpS1159;
        int32_t _M0L6_2atmpS1160;
        int32_t _M0L6_2atmpS1158;
        _M0L5indexS323->$0 = _M0L6_2atmpS1156;
        moonbit_decref(_M0L5indexS323);
        _M0L6_2atmpS1159 = (int32_t)_M0L2c1S324;
        _M0L6_2atmpS1160 = (int32_t)_M0L2c2S325;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS1158
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1159, _M0L6_2atmpS1160);
        return _M0L6_2atmpS1158;
      }
    } else {
      moonbit_decref(_M0L4selfS321.$0);
    }
    _M0L3valS1165 = _M0L5indexS323->$0;
    _M0L6_2atmpS1164 = _M0L3valS1165 + 1;
    _M0L5indexS323->$0 = _M0L6_2atmpS1164;
    moonbit_decref(_M0L5indexS323);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS1166 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S324);
    return _M0L6_2atmpS1166;
  } else {
    moonbit_decref(_M0L5indexS323);
    moonbit_decref(_M0L4selfS321.$0);
    return -1;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS312,
  struct _M0TPB6Logger _M0L6loggerS310
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS311;
  int32_t _M0L3lenS313;
  int32_t _M0L1iS314;
  int32_t _M0L3segS315;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS310.$1) {
    moonbit_incref(_M0L6loggerS310.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS310.$0->$method_3(_M0L6loggerS310.$1, 34);
  moonbit_incref(_M0L4selfS312);
  if (_M0L6loggerS310.$1) {
    moonbit_incref(_M0L6loggerS310.$1);
  }
  _M0L6_2aenvS311
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS311)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS311->$0 = _M0L4selfS312;
  _M0L6_2aenvS311->$1_0 = _M0L6loggerS310.$0;
  _M0L6_2aenvS311->$1_1 = _M0L6loggerS310.$1;
  _M0L3lenS313 = Moonbit_array_length(_M0L4selfS312);
  _M0L1iS314 = 0;
  _M0L3segS315 = 0;
  _2afor_316:;
  while (1) {
    int32_t _M0L4codeS317;
    int32_t _M0L1cS319;
    int32_t _M0L6_2atmpS1132;
    int32_t _M0L6_2atmpS1133;
    int32_t _M0L6_2atmpS1134;
    int32_t _tmp_2486;
    int32_t _tmp_2487;
    if (_M0L1iS314 >= _M0L3lenS313) {
      moonbit_decref(_M0L4selfS312);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
      break;
    }
    _M0L4codeS317 = _M0L4selfS312[_M0L1iS314];
    switch (_M0L4codeS317) {
      case 34: {
        _M0L1cS319 = _M0L4codeS317;
        goto join_318;
        break;
      }
      
      case 92: {
        _M0L1cS319 = _M0L4codeS317;
        goto join_318;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1135;
        int32_t _M0L6_2atmpS1136;
        moonbit_incref(_M0L6_2aenvS311);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
        if (_M0L6loggerS310.$1) {
          moonbit_incref(_M0L6loggerS310.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS310.$0->$method_0(_M0L6loggerS310.$1, (moonbit_string_t)moonbit_string_literal_24.data);
        _M0L6_2atmpS1135 = _M0L1iS314 + 1;
        _M0L6_2atmpS1136 = _M0L1iS314 + 1;
        _M0L1iS314 = _M0L6_2atmpS1135;
        _M0L3segS315 = _M0L6_2atmpS1136;
        goto _2afor_316;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1137;
        int32_t _M0L6_2atmpS1138;
        moonbit_incref(_M0L6_2aenvS311);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
        if (_M0L6loggerS310.$1) {
          moonbit_incref(_M0L6loggerS310.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS310.$0->$method_0(_M0L6loggerS310.$1, (moonbit_string_t)moonbit_string_literal_25.data);
        _M0L6_2atmpS1137 = _M0L1iS314 + 1;
        _M0L6_2atmpS1138 = _M0L1iS314 + 1;
        _M0L1iS314 = _M0L6_2atmpS1137;
        _M0L3segS315 = _M0L6_2atmpS1138;
        goto _2afor_316;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1139;
        int32_t _M0L6_2atmpS1140;
        moonbit_incref(_M0L6_2aenvS311);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
        if (_M0L6loggerS310.$1) {
          moonbit_incref(_M0L6loggerS310.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS310.$0->$method_0(_M0L6loggerS310.$1, (moonbit_string_t)moonbit_string_literal_26.data);
        _M0L6_2atmpS1139 = _M0L1iS314 + 1;
        _M0L6_2atmpS1140 = _M0L1iS314 + 1;
        _M0L1iS314 = _M0L6_2atmpS1139;
        _M0L3segS315 = _M0L6_2atmpS1140;
        goto _2afor_316;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1141;
        int32_t _M0L6_2atmpS1142;
        moonbit_incref(_M0L6_2aenvS311);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
        if (_M0L6loggerS310.$1) {
          moonbit_incref(_M0L6loggerS310.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS310.$0->$method_0(_M0L6loggerS310.$1, (moonbit_string_t)moonbit_string_literal_27.data);
        _M0L6_2atmpS1141 = _M0L1iS314 + 1;
        _M0L6_2atmpS1142 = _M0L1iS314 + 1;
        _M0L1iS314 = _M0L6_2atmpS1141;
        _M0L3segS315 = _M0L6_2atmpS1142;
        goto _2afor_316;
        break;
      }
      default: {
        if (_M0L4codeS317 < 32) {
          int32_t _M0L6_2atmpS1144;
          moonbit_string_t _M0L6_2atmpS1143;
          int32_t _M0L6_2atmpS1145;
          int32_t _M0L6_2atmpS1146;
          moonbit_incref(_M0L6_2aenvS311);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
          if (_M0L6loggerS310.$1) {
            moonbit_incref(_M0L6loggerS310.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS310.$0->$method_0(_M0L6loggerS310.$1, (moonbit_string_t)moonbit_string_literal_28.data);
          _M0L6_2atmpS1144 = _M0L4codeS317 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1143 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1144);
          if (_M0L6loggerS310.$1) {
            moonbit_incref(_M0L6loggerS310.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS310.$0->$method_0(_M0L6loggerS310.$1, _M0L6_2atmpS1143);
          if (_M0L6loggerS310.$1) {
            moonbit_incref(_M0L6loggerS310.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS310.$0->$method_3(_M0L6loggerS310.$1, 125);
          _M0L6_2atmpS1145 = _M0L1iS314 + 1;
          _M0L6_2atmpS1146 = _M0L1iS314 + 1;
          _M0L1iS314 = _M0L6_2atmpS1145;
          _M0L3segS315 = _M0L6_2atmpS1146;
          goto _2afor_316;
        } else {
          int32_t _M0L6_2atmpS1147 = _M0L1iS314 + 1;
          int32_t _tmp_2485 = _M0L3segS315;
          _M0L1iS314 = _M0L6_2atmpS1147;
          _M0L3segS315 = _tmp_2485;
          goto _2afor_316;
        }
        break;
      }
    }
    goto joinlet_2484;
    join_318:;
    moonbit_incref(_M0L6_2aenvS311);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS311, _M0L3segS315, _M0L1iS314);
    if (_M0L6loggerS310.$1) {
      moonbit_incref(_M0L6loggerS310.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS310.$0->$method_3(_M0L6loggerS310.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1132 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS319);
    if (_M0L6loggerS310.$1) {
      moonbit_incref(_M0L6loggerS310.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS310.$0->$method_3(_M0L6loggerS310.$1, _M0L6_2atmpS1132);
    _M0L6_2atmpS1133 = _M0L1iS314 + 1;
    _M0L6_2atmpS1134 = _M0L1iS314 + 1;
    _M0L1iS314 = _M0L6_2atmpS1133;
    _M0L3segS315 = _M0L6_2atmpS1134;
    continue;
    joinlet_2484:;
    _tmp_2486 = _M0L1iS314;
    _tmp_2487 = _M0L3segS315;
    _M0L1iS314 = _tmp_2486;
    _M0L3segS315 = _tmp_2487;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS310.$0->$method_3(_M0L6loggerS310.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS306,
  int32_t _M0L3segS309,
  int32_t _M0L1iS308
) {
  struct _M0TPB6Logger _M0L8_2afieldS2308;
  struct _M0TPB6Logger _M0L6loggerS305;
  moonbit_string_t _M0L8_2afieldS2307;
  int32_t _M0L6_2acntS2390;
  moonbit_string_t _M0L4selfS307;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS2308
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS306->$1_0, _M0L6_2aenvS306->$1_1
  };
  _M0L6loggerS305 = _M0L8_2afieldS2308;
  _M0L8_2afieldS2307 = _M0L6_2aenvS306->$0;
  _M0L6_2acntS2390 = Moonbit_object_header(_M0L6_2aenvS306)->rc;
  if (_M0L6_2acntS2390 > 1) {
    int32_t _M0L11_2anew__cntS2391 = _M0L6_2acntS2390 - 1;
    Moonbit_object_header(_M0L6_2aenvS306)->rc = _M0L11_2anew__cntS2391;
    if (_M0L6loggerS305.$1) {
      moonbit_incref(_M0L6loggerS305.$1);
    }
    moonbit_incref(_M0L8_2afieldS2307);
  } else if (_M0L6_2acntS2390 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS306);
  }
  _M0L4selfS307 = _M0L8_2afieldS2307;
  if (_M0L1iS308 > _M0L3segS309) {
    int32_t _M0L6_2atmpS1131 = _M0L1iS308 - _M0L3segS309;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS305.$0->$method_1(_M0L6loggerS305.$1, _M0L4selfS307, _M0L3segS309, _M0L6_2atmpS1131);
  } else {
    moonbit_decref(_M0L4selfS307);
    if (_M0L6loggerS305.$1) {
      moonbit_decref(_M0L6loggerS305.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS304) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS303;
  int32_t _M0L6_2atmpS1128;
  int32_t _M0L6_2atmpS1127;
  int32_t _M0L6_2atmpS1130;
  int32_t _M0L6_2atmpS1129;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1126;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS303 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1128 = _M0IPC14byte4BytePB3Div3div(_M0L1bS304, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1127
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1128);
  moonbit_incref(_M0L7_2aselfS303);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS303, _M0L6_2atmpS1127);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1130 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS304, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1129
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1130);
  moonbit_incref(_M0L7_2aselfS303);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS303, _M0L6_2atmpS1129);
  _M0L6_2atmpS1126 = _M0L7_2aselfS303;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1126);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS302) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS302 < 10) {
    int32_t _M0L6_2atmpS1123;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1123 = _M0IPC14byte4BytePB3Add3add(_M0L1iS302, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1123);
  } else {
    int32_t _M0L6_2atmpS1125;
    int32_t _M0L6_2atmpS1124;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1125 = _M0IPC14byte4BytePB3Add3add(_M0L1iS302, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1124 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1125, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1124);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS300,
  int32_t _M0L4thatS301
) {
  int32_t _M0L6_2atmpS1121;
  int32_t _M0L6_2atmpS1122;
  int32_t _M0L6_2atmpS1120;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1121 = (int32_t)_M0L4selfS300;
  _M0L6_2atmpS1122 = (int32_t)_M0L4thatS301;
  _M0L6_2atmpS1120 = _M0L6_2atmpS1121 - _M0L6_2atmpS1122;
  return _M0L6_2atmpS1120 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS298,
  int32_t _M0L4thatS299
) {
  int32_t _M0L6_2atmpS1118;
  int32_t _M0L6_2atmpS1119;
  int32_t _M0L6_2atmpS1117;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1118 = (int32_t)_M0L4selfS298;
  _M0L6_2atmpS1119 = (int32_t)_M0L4thatS299;
  _M0L6_2atmpS1117 = _M0L6_2atmpS1118 % _M0L6_2atmpS1119;
  return _M0L6_2atmpS1117 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS296,
  int32_t _M0L4thatS297
) {
  int32_t _M0L6_2atmpS1115;
  int32_t _M0L6_2atmpS1116;
  int32_t _M0L6_2atmpS1114;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1115 = (int32_t)_M0L4selfS296;
  _M0L6_2atmpS1116 = (int32_t)_M0L4thatS297;
  _M0L6_2atmpS1114 = _M0L6_2atmpS1115 / _M0L6_2atmpS1116;
  return _M0L6_2atmpS1114 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS294,
  int32_t _M0L4thatS295
) {
  int32_t _M0L6_2atmpS1112;
  int32_t _M0L6_2atmpS1113;
  int32_t _M0L6_2atmpS1111;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1112 = (int32_t)_M0L4selfS294;
  _M0L6_2atmpS1113 = (int32_t)_M0L4thatS295;
  _M0L6_2atmpS1111 = _M0L6_2atmpS1112 + _M0L6_2atmpS1113;
  return _M0L6_2atmpS1111 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS291,
  int32_t _M0L5startS289,
  int32_t _M0L3endS290
) {
  int32_t _if__result_2488;
  int32_t _M0L3lenS292;
  int32_t _M0L6_2atmpS1109;
  int32_t _M0L6_2atmpS1110;
  moonbit_bytes_t _M0L5bytesS293;
  moonbit_bytes_t _M0L6_2atmpS1108;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS289 == 0) {
    int32_t _M0L6_2atmpS1107 = Moonbit_array_length(_M0L3strS291);
    _if__result_2488 = _M0L3endS290 == _M0L6_2atmpS1107;
  } else {
    _if__result_2488 = 0;
  }
  if (_if__result_2488) {
    return _M0L3strS291;
  }
  _M0L3lenS292 = _M0L3endS290 - _M0L5startS289;
  _M0L6_2atmpS1109 = _M0L3lenS292 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1110 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS293
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1109, _M0L6_2atmpS1110);
  moonbit_incref(_M0L5bytesS293);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS293, 0, _M0L3strS291, _M0L5startS289, _M0L3lenS292);
  _M0L6_2atmpS1108 = _M0L5bytesS293;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1108, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS287) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS287;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS288) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS288;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS271,
  int32_t _M0L5radixS270
) {
  int32_t _if__result_2489;
  int32_t _M0L12is__negativeS272;
  uint32_t _M0L3numS273;
  uint16_t* _M0L6bufferS274;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS270 < 2) {
    _if__result_2489 = 1;
  } else {
    _if__result_2489 = _M0L5radixS270 > 36;
  }
  if (_if__result_2489) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_29.data, (moonbit_string_t)moonbit_string_literal_30.data);
  }
  if (_M0L4selfS271 == 0) {
    return (moonbit_string_t)moonbit_string_literal_31.data;
  }
  _M0L12is__negativeS272 = _M0L4selfS271 < 0;
  if (_M0L12is__negativeS272) {
    int32_t _M0L6_2atmpS1106 = -_M0L4selfS271;
    _M0L3numS273 = *(uint32_t*)&_M0L6_2atmpS1106;
  } else {
    _M0L3numS273 = *(uint32_t*)&_M0L4selfS271;
  }
  switch (_M0L5radixS270) {
    case 10: {
      int32_t _M0L10digit__lenS275;
      int32_t _M0L6_2atmpS1103;
      int32_t _M0L10total__lenS276;
      uint16_t* _M0L6bufferS277;
      int32_t _M0L12digit__startS278;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS275 = _M0FPB12dec__count32(_M0L3numS273);
      if (_M0L12is__negativeS272) {
        _M0L6_2atmpS1103 = 1;
      } else {
        _M0L6_2atmpS1103 = 0;
      }
      _M0L10total__lenS276 = _M0L10digit__lenS275 + _M0L6_2atmpS1103;
      _M0L6bufferS277
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS276, 0);
      if (_M0L12is__negativeS272) {
        _M0L12digit__startS278 = 1;
      } else {
        _M0L12digit__startS278 = 0;
      }
      moonbit_incref(_M0L6bufferS277);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS277, _M0L3numS273, _M0L12digit__startS278, _M0L10total__lenS276);
      _M0L6bufferS274 = _M0L6bufferS277;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS279;
      int32_t _M0L6_2atmpS1104;
      int32_t _M0L10total__lenS280;
      uint16_t* _M0L6bufferS281;
      int32_t _M0L12digit__startS282;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS279 = _M0FPB12hex__count32(_M0L3numS273);
      if (_M0L12is__negativeS272) {
        _M0L6_2atmpS1104 = 1;
      } else {
        _M0L6_2atmpS1104 = 0;
      }
      _M0L10total__lenS280 = _M0L10digit__lenS279 + _M0L6_2atmpS1104;
      _M0L6bufferS281
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS280, 0);
      if (_M0L12is__negativeS272) {
        _M0L12digit__startS282 = 1;
      } else {
        _M0L12digit__startS282 = 0;
      }
      moonbit_incref(_M0L6bufferS281);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS281, _M0L3numS273, _M0L12digit__startS282, _M0L10total__lenS280);
      _M0L6bufferS274 = _M0L6bufferS281;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS283;
      int32_t _M0L6_2atmpS1105;
      int32_t _M0L10total__lenS284;
      uint16_t* _M0L6bufferS285;
      int32_t _M0L12digit__startS286;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS283
      = _M0FPB14radix__count32(_M0L3numS273, _M0L5radixS270);
      if (_M0L12is__negativeS272) {
        _M0L6_2atmpS1105 = 1;
      } else {
        _M0L6_2atmpS1105 = 0;
      }
      _M0L10total__lenS284 = _M0L10digit__lenS283 + _M0L6_2atmpS1105;
      _M0L6bufferS285
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS284, 0);
      if (_M0L12is__negativeS272) {
        _M0L12digit__startS286 = 1;
      } else {
        _M0L12digit__startS286 = 0;
      }
      moonbit_incref(_M0L6bufferS285);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS285, _M0L3numS273, _M0L12digit__startS286, _M0L10total__lenS284, _M0L5radixS270);
      _M0L6bufferS274 = _M0L6bufferS285;
      break;
    }
  }
  if (_M0L12is__negativeS272) {
    _M0L6bufferS274[0] = 45;
  }
  return _M0L6bufferS274;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS264,
  int32_t _M0L5radixS267
) {
  uint32_t _M0Lm3numS265;
  uint32_t _M0L4baseS266;
  int32_t _M0Lm5countS268;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS264 == 0u) {
    return 1;
  }
  _M0Lm3numS265 = _M0L5valueS264;
  _M0L4baseS266 = *(uint32_t*)&_M0L5radixS267;
  _M0Lm5countS268 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1100 = _M0Lm3numS265;
    if (_M0L6_2atmpS1100 > 0u) {
      int32_t _M0L6_2atmpS1101 = _M0Lm5countS268;
      uint32_t _M0L6_2atmpS1102;
      _M0Lm5countS268 = _M0L6_2atmpS1101 + 1;
      _M0L6_2atmpS1102 = _M0Lm3numS265;
      _M0Lm3numS265 = _M0L6_2atmpS1102 / _M0L4baseS266;
      continue;
    }
    break;
  }
  return _M0Lm5countS268;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS262) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS262 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS263;
    int32_t _M0L6_2atmpS1099;
    int32_t _M0L6_2atmpS1098;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS263 = moonbit_clz32(_M0L5valueS262);
    _M0L6_2atmpS1099 = 31 - _M0L14leading__zerosS263;
    _M0L6_2atmpS1098 = _M0L6_2atmpS1099 / 4;
    return _M0L6_2atmpS1098 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS261) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS261 >= 100000u) {
    if (_M0L5valueS261 >= 10000000u) {
      if (_M0L5valueS261 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS261 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS261 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS261 >= 1000u) {
    if (_M0L5valueS261 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS261 >= 100u) {
    return 3;
  } else if (_M0L5valueS261 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS251,
  uint32_t _M0L3numS239,
  int32_t _M0L12digit__startS242,
  int32_t _M0L10total__lenS241
) {
  uint32_t _M0Lm3numS238;
  int32_t _M0Lm6offsetS240;
  uint32_t _M0L6_2atmpS1097;
  int32_t _M0Lm9remainingS253;
  int32_t _M0L6_2atmpS1078;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS238 = _M0L3numS239;
  _M0Lm6offsetS240 = _M0L10total__lenS241 - _M0L12digit__startS242;
  while (1) {
    uint32_t _M0L6_2atmpS1041 = _M0Lm3numS238;
    if (_M0L6_2atmpS1041 >= 10000u) {
      uint32_t _M0L6_2atmpS1064 = _M0Lm3numS238;
      uint32_t _M0L1tS243 = _M0L6_2atmpS1064 / 10000u;
      uint32_t _M0L6_2atmpS1063 = _M0Lm3numS238;
      uint32_t _M0L6_2atmpS1062 = _M0L6_2atmpS1063 % 10000u;
      int32_t _M0L1rS244 = *(int32_t*)&_M0L6_2atmpS1062;
      int32_t _M0L2d1S245;
      int32_t _M0L2d2S246;
      int32_t _M0L6_2atmpS1042;
      int32_t _M0L6_2atmpS1061;
      int32_t _M0L6_2atmpS1060;
      int32_t _M0L6d1__hiS247;
      int32_t _M0L6_2atmpS1059;
      int32_t _M0L6_2atmpS1058;
      int32_t _M0L6d1__loS248;
      int32_t _M0L6_2atmpS1057;
      int32_t _M0L6_2atmpS1056;
      int32_t _M0L6d2__hiS249;
      int32_t _M0L6_2atmpS1055;
      int32_t _M0L6_2atmpS1054;
      int32_t _M0L6d2__loS250;
      int32_t _M0L6_2atmpS1044;
      int32_t _M0L6_2atmpS1043;
      int32_t _M0L6_2atmpS1047;
      int32_t _M0L6_2atmpS1046;
      int32_t _M0L6_2atmpS1045;
      int32_t _M0L6_2atmpS1050;
      int32_t _M0L6_2atmpS1049;
      int32_t _M0L6_2atmpS1048;
      int32_t _M0L6_2atmpS1053;
      int32_t _M0L6_2atmpS1052;
      int32_t _M0L6_2atmpS1051;
      _M0Lm3numS238 = _M0L1tS243;
      _M0L2d1S245 = _M0L1rS244 / 100;
      _M0L2d2S246 = _M0L1rS244 % 100;
      _M0L6_2atmpS1042 = _M0Lm6offsetS240;
      _M0Lm6offsetS240 = _M0L6_2atmpS1042 - 4;
      _M0L6_2atmpS1061 = _M0L2d1S245 / 10;
      _M0L6_2atmpS1060 = 48 + _M0L6_2atmpS1061;
      _M0L6d1__hiS247 = (uint16_t)_M0L6_2atmpS1060;
      _M0L6_2atmpS1059 = _M0L2d1S245 % 10;
      _M0L6_2atmpS1058 = 48 + _M0L6_2atmpS1059;
      _M0L6d1__loS248 = (uint16_t)_M0L6_2atmpS1058;
      _M0L6_2atmpS1057 = _M0L2d2S246 / 10;
      _M0L6_2atmpS1056 = 48 + _M0L6_2atmpS1057;
      _M0L6d2__hiS249 = (uint16_t)_M0L6_2atmpS1056;
      _M0L6_2atmpS1055 = _M0L2d2S246 % 10;
      _M0L6_2atmpS1054 = 48 + _M0L6_2atmpS1055;
      _M0L6d2__loS250 = (uint16_t)_M0L6_2atmpS1054;
      _M0L6_2atmpS1044 = _M0Lm6offsetS240;
      _M0L6_2atmpS1043 = _M0L12digit__startS242 + _M0L6_2atmpS1044;
      _M0L6bufferS251[_M0L6_2atmpS1043] = _M0L6d1__hiS247;
      _M0L6_2atmpS1047 = _M0Lm6offsetS240;
      _M0L6_2atmpS1046 = _M0L12digit__startS242 + _M0L6_2atmpS1047;
      _M0L6_2atmpS1045 = _M0L6_2atmpS1046 + 1;
      _M0L6bufferS251[_M0L6_2atmpS1045] = _M0L6d1__loS248;
      _M0L6_2atmpS1050 = _M0Lm6offsetS240;
      _M0L6_2atmpS1049 = _M0L12digit__startS242 + _M0L6_2atmpS1050;
      _M0L6_2atmpS1048 = _M0L6_2atmpS1049 + 2;
      _M0L6bufferS251[_M0L6_2atmpS1048] = _M0L6d2__hiS249;
      _M0L6_2atmpS1053 = _M0Lm6offsetS240;
      _M0L6_2atmpS1052 = _M0L12digit__startS242 + _M0L6_2atmpS1053;
      _M0L6_2atmpS1051 = _M0L6_2atmpS1052 + 3;
      _M0L6bufferS251[_M0L6_2atmpS1051] = _M0L6d2__loS250;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1097 = _M0Lm3numS238;
  _M0Lm9remainingS253 = *(int32_t*)&_M0L6_2atmpS1097;
  while (1) {
    int32_t _M0L6_2atmpS1065 = _M0Lm9remainingS253;
    if (_M0L6_2atmpS1065 >= 100) {
      int32_t _M0L6_2atmpS1077 = _M0Lm9remainingS253;
      int32_t _M0L1tS254 = _M0L6_2atmpS1077 / 100;
      int32_t _M0L6_2atmpS1076 = _M0Lm9remainingS253;
      int32_t _M0L1dS255 = _M0L6_2atmpS1076 % 100;
      int32_t _M0L6_2atmpS1066;
      int32_t _M0L6_2atmpS1075;
      int32_t _M0L6_2atmpS1074;
      int32_t _M0L5d__hiS256;
      int32_t _M0L6_2atmpS1073;
      int32_t _M0L6_2atmpS1072;
      int32_t _M0L5d__loS257;
      int32_t _M0L6_2atmpS1068;
      int32_t _M0L6_2atmpS1067;
      int32_t _M0L6_2atmpS1071;
      int32_t _M0L6_2atmpS1070;
      int32_t _M0L6_2atmpS1069;
      _M0Lm9remainingS253 = _M0L1tS254;
      _M0L6_2atmpS1066 = _M0Lm6offsetS240;
      _M0Lm6offsetS240 = _M0L6_2atmpS1066 - 2;
      _M0L6_2atmpS1075 = _M0L1dS255 / 10;
      _M0L6_2atmpS1074 = 48 + _M0L6_2atmpS1075;
      _M0L5d__hiS256 = (uint16_t)_M0L6_2atmpS1074;
      _M0L6_2atmpS1073 = _M0L1dS255 % 10;
      _M0L6_2atmpS1072 = 48 + _M0L6_2atmpS1073;
      _M0L5d__loS257 = (uint16_t)_M0L6_2atmpS1072;
      _M0L6_2atmpS1068 = _M0Lm6offsetS240;
      _M0L6_2atmpS1067 = _M0L12digit__startS242 + _M0L6_2atmpS1068;
      _M0L6bufferS251[_M0L6_2atmpS1067] = _M0L5d__hiS256;
      _M0L6_2atmpS1071 = _M0Lm6offsetS240;
      _M0L6_2atmpS1070 = _M0L12digit__startS242 + _M0L6_2atmpS1071;
      _M0L6_2atmpS1069 = _M0L6_2atmpS1070 + 1;
      _M0L6bufferS251[_M0L6_2atmpS1069] = _M0L5d__loS257;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1078 = _M0Lm9remainingS253;
  if (_M0L6_2atmpS1078 >= 10) {
    int32_t _M0L6_2atmpS1079 = _M0Lm6offsetS240;
    int32_t _M0L6_2atmpS1090;
    int32_t _M0L6_2atmpS1089;
    int32_t _M0L6_2atmpS1088;
    int32_t _M0L5d__hiS259;
    int32_t _M0L6_2atmpS1087;
    int32_t _M0L6_2atmpS1086;
    int32_t _M0L6_2atmpS1085;
    int32_t _M0L5d__loS260;
    int32_t _M0L6_2atmpS1081;
    int32_t _M0L6_2atmpS1080;
    int32_t _M0L6_2atmpS1084;
    int32_t _M0L6_2atmpS1083;
    int32_t _M0L6_2atmpS1082;
    _M0Lm6offsetS240 = _M0L6_2atmpS1079 - 2;
    _M0L6_2atmpS1090 = _M0Lm9remainingS253;
    _M0L6_2atmpS1089 = _M0L6_2atmpS1090 / 10;
    _M0L6_2atmpS1088 = 48 + _M0L6_2atmpS1089;
    _M0L5d__hiS259 = (uint16_t)_M0L6_2atmpS1088;
    _M0L6_2atmpS1087 = _M0Lm9remainingS253;
    _M0L6_2atmpS1086 = _M0L6_2atmpS1087 % 10;
    _M0L6_2atmpS1085 = 48 + _M0L6_2atmpS1086;
    _M0L5d__loS260 = (uint16_t)_M0L6_2atmpS1085;
    _M0L6_2atmpS1081 = _M0Lm6offsetS240;
    _M0L6_2atmpS1080 = _M0L12digit__startS242 + _M0L6_2atmpS1081;
    _M0L6bufferS251[_M0L6_2atmpS1080] = _M0L5d__hiS259;
    _M0L6_2atmpS1084 = _M0Lm6offsetS240;
    _M0L6_2atmpS1083 = _M0L12digit__startS242 + _M0L6_2atmpS1084;
    _M0L6_2atmpS1082 = _M0L6_2atmpS1083 + 1;
    _M0L6bufferS251[_M0L6_2atmpS1082] = _M0L5d__loS260;
    moonbit_decref(_M0L6bufferS251);
  } else {
    int32_t _M0L6_2atmpS1091 = _M0Lm6offsetS240;
    int32_t _M0L6_2atmpS1096;
    int32_t _M0L6_2atmpS1092;
    int32_t _M0L6_2atmpS1095;
    int32_t _M0L6_2atmpS1094;
    int32_t _M0L6_2atmpS1093;
    _M0Lm6offsetS240 = _M0L6_2atmpS1091 - 1;
    _M0L6_2atmpS1096 = _M0Lm6offsetS240;
    _M0L6_2atmpS1092 = _M0L12digit__startS242 + _M0L6_2atmpS1096;
    _M0L6_2atmpS1095 = _M0Lm9remainingS253;
    _M0L6_2atmpS1094 = 48 + _M0L6_2atmpS1095;
    _M0L6_2atmpS1093 = (uint16_t)_M0L6_2atmpS1094;
    _M0L6bufferS251[_M0L6_2atmpS1092] = _M0L6_2atmpS1093;
    moonbit_decref(_M0L6bufferS251);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS233,
  uint32_t _M0L3numS227,
  int32_t _M0L12digit__startS225,
  int32_t _M0L10total__lenS224,
  int32_t _M0L5radixS229
) {
  int32_t _M0Lm6offsetS223;
  uint32_t _M0Lm1nS226;
  uint32_t _M0L4baseS228;
  int32_t _M0L6_2atmpS1023;
  int32_t _M0L6_2atmpS1022;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS223 = _M0L10total__lenS224 - _M0L12digit__startS225;
  _M0Lm1nS226 = _M0L3numS227;
  _M0L4baseS228 = *(uint32_t*)&_M0L5radixS229;
  _M0L6_2atmpS1023 = _M0L5radixS229 - 1;
  _M0L6_2atmpS1022 = _M0L5radixS229 & _M0L6_2atmpS1023;
  if (_M0L6_2atmpS1022 == 0) {
    int32_t _M0L5shiftS230;
    uint32_t _M0L4maskS231;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS230 = moonbit_ctz32(_M0L5radixS229);
    _M0L4maskS231 = _M0L4baseS228 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1024 = _M0Lm1nS226;
      if (_M0L6_2atmpS1024 > 0u) {
        int32_t _M0L6_2atmpS1025 = _M0Lm6offsetS223;
        uint32_t _M0L6_2atmpS1031;
        uint32_t _M0L6_2atmpS1030;
        int32_t _M0L5digitS232;
        int32_t _M0L6_2atmpS1028;
        int32_t _M0L6_2atmpS1026;
        int32_t _M0L6_2atmpS1027;
        uint32_t _M0L6_2atmpS1029;
        _M0Lm6offsetS223 = _M0L6_2atmpS1025 - 1;
        _M0L6_2atmpS1031 = _M0Lm1nS226;
        _M0L6_2atmpS1030 = _M0L6_2atmpS1031 & _M0L4maskS231;
        _M0L5digitS232 = *(int32_t*)&_M0L6_2atmpS1030;
        _M0L6_2atmpS1028 = _M0Lm6offsetS223;
        _M0L6_2atmpS1026 = _M0L12digit__startS225 + _M0L6_2atmpS1028;
        _M0L6_2atmpS1027
        = ((moonbit_string_t)moonbit_string_literal_32.data)[
          _M0L5digitS232
        ];
        _M0L6bufferS233[_M0L6_2atmpS1026] = _M0L6_2atmpS1027;
        _M0L6_2atmpS1029 = _M0Lm1nS226;
        _M0Lm1nS226 = _M0L6_2atmpS1029 >> (_M0L5shiftS230 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS233);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1032 = _M0Lm1nS226;
      if (_M0L6_2atmpS1032 > 0u) {
        int32_t _M0L6_2atmpS1033 = _M0Lm6offsetS223;
        uint32_t _M0L6_2atmpS1040;
        uint32_t _M0L1qS235;
        uint32_t _M0L6_2atmpS1038;
        uint32_t _M0L6_2atmpS1039;
        uint32_t _M0L6_2atmpS1037;
        int32_t _M0L5digitS236;
        int32_t _M0L6_2atmpS1036;
        int32_t _M0L6_2atmpS1034;
        int32_t _M0L6_2atmpS1035;
        _M0Lm6offsetS223 = _M0L6_2atmpS1033 - 1;
        _M0L6_2atmpS1040 = _M0Lm1nS226;
        _M0L1qS235 = _M0L6_2atmpS1040 / _M0L4baseS228;
        _M0L6_2atmpS1038 = _M0Lm1nS226;
        _M0L6_2atmpS1039 = _M0L1qS235 * _M0L4baseS228;
        _M0L6_2atmpS1037 = _M0L6_2atmpS1038 - _M0L6_2atmpS1039;
        _M0L5digitS236 = *(int32_t*)&_M0L6_2atmpS1037;
        _M0L6_2atmpS1036 = _M0Lm6offsetS223;
        _M0L6_2atmpS1034 = _M0L12digit__startS225 + _M0L6_2atmpS1036;
        _M0L6_2atmpS1035
        = ((moonbit_string_t)moonbit_string_literal_32.data)[
          _M0L5digitS236
        ];
        _M0L6bufferS233[_M0L6_2atmpS1034] = _M0L6_2atmpS1035;
        _M0Lm1nS226 = _M0L1qS235;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS233);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS220,
  uint32_t _M0L3numS216,
  int32_t _M0L12digit__startS214,
  int32_t _M0L10total__lenS213
) {
  int32_t _M0Lm6offsetS212;
  uint32_t _M0Lm1nS215;
  int32_t _M0L6_2atmpS1018;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS212 = _M0L10total__lenS213 - _M0L12digit__startS214;
  _M0Lm1nS215 = _M0L3numS216;
  while (1) {
    int32_t _M0L6_2atmpS1006 = _M0Lm6offsetS212;
    if (_M0L6_2atmpS1006 >= 2) {
      int32_t _M0L6_2atmpS1007 = _M0Lm6offsetS212;
      uint32_t _M0L6_2atmpS1017;
      uint32_t _M0L6_2atmpS1016;
      int32_t _M0L9byte__valS217;
      int32_t _M0L2hiS218;
      int32_t _M0L2loS219;
      int32_t _M0L6_2atmpS1010;
      int32_t _M0L6_2atmpS1008;
      int32_t _M0L6_2atmpS1009;
      int32_t _M0L6_2atmpS1014;
      int32_t _M0L6_2atmpS1013;
      int32_t _M0L6_2atmpS1011;
      int32_t _M0L6_2atmpS1012;
      uint32_t _M0L6_2atmpS1015;
      _M0Lm6offsetS212 = _M0L6_2atmpS1007 - 2;
      _M0L6_2atmpS1017 = _M0Lm1nS215;
      _M0L6_2atmpS1016 = _M0L6_2atmpS1017 & 255u;
      _M0L9byte__valS217 = *(int32_t*)&_M0L6_2atmpS1016;
      _M0L2hiS218 = _M0L9byte__valS217 / 16;
      _M0L2loS219 = _M0L9byte__valS217 % 16;
      _M0L6_2atmpS1010 = _M0Lm6offsetS212;
      _M0L6_2atmpS1008 = _M0L12digit__startS214 + _M0L6_2atmpS1010;
      _M0L6_2atmpS1009
      = ((moonbit_string_t)moonbit_string_literal_32.data)[
        _M0L2hiS218
      ];
      _M0L6bufferS220[_M0L6_2atmpS1008] = _M0L6_2atmpS1009;
      _M0L6_2atmpS1014 = _M0Lm6offsetS212;
      _M0L6_2atmpS1013 = _M0L12digit__startS214 + _M0L6_2atmpS1014;
      _M0L6_2atmpS1011 = _M0L6_2atmpS1013 + 1;
      _M0L6_2atmpS1012
      = ((moonbit_string_t)moonbit_string_literal_32.data)[
        _M0L2loS219
      ];
      _M0L6bufferS220[_M0L6_2atmpS1011] = _M0L6_2atmpS1012;
      _M0L6_2atmpS1015 = _M0Lm1nS215;
      _M0Lm1nS215 = _M0L6_2atmpS1015 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1018 = _M0Lm6offsetS212;
  if (_M0L6_2atmpS1018 == 1) {
    uint32_t _M0L6_2atmpS1021 = _M0Lm1nS215;
    uint32_t _M0L6_2atmpS1020 = _M0L6_2atmpS1021 & 15u;
    int32_t _M0L6nibbleS222 = *(int32_t*)&_M0L6_2atmpS1020;
    int32_t _M0L6_2atmpS1019 =
      ((moonbit_string_t)moonbit_string_literal_32.data)[_M0L6nibbleS222];
    _M0L6bufferS220[_M0L12digit__startS214] = _M0L6_2atmpS1019;
    moonbit_decref(_M0L6bufferS220);
  } else {
    moonbit_decref(_M0L6bufferS220);
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger _M0L4selfS195,
  struct _M0TWEOs* _M0L4iterS199,
  moonbit_string_t _M0L6prefixS196,
  moonbit_string_t _M0L6suffixS211,
  moonbit_string_t _M0L3sepS202,
  int32_t _M0L8trailingS197
) {
  #line 156 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  if (_M0L4selfS195.$1) {
    moonbit_incref(_M0L4selfS195.$1);
  }
  #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L4selfS195.$0->$method_0(_M0L4selfS195.$1, _M0L6prefixS196);
  if (_M0L8trailingS197) {
    while (1) {
      moonbit_string_t _M0L7_2abindS198;
      moonbit_incref(_M0L4iterS199);
      #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
      _M0L7_2abindS198 = _M0MPB4Iter4nextGsE(_M0L4iterS199);
      if (_M0L7_2abindS198 == 0) {
        moonbit_decref(_M0L3sepS202);
        moonbit_decref(_M0L4iterS199);
        if (_M0L7_2abindS198) {
          moonbit_decref(_M0L7_2abindS198);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS200 = _M0L7_2abindS198;
        moonbit_string_t _M0L4_2axS201 = _M0L7_2aSomeS200;
        if (_M0L4selfS195.$1) {
          moonbit_incref(_M0L4selfS195.$1);
        }
        #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0MPB6Logger13write__objectGsE(_M0L4selfS195, _M0L4_2axS201);
        moonbit_incref(_M0L3sepS202);
        if (_M0L4selfS195.$1) {
          moonbit_incref(_M0L4selfS195.$1);
        }
        #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0L4selfS195.$0->$method_0(_M0L4selfS195.$1, _M0L3sepS202);
        continue;
      }
      break;
    }
  } else {
    moonbit_string_t _M0L7_2abindS204;
    moonbit_incref(_M0L4iterS199);
    #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
    _M0L7_2abindS204 = _M0MPB4Iter4nextGsE(_M0L4iterS199);
    if (_M0L7_2abindS204 == 0) {
      if (_M0L7_2abindS204) {
        moonbit_decref(_M0L7_2abindS204);
      }
      moonbit_decref(_M0L3sepS202);
      moonbit_decref(_M0L4iterS199);
    } else {
      moonbit_string_t _M0L7_2aSomeS205 = _M0L7_2abindS204;
      moonbit_string_t _M0L4_2axS206 = _M0L7_2aSomeS205;
      if (_M0L4selfS195.$1) {
        moonbit_incref(_M0L4selfS195.$1);
      }
      #line 171 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
      _M0MPB6Logger13write__objectGsE(_M0L4selfS195, _M0L4_2axS206);
      while (1) {
        moonbit_string_t _M0L7_2abindS207;
        moonbit_incref(_M0L4iterS199);
        #line 172 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0L7_2abindS207 = _M0MPB4Iter4nextGsE(_M0L4iterS199);
        if (_M0L7_2abindS207 == 0) {
          if (_M0L7_2abindS207) {
            moonbit_decref(_M0L7_2abindS207);
          }
          moonbit_decref(_M0L3sepS202);
          moonbit_decref(_M0L4iterS199);
        } else {
          moonbit_string_t _M0L7_2aSomeS208 = _M0L7_2abindS207;
          moonbit_string_t _M0L4_2axS209 = _M0L7_2aSomeS208;
          moonbit_incref(_M0L3sepS202);
          if (_M0L4selfS195.$1) {
            moonbit_incref(_M0L4selfS195.$1);
          }
          #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0L4selfS195.$0->$method_0(_M0L4selfS195.$1, _M0L3sepS202);
          if (_M0L4selfS195.$1) {
            moonbit_incref(_M0L4selfS195.$1);
          }
          #line 174 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0MPB6Logger13write__objectGsE(_M0L4selfS195, _M0L4_2axS209);
          continue;
        }
        break;
      }
    }
  }
  #line 177 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L4selfS195.$0->$method_0(_M0L4selfS195.$1, _M0L6suffixS211);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS192) {
  struct _M0TWEOs* _M0L7_2afuncS191;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS191 = _M0L4selfS192;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS191->code(_M0L7_2afuncS191);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS194) {
  struct _M0TWEOc* _M0L7_2afuncS193;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS193 = _M0L4selfS194;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS193->code(_M0L7_2afuncS193);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE* _M0L4selfS184
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS183;
  struct _M0TPB6Logger _M0L6_2atmpS1002;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS183 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS183);
  _M0L6_2atmpS1002
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS183
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L4selfS184, _M0L6_2atmpS1002);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS183);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS186
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS185;
  struct _M0TPB6Logger _M0L6_2atmpS1003;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS185 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS185);
  _M0L6_2atmpS1003
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS185
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS186, _M0L6_2atmpS1003);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS185);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS188
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS187;
  struct _M0TPB6Logger _M0L6_2atmpS1004;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS187 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS187);
  _M0L6_2atmpS1004
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS187
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS188, _M0L6_2atmpS1004);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS187);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS190
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS189;
  struct _M0TPB6Logger _M0L6_2atmpS1005;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS189 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS189);
  _M0L6_2atmpS1005
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS189
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS190, _M0L6_2atmpS1005);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS189);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS182
) {
  int32_t _M0L8_2afieldS2309;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2309 = _M0L4selfS182.$1;
  moonbit_decref(_M0L4selfS182.$0);
  return _M0L8_2afieldS2309;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS181
) {
  int32_t _M0L3endS1000;
  int32_t _M0L8_2afieldS2310;
  int32_t _M0L5startS1001;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1000 = _M0L4selfS181.$2;
  _M0L8_2afieldS2310 = _M0L4selfS181.$1;
  moonbit_decref(_M0L4selfS181.$0);
  _M0L5startS1001 = _M0L8_2afieldS2310;
  return _M0L3endS1000 - _M0L5startS1001;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS180
) {
  moonbit_string_t _M0L8_2afieldS2311;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2311 = _M0L4selfS180.$0;
  return _M0L8_2afieldS2311;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS176,
  moonbit_string_t _M0L5valueS177,
  int32_t _M0L5startS178,
  int32_t _M0L3lenS179
) {
  int32_t _M0L6_2atmpS999;
  int64_t _M0L6_2atmpS998;
  struct _M0TPC16string10StringView _M0L6_2atmpS997;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS999 = _M0L5startS178 + _M0L3lenS179;
  _M0L6_2atmpS998 = (int64_t)_M0L6_2atmpS999;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS997
  = _M0MPC16string6String11sub_2einner(_M0L5valueS177, _M0L5startS178, _M0L6_2atmpS998);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS176, _M0L6_2atmpS997);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS169,
  int32_t _M0L5startS175,
  int64_t _M0L3endS171
) {
  int32_t _M0L3lenS168;
  int32_t _M0L3endS170;
  int32_t _M0L5startS174;
  int32_t _if__result_2498;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS168 = Moonbit_array_length(_M0L4selfS169);
  if (_M0L3endS171 == 4294967296ll) {
    _M0L3endS170 = _M0L3lenS168;
  } else {
    int64_t _M0L7_2aSomeS172 = _M0L3endS171;
    int32_t _M0L6_2aendS173 = (int32_t)_M0L7_2aSomeS172;
    if (_M0L6_2aendS173 < 0) {
      _M0L3endS170 = _M0L3lenS168 + _M0L6_2aendS173;
    } else {
      _M0L3endS170 = _M0L6_2aendS173;
    }
  }
  if (_M0L5startS175 < 0) {
    _M0L5startS174 = _M0L3lenS168 + _M0L5startS175;
  } else {
    _M0L5startS174 = _M0L5startS175;
  }
  if (_M0L5startS174 >= 0) {
    if (_M0L5startS174 <= _M0L3endS170) {
      _if__result_2498 = _M0L3endS170 <= _M0L3lenS168;
    } else {
      _if__result_2498 = 0;
    }
  } else {
    _if__result_2498 = 0;
  }
  if (_if__result_2498) {
    if (_M0L5startS174 < _M0L3lenS168) {
      int32_t _M0L6_2atmpS994 = _M0L4selfS169[_M0L5startS174];
      int32_t _M0L6_2atmpS993;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS993
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS994);
      if (!_M0L6_2atmpS993) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS170 < _M0L3lenS168) {
      int32_t _M0L6_2atmpS996 = _M0L4selfS169[_M0L3endS170];
      int32_t _M0L6_2atmpS995;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS995
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS996);
      if (!_M0L6_2atmpS995) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS174,
                                                 _M0L3endS170,
                                                 _M0L4selfS169};
  } else {
    moonbit_decref(_M0L4selfS169);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS166,
  moonbit_string_t _M0L3strS167
) {
  int32_t _M0L3lenS983;
  int32_t _M0L6_2atmpS985;
  int32_t _M0L6_2atmpS984;
  int32_t _M0L6_2atmpS982;
  moonbit_bytes_t _M0L8_2afieldS2313;
  moonbit_bytes_t _M0L4dataS986;
  int32_t _M0L3lenS987;
  int32_t _M0L6_2atmpS988;
  int32_t _M0L3lenS990;
  int32_t _M0L6_2atmpS2312;
  int32_t _M0L6_2atmpS992;
  int32_t _M0L6_2atmpS991;
  int32_t _M0L6_2atmpS989;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS983 = _M0L4selfS166->$1;
  _M0L6_2atmpS985 = Moonbit_array_length(_M0L3strS167);
  _M0L6_2atmpS984 = _M0L6_2atmpS985 * 2;
  _M0L6_2atmpS982 = _M0L3lenS983 + _M0L6_2atmpS984;
  moonbit_incref(_M0L4selfS166);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS166, _M0L6_2atmpS982);
  _M0L8_2afieldS2313 = _M0L4selfS166->$0;
  _M0L4dataS986 = _M0L8_2afieldS2313;
  _M0L3lenS987 = _M0L4selfS166->$1;
  _M0L6_2atmpS988 = Moonbit_array_length(_M0L3strS167);
  moonbit_incref(_M0L4dataS986);
  moonbit_incref(_M0L3strS167);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS986, _M0L3lenS987, _M0L3strS167, 0, _M0L6_2atmpS988);
  _M0L3lenS990 = _M0L4selfS166->$1;
  _M0L6_2atmpS2312 = Moonbit_array_length(_M0L3strS167);
  moonbit_decref(_M0L3strS167);
  _M0L6_2atmpS992 = _M0L6_2atmpS2312;
  _M0L6_2atmpS991 = _M0L6_2atmpS992 * 2;
  _M0L6_2atmpS989 = _M0L3lenS990 + _M0L6_2atmpS991;
  _M0L4selfS166->$1 = _M0L6_2atmpS989;
  moonbit_decref(_M0L4selfS166);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS158,
  int32_t _M0L13bytes__offsetS153,
  moonbit_string_t _M0L3strS160,
  int32_t _M0L11str__offsetS156,
  int32_t _M0L6lengthS154
) {
  int32_t _M0L6_2atmpS981;
  int32_t _M0L6_2atmpS980;
  int32_t _M0L2e1S152;
  int32_t _M0L6_2atmpS979;
  int32_t _M0L2e2S155;
  int32_t _M0L4len1S157;
  int32_t _M0L4len2S159;
  int32_t _if__result_2499;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS981 = _M0L6lengthS154 * 2;
  _M0L6_2atmpS980 = _M0L13bytes__offsetS153 + _M0L6_2atmpS981;
  _M0L2e1S152 = _M0L6_2atmpS980 - 1;
  _M0L6_2atmpS979 = _M0L11str__offsetS156 + _M0L6lengthS154;
  _M0L2e2S155 = _M0L6_2atmpS979 - 1;
  _M0L4len1S157 = Moonbit_array_length(_M0L4selfS158);
  _M0L4len2S159 = Moonbit_array_length(_M0L3strS160);
  if (_M0L6lengthS154 >= 0) {
    if (_M0L13bytes__offsetS153 >= 0) {
      if (_M0L2e1S152 < _M0L4len1S157) {
        if (_M0L11str__offsetS156 >= 0) {
          _if__result_2499 = _M0L2e2S155 < _M0L4len2S159;
        } else {
          _if__result_2499 = 0;
        }
      } else {
        _if__result_2499 = 0;
      }
    } else {
      _if__result_2499 = 0;
    }
  } else {
    _if__result_2499 = 0;
  }
  if (_if__result_2499) {
    int32_t _M0L16end__str__offsetS161 =
      _M0L11str__offsetS156 + _M0L6lengthS154;
    int32_t _M0L1iS162 = _M0L11str__offsetS156;
    int32_t _M0L1jS163 = _M0L13bytes__offsetS153;
    while (1) {
      if (_M0L1iS162 < _M0L16end__str__offsetS161) {
        int32_t _M0L6_2atmpS976 = _M0L3strS160[_M0L1iS162];
        int32_t _M0L6_2atmpS975 = (int32_t)_M0L6_2atmpS976;
        uint32_t _M0L1cS164 = *(uint32_t*)&_M0L6_2atmpS975;
        uint32_t _M0L6_2atmpS971 = _M0L1cS164 & 255u;
        int32_t _M0L6_2atmpS970;
        int32_t _M0L6_2atmpS972;
        uint32_t _M0L6_2atmpS974;
        int32_t _M0L6_2atmpS973;
        int32_t _M0L6_2atmpS977;
        int32_t _M0L6_2atmpS978;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS970 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS971);
        if (
          _M0L1jS163 < 0 || _M0L1jS163 >= Moonbit_array_length(_M0L4selfS158)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS158[_M0L1jS163] = _M0L6_2atmpS970;
        _M0L6_2atmpS972 = _M0L1jS163 + 1;
        _M0L6_2atmpS974 = _M0L1cS164 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS973 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS974);
        if (
          _M0L6_2atmpS972 < 0
          || _M0L6_2atmpS972 >= Moonbit_array_length(_M0L4selfS158)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS158[_M0L6_2atmpS972] = _M0L6_2atmpS973;
        _M0L6_2atmpS977 = _M0L1iS162 + 1;
        _M0L6_2atmpS978 = _M0L1jS163 + 2;
        _M0L1iS162 = _M0L6_2atmpS977;
        _M0L1jS163 = _M0L6_2atmpS978;
        continue;
      } else {
        moonbit_decref(_M0L3strS160);
        moonbit_decref(_M0L4selfS158);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS160);
    moonbit_decref(_M0L4selfS158);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS98
) {
  int32_t _M0L6_2atmpS969;
  struct _M0TPC16string10StringView _M0L7_2abindS97;
  moonbit_string_t _M0L7_2adataS99;
  int32_t _M0L8_2astartS100;
  int32_t _M0L6_2atmpS968;
  int32_t _M0L6_2aendS101;
  int32_t _M0Lm9_2acursorS102;
  int32_t _M0Lm13accept__stateS103;
  int32_t _M0Lm10match__endS104;
  int32_t _M0Lm20match__tag__saver__0S105;
  int32_t _M0Lm20match__tag__saver__1S106;
  int32_t _M0Lm20match__tag__saver__2S107;
  int32_t _M0Lm20match__tag__saver__3S108;
  int32_t _M0Lm20match__tag__saver__4S109;
  int32_t _M0Lm6tag__0S110;
  int32_t _M0Lm6tag__1S111;
  int32_t _M0Lm9tag__1__1S112;
  int32_t _M0Lm9tag__1__2S113;
  int32_t _M0Lm6tag__3S114;
  int32_t _M0Lm6tag__2S115;
  int32_t _M0Lm9tag__2__1S116;
  int32_t _M0Lm6tag__4S117;
  int32_t _M0L6_2atmpS926;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS969 = Moonbit_array_length(_M0L4reprS98);
  _M0L7_2abindS97
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS969, _M0L4reprS98
  };
  moonbit_incref(_M0L7_2abindS97.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS99 = _M0MPC16string10StringView4data(_M0L7_2abindS97);
  moonbit_incref(_M0L7_2abindS97.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS100
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS97);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS968 = _M0MPC16string10StringView6length(_M0L7_2abindS97);
  _M0L6_2aendS101 = _M0L8_2astartS100 + _M0L6_2atmpS968;
  _M0Lm9_2acursorS102 = _M0L8_2astartS100;
  _M0Lm13accept__stateS103 = -1;
  _M0Lm10match__endS104 = -1;
  _M0Lm20match__tag__saver__0S105 = -1;
  _M0Lm20match__tag__saver__1S106 = -1;
  _M0Lm20match__tag__saver__2S107 = -1;
  _M0Lm20match__tag__saver__3S108 = -1;
  _M0Lm20match__tag__saver__4S109 = -1;
  _M0Lm6tag__0S110 = -1;
  _M0Lm6tag__1S111 = -1;
  _M0Lm9tag__1__1S112 = -1;
  _M0Lm9tag__1__2S113 = -1;
  _M0Lm6tag__3S114 = -1;
  _M0Lm6tag__2S115 = -1;
  _M0Lm9tag__2__1S116 = -1;
  _M0Lm6tag__4S117 = -1;
  _M0L6_2atmpS926 = _M0Lm9_2acursorS102;
  if (_M0L6_2atmpS926 < _M0L6_2aendS101) {
    int32_t _M0L6_2atmpS928 = _M0Lm9_2acursorS102;
    int32_t _M0L6_2atmpS927;
    moonbit_incref(_M0L7_2adataS99);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS927
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS928);
    if (_M0L6_2atmpS927 == 64) {
      int32_t _M0L6_2atmpS929 = _M0Lm9_2acursorS102;
      _M0Lm9_2acursorS102 = _M0L6_2atmpS929 + 1;
      while (1) {
        int32_t _M0L6_2atmpS930;
        _M0Lm6tag__0S110 = _M0Lm9_2acursorS102;
        _M0L6_2atmpS930 = _M0Lm9_2acursorS102;
        if (_M0L6_2atmpS930 < _M0L6_2aendS101) {
          int32_t _M0L6_2atmpS967 = _M0Lm9_2acursorS102;
          int32_t _M0L10next__charS125;
          int32_t _M0L6_2atmpS931;
          moonbit_incref(_M0L7_2adataS99);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS125
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS967);
          _M0L6_2atmpS931 = _M0Lm9_2acursorS102;
          _M0Lm9_2acursorS102 = _M0L6_2atmpS931 + 1;
          if (_M0L10next__charS125 == 58) {
            int32_t _M0L6_2atmpS932 = _M0Lm9_2acursorS102;
            if (_M0L6_2atmpS932 < _M0L6_2aendS101) {
              int32_t _M0L6_2atmpS933 = _M0Lm9_2acursorS102;
              int32_t _M0L12dispatch__15S126;
              _M0Lm9_2acursorS102 = _M0L6_2atmpS933 + 1;
              _M0L12dispatch__15S126 = 0;
              loop__label__15_129:;
              while (1) {
                int32_t _M0L6_2atmpS934;
                switch (_M0L12dispatch__15S126) {
                  case 3: {
                    int32_t _M0L6_2atmpS937;
                    _M0Lm9tag__1__2S113 = _M0Lm9tag__1__1S112;
                    _M0Lm9tag__1__1S112 = _M0Lm6tag__1S111;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS937 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS937 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS942 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS133;
                      int32_t _M0L6_2atmpS938;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS133
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS942);
                      _M0L6_2atmpS938 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS938 + 1;
                      if (_M0L10next__charS133 < 58) {
                        if (_M0L10next__charS133 < 48) {
                          goto join_132;
                        } else {
                          int32_t _M0L6_2atmpS939;
                          _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                          _M0Lm9tag__2__1S116 = _M0Lm6tag__2S115;
                          _M0Lm6tag__2S115 = _M0Lm9_2acursorS102;
                          _M0Lm6tag__3S114 = _M0Lm9_2acursorS102;
                          _M0L6_2atmpS939 = _M0Lm9_2acursorS102;
                          if (_M0L6_2atmpS939 < _M0L6_2aendS101) {
                            int32_t _M0L6_2atmpS941 = _M0Lm9_2acursorS102;
                            int32_t _M0L10next__charS135;
                            int32_t _M0L6_2atmpS940;
                            moonbit_incref(_M0L7_2adataS99);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS135
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS941);
                            _M0L6_2atmpS940 = _M0Lm9_2acursorS102;
                            _M0Lm9_2acursorS102 = _M0L6_2atmpS940 + 1;
                            if (_M0L10next__charS135 < 48) {
                              if (_M0L10next__charS135 == 45) {
                                goto join_127;
                              } else {
                                goto join_134;
                              }
                            } else if (_M0L10next__charS135 > 57) {
                              if (_M0L10next__charS135 < 59) {
                                _M0L12dispatch__15S126 = 3;
                                goto loop__label__15_129;
                              } else {
                                goto join_134;
                              }
                            } else {
                              _M0L12dispatch__15S126 = 6;
                              goto loop__label__15_129;
                            }
                            join_134:;
                            _M0L12dispatch__15S126 = 0;
                            goto loop__label__15_129;
                          } else {
                            goto join_118;
                          }
                        }
                      } else if (_M0L10next__charS133 > 58) {
                        goto join_132;
                      } else {
                        _M0L12dispatch__15S126 = 1;
                        goto loop__label__15_129;
                      }
                      join_132:;
                      _M0L12dispatch__15S126 = 0;
                      goto loop__label__15_129;
                    } else {
                      goto join_118;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS943;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0Lm6tag__2S115 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS943 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS943 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS945 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS137;
                      int32_t _M0L6_2atmpS944;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS137
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS945);
                      _M0L6_2atmpS944 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS944 + 1;
                      if (_M0L10next__charS137 < 58) {
                        if (_M0L10next__charS137 < 48) {
                          goto join_136;
                        } else {
                          _M0L12dispatch__15S126 = 2;
                          goto loop__label__15_129;
                        }
                      } else if (_M0L10next__charS137 > 58) {
                        goto join_136;
                      } else {
                        _M0L12dispatch__15S126 = 3;
                        goto loop__label__15_129;
                      }
                      join_136:;
                      _M0L12dispatch__15S126 = 0;
                      goto loop__label__15_129;
                    } else {
                      goto join_118;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS946;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS946 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS946 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS948 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS138;
                      int32_t _M0L6_2atmpS947;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS138
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS948);
                      _M0L6_2atmpS947 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS947 + 1;
                      if (_M0L10next__charS138 == 58) {
                        _M0L12dispatch__15S126 = 1;
                        goto loop__label__15_129;
                      } else {
                        _M0L12dispatch__15S126 = 0;
                        goto loop__label__15_129;
                      }
                    } else {
                      goto join_118;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS949;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0Lm6tag__4S117 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS949 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS949 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS957 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS140;
                      int32_t _M0L6_2atmpS950;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS140
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS957);
                      _M0L6_2atmpS950 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS950 + 1;
                      if (_M0L10next__charS140 < 58) {
                        if (_M0L10next__charS140 < 48) {
                          goto join_139;
                        } else {
                          _M0L12dispatch__15S126 = 4;
                          goto loop__label__15_129;
                        }
                      } else if (_M0L10next__charS140 > 58) {
                        goto join_139;
                      } else {
                        int32_t _M0L6_2atmpS951;
                        _M0Lm9tag__1__2S113 = _M0Lm9tag__1__1S112;
                        _M0Lm9tag__1__1S112 = _M0Lm6tag__1S111;
                        _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                        _M0L6_2atmpS951 = _M0Lm9_2acursorS102;
                        if (_M0L6_2atmpS951 < _M0L6_2aendS101) {
                          int32_t _M0L6_2atmpS956 = _M0Lm9_2acursorS102;
                          int32_t _M0L10next__charS142;
                          int32_t _M0L6_2atmpS952;
                          moonbit_incref(_M0L7_2adataS99);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS142
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS956);
                          _M0L6_2atmpS952 = _M0Lm9_2acursorS102;
                          _M0Lm9_2acursorS102 = _M0L6_2atmpS952 + 1;
                          if (_M0L10next__charS142 < 58) {
                            if (_M0L10next__charS142 < 48) {
                              goto join_141;
                            } else {
                              int32_t _M0L6_2atmpS953;
                              _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                              _M0Lm9tag__2__1S116 = _M0Lm6tag__2S115;
                              _M0Lm6tag__2S115 = _M0Lm9_2acursorS102;
                              _M0L6_2atmpS953 = _M0Lm9_2acursorS102;
                              if (_M0L6_2atmpS953 < _M0L6_2aendS101) {
                                int32_t _M0L6_2atmpS955 = _M0Lm9_2acursorS102;
                                int32_t _M0L10next__charS144;
                                int32_t _M0L6_2atmpS954;
                                moonbit_incref(_M0L7_2adataS99);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS144
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS955);
                                _M0L6_2atmpS954 = _M0Lm9_2acursorS102;
                                _M0Lm9_2acursorS102 = _M0L6_2atmpS954 + 1;
                                if (_M0L10next__charS144 < 58) {
                                  if (_M0L10next__charS144 < 48) {
                                    goto join_143;
                                  } else {
                                    _M0L12dispatch__15S126 = 5;
                                    goto loop__label__15_129;
                                  }
                                } else if (_M0L10next__charS144 > 58) {
                                  goto join_143;
                                } else {
                                  _M0L12dispatch__15S126 = 3;
                                  goto loop__label__15_129;
                                }
                                join_143:;
                                _M0L12dispatch__15S126 = 0;
                                goto loop__label__15_129;
                              } else {
                                goto join_131;
                              }
                            }
                          } else if (_M0L10next__charS142 > 58) {
                            goto join_141;
                          } else {
                            _M0L12dispatch__15S126 = 1;
                            goto loop__label__15_129;
                          }
                          join_141:;
                          _M0L12dispatch__15S126 = 0;
                          goto loop__label__15_129;
                        } else {
                          goto join_118;
                        }
                      }
                      join_139:;
                      _M0L12dispatch__15S126 = 0;
                      goto loop__label__15_129;
                    } else {
                      goto join_118;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS958;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0Lm6tag__2S115 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS958 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS958 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS960 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS146;
                      int32_t _M0L6_2atmpS959;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS146
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS960);
                      _M0L6_2atmpS959 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS959 + 1;
                      if (_M0L10next__charS146 < 58) {
                        if (_M0L10next__charS146 < 48) {
                          goto join_145;
                        } else {
                          _M0L12dispatch__15S126 = 5;
                          goto loop__label__15_129;
                        }
                      } else if (_M0L10next__charS146 > 58) {
                        goto join_145;
                      } else {
                        _M0L12dispatch__15S126 = 3;
                        goto loop__label__15_129;
                      }
                      join_145:;
                      _M0L12dispatch__15S126 = 0;
                      goto loop__label__15_129;
                    } else {
                      goto join_131;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS961;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0Lm6tag__2S115 = _M0Lm9_2acursorS102;
                    _M0Lm6tag__3S114 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS961 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS961 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS963 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS148;
                      int32_t _M0L6_2atmpS962;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS148
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS963);
                      _M0L6_2atmpS962 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS962 + 1;
                      if (_M0L10next__charS148 < 48) {
                        if (_M0L10next__charS148 == 45) {
                          goto join_127;
                        } else {
                          goto join_147;
                        }
                      } else if (_M0L10next__charS148 > 57) {
                        if (_M0L10next__charS148 < 59) {
                          _M0L12dispatch__15S126 = 3;
                          goto loop__label__15_129;
                        } else {
                          goto join_147;
                        }
                      } else {
                        _M0L12dispatch__15S126 = 6;
                        goto loop__label__15_129;
                      }
                      join_147:;
                      _M0L12dispatch__15S126 = 0;
                      goto loop__label__15_129;
                    } else {
                      goto join_118;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS964;
                    _M0Lm9tag__1__1S112 = _M0Lm6tag__1S111;
                    _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                    _M0L6_2atmpS964 = _M0Lm9_2acursorS102;
                    if (_M0L6_2atmpS964 < _M0L6_2aendS101) {
                      int32_t _M0L6_2atmpS966 = _M0Lm9_2acursorS102;
                      int32_t _M0L10next__charS150;
                      int32_t _M0L6_2atmpS965;
                      moonbit_incref(_M0L7_2adataS99);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS150
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS966);
                      _M0L6_2atmpS965 = _M0Lm9_2acursorS102;
                      _M0Lm9_2acursorS102 = _M0L6_2atmpS965 + 1;
                      if (_M0L10next__charS150 < 58) {
                        if (_M0L10next__charS150 < 48) {
                          goto join_149;
                        } else {
                          _M0L12dispatch__15S126 = 2;
                          goto loop__label__15_129;
                        }
                      } else if (_M0L10next__charS150 > 58) {
                        goto join_149;
                      } else {
                        _M0L12dispatch__15S126 = 1;
                        goto loop__label__15_129;
                      }
                      join_149:;
                      _M0L12dispatch__15S126 = 0;
                      goto loop__label__15_129;
                    } else {
                      goto join_118;
                    }
                    break;
                  }
                  default: {
                    goto join_118;
                    break;
                  }
                }
                join_131:;
                _M0Lm6tag__1S111 = _M0Lm9tag__1__2S113;
                _M0Lm6tag__2S115 = _M0Lm9tag__2__1S116;
                _M0Lm20match__tag__saver__0S105 = _M0Lm6tag__0S110;
                _M0Lm20match__tag__saver__1S106 = _M0Lm6tag__1S111;
                _M0Lm20match__tag__saver__2S107 = _M0Lm6tag__2S115;
                _M0Lm20match__tag__saver__3S108 = _M0Lm6tag__3S114;
                _M0Lm20match__tag__saver__4S109 = _M0Lm6tag__4S117;
                _M0Lm13accept__stateS103 = 0;
                _M0Lm10match__endS104 = _M0Lm9_2acursorS102;
                goto join_118;
                join_127:;
                _M0Lm9tag__1__1S112 = _M0Lm9tag__1__2S113;
                _M0Lm6tag__1S111 = _M0Lm9_2acursorS102;
                _M0Lm6tag__2S115 = _M0Lm9tag__2__1S116;
                _M0L6_2atmpS934 = _M0Lm9_2acursorS102;
                if (_M0L6_2atmpS934 < _M0L6_2aendS101) {
                  int32_t _M0L6_2atmpS936 = _M0Lm9_2acursorS102;
                  int32_t _M0L10next__charS130;
                  int32_t _M0L6_2atmpS935;
                  moonbit_incref(_M0L7_2adataS99);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS130
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS99, _M0L6_2atmpS936);
                  _M0L6_2atmpS935 = _M0Lm9_2acursorS102;
                  _M0Lm9_2acursorS102 = _M0L6_2atmpS935 + 1;
                  if (_M0L10next__charS130 < 58) {
                    if (_M0L10next__charS130 < 48) {
                      goto join_128;
                    } else {
                      _M0L12dispatch__15S126 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS130 > 58) {
                    goto join_128;
                  } else {
                    _M0L12dispatch__15S126 = 1;
                    continue;
                  }
                  join_128:;
                  _M0L12dispatch__15S126 = 0;
                  continue;
                } else {
                  goto join_118;
                }
                break;
              }
            } else {
              goto join_118;
            }
          } else {
            continue;
          }
        } else {
          goto join_118;
        }
        break;
      }
    } else {
      goto join_118;
    }
  } else {
    goto join_118;
  }
  join_118:;
  switch (_M0Lm13accept__stateS103) {
    case 0: {
      int32_t _M0L6_2atmpS925 = _M0Lm20match__tag__saver__1S106;
      int32_t _M0L6_2atmpS924 = _M0L6_2atmpS925 + 1;
      int64_t _M0L6_2atmpS921 = (int64_t)_M0L6_2atmpS924;
      int32_t _M0L6_2atmpS923 = _M0Lm20match__tag__saver__2S107;
      int64_t _M0L6_2atmpS922 = (int64_t)_M0L6_2atmpS923;
      struct _M0TPC16string10StringView _M0L11start__lineS119;
      int32_t _M0L6_2atmpS920;
      int32_t _M0L6_2atmpS919;
      int64_t _M0L6_2atmpS916;
      int32_t _M0L6_2atmpS918;
      int64_t _M0L6_2atmpS917;
      struct _M0TPC16string10StringView _M0L13start__columnS120;
      int32_t _M0L6_2atmpS915;
      int64_t _M0L6_2atmpS912;
      int32_t _M0L6_2atmpS914;
      int64_t _M0L6_2atmpS913;
      struct _M0TPC16string10StringView _M0L3pkgS121;
      int32_t _M0L6_2atmpS911;
      int32_t _M0L6_2atmpS910;
      int64_t _M0L6_2atmpS907;
      int32_t _M0L6_2atmpS909;
      int64_t _M0L6_2atmpS908;
      struct _M0TPC16string10StringView _M0L8filenameS122;
      int32_t _M0L6_2atmpS906;
      int32_t _M0L6_2atmpS905;
      int64_t _M0L6_2atmpS902;
      int32_t _M0L6_2atmpS904;
      int64_t _M0L6_2atmpS903;
      struct _M0TPC16string10StringView _M0L9end__lineS123;
      int32_t _M0L6_2atmpS901;
      int32_t _M0L6_2atmpS900;
      int64_t _M0L6_2atmpS897;
      int32_t _M0L6_2atmpS899;
      int64_t _M0L6_2atmpS898;
      struct _M0TPC16string10StringView _M0L11end__columnS124;
      struct _M0TPB13SourceLocRepr* _block_2516;
      moonbit_incref(_M0L7_2adataS99);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS119
      = _M0MPC16string6String4view(_M0L7_2adataS99, _M0L6_2atmpS921, _M0L6_2atmpS922);
      _M0L6_2atmpS920 = _M0Lm20match__tag__saver__2S107;
      _M0L6_2atmpS919 = _M0L6_2atmpS920 + 1;
      _M0L6_2atmpS916 = (int64_t)_M0L6_2atmpS919;
      _M0L6_2atmpS918 = _M0Lm20match__tag__saver__3S108;
      _M0L6_2atmpS917 = (int64_t)_M0L6_2atmpS918;
      moonbit_incref(_M0L7_2adataS99);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS120
      = _M0MPC16string6String4view(_M0L7_2adataS99, _M0L6_2atmpS916, _M0L6_2atmpS917);
      _M0L6_2atmpS915 = _M0L8_2astartS100 + 1;
      _M0L6_2atmpS912 = (int64_t)_M0L6_2atmpS915;
      _M0L6_2atmpS914 = _M0Lm20match__tag__saver__0S105;
      _M0L6_2atmpS913 = (int64_t)_M0L6_2atmpS914;
      moonbit_incref(_M0L7_2adataS99);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS121
      = _M0MPC16string6String4view(_M0L7_2adataS99, _M0L6_2atmpS912, _M0L6_2atmpS913);
      _M0L6_2atmpS911 = _M0Lm20match__tag__saver__0S105;
      _M0L6_2atmpS910 = _M0L6_2atmpS911 + 1;
      _M0L6_2atmpS907 = (int64_t)_M0L6_2atmpS910;
      _M0L6_2atmpS909 = _M0Lm20match__tag__saver__1S106;
      _M0L6_2atmpS908 = (int64_t)_M0L6_2atmpS909;
      moonbit_incref(_M0L7_2adataS99);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS122
      = _M0MPC16string6String4view(_M0L7_2adataS99, _M0L6_2atmpS907, _M0L6_2atmpS908);
      _M0L6_2atmpS906 = _M0Lm20match__tag__saver__3S108;
      _M0L6_2atmpS905 = _M0L6_2atmpS906 + 1;
      _M0L6_2atmpS902 = (int64_t)_M0L6_2atmpS905;
      _M0L6_2atmpS904 = _M0Lm20match__tag__saver__4S109;
      _M0L6_2atmpS903 = (int64_t)_M0L6_2atmpS904;
      moonbit_incref(_M0L7_2adataS99);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS123
      = _M0MPC16string6String4view(_M0L7_2adataS99, _M0L6_2atmpS902, _M0L6_2atmpS903);
      _M0L6_2atmpS901 = _M0Lm20match__tag__saver__4S109;
      _M0L6_2atmpS900 = _M0L6_2atmpS901 + 1;
      _M0L6_2atmpS897 = (int64_t)_M0L6_2atmpS900;
      _M0L6_2atmpS899 = _M0Lm10match__endS104;
      _M0L6_2atmpS898 = (int64_t)_M0L6_2atmpS899;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS124
      = _M0MPC16string6String4view(_M0L7_2adataS99, _M0L6_2atmpS897, _M0L6_2atmpS898);
      _block_2516
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2516)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_2516->$0_0 = _M0L3pkgS121.$0;
      _block_2516->$0_1 = _M0L3pkgS121.$1;
      _block_2516->$0_2 = _M0L3pkgS121.$2;
      _block_2516->$1_0 = _M0L8filenameS122.$0;
      _block_2516->$1_1 = _M0L8filenameS122.$1;
      _block_2516->$1_2 = _M0L8filenameS122.$2;
      _block_2516->$2_0 = _M0L11start__lineS119.$0;
      _block_2516->$2_1 = _M0L11start__lineS119.$1;
      _block_2516->$2_2 = _M0L11start__lineS119.$2;
      _block_2516->$3_0 = _M0L13start__columnS120.$0;
      _block_2516->$3_1 = _M0L13start__columnS120.$1;
      _block_2516->$3_2 = _M0L13start__columnS120.$2;
      _block_2516->$4_0 = _M0L9end__lineS123.$0;
      _block_2516->$4_1 = _M0L9end__lineS123.$1;
      _block_2516->$4_2 = _M0L9end__lineS123.$2;
      _block_2516->$5_0 = _M0L11end__columnS124.$0;
      _block_2516->$5_1 = _M0L11end__columnS124.$1;
      _block_2516->$5_2 = _M0L11end__columnS124.$2;
      return _block_2516;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS99);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS95,
  int32_t _M0L5indexS96
) {
  int32_t _M0L3lenS94;
  int32_t _if__result_2517;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS94 = _M0L4selfS95->$1;
  if (_M0L5indexS96 >= 0) {
    _if__result_2517 = _M0L5indexS96 < _M0L3lenS94;
  } else {
    _if__result_2517 = 0;
  }
  if (_if__result_2517) {
    moonbit_string_t* _M0L6_2atmpS896;
    moonbit_string_t _M0L6_2atmpS2314;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS896 = _M0MPC15array5Array6bufferGsE(_M0L4selfS95);
    if (
      _M0L5indexS96 < 0
      || _M0L5indexS96 >= Moonbit_array_length(_M0L6_2atmpS896)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2314 = (moonbit_string_t)_M0L6_2atmpS896[_M0L5indexS96];
    moonbit_incref(_M0L6_2atmpS2314);
    moonbit_decref(_M0L6_2atmpS896);
    return _M0L6_2atmpS2314;
  } else {
    moonbit_decref(_M0L4selfS95);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS92
) {
  moonbit_string_t* _M0L8_2afieldS2315;
  int32_t _M0L6_2acntS2392;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2315 = _M0L4selfS92->$0;
  _M0L6_2acntS2392 = Moonbit_object_header(_M0L4selfS92)->rc;
  if (_M0L6_2acntS2392 > 1) {
    int32_t _M0L11_2anew__cntS2393 = _M0L6_2acntS2392 - 1;
    Moonbit_object_header(_M0L4selfS92)->rc = _M0L11_2anew__cntS2393;
    moonbit_incref(_M0L8_2afieldS2315);
  } else if (_M0L6_2acntS2392 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS92);
  }
  return _M0L8_2afieldS2315;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS93
) {
  struct _M0TUsiE** _M0L8_2afieldS2316;
  int32_t _M0L6_2acntS2394;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2316 = _M0L4selfS93->$0;
  _M0L6_2acntS2394 = Moonbit_object_header(_M0L4selfS93)->rc;
  if (_M0L6_2acntS2394 > 1) {
    int32_t _M0L11_2anew__cntS2395 = _M0L6_2acntS2394 - 1;
    Moonbit_object_header(_M0L4selfS93)->rc = _M0L11_2anew__cntS2395;
    moonbit_incref(_M0L8_2afieldS2316);
  } else if (_M0L6_2acntS2394 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS93);
  }
  return _M0L8_2afieldS2316;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS91) {
  struct _M0TPB13StringBuilder* _M0L3bufS90;
  struct _M0TPB6Logger _M0L6_2atmpS895;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS90 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS90);
  _M0L6_2atmpS895
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS90
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS91, _M0L6_2atmpS895);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS90);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS894;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS894 = (int32_t)_M0L4selfS89;
  return _M0L6_2atmpS894;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS87,
  int32_t _M0L8trailingS88
) {
  int32_t _M0L6_2atmpS893;
  int32_t _M0L6_2atmpS892;
  int32_t _M0L6_2atmpS891;
  int32_t _M0L6_2atmpS890;
  int32_t _M0L6_2atmpS889;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS893 = _M0L7leadingS87 - 55296;
  _M0L6_2atmpS892 = _M0L6_2atmpS893 * 1024;
  _M0L6_2atmpS891 = _M0L6_2atmpS892 + _M0L8trailingS88;
  _M0L6_2atmpS890 = _M0L6_2atmpS891 - 56320;
  _M0L6_2atmpS889 = _M0L6_2atmpS890 + 65536;
  return _M0L6_2atmpS889;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS86) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS86 >= 56320) {
    return _M0L4selfS86 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS85) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS85 >= 55296) {
    return _M0L4selfS85 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS82,
  int32_t _M0L2chS84
) {
  int32_t _M0L3lenS884;
  int32_t _M0L6_2atmpS883;
  moonbit_bytes_t _M0L8_2afieldS2317;
  moonbit_bytes_t _M0L4dataS887;
  int32_t _M0L3lenS888;
  int32_t _M0L3incS83;
  int32_t _M0L3lenS886;
  int32_t _M0L6_2atmpS885;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS884 = _M0L4selfS82->$1;
  _M0L6_2atmpS883 = _M0L3lenS884 + 4;
  moonbit_incref(_M0L4selfS82);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS82, _M0L6_2atmpS883);
  _M0L8_2afieldS2317 = _M0L4selfS82->$0;
  _M0L4dataS887 = _M0L8_2afieldS2317;
  _M0L3lenS888 = _M0L4selfS82->$1;
  moonbit_incref(_M0L4dataS887);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS83
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS887, _M0L3lenS888, _M0L2chS84);
  _M0L3lenS886 = _M0L4selfS82->$1;
  _M0L6_2atmpS885 = _M0L3lenS886 + _M0L3incS83;
  _M0L4selfS82->$1 = _M0L6_2atmpS885;
  moonbit_decref(_M0L4selfS82);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS77,
  int32_t _M0L8requiredS78
) {
  moonbit_bytes_t _M0L8_2afieldS2321;
  moonbit_bytes_t _M0L4dataS882;
  int32_t _M0L6_2atmpS2320;
  int32_t _M0L12current__lenS76;
  int32_t _M0Lm13enough__spaceS79;
  int32_t _M0L6_2atmpS880;
  int32_t _M0L6_2atmpS881;
  moonbit_bytes_t _M0L9new__dataS81;
  moonbit_bytes_t _M0L8_2afieldS2319;
  moonbit_bytes_t _M0L4dataS878;
  int32_t _M0L3lenS879;
  moonbit_bytes_t _M0L6_2aoldS2318;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2321 = _M0L4selfS77->$0;
  _M0L4dataS882 = _M0L8_2afieldS2321;
  _M0L6_2atmpS2320 = Moonbit_array_length(_M0L4dataS882);
  _M0L12current__lenS76 = _M0L6_2atmpS2320;
  if (_M0L8requiredS78 <= _M0L12current__lenS76) {
    moonbit_decref(_M0L4selfS77);
    return 0;
  }
  _M0Lm13enough__spaceS79 = _M0L12current__lenS76;
  while (1) {
    int32_t _M0L6_2atmpS876 = _M0Lm13enough__spaceS79;
    if (_M0L6_2atmpS876 < _M0L8requiredS78) {
      int32_t _M0L6_2atmpS877 = _M0Lm13enough__spaceS79;
      _M0Lm13enough__spaceS79 = _M0L6_2atmpS877 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS880 = _M0Lm13enough__spaceS79;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS881 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS81
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS880, _M0L6_2atmpS881);
  _M0L8_2afieldS2319 = _M0L4selfS77->$0;
  _M0L4dataS878 = _M0L8_2afieldS2319;
  _M0L3lenS879 = _M0L4selfS77->$1;
  moonbit_incref(_M0L4dataS878);
  moonbit_incref(_M0L9new__dataS81);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS81, 0, _M0L4dataS878, 0, _M0L3lenS879);
  _M0L6_2aoldS2318 = _M0L4selfS77->$0;
  moonbit_decref(_M0L6_2aoldS2318);
  _M0L4selfS77->$0 = _M0L9new__dataS81;
  moonbit_decref(_M0L4selfS77);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS71,
  int32_t _M0L6offsetS72,
  int32_t _M0L5valueS70
) {
  uint32_t _M0L4codeS69;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS69 = _M0MPC14char4Char8to__uint(_M0L5valueS70);
  if (_M0L4codeS69 < 65536u) {
    uint32_t _M0L6_2atmpS859 = _M0L4codeS69 & 255u;
    int32_t _M0L6_2atmpS858;
    int32_t _M0L6_2atmpS860;
    uint32_t _M0L6_2atmpS862;
    int32_t _M0L6_2atmpS861;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS858 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS859);
    if (
      _M0L6offsetS72 < 0
      || _M0L6offsetS72 >= Moonbit_array_length(_M0L4selfS71)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS71[_M0L6offsetS72] = _M0L6_2atmpS858;
    _M0L6_2atmpS860 = _M0L6offsetS72 + 1;
    _M0L6_2atmpS862 = _M0L4codeS69 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS861 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS862);
    if (
      _M0L6_2atmpS860 < 0
      || _M0L6_2atmpS860 >= Moonbit_array_length(_M0L4selfS71)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS71[_M0L6_2atmpS860] = _M0L6_2atmpS861;
    moonbit_decref(_M0L4selfS71);
    return 2;
  } else if (_M0L4codeS69 < 1114112u) {
    uint32_t _M0L2hiS73 = _M0L4codeS69 - 65536u;
    uint32_t _M0L6_2atmpS875 = _M0L2hiS73 >> 10;
    uint32_t _M0L2loS74 = _M0L6_2atmpS875 | 55296u;
    uint32_t _M0L6_2atmpS874 = _M0L2hiS73 & 1023u;
    uint32_t _M0L2hiS75 = _M0L6_2atmpS874 | 56320u;
    uint32_t _M0L6_2atmpS864 = _M0L2loS74 & 255u;
    int32_t _M0L6_2atmpS863;
    int32_t _M0L6_2atmpS865;
    uint32_t _M0L6_2atmpS867;
    int32_t _M0L6_2atmpS866;
    int32_t _M0L6_2atmpS868;
    uint32_t _M0L6_2atmpS870;
    int32_t _M0L6_2atmpS869;
    int32_t _M0L6_2atmpS871;
    uint32_t _M0L6_2atmpS873;
    int32_t _M0L6_2atmpS872;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS863 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS864);
    if (
      _M0L6offsetS72 < 0
      || _M0L6offsetS72 >= Moonbit_array_length(_M0L4selfS71)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS71[_M0L6offsetS72] = _M0L6_2atmpS863;
    _M0L6_2atmpS865 = _M0L6offsetS72 + 1;
    _M0L6_2atmpS867 = _M0L2loS74 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS866 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS867);
    if (
      _M0L6_2atmpS865 < 0
      || _M0L6_2atmpS865 >= Moonbit_array_length(_M0L4selfS71)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS71[_M0L6_2atmpS865] = _M0L6_2atmpS866;
    _M0L6_2atmpS868 = _M0L6offsetS72 + 2;
    _M0L6_2atmpS870 = _M0L2hiS75 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS869 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS870);
    if (
      _M0L6_2atmpS868 < 0
      || _M0L6_2atmpS868 >= Moonbit_array_length(_M0L4selfS71)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS71[_M0L6_2atmpS868] = _M0L6_2atmpS869;
    _M0L6_2atmpS871 = _M0L6offsetS72 + 3;
    _M0L6_2atmpS873 = _M0L2hiS75 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS872 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS873);
    if (
      _M0L6_2atmpS871 < 0
      || _M0L6_2atmpS871 >= Moonbit_array_length(_M0L4selfS71)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS71[_M0L6_2atmpS871] = _M0L6_2atmpS872;
    moonbit_decref(_M0L4selfS71);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS71);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_33.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS68) {
  int32_t _M0L6_2atmpS857;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS857 = *(int32_t*)&_M0L4selfS68;
  return _M0L6_2atmpS857 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS67) {
  int32_t _M0L6_2atmpS856;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS856 = _M0L4selfS67;
  return *(uint32_t*)&_M0L6_2atmpS856;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS66
) {
  moonbit_bytes_t _M0L8_2afieldS2323;
  moonbit_bytes_t _M0L4dataS855;
  moonbit_bytes_t _M0L6_2atmpS852;
  int32_t _M0L8_2afieldS2322;
  int32_t _M0L3lenS854;
  int64_t _M0L6_2atmpS853;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2323 = _M0L4selfS66->$0;
  _M0L4dataS855 = _M0L8_2afieldS2323;
  moonbit_incref(_M0L4dataS855);
  _M0L6_2atmpS852 = _M0L4dataS855;
  _M0L8_2afieldS2322 = _M0L4selfS66->$1;
  moonbit_decref(_M0L4selfS66);
  _M0L3lenS854 = _M0L8_2afieldS2322;
  _M0L6_2atmpS853 = (int64_t)_M0L3lenS854;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS852, 0, _M0L6_2atmpS853);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS61,
  int32_t _M0L6offsetS65,
  int64_t _M0L6lengthS63
) {
  int32_t _M0L3lenS60;
  int32_t _M0L6lengthS62;
  int32_t _if__result_2519;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS60 = Moonbit_array_length(_M0L4selfS61);
  if (_M0L6lengthS63 == 4294967296ll) {
    _M0L6lengthS62 = _M0L3lenS60 - _M0L6offsetS65;
  } else {
    int64_t _M0L7_2aSomeS64 = _M0L6lengthS63;
    _M0L6lengthS62 = (int32_t)_M0L7_2aSomeS64;
  }
  if (_M0L6offsetS65 >= 0) {
    if (_M0L6lengthS62 >= 0) {
      int32_t _M0L6_2atmpS851 = _M0L6offsetS65 + _M0L6lengthS62;
      _if__result_2519 = _M0L6_2atmpS851 <= _M0L3lenS60;
    } else {
      _if__result_2519 = 0;
    }
  } else {
    _if__result_2519 = 0;
  }
  if (_if__result_2519) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS61, _M0L6offsetS65, _M0L6lengthS62);
  } else {
    moonbit_decref(_M0L4selfS61);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS58
) {
  int32_t _M0L7initialS57;
  moonbit_bytes_t _M0L4dataS59;
  struct _M0TPB13StringBuilder* _block_2520;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS58 < 1) {
    _M0L7initialS57 = 1;
  } else {
    _M0L7initialS57 = _M0L10size__hintS58;
  }
  _M0L4dataS59 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS57, 0);
  _block_2520
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2520)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2520->$0 = _M0L4dataS59;
  _block_2520->$1 = 0;
  return _block_2520;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS56) {
  int32_t _M0L6_2atmpS850;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS850 = (int32_t)_M0L4selfS56;
  return _M0L6_2atmpS850;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS46,
  int32_t _M0L11dst__offsetS47,
  moonbit_string_t* _M0L3srcS48,
  int32_t _M0L11src__offsetS49,
  int32_t _M0L3lenS50
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS46, _M0L11dst__offsetS47, _M0L3srcS48, _M0L11src__offsetS49, _M0L3lenS50);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS51,
  int32_t _M0L11dst__offsetS52,
  struct _M0TUsiE** _M0L3srcS53,
  int32_t _M0L11src__offsetS54,
  int32_t _M0L3lenS55
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS51, _M0L11dst__offsetS52, _M0L3srcS53, _M0L11src__offsetS54, _M0L3lenS55);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS19,
  int32_t _M0L11dst__offsetS21,
  moonbit_bytes_t _M0L3srcS20,
  int32_t _M0L11src__offsetS22,
  int32_t _M0L3lenS24
) {
  int32_t _if__result_2521;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS19 == _M0L3srcS20) {
    _if__result_2521 = _M0L11dst__offsetS21 < _M0L11src__offsetS22;
  } else {
    _if__result_2521 = 0;
  }
  if (_if__result_2521) {
    int32_t _M0L1iS23 = 0;
    while (1) {
      if (_M0L1iS23 < _M0L3lenS24) {
        int32_t _M0L6_2atmpS823 = _M0L11dst__offsetS21 + _M0L1iS23;
        int32_t _M0L6_2atmpS825 = _M0L11src__offsetS22 + _M0L1iS23;
        int32_t _M0L6_2atmpS824;
        int32_t _M0L6_2atmpS826;
        if (
          _M0L6_2atmpS825 < 0
          || _M0L6_2atmpS825 >= Moonbit_array_length(_M0L3srcS20)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS824 = (int32_t)_M0L3srcS20[_M0L6_2atmpS825];
        if (
          _M0L6_2atmpS823 < 0
          || _M0L6_2atmpS823 >= Moonbit_array_length(_M0L3dstS19)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS19[_M0L6_2atmpS823] = _M0L6_2atmpS824;
        _M0L6_2atmpS826 = _M0L1iS23 + 1;
        _M0L1iS23 = _M0L6_2atmpS826;
        continue;
      } else {
        moonbit_decref(_M0L3srcS20);
        moonbit_decref(_M0L3dstS19);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS831 = _M0L3lenS24 - 1;
    int32_t _M0L1iS26 = _M0L6_2atmpS831;
    while (1) {
      if (_M0L1iS26 >= 0) {
        int32_t _M0L6_2atmpS827 = _M0L11dst__offsetS21 + _M0L1iS26;
        int32_t _M0L6_2atmpS829 = _M0L11src__offsetS22 + _M0L1iS26;
        int32_t _M0L6_2atmpS828;
        int32_t _M0L6_2atmpS830;
        if (
          _M0L6_2atmpS829 < 0
          || _M0L6_2atmpS829 >= Moonbit_array_length(_M0L3srcS20)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS828 = (int32_t)_M0L3srcS20[_M0L6_2atmpS829];
        if (
          _M0L6_2atmpS827 < 0
          || _M0L6_2atmpS827 >= Moonbit_array_length(_M0L3dstS19)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS19[_M0L6_2atmpS827] = _M0L6_2atmpS828;
        _M0L6_2atmpS830 = _M0L1iS26 - 1;
        _M0L1iS26 = _M0L6_2atmpS830;
        continue;
      } else {
        moonbit_decref(_M0L3srcS20);
        moonbit_decref(_M0L3dstS19);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS28,
  int32_t _M0L11dst__offsetS30,
  moonbit_string_t* _M0L3srcS29,
  int32_t _M0L11src__offsetS31,
  int32_t _M0L3lenS33
) {
  int32_t _if__result_2524;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS28 == _M0L3srcS29) {
    _if__result_2524 = _M0L11dst__offsetS30 < _M0L11src__offsetS31;
  } else {
    _if__result_2524 = 0;
  }
  if (_if__result_2524) {
    int32_t _M0L1iS32 = 0;
    while (1) {
      if (_M0L1iS32 < _M0L3lenS33) {
        int32_t _M0L6_2atmpS832 = _M0L11dst__offsetS30 + _M0L1iS32;
        int32_t _M0L6_2atmpS834 = _M0L11src__offsetS31 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS2325;
        moonbit_string_t _M0L6_2atmpS833;
        moonbit_string_t _M0L6_2aoldS2324;
        int32_t _M0L6_2atmpS835;
        if (
          _M0L6_2atmpS834 < 0
          || _M0L6_2atmpS834 >= Moonbit_array_length(_M0L3srcS29)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2325 = (moonbit_string_t)_M0L3srcS29[_M0L6_2atmpS834];
        _M0L6_2atmpS833 = _M0L6_2atmpS2325;
        if (
          _M0L6_2atmpS832 < 0
          || _M0L6_2atmpS832 >= Moonbit_array_length(_M0L3dstS28)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2324 = (moonbit_string_t)_M0L3dstS28[_M0L6_2atmpS832];
        moonbit_incref(_M0L6_2atmpS833);
        moonbit_decref(_M0L6_2aoldS2324);
        _M0L3dstS28[_M0L6_2atmpS832] = _M0L6_2atmpS833;
        _M0L6_2atmpS835 = _M0L1iS32 + 1;
        _M0L1iS32 = _M0L6_2atmpS835;
        continue;
      } else {
        moonbit_decref(_M0L3srcS29);
        moonbit_decref(_M0L3dstS28);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS840 = _M0L3lenS33 - 1;
    int32_t _M0L1iS35 = _M0L6_2atmpS840;
    while (1) {
      if (_M0L1iS35 >= 0) {
        int32_t _M0L6_2atmpS836 = _M0L11dst__offsetS30 + _M0L1iS35;
        int32_t _M0L6_2atmpS838 = _M0L11src__offsetS31 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS2327;
        moonbit_string_t _M0L6_2atmpS837;
        moonbit_string_t _M0L6_2aoldS2326;
        int32_t _M0L6_2atmpS839;
        if (
          _M0L6_2atmpS838 < 0
          || _M0L6_2atmpS838 >= Moonbit_array_length(_M0L3srcS29)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2327 = (moonbit_string_t)_M0L3srcS29[_M0L6_2atmpS838];
        _M0L6_2atmpS837 = _M0L6_2atmpS2327;
        if (
          _M0L6_2atmpS836 < 0
          || _M0L6_2atmpS836 >= Moonbit_array_length(_M0L3dstS28)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2326 = (moonbit_string_t)_M0L3dstS28[_M0L6_2atmpS836];
        moonbit_incref(_M0L6_2atmpS837);
        moonbit_decref(_M0L6_2aoldS2326);
        _M0L3dstS28[_M0L6_2atmpS836] = _M0L6_2atmpS837;
        _M0L6_2atmpS839 = _M0L1iS35 - 1;
        _M0L1iS35 = _M0L6_2atmpS839;
        continue;
      } else {
        moonbit_decref(_M0L3srcS29);
        moonbit_decref(_M0L3dstS28);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS37,
  int32_t _M0L11dst__offsetS39,
  struct _M0TUsiE** _M0L3srcS38,
  int32_t _M0L11src__offsetS40,
  int32_t _M0L3lenS42
) {
  int32_t _if__result_2527;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS37 == _M0L3srcS38) {
    _if__result_2527 = _M0L11dst__offsetS39 < _M0L11src__offsetS40;
  } else {
    _if__result_2527 = 0;
  }
  if (_if__result_2527) {
    int32_t _M0L1iS41 = 0;
    while (1) {
      if (_M0L1iS41 < _M0L3lenS42) {
        int32_t _M0L6_2atmpS841 = _M0L11dst__offsetS39 + _M0L1iS41;
        int32_t _M0L6_2atmpS843 = _M0L11src__offsetS40 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS2329;
        struct _M0TUsiE* _M0L6_2atmpS842;
        struct _M0TUsiE* _M0L6_2aoldS2328;
        int32_t _M0L6_2atmpS844;
        if (
          _M0L6_2atmpS843 < 0
          || _M0L6_2atmpS843 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2329 = (struct _M0TUsiE*)_M0L3srcS38[_M0L6_2atmpS843];
        _M0L6_2atmpS842 = _M0L6_2atmpS2329;
        if (
          _M0L6_2atmpS841 < 0
          || _M0L6_2atmpS841 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2328 = (struct _M0TUsiE*)_M0L3dstS37[_M0L6_2atmpS841];
        if (_M0L6_2atmpS842) {
          moonbit_incref(_M0L6_2atmpS842);
        }
        if (_M0L6_2aoldS2328) {
          moonbit_decref(_M0L6_2aoldS2328);
        }
        _M0L3dstS37[_M0L6_2atmpS841] = _M0L6_2atmpS842;
        _M0L6_2atmpS844 = _M0L1iS41 + 1;
        _M0L1iS41 = _M0L6_2atmpS844;
        continue;
      } else {
        moonbit_decref(_M0L3srcS38);
        moonbit_decref(_M0L3dstS37);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS849 = _M0L3lenS42 - 1;
    int32_t _M0L1iS44 = _M0L6_2atmpS849;
    while (1) {
      if (_M0L1iS44 >= 0) {
        int32_t _M0L6_2atmpS845 = _M0L11dst__offsetS39 + _M0L1iS44;
        int32_t _M0L6_2atmpS847 = _M0L11src__offsetS40 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS2331;
        struct _M0TUsiE* _M0L6_2atmpS846;
        struct _M0TUsiE* _M0L6_2aoldS2330;
        int32_t _M0L6_2atmpS848;
        if (
          _M0L6_2atmpS847 < 0
          || _M0L6_2atmpS847 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2331 = (struct _M0TUsiE*)_M0L3srcS38[_M0L6_2atmpS847];
        _M0L6_2atmpS846 = _M0L6_2atmpS2331;
        if (
          _M0L6_2atmpS845 < 0
          || _M0L6_2atmpS845 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2330 = (struct _M0TUsiE*)_M0L3dstS37[_M0L6_2atmpS845];
        if (_M0L6_2atmpS846) {
          moonbit_incref(_M0L6_2atmpS846);
        }
        if (_M0L6_2aoldS2330) {
          moonbit_decref(_M0L6_2aoldS2330);
        }
        _M0L3dstS37[_M0L6_2atmpS845] = _M0L6_2atmpS846;
        _M0L6_2atmpS848 = _M0L1iS44 - 1;
        _M0L1iS44 = _M0L6_2atmpS848;
        continue;
      } else {
        moonbit_decref(_M0L3srcS38);
        moonbit_decref(_M0L3dstS37);
      }
      break;
    }
  }
  return 0;
}

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L6stringS7,
  moonbit_string_t _M0L3locS8
) {
  moonbit_string_t _M0L6_2atmpS797;
  moonbit_string_t _M0L6_2atmpS2334;
  moonbit_string_t _M0L6_2atmpS795;
  moonbit_string_t _M0L6_2atmpS796;
  moonbit_string_t _M0L6_2atmpS2333;
  moonbit_string_t _M0L6_2atmpS794;
  moonbit_string_t _M0L6_2atmpS2332;
  moonbit_string_t _M0L6_2atmpS793;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS797 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS7);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2334
  = moonbit_add_string(_M0L6_2atmpS797, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS797);
  _M0L6_2atmpS795 = _M0L6_2atmpS2334;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS796
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS8);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2333 = moonbit_add_string(_M0L6_2atmpS795, _M0L6_2atmpS796);
  moonbit_decref(_M0L6_2atmpS795);
  moonbit_decref(_M0L6_2atmpS796);
  _M0L6_2atmpS794 = _M0L6_2atmpS2333;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2332
  = moonbit_add_string(_M0L6_2atmpS794, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS794);
  _M0L6_2atmpS793 = _M0L6_2atmpS2332;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC15bytes9BytesViewE(_M0L6_2atmpS793);
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS9,
  moonbit_string_t _M0L3locS10
) {
  moonbit_string_t _M0L6_2atmpS802;
  moonbit_string_t _M0L6_2atmpS2337;
  moonbit_string_t _M0L6_2atmpS800;
  moonbit_string_t _M0L6_2atmpS801;
  moonbit_string_t _M0L6_2atmpS2336;
  moonbit_string_t _M0L6_2atmpS799;
  moonbit_string_t _M0L6_2atmpS2335;
  moonbit_string_t _M0L6_2atmpS798;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS802 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS9);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2337
  = moonbit_add_string(_M0L6_2atmpS802, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS802);
  _M0L6_2atmpS800 = _M0L6_2atmpS2337;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS801
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS10);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2336 = moonbit_add_string(_M0L6_2atmpS800, _M0L6_2atmpS801);
  moonbit_decref(_M0L6_2atmpS800);
  moonbit_decref(_M0L6_2atmpS801);
  _M0L6_2atmpS799 = _M0L6_2atmpS2336;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2335
  = moonbit_add_string(_M0L6_2atmpS799, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS799);
  _M0L6_2atmpS798 = _M0L6_2atmpS2335;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS798);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS11,
  moonbit_string_t _M0L3locS12
) {
  moonbit_string_t _M0L6_2atmpS807;
  moonbit_string_t _M0L6_2atmpS2340;
  moonbit_string_t _M0L6_2atmpS805;
  moonbit_string_t _M0L6_2atmpS806;
  moonbit_string_t _M0L6_2atmpS2339;
  moonbit_string_t _M0L6_2atmpS804;
  moonbit_string_t _M0L6_2atmpS2338;
  moonbit_string_t _M0L6_2atmpS803;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS807 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS11);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2340
  = moonbit_add_string(_M0L6_2atmpS807, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS807);
  _M0L6_2atmpS805 = _M0L6_2atmpS2340;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS806
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS12);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2339 = moonbit_add_string(_M0L6_2atmpS805, _M0L6_2atmpS806);
  moonbit_decref(_M0L6_2atmpS805);
  moonbit_decref(_M0L6_2atmpS806);
  _M0L6_2atmpS804 = _M0L6_2atmpS2339;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2338
  = moonbit_add_string(_M0L6_2atmpS804, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS804);
  _M0L6_2atmpS803 = _M0L6_2atmpS2338;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS803);
  return 0;
}

struct _M0TPB9ArrayViewGyE _M0FPB5abortGRPB9ArrayViewGyEE(
  moonbit_string_t _M0L6stringS13,
  moonbit_string_t _M0L3locS14
) {
  moonbit_string_t _M0L6_2atmpS812;
  moonbit_string_t _M0L6_2atmpS2343;
  moonbit_string_t _M0L6_2atmpS810;
  moonbit_string_t _M0L6_2atmpS811;
  moonbit_string_t _M0L6_2atmpS2342;
  moonbit_string_t _M0L6_2atmpS809;
  moonbit_string_t _M0L6_2atmpS2341;
  moonbit_string_t _M0L6_2atmpS808;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS812 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS13);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2343
  = moonbit_add_string(_M0L6_2atmpS812, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS812);
  _M0L6_2atmpS810 = _M0L6_2atmpS2343;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS811
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS14);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2342 = moonbit_add_string(_M0L6_2atmpS810, _M0L6_2atmpS811);
  moonbit_decref(_M0L6_2atmpS810);
  moonbit_decref(_M0L6_2atmpS811);
  _M0L6_2atmpS809 = _M0L6_2atmpS2342;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2341
  = moonbit_add_string(_M0L6_2atmpS809, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS809);
  _M0L6_2atmpS808 = _M0L6_2atmpS2341;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPB9ArrayViewGyEE(_M0L6_2atmpS808);
}

int32_t _M0FPB5abortGyE(
  moonbit_string_t _M0L6stringS15,
  moonbit_string_t _M0L3locS16
) {
  moonbit_string_t _M0L6_2atmpS817;
  moonbit_string_t _M0L6_2atmpS2346;
  moonbit_string_t _M0L6_2atmpS815;
  moonbit_string_t _M0L6_2atmpS816;
  moonbit_string_t _M0L6_2atmpS2345;
  moonbit_string_t _M0L6_2atmpS814;
  moonbit_string_t _M0L6_2atmpS2344;
  moonbit_string_t _M0L6_2atmpS813;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS817 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS15);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2346
  = moonbit_add_string(_M0L6_2atmpS817, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS817);
  _M0L6_2atmpS815 = _M0L6_2atmpS2346;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS816
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2345 = moonbit_add_string(_M0L6_2atmpS815, _M0L6_2atmpS816);
  moonbit_decref(_M0L6_2atmpS815);
  moonbit_decref(_M0L6_2atmpS816);
  _M0L6_2atmpS814 = _M0L6_2atmpS2345;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2344
  = moonbit_add_string(_M0L6_2atmpS814, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS814);
  _M0L6_2atmpS813 = _M0L6_2atmpS2344;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGyE(_M0L6_2atmpS813);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS17,
  moonbit_string_t _M0L3locS18
) {
  moonbit_string_t _M0L6_2atmpS822;
  moonbit_string_t _M0L6_2atmpS2349;
  moonbit_string_t _M0L6_2atmpS820;
  moonbit_string_t _M0L6_2atmpS821;
  moonbit_string_t _M0L6_2atmpS2348;
  moonbit_string_t _M0L6_2atmpS819;
  moonbit_string_t _M0L6_2atmpS2347;
  moonbit_string_t _M0L6_2atmpS818;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS822 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2349
  = moonbit_add_string(_M0L6_2atmpS822, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS822);
  _M0L6_2atmpS820 = _M0L6_2atmpS2349;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS821
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2348 = moonbit_add_string(_M0L6_2atmpS820, _M0L6_2atmpS821);
  moonbit_decref(_M0L6_2atmpS820);
  moonbit_decref(_M0L6_2atmpS821);
  _M0L6_2atmpS819 = _M0L6_2atmpS2348;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2347
  = moonbit_add_string(_M0L6_2atmpS819, (moonbit_string_t)moonbit_string_literal_10.data);
  moonbit_decref(_M0L6_2atmpS819);
  _M0L6_2atmpS818 = _M0L6_2atmpS2347;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS818);
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S3,
  struct _M0TPB6Logger _M0L10_2ax__4934S6
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS4;
  moonbit_string_t _M0L8_2afieldS2350;
  int32_t _M0L6_2acntS2396;
  moonbit_string_t _M0L15_2a_2aarg__4935S5;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS4
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S3;
  _M0L8_2afieldS2350 = _M0L10_2aFailureS4->$0;
  _M0L6_2acntS2396 = Moonbit_object_header(_M0L10_2aFailureS4)->rc;
  if (_M0L6_2acntS2396 > 1) {
    int32_t _M0L11_2anew__cntS2397 = _M0L6_2acntS2396 - 1;
    Moonbit_object_header(_M0L10_2aFailureS4)->rc = _M0L11_2anew__cntS2397;
    moonbit_incref(_M0L8_2afieldS2350);
  } else if (_M0L6_2acntS2396 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS4);
  }
  _M0L15_2a_2aarg__4935S5 = _M0L8_2afieldS2350;
  if (_M0L10_2ax__4934S6.$1) {
    moonbit_incref(_M0L10_2ax__4934S6.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S6.$0->$method_0(_M0L10_2ax__4934S6.$1, (moonbit_string_t)moonbit_string_literal_35.data);
  if (_M0L10_2ax__4934S6.$1) {
    moonbit_incref(_M0L10_2ax__4934S6.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S6, _M0L15_2a_2aarg__4935S5);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S6.$0->$method_0(_M0L10_2ax__4934S6.$1, (moonbit_string_t)moonbit_string_literal_36.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS2,
  moonbit_string_t _M0L3objS1
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS1, _M0L4selfS2);
  return 0;
}

void _M0FP017____moonbit__initC785l332() {
  moonbit_string_t _M0L6_2atmpS787;
  moonbit_string_t _M0L6_2atmpS2351;
  moonbit_string_t _M0L6_2atmpS786;
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  moonbit_incref(_M0FP48clawteam8clawteam8internal4mock8os__args);
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0L6_2atmpS787
  = _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(_M0FP48clawteam8clawteam8internal4mock8os__args);
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0L6_2atmpS2351
  = moonbit_add_string(_M0L6_2atmpS787, (moonbit_string_t)moonbit_string_literal_37.data);
  moonbit_decref(_M0L6_2atmpS787);
  _M0L6_2atmpS786 = _M0L6_2atmpS2351;
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS786);
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS754) {
  switch (Moonbit_object_tag(_M0L4_2aeS754)) {
    case 4: {
      moonbit_decref(_M0L4_2aeS754);
      return (moonbit_string_t)moonbit_string_literal_38.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS754);
      return (moonbit_string_t)moonbit_string_literal_39.data;
      break;
    }
    
    case 5: {
      moonbit_decref(_M0L4_2aeS754);
      return (moonbit_string_t)moonbit_string_literal_40.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS754);
      return (moonbit_string_t)moonbit_string_literal_41.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS754);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS754);
      return (moonbit_string_t)moonbit_string_literal_42.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS778,
  int32_t _M0L8_2aparamS777
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS776 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS778;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS776, _M0L8_2aparamS777);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS775,
  struct _M0TPC16string10StringView _M0L8_2aparamS774
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS773 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS775;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS773, _M0L8_2aparamS774);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS772,
  moonbit_string_t _M0L8_2aparamS769,
  int32_t _M0L8_2aparamS770,
  int32_t _M0L8_2aparamS771
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS768 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS772;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS768, _M0L8_2aparamS769, _M0L8_2aparamS770, _M0L8_2aparamS771);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS767,
  moonbit_string_t _M0L8_2aparamS766
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS765 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS767;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS765, _M0L8_2aparamS766);
  return 0;
}

void moonbit_init() {
  void* _M0L11_2atry__errS613;
  void* _M0L7_2abindS611;
  moonbit_string_t _M0L7_2abindS614;
  int32_t _M0L6_2atmpS790;
  struct _M0TPC16string10StringView _M0L6_2atmpS789;
  struct moonbit_result_3 _tmp_2532;
  moonbit_string_t _M0L6_2atmpS788;
  void(* _M0L6_2atmpS784)();
  #line 292 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0FP48clawteam8clawteam8internal4mock8os__args
  = _M0FP48clawteam8clawteam8internal2os4args();
  _M0L7_2abindS614 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS790 = Moonbit_array_length(_M0L7_2abindS614);
  _M0L6_2atmpS789
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS790, _M0L7_2abindS614
  };
  #line 331 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _tmp_2532 = _M0FP48clawteam8clawteam8internal2os6getenv(_M0L6_2atmpS789);
  if (_tmp_2532.tag) {
    moonbit_string_t const _M0L5_2aokS791 = _tmp_2532.data.ok;
    _M0L6_2atmpS788 = _M0L5_2aokS791;
  } else {
    void* const _M0L6_2aerrS792 = _tmp_2532.data.err;
    _M0L11_2atry__errS613 = _M0L6_2aerrS792;
    goto join_612;
  }
  _M0L7_2abindS611
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok));
  Moonbit_object_header(_M0L7_2abindS611)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok*)_M0L7_2abindS611)->$0
  = _M0L6_2atmpS788;
  goto joinlet_2531;
  join_612:;
  _M0L7_2abindS611
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err));
  Moonbit_object_header(_M0L7_2abindS611)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err*)_M0L7_2abindS611)->$0
  = _M0L11_2atry__errS613;
  joinlet_2531:;
  switch (Moonbit_object_tag(_M0L7_2abindS611)) {
    case 1: {
      struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok* _M0L5_2aOkS615 =
        (struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok*)_M0L7_2abindS611;
      moonbit_string_t _M0L8_2afieldS2352 = _M0L5_2aOkS615->$0;
      int32_t _M0L6_2acntS2398 = Moonbit_object_header(_M0L5_2aOkS615)->rc;
      moonbit_string_t _M0L4_2axS616;
      if (_M0L6_2acntS2398 > 1) {
        int32_t _M0L11_2anew__cntS2399 = _M0L6_2acntS2398 - 1;
        Moonbit_object_header(_M0L5_2aOkS615)->rc = _M0L11_2anew__cntS2399;
        if (_M0L8_2afieldS2352) {
          moonbit_incref(_M0L8_2afieldS2352);
        }
      } else if (_M0L6_2acntS2398 == 1) {
        #line 331 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
        moonbit_free(_M0L5_2aOkS615);
      }
      _M0L4_2axS616 = _M0L8_2afieldS2352;
      if (_M0L4_2axS616 == 0) {
        if (_M0L4_2axS616) {
          moonbit_decref(_M0L4_2axS616);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS617 = _M0L4_2axS616;
        moonbit_string_t _M0L4_2axS618 = _M0L7_2aSomeS617;
        if (
          moonbit_val_array_equal(_M0L4_2axS618, (moonbit_string_t)moonbit_string_literal_45.data)
        ) {
          moonbit_decref(_M0L4_2axS618);
          goto join_610;
        } else if (
                 moonbit_val_array_equal(_M0L4_2axS618, (moonbit_string_t)moonbit_string_literal_44.data)
               ) {
          moonbit_decref(_M0L4_2axS618);
          goto join_610;
        } else {
          moonbit_decref(_M0L4_2axS618);
        }
      }
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS611);
      break;
    }
  }
  goto joinlet_2530;
  join_610:;
  _M0L6_2atmpS784 = &_M0FP017____moonbit__initC785l332;
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0FP48clawteam8clawteam8internal2os6atexit(_M0L6_2atmpS784);
  joinlet_2530:;
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS783;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS748;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS749;
  int32_t _M0L7_2abindS750;
  int32_t _M0L2__S751;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS783
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS748
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS748)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS748->$0 = _M0L6_2atmpS783;
  _M0L12async__testsS748->$1 = 0;
  #line 397 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS749
  = _M0FP38clawteam8clawteam3sdk52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS750 = _M0L7_2abindS749->$1;
  _M0L2__S751 = 0;
  while (1) {
    if (_M0L2__S751 < _M0L7_2abindS750) {
      struct _M0TUsiE** _M0L8_2afieldS2356 = _M0L7_2abindS749->$0;
      struct _M0TUsiE** _M0L3bufS782 = _M0L8_2afieldS2356;
      struct _M0TUsiE* _M0L6_2atmpS2355 =
        (struct _M0TUsiE*)_M0L3bufS782[_M0L2__S751];
      struct _M0TUsiE* _M0L3argS752 = _M0L6_2atmpS2355;
      moonbit_string_t _M0L8_2afieldS2354 = _M0L3argS752->$0;
      moonbit_string_t _M0L6_2atmpS779 = _M0L8_2afieldS2354;
      int32_t _M0L8_2afieldS2353 = _M0L3argS752->$1;
      int32_t _M0L6_2atmpS780 = _M0L8_2afieldS2353;
      int32_t _M0L6_2atmpS781;
      moonbit_incref(_M0L6_2atmpS779);
      moonbit_incref(_M0L12async__testsS748);
      #line 398 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
      _M0FP38clawteam8clawteam3sdk44moonbit__test__driver__internal__do__execute(_M0L12async__testsS748, _M0L6_2atmpS779, _M0L6_2atmpS780);
      _M0L6_2atmpS781 = _M0L2__S751 + 1;
      _M0L2__S751 = _M0L6_2atmpS781;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS749);
    }
    break;
  }
  #line 400 "E:\\moonbit\\clawteam\\sdk\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP38clawteam8clawteam3sdk28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam3sdk34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS748);
  return 0;
}