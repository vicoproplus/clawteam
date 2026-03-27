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
struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal22signal__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal22signal__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
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

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
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

struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal22signal__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal22signal__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal22signal__blackbox__test53____test__7369676e616c5f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS682(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS673(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testC1501l427(
  struct _M0TWEu*
);

int32_t _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testC1497l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS606(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS601(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S588(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal22signal__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal22signal__blackbox__test43____test__7369676e616c5f746573742e6d6274__0(
  
);

int32_t _M0FP48clawteam8clawteam8internal6signal20check__signal__value(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal6signal15signal__sigtstp moonbit_moonclaw_signal_sigtstp

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr*,
  struct _M0TPB6Logger
);

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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1146l570(struct _M0TWEOs*);

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

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

int32_t _M0MPB6Hasher12combine__int(struct _M0TPB6Hasher*, int32_t);

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

int32_t moonbit_moonclaw_signal_sigtstp();

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    115, 105, 103, 110, 97, 108, 95, 116, 101, 115, 116, 46, 109, 98, 
    116, 0
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
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[114]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 113), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 105, 
    103, 110, 97, 108, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_20 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    83, 73, 71, 84, 83, 84, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[116]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 115), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 105, 
    103, 110, 97, 108, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_22 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 105, 103, 110, 97, 108, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 105, 103, 110, 
    97, 108, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_29 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_27 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    32, 110, 111, 116, 32, 115, 117, 112, 112, 111, 114, 116, 101, 100, 
    32, 111, 110, 32, 116, 104, 105, 115, 32, 112, 108, 97, 116, 102, 
    111, 114, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 115, 
    105, 103, 110, 97, 108, 58, 115, 105, 103, 110, 97, 108, 46, 109, 
    98, 116, 58, 55, 58, 53, 45, 55, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    115, 105, 103, 116, 115, 116, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_28 =
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

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal22signal__blackbox__test53____test__7369676e616c5f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal22signal__blackbox__test53____test__7369676e616c5f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS682$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS682
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal22signal__blackbox__test49____test__7369676e616c5f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal22signal__blackbox__test53____test__7369676e616c5f746573742e6d6274__0_2edyncall$closure.data;

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

int32_t _M0FP48clawteam8clawteam8internal6signal7sigtstp;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal22signal__blackbox__test53____test__7369676e616c5f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1532
) {
  return _M0FP48clawteam8clawteam8internal22signal__blackbox__test43____test__7369676e616c5f746573742e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS703,
  moonbit_string_t _M0L8filenameS678,
  int32_t _M0L5indexS681
) {
  struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673* _closure_1754;
  struct _M0TWssbEu* _M0L14handle__resultS673;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS682;
  void* _M0L11_2atry__errS697;
  struct moonbit_result_0 _tmp_1756;
  int32_t _handle__error__result_1757;
  int32_t _M0L6_2atmpS1520;
  void* _M0L3errS698;
  moonbit_string_t _M0L4nameS700;
  struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS701;
  moonbit_string_t _M0L8_2afieldS1533;
  int32_t _M0L6_2acntS1697;
  moonbit_string_t _M0L7_2anameS702;
  #line 526 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS678);
  _closure_1754
  = (struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673*)moonbit_malloc(sizeof(struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673));
  Moonbit_object_header(_closure_1754)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673, $1) >> 2, 1, 0);
  _closure_1754->code
  = &_M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS673;
  _closure_1754->$0 = _M0L5indexS681;
  _closure_1754->$1 = _M0L8filenameS678;
  _M0L14handle__resultS673 = (struct _M0TWssbEu*)_closure_1754;
  _M0L17error__to__stringS682
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS682$closure.data;
  moonbit_incref(_M0L12async__testsS703);
  moonbit_incref(_M0L17error__to__stringS682);
  moonbit_incref(_M0L8filenameS678);
  moonbit_incref(_M0L14handle__resultS673);
  #line 560 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _tmp_1756
  = _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS703, _M0L8filenameS678, _M0L5indexS681, _M0L14handle__resultS673, _M0L17error__to__stringS682);
  if (_tmp_1756.tag) {
    int32_t const _M0L5_2aokS1529 = _tmp_1756.data.ok;
    _handle__error__result_1757 = _M0L5_2aokS1529;
  } else {
    void* const _M0L6_2aerrS1530 = _tmp_1756.data.err;
    moonbit_decref(_M0L12async__testsS703);
    moonbit_decref(_M0L17error__to__stringS682);
    moonbit_decref(_M0L8filenameS678);
    _M0L11_2atry__errS697 = _M0L6_2aerrS1530;
    goto join_696;
  }
  if (_handle__error__result_1757) {
    moonbit_decref(_M0L12async__testsS703);
    moonbit_decref(_M0L17error__to__stringS682);
    moonbit_decref(_M0L8filenameS678);
    _M0L6_2atmpS1520 = 1;
  } else {
    struct moonbit_result_0 _tmp_1758;
    int32_t _handle__error__result_1759;
    moonbit_incref(_M0L12async__testsS703);
    moonbit_incref(_M0L17error__to__stringS682);
    moonbit_incref(_M0L8filenameS678);
    moonbit_incref(_M0L14handle__resultS673);
    #line 563 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    _tmp_1758
    = _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS703, _M0L8filenameS678, _M0L5indexS681, _M0L14handle__resultS673, _M0L17error__to__stringS682);
    if (_tmp_1758.tag) {
      int32_t const _M0L5_2aokS1527 = _tmp_1758.data.ok;
      _handle__error__result_1759 = _M0L5_2aokS1527;
    } else {
      void* const _M0L6_2aerrS1528 = _tmp_1758.data.err;
      moonbit_decref(_M0L12async__testsS703);
      moonbit_decref(_M0L17error__to__stringS682);
      moonbit_decref(_M0L8filenameS678);
      _M0L11_2atry__errS697 = _M0L6_2aerrS1528;
      goto join_696;
    }
    if (_handle__error__result_1759) {
      moonbit_decref(_M0L12async__testsS703);
      moonbit_decref(_M0L17error__to__stringS682);
      moonbit_decref(_M0L8filenameS678);
      _M0L6_2atmpS1520 = 1;
    } else {
      struct moonbit_result_0 _tmp_1760;
      int32_t _handle__error__result_1761;
      moonbit_incref(_M0L12async__testsS703);
      moonbit_incref(_M0L17error__to__stringS682);
      moonbit_incref(_M0L8filenameS678);
      moonbit_incref(_M0L14handle__resultS673);
      #line 566 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _tmp_1760
      = _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS703, _M0L8filenameS678, _M0L5indexS681, _M0L14handle__resultS673, _M0L17error__to__stringS682);
      if (_tmp_1760.tag) {
        int32_t const _M0L5_2aokS1525 = _tmp_1760.data.ok;
        _handle__error__result_1761 = _M0L5_2aokS1525;
      } else {
        void* const _M0L6_2aerrS1526 = _tmp_1760.data.err;
        moonbit_decref(_M0L12async__testsS703);
        moonbit_decref(_M0L17error__to__stringS682);
        moonbit_decref(_M0L8filenameS678);
        _M0L11_2atry__errS697 = _M0L6_2aerrS1526;
        goto join_696;
      }
      if (_handle__error__result_1761) {
        moonbit_decref(_M0L12async__testsS703);
        moonbit_decref(_M0L17error__to__stringS682);
        moonbit_decref(_M0L8filenameS678);
        _M0L6_2atmpS1520 = 1;
      } else {
        struct moonbit_result_0 _tmp_1762;
        int32_t _handle__error__result_1763;
        moonbit_incref(_M0L12async__testsS703);
        moonbit_incref(_M0L17error__to__stringS682);
        moonbit_incref(_M0L8filenameS678);
        moonbit_incref(_M0L14handle__resultS673);
        #line 569 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        _tmp_1762
        = _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS703, _M0L8filenameS678, _M0L5indexS681, _M0L14handle__resultS673, _M0L17error__to__stringS682);
        if (_tmp_1762.tag) {
          int32_t const _M0L5_2aokS1523 = _tmp_1762.data.ok;
          _handle__error__result_1763 = _M0L5_2aokS1523;
        } else {
          void* const _M0L6_2aerrS1524 = _tmp_1762.data.err;
          moonbit_decref(_M0L12async__testsS703);
          moonbit_decref(_M0L17error__to__stringS682);
          moonbit_decref(_M0L8filenameS678);
          _M0L11_2atry__errS697 = _M0L6_2aerrS1524;
          goto join_696;
        }
        if (_handle__error__result_1763) {
          moonbit_decref(_M0L12async__testsS703);
          moonbit_decref(_M0L17error__to__stringS682);
          moonbit_decref(_M0L8filenameS678);
          _M0L6_2atmpS1520 = 1;
        } else {
          struct moonbit_result_0 _tmp_1764;
          moonbit_incref(_M0L14handle__resultS673);
          #line 572 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
          _tmp_1764
          = _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS703, _M0L8filenameS678, _M0L5indexS681, _M0L14handle__resultS673, _M0L17error__to__stringS682);
          if (_tmp_1764.tag) {
            int32_t const _M0L5_2aokS1521 = _tmp_1764.data.ok;
            _M0L6_2atmpS1520 = _M0L5_2aokS1521;
          } else {
            void* const _M0L6_2aerrS1522 = _tmp_1764.data.err;
            _M0L11_2atry__errS697 = _M0L6_2aerrS1522;
            goto join_696;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1520) {
    void* _M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS697
    = _M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531;
    goto join_696;
  } else {
    moonbit_decref(_M0L14handle__resultS673);
  }
  goto joinlet_1755;
  join_696:;
  _M0L3errS698 = _M0L11_2atry__errS697;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS701
  = (struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS698;
  _M0L8_2afieldS1533 = _M0L36_2aMoonBitTestDriverInternalSkipTestS701->$0;
  _M0L6_2acntS1697
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS701)->rc;
  if (_M0L6_2acntS1697 > 1) {
    int32_t _M0L11_2anew__cntS1698 = _M0L6_2acntS1697 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS701)->rc
    = _M0L11_2anew__cntS1698;
    moonbit_incref(_M0L8_2afieldS1533);
  } else if (_M0L6_2acntS1697 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS701);
  }
  _M0L7_2anameS702 = _M0L8_2afieldS1533;
  _M0L4nameS700 = _M0L7_2anameS702;
  goto join_699;
  goto joinlet_1765;
  join_699:;
  #line 580 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS673(_M0L14handle__resultS673, _M0L4nameS700, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_1765:;
  joinlet_1755:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS682(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1519,
  void* _M0L3errS683
) {
  void* _M0L1eS685;
  moonbit_string_t _M0L1eS687;
  #line 549 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS1519);
  switch (Moonbit_object_tag(_M0L3errS683)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS688 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS683;
      moonbit_string_t _M0L8_2afieldS1534 = _M0L10_2aFailureS688->$0;
      int32_t _M0L6_2acntS1699 =
        Moonbit_object_header(_M0L10_2aFailureS688)->rc;
      moonbit_string_t _M0L4_2aeS689;
      if (_M0L6_2acntS1699 > 1) {
        int32_t _M0L11_2anew__cntS1700 = _M0L6_2acntS1699 - 1;
        Moonbit_object_header(_M0L10_2aFailureS688)->rc
        = _M0L11_2anew__cntS1700;
        moonbit_incref(_M0L8_2afieldS1534);
      } else if (_M0L6_2acntS1699 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS688);
      }
      _M0L4_2aeS689 = _M0L8_2afieldS1534;
      _M0L1eS687 = _M0L4_2aeS689;
      goto join_686;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS690 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS683;
      moonbit_string_t _M0L8_2afieldS1535 = _M0L15_2aInspectErrorS690->$0;
      int32_t _M0L6_2acntS1701 =
        Moonbit_object_header(_M0L15_2aInspectErrorS690)->rc;
      moonbit_string_t _M0L4_2aeS691;
      if (_M0L6_2acntS1701 > 1) {
        int32_t _M0L11_2anew__cntS1702 = _M0L6_2acntS1701 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS690)->rc
        = _M0L11_2anew__cntS1702;
        moonbit_incref(_M0L8_2afieldS1535);
      } else if (_M0L6_2acntS1701 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS690);
      }
      _M0L4_2aeS691 = _M0L8_2afieldS1535;
      _M0L1eS687 = _M0L4_2aeS691;
      goto join_686;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS692 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS683;
      moonbit_string_t _M0L8_2afieldS1536 = _M0L16_2aSnapshotErrorS692->$0;
      int32_t _M0L6_2acntS1703 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS692)->rc;
      moonbit_string_t _M0L4_2aeS693;
      if (_M0L6_2acntS1703 > 1) {
        int32_t _M0L11_2anew__cntS1704 = _M0L6_2acntS1703 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS692)->rc
        = _M0L11_2anew__cntS1704;
        moonbit_incref(_M0L8_2afieldS1536);
      } else if (_M0L6_2acntS1703 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS692);
      }
      _M0L4_2aeS693 = _M0L8_2afieldS1536;
      _M0L1eS687 = _M0L4_2aeS693;
      goto join_686;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS694 =
        (struct _M0DTPC15error5Error125clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS683;
      moonbit_string_t _M0L8_2afieldS1537 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS694->$0;
      int32_t _M0L6_2acntS1705 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS694)->rc;
      moonbit_string_t _M0L4_2aeS695;
      if (_M0L6_2acntS1705 > 1) {
        int32_t _M0L11_2anew__cntS1706 = _M0L6_2acntS1705 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS694)->rc
        = _M0L11_2anew__cntS1706;
        moonbit_incref(_M0L8_2afieldS1537);
      } else if (_M0L6_2acntS1705 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS694);
      }
      _M0L4_2aeS695 = _M0L8_2afieldS1537;
      _M0L1eS687 = _M0L4_2aeS695;
      goto join_686;
      break;
    }
    default: {
      _M0L1eS685 = _M0L3errS683;
      goto join_684;
      break;
    }
  }
  join_686:;
  return _M0L1eS687;
  join_684:;
  #line 555 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS685);
}

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS673(
  struct _M0TWssbEu* _M0L6_2aenvS1505,
  moonbit_string_t _M0L8testnameS674,
  moonbit_string_t _M0L7messageS675,
  int32_t _M0L7skippedS676
) {
  struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673* _M0L14_2acasted__envS1506;
  moonbit_string_t _M0L8_2afieldS1547;
  moonbit_string_t _M0L8filenameS678;
  int32_t _M0L8_2afieldS1546;
  int32_t _M0L6_2acntS1707;
  int32_t _M0L5indexS681;
  int32_t _if__result_1768;
  moonbit_string_t _M0L10file__nameS677;
  moonbit_string_t _M0L10test__nameS679;
  moonbit_string_t _M0L7messageS680;
  moonbit_string_t _M0L6_2atmpS1518;
  moonbit_string_t _M0L6_2atmpS1545;
  moonbit_string_t _M0L6_2atmpS1517;
  moonbit_string_t _M0L6_2atmpS1544;
  moonbit_string_t _M0L6_2atmpS1515;
  moonbit_string_t _M0L6_2atmpS1516;
  moonbit_string_t _M0L6_2atmpS1543;
  moonbit_string_t _M0L6_2atmpS1514;
  moonbit_string_t _M0L6_2atmpS1542;
  moonbit_string_t _M0L6_2atmpS1512;
  moonbit_string_t _M0L6_2atmpS1513;
  moonbit_string_t _M0L6_2atmpS1541;
  moonbit_string_t _M0L6_2atmpS1511;
  moonbit_string_t _M0L6_2atmpS1540;
  moonbit_string_t _M0L6_2atmpS1509;
  moonbit_string_t _M0L6_2atmpS1510;
  moonbit_string_t _M0L6_2atmpS1539;
  moonbit_string_t _M0L6_2atmpS1508;
  moonbit_string_t _M0L6_2atmpS1538;
  moonbit_string_t _M0L6_2atmpS1507;
  #line 533 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS1506
  = (struct _M0R128_24clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c673*)_M0L6_2aenvS1505;
  _M0L8_2afieldS1547 = _M0L14_2acasted__envS1506->$1;
  _M0L8filenameS678 = _M0L8_2afieldS1547;
  _M0L8_2afieldS1546 = _M0L14_2acasted__envS1506->$0;
  _M0L6_2acntS1707 = Moonbit_object_header(_M0L14_2acasted__envS1506)->rc;
  if (_M0L6_2acntS1707 > 1) {
    int32_t _M0L11_2anew__cntS1708 = _M0L6_2acntS1707 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1506)->rc
    = _M0L11_2anew__cntS1708;
    moonbit_incref(_M0L8filenameS678);
  } else if (_M0L6_2acntS1707 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1506);
  }
  _M0L5indexS681 = _M0L8_2afieldS1546;
  if (!_M0L7skippedS676) {
    _if__result_1768 = 1;
  } else {
    _if__result_1768 = 0;
  }
  if (_if__result_1768) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS677 = _M0MPC16string6String6escape(_M0L8filenameS678);
  #line 540 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS679 = _M0MPC16string6String6escape(_M0L8testnameS674);
  #line 541 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS680 = _M0MPC16string6String6escape(_M0L7messageS675);
  #line 542 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1518
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS677);
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1545
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1518);
  moonbit_decref(_M0L6_2atmpS1518);
  _M0L6_2atmpS1517 = _M0L6_2atmpS1545;
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1544
  = moonbit_add_string(_M0L6_2atmpS1517, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1517);
  _M0L6_2atmpS1515 = _M0L6_2atmpS1544;
  #line 544 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1516
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS681);
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1543 = moonbit_add_string(_M0L6_2atmpS1515, _M0L6_2atmpS1516);
  moonbit_decref(_M0L6_2atmpS1515);
  moonbit_decref(_M0L6_2atmpS1516);
  _M0L6_2atmpS1514 = _M0L6_2atmpS1543;
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1542
  = moonbit_add_string(_M0L6_2atmpS1514, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1514);
  _M0L6_2atmpS1512 = _M0L6_2atmpS1542;
  #line 544 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1513
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS679);
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1541 = moonbit_add_string(_M0L6_2atmpS1512, _M0L6_2atmpS1513);
  moonbit_decref(_M0L6_2atmpS1512);
  moonbit_decref(_M0L6_2atmpS1513);
  _M0L6_2atmpS1511 = _M0L6_2atmpS1541;
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1540
  = moonbit_add_string(_M0L6_2atmpS1511, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1511);
  _M0L6_2atmpS1509 = _M0L6_2atmpS1540;
  #line 544 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1510
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS680);
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1539 = moonbit_add_string(_M0L6_2atmpS1509, _M0L6_2atmpS1510);
  moonbit_decref(_M0L6_2atmpS1509);
  moonbit_decref(_M0L6_2atmpS1510);
  _M0L6_2atmpS1508 = _M0L6_2atmpS1539;
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1538
  = moonbit_add_string(_M0L6_2atmpS1508, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1508);
  _M0L6_2atmpS1507 = _M0L6_2atmpS1538;
  #line 543 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1507);
  #line 546 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S672,
  moonbit_string_t _M0L8filenameS669,
  int32_t _M0L5indexS663,
  struct _M0TWssbEu* _M0L14handle__resultS659,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS661
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS639;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS668;
  struct _M0TWEuQRPC15error5Error* _M0L1fS641;
  moonbit_string_t* _M0L5attrsS642;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS662;
  moonbit_string_t _M0L4nameS645;
  moonbit_string_t _M0L4nameS643;
  int32_t _M0L6_2atmpS1504;
  struct _M0TWEOs* _M0L5_2aitS647;
  struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__* _closure_1777;
  struct _M0TWEu* _M0L6_2atmpS1495;
  struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__* _closure_1778;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS1496;
  struct moonbit_result_0 _result_1779;
  #line 407 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S672);
  moonbit_incref(_M0FP48clawteam8clawteam8internal22signal__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS668
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal22signal__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS669);
  if (_M0L7_2abindS668 == 0) {
    struct moonbit_result_0 _result_1770;
    if (_M0L7_2abindS668) {
      moonbit_decref(_M0L7_2abindS668);
    }
    moonbit_decref(_M0L17error__to__stringS661);
    moonbit_decref(_M0L14handle__resultS659);
    _result_1770.tag = 1;
    _result_1770.data.ok = 0;
    return _result_1770;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS670 =
      _M0L7_2abindS668;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS671 =
      _M0L7_2aSomeS670;
    _M0L10index__mapS639 = _M0L13_2aindex__mapS671;
    goto join_638;
  }
  join_638:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS662
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS639, _M0L5indexS663);
  if (_M0L7_2abindS662 == 0) {
    struct moonbit_result_0 _result_1772;
    if (_M0L7_2abindS662) {
      moonbit_decref(_M0L7_2abindS662);
    }
    moonbit_decref(_M0L17error__to__stringS661);
    moonbit_decref(_M0L14handle__resultS659);
    _result_1772.tag = 1;
    _result_1772.data.ok = 0;
    return _result_1772;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS664 = _M0L7_2abindS662;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS665 = _M0L7_2aSomeS664;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS1551 = _M0L4_2axS665->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS666 = _M0L8_2afieldS1551;
    moonbit_string_t* _M0L8_2afieldS1550 = _M0L4_2axS665->$1;
    int32_t _M0L6_2acntS1709 = Moonbit_object_header(_M0L4_2axS665)->rc;
    moonbit_string_t* _M0L8_2aattrsS667;
    if (_M0L6_2acntS1709 > 1) {
      int32_t _M0L11_2anew__cntS1710 = _M0L6_2acntS1709 - 1;
      Moonbit_object_header(_M0L4_2axS665)->rc = _M0L11_2anew__cntS1710;
      moonbit_incref(_M0L8_2afieldS1550);
      moonbit_incref(_M0L4_2afS666);
    } else if (_M0L6_2acntS1709 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS665);
    }
    _M0L8_2aattrsS667 = _M0L8_2afieldS1550;
    _M0L1fS641 = _M0L4_2afS666;
    _M0L5attrsS642 = _M0L8_2aattrsS667;
    goto join_640;
  }
  join_640:;
  _M0L6_2atmpS1504 = Moonbit_array_length(_M0L5attrsS642);
  if (_M0L6_2atmpS1504 >= 1) {
    moonbit_string_t _M0L6_2atmpS1549 = (moonbit_string_t)_M0L5attrsS642[0];
    moonbit_string_t _M0L7_2anameS646 = _M0L6_2atmpS1549;
    moonbit_incref(_M0L7_2anameS646);
    _M0L4nameS645 = _M0L7_2anameS646;
    goto join_644;
  } else {
    _M0L4nameS643 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_1773;
  join_644:;
  _M0L4nameS643 = _M0L4nameS645;
  joinlet_1773:;
  #line 417 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS647 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS642);
  while (1) {
    moonbit_string_t _M0L4attrS649;
    moonbit_string_t _M0L7_2abindS656;
    int32_t _M0L6_2atmpS1488;
    int64_t _M0L6_2atmpS1487;
    moonbit_incref(_M0L5_2aitS647);
    #line 419 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS656 = _M0MPB4Iter4nextGsE(_M0L5_2aitS647);
    if (_M0L7_2abindS656 == 0) {
      if (_M0L7_2abindS656) {
        moonbit_decref(_M0L7_2abindS656);
      }
      moonbit_decref(_M0L5_2aitS647);
    } else {
      moonbit_string_t _M0L7_2aSomeS657 = _M0L7_2abindS656;
      moonbit_string_t _M0L7_2aattrS658 = _M0L7_2aSomeS657;
      _M0L4attrS649 = _M0L7_2aattrS658;
      goto join_648;
    }
    goto joinlet_1775;
    join_648:;
    _M0L6_2atmpS1488 = Moonbit_array_length(_M0L4attrS649);
    _M0L6_2atmpS1487 = (int64_t)_M0L6_2atmpS1488;
    moonbit_incref(_M0L4attrS649);
    #line 420 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS649, 5, 0, _M0L6_2atmpS1487)
    ) {
      int32_t _M0L6_2atmpS1494 = _M0L4attrS649[0];
      int32_t _M0L4_2axS650 = _M0L6_2atmpS1494;
      if (_M0L4_2axS650 == 112) {
        int32_t _M0L6_2atmpS1493 = _M0L4attrS649[1];
        int32_t _M0L4_2axS651 = _M0L6_2atmpS1493;
        if (_M0L4_2axS651 == 97) {
          int32_t _M0L6_2atmpS1492 = _M0L4attrS649[2];
          int32_t _M0L4_2axS652 = _M0L6_2atmpS1492;
          if (_M0L4_2axS652 == 110) {
            int32_t _M0L6_2atmpS1491 = _M0L4attrS649[3];
            int32_t _M0L4_2axS653 = _M0L6_2atmpS1491;
            if (_M0L4_2axS653 == 105) {
              int32_t _M0L6_2atmpS1548 = _M0L4attrS649[4];
              int32_t _M0L6_2atmpS1490;
              int32_t _M0L4_2axS654;
              moonbit_decref(_M0L4attrS649);
              _M0L6_2atmpS1490 = _M0L6_2atmpS1548;
              _M0L4_2axS654 = _M0L6_2atmpS1490;
              if (_M0L4_2axS654 == 99) {
                void* _M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1489;
                struct moonbit_result_0 _result_1776;
                moonbit_decref(_M0L17error__to__stringS661);
                moonbit_decref(_M0L14handle__resultS659);
                moonbit_decref(_M0L5_2aitS647);
                moonbit_decref(_M0L1fS641);
                _M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1489
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1489)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 1);
                ((struct _M0DTPC15error5Error127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1489)->$0
                = _M0L4nameS643;
                _result_1776.tag = 0;
                _result_1776.data.err
                = _M0L127clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1489;
                return _result_1776;
              }
            } else {
              moonbit_decref(_M0L4attrS649);
            }
          } else {
            moonbit_decref(_M0L4attrS649);
          }
        } else {
          moonbit_decref(_M0L4attrS649);
        }
      } else {
        moonbit_decref(_M0L4attrS649);
      }
    } else {
      moonbit_decref(_M0L4attrS649);
    }
    continue;
    joinlet_1775:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS659);
  moonbit_incref(_M0L4nameS643);
  _closure_1777
  = (struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__*)moonbit_malloc(sizeof(struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__));
  Moonbit_object_header(_closure_1777)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__, $0) >> 2, 2, 0);
  _closure_1777->code
  = &_M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testC1501l427;
  _closure_1777->$0 = _M0L14handle__resultS659;
  _closure_1777->$1 = _M0L4nameS643;
  _M0L6_2atmpS1495 = (struct _M0TWEu*)_closure_1777;
  _closure_1778
  = (struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__*)moonbit_malloc(sizeof(struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__));
  Moonbit_object_header(_closure_1778)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__, $0) >> 2, 3, 0);
  _closure_1778->code
  = &_M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testC1497l428;
  _closure_1778->$0 = _M0L17error__to__stringS661;
  _closure_1778->$1 = _M0L14handle__resultS659;
  _closure_1778->$2 = _M0L4nameS643;
  _M0L6_2atmpS1496 = (struct _M0TWRPC15error5ErrorEu*)_closure_1778;
  #line 425 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal22signal__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS641, _M0L6_2atmpS1495, _M0L6_2atmpS1496);
  _result_1779.tag = 1;
  _result_1779.data.ok = 1;
  return _result_1779;
}

