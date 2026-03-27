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
struct _M0TWEOs;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGyRPB7NoErrorE3Err;

struct _M0TPC16buffer6Buffer;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok;

struct _M0DTPC15error5Error121clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TWRPC15error5ErrorEu;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC16result6ResultGzRPB7NoErrorE2Ok;

struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPC15bytes9BytesView;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__;

struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB6Logger;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB9ArrayViewGyE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16result6ResultGzRPB7NoErrorE3Err;

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE2Ok;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGsE;

struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPC16result6ResultGyRPB7NoErrorE2Ok;

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE3Err;

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGyRPB7NoErrorE3Err {
  int32_t $0;
  
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

struct _M0DTPC15error5Error121clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC16result6ResultGzRPB7NoErrorE2Ok {
  moonbit_bytes_t $0;
  
};

struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
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

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
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

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_bytes_t $0_0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0DTPC16result6ResultGzRPB7NoErrorE3Err {
  int32_t $0;
  
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

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__ {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_bytes_t $0_0;
  
};

struct _M0TPC13ref3RefGiE {
  int32_t $0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGyRPB7NoErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGsRPC28encoding4utf89MalformedE3Err {
  void* $0;
  
};

struct moonbit_result_1 {
  int tag;
  union { moonbit_string_t ok; void* err;  } data;
  
};

struct moonbit_result_2 {
  int tag;
  union { moonbit_string_t ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

int32_t _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS681(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS676(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S663(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

int32_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN17error__to__stringS641(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN14handle__resultS632(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled41MoonBit__Test__Driver__Internal__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0FP48clawteam8clawteam8internal2os6atexit(void(*)());

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal2os4args();

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal2os6getenv(
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

struct moonbit_result_2 _M0FPC28encoding4utf814decode_2einner(
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

int32_t _M0MPC15bytes5Bytes11from__arrayC1195l455(struct _M0TWuEu*, int32_t);

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

struct _M0TPB9ArrayViewGyE _M0MPC15array10FixedArray12view_2einnerGyE(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array9ArrayView2atGyE(struct _M0TPB9ArrayViewGyE, int32_t);

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(struct _M0TPB5ArrayGsE*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1156l570(struct _M0TWEOs*);

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

int32_t _M0MPC16string10StringView4iterC1104l198(struct _M0TWEOc*);

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

void _M0FP017____moonbit__initC740l332();

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

int32_t moonbit_moonclaw_c_load_byte(void*, int32_t);

void* moonbit_moonclaw_os_getenv(moonbit_bytes_t);

int32_t moonbit_moonclaw_c_is_null(void*);

void atexit(void(*)());

uint64_t moonbit_moonclaw_c_strlen(void*);

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_13 =
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
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 52, 53, 49, 
    58, 53, 45, 52, 53, 49, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[106]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 105), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 99, 109, 100, 47, 116, 101, 115, 116, 45, 116, 111, 
    45, 98, 101, 45, 107, 105, 108, 108, 101, 100, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 
    114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_10 =
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
} const moonbit_string_literal_32 =
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
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_44 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[108]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 107), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 99, 109, 100, 47, 116, 101, 115, 116, 45, 116, 111, 
    45, 98, 101, 45, 107, 105, 108, 108, 101, 100, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_29 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 99, 109, 100, 47, 116, 101, 115, 116, 45, 116, 111, 45, 98, 101, 
    45, 107, 105, 108, 108, 101, 100, 34, 44, 32, 34, 102, 105, 108, 
    101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    67, 73, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_19 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_43 =
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
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    32, 101, 120, 105, 116, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_35 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_18 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 51, 53, 
    58, 53, 45, 49, 51, 55, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_30 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 56, 
    48, 58, 53, 45, 49, 56, 48, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    105, 110, 100, 101, 120, 32, 111, 117, 116, 32, 111, 102, 32, 98, 
    111, 117, 110, 100, 115, 58, 32, 116, 104, 101, 32, 108, 101, 110, 
    32, 105, 115, 32, 102, 114, 111, 109, 32, 48, 32, 116, 111, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_33 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    32, 98, 117, 116, 32, 116, 104, 101, 32, 105, 110, 100, 101, 120, 
    32, 105, 115, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_34 =
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
} const _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN17error__to__stringS641$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN17error__to__stringS641
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

int32_t _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S703
) {
  #line 12 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S703);
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S663;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS676;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS681;
  struct _M0TUsiE** _M0L6_2atmpS1929;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS688;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS689;
  moonbit_string_t _M0L6_2atmpS1928;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS690;
  int32_t _M0L7_2abindS691;
  int32_t _M0L2__S692;
  #line 193 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S663 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS676
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS681 = 0;
  _M0L6_2atmpS1929 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS688
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS688)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS688->$0 = _M0L6_2atmpS1929;
  _M0L16file__and__indexS688->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS689
  = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS676(_M0L57moonbit__test__driver__internal__get__cli__args__internalS676);
  #line 284 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1928 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS689, 1);
  #line 283 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS690
  = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS681(_M0L51moonbit__test__driver__internal__split__mbt__stringS681, _M0L6_2atmpS1928, 47);
  _M0L7_2abindS691 = _M0L10test__argsS690->$1;
  _M0L2__S692 = 0;
  while (1) {
    if (_M0L2__S692 < _M0L7_2abindS691) {
      moonbit_string_t* _M0L8_2afieldS1931 = _M0L10test__argsS690->$0;
      moonbit_string_t* _M0L3bufS1927 = _M0L8_2afieldS1931;
      moonbit_string_t _M0L6_2atmpS1930 =
        (moonbit_string_t)_M0L3bufS1927[_M0L2__S692];
      moonbit_string_t _M0L3argS693 = _M0L6_2atmpS1930;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS694;
      moonbit_string_t _M0L4fileS695;
      moonbit_string_t _M0L5rangeS696;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS697;
      moonbit_string_t _M0L6_2atmpS1925;
      int32_t _M0L5startS698;
      moonbit_string_t _M0L6_2atmpS1924;
      int32_t _M0L3endS699;
      int32_t _M0L1iS700;
      int32_t _M0L6_2atmpS1926;
      moonbit_incref(_M0L3argS693);
      #line 288 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS694
      = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS681(_M0L51moonbit__test__driver__internal__split__mbt__stringS681, _M0L3argS693, 58);
      moonbit_incref(_M0L16file__and__rangeS694);
      #line 289 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS695
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS694, 0);
      #line 290 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS696
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS694, 1);
      #line 291 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS697
      = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS681(_M0L51moonbit__test__driver__internal__split__mbt__stringS681, _M0L5rangeS696, 45);
      moonbit_incref(_M0L15start__and__endS697);
      #line 294 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1925
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS697, 0);
      #line 294 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L5startS698
      = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S663(_M0L45moonbit__test__driver__internal__parse__int__S663, _M0L6_2atmpS1925);
      #line 295 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1924
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS697, 1);
      #line 295 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L3endS699
      = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S663(_M0L45moonbit__test__driver__internal__parse__int__S663, _M0L6_2atmpS1924);
      _M0L1iS700 = _M0L5startS698;
      while (1) {
        if (_M0L1iS700 < _M0L3endS699) {
          struct _M0TUsiE* _M0L8_2atupleS1922;
          int32_t _M0L6_2atmpS1923;
          moonbit_incref(_M0L4fileS695);
          _M0L8_2atupleS1922
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1922)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1922->$0 = _M0L4fileS695;
          _M0L8_2atupleS1922->$1 = _M0L1iS700;
          moonbit_incref(_M0L16file__and__indexS688);
          #line 297 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS688, _M0L8_2atupleS1922);
          _M0L6_2atmpS1923 = _M0L1iS700 + 1;
          _M0L1iS700 = _M0L6_2atmpS1923;
          continue;
        } else {
          moonbit_decref(_M0L4fileS695);
        }
        break;
      }
      _M0L6_2atmpS1926 = _M0L2__S692 + 1;
      _M0L2__S692 = _M0L6_2atmpS1926;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS690);
    }
    break;
  }
  return _M0L16file__and__indexS688;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS681(
  int32_t _M0L6_2aenvS1903,
  moonbit_string_t _M0L1sS682,
  int32_t _M0L3sepS683
) {
  moonbit_string_t* _M0L6_2atmpS1921;
  struct _M0TPB5ArrayGsE* _M0L3resS684;
  struct _M0TPC13ref3RefGiE* _M0L1iS685;
  struct _M0TPC13ref3RefGiE* _M0L5startS686;
  int32_t _M0L3valS1916;
  int32_t _M0L6_2atmpS1917;
  #line 261 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1921 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS684
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS684)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS684->$0 = _M0L6_2atmpS1921;
  _M0L3resS684->$1 = 0;
  _M0L1iS685
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS685)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS685->$0 = 0;
  _M0L5startS686
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS686)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS686->$0 = 0;
  while (1) {
    int32_t _M0L3valS1904 = _M0L1iS685->$0;
    int32_t _M0L6_2atmpS1905 = Moonbit_array_length(_M0L1sS682);
    if (_M0L3valS1904 < _M0L6_2atmpS1905) {
      int32_t _M0L3valS1908 = _M0L1iS685->$0;
      int32_t _M0L6_2atmpS1907;
      int32_t _M0L6_2atmpS1906;
      int32_t _M0L3valS1915;
      int32_t _M0L6_2atmpS1914;
      if (
        _M0L3valS1908 < 0
        || _M0L3valS1908 >= Moonbit_array_length(_M0L1sS682)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1907 = _M0L1sS682[_M0L3valS1908];
      _M0L6_2atmpS1906 = _M0L6_2atmpS1907;
      if (_M0L6_2atmpS1906 == _M0L3sepS683) {
        int32_t _M0L3valS1910 = _M0L5startS686->$0;
        int32_t _M0L3valS1911 = _M0L1iS685->$0;
        moonbit_string_t _M0L6_2atmpS1909;
        int32_t _M0L3valS1913;
        int32_t _M0L6_2atmpS1912;
        moonbit_incref(_M0L1sS682);
        #line 270 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS1909
        = _M0MPC16string6String17unsafe__substring(_M0L1sS682, _M0L3valS1910, _M0L3valS1911);
        moonbit_incref(_M0L3resS684);
        #line 270 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS684, _M0L6_2atmpS1909);
        _M0L3valS1913 = _M0L1iS685->$0;
        _M0L6_2atmpS1912 = _M0L3valS1913 + 1;
        _M0L5startS686->$0 = _M0L6_2atmpS1912;
      }
      _M0L3valS1915 = _M0L1iS685->$0;
      _M0L6_2atmpS1914 = _M0L3valS1915 + 1;
      _M0L1iS685->$0 = _M0L6_2atmpS1914;
      continue;
    } else {
      moonbit_decref(_M0L1iS685);
    }
    break;
  }
  _M0L3valS1916 = _M0L5startS686->$0;
  _M0L6_2atmpS1917 = Moonbit_array_length(_M0L1sS682);
  if (_M0L3valS1916 < _M0L6_2atmpS1917) {
    int32_t _M0L8_2afieldS1932 = _M0L5startS686->$0;
    int32_t _M0L3valS1919;
    int32_t _M0L6_2atmpS1920;
    moonbit_string_t _M0L6_2atmpS1918;
    moonbit_decref(_M0L5startS686);
    _M0L3valS1919 = _M0L8_2afieldS1932;
    _M0L6_2atmpS1920 = Moonbit_array_length(_M0L1sS682);
    #line 276 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS1918
    = _M0MPC16string6String17unsafe__substring(_M0L1sS682, _M0L3valS1919, _M0L6_2atmpS1920);
    moonbit_incref(_M0L3resS684);
    #line 276 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS684, _M0L6_2atmpS1918);
  } else {
    moonbit_decref(_M0L5startS686);
    moonbit_decref(_M0L1sS682);
  }
  return _M0L3resS684;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS676(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669
) {
  moonbit_bytes_t* _M0L3tmpS677;
  int32_t _M0L6_2atmpS1902;
  struct _M0TPB5ArrayGsE* _M0L3resS678;
  int32_t _M0L1iS679;
  #line 250 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS677
  = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1902 = Moonbit_array_length(_M0L3tmpS677);
  #line 254 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L3resS678 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1902);
  _M0L1iS679 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1898 = Moonbit_array_length(_M0L3tmpS677);
    if (_M0L1iS679 < _M0L6_2atmpS1898) {
      moonbit_bytes_t _M0L6_2atmpS1933;
      moonbit_bytes_t _M0L6_2atmpS1900;
      moonbit_string_t _M0L6_2atmpS1899;
      int32_t _M0L6_2atmpS1901;
      if (_M0L1iS679 < 0 || _M0L1iS679 >= Moonbit_array_length(_M0L3tmpS677)) {
        #line 256 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1933 = (moonbit_bytes_t)_M0L3tmpS677[_M0L1iS679];
      _M0L6_2atmpS1900 = _M0L6_2atmpS1933;
      moonbit_incref(_M0L6_2atmpS1900);
      #line 256 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1899
      = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669, _M0L6_2atmpS1900);
      moonbit_incref(_M0L3resS678);
      #line 256 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS678, _M0L6_2atmpS1899);
      _M0L6_2atmpS1901 = _M0L1iS679 + 1;
      _M0L1iS679 = _M0L6_2atmpS1901;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS677);
    }
    break;
  }
  return _M0L3resS678;
}

moonbit_string_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS669(
  int32_t _M0L6_2aenvS1812,
  moonbit_bytes_t _M0L5bytesS670
) {
  struct _M0TPB13StringBuilder* _M0L3resS671;
  int32_t _M0L3lenS672;
  struct _M0TPC13ref3RefGiE* _M0L1iS673;
  #line 206 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L3resS671 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS672 = Moonbit_array_length(_M0L5bytesS670);
  _M0L1iS673
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS673)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS673->$0 = 0;
  while (1) {
    int32_t _M0L3valS1813 = _M0L1iS673->$0;
    if (_M0L3valS1813 < _M0L3lenS672) {
      int32_t _M0L3valS1897 = _M0L1iS673->$0;
      int32_t _M0L6_2atmpS1896;
      int32_t _M0L6_2atmpS1895;
      struct _M0TPC13ref3RefGiE* _M0L1cS674;
      int32_t _M0L3valS1814;
      if (
        _M0L3valS1897 < 0
        || _M0L3valS1897 >= Moonbit_array_length(_M0L5bytesS670)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1896 = _M0L5bytesS670[_M0L3valS1897];
      _M0L6_2atmpS1895 = (int32_t)_M0L6_2atmpS1896;
      _M0L1cS674
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS674)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS674->$0 = _M0L6_2atmpS1895;
      _M0L3valS1814 = _M0L1cS674->$0;
      if (_M0L3valS1814 < 128) {
        int32_t _M0L8_2afieldS1934 = _M0L1cS674->$0;
        int32_t _M0L3valS1816;
        int32_t _M0L6_2atmpS1815;
        int32_t _M0L3valS1818;
        int32_t _M0L6_2atmpS1817;
        moonbit_decref(_M0L1cS674);
        _M0L3valS1816 = _M0L8_2afieldS1934;
        _M0L6_2atmpS1815 = _M0L3valS1816;
        moonbit_incref(_M0L3resS671);
        #line 215 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS671, _M0L6_2atmpS1815);
        _M0L3valS1818 = _M0L1iS673->$0;
        _M0L6_2atmpS1817 = _M0L3valS1818 + 1;
        _M0L1iS673->$0 = _M0L6_2atmpS1817;
      } else {
        int32_t _M0L3valS1819 = _M0L1cS674->$0;
        if (_M0L3valS1819 < 224) {
          int32_t _M0L3valS1821 = _M0L1iS673->$0;
          int32_t _M0L6_2atmpS1820 = _M0L3valS1821 + 1;
          int32_t _M0L3valS1830;
          int32_t _M0L6_2atmpS1829;
          int32_t _M0L6_2atmpS1823;
          int32_t _M0L3valS1828;
          int32_t _M0L6_2atmpS1827;
          int32_t _M0L6_2atmpS1826;
          int32_t _M0L6_2atmpS1825;
          int32_t _M0L6_2atmpS1824;
          int32_t _M0L6_2atmpS1822;
          int32_t _M0L8_2afieldS1935;
          int32_t _M0L3valS1832;
          int32_t _M0L6_2atmpS1831;
          int32_t _M0L3valS1834;
          int32_t _M0L6_2atmpS1833;
          if (_M0L6_2atmpS1820 >= _M0L3lenS672) {
            moonbit_decref(_M0L1cS674);
            moonbit_decref(_M0L1iS673);
            moonbit_decref(_M0L5bytesS670);
            break;
          }
          _M0L3valS1830 = _M0L1cS674->$0;
          _M0L6_2atmpS1829 = _M0L3valS1830 & 31;
          _M0L6_2atmpS1823 = _M0L6_2atmpS1829 << 6;
          _M0L3valS1828 = _M0L1iS673->$0;
          _M0L6_2atmpS1827 = _M0L3valS1828 + 1;
          if (
            _M0L6_2atmpS1827 < 0
            || _M0L6_2atmpS1827 >= Moonbit_array_length(_M0L5bytesS670)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1826 = _M0L5bytesS670[_M0L6_2atmpS1827];
          _M0L6_2atmpS1825 = (int32_t)_M0L6_2atmpS1826;
          _M0L6_2atmpS1824 = _M0L6_2atmpS1825 & 63;
          _M0L6_2atmpS1822 = _M0L6_2atmpS1823 | _M0L6_2atmpS1824;
          _M0L1cS674->$0 = _M0L6_2atmpS1822;
          _M0L8_2afieldS1935 = _M0L1cS674->$0;
          moonbit_decref(_M0L1cS674);
          _M0L3valS1832 = _M0L8_2afieldS1935;
          _M0L6_2atmpS1831 = _M0L3valS1832;
          moonbit_incref(_M0L3resS671);
          #line 222 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS671, _M0L6_2atmpS1831);
          _M0L3valS1834 = _M0L1iS673->$0;
          _M0L6_2atmpS1833 = _M0L3valS1834 + 2;
          _M0L1iS673->$0 = _M0L6_2atmpS1833;
        } else {
          int32_t _M0L3valS1835 = _M0L1cS674->$0;
          if (_M0L3valS1835 < 240) {
            int32_t _M0L3valS1837 = _M0L1iS673->$0;
            int32_t _M0L6_2atmpS1836 = _M0L3valS1837 + 2;
            int32_t _M0L3valS1853;
            int32_t _M0L6_2atmpS1852;
            int32_t _M0L6_2atmpS1845;
            int32_t _M0L3valS1851;
            int32_t _M0L6_2atmpS1850;
            int32_t _M0L6_2atmpS1849;
            int32_t _M0L6_2atmpS1848;
            int32_t _M0L6_2atmpS1847;
            int32_t _M0L6_2atmpS1846;
            int32_t _M0L6_2atmpS1839;
            int32_t _M0L3valS1844;
            int32_t _M0L6_2atmpS1843;
            int32_t _M0L6_2atmpS1842;
            int32_t _M0L6_2atmpS1841;
            int32_t _M0L6_2atmpS1840;
            int32_t _M0L6_2atmpS1838;
            int32_t _M0L8_2afieldS1936;
            int32_t _M0L3valS1855;
            int32_t _M0L6_2atmpS1854;
            int32_t _M0L3valS1857;
            int32_t _M0L6_2atmpS1856;
            if (_M0L6_2atmpS1836 >= _M0L3lenS672) {
              moonbit_decref(_M0L1cS674);
              moonbit_decref(_M0L1iS673);
              moonbit_decref(_M0L5bytesS670);
              break;
            }
            _M0L3valS1853 = _M0L1cS674->$0;
            _M0L6_2atmpS1852 = _M0L3valS1853 & 15;
            _M0L6_2atmpS1845 = _M0L6_2atmpS1852 << 12;
            _M0L3valS1851 = _M0L1iS673->$0;
            _M0L6_2atmpS1850 = _M0L3valS1851 + 1;
            if (
              _M0L6_2atmpS1850 < 0
              || _M0L6_2atmpS1850 >= Moonbit_array_length(_M0L5bytesS670)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1849 = _M0L5bytesS670[_M0L6_2atmpS1850];
            _M0L6_2atmpS1848 = (int32_t)_M0L6_2atmpS1849;
            _M0L6_2atmpS1847 = _M0L6_2atmpS1848 & 63;
            _M0L6_2atmpS1846 = _M0L6_2atmpS1847 << 6;
            _M0L6_2atmpS1839 = _M0L6_2atmpS1845 | _M0L6_2atmpS1846;
            _M0L3valS1844 = _M0L1iS673->$0;
            _M0L6_2atmpS1843 = _M0L3valS1844 + 2;
            if (
              _M0L6_2atmpS1843 < 0
              || _M0L6_2atmpS1843 >= Moonbit_array_length(_M0L5bytesS670)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1842 = _M0L5bytesS670[_M0L6_2atmpS1843];
            _M0L6_2atmpS1841 = (int32_t)_M0L6_2atmpS1842;
            _M0L6_2atmpS1840 = _M0L6_2atmpS1841 & 63;
            _M0L6_2atmpS1838 = _M0L6_2atmpS1839 | _M0L6_2atmpS1840;
            _M0L1cS674->$0 = _M0L6_2atmpS1838;
            _M0L8_2afieldS1936 = _M0L1cS674->$0;
            moonbit_decref(_M0L1cS674);
            _M0L3valS1855 = _M0L8_2afieldS1936;
            _M0L6_2atmpS1854 = _M0L3valS1855;
            moonbit_incref(_M0L3resS671);
            #line 231 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS671, _M0L6_2atmpS1854);
            _M0L3valS1857 = _M0L1iS673->$0;
            _M0L6_2atmpS1856 = _M0L3valS1857 + 3;
            _M0L1iS673->$0 = _M0L6_2atmpS1856;
          } else {
            int32_t _M0L3valS1859 = _M0L1iS673->$0;
            int32_t _M0L6_2atmpS1858 = _M0L3valS1859 + 3;
            int32_t _M0L3valS1882;
            int32_t _M0L6_2atmpS1881;
            int32_t _M0L6_2atmpS1874;
            int32_t _M0L3valS1880;
            int32_t _M0L6_2atmpS1879;
            int32_t _M0L6_2atmpS1878;
            int32_t _M0L6_2atmpS1877;
            int32_t _M0L6_2atmpS1876;
            int32_t _M0L6_2atmpS1875;
            int32_t _M0L6_2atmpS1867;
            int32_t _M0L3valS1873;
            int32_t _M0L6_2atmpS1872;
            int32_t _M0L6_2atmpS1871;
            int32_t _M0L6_2atmpS1870;
            int32_t _M0L6_2atmpS1869;
            int32_t _M0L6_2atmpS1868;
            int32_t _M0L6_2atmpS1861;
            int32_t _M0L3valS1866;
            int32_t _M0L6_2atmpS1865;
            int32_t _M0L6_2atmpS1864;
            int32_t _M0L6_2atmpS1863;
            int32_t _M0L6_2atmpS1862;
            int32_t _M0L6_2atmpS1860;
            int32_t _M0L3valS1884;
            int32_t _M0L6_2atmpS1883;
            int32_t _M0L3valS1888;
            int32_t _M0L6_2atmpS1887;
            int32_t _M0L6_2atmpS1886;
            int32_t _M0L6_2atmpS1885;
            int32_t _M0L8_2afieldS1937;
            int32_t _M0L3valS1892;
            int32_t _M0L6_2atmpS1891;
            int32_t _M0L6_2atmpS1890;
            int32_t _M0L6_2atmpS1889;
            int32_t _M0L3valS1894;
            int32_t _M0L6_2atmpS1893;
            if (_M0L6_2atmpS1858 >= _M0L3lenS672) {
              moonbit_decref(_M0L1cS674);
              moonbit_decref(_M0L1iS673);
              moonbit_decref(_M0L5bytesS670);
              break;
            }
            _M0L3valS1882 = _M0L1cS674->$0;
            _M0L6_2atmpS1881 = _M0L3valS1882 & 7;
            _M0L6_2atmpS1874 = _M0L6_2atmpS1881 << 18;
            _M0L3valS1880 = _M0L1iS673->$0;
            _M0L6_2atmpS1879 = _M0L3valS1880 + 1;
            if (
              _M0L6_2atmpS1879 < 0
              || _M0L6_2atmpS1879 >= Moonbit_array_length(_M0L5bytesS670)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1878 = _M0L5bytesS670[_M0L6_2atmpS1879];
            _M0L6_2atmpS1877 = (int32_t)_M0L6_2atmpS1878;
            _M0L6_2atmpS1876 = _M0L6_2atmpS1877 & 63;
            _M0L6_2atmpS1875 = _M0L6_2atmpS1876 << 12;
            _M0L6_2atmpS1867 = _M0L6_2atmpS1874 | _M0L6_2atmpS1875;
            _M0L3valS1873 = _M0L1iS673->$0;
            _M0L6_2atmpS1872 = _M0L3valS1873 + 2;
            if (
              _M0L6_2atmpS1872 < 0
              || _M0L6_2atmpS1872 >= Moonbit_array_length(_M0L5bytesS670)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1871 = _M0L5bytesS670[_M0L6_2atmpS1872];
            _M0L6_2atmpS1870 = (int32_t)_M0L6_2atmpS1871;
            _M0L6_2atmpS1869 = _M0L6_2atmpS1870 & 63;
            _M0L6_2atmpS1868 = _M0L6_2atmpS1869 << 6;
            _M0L6_2atmpS1861 = _M0L6_2atmpS1867 | _M0L6_2atmpS1868;
            _M0L3valS1866 = _M0L1iS673->$0;
            _M0L6_2atmpS1865 = _M0L3valS1866 + 3;
            if (
              _M0L6_2atmpS1865 < 0
              || _M0L6_2atmpS1865 >= Moonbit_array_length(_M0L5bytesS670)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1864 = _M0L5bytesS670[_M0L6_2atmpS1865];
            _M0L6_2atmpS1863 = (int32_t)_M0L6_2atmpS1864;
            _M0L6_2atmpS1862 = _M0L6_2atmpS1863 & 63;
            _M0L6_2atmpS1860 = _M0L6_2atmpS1861 | _M0L6_2atmpS1862;
            _M0L1cS674->$0 = _M0L6_2atmpS1860;
            _M0L3valS1884 = _M0L1cS674->$0;
            _M0L6_2atmpS1883 = _M0L3valS1884 - 65536;
            _M0L1cS674->$0 = _M0L6_2atmpS1883;
            _M0L3valS1888 = _M0L1cS674->$0;
            _M0L6_2atmpS1887 = _M0L3valS1888 >> 10;
            _M0L6_2atmpS1886 = _M0L6_2atmpS1887 + 55296;
            _M0L6_2atmpS1885 = _M0L6_2atmpS1886;
            moonbit_incref(_M0L3resS671);
            #line 242 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS671, _M0L6_2atmpS1885);
            _M0L8_2afieldS1937 = _M0L1cS674->$0;
            moonbit_decref(_M0L1cS674);
            _M0L3valS1892 = _M0L8_2afieldS1937;
            _M0L6_2atmpS1891 = _M0L3valS1892 & 1023;
            _M0L6_2atmpS1890 = _M0L6_2atmpS1891 + 56320;
            _M0L6_2atmpS1889 = _M0L6_2atmpS1890;
            moonbit_incref(_M0L3resS671);
            #line 243 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS671, _M0L6_2atmpS1889);
            _M0L3valS1894 = _M0L1iS673->$0;
            _M0L6_2atmpS1893 = _M0L3valS1894 + 4;
            _M0L1iS673->$0 = _M0L6_2atmpS1893;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS673);
      moonbit_decref(_M0L5bytesS670);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS671);
}

int32_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S663(
  int32_t _M0L6_2aenvS1805,
  moonbit_string_t _M0L1sS664
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS665;
  int32_t _M0L3lenS666;
  int32_t _M0L1iS667;
  int32_t _M0L8_2afieldS1938;
  #line 197 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L3resS665
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS665)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS665->$0 = 0;
  _M0L3lenS666 = Moonbit_array_length(_M0L1sS664);
  _M0L1iS667 = 0;
  while (1) {
    if (_M0L1iS667 < _M0L3lenS666) {
      int32_t _M0L3valS1810 = _M0L3resS665->$0;
      int32_t _M0L6_2atmpS1807 = _M0L3valS1810 * 10;
      int32_t _M0L6_2atmpS1809;
      int32_t _M0L6_2atmpS1808;
      int32_t _M0L6_2atmpS1806;
      int32_t _M0L6_2atmpS1811;
      if (_M0L1iS667 < 0 || _M0L1iS667 >= Moonbit_array_length(_M0L1sS664)) {
        #line 201 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1809 = _M0L1sS664[_M0L1iS667];
      _M0L6_2atmpS1808 = _M0L6_2atmpS1809 - 48;
      _M0L6_2atmpS1806 = _M0L6_2atmpS1807 + _M0L6_2atmpS1808;
      _M0L3resS665->$0 = _M0L6_2atmpS1806;
      _M0L6_2atmpS1811 = _M0L1iS667 + 1;
      _M0L1iS667 = _M0L6_2atmpS1811;
      continue;
    } else {
      moonbit_decref(_M0L1sS664);
    }
    break;
  }
  _M0L8_2afieldS1938 = _M0L3resS665->$0;
  moonbit_decref(_M0L3resS665);
  return _M0L8_2afieldS1938;
}

int32_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS662,
  moonbit_string_t _M0L8filenameS637,
  int32_t _M0L5indexS640
) {
  struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632* _closure_2268;
  struct _M0TWssbEu* _M0L14handle__resultS632;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS641;
  void* _M0L11_2atry__errS656;
  struct moonbit_result_0 _tmp_2270;
  int32_t _handle__error__result_2271;
  int32_t _M0L6_2atmpS1793;
  void* _M0L3errS657;
  moonbit_string_t _M0L4nameS659;
  struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS660;
  moonbit_string_t _M0L8_2afieldS1939;
  int32_t _M0L6_2acntS2219;
  moonbit_string_t _M0L7_2anameS661;
  #line 483 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS637);
  _closure_2268
  = (struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632*)moonbit_malloc(sizeof(struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632));
  Moonbit_object_header(_closure_2268)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632, $1) >> 2, 1, 0);
  _closure_2268->code
  = &_M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN14handle__resultS632;
  _closure_2268->$0 = _M0L5indexS640;
  _closure_2268->$1 = _M0L8filenameS637;
  _M0L14handle__resultS632 = (struct _M0TWssbEu*)_closure_2268;
  _M0L17error__to__stringS641
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN17error__to__stringS641$closure.data;
  moonbit_incref(_M0L12async__testsS662);
  moonbit_incref(_M0L17error__to__stringS641);
  moonbit_incref(_M0L8filenameS637);
  moonbit_incref(_M0L14handle__resultS632);
  #line 517 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _tmp_2270
  = _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled41MoonBit__Test__Driver__Internal__No__ArgsE(_M0L12async__testsS662, _M0L8filenameS637, _M0L5indexS640, _M0L14handle__resultS632, _M0L17error__to__stringS641);
  if (_tmp_2270.tag) {
    int32_t const _M0L5_2aokS1802 = _tmp_2270.data.ok;
    _handle__error__result_2271 = _M0L5_2aokS1802;
  } else {
    void* const _M0L6_2aerrS1803 = _tmp_2270.data.err;
    moonbit_decref(_M0L12async__testsS662);
    moonbit_decref(_M0L17error__to__stringS641);
    moonbit_decref(_M0L8filenameS637);
    _M0L11_2atry__errS656 = _M0L6_2aerrS1803;
    goto join_655;
  }
  if (_handle__error__result_2271) {
    moonbit_decref(_M0L12async__testsS662);
    moonbit_decref(_M0L17error__to__stringS641);
    moonbit_decref(_M0L8filenameS637);
    _M0L6_2atmpS1793 = 1;
  } else {
    struct moonbit_result_0 _tmp_2272;
    int32_t _handle__error__result_2273;
    moonbit_incref(_M0L12async__testsS662);
    moonbit_incref(_M0L17error__to__stringS641);
    moonbit_incref(_M0L8filenameS637);
    moonbit_incref(_M0L14handle__resultS632);
    #line 520 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
    _tmp_2272
    = _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS662, _M0L8filenameS637, _M0L5indexS640, _M0L14handle__resultS632, _M0L17error__to__stringS641);
    if (_tmp_2272.tag) {
      int32_t const _M0L5_2aokS1800 = _tmp_2272.data.ok;
      _handle__error__result_2273 = _M0L5_2aokS1800;
    } else {
      void* const _M0L6_2aerrS1801 = _tmp_2272.data.err;
      moonbit_decref(_M0L12async__testsS662);
      moonbit_decref(_M0L17error__to__stringS641);
      moonbit_decref(_M0L8filenameS637);
      _M0L11_2atry__errS656 = _M0L6_2aerrS1801;
      goto join_655;
    }
    if (_handle__error__result_2273) {
      moonbit_decref(_M0L12async__testsS662);
      moonbit_decref(_M0L17error__to__stringS641);
      moonbit_decref(_M0L8filenameS637);
      _M0L6_2atmpS1793 = 1;
    } else {
      struct moonbit_result_0 _tmp_2274;
      int32_t _handle__error__result_2275;
      moonbit_incref(_M0L12async__testsS662);
      moonbit_incref(_M0L17error__to__stringS641);
      moonbit_incref(_M0L8filenameS637);
      moonbit_incref(_M0L14handle__resultS632);
      #line 523 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _tmp_2274
      = _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS662, _M0L8filenameS637, _M0L5indexS640, _M0L14handle__resultS632, _M0L17error__to__stringS641);
      if (_tmp_2274.tag) {
        int32_t const _M0L5_2aokS1798 = _tmp_2274.data.ok;
        _handle__error__result_2275 = _M0L5_2aokS1798;
      } else {
        void* const _M0L6_2aerrS1799 = _tmp_2274.data.err;
        moonbit_decref(_M0L12async__testsS662);
        moonbit_decref(_M0L17error__to__stringS641);
        moonbit_decref(_M0L8filenameS637);
        _M0L11_2atry__errS656 = _M0L6_2aerrS1799;
        goto join_655;
      }
      if (_handle__error__result_2275) {
        moonbit_decref(_M0L12async__testsS662);
        moonbit_decref(_M0L17error__to__stringS641);
        moonbit_decref(_M0L8filenameS637);
        _M0L6_2atmpS1793 = 1;
      } else {
        struct moonbit_result_0 _tmp_2276;
        int32_t _handle__error__result_2277;
        moonbit_incref(_M0L12async__testsS662);
        moonbit_incref(_M0L17error__to__stringS641);
        moonbit_incref(_M0L8filenameS637);
        moonbit_incref(_M0L14handle__resultS632);
        #line 526 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        _tmp_2276
        = _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS662, _M0L8filenameS637, _M0L5indexS640, _M0L14handle__resultS632, _M0L17error__to__stringS641);
        if (_tmp_2276.tag) {
          int32_t const _M0L5_2aokS1796 = _tmp_2276.data.ok;
          _handle__error__result_2277 = _M0L5_2aokS1796;
        } else {
          void* const _M0L6_2aerrS1797 = _tmp_2276.data.err;
          moonbit_decref(_M0L12async__testsS662);
          moonbit_decref(_M0L17error__to__stringS641);
          moonbit_decref(_M0L8filenameS637);
          _M0L11_2atry__errS656 = _M0L6_2aerrS1797;
          goto join_655;
        }
        if (_handle__error__result_2277) {
          moonbit_decref(_M0L12async__testsS662);
          moonbit_decref(_M0L17error__to__stringS641);
          moonbit_decref(_M0L8filenameS637);
          _M0L6_2atmpS1793 = 1;
        } else {
          struct moonbit_result_0 _tmp_2278;
          moonbit_incref(_M0L14handle__resultS632);
          #line 529 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
          _tmp_2278
          = _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS662, _M0L8filenameS637, _M0L5indexS640, _M0L14handle__resultS632, _M0L17error__to__stringS641);
          if (_tmp_2278.tag) {
            int32_t const _M0L5_2aokS1794 = _tmp_2278.data.ok;
            _M0L6_2atmpS1793 = _M0L5_2aokS1794;
          } else {
            void* const _M0L6_2aerrS1795 = _tmp_2278.data.err;
            _M0L11_2atry__errS656 = _M0L6_2aerrS1795;
            goto join_655;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1793) {
    void* _M0L123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1804 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1804)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1804)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS656
    = _M0L123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1804;
    goto join_655;
  } else {
    moonbit_decref(_M0L14handle__resultS632);
  }
  goto joinlet_2269;
  join_655:;
  _M0L3errS657 = _M0L11_2atry__errS656;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS660
  = (struct _M0DTPC15error5Error123clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS657;
  _M0L8_2afieldS1939 = _M0L36_2aMoonBitTestDriverInternalSkipTestS660->$0;
  _M0L6_2acntS2219
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS660)->rc;
  if (_M0L6_2acntS2219 > 1) {
    int32_t _M0L11_2anew__cntS2220 = _M0L6_2acntS2219 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS660)->rc
    = _M0L11_2anew__cntS2220;
    moonbit_incref(_M0L8_2afieldS1939);
  } else if (_M0L6_2acntS2219 == 1) {
    #line 536 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS660);
  }
  _M0L7_2anameS661 = _M0L8_2afieldS1939;
  _M0L4nameS659 = _M0L7_2anameS661;
  goto join_658;
  goto joinlet_2279;
  join_658:;
  #line 537 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN14handle__resultS632(_M0L14handle__resultS632, _M0L4nameS659, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2279:;
  joinlet_2269:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN17error__to__stringS641(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1792,
  void* _M0L3errS642
) {
  void* _M0L1eS644;
  moonbit_string_t _M0L1eS646;
  #line 506 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS1792);
  switch (Moonbit_object_tag(_M0L3errS642)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS647 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS642;
      moonbit_string_t _M0L8_2afieldS1940 = _M0L10_2aFailureS647->$0;
      int32_t _M0L6_2acntS2221 =
        Moonbit_object_header(_M0L10_2aFailureS647)->rc;
      moonbit_string_t _M0L4_2aeS648;
      if (_M0L6_2acntS2221 > 1) {
        int32_t _M0L11_2anew__cntS2222 = _M0L6_2acntS2221 - 1;
        Moonbit_object_header(_M0L10_2aFailureS647)->rc
        = _M0L11_2anew__cntS2222;
        moonbit_incref(_M0L8_2afieldS1940);
      } else if (_M0L6_2acntS2221 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS647);
      }
      _M0L4_2aeS648 = _M0L8_2afieldS1940;
      _M0L1eS646 = _M0L4_2aeS648;
      goto join_645;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS649 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS642;
      moonbit_string_t _M0L8_2afieldS1941 = _M0L15_2aInspectErrorS649->$0;
      int32_t _M0L6_2acntS2223 =
        Moonbit_object_header(_M0L15_2aInspectErrorS649)->rc;
      moonbit_string_t _M0L4_2aeS650;
      if (_M0L6_2acntS2223 > 1) {
        int32_t _M0L11_2anew__cntS2224 = _M0L6_2acntS2223 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS649)->rc
        = _M0L11_2anew__cntS2224;
        moonbit_incref(_M0L8_2afieldS1941);
      } else if (_M0L6_2acntS2223 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS649);
      }
      _M0L4_2aeS650 = _M0L8_2afieldS1941;
      _M0L1eS646 = _M0L4_2aeS650;
      goto join_645;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS651 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS642;
      moonbit_string_t _M0L8_2afieldS1942 = _M0L16_2aSnapshotErrorS651->$0;
      int32_t _M0L6_2acntS2225 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS651)->rc;
      moonbit_string_t _M0L4_2aeS652;
      if (_M0L6_2acntS2225 > 1) {
        int32_t _M0L11_2anew__cntS2226 = _M0L6_2acntS2225 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS651)->rc
        = _M0L11_2anew__cntS2226;
        moonbit_incref(_M0L8_2afieldS1942);
      } else if (_M0L6_2acntS2225 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS651);
      }
      _M0L4_2aeS652 = _M0L8_2afieldS1942;
      _M0L1eS646 = _M0L4_2aeS652;
      goto join_645;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error121clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS653 =
        (struct _M0DTPC15error5Error121clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS642;
      moonbit_string_t _M0L8_2afieldS1943 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS653->$0;
      int32_t _M0L6_2acntS2227 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS653)->rc;
      moonbit_string_t _M0L4_2aeS654;
      if (_M0L6_2acntS2227 > 1) {
        int32_t _M0L11_2anew__cntS2228 = _M0L6_2acntS2227 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS653)->rc
        = _M0L11_2anew__cntS2228;
        moonbit_incref(_M0L8_2afieldS1943);
      } else if (_M0L6_2acntS2227 == 1) {
        #line 507 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS653);
      }
      _M0L4_2aeS654 = _M0L8_2afieldS1943;
      _M0L1eS646 = _M0L4_2aeS654;
      goto join_645;
      break;
    }
    default: {
      _M0L1eS644 = _M0L3errS642;
      goto join_643;
      break;
    }
  }
  join_645:;
  return _M0L1eS646;
  join_643:;
  #line 512 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS644);
}

int32_t _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__executeN14handle__resultS632(
  struct _M0TWssbEu* _M0L6_2aenvS1778,
  moonbit_string_t _M0L8testnameS633,
  moonbit_string_t _M0L7messageS634,
  int32_t _M0L7skippedS635
) {
  struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632* _M0L14_2acasted__envS1779;
  moonbit_string_t _M0L8_2afieldS1953;
  moonbit_string_t _M0L8filenameS637;
  int32_t _M0L8_2afieldS1952;
  int32_t _M0L6_2acntS2229;
  int32_t _M0L5indexS640;
  int32_t _if__result_2282;
  moonbit_string_t _M0L10file__nameS636;
  moonbit_string_t _M0L10test__nameS638;
  moonbit_string_t _M0L7messageS639;
  moonbit_string_t _M0L6_2atmpS1791;
  moonbit_string_t _M0L6_2atmpS1951;
  moonbit_string_t _M0L6_2atmpS1790;
  moonbit_string_t _M0L6_2atmpS1950;
  moonbit_string_t _M0L6_2atmpS1788;
  moonbit_string_t _M0L6_2atmpS1789;
  moonbit_string_t _M0L6_2atmpS1949;
  moonbit_string_t _M0L6_2atmpS1787;
  moonbit_string_t _M0L6_2atmpS1948;
  moonbit_string_t _M0L6_2atmpS1785;
  moonbit_string_t _M0L6_2atmpS1786;
  moonbit_string_t _M0L6_2atmpS1947;
  moonbit_string_t _M0L6_2atmpS1784;
  moonbit_string_t _M0L6_2atmpS1946;
  moonbit_string_t _M0L6_2atmpS1782;
  moonbit_string_t _M0L6_2atmpS1783;
  moonbit_string_t _M0L6_2atmpS1945;
  moonbit_string_t _M0L6_2atmpS1781;
  moonbit_string_t _M0L6_2atmpS1944;
  moonbit_string_t _M0L6_2atmpS1780;
  #line 490 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1779
  = (struct _M0R124_24clawteam_2fclawteam_2fcmd_2ftest_2dto_2dbe_2dkilled_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c632*)_M0L6_2aenvS1778;
  _M0L8_2afieldS1953 = _M0L14_2acasted__envS1779->$1;
  _M0L8filenameS637 = _M0L8_2afieldS1953;
  _M0L8_2afieldS1952 = _M0L14_2acasted__envS1779->$0;
  _M0L6_2acntS2229 = Moonbit_object_header(_M0L14_2acasted__envS1779)->rc;
  if (_M0L6_2acntS2229 > 1) {
    int32_t _M0L11_2anew__cntS2230 = _M0L6_2acntS2229 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1779)->rc
    = _M0L11_2anew__cntS2230;
    moonbit_incref(_M0L8filenameS637);
  } else if (_M0L6_2acntS2229 == 1) {
    #line 490 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1779);
  }
  _M0L5indexS640 = _M0L8_2afieldS1952;
  if (!_M0L7skippedS635) {
    _if__result_2282 = 1;
  } else {
    _if__result_2282 = 0;
  }
  if (_if__result_2282) {
    
  }
  #line 496 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS636 = _M0MPC16string6String6escape(_M0L8filenameS637);
  #line 497 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS638 = _M0MPC16string6String6escape(_M0L8testnameS633);
  #line 498 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS639 = _M0MPC16string6String6escape(_M0L7messageS634);
  #line 499 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 501 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1791
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS636);
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1951
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1791);
  moonbit_decref(_M0L6_2atmpS1791);
  _M0L6_2atmpS1790 = _M0L6_2atmpS1951;
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1950
  = moonbit_add_string(_M0L6_2atmpS1790, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1790);
  _M0L6_2atmpS1788 = _M0L6_2atmpS1950;
  #line 501 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1789
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS640);
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1949 = moonbit_add_string(_M0L6_2atmpS1788, _M0L6_2atmpS1789);
  moonbit_decref(_M0L6_2atmpS1788);
  moonbit_decref(_M0L6_2atmpS1789);
  _M0L6_2atmpS1787 = _M0L6_2atmpS1949;
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1948
  = moonbit_add_string(_M0L6_2atmpS1787, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1787);
  _M0L6_2atmpS1785 = _M0L6_2atmpS1948;
  #line 501 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1786
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS638);
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1947 = moonbit_add_string(_M0L6_2atmpS1785, _M0L6_2atmpS1786);
  moonbit_decref(_M0L6_2atmpS1785);
  moonbit_decref(_M0L6_2atmpS1786);
  _M0L6_2atmpS1784 = _M0L6_2atmpS1947;
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1946
  = moonbit_add_string(_M0L6_2atmpS1784, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1784);
  _M0L6_2atmpS1782 = _M0L6_2atmpS1946;
  #line 501 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1783
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS639);
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1945 = moonbit_add_string(_M0L6_2atmpS1782, _M0L6_2atmpS1783);
  moonbit_decref(_M0L6_2atmpS1782);
  moonbit_decref(_M0L6_2atmpS1783);
  _M0L6_2atmpS1781 = _M0L6_2atmpS1945;
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1944
  = moonbit_add_string(_M0L6_2atmpS1781, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1781);
  _M0L6_2atmpS1780 = _M0L6_2atmpS1944;
  #line 500 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1780);
  #line 503 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled41MoonBit__Test__Driver__Internal__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S607,
  moonbit_string_t _M0L12_2adiscard__S608,
  int32_t _M0L12_2adiscard__S609,
  struct _M0TWssbEu* _M0L12_2adiscard__S610,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S611
) {
  struct moonbit_result_0 _result_2283;
  #line 34 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S611);
  moonbit_decref(_M0L12_2adiscard__S610);
  moonbit_decref(_M0L12_2adiscard__S608);
  moonbit_decref(_M0L12_2adiscard__S607);
  _result_2283.tag = 1;
  _result_2283.data.ok = 0;
  return _result_2283;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S612,
  moonbit_string_t _M0L12_2adiscard__S613,
  int32_t _M0L12_2adiscard__S614,
  struct _M0TWssbEu* _M0L12_2adiscard__S615,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S616
) {
  struct moonbit_result_0 _result_2284;
  #line 34 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S616);
  moonbit_decref(_M0L12_2adiscard__S615);
  moonbit_decref(_M0L12_2adiscard__S613);
  moonbit_decref(_M0L12_2adiscard__S612);
  _result_2284.tag = 1;
  _result_2284.data.ok = 0;
  return _result_2284;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S617,
  moonbit_string_t _M0L12_2adiscard__S618,
  int32_t _M0L12_2adiscard__S619,
  struct _M0TWssbEu* _M0L12_2adiscard__S620,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S621
) {
  struct moonbit_result_0 _result_2285;
  #line 34 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S621);
  moonbit_decref(_M0L12_2adiscard__S620);
  moonbit_decref(_M0L12_2adiscard__S618);
  moonbit_decref(_M0L12_2adiscard__S617);
  _result_2285.tag = 1;
  _result_2285.data.ok = 0;
  return _result_2285;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S622,
  moonbit_string_t _M0L12_2adiscard__S623,
  int32_t _M0L12_2adiscard__S624,
  struct _M0TWssbEu* _M0L12_2adiscard__S625,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S626
) {
  struct moonbit_result_0 _result_2286;
  #line 34 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S626);
  moonbit_decref(_M0L12_2adiscard__S625);
  moonbit_decref(_M0L12_2adiscard__S623);
  moonbit_decref(_M0L12_2adiscard__S622);
  _result_2286.tag = 1;
  _result_2286.data.ok = 0;
  return _result_2286;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S627,
  moonbit_string_t _M0L12_2adiscard__S628,
  int32_t _M0L12_2adiscard__S629,
  struct _M0TWssbEu* _M0L12_2adiscard__S630,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S631
) {
  struct moonbit_result_0 _result_2287;
  #line 34 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S631);
  moonbit_decref(_M0L12_2adiscard__S630);
  moonbit_decref(_M0L12_2adiscard__S628);
  moonbit_decref(_M0L12_2adiscard__S627);
  _result_2287.tag = 1;
  _result_2287.data.ok = 0;
  return _result_2287;
}

int32_t _M0FP48clawteam8clawteam8internal2os6atexit(
  void(* _M0L8_2aparamS719)()
) {
  atexit(_M0L8_2aparamS719);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal2os4args() {
  #line 66 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  #line 67 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  return _M0FPC13env4args();
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal2os6getenv(
  struct _M0TPC16string10StringView _M0L3keyS592
) {
  moonbit_bytes_t _M0L6_2atmpS1777;
  void* _M0L6_2atmpS1954;
  void* _M0L6c__strS591;
  uint64_t _M0L6_2atmpS1776;
  int32_t _M0L3lenS593;
  moonbit_bytes_t _M0L3bufS594;
  int32_t _M0L1iS595;
  moonbit_bytes_t _M0L7_2abindS597;
  int32_t _M0L6_2atmpS1773;
  int64_t _M0L6_2atmpS1772;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1771;
  struct moonbit_result_2 _tmp_2290;
  moonbit_string_t _M0L6_2atmpS1770;
  moonbit_string_t _M0L6_2atmpS1769;
  struct moonbit_result_1 _result_2292;
  #line 16 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1777 = _M0FPC28encoding4utf814encode_2einner(_M0L3keyS592, 0);
  #line 17 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1954
  = _M0FP48clawteam8clawteam8internal2os10os__getenv(_M0L6_2atmpS1777);
  moonbit_decref(_M0L6_2atmpS1777);
  _M0L6c__strS591 = _M0L6_2atmpS1954;
  #line 18 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  if (
    _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(_M0L6c__strS591)
  ) {
    moonbit_string_t _M0L6_2atmpS1766 = 0;
    struct moonbit_result_1 _result_2288;
    _result_2288.tag = 1;
    _result_2288.data.ok = _M0L6_2atmpS1766;
    return _result_2288;
  }
  #line 21 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1776
  = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L6c__strS591);
  _M0L3lenS593 = (int32_t)_M0L6_2atmpS1776;
  _M0L3bufS594 = (moonbit_bytes_t)moonbit_make_bytes(_M0L3lenS593, 0);
  _M0L1iS595 = 0;
  while (1) {
    if (_M0L1iS595 < _M0L3lenS593) {
      int32_t _M0L6_2atmpS1767;
      int32_t _M0L6_2atmpS1768;
      #line 24 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
      _M0L6_2atmpS1767
      = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L6c__strS591, _M0L1iS595);
      if (_M0L1iS595 < 0 || _M0L1iS595 >= Moonbit_array_length(_M0L3bufS594)) {
        #line 24 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
        moonbit_panic();
      }
      _M0L3bufS594[_M0L1iS595] = _M0L6_2atmpS1767;
      _M0L6_2atmpS1768 = _M0L1iS595 + 1;
      _M0L1iS595 = _M0L6_2atmpS1768;
      continue;
    }
    break;
  }
  _M0L7_2abindS597 = _M0L3bufS594;
  _M0L6_2atmpS1773 = Moonbit_array_length(_M0L7_2abindS597);
  _M0L6_2atmpS1772 = (int64_t)_M0L6_2atmpS1773;
  #line 26 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _M0L6_2atmpS1771
  = _M0MPC15bytes5Bytes12view_2einner(_M0L7_2abindS597, 0, _M0L6_2atmpS1772);
  #line 26 "E:\\moonbit\\clawteam\\internal\\os\\os.mbt"
  _tmp_2290 = _M0FPC28encoding4utf814decode_2einner(_M0L6_2atmpS1771, 0);
  if (_tmp_2290.tag) {
    moonbit_string_t const _M0L5_2aokS1774 = _tmp_2290.data.ok;
    _M0L6_2atmpS1770 = _M0L5_2aokS1774;
  } else {
    void* const _M0L6_2aerrS1775 = _tmp_2290.data.err;
    struct moonbit_result_1 _result_2291;
    _result_2291.tag = 0;
    _result_2291.data.err = _M0L6_2aerrS1775;
    return _result_2291;
  }
  _M0L6_2atmpS1769 = _M0L6_2atmpS1770;
  _result_2292.tag = 1;
  _result_2292.data.ok = _M0L6_2atmpS1769;
  return _result_2292;
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS589,
  int32_t _M0L6offsetS590
) {
  #line 145 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte(_M0L7pointerS589, _M0L6offsetS590);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void* _M0L4selfS587,
  int32_t _M0L5indexS588
) {
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS587, _M0L5indexS588);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(
  void* _M0L4selfS586
) {
  void* _M0L6_2atmpS1765;
  #line 24 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0L6_2atmpS1765 = _M0L4selfS586;
  #line 25 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0MP48clawteam8clawteam8internal1c7Pointer10__is__null(_M0L6_2atmpS1765);
}

struct moonbit_result_2 _M0FPC28encoding4utf814decode_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS475,
  int32_t _M0L11ignore__bomS476
) {
  struct _M0TPC15bytes9BytesView _M0L5bytesS473;
  int32_t _M0L6_2atmpS1749;
  int32_t _M0L6_2atmpS1748;
  moonbit_bytes_t _M0L1tS481;
  int32_t _M0L4tlenS482;
  int32_t _M0L11_2aparam__0S483;
  struct _M0TPC15bytes9BytesView _M0L11_2aparam__1S484;
  moonbit_bytes_t _M0L6_2atmpS1400;
  int64_t _M0L6_2atmpS1401;
  moonbit_string_t _M0L6_2atmpS1399;
  struct moonbit_result_2 _result_2302;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  if (_M0L11ignore__bomS476) {
    int32_t _M0L3endS1751 = _M0L5bytesS475.$2;
    int32_t _M0L5startS1752 = _M0L5bytesS475.$1;
    int32_t _M0L6_2atmpS1750 = _M0L3endS1751 - _M0L5startS1752;
    if (_M0L6_2atmpS1750 >= 3) {
      moonbit_bytes_t _M0L8_2afieldS2102 = _M0L5bytesS475.$0;
      moonbit_bytes_t _M0L5bytesS1763 = _M0L8_2afieldS2102;
      int32_t _M0L5startS1764 = _M0L5bytesS475.$1;
      int32_t _M0L6_2atmpS2101 = _M0L5bytesS1763[_M0L5startS1764];
      int32_t _M0L4_2axS478 = _M0L6_2atmpS2101;
      if (_M0L4_2axS478 == 239) {
        moonbit_bytes_t _M0L8_2afieldS2100 = _M0L5bytesS475.$0;
        moonbit_bytes_t _M0L5bytesS1760 = _M0L8_2afieldS2100;
        int32_t _M0L5startS1762 = _M0L5bytesS475.$1;
        int32_t _M0L6_2atmpS1761 = _M0L5startS1762 + 1;
        int32_t _M0L6_2atmpS2099 = _M0L5bytesS1760[_M0L6_2atmpS1761];
        int32_t _M0L4_2axS479 = _M0L6_2atmpS2099;
        if (_M0L4_2axS479 == 187) {
          moonbit_bytes_t _M0L8_2afieldS2098 = _M0L5bytesS475.$0;
          moonbit_bytes_t _M0L5bytesS1757 = _M0L8_2afieldS2098;
          int32_t _M0L5startS1759 = _M0L5bytesS475.$1;
          int32_t _M0L6_2atmpS1758 = _M0L5startS1759 + 2;
          int32_t _M0L6_2atmpS2097 = _M0L5bytesS1757[_M0L6_2atmpS1758];
          int32_t _M0L4_2axS480 = _M0L6_2atmpS2097;
          if (_M0L4_2axS480 == 191) {
            moonbit_bytes_t _M0L8_2afieldS2096 = _M0L5bytesS475.$0;
            moonbit_bytes_t _M0L5bytesS1753 = _M0L8_2afieldS2096;
            int32_t _M0L5startS1756 = _M0L5bytesS475.$1;
            int32_t _M0L6_2atmpS1754 = _M0L5startS1756 + 3;
            int32_t _M0L8_2afieldS2095 = _M0L5bytesS475.$2;
            int32_t _M0L3endS1755 = _M0L8_2afieldS2095;
            _M0L5bytesS473
            = (struct _M0TPC15bytes9BytesView){
              _M0L6_2atmpS1754, _M0L3endS1755, _M0L5bytesS1753
            };
          } else {
            goto join_477;
          }
        } else {
          goto join_477;
        }
      } else {
        goto join_477;
      }
    } else {
      goto join_477;
    }
    goto joinlet_2294;
    join_477:;
    goto join_474;
    joinlet_2294:;
  } else {
    goto join_474;
  }
  goto joinlet_2293;
  join_474:;
  _M0L5bytesS473 = _M0L5bytesS475;
  joinlet_2293:;
  moonbit_incref(_M0L5bytesS473.$0);
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS1749 = _M0MPC15bytes9BytesView6length(_M0L5bytesS473);
  _M0L6_2atmpS1748 = _M0L6_2atmpS1749 * 2;
  _M0L1tS481 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1748, 0);
  _M0L11_2aparam__0S483 = 0;
  _M0L11_2aparam__1S484 = _M0L5bytesS473;
  while (1) {
    struct _M0TPC15bytes9BytesView _M0L5bytesS486;
    int32_t _M0L4tlenS488;
    int32_t _M0L2b0S489;
    int32_t _M0L2b1S490;
    int32_t _M0L2b2S491;
    int32_t _M0L2b3S492;
    struct _M0TPC15bytes9BytesView _M0L4restS493;
    int32_t _M0L4tlenS500;
    int32_t _M0L2b0S501;
    int32_t _M0L2b1S502;
    int32_t _M0L2b2S503;
    struct _M0TPC15bytes9BytesView _M0L4restS504;
    int32_t _M0L4tlenS507;
    struct _M0TPC15bytes9BytesView _M0L4restS508;
    int32_t _M0L2b0S509;
    int32_t _M0L2b1S510;
    int32_t _M0L4tlenS513;
    struct _M0TPC15bytes9BytesView _M0L4restS514;
    int32_t _M0L1bS515;
    int32_t _M0L3endS1454 = _M0L11_2aparam__1S484.$2;
    int32_t _M0L5startS1455 = _M0L11_2aparam__1S484.$1;
    int32_t _M0L6_2atmpS1453 = _M0L3endS1454 - _M0L5startS1455;
    int32_t _M0L6_2atmpS1452;
    int32_t _M0L6_2atmpS1451;
    int32_t _M0L6_2atmpS1450;
    int32_t _M0L6_2atmpS1447;
    int32_t _M0L6_2atmpS1449;
    int32_t _M0L6_2atmpS1448;
    int32_t _M0L2chS511;
    int32_t _M0L6_2atmpS1442;
    int32_t _M0L6_2atmpS1443;
    int32_t _M0L6_2atmpS1445;
    int32_t _M0L6_2atmpS1444;
    int32_t _M0L6_2atmpS1446;
    int32_t _M0L6_2atmpS1441;
    int32_t _M0L6_2atmpS1440;
    int32_t _M0L6_2atmpS1436;
    int32_t _M0L6_2atmpS1439;
    int32_t _M0L6_2atmpS1438;
    int32_t _M0L6_2atmpS1437;
    int32_t _M0L6_2atmpS1433;
    int32_t _M0L6_2atmpS1435;
    int32_t _M0L6_2atmpS1434;
    int32_t _M0L2chS505;
    int32_t _M0L6_2atmpS1428;
    int32_t _M0L6_2atmpS1429;
    int32_t _M0L6_2atmpS1431;
    int32_t _M0L6_2atmpS1430;
    int32_t _M0L6_2atmpS1432;
    int32_t _M0L6_2atmpS1427;
    int32_t _M0L6_2atmpS1426;
    int32_t _M0L6_2atmpS1422;
    int32_t _M0L6_2atmpS1425;
    int32_t _M0L6_2atmpS1424;
    int32_t _M0L6_2atmpS1423;
    int32_t _M0L6_2atmpS1418;
    int32_t _M0L6_2atmpS1421;
    int32_t _M0L6_2atmpS1420;
    int32_t _M0L6_2atmpS1419;
    int32_t _M0L6_2atmpS1415;
    int32_t _M0L6_2atmpS1417;
    int32_t _M0L6_2atmpS1416;
    int32_t _M0L2chS494;
    int32_t _M0L3chmS495;
    int32_t _M0L6_2atmpS1414;
    int32_t _M0L3ch1S496;
    int32_t _M0L6_2atmpS1413;
    int32_t _M0L3ch2S497;
    int32_t _M0L6_2atmpS1403;
    int32_t _M0L6_2atmpS1404;
    int32_t _M0L6_2atmpS1406;
    int32_t _M0L6_2atmpS1405;
    int32_t _M0L6_2atmpS1407;
    int32_t _M0L6_2atmpS1408;
    int32_t _M0L6_2atmpS1409;
    int32_t _M0L6_2atmpS1411;
    int32_t _M0L6_2atmpS1410;
    int32_t _M0L6_2atmpS1412;
    void* _M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402;
    struct moonbit_result_2 _result_2301;
    if (_M0L6_2atmpS1453 == 0) {
      moonbit_decref(_M0L11_2aparam__1S484.$0);
      _M0L4tlenS482 = _M0L11_2aparam__0S483;
    } else {
      int32_t _M0L3endS1457 = _M0L11_2aparam__1S484.$2;
      int32_t _M0L5startS1458 = _M0L11_2aparam__1S484.$1;
      int32_t _M0L6_2atmpS1456 = _M0L3endS1457 - _M0L5startS1458;
      if (_M0L6_2atmpS1456 >= 8) {
        moonbit_bytes_t _M0L8_2afieldS2038 = _M0L11_2aparam__1S484.$0;
        moonbit_bytes_t _M0L5bytesS1606 = _M0L8_2afieldS2038;
        int32_t _M0L5startS1607 = _M0L11_2aparam__1S484.$1;
        int32_t _M0L6_2atmpS2037 = _M0L5bytesS1606[_M0L5startS1607];
        int32_t _M0L4_2axS516 = _M0L6_2atmpS2037;
        if (_M0L4_2axS516 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS1984 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1603 = _M0L8_2afieldS1984;
          int32_t _M0L5startS1605 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1604 = _M0L5startS1605 + 1;
          int32_t _M0L6_2atmpS1983 = _M0L5bytesS1603[_M0L6_2atmpS1604];
          int32_t _M0L4_2axS517 = _M0L6_2atmpS1983;
          if (_M0L4_2axS517 <= 127) {
            moonbit_bytes_t _M0L8_2afieldS1980 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1600 = _M0L8_2afieldS1980;
            int32_t _M0L5startS1602 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1601 = _M0L5startS1602 + 2;
            int32_t _M0L6_2atmpS1979 = _M0L5bytesS1600[_M0L6_2atmpS1601];
            int32_t _M0L4_2axS518 = _M0L6_2atmpS1979;
            if (_M0L4_2axS518 <= 127) {
              moonbit_bytes_t _M0L8_2afieldS1976 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1597 = _M0L8_2afieldS1976;
              int32_t _M0L5startS1599 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1598 = _M0L5startS1599 + 3;
              int32_t _M0L6_2atmpS1975 = _M0L5bytesS1597[_M0L6_2atmpS1598];
              int32_t _M0L4_2axS519 = _M0L6_2atmpS1975;
              if (_M0L4_2axS519 <= 127) {
                moonbit_bytes_t _M0L8_2afieldS1972 = _M0L11_2aparam__1S484.$0;
                moonbit_bytes_t _M0L5bytesS1594 = _M0L8_2afieldS1972;
                int32_t _M0L5startS1596 = _M0L11_2aparam__1S484.$1;
                int32_t _M0L6_2atmpS1595 = _M0L5startS1596 + 4;
                int32_t _M0L6_2atmpS1971 = _M0L5bytesS1594[_M0L6_2atmpS1595];
                int32_t _M0L4_2axS520 = _M0L6_2atmpS1971;
                if (_M0L4_2axS520 <= 127) {
                  moonbit_bytes_t _M0L8_2afieldS1968 =
                    _M0L11_2aparam__1S484.$0;
                  moonbit_bytes_t _M0L5bytesS1591 = _M0L8_2afieldS1968;
                  int32_t _M0L5startS1593 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1592 = _M0L5startS1593 + 5;
                  int32_t _M0L6_2atmpS1967 =
                    _M0L5bytesS1591[_M0L6_2atmpS1592];
                  int32_t _M0L4_2axS521 = _M0L6_2atmpS1967;
                  if (_M0L4_2axS521 <= 127) {
                    moonbit_bytes_t _M0L8_2afieldS1964 =
                      _M0L11_2aparam__1S484.$0;
                    moonbit_bytes_t _M0L5bytesS1588 = _M0L8_2afieldS1964;
                    int32_t _M0L5startS1590 = _M0L11_2aparam__1S484.$1;
                    int32_t _M0L6_2atmpS1589 = _M0L5startS1590 + 6;
                    int32_t _M0L6_2atmpS1963 =
                      _M0L5bytesS1588[_M0L6_2atmpS1589];
                    int32_t _M0L4_2axS522 = _M0L6_2atmpS1963;
                    if (_M0L4_2axS522 <= 127) {
                      moonbit_bytes_t _M0L8_2afieldS1960 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1585 = _M0L8_2afieldS1960;
                      int32_t _M0L5startS1587 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1586 = _M0L5startS1587 + 7;
                      int32_t _M0L6_2atmpS1959 =
                        _M0L5bytesS1585[_M0L6_2atmpS1586];
                      int32_t _M0L4_2axS523 = _M0L6_2atmpS1959;
                      if (_M0L4_2axS523 <= 127) {
                        moonbit_bytes_t _M0L8_2afieldS1956 =
                          _M0L11_2aparam__1S484.$0;
                        moonbit_bytes_t _M0L5bytesS1581 = _M0L8_2afieldS1956;
                        int32_t _M0L5startS1584 = _M0L11_2aparam__1S484.$1;
                        int32_t _M0L6_2atmpS1582 = _M0L5startS1584 + 8;
                        int32_t _M0L8_2afieldS1955 = _M0L11_2aparam__1S484.$2;
                        int32_t _M0L3endS1583 = _M0L8_2afieldS1955;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS524 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1582,
                                                             _M0L3endS1583,
                                                             _M0L5bytesS1581};
                        int32_t _M0L6_2atmpS1573;
                        int32_t _M0L6_2atmpS1574;
                        int32_t _M0L6_2atmpS1575;
                        int32_t _M0L6_2atmpS1576;
                        int32_t _M0L6_2atmpS1577;
                        int32_t _M0L6_2atmpS1578;
                        int32_t _M0L6_2atmpS1579;
                        int32_t _M0L6_2atmpS1580;
                        _M0L1tS481[_M0L11_2aparam__0S483] = _M0L4_2axS516;
                        _M0L6_2atmpS1573 = _M0L11_2aparam__0S483 + 2;
                        _M0L1tS481[_M0L6_2atmpS1573] = _M0L4_2axS517;
                        _M0L6_2atmpS1574 = _M0L11_2aparam__0S483 + 4;
                        _M0L1tS481[_M0L6_2atmpS1574] = _M0L4_2axS518;
                        _M0L6_2atmpS1575 = _M0L11_2aparam__0S483 + 6;
                        _M0L1tS481[_M0L6_2atmpS1575] = _M0L4_2axS519;
                        _M0L6_2atmpS1576 = _M0L11_2aparam__0S483 + 8;
                        _M0L1tS481[_M0L6_2atmpS1576] = _M0L4_2axS520;
                        _M0L6_2atmpS1577 = _M0L11_2aparam__0S483 + 10;
                        _M0L1tS481[_M0L6_2atmpS1577] = _M0L4_2axS521;
                        _M0L6_2atmpS1578 = _M0L11_2aparam__0S483 + 12;
                        _M0L1tS481[_M0L6_2atmpS1578] = _M0L4_2axS522;
                        _M0L6_2atmpS1579 = _M0L11_2aparam__0S483 + 14;
                        _M0L1tS481[_M0L6_2atmpS1579] = _M0L4_2axS523;
                        _M0L6_2atmpS1580 = _M0L11_2aparam__0S483 + 16;
                        _M0L11_2aparam__0S483 = _M0L6_2atmpS1580;
                        _M0L11_2aparam__1S484 = _M0L4_2axS524;
                        continue;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS1958 =
                          _M0L11_2aparam__1S484.$0;
                        moonbit_bytes_t _M0L5bytesS1569 = _M0L8_2afieldS1958;
                        int32_t _M0L5startS1572 = _M0L11_2aparam__1S484.$1;
                        int32_t _M0L6_2atmpS1570 = _M0L5startS1572 + 1;
                        int32_t _M0L8_2afieldS1957 = _M0L11_2aparam__1S484.$2;
                        int32_t _M0L3endS1571 = _M0L8_2afieldS1957;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS525 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1570,
                                                             _M0L3endS1571,
                                                             _M0L5bytesS1569};
                        _M0L4tlenS513 = _M0L11_2aparam__0S483;
                        _M0L4restS514 = _M0L4_2axS525;
                        _M0L1bS515 = _M0L4_2axS516;
                        goto join_512;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS1962 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1565 = _M0L8_2afieldS1962;
                      int32_t _M0L5startS1568 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1566 = _M0L5startS1568 + 1;
                      int32_t _M0L8_2afieldS1961 = _M0L11_2aparam__1S484.$2;
                      int32_t _M0L3endS1567 = _M0L8_2afieldS1961;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS526 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1566,
                                                           _M0L3endS1567,
                                                           _M0L5bytesS1565};
                      _M0L4tlenS513 = _M0L11_2aparam__0S483;
                      _M0L4restS514 = _M0L4_2axS526;
                      _M0L1bS515 = _M0L4_2axS516;
                      goto join_512;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS1966 =
                      _M0L11_2aparam__1S484.$0;
                    moonbit_bytes_t _M0L5bytesS1561 = _M0L8_2afieldS1966;
                    int32_t _M0L5startS1564 = _M0L11_2aparam__1S484.$1;
                    int32_t _M0L6_2atmpS1562 = _M0L5startS1564 + 1;
                    int32_t _M0L8_2afieldS1965 = _M0L11_2aparam__1S484.$2;
                    int32_t _M0L3endS1563 = _M0L8_2afieldS1965;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS527 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1562,
                                                         _M0L3endS1563,
                                                         _M0L5bytesS1561};
                    _M0L4tlenS513 = _M0L11_2aparam__0S483;
                    _M0L4restS514 = _M0L4_2axS527;
                    _M0L1bS515 = _M0L4_2axS516;
                    goto join_512;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS1970 =
                    _M0L11_2aparam__1S484.$0;
                  moonbit_bytes_t _M0L5bytesS1557 = _M0L8_2afieldS1970;
                  int32_t _M0L5startS1560 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1558 = _M0L5startS1560 + 1;
                  int32_t _M0L8_2afieldS1969 = _M0L11_2aparam__1S484.$2;
                  int32_t _M0L3endS1559 = _M0L8_2afieldS1969;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS528 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1558,
                                                       _M0L3endS1559,
                                                       _M0L5bytesS1557};
                  _M0L4tlenS513 = _M0L11_2aparam__0S483;
                  _M0L4restS514 = _M0L4_2axS528;
                  _M0L1bS515 = _M0L4_2axS516;
                  goto join_512;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS1974 = _M0L11_2aparam__1S484.$0;
                moonbit_bytes_t _M0L5bytesS1553 = _M0L8_2afieldS1974;
                int32_t _M0L5startS1556 = _M0L11_2aparam__1S484.$1;
                int32_t _M0L6_2atmpS1554 = _M0L5startS1556 + 1;
                int32_t _M0L8_2afieldS1973 = _M0L11_2aparam__1S484.$2;
                int32_t _M0L3endS1555 = _M0L8_2afieldS1973;
                struct _M0TPC15bytes9BytesView _M0L4_2axS529 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1554,
                                                     _M0L3endS1555,
                                                     _M0L5bytesS1553};
                _M0L4tlenS513 = _M0L11_2aparam__0S483;
                _M0L4restS514 = _M0L4_2axS529;
                _M0L1bS515 = _M0L4_2axS516;
                goto join_512;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS1978 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1549 = _M0L8_2afieldS1978;
              int32_t _M0L5startS1552 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1550 = _M0L5startS1552 + 1;
              int32_t _M0L8_2afieldS1977 = _M0L11_2aparam__1S484.$2;
              int32_t _M0L3endS1551 = _M0L8_2afieldS1977;
              struct _M0TPC15bytes9BytesView _M0L4_2axS530 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1550,
                                                   _M0L3endS1551,
                                                   _M0L5bytesS1549};
              _M0L4tlenS513 = _M0L11_2aparam__0S483;
              _M0L4restS514 = _M0L4_2axS530;
              _M0L1bS515 = _M0L4_2axS516;
              goto join_512;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS1982 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1545 = _M0L8_2afieldS1982;
            int32_t _M0L5startS1548 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1546 = _M0L5startS1548 + 1;
            int32_t _M0L8_2afieldS1981 = _M0L11_2aparam__1S484.$2;
            int32_t _M0L3endS1547 = _M0L8_2afieldS1981;
            struct _M0TPC15bytes9BytesView _M0L4_2axS531 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1546,
                                                 _M0L3endS1547,
                                                 _M0L5bytesS1545};
            _M0L4tlenS513 = _M0L11_2aparam__0S483;
            _M0L4restS514 = _M0L4_2axS531;
            _M0L1bS515 = _M0L4_2axS516;
            goto join_512;
          }
        } else if (_M0L4_2axS516 >= 194 && _M0L4_2axS516 <= 223) {
          moonbit_bytes_t _M0L8_2afieldS1988 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1542 = _M0L8_2afieldS1988;
          int32_t _M0L5startS1544 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1543 = _M0L5startS1544 + 1;
          int32_t _M0L6_2atmpS1987 = _M0L5bytesS1542[_M0L6_2atmpS1543];
          int32_t _M0L4_2axS532 = _M0L6_2atmpS1987;
          if (_M0L4_2axS532 >= 128 && _M0L4_2axS532 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS1986 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1538 = _M0L8_2afieldS1986;
            int32_t _M0L5startS1541 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1539 = _M0L5startS1541 + 2;
            int32_t _M0L8_2afieldS1985 = _M0L11_2aparam__1S484.$2;
            int32_t _M0L3endS1540 = _M0L8_2afieldS1985;
            struct _M0TPC15bytes9BytesView _M0L4_2axS533 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1539,
                                                 _M0L3endS1540,
                                                 _M0L5bytesS1538};
            _M0L4tlenS507 = _M0L11_2aparam__0S483;
            _M0L4restS508 = _M0L4_2axS533;
            _M0L2b0S509 = _M0L4_2axS516;
            _M0L2b1S510 = _M0L4_2axS532;
            goto join_506;
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 == 224) {
          moonbit_bytes_t _M0L8_2afieldS1994 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1535 = _M0L8_2afieldS1994;
          int32_t _M0L5startS1537 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1536 = _M0L5startS1537 + 1;
          int32_t _M0L6_2atmpS1993 = _M0L5bytesS1535[_M0L6_2atmpS1536];
          int32_t _M0L4_2axS534 = _M0L6_2atmpS1993;
          if (_M0L4_2axS534 >= 160 && _M0L4_2axS534 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS1992 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1532 = _M0L8_2afieldS1992;
            int32_t _M0L5startS1534 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1533 = _M0L5startS1534 + 2;
            int32_t _M0L6_2atmpS1991 = _M0L5bytesS1532[_M0L6_2atmpS1533];
            int32_t _M0L4_2axS535 = _M0L6_2atmpS1991;
            if (_M0L4_2axS535 >= 128 && _M0L4_2axS535 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS1990 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1528 = _M0L8_2afieldS1990;
              int32_t _M0L5startS1531 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1529 = _M0L5startS1531 + 3;
              int32_t _M0L8_2afieldS1989 = _M0L11_2aparam__1S484.$2;
              int32_t _M0L3endS1530 = _M0L8_2afieldS1989;
              struct _M0TPC15bytes9BytesView _M0L4_2axS536 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1529,
                                                   _M0L3endS1530,
                                                   _M0L5bytesS1528};
              _M0L4tlenS500 = _M0L11_2aparam__0S483;
              _M0L2b0S501 = _M0L4_2axS516;
              _M0L2b1S502 = _M0L4_2axS534;
              _M0L2b2S503 = _M0L4_2axS535;
              _M0L4restS504 = _M0L4_2axS536;
              goto join_499;
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 >= 225 && _M0L4_2axS516 <= 236) {
          moonbit_bytes_t _M0L8_2afieldS2000 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1525 = _M0L8_2afieldS2000;
          int32_t _M0L5startS1527 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1526 = _M0L5startS1527 + 1;
          int32_t _M0L6_2atmpS1999 = _M0L5bytesS1525[_M0L6_2atmpS1526];
          int32_t _M0L4_2axS537 = _M0L6_2atmpS1999;
          if (_M0L4_2axS537 >= 128 && _M0L4_2axS537 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS1998 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1522 = _M0L8_2afieldS1998;
            int32_t _M0L5startS1524 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1523 = _M0L5startS1524 + 2;
            int32_t _M0L6_2atmpS1997 = _M0L5bytesS1522[_M0L6_2atmpS1523];
            int32_t _M0L4_2axS538 = _M0L6_2atmpS1997;
            if (_M0L4_2axS538 >= 128 && _M0L4_2axS538 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS1996 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1518 = _M0L8_2afieldS1996;
              int32_t _M0L5startS1521 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1519 = _M0L5startS1521 + 3;
              int32_t _M0L8_2afieldS1995 = _M0L11_2aparam__1S484.$2;
              int32_t _M0L3endS1520 = _M0L8_2afieldS1995;
              struct _M0TPC15bytes9BytesView _M0L4_2axS539 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1519,
                                                   _M0L3endS1520,
                                                   _M0L5bytesS1518};
              _M0L4tlenS500 = _M0L11_2aparam__0S483;
              _M0L2b0S501 = _M0L4_2axS516;
              _M0L2b1S502 = _M0L4_2axS537;
              _M0L2b2S503 = _M0L4_2axS538;
              _M0L4restS504 = _M0L4_2axS539;
              goto join_499;
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 == 237) {
          moonbit_bytes_t _M0L8_2afieldS2006 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1515 = _M0L8_2afieldS2006;
          int32_t _M0L5startS1517 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1516 = _M0L5startS1517 + 1;
          int32_t _M0L6_2atmpS2005 = _M0L5bytesS1515[_M0L6_2atmpS1516];
          int32_t _M0L4_2axS540 = _M0L6_2atmpS2005;
          if (_M0L4_2axS540 >= 128 && _M0L4_2axS540 <= 159) {
            moonbit_bytes_t _M0L8_2afieldS2004 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1512 = _M0L8_2afieldS2004;
            int32_t _M0L5startS1514 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1513 = _M0L5startS1514 + 2;
            int32_t _M0L6_2atmpS2003 = _M0L5bytesS1512[_M0L6_2atmpS1513];
            int32_t _M0L4_2axS541 = _M0L6_2atmpS2003;
            if (_M0L4_2axS541 >= 128 && _M0L4_2axS541 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2002 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1508 = _M0L8_2afieldS2002;
              int32_t _M0L5startS1511 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1509 = _M0L5startS1511 + 3;
              int32_t _M0L8_2afieldS2001 = _M0L11_2aparam__1S484.$2;
              int32_t _M0L3endS1510 = _M0L8_2afieldS2001;
              struct _M0TPC15bytes9BytesView _M0L4_2axS542 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1509,
                                                   _M0L3endS1510,
                                                   _M0L5bytesS1508};
              _M0L4tlenS500 = _M0L11_2aparam__0S483;
              _M0L2b0S501 = _M0L4_2axS516;
              _M0L2b1S502 = _M0L4_2axS540;
              _M0L2b2S503 = _M0L4_2axS541;
              _M0L4restS504 = _M0L4_2axS542;
              goto join_499;
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 >= 238 && _M0L4_2axS516 <= 239) {
          moonbit_bytes_t _M0L8_2afieldS2012 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1505 = _M0L8_2afieldS2012;
          int32_t _M0L5startS1507 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1506 = _M0L5startS1507 + 1;
          int32_t _M0L6_2atmpS2011 = _M0L5bytesS1505[_M0L6_2atmpS1506];
          int32_t _M0L4_2axS543 = _M0L6_2atmpS2011;
          if (_M0L4_2axS543 >= 128 && _M0L4_2axS543 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2010 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1502 = _M0L8_2afieldS2010;
            int32_t _M0L5startS1504 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1503 = _M0L5startS1504 + 2;
            int32_t _M0L6_2atmpS2009 = _M0L5bytesS1502[_M0L6_2atmpS1503];
            int32_t _M0L4_2axS544 = _M0L6_2atmpS2009;
            if (_M0L4_2axS544 >= 128 && _M0L4_2axS544 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2008 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1498 = _M0L8_2afieldS2008;
              int32_t _M0L5startS1501 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1499 = _M0L5startS1501 + 3;
              int32_t _M0L8_2afieldS2007 = _M0L11_2aparam__1S484.$2;
              int32_t _M0L3endS1500 = _M0L8_2afieldS2007;
              struct _M0TPC15bytes9BytesView _M0L4_2axS545 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1499,
                                                   _M0L3endS1500,
                                                   _M0L5bytesS1498};
              _M0L4tlenS500 = _M0L11_2aparam__0S483;
              _M0L2b0S501 = _M0L4_2axS516;
              _M0L2b1S502 = _M0L4_2axS543;
              _M0L2b2S503 = _M0L4_2axS544;
              _M0L4restS504 = _M0L4_2axS545;
              goto join_499;
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 == 240) {
          moonbit_bytes_t _M0L8_2afieldS2020 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1495 = _M0L8_2afieldS2020;
          int32_t _M0L5startS1497 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1496 = _M0L5startS1497 + 1;
          int32_t _M0L6_2atmpS2019 = _M0L5bytesS1495[_M0L6_2atmpS1496];
          int32_t _M0L4_2axS546 = _M0L6_2atmpS2019;
          if (_M0L4_2axS546 >= 144 && _M0L4_2axS546 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2018 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1492 = _M0L8_2afieldS2018;
            int32_t _M0L5startS1494 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1493 = _M0L5startS1494 + 2;
            int32_t _M0L6_2atmpS2017 = _M0L5bytesS1492[_M0L6_2atmpS1493];
            int32_t _M0L4_2axS547 = _M0L6_2atmpS2017;
            if (_M0L4_2axS547 >= 128 && _M0L4_2axS547 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2016 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1489 = _M0L8_2afieldS2016;
              int32_t _M0L5startS1491 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1490 = _M0L5startS1491 + 3;
              int32_t _M0L6_2atmpS2015 = _M0L5bytesS1489[_M0L6_2atmpS1490];
              int32_t _M0L4_2axS548 = _M0L6_2atmpS2015;
              if (_M0L4_2axS548 >= 128 && _M0L4_2axS548 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2014 = _M0L11_2aparam__1S484.$0;
                moonbit_bytes_t _M0L5bytesS1485 = _M0L8_2afieldS2014;
                int32_t _M0L5startS1488 = _M0L11_2aparam__1S484.$1;
                int32_t _M0L6_2atmpS1486 = _M0L5startS1488 + 4;
                int32_t _M0L8_2afieldS2013 = _M0L11_2aparam__1S484.$2;
                int32_t _M0L3endS1487 = _M0L8_2afieldS2013;
                struct _M0TPC15bytes9BytesView _M0L4_2axS549 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1486,
                                                     _M0L3endS1487,
                                                     _M0L5bytesS1485};
                _M0L4tlenS488 = _M0L11_2aparam__0S483;
                _M0L2b0S489 = _M0L4_2axS516;
                _M0L2b1S490 = _M0L4_2axS546;
                _M0L2b2S491 = _M0L4_2axS547;
                _M0L2b3S492 = _M0L4_2axS548;
                _M0L4restS493 = _M0L4_2axS549;
                goto join_487;
              } else {
                moonbit_decref(_M0L1tS481);
                _M0L5bytesS486 = _M0L11_2aparam__1S484;
                goto join_485;
              }
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 >= 241 && _M0L4_2axS516 <= 243) {
          moonbit_bytes_t _M0L8_2afieldS2028 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1482 = _M0L8_2afieldS2028;
          int32_t _M0L5startS1484 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1483 = _M0L5startS1484 + 1;
          int32_t _M0L6_2atmpS2027 = _M0L5bytesS1482[_M0L6_2atmpS1483];
          int32_t _M0L4_2axS550 = _M0L6_2atmpS2027;
          if (_M0L4_2axS550 >= 128 && _M0L4_2axS550 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS2026 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1479 = _M0L8_2afieldS2026;
            int32_t _M0L5startS1481 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1480 = _M0L5startS1481 + 2;
            int32_t _M0L6_2atmpS2025 = _M0L5bytesS1479[_M0L6_2atmpS1480];
            int32_t _M0L4_2axS551 = _M0L6_2atmpS2025;
            if (_M0L4_2axS551 >= 128 && _M0L4_2axS551 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2024 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1476 = _M0L8_2afieldS2024;
              int32_t _M0L5startS1478 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1477 = _M0L5startS1478 + 3;
              int32_t _M0L6_2atmpS2023 = _M0L5bytesS1476[_M0L6_2atmpS1477];
              int32_t _M0L4_2axS552 = _M0L6_2atmpS2023;
              if (_M0L4_2axS552 >= 128 && _M0L4_2axS552 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2022 = _M0L11_2aparam__1S484.$0;
                moonbit_bytes_t _M0L5bytesS1472 = _M0L8_2afieldS2022;
                int32_t _M0L5startS1475 = _M0L11_2aparam__1S484.$1;
                int32_t _M0L6_2atmpS1473 = _M0L5startS1475 + 4;
                int32_t _M0L8_2afieldS2021 = _M0L11_2aparam__1S484.$2;
                int32_t _M0L3endS1474 = _M0L8_2afieldS2021;
                struct _M0TPC15bytes9BytesView _M0L4_2axS553 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1473,
                                                     _M0L3endS1474,
                                                     _M0L5bytesS1472};
                _M0L4tlenS488 = _M0L11_2aparam__0S483;
                _M0L2b0S489 = _M0L4_2axS516;
                _M0L2b1S490 = _M0L4_2axS550;
                _M0L2b2S491 = _M0L4_2axS551;
                _M0L2b3S492 = _M0L4_2axS552;
                _M0L4restS493 = _M0L4_2axS553;
                goto join_487;
              } else {
                moonbit_decref(_M0L1tS481);
                _M0L5bytesS486 = _M0L11_2aparam__1S484;
                goto join_485;
              }
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else if (_M0L4_2axS516 == 244) {
          moonbit_bytes_t _M0L8_2afieldS2036 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1469 = _M0L8_2afieldS2036;
          int32_t _M0L5startS1471 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1470 = _M0L5startS1471 + 1;
          int32_t _M0L6_2atmpS2035 = _M0L5bytesS1469[_M0L6_2atmpS1470];
          int32_t _M0L4_2axS554 = _M0L6_2atmpS2035;
          if (_M0L4_2axS554 >= 128 && _M0L4_2axS554 <= 143) {
            moonbit_bytes_t _M0L8_2afieldS2034 = _M0L11_2aparam__1S484.$0;
            moonbit_bytes_t _M0L5bytesS1466 = _M0L8_2afieldS2034;
            int32_t _M0L5startS1468 = _M0L11_2aparam__1S484.$1;
            int32_t _M0L6_2atmpS1467 = _M0L5startS1468 + 2;
            int32_t _M0L6_2atmpS2033 = _M0L5bytesS1466[_M0L6_2atmpS1467];
            int32_t _M0L4_2axS555 = _M0L6_2atmpS2033;
            if (_M0L4_2axS555 >= 128 && _M0L4_2axS555 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS2032 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1463 = _M0L8_2afieldS2032;
              int32_t _M0L5startS1465 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1464 = _M0L5startS1465 + 3;
              int32_t _M0L6_2atmpS2031 = _M0L5bytesS1463[_M0L6_2atmpS1464];
              int32_t _M0L4_2axS556 = _M0L6_2atmpS2031;
              if (_M0L4_2axS556 >= 128 && _M0L4_2axS556 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2030 = _M0L11_2aparam__1S484.$0;
                moonbit_bytes_t _M0L5bytesS1459 = _M0L8_2afieldS2030;
                int32_t _M0L5startS1462 = _M0L11_2aparam__1S484.$1;
                int32_t _M0L6_2atmpS1460 = _M0L5startS1462 + 4;
                int32_t _M0L8_2afieldS2029 = _M0L11_2aparam__1S484.$2;
                int32_t _M0L3endS1461 = _M0L8_2afieldS2029;
                struct _M0TPC15bytes9BytesView _M0L4_2axS557 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1460,
                                                     _M0L3endS1461,
                                                     _M0L5bytesS1459};
                _M0L4tlenS488 = _M0L11_2aparam__0S483;
                _M0L2b0S489 = _M0L4_2axS516;
                _M0L2b1S490 = _M0L4_2axS554;
                _M0L2b2S491 = _M0L4_2axS555;
                _M0L2b3S492 = _M0L4_2axS556;
                _M0L4restS493 = _M0L4_2axS557;
                goto join_487;
              } else {
                moonbit_decref(_M0L1tS481);
                _M0L5bytesS486 = _M0L11_2aparam__1S484;
                goto join_485;
              }
            } else {
              moonbit_decref(_M0L1tS481);
              _M0L5bytesS486 = _M0L11_2aparam__1S484;
              goto join_485;
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        } else {
          moonbit_decref(_M0L1tS481);
          _M0L5bytesS486 = _M0L11_2aparam__1S484;
          goto join_485;
        }
      } else {
        moonbit_bytes_t _M0L8_2afieldS2094 = _M0L11_2aparam__1S484.$0;
        moonbit_bytes_t _M0L5bytesS1746 = _M0L8_2afieldS2094;
        int32_t _M0L5startS1747 = _M0L11_2aparam__1S484.$1;
        int32_t _M0L6_2atmpS2093 = _M0L5bytesS1746[_M0L5startS1747];
        int32_t _M0L4_2axS558 = _M0L6_2atmpS2093;
        if (_M0L4_2axS558 >= 0 && _M0L4_2axS558 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS2040 = _M0L11_2aparam__1S484.$0;
          moonbit_bytes_t _M0L5bytesS1742 = _M0L8_2afieldS2040;
          int32_t _M0L5startS1745 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1743 = _M0L5startS1745 + 1;
          int32_t _M0L8_2afieldS2039 = _M0L11_2aparam__1S484.$2;
          int32_t _M0L3endS1744 = _M0L8_2afieldS2039;
          struct _M0TPC15bytes9BytesView _M0L4_2axS559 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1743,
                                               _M0L3endS1744,
                                               _M0L5bytesS1742};
          _M0L4tlenS513 = _M0L11_2aparam__0S483;
          _M0L4restS514 = _M0L4_2axS559;
          _M0L1bS515 = _M0L4_2axS558;
          goto join_512;
        } else {
          int32_t _M0L3endS1609 = _M0L11_2aparam__1S484.$2;
          int32_t _M0L5startS1610 = _M0L11_2aparam__1S484.$1;
          int32_t _M0L6_2atmpS1608 = _M0L3endS1609 - _M0L5startS1610;
          if (_M0L6_2atmpS1608 >= 2) {
            if (_M0L4_2axS558 >= 194 && _M0L4_2axS558 <= 223) {
              moonbit_bytes_t _M0L8_2afieldS2044 = _M0L11_2aparam__1S484.$0;
              moonbit_bytes_t _M0L5bytesS1739 = _M0L8_2afieldS2044;
              int32_t _M0L5startS1741 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1740 = _M0L5startS1741 + 1;
              int32_t _M0L6_2atmpS2043 = _M0L5bytesS1739[_M0L6_2atmpS1740];
              int32_t _M0L4_2axS560 = _M0L6_2atmpS2043;
              if (_M0L4_2axS560 >= 128 && _M0L4_2axS560 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS2042 = _M0L11_2aparam__1S484.$0;
                moonbit_bytes_t _M0L5bytesS1735 = _M0L8_2afieldS2042;
                int32_t _M0L5startS1738 = _M0L11_2aparam__1S484.$1;
                int32_t _M0L6_2atmpS1736 = _M0L5startS1738 + 2;
                int32_t _M0L8_2afieldS2041 = _M0L11_2aparam__1S484.$2;
                int32_t _M0L3endS1737 = _M0L8_2afieldS2041;
                struct _M0TPC15bytes9BytesView _M0L4_2axS561 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1736,
                                                     _M0L3endS1737,
                                                     _M0L5bytesS1735};
                _M0L4tlenS507 = _M0L11_2aparam__0S483;
                _M0L4restS508 = _M0L4_2axS561;
                _M0L2b0S509 = _M0L4_2axS558;
                _M0L2b1S510 = _M0L4_2axS560;
                goto join_506;
              } else {
                int32_t _M0L3endS1729;
                int32_t _M0L5startS1730;
                int32_t _M0L6_2atmpS1728;
                moonbit_decref(_M0L1tS481);
                _M0L3endS1729 = _M0L11_2aparam__1S484.$2;
                _M0L5startS1730 = _M0L11_2aparam__1S484.$1;
                _M0L6_2atmpS1728 = _M0L3endS1729 - _M0L5startS1730;
                if (_M0L6_2atmpS1728 >= 3) {
                  int32_t _M0L3endS1733 = _M0L11_2aparam__1S484.$2;
                  int32_t _M0L5startS1734 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1732 = _M0L3endS1733 - _M0L5startS1734;
                  int32_t _M0L6_2atmpS1731 = _M0L6_2atmpS1732 >= 4;
                  _M0L5bytesS486 = _M0L11_2aparam__1S484;
                  goto join_485;
                } else {
                  _M0L5bytesS486 = _M0L11_2aparam__1S484;
                  goto join_485;
                }
              }
            } else {
              int32_t _M0L3endS1612 = _M0L11_2aparam__1S484.$2;
              int32_t _M0L5startS1613 = _M0L11_2aparam__1S484.$1;
              int32_t _M0L6_2atmpS1611 = _M0L3endS1612 - _M0L5startS1613;
              if (_M0L6_2atmpS1611 >= 3) {
                if (_M0L4_2axS558 == 224) {
                  moonbit_bytes_t _M0L8_2afieldS2050 =
                    _M0L11_2aparam__1S484.$0;
                  moonbit_bytes_t _M0L5bytesS1725 = _M0L8_2afieldS2050;
                  int32_t _M0L5startS1727 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1726 = _M0L5startS1727 + 1;
                  int32_t _M0L6_2atmpS2049 =
                    _M0L5bytesS1725[_M0L6_2atmpS1726];
                  int32_t _M0L4_2axS562 = _M0L6_2atmpS2049;
                  if (_M0L4_2axS562 >= 160 && _M0L4_2axS562 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS2048 =
                      _M0L11_2aparam__1S484.$0;
                    moonbit_bytes_t _M0L5bytesS1722 = _M0L8_2afieldS2048;
                    int32_t _M0L5startS1724 = _M0L11_2aparam__1S484.$1;
                    int32_t _M0L6_2atmpS1723 = _M0L5startS1724 + 2;
                    int32_t _M0L6_2atmpS2047 =
                      _M0L5bytesS1722[_M0L6_2atmpS1723];
                    int32_t _M0L4_2axS563 = _M0L6_2atmpS2047;
                    if (_M0L4_2axS563 >= 128 && _M0L4_2axS563 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2046 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1718 = _M0L8_2afieldS2046;
                      int32_t _M0L5startS1721 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1719 = _M0L5startS1721 + 3;
                      int32_t _M0L8_2afieldS2045 = _M0L11_2aparam__1S484.$2;
                      int32_t _M0L3endS1720 = _M0L8_2afieldS2045;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS564 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1719,
                                                           _M0L3endS1720,
                                                           _M0L5bytesS1718};
                      _M0L4tlenS500 = _M0L11_2aparam__0S483;
                      _M0L2b0S501 = _M0L4_2axS558;
                      _M0L2b1S502 = _M0L4_2axS562;
                      _M0L2b2S503 = _M0L4_2axS563;
                      _M0L4restS504 = _M0L4_2axS564;
                      goto join_499;
                    } else {
                      int32_t _M0L3endS1716;
                      int32_t _M0L5startS1717;
                      int32_t _M0L6_2atmpS1715;
                      int32_t _M0L6_2atmpS1714;
                      moonbit_decref(_M0L1tS481);
                      _M0L3endS1716 = _M0L11_2aparam__1S484.$2;
                      _M0L5startS1717 = _M0L11_2aparam__1S484.$1;
                      _M0L6_2atmpS1715 = _M0L3endS1716 - _M0L5startS1717;
                      _M0L6_2atmpS1714 = _M0L6_2atmpS1715 >= 4;
                      _M0L5bytesS486 = _M0L11_2aparam__1S484;
                      goto join_485;
                    }
                  } else {
                    int32_t _M0L3endS1712;
                    int32_t _M0L5startS1713;
                    int32_t _M0L6_2atmpS1711;
                    int32_t _M0L6_2atmpS1710;
                    moonbit_decref(_M0L1tS481);
                    _M0L3endS1712 = _M0L11_2aparam__1S484.$2;
                    _M0L5startS1713 = _M0L11_2aparam__1S484.$1;
                    _M0L6_2atmpS1711 = _M0L3endS1712 - _M0L5startS1713;
                    _M0L6_2atmpS1710 = _M0L6_2atmpS1711 >= 4;
                    _M0L5bytesS486 = _M0L11_2aparam__1S484;
                    goto join_485;
                  }
                } else if (_M0L4_2axS558 >= 225 && _M0L4_2axS558 <= 236) {
                  moonbit_bytes_t _M0L8_2afieldS2056 =
                    _M0L11_2aparam__1S484.$0;
                  moonbit_bytes_t _M0L5bytesS1707 = _M0L8_2afieldS2056;
                  int32_t _M0L5startS1709 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1708 = _M0L5startS1709 + 1;
                  int32_t _M0L6_2atmpS2055 =
                    _M0L5bytesS1707[_M0L6_2atmpS1708];
                  int32_t _M0L4_2axS565 = _M0L6_2atmpS2055;
                  if (_M0L4_2axS565 >= 128 && _M0L4_2axS565 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS2054 =
                      _M0L11_2aparam__1S484.$0;
                    moonbit_bytes_t _M0L5bytesS1704 = _M0L8_2afieldS2054;
                    int32_t _M0L5startS1706 = _M0L11_2aparam__1S484.$1;
                    int32_t _M0L6_2atmpS1705 = _M0L5startS1706 + 2;
                    int32_t _M0L6_2atmpS2053 =
                      _M0L5bytesS1704[_M0L6_2atmpS1705];
                    int32_t _M0L4_2axS566 = _M0L6_2atmpS2053;
                    if (_M0L4_2axS566 >= 128 && _M0L4_2axS566 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2052 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1700 = _M0L8_2afieldS2052;
                      int32_t _M0L5startS1703 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1701 = _M0L5startS1703 + 3;
                      int32_t _M0L8_2afieldS2051 = _M0L11_2aparam__1S484.$2;
                      int32_t _M0L3endS1702 = _M0L8_2afieldS2051;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS567 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1701,
                                                           _M0L3endS1702,
                                                           _M0L5bytesS1700};
                      _M0L4tlenS500 = _M0L11_2aparam__0S483;
                      _M0L2b0S501 = _M0L4_2axS558;
                      _M0L2b1S502 = _M0L4_2axS565;
                      _M0L2b2S503 = _M0L4_2axS566;
                      _M0L4restS504 = _M0L4_2axS567;
                      goto join_499;
                    } else {
                      int32_t _M0L3endS1698;
                      int32_t _M0L5startS1699;
                      int32_t _M0L6_2atmpS1697;
                      int32_t _M0L6_2atmpS1696;
                      moonbit_decref(_M0L1tS481);
                      _M0L3endS1698 = _M0L11_2aparam__1S484.$2;
                      _M0L5startS1699 = _M0L11_2aparam__1S484.$1;
                      _M0L6_2atmpS1697 = _M0L3endS1698 - _M0L5startS1699;
                      _M0L6_2atmpS1696 = _M0L6_2atmpS1697 >= 4;
                      _M0L5bytesS486 = _M0L11_2aparam__1S484;
                      goto join_485;
                    }
                  } else {
                    int32_t _M0L3endS1694;
                    int32_t _M0L5startS1695;
                    int32_t _M0L6_2atmpS1693;
                    int32_t _M0L6_2atmpS1692;
                    moonbit_decref(_M0L1tS481);
                    _M0L3endS1694 = _M0L11_2aparam__1S484.$2;
                    _M0L5startS1695 = _M0L11_2aparam__1S484.$1;
                    _M0L6_2atmpS1693 = _M0L3endS1694 - _M0L5startS1695;
                    _M0L6_2atmpS1692 = _M0L6_2atmpS1693 >= 4;
                    _M0L5bytesS486 = _M0L11_2aparam__1S484;
                    goto join_485;
                  }
                } else if (_M0L4_2axS558 == 237) {
                  moonbit_bytes_t _M0L8_2afieldS2062 =
                    _M0L11_2aparam__1S484.$0;
                  moonbit_bytes_t _M0L5bytesS1689 = _M0L8_2afieldS2062;
                  int32_t _M0L5startS1691 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1690 = _M0L5startS1691 + 1;
                  int32_t _M0L6_2atmpS2061 =
                    _M0L5bytesS1689[_M0L6_2atmpS1690];
                  int32_t _M0L4_2axS568 = _M0L6_2atmpS2061;
                  if (_M0L4_2axS568 >= 128 && _M0L4_2axS568 <= 159) {
                    moonbit_bytes_t _M0L8_2afieldS2060 =
                      _M0L11_2aparam__1S484.$0;
                    moonbit_bytes_t _M0L5bytesS1686 = _M0L8_2afieldS2060;
                    int32_t _M0L5startS1688 = _M0L11_2aparam__1S484.$1;
                    int32_t _M0L6_2atmpS1687 = _M0L5startS1688 + 2;
                    int32_t _M0L6_2atmpS2059 =
                      _M0L5bytesS1686[_M0L6_2atmpS1687];
                    int32_t _M0L4_2axS569 = _M0L6_2atmpS2059;
                    if (_M0L4_2axS569 >= 128 && _M0L4_2axS569 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2058 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1682 = _M0L8_2afieldS2058;
                      int32_t _M0L5startS1685 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1683 = _M0L5startS1685 + 3;
                      int32_t _M0L8_2afieldS2057 = _M0L11_2aparam__1S484.$2;
                      int32_t _M0L3endS1684 = _M0L8_2afieldS2057;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS570 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1683,
                                                           _M0L3endS1684,
                                                           _M0L5bytesS1682};
                      _M0L4tlenS500 = _M0L11_2aparam__0S483;
                      _M0L2b0S501 = _M0L4_2axS558;
                      _M0L2b1S502 = _M0L4_2axS568;
                      _M0L2b2S503 = _M0L4_2axS569;
                      _M0L4restS504 = _M0L4_2axS570;
                      goto join_499;
                    } else {
                      int32_t _M0L3endS1680;
                      int32_t _M0L5startS1681;
                      int32_t _M0L6_2atmpS1679;
                      int32_t _M0L6_2atmpS1678;
                      moonbit_decref(_M0L1tS481);
                      _M0L3endS1680 = _M0L11_2aparam__1S484.$2;
                      _M0L5startS1681 = _M0L11_2aparam__1S484.$1;
                      _M0L6_2atmpS1679 = _M0L3endS1680 - _M0L5startS1681;
                      _M0L6_2atmpS1678 = _M0L6_2atmpS1679 >= 4;
                      _M0L5bytesS486 = _M0L11_2aparam__1S484;
                      goto join_485;
                    }
                  } else {
                    int32_t _M0L3endS1676;
                    int32_t _M0L5startS1677;
                    int32_t _M0L6_2atmpS1675;
                    int32_t _M0L6_2atmpS1674;
                    moonbit_decref(_M0L1tS481);
                    _M0L3endS1676 = _M0L11_2aparam__1S484.$2;
                    _M0L5startS1677 = _M0L11_2aparam__1S484.$1;
                    _M0L6_2atmpS1675 = _M0L3endS1676 - _M0L5startS1677;
                    _M0L6_2atmpS1674 = _M0L6_2atmpS1675 >= 4;
                    _M0L5bytesS486 = _M0L11_2aparam__1S484;
                    goto join_485;
                  }
                } else if (_M0L4_2axS558 >= 238 && _M0L4_2axS558 <= 239) {
                  moonbit_bytes_t _M0L8_2afieldS2068 =
                    _M0L11_2aparam__1S484.$0;
                  moonbit_bytes_t _M0L5bytesS1671 = _M0L8_2afieldS2068;
                  int32_t _M0L5startS1673 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1672 = _M0L5startS1673 + 1;
                  int32_t _M0L6_2atmpS2067 =
                    _M0L5bytesS1671[_M0L6_2atmpS1672];
                  int32_t _M0L4_2axS571 = _M0L6_2atmpS2067;
                  if (_M0L4_2axS571 >= 128 && _M0L4_2axS571 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS2066 =
                      _M0L11_2aparam__1S484.$0;
                    moonbit_bytes_t _M0L5bytesS1668 = _M0L8_2afieldS2066;
                    int32_t _M0L5startS1670 = _M0L11_2aparam__1S484.$1;
                    int32_t _M0L6_2atmpS1669 = _M0L5startS1670 + 2;
                    int32_t _M0L6_2atmpS2065 =
                      _M0L5bytesS1668[_M0L6_2atmpS1669];
                    int32_t _M0L4_2axS572 = _M0L6_2atmpS2065;
                    if (_M0L4_2axS572 >= 128 && _M0L4_2axS572 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS2064 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1664 = _M0L8_2afieldS2064;
                      int32_t _M0L5startS1667 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1665 = _M0L5startS1667 + 3;
                      int32_t _M0L8_2afieldS2063 = _M0L11_2aparam__1S484.$2;
                      int32_t _M0L3endS1666 = _M0L8_2afieldS2063;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS573 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1665,
                                                           _M0L3endS1666,
                                                           _M0L5bytesS1664};
                      _M0L4tlenS500 = _M0L11_2aparam__0S483;
                      _M0L2b0S501 = _M0L4_2axS558;
                      _M0L2b1S502 = _M0L4_2axS571;
                      _M0L2b2S503 = _M0L4_2axS572;
                      _M0L4restS504 = _M0L4_2axS573;
                      goto join_499;
                    } else {
                      int32_t _M0L3endS1662;
                      int32_t _M0L5startS1663;
                      int32_t _M0L6_2atmpS1661;
                      int32_t _M0L6_2atmpS1660;
                      moonbit_decref(_M0L1tS481);
                      _M0L3endS1662 = _M0L11_2aparam__1S484.$2;
                      _M0L5startS1663 = _M0L11_2aparam__1S484.$1;
                      _M0L6_2atmpS1661 = _M0L3endS1662 - _M0L5startS1663;
                      _M0L6_2atmpS1660 = _M0L6_2atmpS1661 >= 4;
                      _M0L5bytesS486 = _M0L11_2aparam__1S484;
                      goto join_485;
                    }
                  } else {
                    int32_t _M0L3endS1658;
                    int32_t _M0L5startS1659;
                    int32_t _M0L6_2atmpS1657;
                    int32_t _M0L6_2atmpS1656;
                    moonbit_decref(_M0L1tS481);
                    _M0L3endS1658 = _M0L11_2aparam__1S484.$2;
                    _M0L5startS1659 = _M0L11_2aparam__1S484.$1;
                    _M0L6_2atmpS1657 = _M0L3endS1658 - _M0L5startS1659;
                    _M0L6_2atmpS1656 = _M0L6_2atmpS1657 >= 4;
                    _M0L5bytesS486 = _M0L11_2aparam__1S484;
                    goto join_485;
                  }
                } else {
                  int32_t _M0L3endS1615 = _M0L11_2aparam__1S484.$2;
                  int32_t _M0L5startS1616 = _M0L11_2aparam__1S484.$1;
                  int32_t _M0L6_2atmpS1614 = _M0L3endS1615 - _M0L5startS1616;
                  if (_M0L6_2atmpS1614 >= 4) {
                    if (_M0L4_2axS558 == 240) {
                      moonbit_bytes_t _M0L8_2afieldS2076 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1653 = _M0L8_2afieldS2076;
                      int32_t _M0L5startS1655 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1654 = _M0L5startS1655 + 1;
                      int32_t _M0L6_2atmpS2075 =
                        _M0L5bytesS1653[_M0L6_2atmpS1654];
                      int32_t _M0L4_2axS574 = _M0L6_2atmpS2075;
                      if (_M0L4_2axS574 >= 144 && _M0L4_2axS574 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS2074 =
                          _M0L11_2aparam__1S484.$0;
                        moonbit_bytes_t _M0L5bytesS1650 = _M0L8_2afieldS2074;
                        int32_t _M0L5startS1652 = _M0L11_2aparam__1S484.$1;
                        int32_t _M0L6_2atmpS1651 = _M0L5startS1652 + 2;
                        int32_t _M0L6_2atmpS2073 =
                          _M0L5bytesS1650[_M0L6_2atmpS1651];
                        int32_t _M0L4_2axS575 = _M0L6_2atmpS2073;
                        if (_M0L4_2axS575 >= 128 && _M0L4_2axS575 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS2072 =
                            _M0L11_2aparam__1S484.$0;
                          moonbit_bytes_t _M0L5bytesS1647 =
                            _M0L8_2afieldS2072;
                          int32_t _M0L5startS1649 = _M0L11_2aparam__1S484.$1;
                          int32_t _M0L6_2atmpS1648 = _M0L5startS1649 + 3;
                          int32_t _M0L6_2atmpS2071 =
                            _M0L5bytesS1647[_M0L6_2atmpS1648];
                          int32_t _M0L4_2axS576 = _M0L6_2atmpS2071;
                          if (_M0L4_2axS576 >= 128 && _M0L4_2axS576 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS2070 =
                              _M0L11_2aparam__1S484.$0;
                            moonbit_bytes_t _M0L5bytesS1643 =
                              _M0L8_2afieldS2070;
                            int32_t _M0L5startS1646 =
                              _M0L11_2aparam__1S484.$1;
                            int32_t _M0L6_2atmpS1644 = _M0L5startS1646 + 4;
                            int32_t _M0L8_2afieldS2069 =
                              _M0L11_2aparam__1S484.$2;
                            int32_t _M0L3endS1645 = _M0L8_2afieldS2069;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS577 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1644,
                                                                 _M0L3endS1645,
                                                                 _M0L5bytesS1643};
                            _M0L4tlenS488 = _M0L11_2aparam__0S483;
                            _M0L2b0S489 = _M0L4_2axS558;
                            _M0L2b1S490 = _M0L4_2axS574;
                            _M0L2b2S491 = _M0L4_2axS575;
                            _M0L2b3S492 = _M0L4_2axS576;
                            _M0L4restS493 = _M0L4_2axS577;
                            goto join_487;
                          } else {
                            moonbit_decref(_M0L1tS481);
                            _M0L5bytesS486 = _M0L11_2aparam__1S484;
                            goto join_485;
                          }
                        } else {
                          moonbit_decref(_M0L1tS481);
                          _M0L5bytesS486 = _M0L11_2aparam__1S484;
                          goto join_485;
                        }
                      } else {
                        moonbit_decref(_M0L1tS481);
                        _M0L5bytesS486 = _M0L11_2aparam__1S484;
                        goto join_485;
                      }
                    } else if (_M0L4_2axS558 >= 241 && _M0L4_2axS558 <= 243) {
                      moonbit_bytes_t _M0L8_2afieldS2084 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1640 = _M0L8_2afieldS2084;
                      int32_t _M0L5startS1642 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1641 = _M0L5startS1642 + 1;
                      int32_t _M0L6_2atmpS2083 =
                        _M0L5bytesS1640[_M0L6_2atmpS1641];
                      int32_t _M0L4_2axS578 = _M0L6_2atmpS2083;
                      if (_M0L4_2axS578 >= 128 && _M0L4_2axS578 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS2082 =
                          _M0L11_2aparam__1S484.$0;
                        moonbit_bytes_t _M0L5bytesS1637 = _M0L8_2afieldS2082;
                        int32_t _M0L5startS1639 = _M0L11_2aparam__1S484.$1;
                        int32_t _M0L6_2atmpS1638 = _M0L5startS1639 + 2;
                        int32_t _M0L6_2atmpS2081 =
                          _M0L5bytesS1637[_M0L6_2atmpS1638];
                        int32_t _M0L4_2axS579 = _M0L6_2atmpS2081;
                        if (_M0L4_2axS579 >= 128 && _M0L4_2axS579 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS2080 =
                            _M0L11_2aparam__1S484.$0;
                          moonbit_bytes_t _M0L5bytesS1634 =
                            _M0L8_2afieldS2080;
                          int32_t _M0L5startS1636 = _M0L11_2aparam__1S484.$1;
                          int32_t _M0L6_2atmpS1635 = _M0L5startS1636 + 3;
                          int32_t _M0L6_2atmpS2079 =
                            _M0L5bytesS1634[_M0L6_2atmpS1635];
                          int32_t _M0L4_2axS580 = _M0L6_2atmpS2079;
                          if (_M0L4_2axS580 >= 128 && _M0L4_2axS580 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS2078 =
                              _M0L11_2aparam__1S484.$0;
                            moonbit_bytes_t _M0L5bytesS1630 =
                              _M0L8_2afieldS2078;
                            int32_t _M0L5startS1633 =
                              _M0L11_2aparam__1S484.$1;
                            int32_t _M0L6_2atmpS1631 = _M0L5startS1633 + 4;
                            int32_t _M0L8_2afieldS2077 =
                              _M0L11_2aparam__1S484.$2;
                            int32_t _M0L3endS1632 = _M0L8_2afieldS2077;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS581 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1631,
                                                                 _M0L3endS1632,
                                                                 _M0L5bytesS1630};
                            _M0L4tlenS488 = _M0L11_2aparam__0S483;
                            _M0L2b0S489 = _M0L4_2axS558;
                            _M0L2b1S490 = _M0L4_2axS578;
                            _M0L2b2S491 = _M0L4_2axS579;
                            _M0L2b3S492 = _M0L4_2axS580;
                            _M0L4restS493 = _M0L4_2axS581;
                            goto join_487;
                          } else {
                            moonbit_decref(_M0L1tS481);
                            _M0L5bytesS486 = _M0L11_2aparam__1S484;
                            goto join_485;
                          }
                        } else {
                          moonbit_decref(_M0L1tS481);
                          _M0L5bytesS486 = _M0L11_2aparam__1S484;
                          goto join_485;
                        }
                      } else {
                        moonbit_decref(_M0L1tS481);
                        _M0L5bytesS486 = _M0L11_2aparam__1S484;
                        goto join_485;
                      }
                    } else if (_M0L4_2axS558 == 244) {
                      moonbit_bytes_t _M0L8_2afieldS2092 =
                        _M0L11_2aparam__1S484.$0;
                      moonbit_bytes_t _M0L5bytesS1627 = _M0L8_2afieldS2092;
                      int32_t _M0L5startS1629 = _M0L11_2aparam__1S484.$1;
                      int32_t _M0L6_2atmpS1628 = _M0L5startS1629 + 1;
                      int32_t _M0L6_2atmpS2091 =
                        _M0L5bytesS1627[_M0L6_2atmpS1628];
                      int32_t _M0L4_2axS582 = _M0L6_2atmpS2091;
                      if (_M0L4_2axS582 >= 128 && _M0L4_2axS582 <= 143) {
                        moonbit_bytes_t _M0L8_2afieldS2090 =
                          _M0L11_2aparam__1S484.$0;
                        moonbit_bytes_t _M0L5bytesS1624 = _M0L8_2afieldS2090;
                        int32_t _M0L5startS1626 = _M0L11_2aparam__1S484.$1;
                        int32_t _M0L6_2atmpS1625 = _M0L5startS1626 + 2;
                        int32_t _M0L6_2atmpS2089 =
                          _M0L5bytesS1624[_M0L6_2atmpS1625];
                        int32_t _M0L4_2axS583 = _M0L6_2atmpS2089;
                        if (_M0L4_2axS583 >= 128 && _M0L4_2axS583 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS2088 =
                            _M0L11_2aparam__1S484.$0;
                          moonbit_bytes_t _M0L5bytesS1621 =
                            _M0L8_2afieldS2088;
                          int32_t _M0L5startS1623 = _M0L11_2aparam__1S484.$1;
                          int32_t _M0L6_2atmpS1622 = _M0L5startS1623 + 3;
                          int32_t _M0L6_2atmpS2087 =
                            _M0L5bytesS1621[_M0L6_2atmpS1622];
                          int32_t _M0L4_2axS584 = _M0L6_2atmpS2087;
                          if (_M0L4_2axS584 >= 128 && _M0L4_2axS584 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS2086 =
                              _M0L11_2aparam__1S484.$0;
                            moonbit_bytes_t _M0L5bytesS1617 =
                              _M0L8_2afieldS2086;
                            int32_t _M0L5startS1620 =
                              _M0L11_2aparam__1S484.$1;
                            int32_t _M0L6_2atmpS1618 = _M0L5startS1620 + 4;
                            int32_t _M0L8_2afieldS2085 =
                              _M0L11_2aparam__1S484.$2;
                            int32_t _M0L3endS1619 = _M0L8_2afieldS2085;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS585 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS1618,
                                                                 _M0L3endS1619,
                                                                 _M0L5bytesS1617};
                            _M0L4tlenS488 = _M0L11_2aparam__0S483;
                            _M0L2b0S489 = _M0L4_2axS558;
                            _M0L2b1S490 = _M0L4_2axS582;
                            _M0L2b2S491 = _M0L4_2axS583;
                            _M0L2b3S492 = _M0L4_2axS584;
                            _M0L4restS493 = _M0L4_2axS585;
                            goto join_487;
                          } else {
                            moonbit_decref(_M0L1tS481);
                            _M0L5bytesS486 = _M0L11_2aparam__1S484;
                            goto join_485;
                          }
                        } else {
                          moonbit_decref(_M0L1tS481);
                          _M0L5bytesS486 = _M0L11_2aparam__1S484;
                          goto join_485;
                        }
                      } else {
                        moonbit_decref(_M0L1tS481);
                        _M0L5bytesS486 = _M0L11_2aparam__1S484;
                        goto join_485;
                      }
                    } else {
                      moonbit_decref(_M0L1tS481);
                      _M0L5bytesS486 = _M0L11_2aparam__1S484;
                      goto join_485;
                    }
                  } else {
                    moonbit_decref(_M0L1tS481);
                    _M0L5bytesS486 = _M0L11_2aparam__1S484;
                    goto join_485;
                  }
                }
              } else {
                moonbit_decref(_M0L1tS481);
                _M0L5bytesS486 = _M0L11_2aparam__1S484;
                goto join_485;
              }
            }
          } else {
            moonbit_decref(_M0L1tS481);
            _M0L5bytesS486 = _M0L11_2aparam__1S484;
            goto join_485;
          }
        }
      }
    }
    goto joinlet_2300;
    join_512:;
    _M0L1tS481[_M0L4tlenS513] = _M0L1bS515;
    _M0L6_2atmpS1452 = _M0L4tlenS513 + 2;
    _M0L11_2aparam__0S483 = _M0L6_2atmpS1452;
    _M0L11_2aparam__1S484 = _M0L4restS514;
    continue;
    joinlet_2300:;
    goto joinlet_2299;
    join_506:;
    _M0L6_2atmpS1451 = (int32_t)_M0L2b0S509;
    _M0L6_2atmpS1450 = _M0L6_2atmpS1451 & 31;
    _M0L6_2atmpS1447 = _M0L6_2atmpS1450 << 6;
    _M0L6_2atmpS1449 = (int32_t)_M0L2b1S510;
    _M0L6_2atmpS1448 = _M0L6_2atmpS1449 & 63;
    _M0L2chS511 = _M0L6_2atmpS1447 | _M0L6_2atmpS1448;
    _M0L6_2atmpS1442 = _M0L2chS511 & 0xff;
    _M0L1tS481[_M0L4tlenS507] = _M0L6_2atmpS1442;
    _M0L6_2atmpS1443 = _M0L4tlenS507 + 1;
    _M0L6_2atmpS1445 = _M0L2chS511 >> 8;
    _M0L6_2atmpS1444 = _M0L6_2atmpS1445 & 0xff;
    _M0L1tS481[_M0L6_2atmpS1443] = _M0L6_2atmpS1444;
    _M0L6_2atmpS1446 = _M0L4tlenS507 + 2;
    _M0L11_2aparam__0S483 = _M0L6_2atmpS1446;
    _M0L11_2aparam__1S484 = _M0L4restS508;
    continue;
    joinlet_2299:;
    goto joinlet_2298;
    join_499:;
    _M0L6_2atmpS1441 = (int32_t)_M0L2b0S501;
    _M0L6_2atmpS1440 = _M0L6_2atmpS1441 & 15;
    _M0L6_2atmpS1436 = _M0L6_2atmpS1440 << 12;
    _M0L6_2atmpS1439 = (int32_t)_M0L2b1S502;
    _M0L6_2atmpS1438 = _M0L6_2atmpS1439 & 63;
    _M0L6_2atmpS1437 = _M0L6_2atmpS1438 << 6;
    _M0L6_2atmpS1433 = _M0L6_2atmpS1436 | _M0L6_2atmpS1437;
    _M0L6_2atmpS1435 = (int32_t)_M0L2b2S503;
    _M0L6_2atmpS1434 = _M0L6_2atmpS1435 & 63;
    _M0L2chS505 = _M0L6_2atmpS1433 | _M0L6_2atmpS1434;
    _M0L6_2atmpS1428 = _M0L2chS505 & 0xff;
    _M0L1tS481[_M0L4tlenS500] = _M0L6_2atmpS1428;
    _M0L6_2atmpS1429 = _M0L4tlenS500 + 1;
    _M0L6_2atmpS1431 = _M0L2chS505 >> 8;
    _M0L6_2atmpS1430 = _M0L6_2atmpS1431 & 0xff;
    _M0L1tS481[_M0L6_2atmpS1429] = _M0L6_2atmpS1430;
    _M0L6_2atmpS1432 = _M0L4tlenS500 + 2;
    _M0L11_2aparam__0S483 = _M0L6_2atmpS1432;
    _M0L11_2aparam__1S484 = _M0L4restS504;
    continue;
    joinlet_2298:;
    goto joinlet_2297;
    join_487:;
    _M0L6_2atmpS1427 = (int32_t)_M0L2b0S489;
    _M0L6_2atmpS1426 = _M0L6_2atmpS1427 & 7;
    _M0L6_2atmpS1422 = _M0L6_2atmpS1426 << 18;
    _M0L6_2atmpS1425 = (int32_t)_M0L2b1S490;
    _M0L6_2atmpS1424 = _M0L6_2atmpS1425 & 63;
    _M0L6_2atmpS1423 = _M0L6_2atmpS1424 << 12;
    _M0L6_2atmpS1418 = _M0L6_2atmpS1422 | _M0L6_2atmpS1423;
    _M0L6_2atmpS1421 = (int32_t)_M0L2b2S491;
    _M0L6_2atmpS1420 = _M0L6_2atmpS1421 & 63;
    _M0L6_2atmpS1419 = _M0L6_2atmpS1420 << 6;
    _M0L6_2atmpS1415 = _M0L6_2atmpS1418 | _M0L6_2atmpS1419;
    _M0L6_2atmpS1417 = (int32_t)_M0L2b3S492;
    _M0L6_2atmpS1416 = _M0L6_2atmpS1417 & 63;
    _M0L2chS494 = _M0L6_2atmpS1415 | _M0L6_2atmpS1416;
    _M0L3chmS495 = _M0L2chS494 - 65536;
    _M0L6_2atmpS1414 = _M0L3chmS495 >> 10;
    _M0L3ch1S496 = _M0L6_2atmpS1414 + 55296;
    _M0L6_2atmpS1413 = _M0L3chmS495 & 1023;
    _M0L3ch2S497 = _M0L6_2atmpS1413 + 56320;
    _M0L6_2atmpS1403 = _M0L3ch1S496 & 0xff;
    _M0L1tS481[_M0L4tlenS488] = _M0L6_2atmpS1403;
    _M0L6_2atmpS1404 = _M0L4tlenS488 + 1;
    _M0L6_2atmpS1406 = _M0L3ch1S496 >> 8;
    _M0L6_2atmpS1405 = _M0L6_2atmpS1406 & 0xff;
    _M0L1tS481[_M0L6_2atmpS1404] = _M0L6_2atmpS1405;
    _M0L6_2atmpS1407 = _M0L4tlenS488 + 2;
    _M0L6_2atmpS1408 = _M0L3ch2S497 & 0xff;
    _M0L1tS481[_M0L6_2atmpS1407] = _M0L6_2atmpS1408;
    _M0L6_2atmpS1409 = _M0L4tlenS488 + 3;
    _M0L6_2atmpS1411 = _M0L3ch2S497 >> 8;
    _M0L6_2atmpS1410 = _M0L6_2atmpS1411 & 0xff;
    _M0L1tS481[_M0L6_2atmpS1409] = _M0L6_2atmpS1410;
    _M0L6_2atmpS1412 = _M0L4tlenS488 + 4;
    _M0L11_2aparam__0S483 = _M0L6_2atmpS1412;
    _M0L11_2aparam__1S484 = _M0L4restS493;
    continue;
    joinlet_2297:;
    goto joinlet_2296;
    join_485:;
    _M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed));
    Moonbit_object_header(_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed, $0_0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed*)_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402)->$0_0
    = _M0L5bytesS486.$0;
    ((struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed*)_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402)->$0_1
    = _M0L5bytesS486.$1;
    ((struct _M0DTPC15error5Error60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformed*)_M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402)->$0_2
    = _M0L5bytesS486.$2;
    _result_2301.tag = 0;
    _result_2301.data.err
    = _M0L60moonbitlang_2fcore_2fencoding_2futf8_2eMalformed_2eMalformedS1402;
    return _result_2301;
    joinlet_2296:;
    break;
  }
  _M0L6_2atmpS1400 = _M0L1tS481;
  _M0L6_2atmpS1401 = (int64_t)_M0L4tlenS482;
  #line 122 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS1399
  = _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1400, 0, _M0L6_2atmpS1401);
  _result_2302.tag = 1;
  _result_2302.data.ok = _M0L6_2atmpS1399;
  return _result_2302;
}

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView _M0L3strS471,
  int32_t _M0L3bomS472
) {
  int32_t _M0L6_2atmpS1398;
  int32_t _M0L6_2atmpS1397;
  struct _M0TPC16buffer6Buffer* _M0L6bufferS470;
  #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  moonbit_incref(_M0L3strS471.$0);
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0L6_2atmpS1398 = _M0MPC16string10StringView6length(_M0L3strS471);
  _M0L6_2atmpS1397 = _M0L6_2atmpS1398 * 4;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0L6bufferS470 = _M0FPC16buffer11new_2einner(_M0L6_2atmpS1397);
  if (_M0L3bomS472 == 1) {
    moonbit_incref(_M0L6bufferS470);
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
    _M0MPC16buffer6Buffer17write__char__utf8(_M0L6bufferS470, 65279);
  }
  moonbit_incref(_M0L6bufferS470);
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0MPC16buffer6Buffer19write__string__utf8(_M0L6bufferS470, _M0L3strS471);
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  return _M0MPC16buffer6Buffer9to__bytes(_M0L6bufferS470);
}

struct _M0TPB5ArrayGsE* _M0FPC13env4args() {
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env.mbt"
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env.mbt"
  return _M0FPC13env24get__cli__args__internal();
}

struct _M0TPB5ArrayGsE* _M0FPC13env24get__cli__args__internal() {
  moonbit_bytes_t* _M0L3tmpS465;
  int32_t _M0L6_2atmpS1396;
  struct _M0TPB5ArrayGsE* _M0L3resS466;
  int32_t _M0L7_2abindS467;
  int32_t _M0L1iS468;
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  _M0L3tmpS465 = _M0FPC13env19get__cli__args__ffi();
  _M0L6_2atmpS1396 = Moonbit_array_length(_M0L3tmpS465);
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  _M0L3resS466 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1396);
  _M0L7_2abindS467 = Moonbit_array_length(_M0L3tmpS465);
  _M0L1iS468 = 0;
  while (1) {
    if (_M0L1iS468 < _M0L7_2abindS467) {
      moonbit_bytes_t _M0L6_2atmpS2103;
      moonbit_bytes_t _M0L6_2atmpS1394;
      moonbit_string_t _M0L6_2atmpS1393;
      int32_t _M0L6_2atmpS1395;
      if (_M0L1iS468 < 0 || _M0L1iS468 >= Moonbit_array_length(_M0L3tmpS465)) {
        #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2103 = (moonbit_bytes_t)_M0L3tmpS465[_M0L1iS468];
      _M0L6_2atmpS1394 = _M0L6_2atmpS2103;
      moonbit_incref(_M0L6_2atmpS1394);
      #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
      _M0L6_2atmpS1393
      = _M0FPC13env28utf8__bytes__to__mbt__string(_M0L6_2atmpS1394);
      moonbit_incref(_M0L3resS466);
      #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS466, _M0L6_2atmpS1393);
      _M0L6_2atmpS1395 = _M0L1iS468 + 1;
      _M0L1iS468 = _M0L6_2atmpS1395;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS465);
    }
    break;
  }
  return _M0L3resS466;
}

moonbit_string_t _M0FPC13env28utf8__bytes__to__mbt__string(
  moonbit_bytes_t _M0L5bytesS461
) {
  struct _M0TPB13StringBuilder* _M0L3resS459;
  int32_t _M0L3lenS460;
  int32_t _M0Lm1iS462;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  _M0L3resS459 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS460 = Moonbit_array_length(_M0L5bytesS461);
  _M0Lm1iS462 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1316 = _M0Lm1iS462;
    if (_M0L6_2atmpS1316 < _M0L3lenS460) {
      int32_t _M0L6_2atmpS1392 = _M0Lm1iS462;
      int32_t _M0L6_2atmpS1391;
      int32_t _M0Lm1cS463;
      int32_t _M0L6_2atmpS1317;
      if (
        _M0L6_2atmpS1392 < 0
        || _M0L6_2atmpS1392 >= Moonbit_array_length(_M0L5bytesS461)
      ) {
        #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1391 = _M0L5bytesS461[_M0L6_2atmpS1392];
      _M0Lm1cS463 = (int32_t)_M0L6_2atmpS1391;
      _M0L6_2atmpS1317 = _M0Lm1cS463;
      if (_M0L6_2atmpS1317 == 0) {
        moonbit_decref(_M0L5bytesS461);
        break;
      } else {
        int32_t _M0L6_2atmpS1318 = _M0Lm1cS463;
        if (_M0L6_2atmpS1318 < 128) {
          int32_t _M0L6_2atmpS1320 = _M0Lm1cS463;
          int32_t _M0L6_2atmpS1319 = _M0L6_2atmpS1320;
          int32_t _M0L6_2atmpS1321;
          moonbit_incref(_M0L3resS459);
          #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS459, _M0L6_2atmpS1319);
          _M0L6_2atmpS1321 = _M0Lm1iS462;
          _M0Lm1iS462 = _M0L6_2atmpS1321 + 1;
        } else {
          int32_t _M0L6_2atmpS1322 = _M0Lm1cS463;
          if (_M0L6_2atmpS1322 < 224) {
            int32_t _M0L6_2atmpS1324 = _M0Lm1iS462;
            int32_t _M0L6_2atmpS1323 = _M0L6_2atmpS1324 + 1;
            int32_t _M0L6_2atmpS1332;
            int32_t _M0L6_2atmpS1331;
            int32_t _M0L6_2atmpS1325;
            int32_t _M0L6_2atmpS1330;
            int32_t _M0L6_2atmpS1329;
            int32_t _M0L6_2atmpS1328;
            int32_t _M0L6_2atmpS1327;
            int32_t _M0L6_2atmpS1326;
            int32_t _M0L6_2atmpS1334;
            int32_t _M0L6_2atmpS1333;
            int32_t _M0L6_2atmpS1335;
            if (_M0L6_2atmpS1323 >= _M0L3lenS460) {
              moonbit_decref(_M0L5bytesS461);
              break;
            }
            _M0L6_2atmpS1332 = _M0Lm1cS463;
            _M0L6_2atmpS1331 = _M0L6_2atmpS1332 & 31;
            _M0L6_2atmpS1325 = _M0L6_2atmpS1331 << 6;
            _M0L6_2atmpS1330 = _M0Lm1iS462;
            _M0L6_2atmpS1329 = _M0L6_2atmpS1330 + 1;
            if (
              _M0L6_2atmpS1329 < 0
              || _M0L6_2atmpS1329 >= Moonbit_array_length(_M0L5bytesS461)
            ) {
              #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1328 = _M0L5bytesS461[_M0L6_2atmpS1329];
            _M0L6_2atmpS1327 = (int32_t)_M0L6_2atmpS1328;
            _M0L6_2atmpS1326 = _M0L6_2atmpS1327 & 63;
            _M0Lm1cS463 = _M0L6_2atmpS1325 | _M0L6_2atmpS1326;
            _M0L6_2atmpS1334 = _M0Lm1cS463;
            _M0L6_2atmpS1333 = _M0L6_2atmpS1334;
            moonbit_incref(_M0L3resS459);
            #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS459, _M0L6_2atmpS1333);
            _M0L6_2atmpS1335 = _M0Lm1iS462;
            _M0Lm1iS462 = _M0L6_2atmpS1335 + 2;
          } else {
            int32_t _M0L6_2atmpS1336 = _M0Lm1cS463;
            if (_M0L6_2atmpS1336 < 240) {
              int32_t _M0L6_2atmpS1338 = _M0Lm1iS462;
              int32_t _M0L6_2atmpS1337 = _M0L6_2atmpS1338 + 2;
              int32_t _M0L6_2atmpS1353;
              int32_t _M0L6_2atmpS1352;
              int32_t _M0L6_2atmpS1345;
              int32_t _M0L6_2atmpS1351;
              int32_t _M0L6_2atmpS1350;
              int32_t _M0L6_2atmpS1349;
              int32_t _M0L6_2atmpS1348;
              int32_t _M0L6_2atmpS1347;
              int32_t _M0L6_2atmpS1346;
              int32_t _M0L6_2atmpS1339;
              int32_t _M0L6_2atmpS1344;
              int32_t _M0L6_2atmpS1343;
              int32_t _M0L6_2atmpS1342;
              int32_t _M0L6_2atmpS1341;
              int32_t _M0L6_2atmpS1340;
              int32_t _M0L6_2atmpS1355;
              int32_t _M0L6_2atmpS1354;
              int32_t _M0L6_2atmpS1356;
              if (_M0L6_2atmpS1337 >= _M0L3lenS460) {
                moonbit_decref(_M0L5bytesS461);
                break;
              }
              _M0L6_2atmpS1353 = _M0Lm1cS463;
              _M0L6_2atmpS1352 = _M0L6_2atmpS1353 & 15;
              _M0L6_2atmpS1345 = _M0L6_2atmpS1352 << 12;
              _M0L6_2atmpS1351 = _M0Lm1iS462;
              _M0L6_2atmpS1350 = _M0L6_2atmpS1351 + 1;
              if (
                _M0L6_2atmpS1350 < 0
                || _M0L6_2atmpS1350 >= Moonbit_array_length(_M0L5bytesS461)
              ) {
                #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1349 = _M0L5bytesS461[_M0L6_2atmpS1350];
              _M0L6_2atmpS1348 = (int32_t)_M0L6_2atmpS1349;
              _M0L6_2atmpS1347 = _M0L6_2atmpS1348 & 63;
              _M0L6_2atmpS1346 = _M0L6_2atmpS1347 << 6;
              _M0L6_2atmpS1339 = _M0L6_2atmpS1345 | _M0L6_2atmpS1346;
              _M0L6_2atmpS1344 = _M0Lm1iS462;
              _M0L6_2atmpS1343 = _M0L6_2atmpS1344 + 2;
              if (
                _M0L6_2atmpS1343 < 0
                || _M0L6_2atmpS1343 >= Moonbit_array_length(_M0L5bytesS461)
              ) {
                #line 54 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1342 = _M0L5bytesS461[_M0L6_2atmpS1343];
              _M0L6_2atmpS1341 = (int32_t)_M0L6_2atmpS1342;
              _M0L6_2atmpS1340 = _M0L6_2atmpS1341 & 63;
              _M0Lm1cS463 = _M0L6_2atmpS1339 | _M0L6_2atmpS1340;
              _M0L6_2atmpS1355 = _M0Lm1cS463;
              _M0L6_2atmpS1354 = _M0L6_2atmpS1355;
              moonbit_incref(_M0L3resS459);
              #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS459, _M0L6_2atmpS1354);
              _M0L6_2atmpS1356 = _M0Lm1iS462;
              _M0Lm1iS462 = _M0L6_2atmpS1356 + 3;
            } else {
              int32_t _M0L6_2atmpS1358 = _M0Lm1iS462;
              int32_t _M0L6_2atmpS1357 = _M0L6_2atmpS1358 + 3;
              int32_t _M0L6_2atmpS1380;
              int32_t _M0L6_2atmpS1379;
              int32_t _M0L6_2atmpS1372;
              int32_t _M0L6_2atmpS1378;
              int32_t _M0L6_2atmpS1377;
              int32_t _M0L6_2atmpS1376;
              int32_t _M0L6_2atmpS1375;
              int32_t _M0L6_2atmpS1374;
              int32_t _M0L6_2atmpS1373;
              int32_t _M0L6_2atmpS1365;
              int32_t _M0L6_2atmpS1371;
              int32_t _M0L6_2atmpS1370;
              int32_t _M0L6_2atmpS1369;
              int32_t _M0L6_2atmpS1368;
              int32_t _M0L6_2atmpS1367;
              int32_t _M0L6_2atmpS1366;
              int32_t _M0L6_2atmpS1359;
              int32_t _M0L6_2atmpS1364;
              int32_t _M0L6_2atmpS1363;
              int32_t _M0L6_2atmpS1362;
              int32_t _M0L6_2atmpS1361;
              int32_t _M0L6_2atmpS1360;
              int32_t _M0L6_2atmpS1381;
              int32_t _M0L6_2atmpS1385;
              int32_t _M0L6_2atmpS1384;
              int32_t _M0L6_2atmpS1383;
              int32_t _M0L6_2atmpS1382;
              int32_t _M0L6_2atmpS1389;
              int32_t _M0L6_2atmpS1388;
              int32_t _M0L6_2atmpS1387;
              int32_t _M0L6_2atmpS1386;
              int32_t _M0L6_2atmpS1390;
              if (_M0L6_2atmpS1357 >= _M0L3lenS460) {
                moonbit_decref(_M0L5bytesS461);
                break;
              }
              _M0L6_2atmpS1380 = _M0Lm1cS463;
              _M0L6_2atmpS1379 = _M0L6_2atmpS1380 & 7;
              _M0L6_2atmpS1372 = _M0L6_2atmpS1379 << 18;
              _M0L6_2atmpS1378 = _M0Lm1iS462;
              _M0L6_2atmpS1377 = _M0L6_2atmpS1378 + 1;
              if (
                _M0L6_2atmpS1377 < 0
                || _M0L6_2atmpS1377 >= Moonbit_array_length(_M0L5bytesS461)
              ) {
                #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1376 = _M0L5bytesS461[_M0L6_2atmpS1377];
              _M0L6_2atmpS1375 = (int32_t)_M0L6_2atmpS1376;
              _M0L6_2atmpS1374 = _M0L6_2atmpS1375 & 63;
              _M0L6_2atmpS1373 = _M0L6_2atmpS1374 << 12;
              _M0L6_2atmpS1365 = _M0L6_2atmpS1372 | _M0L6_2atmpS1373;
              _M0L6_2atmpS1371 = _M0Lm1iS462;
              _M0L6_2atmpS1370 = _M0L6_2atmpS1371 + 2;
              if (
                _M0L6_2atmpS1370 < 0
                || _M0L6_2atmpS1370 >= Moonbit_array_length(_M0L5bytesS461)
              ) {
                #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1369 = _M0L5bytesS461[_M0L6_2atmpS1370];
              _M0L6_2atmpS1368 = (int32_t)_M0L6_2atmpS1369;
              _M0L6_2atmpS1367 = _M0L6_2atmpS1368 & 63;
              _M0L6_2atmpS1366 = _M0L6_2atmpS1367 << 6;
              _M0L6_2atmpS1359 = _M0L6_2atmpS1365 | _M0L6_2atmpS1366;
              _M0L6_2atmpS1364 = _M0Lm1iS462;
              _M0L6_2atmpS1363 = _M0L6_2atmpS1364 + 3;
              if (
                _M0L6_2atmpS1363 < 0
                || _M0L6_2atmpS1363 >= Moonbit_array_length(_M0L5bytesS461)
              ) {
                #line 64 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
                moonbit_panic();
              }
              _M0L6_2atmpS1362 = _M0L5bytesS461[_M0L6_2atmpS1363];
              _M0L6_2atmpS1361 = (int32_t)_M0L6_2atmpS1362;
              _M0L6_2atmpS1360 = _M0L6_2atmpS1361 & 63;
              _M0Lm1cS463 = _M0L6_2atmpS1359 | _M0L6_2atmpS1360;
              _M0L6_2atmpS1381 = _M0Lm1cS463;
              _M0Lm1cS463 = _M0L6_2atmpS1381 - 65536;
              _M0L6_2atmpS1385 = _M0Lm1cS463;
              _M0L6_2atmpS1384 = _M0L6_2atmpS1385 >> 10;
              _M0L6_2atmpS1383 = _M0L6_2atmpS1384 + 55296;
              _M0L6_2atmpS1382 = _M0L6_2atmpS1383;
              moonbit_incref(_M0L3resS459);
              #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS459, _M0L6_2atmpS1382);
              _M0L6_2atmpS1389 = _M0Lm1cS463;
              _M0L6_2atmpS1388 = _M0L6_2atmpS1389 & 1023;
              _M0L6_2atmpS1387 = _M0L6_2atmpS1388 + 56320;
              _M0L6_2atmpS1386 = _M0L6_2atmpS1387;
              moonbit_incref(_M0L3resS459);
              #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS459, _M0L6_2atmpS1386);
              _M0L6_2atmpS1390 = _M0Lm1iS462;
              _M0Lm1iS462 = _M0L6_2atmpS1390 + 4;
            }
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L5bytesS461);
    }
    break;
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\env\\env_native.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS459);
}

moonbit_bytes_t _M0MPC16buffer6Buffer9to__bytes(
  struct _M0TPC16buffer6Buffer* _M0L4selfS458
) {
  moonbit_bytes_t _M0L8_2afieldS2105;
  moonbit_bytes_t _M0L4dataS1313;
  int32_t _M0L8_2afieldS2104;
  int32_t _M0L6_2acntS2231;
  int32_t _M0L3lenS1315;
  int64_t _M0L6_2atmpS1314;
  struct _M0TPB9ArrayViewGyE _M0L6_2atmpS1312;
  #line 1112 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS2105 = _M0L4selfS458->$0;
  _M0L4dataS1313 = _M0L8_2afieldS2105;
  _M0L8_2afieldS2104 = _M0L4selfS458->$1;
  _M0L6_2acntS2231 = Moonbit_object_header(_M0L4selfS458)->rc;
  if (_M0L6_2acntS2231 > 1) {
    int32_t _M0L11_2anew__cntS2232 = _M0L6_2acntS2231 - 1;
    Moonbit_object_header(_M0L4selfS458)->rc = _M0L11_2anew__cntS2232;
    moonbit_incref(_M0L4dataS1313);
  } else if (_M0L6_2acntS2231 == 1) {
    #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    moonbit_free(_M0L4selfS458);
  }
  _M0L3lenS1315 = _M0L8_2afieldS2104;
  _M0L6_2atmpS1314 = (int64_t)_M0L3lenS1315;
  #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS1312
  = _M0MPC15array10FixedArray12view_2einnerGyE(_M0L4dataS1313, 0, _M0L6_2atmpS1314);
  #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  return _M0MPC15bytes5Bytes11from__array(_M0L6_2atmpS1312);
}

int32_t _M0MPC16buffer6Buffer19write__string__utf8(
  struct _M0TPC16buffer6Buffer* _M0L3bufS456,
  struct _M0TPC16string10StringView _M0L6stringS452
) {
  struct _M0TWEOc* _M0L5_2aitS451;
  #line 880 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  #line 880 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L5_2aitS451 = _M0MPC16string10StringView4iter(_M0L6stringS452);
  while (1) {
    int32_t _M0L7_2abindS453;
    moonbit_incref(_M0L5_2aitS451);
    #line 881 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L7_2abindS453 = _M0MPB4Iter4nextGcE(_M0L5_2aitS451);
    if (_M0L7_2abindS453 == -1) {
      moonbit_decref(_M0L3bufS456);
      moonbit_decref(_M0L5_2aitS451);
    } else {
      int32_t _M0L7_2aSomeS454 = _M0L7_2abindS453;
      int32_t _M0L5_2achS455 = _M0L7_2aSomeS454;
      moonbit_incref(_M0L3bufS456);
      #line 882 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      _M0MPC16buffer6Buffer17write__char__utf8(_M0L3bufS456, _M0L5_2achS455);
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16buffer6Buffer17write__char__utf8(
  struct _M0TPC16buffer6Buffer* _M0L3bufS450,
  int32_t _M0L5valueS449
) {
  uint32_t _M0L4codeS448;
  #line 782 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  #line 783 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L4codeS448 = _M0MPC14char4Char8to__uint(_M0L5valueS449);
  if (_M0L4codeS448 < 128u) {
    int32_t _M0L3lenS1304 = _M0L3bufS450->$1;
    int32_t _M0L6_2atmpS1303 = _M0L3lenS1304 + 1;
    moonbit_bytes_t _M0L8_2afieldS2106;
    moonbit_bytes_t _M0L4dataS1305;
    int32_t _M0L3lenS1306;
    uint32_t _M0L6_2atmpS1309;
    uint32_t _M0L6_2atmpS1308;
    int32_t _M0L6_2atmpS1307;
    int32_t _M0L3lenS1311;
    int32_t _M0L6_2atmpS1310;
    moonbit_incref(_M0L3bufS450);
    #line 786 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS450, _M0L6_2atmpS1303);
    _M0L8_2afieldS2106 = _M0L3bufS450->$0;
    _M0L4dataS1305 = _M0L8_2afieldS2106;
    _M0L3lenS1306 = _M0L3bufS450->$1;
    _M0L6_2atmpS1309 = _M0L4codeS448 & 127u;
    _M0L6_2atmpS1308 = _M0L6_2atmpS1309 | 0u;
    moonbit_incref(_M0L4dataS1305);
    #line 787 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1307 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1308);
    if (
      _M0L3lenS1306 < 0
      || _M0L3lenS1306 >= Moonbit_array_length(_M0L4dataS1305)
    ) {
      #line 787 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1305[_M0L3lenS1306] = _M0L6_2atmpS1307;
    moonbit_decref(_M0L4dataS1305);
    _M0L3lenS1311 = _M0L3bufS450->$1;
    _M0L6_2atmpS1310 = _M0L3lenS1311 + 1;
    _M0L3bufS450->$1 = _M0L6_2atmpS1310;
    moonbit_decref(_M0L3bufS450);
  } else if (_M0L4codeS448 < 2048u) {
    int32_t _M0L3lenS1288 = _M0L3bufS450->$1;
    int32_t _M0L6_2atmpS1287 = _M0L3lenS1288 + 2;
    moonbit_bytes_t _M0L8_2afieldS2108;
    moonbit_bytes_t _M0L4dataS1289;
    int32_t _M0L3lenS1290;
    uint32_t _M0L6_2atmpS1294;
    uint32_t _M0L6_2atmpS1293;
    uint32_t _M0L6_2atmpS1292;
    int32_t _M0L6_2atmpS1291;
    moonbit_bytes_t _M0L8_2afieldS2107;
    moonbit_bytes_t _M0L4dataS1295;
    int32_t _M0L3lenS1300;
    int32_t _M0L6_2atmpS1296;
    uint32_t _M0L6_2atmpS1299;
    uint32_t _M0L6_2atmpS1298;
    int32_t _M0L6_2atmpS1297;
    int32_t _M0L3lenS1302;
    int32_t _M0L6_2atmpS1301;
    moonbit_incref(_M0L3bufS450);
    #line 791 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS450, _M0L6_2atmpS1287);
    _M0L8_2afieldS2108 = _M0L3bufS450->$0;
    _M0L4dataS1289 = _M0L8_2afieldS2108;
    _M0L3lenS1290 = _M0L3bufS450->$1;
    _M0L6_2atmpS1294 = _M0L4codeS448 >> 6;
    _M0L6_2atmpS1293 = _M0L6_2atmpS1294 & 31u;
    _M0L6_2atmpS1292 = _M0L6_2atmpS1293 | 192u;
    moonbit_incref(_M0L4dataS1289);
    #line 792 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1291 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1292);
    if (
      _M0L3lenS1290 < 0
      || _M0L3lenS1290 >= Moonbit_array_length(_M0L4dataS1289)
    ) {
      #line 792 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1289[_M0L3lenS1290] = _M0L6_2atmpS1291;
    moonbit_decref(_M0L4dataS1289);
    _M0L8_2afieldS2107 = _M0L3bufS450->$0;
    _M0L4dataS1295 = _M0L8_2afieldS2107;
    _M0L3lenS1300 = _M0L3bufS450->$1;
    _M0L6_2atmpS1296 = _M0L3lenS1300 + 1;
    _M0L6_2atmpS1299 = _M0L4codeS448 & 63u;
    _M0L6_2atmpS1298 = _M0L6_2atmpS1299 | 128u;
    moonbit_incref(_M0L4dataS1295);
    #line 793 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1297 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1298);
    if (
      _M0L6_2atmpS1296 < 0
      || _M0L6_2atmpS1296 >= Moonbit_array_length(_M0L4dataS1295)
    ) {
      #line 793 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1295[_M0L6_2atmpS1296] = _M0L6_2atmpS1297;
    moonbit_decref(_M0L4dataS1295);
    _M0L3lenS1302 = _M0L3bufS450->$1;
    _M0L6_2atmpS1301 = _M0L3lenS1302 + 2;
    _M0L3bufS450->$1 = _M0L6_2atmpS1301;
    moonbit_decref(_M0L3bufS450);
  } else if (_M0L4codeS448 < 65536u) {
    int32_t _M0L3lenS1265 = _M0L3bufS450->$1;
    int32_t _M0L6_2atmpS1264 = _M0L3lenS1265 + 3;
    moonbit_bytes_t _M0L8_2afieldS2111;
    moonbit_bytes_t _M0L4dataS1266;
    int32_t _M0L3lenS1267;
    uint32_t _M0L6_2atmpS1271;
    uint32_t _M0L6_2atmpS1270;
    uint32_t _M0L6_2atmpS1269;
    int32_t _M0L6_2atmpS1268;
    moonbit_bytes_t _M0L8_2afieldS2110;
    moonbit_bytes_t _M0L4dataS1272;
    int32_t _M0L3lenS1278;
    int32_t _M0L6_2atmpS1273;
    uint32_t _M0L6_2atmpS1277;
    uint32_t _M0L6_2atmpS1276;
    uint32_t _M0L6_2atmpS1275;
    int32_t _M0L6_2atmpS1274;
    moonbit_bytes_t _M0L8_2afieldS2109;
    moonbit_bytes_t _M0L4dataS1279;
    int32_t _M0L3lenS1284;
    int32_t _M0L6_2atmpS1280;
    uint32_t _M0L6_2atmpS1283;
    uint32_t _M0L6_2atmpS1282;
    int32_t _M0L6_2atmpS1281;
    int32_t _M0L3lenS1286;
    int32_t _M0L6_2atmpS1285;
    moonbit_incref(_M0L3bufS450);
    #line 797 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS450, _M0L6_2atmpS1264);
    _M0L8_2afieldS2111 = _M0L3bufS450->$0;
    _M0L4dataS1266 = _M0L8_2afieldS2111;
    _M0L3lenS1267 = _M0L3bufS450->$1;
    _M0L6_2atmpS1271 = _M0L4codeS448 >> 12;
    _M0L6_2atmpS1270 = _M0L6_2atmpS1271 & 15u;
    _M0L6_2atmpS1269 = _M0L6_2atmpS1270 | 224u;
    moonbit_incref(_M0L4dataS1266);
    #line 798 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1268 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1269);
    if (
      _M0L3lenS1267 < 0
      || _M0L3lenS1267 >= Moonbit_array_length(_M0L4dataS1266)
    ) {
      #line 798 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1266[_M0L3lenS1267] = _M0L6_2atmpS1268;
    moonbit_decref(_M0L4dataS1266);
    _M0L8_2afieldS2110 = _M0L3bufS450->$0;
    _M0L4dataS1272 = _M0L8_2afieldS2110;
    _M0L3lenS1278 = _M0L3bufS450->$1;
    _M0L6_2atmpS1273 = _M0L3lenS1278 + 1;
    _M0L6_2atmpS1277 = _M0L4codeS448 >> 6;
    _M0L6_2atmpS1276 = _M0L6_2atmpS1277 & 63u;
    _M0L6_2atmpS1275 = _M0L6_2atmpS1276 | 128u;
    moonbit_incref(_M0L4dataS1272);
    #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1274 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1275);
    if (
      _M0L6_2atmpS1273 < 0
      || _M0L6_2atmpS1273 >= Moonbit_array_length(_M0L4dataS1272)
    ) {
      #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1272[_M0L6_2atmpS1273] = _M0L6_2atmpS1274;
    moonbit_decref(_M0L4dataS1272);
    _M0L8_2afieldS2109 = _M0L3bufS450->$0;
    _M0L4dataS1279 = _M0L8_2afieldS2109;
    _M0L3lenS1284 = _M0L3bufS450->$1;
    _M0L6_2atmpS1280 = _M0L3lenS1284 + 2;
    _M0L6_2atmpS1283 = _M0L4codeS448 & 63u;
    _M0L6_2atmpS1282 = _M0L6_2atmpS1283 | 128u;
    moonbit_incref(_M0L4dataS1279);
    #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1281 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1282);
    if (
      _M0L6_2atmpS1280 < 0
      || _M0L6_2atmpS1280 >= Moonbit_array_length(_M0L4dataS1279)
    ) {
      #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1279[_M0L6_2atmpS1280] = _M0L6_2atmpS1281;
    moonbit_decref(_M0L4dataS1279);
    _M0L3lenS1286 = _M0L3bufS450->$1;
    _M0L6_2atmpS1285 = _M0L3lenS1286 + 3;
    _M0L3bufS450->$1 = _M0L6_2atmpS1285;
    moonbit_decref(_M0L3bufS450);
  } else if (_M0L4codeS448 < 1114112u) {
    int32_t _M0L3lenS1235 = _M0L3bufS450->$1;
    int32_t _M0L6_2atmpS1234 = _M0L3lenS1235 + 4;
    moonbit_bytes_t _M0L8_2afieldS2115;
    moonbit_bytes_t _M0L4dataS1236;
    int32_t _M0L3lenS1237;
    uint32_t _M0L6_2atmpS1241;
    uint32_t _M0L6_2atmpS1240;
    uint32_t _M0L6_2atmpS1239;
    int32_t _M0L6_2atmpS1238;
    moonbit_bytes_t _M0L8_2afieldS2114;
    moonbit_bytes_t _M0L4dataS1242;
    int32_t _M0L3lenS1248;
    int32_t _M0L6_2atmpS1243;
    uint32_t _M0L6_2atmpS1247;
    uint32_t _M0L6_2atmpS1246;
    uint32_t _M0L6_2atmpS1245;
    int32_t _M0L6_2atmpS1244;
    moonbit_bytes_t _M0L8_2afieldS2113;
    moonbit_bytes_t _M0L4dataS1249;
    int32_t _M0L3lenS1255;
    int32_t _M0L6_2atmpS1250;
    uint32_t _M0L6_2atmpS1254;
    uint32_t _M0L6_2atmpS1253;
    uint32_t _M0L6_2atmpS1252;
    int32_t _M0L6_2atmpS1251;
    moonbit_bytes_t _M0L8_2afieldS2112;
    moonbit_bytes_t _M0L4dataS1256;
    int32_t _M0L3lenS1261;
    int32_t _M0L6_2atmpS1257;
    uint32_t _M0L6_2atmpS1260;
    uint32_t _M0L6_2atmpS1259;
    int32_t _M0L6_2atmpS1258;
    int32_t _M0L3lenS1263;
    int32_t _M0L6_2atmpS1262;
    moonbit_incref(_M0L3bufS450);
    #line 804 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS450, _M0L6_2atmpS1234);
    _M0L8_2afieldS2115 = _M0L3bufS450->$0;
    _M0L4dataS1236 = _M0L8_2afieldS2115;
    _M0L3lenS1237 = _M0L3bufS450->$1;
    _M0L6_2atmpS1241 = _M0L4codeS448 >> 18;
    _M0L6_2atmpS1240 = _M0L6_2atmpS1241 & 7u;
    _M0L6_2atmpS1239 = _M0L6_2atmpS1240 | 240u;
    moonbit_incref(_M0L4dataS1236);
    #line 805 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1238 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1239);
    if (
      _M0L3lenS1237 < 0
      || _M0L3lenS1237 >= Moonbit_array_length(_M0L4dataS1236)
    ) {
      #line 805 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1236[_M0L3lenS1237] = _M0L6_2atmpS1238;
    moonbit_decref(_M0L4dataS1236);
    _M0L8_2afieldS2114 = _M0L3bufS450->$0;
    _M0L4dataS1242 = _M0L8_2afieldS2114;
    _M0L3lenS1248 = _M0L3bufS450->$1;
    _M0L6_2atmpS1243 = _M0L3lenS1248 + 1;
    _M0L6_2atmpS1247 = _M0L4codeS448 >> 12;
    _M0L6_2atmpS1246 = _M0L6_2atmpS1247 & 63u;
    _M0L6_2atmpS1245 = _M0L6_2atmpS1246 | 128u;
    moonbit_incref(_M0L4dataS1242);
    #line 806 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1244 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1245);
    if (
      _M0L6_2atmpS1243 < 0
      || _M0L6_2atmpS1243 >= Moonbit_array_length(_M0L4dataS1242)
    ) {
      #line 806 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1242[_M0L6_2atmpS1243] = _M0L6_2atmpS1244;
    moonbit_decref(_M0L4dataS1242);
    _M0L8_2afieldS2113 = _M0L3bufS450->$0;
    _M0L4dataS1249 = _M0L8_2afieldS2113;
    _M0L3lenS1255 = _M0L3bufS450->$1;
    _M0L6_2atmpS1250 = _M0L3lenS1255 + 2;
    _M0L6_2atmpS1254 = _M0L4codeS448 >> 6;
    _M0L6_2atmpS1253 = _M0L6_2atmpS1254 & 63u;
    _M0L6_2atmpS1252 = _M0L6_2atmpS1253 | 128u;
    moonbit_incref(_M0L4dataS1249);
    #line 807 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1251 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1252);
    if (
      _M0L6_2atmpS1250 < 0
      || _M0L6_2atmpS1250 >= Moonbit_array_length(_M0L4dataS1249)
    ) {
      #line 807 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1249[_M0L6_2atmpS1250] = _M0L6_2atmpS1251;
    moonbit_decref(_M0L4dataS1249);
    _M0L8_2afieldS2112 = _M0L3bufS450->$0;
    _M0L4dataS1256 = _M0L8_2afieldS2112;
    _M0L3lenS1261 = _M0L3bufS450->$1;
    _M0L6_2atmpS1257 = _M0L3lenS1261 + 3;
    _M0L6_2atmpS1260 = _M0L4codeS448 & 63u;
    _M0L6_2atmpS1259 = _M0L6_2atmpS1260 | 128u;
    moonbit_incref(_M0L4dataS1256);
    #line 808 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1258 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1259);
    if (
      _M0L6_2atmpS1257 < 0
      || _M0L6_2atmpS1257 >= Moonbit_array_length(_M0L4dataS1256)
    ) {
      #line 808 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS1256[_M0L6_2atmpS1257] = _M0L6_2atmpS1258;
    moonbit_decref(_M0L4dataS1256);
    _M0L3lenS1263 = _M0L3bufS450->$1;
    _M0L6_2atmpS1262 = _M0L3lenS1263 + 4;
    _M0L3bufS450->$1 = _M0L6_2atmpS1262;
    moonbit_decref(_M0L3bufS450);
  } else {
    moonbit_decref(_M0L3bufS450);
    #line 811 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_10.data);
  }
  return 0;
}

struct _M0TPC16buffer6Buffer* _M0FPC16buffer11new_2einner(
  int32_t _M0L10size__hintS446
) {
  int32_t _M0L7initialS445;
  int32_t _M0L6_2atmpS1233;
  moonbit_bytes_t _M0L4dataS447;
  struct _M0TPC16buffer6Buffer* _block_2306;
  #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  if (_M0L10size__hintS446 < 1) {
    _M0L7initialS445 = 1;
  } else {
    _M0L7initialS445 = _M0L10size__hintS446;
  }
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS1233 = _M0IPC14byte4BytePB7Default7default();
  _M0L4dataS447
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS445, _M0L6_2atmpS1233);
  _block_2306
  = (struct _M0TPC16buffer6Buffer*)moonbit_malloc(sizeof(struct _M0TPC16buffer6Buffer));
  Moonbit_object_header(_block_2306)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC16buffer6Buffer, $0) >> 2, 1, 0);
  _block_2306->$0 = _M0L4dataS447;
  _block_2306->$1 = 0;
  return _block_2306;
}

int32_t _M0MPC16buffer6Buffer19grow__if__necessary(
  struct _M0TPC16buffer6Buffer* _M0L4selfS439,
  int32_t _M0L8requiredS442
) {
  moonbit_bytes_t _M0L8_2afieldS2123;
  moonbit_bytes_t _M0L4dataS1231;
  int32_t _M0L6_2atmpS2122;
  int32_t _M0L6_2atmpS1230;
  int32_t _M0L5startS438;
  int32_t _M0L13enough__spaceS440;
  int32_t _M0L5spaceS441;
  moonbit_bytes_t _M0L8_2afieldS2119;
  moonbit_bytes_t _M0L4dataS1225;
  int32_t _M0L6_2atmpS2118;
  int32_t _M0L6_2atmpS1224;
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS2123 = _M0L4selfS439->$0;
  _M0L4dataS1231 = _M0L8_2afieldS2123;
  _M0L6_2atmpS2122 = Moonbit_array_length(_M0L4dataS1231);
  _M0L6_2atmpS1230 = _M0L6_2atmpS2122;
  if (_M0L6_2atmpS1230 <= 0) {
    _M0L5startS438 = 1;
  } else {
    moonbit_bytes_t _M0L8_2afieldS2121 = _M0L4selfS439->$0;
    moonbit_bytes_t _M0L4dataS1232 = _M0L8_2afieldS2121;
    int32_t _M0L6_2atmpS2120 = Moonbit_array_length(_M0L4dataS1232);
    _M0L5startS438 = _M0L6_2atmpS2120;
  }
  _M0L5spaceS441 = _M0L5startS438;
  while (1) {
    int32_t _M0L6_2atmpS1229;
    if (_M0L5spaceS441 >= _M0L8requiredS442) {
      _M0L13enough__spaceS440 = _M0L5spaceS441;
      break;
    }
    _M0L6_2atmpS1229 = _M0L5spaceS441 * 2;
    _M0L5spaceS441 = _M0L6_2atmpS1229;
    continue;
    break;
  }
  _M0L8_2afieldS2119 = _M0L4selfS439->$0;
  _M0L4dataS1225 = _M0L8_2afieldS2119;
  _M0L6_2atmpS2118 = Moonbit_array_length(_M0L4dataS1225);
  _M0L6_2atmpS1224 = _M0L6_2atmpS2118;
  if (_M0L13enough__spaceS440 != _M0L6_2atmpS1224) {
    int32_t _M0L6_2atmpS1228;
    moonbit_bytes_t _M0L9new__dataS444;
    moonbit_bytes_t _M0L8_2afieldS2117;
    moonbit_bytes_t _M0L4dataS1226;
    int32_t _M0L3lenS1227;
    moonbit_bytes_t _M0L6_2aoldS2116;
    #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS1228 = _M0IPC14byte4BytePB7Default7default();
    _M0L9new__dataS444
    = (moonbit_bytes_t)moonbit_make_bytes(_M0L13enough__spaceS440, _M0L6_2atmpS1228);
    _M0L8_2afieldS2117 = _M0L4selfS439->$0;
    _M0L4dataS1226 = _M0L8_2afieldS2117;
    _M0L3lenS1227 = _M0L4selfS439->$1;
    moonbit_incref(_M0L4dataS1226);
    moonbit_incref(_M0L9new__dataS444);
    #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS444, 0, _M0L4dataS1226, 0, _M0L3lenS1227);
    _M0L6_2aoldS2116 = _M0L4selfS439->$0;
    moonbit_decref(_M0L6_2aoldS2116);
    _M0L4selfS439->$0 = _M0L9new__dataS444;
    moonbit_decref(_M0L4selfS439);
  } else {
    moonbit_decref(_M0L4selfS439);
  }
  return 0;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS436,
  struct _M0TPB6Logger _M0L6loggerS437
) {
  moonbit_string_t _M0L6_2atmpS1223;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1222;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1223 = _M0L4selfS436;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1222 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1223);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1222, _M0L6loggerS437);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS413,
  struct _M0TPB6Logger _M0L6loggerS435
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2132;
  struct _M0TPC16string10StringView _M0L3pkgS412;
  moonbit_string_t _M0L7_2adataS414;
  int32_t _M0L8_2astartS415;
  int32_t _M0L6_2atmpS1221;
  int32_t _M0L6_2aendS416;
  int32_t _M0Lm9_2acursorS417;
  int32_t _M0Lm13accept__stateS418;
  int32_t _M0Lm10match__endS419;
  int32_t _M0Lm20match__tag__saver__0S420;
  int32_t _M0Lm6tag__0S421;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS422;
  struct _M0TPC16string10StringView _M0L8_2afieldS2131;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS431;
  void* _M0L8_2afieldS2130;
  int32_t _M0L6_2acntS2233;
  void* _M0L16_2apackage__nameS432;
  struct _M0TPC16string10StringView _M0L8_2afieldS2128;
  struct _M0TPC16string10StringView _M0L8filenameS1198;
  struct _M0TPC16string10StringView _M0L8_2afieldS2127;
  struct _M0TPC16string10StringView _M0L11start__lineS1199;
  struct _M0TPC16string10StringView _M0L8_2afieldS2126;
  struct _M0TPC16string10StringView _M0L13start__columnS1200;
  struct _M0TPC16string10StringView _M0L8_2afieldS2125;
  struct _M0TPC16string10StringView _M0L9end__lineS1201;
  struct _M0TPC16string10StringView _M0L8_2afieldS2124;
  int32_t _M0L6_2acntS2237;
  struct _M0TPC16string10StringView _M0L11end__columnS1202;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2132
  = (struct _M0TPC16string10StringView){
    _M0L4selfS413->$0_1, _M0L4selfS413->$0_2, _M0L4selfS413->$0_0
  };
  _M0L3pkgS412 = _M0L8_2afieldS2132;
  moonbit_incref(_M0L3pkgS412.$0);
  moonbit_incref(_M0L3pkgS412.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS414 = _M0MPC16string10StringView4data(_M0L3pkgS412);
  moonbit_incref(_M0L3pkgS412.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS415 = _M0MPC16string10StringView13start__offset(_M0L3pkgS412);
  moonbit_incref(_M0L3pkgS412.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1221 = _M0MPC16string10StringView6length(_M0L3pkgS412);
  _M0L6_2aendS416 = _M0L8_2astartS415 + _M0L6_2atmpS1221;
  _M0Lm9_2acursorS417 = _M0L8_2astartS415;
  _M0Lm13accept__stateS418 = -1;
  _M0Lm10match__endS419 = -1;
  _M0Lm20match__tag__saver__0S420 = -1;
  _M0Lm6tag__0S421 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1213 = _M0Lm9_2acursorS417;
    if (_M0L6_2atmpS1213 < _M0L6_2aendS416) {
      int32_t _M0L6_2atmpS1220 = _M0Lm9_2acursorS417;
      int32_t _M0L10next__charS426;
      int32_t _M0L6_2atmpS1214;
      moonbit_incref(_M0L7_2adataS414);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS426
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS414, _M0L6_2atmpS1220);
      _M0L6_2atmpS1214 = _M0Lm9_2acursorS417;
      _M0Lm9_2acursorS417 = _M0L6_2atmpS1214 + 1;
      if (_M0L10next__charS426 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1215;
          _M0Lm6tag__0S421 = _M0Lm9_2acursorS417;
          _M0L6_2atmpS1215 = _M0Lm9_2acursorS417;
          if (_M0L6_2atmpS1215 < _M0L6_2aendS416) {
            int32_t _M0L6_2atmpS1219 = _M0Lm9_2acursorS417;
            int32_t _M0L10next__charS427;
            int32_t _M0L6_2atmpS1216;
            moonbit_incref(_M0L7_2adataS414);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS427
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS414, _M0L6_2atmpS1219);
            _M0L6_2atmpS1216 = _M0Lm9_2acursorS417;
            _M0Lm9_2acursorS417 = _M0L6_2atmpS1216 + 1;
            if (_M0L10next__charS427 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1217 = _M0Lm9_2acursorS417;
                if (_M0L6_2atmpS1217 < _M0L6_2aendS416) {
                  int32_t _M0L6_2atmpS1218 = _M0Lm9_2acursorS417;
                  _M0Lm9_2acursorS417 = _M0L6_2atmpS1218 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S420 = _M0Lm6tag__0S421;
                  _M0Lm13accept__stateS418 = 0;
                  _M0Lm10match__endS419 = _M0Lm9_2acursorS417;
                  goto join_423;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_423;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_423;
    }
    break;
  }
  goto joinlet_2308;
  join_423:;
  switch (_M0Lm13accept__stateS418) {
    case 0: {
      int32_t _M0L6_2atmpS1211;
      int32_t _M0L6_2atmpS1210;
      int64_t _M0L6_2atmpS1207;
      int32_t _M0L6_2atmpS1209;
      int64_t _M0L6_2atmpS1208;
      struct _M0TPC16string10StringView _M0L13package__nameS424;
      int64_t _M0L6_2atmpS1204;
      int32_t _M0L6_2atmpS1206;
      int64_t _M0L6_2atmpS1205;
      struct _M0TPC16string10StringView _M0L12module__nameS425;
      void* _M0L4SomeS1203;
      moonbit_decref(_M0L3pkgS412.$0);
      _M0L6_2atmpS1211 = _M0Lm20match__tag__saver__0S420;
      _M0L6_2atmpS1210 = _M0L6_2atmpS1211 + 1;
      _M0L6_2atmpS1207 = (int64_t)_M0L6_2atmpS1210;
      _M0L6_2atmpS1209 = _M0Lm10match__endS419;
      _M0L6_2atmpS1208 = (int64_t)_M0L6_2atmpS1209;
      moonbit_incref(_M0L7_2adataS414);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS424
      = _M0MPC16string6String4view(_M0L7_2adataS414, _M0L6_2atmpS1207, _M0L6_2atmpS1208);
      _M0L6_2atmpS1204 = (int64_t)_M0L8_2astartS415;
      _M0L6_2atmpS1206 = _M0Lm20match__tag__saver__0S420;
      _M0L6_2atmpS1205 = (int64_t)_M0L6_2atmpS1206;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS425
      = _M0MPC16string6String4view(_M0L7_2adataS414, _M0L6_2atmpS1204, _M0L6_2atmpS1205);
      _M0L4SomeS1203
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1203)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1203)->$0_0
      = _M0L13package__nameS424.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1203)->$0_1
      = _M0L13package__nameS424.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1203)->$0_2
      = _M0L13package__nameS424.$2;
      _M0L7_2abindS422
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS422)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS422->$0_0 = _M0L12module__nameS425.$0;
      _M0L7_2abindS422->$0_1 = _M0L12module__nameS425.$1;
      _M0L7_2abindS422->$0_2 = _M0L12module__nameS425.$2;
      _M0L7_2abindS422->$1 = _M0L4SomeS1203;
      break;
    }
    default: {
      void* _M0L4NoneS1212;
      moonbit_decref(_M0L7_2adataS414);
      _M0L4NoneS1212
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS422
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS422)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS422->$0_0 = _M0L3pkgS412.$0;
      _M0L7_2abindS422->$0_1 = _M0L3pkgS412.$1;
      _M0L7_2abindS422->$0_2 = _M0L3pkgS412.$2;
      _M0L7_2abindS422->$1 = _M0L4NoneS1212;
      break;
    }
  }
  joinlet_2308:;
  _M0L8_2afieldS2131
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS422->$0_1, _M0L7_2abindS422->$0_2, _M0L7_2abindS422->$0_0
  };
  _M0L15_2amodule__nameS431 = _M0L8_2afieldS2131;
  _M0L8_2afieldS2130 = _M0L7_2abindS422->$1;
  _M0L6_2acntS2233 = Moonbit_object_header(_M0L7_2abindS422)->rc;
  if (_M0L6_2acntS2233 > 1) {
    int32_t _M0L11_2anew__cntS2234 = _M0L6_2acntS2233 - 1;
    Moonbit_object_header(_M0L7_2abindS422)->rc = _M0L11_2anew__cntS2234;
    moonbit_incref(_M0L8_2afieldS2130);
    moonbit_incref(_M0L15_2amodule__nameS431.$0);
  } else if (_M0L6_2acntS2233 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS422);
  }
  _M0L16_2apackage__nameS432 = _M0L8_2afieldS2130;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS432)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS433 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS432;
      struct _M0TPC16string10StringView _M0L8_2afieldS2129 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS433->$0_1,
                                              _M0L7_2aSomeS433->$0_2,
                                              _M0L7_2aSomeS433->$0_0};
      int32_t _M0L6_2acntS2235 = Moonbit_object_header(_M0L7_2aSomeS433)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS434;
      if (_M0L6_2acntS2235 > 1) {
        int32_t _M0L11_2anew__cntS2236 = _M0L6_2acntS2235 - 1;
        Moonbit_object_header(_M0L7_2aSomeS433)->rc = _M0L11_2anew__cntS2236;
        moonbit_incref(_M0L8_2afieldS2129.$0);
      } else if (_M0L6_2acntS2235 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS433);
      }
      _M0L12_2apkg__nameS434 = _M0L8_2afieldS2129;
      if (_M0L6loggerS435.$1) {
        moonbit_incref(_M0L6loggerS435.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L12_2apkg__nameS434);
      if (_M0L6loggerS435.$1) {
        moonbit_incref(_M0L6loggerS435.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS432);
      break;
    }
  }
  _M0L8_2afieldS2128
  = (struct _M0TPC16string10StringView){
    _M0L4selfS413->$1_1, _M0L4selfS413->$1_2, _M0L4selfS413->$1_0
  };
  _M0L8filenameS1198 = _M0L8_2afieldS2128;
  moonbit_incref(_M0L8filenameS1198.$0);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L8filenameS1198);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 58);
  _M0L8_2afieldS2127
  = (struct _M0TPC16string10StringView){
    _M0L4selfS413->$2_1, _M0L4selfS413->$2_2, _M0L4selfS413->$2_0
  };
  _M0L11start__lineS1199 = _M0L8_2afieldS2127;
  moonbit_incref(_M0L11start__lineS1199.$0);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L11start__lineS1199);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 58);
  _M0L8_2afieldS2126
  = (struct _M0TPC16string10StringView){
    _M0L4selfS413->$3_1, _M0L4selfS413->$3_2, _M0L4selfS413->$3_0
  };
  _M0L13start__columnS1200 = _M0L8_2afieldS2126;
  moonbit_incref(_M0L13start__columnS1200.$0);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L13start__columnS1200);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 45);
  _M0L8_2afieldS2125
  = (struct _M0TPC16string10StringView){
    _M0L4selfS413->$4_1, _M0L4selfS413->$4_2, _M0L4selfS413->$4_0
  };
  _M0L9end__lineS1201 = _M0L8_2afieldS2125;
  moonbit_incref(_M0L9end__lineS1201.$0);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L9end__lineS1201);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 58);
  _M0L8_2afieldS2124
  = (struct _M0TPC16string10StringView){
    _M0L4selfS413->$5_1, _M0L4selfS413->$5_2, _M0L4selfS413->$5_0
  };
  _M0L6_2acntS2237 = Moonbit_object_header(_M0L4selfS413)->rc;
  if (_M0L6_2acntS2237 > 1) {
    int32_t _M0L11_2anew__cntS2243 = _M0L6_2acntS2237 - 1;
    Moonbit_object_header(_M0L4selfS413)->rc = _M0L11_2anew__cntS2243;
    moonbit_incref(_M0L8_2afieldS2124.$0);
  } else if (_M0L6_2acntS2237 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2242 =
      (struct _M0TPC16string10StringView){_M0L4selfS413->$4_1,
                                            _M0L4selfS413->$4_2,
                                            _M0L4selfS413->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2241;
    struct _M0TPC16string10StringView _M0L8_2afieldS2240;
    struct _M0TPC16string10StringView _M0L8_2afieldS2239;
    struct _M0TPC16string10StringView _M0L8_2afieldS2238;
    moonbit_decref(_M0L8_2afieldS2242.$0);
    _M0L8_2afieldS2241
    = (struct _M0TPC16string10StringView){
      _M0L4selfS413->$3_1, _M0L4selfS413->$3_2, _M0L4selfS413->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2241.$0);
    _M0L8_2afieldS2240
    = (struct _M0TPC16string10StringView){
      _M0L4selfS413->$2_1, _M0L4selfS413->$2_2, _M0L4selfS413->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2240.$0);
    _M0L8_2afieldS2239
    = (struct _M0TPC16string10StringView){
      _M0L4selfS413->$1_1, _M0L4selfS413->$1_2, _M0L4selfS413->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2239.$0);
    _M0L8_2afieldS2238
    = (struct _M0TPC16string10StringView){
      _M0L4selfS413->$0_1, _M0L4selfS413->$0_2, _M0L4selfS413->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2238.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS413);
  }
  _M0L11end__columnS1202 = _M0L8_2afieldS2124;
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L11end__columnS1202);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS435.$0->$method_2(_M0L6loggerS435.$1, _M0L15_2amodule__nameS431);
  return 0;
}

moonbit_bytes_t _M0MPC15bytes5Bytes11from__array(
  struct _M0TPB9ArrayViewGyE _M0L3arrS410
) {
  int32_t _M0L6_2atmpS1193;
  struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__* _closure_2312;
  struct _M0TWuEu* _M0L6_2atmpS1194;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  moonbit_incref(_M0L3arrS410.$0);
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1193 = _M0MPC15array9ArrayView6lengthGyE(_M0L3arrS410);
  _closure_2312
  = (struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__*)moonbit_malloc(sizeof(struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__));
  Moonbit_object_header(_closure_2312)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__, $0_0) >> 2, 1, 0);
  _closure_2312->code = &_M0MPC15bytes5Bytes11from__arrayC1195l455;
  _closure_2312->$0_0 = _M0L3arrS410.$0;
  _closure_2312->$0_1 = _M0L3arrS410.$1;
  _closure_2312->$0_2 = _M0L3arrS410.$2;
  _M0L6_2atmpS1194 = (struct _M0TWuEu*)_closure_2312;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  return _M0MPC15bytes5Bytes5makei(_M0L6_2atmpS1193, _M0L6_2atmpS1194);
}

int32_t _M0MPC15bytes5Bytes11from__arrayC1195l455(
  struct _M0TWuEu* _M0L6_2aenvS1196,
  int32_t _M0L1iS411
) {
  struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__* _M0L14_2acasted__envS1197;
  struct _M0TPB9ArrayViewGyE _M0L8_2afieldS2133;
  int32_t _M0L6_2acntS2244;
  struct _M0TPB9ArrayViewGyE _M0L3arrS410;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L14_2acasted__envS1197
  = (struct _M0R44Bytes_3a_3afrom__array_2eanon__u1195__l455__*)_M0L6_2aenvS1196;
  _M0L8_2afieldS2133
  = (struct _M0TPB9ArrayViewGyE){
    _M0L14_2acasted__envS1197->$0_1,
      _M0L14_2acasted__envS1197->$0_2,
      _M0L14_2acasted__envS1197->$0_0
  };
  _M0L6_2acntS2244 = Moonbit_object_header(_M0L14_2acasted__envS1197)->rc;
  if (_M0L6_2acntS2244 > 1) {
    int32_t _M0L11_2anew__cntS2245 = _M0L6_2acntS2244 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1197)->rc
    = _M0L11_2anew__cntS2245;
    moonbit_incref(_M0L8_2afieldS2133.$0);
  } else if (_M0L6_2acntS2244 == 1) {
    #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_free(_M0L14_2acasted__envS1197);
  }
  _M0L3arrS410 = _M0L8_2afieldS2133;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  return _M0MPC15array9ArrayView2atGyE(_M0L3arrS410, _M0L1iS411);
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS402,
  int32_t _M0L5startS408,
  int64_t _M0L3endS404
) {
  int32_t _M0L3lenS401;
  int32_t _M0L3endS403;
  int32_t _M0L5startS407;
  int32_t _if__result_2313;
  #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3lenS401 = Moonbit_array_length(_M0L4selfS402);
  if (_M0L3endS404 == 4294967296ll) {
    _M0L3endS403 = _M0L3lenS401;
  } else {
    int64_t _M0L7_2aSomeS405 = _M0L3endS404;
    int32_t _M0L6_2aendS406 = (int32_t)_M0L7_2aSomeS405;
    if (_M0L6_2aendS406 < 0) {
      _M0L3endS403 = _M0L3lenS401 + _M0L6_2aendS406;
    } else {
      _M0L3endS403 = _M0L6_2aendS406;
    }
  }
  if (_M0L5startS408 < 0) {
    _M0L5startS407 = _M0L3lenS401 + _M0L5startS408;
  } else {
    _M0L5startS407 = _M0L5startS408;
  }
  if (_M0L5startS407 >= 0) {
    if (_M0L5startS407 <= _M0L3endS403) {
      _if__result_2313 = _M0L3endS403 <= _M0L3lenS401;
    } else {
      _if__result_2313 = 0;
    }
  } else {
    _if__result_2313 = 0;
  }
  if (_if__result_2313) {
    int32_t _M0L7_2abindS409 = _M0L3endS403 - _M0L5startS407;
    int32_t _M0L6_2atmpS1192 = _M0L5startS407 + _M0L7_2abindS409;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS407,
                                              _M0L6_2atmpS1192,
                                              _M0L4selfS402};
  } else {
    moonbit_decref(_M0L4selfS402);
    #line 180 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    return _M0FPB5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data);
  }
}

int32_t _M0MPC15bytes9BytesView6length(
  struct _M0TPC15bytes9BytesView _M0L4selfS400
) {
  int32_t _M0L3endS1190;
  int32_t _M0L8_2afieldS2134;
  int32_t _M0L5startS1191;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3endS1190 = _M0L4selfS400.$2;
  _M0L8_2afieldS2134 = _M0L4selfS400.$1;
  moonbit_decref(_M0L4selfS400.$0);
  _M0L5startS1191 = _M0L8_2afieldS2134;
  return _M0L3endS1190 - _M0L5startS1191;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS399) {
  moonbit_string_t _M0L6_2atmpS1189;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS1189 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS399);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS1189);
  moonbit_decref(_M0L6_2atmpS1189);
  return 0;
}

moonbit_bytes_t _M0MPC15bytes5Bytes5makei(
  int32_t _M0L6lengthS394,
  struct _M0TWuEu* _M0L5valueS396
) {
  int32_t _M0L6_2atmpS1188;
  moonbit_bytes_t _M0L3arrS395;
  int32_t _M0L1iS397;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  if (_M0L6lengthS394 <= 0) {
    moonbit_decref(_M0L5valueS396);
    return (moonbit_bytes_t)moonbit_bytes_literal_0.data;
  }
  moonbit_incref(_M0L5valueS396);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1188 = _M0L5valueS396->code(_M0L5valueS396, 0);
  _M0L3arrS395
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6lengthS394, _M0L6_2atmpS1188);
  _M0L1iS397 = 1;
  while (1) {
    if (_M0L1iS397 < _M0L6lengthS394) {
      int32_t _M0L6_2atmpS1186;
      int32_t _M0L6_2atmpS1187;
      moonbit_incref(_M0L5valueS396);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      _M0L6_2atmpS1186 = _M0L5valueS396->code(_M0L5valueS396, _M0L1iS397);
      if (_M0L1iS397 < 0 || _M0L1iS397 >= Moonbit_array_length(_M0L3arrS395)) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        moonbit_panic();
      }
      _M0L3arrS395[_M0L1iS397] = _M0L6_2atmpS1186;
      _M0L6_2atmpS1187 = _M0L1iS397 + 1;
      _M0L1iS397 = _M0L6_2atmpS1187;
      continue;
    } else {
      moonbit_decref(_M0L5valueS396);
    }
    break;
  }
  return _M0L3arrS395;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS392,
  int32_t _M0L3idxS393
) {
  int32_t _M0L6_2atmpS2135;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2135 = _M0L4selfS392[_M0L3idxS393];
  moonbit_decref(_M0L4selfS392);
  return _M0L6_2atmpS2135;
}

struct _M0TPB9ArrayViewGyE _M0MPC15array10FixedArray12view_2einnerGyE(
  moonbit_bytes_t _M0L4selfS383,
  int32_t _M0L5startS389,
  int64_t _M0L3endS385
) {
  int32_t _M0L3lenS382;
  int32_t _M0L3endS384;
  int32_t _M0L5startS388;
  int32_t _if__result_2315;
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
      _if__result_2315 = _M0L3endS384 <= _M0L3lenS382;
    } else {
      _if__result_2315 = 0;
    }
  } else {
    _if__result_2315 = 0;
  }
  if (_if__result_2315) {
    moonbit_bytes_t _M0L7_2abindS390 = _M0L4selfS383;
    int32_t _M0L7_2abindS391 = _M0L3endS384 - _M0L5startS388;
    int32_t _M0L6_2atmpS1185 = _M0L5startS388 + _M0L7_2abindS391;
    return (struct _M0TPB9ArrayViewGyE){_M0L5startS388,
                                          _M0L6_2atmpS1185,
                                          _M0L7_2abindS390};
  } else {
    moonbit_decref(_M0L4selfS383);
    #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGRPB9ArrayViewGyEE((moonbit_string_t)moonbit_string_literal_13.data, (moonbit_string_t)moonbit_string_literal_14.data);
  }
}

int32_t _M0MPC15array9ArrayView2atGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS381,
  int32_t _M0L5indexS380
) {
  int32_t _if__result_2316;
  #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  if (_M0L5indexS380 >= 0) {
    int32_t _M0L3endS1172 = _M0L4selfS381.$2;
    int32_t _M0L5startS1173 = _M0L4selfS381.$1;
    int32_t _M0L6_2atmpS1171 = _M0L3endS1172 - _M0L5startS1173;
    _if__result_2316 = _M0L5indexS380 < _M0L6_2atmpS1171;
  } else {
    _if__result_2316 = 0;
  }
  if (_if__result_2316) {
    moonbit_bytes_t _M0L8_2afieldS2138 = _M0L4selfS381.$0;
    moonbit_bytes_t _M0L3bufS1174 = _M0L8_2afieldS2138;
    int32_t _M0L8_2afieldS2137 = _M0L4selfS381.$1;
    int32_t _M0L5startS1176 = _M0L8_2afieldS2137;
    int32_t _M0L6_2atmpS1175 = _M0L5startS1176 + _M0L5indexS380;
    int32_t _M0L6_2atmpS2136;
    if (
      _M0L6_2atmpS1175 < 0
      || _M0L6_2atmpS1175 >= Moonbit_array_length(_M0L3bufS1174)
    ) {
      #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2136 = (int32_t)_M0L3bufS1174[_M0L6_2atmpS1175];
    moonbit_decref(_M0L3bufS1174);
    return _M0L6_2atmpS2136;
  } else {
    int32_t _M0L3endS1183 = _M0L4selfS381.$2;
    int32_t _M0L8_2afieldS2142 = _M0L4selfS381.$1;
    int32_t _M0L5startS1184;
    int32_t _M0L6_2atmpS1182;
    moonbit_string_t _M0L6_2atmpS1181;
    moonbit_string_t _M0L6_2atmpS2141;
    moonbit_string_t _M0L6_2atmpS1180;
    moonbit_string_t _M0L6_2atmpS2140;
    moonbit_string_t _M0L6_2atmpS1178;
    moonbit_string_t _M0L6_2atmpS1179;
    moonbit_string_t _M0L6_2atmpS2139;
    moonbit_string_t _M0L6_2atmpS1177;
    moonbit_decref(_M0L4selfS381.$0);
    _M0L5startS1184 = _M0L8_2afieldS2142;
    _M0L6_2atmpS1182 = _M0L3endS1183 - _M0L5startS1184;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS1181
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS1182);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2141
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_15.data, _M0L6_2atmpS1181);
    moonbit_decref(_M0L6_2atmpS1181);
    _M0L6_2atmpS1180 = _M0L6_2atmpS2141;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2140
    = moonbit_add_string(_M0L6_2atmpS1180, (moonbit_string_t)moonbit_string_literal_16.data);
    moonbit_decref(_M0L6_2atmpS1180);
    _M0L6_2atmpS1178 = _M0L6_2atmpS2140;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS1179
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS380);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2139 = moonbit_add_string(_M0L6_2atmpS1178, _M0L6_2atmpS1179);
    moonbit_decref(_M0L6_2atmpS1178);
    moonbit_decref(_M0L6_2atmpS1179);
    _M0L6_2atmpS1177 = _M0L6_2atmpS2139;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGyE(_M0L6_2atmpS1177, (moonbit_string_t)moonbit_string_literal_17.data);
  }
}

int32_t _M0IPC15array5ArrayPB4Show6outputGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS379,
  struct _M0TPB6Logger _M0L6loggerS378
) {
  struct _M0TWEOs* _M0L6_2atmpS1170;
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1170 = _M0MPC15array5Array4iterGsE(_M0L4selfS379);
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0MPB6Logger19write__iter_2einnerGsE(_M0L6loggerS378, _M0L6_2atmpS1170, (moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, 0);
  return 0;
}

struct _M0TWEOs* _M0MPC15array5Array4iterGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS377
) {
  moonbit_string_t* _M0L8_2afieldS2144;
  moonbit_string_t* _M0L3bufS1168;
  int32_t _M0L8_2afieldS2143;
  int32_t _M0L6_2acntS2246;
  int32_t _M0L3lenS1169;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1167;
  #line 1651 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS2144 = _M0L4selfS377->$0;
  _M0L3bufS1168 = _M0L8_2afieldS2144;
  _M0L8_2afieldS2143 = _M0L4selfS377->$1;
  _M0L6_2acntS2246 = Moonbit_object_header(_M0L4selfS377)->rc;
  if (_M0L6_2acntS2246 > 1) {
    int32_t _M0L11_2anew__cntS2247 = _M0L6_2acntS2246 - 1;
    Moonbit_object_header(_M0L4selfS377)->rc = _M0L11_2anew__cntS2247;
    moonbit_incref(_M0L3bufS1168);
  } else if (_M0L6_2acntS2246 == 1) {
    #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_free(_M0L4selfS377);
  }
  _M0L3lenS1169 = _M0L8_2afieldS2143;
  _M0L6_2atmpS1167
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L3lenS1169, _M0L3bufS1168
  };
  #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1167);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS375
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS374;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__* _closure_2317;
  struct _M0TWEOs* _M0L6_2atmpS1155;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS374
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS374)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS374->$0 = 0;
  _closure_2317
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__));
  Moonbit_object_header(_closure_2317)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__, $0_0) >> 2, 2, 0);
  _closure_2317->code = &_M0MPC15array9ArrayView4iterGsEC1156l570;
  _closure_2317->$0_0 = _M0L4selfS375.$0;
  _closure_2317->$0_1 = _M0L4selfS375.$1;
  _closure_2317->$0_2 = _M0L4selfS375.$2;
  _closure_2317->$1 = _M0L1iS374;
  _M0L6_2atmpS1155 = (struct _M0TWEOs*)_closure_2317;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1155);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1156l570(
  struct _M0TWEOs* _M0L6_2aenvS1157
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__* _M0L14_2acasted__envS1158;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2149;
  struct _M0TPC13ref3RefGiE* _M0L1iS374;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2148;
  int32_t _M0L6_2acntS2248;
  struct _M0TPB9ArrayViewGsE _M0L4selfS375;
  int32_t _M0L3valS1159;
  int32_t _M0L6_2atmpS1160;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1158
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1156__l570__*)_M0L6_2aenvS1157;
  _M0L8_2afieldS2149 = _M0L14_2acasted__envS1158->$1;
  _M0L1iS374 = _M0L8_2afieldS2149;
  _M0L8_2afieldS2148
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1158->$0_1,
      _M0L14_2acasted__envS1158->$0_2,
      _M0L14_2acasted__envS1158->$0_0
  };
  _M0L6_2acntS2248 = Moonbit_object_header(_M0L14_2acasted__envS1158)->rc;
  if (_M0L6_2acntS2248 > 1) {
    int32_t _M0L11_2anew__cntS2249 = _M0L6_2acntS2248 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1158)->rc
    = _M0L11_2anew__cntS2249;
    moonbit_incref(_M0L1iS374);
    moonbit_incref(_M0L8_2afieldS2148.$0);
  } else if (_M0L6_2acntS2248 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1158);
  }
  _M0L4selfS375 = _M0L8_2afieldS2148;
  _M0L3valS1159 = _M0L1iS374->$0;
  moonbit_incref(_M0L4selfS375.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1160 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS375);
  if (_M0L3valS1159 < _M0L6_2atmpS1160) {
    moonbit_string_t* _M0L8_2afieldS2147 = _M0L4selfS375.$0;
    moonbit_string_t* _M0L3bufS1163 = _M0L8_2afieldS2147;
    int32_t _M0L8_2afieldS2146 = _M0L4selfS375.$1;
    int32_t _M0L5startS1165 = _M0L8_2afieldS2146;
    int32_t _M0L3valS1166 = _M0L1iS374->$0;
    int32_t _M0L6_2atmpS1164 = _M0L5startS1165 + _M0L3valS1166;
    moonbit_string_t _M0L6_2atmpS2145 =
      (moonbit_string_t)_M0L3bufS1163[_M0L6_2atmpS1164];
    moonbit_string_t _M0L4elemS376;
    int32_t _M0L3valS1162;
    int32_t _M0L6_2atmpS1161;
    moonbit_incref(_M0L6_2atmpS2145);
    moonbit_decref(_M0L3bufS1163);
    _M0L4elemS376 = _M0L6_2atmpS2145;
    _M0L3valS1162 = _M0L1iS374->$0;
    _M0L6_2atmpS1161 = _M0L3valS1162 + 1;
    _M0L1iS374->$0 = _M0L6_2atmpS1161;
    moonbit_decref(_M0L1iS374);
    return _M0L4elemS376;
  } else {
    moonbit_decref(_M0L4selfS375.$0);
    moonbit_decref(_M0L1iS374);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS373
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS373;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS372,
  struct _M0TPB6Logger _M0L6loggerS371
) {
  moonbit_string_t _M0L6_2atmpS1154;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1154 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS372, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS371.$0->$method_0(_M0L6loggerS371.$1, _M0L6_2atmpS1154);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS365,
  moonbit_string_t _M0L5valueS367
) {
  int32_t _M0L3lenS1144;
  moonbit_string_t* _M0L6_2atmpS1146;
  int32_t _M0L6_2atmpS2152;
  int32_t _M0L6_2atmpS1145;
  int32_t _M0L6lengthS366;
  moonbit_string_t* _M0L8_2afieldS2151;
  moonbit_string_t* _M0L3bufS1147;
  moonbit_string_t _M0L6_2aoldS2150;
  int32_t _M0L6_2atmpS1148;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1144 = _M0L4selfS365->$1;
  moonbit_incref(_M0L4selfS365);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1146 = _M0MPC15array5Array6bufferGsE(_M0L4selfS365);
  _M0L6_2atmpS2152 = Moonbit_array_length(_M0L6_2atmpS1146);
  moonbit_decref(_M0L6_2atmpS1146);
  _M0L6_2atmpS1145 = _M0L6_2atmpS2152;
  if (_M0L3lenS1144 == _M0L6_2atmpS1145) {
    moonbit_incref(_M0L4selfS365);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS365);
  }
  _M0L6lengthS366 = _M0L4selfS365->$1;
  _M0L8_2afieldS2151 = _M0L4selfS365->$0;
  _M0L3bufS1147 = _M0L8_2afieldS2151;
  _M0L6_2aoldS2150 = (moonbit_string_t)_M0L3bufS1147[_M0L6lengthS366];
  moonbit_decref(_M0L6_2aoldS2150);
  _M0L3bufS1147[_M0L6lengthS366] = _M0L5valueS367;
  _M0L6_2atmpS1148 = _M0L6lengthS366 + 1;
  _M0L4selfS365->$1 = _M0L6_2atmpS1148;
  moonbit_decref(_M0L4selfS365);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS368,
  struct _M0TUsiE* _M0L5valueS370
) {
  int32_t _M0L3lenS1149;
  struct _M0TUsiE** _M0L6_2atmpS1151;
  int32_t _M0L6_2atmpS2155;
  int32_t _M0L6_2atmpS1150;
  int32_t _M0L6lengthS369;
  struct _M0TUsiE** _M0L8_2afieldS2154;
  struct _M0TUsiE** _M0L3bufS1152;
  struct _M0TUsiE* _M0L6_2aoldS2153;
  int32_t _M0L6_2atmpS1153;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1149 = _M0L4selfS368->$1;
  moonbit_incref(_M0L4selfS368);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1151 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS368);
  _M0L6_2atmpS2155 = Moonbit_array_length(_M0L6_2atmpS1151);
  moonbit_decref(_M0L6_2atmpS1151);
  _M0L6_2atmpS1150 = _M0L6_2atmpS2155;
  if (_M0L3lenS1149 == _M0L6_2atmpS1150) {
    moonbit_incref(_M0L4selfS368);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS368);
  }
  _M0L6lengthS369 = _M0L4selfS368->$1;
  _M0L8_2afieldS2154 = _M0L4selfS368->$0;
  _M0L3bufS1152 = _M0L8_2afieldS2154;
  _M0L6_2aoldS2153 = (struct _M0TUsiE*)_M0L3bufS1152[_M0L6lengthS369];
  if (_M0L6_2aoldS2153) {
    moonbit_decref(_M0L6_2aoldS2153);
  }
  _M0L3bufS1152[_M0L6lengthS369] = _M0L5valueS370;
  _M0L6_2atmpS1153 = _M0L6lengthS369 + 1;
  _M0L4selfS368->$1 = _M0L6_2atmpS1153;
  moonbit_decref(_M0L4selfS368);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS360) {
  int32_t _M0L8old__capS359;
  int32_t _M0L8new__capS361;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS359 = _M0L4selfS360->$1;
  if (_M0L8old__capS359 == 0) {
    _M0L8new__capS361 = 8;
  } else {
    _M0L8new__capS361 = _M0L8old__capS359 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS360, _M0L8new__capS361);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS363
) {
  int32_t _M0L8old__capS362;
  int32_t _M0L8new__capS364;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS362 = _M0L4selfS363->$1;
  if (_M0L8old__capS362 == 0) {
    _M0L8new__capS364 = 8;
  } else {
    _M0L8new__capS364 = _M0L8old__capS362 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS363, _M0L8new__capS364);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS350,
  int32_t _M0L13new__capacityS348
) {
  moonbit_string_t* _M0L8new__bufS347;
  moonbit_string_t* _M0L8_2afieldS2157;
  moonbit_string_t* _M0L8old__bufS349;
  int32_t _M0L8old__capS351;
  int32_t _M0L9copy__lenS352;
  moonbit_string_t* _M0L6_2aoldS2156;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS347
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS348, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2157 = _M0L4selfS350->$0;
  _M0L8old__bufS349 = _M0L8_2afieldS2157;
  _M0L8old__capS351 = Moonbit_array_length(_M0L8old__bufS349);
  if (_M0L8old__capS351 < _M0L13new__capacityS348) {
    _M0L9copy__lenS352 = _M0L8old__capS351;
  } else {
    _M0L9copy__lenS352 = _M0L13new__capacityS348;
  }
  moonbit_incref(_M0L8old__bufS349);
  moonbit_incref(_M0L8new__bufS347);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS347, 0, _M0L8old__bufS349, 0, _M0L9copy__lenS352);
  _M0L6_2aoldS2156 = _M0L4selfS350->$0;
  moonbit_decref(_M0L6_2aoldS2156);
  _M0L4selfS350->$0 = _M0L8new__bufS347;
  moonbit_decref(_M0L4selfS350);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS356,
  int32_t _M0L13new__capacityS354
) {
  struct _M0TUsiE** _M0L8new__bufS353;
  struct _M0TUsiE** _M0L8_2afieldS2159;
  struct _M0TUsiE** _M0L8old__bufS355;
  int32_t _M0L8old__capS357;
  int32_t _M0L9copy__lenS358;
  struct _M0TUsiE** _M0L6_2aoldS2158;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS353
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS354, 0);
  _M0L8_2afieldS2159 = _M0L4selfS356->$0;
  _M0L8old__bufS355 = _M0L8_2afieldS2159;
  _M0L8old__capS357 = Moonbit_array_length(_M0L8old__bufS355);
  if (_M0L8old__capS357 < _M0L13new__capacityS354) {
    _M0L9copy__lenS358 = _M0L8old__capS357;
  } else {
    _M0L9copy__lenS358 = _M0L13new__capacityS354;
  }
  moonbit_incref(_M0L8old__bufS355);
  moonbit_incref(_M0L8new__bufS353);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS353, 0, _M0L8old__bufS355, 0, _M0L9copy__lenS358);
  _M0L6_2aoldS2158 = _M0L4selfS356->$0;
  moonbit_decref(_M0L6_2aoldS2158);
  _M0L4selfS356->$0 = _M0L8new__bufS353;
  moonbit_decref(_M0L4selfS356);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS346
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS346 == 0) {
    moonbit_string_t* _M0L6_2atmpS1142 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2318 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2318)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2318->$0 = _M0L6_2atmpS1142;
    _block_2318->$1 = 0;
    return _block_2318;
  } else {
    moonbit_string_t* _M0L6_2atmpS1143 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS346, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2319 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2319)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2319->$0 = _M0L6_2atmpS1143;
    _block_2319->$1 = 0;
    return _block_2319;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS344,
  struct _M0TPC16string10StringView _M0L3strS345
) {
  int32_t _M0L3lenS1130;
  int32_t _M0L6_2atmpS1132;
  int32_t _M0L6_2atmpS1131;
  int32_t _M0L6_2atmpS1129;
  moonbit_bytes_t _M0L8_2afieldS2160;
  moonbit_bytes_t _M0L4dataS1133;
  int32_t _M0L3lenS1134;
  moonbit_string_t _M0L6_2atmpS1135;
  int32_t _M0L6_2atmpS1136;
  int32_t _M0L6_2atmpS1137;
  int32_t _M0L3lenS1139;
  int32_t _M0L6_2atmpS1141;
  int32_t _M0L6_2atmpS1140;
  int32_t _M0L6_2atmpS1138;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1130 = _M0L4selfS344->$1;
  moonbit_incref(_M0L3strS345.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1132 = _M0MPC16string10StringView6length(_M0L3strS345);
  _M0L6_2atmpS1131 = _M0L6_2atmpS1132 * 2;
  _M0L6_2atmpS1129 = _M0L3lenS1130 + _M0L6_2atmpS1131;
  moonbit_incref(_M0L4selfS344);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS344, _M0L6_2atmpS1129);
  _M0L8_2afieldS2160 = _M0L4selfS344->$0;
  _M0L4dataS1133 = _M0L8_2afieldS2160;
  _M0L3lenS1134 = _M0L4selfS344->$1;
  moonbit_incref(_M0L4dataS1133);
  moonbit_incref(_M0L3strS345.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1135 = _M0MPC16string10StringView4data(_M0L3strS345);
  moonbit_incref(_M0L3strS345.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1136 = _M0MPC16string10StringView13start__offset(_M0L3strS345);
  moonbit_incref(_M0L3strS345.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1137 = _M0MPC16string10StringView6length(_M0L3strS345);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1133, _M0L3lenS1134, _M0L6_2atmpS1135, _M0L6_2atmpS1136, _M0L6_2atmpS1137);
  _M0L3lenS1139 = _M0L4selfS344->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1141 = _M0MPC16string10StringView6length(_M0L3strS345);
  _M0L6_2atmpS1140 = _M0L6_2atmpS1141 * 2;
  _M0L6_2atmpS1138 = _M0L3lenS1139 + _M0L6_2atmpS1140;
  _M0L4selfS344->$1 = _M0L6_2atmpS1138;
  moonbit_decref(_M0L4selfS344);
  return 0;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS342
) {
  int32_t _M0L3endS1125;
  int32_t _M0L8_2afieldS2161;
  int32_t _M0L5startS1126;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1125 = _M0L4selfS342.$2;
  _M0L8_2afieldS2161 = _M0L4selfS342.$1;
  moonbit_decref(_M0L4selfS342.$0);
  _M0L5startS1126 = _M0L8_2afieldS2161;
  return _M0L3endS1125 - _M0L5startS1126;
}

int32_t _M0MPC15array9ArrayView6lengthGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS343
) {
  int32_t _M0L3endS1127;
  int32_t _M0L8_2afieldS2162;
  int32_t _M0L5startS1128;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1127 = _M0L4selfS343.$2;
  _M0L8_2afieldS2162 = _M0L4selfS343.$1;
  moonbit_decref(_M0L4selfS343.$0);
  _M0L5startS1128 = _M0L8_2afieldS2162;
  return _M0L3endS1127 - _M0L5startS1128;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS340,
  int64_t _M0L19start__offset_2eoptS338,
  int64_t _M0L11end__offsetS341
) {
  int32_t _M0L13start__offsetS337;
  if (_M0L19start__offset_2eoptS338 == 4294967296ll) {
    _M0L13start__offsetS337 = 0;
  } else {
    int64_t _M0L7_2aSomeS339 = _M0L19start__offset_2eoptS338;
    _M0L13start__offsetS337 = (int32_t)_M0L7_2aSomeS339;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS340, _M0L13start__offsetS337, _M0L11end__offsetS341);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS335,
  int32_t _M0L13start__offsetS336,
  int64_t _M0L11end__offsetS333
) {
  int32_t _M0L11end__offsetS332;
  int32_t _if__result_2320;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS333 == 4294967296ll) {
    _M0L11end__offsetS332 = Moonbit_array_length(_M0L4selfS335);
  } else {
    int64_t _M0L7_2aSomeS334 = _M0L11end__offsetS333;
    _M0L11end__offsetS332 = (int32_t)_M0L7_2aSomeS334;
  }
  if (_M0L13start__offsetS336 >= 0) {
    if (_M0L13start__offsetS336 <= _M0L11end__offsetS332) {
      int32_t _M0L6_2atmpS1124 = Moonbit_array_length(_M0L4selfS335);
      _if__result_2320 = _M0L11end__offsetS332 <= _M0L6_2atmpS1124;
    } else {
      _if__result_2320 = 0;
    }
  } else {
    _if__result_2320 = 0;
  }
  if (_if__result_2320) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS336,
                                                 _M0L11end__offsetS332,
                                                 _M0L4selfS335};
  } else {
    moonbit_decref(_M0L4selfS335);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_21.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS327
) {
  int32_t _M0L5startS326;
  int32_t _M0L3endS328;
  struct _M0TPC13ref3RefGiE* _M0L5indexS329;
  struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__* _closure_2321;
  struct _M0TWEOc* _M0L6_2atmpS1103;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS326 = _M0L4selfS327.$1;
  _M0L3endS328 = _M0L4selfS327.$2;
  _M0L5indexS329
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS329)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS329->$0 = _M0L5startS326;
  _closure_2321
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__));
  Moonbit_object_header(_closure_2321)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__, $0) >> 2, 2, 0);
  _closure_2321->code = &_M0MPC16string10StringView4iterC1104l198;
  _closure_2321->$0 = _M0L5indexS329;
  _closure_2321->$1 = _M0L3endS328;
  _closure_2321->$2_0 = _M0L4selfS327.$0;
  _closure_2321->$2_1 = _M0L4selfS327.$1;
  _closure_2321->$2_2 = _M0L4selfS327.$2;
  _M0L6_2atmpS1103 = (struct _M0TWEOc*)_closure_2321;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1103);
}

int32_t _M0MPC16string10StringView4iterC1104l198(
  struct _M0TWEOc* _M0L6_2aenvS1105
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__* _M0L14_2acasted__envS1106;
  struct _M0TPC16string10StringView _M0L8_2afieldS2168;
  struct _M0TPC16string10StringView _M0L4selfS327;
  int32_t _M0L3endS328;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2167;
  int32_t _M0L6_2acntS2250;
  struct _M0TPC13ref3RefGiE* _M0L5indexS329;
  int32_t _M0L3valS1107;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS1106
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1104__l198__*)_M0L6_2aenvS1105;
  _M0L8_2afieldS2168
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1106->$2_1,
      _M0L14_2acasted__envS1106->$2_2,
      _M0L14_2acasted__envS1106->$2_0
  };
  _M0L4selfS327 = _M0L8_2afieldS2168;
  _M0L3endS328 = _M0L14_2acasted__envS1106->$1;
  _M0L8_2afieldS2167 = _M0L14_2acasted__envS1106->$0;
  _M0L6_2acntS2250 = Moonbit_object_header(_M0L14_2acasted__envS1106)->rc;
  if (_M0L6_2acntS2250 > 1) {
    int32_t _M0L11_2anew__cntS2251 = _M0L6_2acntS2250 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1106)->rc
    = _M0L11_2anew__cntS2251;
    moonbit_incref(_M0L4selfS327.$0);
    moonbit_incref(_M0L8_2afieldS2167);
  } else if (_M0L6_2acntS2250 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS1106);
  }
  _M0L5indexS329 = _M0L8_2afieldS2167;
  _M0L3valS1107 = _M0L5indexS329->$0;
  if (_M0L3valS1107 < _M0L3endS328) {
    moonbit_string_t _M0L8_2afieldS2166 = _M0L4selfS327.$0;
    moonbit_string_t _M0L3strS1122 = _M0L8_2afieldS2166;
    int32_t _M0L3valS1123 = _M0L5indexS329->$0;
    int32_t _M0L6_2atmpS2165 = _M0L3strS1122[_M0L3valS1123];
    int32_t _M0L2c1S330 = _M0L6_2atmpS2165;
    int32_t _if__result_2322;
    int32_t _M0L3valS1120;
    int32_t _M0L6_2atmpS1119;
    int32_t _M0L6_2atmpS1121;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S330)) {
      int32_t _M0L3valS1110 = _M0L5indexS329->$0;
      int32_t _M0L6_2atmpS1108 = _M0L3valS1110 + 1;
      int32_t _M0L3endS1109 = _M0L4selfS327.$2;
      _if__result_2322 = _M0L6_2atmpS1108 < _M0L3endS1109;
    } else {
      _if__result_2322 = 0;
    }
    if (_if__result_2322) {
      moonbit_string_t _M0L8_2afieldS2164 = _M0L4selfS327.$0;
      moonbit_string_t _M0L3strS1116 = _M0L8_2afieldS2164;
      int32_t _M0L3valS1118 = _M0L5indexS329->$0;
      int32_t _M0L6_2atmpS1117 = _M0L3valS1118 + 1;
      int32_t _M0L6_2atmpS2163 = _M0L3strS1116[_M0L6_2atmpS1117];
      int32_t _M0L2c2S331;
      moonbit_decref(_M0L3strS1116);
      _M0L2c2S331 = _M0L6_2atmpS2163;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S331)) {
        int32_t _M0L3valS1112 = _M0L5indexS329->$0;
        int32_t _M0L6_2atmpS1111 = _M0L3valS1112 + 2;
        int32_t _M0L6_2atmpS1114;
        int32_t _M0L6_2atmpS1115;
        int32_t _M0L6_2atmpS1113;
        _M0L5indexS329->$0 = _M0L6_2atmpS1111;
        moonbit_decref(_M0L5indexS329);
        _M0L6_2atmpS1114 = (int32_t)_M0L2c1S330;
        _M0L6_2atmpS1115 = (int32_t)_M0L2c2S331;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS1113
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1114, _M0L6_2atmpS1115);
        return _M0L6_2atmpS1113;
      }
    } else {
      moonbit_decref(_M0L4selfS327.$0);
    }
    _M0L3valS1120 = _M0L5indexS329->$0;
    _M0L6_2atmpS1119 = _M0L3valS1120 + 1;
    _M0L5indexS329->$0 = _M0L6_2atmpS1119;
    moonbit_decref(_M0L5indexS329);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS1121 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S330);
    return _M0L6_2atmpS1121;
  } else {
    moonbit_decref(_M0L5indexS329);
    moonbit_decref(_M0L4selfS327.$0);
    return -1;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS318,
  struct _M0TPB6Logger _M0L6loggerS316
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS317;
  int32_t _M0L3lenS319;
  int32_t _M0L1iS320;
  int32_t _M0L3segS321;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS316.$1) {
    moonbit_incref(_M0L6loggerS316.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS316.$0->$method_3(_M0L6loggerS316.$1, 34);
  moonbit_incref(_M0L4selfS318);
  if (_M0L6loggerS316.$1) {
    moonbit_incref(_M0L6loggerS316.$1);
  }
  _M0L6_2aenvS317
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS317)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS317->$0 = _M0L4selfS318;
  _M0L6_2aenvS317->$1_0 = _M0L6loggerS316.$0;
  _M0L6_2aenvS317->$1_1 = _M0L6loggerS316.$1;
  _M0L3lenS319 = Moonbit_array_length(_M0L4selfS318);
  _M0L1iS320 = 0;
  _M0L3segS321 = 0;
  _2afor_322:;
  while (1) {
    int32_t _M0L4codeS323;
    int32_t _M0L1cS325;
    int32_t _M0L6_2atmpS1087;
    int32_t _M0L6_2atmpS1088;
    int32_t _M0L6_2atmpS1089;
    int32_t _tmp_2326;
    int32_t _tmp_2327;
    if (_M0L1iS320 >= _M0L3lenS319) {
      moonbit_decref(_M0L4selfS318);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
      break;
    }
    _M0L4codeS323 = _M0L4selfS318[_M0L1iS320];
    switch (_M0L4codeS323) {
      case 34: {
        _M0L1cS325 = _M0L4codeS323;
        goto join_324;
        break;
      }
      
      case 92: {
        _M0L1cS325 = _M0L4codeS323;
        goto join_324;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1090;
        int32_t _M0L6_2atmpS1091;
        moonbit_incref(_M0L6_2aenvS317);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
        if (_M0L6loggerS316.$1) {
          moonbit_incref(_M0L6loggerS316.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS316.$0->$method_0(_M0L6loggerS316.$1, (moonbit_string_t)moonbit_string_literal_22.data);
        _M0L6_2atmpS1090 = _M0L1iS320 + 1;
        _M0L6_2atmpS1091 = _M0L1iS320 + 1;
        _M0L1iS320 = _M0L6_2atmpS1090;
        _M0L3segS321 = _M0L6_2atmpS1091;
        goto _2afor_322;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1092;
        int32_t _M0L6_2atmpS1093;
        moonbit_incref(_M0L6_2aenvS317);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
        if (_M0L6loggerS316.$1) {
          moonbit_incref(_M0L6loggerS316.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS316.$0->$method_0(_M0L6loggerS316.$1, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L6_2atmpS1092 = _M0L1iS320 + 1;
        _M0L6_2atmpS1093 = _M0L1iS320 + 1;
        _M0L1iS320 = _M0L6_2atmpS1092;
        _M0L3segS321 = _M0L6_2atmpS1093;
        goto _2afor_322;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1094;
        int32_t _M0L6_2atmpS1095;
        moonbit_incref(_M0L6_2aenvS317);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
        if (_M0L6loggerS316.$1) {
          moonbit_incref(_M0L6loggerS316.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS316.$0->$method_0(_M0L6loggerS316.$1, (moonbit_string_t)moonbit_string_literal_24.data);
        _M0L6_2atmpS1094 = _M0L1iS320 + 1;
        _M0L6_2atmpS1095 = _M0L1iS320 + 1;
        _M0L1iS320 = _M0L6_2atmpS1094;
        _M0L3segS321 = _M0L6_2atmpS1095;
        goto _2afor_322;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1096;
        int32_t _M0L6_2atmpS1097;
        moonbit_incref(_M0L6_2aenvS317);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
        if (_M0L6loggerS316.$1) {
          moonbit_incref(_M0L6loggerS316.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS316.$0->$method_0(_M0L6loggerS316.$1, (moonbit_string_t)moonbit_string_literal_25.data);
        _M0L6_2atmpS1096 = _M0L1iS320 + 1;
        _M0L6_2atmpS1097 = _M0L1iS320 + 1;
        _M0L1iS320 = _M0L6_2atmpS1096;
        _M0L3segS321 = _M0L6_2atmpS1097;
        goto _2afor_322;
        break;
      }
      default: {
        if (_M0L4codeS323 < 32) {
          int32_t _M0L6_2atmpS1099;
          moonbit_string_t _M0L6_2atmpS1098;
          int32_t _M0L6_2atmpS1100;
          int32_t _M0L6_2atmpS1101;
          moonbit_incref(_M0L6_2aenvS317);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
          if (_M0L6loggerS316.$1) {
            moonbit_incref(_M0L6loggerS316.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS316.$0->$method_0(_M0L6loggerS316.$1, (moonbit_string_t)moonbit_string_literal_26.data);
          _M0L6_2atmpS1099 = _M0L4codeS323 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1098 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1099);
          if (_M0L6loggerS316.$1) {
            moonbit_incref(_M0L6loggerS316.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS316.$0->$method_0(_M0L6loggerS316.$1, _M0L6_2atmpS1098);
          if (_M0L6loggerS316.$1) {
            moonbit_incref(_M0L6loggerS316.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS316.$0->$method_3(_M0L6loggerS316.$1, 125);
          _M0L6_2atmpS1100 = _M0L1iS320 + 1;
          _M0L6_2atmpS1101 = _M0L1iS320 + 1;
          _M0L1iS320 = _M0L6_2atmpS1100;
          _M0L3segS321 = _M0L6_2atmpS1101;
          goto _2afor_322;
        } else {
          int32_t _M0L6_2atmpS1102 = _M0L1iS320 + 1;
          int32_t _tmp_2325 = _M0L3segS321;
          _M0L1iS320 = _M0L6_2atmpS1102;
          _M0L3segS321 = _tmp_2325;
          goto _2afor_322;
        }
        break;
      }
    }
    goto joinlet_2324;
    join_324:;
    moonbit_incref(_M0L6_2aenvS317);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS317, _M0L3segS321, _M0L1iS320);
    if (_M0L6loggerS316.$1) {
      moonbit_incref(_M0L6loggerS316.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS316.$0->$method_3(_M0L6loggerS316.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1087 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS325);
    if (_M0L6loggerS316.$1) {
      moonbit_incref(_M0L6loggerS316.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS316.$0->$method_3(_M0L6loggerS316.$1, _M0L6_2atmpS1087);
    _M0L6_2atmpS1088 = _M0L1iS320 + 1;
    _M0L6_2atmpS1089 = _M0L1iS320 + 1;
    _M0L1iS320 = _M0L6_2atmpS1088;
    _M0L3segS321 = _M0L6_2atmpS1089;
    continue;
    joinlet_2324:;
    _tmp_2326 = _M0L1iS320;
    _tmp_2327 = _M0L3segS321;
    _M0L1iS320 = _tmp_2326;
    _M0L3segS321 = _tmp_2327;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS316.$0->$method_3(_M0L6loggerS316.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS312,
  int32_t _M0L3segS315,
  int32_t _M0L1iS314
) {
  struct _M0TPB6Logger _M0L8_2afieldS2170;
  struct _M0TPB6Logger _M0L6loggerS311;
  moonbit_string_t _M0L8_2afieldS2169;
  int32_t _M0L6_2acntS2252;
  moonbit_string_t _M0L4selfS313;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS2170
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS312->$1_0, _M0L6_2aenvS312->$1_1
  };
  _M0L6loggerS311 = _M0L8_2afieldS2170;
  _M0L8_2afieldS2169 = _M0L6_2aenvS312->$0;
  _M0L6_2acntS2252 = Moonbit_object_header(_M0L6_2aenvS312)->rc;
  if (_M0L6_2acntS2252 > 1) {
    int32_t _M0L11_2anew__cntS2253 = _M0L6_2acntS2252 - 1;
    Moonbit_object_header(_M0L6_2aenvS312)->rc = _M0L11_2anew__cntS2253;
    if (_M0L6loggerS311.$1) {
      moonbit_incref(_M0L6loggerS311.$1);
    }
    moonbit_incref(_M0L8_2afieldS2169);
  } else if (_M0L6_2acntS2252 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS312);
  }
  _M0L4selfS313 = _M0L8_2afieldS2169;
  if (_M0L1iS314 > _M0L3segS315) {
    int32_t _M0L6_2atmpS1086 = _M0L1iS314 - _M0L3segS315;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS311.$0->$method_1(_M0L6loggerS311.$1, _M0L4selfS313, _M0L3segS315, _M0L6_2atmpS1086);
  } else {
    moonbit_decref(_M0L4selfS313);
    if (_M0L6loggerS311.$1) {
      moonbit_decref(_M0L6loggerS311.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS310) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS309;
  int32_t _M0L6_2atmpS1083;
  int32_t _M0L6_2atmpS1082;
  int32_t _M0L6_2atmpS1085;
  int32_t _M0L6_2atmpS1084;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1081;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS309 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1083 = _M0IPC14byte4BytePB3Div3div(_M0L1bS310, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1082
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1083);
  moonbit_incref(_M0L7_2aselfS309);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS309, _M0L6_2atmpS1082);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1085 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS310, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1084
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1085);
  moonbit_incref(_M0L7_2aselfS309);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS309, _M0L6_2atmpS1084);
  _M0L6_2atmpS1081 = _M0L7_2aselfS309;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1081);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS308) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS308 < 10) {
    int32_t _M0L6_2atmpS1078;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1078 = _M0IPC14byte4BytePB3Add3add(_M0L1iS308, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1078);
  } else {
    int32_t _M0L6_2atmpS1080;
    int32_t _M0L6_2atmpS1079;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1080 = _M0IPC14byte4BytePB3Add3add(_M0L1iS308, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1079 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1080, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1079);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS306,
  int32_t _M0L4thatS307
) {
  int32_t _M0L6_2atmpS1076;
  int32_t _M0L6_2atmpS1077;
  int32_t _M0L6_2atmpS1075;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1076 = (int32_t)_M0L4selfS306;
  _M0L6_2atmpS1077 = (int32_t)_M0L4thatS307;
  _M0L6_2atmpS1075 = _M0L6_2atmpS1076 - _M0L6_2atmpS1077;
  return _M0L6_2atmpS1075 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS304,
  int32_t _M0L4thatS305
) {
  int32_t _M0L6_2atmpS1073;
  int32_t _M0L6_2atmpS1074;
  int32_t _M0L6_2atmpS1072;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1073 = (int32_t)_M0L4selfS304;
  _M0L6_2atmpS1074 = (int32_t)_M0L4thatS305;
  _M0L6_2atmpS1072 = _M0L6_2atmpS1073 % _M0L6_2atmpS1074;
  return _M0L6_2atmpS1072 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS302,
  int32_t _M0L4thatS303
) {
  int32_t _M0L6_2atmpS1070;
  int32_t _M0L6_2atmpS1071;
  int32_t _M0L6_2atmpS1069;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1070 = (int32_t)_M0L4selfS302;
  _M0L6_2atmpS1071 = (int32_t)_M0L4thatS303;
  _M0L6_2atmpS1069 = _M0L6_2atmpS1070 / _M0L6_2atmpS1071;
  return _M0L6_2atmpS1069 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS300,
  int32_t _M0L4thatS301
) {
  int32_t _M0L6_2atmpS1067;
  int32_t _M0L6_2atmpS1068;
  int32_t _M0L6_2atmpS1066;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1067 = (int32_t)_M0L4selfS300;
  _M0L6_2atmpS1068 = (int32_t)_M0L4thatS301;
  _M0L6_2atmpS1066 = _M0L6_2atmpS1067 + _M0L6_2atmpS1068;
  return _M0L6_2atmpS1066 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS297,
  int32_t _M0L5startS295,
  int32_t _M0L3endS296
) {
  int32_t _if__result_2328;
  int32_t _M0L3lenS298;
  int32_t _M0L6_2atmpS1064;
  int32_t _M0L6_2atmpS1065;
  moonbit_bytes_t _M0L5bytesS299;
  moonbit_bytes_t _M0L6_2atmpS1063;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS295 == 0) {
    int32_t _M0L6_2atmpS1062 = Moonbit_array_length(_M0L3strS297);
    _if__result_2328 = _M0L3endS296 == _M0L6_2atmpS1062;
  } else {
    _if__result_2328 = 0;
  }
  if (_if__result_2328) {
    return _M0L3strS297;
  }
  _M0L3lenS298 = _M0L3endS296 - _M0L5startS295;
  _M0L6_2atmpS1064 = _M0L3lenS298 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1065 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS299
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1064, _M0L6_2atmpS1065);
  moonbit_incref(_M0L5bytesS299);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS299, 0, _M0L3strS297, _M0L5startS295, _M0L3lenS298);
  _M0L6_2atmpS1063 = _M0L5bytesS299;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1063, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS293) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS293;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS294) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS294;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS277,
  int32_t _M0L5radixS276
) {
  int32_t _if__result_2329;
  int32_t _M0L12is__negativeS278;
  uint32_t _M0L3numS279;
  uint16_t* _M0L6bufferS280;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS276 < 2) {
    _if__result_2329 = 1;
  } else {
    _if__result_2329 = _M0L5radixS276 > 36;
  }
  if (_if__result_2329) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_27.data, (moonbit_string_t)moonbit_string_literal_28.data);
  }
  if (_M0L4selfS277 == 0) {
    return (moonbit_string_t)moonbit_string_literal_29.data;
  }
  _M0L12is__negativeS278 = _M0L4selfS277 < 0;
  if (_M0L12is__negativeS278) {
    int32_t _M0L6_2atmpS1061 = -_M0L4selfS277;
    _M0L3numS279 = *(uint32_t*)&_M0L6_2atmpS1061;
  } else {
    _M0L3numS279 = *(uint32_t*)&_M0L4selfS277;
  }
  switch (_M0L5radixS276) {
    case 10: {
      int32_t _M0L10digit__lenS281;
      int32_t _M0L6_2atmpS1058;
      int32_t _M0L10total__lenS282;
      uint16_t* _M0L6bufferS283;
      int32_t _M0L12digit__startS284;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS281 = _M0FPB12dec__count32(_M0L3numS279);
      if (_M0L12is__negativeS278) {
        _M0L6_2atmpS1058 = 1;
      } else {
        _M0L6_2atmpS1058 = 0;
      }
      _M0L10total__lenS282 = _M0L10digit__lenS281 + _M0L6_2atmpS1058;
      _M0L6bufferS283
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS282, 0);
      if (_M0L12is__negativeS278) {
        _M0L12digit__startS284 = 1;
      } else {
        _M0L12digit__startS284 = 0;
      }
      moonbit_incref(_M0L6bufferS283);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS283, _M0L3numS279, _M0L12digit__startS284, _M0L10total__lenS282);
      _M0L6bufferS280 = _M0L6bufferS283;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS285;
      int32_t _M0L6_2atmpS1059;
      int32_t _M0L10total__lenS286;
      uint16_t* _M0L6bufferS287;
      int32_t _M0L12digit__startS288;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS285 = _M0FPB12hex__count32(_M0L3numS279);
      if (_M0L12is__negativeS278) {
        _M0L6_2atmpS1059 = 1;
      } else {
        _M0L6_2atmpS1059 = 0;
      }
      _M0L10total__lenS286 = _M0L10digit__lenS285 + _M0L6_2atmpS1059;
      _M0L6bufferS287
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS286, 0);
      if (_M0L12is__negativeS278) {
        _M0L12digit__startS288 = 1;
      } else {
        _M0L12digit__startS288 = 0;
      }
      moonbit_incref(_M0L6bufferS287);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS287, _M0L3numS279, _M0L12digit__startS288, _M0L10total__lenS286);
      _M0L6bufferS280 = _M0L6bufferS287;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS289;
      int32_t _M0L6_2atmpS1060;
      int32_t _M0L10total__lenS290;
      uint16_t* _M0L6bufferS291;
      int32_t _M0L12digit__startS292;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS289
      = _M0FPB14radix__count32(_M0L3numS279, _M0L5radixS276);
      if (_M0L12is__negativeS278) {
        _M0L6_2atmpS1060 = 1;
      } else {
        _M0L6_2atmpS1060 = 0;
      }
      _M0L10total__lenS290 = _M0L10digit__lenS289 + _M0L6_2atmpS1060;
      _M0L6bufferS291
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS290, 0);
      if (_M0L12is__negativeS278) {
        _M0L12digit__startS292 = 1;
      } else {
        _M0L12digit__startS292 = 0;
      }
      moonbit_incref(_M0L6bufferS291);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS291, _M0L3numS279, _M0L12digit__startS292, _M0L10total__lenS290, _M0L5radixS276);
      _M0L6bufferS280 = _M0L6bufferS291;
      break;
    }
  }
  if (_M0L12is__negativeS278) {
    _M0L6bufferS280[0] = 45;
  }
  return _M0L6bufferS280;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS270,
  int32_t _M0L5radixS273
) {
  uint32_t _M0Lm3numS271;
  uint32_t _M0L4baseS272;
  int32_t _M0Lm5countS274;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS270 == 0u) {
    return 1;
  }
  _M0Lm3numS271 = _M0L5valueS270;
  _M0L4baseS272 = *(uint32_t*)&_M0L5radixS273;
  _M0Lm5countS274 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1055 = _M0Lm3numS271;
    if (_M0L6_2atmpS1055 > 0u) {
      int32_t _M0L6_2atmpS1056 = _M0Lm5countS274;
      uint32_t _M0L6_2atmpS1057;
      _M0Lm5countS274 = _M0L6_2atmpS1056 + 1;
      _M0L6_2atmpS1057 = _M0Lm3numS271;
      _M0Lm3numS271 = _M0L6_2atmpS1057 / _M0L4baseS272;
      continue;
    }
    break;
  }
  return _M0Lm5countS274;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS268) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS268 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS269;
    int32_t _M0L6_2atmpS1054;
    int32_t _M0L6_2atmpS1053;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS269 = moonbit_clz32(_M0L5valueS268);
    _M0L6_2atmpS1054 = 31 - _M0L14leading__zerosS269;
    _M0L6_2atmpS1053 = _M0L6_2atmpS1054 / 4;
    return _M0L6_2atmpS1053 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS267) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS267 >= 100000u) {
    if (_M0L5valueS267 >= 10000000u) {
      if (_M0L5valueS267 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS267 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS267 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS267 >= 1000u) {
    if (_M0L5valueS267 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS267 >= 100u) {
    return 3;
  } else if (_M0L5valueS267 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS257,
  uint32_t _M0L3numS245,
  int32_t _M0L12digit__startS248,
  int32_t _M0L10total__lenS247
) {
  uint32_t _M0Lm3numS244;
  int32_t _M0Lm6offsetS246;
  uint32_t _M0L6_2atmpS1052;
  int32_t _M0Lm9remainingS259;
  int32_t _M0L6_2atmpS1033;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS244 = _M0L3numS245;
  _M0Lm6offsetS246 = _M0L10total__lenS247 - _M0L12digit__startS248;
  while (1) {
    uint32_t _M0L6_2atmpS996 = _M0Lm3numS244;
    if (_M0L6_2atmpS996 >= 10000u) {
      uint32_t _M0L6_2atmpS1019 = _M0Lm3numS244;
      uint32_t _M0L1tS249 = _M0L6_2atmpS1019 / 10000u;
      uint32_t _M0L6_2atmpS1018 = _M0Lm3numS244;
      uint32_t _M0L6_2atmpS1017 = _M0L6_2atmpS1018 % 10000u;
      int32_t _M0L1rS250 = *(int32_t*)&_M0L6_2atmpS1017;
      int32_t _M0L2d1S251;
      int32_t _M0L2d2S252;
      int32_t _M0L6_2atmpS997;
      int32_t _M0L6_2atmpS1016;
      int32_t _M0L6_2atmpS1015;
      int32_t _M0L6d1__hiS253;
      int32_t _M0L6_2atmpS1014;
      int32_t _M0L6_2atmpS1013;
      int32_t _M0L6d1__loS254;
      int32_t _M0L6_2atmpS1012;
      int32_t _M0L6_2atmpS1011;
      int32_t _M0L6d2__hiS255;
      int32_t _M0L6_2atmpS1010;
      int32_t _M0L6_2atmpS1009;
      int32_t _M0L6d2__loS256;
      int32_t _M0L6_2atmpS999;
      int32_t _M0L6_2atmpS998;
      int32_t _M0L6_2atmpS1002;
      int32_t _M0L6_2atmpS1001;
      int32_t _M0L6_2atmpS1000;
      int32_t _M0L6_2atmpS1005;
      int32_t _M0L6_2atmpS1004;
      int32_t _M0L6_2atmpS1003;
      int32_t _M0L6_2atmpS1008;
      int32_t _M0L6_2atmpS1007;
      int32_t _M0L6_2atmpS1006;
      _M0Lm3numS244 = _M0L1tS249;
      _M0L2d1S251 = _M0L1rS250 / 100;
      _M0L2d2S252 = _M0L1rS250 % 100;
      _M0L6_2atmpS997 = _M0Lm6offsetS246;
      _M0Lm6offsetS246 = _M0L6_2atmpS997 - 4;
      _M0L6_2atmpS1016 = _M0L2d1S251 / 10;
      _M0L6_2atmpS1015 = 48 + _M0L6_2atmpS1016;
      _M0L6d1__hiS253 = (uint16_t)_M0L6_2atmpS1015;
      _M0L6_2atmpS1014 = _M0L2d1S251 % 10;
      _M0L6_2atmpS1013 = 48 + _M0L6_2atmpS1014;
      _M0L6d1__loS254 = (uint16_t)_M0L6_2atmpS1013;
      _M0L6_2atmpS1012 = _M0L2d2S252 / 10;
      _M0L6_2atmpS1011 = 48 + _M0L6_2atmpS1012;
      _M0L6d2__hiS255 = (uint16_t)_M0L6_2atmpS1011;
      _M0L6_2atmpS1010 = _M0L2d2S252 % 10;
      _M0L6_2atmpS1009 = 48 + _M0L6_2atmpS1010;
      _M0L6d2__loS256 = (uint16_t)_M0L6_2atmpS1009;
      _M0L6_2atmpS999 = _M0Lm6offsetS246;
      _M0L6_2atmpS998 = _M0L12digit__startS248 + _M0L6_2atmpS999;
      _M0L6bufferS257[_M0L6_2atmpS998] = _M0L6d1__hiS253;
      _M0L6_2atmpS1002 = _M0Lm6offsetS246;
      _M0L6_2atmpS1001 = _M0L12digit__startS248 + _M0L6_2atmpS1002;
      _M0L6_2atmpS1000 = _M0L6_2atmpS1001 + 1;
      _M0L6bufferS257[_M0L6_2atmpS1000] = _M0L6d1__loS254;
      _M0L6_2atmpS1005 = _M0Lm6offsetS246;
      _M0L6_2atmpS1004 = _M0L12digit__startS248 + _M0L6_2atmpS1005;
      _M0L6_2atmpS1003 = _M0L6_2atmpS1004 + 2;
      _M0L6bufferS257[_M0L6_2atmpS1003] = _M0L6d2__hiS255;
      _M0L6_2atmpS1008 = _M0Lm6offsetS246;
      _M0L6_2atmpS1007 = _M0L12digit__startS248 + _M0L6_2atmpS1008;
      _M0L6_2atmpS1006 = _M0L6_2atmpS1007 + 3;
      _M0L6bufferS257[_M0L6_2atmpS1006] = _M0L6d2__loS256;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1052 = _M0Lm3numS244;
  _M0Lm9remainingS259 = *(int32_t*)&_M0L6_2atmpS1052;
  while (1) {
    int32_t _M0L6_2atmpS1020 = _M0Lm9remainingS259;
    if (_M0L6_2atmpS1020 >= 100) {
      int32_t _M0L6_2atmpS1032 = _M0Lm9remainingS259;
      int32_t _M0L1tS260 = _M0L6_2atmpS1032 / 100;
      int32_t _M0L6_2atmpS1031 = _M0Lm9remainingS259;
      int32_t _M0L1dS261 = _M0L6_2atmpS1031 % 100;
      int32_t _M0L6_2atmpS1021;
      int32_t _M0L6_2atmpS1030;
      int32_t _M0L6_2atmpS1029;
      int32_t _M0L5d__hiS262;
      int32_t _M0L6_2atmpS1028;
      int32_t _M0L6_2atmpS1027;
      int32_t _M0L5d__loS263;
      int32_t _M0L6_2atmpS1023;
      int32_t _M0L6_2atmpS1022;
      int32_t _M0L6_2atmpS1026;
      int32_t _M0L6_2atmpS1025;
      int32_t _M0L6_2atmpS1024;
      _M0Lm9remainingS259 = _M0L1tS260;
      _M0L6_2atmpS1021 = _M0Lm6offsetS246;
      _M0Lm6offsetS246 = _M0L6_2atmpS1021 - 2;
      _M0L6_2atmpS1030 = _M0L1dS261 / 10;
      _M0L6_2atmpS1029 = 48 + _M0L6_2atmpS1030;
      _M0L5d__hiS262 = (uint16_t)_M0L6_2atmpS1029;
      _M0L6_2atmpS1028 = _M0L1dS261 % 10;
      _M0L6_2atmpS1027 = 48 + _M0L6_2atmpS1028;
      _M0L5d__loS263 = (uint16_t)_M0L6_2atmpS1027;
      _M0L6_2atmpS1023 = _M0Lm6offsetS246;
      _M0L6_2atmpS1022 = _M0L12digit__startS248 + _M0L6_2atmpS1023;
      _M0L6bufferS257[_M0L6_2atmpS1022] = _M0L5d__hiS262;
      _M0L6_2atmpS1026 = _M0Lm6offsetS246;
      _M0L6_2atmpS1025 = _M0L12digit__startS248 + _M0L6_2atmpS1026;
      _M0L6_2atmpS1024 = _M0L6_2atmpS1025 + 1;
      _M0L6bufferS257[_M0L6_2atmpS1024] = _M0L5d__loS263;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1033 = _M0Lm9remainingS259;
  if (_M0L6_2atmpS1033 >= 10) {
    int32_t _M0L6_2atmpS1034 = _M0Lm6offsetS246;
    int32_t _M0L6_2atmpS1045;
    int32_t _M0L6_2atmpS1044;
    int32_t _M0L6_2atmpS1043;
    int32_t _M0L5d__hiS265;
    int32_t _M0L6_2atmpS1042;
    int32_t _M0L6_2atmpS1041;
    int32_t _M0L6_2atmpS1040;
    int32_t _M0L5d__loS266;
    int32_t _M0L6_2atmpS1036;
    int32_t _M0L6_2atmpS1035;
    int32_t _M0L6_2atmpS1039;
    int32_t _M0L6_2atmpS1038;
    int32_t _M0L6_2atmpS1037;
    _M0Lm6offsetS246 = _M0L6_2atmpS1034 - 2;
    _M0L6_2atmpS1045 = _M0Lm9remainingS259;
    _M0L6_2atmpS1044 = _M0L6_2atmpS1045 / 10;
    _M0L6_2atmpS1043 = 48 + _M0L6_2atmpS1044;
    _M0L5d__hiS265 = (uint16_t)_M0L6_2atmpS1043;
    _M0L6_2atmpS1042 = _M0Lm9remainingS259;
    _M0L6_2atmpS1041 = _M0L6_2atmpS1042 % 10;
    _M0L6_2atmpS1040 = 48 + _M0L6_2atmpS1041;
    _M0L5d__loS266 = (uint16_t)_M0L6_2atmpS1040;
    _M0L6_2atmpS1036 = _M0Lm6offsetS246;
    _M0L6_2atmpS1035 = _M0L12digit__startS248 + _M0L6_2atmpS1036;
    _M0L6bufferS257[_M0L6_2atmpS1035] = _M0L5d__hiS265;
    _M0L6_2atmpS1039 = _M0Lm6offsetS246;
    _M0L6_2atmpS1038 = _M0L12digit__startS248 + _M0L6_2atmpS1039;
    _M0L6_2atmpS1037 = _M0L6_2atmpS1038 + 1;
    _M0L6bufferS257[_M0L6_2atmpS1037] = _M0L5d__loS266;
    moonbit_decref(_M0L6bufferS257);
  } else {
    int32_t _M0L6_2atmpS1046 = _M0Lm6offsetS246;
    int32_t _M0L6_2atmpS1051;
    int32_t _M0L6_2atmpS1047;
    int32_t _M0L6_2atmpS1050;
    int32_t _M0L6_2atmpS1049;
    int32_t _M0L6_2atmpS1048;
    _M0Lm6offsetS246 = _M0L6_2atmpS1046 - 1;
    _M0L6_2atmpS1051 = _M0Lm6offsetS246;
    _M0L6_2atmpS1047 = _M0L12digit__startS248 + _M0L6_2atmpS1051;
    _M0L6_2atmpS1050 = _M0Lm9remainingS259;
    _M0L6_2atmpS1049 = 48 + _M0L6_2atmpS1050;
    _M0L6_2atmpS1048 = (uint16_t)_M0L6_2atmpS1049;
    _M0L6bufferS257[_M0L6_2atmpS1047] = _M0L6_2atmpS1048;
    moonbit_decref(_M0L6bufferS257);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS239,
  uint32_t _M0L3numS233,
  int32_t _M0L12digit__startS231,
  int32_t _M0L10total__lenS230,
  int32_t _M0L5radixS235
) {
  int32_t _M0Lm6offsetS229;
  uint32_t _M0Lm1nS232;
  uint32_t _M0L4baseS234;
  int32_t _M0L6_2atmpS978;
  int32_t _M0L6_2atmpS977;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS229 = _M0L10total__lenS230 - _M0L12digit__startS231;
  _M0Lm1nS232 = _M0L3numS233;
  _M0L4baseS234 = *(uint32_t*)&_M0L5radixS235;
  _M0L6_2atmpS978 = _M0L5radixS235 - 1;
  _M0L6_2atmpS977 = _M0L5radixS235 & _M0L6_2atmpS978;
  if (_M0L6_2atmpS977 == 0) {
    int32_t _M0L5shiftS236;
    uint32_t _M0L4maskS237;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS236 = moonbit_ctz32(_M0L5radixS235);
    _M0L4maskS237 = _M0L4baseS234 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS979 = _M0Lm1nS232;
      if (_M0L6_2atmpS979 > 0u) {
        int32_t _M0L6_2atmpS980 = _M0Lm6offsetS229;
        uint32_t _M0L6_2atmpS986;
        uint32_t _M0L6_2atmpS985;
        int32_t _M0L5digitS238;
        int32_t _M0L6_2atmpS983;
        int32_t _M0L6_2atmpS981;
        int32_t _M0L6_2atmpS982;
        uint32_t _M0L6_2atmpS984;
        _M0Lm6offsetS229 = _M0L6_2atmpS980 - 1;
        _M0L6_2atmpS986 = _M0Lm1nS232;
        _M0L6_2atmpS985 = _M0L6_2atmpS986 & _M0L4maskS237;
        _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS985;
        _M0L6_2atmpS983 = _M0Lm6offsetS229;
        _M0L6_2atmpS981 = _M0L12digit__startS231 + _M0L6_2atmpS983;
        _M0L6_2atmpS982
        = ((moonbit_string_t)moonbit_string_literal_30.data)[
          _M0L5digitS238
        ];
        _M0L6bufferS239[_M0L6_2atmpS981] = _M0L6_2atmpS982;
        _M0L6_2atmpS984 = _M0Lm1nS232;
        _M0Lm1nS232 = _M0L6_2atmpS984 >> (_M0L5shiftS236 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS239);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS987 = _M0Lm1nS232;
      if (_M0L6_2atmpS987 > 0u) {
        int32_t _M0L6_2atmpS988 = _M0Lm6offsetS229;
        uint32_t _M0L6_2atmpS995;
        uint32_t _M0L1qS241;
        uint32_t _M0L6_2atmpS993;
        uint32_t _M0L6_2atmpS994;
        uint32_t _M0L6_2atmpS992;
        int32_t _M0L5digitS242;
        int32_t _M0L6_2atmpS991;
        int32_t _M0L6_2atmpS989;
        int32_t _M0L6_2atmpS990;
        _M0Lm6offsetS229 = _M0L6_2atmpS988 - 1;
        _M0L6_2atmpS995 = _M0Lm1nS232;
        _M0L1qS241 = _M0L6_2atmpS995 / _M0L4baseS234;
        _M0L6_2atmpS993 = _M0Lm1nS232;
        _M0L6_2atmpS994 = _M0L1qS241 * _M0L4baseS234;
        _M0L6_2atmpS992 = _M0L6_2atmpS993 - _M0L6_2atmpS994;
        _M0L5digitS242 = *(int32_t*)&_M0L6_2atmpS992;
        _M0L6_2atmpS991 = _M0Lm6offsetS229;
        _M0L6_2atmpS989 = _M0L12digit__startS231 + _M0L6_2atmpS991;
        _M0L6_2atmpS990
        = ((moonbit_string_t)moonbit_string_literal_30.data)[
          _M0L5digitS242
        ];
        _M0L6bufferS239[_M0L6_2atmpS989] = _M0L6_2atmpS990;
        _M0Lm1nS232 = _M0L1qS241;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS239);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS226,
  uint32_t _M0L3numS222,
  int32_t _M0L12digit__startS220,
  int32_t _M0L10total__lenS219
) {
  int32_t _M0Lm6offsetS218;
  uint32_t _M0Lm1nS221;
  int32_t _M0L6_2atmpS973;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS218 = _M0L10total__lenS219 - _M0L12digit__startS220;
  _M0Lm1nS221 = _M0L3numS222;
  while (1) {
    int32_t _M0L6_2atmpS961 = _M0Lm6offsetS218;
    if (_M0L6_2atmpS961 >= 2) {
      int32_t _M0L6_2atmpS962 = _M0Lm6offsetS218;
      uint32_t _M0L6_2atmpS972;
      uint32_t _M0L6_2atmpS971;
      int32_t _M0L9byte__valS223;
      int32_t _M0L2hiS224;
      int32_t _M0L2loS225;
      int32_t _M0L6_2atmpS965;
      int32_t _M0L6_2atmpS963;
      int32_t _M0L6_2atmpS964;
      int32_t _M0L6_2atmpS969;
      int32_t _M0L6_2atmpS968;
      int32_t _M0L6_2atmpS966;
      int32_t _M0L6_2atmpS967;
      uint32_t _M0L6_2atmpS970;
      _M0Lm6offsetS218 = _M0L6_2atmpS962 - 2;
      _M0L6_2atmpS972 = _M0Lm1nS221;
      _M0L6_2atmpS971 = _M0L6_2atmpS972 & 255u;
      _M0L9byte__valS223 = *(int32_t*)&_M0L6_2atmpS971;
      _M0L2hiS224 = _M0L9byte__valS223 / 16;
      _M0L2loS225 = _M0L9byte__valS223 % 16;
      _M0L6_2atmpS965 = _M0Lm6offsetS218;
      _M0L6_2atmpS963 = _M0L12digit__startS220 + _M0L6_2atmpS965;
      _M0L6_2atmpS964
      = ((moonbit_string_t)moonbit_string_literal_30.data)[
        _M0L2hiS224
      ];
      _M0L6bufferS226[_M0L6_2atmpS963] = _M0L6_2atmpS964;
      _M0L6_2atmpS969 = _M0Lm6offsetS218;
      _M0L6_2atmpS968 = _M0L12digit__startS220 + _M0L6_2atmpS969;
      _M0L6_2atmpS966 = _M0L6_2atmpS968 + 1;
      _M0L6_2atmpS967
      = ((moonbit_string_t)moonbit_string_literal_30.data)[
        _M0L2loS225
      ];
      _M0L6bufferS226[_M0L6_2atmpS966] = _M0L6_2atmpS967;
      _M0L6_2atmpS970 = _M0Lm1nS221;
      _M0Lm1nS221 = _M0L6_2atmpS970 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS973 = _M0Lm6offsetS218;
  if (_M0L6_2atmpS973 == 1) {
    uint32_t _M0L6_2atmpS976 = _M0Lm1nS221;
    uint32_t _M0L6_2atmpS975 = _M0L6_2atmpS976 & 15u;
    int32_t _M0L6nibbleS228 = *(int32_t*)&_M0L6_2atmpS975;
    int32_t _M0L6_2atmpS974 =
      ((moonbit_string_t)moonbit_string_literal_30.data)[_M0L6nibbleS228];
    _M0L6bufferS226[_M0L12digit__startS220] = _M0L6_2atmpS974;
    moonbit_decref(_M0L6bufferS226);
  } else {
    moonbit_decref(_M0L6bufferS226);
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGsE(
  struct _M0TPB6Logger _M0L4selfS201,
  struct _M0TWEOs* _M0L4iterS205,
  moonbit_string_t _M0L6prefixS202,
  moonbit_string_t _M0L6suffixS217,
  moonbit_string_t _M0L3sepS208,
  int32_t _M0L8trailingS203
) {
  #line 156 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  if (_M0L4selfS201.$1) {
    moonbit_incref(_M0L4selfS201.$1);
  }
  #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L4selfS201.$0->$method_0(_M0L4selfS201.$1, _M0L6prefixS202);
  if (_M0L8trailingS203) {
    while (1) {
      moonbit_string_t _M0L7_2abindS204;
      moonbit_incref(_M0L4iterS205);
      #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
      _M0L7_2abindS204 = _M0MPB4Iter4nextGsE(_M0L4iterS205);
      if (_M0L7_2abindS204 == 0) {
        moonbit_decref(_M0L3sepS208);
        moonbit_decref(_M0L4iterS205);
        if (_M0L7_2abindS204) {
          moonbit_decref(_M0L7_2abindS204);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS206 = _M0L7_2abindS204;
        moonbit_string_t _M0L4_2axS207 = _M0L7_2aSomeS206;
        if (_M0L4selfS201.$1) {
          moonbit_incref(_M0L4selfS201.$1);
        }
        #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0MPB6Logger13write__objectGsE(_M0L4selfS201, _M0L4_2axS207);
        moonbit_incref(_M0L3sepS208);
        if (_M0L4selfS201.$1) {
          moonbit_incref(_M0L4selfS201.$1);
        }
        #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0L4selfS201.$0->$method_0(_M0L4selfS201.$1, _M0L3sepS208);
        continue;
      }
      break;
    }
  } else {
    moonbit_string_t _M0L7_2abindS210;
    moonbit_incref(_M0L4iterS205);
    #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
    _M0L7_2abindS210 = _M0MPB4Iter4nextGsE(_M0L4iterS205);
    if (_M0L7_2abindS210 == 0) {
      if (_M0L7_2abindS210) {
        moonbit_decref(_M0L7_2abindS210);
      }
      moonbit_decref(_M0L3sepS208);
      moonbit_decref(_M0L4iterS205);
    } else {
      moonbit_string_t _M0L7_2aSomeS211 = _M0L7_2abindS210;
      moonbit_string_t _M0L4_2axS212 = _M0L7_2aSomeS211;
      if (_M0L4selfS201.$1) {
        moonbit_incref(_M0L4selfS201.$1);
      }
      #line 171 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
      _M0MPB6Logger13write__objectGsE(_M0L4selfS201, _M0L4_2axS212);
      while (1) {
        moonbit_string_t _M0L7_2abindS213;
        moonbit_incref(_M0L4iterS205);
        #line 172 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
        _M0L7_2abindS213 = _M0MPB4Iter4nextGsE(_M0L4iterS205);
        if (_M0L7_2abindS213 == 0) {
          if (_M0L7_2abindS213) {
            moonbit_decref(_M0L7_2abindS213);
          }
          moonbit_decref(_M0L3sepS208);
          moonbit_decref(_M0L4iterS205);
        } else {
          moonbit_string_t _M0L7_2aSomeS214 = _M0L7_2abindS213;
          moonbit_string_t _M0L4_2axS215 = _M0L7_2aSomeS214;
          moonbit_incref(_M0L3sepS208);
          if (_M0L4selfS201.$1) {
            moonbit_incref(_M0L4selfS201.$1);
          }
          #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0L4selfS201.$0->$method_0(_M0L4selfS201.$1, _M0L3sepS208);
          if (_M0L4selfS201.$1) {
            moonbit_incref(_M0L4selfS201.$1);
          }
          #line 174 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
          _M0MPB6Logger13write__objectGsE(_M0L4selfS201, _M0L4_2axS215);
          continue;
        }
        break;
      }
    }
  }
  #line 177 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L4selfS201.$0->$method_0(_M0L4selfS201.$1, _M0L6suffixS217);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS198) {
  struct _M0TWEOs* _M0L7_2afuncS197;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS197 = _M0L4selfS198;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS197->code(_M0L7_2afuncS197);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS200) {
  struct _M0TWEOc* _M0L7_2afuncS199;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS199 = _M0L4selfS200;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS199->code(_M0L7_2afuncS199);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(
  struct _M0TPB5ArrayGsE* _M0L4selfS190
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS189;
  struct _M0TPB6Logger _M0L6_2atmpS957;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS189 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS189);
  _M0L6_2atmpS957
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS189
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC15array5ArrayPB4Show6outputGsE(_M0L4selfS190, _M0L6_2atmpS957);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS189);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS192
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS191;
  struct _M0TPB6Logger _M0L6_2atmpS958;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS191 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS191);
  _M0L6_2atmpS958
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS191
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS192, _M0L6_2atmpS958);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS191);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS194
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS193;
  struct _M0TPB6Logger _M0L6_2atmpS959;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS193 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS193);
  _M0L6_2atmpS959
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS193
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS194, _M0L6_2atmpS959);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS193);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS196
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS195;
  struct _M0TPB6Logger _M0L6_2atmpS960;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS195 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS195);
  _M0L6_2atmpS960
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS195
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS196, _M0L6_2atmpS960);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS195);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS188
) {
  int32_t _M0L8_2afieldS2171;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2171 = _M0L4selfS188.$1;
  moonbit_decref(_M0L4selfS188.$0);
  return _M0L8_2afieldS2171;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS187
) {
  int32_t _M0L3endS955;
  int32_t _M0L8_2afieldS2172;
  int32_t _M0L5startS956;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS955 = _M0L4selfS187.$2;
  _M0L8_2afieldS2172 = _M0L4selfS187.$1;
  moonbit_decref(_M0L4selfS187.$0);
  _M0L5startS956 = _M0L8_2afieldS2172;
  return _M0L3endS955 - _M0L5startS956;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS186
) {
  moonbit_string_t _M0L8_2afieldS2173;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2173 = _M0L4selfS186.$0;
  return _M0L8_2afieldS2173;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS182,
  moonbit_string_t _M0L5valueS183,
  int32_t _M0L5startS184,
  int32_t _M0L3lenS185
) {
  int32_t _M0L6_2atmpS954;
  int64_t _M0L6_2atmpS953;
  struct _M0TPC16string10StringView _M0L6_2atmpS952;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS954 = _M0L5startS184 + _M0L3lenS185;
  _M0L6_2atmpS953 = (int64_t)_M0L6_2atmpS954;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS952
  = _M0MPC16string6String11sub_2einner(_M0L5valueS183, _M0L5startS184, _M0L6_2atmpS953);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS182, _M0L6_2atmpS952);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS175,
  int32_t _M0L5startS181,
  int64_t _M0L3endS177
) {
  int32_t _M0L3lenS174;
  int32_t _M0L3endS176;
  int32_t _M0L5startS180;
  int32_t _if__result_2338;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS174 = Moonbit_array_length(_M0L4selfS175);
  if (_M0L3endS177 == 4294967296ll) {
    _M0L3endS176 = _M0L3lenS174;
  } else {
    int64_t _M0L7_2aSomeS178 = _M0L3endS177;
    int32_t _M0L6_2aendS179 = (int32_t)_M0L7_2aSomeS178;
    if (_M0L6_2aendS179 < 0) {
      _M0L3endS176 = _M0L3lenS174 + _M0L6_2aendS179;
    } else {
      _M0L3endS176 = _M0L6_2aendS179;
    }
  }
  if (_M0L5startS181 < 0) {
    _M0L5startS180 = _M0L3lenS174 + _M0L5startS181;
  } else {
    _M0L5startS180 = _M0L5startS181;
  }
  if (_M0L5startS180 >= 0) {
    if (_M0L5startS180 <= _M0L3endS176) {
      _if__result_2338 = _M0L3endS176 <= _M0L3lenS174;
    } else {
      _if__result_2338 = 0;
    }
  } else {
    _if__result_2338 = 0;
  }
  if (_if__result_2338) {
    if (_M0L5startS180 < _M0L3lenS174) {
      int32_t _M0L6_2atmpS949 = _M0L4selfS175[_M0L5startS180];
      int32_t _M0L6_2atmpS948;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS948
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS949);
      if (!_M0L6_2atmpS948) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS176 < _M0L3lenS174) {
      int32_t _M0L6_2atmpS951 = _M0L4selfS175[_M0L3endS176];
      int32_t _M0L6_2atmpS950;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS950
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS951);
      if (!_M0L6_2atmpS950) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS180,
                                                 _M0L3endS176,
                                                 _M0L4selfS175};
  } else {
    moonbit_decref(_M0L4selfS175);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L3strS173
) {
  int32_t _M0L3lenS938;
  int32_t _M0L6_2atmpS940;
  int32_t _M0L6_2atmpS939;
  int32_t _M0L6_2atmpS937;
  moonbit_bytes_t _M0L8_2afieldS2175;
  moonbit_bytes_t _M0L4dataS941;
  int32_t _M0L3lenS942;
  int32_t _M0L6_2atmpS943;
  int32_t _M0L3lenS945;
  int32_t _M0L6_2atmpS2174;
  int32_t _M0L6_2atmpS947;
  int32_t _M0L6_2atmpS946;
  int32_t _M0L6_2atmpS944;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS938 = _M0L4selfS172->$1;
  _M0L6_2atmpS940 = Moonbit_array_length(_M0L3strS173);
  _M0L6_2atmpS939 = _M0L6_2atmpS940 * 2;
  _M0L6_2atmpS937 = _M0L3lenS938 + _M0L6_2atmpS939;
  moonbit_incref(_M0L4selfS172);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS172, _M0L6_2atmpS937);
  _M0L8_2afieldS2175 = _M0L4selfS172->$0;
  _M0L4dataS941 = _M0L8_2afieldS2175;
  _M0L3lenS942 = _M0L4selfS172->$1;
  _M0L6_2atmpS943 = Moonbit_array_length(_M0L3strS173);
  moonbit_incref(_M0L4dataS941);
  moonbit_incref(_M0L3strS173);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS941, _M0L3lenS942, _M0L3strS173, 0, _M0L6_2atmpS943);
  _M0L3lenS945 = _M0L4selfS172->$1;
  _M0L6_2atmpS2174 = Moonbit_array_length(_M0L3strS173);
  moonbit_decref(_M0L3strS173);
  _M0L6_2atmpS947 = _M0L6_2atmpS2174;
  _M0L6_2atmpS946 = _M0L6_2atmpS947 * 2;
  _M0L6_2atmpS944 = _M0L3lenS945 + _M0L6_2atmpS946;
  _M0L4selfS172->$1 = _M0L6_2atmpS944;
  moonbit_decref(_M0L4selfS172);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS164,
  int32_t _M0L13bytes__offsetS159,
  moonbit_string_t _M0L3strS166,
  int32_t _M0L11str__offsetS162,
  int32_t _M0L6lengthS160
) {
  int32_t _M0L6_2atmpS936;
  int32_t _M0L6_2atmpS935;
  int32_t _M0L2e1S158;
  int32_t _M0L6_2atmpS934;
  int32_t _M0L2e2S161;
  int32_t _M0L4len1S163;
  int32_t _M0L4len2S165;
  int32_t _if__result_2339;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS936 = _M0L6lengthS160 * 2;
  _M0L6_2atmpS935 = _M0L13bytes__offsetS159 + _M0L6_2atmpS936;
  _M0L2e1S158 = _M0L6_2atmpS935 - 1;
  _M0L6_2atmpS934 = _M0L11str__offsetS162 + _M0L6lengthS160;
  _M0L2e2S161 = _M0L6_2atmpS934 - 1;
  _M0L4len1S163 = Moonbit_array_length(_M0L4selfS164);
  _M0L4len2S165 = Moonbit_array_length(_M0L3strS166);
  if (_M0L6lengthS160 >= 0) {
    if (_M0L13bytes__offsetS159 >= 0) {
      if (_M0L2e1S158 < _M0L4len1S163) {
        if (_M0L11str__offsetS162 >= 0) {
          _if__result_2339 = _M0L2e2S161 < _M0L4len2S165;
        } else {
          _if__result_2339 = 0;
        }
      } else {
        _if__result_2339 = 0;
      }
    } else {
      _if__result_2339 = 0;
    }
  } else {
    _if__result_2339 = 0;
  }
  if (_if__result_2339) {
    int32_t _M0L16end__str__offsetS167 =
      _M0L11str__offsetS162 + _M0L6lengthS160;
    int32_t _M0L1iS168 = _M0L11str__offsetS162;
    int32_t _M0L1jS169 = _M0L13bytes__offsetS159;
    while (1) {
      if (_M0L1iS168 < _M0L16end__str__offsetS167) {
        int32_t _M0L6_2atmpS931 = _M0L3strS166[_M0L1iS168];
        int32_t _M0L6_2atmpS930 = (int32_t)_M0L6_2atmpS931;
        uint32_t _M0L1cS170 = *(uint32_t*)&_M0L6_2atmpS930;
        uint32_t _M0L6_2atmpS926 = _M0L1cS170 & 255u;
        int32_t _M0L6_2atmpS925;
        int32_t _M0L6_2atmpS927;
        uint32_t _M0L6_2atmpS929;
        int32_t _M0L6_2atmpS928;
        int32_t _M0L6_2atmpS932;
        int32_t _M0L6_2atmpS933;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS925 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS926);
        if (
          _M0L1jS169 < 0 || _M0L1jS169 >= Moonbit_array_length(_M0L4selfS164)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS164[_M0L1jS169] = _M0L6_2atmpS925;
        _M0L6_2atmpS927 = _M0L1jS169 + 1;
        _M0L6_2atmpS929 = _M0L1cS170 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS928 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS929);
        if (
          _M0L6_2atmpS927 < 0
          || _M0L6_2atmpS927 >= Moonbit_array_length(_M0L4selfS164)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS164[_M0L6_2atmpS927] = _M0L6_2atmpS928;
        _M0L6_2atmpS932 = _M0L1iS168 + 1;
        _M0L6_2atmpS933 = _M0L1jS169 + 2;
        _M0L1iS168 = _M0L6_2atmpS932;
        _M0L1jS169 = _M0L6_2atmpS933;
        continue;
      } else {
        moonbit_decref(_M0L3strS166);
        moonbit_decref(_M0L4selfS164);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS166);
    moonbit_decref(_M0L4selfS164);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS104
) {
  int32_t _M0L6_2atmpS924;
  struct _M0TPC16string10StringView _M0L7_2abindS103;
  moonbit_string_t _M0L7_2adataS105;
  int32_t _M0L8_2astartS106;
  int32_t _M0L6_2atmpS923;
  int32_t _M0L6_2aendS107;
  int32_t _M0Lm9_2acursorS108;
  int32_t _M0Lm13accept__stateS109;
  int32_t _M0Lm10match__endS110;
  int32_t _M0Lm20match__tag__saver__0S111;
  int32_t _M0Lm20match__tag__saver__1S112;
  int32_t _M0Lm20match__tag__saver__2S113;
  int32_t _M0Lm20match__tag__saver__3S114;
  int32_t _M0Lm20match__tag__saver__4S115;
  int32_t _M0Lm6tag__0S116;
  int32_t _M0Lm6tag__1S117;
  int32_t _M0Lm9tag__1__1S118;
  int32_t _M0Lm9tag__1__2S119;
  int32_t _M0Lm6tag__3S120;
  int32_t _M0Lm6tag__2S121;
  int32_t _M0Lm9tag__2__1S122;
  int32_t _M0Lm6tag__4S123;
  int32_t _M0L6_2atmpS881;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS924 = Moonbit_array_length(_M0L4reprS104);
  _M0L7_2abindS103
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS924, _M0L4reprS104
  };
  moonbit_incref(_M0L7_2abindS103.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS105 = _M0MPC16string10StringView4data(_M0L7_2abindS103);
  moonbit_incref(_M0L7_2abindS103.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS106
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS103);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS923 = _M0MPC16string10StringView6length(_M0L7_2abindS103);
  _M0L6_2aendS107 = _M0L8_2astartS106 + _M0L6_2atmpS923;
  _M0Lm9_2acursorS108 = _M0L8_2astartS106;
  _M0Lm13accept__stateS109 = -1;
  _M0Lm10match__endS110 = -1;
  _M0Lm20match__tag__saver__0S111 = -1;
  _M0Lm20match__tag__saver__1S112 = -1;
  _M0Lm20match__tag__saver__2S113 = -1;
  _M0Lm20match__tag__saver__3S114 = -1;
  _M0Lm20match__tag__saver__4S115 = -1;
  _M0Lm6tag__0S116 = -1;
  _M0Lm6tag__1S117 = -1;
  _M0Lm9tag__1__1S118 = -1;
  _M0Lm9tag__1__2S119 = -1;
  _M0Lm6tag__3S120 = -1;
  _M0Lm6tag__2S121 = -1;
  _M0Lm9tag__2__1S122 = -1;
  _M0Lm6tag__4S123 = -1;
  _M0L6_2atmpS881 = _M0Lm9_2acursorS108;
  if (_M0L6_2atmpS881 < _M0L6_2aendS107) {
    int32_t _M0L6_2atmpS883 = _M0Lm9_2acursorS108;
    int32_t _M0L6_2atmpS882;
    moonbit_incref(_M0L7_2adataS105);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS882
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS883);
    if (_M0L6_2atmpS882 == 64) {
      int32_t _M0L6_2atmpS884 = _M0Lm9_2acursorS108;
      _M0Lm9_2acursorS108 = _M0L6_2atmpS884 + 1;
      while (1) {
        int32_t _M0L6_2atmpS885;
        _M0Lm6tag__0S116 = _M0Lm9_2acursorS108;
        _M0L6_2atmpS885 = _M0Lm9_2acursorS108;
        if (_M0L6_2atmpS885 < _M0L6_2aendS107) {
          int32_t _M0L6_2atmpS922 = _M0Lm9_2acursorS108;
          int32_t _M0L10next__charS131;
          int32_t _M0L6_2atmpS886;
          moonbit_incref(_M0L7_2adataS105);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS131
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS922);
          _M0L6_2atmpS886 = _M0Lm9_2acursorS108;
          _M0Lm9_2acursorS108 = _M0L6_2atmpS886 + 1;
          if (_M0L10next__charS131 == 58) {
            int32_t _M0L6_2atmpS887 = _M0Lm9_2acursorS108;
            if (_M0L6_2atmpS887 < _M0L6_2aendS107) {
              int32_t _M0L6_2atmpS888 = _M0Lm9_2acursorS108;
              int32_t _M0L12dispatch__15S132;
              _M0Lm9_2acursorS108 = _M0L6_2atmpS888 + 1;
              _M0L12dispatch__15S132 = 0;
              loop__label__15_135:;
              while (1) {
                int32_t _M0L6_2atmpS889;
                switch (_M0L12dispatch__15S132) {
                  case 3: {
                    int32_t _M0L6_2atmpS892;
                    _M0Lm9tag__1__2S119 = _M0Lm9tag__1__1S118;
                    _M0Lm9tag__1__1S118 = _M0Lm6tag__1S117;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS892 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS892 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS897 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS139;
                      int32_t _M0L6_2atmpS893;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS139
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS897);
                      _M0L6_2atmpS893 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS893 + 1;
                      if (_M0L10next__charS139 < 58) {
                        if (_M0L10next__charS139 < 48) {
                          goto join_138;
                        } else {
                          int32_t _M0L6_2atmpS894;
                          _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                          _M0Lm9tag__2__1S122 = _M0Lm6tag__2S121;
                          _M0Lm6tag__2S121 = _M0Lm9_2acursorS108;
                          _M0Lm6tag__3S120 = _M0Lm9_2acursorS108;
                          _M0L6_2atmpS894 = _M0Lm9_2acursorS108;
                          if (_M0L6_2atmpS894 < _M0L6_2aendS107) {
                            int32_t _M0L6_2atmpS896 = _M0Lm9_2acursorS108;
                            int32_t _M0L10next__charS141;
                            int32_t _M0L6_2atmpS895;
                            moonbit_incref(_M0L7_2adataS105);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS141
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS896);
                            _M0L6_2atmpS895 = _M0Lm9_2acursorS108;
                            _M0Lm9_2acursorS108 = _M0L6_2atmpS895 + 1;
                            if (_M0L10next__charS141 < 48) {
                              if (_M0L10next__charS141 == 45) {
                                goto join_133;
                              } else {
                                goto join_140;
                              }
                            } else if (_M0L10next__charS141 > 57) {
                              if (_M0L10next__charS141 < 59) {
                                _M0L12dispatch__15S132 = 3;
                                goto loop__label__15_135;
                              } else {
                                goto join_140;
                              }
                            } else {
                              _M0L12dispatch__15S132 = 6;
                              goto loop__label__15_135;
                            }
                            join_140:;
                            _M0L12dispatch__15S132 = 0;
                            goto loop__label__15_135;
                          } else {
                            goto join_124;
                          }
                        }
                      } else if (_M0L10next__charS139 > 58) {
                        goto join_138;
                      } else {
                        _M0L12dispatch__15S132 = 1;
                        goto loop__label__15_135;
                      }
                      join_138:;
                      _M0L12dispatch__15S132 = 0;
                      goto loop__label__15_135;
                    } else {
                      goto join_124;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS898;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0Lm6tag__2S121 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS898 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS898 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS900 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS143;
                      int32_t _M0L6_2atmpS899;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS143
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS900);
                      _M0L6_2atmpS899 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS899 + 1;
                      if (_M0L10next__charS143 < 58) {
                        if (_M0L10next__charS143 < 48) {
                          goto join_142;
                        } else {
                          _M0L12dispatch__15S132 = 2;
                          goto loop__label__15_135;
                        }
                      } else if (_M0L10next__charS143 > 58) {
                        goto join_142;
                      } else {
                        _M0L12dispatch__15S132 = 3;
                        goto loop__label__15_135;
                      }
                      join_142:;
                      _M0L12dispatch__15S132 = 0;
                      goto loop__label__15_135;
                    } else {
                      goto join_124;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS901;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS901 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS901 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS903 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS144;
                      int32_t _M0L6_2atmpS902;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS144
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS903);
                      _M0L6_2atmpS902 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS902 + 1;
                      if (_M0L10next__charS144 == 58) {
                        _M0L12dispatch__15S132 = 1;
                        goto loop__label__15_135;
                      } else {
                        _M0L12dispatch__15S132 = 0;
                        goto loop__label__15_135;
                      }
                    } else {
                      goto join_124;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS904;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0Lm6tag__4S123 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS904 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS904 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS912 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS146;
                      int32_t _M0L6_2atmpS905;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS146
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS912);
                      _M0L6_2atmpS905 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS905 + 1;
                      if (_M0L10next__charS146 < 58) {
                        if (_M0L10next__charS146 < 48) {
                          goto join_145;
                        } else {
                          _M0L12dispatch__15S132 = 4;
                          goto loop__label__15_135;
                        }
                      } else if (_M0L10next__charS146 > 58) {
                        goto join_145;
                      } else {
                        int32_t _M0L6_2atmpS906;
                        _M0Lm9tag__1__2S119 = _M0Lm9tag__1__1S118;
                        _M0Lm9tag__1__1S118 = _M0Lm6tag__1S117;
                        _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                        _M0L6_2atmpS906 = _M0Lm9_2acursorS108;
                        if (_M0L6_2atmpS906 < _M0L6_2aendS107) {
                          int32_t _M0L6_2atmpS911 = _M0Lm9_2acursorS108;
                          int32_t _M0L10next__charS148;
                          int32_t _M0L6_2atmpS907;
                          moonbit_incref(_M0L7_2adataS105);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS148
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS911);
                          _M0L6_2atmpS907 = _M0Lm9_2acursorS108;
                          _M0Lm9_2acursorS108 = _M0L6_2atmpS907 + 1;
                          if (_M0L10next__charS148 < 58) {
                            if (_M0L10next__charS148 < 48) {
                              goto join_147;
                            } else {
                              int32_t _M0L6_2atmpS908;
                              _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                              _M0Lm9tag__2__1S122 = _M0Lm6tag__2S121;
                              _M0Lm6tag__2S121 = _M0Lm9_2acursorS108;
                              _M0L6_2atmpS908 = _M0Lm9_2acursorS108;
                              if (_M0L6_2atmpS908 < _M0L6_2aendS107) {
                                int32_t _M0L6_2atmpS910 = _M0Lm9_2acursorS108;
                                int32_t _M0L10next__charS150;
                                int32_t _M0L6_2atmpS909;
                                moonbit_incref(_M0L7_2adataS105);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS150
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS910);
                                _M0L6_2atmpS909 = _M0Lm9_2acursorS108;
                                _M0Lm9_2acursorS108 = _M0L6_2atmpS909 + 1;
                                if (_M0L10next__charS150 < 58) {
                                  if (_M0L10next__charS150 < 48) {
                                    goto join_149;
                                  } else {
                                    _M0L12dispatch__15S132 = 5;
                                    goto loop__label__15_135;
                                  }
                                } else if (_M0L10next__charS150 > 58) {
                                  goto join_149;
                                } else {
                                  _M0L12dispatch__15S132 = 3;
                                  goto loop__label__15_135;
                                }
                                join_149:;
                                _M0L12dispatch__15S132 = 0;
                                goto loop__label__15_135;
                              } else {
                                goto join_137;
                              }
                            }
                          } else if (_M0L10next__charS148 > 58) {
                            goto join_147;
                          } else {
                            _M0L12dispatch__15S132 = 1;
                            goto loop__label__15_135;
                          }
                          join_147:;
                          _M0L12dispatch__15S132 = 0;
                          goto loop__label__15_135;
                        } else {
                          goto join_124;
                        }
                      }
                      join_145:;
                      _M0L12dispatch__15S132 = 0;
                      goto loop__label__15_135;
                    } else {
                      goto join_124;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS913;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0Lm6tag__2S121 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS913 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS913 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS915 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS152;
                      int32_t _M0L6_2atmpS914;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS152
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS915);
                      _M0L6_2atmpS914 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS914 + 1;
                      if (_M0L10next__charS152 < 58) {
                        if (_M0L10next__charS152 < 48) {
                          goto join_151;
                        } else {
                          _M0L12dispatch__15S132 = 5;
                          goto loop__label__15_135;
                        }
                      } else if (_M0L10next__charS152 > 58) {
                        goto join_151;
                      } else {
                        _M0L12dispatch__15S132 = 3;
                        goto loop__label__15_135;
                      }
                      join_151:;
                      _M0L12dispatch__15S132 = 0;
                      goto loop__label__15_135;
                    } else {
                      goto join_137;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS916;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0Lm6tag__2S121 = _M0Lm9_2acursorS108;
                    _M0Lm6tag__3S120 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS916 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS916 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS918 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS154;
                      int32_t _M0L6_2atmpS917;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS154
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS918);
                      _M0L6_2atmpS917 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS917 + 1;
                      if (_M0L10next__charS154 < 48) {
                        if (_M0L10next__charS154 == 45) {
                          goto join_133;
                        } else {
                          goto join_153;
                        }
                      } else if (_M0L10next__charS154 > 57) {
                        if (_M0L10next__charS154 < 59) {
                          _M0L12dispatch__15S132 = 3;
                          goto loop__label__15_135;
                        } else {
                          goto join_153;
                        }
                      } else {
                        _M0L12dispatch__15S132 = 6;
                        goto loop__label__15_135;
                      }
                      join_153:;
                      _M0L12dispatch__15S132 = 0;
                      goto loop__label__15_135;
                    } else {
                      goto join_124;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS919;
                    _M0Lm9tag__1__1S118 = _M0Lm6tag__1S117;
                    _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                    _M0L6_2atmpS919 = _M0Lm9_2acursorS108;
                    if (_M0L6_2atmpS919 < _M0L6_2aendS107) {
                      int32_t _M0L6_2atmpS921 = _M0Lm9_2acursorS108;
                      int32_t _M0L10next__charS156;
                      int32_t _M0L6_2atmpS920;
                      moonbit_incref(_M0L7_2adataS105);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS156
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS921);
                      _M0L6_2atmpS920 = _M0Lm9_2acursorS108;
                      _M0Lm9_2acursorS108 = _M0L6_2atmpS920 + 1;
                      if (_M0L10next__charS156 < 58) {
                        if (_M0L10next__charS156 < 48) {
                          goto join_155;
                        } else {
                          _M0L12dispatch__15S132 = 2;
                          goto loop__label__15_135;
                        }
                      } else if (_M0L10next__charS156 > 58) {
                        goto join_155;
                      } else {
                        _M0L12dispatch__15S132 = 1;
                        goto loop__label__15_135;
                      }
                      join_155:;
                      _M0L12dispatch__15S132 = 0;
                      goto loop__label__15_135;
                    } else {
                      goto join_124;
                    }
                    break;
                  }
                  default: {
                    goto join_124;
                    break;
                  }
                }
                join_137:;
                _M0Lm6tag__1S117 = _M0Lm9tag__1__2S119;
                _M0Lm6tag__2S121 = _M0Lm9tag__2__1S122;
                _M0Lm20match__tag__saver__0S111 = _M0Lm6tag__0S116;
                _M0Lm20match__tag__saver__1S112 = _M0Lm6tag__1S117;
                _M0Lm20match__tag__saver__2S113 = _M0Lm6tag__2S121;
                _M0Lm20match__tag__saver__3S114 = _M0Lm6tag__3S120;
                _M0Lm20match__tag__saver__4S115 = _M0Lm6tag__4S123;
                _M0Lm13accept__stateS109 = 0;
                _M0Lm10match__endS110 = _M0Lm9_2acursorS108;
                goto join_124;
                join_133:;
                _M0Lm9tag__1__1S118 = _M0Lm9tag__1__2S119;
                _M0Lm6tag__1S117 = _M0Lm9_2acursorS108;
                _M0Lm6tag__2S121 = _M0Lm9tag__2__1S122;
                _M0L6_2atmpS889 = _M0Lm9_2acursorS108;
                if (_M0L6_2atmpS889 < _M0L6_2aendS107) {
                  int32_t _M0L6_2atmpS891 = _M0Lm9_2acursorS108;
                  int32_t _M0L10next__charS136;
                  int32_t _M0L6_2atmpS890;
                  moonbit_incref(_M0L7_2adataS105);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS136
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS105, _M0L6_2atmpS891);
                  _M0L6_2atmpS890 = _M0Lm9_2acursorS108;
                  _M0Lm9_2acursorS108 = _M0L6_2atmpS890 + 1;
                  if (_M0L10next__charS136 < 58) {
                    if (_M0L10next__charS136 < 48) {
                      goto join_134;
                    } else {
                      _M0L12dispatch__15S132 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS136 > 58) {
                    goto join_134;
                  } else {
                    _M0L12dispatch__15S132 = 1;
                    continue;
                  }
                  join_134:;
                  _M0L12dispatch__15S132 = 0;
                  continue;
                } else {
                  goto join_124;
                }
                break;
              }
            } else {
              goto join_124;
            }
          } else {
            continue;
          }
        } else {
          goto join_124;
        }
        break;
      }
    } else {
      goto join_124;
    }
  } else {
    goto join_124;
  }
  join_124:;
  switch (_M0Lm13accept__stateS109) {
    case 0: {
      int32_t _M0L6_2atmpS880 = _M0Lm20match__tag__saver__1S112;
      int32_t _M0L6_2atmpS879 = _M0L6_2atmpS880 + 1;
      int64_t _M0L6_2atmpS876 = (int64_t)_M0L6_2atmpS879;
      int32_t _M0L6_2atmpS878 = _M0Lm20match__tag__saver__2S113;
      int64_t _M0L6_2atmpS877 = (int64_t)_M0L6_2atmpS878;
      struct _M0TPC16string10StringView _M0L11start__lineS125;
      int32_t _M0L6_2atmpS875;
      int32_t _M0L6_2atmpS874;
      int64_t _M0L6_2atmpS871;
      int32_t _M0L6_2atmpS873;
      int64_t _M0L6_2atmpS872;
      struct _M0TPC16string10StringView _M0L13start__columnS126;
      int32_t _M0L6_2atmpS870;
      int64_t _M0L6_2atmpS867;
      int32_t _M0L6_2atmpS869;
      int64_t _M0L6_2atmpS868;
      struct _M0TPC16string10StringView _M0L3pkgS127;
      int32_t _M0L6_2atmpS866;
      int32_t _M0L6_2atmpS865;
      int64_t _M0L6_2atmpS862;
      int32_t _M0L6_2atmpS864;
      int64_t _M0L6_2atmpS863;
      struct _M0TPC16string10StringView _M0L8filenameS128;
      int32_t _M0L6_2atmpS861;
      int32_t _M0L6_2atmpS860;
      int64_t _M0L6_2atmpS857;
      int32_t _M0L6_2atmpS859;
      int64_t _M0L6_2atmpS858;
      struct _M0TPC16string10StringView _M0L9end__lineS129;
      int32_t _M0L6_2atmpS856;
      int32_t _M0L6_2atmpS855;
      int64_t _M0L6_2atmpS852;
      int32_t _M0L6_2atmpS854;
      int64_t _M0L6_2atmpS853;
      struct _M0TPC16string10StringView _M0L11end__columnS130;
      struct _M0TPB13SourceLocRepr* _block_2356;
      moonbit_incref(_M0L7_2adataS105);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS125
      = _M0MPC16string6String4view(_M0L7_2adataS105, _M0L6_2atmpS876, _M0L6_2atmpS877);
      _M0L6_2atmpS875 = _M0Lm20match__tag__saver__2S113;
      _M0L6_2atmpS874 = _M0L6_2atmpS875 + 1;
      _M0L6_2atmpS871 = (int64_t)_M0L6_2atmpS874;
      _M0L6_2atmpS873 = _M0Lm20match__tag__saver__3S114;
      _M0L6_2atmpS872 = (int64_t)_M0L6_2atmpS873;
      moonbit_incref(_M0L7_2adataS105);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS126
      = _M0MPC16string6String4view(_M0L7_2adataS105, _M0L6_2atmpS871, _M0L6_2atmpS872);
      _M0L6_2atmpS870 = _M0L8_2astartS106 + 1;
      _M0L6_2atmpS867 = (int64_t)_M0L6_2atmpS870;
      _M0L6_2atmpS869 = _M0Lm20match__tag__saver__0S111;
      _M0L6_2atmpS868 = (int64_t)_M0L6_2atmpS869;
      moonbit_incref(_M0L7_2adataS105);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS127
      = _M0MPC16string6String4view(_M0L7_2adataS105, _M0L6_2atmpS867, _M0L6_2atmpS868);
      _M0L6_2atmpS866 = _M0Lm20match__tag__saver__0S111;
      _M0L6_2atmpS865 = _M0L6_2atmpS866 + 1;
      _M0L6_2atmpS862 = (int64_t)_M0L6_2atmpS865;
      _M0L6_2atmpS864 = _M0Lm20match__tag__saver__1S112;
      _M0L6_2atmpS863 = (int64_t)_M0L6_2atmpS864;
      moonbit_incref(_M0L7_2adataS105);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS128
      = _M0MPC16string6String4view(_M0L7_2adataS105, _M0L6_2atmpS862, _M0L6_2atmpS863);
      _M0L6_2atmpS861 = _M0Lm20match__tag__saver__3S114;
      _M0L6_2atmpS860 = _M0L6_2atmpS861 + 1;
      _M0L6_2atmpS857 = (int64_t)_M0L6_2atmpS860;
      _M0L6_2atmpS859 = _M0Lm20match__tag__saver__4S115;
      _M0L6_2atmpS858 = (int64_t)_M0L6_2atmpS859;
      moonbit_incref(_M0L7_2adataS105);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS129
      = _M0MPC16string6String4view(_M0L7_2adataS105, _M0L6_2atmpS857, _M0L6_2atmpS858);
      _M0L6_2atmpS856 = _M0Lm20match__tag__saver__4S115;
      _M0L6_2atmpS855 = _M0L6_2atmpS856 + 1;
      _M0L6_2atmpS852 = (int64_t)_M0L6_2atmpS855;
      _M0L6_2atmpS854 = _M0Lm10match__endS110;
      _M0L6_2atmpS853 = (int64_t)_M0L6_2atmpS854;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS130
      = _M0MPC16string6String4view(_M0L7_2adataS105, _M0L6_2atmpS852, _M0L6_2atmpS853);
      _block_2356
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2356)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_2356->$0_0 = _M0L3pkgS127.$0;
      _block_2356->$0_1 = _M0L3pkgS127.$1;
      _block_2356->$0_2 = _M0L3pkgS127.$2;
      _block_2356->$1_0 = _M0L8filenameS128.$0;
      _block_2356->$1_1 = _M0L8filenameS128.$1;
      _block_2356->$1_2 = _M0L8filenameS128.$2;
      _block_2356->$2_0 = _M0L11start__lineS125.$0;
      _block_2356->$2_1 = _M0L11start__lineS125.$1;
      _block_2356->$2_2 = _M0L11start__lineS125.$2;
      _block_2356->$3_0 = _M0L13start__columnS126.$0;
      _block_2356->$3_1 = _M0L13start__columnS126.$1;
      _block_2356->$3_2 = _M0L13start__columnS126.$2;
      _block_2356->$4_0 = _M0L9end__lineS129.$0;
      _block_2356->$4_1 = _M0L9end__lineS129.$1;
      _block_2356->$4_2 = _M0L9end__lineS129.$2;
      _block_2356->$5_0 = _M0L11end__columnS130.$0;
      _block_2356->$5_1 = _M0L11end__columnS130.$1;
      _block_2356->$5_2 = _M0L11end__columnS130.$2;
      return _block_2356;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS105);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS101,
  int32_t _M0L5indexS102
) {
  int32_t _M0L3lenS100;
  int32_t _if__result_2357;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS100 = _M0L4selfS101->$1;
  if (_M0L5indexS102 >= 0) {
    _if__result_2357 = _M0L5indexS102 < _M0L3lenS100;
  } else {
    _if__result_2357 = 0;
  }
  if (_if__result_2357) {
    moonbit_string_t* _M0L6_2atmpS851;
    moonbit_string_t _M0L6_2atmpS2176;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS851 = _M0MPC15array5Array6bufferGsE(_M0L4selfS101);
    if (
      _M0L5indexS102 < 0
      || _M0L5indexS102 >= Moonbit_array_length(_M0L6_2atmpS851)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2176 = (moonbit_string_t)_M0L6_2atmpS851[_M0L5indexS102];
    moonbit_incref(_M0L6_2atmpS2176);
    moonbit_decref(_M0L6_2atmpS851);
    return _M0L6_2atmpS2176;
  } else {
    moonbit_decref(_M0L4selfS101);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS98
) {
  moonbit_string_t* _M0L8_2afieldS2177;
  int32_t _M0L6_2acntS2254;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2177 = _M0L4selfS98->$0;
  _M0L6_2acntS2254 = Moonbit_object_header(_M0L4selfS98)->rc;
  if (_M0L6_2acntS2254 > 1) {
    int32_t _M0L11_2anew__cntS2255 = _M0L6_2acntS2254 - 1;
    Moonbit_object_header(_M0L4selfS98)->rc = _M0L11_2anew__cntS2255;
    moonbit_incref(_M0L8_2afieldS2177);
  } else if (_M0L6_2acntS2254 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS98);
  }
  return _M0L8_2afieldS2177;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS99
) {
  struct _M0TUsiE** _M0L8_2afieldS2178;
  int32_t _M0L6_2acntS2256;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2178 = _M0L4selfS99->$0;
  _M0L6_2acntS2256 = Moonbit_object_header(_M0L4selfS99)->rc;
  if (_M0L6_2acntS2256 > 1) {
    int32_t _M0L11_2anew__cntS2257 = _M0L6_2acntS2256 - 1;
    Moonbit_object_header(_M0L4selfS99)->rc = _M0L11_2anew__cntS2257;
    moonbit_incref(_M0L8_2afieldS2178);
  } else if (_M0L6_2acntS2256 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS99);
  }
  return _M0L8_2afieldS2178;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS97) {
  struct _M0TPB13StringBuilder* _M0L3bufS96;
  struct _M0TPB6Logger _M0L6_2atmpS850;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS96 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS96);
  _M0L6_2atmpS850
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS96
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS97, _M0L6_2atmpS850);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS96);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS95) {
  int32_t _M0L6_2atmpS849;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS849 = (int32_t)_M0L4selfS95;
  return _M0L6_2atmpS849;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS93,
  int32_t _M0L8trailingS94
) {
  int32_t _M0L6_2atmpS848;
  int32_t _M0L6_2atmpS847;
  int32_t _M0L6_2atmpS846;
  int32_t _M0L6_2atmpS845;
  int32_t _M0L6_2atmpS844;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS848 = _M0L7leadingS93 - 55296;
  _M0L6_2atmpS847 = _M0L6_2atmpS848 * 1024;
  _M0L6_2atmpS846 = _M0L6_2atmpS847 + _M0L8trailingS94;
  _M0L6_2atmpS845 = _M0L6_2atmpS846 - 56320;
  _M0L6_2atmpS844 = _M0L6_2atmpS845 + 65536;
  return _M0L6_2atmpS844;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS92) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS92 >= 56320) {
    return _M0L4selfS92 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS91) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS91 >= 55296) {
    return _M0L4selfS91 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L2chS90
) {
  int32_t _M0L3lenS839;
  int32_t _M0L6_2atmpS838;
  moonbit_bytes_t _M0L8_2afieldS2179;
  moonbit_bytes_t _M0L4dataS842;
  int32_t _M0L3lenS843;
  int32_t _M0L3incS89;
  int32_t _M0L3lenS841;
  int32_t _M0L6_2atmpS840;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS839 = _M0L4selfS88->$1;
  _M0L6_2atmpS838 = _M0L3lenS839 + 4;
  moonbit_incref(_M0L4selfS88);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS88, _M0L6_2atmpS838);
  _M0L8_2afieldS2179 = _M0L4selfS88->$0;
  _M0L4dataS842 = _M0L8_2afieldS2179;
  _M0L3lenS843 = _M0L4selfS88->$1;
  moonbit_incref(_M0L4dataS842);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS89
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS842, _M0L3lenS843, _M0L2chS90);
  _M0L3lenS841 = _M0L4selfS88->$1;
  _M0L6_2atmpS840 = _M0L3lenS841 + _M0L3incS89;
  _M0L4selfS88->$1 = _M0L6_2atmpS840;
  moonbit_decref(_M0L4selfS88);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS83,
  int32_t _M0L8requiredS84
) {
  moonbit_bytes_t _M0L8_2afieldS2183;
  moonbit_bytes_t _M0L4dataS837;
  int32_t _M0L6_2atmpS2182;
  int32_t _M0L12current__lenS82;
  int32_t _M0Lm13enough__spaceS85;
  int32_t _M0L6_2atmpS835;
  int32_t _M0L6_2atmpS836;
  moonbit_bytes_t _M0L9new__dataS87;
  moonbit_bytes_t _M0L8_2afieldS2181;
  moonbit_bytes_t _M0L4dataS833;
  int32_t _M0L3lenS834;
  moonbit_bytes_t _M0L6_2aoldS2180;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2183 = _M0L4selfS83->$0;
  _M0L4dataS837 = _M0L8_2afieldS2183;
  _M0L6_2atmpS2182 = Moonbit_array_length(_M0L4dataS837);
  _M0L12current__lenS82 = _M0L6_2atmpS2182;
  if (_M0L8requiredS84 <= _M0L12current__lenS82) {
    moonbit_decref(_M0L4selfS83);
    return 0;
  }
  _M0Lm13enough__spaceS85 = _M0L12current__lenS82;
  while (1) {
    int32_t _M0L6_2atmpS831 = _M0Lm13enough__spaceS85;
    if (_M0L6_2atmpS831 < _M0L8requiredS84) {
      int32_t _M0L6_2atmpS832 = _M0Lm13enough__spaceS85;
      _M0Lm13enough__spaceS85 = _M0L6_2atmpS832 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS835 = _M0Lm13enough__spaceS85;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS836 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS87
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS835, _M0L6_2atmpS836);
  _M0L8_2afieldS2181 = _M0L4selfS83->$0;
  _M0L4dataS833 = _M0L8_2afieldS2181;
  _M0L3lenS834 = _M0L4selfS83->$1;
  moonbit_incref(_M0L4dataS833);
  moonbit_incref(_M0L9new__dataS87);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS87, 0, _M0L4dataS833, 0, _M0L3lenS834);
  _M0L6_2aoldS2180 = _M0L4selfS83->$0;
  moonbit_decref(_M0L6_2aoldS2180);
  _M0L4selfS83->$0 = _M0L9new__dataS87;
  moonbit_decref(_M0L4selfS83);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS77,
  int32_t _M0L6offsetS78,
  int32_t _M0L5valueS76
) {
  uint32_t _M0L4codeS75;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS75 = _M0MPC14char4Char8to__uint(_M0L5valueS76);
  if (_M0L4codeS75 < 65536u) {
    uint32_t _M0L6_2atmpS814 = _M0L4codeS75 & 255u;
    int32_t _M0L6_2atmpS813;
    int32_t _M0L6_2atmpS815;
    uint32_t _M0L6_2atmpS817;
    int32_t _M0L6_2atmpS816;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS813 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS814);
    if (
      _M0L6offsetS78 < 0
      || _M0L6offsetS78 >= Moonbit_array_length(_M0L4selfS77)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS77[_M0L6offsetS78] = _M0L6_2atmpS813;
    _M0L6_2atmpS815 = _M0L6offsetS78 + 1;
    _M0L6_2atmpS817 = _M0L4codeS75 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS816 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS817);
    if (
      _M0L6_2atmpS815 < 0
      || _M0L6_2atmpS815 >= Moonbit_array_length(_M0L4selfS77)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS77[_M0L6_2atmpS815] = _M0L6_2atmpS816;
    moonbit_decref(_M0L4selfS77);
    return 2;
  } else if (_M0L4codeS75 < 1114112u) {
    uint32_t _M0L2hiS79 = _M0L4codeS75 - 65536u;
    uint32_t _M0L6_2atmpS830 = _M0L2hiS79 >> 10;
    uint32_t _M0L2loS80 = _M0L6_2atmpS830 | 55296u;
    uint32_t _M0L6_2atmpS829 = _M0L2hiS79 & 1023u;
    uint32_t _M0L2hiS81 = _M0L6_2atmpS829 | 56320u;
    uint32_t _M0L6_2atmpS819 = _M0L2loS80 & 255u;
    int32_t _M0L6_2atmpS818;
    int32_t _M0L6_2atmpS820;
    uint32_t _M0L6_2atmpS822;
    int32_t _M0L6_2atmpS821;
    int32_t _M0L6_2atmpS823;
    uint32_t _M0L6_2atmpS825;
    int32_t _M0L6_2atmpS824;
    int32_t _M0L6_2atmpS826;
    uint32_t _M0L6_2atmpS828;
    int32_t _M0L6_2atmpS827;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS818 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS819);
    if (
      _M0L6offsetS78 < 0
      || _M0L6offsetS78 >= Moonbit_array_length(_M0L4selfS77)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS77[_M0L6offsetS78] = _M0L6_2atmpS818;
    _M0L6_2atmpS820 = _M0L6offsetS78 + 1;
    _M0L6_2atmpS822 = _M0L2loS80 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS821 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS822);
    if (
      _M0L6_2atmpS820 < 0
      || _M0L6_2atmpS820 >= Moonbit_array_length(_M0L4selfS77)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS77[_M0L6_2atmpS820] = _M0L6_2atmpS821;
    _M0L6_2atmpS823 = _M0L6offsetS78 + 2;
    _M0L6_2atmpS825 = _M0L2hiS81 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS824 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS825);
    if (
      _M0L6_2atmpS823 < 0
      || _M0L6_2atmpS823 >= Moonbit_array_length(_M0L4selfS77)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS77[_M0L6_2atmpS823] = _M0L6_2atmpS824;
    _M0L6_2atmpS826 = _M0L6offsetS78 + 3;
    _M0L6_2atmpS828 = _M0L2hiS81 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS827 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS828);
    if (
      _M0L6_2atmpS826 < 0
      || _M0L6_2atmpS826 >= Moonbit_array_length(_M0L4selfS77)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS77[_M0L6_2atmpS826] = _M0L6_2atmpS827;
    moonbit_decref(_M0L4selfS77);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS77);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_31.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS74) {
  int32_t _M0L6_2atmpS812;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS812 = *(int32_t*)&_M0L4selfS74;
  return _M0L6_2atmpS812 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS73) {
  int32_t _M0L6_2atmpS811;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS811 = _M0L4selfS73;
  return *(uint32_t*)&_M0L6_2atmpS811;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS72
) {
  moonbit_bytes_t _M0L8_2afieldS2185;
  moonbit_bytes_t _M0L4dataS810;
  moonbit_bytes_t _M0L6_2atmpS807;
  int32_t _M0L8_2afieldS2184;
  int32_t _M0L3lenS809;
  int64_t _M0L6_2atmpS808;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2185 = _M0L4selfS72->$0;
  _M0L4dataS810 = _M0L8_2afieldS2185;
  moonbit_incref(_M0L4dataS810);
  _M0L6_2atmpS807 = _M0L4dataS810;
  _M0L8_2afieldS2184 = _M0L4selfS72->$1;
  moonbit_decref(_M0L4selfS72);
  _M0L3lenS809 = _M0L8_2afieldS2184;
  _M0L6_2atmpS808 = (int64_t)_M0L3lenS809;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS807, 0, _M0L6_2atmpS808);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS67,
  int32_t _M0L6offsetS71,
  int64_t _M0L6lengthS69
) {
  int32_t _M0L3lenS66;
  int32_t _M0L6lengthS68;
  int32_t _if__result_2359;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS66 = Moonbit_array_length(_M0L4selfS67);
  if (_M0L6lengthS69 == 4294967296ll) {
    _M0L6lengthS68 = _M0L3lenS66 - _M0L6offsetS71;
  } else {
    int64_t _M0L7_2aSomeS70 = _M0L6lengthS69;
    _M0L6lengthS68 = (int32_t)_M0L7_2aSomeS70;
  }
  if (_M0L6offsetS71 >= 0) {
    if (_M0L6lengthS68 >= 0) {
      int32_t _M0L6_2atmpS806 = _M0L6offsetS71 + _M0L6lengthS68;
      _if__result_2359 = _M0L6_2atmpS806 <= _M0L3lenS66;
    } else {
      _if__result_2359 = 0;
    }
  } else {
    _if__result_2359 = 0;
  }
  if (_if__result_2359) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS67, _M0L6offsetS71, _M0L6lengthS68);
  } else {
    moonbit_decref(_M0L4selfS67);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS64
) {
  int32_t _M0L7initialS63;
  moonbit_bytes_t _M0L4dataS65;
  struct _M0TPB13StringBuilder* _block_2360;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS64 < 1) {
    _M0L7initialS63 = 1;
  } else {
    _M0L7initialS63 = _M0L10size__hintS64;
  }
  _M0L4dataS65 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS63, 0);
  _block_2360
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2360)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2360->$0 = _M0L4dataS65;
  _block_2360->$1 = 0;
  return _block_2360;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS62) {
  int32_t _M0L6_2atmpS805;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS805 = (int32_t)_M0L4selfS62;
  return _M0L6_2atmpS805;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS52,
  int32_t _M0L11dst__offsetS53,
  moonbit_string_t* _M0L3srcS54,
  int32_t _M0L11src__offsetS55,
  int32_t _M0L3lenS56
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS52, _M0L11dst__offsetS53, _M0L3srcS54, _M0L11src__offsetS55, _M0L3lenS56);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS57,
  int32_t _M0L11dst__offsetS58,
  struct _M0TUsiE** _M0L3srcS59,
  int32_t _M0L11src__offsetS60,
  int32_t _M0L3lenS61
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS57, _M0L11dst__offsetS58, _M0L3srcS59, _M0L11src__offsetS60, _M0L3lenS61);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS25,
  int32_t _M0L11dst__offsetS27,
  moonbit_bytes_t _M0L3srcS26,
  int32_t _M0L11src__offsetS28,
  int32_t _M0L3lenS30
) {
  int32_t _if__result_2361;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS25 == _M0L3srcS26) {
    _if__result_2361 = _M0L11dst__offsetS27 < _M0L11src__offsetS28;
  } else {
    _if__result_2361 = 0;
  }
  if (_if__result_2361) {
    int32_t _M0L1iS29 = 0;
    while (1) {
      if (_M0L1iS29 < _M0L3lenS30) {
        int32_t _M0L6_2atmpS778 = _M0L11dst__offsetS27 + _M0L1iS29;
        int32_t _M0L6_2atmpS780 = _M0L11src__offsetS28 + _M0L1iS29;
        int32_t _M0L6_2atmpS779;
        int32_t _M0L6_2atmpS781;
        if (
          _M0L6_2atmpS780 < 0
          || _M0L6_2atmpS780 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS779 = (int32_t)_M0L3srcS26[_M0L6_2atmpS780];
        if (
          _M0L6_2atmpS778 < 0
          || _M0L6_2atmpS778 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS25[_M0L6_2atmpS778] = _M0L6_2atmpS779;
        _M0L6_2atmpS781 = _M0L1iS29 + 1;
        _M0L1iS29 = _M0L6_2atmpS781;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS786 = _M0L3lenS30 - 1;
    int32_t _M0L1iS32 = _M0L6_2atmpS786;
    while (1) {
      if (_M0L1iS32 >= 0) {
        int32_t _M0L6_2atmpS782 = _M0L11dst__offsetS27 + _M0L1iS32;
        int32_t _M0L6_2atmpS784 = _M0L11src__offsetS28 + _M0L1iS32;
        int32_t _M0L6_2atmpS783;
        int32_t _M0L6_2atmpS785;
        if (
          _M0L6_2atmpS784 < 0
          || _M0L6_2atmpS784 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS783 = (int32_t)_M0L3srcS26[_M0L6_2atmpS784];
        if (
          _M0L6_2atmpS782 < 0
          || _M0L6_2atmpS782 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS25[_M0L6_2atmpS782] = _M0L6_2atmpS783;
        _M0L6_2atmpS785 = _M0L1iS32 - 1;
        _M0L1iS32 = _M0L6_2atmpS785;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS34,
  int32_t _M0L11dst__offsetS36,
  moonbit_string_t* _M0L3srcS35,
  int32_t _M0L11src__offsetS37,
  int32_t _M0L3lenS39
) {
  int32_t _if__result_2364;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS34 == _M0L3srcS35) {
    _if__result_2364 = _M0L11dst__offsetS36 < _M0L11src__offsetS37;
  } else {
    _if__result_2364 = 0;
  }
  if (_if__result_2364) {
    int32_t _M0L1iS38 = 0;
    while (1) {
      if (_M0L1iS38 < _M0L3lenS39) {
        int32_t _M0L6_2atmpS787 = _M0L11dst__offsetS36 + _M0L1iS38;
        int32_t _M0L6_2atmpS789 = _M0L11src__offsetS37 + _M0L1iS38;
        moonbit_string_t _M0L6_2atmpS2187;
        moonbit_string_t _M0L6_2atmpS788;
        moonbit_string_t _M0L6_2aoldS2186;
        int32_t _M0L6_2atmpS790;
        if (
          _M0L6_2atmpS789 < 0
          || _M0L6_2atmpS789 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2187 = (moonbit_string_t)_M0L3srcS35[_M0L6_2atmpS789];
        _M0L6_2atmpS788 = _M0L6_2atmpS2187;
        if (
          _M0L6_2atmpS787 < 0
          || _M0L6_2atmpS787 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2186 = (moonbit_string_t)_M0L3dstS34[_M0L6_2atmpS787];
        moonbit_incref(_M0L6_2atmpS788);
        moonbit_decref(_M0L6_2aoldS2186);
        _M0L3dstS34[_M0L6_2atmpS787] = _M0L6_2atmpS788;
        _M0L6_2atmpS790 = _M0L1iS38 + 1;
        _M0L1iS38 = _M0L6_2atmpS790;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS795 = _M0L3lenS39 - 1;
    int32_t _M0L1iS41 = _M0L6_2atmpS795;
    while (1) {
      if (_M0L1iS41 >= 0) {
        int32_t _M0L6_2atmpS791 = _M0L11dst__offsetS36 + _M0L1iS41;
        int32_t _M0L6_2atmpS793 = _M0L11src__offsetS37 + _M0L1iS41;
        moonbit_string_t _M0L6_2atmpS2189;
        moonbit_string_t _M0L6_2atmpS792;
        moonbit_string_t _M0L6_2aoldS2188;
        int32_t _M0L6_2atmpS794;
        if (
          _M0L6_2atmpS793 < 0
          || _M0L6_2atmpS793 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2189 = (moonbit_string_t)_M0L3srcS35[_M0L6_2atmpS793];
        _M0L6_2atmpS792 = _M0L6_2atmpS2189;
        if (
          _M0L6_2atmpS791 < 0
          || _M0L6_2atmpS791 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2188 = (moonbit_string_t)_M0L3dstS34[_M0L6_2atmpS791];
        moonbit_incref(_M0L6_2atmpS792);
        moonbit_decref(_M0L6_2aoldS2188);
        _M0L3dstS34[_M0L6_2atmpS791] = _M0L6_2atmpS792;
        _M0L6_2atmpS794 = _M0L1iS41 - 1;
        _M0L1iS41 = _M0L6_2atmpS794;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS43,
  int32_t _M0L11dst__offsetS45,
  struct _M0TUsiE** _M0L3srcS44,
  int32_t _M0L11src__offsetS46,
  int32_t _M0L3lenS48
) {
  int32_t _if__result_2367;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS43 == _M0L3srcS44) {
    _if__result_2367 = _M0L11dst__offsetS45 < _M0L11src__offsetS46;
  } else {
    _if__result_2367 = 0;
  }
  if (_if__result_2367) {
    int32_t _M0L1iS47 = 0;
    while (1) {
      if (_M0L1iS47 < _M0L3lenS48) {
        int32_t _M0L6_2atmpS796 = _M0L11dst__offsetS45 + _M0L1iS47;
        int32_t _M0L6_2atmpS798 = _M0L11src__offsetS46 + _M0L1iS47;
        struct _M0TUsiE* _M0L6_2atmpS2191;
        struct _M0TUsiE* _M0L6_2atmpS797;
        struct _M0TUsiE* _M0L6_2aoldS2190;
        int32_t _M0L6_2atmpS799;
        if (
          _M0L6_2atmpS798 < 0
          || _M0L6_2atmpS798 >= Moonbit_array_length(_M0L3srcS44)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2191 = (struct _M0TUsiE*)_M0L3srcS44[_M0L6_2atmpS798];
        _M0L6_2atmpS797 = _M0L6_2atmpS2191;
        if (
          _M0L6_2atmpS796 < 0
          || _M0L6_2atmpS796 >= Moonbit_array_length(_M0L3dstS43)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2190 = (struct _M0TUsiE*)_M0L3dstS43[_M0L6_2atmpS796];
        if (_M0L6_2atmpS797) {
          moonbit_incref(_M0L6_2atmpS797);
        }
        if (_M0L6_2aoldS2190) {
          moonbit_decref(_M0L6_2aoldS2190);
        }
        _M0L3dstS43[_M0L6_2atmpS796] = _M0L6_2atmpS797;
        _M0L6_2atmpS799 = _M0L1iS47 + 1;
        _M0L1iS47 = _M0L6_2atmpS799;
        continue;
      } else {
        moonbit_decref(_M0L3srcS44);
        moonbit_decref(_M0L3dstS43);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS804 = _M0L3lenS48 - 1;
    int32_t _M0L1iS50 = _M0L6_2atmpS804;
    while (1) {
      if (_M0L1iS50 >= 0) {
        int32_t _M0L6_2atmpS800 = _M0L11dst__offsetS45 + _M0L1iS50;
        int32_t _M0L6_2atmpS802 = _M0L11src__offsetS46 + _M0L1iS50;
        struct _M0TUsiE* _M0L6_2atmpS2193;
        struct _M0TUsiE* _M0L6_2atmpS801;
        struct _M0TUsiE* _M0L6_2aoldS2192;
        int32_t _M0L6_2atmpS803;
        if (
          _M0L6_2atmpS802 < 0
          || _M0L6_2atmpS802 >= Moonbit_array_length(_M0L3srcS44)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2193 = (struct _M0TUsiE*)_M0L3srcS44[_M0L6_2atmpS802];
        _M0L6_2atmpS801 = _M0L6_2atmpS2193;
        if (
          _M0L6_2atmpS800 < 0
          || _M0L6_2atmpS800 >= Moonbit_array_length(_M0L3dstS43)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2192 = (struct _M0TUsiE*)_M0L3dstS43[_M0L6_2atmpS800];
        if (_M0L6_2atmpS801) {
          moonbit_incref(_M0L6_2atmpS801);
        }
        if (_M0L6_2aoldS2192) {
          moonbit_decref(_M0L6_2aoldS2192);
        }
        _M0L3dstS43[_M0L6_2atmpS800] = _M0L6_2atmpS801;
        _M0L6_2atmpS803 = _M0L1iS50 - 1;
        _M0L1iS50 = _M0L6_2atmpS803;
        continue;
      } else {
        moonbit_decref(_M0L3srcS44);
        moonbit_decref(_M0L3dstS43);
      }
      break;
    }
  }
  return 0;
}

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L6stringS13,
  moonbit_string_t _M0L3locS14
) {
  moonbit_string_t _M0L6_2atmpS752;
  moonbit_string_t _M0L6_2atmpS2196;
  moonbit_string_t _M0L6_2atmpS750;
  moonbit_string_t _M0L6_2atmpS751;
  moonbit_string_t _M0L6_2atmpS2195;
  moonbit_string_t _M0L6_2atmpS749;
  moonbit_string_t _M0L6_2atmpS2194;
  moonbit_string_t _M0L6_2atmpS748;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS752 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS13);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2196
  = moonbit_add_string(_M0L6_2atmpS752, (moonbit_string_t)moonbit_string_literal_32.data);
  moonbit_decref(_M0L6_2atmpS752);
  _M0L6_2atmpS750 = _M0L6_2atmpS2196;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS751
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS14);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2195 = moonbit_add_string(_M0L6_2atmpS750, _M0L6_2atmpS751);
  moonbit_decref(_M0L6_2atmpS750);
  moonbit_decref(_M0L6_2atmpS751);
  _M0L6_2atmpS749 = _M0L6_2atmpS2195;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2194
  = moonbit_add_string(_M0L6_2atmpS749, (moonbit_string_t)moonbit_string_literal_33.data);
  moonbit_decref(_M0L6_2atmpS749);
  _M0L6_2atmpS748 = _M0L6_2atmpS2194;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC15bytes9BytesViewE(_M0L6_2atmpS748);
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS15,
  moonbit_string_t _M0L3locS16
) {
  moonbit_string_t _M0L6_2atmpS757;
  moonbit_string_t _M0L6_2atmpS2199;
  moonbit_string_t _M0L6_2atmpS755;
  moonbit_string_t _M0L6_2atmpS756;
  moonbit_string_t _M0L6_2atmpS2198;
  moonbit_string_t _M0L6_2atmpS754;
  moonbit_string_t _M0L6_2atmpS2197;
  moonbit_string_t _M0L6_2atmpS753;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS757 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS15);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2199
  = moonbit_add_string(_M0L6_2atmpS757, (moonbit_string_t)moonbit_string_literal_32.data);
  moonbit_decref(_M0L6_2atmpS757);
  _M0L6_2atmpS755 = _M0L6_2atmpS2199;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS756
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2198 = moonbit_add_string(_M0L6_2atmpS755, _M0L6_2atmpS756);
  moonbit_decref(_M0L6_2atmpS755);
  moonbit_decref(_M0L6_2atmpS756);
  _M0L6_2atmpS754 = _M0L6_2atmpS2198;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2197
  = moonbit_add_string(_M0L6_2atmpS754, (moonbit_string_t)moonbit_string_literal_33.data);
  moonbit_decref(_M0L6_2atmpS754);
  _M0L6_2atmpS753 = _M0L6_2atmpS2197;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS753);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS17,
  moonbit_string_t _M0L3locS18
) {
  moonbit_string_t _M0L6_2atmpS762;
  moonbit_string_t _M0L6_2atmpS2202;
  moonbit_string_t _M0L6_2atmpS760;
  moonbit_string_t _M0L6_2atmpS761;
  moonbit_string_t _M0L6_2atmpS2201;
  moonbit_string_t _M0L6_2atmpS759;
  moonbit_string_t _M0L6_2atmpS2200;
  moonbit_string_t _M0L6_2atmpS758;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS762 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2202
  = moonbit_add_string(_M0L6_2atmpS762, (moonbit_string_t)moonbit_string_literal_32.data);
  moonbit_decref(_M0L6_2atmpS762);
  _M0L6_2atmpS760 = _M0L6_2atmpS2202;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS761
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2201 = moonbit_add_string(_M0L6_2atmpS760, _M0L6_2atmpS761);
  moonbit_decref(_M0L6_2atmpS760);
  moonbit_decref(_M0L6_2atmpS761);
  _M0L6_2atmpS759 = _M0L6_2atmpS2201;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2200
  = moonbit_add_string(_M0L6_2atmpS759, (moonbit_string_t)moonbit_string_literal_33.data);
  moonbit_decref(_M0L6_2atmpS759);
  _M0L6_2atmpS758 = _M0L6_2atmpS2200;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS758);
  return 0;
}

struct _M0TPB9ArrayViewGyE _M0FPB5abortGRPB9ArrayViewGyEE(
  moonbit_string_t _M0L6stringS19,
  moonbit_string_t _M0L3locS20
) {
  moonbit_string_t _M0L6_2atmpS767;
  moonbit_string_t _M0L6_2atmpS2205;
  moonbit_string_t _M0L6_2atmpS765;
  moonbit_string_t _M0L6_2atmpS766;
  moonbit_string_t _M0L6_2atmpS2204;
  moonbit_string_t _M0L6_2atmpS764;
  moonbit_string_t _M0L6_2atmpS2203;
  moonbit_string_t _M0L6_2atmpS763;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS767 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2205
  = moonbit_add_string(_M0L6_2atmpS767, (moonbit_string_t)moonbit_string_literal_32.data);
  moonbit_decref(_M0L6_2atmpS767);
  _M0L6_2atmpS765 = _M0L6_2atmpS2205;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS766
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2204 = moonbit_add_string(_M0L6_2atmpS765, _M0L6_2atmpS766);
  moonbit_decref(_M0L6_2atmpS765);
  moonbit_decref(_M0L6_2atmpS766);
  _M0L6_2atmpS764 = _M0L6_2atmpS2204;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2203
  = moonbit_add_string(_M0L6_2atmpS764, (moonbit_string_t)moonbit_string_literal_33.data);
  moonbit_decref(_M0L6_2atmpS764);
  _M0L6_2atmpS763 = _M0L6_2atmpS2203;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPB9ArrayViewGyEE(_M0L6_2atmpS763);
}

int32_t _M0FPB5abortGyE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS772;
  moonbit_string_t _M0L6_2atmpS2208;
  moonbit_string_t _M0L6_2atmpS770;
  moonbit_string_t _M0L6_2atmpS771;
  moonbit_string_t _M0L6_2atmpS2207;
  moonbit_string_t _M0L6_2atmpS769;
  moonbit_string_t _M0L6_2atmpS2206;
  moonbit_string_t _M0L6_2atmpS768;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS772 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2208
  = moonbit_add_string(_M0L6_2atmpS772, (moonbit_string_t)moonbit_string_literal_32.data);
  moonbit_decref(_M0L6_2atmpS772);
  _M0L6_2atmpS770 = _M0L6_2atmpS2208;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS771
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2207 = moonbit_add_string(_M0L6_2atmpS770, _M0L6_2atmpS771);
  moonbit_decref(_M0L6_2atmpS770);
  moonbit_decref(_M0L6_2atmpS771);
  _M0L6_2atmpS769 = _M0L6_2atmpS2207;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2206
  = moonbit_add_string(_M0L6_2atmpS769, (moonbit_string_t)moonbit_string_literal_33.data);
  moonbit_decref(_M0L6_2atmpS769);
  _M0L6_2atmpS768 = _M0L6_2atmpS2206;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGyE(_M0L6_2atmpS768);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS777;
  moonbit_string_t _M0L6_2atmpS2211;
  moonbit_string_t _M0L6_2atmpS775;
  moonbit_string_t _M0L6_2atmpS776;
  moonbit_string_t _M0L6_2atmpS2210;
  moonbit_string_t _M0L6_2atmpS774;
  moonbit_string_t _M0L6_2atmpS2209;
  moonbit_string_t _M0L6_2atmpS773;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS777 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2211
  = moonbit_add_string(_M0L6_2atmpS777, (moonbit_string_t)moonbit_string_literal_32.data);
  moonbit_decref(_M0L6_2atmpS777);
  _M0L6_2atmpS775 = _M0L6_2atmpS2211;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS776
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2210 = moonbit_add_string(_M0L6_2atmpS775, _M0L6_2atmpS776);
  moonbit_decref(_M0L6_2atmpS775);
  moonbit_decref(_M0L6_2atmpS776);
  _M0L6_2atmpS774 = _M0L6_2atmpS2210;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2209
  = moonbit_add_string(_M0L6_2atmpS774, (moonbit_string_t)moonbit_string_literal_33.data);
  moonbit_decref(_M0L6_2atmpS774);
  _M0L6_2atmpS773 = _M0L6_2atmpS2209;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS773);
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S9,
  struct _M0TPB6Logger _M0L10_2ax__4934S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS2212;
  int32_t _M0L6_2acntS2258;
  moonbit_string_t _M0L15_2a_2aarg__4935S11;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S9;
  _M0L8_2afieldS2212 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS2258 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS2258 > 1) {
    int32_t _M0L11_2anew__cntS2259 = _M0L6_2acntS2258 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS2259;
    moonbit_incref(_M0L8_2afieldS2212);
  } else if (_M0L6_2acntS2258 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__4935S11 = _M0L8_2afieldS2212;
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_34.data);
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S12, _M0L15_2a_2aarg__4935S11);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_35.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS8,
  moonbit_string_t _M0L3objS7
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS7, _M0L4selfS8);
  return 0;
}

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L3msgS1
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS1);
  moonbit_decref(_M0L3msgS1);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS2) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS2);
  moonbit_decref(_M0L3msgS2);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGuE(moonbit_string_t _M0L3msgS3) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
  return 0;
}

struct _M0TPB9ArrayViewGyE _M0FPC15abort5abortGRPB9ArrayViewGyEE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGyE(moonbit_string_t _M0L3msgS5) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS5);
  moonbit_decref(_M0L3msgS5);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS6
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS6);
  moonbit_decref(_M0L3msgS6);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

void _M0FP017____moonbit__initC740l332() {
  moonbit_string_t _M0L6_2atmpS742;
  moonbit_string_t _M0L6_2atmpS2213;
  moonbit_string_t _M0L6_2atmpS741;
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  moonbit_incref(_M0FP48clawteam8clawteam8internal4mock8os__args);
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0L6_2atmpS742
  = _M0IP016_24default__implPB4Show10to__stringGRPB5ArrayGsEE(_M0FP48clawteam8clawteam8internal4mock8os__args);
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0L6_2atmpS2213
  = moonbit_add_string(_M0L6_2atmpS742, (moonbit_string_t)moonbit_string_literal_36.data);
  moonbit_decref(_M0L6_2atmpS742);
  _M0L6_2atmpS741 = _M0L6_2atmpS2213;
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS741);
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS710) {
  switch (Moonbit_object_tag(_M0L4_2aeS710)) {
    case 5: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_37.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_38.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_39.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_40.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS710);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_41.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS733,
  int32_t _M0L8_2aparamS732
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS731 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS733;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS731, _M0L8_2aparamS732);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS730,
  struct _M0TPC16string10StringView _M0L8_2aparamS729
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS728 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS730;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS728, _M0L8_2aparamS729);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS727,
  moonbit_string_t _M0L8_2aparamS724,
  int32_t _M0L8_2aparamS725,
  int32_t _M0L8_2aparamS726
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS723 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS727;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS723, _M0L8_2aparamS724, _M0L8_2aparamS725, _M0L8_2aparamS726);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS722,
  moonbit_string_t _M0L8_2aparamS721
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS720 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS722;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS720, _M0L8_2aparamS721);
  return 0;
}

void moonbit_init() {
  void* _M0L11_2atry__errS601;
  void* _M0L7_2abindS599;
  moonbit_string_t _M0L7_2abindS602;
  int32_t _M0L6_2atmpS745;
  struct _M0TPC16string10StringView _M0L6_2atmpS744;
  struct moonbit_result_1 _tmp_2372;
  moonbit_string_t _M0L6_2atmpS743;
  void(* _M0L6_2atmpS739)();
  #line 292 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0FP48clawteam8clawteam8internal4mock8os__args
  = _M0FP48clawteam8clawteam8internal2os4args();
  _M0L7_2abindS602 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS745 = Moonbit_array_length(_M0L7_2abindS602);
  _M0L6_2atmpS744
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS745, _M0L7_2abindS602
  };
  #line 331 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _tmp_2372 = _M0FP48clawteam8clawteam8internal2os6getenv(_M0L6_2atmpS744);
  if (_tmp_2372.tag) {
    moonbit_string_t const _M0L5_2aokS746 = _tmp_2372.data.ok;
    _M0L6_2atmpS743 = _M0L5_2aokS746;
  } else {
    void* const _M0L6_2aerrS747 = _tmp_2372.data.err;
    _M0L11_2atry__errS601 = _M0L6_2aerrS747;
    goto join_600;
  }
  _M0L7_2abindS599
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok));
  Moonbit_object_header(_M0L7_2abindS599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok*)_M0L7_2abindS599)->$0
  = _M0L6_2atmpS743;
  goto joinlet_2371;
  join_600:;
  _M0L7_2abindS599
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err));
  Moonbit_object_header(_M0L7_2abindS599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGOsRPC15error5ErrorE3Err*)_M0L7_2abindS599)->$0
  = _M0L11_2atry__errS601;
  joinlet_2371:;
  switch (Moonbit_object_tag(_M0L7_2abindS599)) {
    case 1: {
      struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok* _M0L5_2aOkS603 =
        (struct _M0DTPC16result6ResultGOsRPC15error5ErrorE2Ok*)_M0L7_2abindS599;
      moonbit_string_t _M0L8_2afieldS2214 = _M0L5_2aOkS603->$0;
      int32_t _M0L6_2acntS2260 = Moonbit_object_header(_M0L5_2aOkS603)->rc;
      moonbit_string_t _M0L4_2axS604;
      if (_M0L6_2acntS2260 > 1) {
        int32_t _M0L11_2anew__cntS2261 = _M0L6_2acntS2260 - 1;
        Moonbit_object_header(_M0L5_2aOkS603)->rc = _M0L11_2anew__cntS2261;
        if (_M0L8_2afieldS2214) {
          moonbit_incref(_M0L8_2afieldS2214);
        }
      } else if (_M0L6_2acntS2260 == 1) {
        #line 331 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
        moonbit_free(_M0L5_2aOkS603);
      }
      _M0L4_2axS604 = _M0L8_2afieldS2214;
      if (_M0L4_2axS604 == 0) {
        if (_M0L4_2axS604) {
          moonbit_decref(_M0L4_2axS604);
        }
      } else {
        moonbit_string_t _M0L7_2aSomeS605 = _M0L4_2axS604;
        moonbit_string_t _M0L4_2axS606 = _M0L7_2aSomeS605;
        if (
          moonbit_val_array_equal(_M0L4_2axS606, (moonbit_string_t)moonbit_string_literal_44.data)
        ) {
          moonbit_decref(_M0L4_2axS606);
          goto join_598;
        } else if (
                 moonbit_val_array_equal(_M0L4_2axS606, (moonbit_string_t)moonbit_string_literal_43.data)
               ) {
          moonbit_decref(_M0L4_2axS606);
          goto join_598;
        } else {
          moonbit_decref(_M0L4_2axS606);
        }
      }
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS599);
      break;
    }
  }
  goto joinlet_2370;
  join_598:;
  _M0L6_2atmpS739 = &_M0FP017____moonbit__initC740l332;
  #line 332 "E:\\moonbit\\clawteam\\internal\\mock\\mock.mbt"
  _M0FP48clawteam8clawteam8internal2os6atexit(_M0L6_2atmpS739);
  joinlet_2370:;
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS738;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS704;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS705;
  int32_t _M0L7_2abindS706;
  int32_t _M0L2__S707;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS738
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS704
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS704)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS704->$0 = _M0L6_2atmpS738;
  _M0L12async__testsS704->$1 = 0;
  #line 397 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS705
  = _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS706 = _M0L7_2abindS705->$1;
  _M0L2__S707 = 0;
  while (1) {
    if (_M0L2__S707 < _M0L7_2abindS706) {
      struct _M0TUsiE** _M0L8_2afieldS2218 = _M0L7_2abindS705->$0;
      struct _M0TUsiE** _M0L3bufS737 = _M0L8_2afieldS2218;
      struct _M0TUsiE* _M0L6_2atmpS2217 =
        (struct _M0TUsiE*)_M0L3bufS737[_M0L2__S707];
      struct _M0TUsiE* _M0L3argS708 = _M0L6_2atmpS2217;
      moonbit_string_t _M0L8_2afieldS2216 = _M0L3argS708->$0;
      moonbit_string_t _M0L6_2atmpS734 = _M0L8_2afieldS2216;
      int32_t _M0L8_2afieldS2215 = _M0L3argS708->$1;
      int32_t _M0L6_2atmpS735 = _M0L8_2afieldS2215;
      int32_t _M0L6_2atmpS736;
      moonbit_incref(_M0L6_2atmpS734);
      moonbit_incref(_M0L12async__testsS704);
      #line 398 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled44moonbit__test__driver__internal__do__execute(_M0L12async__testsS704, _M0L6_2atmpS734, _M0L6_2atmpS735);
      _M0L6_2atmpS736 = _M0L2__S707 + 1;
      _M0L2__S707 = _M0L6_2atmpS736;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS705);
    }
    break;
  }
  #line 400 "E:\\moonbit\\clawteam\\cmd\\test-to-be-killed\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam3cmd23test_2dto_2dbe_2dkilled34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS704);
  return 0;
}