int32_t _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testC1501l427(
  struct _M0TWEu* _M0L6_2aenvS1502
) {
  struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__* _M0L14_2acasted__envS1503;
  moonbit_string_t _M0L8_2afieldS1553;
  moonbit_string_t _M0L4nameS643;
  struct _M0TWssbEu* _M0L8_2afieldS1552;
  int32_t _M0L6_2acntS1711;
  struct _M0TWssbEu* _M0L14handle__resultS659;
  #line 427 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS1503
  = (struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1501__l427__*)_M0L6_2aenvS1502;
  _M0L8_2afieldS1553 = _M0L14_2acasted__envS1503->$1;
  _M0L4nameS643 = _M0L8_2afieldS1553;
  _M0L8_2afieldS1552 = _M0L14_2acasted__envS1503->$0;
  _M0L6_2acntS1711 = Moonbit_object_header(_M0L14_2acasted__envS1503)->rc;
  if (_M0L6_2acntS1711 > 1) {
    int32_t _M0L11_2anew__cntS1712 = _M0L6_2acntS1711 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1503)->rc
    = _M0L11_2anew__cntS1712;
    moonbit_incref(_M0L4nameS643);
    moonbit_incref(_M0L8_2afieldS1552);
  } else if (_M0L6_2acntS1711 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1503);
  }
  _M0L14handle__resultS659 = _M0L8_2afieldS1552;
  #line 427 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS659->code(_M0L14handle__resultS659, _M0L4nameS643, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal22signal__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testC1497l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS1498,
  void* _M0L3errS660
) {
  struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__* _M0L14_2acasted__envS1499;
  moonbit_string_t _M0L8_2afieldS1556;
  moonbit_string_t _M0L4nameS643;
  struct _M0TWssbEu* _M0L8_2afieldS1555;
  struct _M0TWssbEu* _M0L14handle__resultS659;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS1554;
  int32_t _M0L6_2acntS1713;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS661;
  moonbit_string_t _M0L6_2atmpS1500;
  #line 428 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS1499
  = (struct _M0R227_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fsignal__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1497__l428__*)_M0L6_2aenvS1498;
  _M0L8_2afieldS1556 = _M0L14_2acasted__envS1499->$2;
  _M0L4nameS643 = _M0L8_2afieldS1556;
  _M0L8_2afieldS1555 = _M0L14_2acasted__envS1499->$1;
  _M0L14handle__resultS659 = _M0L8_2afieldS1555;
  _M0L8_2afieldS1554 = _M0L14_2acasted__envS1499->$0;
  _M0L6_2acntS1713 = Moonbit_object_header(_M0L14_2acasted__envS1499)->rc;
  if (_M0L6_2acntS1713 > 1) {
    int32_t _M0L11_2anew__cntS1714 = _M0L6_2acntS1713 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1499)->rc
    = _M0L11_2anew__cntS1714;
    moonbit_incref(_M0L4nameS643);
    moonbit_incref(_M0L14handle__resultS659);
    moonbit_incref(_M0L8_2afieldS1554);
  } else if (_M0L6_2acntS1713 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1499);
  }
  _M0L17error__to__stringS661 = _M0L8_2afieldS1554;
  #line 428 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1500
  = _M0L17error__to__stringS661->code(_M0L17error__to__stringS661, _M0L3errS660);
  #line 428 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS659->code(_M0L14handle__resultS659, _M0L4nameS643, _M0L6_2atmpS1500, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS633,
  struct _M0TWEu* _M0L6on__okS634,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS631
) {
  void* _M0L11_2atry__errS629;
  struct moonbit_result_0 _tmp_1781;
  void* _M0L3errS630;
  #line 375 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _tmp_1781 = _M0L1fS633->code(_M0L1fS633);
  if (_tmp_1781.tag) {
    int32_t const _M0L5_2aokS1485 = _tmp_1781.data.ok;
    moonbit_decref(_M0L7on__errS631);
  } else {
    void* const _M0L6_2aerrS1486 = _tmp_1781.data.err;
    moonbit_decref(_M0L6on__okS634);
    _M0L11_2atry__errS629 = _M0L6_2aerrS1486;
    goto join_628;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS634->code(_M0L6on__okS634);
  goto joinlet_1780;
  join_628:;
  _M0L3errS630 = _M0L11_2atry__errS629;
  #line 383 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS631->code(_M0L7on__errS631, _M0L3errS630);
  joinlet_1780:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S588;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS601;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS606;
  struct _M0TUsiE** _M0L6_2atmpS1484;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS613;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS614;
  moonbit_string_t _M0L6_2atmpS1483;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS615;
  int32_t _M0L7_2abindS616;
  int32_t _M0L2__S617;
  #line 193 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S588 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS601
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS606 = 0;
  _M0L6_2atmpS1484 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS613
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS613)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS613->$0 = _M0L6_2atmpS1484;
  _M0L16file__and__indexS613->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS614
  = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS601(_M0L57moonbit__test__driver__internal__get__cli__args__internalS601);
  #line 284 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1483 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS614, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS615
  = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS606(_M0L51moonbit__test__driver__internal__split__mbt__stringS606, _M0L6_2atmpS1483, 47);
  _M0L7_2abindS616 = _M0L10test__argsS615->$1;
  _M0L2__S617 = 0;
  while (1) {
    if (_M0L2__S617 < _M0L7_2abindS616) {
      moonbit_string_t* _M0L8_2afieldS1558 = _M0L10test__argsS615->$0;
      moonbit_string_t* _M0L3bufS1482 = _M0L8_2afieldS1558;
      moonbit_string_t _M0L6_2atmpS1557 =
        (moonbit_string_t)_M0L3bufS1482[_M0L2__S617];
      moonbit_string_t _M0L3argS618 = _M0L6_2atmpS1557;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS619;
      moonbit_string_t _M0L4fileS620;
      moonbit_string_t _M0L5rangeS621;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS622;
      moonbit_string_t _M0L6_2atmpS1480;
      int32_t _M0L5startS623;
      moonbit_string_t _M0L6_2atmpS1479;
      int32_t _M0L3endS624;
      int32_t _M0L1iS625;
      int32_t _M0L6_2atmpS1481;
      moonbit_incref(_M0L3argS618);
      #line 288 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS619
      = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS606(_M0L51moonbit__test__driver__internal__split__mbt__stringS606, _M0L3argS618, 58);
      moonbit_incref(_M0L16file__and__rangeS619);
      #line 289 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS620
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS619, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS621
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS619, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS622
      = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS606(_M0L51moonbit__test__driver__internal__split__mbt__stringS606, _M0L5rangeS621, 45);
      moonbit_incref(_M0L15start__and__endS622);
      #line 294 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1480
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS622, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS623
      = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S588(_M0L45moonbit__test__driver__internal__parse__int__S588, _M0L6_2atmpS1480);
      #line 295 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1479
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS622, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS624
      = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S588(_M0L45moonbit__test__driver__internal__parse__int__S588, _M0L6_2atmpS1479);
      _M0L1iS625 = _M0L5startS623;
      while (1) {
        if (_M0L1iS625 < _M0L3endS624) {
          struct _M0TUsiE* _M0L8_2atupleS1477;
          int32_t _M0L6_2atmpS1478;
          moonbit_incref(_M0L4fileS620);
          _M0L8_2atupleS1477
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1477)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1477->$0 = _M0L4fileS620;
          _M0L8_2atupleS1477->$1 = _M0L1iS625;
          moonbit_incref(_M0L16file__and__indexS613);
          #line 297 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS613, _M0L8_2atupleS1477);
          _M0L6_2atmpS1478 = _M0L1iS625 + 1;
          _M0L1iS625 = _M0L6_2atmpS1478;
          continue;
        } else {
          moonbit_decref(_M0L4fileS620);
        }
        break;
      }
      _M0L6_2atmpS1481 = _M0L2__S617 + 1;
      _M0L2__S617 = _M0L6_2atmpS1481;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS615);
    }
    break;
  }
  return _M0L16file__and__indexS613;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS606(
  int32_t _M0L6_2aenvS1458,
  moonbit_string_t _M0L1sS607,
  int32_t _M0L3sepS608
) {
  moonbit_string_t* _M0L6_2atmpS1476;
  struct _M0TPB5ArrayGsE* _M0L3resS609;
  struct _M0TPC13ref3RefGiE* _M0L1iS610;
  struct _M0TPC13ref3RefGiE* _M0L5startS611;
  int32_t _M0L3valS1471;
  int32_t _M0L6_2atmpS1472;
  #line 261 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1476 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS609
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS609)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS609->$0 = _M0L6_2atmpS1476;
  _M0L3resS609->$1 = 0;
  _M0L1iS610
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS610)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS610->$0 = 0;
  _M0L5startS611
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS611)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS611->$0 = 0;
  while (1) {
    int32_t _M0L3valS1459 = _M0L1iS610->$0;
    int32_t _M0L6_2atmpS1460 = Moonbit_array_length(_M0L1sS607);
    if (_M0L3valS1459 < _M0L6_2atmpS1460) {
      int32_t _M0L3valS1463 = _M0L1iS610->$0;
      int32_t _M0L6_2atmpS1462;
      int32_t _M0L6_2atmpS1461;
      int32_t _M0L3valS1470;
      int32_t _M0L6_2atmpS1469;
      if (
        _M0L3valS1463 < 0
        || _M0L3valS1463 >= Moonbit_array_length(_M0L1sS607)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1462 = _M0L1sS607[_M0L3valS1463];
      _M0L6_2atmpS1461 = _M0L6_2atmpS1462;
      if (_M0L6_2atmpS1461 == _M0L3sepS608) {
        int32_t _M0L3valS1465 = _M0L5startS611->$0;
        int32_t _M0L3valS1466 = _M0L1iS610->$0;
        moonbit_string_t _M0L6_2atmpS1464;
        int32_t _M0L3valS1468;
        int32_t _M0L6_2atmpS1467;
        moonbit_incref(_M0L1sS607);
        #line 270 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS1464
        = _M0MPC16string6String17unsafe__substring(_M0L1sS607, _M0L3valS1465, _M0L3valS1466);
        moonbit_incref(_M0L3resS609);
        #line 270 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS609, _M0L6_2atmpS1464);
        _M0L3valS1468 = _M0L1iS610->$0;
        _M0L6_2atmpS1467 = _M0L3valS1468 + 1;
        _M0L5startS611->$0 = _M0L6_2atmpS1467;
      }
      _M0L3valS1470 = _M0L1iS610->$0;
      _M0L6_2atmpS1469 = _M0L3valS1470 + 1;
      _M0L1iS610->$0 = _M0L6_2atmpS1469;
      continue;
    } else {
      moonbit_decref(_M0L1iS610);
    }
    break;
  }
  _M0L3valS1471 = _M0L5startS611->$0;
  _M0L6_2atmpS1472 = Moonbit_array_length(_M0L1sS607);
  if (_M0L3valS1471 < _M0L6_2atmpS1472) {
    int32_t _M0L8_2afieldS1559 = _M0L5startS611->$0;
    int32_t _M0L3valS1474;
    int32_t _M0L6_2atmpS1475;
    moonbit_string_t _M0L6_2atmpS1473;
    moonbit_decref(_M0L5startS611);
    _M0L3valS1474 = _M0L8_2afieldS1559;
    _M0L6_2atmpS1475 = Moonbit_array_length(_M0L1sS607);
    #line 276 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS1473
    = _M0MPC16string6String17unsafe__substring(_M0L1sS607, _M0L3valS1474, _M0L6_2atmpS1475);
    moonbit_incref(_M0L3resS609);
    #line 276 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS609, _M0L6_2atmpS1473);
  } else {
    moonbit_decref(_M0L5startS611);
    moonbit_decref(_M0L1sS607);
  }
  return _M0L3resS609;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS601(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594
) {
  moonbit_bytes_t* _M0L3tmpS602;
  int32_t _M0L6_2atmpS1457;
  struct _M0TPB5ArrayGsE* _M0L3resS603;
  int32_t _M0L1iS604;
  #line 250 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS602
  = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1457 = Moonbit_array_length(_M0L3tmpS602);
  #line 254 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS603 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1457);
  _M0L1iS604 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1453 = Moonbit_array_length(_M0L3tmpS602);
    if (_M0L1iS604 < _M0L6_2atmpS1453) {
      moonbit_bytes_t _M0L6_2atmpS1560;
      moonbit_bytes_t _M0L6_2atmpS1455;
      moonbit_string_t _M0L6_2atmpS1454;
      int32_t _M0L6_2atmpS1456;
      if (_M0L1iS604 < 0 || _M0L1iS604 >= Moonbit_array_length(_M0L3tmpS602)) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1560 = (moonbit_bytes_t)_M0L3tmpS602[_M0L1iS604];
      _M0L6_2atmpS1455 = _M0L6_2atmpS1560;
      moonbit_incref(_M0L6_2atmpS1455);
      #line 256 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1454
      = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594, _M0L6_2atmpS1455);
      moonbit_incref(_M0L3resS603);
      #line 256 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS603, _M0L6_2atmpS1454);
      _M0L6_2atmpS1456 = _M0L1iS604 + 1;
      _M0L1iS604 = _M0L6_2atmpS1456;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS602);
    }
    break;
  }
  return _M0L3resS603;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS594(
  int32_t _M0L6_2aenvS1367,
  moonbit_bytes_t _M0L5bytesS595
) {
  struct _M0TPB13StringBuilder* _M0L3resS596;
  int32_t _M0L3lenS597;
  struct _M0TPC13ref3RefGiE* _M0L1iS598;
  #line 206 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS596 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS597 = Moonbit_array_length(_M0L5bytesS595);
  _M0L1iS598
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS598)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS598->$0 = 0;
  while (1) {
    int32_t _M0L3valS1368 = _M0L1iS598->$0;
    if (_M0L3valS1368 < _M0L3lenS597) {
      int32_t _M0L3valS1452 = _M0L1iS598->$0;
      int32_t _M0L6_2atmpS1451;
      int32_t _M0L6_2atmpS1450;
      struct _M0TPC13ref3RefGiE* _M0L1cS599;
      int32_t _M0L3valS1369;
      if (
        _M0L3valS1452 < 0
        || _M0L3valS1452 >= Moonbit_array_length(_M0L5bytesS595)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1451 = _M0L5bytesS595[_M0L3valS1452];
      _M0L6_2atmpS1450 = (int32_t)_M0L6_2atmpS1451;
      _M0L1cS599
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS599)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS599->$0 = _M0L6_2atmpS1450;
      _M0L3valS1369 = _M0L1cS599->$0;
      if (_M0L3valS1369 < 128) {
        int32_t _M0L8_2afieldS1561 = _M0L1cS599->$0;
        int32_t _M0L3valS1371;
        int32_t _M0L6_2atmpS1370;
        int32_t _M0L3valS1373;
        int32_t _M0L6_2atmpS1372;
        moonbit_decref(_M0L1cS599);
        _M0L3valS1371 = _M0L8_2afieldS1561;
        _M0L6_2atmpS1370 = _M0L3valS1371;
        moonbit_incref(_M0L3resS596);
        #line 215 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS596, _M0L6_2atmpS1370);
        _M0L3valS1373 = _M0L1iS598->$0;
        _M0L6_2atmpS1372 = _M0L3valS1373 + 1;
        _M0L1iS598->$0 = _M0L6_2atmpS1372;
      } else {
        int32_t _M0L3valS1374 = _M0L1cS599->$0;
        if (_M0L3valS1374 < 224) {
          int32_t _M0L3valS1376 = _M0L1iS598->$0;
          int32_t _M0L6_2atmpS1375 = _M0L3valS1376 + 1;
          int32_t _M0L3valS1385;
          int32_t _M0L6_2atmpS1384;
          int32_t _M0L6_2atmpS1378;
          int32_t _M0L3valS1383;
          int32_t _M0L6_2atmpS1382;
          int32_t _M0L6_2atmpS1381;
          int32_t _M0L6_2atmpS1380;
          int32_t _M0L6_2atmpS1379;
          int32_t _M0L6_2atmpS1377;
          int32_t _M0L8_2afieldS1562;
          int32_t _M0L3valS1387;
          int32_t _M0L6_2atmpS1386;
          int32_t _M0L3valS1389;
          int32_t _M0L6_2atmpS1388;
          if (_M0L6_2atmpS1375 >= _M0L3lenS597) {
            moonbit_decref(_M0L1cS599);
            moonbit_decref(_M0L1iS598);
            moonbit_decref(_M0L5bytesS595);
            break;
          }
          _M0L3valS1385 = _M0L1cS599->$0;
          _M0L6_2atmpS1384 = _M0L3valS1385 & 31;
          _M0L6_2atmpS1378 = _M0L6_2atmpS1384 << 6;
          _M0L3valS1383 = _M0L1iS598->$0;
          _M0L6_2atmpS1382 = _M0L3valS1383 + 1;
          if (
            _M0L6_2atmpS1382 < 0
            || _M0L6_2atmpS1382 >= Moonbit_array_length(_M0L5bytesS595)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1381 = _M0L5bytesS595[_M0L6_2atmpS1382];
          _M0L6_2atmpS1380 = (int32_t)_M0L6_2atmpS1381;
          _M0L6_2atmpS1379 = _M0L6_2atmpS1380 & 63;
          _M0L6_2atmpS1377 = _M0L6_2atmpS1378 | _M0L6_2atmpS1379;
          _M0L1cS599->$0 = _M0L6_2atmpS1377;
          _M0L8_2afieldS1562 = _M0L1cS599->$0;
          moonbit_decref(_M0L1cS599);
          _M0L3valS1387 = _M0L8_2afieldS1562;
          _M0L6_2atmpS1386 = _M0L3valS1387;
          moonbit_incref(_M0L3resS596);
          #line 222 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS596, _M0L6_2atmpS1386);
          _M0L3valS1389 = _M0L1iS598->$0;
          _M0L6_2atmpS1388 = _M0L3valS1389 + 2;
          _M0L1iS598->$0 = _M0L6_2atmpS1388;
        } else {
          int32_t _M0L3valS1390 = _M0L1cS599->$0;
          if (_M0L3valS1390 < 240) {
            int32_t _M0L3valS1392 = _M0L1iS598->$0;
            int32_t _M0L6_2atmpS1391 = _M0L3valS1392 + 2;
            int32_t _M0L3valS1408;
            int32_t _M0L6_2atmpS1407;
            int32_t _M0L6_2atmpS1400;
            int32_t _M0L3valS1406;
            int32_t _M0L6_2atmpS1405;
            int32_t _M0L6_2atmpS1404;
            int32_t _M0L6_2atmpS1403;
            int32_t _M0L6_2atmpS1402;
            int32_t _M0L6_2atmpS1401;
            int32_t _M0L6_2atmpS1394;
            int32_t _M0L3valS1399;
            int32_t _M0L6_2atmpS1398;
            int32_t _M0L6_2atmpS1397;
            int32_t _M0L6_2atmpS1396;
            int32_t _M0L6_2atmpS1395;
            int32_t _M0L6_2atmpS1393;
            int32_t _M0L8_2afieldS1563;
            int32_t _M0L3valS1410;
            int32_t _M0L6_2atmpS1409;
            int32_t _M0L3valS1412;
            int32_t _M0L6_2atmpS1411;
            if (_M0L6_2atmpS1391 >= _M0L3lenS597) {
              moonbit_decref(_M0L1cS599);
              moonbit_decref(_M0L1iS598);
              moonbit_decref(_M0L5bytesS595);
              break;
            }
            _M0L3valS1408 = _M0L1cS599->$0;
            _M0L6_2atmpS1407 = _M0L3valS1408 & 15;
            _M0L6_2atmpS1400 = _M0L6_2atmpS1407 << 12;
            _M0L3valS1406 = _M0L1iS598->$0;
            _M0L6_2atmpS1405 = _M0L3valS1406 + 1;
            if (
              _M0L6_2atmpS1405 < 0
              || _M0L6_2atmpS1405 >= Moonbit_array_length(_M0L5bytesS595)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1404 = _M0L5bytesS595[_M0L6_2atmpS1405];
            _M0L6_2atmpS1403 = (int32_t)_M0L6_2atmpS1404;
            _M0L6_2atmpS1402 = _M0L6_2atmpS1403 & 63;
            _M0L6_2atmpS1401 = _M0L6_2atmpS1402 << 6;
            _M0L6_2atmpS1394 = _M0L6_2atmpS1400 | _M0L6_2atmpS1401;
            _M0L3valS1399 = _M0L1iS598->$0;
            _M0L6_2atmpS1398 = _M0L3valS1399 + 2;
            if (
              _M0L6_2atmpS1398 < 0
              || _M0L6_2atmpS1398 >= Moonbit_array_length(_M0L5bytesS595)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1397 = _M0L5bytesS595[_M0L6_2atmpS1398];
            _M0L6_2atmpS1396 = (int32_t)_M0L6_2atmpS1397;
            _M0L6_2atmpS1395 = _M0L6_2atmpS1396 & 63;
            _M0L6_2atmpS1393 = _M0L6_2atmpS1394 | _M0L6_2atmpS1395;
            _M0L1cS599->$0 = _M0L6_2atmpS1393;
            _M0L8_2afieldS1563 = _M0L1cS599->$0;
            moonbit_decref(_M0L1cS599);
            _M0L3valS1410 = _M0L8_2afieldS1563;
            _M0L6_2atmpS1409 = _M0L3valS1410;
            moonbit_incref(_M0L3resS596);
            #line 231 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS596, _M0L6_2atmpS1409);
            _M0L3valS1412 = _M0L1iS598->$0;
            _M0L6_2atmpS1411 = _M0L3valS1412 + 3;
            _M0L1iS598->$0 = _M0L6_2atmpS1411;
          } else {
            int32_t _M0L3valS1414 = _M0L1iS598->$0;
            int32_t _M0L6_2atmpS1413 = _M0L3valS1414 + 3;
            int32_t _M0L3valS1437;
            int32_t _M0L6_2atmpS1436;
            int32_t _M0L6_2atmpS1429;
            int32_t _M0L3valS1435;
            int32_t _M0L6_2atmpS1434;
            int32_t _M0L6_2atmpS1433;
            int32_t _M0L6_2atmpS1432;
            int32_t _M0L6_2atmpS1431;
            int32_t _M0L6_2atmpS1430;
            int32_t _M0L6_2atmpS1422;
            int32_t _M0L3valS1428;
            int32_t _M0L6_2atmpS1427;
            int32_t _M0L6_2atmpS1426;
            int32_t _M0L6_2atmpS1425;
            int32_t _M0L6_2atmpS1424;
            int32_t _M0L6_2atmpS1423;
            int32_t _M0L6_2atmpS1416;
            int32_t _M0L3valS1421;
            int32_t _M0L6_2atmpS1420;
            int32_t _M0L6_2atmpS1419;
            int32_t _M0L6_2atmpS1418;
            int32_t _M0L6_2atmpS1417;
            int32_t _M0L6_2atmpS1415;
            int32_t _M0L3valS1439;
            int32_t _M0L6_2atmpS1438;
            int32_t _M0L3valS1443;
            int32_t _M0L6_2atmpS1442;
            int32_t _M0L6_2atmpS1441;
            int32_t _M0L6_2atmpS1440;
            int32_t _M0L8_2afieldS1564;
            int32_t _M0L3valS1447;
            int32_t _M0L6_2atmpS1446;
            int32_t _M0L6_2atmpS1445;
            int32_t _M0L6_2atmpS1444;
            int32_t _M0L3valS1449;
            int32_t _M0L6_2atmpS1448;
            if (_M0L6_2atmpS1413 >= _M0L3lenS597) {
              moonbit_decref(_M0L1cS599);
              moonbit_decref(_M0L1iS598);
              moonbit_decref(_M0L5bytesS595);
              break;
            }
            _M0L3valS1437 = _M0L1cS599->$0;
            _M0L6_2atmpS1436 = _M0L3valS1437 & 7;
            _M0L6_2atmpS1429 = _M0L6_2atmpS1436 << 18;
            _M0L3valS1435 = _M0L1iS598->$0;
            _M0L6_2atmpS1434 = _M0L3valS1435 + 1;
            if (
              _M0L6_2atmpS1434 < 0
              || _M0L6_2atmpS1434 >= Moonbit_array_length(_M0L5bytesS595)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1433 = _M0L5bytesS595[_M0L6_2atmpS1434];
            _M0L6_2atmpS1432 = (int32_t)_M0L6_2atmpS1433;
            _M0L6_2atmpS1431 = _M0L6_2atmpS1432 & 63;
            _M0L6_2atmpS1430 = _M0L6_2atmpS1431 << 12;
            _M0L6_2atmpS1422 = _M0L6_2atmpS1429 | _M0L6_2atmpS1430;
            _M0L3valS1428 = _M0L1iS598->$0;
            _M0L6_2atmpS1427 = _M0L3valS1428 + 2;
            if (
              _M0L6_2atmpS1427 < 0
              || _M0L6_2atmpS1427 >= Moonbit_array_length(_M0L5bytesS595)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1426 = _M0L5bytesS595[_M0L6_2atmpS1427];
            _M0L6_2atmpS1425 = (int32_t)_M0L6_2atmpS1426;
            _M0L6_2atmpS1424 = _M0L6_2atmpS1425 & 63;
            _M0L6_2atmpS1423 = _M0L6_2atmpS1424 << 6;
            _M0L6_2atmpS1416 = _M0L6_2atmpS1422 | _M0L6_2atmpS1423;
            _M0L3valS1421 = _M0L1iS598->$0;
            _M0L6_2atmpS1420 = _M0L3valS1421 + 3;
            if (
              _M0L6_2atmpS1420 < 0
              || _M0L6_2atmpS1420 >= Moonbit_array_length(_M0L5bytesS595)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1419 = _M0L5bytesS595[_M0L6_2atmpS1420];
            _M0L6_2atmpS1418 = (int32_t)_M0L6_2atmpS1419;
            _M0L6_2atmpS1417 = _M0L6_2atmpS1418 & 63;
            _M0L6_2atmpS1415 = _M0L6_2atmpS1416 | _M0L6_2atmpS1417;
            _M0L1cS599->$0 = _M0L6_2atmpS1415;
            _M0L3valS1439 = _M0L1cS599->$0;
            _M0L6_2atmpS1438 = _M0L3valS1439 - 65536;
            _M0L1cS599->$0 = _M0L6_2atmpS1438;
            _M0L3valS1443 = _M0L1cS599->$0;
            _M0L6_2atmpS1442 = _M0L3valS1443 >> 10;
            _M0L6_2atmpS1441 = _M0L6_2atmpS1442 + 55296;
            _M0L6_2atmpS1440 = _M0L6_2atmpS1441;
            moonbit_incref(_M0L3resS596);
            #line 242 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS596, _M0L6_2atmpS1440);
            _M0L8_2afieldS1564 = _M0L1cS599->$0;
            moonbit_decref(_M0L1cS599);
            _M0L3valS1447 = _M0L8_2afieldS1564;
            _M0L6_2atmpS1446 = _M0L3valS1447 & 1023;
            _M0L6_2atmpS1445 = _M0L6_2atmpS1446 + 56320;
            _M0L6_2atmpS1444 = _M0L6_2atmpS1445;
            moonbit_incref(_M0L3resS596);
            #line 243 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS596, _M0L6_2atmpS1444);
            _M0L3valS1449 = _M0L1iS598->$0;
            _M0L6_2atmpS1448 = _M0L3valS1449 + 4;
            _M0L1iS598->$0 = _M0L6_2atmpS1448;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS598);
      moonbit_decref(_M0L5bytesS595);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS596);
}

int32_t _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S588(
  int32_t _M0L6_2aenvS1360,
  moonbit_string_t _M0L1sS589
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS590;
  int32_t _M0L3lenS591;
  int32_t _M0L1iS592;
  int32_t _M0L8_2afieldS1565;
  #line 197 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS590
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS590)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS590->$0 = 0;
  _M0L3lenS591 = Moonbit_array_length(_M0L1sS589);
  _M0L1iS592 = 0;
  while (1) {
    if (_M0L1iS592 < _M0L3lenS591) {
      int32_t _M0L3valS1365 = _M0L3resS590->$0;
      int32_t _M0L6_2atmpS1362 = _M0L3valS1365 * 10;
      int32_t _M0L6_2atmpS1364;
      int32_t _M0L6_2atmpS1363;
      int32_t _M0L6_2atmpS1361;
      int32_t _M0L6_2atmpS1366;
      if (_M0L1iS592 < 0 || _M0L1iS592 >= Moonbit_array_length(_M0L1sS589)) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1364 = _M0L1sS589[_M0L1iS592];
      _M0L6_2atmpS1363 = _M0L6_2atmpS1364 - 48;
      _M0L6_2atmpS1361 = _M0L6_2atmpS1362 + _M0L6_2atmpS1363;
      _M0L3resS590->$0 = _M0L6_2atmpS1361;
      _M0L6_2atmpS1366 = _M0L1iS592 + 1;
      _M0L1iS592 = _M0L6_2atmpS1366;
      continue;
    } else {
      moonbit_decref(_M0L1sS589);
    }
    break;
  }
  _M0L8_2afieldS1565 = _M0L3resS590->$0;
  moonbit_decref(_M0L3resS590);
  return _M0L8_2afieldS1565;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S568,
  moonbit_string_t _M0L12_2adiscard__S569,
  int32_t _M0L12_2adiscard__S570,
  struct _M0TWssbEu* _M0L12_2adiscard__S571,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S572
) {
  struct moonbit_result_0 _result_1788;
  #line 34 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S572);
  moonbit_decref(_M0L12_2adiscard__S571);
  moonbit_decref(_M0L12_2adiscard__S569);
  moonbit_decref(_M0L12_2adiscard__S568);
  _result_1788.tag = 1;
  _result_1788.data.ok = 0;
  return _result_1788;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S573,
  moonbit_string_t _M0L12_2adiscard__S574,
  int32_t _M0L12_2adiscard__S575,
  struct _M0TWssbEu* _M0L12_2adiscard__S576,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S577
) {
  struct moonbit_result_0 _result_1789;
  #line 34 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S577);
  moonbit_decref(_M0L12_2adiscard__S576);
  moonbit_decref(_M0L12_2adiscard__S574);
  moonbit_decref(_M0L12_2adiscard__S573);
  _result_1789.tag = 1;
  _result_1789.data.ok = 0;
  return _result_1789;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S578,
  moonbit_string_t _M0L12_2adiscard__S579,
  int32_t _M0L12_2adiscard__S580,
  struct _M0TWssbEu* _M0L12_2adiscard__S581,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S582
) {
  struct moonbit_result_0 _result_1790;
  #line 34 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S582);
  moonbit_decref(_M0L12_2adiscard__S581);
  moonbit_decref(_M0L12_2adiscard__S579);
  moonbit_decref(_M0L12_2adiscard__S578);
  _result_1790.tag = 1;
  _result_1790.data.ok = 0;
  return _result_1790;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal22signal__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S583,
  moonbit_string_t _M0L12_2adiscard__S584,
  int32_t _M0L12_2adiscard__S585,
  struct _M0TWssbEu* _M0L12_2adiscard__S586,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S587
) {
  struct moonbit_result_0 _result_1791;
  #line 34 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S587);
  moonbit_decref(_M0L12_2adiscard__S586);
  moonbit_decref(_M0L12_2adiscard__S584);
  moonbit_decref(_M0L12_2adiscard__S583);
  _result_1791.tag = 1;
  _result_1791.data.ok = 0;
  return _result_1791;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal22signal__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S567
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S567);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal22signal__blackbox__test43____test__7369676e616c5f746573742e6d6274__0(
  
) {
  int32_t _M0L6_2atmpS1359;
  struct moonbit_result_0 _result_1792;
  #line 2 "E:\\moonbit\\clawteam\\internal\\signal\\signal_test.mbt"
  _M0L6_2atmpS1359 = 0;
  _result_1792.tag = 1;
  _result_1792.data.ok = _M0L6_2atmpS1359;
  return _result_1792;
}

int32_t _M0FP48clawteam8clawteam8internal6signal20check__signal__value(
  int32_t _M0L5valueS565,
  moonbit_string_t _M0L4nameS566
) {
  #line 5 "E:\\moonbit\\clawteam\\internal\\signal\\signal.mbt"
  if (_M0L5valueS565 == -1) {
    moonbit_string_t _M0L6_2atmpS1358;
    moonbit_string_t _M0L6_2atmpS1566;
    moonbit_string_t _M0L6_2atmpS1357;
    #line 7 "E:\\moonbit\\clawteam\\internal\\signal\\signal.mbt"
    _M0L6_2atmpS1358
    = _M0IPC16string6StringPB4Show10to__string(_M0L4nameS566);
    #line 7 "E:\\moonbit\\clawteam\\internal\\signal\\signal.mbt"
    _M0L6_2atmpS1566
    = moonbit_add_string(_M0L6_2atmpS1358, (moonbit_string_t)moonbit_string_literal_9.data);
    moonbit_decref(_M0L6_2atmpS1358);
    _M0L6_2atmpS1357 = _M0L6_2atmpS1566;
    #line 7 "E:\\moonbit\\clawteam\\internal\\signal\\signal.mbt"
    _M0FPB5abortGuE(_M0L6_2atmpS1357, (moonbit_string_t)moonbit_string_literal_10.data);
  } else {
    moonbit_decref(_M0L4nameS566);
  }
  return _M0L5valueS565;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS563,
  struct _M0TPB6Logger _M0L6loggerS564
) {
  moonbit_string_t _M0L6_2atmpS1356;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1355;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1356 = _M0L4selfS563;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1355 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1356);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1355, _M0L6loggerS564);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS540,
  struct _M0TPB6Logger _M0L6loggerS562
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS1575;
  struct _M0TPC16string10StringView _M0L3pkgS539;
  moonbit_string_t _M0L7_2adataS541;
  int32_t _M0L8_2astartS542;
  int32_t _M0L6_2atmpS1354;
  int32_t _M0L6_2aendS543;
  int32_t _M0Lm9_2acursorS544;
  int32_t _M0Lm13accept__stateS545;
  int32_t _M0Lm10match__endS546;
  int32_t _M0Lm20match__tag__saver__0S547;
  int32_t _M0Lm6tag__0S548;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS549;
  struct _M0TPC16string10StringView _M0L8_2afieldS1574;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS558;
  void* _M0L8_2afieldS1573;
  int32_t _M0L6_2acntS1715;
  void* _M0L16_2apackage__nameS559;
  struct _M0TPC16string10StringView _M0L8_2afieldS1571;
  struct _M0TPC16string10StringView _M0L8filenameS1331;
  struct _M0TPC16string10StringView _M0L8_2afieldS1570;
  struct _M0TPC16string10StringView _M0L11start__lineS1332;
  struct _M0TPC16string10StringView _M0L8_2afieldS1569;
  struct _M0TPC16string10StringView _M0L13start__columnS1333;
  struct _M0TPC16string10StringView _M0L8_2afieldS1568;
  struct _M0TPC16string10StringView _M0L9end__lineS1334;
  struct _M0TPC16string10StringView _M0L8_2afieldS1567;
  int32_t _M0L6_2acntS1719;
  struct _M0TPC16string10StringView _M0L11end__columnS1335;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS1575
  = (struct _M0TPC16string10StringView){
    _M0L4selfS540->$0_1, _M0L4selfS540->$0_2, _M0L4selfS540->$0_0
  };
  _M0L3pkgS539 = _M0L8_2afieldS1575;
  moonbit_incref(_M0L3pkgS539.$0);
  moonbit_incref(_M0L3pkgS539.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS541 = _M0MPC16string10StringView4data(_M0L3pkgS539);
  moonbit_incref(_M0L3pkgS539.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS542 = _M0MPC16string10StringView13start__offset(_M0L3pkgS539);
  moonbit_incref(_M0L3pkgS539.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1354 = _M0MPC16string10StringView6length(_M0L3pkgS539);
  _M0L6_2aendS543 = _M0L8_2astartS542 + _M0L6_2atmpS1354;
  _M0Lm9_2acursorS544 = _M0L8_2astartS542;
  _M0Lm13accept__stateS545 = -1;
  _M0Lm10match__endS546 = -1;
  _M0Lm20match__tag__saver__0S547 = -1;
  _M0Lm6tag__0S548 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1346 = _M0Lm9_2acursorS544;
    if (_M0L6_2atmpS1346 < _M0L6_2aendS543) {
      int32_t _M0L6_2atmpS1353 = _M0Lm9_2acursorS544;
      int32_t _M0L10next__charS553;
      int32_t _M0L6_2atmpS1347;
      moonbit_incref(_M0L7_2adataS541);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS553
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS541, _M0L6_2atmpS1353);
      _M0L6_2atmpS1347 = _M0Lm9_2acursorS544;
      _M0Lm9_2acursorS544 = _M0L6_2atmpS1347 + 1;
      if (_M0L10next__charS553 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1348;
          _M0Lm6tag__0S548 = _M0Lm9_2acursorS544;
          _M0L6_2atmpS1348 = _M0Lm9_2acursorS544;
          if (_M0L6_2atmpS1348 < _M0L6_2aendS543) {
            int32_t _M0L6_2atmpS1352 = _M0Lm9_2acursorS544;
            int32_t _M0L10next__charS554;
            int32_t _M0L6_2atmpS1349;
            moonbit_incref(_M0L7_2adataS541);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS554
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS541, _M0L6_2atmpS1352);
            _M0L6_2atmpS1349 = _M0Lm9_2acursorS544;
            _M0Lm9_2acursorS544 = _M0L6_2atmpS1349 + 1;
            if (_M0L10next__charS554 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1350 = _M0Lm9_2acursorS544;
                if (_M0L6_2atmpS1350 < _M0L6_2aendS543) {
                  int32_t _M0L6_2atmpS1351 = _M0Lm9_2acursorS544;
                  _M0Lm9_2acursorS544 = _M0L6_2atmpS1351 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S547 = _M0Lm6tag__0S548;
                  _M0Lm13accept__stateS545 = 0;
                  _M0Lm10match__endS546 = _M0Lm9_2acursorS544;
                  goto join_550;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_550;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_550;
    }
    break;
  }
  goto joinlet_1793;
  join_550:;
  switch (_M0Lm13accept__stateS545) {
    case 0: {
      int32_t _M0L6_2atmpS1344;
      int32_t _M0L6_2atmpS1343;
      int64_t _M0L6_2atmpS1340;
      int32_t _M0L6_2atmpS1342;
      int64_t _M0L6_2atmpS1341;
      struct _M0TPC16string10StringView _M0L13package__nameS551;
      int64_t _M0L6_2atmpS1337;
      int32_t _M0L6_2atmpS1339;
      int64_t _M0L6_2atmpS1338;
      struct _M0TPC16string10StringView _M0L12module__nameS552;
      void* _M0L4SomeS1336;
      moonbit_decref(_M0L3pkgS539.$0);
      _M0L6_2atmpS1344 = _M0Lm20match__tag__saver__0S547;
      _M0L6_2atmpS1343 = _M0L6_2atmpS1344 + 1;
      _M0L6_2atmpS1340 = (int64_t)_M0L6_2atmpS1343;
      _M0L6_2atmpS1342 = _M0Lm10match__endS546;
      _M0L6_2atmpS1341 = (int64_t)_M0L6_2atmpS1342;
      moonbit_incref(_M0L7_2adataS541);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS551
      = _M0MPC16string6String4view(_M0L7_2adataS541, _M0L6_2atmpS1340, _M0L6_2atmpS1341);
      _M0L6_2atmpS1337 = (int64_t)_M0L8_2astartS542;
      _M0L6_2atmpS1339 = _M0Lm20match__tag__saver__0S547;
      _M0L6_2atmpS1338 = (int64_t)_M0L6_2atmpS1339;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS552
      = _M0MPC16string6String4view(_M0L7_2adataS541, _M0L6_2atmpS1337, _M0L6_2atmpS1338);
      _M0L4SomeS1336
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1336)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1336)->$0_0
      = _M0L13package__nameS551.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1336)->$0_1
      = _M0L13package__nameS551.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1336)->$0_2
      = _M0L13package__nameS551.$2;
      _M0L7_2abindS549
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS549)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS549->$0_0 = _M0L12module__nameS552.$0;
      _M0L7_2abindS549->$0_1 = _M0L12module__nameS552.$1;
      _M0L7_2abindS549->$0_2 = _M0L12module__nameS552.$2;
      _M0L7_2abindS549->$1 = _M0L4SomeS1336;
      break;
    }
    default: {
      void* _M0L4NoneS1345;
      moonbit_decref(_M0L7_2adataS541);
      _M0L4NoneS1345
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS549
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS549)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS549->$0_0 = _M0L3pkgS539.$0;
      _M0L7_2abindS549->$0_1 = _M0L3pkgS539.$1;
      _M0L7_2abindS549->$0_2 = _M0L3pkgS539.$2;
      _M0L7_2abindS549->$1 = _M0L4NoneS1345;
      break;
    }
  }
  joinlet_1793:;
  _M0L8_2afieldS1574
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS549->$0_1, _M0L7_2abindS549->$0_2, _M0L7_2abindS549->$0_0
  };
  _M0L15_2amodule__nameS558 = _M0L8_2afieldS1574;
  _M0L8_2afieldS1573 = _M0L7_2abindS549->$1;
  _M0L6_2acntS1715 = Moonbit_object_header(_M0L7_2abindS549)->rc;
  if (_M0L6_2acntS1715 > 1) {
    int32_t _M0L11_2anew__cntS1716 = _M0L6_2acntS1715 - 1;
    Moonbit_object_header(_M0L7_2abindS549)->rc = _M0L11_2anew__cntS1716;
    moonbit_incref(_M0L8_2afieldS1573);
    moonbit_incref(_M0L15_2amodule__nameS558.$0);
  } else if (_M0L6_2acntS1715 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS549);
  }
  _M0L16_2apackage__nameS559 = _M0L8_2afieldS1573;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS559)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS560 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS559;
      struct _M0TPC16string10StringView _M0L8_2afieldS1572 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS560->$0_1,
                                              _M0L7_2aSomeS560->$0_2,
                                              _M0L7_2aSomeS560->$0_0};
      int32_t _M0L6_2acntS1717 = Moonbit_object_header(_M0L7_2aSomeS560)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS561;
      if (_M0L6_2acntS1717 > 1) {
        int32_t _M0L11_2anew__cntS1718 = _M0L6_2acntS1717 - 1;
        Moonbit_object_header(_M0L7_2aSomeS560)->rc = _M0L11_2anew__cntS1718;
        moonbit_incref(_M0L8_2afieldS1572.$0);
      } else if (_M0L6_2acntS1717 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS560);
      }
      _M0L12_2apkg__nameS561 = _M0L8_2afieldS1572;
      if (_M0L6loggerS562.$1) {
        moonbit_incref(_M0L6loggerS562.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L12_2apkg__nameS561);
      if (_M0L6loggerS562.$1) {
        moonbit_incref(_M0L6loggerS562.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS562.$0->$method_3(_M0L6loggerS562.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS559);
      break;
    }
  }
  _M0L8_2afieldS1571
  = (struct _M0TPC16string10StringView){
    _M0L4selfS540->$1_1, _M0L4selfS540->$1_2, _M0L4selfS540->$1_0
  };
  _M0L8filenameS1331 = _M0L8_2afieldS1571;
  moonbit_incref(_M0L8filenameS1331.$0);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L8filenameS1331);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_3(_M0L6loggerS562.$1, 58);
  _M0L8_2afieldS1570
  = (struct _M0TPC16string10StringView){
    _M0L4selfS540->$2_1, _M0L4selfS540->$2_2, _M0L4selfS540->$2_0
  };
  _M0L11start__lineS1332 = _M0L8_2afieldS1570;
  moonbit_incref(_M0L11start__lineS1332.$0);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L11start__lineS1332);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_3(_M0L6loggerS562.$1, 58);
  _M0L8_2afieldS1569
  = (struct _M0TPC16string10StringView){
    _M0L4selfS540->$3_1, _M0L4selfS540->$3_2, _M0L4selfS540->$3_0
  };
  _M0L13start__columnS1333 = _M0L8_2afieldS1569;
  moonbit_incref(_M0L13start__columnS1333.$0);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L13start__columnS1333);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_3(_M0L6loggerS562.$1, 45);
  _M0L8_2afieldS1568
  = (struct _M0TPC16string10StringView){
    _M0L4selfS540->$4_1, _M0L4selfS540->$4_2, _M0L4selfS540->$4_0
  };
  _M0L9end__lineS1334 = _M0L8_2afieldS1568;
  moonbit_incref(_M0L9end__lineS1334.$0);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L9end__lineS1334);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_3(_M0L6loggerS562.$1, 58);
  _M0L8_2afieldS1567
  = (struct _M0TPC16string10StringView){
    _M0L4selfS540->$5_1, _M0L4selfS540->$5_2, _M0L4selfS540->$5_0
  };
  _M0L6_2acntS1719 = Moonbit_object_header(_M0L4selfS540)->rc;
  if (_M0L6_2acntS1719 > 1) {
    int32_t _M0L11_2anew__cntS1725 = _M0L6_2acntS1719 - 1;
    Moonbit_object_header(_M0L4selfS540)->rc = _M0L11_2anew__cntS1725;
    moonbit_incref(_M0L8_2afieldS1567.$0);
  } else if (_M0L6_2acntS1719 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS1724 =
      (struct _M0TPC16string10StringView){_M0L4selfS540->$4_1,
                                            _M0L4selfS540->$4_2,
                                            _M0L4selfS540->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS1723;
    struct _M0TPC16string10StringView _M0L8_2afieldS1722;
    struct _M0TPC16string10StringView _M0L8_2afieldS1721;
    struct _M0TPC16string10StringView _M0L8_2afieldS1720;
    moonbit_decref(_M0L8_2afieldS1724.$0);
    _M0L8_2afieldS1723
    = (struct _M0TPC16string10StringView){
      _M0L4selfS540->$3_1, _M0L4selfS540->$3_2, _M0L4selfS540->$3_0
    };
    moonbit_decref(_M0L8_2afieldS1723.$0);
    _M0L8_2afieldS1722
    = (struct _M0TPC16string10StringView){
      _M0L4selfS540->$2_1, _M0L4selfS540->$2_2, _M0L4selfS540->$2_0
    };
    moonbit_decref(_M0L8_2afieldS1722.$0);
    _M0L8_2afieldS1721
    = (struct _M0TPC16string10StringView){
      _M0L4selfS540->$1_1, _M0L4selfS540->$1_2, _M0L4selfS540->$1_0
    };
    moonbit_decref(_M0L8_2afieldS1721.$0);
    _M0L8_2afieldS1720
    = (struct _M0TPC16string10StringView){
      _M0L4selfS540->$0_1, _M0L4selfS540->$0_2, _M0L4selfS540->$0_0
    };
    moonbit_decref(_M0L8_2afieldS1720.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS540);
  }
  _M0L11end__columnS1335 = _M0L8_2afieldS1567;
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L11end__columnS1335);
  if (_M0L6loggerS562.$1) {
    moonbit_incref(_M0L6loggerS562.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_3(_M0L6loggerS562.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS562.$0->$method_2(_M0L6loggerS562.$1, _M0L15_2amodule__nameS558);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS538) {
  moonbit_string_t _M0L6_2atmpS1330;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS1330 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS538);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS1330);
  moonbit_decref(_M0L6_2atmpS1330);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS537,
  struct _M0TPB6Hasher* _M0L6hasherS536
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS536, _M0L4selfS537);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS535,
  struct _M0TPB6Hasher* _M0L6hasherS534
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS534, _M0L4selfS535);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS532,
  moonbit_string_t _M0L5valueS530
) {
  int32_t _M0L7_2abindS529;
  int32_t _M0L1iS531;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS529 = Moonbit_array_length(_M0L5valueS530);
  _M0L1iS531 = 0;
  while (1) {
    if (_M0L1iS531 < _M0L7_2abindS529) {
      int32_t _M0L6_2atmpS1328 = _M0L5valueS530[_M0L1iS531];
      int32_t _M0L6_2atmpS1327 = (int32_t)_M0L6_2atmpS1328;
      uint32_t _M0L6_2atmpS1326 = *(uint32_t*)&_M0L6_2atmpS1327;
      int32_t _M0L6_2atmpS1329;
      moonbit_incref(_M0L4selfS532);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS532, _M0L6_2atmpS1326);
      _M0L6_2atmpS1329 = _M0L1iS531 + 1;
      _M0L1iS531 = _M0L6_2atmpS1329;
      continue;
    } else {
      moonbit_decref(_M0L4selfS532);
      moonbit_decref(_M0L5valueS530);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS527,
  int32_t _M0L3idxS528
) {
  int32_t _M0L6_2atmpS1576;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1576 = _M0L4selfS527[_M0L3idxS528];
  moonbit_decref(_M0L4selfS527);
  return _M0L6_2atmpS1576;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS514,
  int32_t _M0L3keyS510
) {
  int32_t _M0L4hashS509;
  int32_t _M0L14capacity__maskS1311;
  int32_t _M0L6_2atmpS1310;
  int32_t _M0L1iS511;
  int32_t _M0L3idxS512;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS509 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS510);
  _M0L14capacity__maskS1311 = _M0L4selfS514->$3;
  _M0L6_2atmpS1310 = _M0L4hashS509 & _M0L14capacity__maskS1311;
  _M0L1iS511 = 0;
  _M0L3idxS512 = _M0L6_2atmpS1310;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1580 =
      _M0L4selfS514->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1309 =
      _M0L8_2afieldS1580;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1579;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS513;
    if (
      _M0L3idxS512 < 0
      || _M0L3idxS512 >= Moonbit_array_length(_M0L7entriesS1309)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1579
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1309[
        _M0L3idxS512
      ];
    _M0L7_2abindS513 = _M0L6_2atmpS1579;
    if (_M0L7_2abindS513 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1298;
      if (_M0L7_2abindS513) {
        moonbit_incref(_M0L7_2abindS513);
      }
      moonbit_decref(_M0L4selfS514);
      if (_M0L7_2abindS513) {
        moonbit_decref(_M0L7_2abindS513);
      }
      _M0L6_2atmpS1298 = 0;
      return _M0L6_2atmpS1298;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS515 =
        _M0L7_2abindS513;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS516 =
        _M0L7_2aSomeS515;
      int32_t _M0L4hashS1300 = _M0L8_2aentryS516->$3;
      int32_t _if__result_1799;
      int32_t _M0L8_2afieldS1577;
      int32_t _M0L3pslS1303;
      int32_t _M0L6_2atmpS1305;
      int32_t _M0L6_2atmpS1307;
      int32_t _M0L14capacity__maskS1308;
      int32_t _M0L6_2atmpS1306;
      if (_M0L4hashS1300 == _M0L4hashS509) {
        int32_t _M0L3keyS1299 = _M0L8_2aentryS516->$4;
        _if__result_1799 = _M0L3keyS1299 == _M0L3keyS510;
      } else {
        _if__result_1799 = 0;
      }
      if (_if__result_1799) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1578;
        int32_t _M0L6_2acntS1726;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1302;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1301;
        moonbit_incref(_M0L8_2aentryS516);
        moonbit_decref(_M0L4selfS514);
        _M0L8_2afieldS1578 = _M0L8_2aentryS516->$5;
        _M0L6_2acntS1726 = Moonbit_object_header(_M0L8_2aentryS516)->rc;
        if (_M0L6_2acntS1726 > 1) {
          int32_t _M0L11_2anew__cntS1728 = _M0L6_2acntS1726 - 1;
          Moonbit_object_header(_M0L8_2aentryS516)->rc
          = _M0L11_2anew__cntS1728;
          moonbit_incref(_M0L8_2afieldS1578);
        } else if (_M0L6_2acntS1726 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1727 =
            _M0L8_2aentryS516->$1;
          if (_M0L8_2afieldS1727) {
            moonbit_decref(_M0L8_2afieldS1727);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS516);
        }
        _M0L5valueS1302 = _M0L8_2afieldS1578;
        _M0L6_2atmpS1301 = _M0L5valueS1302;
        return _M0L6_2atmpS1301;
      } else {
        moonbit_incref(_M0L8_2aentryS516);
      }
      _M0L8_2afieldS1577 = _M0L8_2aentryS516->$2;
      moonbit_decref(_M0L8_2aentryS516);
      _M0L3pslS1303 = _M0L8_2afieldS1577;
      if (_M0L1iS511 > _M0L3pslS1303) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1304;
        moonbit_decref(_M0L4selfS514);
        _M0L6_2atmpS1304 = 0;
        return _M0L6_2atmpS1304;
      }
      _M0L6_2atmpS1305 = _M0L1iS511 + 1;
      _M0L6_2atmpS1307 = _M0L3idxS512 + 1;
      _M0L14capacity__maskS1308 = _M0L4selfS514->$3;
      _M0L6_2atmpS1306 = _M0L6_2atmpS1307 & _M0L14capacity__maskS1308;
      _M0L1iS511 = _M0L6_2atmpS1305;
      _M0L3idxS512 = _M0L6_2atmpS1306;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS523,
  moonbit_string_t _M0L3keyS519
) {
  int32_t _M0L4hashS518;
  int32_t _M0L14capacity__maskS1325;
  int32_t _M0L6_2atmpS1324;
  int32_t _M0L1iS520;
  int32_t _M0L3idxS521;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS519);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS518 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS519);
  _M0L14capacity__maskS1325 = _M0L4selfS523->$3;
  _M0L6_2atmpS1324 = _M0L4hashS518 & _M0L14capacity__maskS1325;
  _M0L1iS520 = 0;
  _M0L3idxS521 = _M0L6_2atmpS1324;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1586 =
      _M0L4selfS523->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1323 =
      _M0L8_2afieldS1586;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1585;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS522;
    if (
      _M0L3idxS521 < 0
      || _M0L3idxS521 >= Moonbit_array_length(_M0L7entriesS1323)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1585
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1323[
        _M0L3idxS521
      ];
    _M0L7_2abindS522 = _M0L6_2atmpS1585;
    if (_M0L7_2abindS522 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1312;
      if (_M0L7_2abindS522) {
        moonbit_incref(_M0L7_2abindS522);
      }
      moonbit_decref(_M0L4selfS523);
      if (_M0L7_2abindS522) {
        moonbit_decref(_M0L7_2abindS522);
      }
      moonbit_decref(_M0L3keyS519);
      _M0L6_2atmpS1312 = 0;
      return _M0L6_2atmpS1312;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS524 =
        _M0L7_2abindS522;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS525 =
        _M0L7_2aSomeS524;
      int32_t _M0L4hashS1314 = _M0L8_2aentryS525->$3;
      int32_t _if__result_1801;
      int32_t _M0L8_2afieldS1581;
      int32_t _M0L3pslS1317;
      int32_t _M0L6_2atmpS1319;
      int32_t _M0L6_2atmpS1321;
      int32_t _M0L14capacity__maskS1322;
      int32_t _M0L6_2atmpS1320;
      if (_M0L4hashS1314 == _M0L4hashS518) {
        moonbit_string_t _M0L8_2afieldS1584 = _M0L8_2aentryS525->$4;
        moonbit_string_t _M0L3keyS1313 = _M0L8_2afieldS1584;
        int32_t _M0L6_2atmpS1583;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS1583
        = moonbit_val_array_equal(_M0L3keyS1313, _M0L3keyS519);
        _if__result_1801 = _M0L6_2atmpS1583;
      } else {
        _if__result_1801 = 0;
      }
      if (_if__result_1801) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1582;
        int32_t _M0L6_2acntS1729;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1316;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1315;
        moonbit_incref(_M0L8_2aentryS525);
        moonbit_decref(_M0L4selfS523);
        moonbit_decref(_M0L3keyS519);
        _M0L8_2afieldS1582 = _M0L8_2aentryS525->$5;
        _M0L6_2acntS1729 = Moonbit_object_header(_M0L8_2aentryS525)->rc;
        if (_M0L6_2acntS1729 > 1) {
          int32_t _M0L11_2anew__cntS1732 = _M0L6_2acntS1729 - 1;
          Moonbit_object_header(_M0L8_2aentryS525)->rc
          = _M0L11_2anew__cntS1732;
          moonbit_incref(_M0L8_2afieldS1582);
        } else if (_M0L6_2acntS1729 == 1) {
          moonbit_string_t _M0L8_2afieldS1731 = _M0L8_2aentryS525->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1730;
          moonbit_decref(_M0L8_2afieldS1731);
          _M0L8_2afieldS1730 = _M0L8_2aentryS525->$1;
          if (_M0L8_2afieldS1730) {
            moonbit_decref(_M0L8_2afieldS1730);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS525);
        }
        _M0L5valueS1316 = _M0L8_2afieldS1582;
        _M0L6_2atmpS1315 = _M0L5valueS1316;
        return _M0L6_2atmpS1315;
      } else {
        moonbit_incref(_M0L8_2aentryS525);
      }
      _M0L8_2afieldS1581 = _M0L8_2aentryS525->$2;
      moonbit_decref(_M0L8_2aentryS525);
      _M0L3pslS1317 = _M0L8_2afieldS1581;
      if (_M0L1iS520 > _M0L3pslS1317) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1318;
        moonbit_decref(_M0L4selfS523);
        moonbit_decref(_M0L3keyS519);
        _M0L6_2atmpS1318 = 0;
        return _M0L6_2atmpS1318;
      }
      _M0L6_2atmpS1319 = _M0L1iS520 + 1;
      _M0L6_2atmpS1321 = _M0L3idxS521 + 1;
      _M0L14capacity__maskS1322 = _M0L4selfS523->$3;
      _M0L6_2atmpS1320 = _M0L6_2atmpS1321 & _M0L14capacity__maskS1322;
      _M0L1iS520 = _M0L6_2atmpS1319;
      _M0L3idxS521 = _M0L6_2atmpS1320;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS494
) {
  int32_t _M0L6lengthS493;
  int32_t _M0Lm8capacityS495;
  int32_t _M0L6_2atmpS1275;
  int32_t _M0L6_2atmpS1274;
  int32_t _M0L6_2atmpS1285;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS496;
  int32_t _M0L3endS1283;
  int32_t _M0L5startS1284;
  int32_t _M0L7_2abindS497;
  int32_t _M0L2__S498;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS494.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS493
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS494);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS495 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS493);
  _M0L6_2atmpS1275 = _M0Lm8capacityS495;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1274 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1275);
  if (_M0L6lengthS493 > _M0L6_2atmpS1274) {
    int32_t _M0L6_2atmpS1276 = _M0Lm8capacityS495;
    _M0Lm8capacityS495 = _M0L6_2atmpS1276 * 2;
  }
  _M0L6_2atmpS1285 = _M0Lm8capacityS495;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS496
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1285);
  _M0L3endS1283 = _M0L3arrS494.$2;
  _M0L5startS1284 = _M0L3arrS494.$1;
  _M0L7_2abindS497 = _M0L3endS1283 - _M0L5startS1284;
  _M0L2__S498 = 0;
  while (1) {
    if (_M0L2__S498 < _M0L7_2abindS497) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1590 =
        _M0L3arrS494.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1280 =
        _M0L8_2afieldS1590;
      int32_t _M0L5startS1282 = _M0L3arrS494.$1;
      int32_t _M0L6_2atmpS1281 = _M0L5startS1282 + _M0L2__S498;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1589 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1280[
          _M0L6_2atmpS1281
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS499 =
        _M0L6_2atmpS1589;
      moonbit_string_t _M0L8_2afieldS1588 = _M0L1eS499->$0;
      moonbit_string_t _M0L6_2atmpS1277 = _M0L8_2afieldS1588;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1587 =
        _M0L1eS499->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1278 =
        _M0L8_2afieldS1587;
      int32_t _M0L6_2atmpS1279;
      moonbit_incref(_M0L6_2atmpS1278);
      moonbit_incref(_M0L6_2atmpS1277);
      moonbit_incref(_M0L1mS496);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS496, _M0L6_2atmpS1277, _M0L6_2atmpS1278);
      _M0L6_2atmpS1279 = _M0L2__S498 + 1;
      _M0L2__S498 = _M0L6_2atmpS1279;
      continue;
    } else {
      moonbit_decref(_M0L3arrS494.$0);
    }
    break;
  }
  return _M0L1mS496;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS502
) {
  int32_t _M0L6lengthS501;
  int32_t _M0Lm8capacityS503;
  int32_t _M0L6_2atmpS1287;
  int32_t _M0L6_2atmpS1286;
  int32_t _M0L6_2atmpS1297;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS504;
  int32_t _M0L3endS1295;
  int32_t _M0L5startS1296;
  int32_t _M0L7_2abindS505;
  int32_t _M0L2__S506;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS502.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS501
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS502);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS503 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS501);
  _M0L6_2atmpS1287 = _M0Lm8capacityS503;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1286 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1287);
  if (_M0L6lengthS501 > _M0L6_2atmpS1286) {
    int32_t _M0L6_2atmpS1288 = _M0Lm8capacityS503;
    _M0Lm8capacityS503 = _M0L6_2atmpS1288 * 2;
  }
  _M0L6_2atmpS1297 = _M0Lm8capacityS503;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS504
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1297);
  _M0L3endS1295 = _M0L3arrS502.$2;
  _M0L5startS1296 = _M0L3arrS502.$1;
  _M0L7_2abindS505 = _M0L3endS1295 - _M0L5startS1296;
  _M0L2__S506 = 0;
  while (1) {
    if (_M0L2__S506 < _M0L7_2abindS505) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1593 =
        _M0L3arrS502.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1292 =
        _M0L8_2afieldS1593;
      int32_t _M0L5startS1294 = _M0L3arrS502.$1;
      int32_t _M0L6_2atmpS1293 = _M0L5startS1294 + _M0L2__S506;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1592 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1292[
          _M0L6_2atmpS1293
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS507 = _M0L6_2atmpS1592;
      int32_t _M0L6_2atmpS1289 = _M0L1eS507->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1591 =
        _M0L1eS507->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1290 =
        _M0L8_2afieldS1591;
      int32_t _M0L6_2atmpS1291;
      moonbit_incref(_M0L6_2atmpS1290);
      moonbit_incref(_M0L1mS504);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS504, _M0L6_2atmpS1289, _M0L6_2atmpS1290);
      _M0L6_2atmpS1291 = _M0L2__S506 + 1;
      _M0L2__S506 = _M0L6_2atmpS1291;
      continue;
    } else {
      moonbit_decref(_M0L3arrS502.$0);
    }
    break;
  }
  return _M0L1mS504;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS487,
  moonbit_string_t _M0L3keyS488,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS489
) {
  int32_t _M0L6_2atmpS1272;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS488);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1272 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS488);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS487, _M0L3keyS488, _M0L5valueS489, _M0L6_2atmpS1272);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS490,
  int32_t _M0L3keyS491,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS492
) {
  int32_t _M0L6_2atmpS1273;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1273 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS491);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS490, _M0L3keyS491, _M0L5valueS492, _M0L6_2atmpS1273);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS466
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1600;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS465;
  int32_t _M0L8capacityS1264;
  int32_t _M0L13new__capacityS467;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1259;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1258;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS1599;
  int32_t _M0L6_2atmpS1260;
  int32_t _M0L8capacityS1262;
  int32_t _M0L6_2atmpS1261;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1263;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1598;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS468;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1600 = _M0L4selfS466->$5;
  _M0L9old__headS465 = _M0L8_2afieldS1600;
  _M0L8capacityS1264 = _M0L4selfS466->$2;
  _M0L13new__capacityS467 = _M0L8capacityS1264 << 1;
  _M0L6_2atmpS1259 = 0;
  _M0L6_2atmpS1258
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS467, _M0L6_2atmpS1259);
  _M0L6_2aoldS1599 = _M0L4selfS466->$0;
  if (_M0L9old__headS465) {
    moonbit_incref(_M0L9old__headS465);
  }
  moonbit_decref(_M0L6_2aoldS1599);
  _M0L4selfS466->$0 = _M0L6_2atmpS1258;
  _M0L4selfS466->$2 = _M0L13new__capacityS467;
  _M0L6_2atmpS1260 = _M0L13new__capacityS467 - 1;
  _M0L4selfS466->$3 = _M0L6_2atmpS1260;
  _M0L8capacityS1262 = _M0L4selfS466->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1261 = _M0FPB21calc__grow__threshold(_M0L8capacityS1262);
  _M0L4selfS466->$4 = _M0L6_2atmpS1261;
  _M0L4selfS466->$1 = 0;
  _M0L6_2atmpS1263 = 0;
  _M0L6_2aoldS1598 = _M0L4selfS466->$5;
  if (_M0L6_2aoldS1598) {
    moonbit_decref(_M0L6_2aoldS1598);
  }
  _M0L4selfS466->$5 = _M0L6_2atmpS1263;
  _M0L4selfS466->$6 = -1;
  _M0L8_2aparamS468 = _M0L9old__headS465;
  while (1) {
    if (_M0L8_2aparamS468 == 0) {
      if (_M0L8_2aparamS468) {
        moonbit_decref(_M0L8_2aparamS468);
      }
      moonbit_decref(_M0L4selfS466);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS469 =
        _M0L8_2aparamS468;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS470 =
        _M0L7_2aSomeS469;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1597 =
        _M0L4_2axS470->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS471 =
        _M0L8_2afieldS1597;
      moonbit_string_t _M0L8_2afieldS1596 = _M0L4_2axS470->$4;
      moonbit_string_t _M0L6_2akeyS472 = _M0L8_2afieldS1596;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1595 =
        _M0L4_2axS470->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS473 =
        _M0L8_2afieldS1595;
      int32_t _M0L8_2afieldS1594 = _M0L4_2axS470->$3;
      int32_t _M0L6_2acntS1733 = Moonbit_object_header(_M0L4_2axS470)->rc;
      int32_t _M0L7_2ahashS474;
      if (_M0L6_2acntS1733 > 1) {
        int32_t _M0L11_2anew__cntS1734 = _M0L6_2acntS1733 - 1;
        Moonbit_object_header(_M0L4_2axS470)->rc = _M0L11_2anew__cntS1734;
        moonbit_incref(_M0L8_2avalueS473);
        moonbit_incref(_M0L6_2akeyS472);
        if (_M0L7_2anextS471) {
          moonbit_incref(_M0L7_2anextS471);
        }
      } else if (_M0L6_2acntS1733 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS470);
      }
      _M0L7_2ahashS474 = _M0L8_2afieldS1594;
      moonbit_incref(_M0L4selfS466);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS466, _M0L6_2akeyS472, _M0L8_2avalueS473, _M0L7_2ahashS474);
      _M0L8_2aparamS468 = _M0L7_2anextS471;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS477
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1606;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS476;
  int32_t _M0L8capacityS1271;
  int32_t _M0L13new__capacityS478;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1266;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1265;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS1605;
  int32_t _M0L6_2atmpS1267;
  int32_t _M0L8capacityS1269;
  int32_t _M0L6_2atmpS1268;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1270;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1604;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS479;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1606 = _M0L4selfS477->$5;
  _M0L9old__headS476 = _M0L8_2afieldS1606;
  _M0L8capacityS1271 = _M0L4selfS477->$2;
  _M0L13new__capacityS478 = _M0L8capacityS1271 << 1;
  _M0L6_2atmpS1266 = 0;
  _M0L6_2atmpS1265
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS478, _M0L6_2atmpS1266);
  _M0L6_2aoldS1605 = _M0L4selfS477->$0;
  if (_M0L9old__headS476) {
    moonbit_incref(_M0L9old__headS476);
  }
  moonbit_decref(_M0L6_2aoldS1605);
  _M0L4selfS477->$0 = _M0L6_2atmpS1265;
  _M0L4selfS477->$2 = _M0L13new__capacityS478;
  _M0L6_2atmpS1267 = _M0L13new__capacityS478 - 1;
  _M0L4selfS477->$3 = _M0L6_2atmpS1267;
  _M0L8capacityS1269 = _M0L4selfS477->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1268 = _M0FPB21calc__grow__threshold(_M0L8capacityS1269);
  _M0L4selfS477->$4 = _M0L6_2atmpS1268;
  _M0L4selfS477->$1 = 0;
  _M0L6_2atmpS1270 = 0;
  _M0L6_2aoldS1604 = _M0L4selfS477->$5;
  if (_M0L6_2aoldS1604) {
    moonbit_decref(_M0L6_2aoldS1604);
  }
  _M0L4selfS477->$5 = _M0L6_2atmpS1270;
  _M0L4selfS477->$6 = -1;
  _M0L8_2aparamS479 = _M0L9old__headS476;
  while (1) {
    if (_M0L8_2aparamS479 == 0) {
      if (_M0L8_2aparamS479) {
        moonbit_decref(_M0L8_2aparamS479);
      }
      moonbit_decref(_M0L4selfS477);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS480 =
        _M0L8_2aparamS479;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS481 =
        _M0L7_2aSomeS480;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1603 =
        _M0L4_2axS481->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS482 =
        _M0L8_2afieldS1603;
      int32_t _M0L6_2akeyS483 = _M0L4_2axS481->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1602 =
        _M0L4_2axS481->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS484 =
        _M0L8_2afieldS1602;
      int32_t _M0L8_2afieldS1601 = _M0L4_2axS481->$3;
      int32_t _M0L6_2acntS1735 = Moonbit_object_header(_M0L4_2axS481)->rc;
      int32_t _M0L7_2ahashS485;
      if (_M0L6_2acntS1735 > 1) {
        int32_t _M0L11_2anew__cntS1736 = _M0L6_2acntS1735 - 1;
        Moonbit_object_header(_M0L4_2axS481)->rc = _M0L11_2anew__cntS1736;
        moonbit_incref(_M0L8_2avalueS484);
        if (_M0L7_2anextS482) {
          moonbit_incref(_M0L7_2anextS482);
        }
      } else if (_M0L6_2acntS1735 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS481);
      }
      _M0L7_2ahashS485 = _M0L8_2afieldS1601;
      moonbit_incref(_M0L4selfS477);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS477, _M0L6_2akeyS483, _M0L8_2avalueS484, _M0L7_2ahashS485);
      _M0L8_2aparamS479 = _M0L7_2anextS482;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS436,
  moonbit_string_t _M0L3keyS442,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS443,
  int32_t _M0L4hashS438
) {
  int32_t _M0L14capacity__maskS1239;
  int32_t _M0L6_2atmpS1238;
  int32_t _M0L3pslS433;
  int32_t _M0L3idxS434;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1239 = _M0L4selfS436->$3;
  _M0L6_2atmpS1238 = _M0L4hashS438 & _M0L14capacity__maskS1239;
  _M0L3pslS433 = 0;
  _M0L3idxS434 = _M0L6_2atmpS1238;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1611 =
      _M0L4selfS436->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1237 =
      _M0L8_2afieldS1611;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1610;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS435;
    if (
      _M0L3idxS434 < 0
      || _M0L3idxS434 >= Moonbit_array_length(_M0L7entriesS1237)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1610
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1237[
        _M0L3idxS434
      ];
    _M0L7_2abindS435 = _M0L6_2atmpS1610;
    if (_M0L7_2abindS435 == 0) {
      int32_t _M0L4sizeS1222 = _M0L4selfS436->$1;
      int32_t _M0L8grow__atS1223 = _M0L4selfS436->$4;
      int32_t _M0L7_2abindS439;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS440;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS441;
      if (_M0L4sizeS1222 >= _M0L8grow__atS1223) {
        int32_t _M0L14capacity__maskS1225;
        int32_t _M0L6_2atmpS1224;
        moonbit_incref(_M0L4selfS436);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS436);
        _M0L14capacity__maskS1225 = _M0L4selfS436->$3;
        _M0L6_2atmpS1224 = _M0L4hashS438 & _M0L14capacity__maskS1225;
        _M0L3pslS433 = 0;
        _M0L3idxS434 = _M0L6_2atmpS1224;
        continue;
      }
      _M0L7_2abindS439 = _M0L4selfS436->$6;
      _M0L7_2abindS440 = 0;
      _M0L5entryS441
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS441)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS441->$0 = _M0L7_2abindS439;
      _M0L5entryS441->$1 = _M0L7_2abindS440;
      _M0L5entryS441->$2 = _M0L3pslS433;
      _M0L5entryS441->$3 = _M0L4hashS438;
      _M0L5entryS441->$4 = _M0L3keyS442;
      _M0L5entryS441->$5 = _M0L5valueS443;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS436, _M0L3idxS434, _M0L5entryS441);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS444 =
        _M0L7_2abindS435;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS445 =
        _M0L7_2aSomeS444;
      int32_t _M0L4hashS1227 = _M0L14_2acurr__entryS445->$3;
      int32_t _if__result_1807;
      int32_t _M0L3pslS1228;
      int32_t _M0L6_2atmpS1233;
      int32_t _M0L6_2atmpS1235;
      int32_t _M0L14capacity__maskS1236;
      int32_t _M0L6_2atmpS1234;
      if (_M0L4hashS1227 == _M0L4hashS438) {
        moonbit_string_t _M0L8_2afieldS1609 = _M0L14_2acurr__entryS445->$4;
        moonbit_string_t _M0L3keyS1226 = _M0L8_2afieldS1609;
        int32_t _M0L6_2atmpS1608;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS1608
        = moonbit_val_array_equal(_M0L3keyS1226, _M0L3keyS442);
        _if__result_1807 = _M0L6_2atmpS1608;
      } else {
        _if__result_1807 = 0;
      }
      if (_if__result_1807) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1607;
        moonbit_incref(_M0L14_2acurr__entryS445);
        moonbit_decref(_M0L3keyS442);
        moonbit_decref(_M0L4selfS436);
        _M0L6_2aoldS1607 = _M0L14_2acurr__entryS445->$5;
        moonbit_decref(_M0L6_2aoldS1607);
        _M0L14_2acurr__entryS445->$5 = _M0L5valueS443;
        moonbit_decref(_M0L14_2acurr__entryS445);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS445);
      }
      _M0L3pslS1228 = _M0L14_2acurr__entryS445->$2;
      if (_M0L3pslS433 > _M0L3pslS1228) {
        int32_t _M0L4sizeS1229 = _M0L4selfS436->$1;
        int32_t _M0L8grow__atS1230 = _M0L4selfS436->$4;
        int32_t _M0L7_2abindS446;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS447;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS448;
        if (_M0L4sizeS1229 >= _M0L8grow__atS1230) {
          int32_t _M0L14capacity__maskS1232;
          int32_t _M0L6_2atmpS1231;
          moonbit_decref(_M0L14_2acurr__entryS445);
          moonbit_incref(_M0L4selfS436);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS436);
          _M0L14capacity__maskS1232 = _M0L4selfS436->$3;
          _M0L6_2atmpS1231 = _M0L4hashS438 & _M0L14capacity__maskS1232;
          _M0L3pslS433 = 0;
          _M0L3idxS434 = _M0L6_2atmpS1231;
          continue;
        }
        moonbit_incref(_M0L4selfS436);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS436, _M0L3idxS434, _M0L14_2acurr__entryS445);
        _M0L7_2abindS446 = _M0L4selfS436->$6;
        _M0L7_2abindS447 = 0;
        _M0L5entryS448
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS448)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS448->$0 = _M0L7_2abindS446;
        _M0L5entryS448->$1 = _M0L7_2abindS447;
        _M0L5entryS448->$2 = _M0L3pslS433;
        _M0L5entryS448->$3 = _M0L4hashS438;
        _M0L5entryS448->$4 = _M0L3keyS442;
        _M0L5entryS448->$5 = _M0L5valueS443;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS436, _M0L3idxS434, _M0L5entryS448);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS445);
      }
      _M0L6_2atmpS1233 = _M0L3pslS433 + 1;
      _M0L6_2atmpS1235 = _M0L3idxS434 + 1;
      _M0L14capacity__maskS1236 = _M0L4selfS436->$3;
      _M0L6_2atmpS1234 = _M0L6_2atmpS1235 & _M0L14capacity__maskS1236;
      _M0L3pslS433 = _M0L6_2atmpS1233;
      _M0L3idxS434 = _M0L6_2atmpS1234;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS452,
  int32_t _M0L3keyS458,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS459,
  int32_t _M0L4hashS454
) {
  int32_t _M0L14capacity__maskS1257;
  int32_t _M0L6_2atmpS1256;
  int32_t _M0L3pslS449;
  int32_t _M0L3idxS450;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1257 = _M0L4selfS452->$3;
  _M0L6_2atmpS1256 = _M0L4hashS454 & _M0L14capacity__maskS1257;
  _M0L3pslS449 = 0;
  _M0L3idxS450 = _M0L6_2atmpS1256;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1614 =
      _M0L4selfS452->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1255 =
      _M0L8_2afieldS1614;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1613;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS451;
    if (
      _M0L3idxS450 < 0
      || _M0L3idxS450 >= Moonbit_array_length(_M0L7entriesS1255)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1613
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1255[
        _M0L3idxS450
      ];
    _M0L7_2abindS451 = _M0L6_2atmpS1613;
    if (_M0L7_2abindS451 == 0) {
      int32_t _M0L4sizeS1240 = _M0L4selfS452->$1;
      int32_t _M0L8grow__atS1241 = _M0L4selfS452->$4;
      int32_t _M0L7_2abindS455;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS456;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS457;
      if (_M0L4sizeS1240 >= _M0L8grow__atS1241) {
        int32_t _M0L14capacity__maskS1243;
        int32_t _M0L6_2atmpS1242;
        moonbit_incref(_M0L4selfS452);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS452);
        _M0L14capacity__maskS1243 = _M0L4selfS452->$3;
        _M0L6_2atmpS1242 = _M0L4hashS454 & _M0L14capacity__maskS1243;
        _M0L3pslS449 = 0;
        _M0L3idxS450 = _M0L6_2atmpS1242;
        continue;
      }
      _M0L7_2abindS455 = _M0L4selfS452->$6;
      _M0L7_2abindS456 = 0;
      _M0L5entryS457
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS457)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS457->$0 = _M0L7_2abindS455;
      _M0L5entryS457->$1 = _M0L7_2abindS456;
      _M0L5entryS457->$2 = _M0L3pslS449;
      _M0L5entryS457->$3 = _M0L4hashS454;
      _M0L5entryS457->$4 = _M0L3keyS458;
      _M0L5entryS457->$5 = _M0L5valueS459;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS452, _M0L3idxS450, _M0L5entryS457);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS460 =
        _M0L7_2abindS451;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS461 =
        _M0L7_2aSomeS460;
      int32_t _M0L4hashS1245 = _M0L14_2acurr__entryS461->$3;
      int32_t _if__result_1809;
      int32_t _M0L3pslS1246;
      int32_t _M0L6_2atmpS1251;
      int32_t _M0L6_2atmpS1253;
      int32_t _M0L14capacity__maskS1254;
      int32_t _M0L6_2atmpS1252;
      if (_M0L4hashS1245 == _M0L4hashS454) {
        int32_t _M0L3keyS1244 = _M0L14_2acurr__entryS461->$4;
        _if__result_1809 = _M0L3keyS1244 == _M0L3keyS458;
      } else {
        _if__result_1809 = 0;
      }
      if (_if__result_1809) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS1612;
        moonbit_incref(_M0L14_2acurr__entryS461);
        moonbit_decref(_M0L4selfS452);
        _M0L6_2aoldS1612 = _M0L14_2acurr__entryS461->$5;
        moonbit_decref(_M0L6_2aoldS1612);
        _M0L14_2acurr__entryS461->$5 = _M0L5valueS459;
        moonbit_decref(_M0L14_2acurr__entryS461);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS461);
      }
      _M0L3pslS1246 = _M0L14_2acurr__entryS461->$2;
      if (_M0L3pslS449 > _M0L3pslS1246) {
        int32_t _M0L4sizeS1247 = _M0L4selfS452->$1;
        int32_t _M0L8grow__atS1248 = _M0L4selfS452->$4;
        int32_t _M0L7_2abindS462;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS463;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS464;
        if (_M0L4sizeS1247 >= _M0L8grow__atS1248) {
          int32_t _M0L14capacity__maskS1250;
          int32_t _M0L6_2atmpS1249;
          moonbit_decref(_M0L14_2acurr__entryS461);
          moonbit_incref(_M0L4selfS452);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS452);
          _M0L14capacity__maskS1250 = _M0L4selfS452->$3;
          _M0L6_2atmpS1249 = _M0L4hashS454 & _M0L14capacity__maskS1250;
          _M0L3pslS449 = 0;
          _M0L3idxS450 = _M0L6_2atmpS1249;
          continue;
        }
        moonbit_incref(_M0L4selfS452);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS452, _M0L3idxS450, _M0L14_2acurr__entryS461);
        _M0L7_2abindS462 = _M0L4selfS452->$6;
        _M0L7_2abindS463 = 0;
        _M0L5entryS464
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS464)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS464->$0 = _M0L7_2abindS462;
        _M0L5entryS464->$1 = _M0L7_2abindS463;
        _M0L5entryS464->$2 = _M0L3pslS449;
        _M0L5entryS464->$3 = _M0L4hashS454;
        _M0L5entryS464->$4 = _M0L3keyS458;
        _M0L5entryS464->$5 = _M0L5valueS459;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS452, _M0L3idxS450, _M0L5entryS464);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS461);
      }
      _M0L6_2atmpS1251 = _M0L3pslS449 + 1;
      _M0L6_2atmpS1253 = _M0L3idxS450 + 1;
      _M0L14capacity__maskS1254 = _M0L4selfS452->$3;
      _M0L6_2atmpS1252 = _M0L6_2atmpS1253 & _M0L14capacity__maskS1254;
      _M0L3pslS449 = _M0L6_2atmpS1251;
      _M0L3idxS450 = _M0L6_2atmpS1252;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS417,
  int32_t _M0L3idxS422,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS421
) {
  int32_t _M0L3pslS1205;
  int32_t _M0L6_2atmpS1201;
  int32_t _M0L6_2atmpS1203;
  int32_t _M0L14capacity__maskS1204;
  int32_t _M0L6_2atmpS1202;
  int32_t _M0L3pslS413;
  int32_t _M0L3idxS414;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS415;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1205 = _M0L5entryS421->$2;
  _M0L6_2atmpS1201 = _M0L3pslS1205 + 1;
  _M0L6_2atmpS1203 = _M0L3idxS422 + 1;
  _M0L14capacity__maskS1204 = _M0L4selfS417->$3;
  _M0L6_2atmpS1202 = _M0L6_2atmpS1203 & _M0L14capacity__maskS1204;
  _M0L3pslS413 = _M0L6_2atmpS1201;
  _M0L3idxS414 = _M0L6_2atmpS1202;
  _M0L5entryS415 = _M0L5entryS421;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1616 =
      _M0L4selfS417->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1200 =
      _M0L8_2afieldS1616;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1615;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS416;
    if (
      _M0L3idxS414 < 0
      || _M0L3idxS414 >= Moonbit_array_length(_M0L7entriesS1200)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1615
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1200[
        _M0L3idxS414
      ];
    _M0L7_2abindS416 = _M0L6_2atmpS1615;
    if (_M0L7_2abindS416 == 0) {
      _M0L5entryS415->$2 = _M0L3pslS413;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS417, _M0L5entryS415, _M0L3idxS414);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS419 =
        _M0L7_2abindS416;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS420 =
        _M0L7_2aSomeS419;
      int32_t _M0L3pslS1190 = _M0L14_2acurr__entryS420->$2;
      if (_M0L3pslS413 > _M0L3pslS1190) {
        int32_t _M0L3pslS1195;
        int32_t _M0L6_2atmpS1191;
        int32_t _M0L6_2atmpS1193;
        int32_t _M0L14capacity__maskS1194;
        int32_t _M0L6_2atmpS1192;
        _M0L5entryS415->$2 = _M0L3pslS413;
        moonbit_incref(_M0L14_2acurr__entryS420);
        moonbit_incref(_M0L4selfS417);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS417, _M0L5entryS415, _M0L3idxS414);
        _M0L3pslS1195 = _M0L14_2acurr__entryS420->$2;
        _M0L6_2atmpS1191 = _M0L3pslS1195 + 1;
        _M0L6_2atmpS1193 = _M0L3idxS414 + 1;
        _M0L14capacity__maskS1194 = _M0L4selfS417->$3;
        _M0L6_2atmpS1192 = _M0L6_2atmpS1193 & _M0L14capacity__maskS1194;
        _M0L3pslS413 = _M0L6_2atmpS1191;
        _M0L3idxS414 = _M0L6_2atmpS1192;
        _M0L5entryS415 = _M0L14_2acurr__entryS420;
        continue;
      } else {
        int32_t _M0L6_2atmpS1196 = _M0L3pslS413 + 1;
        int32_t _M0L6_2atmpS1198 = _M0L3idxS414 + 1;
        int32_t _M0L14capacity__maskS1199 = _M0L4selfS417->$3;
        int32_t _M0L6_2atmpS1197 =
          _M0L6_2atmpS1198 & _M0L14capacity__maskS1199;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_1811 =
          _M0L5entryS415;
        _M0L3pslS413 = _M0L6_2atmpS1196;
        _M0L3idxS414 = _M0L6_2atmpS1197;
        _M0L5entryS415 = _tmp_1811;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS427,
  int32_t _M0L3idxS432,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS431
) {
  int32_t _M0L3pslS1221;
  int32_t _M0L6_2atmpS1217;
  int32_t _M0L6_2atmpS1219;
  int32_t _M0L14capacity__maskS1220;
  int32_t _M0L6_2atmpS1218;
  int32_t _M0L3pslS423;
  int32_t _M0L3idxS424;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS425;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1221 = _M0L5entryS431->$2;
  _M0L6_2atmpS1217 = _M0L3pslS1221 + 1;
  _M0L6_2atmpS1219 = _M0L3idxS432 + 1;
  _M0L14capacity__maskS1220 = _M0L4selfS427->$3;
  _M0L6_2atmpS1218 = _M0L6_2atmpS1219 & _M0L14capacity__maskS1220;
  _M0L3pslS423 = _M0L6_2atmpS1217;
  _M0L3idxS424 = _M0L6_2atmpS1218;
  _M0L5entryS425 = _M0L5entryS431;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1618 =
      _M0L4selfS427->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1216 =
      _M0L8_2afieldS1618;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1617;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS426;
    if (
      _M0L3idxS424 < 0
      || _M0L3idxS424 >= Moonbit_array_length(_M0L7entriesS1216)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1617
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1216[
        _M0L3idxS424
      ];
    _M0L7_2abindS426 = _M0L6_2atmpS1617;
    if (_M0L7_2abindS426 == 0) {
      _M0L5entryS425->$2 = _M0L3pslS423;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS427, _M0L5entryS425, _M0L3idxS424);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS429 =
        _M0L7_2abindS426;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS430 =
        _M0L7_2aSomeS429;
      int32_t _M0L3pslS1206 = _M0L14_2acurr__entryS430->$2;
      if (_M0L3pslS423 > _M0L3pslS1206) {
        int32_t _M0L3pslS1211;
        int32_t _M0L6_2atmpS1207;
        int32_t _M0L6_2atmpS1209;
        int32_t _M0L14capacity__maskS1210;
        int32_t _M0L6_2atmpS1208;
        _M0L5entryS425->$2 = _M0L3pslS423;
        moonbit_incref(_M0L14_2acurr__entryS430);
        moonbit_incref(_M0L4selfS427);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS427, _M0L5entryS425, _M0L3idxS424);
        _M0L3pslS1211 = _M0L14_2acurr__entryS430->$2;
        _M0L6_2atmpS1207 = _M0L3pslS1211 + 1;
        _M0L6_2atmpS1209 = _M0L3idxS424 + 1;
        _M0L14capacity__maskS1210 = _M0L4selfS427->$3;
        _M0L6_2atmpS1208 = _M0L6_2atmpS1209 & _M0L14capacity__maskS1210;
        _M0L3pslS423 = _M0L6_2atmpS1207;
        _M0L3idxS424 = _M0L6_2atmpS1208;
        _M0L5entryS425 = _M0L14_2acurr__entryS430;
        continue;
      } else {
        int32_t _M0L6_2atmpS1212 = _M0L3pslS423 + 1;
        int32_t _M0L6_2atmpS1214 = _M0L3idxS424 + 1;
        int32_t _M0L14capacity__maskS1215 = _M0L4selfS427->$3;
        int32_t _M0L6_2atmpS1213 =
          _M0L6_2atmpS1214 & _M0L14capacity__maskS1215;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_1813 =
          _M0L5entryS425;
        _M0L3pslS423 = _M0L6_2atmpS1212;
        _M0L3idxS424 = _M0L6_2atmpS1213;
        _M0L5entryS425 = _tmp_1813;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS401,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS403,
  int32_t _M0L8new__idxS402
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1621;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1186;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1187;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1620;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1619;
  int32_t _M0L6_2acntS1737;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS404;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1621 = _M0L4selfS401->$0;
  _M0L7entriesS1186 = _M0L8_2afieldS1621;
  moonbit_incref(_M0L5entryS403);
  _M0L6_2atmpS1187 = _M0L5entryS403;
  if (
    _M0L8new__idxS402 < 0
    || _M0L8new__idxS402 >= Moonbit_array_length(_M0L7entriesS1186)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1620
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1186[
      _M0L8new__idxS402
    ];
  if (_M0L6_2aoldS1620) {
    moonbit_decref(_M0L6_2aoldS1620);
  }
  _M0L7entriesS1186[_M0L8new__idxS402] = _M0L6_2atmpS1187;
  _M0L8_2afieldS1619 = _M0L5entryS403->$1;
  _M0L6_2acntS1737 = Moonbit_object_header(_M0L5entryS403)->rc;
  if (_M0L6_2acntS1737 > 1) {
    int32_t _M0L11_2anew__cntS1740 = _M0L6_2acntS1737 - 1;
    Moonbit_object_header(_M0L5entryS403)->rc = _M0L11_2anew__cntS1740;
    if (_M0L8_2afieldS1619) {
      moonbit_incref(_M0L8_2afieldS1619);
    }
  } else if (_M0L6_2acntS1737 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1739 =
      _M0L5entryS403->$5;
    moonbit_string_t _M0L8_2afieldS1738;
    moonbit_decref(_M0L8_2afieldS1739);
    _M0L8_2afieldS1738 = _M0L5entryS403->$4;
    moonbit_decref(_M0L8_2afieldS1738);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS403);
  }
  _M0L7_2abindS404 = _M0L8_2afieldS1619;
  if (_M0L7_2abindS404 == 0) {
    if (_M0L7_2abindS404) {
      moonbit_decref(_M0L7_2abindS404);
    }
    _M0L4selfS401->$6 = _M0L8new__idxS402;
    moonbit_decref(_M0L4selfS401);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS405;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS406;
    moonbit_decref(_M0L4selfS401);
    _M0L7_2aSomeS405 = _M0L7_2abindS404;
    _M0L7_2anextS406 = _M0L7_2aSomeS405;
    _M0L7_2anextS406->$0 = _M0L8new__idxS402;
    moonbit_decref(_M0L7_2anextS406);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS407,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS409,
  int32_t _M0L8new__idxS408
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1624;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1188;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1189;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1623;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1622;
  int32_t _M0L6_2acntS1741;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS410;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1624 = _M0L4selfS407->$0;
  _M0L7entriesS1188 = _M0L8_2afieldS1624;
  moonbit_incref(_M0L5entryS409);
  _M0L6_2atmpS1189 = _M0L5entryS409;
  if (
    _M0L8new__idxS408 < 0
    || _M0L8new__idxS408 >= Moonbit_array_length(_M0L7entriesS1188)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1623
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1188[
      _M0L8new__idxS408
    ];
  if (_M0L6_2aoldS1623) {
    moonbit_decref(_M0L6_2aoldS1623);
  }
  _M0L7entriesS1188[_M0L8new__idxS408] = _M0L6_2atmpS1189;
  _M0L8_2afieldS1622 = _M0L5entryS409->$1;
  _M0L6_2acntS1741 = Moonbit_object_header(_M0L5entryS409)->rc;
  if (_M0L6_2acntS1741 > 1) {
    int32_t _M0L11_2anew__cntS1743 = _M0L6_2acntS1741 - 1;
    Moonbit_object_header(_M0L5entryS409)->rc = _M0L11_2anew__cntS1743;
    if (_M0L8_2afieldS1622) {
      moonbit_incref(_M0L8_2afieldS1622);
    }
  } else if (_M0L6_2acntS1741 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1742 =
      _M0L5entryS409->$5;
    moonbit_decref(_M0L8_2afieldS1742);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS409);
  }
  _M0L7_2abindS410 = _M0L8_2afieldS1622;
  if (_M0L7_2abindS410 == 0) {
    if (_M0L7_2abindS410) {
      moonbit_decref(_M0L7_2abindS410);
    }
    _M0L4selfS407->$6 = _M0L8new__idxS408;
    moonbit_decref(_M0L4selfS407);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS411;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS412;
    moonbit_decref(_M0L4selfS407);
    _M0L7_2aSomeS411 = _M0L7_2abindS410;
    _M0L7_2anextS412 = _M0L7_2aSomeS411;
    _M0L7_2anextS412->$0 = _M0L8new__idxS408;
    moonbit_decref(_M0L7_2anextS412);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS394,
  int32_t _M0L3idxS396,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS395
) {
  int32_t _M0L7_2abindS393;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1626;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1173;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1174;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1625;
  int32_t _M0L4sizeS1176;
  int32_t _M0L6_2atmpS1175;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS393 = _M0L4selfS394->$6;
  switch (_M0L7_2abindS393) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1168;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1627;
      moonbit_incref(_M0L5entryS395);
      _M0L6_2atmpS1168 = _M0L5entryS395;
      _M0L6_2aoldS1627 = _M0L4selfS394->$5;
      if (_M0L6_2aoldS1627) {
        moonbit_decref(_M0L6_2aoldS1627);
      }
      _M0L4selfS394->$5 = _M0L6_2atmpS1168;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1630 =
        _M0L4selfS394->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1172 =
        _M0L8_2afieldS1630;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1629;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1171;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1169;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1170;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1628;
      if (
        _M0L7_2abindS393 < 0
        || _M0L7_2abindS393 >= Moonbit_array_length(_M0L7entriesS1172)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1629
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1172[
          _M0L7_2abindS393
        ];
      _M0L6_2atmpS1171 = _M0L6_2atmpS1629;
      if (_M0L6_2atmpS1171) {
        moonbit_incref(_M0L6_2atmpS1171);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1169
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1171);
      moonbit_incref(_M0L5entryS395);
      _M0L6_2atmpS1170 = _M0L5entryS395;
      _M0L6_2aoldS1628 = _M0L6_2atmpS1169->$1;
      if (_M0L6_2aoldS1628) {
        moonbit_decref(_M0L6_2aoldS1628);
      }
      _M0L6_2atmpS1169->$1 = _M0L6_2atmpS1170;
      moonbit_decref(_M0L6_2atmpS1169);
      break;
    }
  }
  _M0L4selfS394->$6 = _M0L3idxS396;
  _M0L8_2afieldS1626 = _M0L4selfS394->$0;
  _M0L7entriesS1173 = _M0L8_2afieldS1626;
  _M0L6_2atmpS1174 = _M0L5entryS395;
  if (
    _M0L3idxS396 < 0
    || _M0L3idxS396 >= Moonbit_array_length(_M0L7entriesS1173)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1625
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1173[
      _M0L3idxS396
    ];
  if (_M0L6_2aoldS1625) {
    moonbit_decref(_M0L6_2aoldS1625);
  }
  _M0L7entriesS1173[_M0L3idxS396] = _M0L6_2atmpS1174;
  _M0L4sizeS1176 = _M0L4selfS394->$1;
  _M0L6_2atmpS1175 = _M0L4sizeS1176 + 1;
  _M0L4selfS394->$1 = _M0L6_2atmpS1175;
  moonbit_decref(_M0L4selfS394);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS398,
  int32_t _M0L3idxS400,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS399
) {
  int32_t _M0L7_2abindS397;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1632;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1182;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1183;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1631;
  int32_t _M0L4sizeS1185;
  int32_t _M0L6_2atmpS1184;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS397 = _M0L4selfS398->$6;
  switch (_M0L7_2abindS397) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1177;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1633;
      moonbit_incref(_M0L5entryS399);
      _M0L6_2atmpS1177 = _M0L5entryS399;
      _M0L6_2aoldS1633 = _M0L4selfS398->$5;
      if (_M0L6_2aoldS1633) {
        moonbit_decref(_M0L6_2aoldS1633);
      }
      _M0L4selfS398->$5 = _M0L6_2atmpS1177;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1636 =
        _M0L4selfS398->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1181 =
        _M0L8_2afieldS1636;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1635;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1180;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1178;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1179;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1634;
      if (
        _M0L7_2abindS397 < 0
        || _M0L7_2abindS397 >= Moonbit_array_length(_M0L7entriesS1181)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1635
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1181[
          _M0L7_2abindS397
        ];
      _M0L6_2atmpS1180 = _M0L6_2atmpS1635;
      if (_M0L6_2atmpS1180) {
        moonbit_incref(_M0L6_2atmpS1180);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1178
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1180);
      moonbit_incref(_M0L5entryS399);
      _M0L6_2atmpS1179 = _M0L5entryS399;
      _M0L6_2aoldS1634 = _M0L6_2atmpS1178->$1;
      if (_M0L6_2aoldS1634) {
        moonbit_decref(_M0L6_2aoldS1634);
      }
      _M0L6_2atmpS1178->$1 = _M0L6_2atmpS1179;
      moonbit_decref(_M0L6_2atmpS1178);
      break;
    }
  }
  _M0L4selfS398->$6 = _M0L3idxS400;
  _M0L8_2afieldS1632 = _M0L4selfS398->$0;
  _M0L7entriesS1182 = _M0L8_2afieldS1632;
  _M0L6_2atmpS1183 = _M0L5entryS399;
  if (
    _M0L3idxS400 < 0
    || _M0L3idxS400 >= Moonbit_array_length(_M0L7entriesS1182)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1631
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1182[
      _M0L3idxS400
    ];
  if (_M0L6_2aoldS1631) {
    moonbit_decref(_M0L6_2aoldS1631);
  }
  _M0L7entriesS1182[_M0L3idxS400] = _M0L6_2atmpS1183;
  _M0L4sizeS1185 = _M0L4selfS398->$1;
  _M0L6_2atmpS1184 = _M0L4sizeS1185 + 1;
  _M0L4selfS398->$1 = _M0L6_2atmpS1184;
  moonbit_decref(_M0L4selfS398);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS382
) {
  int32_t _M0L8capacityS381;
  int32_t _M0L7_2abindS383;
  int32_t _M0L7_2abindS384;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1166;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS385;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS386;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_1814;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS381
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS382);
  _M0L7_2abindS383 = _M0L8capacityS381 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS384 = _M0FPB21calc__grow__threshold(_M0L8capacityS381);
  _M0L6_2atmpS1166 = 0;
  _M0L7_2abindS385
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS381, _M0L6_2atmpS1166);
  _M0L7_2abindS386 = 0;
  _block_1814
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_1814)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_1814->$0 = _M0L7_2abindS385;
  _block_1814->$1 = 0;
  _block_1814->$2 = _M0L8capacityS381;
  _block_1814->$3 = _M0L7_2abindS383;
  _block_1814->$4 = _M0L7_2abindS384;
  _block_1814->$5 = _M0L7_2abindS386;
  _block_1814->$6 = -1;
  return _block_1814;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS388
) {
  int32_t _M0L8capacityS387;
  int32_t _M0L7_2abindS389;
  int32_t _M0L7_2abindS390;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1167;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS391;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS392;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_1815;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS387
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS388);
  _M0L7_2abindS389 = _M0L8capacityS387 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS390 = _M0FPB21calc__grow__threshold(_M0L8capacityS387);
  _M0L6_2atmpS1167 = 0;
  _M0L7_2abindS391
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS387, _M0L6_2atmpS1167);
  _M0L7_2abindS392 = 0;
  _block_1815
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_1815)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_1815->$0 = _M0L7_2abindS391;
  _block_1815->$1 = 0;
  _block_1815->$2 = _M0L8capacityS387;
  _block_1815->$3 = _M0L7_2abindS389;
  _block_1815->$4 = _M0L7_2abindS390;
  _block_1815->$5 = _M0L7_2abindS392;
  _block_1815->$6 = -1;
  return _block_1815;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS380) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS380 >= 0) {
    int32_t _M0L6_2atmpS1165;
    int32_t _M0L6_2atmpS1164;
    int32_t _M0L6_2atmpS1163;
    int32_t _M0L6_2atmpS1162;
    if (_M0L4selfS380 <= 1) {
      return 1;
    }
    if (_M0L4selfS380 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1165 = _M0L4selfS380 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1164 = moonbit_clz32(_M0L6_2atmpS1165);
    _M0L6_2atmpS1163 = _M0L6_2atmpS1164 - 1;
    _M0L6_2atmpS1162 = 2147483647 >> (_M0L6_2atmpS1163 & 31);
    return _M0L6_2atmpS1162 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS379) {
  int32_t _M0L6_2atmpS1161;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1161 = _M0L8capacityS379 * 13;
  return _M0L6_2atmpS1161 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS375
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS375 == 0) {
    if (_M0L4selfS375) {
      moonbit_decref(_M0L4selfS375);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS376 =
      _M0L4selfS375;
    return _M0L7_2aSomeS376;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS377
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS377 == 0) {
    if (_M0L4selfS377) {
      moonbit_decref(_M0L4selfS377);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS378 =
      _M0L4selfS377;
    return _M0L7_2aSomeS378;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS374
) {
  moonbit_string_t* _M0L6_2atmpS1160;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1160 = _M0L4selfS374;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1160);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS373
) {
  moonbit_string_t* _M0L6_2atmpS1158;
  int32_t _M0L6_2atmpS1637;
  int32_t _M0L6_2atmpS1159;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1157;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS373);
  _M0L6_2atmpS1158 = _M0L4selfS373;
  _M0L6_2atmpS1637 = Moonbit_array_length(_M0L4selfS373);
  moonbit_decref(_M0L4selfS373);
  _M0L6_2atmpS1159 = _M0L6_2atmpS1637;
  _M0L6_2atmpS1157
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1159, _M0L6_2atmpS1158
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1157);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS371
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS370;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__* _closure_1816;
  struct _M0TWEOs* _M0L6_2atmpS1145;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS370
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS370)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS370->$0 = 0;
  _closure_1816
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__));
  Moonbit_object_header(_closure_1816)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__, $0_0) >> 2, 2, 0);
  _closure_1816->code = &_M0MPC15array9ArrayView4iterGsEC1146l570;
  _closure_1816->$0_0 = _M0L4selfS371.$0;
  _closure_1816->$0_1 = _M0L4selfS371.$1;
  _closure_1816->$0_2 = _M0L4selfS371.$2;
  _closure_1816->$1 = _M0L1iS370;
  _M0L6_2atmpS1145 = (struct _M0TWEOs*)_closure_1816;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1145);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1146l570(
  struct _M0TWEOs* _M0L6_2aenvS1147
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__* _M0L14_2acasted__envS1148;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS1642;
  struct _M0TPC13ref3RefGiE* _M0L1iS370;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS1641;
  int32_t _M0L6_2acntS1744;
  struct _M0TPB9ArrayViewGsE _M0L4selfS371;
  int32_t _M0L3valS1149;
  int32_t _M0L6_2atmpS1150;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1148
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1146__l570__*)_M0L6_2aenvS1147;
  _M0L8_2afieldS1642 = _M0L14_2acasted__envS1148->$1;
  _M0L1iS370 = _M0L8_2afieldS1642;
  _M0L8_2afieldS1641
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1148->$0_1,
      _M0L14_2acasted__envS1148->$0_2,
      _M0L14_2acasted__envS1148->$0_0
  };
  _M0L6_2acntS1744 = Moonbit_object_header(_M0L14_2acasted__envS1148)->rc;
  if (_M0L6_2acntS1744 > 1) {
    int32_t _M0L11_2anew__cntS1745 = _M0L6_2acntS1744 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1148)->rc
    = _M0L11_2anew__cntS1745;
    moonbit_incref(_M0L1iS370);
    moonbit_incref(_M0L8_2afieldS1641.$0);
  } else if (_M0L6_2acntS1744 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1148);
  }
  _M0L4selfS371 = _M0L8_2afieldS1641;
  _M0L3valS1149 = _M0L1iS370->$0;
  moonbit_incref(_M0L4selfS371.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1150 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS371);
  if (_M0L3valS1149 < _M0L6_2atmpS1150) {
    moonbit_string_t* _M0L8_2afieldS1640 = _M0L4selfS371.$0;
    moonbit_string_t* _M0L3bufS1153 = _M0L8_2afieldS1640;
    int32_t _M0L8_2afieldS1639 = _M0L4selfS371.$1;
    int32_t _M0L5startS1155 = _M0L8_2afieldS1639;
    int32_t _M0L3valS1156 = _M0L1iS370->$0;
    int32_t _M0L6_2atmpS1154 = _M0L5startS1155 + _M0L3valS1156;
    moonbit_string_t _M0L6_2atmpS1638 =
      (moonbit_string_t)_M0L3bufS1153[_M0L6_2atmpS1154];
    moonbit_string_t _M0L4elemS372;
    int32_t _M0L3valS1152;
    int32_t _M0L6_2atmpS1151;
    moonbit_incref(_M0L6_2atmpS1638);
    moonbit_decref(_M0L3bufS1153);
    _M0L4elemS372 = _M0L6_2atmpS1638;
    _M0L3valS1152 = _M0L1iS370->$0;
    _M0L6_2atmpS1151 = _M0L3valS1152 + 1;
    _M0L1iS370->$0 = _M0L6_2atmpS1151;
    moonbit_decref(_M0L1iS370);
    return _M0L4elemS372;
  } else {
    moonbit_decref(_M0L4selfS371.$0);
    moonbit_decref(_M0L1iS370);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS369
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS369;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS368,
  struct _M0TPB6Logger _M0L6loggerS367
) {
  moonbit_string_t _M0L6_2atmpS1144;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1144 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS368, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS367.$0->$method_0(_M0L6loggerS367.$1, _M0L6_2atmpS1144);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS361,
  moonbit_string_t _M0L5valueS363
) {
  int32_t _M0L3lenS1134;
  moonbit_string_t* _M0L6_2atmpS1136;
  int32_t _M0L6_2atmpS1645;
  int32_t _M0L6_2atmpS1135;
  int32_t _M0L6lengthS362;
  moonbit_string_t* _M0L8_2afieldS1644;
  moonbit_string_t* _M0L3bufS1137;
  moonbit_string_t _M0L6_2aoldS1643;
  int32_t _M0L6_2atmpS1138;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1134 = _M0L4selfS361->$1;
  moonbit_incref(_M0L4selfS361);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1136 = _M0MPC15array5Array6bufferGsE(_M0L4selfS361);
  _M0L6_2atmpS1645 = Moonbit_array_length(_M0L6_2atmpS1136);
  moonbit_decref(_M0L6_2atmpS1136);
  _M0L6_2atmpS1135 = _M0L6_2atmpS1645;
  if (_M0L3lenS1134 == _M0L6_2atmpS1135) {
    moonbit_incref(_M0L4selfS361);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS361);
  }
  _M0L6lengthS362 = _M0L4selfS361->$1;
  _M0L8_2afieldS1644 = _M0L4selfS361->$0;
  _M0L3bufS1137 = _M0L8_2afieldS1644;
  _M0L6_2aoldS1643 = (moonbit_string_t)_M0L3bufS1137[_M0L6lengthS362];
  moonbit_decref(_M0L6_2aoldS1643);
  _M0L3bufS1137[_M0L6lengthS362] = _M0L5valueS363;
  _M0L6_2atmpS1138 = _M0L6lengthS362 + 1;
  _M0L4selfS361->$1 = _M0L6_2atmpS1138;
  moonbit_decref(_M0L4selfS361);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS364,
  struct _M0TUsiE* _M0L5valueS366
) {
  int32_t _M0L3lenS1139;
  struct _M0TUsiE** _M0L6_2atmpS1141;
  int32_t _M0L6_2atmpS1648;
  int32_t _M0L6_2atmpS1140;
  int32_t _M0L6lengthS365;
  struct _M0TUsiE** _M0L8_2afieldS1647;
  struct _M0TUsiE** _M0L3bufS1142;
  struct _M0TUsiE* _M0L6_2aoldS1646;
  int32_t _M0L6_2atmpS1143;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1139 = _M0L4selfS364->$1;
  moonbit_incref(_M0L4selfS364);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1141 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS364);
  _M0L6_2atmpS1648 = Moonbit_array_length(_M0L6_2atmpS1141);
  moonbit_decref(_M0L6_2atmpS1141);
  _M0L6_2atmpS1140 = _M0L6_2atmpS1648;
  if (_M0L3lenS1139 == _M0L6_2atmpS1140) {
    moonbit_incref(_M0L4selfS364);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS364);
  }
  _M0L6lengthS365 = _M0L4selfS364->$1;
  _M0L8_2afieldS1647 = _M0L4selfS364->$0;
  _M0L3bufS1142 = _M0L8_2afieldS1647;
  _M0L6_2aoldS1646 = (struct _M0TUsiE*)_M0L3bufS1142[_M0L6lengthS365];
  if (_M0L6_2aoldS1646) {
    moonbit_decref(_M0L6_2aoldS1646);
  }
  _M0L3bufS1142[_M0L6lengthS365] = _M0L5valueS366;
  _M0L6_2atmpS1143 = _M0L6lengthS365 + 1;
  _M0L4selfS364->$1 = _M0L6_2atmpS1143;
  moonbit_decref(_M0L4selfS364);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS356) {
  int32_t _M0L8old__capS355;
  int32_t _M0L8new__capS357;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS355 = _M0L4selfS356->$1;
  if (_M0L8old__capS355 == 0) {
    _M0L8new__capS357 = 8;
  } else {
    _M0L8new__capS357 = _M0L8old__capS355 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS356, _M0L8new__capS357);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS359
) {
  int32_t _M0L8old__capS358;
  int32_t _M0L8new__capS360;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS358 = _M0L4selfS359->$1;
  if (_M0L8old__capS358 == 0) {
    _M0L8new__capS360 = 8;
  } else {
    _M0L8new__capS360 = _M0L8old__capS358 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS359, _M0L8new__capS360);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS346,
  int32_t _M0L13new__capacityS344
) {
  moonbit_string_t* _M0L8new__bufS343;
  moonbit_string_t* _M0L8_2afieldS1650;
  moonbit_string_t* _M0L8old__bufS345;
  int32_t _M0L8old__capS347;
  int32_t _M0L9copy__lenS348;
  moonbit_string_t* _M0L6_2aoldS1649;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS343
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS344, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS1650 = _M0L4selfS346->$0;
  _M0L8old__bufS345 = _M0L8_2afieldS1650;
  _M0L8old__capS347 = Moonbit_array_length(_M0L8old__bufS345);
  if (_M0L8old__capS347 < _M0L13new__capacityS344) {
    _M0L9copy__lenS348 = _M0L8old__capS347;
  } else {
    _M0L9copy__lenS348 = _M0L13new__capacityS344;
  }
  moonbit_incref(_M0L8old__bufS345);
  moonbit_incref(_M0L8new__bufS343);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS343, 0, _M0L8old__bufS345, 0, _M0L9copy__lenS348);
  _M0L6_2aoldS1649 = _M0L4selfS346->$0;
  moonbit_decref(_M0L6_2aoldS1649);
  _M0L4selfS346->$0 = _M0L8new__bufS343;
  moonbit_decref(_M0L4selfS346);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS352,
  int32_t _M0L13new__capacityS350
) {
  struct _M0TUsiE** _M0L8new__bufS349;
  struct _M0TUsiE** _M0L8_2afieldS1652;
  struct _M0TUsiE** _M0L8old__bufS351;
  int32_t _M0L8old__capS353;
  int32_t _M0L9copy__lenS354;
  struct _M0TUsiE** _M0L6_2aoldS1651;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS349
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS350, 0);
  _M0L8_2afieldS1652 = _M0L4selfS352->$0;
  _M0L8old__bufS351 = _M0L8_2afieldS1652;
  _M0L8old__capS353 = Moonbit_array_length(_M0L8old__bufS351);
  if (_M0L8old__capS353 < _M0L13new__capacityS350) {
    _M0L9copy__lenS354 = _M0L8old__capS353;
  } else {
    _M0L9copy__lenS354 = _M0L13new__capacityS350;
  }
  moonbit_incref(_M0L8old__bufS351);
  moonbit_incref(_M0L8new__bufS349);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS349, 0, _M0L8old__bufS351, 0, _M0L9copy__lenS354);
  _M0L6_2aoldS1651 = _M0L4selfS352->$0;
  moonbit_decref(_M0L6_2aoldS1651);
  _M0L4selfS352->$0 = _M0L8new__bufS349;
  moonbit_decref(_M0L4selfS352);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS342
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS342 == 0) {
    moonbit_string_t* _M0L6_2atmpS1132 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_1817 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1817)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1817->$0 = _M0L6_2atmpS1132;
    _block_1817->$1 = 0;
    return _block_1817;
  } else {
    moonbit_string_t* _M0L6_2atmpS1133 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS342, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_1818 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1818)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1818->$0 = _M0L6_2atmpS1133;
    _block_1818->$1 = 0;
    return _block_1818;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS340,
  struct _M0TPC16string10StringView _M0L3strS341
) {
  int32_t _M0L3lenS1120;
  int32_t _M0L6_2atmpS1122;
  int32_t _M0L6_2atmpS1121;
  int32_t _M0L6_2atmpS1119;
  moonbit_bytes_t _M0L8_2afieldS1653;
  moonbit_bytes_t _M0L4dataS1123;
  int32_t _M0L3lenS1124;
  moonbit_string_t _M0L6_2atmpS1125;
  int32_t _M0L6_2atmpS1126;
  int32_t _M0L6_2atmpS1127;
  int32_t _M0L3lenS1129;
  int32_t _M0L6_2atmpS1131;
  int32_t _M0L6_2atmpS1130;
  int32_t _M0L6_2atmpS1128;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1120 = _M0L4selfS340->$1;
  moonbit_incref(_M0L3strS341.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1122 = _M0MPC16string10StringView6length(_M0L3strS341);
  _M0L6_2atmpS1121 = _M0L6_2atmpS1122 * 2;
  _M0L6_2atmpS1119 = _M0L3lenS1120 + _M0L6_2atmpS1121;
  moonbit_incref(_M0L4selfS340);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS340, _M0L6_2atmpS1119);
  _M0L8_2afieldS1653 = _M0L4selfS340->$0;
  _M0L4dataS1123 = _M0L8_2afieldS1653;
  _M0L3lenS1124 = _M0L4selfS340->$1;
  moonbit_incref(_M0L4dataS1123);
  moonbit_incref(_M0L3strS341.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1125 = _M0MPC16string10StringView4data(_M0L3strS341);
  moonbit_incref(_M0L3strS341.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1126 = _M0MPC16string10StringView13start__offset(_M0L3strS341);
  moonbit_incref(_M0L3strS341.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1127 = _M0MPC16string10StringView6length(_M0L3strS341);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1123, _M0L3lenS1124, _M0L6_2atmpS1125, _M0L6_2atmpS1126, _M0L6_2atmpS1127);
  _M0L3lenS1129 = _M0L4selfS340->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1131 = _M0MPC16string10StringView6length(_M0L3strS341);
  _M0L6_2atmpS1130 = _M0L6_2atmpS1131 * 2;
  _M0L6_2atmpS1128 = _M0L3lenS1129 + _M0L6_2atmpS1130;
  _M0L4selfS340->$1 = _M0L6_2atmpS1128;
  moonbit_decref(_M0L4selfS340);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS332,
  int32_t _M0L3lenS335,
  int32_t _M0L13start__offsetS339,
  int64_t _M0L11end__offsetS330
) {
  int32_t _M0L11end__offsetS329;
  int32_t _M0L5indexS333;
  int32_t _M0L5countS334;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS330 == 4294967296ll) {
    _M0L11end__offsetS329 = Moonbit_array_length(_M0L4selfS332);
  } else {
    int64_t _M0L7_2aSomeS331 = _M0L11end__offsetS330;
    _M0L11end__offsetS329 = (int32_t)_M0L7_2aSomeS331;
  }
  _M0L5indexS333 = _M0L13start__offsetS339;
  _M0L5countS334 = 0;
  while (1) {
    int32_t _if__result_1820;
    if (_M0L5indexS333 < _M0L11end__offsetS329) {
      _if__result_1820 = _M0L5countS334 < _M0L3lenS335;
    } else {
      _if__result_1820 = 0;
    }
    if (_if__result_1820) {
      int32_t _M0L2c1S336 = _M0L4selfS332[_M0L5indexS333];
      int32_t _if__result_1821;
      int32_t _M0L6_2atmpS1117;
      int32_t _M0L6_2atmpS1118;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S336)) {
        int32_t _M0L6_2atmpS1113 = _M0L5indexS333 + 1;
        _if__result_1821 = _M0L6_2atmpS1113 < _M0L11end__offsetS329;
      } else {
        _if__result_1821 = 0;
      }
      if (_if__result_1821) {
        int32_t _M0L6_2atmpS1116 = _M0L5indexS333 + 1;
        int32_t _M0L2c2S337 = _M0L4selfS332[_M0L6_2atmpS1116];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S337)) {
          int32_t _M0L6_2atmpS1114 = _M0L5indexS333 + 2;
          int32_t _M0L6_2atmpS1115 = _M0L5countS334 + 1;
          _M0L5indexS333 = _M0L6_2atmpS1114;
          _M0L5countS334 = _M0L6_2atmpS1115;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data);
        }
      }
      _M0L6_2atmpS1117 = _M0L5indexS333 + 1;
      _M0L6_2atmpS1118 = _M0L5countS334 + 1;
      _M0L5indexS333 = _M0L6_2atmpS1117;
      _M0L5countS334 = _M0L6_2atmpS1118;
      continue;
    } else {
      moonbit_decref(_M0L4selfS332);
      return _M0L5countS334 >= _M0L3lenS335;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS326
) {
  int32_t _M0L3endS1107;
  int32_t _M0L8_2afieldS1654;
  int32_t _M0L5startS1108;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1107 = _M0L4selfS326.$2;
  _M0L8_2afieldS1654 = _M0L4selfS326.$1;
  moonbit_decref(_M0L4selfS326.$0);
  _M0L5startS1108 = _M0L8_2afieldS1654;
  return _M0L3endS1107 - _M0L5startS1108;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS327
) {
  int32_t _M0L3endS1109;
  int32_t _M0L8_2afieldS1655;
  int32_t _M0L5startS1110;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1109 = _M0L4selfS327.$2;
  _M0L8_2afieldS1655 = _M0L4selfS327.$1;
  moonbit_decref(_M0L4selfS327.$0);
  _M0L5startS1110 = _M0L8_2afieldS1655;
  return _M0L3endS1109 - _M0L5startS1110;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS328
) {
  int32_t _M0L3endS1111;
  int32_t _M0L8_2afieldS1656;
  int32_t _M0L5startS1112;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1111 = _M0L4selfS328.$2;
  _M0L8_2afieldS1656 = _M0L4selfS328.$1;
  moonbit_decref(_M0L4selfS328.$0);
  _M0L5startS1112 = _M0L8_2afieldS1656;
  return _M0L3endS1111 - _M0L5startS1112;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS324,
  int64_t _M0L19start__offset_2eoptS322,
  int64_t _M0L11end__offsetS325
) {
  int32_t _M0L13start__offsetS321;
  if (_M0L19start__offset_2eoptS322 == 4294967296ll) {
    _M0L13start__offsetS321 = 0;
  } else {
    int64_t _M0L7_2aSomeS323 = _M0L19start__offset_2eoptS322;
    _M0L13start__offsetS321 = (int32_t)_M0L7_2aSomeS323;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS324, _M0L13start__offsetS321, _M0L11end__offsetS325);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS319,
  int32_t _M0L13start__offsetS320,
  int64_t _M0L11end__offsetS317
) {
  int32_t _M0L11end__offsetS316;
  int32_t _if__result_1822;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS317 == 4294967296ll) {
    _M0L11end__offsetS316 = Moonbit_array_length(_M0L4selfS319);
  } else {
    int64_t _M0L7_2aSomeS318 = _M0L11end__offsetS317;
    _M0L11end__offsetS316 = (int32_t)_M0L7_2aSomeS318;
  }
  if (_M0L13start__offsetS320 >= 0) {
    if (_M0L13start__offsetS320 <= _M0L11end__offsetS316) {
      int32_t _M0L6_2atmpS1106 = Moonbit_array_length(_M0L4selfS319);
      _if__result_1822 = _M0L11end__offsetS316 <= _M0L6_2atmpS1106;
    } else {
      _if__result_1822 = 0;
    }
  } else {
    _if__result_1822 = 0;
  }
  if (_if__result_1822) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS320,
                                                 _M0L11end__offsetS316,
                                                 _M0L4selfS319};
  } else {
    moonbit_decref(_M0L4selfS319);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_13.data, (moonbit_string_t)moonbit_string_literal_14.data);
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS308,
  struct _M0TPB6Logger _M0L6loggerS306
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS307;
  int32_t _M0L3lenS309;
  int32_t _M0L1iS310;
  int32_t _M0L3segS311;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS306.$1) {
    moonbit_incref(_M0L6loggerS306.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS306.$0->$method_3(_M0L6loggerS306.$1, 34);
  moonbit_incref(_M0L4selfS308);
  if (_M0L6loggerS306.$1) {
    moonbit_incref(_M0L6loggerS306.$1);
  }
  _M0L6_2aenvS307
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS307)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS307->$0 = _M0L4selfS308;
  _M0L6_2aenvS307->$1_0 = _M0L6loggerS306.$0;
  _M0L6_2aenvS307->$1_1 = _M0L6loggerS306.$1;
  _M0L3lenS309 = Moonbit_array_length(_M0L4selfS308);
  _M0L1iS310 = 0;
  _M0L3segS311 = 0;
  _2afor_312:;
  while (1) {
    int32_t _M0L4codeS313;
    int32_t _M0L1cS315;
    int32_t _M0L6_2atmpS1090;
    int32_t _M0L6_2atmpS1091;
    int32_t _M0L6_2atmpS1092;
    int32_t _tmp_1826;
    int32_t _tmp_1827;
    if (_M0L1iS310 >= _M0L3lenS309) {
      moonbit_decref(_M0L4selfS308);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
      break;
    }
    _M0L4codeS313 = _M0L4selfS308[_M0L1iS310];
    switch (_M0L4codeS313) {
      case 34: {
        _M0L1cS315 = _M0L4codeS313;
        goto join_314;
        break;
      }
      
      case 92: {
        _M0L1cS315 = _M0L4codeS313;
        goto join_314;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1093;
        int32_t _M0L6_2atmpS1094;
        moonbit_incref(_M0L6_2aenvS307);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
        if (_M0L6loggerS306.$1) {
          moonbit_incref(_M0L6loggerS306.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS306.$0->$method_0(_M0L6loggerS306.$1, (moonbit_string_t)moonbit_string_literal_15.data);
        _M0L6_2atmpS1093 = _M0L1iS310 + 1;
        _M0L6_2atmpS1094 = _M0L1iS310 + 1;
        _M0L1iS310 = _M0L6_2atmpS1093;
        _M0L3segS311 = _M0L6_2atmpS1094;
        goto _2afor_312;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1095;
        int32_t _M0L6_2atmpS1096;
        moonbit_incref(_M0L6_2aenvS307);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
        if (_M0L6loggerS306.$1) {
          moonbit_incref(_M0L6loggerS306.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS306.$0->$method_0(_M0L6loggerS306.$1, (moonbit_string_t)moonbit_string_literal_16.data);
        _M0L6_2atmpS1095 = _M0L1iS310 + 1;
        _M0L6_2atmpS1096 = _M0L1iS310 + 1;
        _M0L1iS310 = _M0L6_2atmpS1095;
        _M0L3segS311 = _M0L6_2atmpS1096;
        goto _2afor_312;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1097;
        int32_t _M0L6_2atmpS1098;
        moonbit_incref(_M0L6_2aenvS307);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
        if (_M0L6loggerS306.$1) {
          moonbit_incref(_M0L6loggerS306.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS306.$0->$method_0(_M0L6loggerS306.$1, (moonbit_string_t)moonbit_string_literal_17.data);
        _M0L6_2atmpS1097 = _M0L1iS310 + 1;
        _M0L6_2atmpS1098 = _M0L1iS310 + 1;
        _M0L1iS310 = _M0L6_2atmpS1097;
        _M0L3segS311 = _M0L6_2atmpS1098;
        goto _2afor_312;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1099;
        int32_t _M0L6_2atmpS1100;
        moonbit_incref(_M0L6_2aenvS307);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
        if (_M0L6loggerS306.$1) {
          moonbit_incref(_M0L6loggerS306.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS306.$0->$method_0(_M0L6loggerS306.$1, (moonbit_string_t)moonbit_string_literal_18.data);
        _M0L6_2atmpS1099 = _M0L1iS310 + 1;
        _M0L6_2atmpS1100 = _M0L1iS310 + 1;
        _M0L1iS310 = _M0L6_2atmpS1099;
        _M0L3segS311 = _M0L6_2atmpS1100;
        goto _2afor_312;
        break;
      }
      default: {
        if (_M0L4codeS313 < 32) {
          int32_t _M0L6_2atmpS1102;
          moonbit_string_t _M0L6_2atmpS1101;
          int32_t _M0L6_2atmpS1103;
          int32_t _M0L6_2atmpS1104;
          moonbit_incref(_M0L6_2aenvS307);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
          if (_M0L6loggerS306.$1) {
            moonbit_incref(_M0L6loggerS306.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS306.$0->$method_0(_M0L6loggerS306.$1, (moonbit_string_t)moonbit_string_literal_19.data);
          _M0L6_2atmpS1102 = _M0L4codeS313 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1101 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1102);
          if (_M0L6loggerS306.$1) {
            moonbit_incref(_M0L6loggerS306.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS306.$0->$method_0(_M0L6loggerS306.$1, _M0L6_2atmpS1101);
          if (_M0L6loggerS306.$1) {
            moonbit_incref(_M0L6loggerS306.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS306.$0->$method_3(_M0L6loggerS306.$1, 125);
          _M0L6_2atmpS1103 = _M0L1iS310 + 1;
          _M0L6_2atmpS1104 = _M0L1iS310 + 1;
          _M0L1iS310 = _M0L6_2atmpS1103;
          _M0L3segS311 = _M0L6_2atmpS1104;
          goto _2afor_312;
        } else {
          int32_t _M0L6_2atmpS1105 = _M0L1iS310 + 1;
          int32_t _tmp_1825 = _M0L3segS311;
          _M0L1iS310 = _M0L6_2atmpS1105;
          _M0L3segS311 = _tmp_1825;
          goto _2afor_312;
        }
        break;
      }
    }
    goto joinlet_1824;
    join_314:;
    moonbit_incref(_M0L6_2aenvS307);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS307, _M0L3segS311, _M0L1iS310);
    if (_M0L6loggerS306.$1) {
      moonbit_incref(_M0L6loggerS306.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS306.$0->$method_3(_M0L6loggerS306.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1090 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS315);
    if (_M0L6loggerS306.$1) {
      moonbit_incref(_M0L6loggerS306.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS306.$0->$method_3(_M0L6loggerS306.$1, _M0L6_2atmpS1090);
    _M0L6_2atmpS1091 = _M0L1iS310 + 1;
    _M0L6_2atmpS1092 = _M0L1iS310 + 1;
    _M0L1iS310 = _M0L6_2atmpS1091;
    _M0L3segS311 = _M0L6_2atmpS1092;
    continue;
    joinlet_1824:;
    _tmp_1826 = _M0L1iS310;
    _tmp_1827 = _M0L3segS311;
    _M0L1iS310 = _tmp_1826;
    _M0L3segS311 = _tmp_1827;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS306.$0->$method_3(_M0L6loggerS306.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS302,
  int32_t _M0L3segS305,
  int32_t _M0L1iS304
) {
  struct _M0TPB6Logger _M0L8_2afieldS1658;
  struct _M0TPB6Logger _M0L6loggerS301;
  moonbit_string_t _M0L8_2afieldS1657;
  int32_t _M0L6_2acntS1746;
  moonbit_string_t _M0L4selfS303;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS1658
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS302->$1_0, _M0L6_2aenvS302->$1_1
  };
  _M0L6loggerS301 = _M0L8_2afieldS1658;
  _M0L8_2afieldS1657 = _M0L6_2aenvS302->$0;
  _M0L6_2acntS1746 = Moonbit_object_header(_M0L6_2aenvS302)->rc;
  if (_M0L6_2acntS1746 > 1) {
    int32_t _M0L11_2anew__cntS1747 = _M0L6_2acntS1746 - 1;
    Moonbit_object_header(_M0L6_2aenvS302)->rc = _M0L11_2anew__cntS1747;
    if (_M0L6loggerS301.$1) {
      moonbit_incref(_M0L6loggerS301.$1);
    }
    moonbit_incref(_M0L8_2afieldS1657);
  } else if (_M0L6_2acntS1746 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS302);
  }
  _M0L4selfS303 = _M0L8_2afieldS1657;
  if (_M0L1iS304 > _M0L3segS305) {
    int32_t _M0L6_2atmpS1089 = _M0L1iS304 - _M0L3segS305;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS301.$0->$method_1(_M0L6loggerS301.$1, _M0L4selfS303, _M0L3segS305, _M0L6_2atmpS1089);
  } else {
    moonbit_decref(_M0L4selfS303);
    if (_M0L6loggerS301.$1) {
      moonbit_decref(_M0L6loggerS301.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS300) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS299;
  int32_t _M0L6_2atmpS1086;
  int32_t _M0L6_2atmpS1085;
  int32_t _M0L6_2atmpS1088;
  int32_t _M0L6_2atmpS1087;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1084;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS299 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1086 = _M0IPC14byte4BytePB3Div3div(_M0L1bS300, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1085
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1086);
  moonbit_incref(_M0L7_2aselfS299);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS299, _M0L6_2atmpS1085);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1088 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS300, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1087
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1088);
  moonbit_incref(_M0L7_2aselfS299);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS299, _M0L6_2atmpS1087);
  _M0L6_2atmpS1084 = _M0L7_2aselfS299;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1084);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS298) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS298 < 10) {
    int32_t _M0L6_2atmpS1081;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1081 = _M0IPC14byte4BytePB3Add3add(_M0L1iS298, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1081);
  } else {
    int32_t _M0L6_2atmpS1083;
    int32_t _M0L6_2atmpS1082;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1083 = _M0IPC14byte4BytePB3Add3add(_M0L1iS298, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1082 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1083, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1082);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS296,
  int32_t _M0L4thatS297
) {
  int32_t _M0L6_2atmpS1079;
  int32_t _M0L6_2atmpS1080;
  int32_t _M0L6_2atmpS1078;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1079 = (int32_t)_M0L4selfS296;
  _M0L6_2atmpS1080 = (int32_t)_M0L4thatS297;
  _M0L6_2atmpS1078 = _M0L6_2atmpS1079 - _M0L6_2atmpS1080;
  return _M0L6_2atmpS1078 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS294,
  int32_t _M0L4thatS295
) {
  int32_t _M0L6_2atmpS1076;
  int32_t _M0L6_2atmpS1077;
  int32_t _M0L6_2atmpS1075;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1076 = (int32_t)_M0L4selfS294;
  _M0L6_2atmpS1077 = (int32_t)_M0L4thatS295;
  _M0L6_2atmpS1075 = _M0L6_2atmpS1076 % _M0L6_2atmpS1077;
  return _M0L6_2atmpS1075 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS292,
  int32_t _M0L4thatS293
) {
  int32_t _M0L6_2atmpS1073;
  int32_t _M0L6_2atmpS1074;
  int32_t _M0L6_2atmpS1072;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1073 = (int32_t)_M0L4selfS292;
  _M0L6_2atmpS1074 = (int32_t)_M0L4thatS293;
  _M0L6_2atmpS1072 = _M0L6_2atmpS1073 / _M0L6_2atmpS1074;
  return _M0L6_2atmpS1072 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS290,
  int32_t _M0L4thatS291
) {
  int32_t _M0L6_2atmpS1070;
  int32_t _M0L6_2atmpS1071;
  int32_t _M0L6_2atmpS1069;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1070 = (int32_t)_M0L4selfS290;
  _M0L6_2atmpS1071 = (int32_t)_M0L4thatS291;
  _M0L6_2atmpS1069 = _M0L6_2atmpS1070 + _M0L6_2atmpS1071;
  return _M0L6_2atmpS1069 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS287,
  int32_t _M0L5startS285,
  int32_t _M0L3endS286
) {
  int32_t _if__result_1828;
  int32_t _M0L3lenS288;
  int32_t _M0L6_2atmpS1067;
  int32_t _M0L6_2atmpS1068;
  moonbit_bytes_t _M0L5bytesS289;
  moonbit_bytes_t _M0L6_2atmpS1066;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS285 == 0) {
    int32_t _M0L6_2atmpS1065 = Moonbit_array_length(_M0L3strS287);
    _if__result_1828 = _M0L3endS286 == _M0L6_2atmpS1065;
  } else {
    _if__result_1828 = 0;
  }
  if (_if__result_1828) {
    return _M0L3strS287;
  }
  _M0L3lenS288 = _M0L3endS286 - _M0L5startS285;
  _M0L6_2atmpS1067 = _M0L3lenS288 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1068 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS289
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1067, _M0L6_2atmpS1068);
  moonbit_incref(_M0L5bytesS289);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS289, 0, _M0L3strS287, _M0L5startS285, _M0L3lenS288);
  _M0L6_2atmpS1066 = _M0L5bytesS289;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1066, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS284) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS284;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS268,
  int32_t _M0L5radixS267
) {
  int32_t _if__result_1829;
  int32_t _M0L12is__negativeS269;
  uint32_t _M0L3numS270;
  uint16_t* _M0L6bufferS271;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS267 < 2) {
    _if__result_1829 = 1;
  } else {
    _if__result_1829 = _M0L5radixS267 > 36;
  }
  if (_if__result_1829) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data);
  }
  if (_M0L4selfS268 == 0) {
    return (moonbit_string_t)moonbit_string_literal_22.data;
  }
  _M0L12is__negativeS269 = _M0L4selfS268 < 0;
  if (_M0L12is__negativeS269) {
    int32_t _M0L6_2atmpS1064 = -_M0L4selfS268;
    _M0L3numS270 = *(uint32_t*)&_M0L6_2atmpS1064;
  } else {
    _M0L3numS270 = *(uint32_t*)&_M0L4selfS268;
  }
  switch (_M0L5radixS267) {
    case 10: {
      int32_t _M0L10digit__lenS272;
      int32_t _M0L6_2atmpS1061;
      int32_t _M0L10total__lenS273;
      uint16_t* _M0L6bufferS274;
      int32_t _M0L12digit__startS275;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS272 = _M0FPB12dec__count32(_M0L3numS270);
      if (_M0L12is__negativeS269) {
        _M0L6_2atmpS1061 = 1;
      } else {
        _M0L6_2atmpS1061 = 0;
      }
      _M0L10total__lenS273 = _M0L10digit__lenS272 + _M0L6_2atmpS1061;
      _M0L6bufferS274
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS273, 0);
      if (_M0L12is__negativeS269) {
        _M0L12digit__startS275 = 1;
      } else {
        _M0L12digit__startS275 = 0;
      }
      moonbit_incref(_M0L6bufferS274);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS274, _M0L3numS270, _M0L12digit__startS275, _M0L10total__lenS273);
      _M0L6bufferS271 = _M0L6bufferS274;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS276;
      int32_t _M0L6_2atmpS1062;
      int32_t _M0L10total__lenS277;
      uint16_t* _M0L6bufferS278;
      int32_t _M0L12digit__startS279;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS276 = _M0FPB12hex__count32(_M0L3numS270);
      if (_M0L12is__negativeS269) {
        _M0L6_2atmpS1062 = 1;
      } else {
        _M0L6_2atmpS1062 = 0;
      }
      _M0L10total__lenS277 = _M0L10digit__lenS276 + _M0L6_2atmpS1062;
      _M0L6bufferS278
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS277, 0);
      if (_M0L12is__negativeS269) {
        _M0L12digit__startS279 = 1;
      } else {
        _M0L12digit__startS279 = 0;
      }
      moonbit_incref(_M0L6bufferS278);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS278, _M0L3numS270, _M0L12digit__startS279, _M0L10total__lenS277);
      _M0L6bufferS271 = _M0L6bufferS278;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS280;
      int32_t _M0L6_2atmpS1063;
      int32_t _M0L10total__lenS281;
      uint16_t* _M0L6bufferS282;
      int32_t _M0L12digit__startS283;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS280
      = _M0FPB14radix__count32(_M0L3numS270, _M0L5radixS267);
      if (_M0L12is__negativeS269) {
        _M0L6_2atmpS1063 = 1;
      } else {
        _M0L6_2atmpS1063 = 0;
      }
      _M0L10total__lenS281 = _M0L10digit__lenS280 + _M0L6_2atmpS1063;
      _M0L6bufferS282
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS281, 0);
      if (_M0L12is__negativeS269) {
        _M0L12digit__startS283 = 1;
      } else {
        _M0L12digit__startS283 = 0;
      }
      moonbit_incref(_M0L6bufferS282);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS282, _M0L3numS270, _M0L12digit__startS283, _M0L10total__lenS281, _M0L5radixS267);
      _M0L6bufferS271 = _M0L6bufferS282;
      break;
    }
  }
  if (_M0L12is__negativeS269) {
    _M0L6bufferS271[0] = 45;
  }
  return _M0L6bufferS271;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS261,
  int32_t _M0L5radixS264
) {
  uint32_t _M0Lm3numS262;
  uint32_t _M0L4baseS263;
  int32_t _M0Lm5countS265;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS261 == 0u) {
    return 1;
  }
  _M0Lm3numS262 = _M0L5valueS261;
  _M0L4baseS263 = *(uint32_t*)&_M0L5radixS264;
  _M0Lm5countS265 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1058 = _M0Lm3numS262;
    if (_M0L6_2atmpS1058 > 0u) {
      int32_t _M0L6_2atmpS1059 = _M0Lm5countS265;
      uint32_t _M0L6_2atmpS1060;
      _M0Lm5countS265 = _M0L6_2atmpS1059 + 1;
      _M0L6_2atmpS1060 = _M0Lm3numS262;
      _M0Lm3numS262 = _M0L6_2atmpS1060 / _M0L4baseS263;
      continue;
    }
    break;
  }
  return _M0Lm5countS265;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS259) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS259 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS260;
    int32_t _M0L6_2atmpS1057;
    int32_t _M0L6_2atmpS1056;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS260 = moonbit_clz32(_M0L5valueS259);
    _M0L6_2atmpS1057 = 31 - _M0L14leading__zerosS260;
    _M0L6_2atmpS1056 = _M0L6_2atmpS1057 / 4;
    return _M0L6_2atmpS1056 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS258) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS258 >= 100000u) {
    if (_M0L5valueS258 >= 10000000u) {
      if (_M0L5valueS258 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS258 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS258 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS258 >= 1000u) {
    if (_M0L5valueS258 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS258 >= 100u) {
    return 3;
  } else if (_M0L5valueS258 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS248,
  uint32_t _M0L3numS236,
  int32_t _M0L12digit__startS239,
  int32_t _M0L10total__lenS238
) {
  uint32_t _M0Lm3numS235;
  int32_t _M0Lm6offsetS237;
  uint32_t _M0L6_2atmpS1055;
  int32_t _M0Lm9remainingS250;
  int32_t _M0L6_2atmpS1036;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS235 = _M0L3numS236;
  _M0Lm6offsetS237 = _M0L10total__lenS238 - _M0L12digit__startS239;
  while (1) {
    uint32_t _M0L6_2atmpS999 = _M0Lm3numS235;
    if (_M0L6_2atmpS999 >= 10000u) {
      uint32_t _M0L6_2atmpS1022 = _M0Lm3numS235;
      uint32_t _M0L1tS240 = _M0L6_2atmpS1022 / 10000u;
      uint32_t _M0L6_2atmpS1021 = _M0Lm3numS235;
      uint32_t _M0L6_2atmpS1020 = _M0L6_2atmpS1021 % 10000u;
      int32_t _M0L1rS241 = *(int32_t*)&_M0L6_2atmpS1020;
      int32_t _M0L2d1S242;
      int32_t _M0L2d2S243;
      int32_t _M0L6_2atmpS1000;
      int32_t _M0L6_2atmpS1019;
      int32_t _M0L6_2atmpS1018;
      int32_t _M0L6d1__hiS244;
      int32_t _M0L6_2atmpS1017;
      int32_t _M0L6_2atmpS1016;
      int32_t _M0L6d1__loS245;
      int32_t _M0L6_2atmpS1015;
      int32_t _M0L6_2atmpS1014;
      int32_t _M0L6d2__hiS246;
      int32_t _M0L6_2atmpS1013;
      int32_t _M0L6_2atmpS1012;
      int32_t _M0L6d2__loS247;
      int32_t _M0L6_2atmpS1002;
      int32_t _M0L6_2atmpS1001;
      int32_t _M0L6_2atmpS1005;
      int32_t _M0L6_2atmpS1004;
      int32_t _M0L6_2atmpS1003;
      int32_t _M0L6_2atmpS1008;
      int32_t _M0L6_2atmpS1007;
      int32_t _M0L6_2atmpS1006;
      int32_t _M0L6_2atmpS1011;
      int32_t _M0L6_2atmpS1010;
      int32_t _M0L6_2atmpS1009;
      _M0Lm3numS235 = _M0L1tS240;
      _M0L2d1S242 = _M0L1rS241 / 100;
      _M0L2d2S243 = _M0L1rS241 % 100;
      _M0L6_2atmpS1000 = _M0Lm6offsetS237;
      _M0Lm6offsetS237 = _M0L6_2atmpS1000 - 4;
      _M0L6_2atmpS1019 = _M0L2d1S242 / 10;
      _M0L6_2atmpS1018 = 48 + _M0L6_2atmpS1019;
      _M0L6d1__hiS244 = (uint16_t)_M0L6_2atmpS1018;
      _M0L6_2atmpS1017 = _M0L2d1S242 % 10;
      _M0L6_2atmpS1016 = 48 + _M0L6_2atmpS1017;
      _M0L6d1__loS245 = (uint16_t)_M0L6_2atmpS1016;
      _M0L6_2atmpS1015 = _M0L2d2S243 / 10;
      _M0L6_2atmpS1014 = 48 + _M0L6_2atmpS1015;
      _M0L6d2__hiS246 = (uint16_t)_M0L6_2atmpS1014;
      _M0L6_2atmpS1013 = _M0L2d2S243 % 10;
      _M0L6_2atmpS1012 = 48 + _M0L6_2atmpS1013;
      _M0L6d2__loS247 = (uint16_t)_M0L6_2atmpS1012;
      _M0L6_2atmpS1002 = _M0Lm6offsetS237;
      _M0L6_2atmpS1001 = _M0L12digit__startS239 + _M0L6_2atmpS1002;
      _M0L6bufferS248[_M0L6_2atmpS1001] = _M0L6d1__hiS244;
      _M0L6_2atmpS1005 = _M0Lm6offsetS237;
      _M0L6_2atmpS1004 = _M0L12digit__startS239 + _M0L6_2atmpS1005;
      _M0L6_2atmpS1003 = _M0L6_2atmpS1004 + 1;
      _M0L6bufferS248[_M0L6_2atmpS1003] = _M0L6d1__loS245;
      _M0L6_2atmpS1008 = _M0Lm6offsetS237;
      _M0L6_2atmpS1007 = _M0L12digit__startS239 + _M0L6_2atmpS1008;
      _M0L6_2atmpS1006 = _M0L6_2atmpS1007 + 2;
      _M0L6bufferS248[_M0L6_2atmpS1006] = _M0L6d2__hiS246;
      _M0L6_2atmpS1011 = _M0Lm6offsetS237;
      _M0L6_2atmpS1010 = _M0L12digit__startS239 + _M0L6_2atmpS1011;
      _M0L6_2atmpS1009 = _M0L6_2atmpS1010 + 3;
      _M0L6bufferS248[_M0L6_2atmpS1009] = _M0L6d2__loS247;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1055 = _M0Lm3numS235;
  _M0Lm9remainingS250 = *(int32_t*)&_M0L6_2atmpS1055;
  while (1) {
    int32_t _M0L6_2atmpS1023 = _M0Lm9remainingS250;
    if (_M0L6_2atmpS1023 >= 100) {
      int32_t _M0L6_2atmpS1035 = _M0Lm9remainingS250;
      int32_t _M0L1tS251 = _M0L6_2atmpS1035 / 100;
      int32_t _M0L6_2atmpS1034 = _M0Lm9remainingS250;
      int32_t _M0L1dS252 = _M0L6_2atmpS1034 % 100;
      int32_t _M0L6_2atmpS1024;
      int32_t _M0L6_2atmpS1033;
      int32_t _M0L6_2atmpS1032;
      int32_t _M0L5d__hiS253;
      int32_t _M0L6_2atmpS1031;
      int32_t _M0L6_2atmpS1030;
      int32_t _M0L5d__loS254;
      int32_t _M0L6_2atmpS1026;
      int32_t _M0L6_2atmpS1025;
      int32_t _M0L6_2atmpS1029;
      int32_t _M0L6_2atmpS1028;
      int32_t _M0L6_2atmpS1027;
      _M0Lm9remainingS250 = _M0L1tS251;
      _M0L6_2atmpS1024 = _M0Lm6offsetS237;
      _M0Lm6offsetS237 = _M0L6_2atmpS1024 - 2;
      _M0L6_2atmpS1033 = _M0L1dS252 / 10;
      _M0L6_2atmpS1032 = 48 + _M0L6_2atmpS1033;
      _M0L5d__hiS253 = (uint16_t)_M0L6_2atmpS1032;
      _M0L6_2atmpS1031 = _M0L1dS252 % 10;
      _M0L6_2atmpS1030 = 48 + _M0L6_2atmpS1031;
      _M0L5d__loS254 = (uint16_t)_M0L6_2atmpS1030;
      _M0L6_2atmpS1026 = _M0Lm6offsetS237;
      _M0L6_2atmpS1025 = _M0L12digit__startS239 + _M0L6_2atmpS1026;
      _M0L6bufferS248[_M0L6_2atmpS1025] = _M0L5d__hiS253;
      _M0L6_2atmpS1029 = _M0Lm6offsetS237;
      _M0L6_2atmpS1028 = _M0L12digit__startS239 + _M0L6_2atmpS1029;
      _M0L6_2atmpS1027 = _M0L6_2atmpS1028 + 1;
      _M0L6bufferS248[_M0L6_2atmpS1027] = _M0L5d__loS254;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1036 = _M0Lm9remainingS250;
  if (_M0L6_2atmpS1036 >= 10) {
    int32_t _M0L6_2atmpS1037 = _M0Lm6offsetS237;
    int32_t _M0L6_2atmpS1048;
    int32_t _M0L6_2atmpS1047;
    int32_t _M0L6_2atmpS1046;
    int32_t _M0L5d__hiS256;
    int32_t _M0L6_2atmpS1045;
    int32_t _M0L6_2atmpS1044;
    int32_t _M0L6_2atmpS1043;
    int32_t _M0L5d__loS257;
    int32_t _M0L6_2atmpS1039;
    int32_t _M0L6_2atmpS1038;
    int32_t _M0L6_2atmpS1042;
    int32_t _M0L6_2atmpS1041;
    int32_t _M0L6_2atmpS1040;
    _M0Lm6offsetS237 = _M0L6_2atmpS1037 - 2;
    _M0L6_2atmpS1048 = _M0Lm9remainingS250;
    _M0L6_2atmpS1047 = _M0L6_2atmpS1048 / 10;
    _M0L6_2atmpS1046 = 48 + _M0L6_2atmpS1047;
    _M0L5d__hiS256 = (uint16_t)_M0L6_2atmpS1046;
    _M0L6_2atmpS1045 = _M0Lm9remainingS250;
    _M0L6_2atmpS1044 = _M0L6_2atmpS1045 % 10;
    _M0L6_2atmpS1043 = 48 + _M0L6_2atmpS1044;
    _M0L5d__loS257 = (uint16_t)_M0L6_2atmpS1043;
    _M0L6_2atmpS1039 = _M0Lm6offsetS237;
    _M0L6_2atmpS1038 = _M0L12digit__startS239 + _M0L6_2atmpS1039;
    _M0L6bufferS248[_M0L6_2atmpS1038] = _M0L5d__hiS256;
    _M0L6_2atmpS1042 = _M0Lm6offsetS237;
    _M0L6_2atmpS1041 = _M0L12digit__startS239 + _M0L6_2atmpS1042;
    _M0L6_2atmpS1040 = _M0L6_2atmpS1041 + 1;
    _M0L6bufferS248[_M0L6_2atmpS1040] = _M0L5d__loS257;
    moonbit_decref(_M0L6bufferS248);
  } else {
    int32_t _M0L6_2atmpS1049 = _M0Lm6offsetS237;
    int32_t _M0L6_2atmpS1054;
    int32_t _M0L6_2atmpS1050;
    int32_t _M0L6_2atmpS1053;
    int32_t _M0L6_2atmpS1052;
    int32_t _M0L6_2atmpS1051;
    _M0Lm6offsetS237 = _M0L6_2atmpS1049 - 1;
    _M0L6_2atmpS1054 = _M0Lm6offsetS237;
    _M0L6_2atmpS1050 = _M0L12digit__startS239 + _M0L6_2atmpS1054;
    _M0L6_2atmpS1053 = _M0Lm9remainingS250;
    _M0L6_2atmpS1052 = 48 + _M0L6_2atmpS1053;
    _M0L6_2atmpS1051 = (uint16_t)_M0L6_2atmpS1052;
    _M0L6bufferS248[_M0L6_2atmpS1050] = _M0L6_2atmpS1051;
    moonbit_decref(_M0L6bufferS248);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS230,
  uint32_t _M0L3numS224,
  int32_t _M0L12digit__startS222,
  int32_t _M0L10total__lenS221,
  int32_t _M0L5radixS226
) {
  int32_t _M0Lm6offsetS220;
  uint32_t _M0Lm1nS223;
  uint32_t _M0L4baseS225;
  int32_t _M0L6_2atmpS981;
  int32_t _M0L6_2atmpS980;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS220 = _M0L10total__lenS221 - _M0L12digit__startS222;
  _M0Lm1nS223 = _M0L3numS224;
  _M0L4baseS225 = *(uint32_t*)&_M0L5radixS226;
  _M0L6_2atmpS981 = _M0L5radixS226 - 1;
  _M0L6_2atmpS980 = _M0L5radixS226 & _M0L6_2atmpS981;
  if (_M0L6_2atmpS980 == 0) {
    int32_t _M0L5shiftS227;
    uint32_t _M0L4maskS228;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS227 = moonbit_ctz32(_M0L5radixS226);
    _M0L4maskS228 = _M0L4baseS225 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS982 = _M0Lm1nS223;
      if (_M0L6_2atmpS982 > 0u) {
        int32_t _M0L6_2atmpS983 = _M0Lm6offsetS220;
        uint32_t _M0L6_2atmpS989;
        uint32_t _M0L6_2atmpS988;
        int32_t _M0L5digitS229;
        int32_t _M0L6_2atmpS986;
        int32_t _M0L6_2atmpS984;
        int32_t _M0L6_2atmpS985;
        uint32_t _M0L6_2atmpS987;
        _M0Lm6offsetS220 = _M0L6_2atmpS983 - 1;
        _M0L6_2atmpS989 = _M0Lm1nS223;
        _M0L6_2atmpS988 = _M0L6_2atmpS989 & _M0L4maskS228;
        _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS988;
        _M0L6_2atmpS986 = _M0Lm6offsetS220;
        _M0L6_2atmpS984 = _M0L12digit__startS222 + _M0L6_2atmpS986;
        _M0L6_2atmpS985
        = ((moonbit_string_t)moonbit_string_literal_23.data)[
          _M0L5digitS229
        ];
        _M0L6bufferS230[_M0L6_2atmpS984] = _M0L6_2atmpS985;
        _M0L6_2atmpS987 = _M0Lm1nS223;
        _M0Lm1nS223 = _M0L6_2atmpS987 >> (_M0L5shiftS227 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS230);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS990 = _M0Lm1nS223;
      if (_M0L6_2atmpS990 > 0u) {
        int32_t _M0L6_2atmpS991 = _M0Lm6offsetS220;
        uint32_t _M0L6_2atmpS998;
        uint32_t _M0L1qS232;
        uint32_t _M0L6_2atmpS996;
        uint32_t _M0L6_2atmpS997;
        uint32_t _M0L6_2atmpS995;
        int32_t _M0L5digitS233;
        int32_t _M0L6_2atmpS994;
        int32_t _M0L6_2atmpS992;
        int32_t _M0L6_2atmpS993;
        _M0Lm6offsetS220 = _M0L6_2atmpS991 - 1;
        _M0L6_2atmpS998 = _M0Lm1nS223;
        _M0L1qS232 = _M0L6_2atmpS998 / _M0L4baseS225;
        _M0L6_2atmpS996 = _M0Lm1nS223;
        _M0L6_2atmpS997 = _M0L1qS232 * _M0L4baseS225;
        _M0L6_2atmpS995 = _M0L6_2atmpS996 - _M0L6_2atmpS997;
        _M0L5digitS233 = *(int32_t*)&_M0L6_2atmpS995;
        _M0L6_2atmpS994 = _M0Lm6offsetS220;
        _M0L6_2atmpS992 = _M0L12digit__startS222 + _M0L6_2atmpS994;
        _M0L6_2atmpS993
        = ((moonbit_string_t)moonbit_string_literal_23.data)[
          _M0L5digitS233
        ];
        _M0L6bufferS230[_M0L6_2atmpS992] = _M0L6_2atmpS993;
        _M0Lm1nS223 = _M0L1qS232;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS230);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS217,
  uint32_t _M0L3numS213,
  int32_t _M0L12digit__startS211,
  int32_t _M0L10total__lenS210
) {
  int32_t _M0Lm6offsetS209;
  uint32_t _M0Lm1nS212;
  int32_t _M0L6_2atmpS976;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS209 = _M0L10total__lenS210 - _M0L12digit__startS211;
  _M0Lm1nS212 = _M0L3numS213;
  while (1) {
    int32_t _M0L6_2atmpS964 = _M0Lm6offsetS209;
    if (_M0L6_2atmpS964 >= 2) {
      int32_t _M0L6_2atmpS965 = _M0Lm6offsetS209;
      uint32_t _M0L6_2atmpS975;
      uint32_t _M0L6_2atmpS974;
      int32_t _M0L9byte__valS214;
      int32_t _M0L2hiS215;
      int32_t _M0L2loS216;
      int32_t _M0L6_2atmpS968;
      int32_t _M0L6_2atmpS966;
      int32_t _M0L6_2atmpS967;
      int32_t _M0L6_2atmpS972;
      int32_t _M0L6_2atmpS971;
      int32_t _M0L6_2atmpS969;
      int32_t _M0L6_2atmpS970;
      uint32_t _M0L6_2atmpS973;
      _M0Lm6offsetS209 = _M0L6_2atmpS965 - 2;
      _M0L6_2atmpS975 = _M0Lm1nS212;
      _M0L6_2atmpS974 = _M0L6_2atmpS975 & 255u;
      _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS974;
      _M0L2hiS215 = _M0L9byte__valS214 / 16;
      _M0L2loS216 = _M0L9byte__valS214 % 16;
      _M0L6_2atmpS968 = _M0Lm6offsetS209;
      _M0L6_2atmpS966 = _M0L12digit__startS211 + _M0L6_2atmpS968;
      _M0L6_2atmpS967
      = ((moonbit_string_t)moonbit_string_literal_23.data)[
        _M0L2hiS215
      ];
      _M0L6bufferS217[_M0L6_2atmpS966] = _M0L6_2atmpS967;
      _M0L6_2atmpS972 = _M0Lm6offsetS209;
      _M0L6_2atmpS971 = _M0L12digit__startS211 + _M0L6_2atmpS972;
      _M0L6_2atmpS969 = _M0L6_2atmpS971 + 1;
      _M0L6_2atmpS970
      = ((moonbit_string_t)moonbit_string_literal_23.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS969] = _M0L6_2atmpS970;
      _M0L6_2atmpS973 = _M0Lm1nS212;
      _M0Lm1nS212 = _M0L6_2atmpS973 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS976 = _M0Lm6offsetS209;
  if (_M0L6_2atmpS976 == 1) {
    uint32_t _M0L6_2atmpS979 = _M0Lm1nS212;
    uint32_t _M0L6_2atmpS978 = _M0L6_2atmpS979 & 15u;
    int32_t _M0L6nibbleS219 = *(int32_t*)&_M0L6_2atmpS978;
    int32_t _M0L6_2atmpS977 =
      ((moonbit_string_t)moonbit_string_literal_23.data)[_M0L6nibbleS219];
    _M0L6bufferS217[_M0L12digit__startS211] = _M0L6_2atmpS977;
    moonbit_decref(_M0L6bufferS217);
  } else {
    moonbit_decref(_M0L6bufferS217);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS208) {
  struct _M0TWEOs* _M0L7_2afuncS207;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS207 = _M0L4selfS208;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS207->code(_M0L7_2afuncS207);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS202
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS201;
  struct _M0TPB6Logger _M0L6_2atmpS961;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS201 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS201);
  _M0L6_2atmpS961
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS201
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS202, _M0L6_2atmpS961);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS201);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS204
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS203;
  struct _M0TPB6Logger _M0L6_2atmpS962;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS203 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS203);
  _M0L6_2atmpS962
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS203
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS204, _M0L6_2atmpS962);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS203);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS206
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS205;
  struct _M0TPB6Logger _M0L6_2atmpS963;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS205 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS205);
  _M0L6_2atmpS963
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS205
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS206, _M0L6_2atmpS963);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS205);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS200
) {
  int32_t _M0L8_2afieldS1659;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1659 = _M0L4selfS200.$1;
  moonbit_decref(_M0L4selfS200.$0);
  return _M0L8_2afieldS1659;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS199
) {
  int32_t _M0L3endS959;
  int32_t _M0L8_2afieldS1660;
  int32_t _M0L5startS960;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS959 = _M0L4selfS199.$2;
  _M0L8_2afieldS1660 = _M0L4selfS199.$1;
  moonbit_decref(_M0L4selfS199.$0);
  _M0L5startS960 = _M0L8_2afieldS1660;
  return _M0L3endS959 - _M0L5startS960;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS198
) {
  moonbit_string_t _M0L8_2afieldS1661;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1661 = _M0L4selfS198.$0;
  return _M0L8_2afieldS1661;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS194,
  moonbit_string_t _M0L5valueS195,
  int32_t _M0L5startS196,
  int32_t _M0L3lenS197
) {
  int32_t _M0L6_2atmpS958;
  int64_t _M0L6_2atmpS957;
  struct _M0TPC16string10StringView _M0L6_2atmpS956;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS958 = _M0L5startS196 + _M0L3lenS197;
  _M0L6_2atmpS957 = (int64_t)_M0L6_2atmpS958;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS956
  = _M0MPC16string6String11sub_2einner(_M0L5valueS195, _M0L5startS196, _M0L6_2atmpS957);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS194, _M0L6_2atmpS956);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS187,
  int32_t _M0L5startS193,
  int64_t _M0L3endS189
) {
  int32_t _M0L3lenS186;
  int32_t _M0L3endS188;
  int32_t _M0L5startS192;
  int32_t _if__result_1836;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS186 = Moonbit_array_length(_M0L4selfS187);
  if (_M0L3endS189 == 4294967296ll) {
    _M0L3endS188 = _M0L3lenS186;
  } else {
    int64_t _M0L7_2aSomeS190 = _M0L3endS189;
    int32_t _M0L6_2aendS191 = (int32_t)_M0L7_2aSomeS190;
    if (_M0L6_2aendS191 < 0) {
      _M0L3endS188 = _M0L3lenS186 + _M0L6_2aendS191;
    } else {
      _M0L3endS188 = _M0L6_2aendS191;
    }
  }
  if (_M0L5startS193 < 0) {
    _M0L5startS192 = _M0L3lenS186 + _M0L5startS193;
  } else {
    _M0L5startS192 = _M0L5startS193;
  }
  if (_M0L5startS192 >= 0) {
    if (_M0L5startS192 <= _M0L3endS188) {
      _if__result_1836 = _M0L3endS188 <= _M0L3lenS186;
    } else {
      _if__result_1836 = 0;
    }
  } else {
    _if__result_1836 = 0;
  }
  if (_if__result_1836) {
    if (_M0L5startS192 < _M0L3lenS186) {
      int32_t _M0L6_2atmpS953 = _M0L4selfS187[_M0L5startS192];
      int32_t _M0L6_2atmpS952;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS952
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS953);
      if (!_M0L6_2atmpS952) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS188 < _M0L3lenS186) {
      int32_t _M0L6_2atmpS955 = _M0L4selfS187[_M0L3endS188];
      int32_t _M0L6_2atmpS954;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS954
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS955);
      if (!_M0L6_2atmpS954) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS192,
                                                 _M0L3endS188,
                                                 _M0L4selfS187};
  } else {
    moonbit_decref(_M0L4selfS187);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS183) {
  struct _M0TPB6Hasher* _M0L1hS182;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS182 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS182);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS182, _M0L4selfS183);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS182);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS185
) {
  struct _M0TPB6Hasher* _M0L1hS184;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS184 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS184);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS184, _M0L4selfS185);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS184);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS180) {
  int32_t _M0L4seedS179;
  if (_M0L10seed_2eoptS180 == 4294967296ll) {
    _M0L4seedS179 = 0;
  } else {
    int64_t _M0L7_2aSomeS181 = _M0L10seed_2eoptS180;
    _M0L4seedS179 = (int32_t)_M0L7_2aSomeS181;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS179);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS178) {
  uint32_t _M0L6_2atmpS951;
  uint32_t _M0L6_2atmpS950;
  struct _M0TPB6Hasher* _block_1837;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS951 = *(uint32_t*)&_M0L4seedS178;
  _M0L6_2atmpS950 = _M0L6_2atmpS951 + 374761393u;
  _block_1837
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_1837)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_1837->$0 = _M0L6_2atmpS950;
  return _block_1837;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS177) {
  uint32_t _M0L6_2atmpS949;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS949 = _M0MPB6Hasher9avalanche(_M0L4selfS177);
  return *(int32_t*)&_M0L6_2atmpS949;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS176) {
  uint32_t _M0L8_2afieldS1662;
  uint32_t _M0Lm3accS175;
  uint32_t _M0L6_2atmpS938;
  uint32_t _M0L6_2atmpS940;
  uint32_t _M0L6_2atmpS939;
  uint32_t _M0L6_2atmpS941;
  uint32_t _M0L6_2atmpS942;
  uint32_t _M0L6_2atmpS944;
  uint32_t _M0L6_2atmpS943;
  uint32_t _M0L6_2atmpS945;
  uint32_t _M0L6_2atmpS946;
  uint32_t _M0L6_2atmpS948;
  uint32_t _M0L6_2atmpS947;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS1662 = _M0L4selfS176->$0;
  moonbit_decref(_M0L4selfS176);
  _M0Lm3accS175 = _M0L8_2afieldS1662;
  _M0L6_2atmpS938 = _M0Lm3accS175;
  _M0L6_2atmpS940 = _M0Lm3accS175;
  _M0L6_2atmpS939 = _M0L6_2atmpS940 >> 15;
  _M0Lm3accS175 = _M0L6_2atmpS938 ^ _M0L6_2atmpS939;
  _M0L6_2atmpS941 = _M0Lm3accS175;
  _M0Lm3accS175 = _M0L6_2atmpS941 * 2246822519u;
  _M0L6_2atmpS942 = _M0Lm3accS175;
  _M0L6_2atmpS944 = _M0Lm3accS175;
  _M0L6_2atmpS943 = _M0L6_2atmpS944 >> 13;
  _M0Lm3accS175 = _M0L6_2atmpS942 ^ _M0L6_2atmpS943;
  _M0L6_2atmpS945 = _M0Lm3accS175;
  _M0Lm3accS175 = _M0L6_2atmpS945 * 3266489917u;
  _M0L6_2atmpS946 = _M0Lm3accS175;
  _M0L6_2atmpS948 = _M0Lm3accS175;
  _M0L6_2atmpS947 = _M0L6_2atmpS948 >> 16;
  _M0Lm3accS175 = _M0L6_2atmpS946 ^ _M0L6_2atmpS947;
  return _M0Lm3accS175;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS172,
  int32_t _M0L5valueS171
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS171, _M0L4selfS172);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS174,
  moonbit_string_t _M0L5valueS173
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS173, _M0L4selfS174);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS169,
  int32_t _M0L5valueS170
) {
  uint32_t _M0L6_2atmpS937;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS937 = *(uint32_t*)&_M0L5valueS170;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS169, _M0L6_2atmpS937);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS167,
  moonbit_string_t _M0L3strS168
) {
  int32_t _M0L3lenS927;
  int32_t _M0L6_2atmpS929;
  int32_t _M0L6_2atmpS928;
  int32_t _M0L6_2atmpS926;
  moonbit_bytes_t _M0L8_2afieldS1664;
  moonbit_bytes_t _M0L4dataS930;
  int32_t _M0L3lenS931;
  int32_t _M0L6_2atmpS932;
  int32_t _M0L3lenS934;
  int32_t _M0L6_2atmpS1663;
  int32_t _M0L6_2atmpS936;
  int32_t _M0L6_2atmpS935;
  int32_t _M0L6_2atmpS933;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS927 = _M0L4selfS167->$1;
  _M0L6_2atmpS929 = Moonbit_array_length(_M0L3strS168);
  _M0L6_2atmpS928 = _M0L6_2atmpS929 * 2;
  _M0L6_2atmpS926 = _M0L3lenS927 + _M0L6_2atmpS928;
  moonbit_incref(_M0L4selfS167);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS167, _M0L6_2atmpS926);
  _M0L8_2afieldS1664 = _M0L4selfS167->$0;
  _M0L4dataS930 = _M0L8_2afieldS1664;
  _M0L3lenS931 = _M0L4selfS167->$1;
  _M0L6_2atmpS932 = Moonbit_array_length(_M0L3strS168);
  moonbit_incref(_M0L4dataS930);
  moonbit_incref(_M0L3strS168);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS930, _M0L3lenS931, _M0L3strS168, 0, _M0L6_2atmpS932);
  _M0L3lenS934 = _M0L4selfS167->$1;
  _M0L6_2atmpS1663 = Moonbit_array_length(_M0L3strS168);
  moonbit_decref(_M0L3strS168);
  _M0L6_2atmpS936 = _M0L6_2atmpS1663;
  _M0L6_2atmpS935 = _M0L6_2atmpS936 * 2;
  _M0L6_2atmpS933 = _M0L3lenS934 + _M0L6_2atmpS935;
  _M0L4selfS167->$1 = _M0L6_2atmpS933;
  moonbit_decref(_M0L4selfS167);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS159,
  int32_t _M0L13bytes__offsetS154,
  moonbit_string_t _M0L3strS161,
  int32_t _M0L11str__offsetS157,
  int32_t _M0L6lengthS155
) {
  int32_t _M0L6_2atmpS925;
  int32_t _M0L6_2atmpS924;
  int32_t _M0L2e1S153;
  int32_t _M0L6_2atmpS923;
  int32_t _M0L2e2S156;
  int32_t _M0L4len1S158;
  int32_t _M0L4len2S160;
  int32_t _if__result_1838;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS925 = _M0L6lengthS155 * 2;
  _M0L6_2atmpS924 = _M0L13bytes__offsetS154 + _M0L6_2atmpS925;
  _M0L2e1S153 = _M0L6_2atmpS924 - 1;
  _M0L6_2atmpS923 = _M0L11str__offsetS157 + _M0L6lengthS155;
  _M0L2e2S156 = _M0L6_2atmpS923 - 1;
  _M0L4len1S158 = Moonbit_array_length(_M0L4selfS159);
  _M0L4len2S160 = Moonbit_array_length(_M0L3strS161);
  if (_M0L6lengthS155 >= 0) {
    if (_M0L13bytes__offsetS154 >= 0) {
      if (_M0L2e1S153 < _M0L4len1S158) {
        if (_M0L11str__offsetS157 >= 0) {
          _if__result_1838 = _M0L2e2S156 < _M0L4len2S160;
        } else {
          _if__result_1838 = 0;
        }
      } else {
        _if__result_1838 = 0;
      }
    } else {
      _if__result_1838 = 0;
    }
  } else {
    _if__result_1838 = 0;
  }
  if (_if__result_1838) {
    int32_t _M0L16end__str__offsetS162 =
      _M0L11str__offsetS157 + _M0L6lengthS155;
    int32_t _M0L1iS163 = _M0L11str__offsetS157;
    int32_t _M0L1jS164 = _M0L13bytes__offsetS154;
    while (1) {
      if (_M0L1iS163 < _M0L16end__str__offsetS162) {
        int32_t _M0L6_2atmpS920 = _M0L3strS161[_M0L1iS163];
        int32_t _M0L6_2atmpS919 = (int32_t)_M0L6_2atmpS920;
        uint32_t _M0L1cS165 = *(uint32_t*)&_M0L6_2atmpS919;
        uint32_t _M0L6_2atmpS915 = _M0L1cS165 & 255u;
        int32_t _M0L6_2atmpS914;
        int32_t _M0L6_2atmpS916;
        uint32_t _M0L6_2atmpS918;
        int32_t _M0L6_2atmpS917;
        int32_t _M0L6_2atmpS921;
        int32_t _M0L6_2atmpS922;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS914 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS915);
        if (
          _M0L1jS164 < 0 || _M0L1jS164 >= Moonbit_array_length(_M0L4selfS159)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS159[_M0L1jS164] = _M0L6_2atmpS914;
        _M0L6_2atmpS916 = _M0L1jS164 + 1;
        _M0L6_2atmpS918 = _M0L1cS165 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS917 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS918);
        if (
          _M0L6_2atmpS916 < 0
          || _M0L6_2atmpS916 >= Moonbit_array_length(_M0L4selfS159)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS159[_M0L6_2atmpS916] = _M0L6_2atmpS917;
        _M0L6_2atmpS921 = _M0L1iS163 + 1;
        _M0L6_2atmpS922 = _M0L1jS164 + 2;
        _M0L1iS163 = _M0L6_2atmpS921;
        _M0L1jS164 = _M0L6_2atmpS922;
        continue;
      } else {
        moonbit_decref(_M0L3strS161);
        moonbit_decref(_M0L4selfS159);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS161);
    moonbit_decref(_M0L4selfS159);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS99
) {
  int32_t _M0L6_2atmpS913;
  struct _M0TPC16string10StringView _M0L7_2abindS98;
  moonbit_string_t _M0L7_2adataS100;
  int32_t _M0L8_2astartS101;
  int32_t _M0L6_2atmpS912;
  int32_t _M0L6_2aendS102;
  int32_t _M0Lm9_2acursorS103;
  int32_t _M0Lm13accept__stateS104;
  int32_t _M0Lm10match__endS105;
  int32_t _M0Lm20match__tag__saver__0S106;
  int32_t _M0Lm20match__tag__saver__1S107;
  int32_t _M0Lm20match__tag__saver__2S108;
  int32_t _M0Lm20match__tag__saver__3S109;
  int32_t _M0Lm20match__tag__saver__4S110;
  int32_t _M0Lm6tag__0S111;
  int32_t _M0Lm6tag__1S112;
  int32_t _M0Lm9tag__1__1S113;
  int32_t _M0Lm9tag__1__2S114;
  int32_t _M0Lm6tag__3S115;
  int32_t _M0Lm6tag__2S116;
  int32_t _M0Lm9tag__2__1S117;
  int32_t _M0Lm6tag__4S118;
  int32_t _M0L6_2atmpS870;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS913 = Moonbit_array_length(_M0L4reprS99);
  _M0L7_2abindS98
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS913, _M0L4reprS99
  };
  moonbit_incref(_M0L7_2abindS98.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS100 = _M0MPC16string10StringView4data(_M0L7_2abindS98);
  moonbit_incref(_M0L7_2abindS98.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS101
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS98);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS912 = _M0MPC16string10StringView6length(_M0L7_2abindS98);
  _M0L6_2aendS102 = _M0L8_2astartS101 + _M0L6_2atmpS912;
  _M0Lm9_2acursorS103 = _M0L8_2astartS101;
  _M0Lm13accept__stateS104 = -1;
  _M0Lm10match__endS105 = -1;
  _M0Lm20match__tag__saver__0S106 = -1;
  _M0Lm20match__tag__saver__1S107 = -1;
  _M0Lm20match__tag__saver__2S108 = -1;
  _M0Lm20match__tag__saver__3S109 = -1;
  _M0Lm20match__tag__saver__4S110 = -1;
  _M0Lm6tag__0S111 = -1;
  _M0Lm6tag__1S112 = -1;
  _M0Lm9tag__1__1S113 = -1;
  _M0Lm9tag__1__2S114 = -1;
  _M0Lm6tag__3S115 = -1;
  _M0Lm6tag__2S116 = -1;
  _M0Lm9tag__2__1S117 = -1;
  _M0Lm6tag__4S118 = -1;
  _M0L6_2atmpS870 = _M0Lm9_2acursorS103;
  if (_M0L6_2atmpS870 < _M0L6_2aendS102) {
    int32_t _M0L6_2atmpS872 = _M0Lm9_2acursorS103;
    int32_t _M0L6_2atmpS871;
    moonbit_incref(_M0L7_2adataS100);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS871
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS872);
    if (_M0L6_2atmpS871 == 64) {
      int32_t _M0L6_2atmpS873 = _M0Lm9_2acursorS103;
      _M0Lm9_2acursorS103 = _M0L6_2atmpS873 + 1;
      while (1) {
        int32_t _M0L6_2atmpS874;
        _M0Lm6tag__0S111 = _M0Lm9_2acursorS103;
        _M0L6_2atmpS874 = _M0Lm9_2acursorS103;
        if (_M0L6_2atmpS874 < _M0L6_2aendS102) {
          int32_t _M0L6_2atmpS911 = _M0Lm9_2acursorS103;
          int32_t _M0L10next__charS126;
          int32_t _M0L6_2atmpS875;
          moonbit_incref(_M0L7_2adataS100);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS126
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS911);
          _M0L6_2atmpS875 = _M0Lm9_2acursorS103;
          _M0Lm9_2acursorS103 = _M0L6_2atmpS875 + 1;
          if (_M0L10next__charS126 == 58) {
            int32_t _M0L6_2atmpS876 = _M0Lm9_2acursorS103;
            if (_M0L6_2atmpS876 < _M0L6_2aendS102) {
              int32_t _M0L6_2atmpS877 = _M0Lm9_2acursorS103;
              int32_t _M0L12dispatch__15S127;
              _M0Lm9_2acursorS103 = _M0L6_2atmpS877 + 1;
              _M0L12dispatch__15S127 = 0;
              loop__label__15_130:;
              while (1) {
                int32_t _M0L6_2atmpS878;
                switch (_M0L12dispatch__15S127) {
                  case 3: {
                    int32_t _M0L6_2atmpS881;
                    _M0Lm9tag__1__2S114 = _M0Lm9tag__1__1S113;
                    _M0Lm9tag__1__1S113 = _M0Lm6tag__1S112;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS881 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS881 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS886 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS134;
                      int32_t _M0L6_2atmpS882;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS134
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS886);
                      _M0L6_2atmpS882 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS882 + 1;
                      if (_M0L10next__charS134 < 58) {
                        if (_M0L10next__charS134 < 48) {
                          goto join_133;
                        } else {
                          int32_t _M0L6_2atmpS883;
                          _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                          _M0Lm9tag__2__1S117 = _M0Lm6tag__2S116;
                          _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                          _M0Lm6tag__3S115 = _M0Lm9_2acursorS103;
                          _M0L6_2atmpS883 = _M0Lm9_2acursorS103;
                          if (_M0L6_2atmpS883 < _M0L6_2aendS102) {
                            int32_t _M0L6_2atmpS885 = _M0Lm9_2acursorS103;
                            int32_t _M0L10next__charS136;
                            int32_t _M0L6_2atmpS884;
                            moonbit_incref(_M0L7_2adataS100);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS136
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS885);
                            _M0L6_2atmpS884 = _M0Lm9_2acursorS103;
                            _M0Lm9_2acursorS103 = _M0L6_2atmpS884 + 1;
                            if (_M0L10next__charS136 < 48) {
                              if (_M0L10next__charS136 == 45) {
                                goto join_128;
                              } else {
                                goto join_135;
                              }
                            } else if (_M0L10next__charS136 > 57) {
                              if (_M0L10next__charS136 < 59) {
                                _M0L12dispatch__15S127 = 3;
                                goto loop__label__15_130;
                              } else {
                                goto join_135;
                              }
                            } else {
                              _M0L12dispatch__15S127 = 6;
                              goto loop__label__15_130;
                            }
                            join_135:;
                            _M0L12dispatch__15S127 = 0;
                            goto loop__label__15_130;
                          } else {
                            goto join_119;
                          }
                        }
                      } else if (_M0L10next__charS134 > 58) {
                        goto join_133;
                      } else {
                        _M0L12dispatch__15S127 = 1;
                        goto loop__label__15_130;
                      }
                      join_133:;
                      _M0L12dispatch__15S127 = 0;
                      goto loop__label__15_130;
                    } else {
                      goto join_119;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS887;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS887 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS887 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS889 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS138;
                      int32_t _M0L6_2atmpS888;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS138
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS889);
                      _M0L6_2atmpS888 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS888 + 1;
                      if (_M0L10next__charS138 < 58) {
                        if (_M0L10next__charS138 < 48) {
                          goto join_137;
                        } else {
                          _M0L12dispatch__15S127 = 2;
                          goto loop__label__15_130;
                        }
                      } else if (_M0L10next__charS138 > 58) {
                        goto join_137;
                      } else {
                        _M0L12dispatch__15S127 = 3;
                        goto loop__label__15_130;
                      }
                      join_137:;
                      _M0L12dispatch__15S127 = 0;
                      goto loop__label__15_130;
                    } else {
                      goto join_119;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS890;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS890 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS890 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS892 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS139;
                      int32_t _M0L6_2atmpS891;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS139
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS892);
                      _M0L6_2atmpS891 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS891 + 1;
                      if (_M0L10next__charS139 == 58) {
                        _M0L12dispatch__15S127 = 1;
                        goto loop__label__15_130;
                      } else {
                        _M0L12dispatch__15S127 = 0;
                        goto loop__label__15_130;
                      }
                    } else {
                      goto join_119;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS893;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__4S118 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS893 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS893 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS901 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS141;
                      int32_t _M0L6_2atmpS894;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS141
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS901);
                      _M0L6_2atmpS894 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS894 + 1;
                      if (_M0L10next__charS141 < 58) {
                        if (_M0L10next__charS141 < 48) {
                          goto join_140;
                        } else {
                          _M0L12dispatch__15S127 = 4;
                          goto loop__label__15_130;
                        }
                      } else if (_M0L10next__charS141 > 58) {
                        goto join_140;
                      } else {
                        int32_t _M0L6_2atmpS895;
                        _M0Lm9tag__1__2S114 = _M0Lm9tag__1__1S113;
                        _M0Lm9tag__1__1S113 = _M0Lm6tag__1S112;
                        _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                        _M0L6_2atmpS895 = _M0Lm9_2acursorS103;
                        if (_M0L6_2atmpS895 < _M0L6_2aendS102) {
                          int32_t _M0L6_2atmpS900 = _M0Lm9_2acursorS103;
                          int32_t _M0L10next__charS143;
                          int32_t _M0L6_2atmpS896;
                          moonbit_incref(_M0L7_2adataS100);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS143
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS900);
                          _M0L6_2atmpS896 = _M0Lm9_2acursorS103;
                          _M0Lm9_2acursorS103 = _M0L6_2atmpS896 + 1;
                          if (_M0L10next__charS143 < 58) {
                            if (_M0L10next__charS143 < 48) {
                              goto join_142;
                            } else {
                              int32_t _M0L6_2atmpS897;
                              _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                              _M0Lm9tag__2__1S117 = _M0Lm6tag__2S116;
                              _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                              _M0L6_2atmpS897 = _M0Lm9_2acursorS103;
                              if (_M0L6_2atmpS897 < _M0L6_2aendS102) {
                                int32_t _M0L6_2atmpS899 = _M0Lm9_2acursorS103;
                                int32_t _M0L10next__charS145;
                                int32_t _M0L6_2atmpS898;
                                moonbit_incref(_M0L7_2adataS100);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS145
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS899);
                                _M0L6_2atmpS898 = _M0Lm9_2acursorS103;
                                _M0Lm9_2acursorS103 = _M0L6_2atmpS898 + 1;
                                if (_M0L10next__charS145 < 58) {
                                  if (_M0L10next__charS145 < 48) {
                                    goto join_144;
                                  } else {
                                    _M0L12dispatch__15S127 = 5;
                                    goto loop__label__15_130;
                                  }
                                } else if (_M0L10next__charS145 > 58) {
                                  goto join_144;
                                } else {
                                  _M0L12dispatch__15S127 = 3;
                                  goto loop__label__15_130;
                                }
                                join_144:;
                                _M0L12dispatch__15S127 = 0;
                                goto loop__label__15_130;
                              } else {
                                goto join_132;
                              }
                            }
                          } else if (_M0L10next__charS143 > 58) {
                            goto join_142;
                          } else {
                            _M0L12dispatch__15S127 = 1;
                            goto loop__label__15_130;
                          }
                          join_142:;
                          _M0L12dispatch__15S127 = 0;
                          goto loop__label__15_130;
                        } else {
                          goto join_119;
                        }
                      }
                      join_140:;
                      _M0L12dispatch__15S127 = 0;
                      goto loop__label__15_130;
                    } else {
                      goto join_119;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS902;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS902 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS902 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS904 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS147;
                      int32_t _M0L6_2atmpS903;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS147
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS904);
                      _M0L6_2atmpS903 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS903 + 1;
                      if (_M0L10next__charS147 < 58) {
                        if (_M0L10next__charS147 < 48) {
                          goto join_146;
                        } else {
                          _M0L12dispatch__15S127 = 5;
                          goto loop__label__15_130;
                        }
                      } else if (_M0L10next__charS147 > 58) {
                        goto join_146;
                      } else {
                        _M0L12dispatch__15S127 = 3;
                        goto loop__label__15_130;
                      }
                      join_146:;
                      _M0L12dispatch__15S127 = 0;
                      goto loop__label__15_130;
                    } else {
                      goto join_132;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS905;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__3S115 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS905 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS905 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS907 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS149;
                      int32_t _M0L6_2atmpS906;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS149
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS907);
                      _M0L6_2atmpS906 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS906 + 1;
                      if (_M0L10next__charS149 < 48) {
                        if (_M0L10next__charS149 == 45) {
                          goto join_128;
                        } else {
                          goto join_148;
                        }
                      } else if (_M0L10next__charS149 > 57) {
                        if (_M0L10next__charS149 < 59) {
                          _M0L12dispatch__15S127 = 3;
                          goto loop__label__15_130;
                        } else {
                          goto join_148;
                        }
                      } else {
                        _M0L12dispatch__15S127 = 6;
                        goto loop__label__15_130;
                      }
                      join_148:;
                      _M0L12dispatch__15S127 = 0;
                      goto loop__label__15_130;
                    } else {
                      goto join_119;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS908;
                    _M0Lm9tag__1__1S113 = _M0Lm6tag__1S112;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS908 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS908 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS910 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS151;
                      int32_t _M0L6_2atmpS909;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS151
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS910);
                      _M0L6_2atmpS909 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS909 + 1;
                      if (_M0L10next__charS151 < 58) {
                        if (_M0L10next__charS151 < 48) {
                          goto join_150;
                        } else {
                          _M0L12dispatch__15S127 = 2;
                          goto loop__label__15_130;
                        }
                      } else if (_M0L10next__charS151 > 58) {
                        goto join_150;
                      } else {
                        _M0L12dispatch__15S127 = 1;
                        goto loop__label__15_130;
                      }
                      join_150:;
                      _M0L12dispatch__15S127 = 0;
                      goto loop__label__15_130;
                    } else {
                      goto join_119;
                    }
                    break;
                  }
                  default: {
                    goto join_119;
                    break;
                  }
                }
                join_132:;
                _M0Lm6tag__1S112 = _M0Lm9tag__1__2S114;
                _M0Lm6tag__2S116 = _M0Lm9tag__2__1S117;
                _M0Lm20match__tag__saver__0S106 = _M0Lm6tag__0S111;
                _M0Lm20match__tag__saver__1S107 = _M0Lm6tag__1S112;
                _M0Lm20match__tag__saver__2S108 = _M0Lm6tag__2S116;
                _M0Lm20match__tag__saver__3S109 = _M0Lm6tag__3S115;
                _M0Lm20match__tag__saver__4S110 = _M0Lm6tag__4S118;
                _M0Lm13accept__stateS104 = 0;
                _M0Lm10match__endS105 = _M0Lm9_2acursorS103;
                goto join_119;
                join_128:;
                _M0Lm9tag__1__1S113 = _M0Lm9tag__1__2S114;
                _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                _M0Lm6tag__2S116 = _M0Lm9tag__2__1S117;
                _M0L6_2atmpS878 = _M0Lm9_2acursorS103;
                if (_M0L6_2atmpS878 < _M0L6_2aendS102) {
                  int32_t _M0L6_2atmpS880 = _M0Lm9_2acursorS103;
                  int32_t _M0L10next__charS131;
                  int32_t _M0L6_2atmpS879;
                  moonbit_incref(_M0L7_2adataS100);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS131
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS880);
                  _M0L6_2atmpS879 = _M0Lm9_2acursorS103;
                  _M0Lm9_2acursorS103 = _M0L6_2atmpS879 + 1;
                  if (_M0L10next__charS131 < 58) {
                    if (_M0L10next__charS131 < 48) {
                      goto join_129;
                    } else {
                      _M0L12dispatch__15S127 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS131 > 58) {
                    goto join_129;
                  } else {
                    _M0L12dispatch__15S127 = 1;
                    continue;
                  }
                  join_129:;
                  _M0L12dispatch__15S127 = 0;
                  continue;
                } else {
                  goto join_119;
                }
                break;
              }
            } else {
              goto join_119;
            }
          } else {
            continue;
          }
        } else {
          goto join_119;
        }
        break;
      }
    } else {
      goto join_119;
    }
  } else {
    goto join_119;
  }
  join_119:;
  switch (_M0Lm13accept__stateS104) {
    case 0: {
      int32_t _M0L6_2atmpS869 = _M0Lm20match__tag__saver__1S107;
      int32_t _M0L6_2atmpS868 = _M0L6_2atmpS869 + 1;
      int64_t _M0L6_2atmpS865 = (int64_t)_M0L6_2atmpS868;
      int32_t _M0L6_2atmpS867 = _M0Lm20match__tag__saver__2S108;
      int64_t _M0L6_2atmpS866 = (int64_t)_M0L6_2atmpS867;
      struct _M0TPC16string10StringView _M0L11start__lineS120;
      int32_t _M0L6_2atmpS864;
      int32_t _M0L6_2atmpS863;
      int64_t _M0L6_2atmpS860;
      int32_t _M0L6_2atmpS862;
      int64_t _M0L6_2atmpS861;
      struct _M0TPC16string10StringView _M0L13start__columnS121;
      int32_t _M0L6_2atmpS859;
      int64_t _M0L6_2atmpS856;
      int32_t _M0L6_2atmpS858;
      int64_t _M0L6_2atmpS857;
      struct _M0TPC16string10StringView _M0L3pkgS122;
      int32_t _M0L6_2atmpS855;
      int32_t _M0L6_2atmpS854;
      int64_t _M0L6_2atmpS851;
      int32_t _M0L6_2atmpS853;
      int64_t _M0L6_2atmpS852;
      struct _M0TPC16string10StringView _M0L8filenameS123;
      int32_t _M0L6_2atmpS850;
      int32_t _M0L6_2atmpS849;
      int64_t _M0L6_2atmpS846;
      int32_t _M0L6_2atmpS848;
      int64_t _M0L6_2atmpS847;
      struct _M0TPC16string10StringView _M0L9end__lineS124;
      int32_t _M0L6_2atmpS845;
      int32_t _M0L6_2atmpS844;
      int64_t _M0L6_2atmpS841;
      int32_t _M0L6_2atmpS843;
      int64_t _M0L6_2atmpS842;
      struct _M0TPC16string10StringView _M0L11end__columnS125;
      struct _M0TPB13SourceLocRepr* _block_1855;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS120
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS865, _M0L6_2atmpS866);
      _M0L6_2atmpS864 = _M0Lm20match__tag__saver__2S108;
      _M0L6_2atmpS863 = _M0L6_2atmpS864 + 1;
      _M0L6_2atmpS860 = (int64_t)_M0L6_2atmpS863;
      _M0L6_2atmpS862 = _M0Lm20match__tag__saver__3S109;
      _M0L6_2atmpS861 = (int64_t)_M0L6_2atmpS862;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS121
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS860, _M0L6_2atmpS861);
      _M0L6_2atmpS859 = _M0L8_2astartS101 + 1;
      _M0L6_2atmpS856 = (int64_t)_M0L6_2atmpS859;
      _M0L6_2atmpS858 = _M0Lm20match__tag__saver__0S106;
      _M0L6_2atmpS857 = (int64_t)_M0L6_2atmpS858;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS122
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS856, _M0L6_2atmpS857);
      _M0L6_2atmpS855 = _M0Lm20match__tag__saver__0S106;
      _M0L6_2atmpS854 = _M0L6_2atmpS855 + 1;
      _M0L6_2atmpS851 = (int64_t)_M0L6_2atmpS854;
      _M0L6_2atmpS853 = _M0Lm20match__tag__saver__1S107;
      _M0L6_2atmpS852 = (int64_t)_M0L6_2atmpS853;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS123
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS851, _M0L6_2atmpS852);
      _M0L6_2atmpS850 = _M0Lm20match__tag__saver__3S109;
      _M0L6_2atmpS849 = _M0L6_2atmpS850 + 1;
      _M0L6_2atmpS846 = (int64_t)_M0L6_2atmpS849;
      _M0L6_2atmpS848 = _M0Lm20match__tag__saver__4S110;
      _M0L6_2atmpS847 = (int64_t)_M0L6_2atmpS848;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS124
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS846, _M0L6_2atmpS847);
      _M0L6_2atmpS845 = _M0Lm20match__tag__saver__4S110;
      _M0L6_2atmpS844 = _M0L6_2atmpS845 + 1;
      _M0L6_2atmpS841 = (int64_t)_M0L6_2atmpS844;
      _M0L6_2atmpS843 = _M0Lm10match__endS105;
      _M0L6_2atmpS842 = (int64_t)_M0L6_2atmpS843;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS125
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS841, _M0L6_2atmpS842);
      _block_1855
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_1855)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_1855->$0_0 = _M0L3pkgS122.$0;
      _block_1855->$0_1 = _M0L3pkgS122.$1;
      _block_1855->$0_2 = _M0L3pkgS122.$2;
      _block_1855->$1_0 = _M0L8filenameS123.$0;
      _block_1855->$1_1 = _M0L8filenameS123.$1;
      _block_1855->$1_2 = _M0L8filenameS123.$2;
      _block_1855->$2_0 = _M0L11start__lineS120.$0;
      _block_1855->$2_1 = _M0L11start__lineS120.$1;
      _block_1855->$2_2 = _M0L11start__lineS120.$2;
      _block_1855->$3_0 = _M0L13start__columnS121.$0;
      _block_1855->$3_1 = _M0L13start__columnS121.$1;
      _block_1855->$3_2 = _M0L13start__columnS121.$2;
      _block_1855->$4_0 = _M0L9end__lineS124.$0;
      _block_1855->$4_1 = _M0L9end__lineS124.$1;
      _block_1855->$4_2 = _M0L9end__lineS124.$2;
      _block_1855->$5_0 = _M0L11end__columnS125.$0;
      _block_1855->$5_1 = _M0L11end__columnS125.$1;
      _block_1855->$5_2 = _M0L11end__columnS125.$2;
      return _block_1855;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS100);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS96,
  int32_t _M0L5indexS97
) {
  int32_t _M0L3lenS95;
  int32_t _if__result_1856;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS95 = _M0L4selfS96->$1;
  if (_M0L5indexS97 >= 0) {
    _if__result_1856 = _M0L5indexS97 < _M0L3lenS95;
  } else {
    _if__result_1856 = 0;
  }
  if (_if__result_1856) {
    moonbit_string_t* _M0L6_2atmpS840;
    moonbit_string_t _M0L6_2atmpS1665;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS840 = _M0MPC15array5Array6bufferGsE(_M0L4selfS96);
    if (
      _M0L5indexS97 < 0
      || _M0L5indexS97 >= Moonbit_array_length(_M0L6_2atmpS840)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1665 = (moonbit_string_t)_M0L6_2atmpS840[_M0L5indexS97];
    moonbit_incref(_M0L6_2atmpS1665);
    moonbit_decref(_M0L6_2atmpS840);
    return _M0L6_2atmpS1665;
  } else {
    moonbit_decref(_M0L4selfS96);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS93
) {
  moonbit_string_t* _M0L8_2afieldS1666;
  int32_t _M0L6_2acntS1748;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS1666 = _M0L4selfS93->$0;
  _M0L6_2acntS1748 = Moonbit_object_header(_M0L4selfS93)->rc;
  if (_M0L6_2acntS1748 > 1) {
    int32_t _M0L11_2anew__cntS1749 = _M0L6_2acntS1748 - 1;
    Moonbit_object_header(_M0L4selfS93)->rc = _M0L11_2anew__cntS1749;
    moonbit_incref(_M0L8_2afieldS1666);
  } else if (_M0L6_2acntS1748 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS93);
  }
  return _M0L8_2afieldS1666;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS94
) {
  struct _M0TUsiE** _M0L8_2afieldS1667;
  int32_t _M0L6_2acntS1750;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS1667 = _M0L4selfS94->$0;
  _M0L6_2acntS1750 = Moonbit_object_header(_M0L4selfS94)->rc;
  if (_M0L6_2acntS1750 > 1) {
    int32_t _M0L11_2anew__cntS1751 = _M0L6_2acntS1750 - 1;
    Moonbit_object_header(_M0L4selfS94)->rc = _M0L11_2anew__cntS1751;
    moonbit_incref(_M0L8_2afieldS1667);
  } else if (_M0L6_2acntS1750 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS94);
  }
  return _M0L8_2afieldS1667;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS92) {
  struct _M0TPB13StringBuilder* _M0L3bufS91;
  struct _M0TPB6Logger _M0L6_2atmpS839;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS91 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS91);
  _M0L6_2atmpS839
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS91
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS92, _M0L6_2atmpS839);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS91);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS90) {
  int32_t _M0L6_2atmpS838;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS838 = (int32_t)_M0L4selfS90;
  return _M0L6_2atmpS838;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS89) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS89 >= 56320) {
    return _M0L4selfS89 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS88) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS88 >= 55296) {
    return _M0L4selfS88 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS85,
  int32_t _M0L2chS87
) {
  int32_t _M0L3lenS833;
  int32_t _M0L6_2atmpS832;
  moonbit_bytes_t _M0L8_2afieldS1668;
  moonbit_bytes_t _M0L4dataS836;
  int32_t _M0L3lenS837;
  int32_t _M0L3incS86;
  int32_t _M0L3lenS835;
  int32_t _M0L6_2atmpS834;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS833 = _M0L4selfS85->$1;
  _M0L6_2atmpS832 = _M0L3lenS833 + 4;
  moonbit_incref(_M0L4selfS85);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS85, _M0L6_2atmpS832);
  _M0L8_2afieldS1668 = _M0L4selfS85->$0;
  _M0L4dataS836 = _M0L8_2afieldS1668;
  _M0L3lenS837 = _M0L4selfS85->$1;
  moonbit_incref(_M0L4dataS836);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS86
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS836, _M0L3lenS837, _M0L2chS87);
  _M0L3lenS835 = _M0L4selfS85->$1;
  _M0L6_2atmpS834 = _M0L3lenS835 + _M0L3incS86;
  _M0L4selfS85->$1 = _M0L6_2atmpS834;
  moonbit_decref(_M0L4selfS85);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS80,
  int32_t _M0L8requiredS81
) {
  moonbit_bytes_t _M0L8_2afieldS1672;
  moonbit_bytes_t _M0L4dataS831;
  int32_t _M0L6_2atmpS1671;
  int32_t _M0L12current__lenS79;
  int32_t _M0Lm13enough__spaceS82;
  int32_t _M0L6_2atmpS829;
  int32_t _M0L6_2atmpS830;
  moonbit_bytes_t _M0L9new__dataS84;
  moonbit_bytes_t _M0L8_2afieldS1670;
  moonbit_bytes_t _M0L4dataS827;
  int32_t _M0L3lenS828;
  moonbit_bytes_t _M0L6_2aoldS1669;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS1672 = _M0L4selfS80->$0;
  _M0L4dataS831 = _M0L8_2afieldS1672;
  _M0L6_2atmpS1671 = Moonbit_array_length(_M0L4dataS831);
  _M0L12current__lenS79 = _M0L6_2atmpS1671;
  if (_M0L8requiredS81 <= _M0L12current__lenS79) {
    moonbit_decref(_M0L4selfS80);
    return 0;
  }
  _M0Lm13enough__spaceS82 = _M0L12current__lenS79;
  while (1) {
    int32_t _M0L6_2atmpS825 = _M0Lm13enough__spaceS82;
    if (_M0L6_2atmpS825 < _M0L8requiredS81) {
      int32_t _M0L6_2atmpS826 = _M0Lm13enough__spaceS82;
      _M0Lm13enough__spaceS82 = _M0L6_2atmpS826 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS829 = _M0Lm13enough__spaceS82;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS830 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS84
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS829, _M0L6_2atmpS830);
  _M0L8_2afieldS1670 = _M0L4selfS80->$0;
  _M0L4dataS827 = _M0L8_2afieldS1670;
  _M0L3lenS828 = _M0L4selfS80->$1;
  moonbit_incref(_M0L4dataS827);
  moonbit_incref(_M0L9new__dataS84);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS84, 0, _M0L4dataS827, 0, _M0L3lenS828);
  _M0L6_2aoldS1669 = _M0L4selfS80->$0;
  moonbit_decref(_M0L6_2aoldS1669);
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
    uint32_t _M0L6_2atmpS808 = _M0L4codeS72 & 255u;
    int32_t _M0L6_2atmpS807;
    int32_t _M0L6_2atmpS809;
    uint32_t _M0L6_2atmpS811;
    int32_t _M0L6_2atmpS810;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS807 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS808);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS807;
    _M0L6_2atmpS809 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS811 = _M0L4codeS72 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS810 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS811);
    if (
      _M0L6_2atmpS809 < 0
      || _M0L6_2atmpS809 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS809] = _M0L6_2atmpS810;
    moonbit_decref(_M0L4selfS74);
    return 2;
  } else if (_M0L4codeS72 < 1114112u) {
    uint32_t _M0L2hiS76 = _M0L4codeS72 - 65536u;
    uint32_t _M0L6_2atmpS824 = _M0L2hiS76 >> 10;
    uint32_t _M0L2loS77 = _M0L6_2atmpS824 | 55296u;
    uint32_t _M0L6_2atmpS823 = _M0L2hiS76 & 1023u;
    uint32_t _M0L2hiS78 = _M0L6_2atmpS823 | 56320u;
    uint32_t _M0L6_2atmpS813 = _M0L2loS77 & 255u;
    int32_t _M0L6_2atmpS812;
    int32_t _M0L6_2atmpS814;
    uint32_t _M0L6_2atmpS816;
    int32_t _M0L6_2atmpS815;
    int32_t _M0L6_2atmpS817;
    uint32_t _M0L6_2atmpS819;
    int32_t _M0L6_2atmpS818;
    int32_t _M0L6_2atmpS820;
    uint32_t _M0L6_2atmpS822;
    int32_t _M0L6_2atmpS821;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS812 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS813);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS812;
    _M0L6_2atmpS814 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS816 = _M0L2loS77 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS815 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS816);
    if (
      _M0L6_2atmpS814 < 0
      || _M0L6_2atmpS814 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS814] = _M0L6_2atmpS815;
    _M0L6_2atmpS817 = _M0L6offsetS75 + 2;
    _M0L6_2atmpS819 = _M0L2hiS78 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS818 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS819);
    if (
      _M0L6_2atmpS817 < 0
      || _M0L6_2atmpS817 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS817] = _M0L6_2atmpS818;
    _M0L6_2atmpS820 = _M0L6offsetS75 + 3;
    _M0L6_2atmpS822 = _M0L2hiS78 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS821 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS822);
    if (
      _M0L6_2atmpS820 < 0
      || _M0L6_2atmpS820 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS820] = _M0L6_2atmpS821;
    moonbit_decref(_M0L4selfS74);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS74);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_24.data, (moonbit_string_t)moonbit_string_literal_25.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS71) {
  int32_t _M0L6_2atmpS806;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS806 = *(int32_t*)&_M0L4selfS71;
  return _M0L6_2atmpS806 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS805;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS805 = _M0L4selfS70;
  return *(uint32_t*)&_M0L6_2atmpS805;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS69
) {
  moonbit_bytes_t _M0L8_2afieldS1674;
  moonbit_bytes_t _M0L4dataS804;
  moonbit_bytes_t _M0L6_2atmpS801;
  int32_t _M0L8_2afieldS1673;
  int32_t _M0L3lenS803;
  int64_t _M0L6_2atmpS802;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS1674 = _M0L4selfS69->$0;
  _M0L4dataS804 = _M0L8_2afieldS1674;
  moonbit_incref(_M0L4dataS804);
  _M0L6_2atmpS801 = _M0L4dataS804;
  _M0L8_2afieldS1673 = _M0L4selfS69->$1;
  moonbit_decref(_M0L4selfS69);
  _M0L3lenS803 = _M0L8_2afieldS1673;
  _M0L6_2atmpS802 = (int64_t)_M0L3lenS803;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS801, 0, _M0L6_2atmpS802);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS64,
  int32_t _M0L6offsetS68,
  int64_t _M0L6lengthS66
) {
  int32_t _M0L3lenS63;
  int32_t _M0L6lengthS65;
  int32_t _if__result_1858;
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
      int32_t _M0L6_2atmpS800 = _M0L6offsetS68 + _M0L6lengthS65;
      _if__result_1858 = _M0L6_2atmpS800 <= _M0L3lenS63;
    } else {
      _if__result_1858 = 0;
    }
  } else {
    _if__result_1858 = 0;
  }
  if (_if__result_1858) {
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
  struct _M0TPB13StringBuilder* _block_1859;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS61 < 1) {
    _M0L7initialS60 = 1;
  } else {
    _M0L7initialS60 = _M0L10size__hintS61;
  }
  _M0L4dataS62 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS60, 0);
  _block_1859
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_1859)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_1859->$0 = _M0L4dataS62;
  _block_1859->$1 = 0;
  return _block_1859;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS799;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS799 = (int32_t)_M0L4selfS59;
  return _M0L6_2atmpS799;
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
  int32_t _if__result_1860;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS22 == _M0L3srcS23) {
    _if__result_1860 = _M0L11dst__offsetS24 < _M0L11src__offsetS25;
  } else {
    _if__result_1860 = 0;
  }
  if (_if__result_1860) {
    int32_t _M0L1iS26 = 0;
    while (1) {
      if (_M0L1iS26 < _M0L3lenS27) {
        int32_t _M0L6_2atmpS772 = _M0L11dst__offsetS24 + _M0L1iS26;
        int32_t _M0L6_2atmpS774 = _M0L11src__offsetS25 + _M0L1iS26;
        int32_t _M0L6_2atmpS773;
        int32_t _M0L6_2atmpS775;
        if (
          _M0L6_2atmpS774 < 0
          || _M0L6_2atmpS774 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS773 = (int32_t)_M0L3srcS23[_M0L6_2atmpS774];
        if (
          _M0L6_2atmpS772 < 0
          || _M0L6_2atmpS772 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS772] = _M0L6_2atmpS773;
        _M0L6_2atmpS775 = _M0L1iS26 + 1;
        _M0L1iS26 = _M0L6_2atmpS775;
        continue;
      } else {
        moonbit_decref(_M0L3srcS23);
        moonbit_decref(_M0L3dstS22);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS780 = _M0L3lenS27 - 1;
    int32_t _M0L1iS29 = _M0L6_2atmpS780;
    while (1) {
      if (_M0L1iS29 >= 0) {
        int32_t _M0L6_2atmpS776 = _M0L11dst__offsetS24 + _M0L1iS29;
        int32_t _M0L6_2atmpS778 = _M0L11src__offsetS25 + _M0L1iS29;
        int32_t _M0L6_2atmpS777;
        int32_t _M0L6_2atmpS779;
        if (
          _M0L6_2atmpS778 < 0
          || _M0L6_2atmpS778 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS777 = (int32_t)_M0L3srcS23[_M0L6_2atmpS778];
        if (
          _M0L6_2atmpS776 < 0
          || _M0L6_2atmpS776 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS776] = _M0L6_2atmpS777;
        _M0L6_2atmpS779 = _M0L1iS29 - 1;
        _M0L1iS29 = _M0L6_2atmpS779;
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
  int32_t _if__result_1863;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS31 == _M0L3srcS32) {
    _if__result_1863 = _M0L11dst__offsetS33 < _M0L11src__offsetS34;
  } else {
    _if__result_1863 = 0;
  }
  if (_if__result_1863) {
    int32_t _M0L1iS35 = 0;
    while (1) {
      if (_M0L1iS35 < _M0L3lenS36) {
        int32_t _M0L6_2atmpS781 = _M0L11dst__offsetS33 + _M0L1iS35;
        int32_t _M0L6_2atmpS783 = _M0L11src__offsetS34 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS1676;
        moonbit_string_t _M0L6_2atmpS782;
        moonbit_string_t _M0L6_2aoldS1675;
        int32_t _M0L6_2atmpS784;
        if (
          _M0L6_2atmpS783 < 0
          || _M0L6_2atmpS783 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1676 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS783];
        _M0L6_2atmpS782 = _M0L6_2atmpS1676;
        if (
          _M0L6_2atmpS781 < 0
          || _M0L6_2atmpS781 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1675 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS781];
        moonbit_incref(_M0L6_2atmpS782);
        moonbit_decref(_M0L6_2aoldS1675);
        _M0L3dstS31[_M0L6_2atmpS781] = _M0L6_2atmpS782;
        _M0L6_2atmpS784 = _M0L1iS35 + 1;
        _M0L1iS35 = _M0L6_2atmpS784;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS789 = _M0L3lenS36 - 1;
    int32_t _M0L1iS38 = _M0L6_2atmpS789;
    while (1) {
      if (_M0L1iS38 >= 0) {
        int32_t _M0L6_2atmpS785 = _M0L11dst__offsetS33 + _M0L1iS38;
        int32_t _M0L6_2atmpS787 = _M0L11src__offsetS34 + _M0L1iS38;
        moonbit_string_t _M0L6_2atmpS1678;
        moonbit_string_t _M0L6_2atmpS786;
        moonbit_string_t _M0L6_2aoldS1677;
        int32_t _M0L6_2atmpS788;
        if (
          _M0L6_2atmpS787 < 0
          || _M0L6_2atmpS787 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1678 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS787];
        _M0L6_2atmpS786 = _M0L6_2atmpS1678;
        if (
          _M0L6_2atmpS785 < 0
          || _M0L6_2atmpS785 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1677 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS785];
        moonbit_incref(_M0L6_2atmpS786);
        moonbit_decref(_M0L6_2aoldS1677);
        _M0L3dstS31[_M0L6_2atmpS785] = _M0L6_2atmpS786;
        _M0L6_2atmpS788 = _M0L1iS38 - 1;
        _M0L1iS38 = _M0L6_2atmpS788;
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
  int32_t _if__result_1866;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS40 == _M0L3srcS41) {
    _if__result_1866 = _M0L11dst__offsetS42 < _M0L11src__offsetS43;
  } else {
    _if__result_1866 = 0;
  }
  if (_if__result_1866) {
    int32_t _M0L1iS44 = 0;
    while (1) {
      if (_M0L1iS44 < _M0L3lenS45) {
        int32_t _M0L6_2atmpS790 = _M0L11dst__offsetS42 + _M0L1iS44;
        int32_t _M0L6_2atmpS792 = _M0L11src__offsetS43 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS1680;
        struct _M0TUsiE* _M0L6_2atmpS791;
        struct _M0TUsiE* _M0L6_2aoldS1679;
        int32_t _M0L6_2atmpS793;
        if (
          _M0L6_2atmpS792 < 0
          || _M0L6_2atmpS792 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1680 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS792];
        _M0L6_2atmpS791 = _M0L6_2atmpS1680;
        if (
          _M0L6_2atmpS790 < 0
          || _M0L6_2atmpS790 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1679 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS790];
        if (_M0L6_2atmpS791) {
          moonbit_incref(_M0L6_2atmpS791);
        }
        if (_M0L6_2aoldS1679) {
          moonbit_decref(_M0L6_2aoldS1679);
        }
        _M0L3dstS40[_M0L6_2atmpS790] = _M0L6_2atmpS791;
        _M0L6_2atmpS793 = _M0L1iS44 + 1;
        _M0L1iS44 = _M0L6_2atmpS793;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS798 = _M0L3lenS45 - 1;
    int32_t _M0L1iS47 = _M0L6_2atmpS798;
    while (1) {
      if (_M0L1iS47 >= 0) {
        int32_t _M0L6_2atmpS794 = _M0L11dst__offsetS42 + _M0L1iS47;
        int32_t _M0L6_2atmpS796 = _M0L11src__offsetS43 + _M0L1iS47;
        struct _M0TUsiE* _M0L6_2atmpS1682;
        struct _M0TUsiE* _M0L6_2atmpS795;
        struct _M0TUsiE* _M0L6_2aoldS1681;
        int32_t _M0L6_2atmpS797;
        if (
          _M0L6_2atmpS796 < 0
          || _M0L6_2atmpS796 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1682 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS796];
        _M0L6_2atmpS795 = _M0L6_2atmpS1682;
        if (
          _M0L6_2atmpS794 < 0
          || _M0L6_2atmpS794 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1681 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS794];
        if (_M0L6_2atmpS795) {
          moonbit_incref(_M0L6_2atmpS795);
        }
        if (_M0L6_2aoldS1681) {
          moonbit_decref(_M0L6_2aoldS1681);
        }
        _M0L3dstS40[_M0L6_2atmpS794] = _M0L6_2atmpS795;
        _M0L6_2atmpS797 = _M0L1iS47 - 1;
        _M0L1iS47 = _M0L6_2atmpS797;
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
  moonbit_string_t _M0L6_2atmpS761;
  moonbit_string_t _M0L6_2atmpS1685;
  moonbit_string_t _M0L6_2atmpS759;
  moonbit_string_t _M0L6_2atmpS760;
  moonbit_string_t _M0L6_2atmpS1684;
  moonbit_string_t _M0L6_2atmpS758;
  moonbit_string_t _M0L6_2atmpS1683;
  moonbit_string_t _M0L6_2atmpS757;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS761 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1685
  = moonbit_add_string(_M0L6_2atmpS761, (moonbit_string_t)moonbit_string_literal_26.data);
  moonbit_decref(_M0L6_2atmpS761);
  _M0L6_2atmpS759 = _M0L6_2atmpS1685;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS760
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1684 = moonbit_add_string(_M0L6_2atmpS759, _M0L6_2atmpS760);
  moonbit_decref(_M0L6_2atmpS759);
  moonbit_decref(_M0L6_2atmpS760);
  _M0L6_2atmpS758 = _M0L6_2atmpS1684;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1683
  = moonbit_add_string(_M0L6_2atmpS758, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS758);
  _M0L6_2atmpS757 = _M0L6_2atmpS1683;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS757);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS18,
  moonbit_string_t _M0L3locS19
) {
  moonbit_string_t _M0L6_2atmpS766;
  moonbit_string_t _M0L6_2atmpS1688;
  moonbit_string_t _M0L6_2atmpS764;
  moonbit_string_t _M0L6_2atmpS765;
  moonbit_string_t _M0L6_2atmpS1687;
  moonbit_string_t _M0L6_2atmpS763;
  moonbit_string_t _M0L6_2atmpS1686;
  moonbit_string_t _M0L6_2atmpS762;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS766 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1688
  = moonbit_add_string(_M0L6_2atmpS766, (moonbit_string_t)moonbit_string_literal_26.data);
  moonbit_decref(_M0L6_2atmpS766);
  _M0L6_2atmpS764 = _M0L6_2atmpS1688;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS765
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1687 = moonbit_add_string(_M0L6_2atmpS764, _M0L6_2atmpS765);
  moonbit_decref(_M0L6_2atmpS764);
  moonbit_decref(_M0L6_2atmpS765);
  _M0L6_2atmpS763 = _M0L6_2atmpS1687;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1686
  = moonbit_add_string(_M0L6_2atmpS763, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS763);
  _M0L6_2atmpS762 = _M0L6_2atmpS1686;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS762);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS771;
  moonbit_string_t _M0L6_2atmpS1691;
  moonbit_string_t _M0L6_2atmpS769;
  moonbit_string_t _M0L6_2atmpS770;
  moonbit_string_t _M0L6_2atmpS1690;
  moonbit_string_t _M0L6_2atmpS768;
  moonbit_string_t _M0L6_2atmpS1689;
  moonbit_string_t _M0L6_2atmpS767;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS771 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1691
  = moonbit_add_string(_M0L6_2atmpS771, (moonbit_string_t)moonbit_string_literal_26.data);
  moonbit_decref(_M0L6_2atmpS771);
  _M0L6_2atmpS769 = _M0L6_2atmpS1691;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS770
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1690 = moonbit_add_string(_M0L6_2atmpS769, _M0L6_2atmpS770);
  moonbit_decref(_M0L6_2atmpS769);
  moonbit_decref(_M0L6_2atmpS770);
  _M0L6_2atmpS768 = _M0L6_2atmpS1690;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1689
  = moonbit_add_string(_M0L6_2atmpS768, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS768);
  _M0L6_2atmpS767 = _M0L6_2atmpS1689;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS767);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5valueS15
) {
  uint32_t _M0L3accS756;
  uint32_t _M0L6_2atmpS755;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS756 = _M0L4selfS14->$0;
  _M0L6_2atmpS755 = _M0L3accS756 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS755;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS753;
  uint32_t _M0L6_2atmpS754;
  uint32_t _M0L6_2atmpS752;
  uint32_t _M0L6_2atmpS751;
  uint32_t _M0L6_2atmpS750;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS753 = _M0L4selfS12->$0;
  _M0L6_2atmpS754 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS752 = _M0L3accS753 + _M0L6_2atmpS754;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS751 = _M0FPB4rotl(_M0L6_2atmpS752, 17);
  _M0L6_2atmpS750 = _M0L6_2atmpS751 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS750;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS747;
  int32_t _M0L6_2atmpS749;
  uint32_t _M0L6_2atmpS748;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS747 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS749 = 32 - _M0L1rS11;
  _M0L6_2atmpS748 = _M0L1xS10 >> (_M0L6_2atmpS749 & 31);
  return _M0L6_2atmpS747 | _M0L6_2atmpS748;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S6,
  struct _M0TPB6Logger _M0L10_2ax__4934S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS1692;
  int32_t _M0L6_2acntS1752;
  moonbit_string_t _M0L15_2a_2aarg__4935S8;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S6;
  _M0L8_2afieldS1692 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS1752 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS1752 > 1) {
    int32_t _M0L11_2anew__cntS1753 = _M0L6_2acntS1752 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS1753;
    moonbit_incref(_M0L8_2afieldS1692);
  } else if (_M0L6_2acntS1752 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__4935S8 = _M0L8_2afieldS1692;
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_28.data);
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S9, _M0L15_2a_2aarg__4935S8);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_29.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS710) {
  switch (Moonbit_object_tag(_M0L4_2aeS710)) {
    case 4: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_30.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_31.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_32.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS710);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS710);
      return (moonbit_string_t)moonbit_string_literal_33.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS727,
  int32_t _M0L8_2aparamS726
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS725 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS727;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS725, _M0L8_2aparamS726);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS724,
  struct _M0TPC16string10StringView _M0L8_2aparamS723
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS722 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS724;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS722, _M0L8_2aparamS723);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS721,
  moonbit_string_t _M0L8_2aparamS718,
  int32_t _M0L8_2aparamS719,
  int32_t _M0L8_2aparamS720
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS717 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS721;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS717, _M0L8_2aparamS718, _M0L8_2aparamS719, _M0L8_2aparamS720);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS716,
  moonbit_string_t _M0L8_2aparamS715
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS714 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS716;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS714, _M0L8_2aparamS715);
  return 0;
}

void moonbit_init() {
  int32_t _M0L6_2atmpS733;
  moonbit_string_t* _M0L6_2atmpS746;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS745;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS744;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS636;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS743;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS742;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS741;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS736;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS637;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS740;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS739;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS738;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS737;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS635;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS735;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS734;
  #line 13 "E:\\moonbit\\clawteam\\internal\\signal\\signal.mbt"
  _M0L6_2atmpS733
  = _M0FP48clawteam8clawteam8internal6signal15signal__sigtstp();
  #line 13 "E:\\moonbit\\clawteam\\internal\\signal\\signal.mbt"
  _M0FP48clawteam8clawteam8internal6signal7sigtstp
  = _M0FP48clawteam8clawteam8internal6signal20check__signal__value(_M0L6_2atmpS733, (moonbit_string_t)moonbit_string_literal_34.data);
  _M0L6_2atmpS746 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS746[0] = (moonbit_string_t)moonbit_string_literal_35.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal22signal__blackbox__test49____test__7369676e616c5f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS745
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS745)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS745->$0
  = _M0FP48clawteam8clawteam8internal22signal__blackbox__test49____test__7369676e616c5f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS745->$1 = _M0L6_2atmpS746;
  _M0L8_2atupleS744
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS744)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS744->$0 = 0;
  _M0L8_2atupleS744->$1 = _M0L8_2atupleS745;
  _M0L7_2abindS636
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS636[0] = _M0L8_2atupleS744;
  _M0L6_2atmpS743 = _M0L7_2abindS636;
  _M0L6_2atmpS742
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS743
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS741
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS742);
  _M0L8_2atupleS736
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS736)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS736->$0 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L8_2atupleS736->$1 = _M0L6_2atmpS741;
  _M0L7_2abindS637
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS740 = _M0L7_2abindS637;
  _M0L6_2atmpS739
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS740
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS738
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS739);
  _M0L8_2atupleS737
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS737)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS737->$0 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L8_2atupleS737->$1 = _M0L6_2atmpS738;
  _M0L7_2abindS635
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS635[0] = _M0L8_2atupleS736;
  _M0L7_2abindS635[1] = _M0L8_2atupleS737;
  _M0L6_2atmpS735 = _M0L7_2abindS635;
  _M0L6_2atmpS734
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS735
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal22signal__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS734);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS732;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS704;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS705;
  int32_t _M0L7_2abindS706;
  int32_t _M0L2__S707;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS732
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS704
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS704)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS704->$0 = _M0L6_2atmpS732;
  _M0L12async__testsS704->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS705
  = _M0FP48clawteam8clawteam8internal22signal__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS706 = _M0L7_2abindS705->$1;
  _M0L2__S707 = 0;
  while (1) {
    if (_M0L2__S707 < _M0L7_2abindS706) {
      struct _M0TUsiE** _M0L8_2afieldS1696 = _M0L7_2abindS705->$0;
      struct _M0TUsiE** _M0L3bufS731 = _M0L8_2afieldS1696;
      struct _M0TUsiE* _M0L6_2atmpS1695 =
        (struct _M0TUsiE*)_M0L3bufS731[_M0L2__S707];
      struct _M0TUsiE* _M0L3argS708 = _M0L6_2atmpS1695;
      moonbit_string_t _M0L8_2afieldS1694 = _M0L3argS708->$0;
      moonbit_string_t _M0L6_2atmpS728 = _M0L8_2afieldS1694;
      int32_t _M0L8_2afieldS1693 = _M0L3argS708->$1;
      int32_t _M0L6_2atmpS729 = _M0L8_2afieldS1693;
      int32_t _M0L6_2atmpS730;
      moonbit_incref(_M0L6_2atmpS728);
      moonbit_incref(_M0L12async__testsS704);
      #line 441 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal22signal__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS704, _M0L6_2atmpS728, _M0L6_2atmpS729);
      _M0L6_2atmpS730 = _M0L2__S707 + 1;
      _M0L2__S707 = _M0L6_2atmpS730;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS705);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\internal\\signal\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal22signal__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal22signal__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS704);
  return 0;
}