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

struct _M0TPB5EntryGsRPC16string10StringViewE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TUsRPC16string10StringViewE;

struct _M0DTPB4Json5Array;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__;

struct _M0R38String_3a_3aiter_2eanon__u2109__l247__;

struct _M0TWssbEu;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools4todo33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB3MapGsRPC16string10StringViewE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0TWsERPB4Json;

struct _M0BTPB6Logger;

struct _M0TWEOUsRPC16string10StringViewE;

struct _M0DTPC15error5Error104clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools4todo33MoonBitTestDriverInternalSkipTestE3Err;

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

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__;

struct _M0TPC13ref3RefGORPB13StringBuilderE;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0KTPB6ToJsonTPB5ArrayGsE;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0KTPB6ToJsonTPB3MapGsRPC16string10StringViewE;

struct _M0TPB9ArrayViewGsE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__;

struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

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

struct _M0TPB5EntryGsRPC16string10StringViewE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $5_1;
  int32_t $5_2;
  struct _M0TPB5EntryGsRPC16string10StringViewE* $1;
  moonbit_string_t $4;
  moonbit_string_t $5_0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TUsRPC16string10StringViewE {
  int32_t $1_1;
  int32_t $1_2;
  moonbit_string_t $0;
  moonbit_string_t $1_0;
  
};

struct _M0DTPB4Json5Array {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2109__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0TUsRPB6LoggerE {
  moonbit_string_t $0;
  struct _M0BTPB6Logger* $1_0;
  void* $1_1;
  
};

struct _M0TWEOc {
  int32_t(* code)(struct _M0TWEOc*);
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools4todo33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TPB3MapGsRPC16string10StringViewE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRPC16string10StringViewE** $0;
  struct _M0TPB5EntryGsRPC16string10StringViewE* $5;
  
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

struct _M0TWsERPB4Json {
  void*(* code)(struct _M0TWsERPB4Json*, moonbit_string_t);
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  
};

struct _M0TWEOUsRPC16string10StringViewE {
  struct _M0TUsRPC16string10StringViewE*(* code)(
    struct _M0TWEOUsRPC16string10StringViewE*
  );
  
};

struct _M0DTPC15error5Error104clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPC16string10StringViewE** $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
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

struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__ {
  struct _M0TUsRPC16string10StringViewE*(* code)(
    struct _M0TWEOUsRPC16string10StringViewE*
  );
  struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE* $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools4todo33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
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

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
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

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0TPC13ref3RefGORPB13StringBuilderE {
  struct _M0TPB13StringBuilder* $0;
  
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

struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE {
  struct _M0TPB5EntryGsRPC16string10StringViewE* $0;
  
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

struct _M0KTPB6ToJsonTPB5ArrayGsE {
  struct _M0BTPB6ToJson* $0;
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

struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct _M0KTPB6ToJsonTPB3MapGsRPC16string10StringViewE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err {
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

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IPC16string6StringPB6ToJson18to__json_2edyncall(
  struct _M0TWsERPB4Json*,
  moonbit_string_t
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN17error__to__stringS1422(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN14handle__resultS1413(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testC3355l436(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testC3351l437(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools4todo45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1342(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1337(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1324(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools4todo28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools4todo34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo35____test__6d616e616765722e6d6274__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo35____test__6d616e616765722e6d6274__0(
  
);

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam5tools4todo22remove__list__prefixes(
  struct _M0TPC16string10StringView
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools4todo19extract__task__tags(
  moonbit_string_t
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

struct _M0TUsRPC16string10StringViewE* _M0MPB5Iter24nextGsRPC16string10StringViewE(
  struct _M0TWEOUsRPC16string10StringViewE*
);

void* _M0IPB3MapPB6ToJson8to__jsonGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGsE(struct _M0TPB5ArrayGsE*);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGsRPB4JsonE(
  struct _M0TPB5ArrayGsE*,
  struct _M0TWsERPB4Json*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPC16string10StringViewE* _M0MPB3Map5iter2GsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*
);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TWEOUsRPC16string10StringViewE* _M0MPB3Map4iterGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*
);

struct _M0TUsRPC16string10StringViewE* _M0MPB3Map4iterGsRPC16string10StringViewEC2451l591(
  struct _M0TWEOUsRPC16string10StringViewE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2445l591(
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

struct _M0TPB3MapGsRPC16string10StringViewE* _M0MPB3Map11from__arrayGsRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE
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

int32_t _M0MPB3Map3setGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*,
  moonbit_string_t,
  struct _M0TPC16string10StringView
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

int32_t _M0MPB3Map4growGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*
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

int32_t _M0MPB3Map15set__with__hashGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*,
  moonbit_string_t,
  struct _M0TPC16string10StringView,
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

int32_t _M0MPB3Map10push__awayGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*,
  int32_t,
  struct _M0TPB5EntryGsRPC16string10StringViewE*
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

int32_t _M0MPB3Map10set__entryGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*,
  struct _M0TPB5EntryGsRPC16string10StringViewE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE*,
  int32_t,
  struct _M0TPB5EntryGsRPC16string10StringViewE*
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

struct _M0TPB3MapGsRPC16string10StringViewE* _M0MPB3Map11new_2einnerGsRPC16string10StringViewE(
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

struct _M0TPB5EntryGsRPC16string10StringViewE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPC16string10StringViewEE(
  struct _M0TPB5EntryGsRPC16string10StringViewE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2128l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2109l247(struct _M0TWEOc*);

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

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPC15array9ArrayView6lengthGsE(struct _M0TPB9ArrayViewGsE);

int32_t _M0MPC15array9ArrayView6lengthGUsRPC16string10StringViewEE(
  struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE
);

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

struct _M0TWEOUsRPC16string10StringViewE* _M0MPB4Iter3newGUsRPC16string10StringViewEE(
  struct _M0TWEOUsRPC16string10StringViewE*
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

struct _M0TUsRPC16string10StringViewE* _M0MPB4Iter4nextGUsRPC16string10StringViewEE(
  struct _M0TWEOUsRPC16string10StringViewE*
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

int32_t _M0MPC16string6String16unsafe__char__at(moonbit_string_t, int32_t);

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

int64_t _M0FPB5abortGOiE(moonbit_string_t, moonbit_string_t);

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

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t);

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

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE(
  void*
);

void* _M0IPB3MapPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsRPC16string10StringViewE(
  void*
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
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    70, 105, 114, 115, 116, 32, 116, 97, 115, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    32, 32, 32, 32, 49, 50, 32, 32, 32, 70, 105, 114, 115, 116, 32, 116, 
    97, 115, 107, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    84, 97, 98, 98, 101, 100, 32, 110, 117, 109, 98, 101, 114, 101, 100, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 
    58, 109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 58, 50, 49, 
    50, 58, 51, 45, 50, 50, 53, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[97]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 96), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 
    112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 
    101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 
    110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_65 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[95]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 94), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 
    114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    49, 50, 51, 97, 98, 99, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    73, 110, 100, 101, 110, 116, 101, 100, 32, 98, 117, 108, 108, 101, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    116, 111, 111, 108, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 
    58, 109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 58, 50, 49, 
    50, 58, 51, 50, 45, 50, 50, 53, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_45 =
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
} const moonbit_string_literal_95 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    51, 46, 9, 84, 97, 98, 98, 101, 100, 32, 110, 117, 109, 98, 101, 
    114, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 
    58, 109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 58, 49, 52, 
    49, 58, 53, 48, 45, 49, 52, 49, 58, 55, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_55 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_46 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 
    58, 109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 58, 50, 49, 
    50, 58, 49, 54, 45, 50, 49, 50, 58, 50, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    45, 32, 66, 117, 108, 108, 101, 116, 32, 112, 111, 105, 110, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    84, 97, 98, 98, 101, 100, 32, 98, 117, 108, 108, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    101, 120, 116, 114, 97, 99, 116, 95, 116, 97, 115, 107, 95, 116, 
    97, 103, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    42, 9, 84, 97, 98, 98, 101, 100, 32, 98, 117, 108, 108, 101, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_88 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    50, 46, 32, 32, 32, 83, 101, 99, 111, 110, 100, 32, 116, 97, 115, 
    107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[57]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 56), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 34, 44, 32, 
    34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_37 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    112, 114, 111, 109, 112, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_69 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 
    58, 109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 58, 49, 52, 
    49, 58, 51, 45, 49, 52, 49, 58, 56, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    78, 111, 32, 112, 114, 101, 102, 105, 120, 32, 104, 101, 114, 101, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    117, 116, 105, 108, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    83, 101, 99, 111, 110, 100, 32, 116, 97, 115, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 116, 111, 100, 111, 
    58, 109, 97, 110, 97, 103, 101, 114, 46, 109, 98, 116, 58, 49, 52, 
    49, 58, 49, 54, 45, 49, 52, 49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    49, 46, 32, 32, 32, 70, 105, 114, 115, 116, 32, 116, 97, 115, 107, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    66, 117, 108, 108, 101, 116, 32, 112, 111, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 105, 114, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    49, 50, 32, 32, 32, 70, 105, 114, 115, 116, 32, 116, 97, 115, 107, 
    32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    65, 110, 111, 116, 104, 101, 114, 32, 98, 117, 108, 108, 101, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    45, 32, 32, 73, 110, 100, 101, 110, 116, 101, 100, 32, 98, 117, 108, 
    108, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[81]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 80), 
    84, 104, 105, 115, 32, 105, 115, 32, 97, 32, 116, 101, 115, 116, 
    46, 10, 60, 116, 97, 115, 107, 62, 70, 105, 114, 115, 116, 32, 116, 
    97, 115, 107, 60, 47, 116, 97, 115, 107, 62, 10, 83, 111, 109, 101, 
    32, 116, 101, 120, 116, 46, 10, 60, 116, 97, 115, 107, 62, 83, 101, 
    99, 111, 110, 100, 32, 116, 97, 115, 107, 60, 47, 116, 97, 115, 107, 
    62, 10, 69, 110, 100, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    42, 32, 65, 110, 111, 116, 104, 101, 114, 32, 98, 117, 108, 108, 
    101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    114, 101, 109, 111, 118, 101, 95, 108, 105, 115, 116, 95, 112, 114, 
    101, 102, 105, 120, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 99, 104, 101, 109, 97, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    116, 121, 112, 101, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    49, 46, 102, 105, 114, 115, 116, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWsERPB4Json data; 
} const _M0IPC16string6StringPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IPC16string6StringPB6ToJson18to__json_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN17error__to__stringS1422$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN17error__to__stringS1422
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__0_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam5tools4todo41____test__6d616e616765722e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__1_2edyncall$closure.data;

struct _M0TWsERPB4Json* _M0IPC16string6StringPB6ToJson14to__json_2eclo =
  (struct _M0TWsERPB4Json*)&_M0IPC16string6StringPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam5tools4todo41____test__6d616e616765722e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__0_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0167moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPB3MapPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsRPC16string10StringViewE}
  };

struct _M0BTPB6ToJson* _M0FP0167moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0167moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0123moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE}
  };

struct _M0BTPB6ToJson* _M0FP0123moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0123moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1091$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1091 =
  &_M0FPB31ryu__to__string_2erecord_2f1091$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam5tools4todo48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3388
) {
  return _M0FP48clawteam8clawteam5tools4todo35____test__6d616e616765722e6d6274__0();
}

void* _M0IPC16string6StringPB6ToJson18to__json_2edyncall(
  struct _M0TWsERPB4Json* _M0L6_2aenvS3387,
  moonbit_string_t _M0L4selfS8
) {
  return _M0IPC16string6StringPB6ToJson8to__json(_M0L4selfS8);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo45____test__6d616e616765722e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3386
) {
  return _M0FP48clawteam8clawteam5tools4todo35____test__6d616e616765722e6d6274__1();
}

int32_t _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1443,
  moonbit_string_t _M0L8filenameS1418,
  int32_t _M0L5indexS1421
) {
  struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413* _closure_3881;
  struct _M0TWssbEu* _M0L14handle__resultS1413;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1422;
  void* _M0L11_2atry__errS1437;
  struct moonbit_result_0 _tmp_3883;
  int32_t _handle__error__result_3884;
  int32_t _M0L6_2atmpS3374;
  void* _M0L3errS1438;
  moonbit_string_t _M0L4nameS1440;
  struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1441;
  moonbit_string_t _M0L8_2afieldS3389;
  int32_t _M0L6_2acntS3779;
  moonbit_string_t _M0L7_2anameS1442;
  #line 535 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1418);
  _closure_3881
  = (struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413*)moonbit_malloc(sizeof(struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413));
  Moonbit_object_header(_closure_3881)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413, $1) >> 2, 1, 0);
  _closure_3881->code
  = &_M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN14handle__resultS1413;
  _closure_3881->$0 = _M0L5indexS1421;
  _closure_3881->$1 = _M0L8filenameS1418;
  _M0L14handle__resultS1413 = (struct _M0TWssbEu*)_closure_3881;
  _M0L17error__to__stringS1422
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN17error__to__stringS1422$closure.data;
  moonbit_incref(_M0L12async__testsS1443);
  moonbit_incref(_M0L17error__to__stringS1422);
  moonbit_incref(_M0L8filenameS1418);
  moonbit_incref(_M0L14handle__resultS1413);
  #line 569 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _tmp_3883
  = _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__test(_M0L12async__testsS1443, _M0L8filenameS1418, _M0L5indexS1421, _M0L14handle__resultS1413, _M0L17error__to__stringS1422);
  if (_tmp_3883.tag) {
    int32_t const _M0L5_2aokS3383 = _tmp_3883.data.ok;
    _handle__error__result_3884 = _M0L5_2aokS3383;
  } else {
    void* const _M0L6_2aerrS3384 = _tmp_3883.data.err;
    moonbit_decref(_M0L12async__testsS1443);
    moonbit_decref(_M0L17error__to__stringS1422);
    moonbit_decref(_M0L8filenameS1418);
    _M0L11_2atry__errS1437 = _M0L6_2aerrS3384;
    goto join_1436;
  }
  if (_handle__error__result_3884) {
    moonbit_decref(_M0L12async__testsS1443);
    moonbit_decref(_M0L17error__to__stringS1422);
    moonbit_decref(_M0L8filenameS1418);
    _M0L6_2atmpS3374 = 1;
  } else {
    struct moonbit_result_0 _tmp_3885;
    int32_t _handle__error__result_3886;
    moonbit_incref(_M0L12async__testsS1443);
    moonbit_incref(_M0L17error__to__stringS1422);
    moonbit_incref(_M0L8filenameS1418);
    moonbit_incref(_M0L14handle__resultS1413);
    #line 572 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    _tmp_3885
    = _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1443, _M0L8filenameS1418, _M0L5indexS1421, _M0L14handle__resultS1413, _M0L17error__to__stringS1422);
    if (_tmp_3885.tag) {
      int32_t const _M0L5_2aokS3381 = _tmp_3885.data.ok;
      _handle__error__result_3886 = _M0L5_2aokS3381;
    } else {
      void* const _M0L6_2aerrS3382 = _tmp_3885.data.err;
      moonbit_decref(_M0L12async__testsS1443);
      moonbit_decref(_M0L17error__to__stringS1422);
      moonbit_decref(_M0L8filenameS1418);
      _M0L11_2atry__errS1437 = _M0L6_2aerrS3382;
      goto join_1436;
    }
    if (_handle__error__result_3886) {
      moonbit_decref(_M0L12async__testsS1443);
      moonbit_decref(_M0L17error__to__stringS1422);
      moonbit_decref(_M0L8filenameS1418);
      _M0L6_2atmpS3374 = 1;
    } else {
      struct moonbit_result_0 _tmp_3887;
      int32_t _handle__error__result_3888;
      moonbit_incref(_M0L12async__testsS1443);
      moonbit_incref(_M0L17error__to__stringS1422);
      moonbit_incref(_M0L8filenameS1418);
      moonbit_incref(_M0L14handle__resultS1413);
      #line 575 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _tmp_3887
      = _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1443, _M0L8filenameS1418, _M0L5indexS1421, _M0L14handle__resultS1413, _M0L17error__to__stringS1422);
      if (_tmp_3887.tag) {
        int32_t const _M0L5_2aokS3379 = _tmp_3887.data.ok;
        _handle__error__result_3888 = _M0L5_2aokS3379;
      } else {
        void* const _M0L6_2aerrS3380 = _tmp_3887.data.err;
        moonbit_decref(_M0L12async__testsS1443);
        moonbit_decref(_M0L17error__to__stringS1422);
        moonbit_decref(_M0L8filenameS1418);
        _M0L11_2atry__errS1437 = _M0L6_2aerrS3380;
        goto join_1436;
      }
      if (_handle__error__result_3888) {
        moonbit_decref(_M0L12async__testsS1443);
        moonbit_decref(_M0L17error__to__stringS1422);
        moonbit_decref(_M0L8filenameS1418);
        _M0L6_2atmpS3374 = 1;
      } else {
        struct moonbit_result_0 _tmp_3889;
        int32_t _handle__error__result_3890;
        moonbit_incref(_M0L12async__testsS1443);
        moonbit_incref(_M0L17error__to__stringS1422);
        moonbit_incref(_M0L8filenameS1418);
        moonbit_incref(_M0L14handle__resultS1413);
        #line 578 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        _tmp_3889
        = _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1443, _M0L8filenameS1418, _M0L5indexS1421, _M0L14handle__resultS1413, _M0L17error__to__stringS1422);
        if (_tmp_3889.tag) {
          int32_t const _M0L5_2aokS3377 = _tmp_3889.data.ok;
          _handle__error__result_3890 = _M0L5_2aokS3377;
        } else {
          void* const _M0L6_2aerrS3378 = _tmp_3889.data.err;
          moonbit_decref(_M0L12async__testsS1443);
          moonbit_decref(_M0L17error__to__stringS1422);
          moonbit_decref(_M0L8filenameS1418);
          _M0L11_2atry__errS1437 = _M0L6_2aerrS3378;
          goto join_1436;
        }
        if (_handle__error__result_3890) {
          moonbit_decref(_M0L12async__testsS1443);
          moonbit_decref(_M0L17error__to__stringS1422);
          moonbit_decref(_M0L8filenameS1418);
          _M0L6_2atmpS3374 = 1;
        } else {
          struct moonbit_result_0 _tmp_3891;
          moonbit_incref(_M0L14handle__resultS1413);
          #line 581 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
          _tmp_3891
          = _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1443, _M0L8filenameS1418, _M0L5indexS1421, _M0L14handle__resultS1413, _M0L17error__to__stringS1422);
          if (_tmp_3891.tag) {
            int32_t const _M0L5_2aokS3375 = _tmp_3891.data.ok;
            _M0L6_2atmpS3374 = _M0L5_2aokS3375;
          } else {
            void* const _M0L6_2aerrS3376 = _tmp_3891.data.err;
            _M0L11_2atry__errS1437 = _M0L6_2aerrS3376;
            goto join_1436;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3374) {
    void* _M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3385 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3385)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3385)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1437
    = _M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3385;
    goto join_1436;
  } else {
    moonbit_decref(_M0L14handle__resultS1413);
  }
  goto joinlet_3882;
  join_1436:;
  _M0L3errS1438 = _M0L11_2atry__errS1437;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1441
  = (struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1438;
  _M0L8_2afieldS3389 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1441->$0;
  _M0L6_2acntS3779
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1441)->rc;
  if (_M0L6_2acntS3779 > 1) {
    int32_t _M0L11_2anew__cntS3780 = _M0L6_2acntS3779 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1441)->rc
    = _M0L11_2anew__cntS3780;
    moonbit_incref(_M0L8_2afieldS3389);
  } else if (_M0L6_2acntS3779 == 1) {
    #line 588 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1441);
  }
  _M0L7_2anameS1442 = _M0L8_2afieldS3389;
  _M0L4nameS1440 = _M0L7_2anameS1442;
  goto join_1439;
  goto joinlet_3892;
  join_1439:;
  #line 589 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN14handle__resultS1413(_M0L14handle__resultS1413, _M0L4nameS1440, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3892:;
  joinlet_3882:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN17error__to__stringS1422(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3373,
  void* _M0L3errS1423
) {
  void* _M0L1eS1425;
  moonbit_string_t _M0L1eS1427;
  #line 558 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3373);
  switch (Moonbit_object_tag(_M0L3errS1423)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1428 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1423;
      moonbit_string_t _M0L8_2afieldS3390 = _M0L10_2aFailureS1428->$0;
      int32_t _M0L6_2acntS3781 =
        Moonbit_object_header(_M0L10_2aFailureS1428)->rc;
      moonbit_string_t _M0L4_2aeS1429;
      if (_M0L6_2acntS3781 > 1) {
        int32_t _M0L11_2anew__cntS3782 = _M0L6_2acntS3781 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1428)->rc
        = _M0L11_2anew__cntS3782;
        moonbit_incref(_M0L8_2afieldS3390);
      } else if (_M0L6_2acntS3781 == 1) {
        #line 559 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1428);
      }
      _M0L4_2aeS1429 = _M0L8_2afieldS3390;
      _M0L1eS1427 = _M0L4_2aeS1429;
      goto join_1426;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1430 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1423;
      moonbit_string_t _M0L8_2afieldS3391 = _M0L15_2aInspectErrorS1430->$0;
      int32_t _M0L6_2acntS3783 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1430)->rc;
      moonbit_string_t _M0L4_2aeS1431;
      if (_M0L6_2acntS3783 > 1) {
        int32_t _M0L11_2anew__cntS3784 = _M0L6_2acntS3783 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1430)->rc
        = _M0L11_2anew__cntS3784;
        moonbit_incref(_M0L8_2afieldS3391);
      } else if (_M0L6_2acntS3783 == 1) {
        #line 559 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1430);
      }
      _M0L4_2aeS1431 = _M0L8_2afieldS3391;
      _M0L1eS1427 = _M0L4_2aeS1431;
      goto join_1426;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1432 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1423;
      moonbit_string_t _M0L8_2afieldS3392 = _M0L16_2aSnapshotErrorS1432->$0;
      int32_t _M0L6_2acntS3785 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1432)->rc;
      moonbit_string_t _M0L4_2aeS1433;
      if (_M0L6_2acntS3785 > 1) {
        int32_t _M0L11_2anew__cntS3786 = _M0L6_2acntS3785 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1432)->rc
        = _M0L11_2anew__cntS3786;
        moonbit_incref(_M0L8_2afieldS3392);
      } else if (_M0L6_2acntS3785 == 1) {
        #line 559 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1432);
      }
      _M0L4_2aeS1433 = _M0L8_2afieldS3392;
      _M0L1eS1427 = _M0L4_2aeS1433;
      goto join_1426;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error104clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1434 =
        (struct _M0DTPC15error5Error104clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1423;
      moonbit_string_t _M0L8_2afieldS3393 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1434->$0;
      int32_t _M0L6_2acntS3787 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1434)->rc;
      moonbit_string_t _M0L4_2aeS1435;
      if (_M0L6_2acntS3787 > 1) {
        int32_t _M0L11_2anew__cntS3788 = _M0L6_2acntS3787 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1434)->rc
        = _M0L11_2anew__cntS3788;
        moonbit_incref(_M0L8_2afieldS3393);
      } else if (_M0L6_2acntS3787 == 1) {
        #line 559 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1434);
      }
      _M0L4_2aeS1435 = _M0L8_2afieldS3393;
      _M0L1eS1427 = _M0L4_2aeS1435;
      goto join_1426;
      break;
    }
    default: {
      _M0L1eS1425 = _M0L3errS1423;
      goto join_1424;
      break;
    }
  }
  join_1426:;
  return _M0L1eS1427;
  join_1424:;
  #line 564 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1425);
}

int32_t _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__executeN14handle__resultS1413(
  struct _M0TWssbEu* _M0L6_2aenvS3359,
  moonbit_string_t _M0L8testnameS1414,
  moonbit_string_t _M0L7messageS1415,
  int32_t _M0L7skippedS1416
) {
  struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413* _M0L14_2acasted__envS3360;
  moonbit_string_t _M0L8_2afieldS3403;
  moonbit_string_t _M0L8filenameS1418;
  int32_t _M0L8_2afieldS3402;
  int32_t _M0L6_2acntS3789;
  int32_t _M0L5indexS1421;
  int32_t _if__result_3895;
  moonbit_string_t _M0L10file__nameS1417;
  moonbit_string_t _M0L10test__nameS1419;
  moonbit_string_t _M0L7messageS1420;
  moonbit_string_t _M0L6_2atmpS3372;
  moonbit_string_t _M0L6_2atmpS3401;
  moonbit_string_t _M0L6_2atmpS3371;
  moonbit_string_t _M0L6_2atmpS3400;
  moonbit_string_t _M0L6_2atmpS3369;
  moonbit_string_t _M0L6_2atmpS3370;
  moonbit_string_t _M0L6_2atmpS3399;
  moonbit_string_t _M0L6_2atmpS3368;
  moonbit_string_t _M0L6_2atmpS3398;
  moonbit_string_t _M0L6_2atmpS3366;
  moonbit_string_t _M0L6_2atmpS3367;
  moonbit_string_t _M0L6_2atmpS3397;
  moonbit_string_t _M0L6_2atmpS3365;
  moonbit_string_t _M0L6_2atmpS3396;
  moonbit_string_t _M0L6_2atmpS3363;
  moonbit_string_t _M0L6_2atmpS3364;
  moonbit_string_t _M0L6_2atmpS3395;
  moonbit_string_t _M0L6_2atmpS3362;
  moonbit_string_t _M0L6_2atmpS3394;
  moonbit_string_t _M0L6_2atmpS3361;
  #line 542 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3360
  = (struct _M0R108_24clawteam_2fclawteam_2ftools_2ftodo_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1413*)_M0L6_2aenvS3359;
  _M0L8_2afieldS3403 = _M0L14_2acasted__envS3360->$1;
  _M0L8filenameS1418 = _M0L8_2afieldS3403;
  _M0L8_2afieldS3402 = _M0L14_2acasted__envS3360->$0;
  _M0L6_2acntS3789 = Moonbit_object_header(_M0L14_2acasted__envS3360)->rc;
  if (_M0L6_2acntS3789 > 1) {
    int32_t _M0L11_2anew__cntS3790 = _M0L6_2acntS3789 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3360)->rc
    = _M0L11_2anew__cntS3790;
    moonbit_incref(_M0L8filenameS1418);
  } else if (_M0L6_2acntS3789 == 1) {
    #line 542 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3360);
  }
  _M0L5indexS1421 = _M0L8_2afieldS3402;
  if (!_M0L7skippedS1416) {
    _if__result_3895 = 1;
  } else {
    _if__result_3895 = 0;
  }
  if (_if__result_3895) {
    
  }
  #line 548 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1417 = _M0MPC16string6String6escape(_M0L8filenameS1418);
  #line 549 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1419 = _M0MPC16string6String6escape(_M0L8testnameS1414);
  #line 550 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1420 = _M0MPC16string6String6escape(_M0L7messageS1415);
  #line 551 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 553 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3372
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1417);
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3401
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3372);
  moonbit_decref(_M0L6_2atmpS3372);
  _M0L6_2atmpS3371 = _M0L6_2atmpS3401;
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3400
  = moonbit_add_string(_M0L6_2atmpS3371, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3371);
  _M0L6_2atmpS3369 = _M0L6_2atmpS3400;
  #line 553 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3370
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1421);
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3399 = moonbit_add_string(_M0L6_2atmpS3369, _M0L6_2atmpS3370);
  moonbit_decref(_M0L6_2atmpS3369);
  moonbit_decref(_M0L6_2atmpS3370);
  _M0L6_2atmpS3368 = _M0L6_2atmpS3399;
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3398
  = moonbit_add_string(_M0L6_2atmpS3368, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3368);
  _M0L6_2atmpS3366 = _M0L6_2atmpS3398;
  #line 553 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3367
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1419);
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3397 = moonbit_add_string(_M0L6_2atmpS3366, _M0L6_2atmpS3367);
  moonbit_decref(_M0L6_2atmpS3366);
  moonbit_decref(_M0L6_2atmpS3367);
  _M0L6_2atmpS3365 = _M0L6_2atmpS3397;
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3396
  = moonbit_add_string(_M0L6_2atmpS3365, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3365);
  _M0L6_2atmpS3363 = _M0L6_2atmpS3396;
  #line 553 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3364
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1420);
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3395 = moonbit_add_string(_M0L6_2atmpS3363, _M0L6_2atmpS3364);
  moonbit_decref(_M0L6_2atmpS3363);
  moonbit_decref(_M0L6_2atmpS3364);
  _M0L6_2atmpS3362 = _M0L6_2atmpS3395;
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3394
  = moonbit_add_string(_M0L6_2atmpS3362, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3362);
  _M0L6_2atmpS3361 = _M0L6_2atmpS3394;
  #line 552 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3361);
  #line 555 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1412,
  moonbit_string_t _M0L8filenameS1409,
  int32_t _M0L5indexS1403,
  struct _M0TWssbEu* _M0L14handle__resultS1399,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1401
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1379;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1408;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1381;
  moonbit_string_t* _M0L5attrsS1382;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1402;
  moonbit_string_t _M0L4nameS1385;
  moonbit_string_t _M0L4nameS1383;
  int32_t _M0L6_2atmpS3358;
  struct _M0TWEOs* _M0L5_2aitS1387;
  struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__* _closure_3904;
  struct _M0TWEOc* _M0L6_2atmpS3349;
  struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__* _closure_3905;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3350;
  struct moonbit_result_0 _result_3906;
  #line 416 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1412);
  moonbit_incref(_M0FP48clawteam8clawteam5tools4todo48moonbit__test__driver__internal__no__args__tests);
  #line 423 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1408
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam5tools4todo48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1409);
  if (_M0L7_2abindS1408 == 0) {
    struct moonbit_result_0 _result_3897;
    if (_M0L7_2abindS1408) {
      moonbit_decref(_M0L7_2abindS1408);
    }
    moonbit_decref(_M0L17error__to__stringS1401);
    moonbit_decref(_M0L14handle__resultS1399);
    _result_3897.tag = 1;
    _result_3897.data.ok = 0;
    return _result_3897;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1410 =
      _M0L7_2abindS1408;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1411 =
      _M0L7_2aSomeS1410;
    _M0L10index__mapS1379 = _M0L13_2aindex__mapS1411;
    goto join_1378;
  }
  join_1378:;
  #line 425 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1402
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1379, _M0L5indexS1403);
  if (_M0L7_2abindS1402 == 0) {
    struct moonbit_result_0 _result_3899;
    if (_M0L7_2abindS1402) {
      moonbit_decref(_M0L7_2abindS1402);
    }
    moonbit_decref(_M0L17error__to__stringS1401);
    moonbit_decref(_M0L14handle__resultS1399);
    _result_3899.tag = 1;
    _result_3899.data.ok = 0;
    return _result_3899;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1404 =
      _M0L7_2abindS1402;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1405 = _M0L7_2aSomeS1404;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3407 = _M0L4_2axS1405->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1406 = _M0L8_2afieldS3407;
    moonbit_string_t* _M0L8_2afieldS3406 = _M0L4_2axS1405->$1;
    int32_t _M0L6_2acntS3791 = Moonbit_object_header(_M0L4_2axS1405)->rc;
    moonbit_string_t* _M0L8_2aattrsS1407;
    if (_M0L6_2acntS3791 > 1) {
      int32_t _M0L11_2anew__cntS3792 = _M0L6_2acntS3791 - 1;
      Moonbit_object_header(_M0L4_2axS1405)->rc = _M0L11_2anew__cntS3792;
      moonbit_incref(_M0L8_2afieldS3406);
      moonbit_incref(_M0L4_2afS1406);
    } else if (_M0L6_2acntS3791 == 1) {
      #line 423 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1405);
    }
    _M0L8_2aattrsS1407 = _M0L8_2afieldS3406;
    _M0L1fS1381 = _M0L4_2afS1406;
    _M0L5attrsS1382 = _M0L8_2aattrsS1407;
    goto join_1380;
  }
  join_1380:;
  _M0L6_2atmpS3358 = Moonbit_array_length(_M0L5attrsS1382);
  if (_M0L6_2atmpS3358 >= 1) {
    moonbit_string_t _M0L6_2atmpS3405 = (moonbit_string_t)_M0L5attrsS1382[0];
    moonbit_string_t _M0L7_2anameS1386 = _M0L6_2atmpS3405;
    moonbit_incref(_M0L7_2anameS1386);
    _M0L4nameS1385 = _M0L7_2anameS1386;
    goto join_1384;
  } else {
    _M0L4nameS1383 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3900;
  join_1384:;
  _M0L4nameS1383 = _M0L4nameS1385;
  joinlet_3900:;
  #line 426 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1387 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1382);
  while (1) {
    moonbit_string_t _M0L4attrS1389;
    moonbit_string_t _M0L7_2abindS1396;
    int32_t _M0L6_2atmpS3342;
    int64_t _M0L6_2atmpS3341;
    moonbit_incref(_M0L5_2aitS1387);
    #line 428 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1396 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1387);
    if (_M0L7_2abindS1396 == 0) {
      if (_M0L7_2abindS1396) {
        moonbit_decref(_M0L7_2abindS1396);
      }
      moonbit_decref(_M0L5_2aitS1387);
    } else {
      moonbit_string_t _M0L7_2aSomeS1397 = _M0L7_2abindS1396;
      moonbit_string_t _M0L7_2aattrS1398 = _M0L7_2aSomeS1397;
      _M0L4attrS1389 = _M0L7_2aattrS1398;
      goto join_1388;
    }
    goto joinlet_3902;
    join_1388:;
    _M0L6_2atmpS3342 = Moonbit_array_length(_M0L4attrS1389);
    _M0L6_2atmpS3341 = (int64_t)_M0L6_2atmpS3342;
    moonbit_incref(_M0L4attrS1389);
    #line 429 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1389, 5, 0, _M0L6_2atmpS3341)
    ) {
      int32_t _M0L6_2atmpS3348 = _M0L4attrS1389[0];
      int32_t _M0L4_2axS1390 = _M0L6_2atmpS3348;
      if (_M0L4_2axS1390 == 112) {
        int32_t _M0L6_2atmpS3347 = _M0L4attrS1389[1];
        int32_t _M0L4_2axS1391 = _M0L6_2atmpS3347;
        if (_M0L4_2axS1391 == 97) {
          int32_t _M0L6_2atmpS3346 = _M0L4attrS1389[2];
          int32_t _M0L4_2axS1392 = _M0L6_2atmpS3346;
          if (_M0L4_2axS1392 == 110) {
            int32_t _M0L6_2atmpS3345 = _M0L4attrS1389[3];
            int32_t _M0L4_2axS1393 = _M0L6_2atmpS3345;
            if (_M0L4_2axS1393 == 105) {
              int32_t _M0L6_2atmpS3404 = _M0L4attrS1389[4];
              int32_t _M0L6_2atmpS3344;
              int32_t _M0L4_2axS1394;
              moonbit_decref(_M0L4attrS1389);
              _M0L6_2atmpS3344 = _M0L6_2atmpS3404;
              _M0L4_2axS1394 = _M0L6_2atmpS3344;
              if (_M0L4_2axS1394 == 99) {
                void* _M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3343;
                struct moonbit_result_0 _result_3903;
                moonbit_decref(_M0L17error__to__stringS1401);
                moonbit_decref(_M0L14handle__resultS1399);
                moonbit_decref(_M0L5_2aitS1387);
                moonbit_decref(_M0L1fS1381);
                _M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3343
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3343)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3343)->$0
                = _M0L4nameS1383;
                _result_3903.tag = 0;
                _result_3903.data.err
                = _M0L106clawteam_2fclawteam_2ftools_2ftodo_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3343;
                return _result_3903;
              }
            } else {
              moonbit_decref(_M0L4attrS1389);
            }
          } else {
            moonbit_decref(_M0L4attrS1389);
          }
        } else {
          moonbit_decref(_M0L4attrS1389);
        }
      } else {
        moonbit_decref(_M0L4attrS1389);
      }
    } else {
      moonbit_decref(_M0L4attrS1389);
    }
    continue;
    joinlet_3902:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1399);
  moonbit_incref(_M0L4nameS1383);
  _closure_3904
  = (struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__*)moonbit_malloc(sizeof(struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__));
  Moonbit_object_header(_closure_3904)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__, $0) >> 2, 2, 0);
  _closure_3904->code
  = &_M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testC3355l436;
  _closure_3904->$0 = _M0L14handle__resultS1399;
  _closure_3904->$1 = _M0L4nameS1383;
  _M0L6_2atmpS3349 = (struct _M0TWEOc*)_closure_3904;
  _closure_3905
  = (struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__*)moonbit_malloc(sizeof(struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__));
  Moonbit_object_header(_closure_3905)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__, $0) >> 2, 3, 0);
  _closure_3905->code
  = &_M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testC3351l437;
  _closure_3905->$0 = _M0L17error__to__stringS1401;
  _closure_3905->$1 = _M0L14handle__resultS1399;
  _closure_3905->$2 = _M0L4nameS1383;
  _M0L6_2atmpS3350 = (struct _M0TWRPC15error5ErrorEu*)_closure_3905;
  #line 434 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools4todo45moonbit__test__driver__internal__catch__error(_M0L1fS1381, _M0L6_2atmpS3349, _M0L6_2atmpS3350);
  _result_3906.tag = 1;
  _result_3906.data.ok = 1;
  return _result_3906;
}

int32_t _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testC3355l436(
  struct _M0TWEOc* _M0L6_2aenvS3356
) {
  struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__* _M0L14_2acasted__envS3357;
  moonbit_string_t _M0L8_2afieldS3409;
  moonbit_string_t _M0L4nameS1383;
  struct _M0TWssbEu* _M0L8_2afieldS3408;
  int32_t _M0L6_2acntS3793;
  struct _M0TWssbEu* _M0L14handle__resultS1399;
  #line 436 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3357
  = (struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3355__l436__*)_M0L6_2aenvS3356;
  _M0L8_2afieldS3409 = _M0L14_2acasted__envS3357->$1;
  _M0L4nameS1383 = _M0L8_2afieldS3409;
  _M0L8_2afieldS3408 = _M0L14_2acasted__envS3357->$0;
  _M0L6_2acntS3793 = Moonbit_object_header(_M0L14_2acasted__envS3357)->rc;
  if (_M0L6_2acntS3793 > 1) {
    int32_t _M0L11_2anew__cntS3794 = _M0L6_2acntS3793 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3357)->rc
    = _M0L11_2anew__cntS3794;
    moonbit_incref(_M0L4nameS1383);
    moonbit_incref(_M0L8_2afieldS3408);
  } else if (_M0L6_2acntS3793 == 1) {
    #line 436 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3357);
  }
  _M0L14handle__resultS1399 = _M0L8_2afieldS3408;
  #line 436 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1399->code(_M0L14handle__resultS1399, _M0L4nameS1383, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam5tools4todo41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testC3351l437(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3352,
  void* _M0L3errS1400
) {
  struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__* _M0L14_2acasted__envS3353;
  moonbit_string_t _M0L8_2afieldS3412;
  moonbit_string_t _M0L4nameS1383;
  struct _M0TWssbEu* _M0L8_2afieldS3411;
  struct _M0TWssbEu* _M0L14handle__resultS1399;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3410;
  int32_t _M0L6_2acntS3795;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1401;
  moonbit_string_t _M0L6_2atmpS3354;
  #line 437 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3353
  = (struct _M0R185_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2ftodo_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3351__l437__*)_M0L6_2aenvS3352;
  _M0L8_2afieldS3412 = _M0L14_2acasted__envS3353->$2;
  _M0L4nameS1383 = _M0L8_2afieldS3412;
  _M0L8_2afieldS3411 = _M0L14_2acasted__envS3353->$1;
  _M0L14handle__resultS1399 = _M0L8_2afieldS3411;
  _M0L8_2afieldS3410 = _M0L14_2acasted__envS3353->$0;
  _M0L6_2acntS3795 = Moonbit_object_header(_M0L14_2acasted__envS3353)->rc;
  if (_M0L6_2acntS3795 > 1) {
    int32_t _M0L11_2anew__cntS3796 = _M0L6_2acntS3795 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3353)->rc
    = _M0L11_2anew__cntS3796;
    moonbit_incref(_M0L4nameS1383);
    moonbit_incref(_M0L14handle__resultS1399);
    moonbit_incref(_M0L8_2afieldS3410);
  } else if (_M0L6_2acntS3795 == 1) {
    #line 437 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3353);
  }
  _M0L17error__to__stringS1401 = _M0L8_2afieldS3410;
  #line 437 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3354
  = _M0L17error__to__stringS1401->code(_M0L17error__to__stringS1401, _M0L3errS1400);
  #line 437 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1399->code(_M0L14handle__resultS1399, _M0L4nameS1383, _M0L6_2atmpS3354, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam5tools4todo45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1369,
  struct _M0TWEOc* _M0L6on__okS1370,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1367
) {
  void* _M0L11_2atry__errS1365;
  struct moonbit_result_0 _tmp_3908;
  void* _M0L3errS1366;
  #line 375 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _tmp_3908 = _M0L1fS1369->code(_M0L1fS1369);
  if (_tmp_3908.tag) {
    int32_t const _M0L5_2aokS3339 = _tmp_3908.data.ok;
    moonbit_decref(_M0L7on__errS1367);
  } else {
    void* const _M0L6_2aerrS3340 = _tmp_3908.data.err;
    moonbit_decref(_M0L6on__okS1370);
    _M0L11_2atry__errS1365 = _M0L6_2aerrS3340;
    goto join_1364;
  }
  #line 382 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1370->code(_M0L6on__okS1370);
  goto joinlet_3907;
  join_1364:;
  _M0L3errS1366 = _M0L11_2atry__errS1365;
  #line 383 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1367->code(_M0L7on__errS1367, _M0L3errS1366);
  joinlet_3907:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1324;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1337;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1342;
  struct _M0TUsiE** _M0L6_2atmpS3338;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1349;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1350;
  moonbit_string_t _M0L6_2atmpS3337;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1351;
  int32_t _M0L7_2abindS1352;
  int32_t _M0L2__S1353;
  #line 193 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1324 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1337
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1342 = 0;
  _M0L6_2atmpS3338 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1349
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1349)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1349->$0 = _M0L6_2atmpS3338;
  _M0L16file__and__indexS1349->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1350
  = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1337(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1337);
  #line 284 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3337 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1350, 1);
  #line 283 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1351
  = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1342(_M0L51moonbit__test__driver__internal__split__mbt__stringS1342, _M0L6_2atmpS3337, 47);
  _M0L7_2abindS1352 = _M0L10test__argsS1351->$1;
  _M0L2__S1353 = 0;
  while (1) {
    if (_M0L2__S1353 < _M0L7_2abindS1352) {
      moonbit_string_t* _M0L8_2afieldS3414 = _M0L10test__argsS1351->$0;
      moonbit_string_t* _M0L3bufS3336 = _M0L8_2afieldS3414;
      moonbit_string_t _M0L6_2atmpS3413 =
        (moonbit_string_t)_M0L3bufS3336[_M0L2__S1353];
      moonbit_string_t _M0L3argS1354 = _M0L6_2atmpS3413;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1355;
      moonbit_string_t _M0L4fileS1356;
      moonbit_string_t _M0L5rangeS1357;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1358;
      moonbit_string_t _M0L6_2atmpS3334;
      int32_t _M0L5startS1359;
      moonbit_string_t _M0L6_2atmpS3333;
      int32_t _M0L3endS1360;
      int32_t _M0L1iS1361;
      int32_t _M0L6_2atmpS3335;
      moonbit_incref(_M0L3argS1354);
      #line 288 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1355
      = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1342(_M0L51moonbit__test__driver__internal__split__mbt__stringS1342, _M0L3argS1354, 58);
      moonbit_incref(_M0L16file__and__rangeS1355);
      #line 289 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1356
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1355, 0);
      #line 290 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1357
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1355, 1);
      #line 291 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1358
      = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1342(_M0L51moonbit__test__driver__internal__split__mbt__stringS1342, _M0L5rangeS1357, 45);
      moonbit_incref(_M0L15start__and__endS1358);
      #line 294 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3334
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1358, 0);
      #line 294 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1359
      = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1324(_M0L45moonbit__test__driver__internal__parse__int__S1324, _M0L6_2atmpS3334);
      #line 295 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3333
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1358, 1);
      #line 295 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1360
      = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1324(_M0L45moonbit__test__driver__internal__parse__int__S1324, _M0L6_2atmpS3333);
      _M0L1iS1361 = _M0L5startS1359;
      while (1) {
        if (_M0L1iS1361 < _M0L3endS1360) {
          struct _M0TUsiE* _M0L8_2atupleS3331;
          int32_t _M0L6_2atmpS3332;
          moonbit_incref(_M0L4fileS1356);
          _M0L8_2atupleS3331
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3331)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3331->$0 = _M0L4fileS1356;
          _M0L8_2atupleS3331->$1 = _M0L1iS1361;
          moonbit_incref(_M0L16file__and__indexS1349);
          #line 297 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1349, _M0L8_2atupleS3331);
          _M0L6_2atmpS3332 = _M0L1iS1361 + 1;
          _M0L1iS1361 = _M0L6_2atmpS3332;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1356);
        }
        break;
      }
      _M0L6_2atmpS3335 = _M0L2__S1353 + 1;
      _M0L2__S1353 = _M0L6_2atmpS3335;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1351);
    }
    break;
  }
  return _M0L16file__and__indexS1349;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1342(
  int32_t _M0L6_2aenvS3312,
  moonbit_string_t _M0L1sS1343,
  int32_t _M0L3sepS1344
) {
  moonbit_string_t* _M0L6_2atmpS3330;
  struct _M0TPB5ArrayGsE* _M0L3resS1345;
  struct _M0TPC13ref3RefGiE* _M0L1iS1346;
  struct _M0TPC13ref3RefGiE* _M0L5startS1347;
  int32_t _M0L3valS3325;
  int32_t _M0L6_2atmpS3326;
  #line 261 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3330 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1345
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1345)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1345->$0 = _M0L6_2atmpS3330;
  _M0L3resS1345->$1 = 0;
  _M0L1iS1346
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1346)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1346->$0 = 0;
  _M0L5startS1347
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1347)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1347->$0 = 0;
  while (1) {
    int32_t _M0L3valS3313 = _M0L1iS1346->$0;
    int32_t _M0L6_2atmpS3314 = Moonbit_array_length(_M0L1sS1343);
    if (_M0L3valS3313 < _M0L6_2atmpS3314) {
      int32_t _M0L3valS3317 = _M0L1iS1346->$0;
      int32_t _M0L6_2atmpS3316;
      int32_t _M0L6_2atmpS3315;
      int32_t _M0L3valS3324;
      int32_t _M0L6_2atmpS3323;
      if (
        _M0L3valS3317 < 0
        || _M0L3valS3317 >= Moonbit_array_length(_M0L1sS1343)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3316 = _M0L1sS1343[_M0L3valS3317];
      _M0L6_2atmpS3315 = _M0L6_2atmpS3316;
      if (_M0L6_2atmpS3315 == _M0L3sepS1344) {
        int32_t _M0L3valS3319 = _M0L5startS1347->$0;
        int32_t _M0L3valS3320 = _M0L1iS1346->$0;
        moonbit_string_t _M0L6_2atmpS3318;
        int32_t _M0L3valS3322;
        int32_t _M0L6_2atmpS3321;
        moonbit_incref(_M0L1sS1343);
        #line 270 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3318
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1343, _M0L3valS3319, _M0L3valS3320);
        moonbit_incref(_M0L3resS1345);
        #line 270 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1345, _M0L6_2atmpS3318);
        _M0L3valS3322 = _M0L1iS1346->$0;
        _M0L6_2atmpS3321 = _M0L3valS3322 + 1;
        _M0L5startS1347->$0 = _M0L6_2atmpS3321;
      }
      _M0L3valS3324 = _M0L1iS1346->$0;
      _M0L6_2atmpS3323 = _M0L3valS3324 + 1;
      _M0L1iS1346->$0 = _M0L6_2atmpS3323;
      continue;
    } else {
      moonbit_decref(_M0L1iS1346);
    }
    break;
  }
  _M0L3valS3325 = _M0L5startS1347->$0;
  _M0L6_2atmpS3326 = Moonbit_array_length(_M0L1sS1343);
  if (_M0L3valS3325 < _M0L6_2atmpS3326) {
    int32_t _M0L8_2afieldS3415 = _M0L5startS1347->$0;
    int32_t _M0L3valS3328;
    int32_t _M0L6_2atmpS3329;
    moonbit_string_t _M0L6_2atmpS3327;
    moonbit_decref(_M0L5startS1347);
    _M0L3valS3328 = _M0L8_2afieldS3415;
    _M0L6_2atmpS3329 = Moonbit_array_length(_M0L1sS1343);
    #line 276 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3327
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1343, _M0L3valS3328, _M0L6_2atmpS3329);
    moonbit_incref(_M0L3resS1345);
    #line 276 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1345, _M0L6_2atmpS3327);
  } else {
    moonbit_decref(_M0L5startS1347);
    moonbit_decref(_M0L1sS1343);
  }
  return _M0L3resS1345;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1337(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330
) {
  moonbit_bytes_t* _M0L3tmpS1338;
  int32_t _M0L6_2atmpS3311;
  struct _M0TPB5ArrayGsE* _M0L3resS1339;
  int32_t _M0L1iS1340;
  #line 250 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1338
  = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3311 = Moonbit_array_length(_M0L3tmpS1338);
  #line 254 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1339 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3311);
  _M0L1iS1340 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3307 = Moonbit_array_length(_M0L3tmpS1338);
    if (_M0L1iS1340 < _M0L6_2atmpS3307) {
      moonbit_bytes_t _M0L6_2atmpS3416;
      moonbit_bytes_t _M0L6_2atmpS3309;
      moonbit_string_t _M0L6_2atmpS3308;
      int32_t _M0L6_2atmpS3310;
      if (
        _M0L1iS1340 < 0 || _M0L1iS1340 >= Moonbit_array_length(_M0L3tmpS1338)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3416 = (moonbit_bytes_t)_M0L3tmpS1338[_M0L1iS1340];
      _M0L6_2atmpS3309 = _M0L6_2atmpS3416;
      moonbit_incref(_M0L6_2atmpS3309);
      #line 256 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3308
      = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330, _M0L6_2atmpS3309);
      moonbit_incref(_M0L3resS1339);
      #line 256 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1339, _M0L6_2atmpS3308);
      _M0L6_2atmpS3310 = _M0L1iS1340 + 1;
      _M0L1iS1340 = _M0L6_2atmpS3310;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1338);
    }
    break;
  }
  return _M0L3resS1339;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1330(
  int32_t _M0L6_2aenvS3221,
  moonbit_bytes_t _M0L5bytesS1331
) {
  struct _M0TPB13StringBuilder* _M0L3resS1332;
  int32_t _M0L3lenS1333;
  struct _M0TPC13ref3RefGiE* _M0L1iS1334;
  #line 206 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1332 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1333 = Moonbit_array_length(_M0L5bytesS1331);
  _M0L1iS1334
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1334)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1334->$0 = 0;
  while (1) {
    int32_t _M0L3valS3222 = _M0L1iS1334->$0;
    if (_M0L3valS3222 < _M0L3lenS1333) {
      int32_t _M0L3valS3306 = _M0L1iS1334->$0;
      int32_t _M0L6_2atmpS3305;
      int32_t _M0L6_2atmpS3304;
      struct _M0TPC13ref3RefGiE* _M0L1cS1335;
      int32_t _M0L3valS3223;
      if (
        _M0L3valS3306 < 0
        || _M0L3valS3306 >= Moonbit_array_length(_M0L5bytesS1331)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3305 = _M0L5bytesS1331[_M0L3valS3306];
      _M0L6_2atmpS3304 = (int32_t)_M0L6_2atmpS3305;
      _M0L1cS1335
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1335)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1335->$0 = _M0L6_2atmpS3304;
      _M0L3valS3223 = _M0L1cS1335->$0;
      if (_M0L3valS3223 < 128) {
        int32_t _M0L8_2afieldS3417 = _M0L1cS1335->$0;
        int32_t _M0L3valS3225;
        int32_t _M0L6_2atmpS3224;
        int32_t _M0L3valS3227;
        int32_t _M0L6_2atmpS3226;
        moonbit_decref(_M0L1cS1335);
        _M0L3valS3225 = _M0L8_2afieldS3417;
        _M0L6_2atmpS3224 = _M0L3valS3225;
        moonbit_incref(_M0L3resS1332);
        #line 215 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1332, _M0L6_2atmpS3224);
        _M0L3valS3227 = _M0L1iS1334->$0;
        _M0L6_2atmpS3226 = _M0L3valS3227 + 1;
        _M0L1iS1334->$0 = _M0L6_2atmpS3226;
      } else {
        int32_t _M0L3valS3228 = _M0L1cS1335->$0;
        if (_M0L3valS3228 < 224) {
          int32_t _M0L3valS3230 = _M0L1iS1334->$0;
          int32_t _M0L6_2atmpS3229 = _M0L3valS3230 + 1;
          int32_t _M0L3valS3239;
          int32_t _M0L6_2atmpS3238;
          int32_t _M0L6_2atmpS3232;
          int32_t _M0L3valS3237;
          int32_t _M0L6_2atmpS3236;
          int32_t _M0L6_2atmpS3235;
          int32_t _M0L6_2atmpS3234;
          int32_t _M0L6_2atmpS3233;
          int32_t _M0L6_2atmpS3231;
          int32_t _M0L8_2afieldS3418;
          int32_t _M0L3valS3241;
          int32_t _M0L6_2atmpS3240;
          int32_t _M0L3valS3243;
          int32_t _M0L6_2atmpS3242;
          if (_M0L6_2atmpS3229 >= _M0L3lenS1333) {
            moonbit_decref(_M0L1cS1335);
            moonbit_decref(_M0L1iS1334);
            moonbit_decref(_M0L5bytesS1331);
            break;
          }
          _M0L3valS3239 = _M0L1cS1335->$0;
          _M0L6_2atmpS3238 = _M0L3valS3239 & 31;
          _M0L6_2atmpS3232 = _M0L6_2atmpS3238 << 6;
          _M0L3valS3237 = _M0L1iS1334->$0;
          _M0L6_2atmpS3236 = _M0L3valS3237 + 1;
          if (
            _M0L6_2atmpS3236 < 0
            || _M0L6_2atmpS3236 >= Moonbit_array_length(_M0L5bytesS1331)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3235 = _M0L5bytesS1331[_M0L6_2atmpS3236];
          _M0L6_2atmpS3234 = (int32_t)_M0L6_2atmpS3235;
          _M0L6_2atmpS3233 = _M0L6_2atmpS3234 & 63;
          _M0L6_2atmpS3231 = _M0L6_2atmpS3232 | _M0L6_2atmpS3233;
          _M0L1cS1335->$0 = _M0L6_2atmpS3231;
          _M0L8_2afieldS3418 = _M0L1cS1335->$0;
          moonbit_decref(_M0L1cS1335);
          _M0L3valS3241 = _M0L8_2afieldS3418;
          _M0L6_2atmpS3240 = _M0L3valS3241;
          moonbit_incref(_M0L3resS1332);
          #line 222 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1332, _M0L6_2atmpS3240);
          _M0L3valS3243 = _M0L1iS1334->$0;
          _M0L6_2atmpS3242 = _M0L3valS3243 + 2;
          _M0L1iS1334->$0 = _M0L6_2atmpS3242;
        } else {
          int32_t _M0L3valS3244 = _M0L1cS1335->$0;
          if (_M0L3valS3244 < 240) {
            int32_t _M0L3valS3246 = _M0L1iS1334->$0;
            int32_t _M0L6_2atmpS3245 = _M0L3valS3246 + 2;
            int32_t _M0L3valS3262;
            int32_t _M0L6_2atmpS3261;
            int32_t _M0L6_2atmpS3254;
            int32_t _M0L3valS3260;
            int32_t _M0L6_2atmpS3259;
            int32_t _M0L6_2atmpS3258;
            int32_t _M0L6_2atmpS3257;
            int32_t _M0L6_2atmpS3256;
            int32_t _M0L6_2atmpS3255;
            int32_t _M0L6_2atmpS3248;
            int32_t _M0L3valS3253;
            int32_t _M0L6_2atmpS3252;
            int32_t _M0L6_2atmpS3251;
            int32_t _M0L6_2atmpS3250;
            int32_t _M0L6_2atmpS3249;
            int32_t _M0L6_2atmpS3247;
            int32_t _M0L8_2afieldS3419;
            int32_t _M0L3valS3264;
            int32_t _M0L6_2atmpS3263;
            int32_t _M0L3valS3266;
            int32_t _M0L6_2atmpS3265;
            if (_M0L6_2atmpS3245 >= _M0L3lenS1333) {
              moonbit_decref(_M0L1cS1335);
              moonbit_decref(_M0L1iS1334);
              moonbit_decref(_M0L5bytesS1331);
              break;
            }
            _M0L3valS3262 = _M0L1cS1335->$0;
            _M0L6_2atmpS3261 = _M0L3valS3262 & 15;
            _M0L6_2atmpS3254 = _M0L6_2atmpS3261 << 12;
            _M0L3valS3260 = _M0L1iS1334->$0;
            _M0L6_2atmpS3259 = _M0L3valS3260 + 1;
            if (
              _M0L6_2atmpS3259 < 0
              || _M0L6_2atmpS3259 >= Moonbit_array_length(_M0L5bytesS1331)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3258 = _M0L5bytesS1331[_M0L6_2atmpS3259];
            _M0L6_2atmpS3257 = (int32_t)_M0L6_2atmpS3258;
            _M0L6_2atmpS3256 = _M0L6_2atmpS3257 & 63;
            _M0L6_2atmpS3255 = _M0L6_2atmpS3256 << 6;
            _M0L6_2atmpS3248 = _M0L6_2atmpS3254 | _M0L6_2atmpS3255;
            _M0L3valS3253 = _M0L1iS1334->$0;
            _M0L6_2atmpS3252 = _M0L3valS3253 + 2;
            if (
              _M0L6_2atmpS3252 < 0
              || _M0L6_2atmpS3252 >= Moonbit_array_length(_M0L5bytesS1331)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3251 = _M0L5bytesS1331[_M0L6_2atmpS3252];
            _M0L6_2atmpS3250 = (int32_t)_M0L6_2atmpS3251;
            _M0L6_2atmpS3249 = _M0L6_2atmpS3250 & 63;
            _M0L6_2atmpS3247 = _M0L6_2atmpS3248 | _M0L6_2atmpS3249;
            _M0L1cS1335->$0 = _M0L6_2atmpS3247;
            _M0L8_2afieldS3419 = _M0L1cS1335->$0;
            moonbit_decref(_M0L1cS1335);
            _M0L3valS3264 = _M0L8_2afieldS3419;
            _M0L6_2atmpS3263 = _M0L3valS3264;
            moonbit_incref(_M0L3resS1332);
            #line 231 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1332, _M0L6_2atmpS3263);
            _M0L3valS3266 = _M0L1iS1334->$0;
            _M0L6_2atmpS3265 = _M0L3valS3266 + 3;
            _M0L1iS1334->$0 = _M0L6_2atmpS3265;
          } else {
            int32_t _M0L3valS3268 = _M0L1iS1334->$0;
            int32_t _M0L6_2atmpS3267 = _M0L3valS3268 + 3;
            int32_t _M0L3valS3291;
            int32_t _M0L6_2atmpS3290;
            int32_t _M0L6_2atmpS3283;
            int32_t _M0L3valS3289;
            int32_t _M0L6_2atmpS3288;
            int32_t _M0L6_2atmpS3287;
            int32_t _M0L6_2atmpS3286;
            int32_t _M0L6_2atmpS3285;
            int32_t _M0L6_2atmpS3284;
            int32_t _M0L6_2atmpS3276;
            int32_t _M0L3valS3282;
            int32_t _M0L6_2atmpS3281;
            int32_t _M0L6_2atmpS3280;
            int32_t _M0L6_2atmpS3279;
            int32_t _M0L6_2atmpS3278;
            int32_t _M0L6_2atmpS3277;
            int32_t _M0L6_2atmpS3270;
            int32_t _M0L3valS3275;
            int32_t _M0L6_2atmpS3274;
            int32_t _M0L6_2atmpS3273;
            int32_t _M0L6_2atmpS3272;
            int32_t _M0L6_2atmpS3271;
            int32_t _M0L6_2atmpS3269;
            int32_t _M0L3valS3293;
            int32_t _M0L6_2atmpS3292;
            int32_t _M0L3valS3297;
            int32_t _M0L6_2atmpS3296;
            int32_t _M0L6_2atmpS3295;
            int32_t _M0L6_2atmpS3294;
            int32_t _M0L8_2afieldS3420;
            int32_t _M0L3valS3301;
            int32_t _M0L6_2atmpS3300;
            int32_t _M0L6_2atmpS3299;
            int32_t _M0L6_2atmpS3298;
            int32_t _M0L3valS3303;
            int32_t _M0L6_2atmpS3302;
            if (_M0L6_2atmpS3267 >= _M0L3lenS1333) {
              moonbit_decref(_M0L1cS1335);
              moonbit_decref(_M0L1iS1334);
              moonbit_decref(_M0L5bytesS1331);
              break;
            }
            _M0L3valS3291 = _M0L1cS1335->$0;
            _M0L6_2atmpS3290 = _M0L3valS3291 & 7;
            _M0L6_2atmpS3283 = _M0L6_2atmpS3290 << 18;
            _M0L3valS3289 = _M0L1iS1334->$0;
            _M0L6_2atmpS3288 = _M0L3valS3289 + 1;
            if (
              _M0L6_2atmpS3288 < 0
              || _M0L6_2atmpS3288 >= Moonbit_array_length(_M0L5bytesS1331)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3287 = _M0L5bytesS1331[_M0L6_2atmpS3288];
            _M0L6_2atmpS3286 = (int32_t)_M0L6_2atmpS3287;
            _M0L6_2atmpS3285 = _M0L6_2atmpS3286 & 63;
            _M0L6_2atmpS3284 = _M0L6_2atmpS3285 << 12;
            _M0L6_2atmpS3276 = _M0L6_2atmpS3283 | _M0L6_2atmpS3284;
            _M0L3valS3282 = _M0L1iS1334->$0;
            _M0L6_2atmpS3281 = _M0L3valS3282 + 2;
            if (
              _M0L6_2atmpS3281 < 0
              || _M0L6_2atmpS3281 >= Moonbit_array_length(_M0L5bytesS1331)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3280 = _M0L5bytesS1331[_M0L6_2atmpS3281];
            _M0L6_2atmpS3279 = (int32_t)_M0L6_2atmpS3280;
            _M0L6_2atmpS3278 = _M0L6_2atmpS3279 & 63;
            _M0L6_2atmpS3277 = _M0L6_2atmpS3278 << 6;
            _M0L6_2atmpS3270 = _M0L6_2atmpS3276 | _M0L6_2atmpS3277;
            _M0L3valS3275 = _M0L1iS1334->$0;
            _M0L6_2atmpS3274 = _M0L3valS3275 + 3;
            if (
              _M0L6_2atmpS3274 < 0
              || _M0L6_2atmpS3274 >= Moonbit_array_length(_M0L5bytesS1331)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3273 = _M0L5bytesS1331[_M0L6_2atmpS3274];
            _M0L6_2atmpS3272 = (int32_t)_M0L6_2atmpS3273;
            _M0L6_2atmpS3271 = _M0L6_2atmpS3272 & 63;
            _M0L6_2atmpS3269 = _M0L6_2atmpS3270 | _M0L6_2atmpS3271;
            _M0L1cS1335->$0 = _M0L6_2atmpS3269;
            _M0L3valS3293 = _M0L1cS1335->$0;
            _M0L6_2atmpS3292 = _M0L3valS3293 - 65536;
            _M0L1cS1335->$0 = _M0L6_2atmpS3292;
            _M0L3valS3297 = _M0L1cS1335->$0;
            _M0L6_2atmpS3296 = _M0L3valS3297 >> 10;
            _M0L6_2atmpS3295 = _M0L6_2atmpS3296 + 55296;
            _M0L6_2atmpS3294 = _M0L6_2atmpS3295;
            moonbit_incref(_M0L3resS1332);
            #line 242 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1332, _M0L6_2atmpS3294);
            _M0L8_2afieldS3420 = _M0L1cS1335->$0;
            moonbit_decref(_M0L1cS1335);
            _M0L3valS3301 = _M0L8_2afieldS3420;
            _M0L6_2atmpS3300 = _M0L3valS3301 & 1023;
            _M0L6_2atmpS3299 = _M0L6_2atmpS3300 + 56320;
            _M0L6_2atmpS3298 = _M0L6_2atmpS3299;
            moonbit_incref(_M0L3resS1332);
            #line 243 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1332, _M0L6_2atmpS3298);
            _M0L3valS3303 = _M0L1iS1334->$0;
            _M0L6_2atmpS3302 = _M0L3valS3303 + 4;
            _M0L1iS1334->$0 = _M0L6_2atmpS3302;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1334);
      moonbit_decref(_M0L5bytesS1331);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1332);
}

int32_t _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1324(
  int32_t _M0L6_2aenvS3214,
  moonbit_string_t _M0L1sS1325
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1326;
  int32_t _M0L3lenS1327;
  int32_t _M0L1iS1328;
  int32_t _M0L8_2afieldS3421;
  #line 197 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1326
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1326)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1326->$0 = 0;
  _M0L3lenS1327 = Moonbit_array_length(_M0L1sS1325);
  _M0L1iS1328 = 0;
  while (1) {
    if (_M0L1iS1328 < _M0L3lenS1327) {
      int32_t _M0L3valS3219 = _M0L3resS1326->$0;
      int32_t _M0L6_2atmpS3216 = _M0L3valS3219 * 10;
      int32_t _M0L6_2atmpS3218;
      int32_t _M0L6_2atmpS3217;
      int32_t _M0L6_2atmpS3215;
      int32_t _M0L6_2atmpS3220;
      if (
        _M0L1iS1328 < 0 || _M0L1iS1328 >= Moonbit_array_length(_M0L1sS1325)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3218 = _M0L1sS1325[_M0L1iS1328];
      _M0L6_2atmpS3217 = _M0L6_2atmpS3218 - 48;
      _M0L6_2atmpS3215 = _M0L6_2atmpS3216 + _M0L6_2atmpS3217;
      _M0L3resS1326->$0 = _M0L6_2atmpS3215;
      _M0L6_2atmpS3220 = _M0L1iS1328 + 1;
      _M0L1iS1328 = _M0L6_2atmpS3220;
      continue;
    } else {
      moonbit_decref(_M0L1sS1325);
    }
    break;
  }
  _M0L8_2afieldS3421 = _M0L3resS1326->$0;
  moonbit_decref(_M0L3resS1326);
  return _M0L8_2afieldS3421;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1304,
  moonbit_string_t _M0L12_2adiscard__S1305,
  int32_t _M0L12_2adiscard__S1306,
  struct _M0TWssbEu* _M0L12_2adiscard__S1307,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1308
) {
  struct moonbit_result_0 _result_3915;
  #line 34 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1308);
  moonbit_decref(_M0L12_2adiscard__S1307);
  moonbit_decref(_M0L12_2adiscard__S1305);
  moonbit_decref(_M0L12_2adiscard__S1304);
  _result_3915.tag = 1;
  _result_3915.data.ok = 0;
  return _result_3915;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1309,
  moonbit_string_t _M0L12_2adiscard__S1310,
  int32_t _M0L12_2adiscard__S1311,
  struct _M0TWssbEu* _M0L12_2adiscard__S1312,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1313
) {
  struct moonbit_result_0 _result_3916;
  #line 34 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1313);
  moonbit_decref(_M0L12_2adiscard__S1312);
  moonbit_decref(_M0L12_2adiscard__S1310);
  moonbit_decref(_M0L12_2adiscard__S1309);
  _result_3916.tag = 1;
  _result_3916.data.ok = 0;
  return _result_3916;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1314,
  moonbit_string_t _M0L12_2adiscard__S1315,
  int32_t _M0L12_2adiscard__S1316,
  struct _M0TWssbEu* _M0L12_2adiscard__S1317,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1318
) {
  struct moonbit_result_0 _result_3917;
  #line 34 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1318);
  moonbit_decref(_M0L12_2adiscard__S1317);
  moonbit_decref(_M0L12_2adiscard__S1315);
  moonbit_decref(_M0L12_2adiscard__S1314);
  _result_3917.tag = 1;
  _result_3917.data.ok = 0;
  return _result_3917;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools4todo21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools4todo50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1319,
  moonbit_string_t _M0L12_2adiscard__S1320,
  int32_t _M0L12_2adiscard__S1321,
  struct _M0TWssbEu* _M0L12_2adiscard__S1322,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1323
) {
  struct moonbit_result_0 _result_3918;
  #line 34 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1323);
  moonbit_decref(_M0L12_2adiscard__S1322);
  moonbit_decref(_M0L12_2adiscard__S1320);
  moonbit_decref(_M0L12_2adiscard__S1319);
  _result_3918.tag = 1;
  _result_3918.data.ok = 0;
  return _result_3918;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools4todo28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools4todo34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1303
) {
  #line 12 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1303);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo35____test__6d616e616765722e6d6274__1(
  
) {
  moonbit_string_t* _M0L6_2atmpS3213;
  struct _M0TPB5ArrayGsE* _M0L4dataS1295;
  struct _M0TUsRPC16string10StringViewE** _M0L7_2abindS1297;
  struct _M0TUsRPC16string10StringViewE** _M0L6_2atmpS3212;
  struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE _M0L6_2atmpS3211;
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L6outputS1296;
  int32_t _M0L7_2abindS1298;
  int32_t _M0L2__S1299;
  struct _M0TPB6ToJson _M0L6_2atmpS3175;
  void* _M0L6_2atmpS3210;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3187;
  void* _M0L6_2atmpS3209;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3188;
  void* _M0L6_2atmpS3208;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3189;
  void* _M0L6_2atmpS3207;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3190;
  void* _M0L6_2atmpS3206;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3191;
  void* _M0L6_2atmpS3205;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3192;
  void* _M0L6_2atmpS3204;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3193;
  void* _M0L6_2atmpS3203;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3194;
  void* _M0L6_2atmpS3202;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3195;
  void* _M0L6_2atmpS3201;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3196;
  void* _M0L6_2atmpS3200;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3197;
  void* _M0L6_2atmpS3199;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3198;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1302;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3186;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3185;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3184;
  void* _M0L6_2atmpS3183;
  void* _M0L6_2atmpS3176;
  moonbit_string_t _M0L6_2atmpS3179;
  moonbit_string_t _M0L6_2atmpS3180;
  moonbit_string_t _M0L6_2atmpS3181;
  moonbit_string_t _M0L6_2atmpS3182;
  moonbit_string_t* _M0L6_2atmpS3178;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3177;
  #line 202 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3213 = (moonbit_string_t*)moonbit_make_ref_array_raw(12);
  _M0L6_2atmpS3213[0] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3213[1] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3213[2] = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3213[3] = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3213[4] = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3213[5] = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3213[6] = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3213[7] = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3213[8] = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3213[9] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3213[10] = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3213[11] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L4dataS1295
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L4dataS1295)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L4dataS1295->$0 = _M0L6_2atmpS3213;
  _M0L4dataS1295->$1 = 12;
  _M0L7_2abindS1297
  = (struct _M0TUsRPC16string10StringViewE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3212 = _M0L7_2abindS1297;
  _M0L6_2atmpS3211
  = (struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE){
    0, 0, _M0L6_2atmpS3212
  };
  #line 208 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6outputS1296
  = _M0MPB3Map11from__arrayGsRPC16string10StringViewE(_M0L6_2atmpS3211);
  _M0L7_2abindS1298 = _M0L4dataS1295->$1;
  _M0L2__S1299 = 0;
  while (1) {
    if (_M0L2__S1299 < _M0L7_2abindS1298) {
      moonbit_string_t* _M0L8_2afieldS3423 = _M0L4dataS1295->$0;
      moonbit_string_t* _M0L3bufS3174 = _M0L8_2afieldS3423;
      moonbit_string_t _M0L6_2atmpS3422 =
        (moonbit_string_t)_M0L3bufS3174[_M0L2__S1299];
      moonbit_string_t _M0L1dS1300 = _M0L6_2atmpS3422;
      int32_t _M0L6_2atmpS3172 = Moonbit_array_length(_M0L1dS1300);
      struct _M0TPC16string10StringView _M0L6_2atmpS3171;
      struct _M0TPC16string10StringView _M0L6_2atmpS3170;
      int32_t _M0L6_2atmpS3173;
      moonbit_incref(_M0L1dS1300);
      moonbit_incref(_M0L1dS1300);
      _M0L6_2atmpS3171
      = (struct _M0TPC16string10StringView){
        0, _M0L6_2atmpS3172, _M0L1dS1300
      };
      #line 210 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0L6_2atmpS3170
      = _M0FP48clawteam8clawteam5tools4todo22remove__list__prefixes(_M0L6_2atmpS3171);
      moonbit_incref(_M0L6outputS1296);
      #line 210 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0MPB3Map3setGsRPC16string10StringViewE(_M0L6outputS1296, _M0L1dS1300, _M0L6_2atmpS3170);
      _M0L6_2atmpS3173 = _M0L2__S1299 + 1;
      _M0L2__S1299 = _M0L6_2atmpS3173;
      continue;
    } else {
      moonbit_decref(_M0L4dataS1295);
    }
    break;
  }
  _M0L6_2atmpS3175
  = (struct _M0TPB6ToJson){
    _M0FP0167moonbitlang_2fcore_2fbuiltin_2fMap_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6outputS1296
  };
  #line 213 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3210
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L8_2atupleS3187
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3187)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3187->$0 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L8_2atupleS3187->$1 = _M0L6_2atmpS3210;
  #line 214 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3209
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3188
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3188)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3188->$0 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L8_2atupleS3188->$1 = _M0L6_2atmpS3209;
  #line 215 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3208
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L8_2atupleS3189
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3189)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3189->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3189->$1 = _M0L6_2atmpS3208;
  #line 216 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3207
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_21.data);
  _M0L8_2atupleS3190
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3190)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3190->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3190->$1 = _M0L6_2atmpS3207;
  #line 217 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3206
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3191
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3191)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3191->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS3191->$1 = _M0L6_2atmpS3206;
  #line 218 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3205
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS3192
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3192)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3192->$0 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L8_2atupleS3192->$1 = _M0L6_2atmpS3205;
  #line 219 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3204
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_24.data);
  _M0L8_2atupleS3193
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3193)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3193->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L8_2atupleS3193->$1 = _M0L6_2atmpS3204;
  #line 220 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3203
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3194
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3194)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3194->$0 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L8_2atupleS3194->$1 = _M0L6_2atmpS3203;
  #line 221 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3202
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_17.data);
  _M0L8_2atupleS3195
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3195)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3195->$0 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L8_2atupleS3195->$1 = _M0L6_2atmpS3202;
  #line 222 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3201
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS3196
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3196)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3196->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3196->$1 = _M0L6_2atmpS3201;
  #line 223 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3200
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_27.data);
  _M0L8_2atupleS3197
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3197)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3197->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3197->$1 = _M0L6_2atmpS3200;
  #line 224 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3199
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_28.data);
  _M0L8_2atupleS3198
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3198)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3198->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3198->$1 = _M0L6_2atmpS3199;
  _M0L7_2abindS1302
  = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(12);
  _M0L7_2abindS1302[0] = _M0L8_2atupleS3187;
  _M0L7_2abindS1302[1] = _M0L8_2atupleS3188;
  _M0L7_2abindS1302[2] = _M0L8_2atupleS3189;
  _M0L7_2abindS1302[3] = _M0L8_2atupleS3190;
  _M0L7_2abindS1302[4] = _M0L8_2atupleS3191;
  _M0L7_2abindS1302[5] = _M0L8_2atupleS3192;
  _M0L7_2abindS1302[6] = _M0L8_2atupleS3193;
  _M0L7_2abindS1302[7] = _M0L8_2atupleS3194;
  _M0L7_2abindS1302[8] = _M0L8_2atupleS3195;
  _M0L7_2abindS1302[9] = _M0L8_2atupleS3196;
  _M0L7_2abindS1302[10] = _M0L8_2atupleS3197;
  _M0L7_2abindS1302[11] = _M0L8_2atupleS3198;
  _M0L6_2atmpS3186 = _M0L7_2abindS1302;
  _M0L6_2atmpS3185
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 12, _M0L6_2atmpS3186
  };
  #line 212 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3184 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3185);
  #line 212 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3183 = _M0MPC14json4Json6object(_M0L6_2atmpS3184);
  _M0L6_2atmpS3176 = _M0L6_2atmpS3183;
  _M0L6_2atmpS3179 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3180 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3181 = 0;
  _M0L6_2atmpS3182 = 0;
  _M0L6_2atmpS3178 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3178[0] = _M0L6_2atmpS3179;
  _M0L6_2atmpS3178[1] = _M0L6_2atmpS3180;
  _M0L6_2atmpS3178[2] = _M0L6_2atmpS3181;
  _M0L6_2atmpS3178[3] = _M0L6_2atmpS3182;
  _M0L6_2atmpS3177
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3177)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3177->$0 = _M0L6_2atmpS3178;
  _M0L6_2atmpS3177->$1 = 4;
  #line 212 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3175, _M0L6_2atmpS3176, (moonbit_string_t)moonbit_string_literal_31.data, _M0L6_2atmpS3177);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools4todo35____test__6d616e616765722e6d6274__0(
  
) {
  moonbit_string_t _M0L5tasksS1294;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3169;
  struct _M0TPB6ToJson _M0L6_2atmpS3156;
  void* _M0L6_2atmpS3167;
  void* _M0L6_2atmpS3168;
  void** _M0L6_2atmpS3166;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3165;
  void* _M0L6_2atmpS3164;
  void* _M0L6_2atmpS3157;
  moonbit_string_t _M0L6_2atmpS3160;
  moonbit_string_t _M0L6_2atmpS3161;
  moonbit_string_t _M0L6_2atmpS3162;
  moonbit_string_t _M0L6_2atmpS3163;
  moonbit_string_t* _M0L6_2atmpS3159;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3158;
  #line 134 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L5tasksS1294 = (moonbit_string_t)moonbit_string_literal_32.data;
  #line 141 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3169
  = _M0FP48clawteam8clawteam5tools4todo19extract__task__tags(_M0L5tasksS1294);
  _M0L6_2atmpS3156
  = (struct _M0TPB6ToJson){
    _M0FP0123moonbitlang_2fcore_2fbuiltin_2fArray_5bString_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3169
  };
  #line 141 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3167
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  #line 141 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3168
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L6_2atmpS3166 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3166[0] = _M0L6_2atmpS3167;
  _M0L6_2atmpS3166[1] = _M0L6_2atmpS3168;
  _M0L6_2atmpS3165
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3165)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3165->$0 = _M0L6_2atmpS3166;
  _M0L6_2atmpS3165->$1 = 2;
  #line 141 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3164 = _M0MPC14json4Json5array(_M0L6_2atmpS3165);
  _M0L6_2atmpS3157 = _M0L6_2atmpS3164;
  _M0L6_2atmpS3160 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3161 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3162 = 0;
  _M0L6_2atmpS3163 = 0;
  _M0L6_2atmpS3159 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3159[0] = _M0L6_2atmpS3160;
  _M0L6_2atmpS3159[1] = _M0L6_2atmpS3161;
  _M0L6_2atmpS3159[2] = _M0L6_2atmpS3162;
  _M0L6_2atmpS3159[3] = _M0L6_2atmpS3163;
  _M0L6_2atmpS3158
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3158)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3158->$0 = _M0L6_2atmpS3159;
  _M0L6_2atmpS3158->$1 = 4;
  #line 141 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3156, _M0L6_2atmpS3157, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS3158);
}

struct _M0TPC16string10StringView _M0FP48clawteam8clawteam5tools4todo22remove__list__prefixes(
  struct _M0TPC16string10StringView _M0L4lineS1269
) {
  moonbit_string_t _M0L7_2adataS1268;
  int32_t _M0L8_2astartS1270;
  int32_t _M0L6_2atmpS3155;
  int32_t _M0L6_2aendS1271;
  int32_t _M0Lm9_2acursorS1272;
  int32_t _M0Lm13accept__stateS1273;
  int32_t _M0Lm10match__endS1274;
  struct _M0TPC16string10StringView _M0L4restS1276;
  struct _M0TPC16string10StringView _M0L4restS1278;
  struct _M0TPC16string10StringView _M0L4restS1280;
  int32_t _M0L6_2atmpS3140;
  #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  moonbit_incref(_M0L4lineS1269.$0);
  #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L7_2adataS1268 = _M0MPC16string10StringView4data(_M0L4lineS1269);
  moonbit_incref(_M0L4lineS1269.$0);
  #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L8_2astartS1270
  = _M0MPC16string10StringView13start__offset(_M0L4lineS1269);
  moonbit_incref(_M0L4lineS1269.$0);
  #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3155 = _M0MPC16string10StringView6length(_M0L4lineS1269);
  _M0L6_2aendS1271 = _M0L8_2astartS1270 + _M0L6_2atmpS3155;
  _M0Lm9_2acursorS1272 = _M0L8_2astartS1270;
  _M0Lm13accept__stateS1273 = -1;
  _M0Lm10match__endS1274 = -1;
  _M0L6_2atmpS3140 = _M0Lm9_2acursorS1272;
  if (_M0L6_2atmpS3140 < _M0L6_2aendS1271) {
    int32_t _M0L6_2atmpS3154 = _M0Lm9_2acursorS1272;
    int32_t _M0L10next__charS1282;
    int32_t _M0L6_2atmpS3141;
    moonbit_incref(_M0L7_2adataS1268);
    #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
    _M0L10next__charS1282
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1268, _M0L6_2atmpS3154);
    _M0L6_2atmpS3141 = _M0Lm9_2acursorS1272;
    _M0Lm9_2acursorS1272 = _M0L6_2atmpS3141 + 1;
    if (_M0L10next__charS1282 < 45) {
      if (_M0L10next__charS1282 == 42) {
        while (1) {
          int32_t _M0L6_2atmpS3142;
          _M0Lm13accept__stateS1273 = 0;
          _M0Lm10match__endS1274 = _M0Lm9_2acursorS1272;
          _M0L6_2atmpS3142 = _M0Lm9_2acursorS1272;
          if (_M0L6_2atmpS3142 < _M0L6_2aendS1271) {
            int32_t _M0L6_2atmpS3144 = _M0Lm9_2acursorS1272;
            int32_t _M0L10next__charS1285;
            int32_t _M0L6_2atmpS3143;
            moonbit_incref(_M0L7_2adataS1268);
            #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
            _M0L10next__charS1285
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1268, _M0L6_2atmpS3144);
            _M0L6_2atmpS3143 = _M0Lm9_2acursorS1272;
            _M0Lm9_2acursorS1272 = _M0L6_2atmpS3143 + 1;
            if (_M0L10next__charS1285 < 14) {
              if (_M0L10next__charS1285 < 9) {
                goto join_1281;
              } else {
                goto join_1283;
              }
            } else if (_M0L10next__charS1285 > 31) {
              if (_M0L10next__charS1285 < 33) {
                goto join_1283;
              } else {
                goto join_1281;
              }
            } else {
              goto join_1281;
            }
            join_1283:;
            continue;
          } else {
            goto join_1281;
          }
          break;
        }
      } else {
        goto join_1281;
      }
    } else if (_M0L10next__charS1282 > 45) {
      if (_M0L10next__charS1282 >= 48 && _M0L10next__charS1282 <= 57) {
        while (1) {
          int32_t _M0L6_2atmpS3145 = _M0Lm9_2acursorS1272;
          if (_M0L6_2atmpS3145 < _M0L6_2aendS1271) {
            int32_t _M0L6_2atmpS3150 = _M0Lm9_2acursorS1272;
            int32_t _M0L10next__charS1286;
            int32_t _M0L6_2atmpS3146;
            moonbit_incref(_M0L7_2adataS1268);
            #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
            _M0L10next__charS1286
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1268, _M0L6_2atmpS3150);
            _M0L6_2atmpS3146 = _M0Lm9_2acursorS1272;
            _M0Lm9_2acursorS1272 = _M0L6_2atmpS3146 + 1;
            if (_M0L10next__charS1286 < 47) {
              if (_M0L10next__charS1286 < 46) {
                goto join_1281;
              } else {
                while (1) {
                  int32_t _M0L6_2atmpS3147;
                  _M0Lm13accept__stateS1273 = 2;
                  _M0Lm10match__endS1274 = _M0Lm9_2acursorS1272;
                  _M0L6_2atmpS3147 = _M0Lm9_2acursorS1272;
                  if (_M0L6_2atmpS3147 < _M0L6_2aendS1271) {
                    int32_t _M0L6_2atmpS3149 = _M0Lm9_2acursorS1272;
                    int32_t _M0L10next__charS1289;
                    int32_t _M0L6_2atmpS3148;
                    moonbit_incref(_M0L7_2adataS1268);
                    #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                    _M0L10next__charS1289
                    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1268, _M0L6_2atmpS3149);
                    _M0L6_2atmpS3148 = _M0Lm9_2acursorS1272;
                    _M0Lm9_2acursorS1272 = _M0L6_2atmpS3148 + 1;
                    if (_M0L10next__charS1289 < 14) {
                      if (_M0L10next__charS1289 < 9) {
                        goto join_1281;
                      } else {
                        goto join_1287;
                      }
                    } else if (_M0L10next__charS1289 > 31) {
                      if (_M0L10next__charS1289 < 33) {
                        goto join_1287;
                      } else {
                        goto join_1281;
                      }
                    } else {
                      goto join_1281;
                    }
                    join_1287:;
                    continue;
                  } else {
                    goto join_1281;
                  }
                  break;
                }
              }
            } else if (_M0L10next__charS1286 > 47) {
              if (_M0L10next__charS1286 < 58) {
                continue;
              } else {
                goto join_1281;
              }
            } else {
              goto join_1281;
            }
          } else {
            goto join_1281;
          }
          break;
        }
      } else {
        goto join_1281;
      }
    } else {
      while (1) {
        int32_t _M0L6_2atmpS3151;
        _M0Lm13accept__stateS1273 = 1;
        _M0Lm10match__endS1274 = _M0Lm9_2acursorS1272;
        _M0L6_2atmpS3151 = _M0Lm9_2acursorS1272;
        if (_M0L6_2atmpS3151 < _M0L6_2aendS1271) {
          int32_t _M0L6_2atmpS3153 = _M0Lm9_2acursorS1272;
          int32_t _M0L10next__charS1293;
          int32_t _M0L6_2atmpS3152;
          moonbit_incref(_M0L7_2adataS1268);
          #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
          _M0L10next__charS1293
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1268, _M0L6_2atmpS3153);
          _M0L6_2atmpS3152 = _M0Lm9_2acursorS1272;
          _M0Lm9_2acursorS1272 = _M0L6_2atmpS3152 + 1;
          if (_M0L10next__charS1293 < 14) {
            if (_M0L10next__charS1293 < 9) {
              goto join_1281;
            } else {
              goto join_1291;
            }
          } else if (_M0L10next__charS1293 > 31) {
            if (_M0L10next__charS1293 < 33) {
              goto join_1291;
            } else {
              goto join_1281;
            }
          } else {
            goto join_1281;
          }
          join_1291:;
          continue;
        } else {
          goto join_1281;
        }
        break;
      }
    }
  } else {
    goto join_1281;
  }
  join_1281:;
  switch (_M0Lm13accept__stateS1273) {
    case 2: {
      int32_t _M0L6_2atmpS3131;
      int64_t _M0L6_2atmpS3129;
      int64_t _M0L6_2atmpS3130;
      struct _M0TPC16string10StringView _M0L6_2atmpS3128;
      moonbit_decref(_M0L4lineS1269.$0);
      _M0L6_2atmpS3131 = _M0Lm10match__endS1274;
      _M0L6_2atmpS3129 = (int64_t)_M0L6_2atmpS3131;
      _M0L6_2atmpS3130 = (int64_t)_M0L6_2aendS1271;
      #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0L6_2atmpS3128
      = _M0MPC16string6String4view(_M0L7_2adataS1268, _M0L6_2atmpS3129, _M0L6_2atmpS3130);
      _M0L4restS1280 = _M0L6_2atmpS3128;
      goto join_1279;
      break;
    }
    
    case 1: {
      int32_t _M0L6_2atmpS3135;
      int64_t _M0L6_2atmpS3133;
      int64_t _M0L6_2atmpS3134;
      struct _M0TPC16string10StringView _M0L6_2atmpS3132;
      moonbit_decref(_M0L4lineS1269.$0);
      _M0L6_2atmpS3135 = _M0Lm10match__endS1274;
      _M0L6_2atmpS3133 = (int64_t)_M0L6_2atmpS3135;
      _M0L6_2atmpS3134 = (int64_t)_M0L6_2aendS1271;
      #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0L6_2atmpS3132
      = _M0MPC16string6String4view(_M0L7_2adataS1268, _M0L6_2atmpS3133, _M0L6_2atmpS3134);
      _M0L4restS1278 = _M0L6_2atmpS3132;
      goto join_1277;
      break;
    }
    
    case 0: {
      int32_t _M0L6_2atmpS3139;
      int64_t _M0L6_2atmpS3137;
      int64_t _M0L6_2atmpS3138;
      struct _M0TPC16string10StringView _M0L6_2atmpS3136;
      moonbit_decref(_M0L4lineS1269.$0);
      _M0L6_2atmpS3139 = _M0Lm10match__endS1274;
      _M0L6_2atmpS3137 = (int64_t)_M0L6_2atmpS3139;
      _M0L6_2atmpS3138 = (int64_t)_M0L6_2aendS1271;
      #line 192 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0L6_2atmpS3136
      = _M0MPC16string6String4view(_M0L7_2adataS1268, _M0L6_2atmpS3137, _M0L6_2atmpS3138);
      _M0L4restS1276 = _M0L6_2atmpS3136;
      goto join_1275;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS1268);
      return _M0L4lineS1269;
      break;
    }
  }
  join_1279:;
  return _M0L4restS1280;
  join_1277:;
  return _M0L4restS1278;
  join_1275:;
  return _M0L4restS1276;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools4todo19extract__task__tags(
  moonbit_string_t _M0L7contentS1267
) {
  moonbit_string_t* _M0L6_2atmpS3127;
  struct _M0TPB5ArrayGsE* _M0L5tasksS1204;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS3126;
  struct _M0TPC13ref3RefGORPB13StringBuilderE* _M0L4taskS1205;
  int32_t _M0L6_2atmpS3125;
  struct _M0TPC16string10StringView _M0L6_2atmpS3124;
  struct _M0TPC16string10StringView _M0L8_2aparamS1206;
  #line 107 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
  _M0L6_2atmpS3127 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L5tasksS1204
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L5tasksS1204)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L5tasksS1204->$0 = _M0L6_2atmpS3127;
  _M0L5tasksS1204->$1 = 0;
  _M0L6_2atmpS3126 = 0;
  _M0L4taskS1205
  = (struct _M0TPC13ref3RefGORPB13StringBuilderE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB13StringBuilderE));
  Moonbit_object_header(_M0L4taskS1205)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB13StringBuilderE, $0) >> 2, 1, 0);
  _M0L4taskS1205->$0 = _M0L6_2atmpS3126;
  _M0L6_2atmpS3125 = Moonbit_array_length(_M0L7contentS1267);
  _M0L6_2atmpS3124
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3125, _M0L7contentS1267
  };
  _M0L8_2aparamS1206 = _M0L6_2atmpS3124;
  while (1) {
    struct _M0TPC16string10StringView _M0L4restS1208;
    int32_t _M0L1cS1209;
    struct _M0TPC16string10StringView _M0L4restS1217;
    struct _M0TPC16string10StringView _M0L4restS1224;
    moonbit_string_t _M0L8_2afieldS3499 = _M0L8_2aparamS1206.$0;
    moonbit_string_t _M0L3strS2923 = _M0L8_2afieldS3499;
    int32_t _M0L5startS2924 = _M0L8_2aparamS1206.$1;
    int32_t _M0L3endS2926 = _M0L8_2aparamS1206.$2;
    int64_t _M0L6_2atmpS2925 = (int64_t)_M0L3endS2926;
    struct _M0TPB13StringBuilder* _M0L6_2atmpS2922;
    struct _M0TPB13StringBuilder* _M0L6_2atmpS2921;
    struct _M0TPB13StringBuilder* _M0L6_2aoldS3427;
    struct _M0TPB13StringBuilder* _M0L1tS1219;
    struct _M0TPB13StringBuilder* _M0L8_2afieldS3426;
    struct _M0TPB13StringBuilder* _M0L7_2abindS1220;
    moonbit_string_t _M0L6_2atmpS2919;
    struct _M0TPB13StringBuilder* _M0L6_2atmpS2920;
    struct _M0TPB13StringBuilder* _M0L6_2aoldS3425;
    struct _M0TPB13StringBuilder* _M0L1tS1211;
    struct _M0TPB13StringBuilder* _M0L8_2afieldS3424;
    struct _M0TPB13StringBuilder* _M0L7_2abindS1212;
    moonbit_incref(_M0L3strS2923);
    #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L3strS2923, 6, _M0L5startS2924, _M0L6_2atmpS2925)
    ) {
      moonbit_string_t _M0L8_2afieldS3492 = _M0L8_2aparamS1206.$0;
      moonbit_string_t _M0L3strS3099 = _M0L8_2afieldS3492;
      moonbit_string_t _M0L8_2afieldS3491 = _M0L8_2aparamS1206.$0;
      moonbit_string_t _M0L3strS3102 = _M0L8_2afieldS3491;
      int32_t _M0L5startS3103 = _M0L8_2aparamS1206.$1;
      int32_t _M0L3endS3105 = _M0L8_2aparamS1206.$2;
      int64_t _M0L6_2atmpS3104 = (int64_t)_M0L3endS3105;
      int64_t _M0L6_2atmpS3101;
      int32_t _M0L6_2atmpS3100;
      int32_t _M0L4_2axS1225;
      moonbit_incref(_M0L3strS3102);
      moonbit_incref(_M0L3strS3099);
      #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0L6_2atmpS3101
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3102, 0, _M0L5startS3103, _M0L6_2atmpS3104);
      _M0L6_2atmpS3100 = (int32_t)_M0L6_2atmpS3101;
      #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      _M0L4_2axS1225
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS3099, _M0L6_2atmpS3100);
      if (_M0L4_2axS1225 == 60) {
        moonbit_string_t _M0L8_2afieldS3487 = _M0L8_2aparamS1206.$0;
        moonbit_string_t _M0L3strS3092 = _M0L8_2afieldS3487;
        moonbit_string_t _M0L8_2afieldS3486 = _M0L8_2aparamS1206.$0;
        moonbit_string_t _M0L3strS3095 = _M0L8_2afieldS3486;
        int32_t _M0L5startS3096 = _M0L8_2aparamS1206.$1;
        int32_t _M0L3endS3098 = _M0L8_2aparamS1206.$2;
        int64_t _M0L6_2atmpS3097 = (int64_t)_M0L3endS3098;
        int64_t _M0L6_2atmpS3094;
        int32_t _M0L6_2atmpS3093;
        int32_t _M0L4_2axS1226;
        moonbit_incref(_M0L3strS3095);
        moonbit_incref(_M0L3strS3092);
        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
        _M0L6_2atmpS3094
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3095, 1, _M0L5startS3096, _M0L6_2atmpS3097);
        _M0L6_2atmpS3093 = (int32_t)_M0L6_2atmpS3094;
        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
        _M0L4_2axS1226
        = _M0MPC16string6String16unsafe__char__at(_M0L3strS3092, _M0L6_2atmpS3093);
        if (_M0L4_2axS1226 == 116) {
          moonbit_string_t _M0L8_2afieldS3450 = _M0L8_2aparamS1206.$0;
          moonbit_string_t _M0L3strS3085 = _M0L8_2afieldS3450;
          moonbit_string_t _M0L8_2afieldS3449 = _M0L8_2aparamS1206.$0;
          moonbit_string_t _M0L3strS3088 = _M0L8_2afieldS3449;
          int32_t _M0L5startS3089 = _M0L8_2aparamS1206.$1;
          int32_t _M0L3endS3091 = _M0L8_2aparamS1206.$2;
          int64_t _M0L6_2atmpS3090 = (int64_t)_M0L3endS3091;
          int64_t _M0L6_2atmpS3087;
          int32_t _M0L6_2atmpS3086;
          int32_t _M0L4_2axS1227;
          moonbit_incref(_M0L3strS3088);
          moonbit_incref(_M0L3strS3085);
          #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
          _M0L6_2atmpS3087
          = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3088, 2, _M0L5startS3089, _M0L6_2atmpS3090);
          _M0L6_2atmpS3086 = (int32_t)_M0L6_2atmpS3087;
          #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
          _M0L4_2axS1227
          = _M0MPC16string6String16unsafe__char__at(_M0L3strS3085, _M0L6_2atmpS3086);
          if (_M0L4_2axS1227 == 97) {
            moonbit_string_t _M0L8_2afieldS3445 = _M0L8_2aparamS1206.$0;
            moonbit_string_t _M0L3strS3078 = _M0L8_2afieldS3445;
            moonbit_string_t _M0L8_2afieldS3444 = _M0L8_2aparamS1206.$0;
            moonbit_string_t _M0L3strS3081 = _M0L8_2afieldS3444;
            int32_t _M0L5startS3082 = _M0L8_2aparamS1206.$1;
            int32_t _M0L3endS3084 = _M0L8_2aparamS1206.$2;
            int64_t _M0L6_2atmpS3083 = (int64_t)_M0L3endS3084;
            int64_t _M0L6_2atmpS3080;
            int32_t _M0L6_2atmpS3079;
            int32_t _M0L4_2axS1228;
            moonbit_incref(_M0L3strS3081);
            moonbit_incref(_M0L3strS3078);
            #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
            _M0L6_2atmpS3080
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3081, 3, _M0L5startS3082, _M0L6_2atmpS3083);
            _M0L6_2atmpS3079 = (int32_t)_M0L6_2atmpS3080;
            #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
            _M0L4_2axS1228
            = _M0MPC16string6String16unsafe__char__at(_M0L3strS3078, _M0L6_2atmpS3079);
            if (_M0L4_2axS1228 == 115) {
              moonbit_string_t _M0L8_2afieldS3440 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS3071 = _M0L8_2afieldS3440;
              moonbit_string_t _M0L8_2afieldS3439 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS3074 = _M0L8_2afieldS3439;
              int32_t _M0L5startS3075 = _M0L8_2aparamS1206.$1;
              int32_t _M0L3endS3077 = _M0L8_2aparamS1206.$2;
              int64_t _M0L6_2atmpS3076 = (int64_t)_M0L3endS3077;
              int64_t _M0L6_2atmpS3073;
              int32_t _M0L6_2atmpS3072;
              int32_t _M0L4_2axS1229;
              moonbit_incref(_M0L3strS3074);
              moonbit_incref(_M0L3strS3071);
              #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
              _M0L6_2atmpS3073
              = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3074, 4, _M0L5startS3075, _M0L6_2atmpS3076);
              _M0L6_2atmpS3072 = (int32_t)_M0L6_2atmpS3073;
              #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
              _M0L4_2axS1229
              = _M0MPC16string6String16unsafe__char__at(_M0L3strS3071, _M0L6_2atmpS3072);
              if (_M0L4_2axS1229 == 107) {
                moonbit_string_t _M0L8_2afieldS3435 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS3064 = _M0L8_2afieldS3435;
                moonbit_string_t _M0L8_2afieldS3434 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS3067 = _M0L8_2afieldS3434;
                int32_t _M0L5startS3068 = _M0L8_2aparamS1206.$1;
                int32_t _M0L3endS3070 = _M0L8_2aparamS1206.$2;
                int64_t _M0L6_2atmpS3069 = (int64_t)_M0L3endS3070;
                int64_t _M0L6_2atmpS3066;
                int32_t _M0L6_2atmpS3065;
                int32_t _M0L4_2axS1230;
                moonbit_incref(_M0L3strS3067);
                moonbit_incref(_M0L3strS3064);
                #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                _M0L6_2atmpS3066
                = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3067, 5, _M0L5startS3068, _M0L6_2atmpS3069);
                _M0L6_2atmpS3065 = (int32_t)_M0L6_2atmpS3066;
                #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                _M0L4_2axS1230
                = _M0MPC16string6String16unsafe__char__at(_M0L3strS3064, _M0L6_2atmpS3065);
                if (_M0L4_2axS1230 == 62) {
                  moonbit_string_t _M0L8_2afieldS3430 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS3057 = _M0L8_2afieldS3430;
                  moonbit_string_t _M0L8_2afieldS3429 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS3060 = _M0L8_2afieldS3429;
                  int32_t _M0L5startS3061 = _M0L8_2aparamS1206.$1;
                  int32_t _M0L3endS3063 = _M0L8_2aparamS1206.$2;
                  int64_t _M0L6_2atmpS3062 = (int64_t)_M0L3endS3063;
                  int64_t _M0L7_2abindS1451;
                  int32_t _M0L6_2atmpS3058;
                  int32_t _M0L8_2afieldS3428;
                  int32_t _M0L3endS3059;
                  struct _M0TPC16string10StringView _M0L4_2axS1231;
                  moonbit_incref(_M0L3strS3060);
                  moonbit_incref(_M0L3strS3057);
                  #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                  _M0L7_2abindS1451
                  = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3060, 6, _M0L5startS3061, _M0L6_2atmpS3062);
                  if (_M0L7_2abindS1451 == 4294967296ll) {
                    _M0L6_2atmpS3058 = _M0L8_2aparamS1206.$2;
                  } else {
                    int64_t _M0L7_2aSomeS1232 = _M0L7_2abindS1451;
                    _M0L6_2atmpS3058 = (int32_t)_M0L7_2aSomeS1232;
                  }
                  _M0L8_2afieldS3428 = _M0L8_2aparamS1206.$2;
                  moonbit_decref(_M0L8_2aparamS1206.$0);
                  _M0L3endS3059 = _M0L8_2afieldS3428;
                  _M0L4_2axS1231
                  = (struct _M0TPC16string10StringView){
                    _M0L6_2atmpS3058, _M0L3endS3059, _M0L3strS3057
                  };
                  _M0L4restS1224 = _M0L4_2axS1231;
                  goto join_1223;
                } else {
                  moonbit_string_t _M0L8_2afieldS3433 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS3050 = _M0L8_2afieldS3433;
                  moonbit_string_t _M0L8_2afieldS3432 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS3053 = _M0L8_2afieldS3432;
                  int32_t _M0L5startS3054 = _M0L8_2aparamS1206.$1;
                  int32_t _M0L3endS3056 = _M0L8_2aparamS1206.$2;
                  int64_t _M0L6_2atmpS3055 = (int64_t)_M0L3endS3056;
                  int64_t _M0L7_2abindS1452;
                  int32_t _M0L6_2atmpS3051;
                  int32_t _M0L8_2afieldS3431;
                  int32_t _M0L3endS3052;
                  struct _M0TPC16string10StringView _M0L4_2axS1233;
                  moonbit_incref(_M0L3strS3053);
                  moonbit_incref(_M0L3strS3050);
                  #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                  _M0L7_2abindS1452
                  = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3053, 1, _M0L5startS3054, _M0L6_2atmpS3055);
                  if (_M0L7_2abindS1452 == 4294967296ll) {
                    _M0L6_2atmpS3051 = _M0L8_2aparamS1206.$2;
                  } else {
                    int64_t _M0L7_2aSomeS1234 = _M0L7_2abindS1452;
                    _M0L6_2atmpS3051 = (int32_t)_M0L7_2aSomeS1234;
                  }
                  _M0L8_2afieldS3431 = _M0L8_2aparamS1206.$2;
                  moonbit_decref(_M0L8_2aparamS1206.$0);
                  _M0L3endS3052 = _M0L8_2afieldS3431;
                  _M0L4_2axS1233
                  = (struct _M0TPC16string10StringView){
                    _M0L6_2atmpS3051, _M0L3endS3052, _M0L3strS3050
                  };
                  _M0L4restS1208 = _M0L4_2axS1233;
                  _M0L1cS1209 = _M0L4_2axS1225;
                  goto join_1207;
                }
              } else {
                moonbit_string_t _M0L8_2afieldS3438 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS3043 = _M0L8_2afieldS3438;
                moonbit_string_t _M0L8_2afieldS3437 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS3046 = _M0L8_2afieldS3437;
                int32_t _M0L5startS3047 = _M0L8_2aparamS1206.$1;
                int32_t _M0L3endS3049 = _M0L8_2aparamS1206.$2;
                int64_t _M0L6_2atmpS3048 = (int64_t)_M0L3endS3049;
                int64_t _M0L7_2abindS1453;
                int32_t _M0L6_2atmpS3044;
                int32_t _M0L8_2afieldS3436;
                int32_t _M0L3endS3045;
                struct _M0TPC16string10StringView _M0L4_2axS1235;
                moonbit_incref(_M0L3strS3046);
                moonbit_incref(_M0L3strS3043);
                #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                _M0L7_2abindS1453
                = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3046, 1, _M0L5startS3047, _M0L6_2atmpS3048);
                if (_M0L7_2abindS1453 == 4294967296ll) {
                  _M0L6_2atmpS3044 = _M0L8_2aparamS1206.$2;
                } else {
                  int64_t _M0L7_2aSomeS1236 = _M0L7_2abindS1453;
                  _M0L6_2atmpS3044 = (int32_t)_M0L7_2aSomeS1236;
                }
                _M0L8_2afieldS3436 = _M0L8_2aparamS1206.$2;
                moonbit_decref(_M0L8_2aparamS1206.$0);
                _M0L3endS3045 = _M0L8_2afieldS3436;
                _M0L4_2axS1235
                = (struct _M0TPC16string10StringView){
                  _M0L6_2atmpS3044, _M0L3endS3045, _M0L3strS3043
                };
                _M0L4restS1208 = _M0L4_2axS1235;
                _M0L1cS1209 = _M0L4_2axS1225;
                goto join_1207;
              }
            } else {
              moonbit_string_t _M0L8_2afieldS3443 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS3036 = _M0L8_2afieldS3443;
              moonbit_string_t _M0L8_2afieldS3442 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS3039 = _M0L8_2afieldS3442;
              int32_t _M0L5startS3040 = _M0L8_2aparamS1206.$1;
              int32_t _M0L3endS3042 = _M0L8_2aparamS1206.$2;
              int64_t _M0L6_2atmpS3041 = (int64_t)_M0L3endS3042;
              int64_t _M0L7_2abindS1454;
              int32_t _M0L6_2atmpS3037;
              int32_t _M0L8_2afieldS3441;
              int32_t _M0L3endS3038;
              struct _M0TPC16string10StringView _M0L4_2axS1237;
              moonbit_incref(_M0L3strS3039);
              moonbit_incref(_M0L3strS3036);
              #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
              _M0L7_2abindS1454
              = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3039, 1, _M0L5startS3040, _M0L6_2atmpS3041);
              if (_M0L7_2abindS1454 == 4294967296ll) {
                _M0L6_2atmpS3037 = _M0L8_2aparamS1206.$2;
              } else {
                int64_t _M0L7_2aSomeS1238 = _M0L7_2abindS1454;
                _M0L6_2atmpS3037 = (int32_t)_M0L7_2aSomeS1238;
              }
              _M0L8_2afieldS3441 = _M0L8_2aparamS1206.$2;
              moonbit_decref(_M0L8_2aparamS1206.$0);
              _M0L3endS3038 = _M0L8_2afieldS3441;
              _M0L4_2axS1237
              = (struct _M0TPC16string10StringView){
                _M0L6_2atmpS3037, _M0L3endS3038, _M0L3strS3036
              };
              _M0L4restS1208 = _M0L4_2axS1237;
              _M0L1cS1209 = _M0L4_2axS1225;
              goto join_1207;
            }
          } else {
            moonbit_string_t _M0L8_2afieldS3448 = _M0L8_2aparamS1206.$0;
            moonbit_string_t _M0L3strS3029 = _M0L8_2afieldS3448;
            moonbit_string_t _M0L8_2afieldS3447 = _M0L8_2aparamS1206.$0;
            moonbit_string_t _M0L3strS3032 = _M0L8_2afieldS3447;
            int32_t _M0L5startS3033 = _M0L8_2aparamS1206.$1;
            int32_t _M0L3endS3035 = _M0L8_2aparamS1206.$2;
            int64_t _M0L6_2atmpS3034 = (int64_t)_M0L3endS3035;
            int64_t _M0L7_2abindS1455;
            int32_t _M0L6_2atmpS3030;
            int32_t _M0L8_2afieldS3446;
            int32_t _M0L3endS3031;
            struct _M0TPC16string10StringView _M0L4_2axS1239;
            moonbit_incref(_M0L3strS3032);
            moonbit_incref(_M0L3strS3029);
            #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
            _M0L7_2abindS1455
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3032, 1, _M0L5startS3033, _M0L6_2atmpS3034);
            if (_M0L7_2abindS1455 == 4294967296ll) {
              _M0L6_2atmpS3030 = _M0L8_2aparamS1206.$2;
            } else {
              int64_t _M0L7_2aSomeS1240 = _M0L7_2abindS1455;
              _M0L6_2atmpS3030 = (int32_t)_M0L7_2aSomeS1240;
            }
            _M0L8_2afieldS3446 = _M0L8_2aparamS1206.$2;
            moonbit_decref(_M0L8_2aparamS1206.$0);
            _M0L3endS3031 = _M0L8_2afieldS3446;
            _M0L4_2axS1239
            = (struct _M0TPC16string10StringView){
              _M0L6_2atmpS3030, _M0L3endS3031, _M0L3strS3029
            };
            _M0L4restS1208 = _M0L4_2axS1239;
            _M0L1cS1209 = _M0L4_2axS1225;
            goto join_1207;
          }
        } else {
          moonbit_string_t _M0L8_2afieldS3485 = _M0L8_2aparamS1206.$0;
          moonbit_string_t _M0L3strS2934 = _M0L8_2afieldS3485;
          int32_t _M0L5startS2935 = _M0L8_2aparamS1206.$1;
          int32_t _M0L3endS2937 = _M0L8_2aparamS1206.$2;
          int64_t _M0L6_2atmpS2936 = (int64_t)_M0L3endS2937;
          moonbit_incref(_M0L3strS2934);
          #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
          if (
            _M0MPC16string6String24char__length__ge_2einner(_M0L3strS2934, 7, _M0L5startS2935, _M0L6_2atmpS2936)
          ) {
            if (_M0L4_2axS1226 == 47) {
              moonbit_string_t _M0L8_2afieldS3478 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS3015 = _M0L8_2afieldS3478;
              moonbit_string_t _M0L8_2afieldS3477 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS3018 = _M0L8_2afieldS3477;
              int32_t _M0L5startS3019 = _M0L8_2aparamS1206.$1;
              int32_t _M0L3endS3021 = _M0L8_2aparamS1206.$2;
              int64_t _M0L6_2atmpS3020 = (int64_t)_M0L3endS3021;
              int64_t _M0L6_2atmpS3017;
              int32_t _M0L6_2atmpS3016;
              int32_t _M0L4_2axS1241;
              moonbit_incref(_M0L3strS3018);
              moonbit_incref(_M0L3strS3015);
              #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
              _M0L6_2atmpS3017
              = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3018, 2, _M0L5startS3019, _M0L6_2atmpS3020);
              _M0L6_2atmpS3016 = (int32_t)_M0L6_2atmpS3017;
              #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
              _M0L4_2axS1241
              = _M0MPC16string6String16unsafe__char__at(_M0L3strS3015, _M0L6_2atmpS3016);
              if (_M0L4_2axS1241 == 116) {
                moonbit_string_t _M0L8_2afieldS3473 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS3008 = _M0L8_2afieldS3473;
                moonbit_string_t _M0L8_2afieldS3472 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS3011 = _M0L8_2afieldS3472;
                int32_t _M0L5startS3012 = _M0L8_2aparamS1206.$1;
                int32_t _M0L3endS3014 = _M0L8_2aparamS1206.$2;
                int64_t _M0L6_2atmpS3013 = (int64_t)_M0L3endS3014;
                int64_t _M0L6_2atmpS3010;
                int32_t _M0L6_2atmpS3009;
                int32_t _M0L4_2axS1242;
                moonbit_incref(_M0L3strS3011);
                moonbit_incref(_M0L3strS3008);
                #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                _M0L6_2atmpS3010
                = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3011, 3, _M0L5startS3012, _M0L6_2atmpS3013);
                _M0L6_2atmpS3009 = (int32_t)_M0L6_2atmpS3010;
                #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                _M0L4_2axS1242
                = _M0MPC16string6String16unsafe__char__at(_M0L3strS3008, _M0L6_2atmpS3009);
                if (_M0L4_2axS1242 == 97) {
                  moonbit_string_t _M0L8_2afieldS3468 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS3001 = _M0L8_2afieldS3468;
                  moonbit_string_t _M0L8_2afieldS3467 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS3004 = _M0L8_2afieldS3467;
                  int32_t _M0L5startS3005 = _M0L8_2aparamS1206.$1;
                  int32_t _M0L3endS3007 = _M0L8_2aparamS1206.$2;
                  int64_t _M0L6_2atmpS3006 = (int64_t)_M0L3endS3007;
                  int64_t _M0L6_2atmpS3003;
                  int32_t _M0L6_2atmpS3002;
                  int32_t _M0L4_2axS1243;
                  moonbit_incref(_M0L3strS3004);
                  moonbit_incref(_M0L3strS3001);
                  #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                  _M0L6_2atmpS3003
                  = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3004, 4, _M0L5startS3005, _M0L6_2atmpS3006);
                  _M0L6_2atmpS3002 = (int32_t)_M0L6_2atmpS3003;
                  #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                  _M0L4_2axS1243
                  = _M0MPC16string6String16unsafe__char__at(_M0L3strS3001, _M0L6_2atmpS3002);
                  if (_M0L4_2axS1243 == 115) {
                    moonbit_string_t _M0L8_2afieldS3463 =
                      _M0L8_2aparamS1206.$0;
                    moonbit_string_t _M0L3strS2994 = _M0L8_2afieldS3463;
                    moonbit_string_t _M0L8_2afieldS3462 =
                      _M0L8_2aparamS1206.$0;
                    moonbit_string_t _M0L3strS2997 = _M0L8_2afieldS3462;
                    int32_t _M0L5startS2998 = _M0L8_2aparamS1206.$1;
                    int32_t _M0L3endS3000 = _M0L8_2aparamS1206.$2;
                    int64_t _M0L6_2atmpS2999 = (int64_t)_M0L3endS3000;
                    int64_t _M0L6_2atmpS2996;
                    int32_t _M0L6_2atmpS2995;
                    int32_t _M0L4_2axS1244;
                    moonbit_incref(_M0L3strS2997);
                    moonbit_incref(_M0L3strS2994);
                    #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                    _M0L6_2atmpS2996
                    = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2997, 5, _M0L5startS2998, _M0L6_2atmpS2999);
                    _M0L6_2atmpS2995 = (int32_t)_M0L6_2atmpS2996;
                    #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                    _M0L4_2axS1244
                    = _M0MPC16string6String16unsafe__char__at(_M0L3strS2994, _M0L6_2atmpS2995);
                    if (_M0L4_2axS1244 == 107) {
                      moonbit_string_t _M0L8_2afieldS3458 =
                        _M0L8_2aparamS1206.$0;
                      moonbit_string_t _M0L3strS2987 = _M0L8_2afieldS3458;
                      moonbit_string_t _M0L8_2afieldS3457 =
                        _M0L8_2aparamS1206.$0;
                      moonbit_string_t _M0L3strS2990 = _M0L8_2afieldS3457;
                      int32_t _M0L5startS2991 = _M0L8_2aparamS1206.$1;
                      int32_t _M0L3endS2993 = _M0L8_2aparamS1206.$2;
                      int64_t _M0L6_2atmpS2992 = (int64_t)_M0L3endS2993;
                      int64_t _M0L6_2atmpS2989;
                      int32_t _M0L6_2atmpS2988;
                      int32_t _M0L4_2axS1245;
                      moonbit_incref(_M0L3strS2990);
                      moonbit_incref(_M0L3strS2987);
                      #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                      _M0L6_2atmpS2989
                      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2990, 6, _M0L5startS2991, _M0L6_2atmpS2992);
                      _M0L6_2atmpS2988 = (int32_t)_M0L6_2atmpS2989;
                      #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                      _M0L4_2axS1245
                      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2987, _M0L6_2atmpS2988);
                      if (_M0L4_2axS1245 == 62) {
                        moonbit_string_t _M0L8_2afieldS3453 =
                          _M0L8_2aparamS1206.$0;
                        moonbit_string_t _M0L3strS2980 = _M0L8_2afieldS3453;
                        moonbit_string_t _M0L8_2afieldS3452 =
                          _M0L8_2aparamS1206.$0;
                        moonbit_string_t _M0L3strS2983 = _M0L8_2afieldS3452;
                        int32_t _M0L5startS2984 = _M0L8_2aparamS1206.$1;
                        int32_t _M0L3endS2986 = _M0L8_2aparamS1206.$2;
                        int64_t _M0L6_2atmpS2985 = (int64_t)_M0L3endS2986;
                        int64_t _M0L7_2abindS1456;
                        int32_t _M0L6_2atmpS2981;
                        int32_t _M0L8_2afieldS3451;
                        int32_t _M0L3endS2982;
                        struct _M0TPC16string10StringView _M0L4_2axS1246;
                        moonbit_incref(_M0L3strS2983);
                        moonbit_incref(_M0L3strS2980);
                        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                        _M0L7_2abindS1456
                        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2983, 7, _M0L5startS2984, _M0L6_2atmpS2985);
                        if (_M0L7_2abindS1456 == 4294967296ll) {
                          _M0L6_2atmpS2981 = _M0L8_2aparamS1206.$2;
                        } else {
                          int64_t _M0L7_2aSomeS1247 = _M0L7_2abindS1456;
                          _M0L6_2atmpS2981 = (int32_t)_M0L7_2aSomeS1247;
                        }
                        _M0L8_2afieldS3451 = _M0L8_2aparamS1206.$2;
                        moonbit_decref(_M0L8_2aparamS1206.$0);
                        _M0L3endS2982 = _M0L8_2afieldS3451;
                        _M0L4_2axS1246
                        = (struct _M0TPC16string10StringView){
                          _M0L6_2atmpS2981, _M0L3endS2982, _M0L3strS2980
                        };
                        _M0L4restS1217 = _M0L4_2axS1246;
                        goto join_1216;
                      } else {
                        moonbit_string_t _M0L8_2afieldS3456 =
                          _M0L8_2aparamS1206.$0;
                        moonbit_string_t _M0L3strS2973 = _M0L8_2afieldS3456;
                        moonbit_string_t _M0L8_2afieldS3455 =
                          _M0L8_2aparamS1206.$0;
                        moonbit_string_t _M0L3strS2976 = _M0L8_2afieldS3455;
                        int32_t _M0L5startS2977 = _M0L8_2aparamS1206.$1;
                        int32_t _M0L3endS2979 = _M0L8_2aparamS1206.$2;
                        int64_t _M0L6_2atmpS2978 = (int64_t)_M0L3endS2979;
                        int64_t _M0L7_2abindS1457;
                        int32_t _M0L6_2atmpS2974;
                        int32_t _M0L8_2afieldS3454;
                        int32_t _M0L3endS2975;
                        struct _M0TPC16string10StringView _M0L4_2axS1248;
                        moonbit_incref(_M0L3strS2976);
                        moonbit_incref(_M0L3strS2973);
                        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                        _M0L7_2abindS1457
                        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2976, 1, _M0L5startS2977, _M0L6_2atmpS2978);
                        if (_M0L7_2abindS1457 == 4294967296ll) {
                          _M0L6_2atmpS2974 = _M0L8_2aparamS1206.$2;
                        } else {
                          int64_t _M0L7_2aSomeS1249 = _M0L7_2abindS1457;
                          _M0L6_2atmpS2974 = (int32_t)_M0L7_2aSomeS1249;
                        }
                        _M0L8_2afieldS3454 = _M0L8_2aparamS1206.$2;
                        moonbit_decref(_M0L8_2aparamS1206.$0);
                        _M0L3endS2975 = _M0L8_2afieldS3454;
                        _M0L4_2axS1248
                        = (struct _M0TPC16string10StringView){
                          _M0L6_2atmpS2974, _M0L3endS2975, _M0L3strS2973
                        };
                        _M0L4restS1208 = _M0L4_2axS1248;
                        _M0L1cS1209 = _M0L4_2axS1225;
                        goto join_1207;
                      }
                    } else {
                      moonbit_string_t _M0L8_2afieldS3461 =
                        _M0L8_2aparamS1206.$0;
                      moonbit_string_t _M0L3strS2966 = _M0L8_2afieldS3461;
                      moonbit_string_t _M0L8_2afieldS3460 =
                        _M0L8_2aparamS1206.$0;
                      moonbit_string_t _M0L3strS2969 = _M0L8_2afieldS3460;
                      int32_t _M0L5startS2970 = _M0L8_2aparamS1206.$1;
                      int32_t _M0L3endS2972 = _M0L8_2aparamS1206.$2;
                      int64_t _M0L6_2atmpS2971 = (int64_t)_M0L3endS2972;
                      int64_t _M0L7_2abindS1458;
                      int32_t _M0L6_2atmpS2967;
                      int32_t _M0L8_2afieldS3459;
                      int32_t _M0L3endS2968;
                      struct _M0TPC16string10StringView _M0L4_2axS1250;
                      moonbit_incref(_M0L3strS2969);
                      moonbit_incref(_M0L3strS2966);
                      #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                      _M0L7_2abindS1458
                      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2969, 1, _M0L5startS2970, _M0L6_2atmpS2971);
                      if (_M0L7_2abindS1458 == 4294967296ll) {
                        _M0L6_2atmpS2967 = _M0L8_2aparamS1206.$2;
                      } else {
                        int64_t _M0L7_2aSomeS1251 = _M0L7_2abindS1458;
                        _M0L6_2atmpS2967 = (int32_t)_M0L7_2aSomeS1251;
                      }
                      _M0L8_2afieldS3459 = _M0L8_2aparamS1206.$2;
                      moonbit_decref(_M0L8_2aparamS1206.$0);
                      _M0L3endS2968 = _M0L8_2afieldS3459;
                      _M0L4_2axS1250
                      = (struct _M0TPC16string10StringView){
                        _M0L6_2atmpS2967, _M0L3endS2968, _M0L3strS2966
                      };
                      _M0L4restS1208 = _M0L4_2axS1250;
                      _M0L1cS1209 = _M0L4_2axS1225;
                      goto join_1207;
                    }
                  } else {
                    moonbit_string_t _M0L8_2afieldS3466 =
                      _M0L8_2aparamS1206.$0;
                    moonbit_string_t _M0L3strS2959 = _M0L8_2afieldS3466;
                    moonbit_string_t _M0L8_2afieldS3465 =
                      _M0L8_2aparamS1206.$0;
                    moonbit_string_t _M0L3strS2962 = _M0L8_2afieldS3465;
                    int32_t _M0L5startS2963 = _M0L8_2aparamS1206.$1;
                    int32_t _M0L3endS2965 = _M0L8_2aparamS1206.$2;
                    int64_t _M0L6_2atmpS2964 = (int64_t)_M0L3endS2965;
                    int64_t _M0L7_2abindS1459;
                    int32_t _M0L6_2atmpS2960;
                    int32_t _M0L8_2afieldS3464;
                    int32_t _M0L3endS2961;
                    struct _M0TPC16string10StringView _M0L4_2axS1252;
                    moonbit_incref(_M0L3strS2962);
                    moonbit_incref(_M0L3strS2959);
                    #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                    _M0L7_2abindS1459
                    = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2962, 1, _M0L5startS2963, _M0L6_2atmpS2964);
                    if (_M0L7_2abindS1459 == 4294967296ll) {
                      _M0L6_2atmpS2960 = _M0L8_2aparamS1206.$2;
                    } else {
                      int64_t _M0L7_2aSomeS1253 = _M0L7_2abindS1459;
                      _M0L6_2atmpS2960 = (int32_t)_M0L7_2aSomeS1253;
                    }
                    _M0L8_2afieldS3464 = _M0L8_2aparamS1206.$2;
                    moonbit_decref(_M0L8_2aparamS1206.$0);
                    _M0L3endS2961 = _M0L8_2afieldS3464;
                    _M0L4_2axS1252
                    = (struct _M0TPC16string10StringView){
                      _M0L6_2atmpS2960, _M0L3endS2961, _M0L3strS2959
                    };
                    _M0L4restS1208 = _M0L4_2axS1252;
                    _M0L1cS1209 = _M0L4_2axS1225;
                    goto join_1207;
                  }
                } else {
                  moonbit_string_t _M0L8_2afieldS3471 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS2952 = _M0L8_2afieldS3471;
                  moonbit_string_t _M0L8_2afieldS3470 = _M0L8_2aparamS1206.$0;
                  moonbit_string_t _M0L3strS2955 = _M0L8_2afieldS3470;
                  int32_t _M0L5startS2956 = _M0L8_2aparamS1206.$1;
                  int32_t _M0L3endS2958 = _M0L8_2aparamS1206.$2;
                  int64_t _M0L6_2atmpS2957 = (int64_t)_M0L3endS2958;
                  int64_t _M0L7_2abindS1460;
                  int32_t _M0L6_2atmpS2953;
                  int32_t _M0L8_2afieldS3469;
                  int32_t _M0L3endS2954;
                  struct _M0TPC16string10StringView _M0L4_2axS1254;
                  moonbit_incref(_M0L3strS2955);
                  moonbit_incref(_M0L3strS2952);
                  #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                  _M0L7_2abindS1460
                  = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2955, 1, _M0L5startS2956, _M0L6_2atmpS2957);
                  if (_M0L7_2abindS1460 == 4294967296ll) {
                    _M0L6_2atmpS2953 = _M0L8_2aparamS1206.$2;
                  } else {
                    int64_t _M0L7_2aSomeS1255 = _M0L7_2abindS1460;
                    _M0L6_2atmpS2953 = (int32_t)_M0L7_2aSomeS1255;
                  }
                  _M0L8_2afieldS3469 = _M0L8_2aparamS1206.$2;
                  moonbit_decref(_M0L8_2aparamS1206.$0);
                  _M0L3endS2954 = _M0L8_2afieldS3469;
                  _M0L4_2axS1254
                  = (struct _M0TPC16string10StringView){
                    _M0L6_2atmpS2953, _M0L3endS2954, _M0L3strS2952
                  };
                  _M0L4restS1208 = _M0L4_2axS1254;
                  _M0L1cS1209 = _M0L4_2axS1225;
                  goto join_1207;
                }
              } else {
                moonbit_string_t _M0L8_2afieldS3476 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS2945 = _M0L8_2afieldS3476;
                moonbit_string_t _M0L8_2afieldS3475 = _M0L8_2aparamS1206.$0;
                moonbit_string_t _M0L3strS2948 = _M0L8_2afieldS3475;
                int32_t _M0L5startS2949 = _M0L8_2aparamS1206.$1;
                int32_t _M0L3endS2951 = _M0L8_2aparamS1206.$2;
                int64_t _M0L6_2atmpS2950 = (int64_t)_M0L3endS2951;
                int64_t _M0L7_2abindS1461;
                int32_t _M0L6_2atmpS2946;
                int32_t _M0L8_2afieldS3474;
                int32_t _M0L3endS2947;
                struct _M0TPC16string10StringView _M0L4_2axS1256;
                moonbit_incref(_M0L3strS2948);
                moonbit_incref(_M0L3strS2945);
                #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
                _M0L7_2abindS1461
                = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2948, 1, _M0L5startS2949, _M0L6_2atmpS2950);
                if (_M0L7_2abindS1461 == 4294967296ll) {
                  _M0L6_2atmpS2946 = _M0L8_2aparamS1206.$2;
                } else {
                  int64_t _M0L7_2aSomeS1257 = _M0L7_2abindS1461;
                  _M0L6_2atmpS2946 = (int32_t)_M0L7_2aSomeS1257;
                }
                _M0L8_2afieldS3474 = _M0L8_2aparamS1206.$2;
                moonbit_decref(_M0L8_2aparamS1206.$0);
                _M0L3endS2947 = _M0L8_2afieldS3474;
                _M0L4_2axS1256
                = (struct _M0TPC16string10StringView){
                  _M0L6_2atmpS2946, _M0L3endS2947, _M0L3strS2945
                };
                _M0L4restS1208 = _M0L4_2axS1256;
                _M0L1cS1209 = _M0L4_2axS1225;
                goto join_1207;
              }
            } else {
              moonbit_string_t _M0L8_2afieldS3481 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS2938 = _M0L8_2afieldS3481;
              moonbit_string_t _M0L8_2afieldS3480 = _M0L8_2aparamS1206.$0;
              moonbit_string_t _M0L3strS2941 = _M0L8_2afieldS3480;
              int32_t _M0L5startS2942 = _M0L8_2aparamS1206.$1;
              int32_t _M0L3endS2944 = _M0L8_2aparamS1206.$2;
              int64_t _M0L6_2atmpS2943 = (int64_t)_M0L3endS2944;
              int64_t _M0L7_2abindS1462;
              int32_t _M0L6_2atmpS2939;
              int32_t _M0L8_2afieldS3479;
              int32_t _M0L3endS2940;
              struct _M0TPC16string10StringView _M0L4_2axS1258;
              moonbit_incref(_M0L3strS2941);
              moonbit_incref(_M0L3strS2938);
              #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
              _M0L7_2abindS1462
              = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2941, 1, _M0L5startS2942, _M0L6_2atmpS2943);
              if (_M0L7_2abindS1462 == 4294967296ll) {
                _M0L6_2atmpS2939 = _M0L8_2aparamS1206.$2;
              } else {
                int64_t _M0L7_2aSomeS1259 = _M0L7_2abindS1462;
                _M0L6_2atmpS2939 = (int32_t)_M0L7_2aSomeS1259;
              }
              _M0L8_2afieldS3479 = _M0L8_2aparamS1206.$2;
              moonbit_decref(_M0L8_2aparamS1206.$0);
              _M0L3endS2940 = _M0L8_2afieldS3479;
              _M0L4_2axS1258
              = (struct _M0TPC16string10StringView){
                _M0L6_2atmpS2939, _M0L3endS2940, _M0L3strS2938
              };
              _M0L4restS1208 = _M0L4_2axS1258;
              _M0L1cS1209 = _M0L4_2axS1225;
              goto join_1207;
            }
          } else {
            moonbit_string_t _M0L8_2afieldS3484 = _M0L8_2aparamS1206.$0;
            moonbit_string_t _M0L3strS3022 = _M0L8_2afieldS3484;
            moonbit_string_t _M0L8_2afieldS3483 = _M0L8_2aparamS1206.$0;
            moonbit_string_t _M0L3strS3025 = _M0L8_2afieldS3483;
            int32_t _M0L5startS3026 = _M0L8_2aparamS1206.$1;
            int32_t _M0L3endS3028 = _M0L8_2aparamS1206.$2;
            int64_t _M0L6_2atmpS3027 = (int64_t)_M0L3endS3028;
            int64_t _M0L7_2abindS1463;
            int32_t _M0L6_2atmpS3023;
            int32_t _M0L8_2afieldS3482;
            int32_t _M0L3endS3024;
            struct _M0TPC16string10StringView _M0L4_2axS1260;
            moonbit_incref(_M0L3strS3025);
            moonbit_incref(_M0L3strS3022);
            #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
            _M0L7_2abindS1463
            = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3025, 1, _M0L5startS3026, _M0L6_2atmpS3027);
            if (_M0L7_2abindS1463 == 4294967296ll) {
              _M0L6_2atmpS3023 = _M0L8_2aparamS1206.$2;
            } else {
              int64_t _M0L7_2aSomeS1261 = _M0L7_2abindS1463;
              _M0L6_2atmpS3023 = (int32_t)_M0L7_2aSomeS1261;
            }
            _M0L8_2afieldS3482 = _M0L8_2aparamS1206.$2;
            moonbit_decref(_M0L8_2aparamS1206.$0);
            _M0L3endS3024 = _M0L8_2afieldS3482;
            _M0L4_2axS1260
            = (struct _M0TPC16string10StringView){
              _M0L6_2atmpS3023, _M0L3endS3024, _M0L3strS3022
            };
            _M0L4restS1208 = _M0L4_2axS1260;
            _M0L1cS1209 = _M0L4_2axS1225;
            goto join_1207;
          }
        }
      } else {
        moonbit_string_t _M0L8_2afieldS3490 = _M0L8_2aparamS1206.$0;
        moonbit_string_t _M0L3strS2927 = _M0L8_2afieldS3490;
        moonbit_string_t _M0L8_2afieldS3489 = _M0L8_2aparamS1206.$0;
        moonbit_string_t _M0L3strS2930 = _M0L8_2afieldS3489;
        int32_t _M0L5startS2931 = _M0L8_2aparamS1206.$1;
        int32_t _M0L3endS2933 = _M0L8_2aparamS1206.$2;
        int64_t _M0L6_2atmpS2932 = (int64_t)_M0L3endS2933;
        int64_t _M0L7_2abindS1464;
        int32_t _M0L6_2atmpS2928;
        int32_t _M0L8_2afieldS3488;
        int32_t _M0L3endS2929;
        struct _M0TPC16string10StringView _M0L4_2axS1262;
        moonbit_incref(_M0L3strS2930);
        moonbit_incref(_M0L3strS2927);
        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
        _M0L7_2abindS1464
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2930, 1, _M0L5startS2931, _M0L6_2atmpS2932);
        if (_M0L7_2abindS1464 == 4294967296ll) {
          _M0L6_2atmpS2928 = _M0L8_2aparamS1206.$2;
        } else {
          int64_t _M0L7_2aSomeS1263 = _M0L7_2abindS1464;
          _M0L6_2atmpS2928 = (int32_t)_M0L7_2aSomeS1263;
        }
        _M0L8_2afieldS3488 = _M0L8_2aparamS1206.$2;
        moonbit_decref(_M0L8_2aparamS1206.$0);
        _M0L3endS2929 = _M0L8_2afieldS3488;
        _M0L4_2axS1262
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS2928, _M0L3endS2929, _M0L3strS2927
        };
        _M0L4restS1208 = _M0L4_2axS1262;
        _M0L1cS1209 = _M0L4_2axS1225;
        goto join_1207;
      }
    } else {
      moonbit_string_t _M0L8_2afieldS3498 = _M0L8_2aparamS1206.$0;
      moonbit_string_t _M0L3strS3106 = _M0L8_2afieldS3498;
      int32_t _M0L5startS3107 = _M0L8_2aparamS1206.$1;
      int32_t _M0L3endS3109 = _M0L8_2aparamS1206.$2;
      int64_t _M0L6_2atmpS3108 = (int64_t)_M0L3endS3109;
      moonbit_incref(_M0L3strS3106);
      #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
      if (
        _M0MPC16string6String24char__length__ge_2einner(_M0L3strS3106, 1, _M0L5startS3107, _M0L6_2atmpS3108)
      ) {
        moonbit_string_t _M0L8_2afieldS3497 = _M0L8_2aparamS1206.$0;
        moonbit_string_t _M0L3strS3117 = _M0L8_2afieldS3497;
        moonbit_string_t _M0L8_2afieldS3496 = _M0L8_2aparamS1206.$0;
        moonbit_string_t _M0L3strS3120 = _M0L8_2afieldS3496;
        int32_t _M0L5startS3121 = _M0L8_2aparamS1206.$1;
        int32_t _M0L3endS3123 = _M0L8_2aparamS1206.$2;
        int64_t _M0L6_2atmpS3122 = (int64_t)_M0L3endS3123;
        int64_t _M0L6_2atmpS3119;
        int32_t _M0L6_2atmpS3118;
        int32_t _M0L4_2acS1264;
        moonbit_string_t _M0L8_2afieldS3495;
        moonbit_string_t _M0L3strS3110;
        moonbit_string_t _M0L8_2afieldS3494;
        moonbit_string_t _M0L3strS3113;
        int32_t _M0L5startS3114;
        int32_t _M0L3endS3116;
        int64_t _M0L6_2atmpS3115;
        int64_t _M0L7_2abindS1465;
        int32_t _M0L6_2atmpS3111;
        int32_t _M0L8_2afieldS3493;
        int32_t _M0L3endS3112;
        struct _M0TPC16string10StringView _M0L4_2axS1265;
        moonbit_incref(_M0L3strS3120);
        moonbit_incref(_M0L3strS3117);
        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
        _M0L6_2atmpS3119
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3120, 0, _M0L5startS3121, _M0L6_2atmpS3122);
        _M0L6_2atmpS3118 = (int32_t)_M0L6_2atmpS3119;
        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
        _M0L4_2acS1264
        = _M0MPC16string6String16unsafe__char__at(_M0L3strS3117, _M0L6_2atmpS3118);
        _M0L8_2afieldS3495 = _M0L8_2aparamS1206.$0;
        _M0L3strS3110 = _M0L8_2afieldS3495;
        _M0L8_2afieldS3494 = _M0L8_2aparamS1206.$0;
        _M0L3strS3113 = _M0L8_2afieldS3494;
        _M0L5startS3114 = _M0L8_2aparamS1206.$1;
        _M0L3endS3116 = _M0L8_2aparamS1206.$2;
        _M0L6_2atmpS3115 = (int64_t)_M0L3endS3116;
        moonbit_incref(_M0L3strS3113);
        moonbit_incref(_M0L3strS3110);
        #line 110 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
        _M0L7_2abindS1465
        = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS3113, 1, _M0L5startS3114, _M0L6_2atmpS3115);
        if (_M0L7_2abindS1465 == 4294967296ll) {
          _M0L6_2atmpS3111 = _M0L8_2aparamS1206.$2;
        } else {
          int64_t _M0L7_2aSomeS1266 = _M0L7_2abindS1465;
          _M0L6_2atmpS3111 = (int32_t)_M0L7_2aSomeS1266;
        }
        _M0L8_2afieldS3493 = _M0L8_2aparamS1206.$2;
        moonbit_decref(_M0L8_2aparamS1206.$0);
        _M0L3endS3112 = _M0L8_2afieldS3493;
        _M0L4_2axS1265
        = (struct _M0TPC16string10StringView){
          _M0L6_2atmpS3111, _M0L3endS3112, _M0L3strS3110
        };
        _M0L4restS1208 = _M0L4_2axS1265;
        _M0L1cS1209 = _M0L4_2acS1264;
        goto join_1207;
      } else {
        moonbit_decref(_M0L8_2aparamS1206.$0);
        moonbit_decref(_M0L4taskS1205);
        break;
      }
    }
    goto joinlet_3934;
    join_1223:;
    #line 112 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
    _M0L6_2atmpS2922 = _M0MPB13StringBuilder11new_2einner(0);
    _M0L6_2atmpS2921 = _M0L6_2atmpS2922;
    _M0L6_2aoldS3427 = _M0L4taskS1205->$0;
    if (_M0L6_2aoldS3427) {
      moonbit_decref(_M0L6_2aoldS3427);
    }
    _M0L4taskS1205->$0 = _M0L6_2atmpS2921;
    _M0L8_2aparamS1206 = _M0L4restS1224;
    continue;
    joinlet_3934:;
    goto joinlet_3933;
    join_1216:;
    _M0L8_2afieldS3426 = _M0L4taskS1205->$0;
    _M0L7_2abindS1220 = _M0L8_2afieldS3426;
    if (_M0L7_2abindS1220 == 0) {
      
    } else {
      struct _M0TPB13StringBuilder* _M0L7_2aSomeS1221 = _M0L7_2abindS1220;
      struct _M0TPB13StringBuilder* _M0L4_2atS1222 = _M0L7_2aSomeS1221;
      moonbit_incref(_M0L4_2atS1222);
      _M0L1tS1219 = _M0L4_2atS1222;
      goto join_1218;
    }
    goto joinlet_3935;
    join_1218:;
    #line 117 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
    _M0L6_2atmpS2919 = _M0MPB13StringBuilder10to__string(_M0L1tS1219);
    moonbit_incref(_M0L5tasksS1204);
    #line 117 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
    _M0MPC15array5Array4pushGsE(_M0L5tasksS1204, _M0L6_2atmpS2919);
    _M0L6_2atmpS2920 = 0;
    _M0L6_2aoldS3425 = _M0L4taskS1205->$0;
    if (_M0L6_2aoldS3425) {
      moonbit_decref(_M0L6_2aoldS3425);
    }
    _M0L4taskS1205->$0 = _M0L6_2atmpS2920;
    joinlet_3935:;
    _M0L8_2aparamS1206 = _M0L4restS1217;
    continue;
    joinlet_3933:;
    goto joinlet_3932;
    join_1207:;
    _M0L8_2afieldS3424 = _M0L4taskS1205->$0;
    _M0L7_2abindS1212 = _M0L8_2afieldS3424;
    if (_M0L7_2abindS1212 == 0) {
      
    } else {
      struct _M0TPB13StringBuilder* _M0L7_2aSomeS1213 = _M0L7_2abindS1212;
      struct _M0TPB13StringBuilder* _M0L4_2atS1214 = _M0L7_2aSomeS1213;
      moonbit_incref(_M0L4_2atS1214);
      _M0L1tS1211 = _M0L4_2atS1214;
      goto join_1210;
    }
    goto joinlet_3936;
    join_1210:;
    #line 124 "E:\\moonbit\\clawteam\\tools\\todo\\manager.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L1tS1211, _M0L1cS1209);
    joinlet_3936:;
    _M0L8_2aparamS1206 = _M0L4restS1208;
    continue;
    joinlet_3932:;
    break;
  }
  return _M0L5tasksS1204;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1199,
  void* _M0L7contentS1201,
  moonbit_string_t _M0L3locS1195,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1197
) {
  moonbit_string_t _M0L3locS1194;
  moonbit_string_t _M0L9args__locS1196;
  void* _M0L6_2atmpS2917;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2918;
  moonbit_string_t _M0L6actualS1198;
  moonbit_string_t _M0L4wantS1200;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1194 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1195);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1196 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1197);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2917 = _M0L3objS1199.$0->$method_0(_M0L3objS1199.$1);
  _M0L6_2atmpS2918 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1198
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2917, 0, 0, _M0L6_2atmpS2918);
  if (_M0L7contentS1201 == 0) {
    void* _M0L6_2atmpS2914;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2915;
    if (_M0L7contentS1201) {
      moonbit_decref(_M0L7contentS1201);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2914
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2915 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1200
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2914, 0, 0, _M0L6_2atmpS2915);
  } else {
    void* _M0L7_2aSomeS1202 = _M0L7contentS1201;
    void* _M0L4_2axS1203 = _M0L7_2aSomeS1202;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2916 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1200
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1203, 0, 0, _M0L6_2atmpS2916);
  }
  moonbit_incref(_M0L4wantS1200);
  moonbit_incref(_M0L6actualS1198);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1198, _M0L4wantS1200)
  ) {
    moonbit_string_t _M0L6_2atmpS2912;
    moonbit_string_t _M0L6_2atmpS3507;
    moonbit_string_t _M0L6_2atmpS2911;
    moonbit_string_t _M0L6_2atmpS3506;
    moonbit_string_t _M0L6_2atmpS2909;
    moonbit_string_t _M0L6_2atmpS2910;
    moonbit_string_t _M0L6_2atmpS3505;
    moonbit_string_t _M0L6_2atmpS2908;
    moonbit_string_t _M0L6_2atmpS3504;
    moonbit_string_t _M0L6_2atmpS2905;
    moonbit_string_t _M0L6_2atmpS2907;
    moonbit_string_t _M0L6_2atmpS2906;
    moonbit_string_t _M0L6_2atmpS3503;
    moonbit_string_t _M0L6_2atmpS2904;
    moonbit_string_t _M0L6_2atmpS3502;
    moonbit_string_t _M0L6_2atmpS2901;
    moonbit_string_t _M0L6_2atmpS2903;
    moonbit_string_t _M0L6_2atmpS2902;
    moonbit_string_t _M0L6_2atmpS3501;
    moonbit_string_t _M0L6_2atmpS2900;
    moonbit_string_t _M0L6_2atmpS3500;
    moonbit_string_t _M0L6_2atmpS2899;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2898;
    struct moonbit_result_0 _result_3937;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2912
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1194);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3507
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS2912);
    moonbit_decref(_M0L6_2atmpS2912);
    _M0L6_2atmpS2911 = _M0L6_2atmpS3507;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3506
    = moonbit_add_string(_M0L6_2atmpS2911, (moonbit_string_t)moonbit_string_literal_37.data);
    moonbit_decref(_M0L6_2atmpS2911);
    _M0L6_2atmpS2909 = _M0L6_2atmpS3506;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2910
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1196);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3505 = moonbit_add_string(_M0L6_2atmpS2909, _M0L6_2atmpS2910);
    moonbit_decref(_M0L6_2atmpS2909);
    moonbit_decref(_M0L6_2atmpS2910);
    _M0L6_2atmpS2908 = _M0L6_2atmpS3505;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3504
    = moonbit_add_string(_M0L6_2atmpS2908, (moonbit_string_t)moonbit_string_literal_38.data);
    moonbit_decref(_M0L6_2atmpS2908);
    _M0L6_2atmpS2905 = _M0L6_2atmpS3504;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2907 = _M0MPC16string6String6escape(_M0L4wantS1200);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2906
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2907);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3503 = moonbit_add_string(_M0L6_2atmpS2905, _M0L6_2atmpS2906);
    moonbit_decref(_M0L6_2atmpS2905);
    moonbit_decref(_M0L6_2atmpS2906);
    _M0L6_2atmpS2904 = _M0L6_2atmpS3503;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3502
    = moonbit_add_string(_M0L6_2atmpS2904, (moonbit_string_t)moonbit_string_literal_39.data);
    moonbit_decref(_M0L6_2atmpS2904);
    _M0L6_2atmpS2901 = _M0L6_2atmpS3502;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2903 = _M0MPC16string6String6escape(_M0L6actualS1198);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2902
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2903);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3501 = moonbit_add_string(_M0L6_2atmpS2901, _M0L6_2atmpS2902);
    moonbit_decref(_M0L6_2atmpS2901);
    moonbit_decref(_M0L6_2atmpS2902);
    _M0L6_2atmpS2900 = _M0L6_2atmpS3501;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3500
    = moonbit_add_string(_M0L6_2atmpS2900, (moonbit_string_t)moonbit_string_literal_40.data);
    moonbit_decref(_M0L6_2atmpS2900);
    _M0L6_2atmpS2899 = _M0L6_2atmpS3500;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2898
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2898)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2898)->$0
    = _M0L6_2atmpS2899;
    _result_3937.tag = 0;
    _result_3937.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2898;
    return _result_3937;
  } else {
    int32_t _M0L6_2atmpS2913;
    struct moonbit_result_0 _result_3938;
    moonbit_decref(_M0L4wantS1200);
    moonbit_decref(_M0L6actualS1198);
    moonbit_decref(_M0L9args__locS1196);
    moonbit_decref(_M0L3locS1194);
    _M0L6_2atmpS2913 = 0;
    _result_3938.tag = 1;
    _result_3938.data.ok = _M0L6_2atmpS2913;
    return _result_3938;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1193,
  int32_t _M0L13escape__slashS1165,
  int32_t _M0L6indentS1160,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1186
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1152;
  void** _M0L6_2atmpS2897;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1153;
  int32_t _M0Lm5depthS1154;
  void* _M0L6_2atmpS2896;
  void* _M0L8_2aparamS1155;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1152 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2897 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1153
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1153)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1153->$0 = _M0L6_2atmpS2897;
  _M0L5stackS1153->$1 = 0;
  _M0Lm5depthS1154 = 0;
  _M0L6_2atmpS2896 = _M0L4selfS1193;
  _M0L8_2aparamS1155 = _M0L6_2atmpS2896;
  _2aloop_1171:;
  while (1) {
    if (_M0L8_2aparamS1155 == 0) {
      int32_t _M0L3lenS2858;
      if (_M0L8_2aparamS1155) {
        moonbit_decref(_M0L8_2aparamS1155);
      }
      _M0L3lenS2858 = _M0L5stackS1153->$1;
      if (_M0L3lenS2858 == 0) {
        if (_M0L8replacerS1186) {
          moonbit_decref(_M0L8replacerS1186);
        }
        moonbit_decref(_M0L5stackS1153);
        break;
      } else {
        void** _M0L8_2afieldS3515 = _M0L5stackS1153->$0;
        void** _M0L3bufS2882 = _M0L8_2afieldS3515;
        int32_t _M0L3lenS2884 = _M0L5stackS1153->$1;
        int32_t _M0L6_2atmpS2883 = _M0L3lenS2884 - 1;
        void* _M0L6_2atmpS3514 = (void*)_M0L3bufS2882[_M0L6_2atmpS2883];
        void* _M0L4_2axS1172 = _M0L6_2atmpS3514;
        switch (Moonbit_object_tag(_M0L4_2axS1172)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1173 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1172;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3510 =
              _M0L8_2aArrayS1173->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1174 =
              _M0L8_2afieldS3510;
            int32_t _M0L4_2aiS1175 = _M0L8_2aArrayS1173->$1;
            int32_t _M0L3lenS2870 = _M0L6_2aarrS1174->$1;
            if (_M0L4_2aiS1175 < _M0L3lenS2870) {
              int32_t _if__result_3940;
              void** _M0L8_2afieldS3509;
              void** _M0L3bufS2876;
              void* _M0L6_2atmpS3508;
              void* _M0L7elementS1176;
              int32_t _M0L6_2atmpS2871;
              void* _M0L6_2atmpS2874;
              if (_M0L4_2aiS1175 < 0) {
                _if__result_3940 = 1;
              } else {
                int32_t _M0L3lenS2875 = _M0L6_2aarrS1174->$1;
                _if__result_3940 = _M0L4_2aiS1175 >= _M0L3lenS2875;
              }
              if (_if__result_3940) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3509 = _M0L6_2aarrS1174->$0;
              _M0L3bufS2876 = _M0L8_2afieldS3509;
              _M0L6_2atmpS3508 = (void*)_M0L3bufS2876[_M0L4_2aiS1175];
              _M0L7elementS1176 = _M0L6_2atmpS3508;
              _M0L6_2atmpS2871 = _M0L4_2aiS1175 + 1;
              _M0L8_2aArrayS1173->$1 = _M0L6_2atmpS2871;
              if (_M0L4_2aiS1175 > 0) {
                int32_t _M0L6_2atmpS2873;
                moonbit_string_t _M0L6_2atmpS2872;
                moonbit_incref(_M0L7elementS1176);
                moonbit_incref(_M0L3bufS1152);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 44);
                _M0L6_2atmpS2873 = _M0Lm5depthS1154;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2872
                = _M0FPC14json11indent__str(_M0L6_2atmpS2873, _M0L6indentS1160);
                moonbit_incref(_M0L3bufS1152);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2872);
              } else {
                moonbit_incref(_M0L7elementS1176);
              }
              _M0L6_2atmpS2874 = _M0L7elementS1176;
              _M0L8_2aparamS1155 = _M0L6_2atmpS2874;
              goto _2aloop_1171;
            } else {
              int32_t _M0L6_2atmpS2877 = _M0Lm5depthS1154;
              void* _M0L6_2atmpS2878;
              int32_t _M0L6_2atmpS2880;
              moonbit_string_t _M0L6_2atmpS2879;
              void* _M0L6_2atmpS2881;
              _M0Lm5depthS1154 = _M0L6_2atmpS2877 - 1;
              moonbit_incref(_M0L5stackS1153);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2878
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1153);
              if (_M0L6_2atmpS2878) {
                moonbit_decref(_M0L6_2atmpS2878);
              }
              _M0L6_2atmpS2880 = _M0Lm5depthS1154;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2879
              = _M0FPC14json11indent__str(_M0L6_2atmpS2880, _M0L6indentS1160);
              moonbit_incref(_M0L3bufS1152);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2879);
              moonbit_incref(_M0L3bufS1152);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 93);
              _M0L6_2atmpS2881 = 0;
              _M0L8_2aparamS1155 = _M0L6_2atmpS2881;
              goto _2aloop_1171;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1177 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1172;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3513 =
              _M0L9_2aObjectS1177->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1178 =
              _M0L8_2afieldS3513;
            int32_t _M0L8_2afirstS1179 = _M0L9_2aObjectS1177->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1180;
            moonbit_incref(_M0L11_2aiteratorS1178);
            moonbit_incref(_M0L9_2aObjectS1177);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1180
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1178);
            if (_M0L7_2abindS1180 == 0) {
              int32_t _M0L6_2atmpS2859;
              void* _M0L6_2atmpS2860;
              int32_t _M0L6_2atmpS2862;
              moonbit_string_t _M0L6_2atmpS2861;
              void* _M0L6_2atmpS2863;
              if (_M0L7_2abindS1180) {
                moonbit_decref(_M0L7_2abindS1180);
              }
              moonbit_decref(_M0L9_2aObjectS1177);
              _M0L6_2atmpS2859 = _M0Lm5depthS1154;
              _M0Lm5depthS1154 = _M0L6_2atmpS2859 - 1;
              moonbit_incref(_M0L5stackS1153);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2860
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1153);
              if (_M0L6_2atmpS2860) {
                moonbit_decref(_M0L6_2atmpS2860);
              }
              _M0L6_2atmpS2862 = _M0Lm5depthS1154;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2861
              = _M0FPC14json11indent__str(_M0L6_2atmpS2862, _M0L6indentS1160);
              moonbit_incref(_M0L3bufS1152);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2861);
              moonbit_incref(_M0L3bufS1152);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 125);
              _M0L6_2atmpS2863 = 0;
              _M0L8_2aparamS1155 = _M0L6_2atmpS2863;
              goto _2aloop_1171;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1181 = _M0L7_2abindS1180;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1182 = _M0L7_2aSomeS1181;
              moonbit_string_t _M0L8_2afieldS3512 = _M0L4_2axS1182->$0;
              moonbit_string_t _M0L4_2akS1183 = _M0L8_2afieldS3512;
              void* _M0L8_2afieldS3511 = _M0L4_2axS1182->$1;
              int32_t _M0L6_2acntS3797 =
                Moonbit_object_header(_M0L4_2axS1182)->rc;
              void* _M0L4_2avS1184;
              void* _M0Lm2v2S1185;
              moonbit_string_t _M0L6_2atmpS2867;
              void* _M0L6_2atmpS2869;
              void* _M0L6_2atmpS2868;
              if (_M0L6_2acntS3797 > 1) {
                int32_t _M0L11_2anew__cntS3798 = _M0L6_2acntS3797 - 1;
                Moonbit_object_header(_M0L4_2axS1182)->rc
                = _M0L11_2anew__cntS3798;
                moonbit_incref(_M0L8_2afieldS3511);
                moonbit_incref(_M0L4_2akS1183);
              } else if (_M0L6_2acntS3797 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1182);
              }
              _M0L4_2avS1184 = _M0L8_2afieldS3511;
              _M0Lm2v2S1185 = _M0L4_2avS1184;
              if (_M0L8replacerS1186 == 0) {
                moonbit_incref(_M0Lm2v2S1185);
                moonbit_decref(_M0L4_2avS1184);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1187 =
                  _M0L8replacerS1186;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1188 =
                  _M0L7_2aSomeS1187;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1189 =
                  _M0L11_2areplacerS1188;
                void* _M0L7_2abindS1190;
                moonbit_incref(_M0L7_2afuncS1189);
                moonbit_incref(_M0L4_2akS1183);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1190
                = _M0L7_2afuncS1189->code(_M0L7_2afuncS1189, _M0L4_2akS1183, _M0L4_2avS1184);
                if (_M0L7_2abindS1190 == 0) {
                  void* _M0L6_2atmpS2864;
                  if (_M0L7_2abindS1190) {
                    moonbit_decref(_M0L7_2abindS1190);
                  }
                  moonbit_decref(_M0L4_2akS1183);
                  moonbit_decref(_M0L9_2aObjectS1177);
                  _M0L6_2atmpS2864 = 0;
                  _M0L8_2aparamS1155 = _M0L6_2atmpS2864;
                  goto _2aloop_1171;
                } else {
                  void* _M0L7_2aSomeS1191 = _M0L7_2abindS1190;
                  void* _M0L4_2avS1192 = _M0L7_2aSomeS1191;
                  _M0Lm2v2S1185 = _M0L4_2avS1192;
                }
              }
              if (!_M0L8_2afirstS1179) {
                int32_t _M0L6_2atmpS2866;
                moonbit_string_t _M0L6_2atmpS2865;
                moonbit_incref(_M0L3bufS1152);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 44);
                _M0L6_2atmpS2866 = _M0Lm5depthS1154;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2865
                = _M0FPC14json11indent__str(_M0L6_2atmpS2866, _M0L6indentS1160);
                moonbit_incref(_M0L3bufS1152);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2865);
              }
              moonbit_incref(_M0L3bufS1152);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2867
              = _M0FPC14json6escape(_M0L4_2akS1183, _M0L13escape__slashS1165);
              moonbit_incref(_M0L3bufS1152);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2867);
              moonbit_incref(_M0L3bufS1152);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 34);
              moonbit_incref(_M0L3bufS1152);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 58);
              if (_M0L6indentS1160 > 0) {
                moonbit_incref(_M0L3bufS1152);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 32);
              }
              _M0L9_2aObjectS1177->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1177);
              _M0L6_2atmpS2869 = _M0Lm2v2S1185;
              _M0L6_2atmpS2868 = _M0L6_2atmpS2869;
              _M0L8_2aparamS1155 = _M0L6_2atmpS2868;
              goto _2aloop_1171;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1156 = _M0L8_2aparamS1155;
      void* _M0L8_2avalueS1157 = _M0L7_2aSomeS1156;
      void* _M0L6_2atmpS2895;
      switch (Moonbit_object_tag(_M0L8_2avalueS1157)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1158 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1157;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3516 =
            _M0L9_2aObjectS1158->$0;
          int32_t _M0L6_2acntS3799 =
            Moonbit_object_header(_M0L9_2aObjectS1158)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1159;
          if (_M0L6_2acntS3799 > 1) {
            int32_t _M0L11_2anew__cntS3800 = _M0L6_2acntS3799 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1158)->rc
            = _M0L11_2anew__cntS3800;
            moonbit_incref(_M0L8_2afieldS3516);
          } else if (_M0L6_2acntS3799 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1158);
          }
          _M0L10_2amembersS1159 = _M0L8_2afieldS3516;
          moonbit_incref(_M0L10_2amembersS1159);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1159)) {
            moonbit_decref(_M0L10_2amembersS1159);
            moonbit_incref(_M0L3bufS1152);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, (moonbit_string_t)moonbit_string_literal_41.data);
          } else {
            int32_t _M0L6_2atmpS2890 = _M0Lm5depthS1154;
            int32_t _M0L6_2atmpS2892;
            moonbit_string_t _M0L6_2atmpS2891;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2894;
            void* _M0L6ObjectS2893;
            _M0Lm5depthS1154 = _M0L6_2atmpS2890 + 1;
            moonbit_incref(_M0L3bufS1152);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 123);
            _M0L6_2atmpS2892 = _M0Lm5depthS1154;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2891
            = _M0FPC14json11indent__str(_M0L6_2atmpS2892, _M0L6indentS1160);
            moonbit_incref(_M0L3bufS1152);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2891);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2894
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1159);
            _M0L6ObjectS2893
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2893)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2893)->$0
            = _M0L6_2atmpS2894;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2893)->$1
            = 1;
            moonbit_incref(_M0L5stackS1153);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1153, _M0L6ObjectS2893);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1161 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1157;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3517 =
            _M0L8_2aArrayS1161->$0;
          int32_t _M0L6_2acntS3801 =
            Moonbit_object_header(_M0L8_2aArrayS1161)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1162;
          if (_M0L6_2acntS3801 > 1) {
            int32_t _M0L11_2anew__cntS3802 = _M0L6_2acntS3801 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1161)->rc
            = _M0L11_2anew__cntS3802;
            moonbit_incref(_M0L8_2afieldS3517);
          } else if (_M0L6_2acntS3801 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1161);
          }
          _M0L6_2aarrS1162 = _M0L8_2afieldS3517;
          moonbit_incref(_M0L6_2aarrS1162);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1162)) {
            moonbit_decref(_M0L6_2aarrS1162);
            moonbit_incref(_M0L3bufS1152);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, (moonbit_string_t)moonbit_string_literal_42.data);
          } else {
            int32_t _M0L6_2atmpS2886 = _M0Lm5depthS1154;
            int32_t _M0L6_2atmpS2888;
            moonbit_string_t _M0L6_2atmpS2887;
            void* _M0L5ArrayS2889;
            _M0Lm5depthS1154 = _M0L6_2atmpS2886 + 1;
            moonbit_incref(_M0L3bufS1152);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 91);
            _M0L6_2atmpS2888 = _M0Lm5depthS1154;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2887
            = _M0FPC14json11indent__str(_M0L6_2atmpS2888, _M0L6indentS1160);
            moonbit_incref(_M0L3bufS1152);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2887);
            _M0L5ArrayS2889
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2889)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2889)->$0
            = _M0L6_2aarrS1162;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2889)->$1
            = 0;
            moonbit_incref(_M0L5stackS1153);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1153, _M0L5ArrayS2889);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1163 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1157;
          moonbit_string_t _M0L8_2afieldS3518 = _M0L9_2aStringS1163->$0;
          int32_t _M0L6_2acntS3803 =
            Moonbit_object_header(_M0L9_2aStringS1163)->rc;
          moonbit_string_t _M0L4_2asS1164;
          moonbit_string_t _M0L6_2atmpS2885;
          if (_M0L6_2acntS3803 > 1) {
            int32_t _M0L11_2anew__cntS3804 = _M0L6_2acntS3803 - 1;
            Moonbit_object_header(_M0L9_2aStringS1163)->rc
            = _M0L11_2anew__cntS3804;
            moonbit_incref(_M0L8_2afieldS3518);
          } else if (_M0L6_2acntS3803 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1163);
          }
          _M0L4_2asS1164 = _M0L8_2afieldS3518;
          moonbit_incref(_M0L3bufS1152);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2885
          = _M0FPC14json6escape(_M0L4_2asS1164, _M0L13escape__slashS1165);
          moonbit_incref(_M0L3bufS1152);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L6_2atmpS2885);
          moonbit_incref(_M0L3bufS1152);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1152, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1166 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1157;
          double _M0L4_2anS1167 = _M0L9_2aNumberS1166->$0;
          moonbit_string_t _M0L8_2afieldS3519 = _M0L9_2aNumberS1166->$1;
          int32_t _M0L6_2acntS3805 =
            Moonbit_object_header(_M0L9_2aNumberS1166)->rc;
          moonbit_string_t _M0L7_2areprS1168;
          if (_M0L6_2acntS3805 > 1) {
            int32_t _M0L11_2anew__cntS3806 = _M0L6_2acntS3805 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1166)->rc
            = _M0L11_2anew__cntS3806;
            if (_M0L8_2afieldS3519) {
              moonbit_incref(_M0L8_2afieldS3519);
            }
          } else if (_M0L6_2acntS3805 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1166);
          }
          _M0L7_2areprS1168 = _M0L8_2afieldS3519;
          if (_M0L7_2areprS1168 == 0) {
            if (_M0L7_2areprS1168) {
              moonbit_decref(_M0L7_2areprS1168);
            }
            moonbit_incref(_M0L3bufS1152);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1152, _M0L4_2anS1167);
          } else {
            moonbit_string_t _M0L7_2aSomeS1169 = _M0L7_2areprS1168;
            moonbit_string_t _M0L4_2arS1170 = _M0L7_2aSomeS1169;
            moonbit_incref(_M0L3bufS1152);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, _M0L4_2arS1170);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1152);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, (moonbit_string_t)moonbit_string_literal_43.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1152);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, (moonbit_string_t)moonbit_string_literal_44.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1157);
          moonbit_incref(_M0L3bufS1152);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1152, (moonbit_string_t)moonbit_string_literal_45.data);
          break;
        }
      }
      _M0L6_2atmpS2895 = 0;
      _M0L8_2aparamS1155 = _M0L6_2atmpS2895;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1152);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1151,
  int32_t _M0L6indentS1149
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1149 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1150 = _M0L6indentS1149 * _M0L5levelS1151;
    switch (_M0L6spacesS1150) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_46.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_47.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_48.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_49.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_50.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_51.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_52.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_53.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_54.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2857;
        moonbit_string_t _M0L6_2atmpS3520;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2857
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_55.data, _M0L6spacesS1150);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3520
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_46.data, _M0L6_2atmpS2857);
        moonbit_decref(_M0L6_2atmpS2857);
        return _M0L6_2atmpS3520;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1141,
  int32_t _M0L13escape__slashS1146
) {
  int32_t _M0L6_2atmpS2856;
  struct _M0TPB13StringBuilder* _M0L3bufS1140;
  struct _M0TWEOc* _M0L5_2aitS1142;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2856 = Moonbit_array_length(_M0L3strS1141);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1140 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2856);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1142 = _M0MPC16string6String4iter(_M0L3strS1141);
  while (1) {
    int32_t _M0L7_2abindS1143;
    moonbit_incref(_M0L5_2aitS1142);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1143 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1142);
    if (_M0L7_2abindS1143 == -1) {
      moonbit_decref(_M0L5_2aitS1142);
    } else {
      int32_t _M0L7_2aSomeS1144 = _M0L7_2abindS1143;
      int32_t _M0L4_2acS1145 = _M0L7_2aSomeS1144;
      if (_M0L4_2acS1145 == 34) {
        moonbit_incref(_M0L3bufS1140);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_56.data);
      } else if (_M0L4_2acS1145 == 92) {
        moonbit_incref(_M0L3bufS1140);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_57.data);
      } else if (_M0L4_2acS1145 == 47) {
        if (_M0L13escape__slashS1146) {
          moonbit_incref(_M0L3bufS1140);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_58.data);
        } else {
          moonbit_incref(_M0L3bufS1140);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1140, _M0L4_2acS1145);
        }
      } else if (_M0L4_2acS1145 == 10) {
        moonbit_incref(_M0L3bufS1140);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_59.data);
      } else if (_M0L4_2acS1145 == 13) {
        moonbit_incref(_M0L3bufS1140);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_60.data);
      } else if (_M0L4_2acS1145 == 8) {
        moonbit_incref(_M0L3bufS1140);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_61.data);
      } else if (_M0L4_2acS1145 == 9) {
        moonbit_incref(_M0L3bufS1140);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_62.data);
      } else {
        int32_t _M0L4codeS1147 = _M0L4_2acS1145;
        if (_M0L4codeS1147 == 12) {
          moonbit_incref(_M0L3bufS1140);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_63.data);
        } else if (_M0L4codeS1147 < 32) {
          int32_t _M0L6_2atmpS2855;
          moonbit_string_t _M0L6_2atmpS2854;
          moonbit_incref(_M0L3bufS1140);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, (moonbit_string_t)moonbit_string_literal_64.data);
          _M0L6_2atmpS2855 = _M0L4codeS1147 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2854 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2855);
          moonbit_incref(_M0L3bufS1140);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1140, _M0L6_2atmpS2854);
        } else {
          moonbit_incref(_M0L3bufS1140);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1140, _M0L4_2acS1145);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1140);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1139
) {
  int32_t _M0L8_2afieldS3521;
  int32_t _M0L3lenS2853;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3521 = _M0L4selfS1139->$1;
  moonbit_decref(_M0L4selfS1139);
  _M0L3lenS2853 = _M0L8_2afieldS3521;
  return _M0L3lenS2853 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1136
) {
  int32_t _M0L3lenS1135;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1135 = _M0L4selfS1136->$1;
  if (_M0L3lenS1135 == 0) {
    moonbit_decref(_M0L4selfS1136);
    return 0;
  } else {
    int32_t _M0L5indexS1137 = _M0L3lenS1135 - 1;
    void** _M0L8_2afieldS3525 = _M0L4selfS1136->$0;
    void** _M0L3bufS2852 = _M0L8_2afieldS3525;
    void* _M0L6_2atmpS3524 = (void*)_M0L3bufS2852[_M0L5indexS1137];
    void* _M0L1vS1138 = _M0L6_2atmpS3524;
    void** _M0L8_2afieldS3523 = _M0L4selfS1136->$0;
    void** _M0L3bufS2851 = _M0L8_2afieldS3523;
    void* _M0L6_2aoldS3522;
    if (
      _M0L5indexS1137 < 0
      || _M0L5indexS1137 >= Moonbit_array_length(_M0L3bufS2851)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3522 = (void*)_M0L3bufS2851[_M0L5indexS1137];
    moonbit_incref(_M0L1vS1138);
    moonbit_decref(_M0L6_2aoldS3522);
    if (
      _M0L5indexS1137 < 0
      || _M0L5indexS1137 >= Moonbit_array_length(_M0L3bufS2851)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2851[_M0L5indexS1137]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1136->$1 = _M0L5indexS1137;
    moonbit_decref(_M0L4selfS1136);
    return _M0L1vS1138;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1133,
  struct _M0TPB6Logger _M0L6loggerS1134
) {
  moonbit_string_t _M0L6_2atmpS2850;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2849;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2850 = _M0L4selfS1133;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2849 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2850);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2849, _M0L6loggerS1134);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1110,
  struct _M0TPB6Logger _M0L6loggerS1132
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3534;
  struct _M0TPC16string10StringView _M0L3pkgS1109;
  moonbit_string_t _M0L7_2adataS1111;
  int32_t _M0L8_2astartS1112;
  int32_t _M0L6_2atmpS2848;
  int32_t _M0L6_2aendS1113;
  int32_t _M0Lm9_2acursorS1114;
  int32_t _M0Lm13accept__stateS1115;
  int32_t _M0Lm10match__endS1116;
  int32_t _M0Lm20match__tag__saver__0S1117;
  int32_t _M0Lm6tag__0S1118;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1119;
  struct _M0TPC16string10StringView _M0L8_2afieldS3533;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1128;
  void* _M0L8_2afieldS3532;
  int32_t _M0L6_2acntS3807;
  void* _M0L16_2apackage__nameS1129;
  struct _M0TPC16string10StringView _M0L8_2afieldS3530;
  struct _M0TPC16string10StringView _M0L8filenameS2825;
  struct _M0TPC16string10StringView _M0L8_2afieldS3529;
  struct _M0TPC16string10StringView _M0L11start__lineS2826;
  struct _M0TPC16string10StringView _M0L8_2afieldS3528;
  struct _M0TPC16string10StringView _M0L13start__columnS2827;
  struct _M0TPC16string10StringView _M0L8_2afieldS3527;
  struct _M0TPC16string10StringView _M0L9end__lineS2828;
  struct _M0TPC16string10StringView _M0L8_2afieldS3526;
  int32_t _M0L6_2acntS3811;
  struct _M0TPC16string10StringView _M0L11end__columnS2829;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3534
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1110->$0_1, _M0L4selfS1110->$0_2, _M0L4selfS1110->$0_0
  };
  _M0L3pkgS1109 = _M0L8_2afieldS3534;
  moonbit_incref(_M0L3pkgS1109.$0);
  moonbit_incref(_M0L3pkgS1109.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1111 = _M0MPC16string10StringView4data(_M0L3pkgS1109);
  moonbit_incref(_M0L3pkgS1109.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1112
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1109);
  moonbit_incref(_M0L3pkgS1109.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2848 = _M0MPC16string10StringView6length(_M0L3pkgS1109);
  _M0L6_2aendS1113 = _M0L8_2astartS1112 + _M0L6_2atmpS2848;
  _M0Lm9_2acursorS1114 = _M0L8_2astartS1112;
  _M0Lm13accept__stateS1115 = -1;
  _M0Lm10match__endS1116 = -1;
  _M0Lm20match__tag__saver__0S1117 = -1;
  _M0Lm6tag__0S1118 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2840 = _M0Lm9_2acursorS1114;
    if (_M0L6_2atmpS2840 < _M0L6_2aendS1113) {
      int32_t _M0L6_2atmpS2847 = _M0Lm9_2acursorS1114;
      int32_t _M0L10next__charS1123;
      int32_t _M0L6_2atmpS2841;
      moonbit_incref(_M0L7_2adataS1111);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1123
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1111, _M0L6_2atmpS2847);
      _M0L6_2atmpS2841 = _M0Lm9_2acursorS1114;
      _M0Lm9_2acursorS1114 = _M0L6_2atmpS2841 + 1;
      if (_M0L10next__charS1123 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2842;
          _M0Lm6tag__0S1118 = _M0Lm9_2acursorS1114;
          _M0L6_2atmpS2842 = _M0Lm9_2acursorS1114;
          if (_M0L6_2atmpS2842 < _M0L6_2aendS1113) {
            int32_t _M0L6_2atmpS2846 = _M0Lm9_2acursorS1114;
            int32_t _M0L10next__charS1124;
            int32_t _M0L6_2atmpS2843;
            moonbit_incref(_M0L7_2adataS1111);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1124
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1111, _M0L6_2atmpS2846);
            _M0L6_2atmpS2843 = _M0Lm9_2acursorS1114;
            _M0Lm9_2acursorS1114 = _M0L6_2atmpS2843 + 1;
            if (_M0L10next__charS1124 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2844 = _M0Lm9_2acursorS1114;
                if (_M0L6_2atmpS2844 < _M0L6_2aendS1113) {
                  int32_t _M0L6_2atmpS2845 = _M0Lm9_2acursorS1114;
                  _M0Lm9_2acursorS1114 = _M0L6_2atmpS2845 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1117 = _M0Lm6tag__0S1118;
                  _M0Lm13accept__stateS1115 = 0;
                  _M0Lm10match__endS1116 = _M0Lm9_2acursorS1114;
                  goto join_1120;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1120;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1120;
    }
    break;
  }
  goto joinlet_3942;
  join_1120:;
  switch (_M0Lm13accept__stateS1115) {
    case 0: {
      int32_t _M0L6_2atmpS2838;
      int32_t _M0L6_2atmpS2837;
      int64_t _M0L6_2atmpS2834;
      int32_t _M0L6_2atmpS2836;
      int64_t _M0L6_2atmpS2835;
      struct _M0TPC16string10StringView _M0L13package__nameS1121;
      int64_t _M0L6_2atmpS2831;
      int32_t _M0L6_2atmpS2833;
      int64_t _M0L6_2atmpS2832;
      struct _M0TPC16string10StringView _M0L12module__nameS1122;
      void* _M0L4SomeS2830;
      moonbit_decref(_M0L3pkgS1109.$0);
      _M0L6_2atmpS2838 = _M0Lm20match__tag__saver__0S1117;
      _M0L6_2atmpS2837 = _M0L6_2atmpS2838 + 1;
      _M0L6_2atmpS2834 = (int64_t)_M0L6_2atmpS2837;
      _M0L6_2atmpS2836 = _M0Lm10match__endS1116;
      _M0L6_2atmpS2835 = (int64_t)_M0L6_2atmpS2836;
      moonbit_incref(_M0L7_2adataS1111);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1121
      = _M0MPC16string6String4view(_M0L7_2adataS1111, _M0L6_2atmpS2834, _M0L6_2atmpS2835);
      _M0L6_2atmpS2831 = (int64_t)_M0L8_2astartS1112;
      _M0L6_2atmpS2833 = _M0Lm20match__tag__saver__0S1117;
      _M0L6_2atmpS2832 = (int64_t)_M0L6_2atmpS2833;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1122
      = _M0MPC16string6String4view(_M0L7_2adataS1111, _M0L6_2atmpS2831, _M0L6_2atmpS2832);
      _M0L4SomeS2830
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2830)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2830)->$0_0
      = _M0L13package__nameS1121.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2830)->$0_1
      = _M0L13package__nameS1121.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2830)->$0_2
      = _M0L13package__nameS1121.$2;
      _M0L7_2abindS1119
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1119)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1119->$0_0 = _M0L12module__nameS1122.$0;
      _M0L7_2abindS1119->$0_1 = _M0L12module__nameS1122.$1;
      _M0L7_2abindS1119->$0_2 = _M0L12module__nameS1122.$2;
      _M0L7_2abindS1119->$1 = _M0L4SomeS2830;
      break;
    }
    default: {
      void* _M0L4NoneS2839;
      moonbit_decref(_M0L7_2adataS1111);
      _M0L4NoneS2839
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1119
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1119)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1119->$0_0 = _M0L3pkgS1109.$0;
      _M0L7_2abindS1119->$0_1 = _M0L3pkgS1109.$1;
      _M0L7_2abindS1119->$0_2 = _M0L3pkgS1109.$2;
      _M0L7_2abindS1119->$1 = _M0L4NoneS2839;
      break;
    }
  }
  joinlet_3942:;
  _M0L8_2afieldS3533
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1119->$0_1, _M0L7_2abindS1119->$0_2, _M0L7_2abindS1119->$0_0
  };
  _M0L15_2amodule__nameS1128 = _M0L8_2afieldS3533;
  _M0L8_2afieldS3532 = _M0L7_2abindS1119->$1;
  _M0L6_2acntS3807 = Moonbit_object_header(_M0L7_2abindS1119)->rc;
  if (_M0L6_2acntS3807 > 1) {
    int32_t _M0L11_2anew__cntS3808 = _M0L6_2acntS3807 - 1;
    Moonbit_object_header(_M0L7_2abindS1119)->rc = _M0L11_2anew__cntS3808;
    moonbit_incref(_M0L8_2afieldS3532);
    moonbit_incref(_M0L15_2amodule__nameS1128.$0);
  } else if (_M0L6_2acntS3807 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1119);
  }
  _M0L16_2apackage__nameS1129 = _M0L8_2afieldS3532;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1129)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1130 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1129;
      struct _M0TPC16string10StringView _M0L8_2afieldS3531 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1130->$0_1,
                                              _M0L7_2aSomeS1130->$0_2,
                                              _M0L7_2aSomeS1130->$0_0};
      int32_t _M0L6_2acntS3809 = Moonbit_object_header(_M0L7_2aSomeS1130)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1131;
      if (_M0L6_2acntS3809 > 1) {
        int32_t _M0L11_2anew__cntS3810 = _M0L6_2acntS3809 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1130)->rc = _M0L11_2anew__cntS3810;
        moonbit_incref(_M0L8_2afieldS3531.$0);
      } else if (_M0L6_2acntS3809 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1130);
      }
      _M0L12_2apkg__nameS1131 = _M0L8_2afieldS3531;
      if (_M0L6loggerS1132.$1) {
        moonbit_incref(_M0L6loggerS1132.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L12_2apkg__nameS1131);
      if (_M0L6loggerS1132.$1) {
        moonbit_incref(_M0L6loggerS1132.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1132.$0->$method_3(_M0L6loggerS1132.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1129);
      break;
    }
  }
  _M0L8_2afieldS3530
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1110->$1_1, _M0L4selfS1110->$1_2, _M0L4selfS1110->$1_0
  };
  _M0L8filenameS2825 = _M0L8_2afieldS3530;
  moonbit_incref(_M0L8filenameS2825.$0);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L8filenameS2825);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_3(_M0L6loggerS1132.$1, 58);
  _M0L8_2afieldS3529
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1110->$2_1, _M0L4selfS1110->$2_2, _M0L4selfS1110->$2_0
  };
  _M0L11start__lineS2826 = _M0L8_2afieldS3529;
  moonbit_incref(_M0L11start__lineS2826.$0);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L11start__lineS2826);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_3(_M0L6loggerS1132.$1, 58);
  _M0L8_2afieldS3528
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1110->$3_1, _M0L4selfS1110->$3_2, _M0L4selfS1110->$3_0
  };
  _M0L13start__columnS2827 = _M0L8_2afieldS3528;
  moonbit_incref(_M0L13start__columnS2827.$0);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L13start__columnS2827);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_3(_M0L6loggerS1132.$1, 45);
  _M0L8_2afieldS3527
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1110->$4_1, _M0L4selfS1110->$4_2, _M0L4selfS1110->$4_0
  };
  _M0L9end__lineS2828 = _M0L8_2afieldS3527;
  moonbit_incref(_M0L9end__lineS2828.$0);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L9end__lineS2828);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_3(_M0L6loggerS1132.$1, 58);
  _M0L8_2afieldS3526
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1110->$5_1, _M0L4selfS1110->$5_2, _M0L4selfS1110->$5_0
  };
  _M0L6_2acntS3811 = Moonbit_object_header(_M0L4selfS1110)->rc;
  if (_M0L6_2acntS3811 > 1) {
    int32_t _M0L11_2anew__cntS3817 = _M0L6_2acntS3811 - 1;
    Moonbit_object_header(_M0L4selfS1110)->rc = _M0L11_2anew__cntS3817;
    moonbit_incref(_M0L8_2afieldS3526.$0);
  } else if (_M0L6_2acntS3811 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3816 =
      (struct _M0TPC16string10StringView){_M0L4selfS1110->$4_1,
                                            _M0L4selfS1110->$4_2,
                                            _M0L4selfS1110->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3815;
    struct _M0TPC16string10StringView _M0L8_2afieldS3814;
    struct _M0TPC16string10StringView _M0L8_2afieldS3813;
    struct _M0TPC16string10StringView _M0L8_2afieldS3812;
    moonbit_decref(_M0L8_2afieldS3816.$0);
    _M0L8_2afieldS3815
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1110->$3_1, _M0L4selfS1110->$3_2, _M0L4selfS1110->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3815.$0);
    _M0L8_2afieldS3814
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1110->$2_1, _M0L4selfS1110->$2_2, _M0L4selfS1110->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3814.$0);
    _M0L8_2afieldS3813
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1110->$1_1, _M0L4selfS1110->$1_2, _M0L4selfS1110->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3813.$0);
    _M0L8_2afieldS3812
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1110->$0_1, _M0L4selfS1110->$0_2, _M0L4selfS1110->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3812.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1110);
  }
  _M0L11end__columnS2829 = _M0L8_2afieldS3526;
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L11end__columnS2829);
  if (_M0L6loggerS1132.$1) {
    moonbit_incref(_M0L6loggerS1132.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_3(_M0L6loggerS1132.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1132.$0->$method_2(_M0L6loggerS1132.$1, _M0L15_2amodule__nameS1128);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1108) {
  moonbit_string_t _M0L6_2atmpS2824;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2824
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1108);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2824);
  moonbit_decref(_M0L6_2atmpS2824);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1107,
  struct _M0TPB6Logger _M0L6loggerS1106
) {
  moonbit_string_t _M0L6_2atmpS2823;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2823 = _M0MPC16double6Double10to__string(_M0L4selfS1107);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1106.$0->$method_0(_M0L6loggerS1106.$1, _M0L6_2atmpS2823);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1105) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1105);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1092) {
  uint64_t _M0L4bitsS1093;
  uint64_t _M0L6_2atmpS2822;
  uint64_t _M0L6_2atmpS2821;
  int32_t _M0L8ieeeSignS1094;
  uint64_t _M0L12ieeeMantissaS1095;
  uint64_t _M0L6_2atmpS2820;
  uint64_t _M0L6_2atmpS2819;
  int32_t _M0L12ieeeExponentS1096;
  int32_t _if__result_3946;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1097;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1098;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2818;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1092 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_65.data;
  }
  _M0L4bitsS1093 = *(int64_t*)&_M0L3valS1092;
  _M0L6_2atmpS2822 = _M0L4bitsS1093 >> 63;
  _M0L6_2atmpS2821 = _M0L6_2atmpS2822 & 1ull;
  _M0L8ieeeSignS1094 = _M0L6_2atmpS2821 != 0ull;
  _M0L12ieeeMantissaS1095 = _M0L4bitsS1093 & 4503599627370495ull;
  _M0L6_2atmpS2820 = _M0L4bitsS1093 >> 52;
  _M0L6_2atmpS2819 = _M0L6_2atmpS2820 & 2047ull;
  _M0L12ieeeExponentS1096 = (int32_t)_M0L6_2atmpS2819;
  if (_M0L12ieeeExponentS1096 == 2047) {
    _if__result_3946 = 1;
  } else if (_M0L12ieeeExponentS1096 == 0) {
    _if__result_3946 = _M0L12ieeeMantissaS1095 == 0ull;
  } else {
    _if__result_3946 = 0;
  }
  if (_if__result_3946) {
    int32_t _M0L6_2atmpS2807 = _M0L12ieeeExponentS1096 != 0;
    int32_t _M0L6_2atmpS2808 = _M0L12ieeeMantissaS1095 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1094, _M0L6_2atmpS2807, _M0L6_2atmpS2808);
  }
  _M0Lm1vS1097 = _M0FPB31ryu__to__string_2erecord_2f1091;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1098
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1095, _M0L12ieeeExponentS1096);
  if (_M0L5smallS1098 == 0) {
    uint32_t _M0L6_2atmpS2809;
    if (_M0L5smallS1098) {
      moonbit_decref(_M0L5smallS1098);
    }
    _M0L6_2atmpS2809 = *(uint32_t*)&_M0L12ieeeExponentS1096;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1097 = _M0FPB3d2d(_M0L12ieeeMantissaS1095, _M0L6_2atmpS2809);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1099 = _M0L5smallS1098;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1100 = _M0L7_2aSomeS1099;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1101 = _M0L4_2afS1100;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2817 = _M0Lm1xS1101;
      uint64_t _M0L8_2afieldS3537 = _M0L6_2atmpS2817->$0;
      uint64_t _M0L8mantissaS2816 = _M0L8_2afieldS3537;
      uint64_t _M0L1qS1102 = _M0L8mantissaS2816 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2815 = _M0Lm1xS1101;
      uint64_t _M0L8_2afieldS3536 = _M0L6_2atmpS2815->$0;
      uint64_t _M0L8mantissaS2813 = _M0L8_2afieldS3536;
      uint64_t _M0L6_2atmpS2814 = 10ull * _M0L1qS1102;
      uint64_t _M0L1rS1103 = _M0L8mantissaS2813 - _M0L6_2atmpS2814;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2812;
      int32_t _M0L8_2afieldS3535;
      int32_t _M0L8exponentS2811;
      int32_t _M0L6_2atmpS2810;
      if (_M0L1rS1103 != 0ull) {
        break;
      }
      _M0L6_2atmpS2812 = _M0Lm1xS1101;
      _M0L8_2afieldS3535 = _M0L6_2atmpS2812->$1;
      moonbit_decref(_M0L6_2atmpS2812);
      _M0L8exponentS2811 = _M0L8_2afieldS3535;
      _M0L6_2atmpS2810 = _M0L8exponentS2811 + 1;
      _M0Lm1xS1101
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1101)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1101->$0 = _M0L1qS1102;
      _M0Lm1xS1101->$1 = _M0L6_2atmpS2810;
      continue;
      break;
    }
    _M0Lm1vS1097 = _M0Lm1xS1101;
  }
  _M0L6_2atmpS2818 = _M0Lm1vS1097;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2818, _M0L8ieeeSignS1094);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1086,
  int32_t _M0L12ieeeExponentS1088
) {
  uint64_t _M0L2m2S1085;
  int32_t _M0L6_2atmpS2806;
  int32_t _M0L2e2S1087;
  int32_t _M0L6_2atmpS2805;
  uint64_t _M0L6_2atmpS2804;
  uint64_t _M0L4maskS1089;
  uint64_t _M0L8fractionS1090;
  int32_t _M0L6_2atmpS2803;
  uint64_t _M0L6_2atmpS2802;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2801;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1085 = 4503599627370496ull | _M0L12ieeeMantissaS1086;
  _M0L6_2atmpS2806 = _M0L12ieeeExponentS1088 - 1023;
  _M0L2e2S1087 = _M0L6_2atmpS2806 - 52;
  if (_M0L2e2S1087 > 0) {
    return 0;
  }
  if (_M0L2e2S1087 < -52) {
    return 0;
  }
  _M0L6_2atmpS2805 = -_M0L2e2S1087;
  _M0L6_2atmpS2804 = 1ull << (_M0L6_2atmpS2805 & 63);
  _M0L4maskS1089 = _M0L6_2atmpS2804 - 1ull;
  _M0L8fractionS1090 = _M0L2m2S1085 & _M0L4maskS1089;
  if (_M0L8fractionS1090 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2803 = -_M0L2e2S1087;
  _M0L6_2atmpS2802 = _M0L2m2S1085 >> (_M0L6_2atmpS2803 & 63);
  _M0L6_2atmpS2801
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2801)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2801->$0 = _M0L6_2atmpS2802;
  _M0L6_2atmpS2801->$1 = 0;
  return _M0L6_2atmpS2801;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1059,
  int32_t _M0L4signS1057
) {
  int32_t _M0L6_2atmpS2800;
  moonbit_bytes_t _M0L6resultS1055;
  int32_t _M0Lm5indexS1056;
  uint64_t _M0Lm6outputS1058;
  uint64_t _M0L6_2atmpS2799;
  int32_t _M0L7olengthS1060;
  int32_t _M0L8_2afieldS3538;
  int32_t _M0L8exponentS2798;
  int32_t _M0L6_2atmpS2797;
  int32_t _M0Lm3expS1061;
  int32_t _M0L6_2atmpS2796;
  int32_t _M0L6_2atmpS2794;
  int32_t _M0L18scientificNotationS1062;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2800 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1055
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2800);
  _M0Lm5indexS1056 = 0;
  if (_M0L4signS1057) {
    int32_t _M0L6_2atmpS2669 = _M0Lm5indexS1056;
    int32_t _M0L6_2atmpS2670;
    if (
      _M0L6_2atmpS2669 < 0
      || _M0L6_2atmpS2669 >= Moonbit_array_length(_M0L6resultS1055)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1055[_M0L6_2atmpS2669] = 45;
    _M0L6_2atmpS2670 = _M0Lm5indexS1056;
    _M0Lm5indexS1056 = _M0L6_2atmpS2670 + 1;
  }
  _M0Lm6outputS1058 = _M0L1vS1059->$0;
  _M0L6_2atmpS2799 = _M0Lm6outputS1058;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1060 = _M0FPB17decimal__length17(_M0L6_2atmpS2799);
  _M0L8_2afieldS3538 = _M0L1vS1059->$1;
  moonbit_decref(_M0L1vS1059);
  _M0L8exponentS2798 = _M0L8_2afieldS3538;
  _M0L6_2atmpS2797 = _M0L8exponentS2798 + _M0L7olengthS1060;
  _M0Lm3expS1061 = _M0L6_2atmpS2797 - 1;
  _M0L6_2atmpS2796 = _M0Lm3expS1061;
  if (_M0L6_2atmpS2796 >= -6) {
    int32_t _M0L6_2atmpS2795 = _M0Lm3expS1061;
    _M0L6_2atmpS2794 = _M0L6_2atmpS2795 < 21;
  } else {
    _M0L6_2atmpS2794 = 0;
  }
  _M0L18scientificNotationS1062 = !_M0L6_2atmpS2794;
  if (_M0L18scientificNotationS1062) {
    int32_t _M0L7_2abindS1063 = _M0L7olengthS1060 - 1;
    int32_t _M0L1iS1064 = 0;
    int32_t _M0L6_2atmpS2680;
    uint64_t _M0L6_2atmpS2685;
    int32_t _M0L6_2atmpS2684;
    int32_t _M0L6_2atmpS2683;
    int32_t _M0L6_2atmpS2682;
    int32_t _M0L6_2atmpS2681;
    int32_t _M0L6_2atmpS2689;
    int32_t _M0L6_2atmpS2690;
    int32_t _M0L6_2atmpS2691;
    int32_t _M0L6_2atmpS2692;
    int32_t _M0L6_2atmpS2693;
    int32_t _M0L6_2atmpS2699;
    int32_t _M0L6_2atmpS2732;
    while (1) {
      if (_M0L1iS1064 < _M0L7_2abindS1063) {
        uint64_t _M0L6_2atmpS2678 = _M0Lm6outputS1058;
        uint64_t _M0L1cS1065 = _M0L6_2atmpS2678 % 10ull;
        uint64_t _M0L6_2atmpS2671 = _M0Lm6outputS1058;
        int32_t _M0L6_2atmpS2677;
        int32_t _M0L6_2atmpS2676;
        int32_t _M0L6_2atmpS2672;
        int32_t _M0L6_2atmpS2675;
        int32_t _M0L6_2atmpS2674;
        int32_t _M0L6_2atmpS2673;
        int32_t _M0L6_2atmpS2679;
        _M0Lm6outputS1058 = _M0L6_2atmpS2671 / 10ull;
        _M0L6_2atmpS2677 = _M0Lm5indexS1056;
        _M0L6_2atmpS2676 = _M0L6_2atmpS2677 + _M0L7olengthS1060;
        _M0L6_2atmpS2672 = _M0L6_2atmpS2676 - _M0L1iS1064;
        _M0L6_2atmpS2675 = (int32_t)_M0L1cS1065;
        _M0L6_2atmpS2674 = 48 + _M0L6_2atmpS2675;
        _M0L6_2atmpS2673 = _M0L6_2atmpS2674 & 0xff;
        if (
          _M0L6_2atmpS2672 < 0
          || _M0L6_2atmpS2672 >= Moonbit_array_length(_M0L6resultS1055)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1055[_M0L6_2atmpS2672] = _M0L6_2atmpS2673;
        _M0L6_2atmpS2679 = _M0L1iS1064 + 1;
        _M0L1iS1064 = _M0L6_2atmpS2679;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2680 = _M0Lm5indexS1056;
    _M0L6_2atmpS2685 = _M0Lm6outputS1058;
    _M0L6_2atmpS2684 = (int32_t)_M0L6_2atmpS2685;
    _M0L6_2atmpS2683 = _M0L6_2atmpS2684 % 10;
    _M0L6_2atmpS2682 = 48 + _M0L6_2atmpS2683;
    _M0L6_2atmpS2681 = _M0L6_2atmpS2682 & 0xff;
    if (
      _M0L6_2atmpS2680 < 0
      || _M0L6_2atmpS2680 >= Moonbit_array_length(_M0L6resultS1055)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1055[_M0L6_2atmpS2680] = _M0L6_2atmpS2681;
    if (_M0L7olengthS1060 > 1) {
      int32_t _M0L6_2atmpS2687 = _M0Lm5indexS1056;
      int32_t _M0L6_2atmpS2686 = _M0L6_2atmpS2687 + 1;
      if (
        _M0L6_2atmpS2686 < 0
        || _M0L6_2atmpS2686 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2686] = 46;
    } else {
      int32_t _M0L6_2atmpS2688 = _M0Lm5indexS1056;
      _M0Lm5indexS1056 = _M0L6_2atmpS2688 - 1;
    }
    _M0L6_2atmpS2689 = _M0Lm5indexS1056;
    _M0L6_2atmpS2690 = _M0L7olengthS1060 + 1;
    _M0Lm5indexS1056 = _M0L6_2atmpS2689 + _M0L6_2atmpS2690;
    _M0L6_2atmpS2691 = _M0Lm5indexS1056;
    if (
      _M0L6_2atmpS2691 < 0
      || _M0L6_2atmpS2691 >= Moonbit_array_length(_M0L6resultS1055)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1055[_M0L6_2atmpS2691] = 101;
    _M0L6_2atmpS2692 = _M0Lm5indexS1056;
    _M0Lm5indexS1056 = _M0L6_2atmpS2692 + 1;
    _M0L6_2atmpS2693 = _M0Lm3expS1061;
    if (_M0L6_2atmpS2693 < 0) {
      int32_t _M0L6_2atmpS2694 = _M0Lm5indexS1056;
      int32_t _M0L6_2atmpS2695;
      int32_t _M0L6_2atmpS2696;
      if (
        _M0L6_2atmpS2694 < 0
        || _M0L6_2atmpS2694 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2694] = 45;
      _M0L6_2atmpS2695 = _M0Lm5indexS1056;
      _M0Lm5indexS1056 = _M0L6_2atmpS2695 + 1;
      _M0L6_2atmpS2696 = _M0Lm3expS1061;
      _M0Lm3expS1061 = -_M0L6_2atmpS2696;
    } else {
      int32_t _M0L6_2atmpS2697 = _M0Lm5indexS1056;
      int32_t _M0L6_2atmpS2698;
      if (
        _M0L6_2atmpS2697 < 0
        || _M0L6_2atmpS2697 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2697] = 43;
      _M0L6_2atmpS2698 = _M0Lm5indexS1056;
      _M0Lm5indexS1056 = _M0L6_2atmpS2698 + 1;
    }
    _M0L6_2atmpS2699 = _M0Lm3expS1061;
    if (_M0L6_2atmpS2699 >= 100) {
      int32_t _M0L6_2atmpS2715 = _M0Lm3expS1061;
      int32_t _M0L1aS1067 = _M0L6_2atmpS2715 / 100;
      int32_t _M0L6_2atmpS2714 = _M0Lm3expS1061;
      int32_t _M0L6_2atmpS2713 = _M0L6_2atmpS2714 / 10;
      int32_t _M0L1bS1068 = _M0L6_2atmpS2713 % 10;
      int32_t _M0L6_2atmpS2712 = _M0Lm3expS1061;
      int32_t _M0L1cS1069 = _M0L6_2atmpS2712 % 10;
      int32_t _M0L6_2atmpS2700 = _M0Lm5indexS1056;
      int32_t _M0L6_2atmpS2702 = 48 + _M0L1aS1067;
      int32_t _M0L6_2atmpS2701 = _M0L6_2atmpS2702 & 0xff;
      int32_t _M0L6_2atmpS2706;
      int32_t _M0L6_2atmpS2703;
      int32_t _M0L6_2atmpS2705;
      int32_t _M0L6_2atmpS2704;
      int32_t _M0L6_2atmpS2710;
      int32_t _M0L6_2atmpS2707;
      int32_t _M0L6_2atmpS2709;
      int32_t _M0L6_2atmpS2708;
      int32_t _M0L6_2atmpS2711;
      if (
        _M0L6_2atmpS2700 < 0
        || _M0L6_2atmpS2700 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2700] = _M0L6_2atmpS2701;
      _M0L6_2atmpS2706 = _M0Lm5indexS1056;
      _M0L6_2atmpS2703 = _M0L6_2atmpS2706 + 1;
      _M0L6_2atmpS2705 = 48 + _M0L1bS1068;
      _M0L6_2atmpS2704 = _M0L6_2atmpS2705 & 0xff;
      if (
        _M0L6_2atmpS2703 < 0
        || _M0L6_2atmpS2703 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2703] = _M0L6_2atmpS2704;
      _M0L6_2atmpS2710 = _M0Lm5indexS1056;
      _M0L6_2atmpS2707 = _M0L6_2atmpS2710 + 2;
      _M0L6_2atmpS2709 = 48 + _M0L1cS1069;
      _M0L6_2atmpS2708 = _M0L6_2atmpS2709 & 0xff;
      if (
        _M0L6_2atmpS2707 < 0
        || _M0L6_2atmpS2707 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2707] = _M0L6_2atmpS2708;
      _M0L6_2atmpS2711 = _M0Lm5indexS1056;
      _M0Lm5indexS1056 = _M0L6_2atmpS2711 + 3;
    } else {
      int32_t _M0L6_2atmpS2716 = _M0Lm3expS1061;
      if (_M0L6_2atmpS2716 >= 10) {
        int32_t _M0L6_2atmpS2726 = _M0Lm3expS1061;
        int32_t _M0L1aS1070 = _M0L6_2atmpS2726 / 10;
        int32_t _M0L6_2atmpS2725 = _M0Lm3expS1061;
        int32_t _M0L1bS1071 = _M0L6_2atmpS2725 % 10;
        int32_t _M0L6_2atmpS2717 = _M0Lm5indexS1056;
        int32_t _M0L6_2atmpS2719 = 48 + _M0L1aS1070;
        int32_t _M0L6_2atmpS2718 = _M0L6_2atmpS2719 & 0xff;
        int32_t _M0L6_2atmpS2723;
        int32_t _M0L6_2atmpS2720;
        int32_t _M0L6_2atmpS2722;
        int32_t _M0L6_2atmpS2721;
        int32_t _M0L6_2atmpS2724;
        if (
          _M0L6_2atmpS2717 < 0
          || _M0L6_2atmpS2717 >= Moonbit_array_length(_M0L6resultS1055)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1055[_M0L6_2atmpS2717] = _M0L6_2atmpS2718;
        _M0L6_2atmpS2723 = _M0Lm5indexS1056;
        _M0L6_2atmpS2720 = _M0L6_2atmpS2723 + 1;
        _M0L6_2atmpS2722 = 48 + _M0L1bS1071;
        _M0L6_2atmpS2721 = _M0L6_2atmpS2722 & 0xff;
        if (
          _M0L6_2atmpS2720 < 0
          || _M0L6_2atmpS2720 >= Moonbit_array_length(_M0L6resultS1055)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1055[_M0L6_2atmpS2720] = _M0L6_2atmpS2721;
        _M0L6_2atmpS2724 = _M0Lm5indexS1056;
        _M0Lm5indexS1056 = _M0L6_2atmpS2724 + 2;
      } else {
        int32_t _M0L6_2atmpS2727 = _M0Lm5indexS1056;
        int32_t _M0L6_2atmpS2730 = _M0Lm3expS1061;
        int32_t _M0L6_2atmpS2729 = 48 + _M0L6_2atmpS2730;
        int32_t _M0L6_2atmpS2728 = _M0L6_2atmpS2729 & 0xff;
        int32_t _M0L6_2atmpS2731;
        if (
          _M0L6_2atmpS2727 < 0
          || _M0L6_2atmpS2727 >= Moonbit_array_length(_M0L6resultS1055)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1055[_M0L6_2atmpS2727] = _M0L6_2atmpS2728;
        _M0L6_2atmpS2731 = _M0Lm5indexS1056;
        _M0Lm5indexS1056 = _M0L6_2atmpS2731 + 1;
      }
    }
    _M0L6_2atmpS2732 = _M0Lm5indexS1056;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1055, 0, _M0L6_2atmpS2732);
  } else {
    int32_t _M0L6_2atmpS2733 = _M0Lm3expS1061;
    int32_t _M0L6_2atmpS2793;
    if (_M0L6_2atmpS2733 < 0) {
      int32_t _M0L6_2atmpS2734 = _M0Lm5indexS1056;
      int32_t _M0L6_2atmpS2735;
      int32_t _M0L6_2atmpS2736;
      int32_t _M0L6_2atmpS2737;
      int32_t _M0L1iS1072;
      int32_t _M0L7currentS1074;
      int32_t _M0L1iS1075;
      if (
        _M0L6_2atmpS2734 < 0
        || _M0L6_2atmpS2734 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2734] = 48;
      _M0L6_2atmpS2735 = _M0Lm5indexS1056;
      _M0Lm5indexS1056 = _M0L6_2atmpS2735 + 1;
      _M0L6_2atmpS2736 = _M0Lm5indexS1056;
      if (
        _M0L6_2atmpS2736 < 0
        || _M0L6_2atmpS2736 >= Moonbit_array_length(_M0L6resultS1055)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1055[_M0L6_2atmpS2736] = 46;
      _M0L6_2atmpS2737 = _M0Lm5indexS1056;
      _M0Lm5indexS1056 = _M0L6_2atmpS2737 + 1;
      _M0L1iS1072 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2738 = _M0Lm3expS1061;
        if (_M0L1iS1072 > _M0L6_2atmpS2738) {
          int32_t _M0L6_2atmpS2739 = _M0Lm5indexS1056;
          int32_t _M0L6_2atmpS2740;
          int32_t _M0L6_2atmpS2741;
          if (
            _M0L6_2atmpS2739 < 0
            || _M0L6_2atmpS2739 >= Moonbit_array_length(_M0L6resultS1055)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1055[_M0L6_2atmpS2739] = 48;
          _M0L6_2atmpS2740 = _M0Lm5indexS1056;
          _M0Lm5indexS1056 = _M0L6_2atmpS2740 + 1;
          _M0L6_2atmpS2741 = _M0L1iS1072 - 1;
          _M0L1iS1072 = _M0L6_2atmpS2741;
          continue;
        }
        break;
      }
      _M0L7currentS1074 = _M0Lm5indexS1056;
      _M0L1iS1075 = 0;
      while (1) {
        if (_M0L1iS1075 < _M0L7olengthS1060) {
          int32_t _M0L6_2atmpS2749 = _M0L7currentS1074 + _M0L7olengthS1060;
          int32_t _M0L6_2atmpS2748 = _M0L6_2atmpS2749 - _M0L1iS1075;
          int32_t _M0L6_2atmpS2742 = _M0L6_2atmpS2748 - 1;
          uint64_t _M0L6_2atmpS2747 = _M0Lm6outputS1058;
          uint64_t _M0L6_2atmpS2746 = _M0L6_2atmpS2747 % 10ull;
          int32_t _M0L6_2atmpS2745 = (int32_t)_M0L6_2atmpS2746;
          int32_t _M0L6_2atmpS2744 = 48 + _M0L6_2atmpS2745;
          int32_t _M0L6_2atmpS2743 = _M0L6_2atmpS2744 & 0xff;
          uint64_t _M0L6_2atmpS2750;
          int32_t _M0L6_2atmpS2751;
          int32_t _M0L6_2atmpS2752;
          if (
            _M0L6_2atmpS2742 < 0
            || _M0L6_2atmpS2742 >= Moonbit_array_length(_M0L6resultS1055)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1055[_M0L6_2atmpS2742] = _M0L6_2atmpS2743;
          _M0L6_2atmpS2750 = _M0Lm6outputS1058;
          _M0Lm6outputS1058 = _M0L6_2atmpS2750 / 10ull;
          _M0L6_2atmpS2751 = _M0Lm5indexS1056;
          _M0Lm5indexS1056 = _M0L6_2atmpS2751 + 1;
          _M0L6_2atmpS2752 = _M0L1iS1075 + 1;
          _M0L1iS1075 = _M0L6_2atmpS2752;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2754 = _M0Lm3expS1061;
      int32_t _M0L6_2atmpS2753 = _M0L6_2atmpS2754 + 1;
      if (_M0L6_2atmpS2753 >= _M0L7olengthS1060) {
        int32_t _M0L1iS1077 = 0;
        int32_t _M0L6_2atmpS2766;
        int32_t _M0L6_2atmpS2770;
        int32_t _M0L7_2abindS1079;
        int32_t _M0L2__S1080;
        while (1) {
          if (_M0L1iS1077 < _M0L7olengthS1060) {
            int32_t _M0L6_2atmpS2763 = _M0Lm5indexS1056;
            int32_t _M0L6_2atmpS2762 = _M0L6_2atmpS2763 + _M0L7olengthS1060;
            int32_t _M0L6_2atmpS2761 = _M0L6_2atmpS2762 - _M0L1iS1077;
            int32_t _M0L6_2atmpS2755 = _M0L6_2atmpS2761 - 1;
            uint64_t _M0L6_2atmpS2760 = _M0Lm6outputS1058;
            uint64_t _M0L6_2atmpS2759 = _M0L6_2atmpS2760 % 10ull;
            int32_t _M0L6_2atmpS2758 = (int32_t)_M0L6_2atmpS2759;
            int32_t _M0L6_2atmpS2757 = 48 + _M0L6_2atmpS2758;
            int32_t _M0L6_2atmpS2756 = _M0L6_2atmpS2757 & 0xff;
            uint64_t _M0L6_2atmpS2764;
            int32_t _M0L6_2atmpS2765;
            if (
              _M0L6_2atmpS2755 < 0
              || _M0L6_2atmpS2755 >= Moonbit_array_length(_M0L6resultS1055)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1055[_M0L6_2atmpS2755] = _M0L6_2atmpS2756;
            _M0L6_2atmpS2764 = _M0Lm6outputS1058;
            _M0Lm6outputS1058 = _M0L6_2atmpS2764 / 10ull;
            _M0L6_2atmpS2765 = _M0L1iS1077 + 1;
            _M0L1iS1077 = _M0L6_2atmpS2765;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2766 = _M0Lm5indexS1056;
        _M0Lm5indexS1056 = _M0L6_2atmpS2766 + _M0L7olengthS1060;
        _M0L6_2atmpS2770 = _M0Lm3expS1061;
        _M0L7_2abindS1079 = _M0L6_2atmpS2770 + 1;
        _M0L2__S1080 = _M0L7olengthS1060;
        while (1) {
          if (_M0L2__S1080 < _M0L7_2abindS1079) {
            int32_t _M0L6_2atmpS2767 = _M0Lm5indexS1056;
            int32_t _M0L6_2atmpS2768;
            int32_t _M0L6_2atmpS2769;
            if (
              _M0L6_2atmpS2767 < 0
              || _M0L6_2atmpS2767 >= Moonbit_array_length(_M0L6resultS1055)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1055[_M0L6_2atmpS2767] = 48;
            _M0L6_2atmpS2768 = _M0Lm5indexS1056;
            _M0Lm5indexS1056 = _M0L6_2atmpS2768 + 1;
            _M0L6_2atmpS2769 = _M0L2__S1080 + 1;
            _M0L2__S1080 = _M0L6_2atmpS2769;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2792 = _M0Lm5indexS1056;
        int32_t _M0Lm7currentS1082 = _M0L6_2atmpS2792 + 1;
        int32_t _M0L1iS1083 = 0;
        int32_t _M0L6_2atmpS2790;
        int32_t _M0L6_2atmpS2791;
        while (1) {
          if (_M0L1iS1083 < _M0L7olengthS1060) {
            int32_t _M0L6_2atmpS2773 = _M0L7olengthS1060 - _M0L1iS1083;
            int32_t _M0L6_2atmpS2771 = _M0L6_2atmpS2773 - 1;
            int32_t _M0L6_2atmpS2772 = _M0Lm3expS1061;
            int32_t _M0L6_2atmpS2787;
            int32_t _M0L6_2atmpS2786;
            int32_t _M0L6_2atmpS2785;
            int32_t _M0L6_2atmpS2779;
            uint64_t _M0L6_2atmpS2784;
            uint64_t _M0L6_2atmpS2783;
            int32_t _M0L6_2atmpS2782;
            int32_t _M0L6_2atmpS2781;
            int32_t _M0L6_2atmpS2780;
            uint64_t _M0L6_2atmpS2788;
            int32_t _M0L6_2atmpS2789;
            if (_M0L6_2atmpS2771 == _M0L6_2atmpS2772) {
              int32_t _M0L6_2atmpS2777 = _M0Lm7currentS1082;
              int32_t _M0L6_2atmpS2776 = _M0L6_2atmpS2777 + _M0L7olengthS1060;
              int32_t _M0L6_2atmpS2775 = _M0L6_2atmpS2776 - _M0L1iS1083;
              int32_t _M0L6_2atmpS2774 = _M0L6_2atmpS2775 - 1;
              int32_t _M0L6_2atmpS2778;
              if (
                _M0L6_2atmpS2774 < 0
                || _M0L6_2atmpS2774 >= Moonbit_array_length(_M0L6resultS1055)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1055[_M0L6_2atmpS2774] = 46;
              _M0L6_2atmpS2778 = _M0Lm7currentS1082;
              _M0Lm7currentS1082 = _M0L6_2atmpS2778 - 1;
            }
            _M0L6_2atmpS2787 = _M0Lm7currentS1082;
            _M0L6_2atmpS2786 = _M0L6_2atmpS2787 + _M0L7olengthS1060;
            _M0L6_2atmpS2785 = _M0L6_2atmpS2786 - _M0L1iS1083;
            _M0L6_2atmpS2779 = _M0L6_2atmpS2785 - 1;
            _M0L6_2atmpS2784 = _M0Lm6outputS1058;
            _M0L6_2atmpS2783 = _M0L6_2atmpS2784 % 10ull;
            _M0L6_2atmpS2782 = (int32_t)_M0L6_2atmpS2783;
            _M0L6_2atmpS2781 = 48 + _M0L6_2atmpS2782;
            _M0L6_2atmpS2780 = _M0L6_2atmpS2781 & 0xff;
            if (
              _M0L6_2atmpS2779 < 0
              || _M0L6_2atmpS2779 >= Moonbit_array_length(_M0L6resultS1055)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1055[_M0L6_2atmpS2779] = _M0L6_2atmpS2780;
            _M0L6_2atmpS2788 = _M0Lm6outputS1058;
            _M0Lm6outputS1058 = _M0L6_2atmpS2788 / 10ull;
            _M0L6_2atmpS2789 = _M0L1iS1083 + 1;
            _M0L1iS1083 = _M0L6_2atmpS2789;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2790 = _M0Lm5indexS1056;
        _M0L6_2atmpS2791 = _M0L7olengthS1060 + 1;
        _M0Lm5indexS1056 = _M0L6_2atmpS2790 + _M0L6_2atmpS2791;
      }
    }
    _M0L6_2atmpS2793 = _M0Lm5indexS1056;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1055, 0, _M0L6_2atmpS2793);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1001,
  uint32_t _M0L12ieeeExponentS1000
) {
  int32_t _M0Lm2e2S998;
  uint64_t _M0Lm2m2S999;
  uint64_t _M0L6_2atmpS2668;
  uint64_t _M0L6_2atmpS2667;
  int32_t _M0L4evenS1002;
  uint64_t _M0L6_2atmpS2666;
  uint64_t _M0L2mvS1003;
  int32_t _M0L7mmShiftS1004;
  uint64_t _M0Lm2vrS1005;
  uint64_t _M0Lm2vpS1006;
  uint64_t _M0Lm2vmS1007;
  int32_t _M0Lm3e10S1008;
  int32_t _M0Lm17vmIsTrailingZerosS1009;
  int32_t _M0Lm17vrIsTrailingZerosS1010;
  int32_t _M0L6_2atmpS2568;
  int32_t _M0Lm7removedS1029;
  int32_t _M0Lm16lastRemovedDigitS1030;
  uint64_t _M0Lm6outputS1031;
  int32_t _M0L6_2atmpS2664;
  int32_t _M0L6_2atmpS2665;
  int32_t _M0L3expS1054;
  uint64_t _M0L6_2atmpS2663;
  struct _M0TPB17FloatingDecimal64* _block_3959;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S998 = 0;
  _M0Lm2m2S999 = 0ull;
  if (_M0L12ieeeExponentS1000 == 0u) {
    _M0Lm2e2S998 = -1076;
    _M0Lm2m2S999 = _M0L12ieeeMantissaS1001;
  } else {
    int32_t _M0L6_2atmpS2567 = *(int32_t*)&_M0L12ieeeExponentS1000;
    int32_t _M0L6_2atmpS2566 = _M0L6_2atmpS2567 - 1023;
    int32_t _M0L6_2atmpS2565 = _M0L6_2atmpS2566 - 52;
    _M0Lm2e2S998 = _M0L6_2atmpS2565 - 2;
    _M0Lm2m2S999 = 4503599627370496ull | _M0L12ieeeMantissaS1001;
  }
  _M0L6_2atmpS2668 = _M0Lm2m2S999;
  _M0L6_2atmpS2667 = _M0L6_2atmpS2668 & 1ull;
  _M0L4evenS1002 = _M0L6_2atmpS2667 == 0ull;
  _M0L6_2atmpS2666 = _M0Lm2m2S999;
  _M0L2mvS1003 = 4ull * _M0L6_2atmpS2666;
  if (_M0L12ieeeMantissaS1001 != 0ull) {
    _M0L7mmShiftS1004 = 1;
  } else {
    _M0L7mmShiftS1004 = _M0L12ieeeExponentS1000 <= 1u;
  }
  _M0Lm2vrS1005 = 0ull;
  _M0Lm2vpS1006 = 0ull;
  _M0Lm2vmS1007 = 0ull;
  _M0Lm3e10S1008 = 0;
  _M0Lm17vmIsTrailingZerosS1009 = 0;
  _M0Lm17vrIsTrailingZerosS1010 = 0;
  _M0L6_2atmpS2568 = _M0Lm2e2S998;
  if (_M0L6_2atmpS2568 >= 0) {
    int32_t _M0L6_2atmpS2590 = _M0Lm2e2S998;
    int32_t _M0L6_2atmpS2586;
    int32_t _M0L6_2atmpS2589;
    int32_t _M0L6_2atmpS2588;
    int32_t _M0L6_2atmpS2587;
    int32_t _M0L1qS1011;
    int32_t _M0L6_2atmpS2585;
    int32_t _M0L6_2atmpS2584;
    int32_t _M0L1kS1012;
    int32_t _M0L6_2atmpS2583;
    int32_t _M0L6_2atmpS2582;
    int32_t _M0L6_2atmpS2581;
    int32_t _M0L1iS1013;
    struct _M0TPB8Pow5Pair _M0L4pow5S1014;
    uint64_t _M0L6_2atmpS2580;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1015;
    uint64_t _M0L8_2avrOutS1016;
    uint64_t _M0L8_2avpOutS1017;
    uint64_t _M0L8_2avmOutS1018;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2586 = _M0FPB9log10Pow2(_M0L6_2atmpS2590);
    _M0L6_2atmpS2589 = _M0Lm2e2S998;
    _M0L6_2atmpS2588 = _M0L6_2atmpS2589 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2587 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2588);
    _M0L1qS1011 = _M0L6_2atmpS2586 - _M0L6_2atmpS2587;
    _M0Lm3e10S1008 = _M0L1qS1011;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2585 = _M0FPB8pow5bits(_M0L1qS1011);
    _M0L6_2atmpS2584 = 125 + _M0L6_2atmpS2585;
    _M0L1kS1012 = _M0L6_2atmpS2584 - 1;
    _M0L6_2atmpS2583 = _M0Lm2e2S998;
    _M0L6_2atmpS2582 = -_M0L6_2atmpS2583;
    _M0L6_2atmpS2581 = _M0L6_2atmpS2582 + _M0L1qS1011;
    _M0L1iS1013 = _M0L6_2atmpS2581 + _M0L1kS1012;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1014 = _M0FPB22double__computeInvPow5(_M0L1qS1011);
    _M0L6_2atmpS2580 = _M0Lm2m2S999;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1015
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2580, _M0L4pow5S1014, _M0L1iS1013, _M0L7mmShiftS1004);
    _M0L8_2avrOutS1016 = _M0L7_2abindS1015.$0;
    _M0L8_2avpOutS1017 = _M0L7_2abindS1015.$1;
    _M0L8_2avmOutS1018 = _M0L7_2abindS1015.$2;
    _M0Lm2vrS1005 = _M0L8_2avrOutS1016;
    _M0Lm2vpS1006 = _M0L8_2avpOutS1017;
    _M0Lm2vmS1007 = _M0L8_2avmOutS1018;
    if (_M0L1qS1011 <= 21) {
      int32_t _M0L6_2atmpS2576 = (int32_t)_M0L2mvS1003;
      uint64_t _M0L6_2atmpS2579 = _M0L2mvS1003 / 5ull;
      int32_t _M0L6_2atmpS2578 = (int32_t)_M0L6_2atmpS2579;
      int32_t _M0L6_2atmpS2577 = 5 * _M0L6_2atmpS2578;
      int32_t _M0L6mvMod5S1019 = _M0L6_2atmpS2576 - _M0L6_2atmpS2577;
      if (_M0L6mvMod5S1019 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1010
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1003, _M0L1qS1011);
      } else if (_M0L4evenS1002) {
        uint64_t _M0L6_2atmpS2570 = _M0L2mvS1003 - 1ull;
        uint64_t _M0L6_2atmpS2571;
        uint64_t _M0L6_2atmpS2569;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2571 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1004);
        _M0L6_2atmpS2569 = _M0L6_2atmpS2570 - _M0L6_2atmpS2571;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1009
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2569, _M0L1qS1011);
      } else {
        uint64_t _M0L6_2atmpS2572 = _M0Lm2vpS1006;
        uint64_t _M0L6_2atmpS2575 = _M0L2mvS1003 + 2ull;
        int32_t _M0L6_2atmpS2574;
        uint64_t _M0L6_2atmpS2573;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2574
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2575, _M0L1qS1011);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2573 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2574);
        _M0Lm2vpS1006 = _M0L6_2atmpS2572 - _M0L6_2atmpS2573;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2604 = _M0Lm2e2S998;
    int32_t _M0L6_2atmpS2603 = -_M0L6_2atmpS2604;
    int32_t _M0L6_2atmpS2598;
    int32_t _M0L6_2atmpS2602;
    int32_t _M0L6_2atmpS2601;
    int32_t _M0L6_2atmpS2600;
    int32_t _M0L6_2atmpS2599;
    int32_t _M0L1qS1020;
    int32_t _M0L6_2atmpS2591;
    int32_t _M0L6_2atmpS2597;
    int32_t _M0L6_2atmpS2596;
    int32_t _M0L1iS1021;
    int32_t _M0L6_2atmpS2595;
    int32_t _M0L1kS1022;
    int32_t _M0L1jS1023;
    struct _M0TPB8Pow5Pair _M0L4pow5S1024;
    uint64_t _M0L6_2atmpS2594;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1025;
    uint64_t _M0L8_2avrOutS1026;
    uint64_t _M0L8_2avpOutS1027;
    uint64_t _M0L8_2avmOutS1028;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2598 = _M0FPB9log10Pow5(_M0L6_2atmpS2603);
    _M0L6_2atmpS2602 = _M0Lm2e2S998;
    _M0L6_2atmpS2601 = -_M0L6_2atmpS2602;
    _M0L6_2atmpS2600 = _M0L6_2atmpS2601 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2599 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2600);
    _M0L1qS1020 = _M0L6_2atmpS2598 - _M0L6_2atmpS2599;
    _M0L6_2atmpS2591 = _M0Lm2e2S998;
    _M0Lm3e10S1008 = _M0L1qS1020 + _M0L6_2atmpS2591;
    _M0L6_2atmpS2597 = _M0Lm2e2S998;
    _M0L6_2atmpS2596 = -_M0L6_2atmpS2597;
    _M0L1iS1021 = _M0L6_2atmpS2596 - _M0L1qS1020;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2595 = _M0FPB8pow5bits(_M0L1iS1021);
    _M0L1kS1022 = _M0L6_2atmpS2595 - 125;
    _M0L1jS1023 = _M0L1qS1020 - _M0L1kS1022;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1024 = _M0FPB19double__computePow5(_M0L1iS1021);
    _M0L6_2atmpS2594 = _M0Lm2m2S999;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1025
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2594, _M0L4pow5S1024, _M0L1jS1023, _M0L7mmShiftS1004);
    _M0L8_2avrOutS1026 = _M0L7_2abindS1025.$0;
    _M0L8_2avpOutS1027 = _M0L7_2abindS1025.$1;
    _M0L8_2avmOutS1028 = _M0L7_2abindS1025.$2;
    _M0Lm2vrS1005 = _M0L8_2avrOutS1026;
    _M0Lm2vpS1006 = _M0L8_2avpOutS1027;
    _M0Lm2vmS1007 = _M0L8_2avmOutS1028;
    if (_M0L1qS1020 <= 1) {
      _M0Lm17vrIsTrailingZerosS1010 = 1;
      if (_M0L4evenS1002) {
        int32_t _M0L6_2atmpS2592;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2592 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1004);
        _M0Lm17vmIsTrailingZerosS1009 = _M0L6_2atmpS2592 == 1;
      } else {
        uint64_t _M0L6_2atmpS2593 = _M0Lm2vpS1006;
        _M0Lm2vpS1006 = _M0L6_2atmpS2593 - 1ull;
      }
    } else if (_M0L1qS1020 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1010
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1003, _M0L1qS1020);
    }
  }
  _M0Lm7removedS1029 = 0;
  _M0Lm16lastRemovedDigitS1030 = 0;
  _M0Lm6outputS1031 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1009 || _M0Lm17vrIsTrailingZerosS1010) {
    int32_t _if__result_3956;
    uint64_t _M0L6_2atmpS2634;
    uint64_t _M0L6_2atmpS2640;
    uint64_t _M0L6_2atmpS2641;
    int32_t _if__result_3957;
    int32_t _M0L6_2atmpS2637;
    int64_t _M0L6_2atmpS2636;
    uint64_t _M0L6_2atmpS2635;
    while (1) {
      uint64_t _M0L6_2atmpS2617 = _M0Lm2vpS1006;
      uint64_t _M0L7vpDiv10S1032 = _M0L6_2atmpS2617 / 10ull;
      uint64_t _M0L6_2atmpS2616 = _M0Lm2vmS1007;
      uint64_t _M0L7vmDiv10S1033 = _M0L6_2atmpS2616 / 10ull;
      uint64_t _M0L6_2atmpS2615;
      int32_t _M0L6_2atmpS2612;
      int32_t _M0L6_2atmpS2614;
      int32_t _M0L6_2atmpS2613;
      int32_t _M0L7vmMod10S1035;
      uint64_t _M0L6_2atmpS2611;
      uint64_t _M0L7vrDiv10S1036;
      uint64_t _M0L6_2atmpS2610;
      int32_t _M0L6_2atmpS2607;
      int32_t _M0L6_2atmpS2609;
      int32_t _M0L6_2atmpS2608;
      int32_t _M0L7vrMod10S1037;
      int32_t _M0L6_2atmpS2606;
      if (_M0L7vpDiv10S1032 <= _M0L7vmDiv10S1033) {
        break;
      }
      _M0L6_2atmpS2615 = _M0Lm2vmS1007;
      _M0L6_2atmpS2612 = (int32_t)_M0L6_2atmpS2615;
      _M0L6_2atmpS2614 = (int32_t)_M0L7vmDiv10S1033;
      _M0L6_2atmpS2613 = 10 * _M0L6_2atmpS2614;
      _M0L7vmMod10S1035 = _M0L6_2atmpS2612 - _M0L6_2atmpS2613;
      _M0L6_2atmpS2611 = _M0Lm2vrS1005;
      _M0L7vrDiv10S1036 = _M0L6_2atmpS2611 / 10ull;
      _M0L6_2atmpS2610 = _M0Lm2vrS1005;
      _M0L6_2atmpS2607 = (int32_t)_M0L6_2atmpS2610;
      _M0L6_2atmpS2609 = (int32_t)_M0L7vrDiv10S1036;
      _M0L6_2atmpS2608 = 10 * _M0L6_2atmpS2609;
      _M0L7vrMod10S1037 = _M0L6_2atmpS2607 - _M0L6_2atmpS2608;
      if (_M0Lm17vmIsTrailingZerosS1009) {
        _M0Lm17vmIsTrailingZerosS1009 = _M0L7vmMod10S1035 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1009 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1010) {
        int32_t _M0L6_2atmpS2605 = _M0Lm16lastRemovedDigitS1030;
        _M0Lm17vrIsTrailingZerosS1010 = _M0L6_2atmpS2605 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1010 = 0;
      }
      _M0Lm16lastRemovedDigitS1030 = _M0L7vrMod10S1037;
      _M0Lm2vrS1005 = _M0L7vrDiv10S1036;
      _M0Lm2vpS1006 = _M0L7vpDiv10S1032;
      _M0Lm2vmS1007 = _M0L7vmDiv10S1033;
      _M0L6_2atmpS2606 = _M0Lm7removedS1029;
      _M0Lm7removedS1029 = _M0L6_2atmpS2606 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1009) {
      while (1) {
        uint64_t _M0L6_2atmpS2630 = _M0Lm2vmS1007;
        uint64_t _M0L7vmDiv10S1038 = _M0L6_2atmpS2630 / 10ull;
        uint64_t _M0L6_2atmpS2629 = _M0Lm2vmS1007;
        int32_t _M0L6_2atmpS2626 = (int32_t)_M0L6_2atmpS2629;
        int32_t _M0L6_2atmpS2628 = (int32_t)_M0L7vmDiv10S1038;
        int32_t _M0L6_2atmpS2627 = 10 * _M0L6_2atmpS2628;
        int32_t _M0L7vmMod10S1039 = _M0L6_2atmpS2626 - _M0L6_2atmpS2627;
        uint64_t _M0L6_2atmpS2625;
        uint64_t _M0L7vpDiv10S1041;
        uint64_t _M0L6_2atmpS2624;
        uint64_t _M0L7vrDiv10S1042;
        uint64_t _M0L6_2atmpS2623;
        int32_t _M0L6_2atmpS2620;
        int32_t _M0L6_2atmpS2622;
        int32_t _M0L6_2atmpS2621;
        int32_t _M0L7vrMod10S1043;
        int32_t _M0L6_2atmpS2619;
        if (_M0L7vmMod10S1039 != 0) {
          break;
        }
        _M0L6_2atmpS2625 = _M0Lm2vpS1006;
        _M0L7vpDiv10S1041 = _M0L6_2atmpS2625 / 10ull;
        _M0L6_2atmpS2624 = _M0Lm2vrS1005;
        _M0L7vrDiv10S1042 = _M0L6_2atmpS2624 / 10ull;
        _M0L6_2atmpS2623 = _M0Lm2vrS1005;
        _M0L6_2atmpS2620 = (int32_t)_M0L6_2atmpS2623;
        _M0L6_2atmpS2622 = (int32_t)_M0L7vrDiv10S1042;
        _M0L6_2atmpS2621 = 10 * _M0L6_2atmpS2622;
        _M0L7vrMod10S1043 = _M0L6_2atmpS2620 - _M0L6_2atmpS2621;
        if (_M0Lm17vrIsTrailingZerosS1010) {
          int32_t _M0L6_2atmpS2618 = _M0Lm16lastRemovedDigitS1030;
          _M0Lm17vrIsTrailingZerosS1010 = _M0L6_2atmpS2618 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1010 = 0;
        }
        _M0Lm16lastRemovedDigitS1030 = _M0L7vrMod10S1043;
        _M0Lm2vrS1005 = _M0L7vrDiv10S1042;
        _M0Lm2vpS1006 = _M0L7vpDiv10S1041;
        _M0Lm2vmS1007 = _M0L7vmDiv10S1038;
        _M0L6_2atmpS2619 = _M0Lm7removedS1029;
        _M0Lm7removedS1029 = _M0L6_2atmpS2619 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1010) {
      int32_t _M0L6_2atmpS2633 = _M0Lm16lastRemovedDigitS1030;
      if (_M0L6_2atmpS2633 == 5) {
        uint64_t _M0L6_2atmpS2632 = _M0Lm2vrS1005;
        uint64_t _M0L6_2atmpS2631 = _M0L6_2atmpS2632 % 2ull;
        _if__result_3956 = _M0L6_2atmpS2631 == 0ull;
      } else {
        _if__result_3956 = 0;
      }
    } else {
      _if__result_3956 = 0;
    }
    if (_if__result_3956) {
      _M0Lm16lastRemovedDigitS1030 = 4;
    }
    _M0L6_2atmpS2634 = _M0Lm2vrS1005;
    _M0L6_2atmpS2640 = _M0Lm2vrS1005;
    _M0L6_2atmpS2641 = _M0Lm2vmS1007;
    if (_M0L6_2atmpS2640 == _M0L6_2atmpS2641) {
      if (!_M0L4evenS1002) {
        _if__result_3957 = 1;
      } else {
        int32_t _M0L6_2atmpS2639 = _M0Lm17vmIsTrailingZerosS1009;
        _if__result_3957 = !_M0L6_2atmpS2639;
      }
    } else {
      _if__result_3957 = 0;
    }
    if (_if__result_3957) {
      _M0L6_2atmpS2637 = 1;
    } else {
      int32_t _M0L6_2atmpS2638 = _M0Lm16lastRemovedDigitS1030;
      _M0L6_2atmpS2637 = _M0L6_2atmpS2638 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2636 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2637);
    _M0L6_2atmpS2635 = *(uint64_t*)&_M0L6_2atmpS2636;
    _M0Lm6outputS1031 = _M0L6_2atmpS2634 + _M0L6_2atmpS2635;
  } else {
    int32_t _M0Lm7roundUpS1044 = 0;
    uint64_t _M0L6_2atmpS2662 = _M0Lm2vpS1006;
    uint64_t _M0L8vpDiv100S1045 = _M0L6_2atmpS2662 / 100ull;
    uint64_t _M0L6_2atmpS2661 = _M0Lm2vmS1007;
    uint64_t _M0L8vmDiv100S1046 = _M0L6_2atmpS2661 / 100ull;
    uint64_t _M0L6_2atmpS2656;
    uint64_t _M0L6_2atmpS2659;
    uint64_t _M0L6_2atmpS2660;
    int32_t _M0L6_2atmpS2658;
    uint64_t _M0L6_2atmpS2657;
    if (_M0L8vpDiv100S1045 > _M0L8vmDiv100S1046) {
      uint64_t _M0L6_2atmpS2647 = _M0Lm2vrS1005;
      uint64_t _M0L8vrDiv100S1047 = _M0L6_2atmpS2647 / 100ull;
      uint64_t _M0L6_2atmpS2646 = _M0Lm2vrS1005;
      int32_t _M0L6_2atmpS2643 = (int32_t)_M0L6_2atmpS2646;
      int32_t _M0L6_2atmpS2645 = (int32_t)_M0L8vrDiv100S1047;
      int32_t _M0L6_2atmpS2644 = 100 * _M0L6_2atmpS2645;
      int32_t _M0L8vrMod100S1048 = _M0L6_2atmpS2643 - _M0L6_2atmpS2644;
      int32_t _M0L6_2atmpS2642;
      _M0Lm7roundUpS1044 = _M0L8vrMod100S1048 >= 50;
      _M0Lm2vrS1005 = _M0L8vrDiv100S1047;
      _M0Lm2vpS1006 = _M0L8vpDiv100S1045;
      _M0Lm2vmS1007 = _M0L8vmDiv100S1046;
      _M0L6_2atmpS2642 = _M0Lm7removedS1029;
      _M0Lm7removedS1029 = _M0L6_2atmpS2642 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2655 = _M0Lm2vpS1006;
      uint64_t _M0L7vpDiv10S1049 = _M0L6_2atmpS2655 / 10ull;
      uint64_t _M0L6_2atmpS2654 = _M0Lm2vmS1007;
      uint64_t _M0L7vmDiv10S1050 = _M0L6_2atmpS2654 / 10ull;
      uint64_t _M0L6_2atmpS2653;
      uint64_t _M0L7vrDiv10S1052;
      uint64_t _M0L6_2atmpS2652;
      int32_t _M0L6_2atmpS2649;
      int32_t _M0L6_2atmpS2651;
      int32_t _M0L6_2atmpS2650;
      int32_t _M0L7vrMod10S1053;
      int32_t _M0L6_2atmpS2648;
      if (_M0L7vpDiv10S1049 <= _M0L7vmDiv10S1050) {
        break;
      }
      _M0L6_2atmpS2653 = _M0Lm2vrS1005;
      _M0L7vrDiv10S1052 = _M0L6_2atmpS2653 / 10ull;
      _M0L6_2atmpS2652 = _M0Lm2vrS1005;
      _M0L6_2atmpS2649 = (int32_t)_M0L6_2atmpS2652;
      _M0L6_2atmpS2651 = (int32_t)_M0L7vrDiv10S1052;
      _M0L6_2atmpS2650 = 10 * _M0L6_2atmpS2651;
      _M0L7vrMod10S1053 = _M0L6_2atmpS2649 - _M0L6_2atmpS2650;
      _M0Lm7roundUpS1044 = _M0L7vrMod10S1053 >= 5;
      _M0Lm2vrS1005 = _M0L7vrDiv10S1052;
      _M0Lm2vpS1006 = _M0L7vpDiv10S1049;
      _M0Lm2vmS1007 = _M0L7vmDiv10S1050;
      _M0L6_2atmpS2648 = _M0Lm7removedS1029;
      _M0Lm7removedS1029 = _M0L6_2atmpS2648 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2656 = _M0Lm2vrS1005;
    _M0L6_2atmpS2659 = _M0Lm2vrS1005;
    _M0L6_2atmpS2660 = _M0Lm2vmS1007;
    _M0L6_2atmpS2658
    = _M0L6_2atmpS2659 == _M0L6_2atmpS2660 || _M0Lm7roundUpS1044;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2657 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2658);
    _M0Lm6outputS1031 = _M0L6_2atmpS2656 + _M0L6_2atmpS2657;
  }
  _M0L6_2atmpS2664 = _M0Lm3e10S1008;
  _M0L6_2atmpS2665 = _M0Lm7removedS1029;
  _M0L3expS1054 = _M0L6_2atmpS2664 + _M0L6_2atmpS2665;
  _M0L6_2atmpS2663 = _M0Lm6outputS1031;
  _block_3959
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3959)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3959->$0 = _M0L6_2atmpS2663;
  _block_3959->$1 = _M0L3expS1054;
  return _block_3959;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS997) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS997) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS996) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS996) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS995) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS995) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS994) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS994 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS994 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS994 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS994 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS994 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS994 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS994 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS994 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS994 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS994 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS994 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS994 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS994 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS994 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS994 >= 100ull) {
    return 3;
  }
  if (_M0L1vS994 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS977) {
  int32_t _M0L6_2atmpS2564;
  int32_t _M0L6_2atmpS2563;
  int32_t _M0L4baseS976;
  int32_t _M0L5base2S978;
  int32_t _M0L6offsetS979;
  int32_t _M0L6_2atmpS2562;
  uint64_t _M0L4mul0S980;
  int32_t _M0L6_2atmpS2561;
  int32_t _M0L6_2atmpS2560;
  uint64_t _M0L4mul1S981;
  uint64_t _M0L1mS982;
  struct _M0TPB7Umul128 _M0L7_2abindS983;
  uint64_t _M0L7_2alow1S984;
  uint64_t _M0L8_2ahigh1S985;
  struct _M0TPB7Umul128 _M0L7_2abindS986;
  uint64_t _M0L7_2alow0S987;
  uint64_t _M0L8_2ahigh0S988;
  uint64_t _M0L3sumS989;
  uint64_t _M0Lm5high1S990;
  int32_t _M0L6_2atmpS2558;
  int32_t _M0L6_2atmpS2559;
  int32_t _M0L5deltaS991;
  uint64_t _M0L6_2atmpS2557;
  uint64_t _M0L6_2atmpS2549;
  int32_t _M0L6_2atmpS2556;
  uint32_t _M0L6_2atmpS2553;
  int32_t _M0L6_2atmpS2555;
  int32_t _M0L6_2atmpS2554;
  uint32_t _M0L6_2atmpS2552;
  uint32_t _M0L6_2atmpS2551;
  uint64_t _M0L6_2atmpS2550;
  uint64_t _M0L1aS992;
  uint64_t _M0L6_2atmpS2548;
  uint64_t _M0L1bS993;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2564 = _M0L1iS977 + 26;
  _M0L6_2atmpS2563 = _M0L6_2atmpS2564 - 1;
  _M0L4baseS976 = _M0L6_2atmpS2563 / 26;
  _M0L5base2S978 = _M0L4baseS976 * 26;
  _M0L6offsetS979 = _M0L5base2S978 - _M0L1iS977;
  _M0L6_2atmpS2562 = _M0L4baseS976 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S980
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2562);
  _M0L6_2atmpS2561 = _M0L4baseS976 * 2;
  _M0L6_2atmpS2560 = _M0L6_2atmpS2561 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S981
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2560);
  if (_M0L6offsetS979 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S980, _M0L4mul1S981};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS982
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS979);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS983 = _M0FPB7umul128(_M0L1mS982, _M0L4mul1S981);
  _M0L7_2alow1S984 = _M0L7_2abindS983.$0;
  _M0L8_2ahigh1S985 = _M0L7_2abindS983.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS986 = _M0FPB7umul128(_M0L1mS982, _M0L4mul0S980);
  _M0L7_2alow0S987 = _M0L7_2abindS986.$0;
  _M0L8_2ahigh0S988 = _M0L7_2abindS986.$1;
  _M0L3sumS989 = _M0L8_2ahigh0S988 + _M0L7_2alow1S984;
  _M0Lm5high1S990 = _M0L8_2ahigh1S985;
  if (_M0L3sumS989 < _M0L8_2ahigh0S988) {
    uint64_t _M0L6_2atmpS2547 = _M0Lm5high1S990;
    _M0Lm5high1S990 = _M0L6_2atmpS2547 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2558 = _M0FPB8pow5bits(_M0L5base2S978);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2559 = _M0FPB8pow5bits(_M0L1iS977);
  _M0L5deltaS991 = _M0L6_2atmpS2558 - _M0L6_2atmpS2559;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2557
  = _M0FPB13shiftright128(_M0L7_2alow0S987, _M0L3sumS989, _M0L5deltaS991);
  _M0L6_2atmpS2549 = _M0L6_2atmpS2557 + 1ull;
  _M0L6_2atmpS2556 = _M0L1iS977 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2553
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2556);
  _M0L6_2atmpS2555 = _M0L1iS977 % 16;
  _M0L6_2atmpS2554 = _M0L6_2atmpS2555 << 1;
  _M0L6_2atmpS2552 = _M0L6_2atmpS2553 >> (_M0L6_2atmpS2554 & 31);
  _M0L6_2atmpS2551 = _M0L6_2atmpS2552 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2550 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2551);
  _M0L1aS992 = _M0L6_2atmpS2549 + _M0L6_2atmpS2550;
  _M0L6_2atmpS2548 = _M0Lm5high1S990;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS993
  = _M0FPB13shiftright128(_M0L3sumS989, _M0L6_2atmpS2548, _M0L5deltaS991);
  return (struct _M0TPB8Pow5Pair){_M0L1aS992, _M0L1bS993};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS959) {
  int32_t _M0L4baseS958;
  int32_t _M0L5base2S960;
  int32_t _M0L6offsetS961;
  int32_t _M0L6_2atmpS2546;
  uint64_t _M0L4mul0S962;
  int32_t _M0L6_2atmpS2545;
  int32_t _M0L6_2atmpS2544;
  uint64_t _M0L4mul1S963;
  uint64_t _M0L1mS964;
  struct _M0TPB7Umul128 _M0L7_2abindS965;
  uint64_t _M0L7_2alow1S966;
  uint64_t _M0L8_2ahigh1S967;
  struct _M0TPB7Umul128 _M0L7_2abindS968;
  uint64_t _M0L7_2alow0S969;
  uint64_t _M0L8_2ahigh0S970;
  uint64_t _M0L3sumS971;
  uint64_t _M0Lm5high1S972;
  int32_t _M0L6_2atmpS2542;
  int32_t _M0L6_2atmpS2543;
  int32_t _M0L5deltaS973;
  uint64_t _M0L6_2atmpS2534;
  int32_t _M0L6_2atmpS2541;
  uint32_t _M0L6_2atmpS2538;
  int32_t _M0L6_2atmpS2540;
  int32_t _M0L6_2atmpS2539;
  uint32_t _M0L6_2atmpS2537;
  uint32_t _M0L6_2atmpS2536;
  uint64_t _M0L6_2atmpS2535;
  uint64_t _M0L1aS974;
  uint64_t _M0L6_2atmpS2533;
  uint64_t _M0L1bS975;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS958 = _M0L1iS959 / 26;
  _M0L5base2S960 = _M0L4baseS958 * 26;
  _M0L6offsetS961 = _M0L1iS959 - _M0L5base2S960;
  _M0L6_2atmpS2546 = _M0L4baseS958 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S962
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2546);
  _M0L6_2atmpS2545 = _M0L4baseS958 * 2;
  _M0L6_2atmpS2544 = _M0L6_2atmpS2545 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S963
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2544);
  if (_M0L6offsetS961 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S962, _M0L4mul1S963};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS964
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS961);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS965 = _M0FPB7umul128(_M0L1mS964, _M0L4mul1S963);
  _M0L7_2alow1S966 = _M0L7_2abindS965.$0;
  _M0L8_2ahigh1S967 = _M0L7_2abindS965.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS968 = _M0FPB7umul128(_M0L1mS964, _M0L4mul0S962);
  _M0L7_2alow0S969 = _M0L7_2abindS968.$0;
  _M0L8_2ahigh0S970 = _M0L7_2abindS968.$1;
  _M0L3sumS971 = _M0L8_2ahigh0S970 + _M0L7_2alow1S966;
  _M0Lm5high1S972 = _M0L8_2ahigh1S967;
  if (_M0L3sumS971 < _M0L8_2ahigh0S970) {
    uint64_t _M0L6_2atmpS2532 = _M0Lm5high1S972;
    _M0Lm5high1S972 = _M0L6_2atmpS2532 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2542 = _M0FPB8pow5bits(_M0L1iS959);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2543 = _M0FPB8pow5bits(_M0L5base2S960);
  _M0L5deltaS973 = _M0L6_2atmpS2542 - _M0L6_2atmpS2543;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2534
  = _M0FPB13shiftright128(_M0L7_2alow0S969, _M0L3sumS971, _M0L5deltaS973);
  _M0L6_2atmpS2541 = _M0L1iS959 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2538
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2541);
  _M0L6_2atmpS2540 = _M0L1iS959 % 16;
  _M0L6_2atmpS2539 = _M0L6_2atmpS2540 << 1;
  _M0L6_2atmpS2537 = _M0L6_2atmpS2538 >> (_M0L6_2atmpS2539 & 31);
  _M0L6_2atmpS2536 = _M0L6_2atmpS2537 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2535 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2536);
  _M0L1aS974 = _M0L6_2atmpS2534 + _M0L6_2atmpS2535;
  _M0L6_2atmpS2533 = _M0Lm5high1S972;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS975
  = _M0FPB13shiftright128(_M0L3sumS971, _M0L6_2atmpS2533, _M0L5deltaS973);
  return (struct _M0TPB8Pow5Pair){_M0L1aS974, _M0L1bS975};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS932,
  struct _M0TPB8Pow5Pair _M0L3mulS929,
  int32_t _M0L1jS945,
  int32_t _M0L7mmShiftS947
) {
  uint64_t _M0L7_2amul0S928;
  uint64_t _M0L7_2amul1S930;
  uint64_t _M0L1mS931;
  struct _M0TPB7Umul128 _M0L7_2abindS933;
  uint64_t _M0L5_2aloS934;
  uint64_t _M0L6_2atmpS935;
  struct _M0TPB7Umul128 _M0L7_2abindS936;
  uint64_t _M0L6_2alo2S937;
  uint64_t _M0L6_2ahi2S938;
  uint64_t _M0L3midS939;
  uint64_t _M0L6_2atmpS2531;
  uint64_t _M0L2hiS940;
  uint64_t _M0L3lo2S941;
  uint64_t _M0L6_2atmpS2529;
  uint64_t _M0L6_2atmpS2530;
  uint64_t _M0L4mid2S942;
  uint64_t _M0L6_2atmpS2528;
  uint64_t _M0L3hi2S943;
  int32_t _M0L6_2atmpS2527;
  int32_t _M0L6_2atmpS2526;
  uint64_t _M0L2vpS944;
  uint64_t _M0Lm2vmS946;
  int32_t _M0L6_2atmpS2525;
  int32_t _M0L6_2atmpS2524;
  uint64_t _M0L2vrS957;
  uint64_t _M0L6_2atmpS2523;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S928 = _M0L3mulS929.$0;
  _M0L7_2amul1S930 = _M0L3mulS929.$1;
  _M0L1mS931 = _M0L1mS932 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS933 = _M0FPB7umul128(_M0L1mS931, _M0L7_2amul0S928);
  _M0L5_2aloS934 = _M0L7_2abindS933.$0;
  _M0L6_2atmpS935 = _M0L7_2abindS933.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS936 = _M0FPB7umul128(_M0L1mS931, _M0L7_2amul1S930);
  _M0L6_2alo2S937 = _M0L7_2abindS936.$0;
  _M0L6_2ahi2S938 = _M0L7_2abindS936.$1;
  _M0L3midS939 = _M0L6_2atmpS935 + _M0L6_2alo2S937;
  if (_M0L3midS939 < _M0L6_2atmpS935) {
    _M0L6_2atmpS2531 = 1ull;
  } else {
    _M0L6_2atmpS2531 = 0ull;
  }
  _M0L2hiS940 = _M0L6_2ahi2S938 + _M0L6_2atmpS2531;
  _M0L3lo2S941 = _M0L5_2aloS934 + _M0L7_2amul0S928;
  _M0L6_2atmpS2529 = _M0L3midS939 + _M0L7_2amul1S930;
  if (_M0L3lo2S941 < _M0L5_2aloS934) {
    _M0L6_2atmpS2530 = 1ull;
  } else {
    _M0L6_2atmpS2530 = 0ull;
  }
  _M0L4mid2S942 = _M0L6_2atmpS2529 + _M0L6_2atmpS2530;
  if (_M0L4mid2S942 < _M0L3midS939) {
    _M0L6_2atmpS2528 = 1ull;
  } else {
    _M0L6_2atmpS2528 = 0ull;
  }
  _M0L3hi2S943 = _M0L2hiS940 + _M0L6_2atmpS2528;
  _M0L6_2atmpS2527 = _M0L1jS945 - 64;
  _M0L6_2atmpS2526 = _M0L6_2atmpS2527 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS944
  = _M0FPB13shiftright128(_M0L4mid2S942, _M0L3hi2S943, _M0L6_2atmpS2526);
  _M0Lm2vmS946 = 0ull;
  if (_M0L7mmShiftS947) {
    uint64_t _M0L3lo3S948 = _M0L5_2aloS934 - _M0L7_2amul0S928;
    uint64_t _M0L6_2atmpS2513 = _M0L3midS939 - _M0L7_2amul1S930;
    uint64_t _M0L6_2atmpS2514;
    uint64_t _M0L4mid3S949;
    uint64_t _M0L6_2atmpS2512;
    uint64_t _M0L3hi3S950;
    int32_t _M0L6_2atmpS2511;
    int32_t _M0L6_2atmpS2510;
    if (_M0L5_2aloS934 < _M0L3lo3S948) {
      _M0L6_2atmpS2514 = 1ull;
    } else {
      _M0L6_2atmpS2514 = 0ull;
    }
    _M0L4mid3S949 = _M0L6_2atmpS2513 - _M0L6_2atmpS2514;
    if (_M0L3midS939 < _M0L4mid3S949) {
      _M0L6_2atmpS2512 = 1ull;
    } else {
      _M0L6_2atmpS2512 = 0ull;
    }
    _M0L3hi3S950 = _M0L2hiS940 - _M0L6_2atmpS2512;
    _M0L6_2atmpS2511 = _M0L1jS945 - 64;
    _M0L6_2atmpS2510 = _M0L6_2atmpS2511 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS946
    = _M0FPB13shiftright128(_M0L4mid3S949, _M0L3hi3S950, _M0L6_2atmpS2510);
  } else {
    uint64_t _M0L3lo3S951 = _M0L5_2aloS934 + _M0L5_2aloS934;
    uint64_t _M0L6_2atmpS2521 = _M0L3midS939 + _M0L3midS939;
    uint64_t _M0L6_2atmpS2522;
    uint64_t _M0L4mid3S952;
    uint64_t _M0L6_2atmpS2519;
    uint64_t _M0L6_2atmpS2520;
    uint64_t _M0L3hi3S953;
    uint64_t _M0L3lo4S954;
    uint64_t _M0L6_2atmpS2517;
    uint64_t _M0L6_2atmpS2518;
    uint64_t _M0L4mid4S955;
    uint64_t _M0L6_2atmpS2516;
    uint64_t _M0L3hi4S956;
    int32_t _M0L6_2atmpS2515;
    if (_M0L3lo3S951 < _M0L5_2aloS934) {
      _M0L6_2atmpS2522 = 1ull;
    } else {
      _M0L6_2atmpS2522 = 0ull;
    }
    _M0L4mid3S952 = _M0L6_2atmpS2521 + _M0L6_2atmpS2522;
    _M0L6_2atmpS2519 = _M0L2hiS940 + _M0L2hiS940;
    if (_M0L4mid3S952 < _M0L3midS939) {
      _M0L6_2atmpS2520 = 1ull;
    } else {
      _M0L6_2atmpS2520 = 0ull;
    }
    _M0L3hi3S953 = _M0L6_2atmpS2519 + _M0L6_2atmpS2520;
    _M0L3lo4S954 = _M0L3lo3S951 - _M0L7_2amul0S928;
    _M0L6_2atmpS2517 = _M0L4mid3S952 - _M0L7_2amul1S930;
    if (_M0L3lo3S951 < _M0L3lo4S954) {
      _M0L6_2atmpS2518 = 1ull;
    } else {
      _M0L6_2atmpS2518 = 0ull;
    }
    _M0L4mid4S955 = _M0L6_2atmpS2517 - _M0L6_2atmpS2518;
    if (_M0L4mid3S952 < _M0L4mid4S955) {
      _M0L6_2atmpS2516 = 1ull;
    } else {
      _M0L6_2atmpS2516 = 0ull;
    }
    _M0L3hi4S956 = _M0L3hi3S953 - _M0L6_2atmpS2516;
    _M0L6_2atmpS2515 = _M0L1jS945 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS946
    = _M0FPB13shiftright128(_M0L4mid4S955, _M0L3hi4S956, _M0L6_2atmpS2515);
  }
  _M0L6_2atmpS2525 = _M0L1jS945 - 64;
  _M0L6_2atmpS2524 = _M0L6_2atmpS2525 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS957
  = _M0FPB13shiftright128(_M0L3midS939, _M0L2hiS940, _M0L6_2atmpS2524);
  _M0L6_2atmpS2523 = _M0Lm2vmS946;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS957,
                                                _M0L2vpS944,
                                                _M0L6_2atmpS2523};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS926,
  int32_t _M0L1pS927
) {
  uint64_t _M0L6_2atmpS2509;
  uint64_t _M0L6_2atmpS2508;
  uint64_t _M0L6_2atmpS2507;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2509 = 1ull << (_M0L1pS927 & 63);
  _M0L6_2atmpS2508 = _M0L6_2atmpS2509 - 1ull;
  _M0L6_2atmpS2507 = _M0L5valueS926 & _M0L6_2atmpS2508;
  return _M0L6_2atmpS2507 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS924,
  int32_t _M0L1pS925
) {
  int32_t _M0L6_2atmpS2506;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2506 = _M0FPB10pow5Factor(_M0L5valueS924);
  return _M0L6_2atmpS2506 >= _M0L1pS925;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS920) {
  uint64_t _M0L6_2atmpS2494;
  uint64_t _M0L6_2atmpS2495;
  uint64_t _M0L6_2atmpS2496;
  uint64_t _M0L6_2atmpS2497;
  int32_t _M0Lm5countS921;
  uint64_t _M0Lm5valueS922;
  uint64_t _M0L6_2atmpS2505;
  moonbit_string_t _M0L6_2atmpS2504;
  moonbit_string_t _M0L6_2atmpS3539;
  moonbit_string_t _M0L6_2atmpS2503;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2494 = _M0L5valueS920 % 5ull;
  if (_M0L6_2atmpS2494 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2495 = _M0L5valueS920 % 25ull;
  if (_M0L6_2atmpS2495 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2496 = _M0L5valueS920 % 125ull;
  if (_M0L6_2atmpS2496 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2497 = _M0L5valueS920 % 625ull;
  if (_M0L6_2atmpS2497 != 0ull) {
    return 3;
  }
  _M0Lm5countS921 = 4;
  _M0Lm5valueS922 = _M0L5valueS920 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2498 = _M0Lm5valueS922;
    if (_M0L6_2atmpS2498 > 0ull) {
      uint64_t _M0L6_2atmpS2500 = _M0Lm5valueS922;
      uint64_t _M0L6_2atmpS2499 = _M0L6_2atmpS2500 % 5ull;
      uint64_t _M0L6_2atmpS2501;
      int32_t _M0L6_2atmpS2502;
      if (_M0L6_2atmpS2499 != 0ull) {
        return _M0Lm5countS921;
      }
      _M0L6_2atmpS2501 = _M0Lm5valueS922;
      _M0Lm5valueS922 = _M0L6_2atmpS2501 / 5ull;
      _M0L6_2atmpS2502 = _M0Lm5countS921;
      _M0Lm5countS921 = _M0L6_2atmpS2502 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2505 = _M0Lm5valueS922;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2504
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2505);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3539
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS2504);
  moonbit_decref(_M0L6_2atmpS2504);
  _M0L6_2atmpS2503 = _M0L6_2atmpS3539;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2503, (moonbit_string_t)moonbit_string_literal_67.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS919,
  uint64_t _M0L2hiS917,
  int32_t _M0L4distS918
) {
  int32_t _M0L6_2atmpS2493;
  uint64_t _M0L6_2atmpS2491;
  uint64_t _M0L6_2atmpS2492;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2493 = 64 - _M0L4distS918;
  _M0L6_2atmpS2491 = _M0L2hiS917 << (_M0L6_2atmpS2493 & 63);
  _M0L6_2atmpS2492 = _M0L2loS919 >> (_M0L4distS918 & 63);
  return _M0L6_2atmpS2491 | _M0L6_2atmpS2492;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS907,
  uint64_t _M0L1bS910
) {
  uint64_t _M0L3aLoS906;
  uint64_t _M0L3aHiS908;
  uint64_t _M0L3bLoS909;
  uint64_t _M0L3bHiS911;
  uint64_t _M0L1xS912;
  uint64_t _M0L6_2atmpS2489;
  uint64_t _M0L6_2atmpS2490;
  uint64_t _M0L1yS913;
  uint64_t _M0L6_2atmpS2487;
  uint64_t _M0L6_2atmpS2488;
  uint64_t _M0L1zS914;
  uint64_t _M0L6_2atmpS2485;
  uint64_t _M0L6_2atmpS2486;
  uint64_t _M0L6_2atmpS2483;
  uint64_t _M0L6_2atmpS2484;
  uint64_t _M0L1wS915;
  uint64_t _M0L2loS916;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS906 = _M0L1aS907 & 4294967295ull;
  _M0L3aHiS908 = _M0L1aS907 >> 32;
  _M0L3bLoS909 = _M0L1bS910 & 4294967295ull;
  _M0L3bHiS911 = _M0L1bS910 >> 32;
  _M0L1xS912 = _M0L3aLoS906 * _M0L3bLoS909;
  _M0L6_2atmpS2489 = _M0L3aHiS908 * _M0L3bLoS909;
  _M0L6_2atmpS2490 = _M0L1xS912 >> 32;
  _M0L1yS913 = _M0L6_2atmpS2489 + _M0L6_2atmpS2490;
  _M0L6_2atmpS2487 = _M0L3aLoS906 * _M0L3bHiS911;
  _M0L6_2atmpS2488 = _M0L1yS913 & 4294967295ull;
  _M0L1zS914 = _M0L6_2atmpS2487 + _M0L6_2atmpS2488;
  _M0L6_2atmpS2485 = _M0L3aHiS908 * _M0L3bHiS911;
  _M0L6_2atmpS2486 = _M0L1yS913 >> 32;
  _M0L6_2atmpS2483 = _M0L6_2atmpS2485 + _M0L6_2atmpS2486;
  _M0L6_2atmpS2484 = _M0L1zS914 >> 32;
  _M0L1wS915 = _M0L6_2atmpS2483 + _M0L6_2atmpS2484;
  _M0L2loS916 = _M0L1aS907 * _M0L1bS910;
  return (struct _M0TPB7Umul128){_M0L2loS916, _M0L1wS915};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS901,
  int32_t _M0L4fromS905,
  int32_t _M0L2toS903
) {
  int32_t _M0L6_2atmpS2482;
  struct _M0TPB13StringBuilder* _M0L3bufS900;
  int32_t _M0L1iS902;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2482 = Moonbit_array_length(_M0L5bytesS901);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS900 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2482);
  _M0L1iS902 = _M0L4fromS905;
  while (1) {
    if (_M0L1iS902 < _M0L2toS903) {
      int32_t _M0L6_2atmpS2480;
      int32_t _M0L6_2atmpS2479;
      int32_t _M0L6_2atmpS2481;
      if (
        _M0L1iS902 < 0 || _M0L1iS902 >= Moonbit_array_length(_M0L5bytesS901)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2480 = (int32_t)_M0L5bytesS901[_M0L1iS902];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2479 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2480);
      moonbit_incref(_M0L3bufS900);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS900, _M0L6_2atmpS2479);
      _M0L6_2atmpS2481 = _M0L1iS902 + 1;
      _M0L1iS902 = _M0L6_2atmpS2481;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS901);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS900);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS899) {
  int32_t _M0L6_2atmpS2478;
  uint32_t _M0L6_2atmpS2477;
  uint32_t _M0L6_2atmpS2476;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2478 = _M0L1eS899 * 78913;
  _M0L6_2atmpS2477 = *(uint32_t*)&_M0L6_2atmpS2478;
  _M0L6_2atmpS2476 = _M0L6_2atmpS2477 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2476;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS898) {
  int32_t _M0L6_2atmpS2475;
  uint32_t _M0L6_2atmpS2474;
  uint32_t _M0L6_2atmpS2473;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2475 = _M0L1eS898 * 732923;
  _M0L6_2atmpS2474 = *(uint32_t*)&_M0L6_2atmpS2475;
  _M0L6_2atmpS2473 = _M0L6_2atmpS2474 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2473;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS896,
  int32_t _M0L8exponentS897,
  int32_t _M0L8mantissaS894
) {
  moonbit_string_t _M0L1sS895;
  moonbit_string_t _M0L6_2atmpS3540;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS894) {
    return (moonbit_string_t)moonbit_string_literal_68.data;
  }
  if (_M0L4signS896) {
    _M0L1sS895 = (moonbit_string_t)moonbit_string_literal_69.data;
  } else {
    _M0L1sS895 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS897) {
    moonbit_string_t _M0L6_2atmpS3541;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3541
    = moonbit_add_string(_M0L1sS895, (moonbit_string_t)moonbit_string_literal_70.data);
    moonbit_decref(_M0L1sS895);
    return _M0L6_2atmpS3541;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3540
  = moonbit_add_string(_M0L1sS895, (moonbit_string_t)moonbit_string_literal_71.data);
  moonbit_decref(_M0L1sS895);
  return _M0L6_2atmpS3540;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS893) {
  int32_t _M0L6_2atmpS2472;
  uint32_t _M0L6_2atmpS2471;
  uint32_t _M0L6_2atmpS2470;
  int32_t _M0L6_2atmpS2469;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2472 = _M0L1eS893 * 1217359;
  _M0L6_2atmpS2471 = *(uint32_t*)&_M0L6_2atmpS2472;
  _M0L6_2atmpS2470 = _M0L6_2atmpS2471 >> 19;
  _M0L6_2atmpS2469 = *(int32_t*)&_M0L6_2atmpS2470;
  return _M0L6_2atmpS2469 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS892,
  struct _M0TPB6Hasher* _M0L6hasherS891
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS891, _M0L4selfS892);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS890,
  struct _M0TPB6Hasher* _M0L6hasherS889
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS889, _M0L4selfS890);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS887,
  moonbit_string_t _M0L5valueS885
) {
  int32_t _M0L7_2abindS884;
  int32_t _M0L1iS886;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS884 = Moonbit_array_length(_M0L5valueS885);
  _M0L1iS886 = 0;
  while (1) {
    if (_M0L1iS886 < _M0L7_2abindS884) {
      int32_t _M0L6_2atmpS2467 = _M0L5valueS885[_M0L1iS886];
      int32_t _M0L6_2atmpS2466 = (int32_t)_M0L6_2atmpS2467;
      uint32_t _M0L6_2atmpS2465 = *(uint32_t*)&_M0L6_2atmpS2466;
      int32_t _M0L6_2atmpS2468;
      moonbit_incref(_M0L4selfS887);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS887, _M0L6_2atmpS2465);
      _M0L6_2atmpS2468 = _M0L1iS886 + 1;
      _M0L1iS886 = _M0L6_2atmpS2468;
      continue;
    } else {
      moonbit_decref(_M0L4selfS887);
      moonbit_decref(_M0L5valueS885);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS882,
  int32_t _M0L3idxS883
) {
  int32_t _M0L6_2atmpS3542;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3542 = _M0L4selfS882[_M0L3idxS883];
  moonbit_decref(_M0L4selfS882);
  return _M0L6_2atmpS3542;
}

struct _M0TUsRPC16string10StringViewE* _M0MPB5Iter24nextGsRPC16string10StringViewE(
  struct _M0TWEOUsRPC16string10StringViewE* _M0L4selfS881
) {
  #line 904 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  #line 905 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0MPB4Iter4nextGUsRPC16string10StringViewEE(_M0L4selfS881);
}

void* _M0IPB3MapPB6ToJson8to__jsonGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS873
) {
  int32_t _M0L8capacityS2464;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS872;
  struct _M0TWEOUsRPC16string10StringViewE* _M0L5_2aitS874;
  void* _block_3964;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L8capacityS2464 = _M0L4selfS873->$2;
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6objectS872 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L8capacityS2464);
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L5_2aitS874 = _M0MPB3Map5iter2GsRPC16string10StringViewE(_M0L4selfS873);
  while (1) {
    struct _M0TUsRPC16string10StringViewE* _M0L7_2abindS875;
    moonbit_incref(_M0L5_2aitS874);
    #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    _M0L7_2abindS875
    = _M0MPB5Iter24nextGsRPC16string10StringViewE(_M0L5_2aitS874);
    if (_M0L7_2abindS875 == 0) {
      if (_M0L7_2abindS875) {
        moonbit_decref(_M0L7_2abindS875);
      }
      moonbit_decref(_M0L5_2aitS874);
    } else {
      struct _M0TUsRPC16string10StringViewE* _M0L7_2aSomeS876 =
        _M0L7_2abindS875;
      struct _M0TUsRPC16string10StringViewE* _M0L4_2axS877 = _M0L7_2aSomeS876;
      moonbit_string_t _M0L8_2afieldS3544 = _M0L4_2axS877->$0;
      moonbit_string_t _M0L4_2akS878 = _M0L8_2afieldS3544;
      struct _M0TPC16string10StringView _M0L8_2afieldS3543 =
        (struct _M0TPC16string10StringView){_M0L4_2axS877->$1_1,
                                              _M0L4_2axS877->$1_2,
                                              _M0L4_2axS877->$1_0};
      int32_t _M0L6_2acntS3818 = Moonbit_object_header(_M0L4_2axS877)->rc;
      struct _M0TPC16string10StringView _M0L4_2avS879;
      moonbit_string_t _M0L6_2atmpS2462;
      void* _M0L6_2atmpS2463;
      if (_M0L6_2acntS3818 > 1) {
        int32_t _M0L11_2anew__cntS3819 = _M0L6_2acntS3818 - 1;
        Moonbit_object_header(_M0L4_2axS877)->rc = _M0L11_2anew__cntS3819;
        moonbit_incref(_M0L8_2afieldS3543.$0);
        moonbit_incref(_M0L4_2akS878);
      } else if (_M0L6_2acntS3818 == 1) {
        #line 280 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        moonbit_free(_M0L4_2axS877);
      }
      _M0L4_2avS879 = _M0L8_2afieldS3543;
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2462
      = _M0IPC16string6StringPB4Show10to__string(_M0L4_2akS878);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2463
      = _M0IPC16string10StringViewPB6ToJson8to__json(_M0L4_2avS879);
      moonbit_incref(_M0L6objectS872);
      #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L6objectS872, _M0L6_2atmpS2462, _M0L6_2atmpS2463);
      continue;
    }
    break;
  }
  _block_3964 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3964)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3964)->$0 = _M0L6objectS872;
  return _block_3964;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS871
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2461;
  void* _block_3965;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IPC16string6StringPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2461
  = _M0MPC15array5Array3mapGsRPB4JsonE(_M0L4selfS871, _M0IPC16string6StringPB6ToJson14to__json_2eclo);
  _block_3965 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_3965)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_3965)->$0 = _M0L6_2atmpS2461;
  return _block_3965;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGsRPB4JsonE(
  struct _M0TPB5ArrayGsE* _M0L4selfS865,
  struct _M0TWsERPB4Json* _M0L1fS869
) {
  int32_t _M0L3lenS2460;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS864;
  int32_t _M0L7_2abindS866;
  int32_t _M0L1iS867;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2460 = _M0L4selfS865->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS864 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2460);
  _M0L7_2abindS866 = _M0L4selfS865->$1;
  _M0L1iS867 = 0;
  while (1) {
    if (_M0L1iS867 < _M0L7_2abindS866) {
      moonbit_string_t* _M0L8_2afieldS3548 = _M0L4selfS865->$0;
      moonbit_string_t* _M0L3bufS2459 = _M0L8_2afieldS3548;
      moonbit_string_t _M0L6_2atmpS3547 =
        (moonbit_string_t)_M0L3bufS2459[_M0L1iS867];
      moonbit_string_t _M0L1vS868 = _M0L6_2atmpS3547;
      void** _M0L8_2afieldS3546 = _M0L3arrS864->$0;
      void** _M0L3bufS2456 = _M0L8_2afieldS3546;
      void* _M0L6_2atmpS2457;
      void* _M0L6_2aoldS3545;
      int32_t _M0L6_2atmpS2458;
      moonbit_incref(_M0L3bufS2456);
      moonbit_incref(_M0L1fS869);
      moonbit_incref(_M0L1vS868);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2457 = _M0L1fS869->code(_M0L1fS869, _M0L1vS868);
      _M0L6_2aoldS3545 = (void*)_M0L3bufS2456[_M0L1iS867];
      moonbit_decref(_M0L6_2aoldS3545);
      _M0L3bufS2456[_M0L1iS867] = _M0L6_2atmpS2457;
      moonbit_decref(_M0L3bufS2456);
      _M0L6_2atmpS2458 = _M0L1iS867 + 1;
      _M0L1iS867 = _M0L6_2atmpS2458;
      continue;
    } else {
      moonbit_decref(_M0L1fS869);
      moonbit_decref(_M0L4selfS865);
    }
    break;
  }
  return _M0L3arrS864;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS863) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS863;
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS862) {
  void* _block_3967;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3967 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3967)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3967)->$0 = _M0L6objectS862;
  return _block_3967;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS861) {
  void* _block_3968;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3968 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3968)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3968)->$0 = _M0L6stringS861;
  return _block_3968;
}

struct _M0TWEOUsRPC16string10StringViewE* _M0MPB3Map5iter2GsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS860
) {
  #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 606 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRPC16string10StringViewE(_M0L4selfS860);
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS845
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3549;
  int32_t _M0L6_2acntS3820;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2449;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS844;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__* _closure_3969;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2444;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3549 = _M0L4selfS845->$5;
  _M0L6_2acntS3820 = Moonbit_object_header(_M0L4selfS845)->rc;
  if (_M0L6_2acntS3820 > 1) {
    int32_t _M0L11_2anew__cntS3822 = _M0L6_2acntS3820 - 1;
    Moonbit_object_header(_M0L4selfS845)->rc = _M0L11_2anew__cntS3822;
    if (_M0L8_2afieldS3549) {
      moonbit_incref(_M0L8_2afieldS3549);
    }
  } else if (_M0L6_2acntS3820 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3821 = _M0L4selfS845->$0;
    moonbit_decref(_M0L8_2afieldS3821);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS845);
  }
  _M0L4headS2449 = _M0L8_2afieldS3549;
  _M0L11curr__entryS844
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS844)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS844->$0 = _M0L4headS2449;
  _closure_3969
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__));
  Moonbit_object_header(_closure_3969)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__, $0) >> 2, 1, 0);
  _closure_3969->code = &_M0MPB3Map4iterGsRPB4JsonEC2445l591;
  _closure_3969->$0 = _M0L11curr__entryS844;
  _M0L6_2atmpS2444 = (struct _M0TWEOUsRPB4JsonE*)_closure_3969;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2444);
}

struct _M0TWEOUsRPC16string10StringViewE* _M0MPB3Map4iterGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS853
) {
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2afieldS3550;
  int32_t _M0L6_2acntS3823;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L4headS2455;
  struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE* _M0L11curr__entryS852;
  struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__* _closure_3970;
  struct _M0TWEOUsRPC16string10StringViewE* _M0L6_2atmpS2450;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3550 = _M0L4selfS853->$5;
  _M0L6_2acntS3823 = Moonbit_object_header(_M0L4selfS853)->rc;
  if (_M0L6_2acntS3823 > 1) {
    int32_t _M0L11_2anew__cntS3825 = _M0L6_2acntS3823 - 1;
    Moonbit_object_header(_M0L4selfS853)->rc = _M0L11_2anew__cntS3825;
    if (_M0L8_2afieldS3550) {
      moonbit_incref(_M0L8_2afieldS3550);
    }
  } else if (_M0L6_2acntS3823 == 1) {
    struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L8_2afieldS3824 =
      _M0L4selfS853->$0;
    moonbit_decref(_M0L8_2afieldS3824);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS853);
  }
  _M0L4headS2455 = _M0L8_2afieldS3550;
  _M0L11curr__entryS852
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE));
  Moonbit_object_header(_M0L11curr__entryS852)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS852->$0 = _M0L4headS2455;
  _closure_3970
  = (struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__*)moonbit_malloc(sizeof(struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__));
  Moonbit_object_header(_closure_3970)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__, $0) >> 2, 1, 0);
  _closure_3970->code = &_M0MPB3Map4iterGsRPC16string10StringViewEC2451l591;
  _closure_3970->$0 = _M0L11curr__entryS852;
  _M0L6_2atmpS2450 = (struct _M0TWEOUsRPC16string10StringViewE*)_closure_3970;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPC16string10StringViewEE(_M0L6_2atmpS2450);
}

struct _M0TUsRPC16string10StringViewE* _M0MPB3Map4iterGsRPC16string10StringViewEC2451l591(
  struct _M0TWEOUsRPC16string10StringViewE* _M0L6_2aenvS2452
) {
  struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__* _M0L14_2acasted__envS2453;
  struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE* _M0L8_2afieldS3556;
  int32_t _M0L6_2acntS3826;
  struct _M0TPC13ref3RefGORPB5EntryGsRPC16string10StringViewEE* _M0L11curr__entryS852;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2afieldS3555;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS854;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2453
  = (struct _M0R99Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2451__l591__*)_M0L6_2aenvS2452;
  _M0L8_2afieldS3556 = _M0L14_2acasted__envS2453->$0;
  _M0L6_2acntS3826 = Moonbit_object_header(_M0L14_2acasted__envS2453)->rc;
  if (_M0L6_2acntS3826 > 1) {
    int32_t _M0L11_2anew__cntS3827 = _M0L6_2acntS3826 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2453)->rc
    = _M0L11_2anew__cntS3827;
    moonbit_incref(_M0L8_2afieldS3556);
  } else if (_M0L6_2acntS3826 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2453);
  }
  _M0L11curr__entryS852 = _M0L8_2afieldS3556;
  _M0L8_2afieldS3555 = _M0L11curr__entryS852->$0;
  _M0L7_2abindS854 = _M0L8_2afieldS3555;
  if (_M0L7_2abindS854 == 0) {
    moonbit_decref(_M0L11curr__entryS852);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2aSomeS855 =
      _M0L7_2abindS854;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L4_2axS856 =
      _M0L7_2aSomeS855;
    moonbit_string_t _M0L8_2afieldS3554 = _M0L4_2axS856->$4;
    moonbit_string_t _M0L6_2akeyS857 = _M0L8_2afieldS3554;
    struct _M0TPC16string10StringView _M0L8_2afieldS3553 =
      (struct _M0TPC16string10StringView){_M0L4_2axS856->$5_1,
                                            _M0L4_2axS856->$5_2,
                                            _M0L4_2axS856->$5_0};
    struct _M0TPC16string10StringView _M0L8_2avalueS858 = _M0L8_2afieldS3553;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2afieldS3552 =
      _M0L4_2axS856->$1;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2anextS859 =
      _M0L8_2afieldS3552;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2aoldS3551 =
      _M0L11curr__entryS852->$0;
    struct _M0TUsRPC16string10StringViewE* _M0L8_2atupleS2454;
    if (_M0L7_2anextS859) {
      moonbit_incref(_M0L7_2anextS859);
    }
    moonbit_incref(_M0L8_2avalueS858.$0);
    moonbit_incref(_M0L6_2akeyS857);
    if (_M0L6_2aoldS3551) {
      moonbit_decref(_M0L6_2aoldS3551);
    }
    _M0L11curr__entryS852->$0 = _M0L7_2anextS859;
    moonbit_decref(_M0L11curr__entryS852);
    _M0L8_2atupleS2454
    = (struct _M0TUsRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TUsRPC16string10StringViewE));
    Moonbit_object_header(_M0L8_2atupleS2454)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPC16string10StringViewE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2454->$0 = _M0L6_2akeyS857;
    _M0L8_2atupleS2454->$1_0 = _M0L8_2avalueS858.$0;
    _M0L8_2atupleS2454->$1_1 = _M0L8_2avalueS858.$1;
    _M0L8_2atupleS2454->$1_2 = _M0L8_2avalueS858.$2;
    return _M0L8_2atupleS2454;
  }
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2445l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2446
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__* _M0L14_2acasted__envS2447;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3562;
  int32_t _M0L6_2acntS3828;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS844;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3561;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS846;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2447
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2445__l591__*)_M0L6_2aenvS2446;
  _M0L8_2afieldS3562 = _M0L14_2acasted__envS2447->$0;
  _M0L6_2acntS3828 = Moonbit_object_header(_M0L14_2acasted__envS2447)->rc;
  if (_M0L6_2acntS3828 > 1) {
    int32_t _M0L11_2anew__cntS3829 = _M0L6_2acntS3828 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2447)->rc
    = _M0L11_2anew__cntS3829;
    moonbit_incref(_M0L8_2afieldS3562);
  } else if (_M0L6_2acntS3828 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2447);
  }
  _M0L11curr__entryS844 = _M0L8_2afieldS3562;
  _M0L8_2afieldS3561 = _M0L11curr__entryS844->$0;
  _M0L7_2abindS846 = _M0L8_2afieldS3561;
  if (_M0L7_2abindS846 == 0) {
    moonbit_decref(_M0L11curr__entryS844);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS847 = _M0L7_2abindS846;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS848 = _M0L7_2aSomeS847;
    moonbit_string_t _M0L8_2afieldS3560 = _M0L4_2axS848->$4;
    moonbit_string_t _M0L6_2akeyS849 = _M0L8_2afieldS3560;
    void* _M0L8_2afieldS3559 = _M0L4_2axS848->$5;
    void* _M0L8_2avalueS850 = _M0L8_2afieldS3559;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3558 = _M0L4_2axS848->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS851 = _M0L8_2afieldS3558;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3557 =
      _M0L11curr__entryS844->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2448;
    if (_M0L7_2anextS851) {
      moonbit_incref(_M0L7_2anextS851);
    }
    moonbit_incref(_M0L8_2avalueS850);
    moonbit_incref(_M0L6_2akeyS849);
    if (_M0L6_2aoldS3557) {
      moonbit_decref(_M0L6_2aoldS3557);
    }
    _M0L11curr__entryS844->$0 = _M0L7_2anextS851;
    moonbit_decref(_M0L11curr__entryS844);
    _M0L8_2atupleS2448
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2448)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2448->$0 = _M0L6_2akeyS849;
    _M0L8_2atupleS2448->$1 = _M0L8_2avalueS850;
    return _M0L8_2atupleS2448;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS843
) {
  int32_t _M0L8_2afieldS3563;
  int32_t _M0L4sizeS2443;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3563 = _M0L4selfS843->$1;
  moonbit_decref(_M0L4selfS843);
  _M0L4sizeS2443 = _M0L8_2afieldS3563;
  return _M0L4sizeS2443 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS830,
  int32_t _M0L3keyS826
) {
  int32_t _M0L4hashS825;
  int32_t _M0L14capacity__maskS2428;
  int32_t _M0L6_2atmpS2427;
  int32_t _M0L1iS827;
  int32_t _M0L3idxS828;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS825 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS826);
  _M0L14capacity__maskS2428 = _M0L4selfS830->$3;
  _M0L6_2atmpS2427 = _M0L4hashS825 & _M0L14capacity__maskS2428;
  _M0L1iS827 = 0;
  _M0L3idxS828 = _M0L6_2atmpS2427;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3567 =
      _M0L4selfS830->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2426 =
      _M0L8_2afieldS3567;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3566;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS829;
    if (
      _M0L3idxS828 < 0
      || _M0L3idxS828 >= Moonbit_array_length(_M0L7entriesS2426)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3566
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2426[
        _M0L3idxS828
      ];
    _M0L7_2abindS829 = _M0L6_2atmpS3566;
    if (_M0L7_2abindS829 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2415;
      if (_M0L7_2abindS829) {
        moonbit_incref(_M0L7_2abindS829);
      }
      moonbit_decref(_M0L4selfS830);
      if (_M0L7_2abindS829) {
        moonbit_decref(_M0L7_2abindS829);
      }
      _M0L6_2atmpS2415 = 0;
      return _M0L6_2atmpS2415;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS831 =
        _M0L7_2abindS829;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS832 =
        _M0L7_2aSomeS831;
      int32_t _M0L4hashS2417 = _M0L8_2aentryS832->$3;
      int32_t _if__result_3972;
      int32_t _M0L8_2afieldS3564;
      int32_t _M0L3pslS2420;
      int32_t _M0L6_2atmpS2422;
      int32_t _M0L6_2atmpS2424;
      int32_t _M0L14capacity__maskS2425;
      int32_t _M0L6_2atmpS2423;
      if (_M0L4hashS2417 == _M0L4hashS825) {
        int32_t _M0L3keyS2416 = _M0L8_2aentryS832->$4;
        _if__result_3972 = _M0L3keyS2416 == _M0L3keyS826;
      } else {
        _if__result_3972 = 0;
      }
      if (_if__result_3972) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3565;
        int32_t _M0L6_2acntS3830;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2419;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2418;
        moonbit_incref(_M0L8_2aentryS832);
        moonbit_decref(_M0L4selfS830);
        _M0L8_2afieldS3565 = _M0L8_2aentryS832->$5;
        _M0L6_2acntS3830 = Moonbit_object_header(_M0L8_2aentryS832)->rc;
        if (_M0L6_2acntS3830 > 1) {
          int32_t _M0L11_2anew__cntS3832 = _M0L6_2acntS3830 - 1;
          Moonbit_object_header(_M0L8_2aentryS832)->rc
          = _M0L11_2anew__cntS3832;
          moonbit_incref(_M0L8_2afieldS3565);
        } else if (_M0L6_2acntS3830 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3831 =
            _M0L8_2aentryS832->$1;
          if (_M0L8_2afieldS3831) {
            moonbit_decref(_M0L8_2afieldS3831);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS832);
        }
        _M0L5valueS2419 = _M0L8_2afieldS3565;
        _M0L6_2atmpS2418 = _M0L5valueS2419;
        return _M0L6_2atmpS2418;
      } else {
        moonbit_incref(_M0L8_2aentryS832);
      }
      _M0L8_2afieldS3564 = _M0L8_2aentryS832->$2;
      moonbit_decref(_M0L8_2aentryS832);
      _M0L3pslS2420 = _M0L8_2afieldS3564;
      if (_M0L1iS827 > _M0L3pslS2420) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2421;
        moonbit_decref(_M0L4selfS830);
        _M0L6_2atmpS2421 = 0;
        return _M0L6_2atmpS2421;
      }
      _M0L6_2atmpS2422 = _M0L1iS827 + 1;
      _M0L6_2atmpS2424 = _M0L3idxS828 + 1;
      _M0L14capacity__maskS2425 = _M0L4selfS830->$3;
      _M0L6_2atmpS2423 = _M0L6_2atmpS2424 & _M0L14capacity__maskS2425;
      _M0L1iS827 = _M0L6_2atmpS2422;
      _M0L3idxS828 = _M0L6_2atmpS2423;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS839,
  moonbit_string_t _M0L3keyS835
) {
  int32_t _M0L4hashS834;
  int32_t _M0L14capacity__maskS2442;
  int32_t _M0L6_2atmpS2441;
  int32_t _M0L1iS836;
  int32_t _M0L3idxS837;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS835);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS834 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS835);
  _M0L14capacity__maskS2442 = _M0L4selfS839->$3;
  _M0L6_2atmpS2441 = _M0L4hashS834 & _M0L14capacity__maskS2442;
  _M0L1iS836 = 0;
  _M0L3idxS837 = _M0L6_2atmpS2441;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3573 =
      _M0L4selfS839->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2440 =
      _M0L8_2afieldS3573;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3572;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS838;
    if (
      _M0L3idxS837 < 0
      || _M0L3idxS837 >= Moonbit_array_length(_M0L7entriesS2440)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3572
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2440[
        _M0L3idxS837
      ];
    _M0L7_2abindS838 = _M0L6_2atmpS3572;
    if (_M0L7_2abindS838 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2429;
      if (_M0L7_2abindS838) {
        moonbit_incref(_M0L7_2abindS838);
      }
      moonbit_decref(_M0L4selfS839);
      if (_M0L7_2abindS838) {
        moonbit_decref(_M0L7_2abindS838);
      }
      moonbit_decref(_M0L3keyS835);
      _M0L6_2atmpS2429 = 0;
      return _M0L6_2atmpS2429;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS840 =
        _M0L7_2abindS838;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS841 =
        _M0L7_2aSomeS840;
      int32_t _M0L4hashS2431 = _M0L8_2aentryS841->$3;
      int32_t _if__result_3974;
      int32_t _M0L8_2afieldS3568;
      int32_t _M0L3pslS2434;
      int32_t _M0L6_2atmpS2436;
      int32_t _M0L6_2atmpS2438;
      int32_t _M0L14capacity__maskS2439;
      int32_t _M0L6_2atmpS2437;
      if (_M0L4hashS2431 == _M0L4hashS834) {
        moonbit_string_t _M0L8_2afieldS3571 = _M0L8_2aentryS841->$4;
        moonbit_string_t _M0L3keyS2430 = _M0L8_2afieldS3571;
        int32_t _M0L6_2atmpS3570;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3570
        = moonbit_val_array_equal(_M0L3keyS2430, _M0L3keyS835);
        _if__result_3974 = _M0L6_2atmpS3570;
      } else {
        _if__result_3974 = 0;
      }
      if (_if__result_3974) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3569;
        int32_t _M0L6_2acntS3833;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2433;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2432;
        moonbit_incref(_M0L8_2aentryS841);
        moonbit_decref(_M0L4selfS839);
        moonbit_decref(_M0L3keyS835);
        _M0L8_2afieldS3569 = _M0L8_2aentryS841->$5;
        _M0L6_2acntS3833 = Moonbit_object_header(_M0L8_2aentryS841)->rc;
        if (_M0L6_2acntS3833 > 1) {
          int32_t _M0L11_2anew__cntS3836 = _M0L6_2acntS3833 - 1;
          Moonbit_object_header(_M0L8_2aentryS841)->rc
          = _M0L11_2anew__cntS3836;
          moonbit_incref(_M0L8_2afieldS3569);
        } else if (_M0L6_2acntS3833 == 1) {
          moonbit_string_t _M0L8_2afieldS3835 = _M0L8_2aentryS841->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3834;
          moonbit_decref(_M0L8_2afieldS3835);
          _M0L8_2afieldS3834 = _M0L8_2aentryS841->$1;
          if (_M0L8_2afieldS3834) {
            moonbit_decref(_M0L8_2afieldS3834);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS841);
        }
        _M0L5valueS2433 = _M0L8_2afieldS3569;
        _M0L6_2atmpS2432 = _M0L5valueS2433;
        return _M0L6_2atmpS2432;
      } else {
        moonbit_incref(_M0L8_2aentryS841);
      }
      _M0L8_2afieldS3568 = _M0L8_2aentryS841->$2;
      moonbit_decref(_M0L8_2aentryS841);
      _M0L3pslS2434 = _M0L8_2afieldS3568;
      if (_M0L1iS836 > _M0L3pslS2434) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2435;
        moonbit_decref(_M0L4selfS839);
        moonbit_decref(_M0L3keyS835);
        _M0L6_2atmpS2435 = 0;
        return _M0L6_2atmpS2435;
      }
      _M0L6_2atmpS2436 = _M0L1iS836 + 1;
      _M0L6_2atmpS2438 = _M0L3idxS837 + 1;
      _M0L14capacity__maskS2439 = _M0L4selfS839->$3;
      _M0L6_2atmpS2437 = _M0L6_2atmpS2438 & _M0L14capacity__maskS2439;
      _M0L1iS836 = _M0L6_2atmpS2436;
      _M0L3idxS837 = _M0L6_2atmpS2437;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS794
) {
  int32_t _M0L6lengthS793;
  int32_t _M0Lm8capacityS795;
  int32_t _M0L6_2atmpS2368;
  int32_t _M0L6_2atmpS2367;
  int32_t _M0L6_2atmpS2378;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS796;
  int32_t _M0L3endS2376;
  int32_t _M0L5startS2377;
  int32_t _M0L7_2abindS797;
  int32_t _M0L2__S798;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS794.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS793
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS794);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS795 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS793);
  _M0L6_2atmpS2368 = _M0Lm8capacityS795;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2367 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2368);
  if (_M0L6lengthS793 > _M0L6_2atmpS2367) {
    int32_t _M0L6_2atmpS2369 = _M0Lm8capacityS795;
    _M0Lm8capacityS795 = _M0L6_2atmpS2369 * 2;
  }
  _M0L6_2atmpS2378 = _M0Lm8capacityS795;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS796
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2378);
  _M0L3endS2376 = _M0L3arrS794.$2;
  _M0L5startS2377 = _M0L3arrS794.$1;
  _M0L7_2abindS797 = _M0L3endS2376 - _M0L5startS2377;
  _M0L2__S798 = 0;
  while (1) {
    if (_M0L2__S798 < _M0L7_2abindS797) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3577 =
        _M0L3arrS794.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2373 =
        _M0L8_2afieldS3577;
      int32_t _M0L5startS2375 = _M0L3arrS794.$1;
      int32_t _M0L6_2atmpS2374 = _M0L5startS2375 + _M0L2__S798;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3576 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2373[
          _M0L6_2atmpS2374
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS799 =
        _M0L6_2atmpS3576;
      moonbit_string_t _M0L8_2afieldS3575 = _M0L1eS799->$0;
      moonbit_string_t _M0L6_2atmpS2370 = _M0L8_2afieldS3575;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3574 =
        _M0L1eS799->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2371 =
        _M0L8_2afieldS3574;
      int32_t _M0L6_2atmpS2372;
      moonbit_incref(_M0L6_2atmpS2371);
      moonbit_incref(_M0L6_2atmpS2370);
      moonbit_incref(_M0L1mS796);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS796, _M0L6_2atmpS2370, _M0L6_2atmpS2371);
      _M0L6_2atmpS2372 = _M0L2__S798 + 1;
      _M0L2__S798 = _M0L6_2atmpS2372;
      continue;
    } else {
      moonbit_decref(_M0L3arrS794.$0);
    }
    break;
  }
  return _M0L1mS796;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS802
) {
  int32_t _M0L6lengthS801;
  int32_t _M0Lm8capacityS803;
  int32_t _M0L6_2atmpS2380;
  int32_t _M0L6_2atmpS2379;
  int32_t _M0L6_2atmpS2390;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS804;
  int32_t _M0L3endS2388;
  int32_t _M0L5startS2389;
  int32_t _M0L7_2abindS805;
  int32_t _M0L2__S806;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS802.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS801
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS802);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS803 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS801);
  _M0L6_2atmpS2380 = _M0Lm8capacityS803;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2379 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2380);
  if (_M0L6lengthS801 > _M0L6_2atmpS2379) {
    int32_t _M0L6_2atmpS2381 = _M0Lm8capacityS803;
    _M0Lm8capacityS803 = _M0L6_2atmpS2381 * 2;
  }
  _M0L6_2atmpS2390 = _M0Lm8capacityS803;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS804
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2390);
  _M0L3endS2388 = _M0L3arrS802.$2;
  _M0L5startS2389 = _M0L3arrS802.$1;
  _M0L7_2abindS805 = _M0L3endS2388 - _M0L5startS2389;
  _M0L2__S806 = 0;
  while (1) {
    if (_M0L2__S806 < _M0L7_2abindS805) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3580 =
        _M0L3arrS802.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2385 =
        _M0L8_2afieldS3580;
      int32_t _M0L5startS2387 = _M0L3arrS802.$1;
      int32_t _M0L6_2atmpS2386 = _M0L5startS2387 + _M0L2__S806;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3579 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2385[
          _M0L6_2atmpS2386
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS807 = _M0L6_2atmpS3579;
      int32_t _M0L6_2atmpS2382 = _M0L1eS807->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3578 =
        _M0L1eS807->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2383 =
        _M0L8_2afieldS3578;
      int32_t _M0L6_2atmpS2384;
      moonbit_incref(_M0L6_2atmpS2383);
      moonbit_incref(_M0L1mS804);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS804, _M0L6_2atmpS2382, _M0L6_2atmpS2383);
      _M0L6_2atmpS2384 = _M0L2__S806 + 1;
      _M0L2__S806 = _M0L6_2atmpS2384;
      continue;
    } else {
      moonbit_decref(_M0L3arrS802.$0);
    }
    break;
  }
  return _M0L1mS804;
}

struct _M0TPB3MapGsRPC16string10StringViewE* _M0MPB3Map11from__arrayGsRPC16string10StringViewE(
  struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE _M0L3arrS810
) {
  int32_t _M0L6lengthS809;
  int32_t _M0Lm8capacityS811;
  int32_t _M0L6_2atmpS2392;
  int32_t _M0L6_2atmpS2391;
  int32_t _M0L6_2atmpS2402;
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L1mS812;
  int32_t _M0L3endS2400;
  int32_t _M0L5startS2401;
  int32_t _M0L7_2abindS813;
  int32_t _M0L2__S814;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS810.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS809
  = _M0MPC15array9ArrayView6lengthGUsRPC16string10StringViewEE(_M0L3arrS810);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS811 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS809);
  _M0L6_2atmpS2392 = _M0Lm8capacityS811;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2391 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2392);
  if (_M0L6lengthS809 > _M0L6_2atmpS2391) {
    int32_t _M0L6_2atmpS2393 = _M0Lm8capacityS811;
    _M0Lm8capacityS811 = _M0L6_2atmpS2393 * 2;
  }
  _M0L6_2atmpS2402 = _M0Lm8capacityS811;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS812
  = _M0MPB3Map11new_2einnerGsRPC16string10StringViewE(_M0L6_2atmpS2402);
  _M0L3endS2400 = _M0L3arrS810.$2;
  _M0L5startS2401 = _M0L3arrS810.$1;
  _M0L7_2abindS813 = _M0L3endS2400 - _M0L5startS2401;
  _M0L2__S814 = 0;
  while (1) {
    if (_M0L2__S814 < _M0L7_2abindS813) {
      struct _M0TUsRPC16string10StringViewE** _M0L8_2afieldS3584 =
        _M0L3arrS810.$0;
      struct _M0TUsRPC16string10StringViewE** _M0L3bufS2397 =
        _M0L8_2afieldS3584;
      int32_t _M0L5startS2399 = _M0L3arrS810.$1;
      int32_t _M0L6_2atmpS2398 = _M0L5startS2399 + _M0L2__S814;
      struct _M0TUsRPC16string10StringViewE* _M0L6_2atmpS3583 =
        (struct _M0TUsRPC16string10StringViewE*)_M0L3bufS2397[
          _M0L6_2atmpS2398
        ];
      struct _M0TUsRPC16string10StringViewE* _M0L1eS815 = _M0L6_2atmpS3583;
      moonbit_string_t _M0L8_2afieldS3582 = _M0L1eS815->$0;
      moonbit_string_t _M0L6_2atmpS2394 = _M0L8_2afieldS3582;
      struct _M0TPC16string10StringView _M0L8_2afieldS3581 =
        (struct _M0TPC16string10StringView){_M0L1eS815->$1_1,
                                              _M0L1eS815->$1_2,
                                              _M0L1eS815->$1_0};
      struct _M0TPC16string10StringView _M0L6_2atmpS2395 = _M0L8_2afieldS3581;
      int32_t _M0L6_2atmpS2396;
      moonbit_incref(_M0L6_2atmpS2395.$0);
      moonbit_incref(_M0L6_2atmpS2394);
      moonbit_incref(_M0L1mS812);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPC16string10StringViewE(_M0L1mS812, _M0L6_2atmpS2394, _M0L6_2atmpS2395);
      _M0L6_2atmpS2396 = _M0L2__S814 + 1;
      _M0L2__S814 = _M0L6_2atmpS2396;
      continue;
    } else {
      moonbit_decref(_M0L3arrS810.$0);
    }
    break;
  }
  return _M0L1mS812;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS818
) {
  int32_t _M0L6lengthS817;
  int32_t _M0Lm8capacityS819;
  int32_t _M0L6_2atmpS2404;
  int32_t _M0L6_2atmpS2403;
  int32_t _M0L6_2atmpS2414;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS820;
  int32_t _M0L3endS2412;
  int32_t _M0L5startS2413;
  int32_t _M0L7_2abindS821;
  int32_t _M0L2__S822;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS818.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS817 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS818);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS819 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS817);
  _M0L6_2atmpS2404 = _M0Lm8capacityS819;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2403 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2404);
  if (_M0L6lengthS817 > _M0L6_2atmpS2403) {
    int32_t _M0L6_2atmpS2405 = _M0Lm8capacityS819;
    _M0Lm8capacityS819 = _M0L6_2atmpS2405 * 2;
  }
  _M0L6_2atmpS2414 = _M0Lm8capacityS819;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS820 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2414);
  _M0L3endS2412 = _M0L3arrS818.$2;
  _M0L5startS2413 = _M0L3arrS818.$1;
  _M0L7_2abindS821 = _M0L3endS2412 - _M0L5startS2413;
  _M0L2__S822 = 0;
  while (1) {
    if (_M0L2__S822 < _M0L7_2abindS821) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3588 = _M0L3arrS818.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2409 = _M0L8_2afieldS3588;
      int32_t _M0L5startS2411 = _M0L3arrS818.$1;
      int32_t _M0L6_2atmpS2410 = _M0L5startS2411 + _M0L2__S822;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3587 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2409[_M0L6_2atmpS2410];
      struct _M0TUsRPB4JsonE* _M0L1eS823 = _M0L6_2atmpS3587;
      moonbit_string_t _M0L8_2afieldS3586 = _M0L1eS823->$0;
      moonbit_string_t _M0L6_2atmpS2406 = _M0L8_2afieldS3586;
      void* _M0L8_2afieldS3585 = _M0L1eS823->$1;
      void* _M0L6_2atmpS2407 = _M0L8_2afieldS3585;
      int32_t _M0L6_2atmpS2408;
      moonbit_incref(_M0L6_2atmpS2407);
      moonbit_incref(_M0L6_2atmpS2406);
      moonbit_incref(_M0L1mS820);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS820, _M0L6_2atmpS2406, _M0L6_2atmpS2407);
      _M0L6_2atmpS2408 = _M0L2__S822 + 1;
      _M0L2__S822 = _M0L6_2atmpS2408;
      continue;
    } else {
      moonbit_decref(_M0L3arrS818.$0);
    }
    break;
  }
  return _M0L1mS820;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS781,
  moonbit_string_t _M0L3keyS782,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS783
) {
  int32_t _M0L6_2atmpS2363;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS782);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2363 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS782);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS781, _M0L3keyS782, _M0L5valueS783, _M0L6_2atmpS2363);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS784,
  int32_t _M0L3keyS785,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS786
) {
  int32_t _M0L6_2atmpS2364;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2364 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS785);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS784, _M0L3keyS785, _M0L5valueS786, _M0L6_2atmpS2364);
  return 0;
}

int32_t _M0MPB3Map3setGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS787,
  moonbit_string_t _M0L3keyS788,
  struct _M0TPC16string10StringView _M0L5valueS789
) {
  int32_t _M0L6_2atmpS2365;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS788);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2365 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS788);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPC16string10StringViewE(_M0L4selfS787, _M0L3keyS788, _M0L5valueS789, _M0L6_2atmpS2365);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS790,
  moonbit_string_t _M0L3keyS791,
  void* _M0L5valueS792
) {
  int32_t _M0L6_2atmpS2366;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS791);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2366 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS791);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS790, _M0L3keyS791, _M0L5valueS792, _M0L6_2atmpS2366);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS738
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3595;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS737;
  int32_t _M0L8capacityS2341;
  int32_t _M0L13new__capacityS739;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2336;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2335;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3594;
  int32_t _M0L6_2atmpS2337;
  int32_t _M0L8capacityS2339;
  int32_t _M0L6_2atmpS2338;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2340;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3593;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS740;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3595 = _M0L4selfS738->$5;
  _M0L9old__headS737 = _M0L8_2afieldS3595;
  _M0L8capacityS2341 = _M0L4selfS738->$2;
  _M0L13new__capacityS739 = _M0L8capacityS2341 << 1;
  _M0L6_2atmpS2336 = 0;
  _M0L6_2atmpS2335
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS739, _M0L6_2atmpS2336);
  _M0L6_2aoldS3594 = _M0L4selfS738->$0;
  if (_M0L9old__headS737) {
    moonbit_incref(_M0L9old__headS737);
  }
  moonbit_decref(_M0L6_2aoldS3594);
  _M0L4selfS738->$0 = _M0L6_2atmpS2335;
  _M0L4selfS738->$2 = _M0L13new__capacityS739;
  _M0L6_2atmpS2337 = _M0L13new__capacityS739 - 1;
  _M0L4selfS738->$3 = _M0L6_2atmpS2337;
  _M0L8capacityS2339 = _M0L4selfS738->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2338 = _M0FPB21calc__grow__threshold(_M0L8capacityS2339);
  _M0L4selfS738->$4 = _M0L6_2atmpS2338;
  _M0L4selfS738->$1 = 0;
  _M0L6_2atmpS2340 = 0;
  _M0L6_2aoldS3593 = _M0L4selfS738->$5;
  if (_M0L6_2aoldS3593) {
    moonbit_decref(_M0L6_2aoldS3593);
  }
  _M0L4selfS738->$5 = _M0L6_2atmpS2340;
  _M0L4selfS738->$6 = -1;
  _M0L8_2aparamS740 = _M0L9old__headS737;
  while (1) {
    if (_M0L8_2aparamS740 == 0) {
      if (_M0L8_2aparamS740) {
        moonbit_decref(_M0L8_2aparamS740);
      }
      moonbit_decref(_M0L4selfS738);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS741 =
        _M0L8_2aparamS740;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS742 =
        _M0L7_2aSomeS741;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3592 =
        _M0L4_2axS742->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS743 =
        _M0L8_2afieldS3592;
      moonbit_string_t _M0L8_2afieldS3591 = _M0L4_2axS742->$4;
      moonbit_string_t _M0L6_2akeyS744 = _M0L8_2afieldS3591;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3590 =
        _M0L4_2axS742->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS745 =
        _M0L8_2afieldS3590;
      int32_t _M0L8_2afieldS3589 = _M0L4_2axS742->$3;
      int32_t _M0L6_2acntS3837 = Moonbit_object_header(_M0L4_2axS742)->rc;
      int32_t _M0L7_2ahashS746;
      if (_M0L6_2acntS3837 > 1) {
        int32_t _M0L11_2anew__cntS3838 = _M0L6_2acntS3837 - 1;
        Moonbit_object_header(_M0L4_2axS742)->rc = _M0L11_2anew__cntS3838;
        moonbit_incref(_M0L8_2avalueS745);
        moonbit_incref(_M0L6_2akeyS744);
        if (_M0L7_2anextS743) {
          moonbit_incref(_M0L7_2anextS743);
        }
      } else if (_M0L6_2acntS3837 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS742);
      }
      _M0L7_2ahashS746 = _M0L8_2afieldS3589;
      moonbit_incref(_M0L4selfS738);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS738, _M0L6_2akeyS744, _M0L8_2avalueS745, _M0L7_2ahashS746);
      _M0L8_2aparamS740 = _M0L7_2anextS743;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS749
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3601;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS748;
  int32_t _M0L8capacityS2348;
  int32_t _M0L13new__capacityS750;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2343;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2342;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3600;
  int32_t _M0L6_2atmpS2344;
  int32_t _M0L8capacityS2346;
  int32_t _M0L6_2atmpS2345;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2347;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3599;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS751;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3601 = _M0L4selfS749->$5;
  _M0L9old__headS748 = _M0L8_2afieldS3601;
  _M0L8capacityS2348 = _M0L4selfS749->$2;
  _M0L13new__capacityS750 = _M0L8capacityS2348 << 1;
  _M0L6_2atmpS2343 = 0;
  _M0L6_2atmpS2342
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS750, _M0L6_2atmpS2343);
  _M0L6_2aoldS3600 = _M0L4selfS749->$0;
  if (_M0L9old__headS748) {
    moonbit_incref(_M0L9old__headS748);
  }
  moonbit_decref(_M0L6_2aoldS3600);
  _M0L4selfS749->$0 = _M0L6_2atmpS2342;
  _M0L4selfS749->$2 = _M0L13new__capacityS750;
  _M0L6_2atmpS2344 = _M0L13new__capacityS750 - 1;
  _M0L4selfS749->$3 = _M0L6_2atmpS2344;
  _M0L8capacityS2346 = _M0L4selfS749->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2345 = _M0FPB21calc__grow__threshold(_M0L8capacityS2346);
  _M0L4selfS749->$4 = _M0L6_2atmpS2345;
  _M0L4selfS749->$1 = 0;
  _M0L6_2atmpS2347 = 0;
  _M0L6_2aoldS3599 = _M0L4selfS749->$5;
  if (_M0L6_2aoldS3599) {
    moonbit_decref(_M0L6_2aoldS3599);
  }
  _M0L4selfS749->$5 = _M0L6_2atmpS2347;
  _M0L4selfS749->$6 = -1;
  _M0L8_2aparamS751 = _M0L9old__headS748;
  while (1) {
    if (_M0L8_2aparamS751 == 0) {
      if (_M0L8_2aparamS751) {
        moonbit_decref(_M0L8_2aparamS751);
      }
      moonbit_decref(_M0L4selfS749);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS752 =
        _M0L8_2aparamS751;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS753 =
        _M0L7_2aSomeS752;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3598 =
        _M0L4_2axS753->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS754 =
        _M0L8_2afieldS3598;
      int32_t _M0L6_2akeyS755 = _M0L4_2axS753->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3597 =
        _M0L4_2axS753->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS756 =
        _M0L8_2afieldS3597;
      int32_t _M0L8_2afieldS3596 = _M0L4_2axS753->$3;
      int32_t _M0L6_2acntS3839 = Moonbit_object_header(_M0L4_2axS753)->rc;
      int32_t _M0L7_2ahashS757;
      if (_M0L6_2acntS3839 > 1) {
        int32_t _M0L11_2anew__cntS3840 = _M0L6_2acntS3839 - 1;
        Moonbit_object_header(_M0L4_2axS753)->rc = _M0L11_2anew__cntS3840;
        moonbit_incref(_M0L8_2avalueS756);
        if (_M0L7_2anextS754) {
          moonbit_incref(_M0L7_2anextS754);
        }
      } else if (_M0L6_2acntS3839 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS753);
      }
      _M0L7_2ahashS757 = _M0L8_2afieldS3596;
      moonbit_incref(_M0L4selfS749);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS749, _M0L6_2akeyS755, _M0L8_2avalueS756, _M0L7_2ahashS757);
      _M0L8_2aparamS751 = _M0L7_2anextS754;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS760
) {
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2afieldS3608;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L9old__headS759;
  int32_t _M0L8capacityS2355;
  int32_t _M0L13new__capacityS761;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2350;
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L6_2atmpS2349;
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L6_2aoldS3607;
  int32_t _M0L6_2atmpS2351;
  int32_t _M0L8capacityS2353;
  int32_t _M0L6_2atmpS2352;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2354;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2aoldS3606;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2aparamS762;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3608 = _M0L4selfS760->$5;
  _M0L9old__headS759 = _M0L8_2afieldS3608;
  _M0L8capacityS2355 = _M0L4selfS760->$2;
  _M0L13new__capacityS761 = _M0L8capacityS2355 << 1;
  _M0L6_2atmpS2350 = 0;
  _M0L6_2atmpS2349
  = (struct _M0TPB5EntryGsRPC16string10StringViewE**)moonbit_make_ref_array(_M0L13new__capacityS761, _M0L6_2atmpS2350);
  _M0L6_2aoldS3607 = _M0L4selfS760->$0;
  if (_M0L9old__headS759) {
    moonbit_incref(_M0L9old__headS759);
  }
  moonbit_decref(_M0L6_2aoldS3607);
  _M0L4selfS760->$0 = _M0L6_2atmpS2349;
  _M0L4selfS760->$2 = _M0L13new__capacityS761;
  _M0L6_2atmpS2351 = _M0L13new__capacityS761 - 1;
  _M0L4selfS760->$3 = _M0L6_2atmpS2351;
  _M0L8capacityS2353 = _M0L4selfS760->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2352 = _M0FPB21calc__grow__threshold(_M0L8capacityS2353);
  _M0L4selfS760->$4 = _M0L6_2atmpS2352;
  _M0L4selfS760->$1 = 0;
  _M0L6_2atmpS2354 = 0;
  _M0L6_2aoldS3606 = _M0L4selfS760->$5;
  if (_M0L6_2aoldS3606) {
    moonbit_decref(_M0L6_2aoldS3606);
  }
  _M0L4selfS760->$5 = _M0L6_2atmpS2354;
  _M0L4selfS760->$6 = -1;
  _M0L8_2aparamS762 = _M0L9old__headS759;
  while (1) {
    if (_M0L8_2aparamS762 == 0) {
      if (_M0L8_2aparamS762) {
        moonbit_decref(_M0L8_2aparamS762);
      }
      moonbit_decref(_M0L4selfS760);
    } else {
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2aSomeS763 =
        _M0L8_2aparamS762;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L4_2axS764 =
        _M0L7_2aSomeS763;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2afieldS3605 =
        _M0L4_2axS764->$1;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2anextS765 =
        _M0L8_2afieldS3605;
      moonbit_string_t _M0L8_2afieldS3604 = _M0L4_2axS764->$4;
      moonbit_string_t _M0L6_2akeyS766 = _M0L8_2afieldS3604;
      struct _M0TPC16string10StringView _M0L8_2afieldS3603 =
        (struct _M0TPC16string10StringView){_M0L4_2axS764->$5_1,
                                              _M0L4_2axS764->$5_2,
                                              _M0L4_2axS764->$5_0};
      struct _M0TPC16string10StringView _M0L8_2avalueS767 =
        _M0L8_2afieldS3603;
      int32_t _M0L8_2afieldS3602 = _M0L4_2axS764->$3;
      int32_t _M0L6_2acntS3841 = Moonbit_object_header(_M0L4_2axS764)->rc;
      int32_t _M0L7_2ahashS768;
      if (_M0L6_2acntS3841 > 1) {
        int32_t _M0L11_2anew__cntS3842 = _M0L6_2acntS3841 - 1;
        Moonbit_object_header(_M0L4_2axS764)->rc = _M0L11_2anew__cntS3842;
        moonbit_incref(_M0L8_2avalueS767.$0);
        moonbit_incref(_M0L6_2akeyS766);
        if (_M0L7_2anextS765) {
          moonbit_incref(_M0L7_2anextS765);
        }
      } else if (_M0L6_2acntS3841 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS764);
      }
      _M0L7_2ahashS768 = _M0L8_2afieldS3602;
      moonbit_incref(_M0L4selfS760);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPC16string10StringViewE(_M0L4selfS760, _M0L6_2akeyS766, _M0L8_2avalueS767, _M0L7_2ahashS768);
      _M0L8_2aparamS762 = _M0L7_2anextS765;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS771
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3615;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS770;
  int32_t _M0L8capacityS2362;
  int32_t _M0L13new__capacityS772;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2357;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2356;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3614;
  int32_t _M0L6_2atmpS2358;
  int32_t _M0L8capacityS2360;
  int32_t _M0L6_2atmpS2359;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2361;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3613;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS773;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3615 = _M0L4selfS771->$5;
  _M0L9old__headS770 = _M0L8_2afieldS3615;
  _M0L8capacityS2362 = _M0L4selfS771->$2;
  _M0L13new__capacityS772 = _M0L8capacityS2362 << 1;
  _M0L6_2atmpS2357 = 0;
  _M0L6_2atmpS2356
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS772, _M0L6_2atmpS2357);
  _M0L6_2aoldS3614 = _M0L4selfS771->$0;
  if (_M0L9old__headS770) {
    moonbit_incref(_M0L9old__headS770);
  }
  moonbit_decref(_M0L6_2aoldS3614);
  _M0L4selfS771->$0 = _M0L6_2atmpS2356;
  _M0L4selfS771->$2 = _M0L13new__capacityS772;
  _M0L6_2atmpS2358 = _M0L13new__capacityS772 - 1;
  _M0L4selfS771->$3 = _M0L6_2atmpS2358;
  _M0L8capacityS2360 = _M0L4selfS771->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2359 = _M0FPB21calc__grow__threshold(_M0L8capacityS2360);
  _M0L4selfS771->$4 = _M0L6_2atmpS2359;
  _M0L4selfS771->$1 = 0;
  _M0L6_2atmpS2361 = 0;
  _M0L6_2aoldS3613 = _M0L4selfS771->$5;
  if (_M0L6_2aoldS3613) {
    moonbit_decref(_M0L6_2aoldS3613);
  }
  _M0L4selfS771->$5 = _M0L6_2atmpS2361;
  _M0L4selfS771->$6 = -1;
  _M0L8_2aparamS773 = _M0L9old__headS770;
  while (1) {
    if (_M0L8_2aparamS773 == 0) {
      if (_M0L8_2aparamS773) {
        moonbit_decref(_M0L8_2aparamS773);
      }
      moonbit_decref(_M0L4selfS771);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS774 = _M0L8_2aparamS773;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS775 = _M0L7_2aSomeS774;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3612 = _M0L4_2axS775->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS776 = _M0L8_2afieldS3612;
      moonbit_string_t _M0L8_2afieldS3611 = _M0L4_2axS775->$4;
      moonbit_string_t _M0L6_2akeyS777 = _M0L8_2afieldS3611;
      void* _M0L8_2afieldS3610 = _M0L4_2axS775->$5;
      void* _M0L8_2avalueS778 = _M0L8_2afieldS3610;
      int32_t _M0L8_2afieldS3609 = _M0L4_2axS775->$3;
      int32_t _M0L6_2acntS3843 = Moonbit_object_header(_M0L4_2axS775)->rc;
      int32_t _M0L7_2ahashS779;
      if (_M0L6_2acntS3843 > 1) {
        int32_t _M0L11_2anew__cntS3844 = _M0L6_2acntS3843 - 1;
        Moonbit_object_header(_M0L4_2axS775)->rc = _M0L11_2anew__cntS3844;
        moonbit_incref(_M0L8_2avalueS778);
        moonbit_incref(_M0L6_2akeyS777);
        if (_M0L7_2anextS776) {
          moonbit_incref(_M0L7_2anextS776);
        }
      } else if (_M0L6_2acntS3843 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS775);
      }
      _M0L7_2ahashS779 = _M0L8_2afieldS3609;
      moonbit_incref(_M0L4selfS771);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS771, _M0L6_2akeyS777, _M0L8_2avalueS778, _M0L7_2ahashS779);
      _M0L8_2aparamS773 = _M0L7_2anextS776;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS676,
  moonbit_string_t _M0L3keyS682,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS683,
  int32_t _M0L4hashS678
) {
  int32_t _M0L14capacity__maskS2280;
  int32_t _M0L6_2atmpS2279;
  int32_t _M0L3pslS673;
  int32_t _M0L3idxS674;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2280 = _M0L4selfS676->$3;
  _M0L6_2atmpS2279 = _M0L4hashS678 & _M0L14capacity__maskS2280;
  _M0L3pslS673 = 0;
  _M0L3idxS674 = _M0L6_2atmpS2279;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3620 =
      _M0L4selfS676->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2278 =
      _M0L8_2afieldS3620;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3619;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS675;
    if (
      _M0L3idxS674 < 0
      || _M0L3idxS674 >= Moonbit_array_length(_M0L7entriesS2278)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3619
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2278[
        _M0L3idxS674
      ];
    _M0L7_2abindS675 = _M0L6_2atmpS3619;
    if (_M0L7_2abindS675 == 0) {
      int32_t _M0L4sizeS2263 = _M0L4selfS676->$1;
      int32_t _M0L8grow__atS2264 = _M0L4selfS676->$4;
      int32_t _M0L7_2abindS679;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS680;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS681;
      if (_M0L4sizeS2263 >= _M0L8grow__atS2264) {
        int32_t _M0L14capacity__maskS2266;
        int32_t _M0L6_2atmpS2265;
        moonbit_incref(_M0L4selfS676);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS676);
        _M0L14capacity__maskS2266 = _M0L4selfS676->$3;
        _M0L6_2atmpS2265 = _M0L4hashS678 & _M0L14capacity__maskS2266;
        _M0L3pslS673 = 0;
        _M0L3idxS674 = _M0L6_2atmpS2265;
        continue;
      }
      _M0L7_2abindS679 = _M0L4selfS676->$6;
      _M0L7_2abindS680 = 0;
      _M0L5entryS681
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS681)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS681->$0 = _M0L7_2abindS679;
      _M0L5entryS681->$1 = _M0L7_2abindS680;
      _M0L5entryS681->$2 = _M0L3pslS673;
      _M0L5entryS681->$3 = _M0L4hashS678;
      _M0L5entryS681->$4 = _M0L3keyS682;
      _M0L5entryS681->$5 = _M0L5valueS683;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS676, _M0L3idxS674, _M0L5entryS681);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS684 =
        _M0L7_2abindS675;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS685 =
        _M0L7_2aSomeS684;
      int32_t _M0L4hashS2268 = _M0L14_2acurr__entryS685->$3;
      int32_t _if__result_3984;
      int32_t _M0L3pslS2269;
      int32_t _M0L6_2atmpS2274;
      int32_t _M0L6_2atmpS2276;
      int32_t _M0L14capacity__maskS2277;
      int32_t _M0L6_2atmpS2275;
      if (_M0L4hashS2268 == _M0L4hashS678) {
        moonbit_string_t _M0L8_2afieldS3618 = _M0L14_2acurr__entryS685->$4;
        moonbit_string_t _M0L3keyS2267 = _M0L8_2afieldS3618;
        int32_t _M0L6_2atmpS3617;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3617
        = moonbit_val_array_equal(_M0L3keyS2267, _M0L3keyS682);
        _if__result_3984 = _M0L6_2atmpS3617;
      } else {
        _if__result_3984 = 0;
      }
      if (_if__result_3984) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3616;
        moonbit_incref(_M0L14_2acurr__entryS685);
        moonbit_decref(_M0L3keyS682);
        moonbit_decref(_M0L4selfS676);
        _M0L6_2aoldS3616 = _M0L14_2acurr__entryS685->$5;
        moonbit_decref(_M0L6_2aoldS3616);
        _M0L14_2acurr__entryS685->$5 = _M0L5valueS683;
        moonbit_decref(_M0L14_2acurr__entryS685);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS685);
      }
      _M0L3pslS2269 = _M0L14_2acurr__entryS685->$2;
      if (_M0L3pslS673 > _M0L3pslS2269) {
        int32_t _M0L4sizeS2270 = _M0L4selfS676->$1;
        int32_t _M0L8grow__atS2271 = _M0L4selfS676->$4;
        int32_t _M0L7_2abindS686;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS687;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS688;
        if (_M0L4sizeS2270 >= _M0L8grow__atS2271) {
          int32_t _M0L14capacity__maskS2273;
          int32_t _M0L6_2atmpS2272;
          moonbit_decref(_M0L14_2acurr__entryS685);
          moonbit_incref(_M0L4selfS676);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS676);
          _M0L14capacity__maskS2273 = _M0L4selfS676->$3;
          _M0L6_2atmpS2272 = _M0L4hashS678 & _M0L14capacity__maskS2273;
          _M0L3pslS673 = 0;
          _M0L3idxS674 = _M0L6_2atmpS2272;
          continue;
        }
        moonbit_incref(_M0L4selfS676);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS676, _M0L3idxS674, _M0L14_2acurr__entryS685);
        _M0L7_2abindS686 = _M0L4selfS676->$6;
        _M0L7_2abindS687 = 0;
        _M0L5entryS688
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS688)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS688->$0 = _M0L7_2abindS686;
        _M0L5entryS688->$1 = _M0L7_2abindS687;
        _M0L5entryS688->$2 = _M0L3pslS673;
        _M0L5entryS688->$3 = _M0L4hashS678;
        _M0L5entryS688->$4 = _M0L3keyS682;
        _M0L5entryS688->$5 = _M0L5valueS683;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS676, _M0L3idxS674, _M0L5entryS688);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS685);
      }
      _M0L6_2atmpS2274 = _M0L3pslS673 + 1;
      _M0L6_2atmpS2276 = _M0L3idxS674 + 1;
      _M0L14capacity__maskS2277 = _M0L4selfS676->$3;
      _M0L6_2atmpS2275 = _M0L6_2atmpS2276 & _M0L14capacity__maskS2277;
      _M0L3pslS673 = _M0L6_2atmpS2274;
      _M0L3idxS674 = _M0L6_2atmpS2275;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS692,
  int32_t _M0L3keyS698,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS699,
  int32_t _M0L4hashS694
) {
  int32_t _M0L14capacity__maskS2298;
  int32_t _M0L6_2atmpS2297;
  int32_t _M0L3pslS689;
  int32_t _M0L3idxS690;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2298 = _M0L4selfS692->$3;
  _M0L6_2atmpS2297 = _M0L4hashS694 & _M0L14capacity__maskS2298;
  _M0L3pslS689 = 0;
  _M0L3idxS690 = _M0L6_2atmpS2297;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3623 =
      _M0L4selfS692->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2296 =
      _M0L8_2afieldS3623;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3622;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS691;
    if (
      _M0L3idxS690 < 0
      || _M0L3idxS690 >= Moonbit_array_length(_M0L7entriesS2296)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3622
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2296[
        _M0L3idxS690
      ];
    _M0L7_2abindS691 = _M0L6_2atmpS3622;
    if (_M0L7_2abindS691 == 0) {
      int32_t _M0L4sizeS2281 = _M0L4selfS692->$1;
      int32_t _M0L8grow__atS2282 = _M0L4selfS692->$4;
      int32_t _M0L7_2abindS695;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS696;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS697;
      if (_M0L4sizeS2281 >= _M0L8grow__atS2282) {
        int32_t _M0L14capacity__maskS2284;
        int32_t _M0L6_2atmpS2283;
        moonbit_incref(_M0L4selfS692);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS692);
        _M0L14capacity__maskS2284 = _M0L4selfS692->$3;
        _M0L6_2atmpS2283 = _M0L4hashS694 & _M0L14capacity__maskS2284;
        _M0L3pslS689 = 0;
        _M0L3idxS690 = _M0L6_2atmpS2283;
        continue;
      }
      _M0L7_2abindS695 = _M0L4selfS692->$6;
      _M0L7_2abindS696 = 0;
      _M0L5entryS697
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS697)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS697->$0 = _M0L7_2abindS695;
      _M0L5entryS697->$1 = _M0L7_2abindS696;
      _M0L5entryS697->$2 = _M0L3pslS689;
      _M0L5entryS697->$3 = _M0L4hashS694;
      _M0L5entryS697->$4 = _M0L3keyS698;
      _M0L5entryS697->$5 = _M0L5valueS699;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS692, _M0L3idxS690, _M0L5entryS697);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS700 =
        _M0L7_2abindS691;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS701 =
        _M0L7_2aSomeS700;
      int32_t _M0L4hashS2286 = _M0L14_2acurr__entryS701->$3;
      int32_t _if__result_3986;
      int32_t _M0L3pslS2287;
      int32_t _M0L6_2atmpS2292;
      int32_t _M0L6_2atmpS2294;
      int32_t _M0L14capacity__maskS2295;
      int32_t _M0L6_2atmpS2293;
      if (_M0L4hashS2286 == _M0L4hashS694) {
        int32_t _M0L3keyS2285 = _M0L14_2acurr__entryS701->$4;
        _if__result_3986 = _M0L3keyS2285 == _M0L3keyS698;
      } else {
        _if__result_3986 = 0;
      }
      if (_if__result_3986) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3621;
        moonbit_incref(_M0L14_2acurr__entryS701);
        moonbit_decref(_M0L4selfS692);
        _M0L6_2aoldS3621 = _M0L14_2acurr__entryS701->$5;
        moonbit_decref(_M0L6_2aoldS3621);
        _M0L14_2acurr__entryS701->$5 = _M0L5valueS699;
        moonbit_decref(_M0L14_2acurr__entryS701);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS701);
      }
      _M0L3pslS2287 = _M0L14_2acurr__entryS701->$2;
      if (_M0L3pslS689 > _M0L3pslS2287) {
        int32_t _M0L4sizeS2288 = _M0L4selfS692->$1;
        int32_t _M0L8grow__atS2289 = _M0L4selfS692->$4;
        int32_t _M0L7_2abindS702;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS703;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS704;
        if (_M0L4sizeS2288 >= _M0L8grow__atS2289) {
          int32_t _M0L14capacity__maskS2291;
          int32_t _M0L6_2atmpS2290;
          moonbit_decref(_M0L14_2acurr__entryS701);
          moonbit_incref(_M0L4selfS692);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS692);
          _M0L14capacity__maskS2291 = _M0L4selfS692->$3;
          _M0L6_2atmpS2290 = _M0L4hashS694 & _M0L14capacity__maskS2291;
          _M0L3pslS689 = 0;
          _M0L3idxS690 = _M0L6_2atmpS2290;
          continue;
        }
        moonbit_incref(_M0L4selfS692);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS692, _M0L3idxS690, _M0L14_2acurr__entryS701);
        _M0L7_2abindS702 = _M0L4selfS692->$6;
        _M0L7_2abindS703 = 0;
        _M0L5entryS704
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS704)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS704->$0 = _M0L7_2abindS702;
        _M0L5entryS704->$1 = _M0L7_2abindS703;
        _M0L5entryS704->$2 = _M0L3pslS689;
        _M0L5entryS704->$3 = _M0L4hashS694;
        _M0L5entryS704->$4 = _M0L3keyS698;
        _M0L5entryS704->$5 = _M0L5valueS699;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS692, _M0L3idxS690, _M0L5entryS704);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS701);
      }
      _M0L6_2atmpS2292 = _M0L3pslS689 + 1;
      _M0L6_2atmpS2294 = _M0L3idxS690 + 1;
      _M0L14capacity__maskS2295 = _M0L4selfS692->$3;
      _M0L6_2atmpS2293 = _M0L6_2atmpS2294 & _M0L14capacity__maskS2295;
      _M0L3pslS689 = _M0L6_2atmpS2292;
      _M0L3idxS690 = _M0L6_2atmpS2293;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS708,
  moonbit_string_t _M0L3keyS714,
  struct _M0TPC16string10StringView _M0L5valueS715,
  int32_t _M0L4hashS710
) {
  int32_t _M0L14capacity__maskS2316;
  int32_t _M0L6_2atmpS2315;
  int32_t _M0L3pslS705;
  int32_t _M0L3idxS706;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2316 = _M0L4selfS708->$3;
  _M0L6_2atmpS2315 = _M0L4hashS710 & _M0L14capacity__maskS2316;
  _M0L3pslS705 = 0;
  _M0L3idxS706 = _M0L6_2atmpS2315;
  while (1) {
    struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L8_2afieldS3628 =
      _M0L4selfS708->$0;
    struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L7entriesS2314 =
      _M0L8_2afieldS3628;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS3627;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS707;
    if (
      _M0L3idxS706 < 0
      || _M0L3idxS706 >= Moonbit_array_length(_M0L7entriesS2314)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3627
    = (struct _M0TPB5EntryGsRPC16string10StringViewE*)_M0L7entriesS2314[
        _M0L3idxS706
      ];
    _M0L7_2abindS707 = _M0L6_2atmpS3627;
    if (_M0L7_2abindS707 == 0) {
      int32_t _M0L4sizeS2299 = _M0L4selfS708->$1;
      int32_t _M0L8grow__atS2300 = _M0L4selfS708->$4;
      int32_t _M0L7_2abindS711;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS712;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L5entryS713;
      if (_M0L4sizeS2299 >= _M0L8grow__atS2300) {
        int32_t _M0L14capacity__maskS2302;
        int32_t _M0L6_2atmpS2301;
        moonbit_incref(_M0L4selfS708);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPC16string10StringViewE(_M0L4selfS708);
        _M0L14capacity__maskS2302 = _M0L4selfS708->$3;
        _M0L6_2atmpS2301 = _M0L4hashS710 & _M0L14capacity__maskS2302;
        _M0L3pslS705 = 0;
        _M0L3idxS706 = _M0L6_2atmpS2301;
        continue;
      }
      _M0L7_2abindS711 = _M0L4selfS708->$6;
      _M0L7_2abindS712 = 0;
      _M0L5entryS713
      = (struct _M0TPB5EntryGsRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPC16string10StringViewE));
      Moonbit_object_header(_M0L5entryS713)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPC16string10StringViewE, $1) >> 2, 3, 0);
      _M0L5entryS713->$0 = _M0L7_2abindS711;
      _M0L5entryS713->$1 = _M0L7_2abindS712;
      _M0L5entryS713->$2 = _M0L3pslS705;
      _M0L5entryS713->$3 = _M0L4hashS710;
      _M0L5entryS713->$4 = _M0L3keyS714;
      _M0L5entryS713->$5_0 = _M0L5valueS715.$0;
      _M0L5entryS713->$5_1 = _M0L5valueS715.$1;
      _M0L5entryS713->$5_2 = _M0L5valueS715.$2;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPC16string10StringViewE(_M0L4selfS708, _M0L3idxS706, _M0L5entryS713);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2aSomeS716 =
        _M0L7_2abindS707;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L14_2acurr__entryS717 =
        _M0L7_2aSomeS716;
      int32_t _M0L4hashS2304 = _M0L14_2acurr__entryS717->$3;
      int32_t _if__result_3988;
      int32_t _M0L3pslS2305;
      int32_t _M0L6_2atmpS2310;
      int32_t _M0L6_2atmpS2312;
      int32_t _M0L14capacity__maskS2313;
      int32_t _M0L6_2atmpS2311;
      if (_M0L4hashS2304 == _M0L4hashS710) {
        moonbit_string_t _M0L8_2afieldS3626 = _M0L14_2acurr__entryS717->$4;
        moonbit_string_t _M0L3keyS2303 = _M0L8_2afieldS3626;
        int32_t _M0L6_2atmpS3625;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3625
        = moonbit_val_array_equal(_M0L3keyS2303, _M0L3keyS714);
        _if__result_3988 = _M0L6_2atmpS3625;
      } else {
        _if__result_3988 = 0;
      }
      if (_if__result_3988) {
        struct _M0TPC16string10StringView _M0L6_2aoldS3624;
        moonbit_incref(_M0L14_2acurr__entryS717);
        moonbit_decref(_M0L3keyS714);
        moonbit_decref(_M0L4selfS708);
        _M0L6_2aoldS3624
        = (struct _M0TPC16string10StringView){
          _M0L14_2acurr__entryS717->$5_1,
            _M0L14_2acurr__entryS717->$5_2,
            _M0L14_2acurr__entryS717->$5_0
        };
        moonbit_decref(_M0L6_2aoldS3624.$0);
        _M0L14_2acurr__entryS717->$5_0 = _M0L5valueS715.$0;
        _M0L14_2acurr__entryS717->$5_1 = _M0L5valueS715.$1;
        _M0L14_2acurr__entryS717->$5_2 = _M0L5valueS715.$2;
        moonbit_decref(_M0L14_2acurr__entryS717);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS717);
      }
      _M0L3pslS2305 = _M0L14_2acurr__entryS717->$2;
      if (_M0L3pslS705 > _M0L3pslS2305) {
        int32_t _M0L4sizeS2306 = _M0L4selfS708->$1;
        int32_t _M0L8grow__atS2307 = _M0L4selfS708->$4;
        int32_t _M0L7_2abindS718;
        struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS719;
        struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L5entryS720;
        if (_M0L4sizeS2306 >= _M0L8grow__atS2307) {
          int32_t _M0L14capacity__maskS2309;
          int32_t _M0L6_2atmpS2308;
          moonbit_decref(_M0L14_2acurr__entryS717);
          moonbit_incref(_M0L4selfS708);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPC16string10StringViewE(_M0L4selfS708);
          _M0L14capacity__maskS2309 = _M0L4selfS708->$3;
          _M0L6_2atmpS2308 = _M0L4hashS710 & _M0L14capacity__maskS2309;
          _M0L3pslS705 = 0;
          _M0L3idxS706 = _M0L6_2atmpS2308;
          continue;
        }
        moonbit_incref(_M0L4selfS708);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPC16string10StringViewE(_M0L4selfS708, _M0L3idxS706, _M0L14_2acurr__entryS717);
        _M0L7_2abindS718 = _M0L4selfS708->$6;
        _M0L7_2abindS719 = 0;
        _M0L5entryS720
        = (struct _M0TPB5EntryGsRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPC16string10StringViewE));
        Moonbit_object_header(_M0L5entryS720)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPC16string10StringViewE, $1) >> 2, 3, 0);
        _M0L5entryS720->$0 = _M0L7_2abindS718;
        _M0L5entryS720->$1 = _M0L7_2abindS719;
        _M0L5entryS720->$2 = _M0L3pslS705;
        _M0L5entryS720->$3 = _M0L4hashS710;
        _M0L5entryS720->$4 = _M0L3keyS714;
        _M0L5entryS720->$5_0 = _M0L5valueS715.$0;
        _M0L5entryS720->$5_1 = _M0L5valueS715.$1;
        _M0L5entryS720->$5_2 = _M0L5valueS715.$2;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPC16string10StringViewE(_M0L4selfS708, _M0L3idxS706, _M0L5entryS720);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS717);
      }
      _M0L6_2atmpS2310 = _M0L3pslS705 + 1;
      _M0L6_2atmpS2312 = _M0L3idxS706 + 1;
      _M0L14capacity__maskS2313 = _M0L4selfS708->$3;
      _M0L6_2atmpS2311 = _M0L6_2atmpS2312 & _M0L14capacity__maskS2313;
      _M0L3pslS705 = _M0L6_2atmpS2310;
      _M0L3idxS706 = _M0L6_2atmpS2311;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS724,
  moonbit_string_t _M0L3keyS730,
  void* _M0L5valueS731,
  int32_t _M0L4hashS726
) {
  int32_t _M0L14capacity__maskS2334;
  int32_t _M0L6_2atmpS2333;
  int32_t _M0L3pslS721;
  int32_t _M0L3idxS722;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2334 = _M0L4selfS724->$3;
  _M0L6_2atmpS2333 = _M0L4hashS726 & _M0L14capacity__maskS2334;
  _M0L3pslS721 = 0;
  _M0L3idxS722 = _M0L6_2atmpS2333;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3633 = _M0L4selfS724->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2332 = _M0L8_2afieldS3633;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3632;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS723;
    if (
      _M0L3idxS722 < 0
      || _M0L3idxS722 >= Moonbit_array_length(_M0L7entriesS2332)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3632
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2332[_M0L3idxS722];
    _M0L7_2abindS723 = _M0L6_2atmpS3632;
    if (_M0L7_2abindS723 == 0) {
      int32_t _M0L4sizeS2317 = _M0L4selfS724->$1;
      int32_t _M0L8grow__atS2318 = _M0L4selfS724->$4;
      int32_t _M0L7_2abindS727;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS728;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS729;
      if (_M0L4sizeS2317 >= _M0L8grow__atS2318) {
        int32_t _M0L14capacity__maskS2320;
        int32_t _M0L6_2atmpS2319;
        moonbit_incref(_M0L4selfS724);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS724);
        _M0L14capacity__maskS2320 = _M0L4selfS724->$3;
        _M0L6_2atmpS2319 = _M0L4hashS726 & _M0L14capacity__maskS2320;
        _M0L3pslS721 = 0;
        _M0L3idxS722 = _M0L6_2atmpS2319;
        continue;
      }
      _M0L7_2abindS727 = _M0L4selfS724->$6;
      _M0L7_2abindS728 = 0;
      _M0L5entryS729
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS729)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS729->$0 = _M0L7_2abindS727;
      _M0L5entryS729->$1 = _M0L7_2abindS728;
      _M0L5entryS729->$2 = _M0L3pslS721;
      _M0L5entryS729->$3 = _M0L4hashS726;
      _M0L5entryS729->$4 = _M0L3keyS730;
      _M0L5entryS729->$5 = _M0L5valueS731;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS724, _M0L3idxS722, _M0L5entryS729);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS732 = _M0L7_2abindS723;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS733 =
        _M0L7_2aSomeS732;
      int32_t _M0L4hashS2322 = _M0L14_2acurr__entryS733->$3;
      int32_t _if__result_3990;
      int32_t _M0L3pslS2323;
      int32_t _M0L6_2atmpS2328;
      int32_t _M0L6_2atmpS2330;
      int32_t _M0L14capacity__maskS2331;
      int32_t _M0L6_2atmpS2329;
      if (_M0L4hashS2322 == _M0L4hashS726) {
        moonbit_string_t _M0L8_2afieldS3631 = _M0L14_2acurr__entryS733->$4;
        moonbit_string_t _M0L3keyS2321 = _M0L8_2afieldS3631;
        int32_t _M0L6_2atmpS3630;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3630
        = moonbit_val_array_equal(_M0L3keyS2321, _M0L3keyS730);
        _if__result_3990 = _M0L6_2atmpS3630;
      } else {
        _if__result_3990 = 0;
      }
      if (_if__result_3990) {
        void* _M0L6_2aoldS3629;
        moonbit_incref(_M0L14_2acurr__entryS733);
        moonbit_decref(_M0L3keyS730);
        moonbit_decref(_M0L4selfS724);
        _M0L6_2aoldS3629 = _M0L14_2acurr__entryS733->$5;
        moonbit_decref(_M0L6_2aoldS3629);
        _M0L14_2acurr__entryS733->$5 = _M0L5valueS731;
        moonbit_decref(_M0L14_2acurr__entryS733);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS733);
      }
      _M0L3pslS2323 = _M0L14_2acurr__entryS733->$2;
      if (_M0L3pslS721 > _M0L3pslS2323) {
        int32_t _M0L4sizeS2324 = _M0L4selfS724->$1;
        int32_t _M0L8grow__atS2325 = _M0L4selfS724->$4;
        int32_t _M0L7_2abindS734;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS735;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS736;
        if (_M0L4sizeS2324 >= _M0L8grow__atS2325) {
          int32_t _M0L14capacity__maskS2327;
          int32_t _M0L6_2atmpS2326;
          moonbit_decref(_M0L14_2acurr__entryS733);
          moonbit_incref(_M0L4selfS724);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS724);
          _M0L14capacity__maskS2327 = _M0L4selfS724->$3;
          _M0L6_2atmpS2326 = _M0L4hashS726 & _M0L14capacity__maskS2327;
          _M0L3pslS721 = 0;
          _M0L3idxS722 = _M0L6_2atmpS2326;
          continue;
        }
        moonbit_incref(_M0L4selfS724);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS724, _M0L3idxS722, _M0L14_2acurr__entryS733);
        _M0L7_2abindS734 = _M0L4selfS724->$6;
        _M0L7_2abindS735 = 0;
        _M0L5entryS736
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS736)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS736->$0 = _M0L7_2abindS734;
        _M0L5entryS736->$1 = _M0L7_2abindS735;
        _M0L5entryS736->$2 = _M0L3pslS721;
        _M0L5entryS736->$3 = _M0L4hashS726;
        _M0L5entryS736->$4 = _M0L3keyS730;
        _M0L5entryS736->$5 = _M0L5valueS731;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS724, _M0L3idxS722, _M0L5entryS736);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS733);
      }
      _M0L6_2atmpS2328 = _M0L3pslS721 + 1;
      _M0L6_2atmpS2330 = _M0L3idxS722 + 1;
      _M0L14capacity__maskS2331 = _M0L4selfS724->$3;
      _M0L6_2atmpS2329 = _M0L6_2atmpS2330 & _M0L14capacity__maskS2331;
      _M0L3pslS721 = _M0L6_2atmpS2328;
      _M0L3idxS722 = _M0L6_2atmpS2329;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS637,
  int32_t _M0L3idxS642,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS641
) {
  int32_t _M0L3pslS2214;
  int32_t _M0L6_2atmpS2210;
  int32_t _M0L6_2atmpS2212;
  int32_t _M0L14capacity__maskS2213;
  int32_t _M0L6_2atmpS2211;
  int32_t _M0L3pslS633;
  int32_t _M0L3idxS634;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS635;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2214 = _M0L5entryS641->$2;
  _M0L6_2atmpS2210 = _M0L3pslS2214 + 1;
  _M0L6_2atmpS2212 = _M0L3idxS642 + 1;
  _M0L14capacity__maskS2213 = _M0L4selfS637->$3;
  _M0L6_2atmpS2211 = _M0L6_2atmpS2212 & _M0L14capacity__maskS2213;
  _M0L3pslS633 = _M0L6_2atmpS2210;
  _M0L3idxS634 = _M0L6_2atmpS2211;
  _M0L5entryS635 = _M0L5entryS641;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3635 =
      _M0L4selfS637->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2209 =
      _M0L8_2afieldS3635;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3634;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS636;
    if (
      _M0L3idxS634 < 0
      || _M0L3idxS634 >= Moonbit_array_length(_M0L7entriesS2209)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3634
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2209[
        _M0L3idxS634
      ];
    _M0L7_2abindS636 = _M0L6_2atmpS3634;
    if (_M0L7_2abindS636 == 0) {
      _M0L5entryS635->$2 = _M0L3pslS633;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS637, _M0L5entryS635, _M0L3idxS634);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS639 =
        _M0L7_2abindS636;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS640 =
        _M0L7_2aSomeS639;
      int32_t _M0L3pslS2199 = _M0L14_2acurr__entryS640->$2;
      if (_M0L3pslS633 > _M0L3pslS2199) {
        int32_t _M0L3pslS2204;
        int32_t _M0L6_2atmpS2200;
        int32_t _M0L6_2atmpS2202;
        int32_t _M0L14capacity__maskS2203;
        int32_t _M0L6_2atmpS2201;
        _M0L5entryS635->$2 = _M0L3pslS633;
        moonbit_incref(_M0L14_2acurr__entryS640);
        moonbit_incref(_M0L4selfS637);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS637, _M0L5entryS635, _M0L3idxS634);
        _M0L3pslS2204 = _M0L14_2acurr__entryS640->$2;
        _M0L6_2atmpS2200 = _M0L3pslS2204 + 1;
        _M0L6_2atmpS2202 = _M0L3idxS634 + 1;
        _M0L14capacity__maskS2203 = _M0L4selfS637->$3;
        _M0L6_2atmpS2201 = _M0L6_2atmpS2202 & _M0L14capacity__maskS2203;
        _M0L3pslS633 = _M0L6_2atmpS2200;
        _M0L3idxS634 = _M0L6_2atmpS2201;
        _M0L5entryS635 = _M0L14_2acurr__entryS640;
        continue;
      } else {
        int32_t _M0L6_2atmpS2205 = _M0L3pslS633 + 1;
        int32_t _M0L6_2atmpS2207 = _M0L3idxS634 + 1;
        int32_t _M0L14capacity__maskS2208 = _M0L4selfS637->$3;
        int32_t _M0L6_2atmpS2206 =
          _M0L6_2atmpS2207 & _M0L14capacity__maskS2208;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3992 =
          _M0L5entryS635;
        _M0L3pslS633 = _M0L6_2atmpS2205;
        _M0L3idxS634 = _M0L6_2atmpS2206;
        _M0L5entryS635 = _tmp_3992;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS647,
  int32_t _M0L3idxS652,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS651
) {
  int32_t _M0L3pslS2230;
  int32_t _M0L6_2atmpS2226;
  int32_t _M0L6_2atmpS2228;
  int32_t _M0L14capacity__maskS2229;
  int32_t _M0L6_2atmpS2227;
  int32_t _M0L3pslS643;
  int32_t _M0L3idxS644;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS645;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2230 = _M0L5entryS651->$2;
  _M0L6_2atmpS2226 = _M0L3pslS2230 + 1;
  _M0L6_2atmpS2228 = _M0L3idxS652 + 1;
  _M0L14capacity__maskS2229 = _M0L4selfS647->$3;
  _M0L6_2atmpS2227 = _M0L6_2atmpS2228 & _M0L14capacity__maskS2229;
  _M0L3pslS643 = _M0L6_2atmpS2226;
  _M0L3idxS644 = _M0L6_2atmpS2227;
  _M0L5entryS645 = _M0L5entryS651;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3637 =
      _M0L4selfS647->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2225 =
      _M0L8_2afieldS3637;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3636;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS646;
    if (
      _M0L3idxS644 < 0
      || _M0L3idxS644 >= Moonbit_array_length(_M0L7entriesS2225)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3636
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2225[
        _M0L3idxS644
      ];
    _M0L7_2abindS646 = _M0L6_2atmpS3636;
    if (_M0L7_2abindS646 == 0) {
      _M0L5entryS645->$2 = _M0L3pslS643;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS647, _M0L5entryS645, _M0L3idxS644);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS649 =
        _M0L7_2abindS646;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS650 =
        _M0L7_2aSomeS649;
      int32_t _M0L3pslS2215 = _M0L14_2acurr__entryS650->$2;
      if (_M0L3pslS643 > _M0L3pslS2215) {
        int32_t _M0L3pslS2220;
        int32_t _M0L6_2atmpS2216;
        int32_t _M0L6_2atmpS2218;
        int32_t _M0L14capacity__maskS2219;
        int32_t _M0L6_2atmpS2217;
        _M0L5entryS645->$2 = _M0L3pslS643;
        moonbit_incref(_M0L14_2acurr__entryS650);
        moonbit_incref(_M0L4selfS647);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS647, _M0L5entryS645, _M0L3idxS644);
        _M0L3pslS2220 = _M0L14_2acurr__entryS650->$2;
        _M0L6_2atmpS2216 = _M0L3pslS2220 + 1;
        _M0L6_2atmpS2218 = _M0L3idxS644 + 1;
        _M0L14capacity__maskS2219 = _M0L4selfS647->$3;
        _M0L6_2atmpS2217 = _M0L6_2atmpS2218 & _M0L14capacity__maskS2219;
        _M0L3pslS643 = _M0L6_2atmpS2216;
        _M0L3idxS644 = _M0L6_2atmpS2217;
        _M0L5entryS645 = _M0L14_2acurr__entryS650;
        continue;
      } else {
        int32_t _M0L6_2atmpS2221 = _M0L3pslS643 + 1;
        int32_t _M0L6_2atmpS2223 = _M0L3idxS644 + 1;
        int32_t _M0L14capacity__maskS2224 = _M0L4selfS647->$3;
        int32_t _M0L6_2atmpS2222 =
          _M0L6_2atmpS2223 & _M0L14capacity__maskS2224;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3994 =
          _M0L5entryS645;
        _M0L3pslS643 = _M0L6_2atmpS2221;
        _M0L3idxS644 = _M0L6_2atmpS2222;
        _M0L5entryS645 = _tmp_3994;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS657,
  int32_t _M0L3idxS662,
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L5entryS661
) {
  int32_t _M0L3pslS2246;
  int32_t _M0L6_2atmpS2242;
  int32_t _M0L6_2atmpS2244;
  int32_t _M0L14capacity__maskS2245;
  int32_t _M0L6_2atmpS2243;
  int32_t _M0L3pslS653;
  int32_t _M0L3idxS654;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L5entryS655;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2246 = _M0L5entryS661->$2;
  _M0L6_2atmpS2242 = _M0L3pslS2246 + 1;
  _M0L6_2atmpS2244 = _M0L3idxS662 + 1;
  _M0L14capacity__maskS2245 = _M0L4selfS657->$3;
  _M0L6_2atmpS2243 = _M0L6_2atmpS2244 & _M0L14capacity__maskS2245;
  _M0L3pslS653 = _M0L6_2atmpS2242;
  _M0L3idxS654 = _M0L6_2atmpS2243;
  _M0L5entryS655 = _M0L5entryS661;
  while (1) {
    struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L8_2afieldS3639 =
      _M0L4selfS657->$0;
    struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L7entriesS2241 =
      _M0L8_2afieldS3639;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS3638;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS656;
    if (
      _M0L3idxS654 < 0
      || _M0L3idxS654 >= Moonbit_array_length(_M0L7entriesS2241)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3638
    = (struct _M0TPB5EntryGsRPC16string10StringViewE*)_M0L7entriesS2241[
        _M0L3idxS654
      ];
    _M0L7_2abindS656 = _M0L6_2atmpS3638;
    if (_M0L7_2abindS656 == 0) {
      _M0L5entryS655->$2 = _M0L3pslS653;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPC16string10StringViewE(_M0L4selfS657, _M0L5entryS655, _M0L3idxS654);
      break;
    } else {
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2aSomeS659 =
        _M0L7_2abindS656;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L14_2acurr__entryS660 =
        _M0L7_2aSomeS659;
      int32_t _M0L3pslS2231 = _M0L14_2acurr__entryS660->$2;
      if (_M0L3pslS653 > _M0L3pslS2231) {
        int32_t _M0L3pslS2236;
        int32_t _M0L6_2atmpS2232;
        int32_t _M0L6_2atmpS2234;
        int32_t _M0L14capacity__maskS2235;
        int32_t _M0L6_2atmpS2233;
        _M0L5entryS655->$2 = _M0L3pslS653;
        moonbit_incref(_M0L14_2acurr__entryS660);
        moonbit_incref(_M0L4selfS657);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPC16string10StringViewE(_M0L4selfS657, _M0L5entryS655, _M0L3idxS654);
        _M0L3pslS2236 = _M0L14_2acurr__entryS660->$2;
        _M0L6_2atmpS2232 = _M0L3pslS2236 + 1;
        _M0L6_2atmpS2234 = _M0L3idxS654 + 1;
        _M0L14capacity__maskS2235 = _M0L4selfS657->$3;
        _M0L6_2atmpS2233 = _M0L6_2atmpS2234 & _M0L14capacity__maskS2235;
        _M0L3pslS653 = _M0L6_2atmpS2232;
        _M0L3idxS654 = _M0L6_2atmpS2233;
        _M0L5entryS655 = _M0L14_2acurr__entryS660;
        continue;
      } else {
        int32_t _M0L6_2atmpS2237 = _M0L3pslS653 + 1;
        int32_t _M0L6_2atmpS2239 = _M0L3idxS654 + 1;
        int32_t _M0L14capacity__maskS2240 = _M0L4selfS657->$3;
        int32_t _M0L6_2atmpS2238 =
          _M0L6_2atmpS2239 & _M0L14capacity__maskS2240;
        struct _M0TPB5EntryGsRPC16string10StringViewE* _tmp_3996 =
          _M0L5entryS655;
        _M0L3pslS653 = _M0L6_2atmpS2237;
        _M0L3idxS654 = _M0L6_2atmpS2238;
        _M0L5entryS655 = _tmp_3996;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS667,
  int32_t _M0L3idxS672,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS671
) {
  int32_t _M0L3pslS2262;
  int32_t _M0L6_2atmpS2258;
  int32_t _M0L6_2atmpS2260;
  int32_t _M0L14capacity__maskS2261;
  int32_t _M0L6_2atmpS2259;
  int32_t _M0L3pslS663;
  int32_t _M0L3idxS664;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS665;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2262 = _M0L5entryS671->$2;
  _M0L6_2atmpS2258 = _M0L3pslS2262 + 1;
  _M0L6_2atmpS2260 = _M0L3idxS672 + 1;
  _M0L14capacity__maskS2261 = _M0L4selfS667->$3;
  _M0L6_2atmpS2259 = _M0L6_2atmpS2260 & _M0L14capacity__maskS2261;
  _M0L3pslS663 = _M0L6_2atmpS2258;
  _M0L3idxS664 = _M0L6_2atmpS2259;
  _M0L5entryS665 = _M0L5entryS671;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3641 = _M0L4selfS667->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2257 = _M0L8_2afieldS3641;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3640;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS666;
    if (
      _M0L3idxS664 < 0
      || _M0L3idxS664 >= Moonbit_array_length(_M0L7entriesS2257)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3640
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2257[_M0L3idxS664];
    _M0L7_2abindS666 = _M0L6_2atmpS3640;
    if (_M0L7_2abindS666 == 0) {
      _M0L5entryS665->$2 = _M0L3pslS663;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS667, _M0L5entryS665, _M0L3idxS664);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS669 = _M0L7_2abindS666;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS670 =
        _M0L7_2aSomeS669;
      int32_t _M0L3pslS2247 = _M0L14_2acurr__entryS670->$2;
      if (_M0L3pslS663 > _M0L3pslS2247) {
        int32_t _M0L3pslS2252;
        int32_t _M0L6_2atmpS2248;
        int32_t _M0L6_2atmpS2250;
        int32_t _M0L14capacity__maskS2251;
        int32_t _M0L6_2atmpS2249;
        _M0L5entryS665->$2 = _M0L3pslS663;
        moonbit_incref(_M0L14_2acurr__entryS670);
        moonbit_incref(_M0L4selfS667);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS667, _M0L5entryS665, _M0L3idxS664);
        _M0L3pslS2252 = _M0L14_2acurr__entryS670->$2;
        _M0L6_2atmpS2248 = _M0L3pslS2252 + 1;
        _M0L6_2atmpS2250 = _M0L3idxS664 + 1;
        _M0L14capacity__maskS2251 = _M0L4selfS667->$3;
        _M0L6_2atmpS2249 = _M0L6_2atmpS2250 & _M0L14capacity__maskS2251;
        _M0L3pslS663 = _M0L6_2atmpS2248;
        _M0L3idxS664 = _M0L6_2atmpS2249;
        _M0L5entryS665 = _M0L14_2acurr__entryS670;
        continue;
      } else {
        int32_t _M0L6_2atmpS2253 = _M0L3pslS663 + 1;
        int32_t _M0L6_2atmpS2255 = _M0L3idxS664 + 1;
        int32_t _M0L14capacity__maskS2256 = _M0L4selfS667->$3;
        int32_t _M0L6_2atmpS2254 =
          _M0L6_2atmpS2255 & _M0L14capacity__maskS2256;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_3998 = _M0L5entryS665;
        _M0L3pslS663 = _M0L6_2atmpS2253;
        _M0L3idxS664 = _M0L6_2atmpS2254;
        _M0L5entryS665 = _tmp_3998;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS609,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS611,
  int32_t _M0L8new__idxS610
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3644;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2191;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2192;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3643;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3642;
  int32_t _M0L6_2acntS3845;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS612;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3644 = _M0L4selfS609->$0;
  _M0L7entriesS2191 = _M0L8_2afieldS3644;
  moonbit_incref(_M0L5entryS611);
  _M0L6_2atmpS2192 = _M0L5entryS611;
  if (
    _M0L8new__idxS610 < 0
    || _M0L8new__idxS610 >= Moonbit_array_length(_M0L7entriesS2191)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3643
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2191[
      _M0L8new__idxS610
    ];
  if (_M0L6_2aoldS3643) {
    moonbit_decref(_M0L6_2aoldS3643);
  }
  _M0L7entriesS2191[_M0L8new__idxS610] = _M0L6_2atmpS2192;
  _M0L8_2afieldS3642 = _M0L5entryS611->$1;
  _M0L6_2acntS3845 = Moonbit_object_header(_M0L5entryS611)->rc;
  if (_M0L6_2acntS3845 > 1) {
    int32_t _M0L11_2anew__cntS3848 = _M0L6_2acntS3845 - 1;
    Moonbit_object_header(_M0L5entryS611)->rc = _M0L11_2anew__cntS3848;
    if (_M0L8_2afieldS3642) {
      moonbit_incref(_M0L8_2afieldS3642);
    }
  } else if (_M0L6_2acntS3845 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3847 =
      _M0L5entryS611->$5;
    moonbit_string_t _M0L8_2afieldS3846;
    moonbit_decref(_M0L8_2afieldS3847);
    _M0L8_2afieldS3846 = _M0L5entryS611->$4;
    moonbit_decref(_M0L8_2afieldS3846);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS611);
  }
  _M0L7_2abindS612 = _M0L8_2afieldS3642;
  if (_M0L7_2abindS612 == 0) {
    if (_M0L7_2abindS612) {
      moonbit_decref(_M0L7_2abindS612);
    }
    _M0L4selfS609->$6 = _M0L8new__idxS610;
    moonbit_decref(_M0L4selfS609);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS613;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS614;
    moonbit_decref(_M0L4selfS609);
    _M0L7_2aSomeS613 = _M0L7_2abindS612;
    _M0L7_2anextS614 = _M0L7_2aSomeS613;
    _M0L7_2anextS614->$0 = _M0L8new__idxS610;
    moonbit_decref(_M0L7_2anextS614);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS615,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS617,
  int32_t _M0L8new__idxS616
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3647;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2193;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2194;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3646;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3645;
  int32_t _M0L6_2acntS3849;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS618;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3647 = _M0L4selfS615->$0;
  _M0L7entriesS2193 = _M0L8_2afieldS3647;
  moonbit_incref(_M0L5entryS617);
  _M0L6_2atmpS2194 = _M0L5entryS617;
  if (
    _M0L8new__idxS616 < 0
    || _M0L8new__idxS616 >= Moonbit_array_length(_M0L7entriesS2193)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3646
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2193[
      _M0L8new__idxS616
    ];
  if (_M0L6_2aoldS3646) {
    moonbit_decref(_M0L6_2aoldS3646);
  }
  _M0L7entriesS2193[_M0L8new__idxS616] = _M0L6_2atmpS2194;
  _M0L8_2afieldS3645 = _M0L5entryS617->$1;
  _M0L6_2acntS3849 = Moonbit_object_header(_M0L5entryS617)->rc;
  if (_M0L6_2acntS3849 > 1) {
    int32_t _M0L11_2anew__cntS3851 = _M0L6_2acntS3849 - 1;
    Moonbit_object_header(_M0L5entryS617)->rc = _M0L11_2anew__cntS3851;
    if (_M0L8_2afieldS3645) {
      moonbit_incref(_M0L8_2afieldS3645);
    }
  } else if (_M0L6_2acntS3849 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3850 =
      _M0L5entryS617->$5;
    moonbit_decref(_M0L8_2afieldS3850);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS617);
  }
  _M0L7_2abindS618 = _M0L8_2afieldS3645;
  if (_M0L7_2abindS618 == 0) {
    if (_M0L7_2abindS618) {
      moonbit_decref(_M0L7_2abindS618);
    }
    _M0L4selfS615->$6 = _M0L8new__idxS616;
    moonbit_decref(_M0L4selfS615);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS619;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS620;
    moonbit_decref(_M0L4selfS615);
    _M0L7_2aSomeS619 = _M0L7_2abindS618;
    _M0L7_2anextS620 = _M0L7_2aSomeS619;
    _M0L7_2anextS620->$0 = _M0L8new__idxS616;
    moonbit_decref(_M0L7_2anextS620);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS621,
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L5entryS623,
  int32_t _M0L8new__idxS622
) {
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L8_2afieldS3650;
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L7entriesS2195;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2196;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2aoldS3649;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L8_2afieldS3648;
  int32_t _M0L6_2acntS3852;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS624;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3650 = _M0L4selfS621->$0;
  _M0L7entriesS2195 = _M0L8_2afieldS3650;
  moonbit_incref(_M0L5entryS623);
  _M0L6_2atmpS2196 = _M0L5entryS623;
  if (
    _M0L8new__idxS622 < 0
    || _M0L8new__idxS622 >= Moonbit_array_length(_M0L7entriesS2195)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3649
  = (struct _M0TPB5EntryGsRPC16string10StringViewE*)_M0L7entriesS2195[
      _M0L8new__idxS622
    ];
  if (_M0L6_2aoldS3649) {
    moonbit_decref(_M0L6_2aoldS3649);
  }
  _M0L7entriesS2195[_M0L8new__idxS622] = _M0L6_2atmpS2196;
  _M0L8_2afieldS3648 = _M0L5entryS623->$1;
  _M0L6_2acntS3852 = Moonbit_object_header(_M0L5entryS623)->rc;
  if (_M0L6_2acntS3852 > 1) {
    int32_t _M0L11_2anew__cntS3855 = _M0L6_2acntS3852 - 1;
    Moonbit_object_header(_M0L5entryS623)->rc = _M0L11_2anew__cntS3855;
    if (_M0L8_2afieldS3648) {
      moonbit_incref(_M0L8_2afieldS3648);
    }
  } else if (_M0L6_2acntS3852 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3854 =
      (struct _M0TPC16string10StringView){_M0L5entryS623->$5_1,
                                            _M0L5entryS623->$5_2,
                                            _M0L5entryS623->$5_0};
    moonbit_string_t _M0L8_2afieldS3853;
    moonbit_decref(_M0L8_2afieldS3854.$0);
    _M0L8_2afieldS3853 = _M0L5entryS623->$4;
    moonbit_decref(_M0L8_2afieldS3853);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS623);
  }
  _M0L7_2abindS624 = _M0L8_2afieldS3648;
  if (_M0L7_2abindS624 == 0) {
    if (_M0L7_2abindS624) {
      moonbit_decref(_M0L7_2abindS624);
    }
    _M0L4selfS621->$6 = _M0L8new__idxS622;
    moonbit_decref(_M0L4selfS621);
  } else {
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2aSomeS625;
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2anextS626;
    moonbit_decref(_M0L4selfS621);
    _M0L7_2aSomeS625 = _M0L7_2abindS624;
    _M0L7_2anextS626 = _M0L7_2aSomeS625;
    _M0L7_2anextS626->$0 = _M0L8new__idxS622;
    moonbit_decref(_M0L7_2anextS626);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS627,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS629,
  int32_t _M0L8new__idxS628
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3653;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2197;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2198;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3652;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3651;
  int32_t _M0L6_2acntS3856;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS630;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3653 = _M0L4selfS627->$0;
  _M0L7entriesS2197 = _M0L8_2afieldS3653;
  moonbit_incref(_M0L5entryS629);
  _M0L6_2atmpS2198 = _M0L5entryS629;
  if (
    _M0L8new__idxS628 < 0
    || _M0L8new__idxS628 >= Moonbit_array_length(_M0L7entriesS2197)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3652
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2197[_M0L8new__idxS628];
  if (_M0L6_2aoldS3652) {
    moonbit_decref(_M0L6_2aoldS3652);
  }
  _M0L7entriesS2197[_M0L8new__idxS628] = _M0L6_2atmpS2198;
  _M0L8_2afieldS3651 = _M0L5entryS629->$1;
  _M0L6_2acntS3856 = Moonbit_object_header(_M0L5entryS629)->rc;
  if (_M0L6_2acntS3856 > 1) {
    int32_t _M0L11_2anew__cntS3859 = _M0L6_2acntS3856 - 1;
    Moonbit_object_header(_M0L5entryS629)->rc = _M0L11_2anew__cntS3859;
    if (_M0L8_2afieldS3651) {
      moonbit_incref(_M0L8_2afieldS3651);
    }
  } else if (_M0L6_2acntS3856 == 1) {
    void* _M0L8_2afieldS3858 = _M0L5entryS629->$5;
    moonbit_string_t _M0L8_2afieldS3857;
    moonbit_decref(_M0L8_2afieldS3858);
    _M0L8_2afieldS3857 = _M0L5entryS629->$4;
    moonbit_decref(_M0L8_2afieldS3857);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS629);
  }
  _M0L7_2abindS630 = _M0L8_2afieldS3651;
  if (_M0L7_2abindS630 == 0) {
    if (_M0L7_2abindS630) {
      moonbit_decref(_M0L7_2abindS630);
    }
    _M0L4selfS627->$6 = _M0L8new__idxS628;
    moonbit_decref(_M0L4selfS627);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS631;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS632;
    moonbit_decref(_M0L4selfS627);
    _M0L7_2aSomeS631 = _M0L7_2abindS630;
    _M0L7_2anextS632 = _M0L7_2aSomeS631;
    _M0L7_2anextS632->$0 = _M0L8new__idxS628;
    moonbit_decref(_M0L7_2anextS632);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS594,
  int32_t _M0L3idxS596,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS595
) {
  int32_t _M0L7_2abindS593;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3655;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2160;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2161;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3654;
  int32_t _M0L4sizeS2163;
  int32_t _M0L6_2atmpS2162;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS593 = _M0L4selfS594->$6;
  switch (_M0L7_2abindS593) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2155;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3656;
      moonbit_incref(_M0L5entryS595);
      _M0L6_2atmpS2155 = _M0L5entryS595;
      _M0L6_2aoldS3656 = _M0L4selfS594->$5;
      if (_M0L6_2aoldS3656) {
        moonbit_decref(_M0L6_2aoldS3656);
      }
      _M0L4selfS594->$5 = _M0L6_2atmpS2155;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3659 =
        _M0L4selfS594->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2159 =
        _M0L8_2afieldS3659;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3658;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2158;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2156;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2157;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3657;
      if (
        _M0L7_2abindS593 < 0
        || _M0L7_2abindS593 >= Moonbit_array_length(_M0L7entriesS2159)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3658
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2159[
          _M0L7_2abindS593
        ];
      _M0L6_2atmpS2158 = _M0L6_2atmpS3658;
      if (_M0L6_2atmpS2158) {
        moonbit_incref(_M0L6_2atmpS2158);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2156
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2158);
      moonbit_incref(_M0L5entryS595);
      _M0L6_2atmpS2157 = _M0L5entryS595;
      _M0L6_2aoldS3657 = _M0L6_2atmpS2156->$1;
      if (_M0L6_2aoldS3657) {
        moonbit_decref(_M0L6_2aoldS3657);
      }
      _M0L6_2atmpS2156->$1 = _M0L6_2atmpS2157;
      moonbit_decref(_M0L6_2atmpS2156);
      break;
    }
  }
  _M0L4selfS594->$6 = _M0L3idxS596;
  _M0L8_2afieldS3655 = _M0L4selfS594->$0;
  _M0L7entriesS2160 = _M0L8_2afieldS3655;
  _M0L6_2atmpS2161 = _M0L5entryS595;
  if (
    _M0L3idxS596 < 0
    || _M0L3idxS596 >= Moonbit_array_length(_M0L7entriesS2160)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3654
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2160[
      _M0L3idxS596
    ];
  if (_M0L6_2aoldS3654) {
    moonbit_decref(_M0L6_2aoldS3654);
  }
  _M0L7entriesS2160[_M0L3idxS596] = _M0L6_2atmpS2161;
  _M0L4sizeS2163 = _M0L4selfS594->$1;
  _M0L6_2atmpS2162 = _M0L4sizeS2163 + 1;
  _M0L4selfS594->$1 = _M0L6_2atmpS2162;
  moonbit_decref(_M0L4selfS594);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS598,
  int32_t _M0L3idxS600,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS599
) {
  int32_t _M0L7_2abindS597;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3661;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2169;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2170;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3660;
  int32_t _M0L4sizeS2172;
  int32_t _M0L6_2atmpS2171;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS597 = _M0L4selfS598->$6;
  switch (_M0L7_2abindS597) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2164;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3662;
      moonbit_incref(_M0L5entryS599);
      _M0L6_2atmpS2164 = _M0L5entryS599;
      _M0L6_2aoldS3662 = _M0L4selfS598->$5;
      if (_M0L6_2aoldS3662) {
        moonbit_decref(_M0L6_2aoldS3662);
      }
      _M0L4selfS598->$5 = _M0L6_2atmpS2164;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3665 =
        _M0L4selfS598->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2168 =
        _M0L8_2afieldS3665;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3664;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2167;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2165;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2166;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3663;
      if (
        _M0L7_2abindS597 < 0
        || _M0L7_2abindS597 >= Moonbit_array_length(_M0L7entriesS2168)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3664
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2168[
          _M0L7_2abindS597
        ];
      _M0L6_2atmpS2167 = _M0L6_2atmpS3664;
      if (_M0L6_2atmpS2167) {
        moonbit_incref(_M0L6_2atmpS2167);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2165
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2167);
      moonbit_incref(_M0L5entryS599);
      _M0L6_2atmpS2166 = _M0L5entryS599;
      _M0L6_2aoldS3663 = _M0L6_2atmpS2165->$1;
      if (_M0L6_2aoldS3663) {
        moonbit_decref(_M0L6_2aoldS3663);
      }
      _M0L6_2atmpS2165->$1 = _M0L6_2atmpS2166;
      moonbit_decref(_M0L6_2atmpS2165);
      break;
    }
  }
  _M0L4selfS598->$6 = _M0L3idxS600;
  _M0L8_2afieldS3661 = _M0L4selfS598->$0;
  _M0L7entriesS2169 = _M0L8_2afieldS3661;
  _M0L6_2atmpS2170 = _M0L5entryS599;
  if (
    _M0L3idxS600 < 0
    || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS2169)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3660
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2169[
      _M0L3idxS600
    ];
  if (_M0L6_2aoldS3660) {
    moonbit_decref(_M0L6_2aoldS3660);
  }
  _M0L7entriesS2169[_M0L3idxS600] = _M0L6_2atmpS2170;
  _M0L4sizeS2172 = _M0L4selfS598->$1;
  _M0L6_2atmpS2171 = _M0L4sizeS2172 + 1;
  _M0L4selfS598->$1 = _M0L6_2atmpS2171;
  moonbit_decref(_M0L4selfS598);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPC16string10StringViewE(
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L4selfS602,
  int32_t _M0L3idxS604,
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L5entryS603
) {
  int32_t _M0L7_2abindS601;
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L8_2afieldS3667;
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L7entriesS2178;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2179;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2aoldS3666;
  int32_t _M0L4sizeS2181;
  int32_t _M0L6_2atmpS2180;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS601 = _M0L4selfS602->$6;
  switch (_M0L7_2abindS601) {
    case -1: {
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2173;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2aoldS3668;
      moonbit_incref(_M0L5entryS603);
      _M0L6_2atmpS2173 = _M0L5entryS603;
      _M0L6_2aoldS3668 = _M0L4selfS602->$5;
      if (_M0L6_2aoldS3668) {
        moonbit_decref(_M0L6_2aoldS3668);
      }
      _M0L4selfS602->$5 = _M0L6_2atmpS2173;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L8_2afieldS3671 =
        _M0L4selfS602->$0;
      struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L7entriesS2177 =
        _M0L8_2afieldS3671;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS3670;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2176;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2174;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2175;
      struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2aoldS3669;
      if (
        _M0L7_2abindS601 < 0
        || _M0L7_2abindS601 >= Moonbit_array_length(_M0L7entriesS2177)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3670
      = (struct _M0TPB5EntryGsRPC16string10StringViewE*)_M0L7entriesS2177[
          _M0L7_2abindS601
        ];
      _M0L6_2atmpS2176 = _M0L6_2atmpS3670;
      if (_M0L6_2atmpS2176) {
        moonbit_incref(_M0L6_2atmpS2176);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2174
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPC16string10StringViewEE(_M0L6_2atmpS2176);
      moonbit_incref(_M0L5entryS603);
      _M0L6_2atmpS2175 = _M0L5entryS603;
      _M0L6_2aoldS3669 = _M0L6_2atmpS2174->$1;
      if (_M0L6_2aoldS3669) {
        moonbit_decref(_M0L6_2aoldS3669);
      }
      _M0L6_2atmpS2174->$1 = _M0L6_2atmpS2175;
      moonbit_decref(_M0L6_2atmpS2174);
      break;
    }
  }
  _M0L4selfS602->$6 = _M0L3idxS604;
  _M0L8_2afieldS3667 = _M0L4selfS602->$0;
  _M0L7entriesS2178 = _M0L8_2afieldS3667;
  _M0L6_2atmpS2179 = _M0L5entryS603;
  if (
    _M0L3idxS604 < 0
    || _M0L3idxS604 >= Moonbit_array_length(_M0L7entriesS2178)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3666
  = (struct _M0TPB5EntryGsRPC16string10StringViewE*)_M0L7entriesS2178[
      _M0L3idxS604
    ];
  if (_M0L6_2aoldS3666) {
    moonbit_decref(_M0L6_2aoldS3666);
  }
  _M0L7entriesS2178[_M0L3idxS604] = _M0L6_2atmpS2179;
  _M0L4sizeS2181 = _M0L4selfS602->$1;
  _M0L6_2atmpS2180 = _M0L4sizeS2181 + 1;
  _M0L4selfS602->$1 = _M0L6_2atmpS2180;
  moonbit_decref(_M0L4selfS602);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS606,
  int32_t _M0L3idxS608,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS607
) {
  int32_t _M0L7_2abindS605;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3673;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2187;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2188;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3672;
  int32_t _M0L4sizeS2190;
  int32_t _M0L6_2atmpS2189;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS605 = _M0L4selfS606->$6;
  switch (_M0L7_2abindS605) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2182;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3674;
      moonbit_incref(_M0L5entryS607);
      _M0L6_2atmpS2182 = _M0L5entryS607;
      _M0L6_2aoldS3674 = _M0L4selfS606->$5;
      if (_M0L6_2aoldS3674) {
        moonbit_decref(_M0L6_2aoldS3674);
      }
      _M0L4selfS606->$5 = _M0L6_2atmpS2182;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3677 = _M0L4selfS606->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2186 = _M0L8_2afieldS3677;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3676;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2185;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2183;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2184;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3675;
      if (
        _M0L7_2abindS605 < 0
        || _M0L7_2abindS605 >= Moonbit_array_length(_M0L7entriesS2186)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3676
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2186[_M0L7_2abindS605];
      _M0L6_2atmpS2185 = _M0L6_2atmpS3676;
      if (_M0L6_2atmpS2185) {
        moonbit_incref(_M0L6_2atmpS2185);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2183
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2185);
      moonbit_incref(_M0L5entryS607);
      _M0L6_2atmpS2184 = _M0L5entryS607;
      _M0L6_2aoldS3675 = _M0L6_2atmpS2183->$1;
      if (_M0L6_2aoldS3675) {
        moonbit_decref(_M0L6_2aoldS3675);
      }
      _M0L6_2atmpS2183->$1 = _M0L6_2atmpS2184;
      moonbit_decref(_M0L6_2atmpS2183);
      break;
    }
  }
  _M0L4selfS606->$6 = _M0L3idxS608;
  _M0L8_2afieldS3673 = _M0L4selfS606->$0;
  _M0L7entriesS2187 = _M0L8_2afieldS3673;
  _M0L6_2atmpS2188 = _M0L5entryS607;
  if (
    _M0L3idxS608 < 0
    || _M0L3idxS608 >= Moonbit_array_length(_M0L7entriesS2187)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3672
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2187[_M0L3idxS608];
  if (_M0L6_2aoldS3672) {
    moonbit_decref(_M0L6_2aoldS3672);
  }
  _M0L7entriesS2187[_M0L3idxS608] = _M0L6_2atmpS2188;
  _M0L4sizeS2190 = _M0L4selfS606->$1;
  _M0L6_2atmpS2189 = _M0L4sizeS2190 + 1;
  _M0L4selfS606->$1 = _M0L6_2atmpS2189;
  moonbit_decref(_M0L4selfS606);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS570
) {
  int32_t _M0L8capacityS569;
  int32_t _M0L7_2abindS571;
  int32_t _M0L7_2abindS572;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2151;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS573;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS574;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3999;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS569
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS570);
  _M0L7_2abindS571 = _M0L8capacityS569 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS572 = _M0FPB21calc__grow__threshold(_M0L8capacityS569);
  _M0L6_2atmpS2151 = 0;
  _M0L7_2abindS573
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS569, _M0L6_2atmpS2151);
  _M0L7_2abindS574 = 0;
  _block_3999
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3999)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3999->$0 = _M0L7_2abindS573;
  _block_3999->$1 = 0;
  _block_3999->$2 = _M0L8capacityS569;
  _block_3999->$3 = _M0L7_2abindS571;
  _block_3999->$4 = _M0L7_2abindS572;
  _block_3999->$5 = _M0L7_2abindS574;
  _block_3999->$6 = -1;
  return _block_3999;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS576
) {
  int32_t _M0L8capacityS575;
  int32_t _M0L7_2abindS577;
  int32_t _M0L7_2abindS578;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2152;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS579;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS580;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4000;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS575
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS576);
  _M0L7_2abindS577 = _M0L8capacityS575 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS578 = _M0FPB21calc__grow__threshold(_M0L8capacityS575);
  _M0L6_2atmpS2152 = 0;
  _M0L7_2abindS579
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS575, _M0L6_2atmpS2152);
  _M0L7_2abindS580 = 0;
  _block_4000
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4000)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4000->$0 = _M0L7_2abindS579;
  _block_4000->$1 = 0;
  _block_4000->$2 = _M0L8capacityS575;
  _block_4000->$3 = _M0L7_2abindS577;
  _block_4000->$4 = _M0L7_2abindS578;
  _block_4000->$5 = _M0L7_2abindS580;
  _block_4000->$6 = -1;
  return _block_4000;
}

struct _M0TPB3MapGsRPC16string10StringViewE* _M0MPB3Map11new_2einnerGsRPC16string10StringViewE(
  int32_t _M0L8capacityS582
) {
  int32_t _M0L8capacityS581;
  int32_t _M0L7_2abindS583;
  int32_t _M0L7_2abindS584;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L6_2atmpS2153;
  struct _M0TPB5EntryGsRPC16string10StringViewE** _M0L7_2abindS585;
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2abindS586;
  struct _M0TPB3MapGsRPC16string10StringViewE* _block_4001;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS581
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS582);
  _M0L7_2abindS583 = _M0L8capacityS581 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS584 = _M0FPB21calc__grow__threshold(_M0L8capacityS581);
  _M0L6_2atmpS2153 = 0;
  _M0L7_2abindS585
  = (struct _M0TPB5EntryGsRPC16string10StringViewE**)moonbit_make_ref_array(_M0L8capacityS581, _M0L6_2atmpS2153);
  _M0L7_2abindS586 = 0;
  _block_4001
  = (struct _M0TPB3MapGsRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPC16string10StringViewE));
  Moonbit_object_header(_block_4001)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPC16string10StringViewE, $0) >> 2, 2, 0);
  _block_4001->$0 = _M0L7_2abindS585;
  _block_4001->$1 = 0;
  _block_4001->$2 = _M0L8capacityS581;
  _block_4001->$3 = _M0L7_2abindS583;
  _block_4001->$4 = _M0L7_2abindS584;
  _block_4001->$5 = _M0L7_2abindS586;
  _block_4001->$6 = -1;
  return _block_4001;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS588
) {
  int32_t _M0L8capacityS587;
  int32_t _M0L7_2abindS589;
  int32_t _M0L7_2abindS590;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2154;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS591;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS592;
  struct _M0TPB3MapGsRPB4JsonE* _block_4002;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS587
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS588);
  _M0L7_2abindS589 = _M0L8capacityS587 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS590 = _M0FPB21calc__grow__threshold(_M0L8capacityS587);
  _M0L6_2atmpS2154 = 0;
  _M0L7_2abindS591
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS587, _M0L6_2atmpS2154);
  _M0L7_2abindS592 = 0;
  _block_4002
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_4002)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_4002->$0 = _M0L7_2abindS591;
  _block_4002->$1 = 0;
  _block_4002->$2 = _M0L8capacityS587;
  _block_4002->$3 = _M0L7_2abindS589;
  _block_4002->$4 = _M0L7_2abindS590;
  _block_4002->$5 = _M0L7_2abindS592;
  _block_4002->$6 = -1;
  return _block_4002;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS568) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS568 >= 0) {
    int32_t _M0L6_2atmpS2150;
    int32_t _M0L6_2atmpS2149;
    int32_t _M0L6_2atmpS2148;
    int32_t _M0L6_2atmpS2147;
    if (_M0L4selfS568 <= 1) {
      return 1;
    }
    if (_M0L4selfS568 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2150 = _M0L4selfS568 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2149 = moonbit_clz32(_M0L6_2atmpS2150);
    _M0L6_2atmpS2148 = _M0L6_2atmpS2149 - 1;
    _M0L6_2atmpS2147 = 2147483647 >> (_M0L6_2atmpS2148 & 31);
    return _M0L6_2atmpS2147 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS567) {
  int32_t _M0L6_2atmpS2146;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2146 = _M0L8capacityS567 * 13;
  return _M0L6_2atmpS2146 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS559
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS559 == 0) {
    if (_M0L4selfS559) {
      moonbit_decref(_M0L4selfS559);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS560 =
      _M0L4selfS559;
    return _M0L7_2aSomeS560;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS561
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS561 == 0) {
    if (_M0L4selfS561) {
      moonbit_decref(_M0L4selfS561);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS562 =
      _M0L4selfS561;
    return _M0L7_2aSomeS562;
  }
}

struct _M0TPB5EntryGsRPC16string10StringViewE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPC16string10StringViewEE(
  struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L4selfS563
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS563 == 0) {
    if (_M0L4selfS563) {
      moonbit_decref(_M0L4selfS563);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPC16string10StringViewE* _M0L7_2aSomeS564 =
      _M0L4selfS563;
    return _M0L7_2aSomeS564;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS565
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS565 == 0) {
    if (_M0L4selfS565) {
      moonbit_decref(_M0L4selfS565);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS566 = _M0L4selfS565;
    return _M0L7_2aSomeS566;
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS558
) {
  void** _M0L6_2atmpS2145;
  struct _M0TPB5ArrayGRPB4JsonE* _block_4003;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2145
  = (void**)moonbit_make_ref_array(_M0L3lenS558, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_4003
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_4003)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_4003->$0 = _M0L6_2atmpS2145;
  _block_4003->$1 = _M0L3lenS558;
  return _block_4003;
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS557
) {
  moonbit_string_t* _M0L6_2atmpS2144;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2144 = _M0L4selfS557;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2144);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS553,
  int32_t _M0L5indexS554
) {
  uint64_t* _M0L6_2atmpS2142;
  uint64_t _M0L6_2atmpS3678;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2142 = _M0L4selfS553;
  if (
    _M0L5indexS554 < 0
    || _M0L5indexS554 >= Moonbit_array_length(_M0L6_2atmpS2142)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3678 = (uint64_t)_M0L6_2atmpS2142[_M0L5indexS554];
  moonbit_decref(_M0L6_2atmpS2142);
  return _M0L6_2atmpS3678;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS555,
  int32_t _M0L5indexS556
) {
  uint32_t* _M0L6_2atmpS2143;
  uint32_t _M0L6_2atmpS3679;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2143 = _M0L4selfS555;
  if (
    _M0L5indexS556 < 0
    || _M0L5indexS556 >= Moonbit_array_length(_M0L6_2atmpS2143)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3679 = (uint32_t)_M0L6_2atmpS2143[_M0L5indexS556];
  moonbit_decref(_M0L6_2atmpS2143);
  return _M0L6_2atmpS3679;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS552
) {
  moonbit_string_t* _M0L6_2atmpS2140;
  int32_t _M0L6_2atmpS3680;
  int32_t _M0L6_2atmpS2141;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2139;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS552);
  _M0L6_2atmpS2140 = _M0L4selfS552;
  _M0L6_2atmpS3680 = Moonbit_array_length(_M0L4selfS552);
  moonbit_decref(_M0L4selfS552);
  _M0L6_2atmpS2141 = _M0L6_2atmpS3680;
  _M0L6_2atmpS2139
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2141, _M0L6_2atmpS2140
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2139);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS550
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS549;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__* _closure_4004;
  struct _M0TWEOs* _M0L6_2atmpS2127;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS549
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS549)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS549->$0 = 0;
  _closure_4004
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__));
  Moonbit_object_header(_closure_4004)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__, $0_0) >> 2, 2, 0);
  _closure_4004->code = &_M0MPC15array9ArrayView4iterGsEC2128l570;
  _closure_4004->$0_0 = _M0L4selfS550.$0;
  _closure_4004->$0_1 = _M0L4selfS550.$1;
  _closure_4004->$0_2 = _M0L4selfS550.$2;
  _closure_4004->$1 = _M0L1iS549;
  _M0L6_2atmpS2127 = (struct _M0TWEOs*)_closure_4004;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2127);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2128l570(
  struct _M0TWEOs* _M0L6_2aenvS2129
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__* _M0L14_2acasted__envS2130;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3685;
  struct _M0TPC13ref3RefGiE* _M0L1iS549;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3684;
  int32_t _M0L6_2acntS3860;
  struct _M0TPB9ArrayViewGsE _M0L4selfS550;
  int32_t _M0L3valS2131;
  int32_t _M0L6_2atmpS2132;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2130
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2128__l570__*)_M0L6_2aenvS2129;
  _M0L8_2afieldS3685 = _M0L14_2acasted__envS2130->$1;
  _M0L1iS549 = _M0L8_2afieldS3685;
  _M0L8_2afieldS3684
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2130->$0_1,
      _M0L14_2acasted__envS2130->$0_2,
      _M0L14_2acasted__envS2130->$0_0
  };
  _M0L6_2acntS3860 = Moonbit_object_header(_M0L14_2acasted__envS2130)->rc;
  if (_M0L6_2acntS3860 > 1) {
    int32_t _M0L11_2anew__cntS3861 = _M0L6_2acntS3860 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2130)->rc
    = _M0L11_2anew__cntS3861;
    moonbit_incref(_M0L1iS549);
    moonbit_incref(_M0L8_2afieldS3684.$0);
  } else if (_M0L6_2acntS3860 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2130);
  }
  _M0L4selfS550 = _M0L8_2afieldS3684;
  _M0L3valS2131 = _M0L1iS549->$0;
  moonbit_incref(_M0L4selfS550.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2132 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS550);
  if (_M0L3valS2131 < _M0L6_2atmpS2132) {
    moonbit_string_t* _M0L8_2afieldS3683 = _M0L4selfS550.$0;
    moonbit_string_t* _M0L3bufS2135 = _M0L8_2afieldS3683;
    int32_t _M0L8_2afieldS3682 = _M0L4selfS550.$1;
    int32_t _M0L5startS2137 = _M0L8_2afieldS3682;
    int32_t _M0L3valS2138 = _M0L1iS549->$0;
    int32_t _M0L6_2atmpS2136 = _M0L5startS2137 + _M0L3valS2138;
    moonbit_string_t _M0L6_2atmpS3681 =
      (moonbit_string_t)_M0L3bufS2135[_M0L6_2atmpS2136];
    moonbit_string_t _M0L4elemS551;
    int32_t _M0L3valS2134;
    int32_t _M0L6_2atmpS2133;
    moonbit_incref(_M0L6_2atmpS3681);
    moonbit_decref(_M0L3bufS2135);
    _M0L4elemS551 = _M0L6_2atmpS3681;
    _M0L3valS2134 = _M0L1iS549->$0;
    _M0L6_2atmpS2133 = _M0L3valS2134 + 1;
    _M0L1iS549->$0 = _M0L6_2atmpS2133;
    moonbit_decref(_M0L1iS549);
    return _M0L4elemS551;
  } else {
    moonbit_decref(_M0L4selfS550.$0);
    moonbit_decref(_M0L1iS549);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS548
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS548;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS547,
  struct _M0TPB6Logger _M0L6loggerS546
) {
  moonbit_string_t _M0L6_2atmpS2126;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2126
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS547, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS546.$0->$method_0(_M0L6loggerS546.$1, _M0L6_2atmpS2126);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS545,
  struct _M0TPB6Logger _M0L6loggerS544
) {
  moonbit_string_t _M0L6_2atmpS2125;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2125 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS545, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS544.$0->$method_0(_M0L6loggerS544.$1, _M0L6_2atmpS2125);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS539) {
  int32_t _M0L3lenS538;
  struct _M0TPC13ref3RefGiE* _M0L5indexS540;
  struct _M0R38String_3a_3aiter_2eanon__u2109__l247__* _closure_4005;
  struct _M0TWEOc* _M0L6_2atmpS2108;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS538 = Moonbit_array_length(_M0L4selfS539);
  _M0L5indexS540
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS540)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS540->$0 = 0;
  _closure_4005
  = (struct _M0R38String_3a_3aiter_2eanon__u2109__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2109__l247__));
  Moonbit_object_header(_closure_4005)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2109__l247__, $0) >> 2, 2, 0);
  _closure_4005->code = &_M0MPC16string6String4iterC2109l247;
  _closure_4005->$0 = _M0L5indexS540;
  _closure_4005->$1 = _M0L4selfS539;
  _closure_4005->$2 = _M0L3lenS538;
  _M0L6_2atmpS2108 = (struct _M0TWEOc*)_closure_4005;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2108);
}

int32_t _M0MPC16string6String4iterC2109l247(
  struct _M0TWEOc* _M0L6_2aenvS2110
) {
  struct _M0R38String_3a_3aiter_2eanon__u2109__l247__* _M0L14_2acasted__envS2111;
  int32_t _M0L3lenS538;
  moonbit_string_t _M0L8_2afieldS3688;
  moonbit_string_t _M0L4selfS539;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3687;
  int32_t _M0L6_2acntS3862;
  struct _M0TPC13ref3RefGiE* _M0L5indexS540;
  int32_t _M0L3valS2112;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2111
  = (struct _M0R38String_3a_3aiter_2eanon__u2109__l247__*)_M0L6_2aenvS2110;
  _M0L3lenS538 = _M0L14_2acasted__envS2111->$2;
  _M0L8_2afieldS3688 = _M0L14_2acasted__envS2111->$1;
  _M0L4selfS539 = _M0L8_2afieldS3688;
  _M0L8_2afieldS3687 = _M0L14_2acasted__envS2111->$0;
  _M0L6_2acntS3862 = Moonbit_object_header(_M0L14_2acasted__envS2111)->rc;
  if (_M0L6_2acntS3862 > 1) {
    int32_t _M0L11_2anew__cntS3863 = _M0L6_2acntS3862 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2111)->rc
    = _M0L11_2anew__cntS3863;
    moonbit_incref(_M0L4selfS539);
    moonbit_incref(_M0L8_2afieldS3687);
  } else if (_M0L6_2acntS3862 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2111);
  }
  _M0L5indexS540 = _M0L8_2afieldS3687;
  _M0L3valS2112 = _M0L5indexS540->$0;
  if (_M0L3valS2112 < _M0L3lenS538) {
    int32_t _M0L3valS2124 = _M0L5indexS540->$0;
    int32_t _M0L2c1S541 = _M0L4selfS539[_M0L3valS2124];
    int32_t _if__result_4006;
    int32_t _M0L3valS2122;
    int32_t _M0L6_2atmpS2121;
    int32_t _M0L6_2atmpS2123;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S541)) {
      int32_t _M0L3valS2114 = _M0L5indexS540->$0;
      int32_t _M0L6_2atmpS2113 = _M0L3valS2114 + 1;
      _if__result_4006 = _M0L6_2atmpS2113 < _M0L3lenS538;
    } else {
      _if__result_4006 = 0;
    }
    if (_if__result_4006) {
      int32_t _M0L3valS2120 = _M0L5indexS540->$0;
      int32_t _M0L6_2atmpS2119 = _M0L3valS2120 + 1;
      int32_t _M0L6_2atmpS3686 = _M0L4selfS539[_M0L6_2atmpS2119];
      int32_t _M0L2c2S542;
      moonbit_decref(_M0L4selfS539);
      _M0L2c2S542 = _M0L6_2atmpS3686;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S542)) {
        int32_t _M0L6_2atmpS2117 = (int32_t)_M0L2c1S541;
        int32_t _M0L6_2atmpS2118 = (int32_t)_M0L2c2S542;
        int32_t _M0L1cS543;
        int32_t _M0L3valS2116;
        int32_t _M0L6_2atmpS2115;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS543
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2117, _M0L6_2atmpS2118);
        _M0L3valS2116 = _M0L5indexS540->$0;
        _M0L6_2atmpS2115 = _M0L3valS2116 + 2;
        _M0L5indexS540->$0 = _M0L6_2atmpS2115;
        moonbit_decref(_M0L5indexS540);
        return _M0L1cS543;
      }
    } else {
      moonbit_decref(_M0L4selfS539);
    }
    _M0L3valS2122 = _M0L5indexS540->$0;
    _M0L6_2atmpS2121 = _M0L3valS2122 + 1;
    _M0L5indexS540->$0 = _M0L6_2atmpS2121;
    moonbit_decref(_M0L5indexS540);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2123 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S541);
    return _M0L6_2atmpS2123;
  } else {
    moonbit_decref(_M0L5indexS540);
    moonbit_decref(_M0L4selfS539);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS529,
  moonbit_string_t _M0L5valueS531
) {
  int32_t _M0L3lenS2093;
  moonbit_string_t* _M0L6_2atmpS2095;
  int32_t _M0L6_2atmpS3691;
  int32_t _M0L6_2atmpS2094;
  int32_t _M0L6lengthS530;
  moonbit_string_t* _M0L8_2afieldS3690;
  moonbit_string_t* _M0L3bufS2096;
  moonbit_string_t _M0L6_2aoldS3689;
  int32_t _M0L6_2atmpS2097;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2093 = _M0L4selfS529->$1;
  moonbit_incref(_M0L4selfS529);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2095 = _M0MPC15array5Array6bufferGsE(_M0L4selfS529);
  _M0L6_2atmpS3691 = Moonbit_array_length(_M0L6_2atmpS2095);
  moonbit_decref(_M0L6_2atmpS2095);
  _M0L6_2atmpS2094 = _M0L6_2atmpS3691;
  if (_M0L3lenS2093 == _M0L6_2atmpS2094) {
    moonbit_incref(_M0L4selfS529);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS529);
  }
  _M0L6lengthS530 = _M0L4selfS529->$1;
  _M0L8_2afieldS3690 = _M0L4selfS529->$0;
  _M0L3bufS2096 = _M0L8_2afieldS3690;
  _M0L6_2aoldS3689 = (moonbit_string_t)_M0L3bufS2096[_M0L6lengthS530];
  moonbit_decref(_M0L6_2aoldS3689);
  _M0L3bufS2096[_M0L6lengthS530] = _M0L5valueS531;
  _M0L6_2atmpS2097 = _M0L6lengthS530 + 1;
  _M0L4selfS529->$1 = _M0L6_2atmpS2097;
  moonbit_decref(_M0L4selfS529);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS532,
  struct _M0TUsiE* _M0L5valueS534
) {
  int32_t _M0L3lenS2098;
  struct _M0TUsiE** _M0L6_2atmpS2100;
  int32_t _M0L6_2atmpS3694;
  int32_t _M0L6_2atmpS2099;
  int32_t _M0L6lengthS533;
  struct _M0TUsiE** _M0L8_2afieldS3693;
  struct _M0TUsiE** _M0L3bufS2101;
  struct _M0TUsiE* _M0L6_2aoldS3692;
  int32_t _M0L6_2atmpS2102;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2098 = _M0L4selfS532->$1;
  moonbit_incref(_M0L4selfS532);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2100 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS532);
  _M0L6_2atmpS3694 = Moonbit_array_length(_M0L6_2atmpS2100);
  moonbit_decref(_M0L6_2atmpS2100);
  _M0L6_2atmpS2099 = _M0L6_2atmpS3694;
  if (_M0L3lenS2098 == _M0L6_2atmpS2099) {
    moonbit_incref(_M0L4selfS532);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS532);
  }
  _M0L6lengthS533 = _M0L4selfS532->$1;
  _M0L8_2afieldS3693 = _M0L4selfS532->$0;
  _M0L3bufS2101 = _M0L8_2afieldS3693;
  _M0L6_2aoldS3692 = (struct _M0TUsiE*)_M0L3bufS2101[_M0L6lengthS533];
  if (_M0L6_2aoldS3692) {
    moonbit_decref(_M0L6_2aoldS3692);
  }
  _M0L3bufS2101[_M0L6lengthS533] = _M0L5valueS534;
  _M0L6_2atmpS2102 = _M0L6lengthS533 + 1;
  _M0L4selfS532->$1 = _M0L6_2atmpS2102;
  moonbit_decref(_M0L4selfS532);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS535,
  void* _M0L5valueS537
) {
  int32_t _M0L3lenS2103;
  void** _M0L6_2atmpS2105;
  int32_t _M0L6_2atmpS3697;
  int32_t _M0L6_2atmpS2104;
  int32_t _M0L6lengthS536;
  void** _M0L8_2afieldS3696;
  void** _M0L3bufS2106;
  void* _M0L6_2aoldS3695;
  int32_t _M0L6_2atmpS2107;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2103 = _M0L4selfS535->$1;
  moonbit_incref(_M0L4selfS535);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2105
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS535);
  _M0L6_2atmpS3697 = Moonbit_array_length(_M0L6_2atmpS2105);
  moonbit_decref(_M0L6_2atmpS2105);
  _M0L6_2atmpS2104 = _M0L6_2atmpS3697;
  if (_M0L3lenS2103 == _M0L6_2atmpS2104) {
    moonbit_incref(_M0L4selfS535);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS535);
  }
  _M0L6lengthS536 = _M0L4selfS535->$1;
  _M0L8_2afieldS3696 = _M0L4selfS535->$0;
  _M0L3bufS2106 = _M0L8_2afieldS3696;
  _M0L6_2aoldS3695 = (void*)_M0L3bufS2106[_M0L6lengthS536];
  moonbit_decref(_M0L6_2aoldS3695);
  _M0L3bufS2106[_M0L6lengthS536] = _M0L5valueS537;
  _M0L6_2atmpS2107 = _M0L6lengthS536 + 1;
  _M0L4selfS535->$1 = _M0L6_2atmpS2107;
  moonbit_decref(_M0L4selfS535);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS521) {
  int32_t _M0L8old__capS520;
  int32_t _M0L8new__capS522;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS520 = _M0L4selfS521->$1;
  if (_M0L8old__capS520 == 0) {
    _M0L8new__capS522 = 8;
  } else {
    _M0L8new__capS522 = _M0L8old__capS520 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS521, _M0L8new__capS522);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS524
) {
  int32_t _M0L8old__capS523;
  int32_t _M0L8new__capS525;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS523 = _M0L4selfS524->$1;
  if (_M0L8old__capS523 == 0) {
    _M0L8new__capS525 = 8;
  } else {
    _M0L8new__capS525 = _M0L8old__capS523 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS524, _M0L8new__capS525);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS527
) {
  int32_t _M0L8old__capS526;
  int32_t _M0L8new__capS528;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS526 = _M0L4selfS527->$1;
  if (_M0L8old__capS526 == 0) {
    _M0L8new__capS528 = 8;
  } else {
    _M0L8new__capS528 = _M0L8old__capS526 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS527, _M0L8new__capS528);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS505,
  int32_t _M0L13new__capacityS503
) {
  moonbit_string_t* _M0L8new__bufS502;
  moonbit_string_t* _M0L8_2afieldS3699;
  moonbit_string_t* _M0L8old__bufS504;
  int32_t _M0L8old__capS506;
  int32_t _M0L9copy__lenS507;
  moonbit_string_t* _M0L6_2aoldS3698;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS502
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS503, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3699 = _M0L4selfS505->$0;
  _M0L8old__bufS504 = _M0L8_2afieldS3699;
  _M0L8old__capS506 = Moonbit_array_length(_M0L8old__bufS504);
  if (_M0L8old__capS506 < _M0L13new__capacityS503) {
    _M0L9copy__lenS507 = _M0L8old__capS506;
  } else {
    _M0L9copy__lenS507 = _M0L13new__capacityS503;
  }
  moonbit_incref(_M0L8old__bufS504);
  moonbit_incref(_M0L8new__bufS502);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS502, 0, _M0L8old__bufS504, 0, _M0L9copy__lenS507);
  _M0L6_2aoldS3698 = _M0L4selfS505->$0;
  moonbit_decref(_M0L6_2aoldS3698);
  _M0L4selfS505->$0 = _M0L8new__bufS502;
  moonbit_decref(_M0L4selfS505);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS511,
  int32_t _M0L13new__capacityS509
) {
  struct _M0TUsiE** _M0L8new__bufS508;
  struct _M0TUsiE** _M0L8_2afieldS3701;
  struct _M0TUsiE** _M0L8old__bufS510;
  int32_t _M0L8old__capS512;
  int32_t _M0L9copy__lenS513;
  struct _M0TUsiE** _M0L6_2aoldS3700;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS508
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS509, 0);
  _M0L8_2afieldS3701 = _M0L4selfS511->$0;
  _M0L8old__bufS510 = _M0L8_2afieldS3701;
  _M0L8old__capS512 = Moonbit_array_length(_M0L8old__bufS510);
  if (_M0L8old__capS512 < _M0L13new__capacityS509) {
    _M0L9copy__lenS513 = _M0L8old__capS512;
  } else {
    _M0L9copy__lenS513 = _M0L13new__capacityS509;
  }
  moonbit_incref(_M0L8old__bufS510);
  moonbit_incref(_M0L8new__bufS508);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS508, 0, _M0L8old__bufS510, 0, _M0L9copy__lenS513);
  _M0L6_2aoldS3700 = _M0L4selfS511->$0;
  moonbit_decref(_M0L6_2aoldS3700);
  _M0L4selfS511->$0 = _M0L8new__bufS508;
  moonbit_decref(_M0L4selfS511);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS517,
  int32_t _M0L13new__capacityS515
) {
  void** _M0L8new__bufS514;
  void** _M0L8_2afieldS3703;
  void** _M0L8old__bufS516;
  int32_t _M0L8old__capS518;
  int32_t _M0L9copy__lenS519;
  void** _M0L6_2aoldS3702;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS514
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS515, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3703 = _M0L4selfS517->$0;
  _M0L8old__bufS516 = _M0L8_2afieldS3703;
  _M0L8old__capS518 = Moonbit_array_length(_M0L8old__bufS516);
  if (_M0L8old__capS518 < _M0L13new__capacityS515) {
    _M0L9copy__lenS519 = _M0L8old__capS518;
  } else {
    _M0L9copy__lenS519 = _M0L13new__capacityS515;
  }
  moonbit_incref(_M0L8old__bufS516);
  moonbit_incref(_M0L8new__bufS514);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS514, 0, _M0L8old__bufS516, 0, _M0L9copy__lenS519);
  _M0L6_2aoldS3702 = _M0L4selfS517->$0;
  moonbit_decref(_M0L6_2aoldS3702);
  _M0L4selfS517->$0 = _M0L8new__bufS514;
  moonbit_decref(_M0L4selfS517);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS501
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS501 == 0) {
    moonbit_string_t* _M0L6_2atmpS2091 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4007 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4007)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4007->$0 = _M0L6_2atmpS2091;
    _block_4007->$1 = 0;
    return _block_4007;
  } else {
    moonbit_string_t* _M0L6_2atmpS2092 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS501, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4008 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4008)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4008->$0 = _M0L6_2atmpS2092;
    _block_4008->$1 = 0;
    return _block_4008;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS495,
  int32_t _M0L1nS494
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS494 <= 0) {
    moonbit_decref(_M0L4selfS495);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS494 == 1) {
    return _M0L4selfS495;
  } else {
    int32_t _M0L3lenS496 = Moonbit_array_length(_M0L4selfS495);
    int32_t _M0L6_2atmpS2090 = _M0L3lenS496 * _M0L1nS494;
    struct _M0TPB13StringBuilder* _M0L3bufS497;
    moonbit_string_t _M0L3strS498;
    int32_t _M0L2__S499;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS497 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2090);
    _M0L3strS498 = _M0L4selfS495;
    _M0L2__S499 = 0;
    while (1) {
      if (_M0L2__S499 < _M0L1nS494) {
        int32_t _M0L6_2atmpS2089;
        moonbit_incref(_M0L3strS498);
        moonbit_incref(_M0L3bufS497);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS497, _M0L3strS498);
        _M0L6_2atmpS2089 = _M0L2__S499 + 1;
        _M0L2__S499 = _M0L6_2atmpS2089;
        continue;
      } else {
        moonbit_decref(_M0L3strS498);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS497);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS492,
  struct _M0TPC16string10StringView _M0L3strS493
) {
  int32_t _M0L3lenS2077;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L6_2atmpS2078;
  int32_t _M0L6_2atmpS2076;
  moonbit_bytes_t _M0L8_2afieldS3704;
  moonbit_bytes_t _M0L4dataS2080;
  int32_t _M0L3lenS2081;
  moonbit_string_t _M0L6_2atmpS2082;
  int32_t _M0L6_2atmpS2083;
  int32_t _M0L6_2atmpS2084;
  int32_t _M0L3lenS2086;
  int32_t _M0L6_2atmpS2088;
  int32_t _M0L6_2atmpS2087;
  int32_t _M0L6_2atmpS2085;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2077 = _M0L4selfS492->$1;
  moonbit_incref(_M0L3strS493.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2079 = _M0MPC16string10StringView6length(_M0L3strS493);
  _M0L6_2atmpS2078 = _M0L6_2atmpS2079 * 2;
  _M0L6_2atmpS2076 = _M0L3lenS2077 + _M0L6_2atmpS2078;
  moonbit_incref(_M0L4selfS492);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS492, _M0L6_2atmpS2076);
  _M0L8_2afieldS3704 = _M0L4selfS492->$0;
  _M0L4dataS2080 = _M0L8_2afieldS3704;
  _M0L3lenS2081 = _M0L4selfS492->$1;
  moonbit_incref(_M0L4dataS2080);
  moonbit_incref(_M0L3strS493.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2082 = _M0MPC16string10StringView4data(_M0L3strS493);
  moonbit_incref(_M0L3strS493.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2083 = _M0MPC16string10StringView13start__offset(_M0L3strS493);
  moonbit_incref(_M0L3strS493.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2084 = _M0MPC16string10StringView6length(_M0L3strS493);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2080, _M0L3lenS2081, _M0L6_2atmpS2082, _M0L6_2atmpS2083, _M0L6_2atmpS2084);
  _M0L3lenS2086 = _M0L4selfS492->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2088 = _M0MPC16string10StringView6length(_M0L3strS493);
  _M0L6_2atmpS2087 = _M0L6_2atmpS2088 * 2;
  _M0L6_2atmpS2085 = _M0L3lenS2086 + _M0L6_2atmpS2087;
  _M0L4selfS492->$1 = _M0L6_2atmpS2085;
  moonbit_decref(_M0L4selfS492);
  return 0;
}

void* _M0IPC16string10StringViewPB6ToJson8to__json(
  struct _M0TPC16string10StringView _M0L4selfS491
) {
  moonbit_string_t _M0L6_2atmpS2075;
  #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6_2atmpS2075
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L4selfS491);
  #line 600 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS2075);
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS488,
  int32_t _M0L1iS489,
  int32_t _M0L13start__offsetS490,
  int64_t _M0L11end__offsetS486
) {
  int32_t _M0L11end__offsetS485;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS486 == 4294967296ll) {
    _M0L11end__offsetS485 = Moonbit_array_length(_M0L4selfS488);
  } else {
    int64_t _M0L7_2aSomeS487 = _M0L11end__offsetS486;
    _M0L11end__offsetS485 = (int32_t)_M0L7_2aSomeS487;
  }
  if (_M0L1iS489 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS488, _M0L1iS489, _M0L13start__offsetS490, _M0L11end__offsetS485);
  } else {
    int32_t _M0L6_2atmpS2074 = -_M0L1iS489;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS488, _M0L6_2atmpS2074, _M0L13start__offsetS490, _M0L11end__offsetS485);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS483,
  int32_t _M0L1nS481,
  int32_t _M0L13start__offsetS477,
  int32_t _M0L11end__offsetS478
) {
  int32_t _if__result_4010;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS477 >= 0) {
    _if__result_4010 = _M0L13start__offsetS477 <= _M0L11end__offsetS478;
  } else {
    _if__result_4010 = 0;
  }
  if (_if__result_4010) {
    int32_t _M0Lm13utf16__offsetS479 = _M0L13start__offsetS477;
    int32_t _M0Lm11char__countS480 = 0;
    int32_t _M0L6_2atmpS2072;
    int32_t _if__result_4013;
    while (1) {
      int32_t _M0L6_2atmpS2066 = _M0Lm13utf16__offsetS479;
      int32_t _if__result_4012;
      if (_M0L6_2atmpS2066 < _M0L11end__offsetS478) {
        int32_t _M0L6_2atmpS2065 = _M0Lm11char__countS480;
        _if__result_4012 = _M0L6_2atmpS2065 < _M0L1nS481;
      } else {
        _if__result_4012 = 0;
      }
      if (_if__result_4012) {
        int32_t _M0L6_2atmpS2070 = _M0Lm13utf16__offsetS479;
        int32_t _M0L1cS482 = _M0L4selfS483[_M0L6_2atmpS2070];
        int32_t _M0L6_2atmpS2069;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS482)) {
          int32_t _M0L6_2atmpS2067 = _M0Lm13utf16__offsetS479;
          _M0Lm13utf16__offsetS479 = _M0L6_2atmpS2067 + 2;
        } else {
          int32_t _M0L6_2atmpS2068 = _M0Lm13utf16__offsetS479;
          _M0Lm13utf16__offsetS479 = _M0L6_2atmpS2068 + 1;
        }
        _M0L6_2atmpS2069 = _M0Lm11char__countS480;
        _M0Lm11char__countS480 = _M0L6_2atmpS2069 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS483);
      }
      break;
    }
    _M0L6_2atmpS2072 = _M0Lm11char__countS480;
    if (_M0L6_2atmpS2072 < _M0L1nS481) {
      _if__result_4013 = 1;
    } else {
      int32_t _M0L6_2atmpS2071 = _M0Lm13utf16__offsetS479;
      _if__result_4013 = _M0L6_2atmpS2071 >= _M0L11end__offsetS478;
    }
    if (_if__result_4013) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2073 = _M0Lm13utf16__offsetS479;
      return (int64_t)_M0L6_2atmpS2073;
    }
  } else {
    moonbit_decref(_M0L4selfS483);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_72.data, (moonbit_string_t)moonbit_string_literal_73.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS475,
  int32_t _M0L1nS473,
  int32_t _M0L13start__offsetS472,
  int32_t _M0L11end__offsetS471
) {
  int32_t _M0Lm11char__countS469;
  int32_t _M0Lm13utf16__offsetS470;
  int32_t _M0L6_2atmpS2063;
  int32_t _if__result_4016;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS469 = 0;
  _M0Lm13utf16__offsetS470 = _M0L11end__offsetS471;
  while (1) {
    int32_t _M0L6_2atmpS2056 = _M0Lm13utf16__offsetS470;
    int32_t _M0L6_2atmpS2055 = _M0L6_2atmpS2056 - 1;
    int32_t _if__result_4015;
    if (_M0L6_2atmpS2055 >= _M0L13start__offsetS472) {
      int32_t _M0L6_2atmpS2054 = _M0Lm11char__countS469;
      _if__result_4015 = _M0L6_2atmpS2054 < _M0L1nS473;
    } else {
      _if__result_4015 = 0;
    }
    if (_if__result_4015) {
      int32_t _M0L6_2atmpS2061 = _M0Lm13utf16__offsetS470;
      int32_t _M0L6_2atmpS2060 = _M0L6_2atmpS2061 - 1;
      int32_t _M0L1cS474 = _M0L4selfS475[_M0L6_2atmpS2060];
      int32_t _M0L6_2atmpS2059;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS474)) {
        int32_t _M0L6_2atmpS2057 = _M0Lm13utf16__offsetS470;
        _M0Lm13utf16__offsetS470 = _M0L6_2atmpS2057 - 2;
      } else {
        int32_t _M0L6_2atmpS2058 = _M0Lm13utf16__offsetS470;
        _M0Lm13utf16__offsetS470 = _M0L6_2atmpS2058 - 1;
      }
      _M0L6_2atmpS2059 = _M0Lm11char__countS469;
      _M0Lm11char__countS469 = _M0L6_2atmpS2059 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS475);
    }
    break;
  }
  _M0L6_2atmpS2063 = _M0Lm11char__countS469;
  if (_M0L6_2atmpS2063 < _M0L1nS473) {
    _if__result_4016 = 1;
  } else {
    int32_t _M0L6_2atmpS2062 = _M0Lm13utf16__offsetS470;
    _if__result_4016 = _M0L6_2atmpS2062 < _M0L13start__offsetS472;
  }
  if (_if__result_4016) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2064 = _M0Lm13utf16__offsetS470;
    return (int64_t)_M0L6_2atmpS2064;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS461,
  int32_t _M0L3lenS464,
  int32_t _M0L13start__offsetS468,
  int64_t _M0L11end__offsetS459
) {
  int32_t _M0L11end__offsetS458;
  int32_t _M0L5indexS462;
  int32_t _M0L5countS463;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS459 == 4294967296ll) {
    _M0L11end__offsetS458 = Moonbit_array_length(_M0L4selfS461);
  } else {
    int64_t _M0L7_2aSomeS460 = _M0L11end__offsetS459;
    _M0L11end__offsetS458 = (int32_t)_M0L7_2aSomeS460;
  }
  _M0L5indexS462 = _M0L13start__offsetS468;
  _M0L5countS463 = 0;
  while (1) {
    int32_t _if__result_4018;
    if (_M0L5indexS462 < _M0L11end__offsetS458) {
      _if__result_4018 = _M0L5countS463 < _M0L3lenS464;
    } else {
      _if__result_4018 = 0;
    }
    if (_if__result_4018) {
      int32_t _M0L2c1S465 = _M0L4selfS461[_M0L5indexS462];
      int32_t _if__result_4019;
      int32_t _M0L6_2atmpS2052;
      int32_t _M0L6_2atmpS2053;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S465)) {
        int32_t _M0L6_2atmpS2048 = _M0L5indexS462 + 1;
        _if__result_4019 = _M0L6_2atmpS2048 < _M0L11end__offsetS458;
      } else {
        _if__result_4019 = 0;
      }
      if (_if__result_4019) {
        int32_t _M0L6_2atmpS2051 = _M0L5indexS462 + 1;
        int32_t _M0L2c2S466 = _M0L4selfS461[_M0L6_2atmpS2051];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S466)) {
          int32_t _M0L6_2atmpS2049 = _M0L5indexS462 + 2;
          int32_t _M0L6_2atmpS2050 = _M0L5countS463 + 1;
          _M0L5indexS462 = _M0L6_2atmpS2049;
          _M0L5countS463 = _M0L6_2atmpS2050;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_74.data, (moonbit_string_t)moonbit_string_literal_75.data);
        }
      }
      _M0L6_2atmpS2052 = _M0L5indexS462 + 1;
      _M0L6_2atmpS2053 = _M0L5countS463 + 1;
      _M0L5indexS462 = _M0L6_2atmpS2052;
      _M0L5countS463 = _M0L6_2atmpS2053;
      continue;
    } else {
      moonbit_decref(_M0L4selfS461);
      return _M0L5countS463 >= _M0L3lenS464;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS453
) {
  int32_t _M0L3endS2038;
  int32_t _M0L8_2afieldS3705;
  int32_t _M0L5startS2039;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2038 = _M0L4selfS453.$2;
  _M0L8_2afieldS3705 = _M0L4selfS453.$1;
  moonbit_decref(_M0L4selfS453.$0);
  _M0L5startS2039 = _M0L8_2afieldS3705;
  return _M0L3endS2038 - _M0L5startS2039;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS454
) {
  int32_t _M0L3endS2040;
  int32_t _M0L8_2afieldS3706;
  int32_t _M0L5startS2041;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2040 = _M0L4selfS454.$2;
  _M0L8_2afieldS3706 = _M0L4selfS454.$1;
  moonbit_decref(_M0L4selfS454.$0);
  _M0L5startS2041 = _M0L8_2afieldS3706;
  return _M0L3endS2040 - _M0L5startS2041;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS455
) {
  int32_t _M0L3endS2042;
  int32_t _M0L8_2afieldS3707;
  int32_t _M0L5startS2043;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2042 = _M0L4selfS455.$2;
  _M0L8_2afieldS3707 = _M0L4selfS455.$1;
  moonbit_decref(_M0L4selfS455.$0);
  _M0L5startS2043 = _M0L8_2afieldS3707;
  return _M0L3endS2042 - _M0L5startS2043;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPC16string10StringViewEE(
  struct _M0TPB9ArrayViewGUsRPC16string10StringViewEE _M0L4selfS456
) {
  int32_t _M0L3endS2044;
  int32_t _M0L8_2afieldS3708;
  int32_t _M0L5startS2045;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2044 = _M0L4selfS456.$2;
  _M0L8_2afieldS3708 = _M0L4selfS456.$1;
  moonbit_decref(_M0L4selfS456.$0);
  _M0L5startS2045 = _M0L8_2afieldS3708;
  return _M0L3endS2044 - _M0L5startS2045;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS457
) {
  int32_t _M0L3endS2046;
  int32_t _M0L8_2afieldS3709;
  int32_t _M0L5startS2047;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2046 = _M0L4selfS457.$2;
  _M0L8_2afieldS3709 = _M0L4selfS457.$1;
  moonbit_decref(_M0L4selfS457.$0);
  _M0L5startS2047 = _M0L8_2afieldS3709;
  return _M0L3endS2046 - _M0L5startS2047;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS451,
  int64_t _M0L19start__offset_2eoptS449,
  int64_t _M0L11end__offsetS452
) {
  int32_t _M0L13start__offsetS448;
  if (_M0L19start__offset_2eoptS449 == 4294967296ll) {
    _M0L13start__offsetS448 = 0;
  } else {
    int64_t _M0L7_2aSomeS450 = _M0L19start__offset_2eoptS449;
    _M0L13start__offsetS448 = (int32_t)_M0L7_2aSomeS450;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS451, _M0L13start__offsetS448, _M0L11end__offsetS452);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS446,
  int32_t _M0L13start__offsetS447,
  int64_t _M0L11end__offsetS444
) {
  int32_t _M0L11end__offsetS443;
  int32_t _if__result_4020;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS444 == 4294967296ll) {
    _M0L11end__offsetS443 = Moonbit_array_length(_M0L4selfS446);
  } else {
    int64_t _M0L7_2aSomeS445 = _M0L11end__offsetS444;
    _M0L11end__offsetS443 = (int32_t)_M0L7_2aSomeS445;
  }
  if (_M0L13start__offsetS447 >= 0) {
    if (_M0L13start__offsetS447 <= _M0L11end__offsetS443) {
      int32_t _M0L6_2atmpS2037 = Moonbit_array_length(_M0L4selfS446);
      _if__result_4020 = _M0L11end__offsetS443 <= _M0L6_2atmpS2037;
    } else {
      _if__result_4020 = 0;
    }
  } else {
    _if__result_4020 = 0;
  }
  if (_if__result_4020) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS447,
                                                 _M0L11end__offsetS443,
                                                 _M0L4selfS446};
  } else {
    moonbit_decref(_M0L4selfS446);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_76.data, (moonbit_string_t)moonbit_string_literal_77.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS442
) {
  moonbit_string_t _M0L8_2afieldS3711;
  moonbit_string_t _M0L3strS2034;
  int32_t _M0L5startS2035;
  int32_t _M0L8_2afieldS3710;
  int32_t _M0L3endS2036;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3711 = _M0L4selfS442.$0;
  _M0L3strS2034 = _M0L8_2afieldS3711;
  _M0L5startS2035 = _M0L4selfS442.$1;
  _M0L8_2afieldS3710 = _M0L4selfS442.$2;
  _M0L3endS2036 = _M0L8_2afieldS3710;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2034, _M0L5startS2035, _M0L3endS2036);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS440,
  struct _M0TPB6Logger _M0L6loggerS441
) {
  moonbit_string_t _M0L8_2afieldS3713;
  moonbit_string_t _M0L3strS2031;
  int32_t _M0L5startS2032;
  int32_t _M0L8_2afieldS3712;
  int32_t _M0L3endS2033;
  moonbit_string_t _M0L6substrS439;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3713 = _M0L4selfS440.$0;
  _M0L3strS2031 = _M0L8_2afieldS3713;
  _M0L5startS2032 = _M0L4selfS440.$1;
  _M0L8_2afieldS3712 = _M0L4selfS440.$2;
  _M0L3endS2033 = _M0L8_2afieldS3712;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS439
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2031, _M0L5startS2032, _M0L3endS2033);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS439, _M0L6loggerS441);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS431,
  struct _M0TPB6Logger _M0L6loggerS429
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS430;
  int32_t _M0L3lenS432;
  int32_t _M0L1iS433;
  int32_t _M0L3segS434;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS429.$1) {
    moonbit_incref(_M0L6loggerS429.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS429.$0->$method_3(_M0L6loggerS429.$1, 34);
  moonbit_incref(_M0L4selfS431);
  if (_M0L6loggerS429.$1) {
    moonbit_incref(_M0L6loggerS429.$1);
  }
  _M0L6_2aenvS430
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS430)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS430->$0 = _M0L4selfS431;
  _M0L6_2aenvS430->$1_0 = _M0L6loggerS429.$0;
  _M0L6_2aenvS430->$1_1 = _M0L6loggerS429.$1;
  _M0L3lenS432 = Moonbit_array_length(_M0L4selfS431);
  _M0L1iS433 = 0;
  _M0L3segS434 = 0;
  _2afor_435:;
  while (1) {
    int32_t _M0L4codeS436;
    int32_t _M0L1cS438;
    int32_t _M0L6_2atmpS2015;
    int32_t _M0L6_2atmpS2016;
    int32_t _M0L6_2atmpS2017;
    int32_t _tmp_4024;
    int32_t _tmp_4025;
    if (_M0L1iS433 >= _M0L3lenS432) {
      moonbit_decref(_M0L4selfS431);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
      break;
    }
    _M0L4codeS436 = _M0L4selfS431[_M0L1iS433];
    switch (_M0L4codeS436) {
      case 34: {
        _M0L1cS438 = _M0L4codeS436;
        goto join_437;
        break;
      }
      
      case 92: {
        _M0L1cS438 = _M0L4codeS436;
        goto join_437;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2018;
        int32_t _M0L6_2atmpS2019;
        moonbit_incref(_M0L6_2aenvS430);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
        if (_M0L6loggerS429.$1) {
          moonbit_incref(_M0L6loggerS429.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS429.$0->$method_0(_M0L6loggerS429.$1, (moonbit_string_t)moonbit_string_literal_59.data);
        _M0L6_2atmpS2018 = _M0L1iS433 + 1;
        _M0L6_2atmpS2019 = _M0L1iS433 + 1;
        _M0L1iS433 = _M0L6_2atmpS2018;
        _M0L3segS434 = _M0L6_2atmpS2019;
        goto _2afor_435;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2020;
        int32_t _M0L6_2atmpS2021;
        moonbit_incref(_M0L6_2aenvS430);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
        if (_M0L6loggerS429.$1) {
          moonbit_incref(_M0L6loggerS429.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS429.$0->$method_0(_M0L6loggerS429.$1, (moonbit_string_t)moonbit_string_literal_60.data);
        _M0L6_2atmpS2020 = _M0L1iS433 + 1;
        _M0L6_2atmpS2021 = _M0L1iS433 + 1;
        _M0L1iS433 = _M0L6_2atmpS2020;
        _M0L3segS434 = _M0L6_2atmpS2021;
        goto _2afor_435;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2022;
        int32_t _M0L6_2atmpS2023;
        moonbit_incref(_M0L6_2aenvS430);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
        if (_M0L6loggerS429.$1) {
          moonbit_incref(_M0L6loggerS429.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS429.$0->$method_0(_M0L6loggerS429.$1, (moonbit_string_t)moonbit_string_literal_61.data);
        _M0L6_2atmpS2022 = _M0L1iS433 + 1;
        _M0L6_2atmpS2023 = _M0L1iS433 + 1;
        _M0L1iS433 = _M0L6_2atmpS2022;
        _M0L3segS434 = _M0L6_2atmpS2023;
        goto _2afor_435;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2024;
        int32_t _M0L6_2atmpS2025;
        moonbit_incref(_M0L6_2aenvS430);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
        if (_M0L6loggerS429.$1) {
          moonbit_incref(_M0L6loggerS429.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS429.$0->$method_0(_M0L6loggerS429.$1, (moonbit_string_t)moonbit_string_literal_62.data);
        _M0L6_2atmpS2024 = _M0L1iS433 + 1;
        _M0L6_2atmpS2025 = _M0L1iS433 + 1;
        _M0L1iS433 = _M0L6_2atmpS2024;
        _M0L3segS434 = _M0L6_2atmpS2025;
        goto _2afor_435;
        break;
      }
      default: {
        if (_M0L4codeS436 < 32) {
          int32_t _M0L6_2atmpS2027;
          moonbit_string_t _M0L6_2atmpS2026;
          int32_t _M0L6_2atmpS2028;
          int32_t _M0L6_2atmpS2029;
          moonbit_incref(_M0L6_2aenvS430);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
          if (_M0L6loggerS429.$1) {
            moonbit_incref(_M0L6loggerS429.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS429.$0->$method_0(_M0L6loggerS429.$1, (moonbit_string_t)moonbit_string_literal_78.data);
          _M0L6_2atmpS2027 = _M0L4codeS436 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2026 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2027);
          if (_M0L6loggerS429.$1) {
            moonbit_incref(_M0L6loggerS429.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS429.$0->$method_0(_M0L6loggerS429.$1, _M0L6_2atmpS2026);
          if (_M0L6loggerS429.$1) {
            moonbit_incref(_M0L6loggerS429.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS429.$0->$method_3(_M0L6loggerS429.$1, 125);
          _M0L6_2atmpS2028 = _M0L1iS433 + 1;
          _M0L6_2atmpS2029 = _M0L1iS433 + 1;
          _M0L1iS433 = _M0L6_2atmpS2028;
          _M0L3segS434 = _M0L6_2atmpS2029;
          goto _2afor_435;
        } else {
          int32_t _M0L6_2atmpS2030 = _M0L1iS433 + 1;
          int32_t _tmp_4023 = _M0L3segS434;
          _M0L1iS433 = _M0L6_2atmpS2030;
          _M0L3segS434 = _tmp_4023;
          goto _2afor_435;
        }
        break;
      }
    }
    goto joinlet_4022;
    join_437:;
    moonbit_incref(_M0L6_2aenvS430);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS430, _M0L3segS434, _M0L1iS433);
    if (_M0L6loggerS429.$1) {
      moonbit_incref(_M0L6loggerS429.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS429.$0->$method_3(_M0L6loggerS429.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2015 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS438);
    if (_M0L6loggerS429.$1) {
      moonbit_incref(_M0L6loggerS429.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS429.$0->$method_3(_M0L6loggerS429.$1, _M0L6_2atmpS2015);
    _M0L6_2atmpS2016 = _M0L1iS433 + 1;
    _M0L6_2atmpS2017 = _M0L1iS433 + 1;
    _M0L1iS433 = _M0L6_2atmpS2016;
    _M0L3segS434 = _M0L6_2atmpS2017;
    continue;
    joinlet_4022:;
    _tmp_4024 = _M0L1iS433;
    _tmp_4025 = _M0L3segS434;
    _M0L1iS433 = _tmp_4024;
    _M0L3segS434 = _tmp_4025;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS429.$0->$method_3(_M0L6loggerS429.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS425,
  int32_t _M0L3segS428,
  int32_t _M0L1iS427
) {
  struct _M0TPB6Logger _M0L8_2afieldS3715;
  struct _M0TPB6Logger _M0L6loggerS424;
  moonbit_string_t _M0L8_2afieldS3714;
  int32_t _M0L6_2acntS3864;
  moonbit_string_t _M0L4selfS426;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3715
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS425->$1_0, _M0L6_2aenvS425->$1_1
  };
  _M0L6loggerS424 = _M0L8_2afieldS3715;
  _M0L8_2afieldS3714 = _M0L6_2aenvS425->$0;
  _M0L6_2acntS3864 = Moonbit_object_header(_M0L6_2aenvS425)->rc;
  if (_M0L6_2acntS3864 > 1) {
    int32_t _M0L11_2anew__cntS3865 = _M0L6_2acntS3864 - 1;
    Moonbit_object_header(_M0L6_2aenvS425)->rc = _M0L11_2anew__cntS3865;
    if (_M0L6loggerS424.$1) {
      moonbit_incref(_M0L6loggerS424.$1);
    }
    moonbit_incref(_M0L8_2afieldS3714);
  } else if (_M0L6_2acntS3864 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS425);
  }
  _M0L4selfS426 = _M0L8_2afieldS3714;
  if (_M0L1iS427 > _M0L3segS428) {
    int32_t _M0L6_2atmpS2014 = _M0L1iS427 - _M0L3segS428;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS424.$0->$method_1(_M0L6loggerS424.$1, _M0L4selfS426, _M0L3segS428, _M0L6_2atmpS2014);
  } else {
    moonbit_decref(_M0L4selfS426);
    if (_M0L6loggerS424.$1) {
      moonbit_decref(_M0L6loggerS424.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS423) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS422;
  int32_t _M0L6_2atmpS2011;
  int32_t _M0L6_2atmpS2010;
  int32_t _M0L6_2atmpS2013;
  int32_t _M0L6_2atmpS2012;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2009;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS422 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2011 = _M0IPC14byte4BytePB3Div3div(_M0L1bS423, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2010
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2011);
  moonbit_incref(_M0L7_2aselfS422);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS422, _M0L6_2atmpS2010);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2013 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS423, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2012
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2013);
  moonbit_incref(_M0L7_2aselfS422);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS422, _M0L6_2atmpS2012);
  _M0L6_2atmpS2009 = _M0L7_2aselfS422;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2009);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS421) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS421 < 10) {
    int32_t _M0L6_2atmpS2006;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2006 = _M0IPC14byte4BytePB3Add3add(_M0L1iS421, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2006);
  } else {
    int32_t _M0L6_2atmpS2008;
    int32_t _M0L6_2atmpS2007;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2008 = _M0IPC14byte4BytePB3Add3add(_M0L1iS421, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2007 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2008, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2007);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS419,
  int32_t _M0L4thatS420
) {
  int32_t _M0L6_2atmpS2004;
  int32_t _M0L6_2atmpS2005;
  int32_t _M0L6_2atmpS2003;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2004 = (int32_t)_M0L4selfS419;
  _M0L6_2atmpS2005 = (int32_t)_M0L4thatS420;
  _M0L6_2atmpS2003 = _M0L6_2atmpS2004 - _M0L6_2atmpS2005;
  return _M0L6_2atmpS2003 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS417,
  int32_t _M0L4thatS418
) {
  int32_t _M0L6_2atmpS2001;
  int32_t _M0L6_2atmpS2002;
  int32_t _M0L6_2atmpS2000;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2001 = (int32_t)_M0L4selfS417;
  _M0L6_2atmpS2002 = (int32_t)_M0L4thatS418;
  _M0L6_2atmpS2000 = _M0L6_2atmpS2001 % _M0L6_2atmpS2002;
  return _M0L6_2atmpS2000 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS415,
  int32_t _M0L4thatS416
) {
  int32_t _M0L6_2atmpS1998;
  int32_t _M0L6_2atmpS1999;
  int32_t _M0L6_2atmpS1997;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1998 = (int32_t)_M0L4selfS415;
  _M0L6_2atmpS1999 = (int32_t)_M0L4thatS416;
  _M0L6_2atmpS1997 = _M0L6_2atmpS1998 / _M0L6_2atmpS1999;
  return _M0L6_2atmpS1997 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS413,
  int32_t _M0L4thatS414
) {
  int32_t _M0L6_2atmpS1995;
  int32_t _M0L6_2atmpS1996;
  int32_t _M0L6_2atmpS1994;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1995 = (int32_t)_M0L4selfS413;
  _M0L6_2atmpS1996 = (int32_t)_M0L4thatS414;
  _M0L6_2atmpS1994 = _M0L6_2atmpS1995 + _M0L6_2atmpS1996;
  return _M0L6_2atmpS1994 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS410,
  int32_t _M0L5startS408,
  int32_t _M0L3endS409
) {
  int32_t _if__result_4026;
  int32_t _M0L3lenS411;
  int32_t _M0L6_2atmpS1992;
  int32_t _M0L6_2atmpS1993;
  moonbit_bytes_t _M0L5bytesS412;
  moonbit_bytes_t _M0L6_2atmpS1991;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS408 == 0) {
    int32_t _M0L6_2atmpS1990 = Moonbit_array_length(_M0L3strS410);
    _if__result_4026 = _M0L3endS409 == _M0L6_2atmpS1990;
  } else {
    _if__result_4026 = 0;
  }
  if (_if__result_4026) {
    return _M0L3strS410;
  }
  _M0L3lenS411 = _M0L3endS409 - _M0L5startS408;
  _M0L6_2atmpS1992 = _M0L3lenS411 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1993 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS412
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1992, _M0L6_2atmpS1993);
  moonbit_incref(_M0L5bytesS412);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS412, 0, _M0L3strS410, _M0L5startS408, _M0L3lenS411);
  _M0L6_2atmpS1991 = _M0L5bytesS412;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1991, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS404) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS404;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS405
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS405;
}

struct _M0TWEOUsRPC16string10StringViewE* _M0MPB4Iter3newGUsRPC16string10StringViewEE(
  struct _M0TWEOUsRPC16string10StringViewE* _M0L1fS406
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS406;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS407) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS407;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS396,
  int32_t _M0L5radixS395
) {
  int32_t _if__result_4027;
  uint16_t* _M0L6bufferS397;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS395 < 2) {
    _if__result_4027 = 1;
  } else {
    _if__result_4027 = _M0L5radixS395 > 36;
  }
  if (_if__result_4027) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_79.data, (moonbit_string_t)moonbit_string_literal_80.data);
  }
  if (_M0L4selfS396 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_65.data;
  }
  switch (_M0L5radixS395) {
    case 10: {
      int32_t _M0L3lenS398;
      uint16_t* _M0L6bufferS399;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS398 = _M0FPB12dec__count64(_M0L4selfS396);
      _M0L6bufferS399 = (uint16_t*)moonbit_make_string(_M0L3lenS398, 0);
      moonbit_incref(_M0L6bufferS399);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS399, _M0L4selfS396, 0, _M0L3lenS398);
      _M0L6bufferS397 = _M0L6bufferS399;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS400;
      uint16_t* _M0L6bufferS401;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS400 = _M0FPB12hex__count64(_M0L4selfS396);
      _M0L6bufferS401 = (uint16_t*)moonbit_make_string(_M0L3lenS400, 0);
      moonbit_incref(_M0L6bufferS401);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS401, _M0L4selfS396, 0, _M0L3lenS400);
      _M0L6bufferS397 = _M0L6bufferS401;
      break;
    }
    default: {
      int32_t _M0L3lenS402;
      uint16_t* _M0L6bufferS403;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS402 = _M0FPB14radix__count64(_M0L4selfS396, _M0L5radixS395);
      _M0L6bufferS403 = (uint16_t*)moonbit_make_string(_M0L3lenS402, 0);
      moonbit_incref(_M0L6bufferS403);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS403, _M0L4selfS396, 0, _M0L3lenS402, _M0L5radixS395);
      _M0L6bufferS397 = _M0L6bufferS403;
      break;
    }
  }
  return _M0L6bufferS397;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS385,
  uint64_t _M0L3numS373,
  int32_t _M0L12digit__startS376,
  int32_t _M0L10total__lenS375
) {
  uint64_t _M0Lm3numS372;
  int32_t _M0Lm6offsetS374;
  uint64_t _M0L6_2atmpS1989;
  int32_t _M0Lm9remainingS387;
  int32_t _M0L6_2atmpS1970;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS372 = _M0L3numS373;
  _M0Lm6offsetS374 = _M0L10total__lenS375 - _M0L12digit__startS376;
  while (1) {
    uint64_t _M0L6_2atmpS1933 = _M0Lm3numS372;
    if (_M0L6_2atmpS1933 >= 10000ull) {
      uint64_t _M0L6_2atmpS1956 = _M0Lm3numS372;
      uint64_t _M0L1tS377 = _M0L6_2atmpS1956 / 10000ull;
      uint64_t _M0L6_2atmpS1955 = _M0Lm3numS372;
      uint64_t _M0L6_2atmpS1954 = _M0L6_2atmpS1955 % 10000ull;
      int32_t _M0L1rS378 = (int32_t)_M0L6_2atmpS1954;
      int32_t _M0L2d1S379;
      int32_t _M0L2d2S380;
      int32_t _M0L6_2atmpS1934;
      int32_t _M0L6_2atmpS1953;
      int32_t _M0L6_2atmpS1952;
      int32_t _M0L6d1__hiS381;
      int32_t _M0L6_2atmpS1951;
      int32_t _M0L6_2atmpS1950;
      int32_t _M0L6d1__loS382;
      int32_t _M0L6_2atmpS1949;
      int32_t _M0L6_2atmpS1948;
      int32_t _M0L6d2__hiS383;
      int32_t _M0L6_2atmpS1947;
      int32_t _M0L6_2atmpS1946;
      int32_t _M0L6d2__loS384;
      int32_t _M0L6_2atmpS1936;
      int32_t _M0L6_2atmpS1935;
      int32_t _M0L6_2atmpS1939;
      int32_t _M0L6_2atmpS1938;
      int32_t _M0L6_2atmpS1937;
      int32_t _M0L6_2atmpS1942;
      int32_t _M0L6_2atmpS1941;
      int32_t _M0L6_2atmpS1940;
      int32_t _M0L6_2atmpS1945;
      int32_t _M0L6_2atmpS1944;
      int32_t _M0L6_2atmpS1943;
      _M0Lm3numS372 = _M0L1tS377;
      _M0L2d1S379 = _M0L1rS378 / 100;
      _M0L2d2S380 = _M0L1rS378 % 100;
      _M0L6_2atmpS1934 = _M0Lm6offsetS374;
      _M0Lm6offsetS374 = _M0L6_2atmpS1934 - 4;
      _M0L6_2atmpS1953 = _M0L2d1S379 / 10;
      _M0L6_2atmpS1952 = 48 + _M0L6_2atmpS1953;
      _M0L6d1__hiS381 = (uint16_t)_M0L6_2atmpS1952;
      _M0L6_2atmpS1951 = _M0L2d1S379 % 10;
      _M0L6_2atmpS1950 = 48 + _M0L6_2atmpS1951;
      _M0L6d1__loS382 = (uint16_t)_M0L6_2atmpS1950;
      _M0L6_2atmpS1949 = _M0L2d2S380 / 10;
      _M0L6_2atmpS1948 = 48 + _M0L6_2atmpS1949;
      _M0L6d2__hiS383 = (uint16_t)_M0L6_2atmpS1948;
      _M0L6_2atmpS1947 = _M0L2d2S380 % 10;
      _M0L6_2atmpS1946 = 48 + _M0L6_2atmpS1947;
      _M0L6d2__loS384 = (uint16_t)_M0L6_2atmpS1946;
      _M0L6_2atmpS1936 = _M0Lm6offsetS374;
      _M0L6_2atmpS1935 = _M0L12digit__startS376 + _M0L6_2atmpS1936;
      _M0L6bufferS385[_M0L6_2atmpS1935] = _M0L6d1__hiS381;
      _M0L6_2atmpS1939 = _M0Lm6offsetS374;
      _M0L6_2atmpS1938 = _M0L12digit__startS376 + _M0L6_2atmpS1939;
      _M0L6_2atmpS1937 = _M0L6_2atmpS1938 + 1;
      _M0L6bufferS385[_M0L6_2atmpS1937] = _M0L6d1__loS382;
      _M0L6_2atmpS1942 = _M0Lm6offsetS374;
      _M0L6_2atmpS1941 = _M0L12digit__startS376 + _M0L6_2atmpS1942;
      _M0L6_2atmpS1940 = _M0L6_2atmpS1941 + 2;
      _M0L6bufferS385[_M0L6_2atmpS1940] = _M0L6d2__hiS383;
      _M0L6_2atmpS1945 = _M0Lm6offsetS374;
      _M0L6_2atmpS1944 = _M0L12digit__startS376 + _M0L6_2atmpS1945;
      _M0L6_2atmpS1943 = _M0L6_2atmpS1944 + 3;
      _M0L6bufferS385[_M0L6_2atmpS1943] = _M0L6d2__loS384;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1989 = _M0Lm3numS372;
  _M0Lm9remainingS387 = (int32_t)_M0L6_2atmpS1989;
  while (1) {
    int32_t _M0L6_2atmpS1957 = _M0Lm9remainingS387;
    if (_M0L6_2atmpS1957 >= 100) {
      int32_t _M0L6_2atmpS1969 = _M0Lm9remainingS387;
      int32_t _M0L1tS388 = _M0L6_2atmpS1969 / 100;
      int32_t _M0L6_2atmpS1968 = _M0Lm9remainingS387;
      int32_t _M0L1dS389 = _M0L6_2atmpS1968 % 100;
      int32_t _M0L6_2atmpS1958;
      int32_t _M0L6_2atmpS1967;
      int32_t _M0L6_2atmpS1966;
      int32_t _M0L5d__hiS390;
      int32_t _M0L6_2atmpS1965;
      int32_t _M0L6_2atmpS1964;
      int32_t _M0L5d__loS391;
      int32_t _M0L6_2atmpS1960;
      int32_t _M0L6_2atmpS1959;
      int32_t _M0L6_2atmpS1963;
      int32_t _M0L6_2atmpS1962;
      int32_t _M0L6_2atmpS1961;
      _M0Lm9remainingS387 = _M0L1tS388;
      _M0L6_2atmpS1958 = _M0Lm6offsetS374;
      _M0Lm6offsetS374 = _M0L6_2atmpS1958 - 2;
      _M0L6_2atmpS1967 = _M0L1dS389 / 10;
      _M0L6_2atmpS1966 = 48 + _M0L6_2atmpS1967;
      _M0L5d__hiS390 = (uint16_t)_M0L6_2atmpS1966;
      _M0L6_2atmpS1965 = _M0L1dS389 % 10;
      _M0L6_2atmpS1964 = 48 + _M0L6_2atmpS1965;
      _M0L5d__loS391 = (uint16_t)_M0L6_2atmpS1964;
      _M0L6_2atmpS1960 = _M0Lm6offsetS374;
      _M0L6_2atmpS1959 = _M0L12digit__startS376 + _M0L6_2atmpS1960;
      _M0L6bufferS385[_M0L6_2atmpS1959] = _M0L5d__hiS390;
      _M0L6_2atmpS1963 = _M0Lm6offsetS374;
      _M0L6_2atmpS1962 = _M0L12digit__startS376 + _M0L6_2atmpS1963;
      _M0L6_2atmpS1961 = _M0L6_2atmpS1962 + 1;
      _M0L6bufferS385[_M0L6_2atmpS1961] = _M0L5d__loS391;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1970 = _M0Lm9remainingS387;
  if (_M0L6_2atmpS1970 >= 10) {
    int32_t _M0L6_2atmpS1971 = _M0Lm6offsetS374;
    int32_t _M0L6_2atmpS1982;
    int32_t _M0L6_2atmpS1981;
    int32_t _M0L6_2atmpS1980;
    int32_t _M0L5d__hiS393;
    int32_t _M0L6_2atmpS1979;
    int32_t _M0L6_2atmpS1978;
    int32_t _M0L6_2atmpS1977;
    int32_t _M0L5d__loS394;
    int32_t _M0L6_2atmpS1973;
    int32_t _M0L6_2atmpS1972;
    int32_t _M0L6_2atmpS1976;
    int32_t _M0L6_2atmpS1975;
    int32_t _M0L6_2atmpS1974;
    _M0Lm6offsetS374 = _M0L6_2atmpS1971 - 2;
    _M0L6_2atmpS1982 = _M0Lm9remainingS387;
    _M0L6_2atmpS1981 = _M0L6_2atmpS1982 / 10;
    _M0L6_2atmpS1980 = 48 + _M0L6_2atmpS1981;
    _M0L5d__hiS393 = (uint16_t)_M0L6_2atmpS1980;
    _M0L6_2atmpS1979 = _M0Lm9remainingS387;
    _M0L6_2atmpS1978 = _M0L6_2atmpS1979 % 10;
    _M0L6_2atmpS1977 = 48 + _M0L6_2atmpS1978;
    _M0L5d__loS394 = (uint16_t)_M0L6_2atmpS1977;
    _M0L6_2atmpS1973 = _M0Lm6offsetS374;
    _M0L6_2atmpS1972 = _M0L12digit__startS376 + _M0L6_2atmpS1973;
    _M0L6bufferS385[_M0L6_2atmpS1972] = _M0L5d__hiS393;
    _M0L6_2atmpS1976 = _M0Lm6offsetS374;
    _M0L6_2atmpS1975 = _M0L12digit__startS376 + _M0L6_2atmpS1976;
    _M0L6_2atmpS1974 = _M0L6_2atmpS1975 + 1;
    _M0L6bufferS385[_M0L6_2atmpS1974] = _M0L5d__loS394;
    moonbit_decref(_M0L6bufferS385);
  } else {
    int32_t _M0L6_2atmpS1983 = _M0Lm6offsetS374;
    int32_t _M0L6_2atmpS1988;
    int32_t _M0L6_2atmpS1984;
    int32_t _M0L6_2atmpS1987;
    int32_t _M0L6_2atmpS1986;
    int32_t _M0L6_2atmpS1985;
    _M0Lm6offsetS374 = _M0L6_2atmpS1983 - 1;
    _M0L6_2atmpS1988 = _M0Lm6offsetS374;
    _M0L6_2atmpS1984 = _M0L12digit__startS376 + _M0L6_2atmpS1988;
    _M0L6_2atmpS1987 = _M0Lm9remainingS387;
    _M0L6_2atmpS1986 = 48 + _M0L6_2atmpS1987;
    _M0L6_2atmpS1985 = (uint16_t)_M0L6_2atmpS1986;
    _M0L6bufferS385[_M0L6_2atmpS1984] = _M0L6_2atmpS1985;
    moonbit_decref(_M0L6bufferS385);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS367,
  uint64_t _M0L3numS361,
  int32_t _M0L12digit__startS359,
  int32_t _M0L10total__lenS358,
  int32_t _M0L5radixS363
) {
  int32_t _M0Lm6offsetS357;
  uint64_t _M0Lm1nS360;
  uint64_t _M0L4baseS362;
  int32_t _M0L6_2atmpS1915;
  int32_t _M0L6_2atmpS1914;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS357 = _M0L10total__lenS358 - _M0L12digit__startS359;
  _M0Lm1nS360 = _M0L3numS361;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS362 = _M0MPC13int3Int10to__uint64(_M0L5radixS363);
  _M0L6_2atmpS1915 = _M0L5radixS363 - 1;
  _M0L6_2atmpS1914 = _M0L5radixS363 & _M0L6_2atmpS1915;
  if (_M0L6_2atmpS1914 == 0) {
    int32_t _M0L5shiftS364;
    uint64_t _M0L4maskS365;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS364 = moonbit_ctz32(_M0L5radixS363);
    _M0L4maskS365 = _M0L4baseS362 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1916 = _M0Lm1nS360;
      if (_M0L6_2atmpS1916 > 0ull) {
        int32_t _M0L6_2atmpS1917 = _M0Lm6offsetS357;
        uint64_t _M0L6_2atmpS1923;
        uint64_t _M0L6_2atmpS1922;
        int32_t _M0L5digitS366;
        int32_t _M0L6_2atmpS1920;
        int32_t _M0L6_2atmpS1918;
        int32_t _M0L6_2atmpS1919;
        uint64_t _M0L6_2atmpS1921;
        _M0Lm6offsetS357 = _M0L6_2atmpS1917 - 1;
        _M0L6_2atmpS1923 = _M0Lm1nS360;
        _M0L6_2atmpS1922 = _M0L6_2atmpS1923 & _M0L4maskS365;
        _M0L5digitS366 = (int32_t)_M0L6_2atmpS1922;
        _M0L6_2atmpS1920 = _M0Lm6offsetS357;
        _M0L6_2atmpS1918 = _M0L12digit__startS359 + _M0L6_2atmpS1920;
        _M0L6_2atmpS1919
        = ((moonbit_string_t)moonbit_string_literal_81.data)[
          _M0L5digitS366
        ];
        _M0L6bufferS367[_M0L6_2atmpS1918] = _M0L6_2atmpS1919;
        _M0L6_2atmpS1921 = _M0Lm1nS360;
        _M0Lm1nS360 = _M0L6_2atmpS1921 >> (_M0L5shiftS364 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS367);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1924 = _M0Lm1nS360;
      if (_M0L6_2atmpS1924 > 0ull) {
        int32_t _M0L6_2atmpS1925 = _M0Lm6offsetS357;
        uint64_t _M0L6_2atmpS1932;
        uint64_t _M0L1qS369;
        uint64_t _M0L6_2atmpS1930;
        uint64_t _M0L6_2atmpS1931;
        uint64_t _M0L6_2atmpS1929;
        int32_t _M0L5digitS370;
        int32_t _M0L6_2atmpS1928;
        int32_t _M0L6_2atmpS1926;
        int32_t _M0L6_2atmpS1927;
        _M0Lm6offsetS357 = _M0L6_2atmpS1925 - 1;
        _M0L6_2atmpS1932 = _M0Lm1nS360;
        _M0L1qS369 = _M0L6_2atmpS1932 / _M0L4baseS362;
        _M0L6_2atmpS1930 = _M0Lm1nS360;
        _M0L6_2atmpS1931 = _M0L1qS369 * _M0L4baseS362;
        _M0L6_2atmpS1929 = _M0L6_2atmpS1930 - _M0L6_2atmpS1931;
        _M0L5digitS370 = (int32_t)_M0L6_2atmpS1929;
        _M0L6_2atmpS1928 = _M0Lm6offsetS357;
        _M0L6_2atmpS1926 = _M0L12digit__startS359 + _M0L6_2atmpS1928;
        _M0L6_2atmpS1927
        = ((moonbit_string_t)moonbit_string_literal_81.data)[
          _M0L5digitS370
        ];
        _M0L6bufferS367[_M0L6_2atmpS1926] = _M0L6_2atmpS1927;
        _M0Lm1nS360 = _M0L1qS369;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS367);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS354,
  uint64_t _M0L3numS350,
  int32_t _M0L12digit__startS348,
  int32_t _M0L10total__lenS347
) {
  int32_t _M0Lm6offsetS346;
  uint64_t _M0Lm1nS349;
  int32_t _M0L6_2atmpS1910;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS346 = _M0L10total__lenS347 - _M0L12digit__startS348;
  _M0Lm1nS349 = _M0L3numS350;
  while (1) {
    int32_t _M0L6_2atmpS1898 = _M0Lm6offsetS346;
    if (_M0L6_2atmpS1898 >= 2) {
      int32_t _M0L6_2atmpS1899 = _M0Lm6offsetS346;
      uint64_t _M0L6_2atmpS1909;
      uint64_t _M0L6_2atmpS1908;
      int32_t _M0L9byte__valS351;
      int32_t _M0L2hiS352;
      int32_t _M0L2loS353;
      int32_t _M0L6_2atmpS1902;
      int32_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1901;
      int32_t _M0L6_2atmpS1906;
      int32_t _M0L6_2atmpS1905;
      int32_t _M0L6_2atmpS1903;
      int32_t _M0L6_2atmpS1904;
      uint64_t _M0L6_2atmpS1907;
      _M0Lm6offsetS346 = _M0L6_2atmpS1899 - 2;
      _M0L6_2atmpS1909 = _M0Lm1nS349;
      _M0L6_2atmpS1908 = _M0L6_2atmpS1909 & 255ull;
      _M0L9byte__valS351 = (int32_t)_M0L6_2atmpS1908;
      _M0L2hiS352 = _M0L9byte__valS351 / 16;
      _M0L2loS353 = _M0L9byte__valS351 % 16;
      _M0L6_2atmpS1902 = _M0Lm6offsetS346;
      _M0L6_2atmpS1900 = _M0L12digit__startS348 + _M0L6_2atmpS1902;
      _M0L6_2atmpS1901
      = ((moonbit_string_t)moonbit_string_literal_81.data)[
        _M0L2hiS352
      ];
      _M0L6bufferS354[_M0L6_2atmpS1900] = _M0L6_2atmpS1901;
      _M0L6_2atmpS1906 = _M0Lm6offsetS346;
      _M0L6_2atmpS1905 = _M0L12digit__startS348 + _M0L6_2atmpS1906;
      _M0L6_2atmpS1903 = _M0L6_2atmpS1905 + 1;
      _M0L6_2atmpS1904
      = ((moonbit_string_t)moonbit_string_literal_81.data)[
        _M0L2loS353
      ];
      _M0L6bufferS354[_M0L6_2atmpS1903] = _M0L6_2atmpS1904;
      _M0L6_2atmpS1907 = _M0Lm1nS349;
      _M0Lm1nS349 = _M0L6_2atmpS1907 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1910 = _M0Lm6offsetS346;
  if (_M0L6_2atmpS1910 == 1) {
    uint64_t _M0L6_2atmpS1913 = _M0Lm1nS349;
    uint64_t _M0L6_2atmpS1912 = _M0L6_2atmpS1913 & 15ull;
    int32_t _M0L6nibbleS356 = (int32_t)_M0L6_2atmpS1912;
    int32_t _M0L6_2atmpS1911 =
      ((moonbit_string_t)moonbit_string_literal_81.data)[_M0L6nibbleS356];
    _M0L6bufferS354[_M0L12digit__startS348] = _M0L6_2atmpS1911;
    moonbit_decref(_M0L6bufferS354);
  } else {
    moonbit_decref(_M0L6bufferS354);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS340,
  int32_t _M0L5radixS343
) {
  uint64_t _M0Lm3numS341;
  uint64_t _M0L4baseS342;
  int32_t _M0Lm5countS344;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS340 == 0ull) {
    return 1;
  }
  _M0Lm3numS341 = _M0L5valueS340;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS342 = _M0MPC13int3Int10to__uint64(_M0L5radixS343);
  _M0Lm5countS344 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1895 = _M0Lm3numS341;
    if (_M0L6_2atmpS1895 > 0ull) {
      int32_t _M0L6_2atmpS1896 = _M0Lm5countS344;
      uint64_t _M0L6_2atmpS1897;
      _M0Lm5countS344 = _M0L6_2atmpS1896 + 1;
      _M0L6_2atmpS1897 = _M0Lm3numS341;
      _M0Lm3numS341 = _M0L6_2atmpS1897 / _M0L4baseS342;
      continue;
    }
    break;
  }
  return _M0Lm5countS344;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS338) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS338 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS339;
    int32_t _M0L6_2atmpS1894;
    int32_t _M0L6_2atmpS1893;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS339 = moonbit_clz64(_M0L5valueS338);
    _M0L6_2atmpS1894 = 63 - _M0L14leading__zerosS339;
    _M0L6_2atmpS1893 = _M0L6_2atmpS1894 / 4;
    return _M0L6_2atmpS1893 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS337) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS337 >= 10000000000ull) {
    if (_M0L5valueS337 >= 100000000000000ull) {
      if (_M0L5valueS337 >= 10000000000000000ull) {
        if (_M0L5valueS337 >= 1000000000000000000ull) {
          if (_M0L5valueS337 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS337 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS337 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS337 >= 1000000000000ull) {
      if (_M0L5valueS337 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS337 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS337 >= 100000ull) {
    if (_M0L5valueS337 >= 10000000ull) {
      if (_M0L5valueS337 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS337 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS337 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS337 >= 1000ull) {
    if (_M0L5valueS337 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS337 >= 100ull) {
    return 3;
  } else if (_M0L5valueS337 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS321,
  int32_t _M0L5radixS320
) {
  int32_t _if__result_4034;
  int32_t _M0L12is__negativeS322;
  uint32_t _M0L3numS323;
  uint16_t* _M0L6bufferS324;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS320 < 2) {
    _if__result_4034 = 1;
  } else {
    _if__result_4034 = _M0L5radixS320 > 36;
  }
  if (_if__result_4034) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_79.data, (moonbit_string_t)moonbit_string_literal_82.data);
  }
  if (_M0L4selfS321 == 0) {
    return (moonbit_string_t)moonbit_string_literal_65.data;
  }
  _M0L12is__negativeS322 = _M0L4selfS321 < 0;
  if (_M0L12is__negativeS322) {
    int32_t _M0L6_2atmpS1892 = -_M0L4selfS321;
    _M0L3numS323 = *(uint32_t*)&_M0L6_2atmpS1892;
  } else {
    _M0L3numS323 = *(uint32_t*)&_M0L4selfS321;
  }
  switch (_M0L5radixS320) {
    case 10: {
      int32_t _M0L10digit__lenS325;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L10total__lenS326;
      uint16_t* _M0L6bufferS327;
      int32_t _M0L12digit__startS328;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS325 = _M0FPB12dec__count32(_M0L3numS323);
      if (_M0L12is__negativeS322) {
        _M0L6_2atmpS1889 = 1;
      } else {
        _M0L6_2atmpS1889 = 0;
      }
      _M0L10total__lenS326 = _M0L10digit__lenS325 + _M0L6_2atmpS1889;
      _M0L6bufferS327
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS326, 0);
      if (_M0L12is__negativeS322) {
        _M0L12digit__startS328 = 1;
      } else {
        _M0L12digit__startS328 = 0;
      }
      moonbit_incref(_M0L6bufferS327);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS327, _M0L3numS323, _M0L12digit__startS328, _M0L10total__lenS326);
      _M0L6bufferS324 = _M0L6bufferS327;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS329;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L10total__lenS330;
      uint16_t* _M0L6bufferS331;
      int32_t _M0L12digit__startS332;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS329 = _M0FPB12hex__count32(_M0L3numS323);
      if (_M0L12is__negativeS322) {
        _M0L6_2atmpS1890 = 1;
      } else {
        _M0L6_2atmpS1890 = 0;
      }
      _M0L10total__lenS330 = _M0L10digit__lenS329 + _M0L6_2atmpS1890;
      _M0L6bufferS331
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS330, 0);
      if (_M0L12is__negativeS322) {
        _M0L12digit__startS332 = 1;
      } else {
        _M0L12digit__startS332 = 0;
      }
      moonbit_incref(_M0L6bufferS331);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS331, _M0L3numS323, _M0L12digit__startS332, _M0L10total__lenS330);
      _M0L6bufferS324 = _M0L6bufferS331;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS333;
      int32_t _M0L6_2atmpS1891;
      int32_t _M0L10total__lenS334;
      uint16_t* _M0L6bufferS335;
      int32_t _M0L12digit__startS336;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS333
      = _M0FPB14radix__count32(_M0L3numS323, _M0L5radixS320);
      if (_M0L12is__negativeS322) {
        _M0L6_2atmpS1891 = 1;
      } else {
        _M0L6_2atmpS1891 = 0;
      }
      _M0L10total__lenS334 = _M0L10digit__lenS333 + _M0L6_2atmpS1891;
      _M0L6bufferS335
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS334, 0);
      if (_M0L12is__negativeS322) {
        _M0L12digit__startS336 = 1;
      } else {
        _M0L12digit__startS336 = 0;
      }
      moonbit_incref(_M0L6bufferS335);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS335, _M0L3numS323, _M0L12digit__startS336, _M0L10total__lenS334, _M0L5radixS320);
      _M0L6bufferS324 = _M0L6bufferS335;
      break;
    }
  }
  if (_M0L12is__negativeS322) {
    _M0L6bufferS324[0] = 45;
  }
  return _M0L6bufferS324;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS314,
  int32_t _M0L5radixS317
) {
  uint32_t _M0Lm3numS315;
  uint32_t _M0L4baseS316;
  int32_t _M0Lm5countS318;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS314 == 0u) {
    return 1;
  }
  _M0Lm3numS315 = _M0L5valueS314;
  _M0L4baseS316 = *(uint32_t*)&_M0L5radixS317;
  _M0Lm5countS318 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1886 = _M0Lm3numS315;
    if (_M0L6_2atmpS1886 > 0u) {
      int32_t _M0L6_2atmpS1887 = _M0Lm5countS318;
      uint32_t _M0L6_2atmpS1888;
      _M0Lm5countS318 = _M0L6_2atmpS1887 + 1;
      _M0L6_2atmpS1888 = _M0Lm3numS315;
      _M0Lm3numS315 = _M0L6_2atmpS1888 / _M0L4baseS316;
      continue;
    }
    break;
  }
  return _M0Lm5countS318;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS312) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS312 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS313;
    int32_t _M0L6_2atmpS1885;
    int32_t _M0L6_2atmpS1884;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS313 = moonbit_clz32(_M0L5valueS312);
    _M0L6_2atmpS1885 = 31 - _M0L14leading__zerosS313;
    _M0L6_2atmpS1884 = _M0L6_2atmpS1885 / 4;
    return _M0L6_2atmpS1884 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS311) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS311 >= 100000u) {
    if (_M0L5valueS311 >= 10000000u) {
      if (_M0L5valueS311 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS311 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS311 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS311 >= 1000u) {
    if (_M0L5valueS311 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS311 >= 100u) {
    return 3;
  } else if (_M0L5valueS311 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS301,
  uint32_t _M0L3numS289,
  int32_t _M0L12digit__startS292,
  int32_t _M0L10total__lenS291
) {
  uint32_t _M0Lm3numS288;
  int32_t _M0Lm6offsetS290;
  uint32_t _M0L6_2atmpS1883;
  int32_t _M0Lm9remainingS303;
  int32_t _M0L6_2atmpS1864;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS288 = _M0L3numS289;
  _M0Lm6offsetS290 = _M0L10total__lenS291 - _M0L12digit__startS292;
  while (1) {
    uint32_t _M0L6_2atmpS1827 = _M0Lm3numS288;
    if (_M0L6_2atmpS1827 >= 10000u) {
      uint32_t _M0L6_2atmpS1850 = _M0Lm3numS288;
      uint32_t _M0L1tS293 = _M0L6_2atmpS1850 / 10000u;
      uint32_t _M0L6_2atmpS1849 = _M0Lm3numS288;
      uint32_t _M0L6_2atmpS1848 = _M0L6_2atmpS1849 % 10000u;
      int32_t _M0L1rS294 = *(int32_t*)&_M0L6_2atmpS1848;
      int32_t _M0L2d1S295;
      int32_t _M0L2d2S296;
      int32_t _M0L6_2atmpS1828;
      int32_t _M0L6_2atmpS1847;
      int32_t _M0L6_2atmpS1846;
      int32_t _M0L6d1__hiS297;
      int32_t _M0L6_2atmpS1845;
      int32_t _M0L6_2atmpS1844;
      int32_t _M0L6d1__loS298;
      int32_t _M0L6_2atmpS1843;
      int32_t _M0L6_2atmpS1842;
      int32_t _M0L6d2__hiS299;
      int32_t _M0L6_2atmpS1841;
      int32_t _M0L6_2atmpS1840;
      int32_t _M0L6d2__loS300;
      int32_t _M0L6_2atmpS1830;
      int32_t _M0L6_2atmpS1829;
      int32_t _M0L6_2atmpS1833;
      int32_t _M0L6_2atmpS1832;
      int32_t _M0L6_2atmpS1831;
      int32_t _M0L6_2atmpS1836;
      int32_t _M0L6_2atmpS1835;
      int32_t _M0L6_2atmpS1834;
      int32_t _M0L6_2atmpS1839;
      int32_t _M0L6_2atmpS1838;
      int32_t _M0L6_2atmpS1837;
      _M0Lm3numS288 = _M0L1tS293;
      _M0L2d1S295 = _M0L1rS294 / 100;
      _M0L2d2S296 = _M0L1rS294 % 100;
      _M0L6_2atmpS1828 = _M0Lm6offsetS290;
      _M0Lm6offsetS290 = _M0L6_2atmpS1828 - 4;
      _M0L6_2atmpS1847 = _M0L2d1S295 / 10;
      _M0L6_2atmpS1846 = 48 + _M0L6_2atmpS1847;
      _M0L6d1__hiS297 = (uint16_t)_M0L6_2atmpS1846;
      _M0L6_2atmpS1845 = _M0L2d1S295 % 10;
      _M0L6_2atmpS1844 = 48 + _M0L6_2atmpS1845;
      _M0L6d1__loS298 = (uint16_t)_M0L6_2atmpS1844;
      _M0L6_2atmpS1843 = _M0L2d2S296 / 10;
      _M0L6_2atmpS1842 = 48 + _M0L6_2atmpS1843;
      _M0L6d2__hiS299 = (uint16_t)_M0L6_2atmpS1842;
      _M0L6_2atmpS1841 = _M0L2d2S296 % 10;
      _M0L6_2atmpS1840 = 48 + _M0L6_2atmpS1841;
      _M0L6d2__loS300 = (uint16_t)_M0L6_2atmpS1840;
      _M0L6_2atmpS1830 = _M0Lm6offsetS290;
      _M0L6_2atmpS1829 = _M0L12digit__startS292 + _M0L6_2atmpS1830;
      _M0L6bufferS301[_M0L6_2atmpS1829] = _M0L6d1__hiS297;
      _M0L6_2atmpS1833 = _M0Lm6offsetS290;
      _M0L6_2atmpS1832 = _M0L12digit__startS292 + _M0L6_2atmpS1833;
      _M0L6_2atmpS1831 = _M0L6_2atmpS1832 + 1;
      _M0L6bufferS301[_M0L6_2atmpS1831] = _M0L6d1__loS298;
      _M0L6_2atmpS1836 = _M0Lm6offsetS290;
      _M0L6_2atmpS1835 = _M0L12digit__startS292 + _M0L6_2atmpS1836;
      _M0L6_2atmpS1834 = _M0L6_2atmpS1835 + 2;
      _M0L6bufferS301[_M0L6_2atmpS1834] = _M0L6d2__hiS299;
      _M0L6_2atmpS1839 = _M0Lm6offsetS290;
      _M0L6_2atmpS1838 = _M0L12digit__startS292 + _M0L6_2atmpS1839;
      _M0L6_2atmpS1837 = _M0L6_2atmpS1838 + 3;
      _M0L6bufferS301[_M0L6_2atmpS1837] = _M0L6d2__loS300;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1883 = _M0Lm3numS288;
  _M0Lm9remainingS303 = *(int32_t*)&_M0L6_2atmpS1883;
  while (1) {
    int32_t _M0L6_2atmpS1851 = _M0Lm9remainingS303;
    if (_M0L6_2atmpS1851 >= 100) {
      int32_t _M0L6_2atmpS1863 = _M0Lm9remainingS303;
      int32_t _M0L1tS304 = _M0L6_2atmpS1863 / 100;
      int32_t _M0L6_2atmpS1862 = _M0Lm9remainingS303;
      int32_t _M0L1dS305 = _M0L6_2atmpS1862 % 100;
      int32_t _M0L6_2atmpS1852;
      int32_t _M0L6_2atmpS1861;
      int32_t _M0L6_2atmpS1860;
      int32_t _M0L5d__hiS306;
      int32_t _M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1858;
      int32_t _M0L5d__loS307;
      int32_t _M0L6_2atmpS1854;
      int32_t _M0L6_2atmpS1853;
      int32_t _M0L6_2atmpS1857;
      int32_t _M0L6_2atmpS1856;
      int32_t _M0L6_2atmpS1855;
      _M0Lm9remainingS303 = _M0L1tS304;
      _M0L6_2atmpS1852 = _M0Lm6offsetS290;
      _M0Lm6offsetS290 = _M0L6_2atmpS1852 - 2;
      _M0L6_2atmpS1861 = _M0L1dS305 / 10;
      _M0L6_2atmpS1860 = 48 + _M0L6_2atmpS1861;
      _M0L5d__hiS306 = (uint16_t)_M0L6_2atmpS1860;
      _M0L6_2atmpS1859 = _M0L1dS305 % 10;
      _M0L6_2atmpS1858 = 48 + _M0L6_2atmpS1859;
      _M0L5d__loS307 = (uint16_t)_M0L6_2atmpS1858;
      _M0L6_2atmpS1854 = _M0Lm6offsetS290;
      _M0L6_2atmpS1853 = _M0L12digit__startS292 + _M0L6_2atmpS1854;
      _M0L6bufferS301[_M0L6_2atmpS1853] = _M0L5d__hiS306;
      _M0L6_2atmpS1857 = _M0Lm6offsetS290;
      _M0L6_2atmpS1856 = _M0L12digit__startS292 + _M0L6_2atmpS1857;
      _M0L6_2atmpS1855 = _M0L6_2atmpS1856 + 1;
      _M0L6bufferS301[_M0L6_2atmpS1855] = _M0L5d__loS307;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1864 = _M0Lm9remainingS303;
  if (_M0L6_2atmpS1864 >= 10) {
    int32_t _M0L6_2atmpS1865 = _M0Lm6offsetS290;
    int32_t _M0L6_2atmpS1876;
    int32_t _M0L6_2atmpS1875;
    int32_t _M0L6_2atmpS1874;
    int32_t _M0L5d__hiS309;
    int32_t _M0L6_2atmpS1873;
    int32_t _M0L6_2atmpS1872;
    int32_t _M0L6_2atmpS1871;
    int32_t _M0L5d__loS310;
    int32_t _M0L6_2atmpS1867;
    int32_t _M0L6_2atmpS1866;
    int32_t _M0L6_2atmpS1870;
    int32_t _M0L6_2atmpS1869;
    int32_t _M0L6_2atmpS1868;
    _M0Lm6offsetS290 = _M0L6_2atmpS1865 - 2;
    _M0L6_2atmpS1876 = _M0Lm9remainingS303;
    _M0L6_2atmpS1875 = _M0L6_2atmpS1876 / 10;
    _M0L6_2atmpS1874 = 48 + _M0L6_2atmpS1875;
    _M0L5d__hiS309 = (uint16_t)_M0L6_2atmpS1874;
    _M0L6_2atmpS1873 = _M0Lm9remainingS303;
    _M0L6_2atmpS1872 = _M0L6_2atmpS1873 % 10;
    _M0L6_2atmpS1871 = 48 + _M0L6_2atmpS1872;
    _M0L5d__loS310 = (uint16_t)_M0L6_2atmpS1871;
    _M0L6_2atmpS1867 = _M0Lm6offsetS290;
    _M0L6_2atmpS1866 = _M0L12digit__startS292 + _M0L6_2atmpS1867;
    _M0L6bufferS301[_M0L6_2atmpS1866] = _M0L5d__hiS309;
    _M0L6_2atmpS1870 = _M0Lm6offsetS290;
    _M0L6_2atmpS1869 = _M0L12digit__startS292 + _M0L6_2atmpS1870;
    _M0L6_2atmpS1868 = _M0L6_2atmpS1869 + 1;
    _M0L6bufferS301[_M0L6_2atmpS1868] = _M0L5d__loS310;
    moonbit_decref(_M0L6bufferS301);
  } else {
    int32_t _M0L6_2atmpS1877 = _M0Lm6offsetS290;
    int32_t _M0L6_2atmpS1882;
    int32_t _M0L6_2atmpS1878;
    int32_t _M0L6_2atmpS1881;
    int32_t _M0L6_2atmpS1880;
    int32_t _M0L6_2atmpS1879;
    _M0Lm6offsetS290 = _M0L6_2atmpS1877 - 1;
    _M0L6_2atmpS1882 = _M0Lm6offsetS290;
    _M0L6_2atmpS1878 = _M0L12digit__startS292 + _M0L6_2atmpS1882;
    _M0L6_2atmpS1881 = _M0Lm9remainingS303;
    _M0L6_2atmpS1880 = 48 + _M0L6_2atmpS1881;
    _M0L6_2atmpS1879 = (uint16_t)_M0L6_2atmpS1880;
    _M0L6bufferS301[_M0L6_2atmpS1878] = _M0L6_2atmpS1879;
    moonbit_decref(_M0L6bufferS301);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS283,
  uint32_t _M0L3numS277,
  int32_t _M0L12digit__startS275,
  int32_t _M0L10total__lenS274,
  int32_t _M0L5radixS279
) {
  int32_t _M0Lm6offsetS273;
  uint32_t _M0Lm1nS276;
  uint32_t _M0L4baseS278;
  int32_t _M0L6_2atmpS1809;
  int32_t _M0L6_2atmpS1808;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS273 = _M0L10total__lenS274 - _M0L12digit__startS275;
  _M0Lm1nS276 = _M0L3numS277;
  _M0L4baseS278 = *(uint32_t*)&_M0L5radixS279;
  _M0L6_2atmpS1809 = _M0L5radixS279 - 1;
  _M0L6_2atmpS1808 = _M0L5radixS279 & _M0L6_2atmpS1809;
  if (_M0L6_2atmpS1808 == 0) {
    int32_t _M0L5shiftS280;
    uint32_t _M0L4maskS281;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS280 = moonbit_ctz32(_M0L5radixS279);
    _M0L4maskS281 = _M0L4baseS278 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1810 = _M0Lm1nS276;
      if (_M0L6_2atmpS1810 > 0u) {
        int32_t _M0L6_2atmpS1811 = _M0Lm6offsetS273;
        uint32_t _M0L6_2atmpS1817;
        uint32_t _M0L6_2atmpS1816;
        int32_t _M0L5digitS282;
        int32_t _M0L6_2atmpS1814;
        int32_t _M0L6_2atmpS1812;
        int32_t _M0L6_2atmpS1813;
        uint32_t _M0L6_2atmpS1815;
        _M0Lm6offsetS273 = _M0L6_2atmpS1811 - 1;
        _M0L6_2atmpS1817 = _M0Lm1nS276;
        _M0L6_2atmpS1816 = _M0L6_2atmpS1817 & _M0L4maskS281;
        _M0L5digitS282 = *(int32_t*)&_M0L6_2atmpS1816;
        _M0L6_2atmpS1814 = _M0Lm6offsetS273;
        _M0L6_2atmpS1812 = _M0L12digit__startS275 + _M0L6_2atmpS1814;
        _M0L6_2atmpS1813
        = ((moonbit_string_t)moonbit_string_literal_81.data)[
          _M0L5digitS282
        ];
        _M0L6bufferS283[_M0L6_2atmpS1812] = _M0L6_2atmpS1813;
        _M0L6_2atmpS1815 = _M0Lm1nS276;
        _M0Lm1nS276 = _M0L6_2atmpS1815 >> (_M0L5shiftS280 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS283);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1818 = _M0Lm1nS276;
      if (_M0L6_2atmpS1818 > 0u) {
        int32_t _M0L6_2atmpS1819 = _M0Lm6offsetS273;
        uint32_t _M0L6_2atmpS1826;
        uint32_t _M0L1qS285;
        uint32_t _M0L6_2atmpS1824;
        uint32_t _M0L6_2atmpS1825;
        uint32_t _M0L6_2atmpS1823;
        int32_t _M0L5digitS286;
        int32_t _M0L6_2atmpS1822;
        int32_t _M0L6_2atmpS1820;
        int32_t _M0L6_2atmpS1821;
        _M0Lm6offsetS273 = _M0L6_2atmpS1819 - 1;
        _M0L6_2atmpS1826 = _M0Lm1nS276;
        _M0L1qS285 = _M0L6_2atmpS1826 / _M0L4baseS278;
        _M0L6_2atmpS1824 = _M0Lm1nS276;
        _M0L6_2atmpS1825 = _M0L1qS285 * _M0L4baseS278;
        _M0L6_2atmpS1823 = _M0L6_2atmpS1824 - _M0L6_2atmpS1825;
        _M0L5digitS286 = *(int32_t*)&_M0L6_2atmpS1823;
        _M0L6_2atmpS1822 = _M0Lm6offsetS273;
        _M0L6_2atmpS1820 = _M0L12digit__startS275 + _M0L6_2atmpS1822;
        _M0L6_2atmpS1821
        = ((moonbit_string_t)moonbit_string_literal_81.data)[
          _M0L5digitS286
        ];
        _M0L6bufferS283[_M0L6_2atmpS1820] = _M0L6_2atmpS1821;
        _M0Lm1nS276 = _M0L1qS285;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS283);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS270,
  uint32_t _M0L3numS266,
  int32_t _M0L12digit__startS264,
  int32_t _M0L10total__lenS263
) {
  int32_t _M0Lm6offsetS262;
  uint32_t _M0Lm1nS265;
  int32_t _M0L6_2atmpS1804;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS262 = _M0L10total__lenS263 - _M0L12digit__startS264;
  _M0Lm1nS265 = _M0L3numS266;
  while (1) {
    int32_t _M0L6_2atmpS1792 = _M0Lm6offsetS262;
    if (_M0L6_2atmpS1792 >= 2) {
      int32_t _M0L6_2atmpS1793 = _M0Lm6offsetS262;
      uint32_t _M0L6_2atmpS1803;
      uint32_t _M0L6_2atmpS1802;
      int32_t _M0L9byte__valS267;
      int32_t _M0L2hiS268;
      int32_t _M0L2loS269;
      int32_t _M0L6_2atmpS1796;
      int32_t _M0L6_2atmpS1794;
      int32_t _M0L6_2atmpS1795;
      int32_t _M0L6_2atmpS1800;
      int32_t _M0L6_2atmpS1799;
      int32_t _M0L6_2atmpS1797;
      int32_t _M0L6_2atmpS1798;
      uint32_t _M0L6_2atmpS1801;
      _M0Lm6offsetS262 = _M0L6_2atmpS1793 - 2;
      _M0L6_2atmpS1803 = _M0Lm1nS265;
      _M0L6_2atmpS1802 = _M0L6_2atmpS1803 & 255u;
      _M0L9byte__valS267 = *(int32_t*)&_M0L6_2atmpS1802;
      _M0L2hiS268 = _M0L9byte__valS267 / 16;
      _M0L2loS269 = _M0L9byte__valS267 % 16;
      _M0L6_2atmpS1796 = _M0Lm6offsetS262;
      _M0L6_2atmpS1794 = _M0L12digit__startS264 + _M0L6_2atmpS1796;
      _M0L6_2atmpS1795
      = ((moonbit_string_t)moonbit_string_literal_81.data)[
        _M0L2hiS268
      ];
      _M0L6bufferS270[_M0L6_2atmpS1794] = _M0L6_2atmpS1795;
      _M0L6_2atmpS1800 = _M0Lm6offsetS262;
      _M0L6_2atmpS1799 = _M0L12digit__startS264 + _M0L6_2atmpS1800;
      _M0L6_2atmpS1797 = _M0L6_2atmpS1799 + 1;
      _M0L6_2atmpS1798
      = ((moonbit_string_t)moonbit_string_literal_81.data)[
        _M0L2loS269
      ];
      _M0L6bufferS270[_M0L6_2atmpS1797] = _M0L6_2atmpS1798;
      _M0L6_2atmpS1801 = _M0Lm1nS265;
      _M0Lm1nS265 = _M0L6_2atmpS1801 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1804 = _M0Lm6offsetS262;
  if (_M0L6_2atmpS1804 == 1) {
    uint32_t _M0L6_2atmpS1807 = _M0Lm1nS265;
    uint32_t _M0L6_2atmpS1806 = _M0L6_2atmpS1807 & 15u;
    int32_t _M0L6nibbleS272 = *(int32_t*)&_M0L6_2atmpS1806;
    int32_t _M0L6_2atmpS1805 =
      ((moonbit_string_t)moonbit_string_literal_81.data)[_M0L6nibbleS272];
    _M0L6bufferS270[_M0L12digit__startS264] = _M0L6_2atmpS1805;
    moonbit_decref(_M0L6bufferS270);
  } else {
    moonbit_decref(_M0L6bufferS270);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS255) {
  struct _M0TWEOs* _M0L7_2afuncS254;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS254 = _M0L4selfS255;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS254->code(_M0L7_2afuncS254);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS257
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS256;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS256 = _M0L4selfS257;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS256->code(_M0L7_2afuncS256);
}

struct _M0TUsRPC16string10StringViewE* _M0MPB4Iter4nextGUsRPC16string10StringViewEE(
  struct _M0TWEOUsRPC16string10StringViewE* _M0L4selfS259
) {
  struct _M0TWEOUsRPC16string10StringViewE* _M0L7_2afuncS258;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS258 = _M0L4selfS259;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS258->code(_M0L7_2afuncS258);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS261) {
  struct _M0TWEOc* _M0L7_2afuncS260;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS260 = _M0L4selfS261;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS260->code(_M0L7_2afuncS260);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS247
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS246;
  struct _M0TPB6Logger _M0L6_2atmpS1788;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS246 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS246);
  _M0L6_2atmpS1788
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS246
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS247, _M0L6_2atmpS1788);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS246);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS249
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS248;
  struct _M0TPB6Logger _M0L6_2atmpS1789;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS248 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS248);
  _M0L6_2atmpS1789
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS248
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS249, _M0L6_2atmpS1789);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS248);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS251
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS250;
  struct _M0TPB6Logger _M0L6_2atmpS1790;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS250 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS250);
  _M0L6_2atmpS1790
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS250
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS251, _M0L6_2atmpS1790);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS250);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS253
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS252;
  struct _M0TPB6Logger _M0L6_2atmpS1791;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS252 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS252);
  _M0L6_2atmpS1791
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS252
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS253, _M0L6_2atmpS1791);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS252);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS245
) {
  int32_t _M0L8_2afieldS3716;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3716 = _M0L4selfS245.$1;
  moonbit_decref(_M0L4selfS245.$0);
  return _M0L8_2afieldS3716;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS244
) {
  int32_t _M0L3endS1786;
  int32_t _M0L8_2afieldS3717;
  int32_t _M0L5startS1787;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1786 = _M0L4selfS244.$2;
  _M0L8_2afieldS3717 = _M0L4selfS244.$1;
  moonbit_decref(_M0L4selfS244.$0);
  _M0L5startS1787 = _M0L8_2afieldS3717;
  return _M0L3endS1786 - _M0L5startS1787;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS243
) {
  moonbit_string_t _M0L8_2afieldS3718;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3718 = _M0L4selfS243.$0;
  return _M0L8_2afieldS3718;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS239,
  moonbit_string_t _M0L5valueS240,
  int32_t _M0L5startS241,
  int32_t _M0L3lenS242
) {
  int32_t _M0L6_2atmpS1785;
  int64_t _M0L6_2atmpS1784;
  struct _M0TPC16string10StringView _M0L6_2atmpS1783;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1785 = _M0L5startS241 + _M0L3lenS242;
  _M0L6_2atmpS1784 = (int64_t)_M0L6_2atmpS1785;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1783
  = _M0MPC16string6String11sub_2einner(_M0L5valueS240, _M0L5startS241, _M0L6_2atmpS1784);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS239, _M0L6_2atmpS1783);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS232,
  int32_t _M0L5startS238,
  int64_t _M0L3endS234
) {
  int32_t _M0L3lenS231;
  int32_t _M0L3endS233;
  int32_t _M0L5startS237;
  int32_t _if__result_4041;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS231 = Moonbit_array_length(_M0L4selfS232);
  if (_M0L3endS234 == 4294967296ll) {
    _M0L3endS233 = _M0L3lenS231;
  } else {
    int64_t _M0L7_2aSomeS235 = _M0L3endS234;
    int32_t _M0L6_2aendS236 = (int32_t)_M0L7_2aSomeS235;
    if (_M0L6_2aendS236 < 0) {
      _M0L3endS233 = _M0L3lenS231 + _M0L6_2aendS236;
    } else {
      _M0L3endS233 = _M0L6_2aendS236;
    }
  }
  if (_M0L5startS238 < 0) {
    _M0L5startS237 = _M0L3lenS231 + _M0L5startS238;
  } else {
    _M0L5startS237 = _M0L5startS238;
  }
  if (_M0L5startS237 >= 0) {
    if (_M0L5startS237 <= _M0L3endS233) {
      _if__result_4041 = _M0L3endS233 <= _M0L3lenS231;
    } else {
      _if__result_4041 = 0;
    }
  } else {
    _if__result_4041 = 0;
  }
  if (_if__result_4041) {
    if (_M0L5startS237 < _M0L3lenS231) {
      int32_t _M0L6_2atmpS1780 = _M0L4selfS232[_M0L5startS237];
      int32_t _M0L6_2atmpS1779;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1779
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1780);
      if (!_M0L6_2atmpS1779) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS233 < _M0L3lenS231) {
      int32_t _M0L6_2atmpS1782 = _M0L4selfS232[_M0L3endS233];
      int32_t _M0L6_2atmpS1781;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1781
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1782);
      if (!_M0L6_2atmpS1781) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS237,
                                                 _M0L3endS233,
                                                 _M0L4selfS232};
  } else {
    moonbit_decref(_M0L4selfS232);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS228) {
  struct _M0TPB6Hasher* _M0L1hS227;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS227 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS227);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS227, _M0L4selfS228);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS227);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS230
) {
  struct _M0TPB6Hasher* _M0L1hS229;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS229 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS229);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS229, _M0L4selfS230);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS229);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS225) {
  int32_t _M0L4seedS224;
  if (_M0L10seed_2eoptS225 == 4294967296ll) {
    _M0L4seedS224 = 0;
  } else {
    int64_t _M0L7_2aSomeS226 = _M0L10seed_2eoptS225;
    _M0L4seedS224 = (int32_t)_M0L7_2aSomeS226;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS224);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS223) {
  uint32_t _M0L6_2atmpS1778;
  uint32_t _M0L6_2atmpS1777;
  struct _M0TPB6Hasher* _block_4042;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1778 = *(uint32_t*)&_M0L4seedS223;
  _M0L6_2atmpS1777 = _M0L6_2atmpS1778 + 374761393u;
  _block_4042
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4042)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4042->$0 = _M0L6_2atmpS1777;
  return _block_4042;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS222) {
  uint32_t _M0L6_2atmpS1776;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1776 = _M0MPB6Hasher9avalanche(_M0L4selfS222);
  return *(int32_t*)&_M0L6_2atmpS1776;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS221) {
  uint32_t _M0L8_2afieldS3719;
  uint32_t _M0Lm3accS220;
  uint32_t _M0L6_2atmpS1765;
  uint32_t _M0L6_2atmpS1767;
  uint32_t _M0L6_2atmpS1766;
  uint32_t _M0L6_2atmpS1768;
  uint32_t _M0L6_2atmpS1769;
  uint32_t _M0L6_2atmpS1771;
  uint32_t _M0L6_2atmpS1770;
  uint32_t _M0L6_2atmpS1772;
  uint32_t _M0L6_2atmpS1773;
  uint32_t _M0L6_2atmpS1775;
  uint32_t _M0L6_2atmpS1774;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3719 = _M0L4selfS221->$0;
  moonbit_decref(_M0L4selfS221);
  _M0Lm3accS220 = _M0L8_2afieldS3719;
  _M0L6_2atmpS1765 = _M0Lm3accS220;
  _M0L6_2atmpS1767 = _M0Lm3accS220;
  _M0L6_2atmpS1766 = _M0L6_2atmpS1767 >> 15;
  _M0Lm3accS220 = _M0L6_2atmpS1765 ^ _M0L6_2atmpS1766;
  _M0L6_2atmpS1768 = _M0Lm3accS220;
  _M0Lm3accS220 = _M0L6_2atmpS1768 * 2246822519u;
  _M0L6_2atmpS1769 = _M0Lm3accS220;
  _M0L6_2atmpS1771 = _M0Lm3accS220;
  _M0L6_2atmpS1770 = _M0L6_2atmpS1771 >> 13;
  _M0Lm3accS220 = _M0L6_2atmpS1769 ^ _M0L6_2atmpS1770;
  _M0L6_2atmpS1772 = _M0Lm3accS220;
  _M0Lm3accS220 = _M0L6_2atmpS1772 * 3266489917u;
  _M0L6_2atmpS1773 = _M0Lm3accS220;
  _M0L6_2atmpS1775 = _M0Lm3accS220;
  _M0L6_2atmpS1774 = _M0L6_2atmpS1775 >> 16;
  _M0Lm3accS220 = _M0L6_2atmpS1773 ^ _M0L6_2atmpS1774;
  return _M0Lm3accS220;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS218,
  moonbit_string_t _M0L1yS219
) {
  int32_t _M0L6_2atmpS3720;
  int32_t _M0L6_2atmpS1764;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3720 = moonbit_val_array_equal(_M0L1xS218, _M0L1yS219);
  moonbit_decref(_M0L1xS218);
  moonbit_decref(_M0L1yS219);
  _M0L6_2atmpS1764 = _M0L6_2atmpS3720;
  return !_M0L6_2atmpS1764;
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
  int64_t _M0L6_2atmpS1763;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1763 = (int64_t)_M0L4selfS213;
  return *(uint64_t*)&_M0L6_2atmpS1763;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS211,
  int32_t _M0L5valueS212
) {
  uint32_t _M0L6_2atmpS1762;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1762 = *(uint32_t*)&_M0L5valueS212;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS211, _M0L6_2atmpS1762);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS204
) {
  struct _M0TPB13StringBuilder* _M0L3bufS202;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS203;
  int32_t _M0L7_2abindS205;
  int32_t _M0L1iS206;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS202 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS203 = _M0L4selfS204;
  moonbit_incref(_M0L3bufS202);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS202, 91);
  _M0L7_2abindS205 = _M0L7_2aselfS203->$1;
  _M0L1iS206 = 0;
  while (1) {
    if (_M0L1iS206 < _M0L7_2abindS205) {
      int32_t _if__result_4044;
      moonbit_string_t* _M0L8_2afieldS3722;
      moonbit_string_t* _M0L3bufS1760;
      moonbit_string_t _M0L6_2atmpS3721;
      moonbit_string_t _M0L4itemS207;
      int32_t _M0L6_2atmpS1761;
      if (_M0L1iS206 != 0) {
        moonbit_incref(_M0L3bufS202);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS202, (moonbit_string_t)moonbit_string_literal_83.data);
      }
      if (_M0L1iS206 < 0) {
        _if__result_4044 = 1;
      } else {
        int32_t _M0L3lenS1759 = _M0L7_2aselfS203->$1;
        _if__result_4044 = _M0L1iS206 >= _M0L3lenS1759;
      }
      if (_if__result_4044) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3722 = _M0L7_2aselfS203->$0;
      _M0L3bufS1760 = _M0L8_2afieldS3722;
      _M0L6_2atmpS3721 = (moonbit_string_t)_M0L3bufS1760[_M0L1iS206];
      _M0L4itemS207 = _M0L6_2atmpS3721;
      if (_M0L4itemS207 == 0) {
        moonbit_incref(_M0L3bufS202);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS202, (moonbit_string_t)moonbit_string_literal_45.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS208 = _M0L4itemS207;
        moonbit_string_t _M0L6_2alocS209 = _M0L7_2aSomeS208;
        moonbit_string_t _M0L6_2atmpS1758;
        moonbit_incref(_M0L6_2alocS209);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1758
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS209);
        moonbit_incref(_M0L3bufS202);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS202, _M0L6_2atmpS1758);
      }
      _M0L6_2atmpS1761 = _M0L1iS206 + 1;
      _M0L1iS206 = _M0L6_2atmpS1761;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS203);
    }
    break;
  }
  moonbit_incref(_M0L3bufS202);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS202, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS202);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS201
) {
  moonbit_string_t _M0L6_2atmpS1757;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1756;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1757 = _M0L4selfS201;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1756 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1757);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1756);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS200
) {
  struct _M0TPB13StringBuilder* _M0L2sbS199;
  struct _M0TPC16string10StringView _M0L8_2afieldS3735;
  struct _M0TPC16string10StringView _M0L3pkgS1741;
  moonbit_string_t _M0L6_2atmpS1740;
  moonbit_string_t _M0L6_2atmpS3734;
  moonbit_string_t _M0L6_2atmpS1739;
  moonbit_string_t _M0L6_2atmpS3733;
  moonbit_string_t _M0L6_2atmpS1738;
  struct _M0TPC16string10StringView _M0L8_2afieldS3732;
  struct _M0TPC16string10StringView _M0L8filenameS1742;
  struct _M0TPC16string10StringView _M0L8_2afieldS3731;
  struct _M0TPC16string10StringView _M0L11start__lineS1745;
  moonbit_string_t _M0L6_2atmpS1744;
  moonbit_string_t _M0L6_2atmpS3730;
  moonbit_string_t _M0L6_2atmpS1743;
  struct _M0TPC16string10StringView _M0L8_2afieldS3729;
  struct _M0TPC16string10StringView _M0L13start__columnS1748;
  moonbit_string_t _M0L6_2atmpS1747;
  moonbit_string_t _M0L6_2atmpS3728;
  moonbit_string_t _M0L6_2atmpS1746;
  struct _M0TPC16string10StringView _M0L8_2afieldS3727;
  struct _M0TPC16string10StringView _M0L9end__lineS1751;
  moonbit_string_t _M0L6_2atmpS1750;
  moonbit_string_t _M0L6_2atmpS3726;
  moonbit_string_t _M0L6_2atmpS1749;
  struct _M0TPC16string10StringView _M0L8_2afieldS3725;
  int32_t _M0L6_2acntS3866;
  struct _M0TPC16string10StringView _M0L11end__columnS1755;
  moonbit_string_t _M0L6_2atmpS1754;
  moonbit_string_t _M0L6_2atmpS3724;
  moonbit_string_t _M0L6_2atmpS1753;
  moonbit_string_t _M0L6_2atmpS3723;
  moonbit_string_t _M0L6_2atmpS1752;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS199 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3735
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$0_1, _M0L4selfS200->$0_2, _M0L4selfS200->$0_0
  };
  _M0L3pkgS1741 = _M0L8_2afieldS3735;
  moonbit_incref(_M0L3pkgS1741.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1740
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1741);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3734
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_84.data, _M0L6_2atmpS1740);
  moonbit_decref(_M0L6_2atmpS1740);
  _M0L6_2atmpS1739 = _M0L6_2atmpS3734;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3733
  = moonbit_add_string(_M0L6_2atmpS1739, (moonbit_string_t)moonbit_string_literal_85.data);
  moonbit_decref(_M0L6_2atmpS1739);
  _M0L6_2atmpS1738 = _M0L6_2atmpS3733;
  moonbit_incref(_M0L2sbS199);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1738);
  moonbit_incref(_M0L2sbS199);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, (moonbit_string_t)moonbit_string_literal_86.data);
  _M0L8_2afieldS3732
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$1_1, _M0L4selfS200->$1_2, _M0L4selfS200->$1_0
  };
  _M0L8filenameS1742 = _M0L8_2afieldS3732;
  moonbit_incref(_M0L8filenameS1742.$0);
  moonbit_incref(_M0L2sbS199);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS199, _M0L8filenameS1742);
  _M0L8_2afieldS3731
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$2_1, _M0L4selfS200->$2_2, _M0L4selfS200->$2_0
  };
  _M0L11start__lineS1745 = _M0L8_2afieldS3731;
  moonbit_incref(_M0L11start__lineS1745.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1744
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1745);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3730
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_87.data, _M0L6_2atmpS1744);
  moonbit_decref(_M0L6_2atmpS1744);
  _M0L6_2atmpS1743 = _M0L6_2atmpS3730;
  moonbit_incref(_M0L2sbS199);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1743);
  _M0L8_2afieldS3729
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$3_1, _M0L4selfS200->$3_2, _M0L4selfS200->$3_0
  };
  _M0L13start__columnS1748 = _M0L8_2afieldS3729;
  moonbit_incref(_M0L13start__columnS1748.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1747
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1748);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3728
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_88.data, _M0L6_2atmpS1747);
  moonbit_decref(_M0L6_2atmpS1747);
  _M0L6_2atmpS1746 = _M0L6_2atmpS3728;
  moonbit_incref(_M0L2sbS199);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1746);
  _M0L8_2afieldS3727
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$4_1, _M0L4selfS200->$4_2, _M0L4selfS200->$4_0
  };
  _M0L9end__lineS1751 = _M0L8_2afieldS3727;
  moonbit_incref(_M0L9end__lineS1751.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1750
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1751);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3726
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_89.data, _M0L6_2atmpS1750);
  moonbit_decref(_M0L6_2atmpS1750);
  _M0L6_2atmpS1749 = _M0L6_2atmpS3726;
  moonbit_incref(_M0L2sbS199);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1749);
  _M0L8_2afieldS3725
  = (struct _M0TPC16string10StringView){
    _M0L4selfS200->$5_1, _M0L4selfS200->$5_2, _M0L4selfS200->$5_0
  };
  _M0L6_2acntS3866 = Moonbit_object_header(_M0L4selfS200)->rc;
  if (_M0L6_2acntS3866 > 1) {
    int32_t _M0L11_2anew__cntS3872 = _M0L6_2acntS3866 - 1;
    Moonbit_object_header(_M0L4selfS200)->rc = _M0L11_2anew__cntS3872;
    moonbit_incref(_M0L8_2afieldS3725.$0);
  } else if (_M0L6_2acntS3866 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3871 =
      (struct _M0TPC16string10StringView){_M0L4selfS200->$4_1,
                                            _M0L4selfS200->$4_2,
                                            _M0L4selfS200->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3870;
    struct _M0TPC16string10StringView _M0L8_2afieldS3869;
    struct _M0TPC16string10StringView _M0L8_2afieldS3868;
    struct _M0TPC16string10StringView _M0L8_2afieldS3867;
    moonbit_decref(_M0L8_2afieldS3871.$0);
    _M0L8_2afieldS3870
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$3_1, _M0L4selfS200->$3_2, _M0L4selfS200->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3870.$0);
    _M0L8_2afieldS3869
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$2_1, _M0L4selfS200->$2_2, _M0L4selfS200->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3869.$0);
    _M0L8_2afieldS3868
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$1_1, _M0L4selfS200->$1_2, _M0L4selfS200->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3868.$0);
    _M0L8_2afieldS3867
    = (struct _M0TPC16string10StringView){
      _M0L4selfS200->$0_1, _M0L4selfS200->$0_2, _M0L4selfS200->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3867.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS200);
  }
  _M0L11end__columnS1755 = _M0L8_2afieldS3725;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1754
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1755);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3724
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_90.data, _M0L6_2atmpS1754);
  moonbit_decref(_M0L6_2atmpS1754);
  _M0L6_2atmpS1753 = _M0L6_2atmpS3724;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3723
  = moonbit_add_string(_M0L6_2atmpS1753, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1753);
  _M0L6_2atmpS1752 = _M0L6_2atmpS3723;
  moonbit_incref(_M0L2sbS199);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS199, _M0L6_2atmpS1752);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS199);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS197,
  moonbit_string_t _M0L3strS198
) {
  int32_t _M0L3lenS1728;
  int32_t _M0L6_2atmpS1730;
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L6_2atmpS1727;
  moonbit_bytes_t _M0L8_2afieldS3737;
  moonbit_bytes_t _M0L4dataS1731;
  int32_t _M0L3lenS1732;
  int32_t _M0L6_2atmpS1733;
  int32_t _M0L3lenS1735;
  int32_t _M0L6_2atmpS3736;
  int32_t _M0L6_2atmpS1737;
  int32_t _M0L6_2atmpS1736;
  int32_t _M0L6_2atmpS1734;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1728 = _M0L4selfS197->$1;
  _M0L6_2atmpS1730 = Moonbit_array_length(_M0L3strS198);
  _M0L6_2atmpS1729 = _M0L6_2atmpS1730 * 2;
  _M0L6_2atmpS1727 = _M0L3lenS1728 + _M0L6_2atmpS1729;
  moonbit_incref(_M0L4selfS197);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS197, _M0L6_2atmpS1727);
  _M0L8_2afieldS3737 = _M0L4selfS197->$0;
  _M0L4dataS1731 = _M0L8_2afieldS3737;
  _M0L3lenS1732 = _M0L4selfS197->$1;
  _M0L6_2atmpS1733 = Moonbit_array_length(_M0L3strS198);
  moonbit_incref(_M0L4dataS1731);
  moonbit_incref(_M0L3strS198);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1731, _M0L3lenS1732, _M0L3strS198, 0, _M0L6_2atmpS1733);
  _M0L3lenS1735 = _M0L4selfS197->$1;
  _M0L6_2atmpS3736 = Moonbit_array_length(_M0L3strS198);
  moonbit_decref(_M0L3strS198);
  _M0L6_2atmpS1737 = _M0L6_2atmpS3736;
  _M0L6_2atmpS1736 = _M0L6_2atmpS1737 * 2;
  _M0L6_2atmpS1734 = _M0L3lenS1735 + _M0L6_2atmpS1736;
  _M0L4selfS197->$1 = _M0L6_2atmpS1734;
  moonbit_decref(_M0L4selfS197);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS189,
  int32_t _M0L13bytes__offsetS184,
  moonbit_string_t _M0L3strS191,
  int32_t _M0L11str__offsetS187,
  int32_t _M0L6lengthS185
) {
  int32_t _M0L6_2atmpS1726;
  int32_t _M0L6_2atmpS1725;
  int32_t _M0L2e1S183;
  int32_t _M0L6_2atmpS1724;
  int32_t _M0L2e2S186;
  int32_t _M0L4len1S188;
  int32_t _M0L4len2S190;
  int32_t _if__result_4045;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1726 = _M0L6lengthS185 * 2;
  _M0L6_2atmpS1725 = _M0L13bytes__offsetS184 + _M0L6_2atmpS1726;
  _M0L2e1S183 = _M0L6_2atmpS1725 - 1;
  _M0L6_2atmpS1724 = _M0L11str__offsetS187 + _M0L6lengthS185;
  _M0L2e2S186 = _M0L6_2atmpS1724 - 1;
  _M0L4len1S188 = Moonbit_array_length(_M0L4selfS189);
  _M0L4len2S190 = Moonbit_array_length(_M0L3strS191);
  if (_M0L6lengthS185 >= 0) {
    if (_M0L13bytes__offsetS184 >= 0) {
      if (_M0L2e1S183 < _M0L4len1S188) {
        if (_M0L11str__offsetS187 >= 0) {
          _if__result_4045 = _M0L2e2S186 < _M0L4len2S190;
        } else {
          _if__result_4045 = 0;
        }
      } else {
        _if__result_4045 = 0;
      }
    } else {
      _if__result_4045 = 0;
    }
  } else {
    _if__result_4045 = 0;
  }
  if (_if__result_4045) {
    int32_t _M0L16end__str__offsetS192 =
      _M0L11str__offsetS187 + _M0L6lengthS185;
    int32_t _M0L1iS193 = _M0L11str__offsetS187;
    int32_t _M0L1jS194 = _M0L13bytes__offsetS184;
    while (1) {
      if (_M0L1iS193 < _M0L16end__str__offsetS192) {
        int32_t _M0L6_2atmpS1721 = _M0L3strS191[_M0L1iS193];
        int32_t _M0L6_2atmpS1720 = (int32_t)_M0L6_2atmpS1721;
        uint32_t _M0L1cS195 = *(uint32_t*)&_M0L6_2atmpS1720;
        uint32_t _M0L6_2atmpS1716 = _M0L1cS195 & 255u;
        int32_t _M0L6_2atmpS1715;
        int32_t _M0L6_2atmpS1717;
        uint32_t _M0L6_2atmpS1719;
        int32_t _M0L6_2atmpS1718;
        int32_t _M0L6_2atmpS1722;
        int32_t _M0L6_2atmpS1723;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1715 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1716);
        if (
          _M0L1jS194 < 0 || _M0L1jS194 >= Moonbit_array_length(_M0L4selfS189)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS189[_M0L1jS194] = _M0L6_2atmpS1715;
        _M0L6_2atmpS1717 = _M0L1jS194 + 1;
        _M0L6_2atmpS1719 = _M0L1cS195 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1718 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1719);
        if (
          _M0L6_2atmpS1717 < 0
          || _M0L6_2atmpS1717 >= Moonbit_array_length(_M0L4selfS189)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS189[_M0L6_2atmpS1717] = _M0L6_2atmpS1718;
        _M0L6_2atmpS1722 = _M0L1iS193 + 1;
        _M0L6_2atmpS1723 = _M0L1jS194 + 2;
        _M0L1iS193 = _M0L6_2atmpS1722;
        _M0L1jS194 = _M0L6_2atmpS1723;
        continue;
      } else {
        moonbit_decref(_M0L3strS191);
        moonbit_decref(_M0L4selfS189);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS191);
    moonbit_decref(_M0L4selfS189);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS180,
  double _M0L3objS179
) {
  struct _M0TPB6Logger _M0L6_2atmpS1713;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1713
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS180
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS179, _M0L6_2atmpS1713);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS182,
  struct _M0TPC16string10StringView _M0L3objS181
) {
  struct _M0TPB6Logger _M0L6_2atmpS1714;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1714
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS182
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS181, _M0L6_2atmpS1714);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS125
) {
  int32_t _M0L6_2atmpS1712;
  struct _M0TPC16string10StringView _M0L7_2abindS124;
  moonbit_string_t _M0L7_2adataS126;
  int32_t _M0L8_2astartS127;
  int32_t _M0L6_2atmpS1711;
  int32_t _M0L6_2aendS128;
  int32_t _M0Lm9_2acursorS129;
  int32_t _M0Lm13accept__stateS130;
  int32_t _M0Lm10match__endS131;
  int32_t _M0Lm20match__tag__saver__0S132;
  int32_t _M0Lm20match__tag__saver__1S133;
  int32_t _M0Lm20match__tag__saver__2S134;
  int32_t _M0Lm20match__tag__saver__3S135;
  int32_t _M0Lm20match__tag__saver__4S136;
  int32_t _M0Lm6tag__0S137;
  int32_t _M0Lm6tag__1S138;
  int32_t _M0Lm9tag__1__1S139;
  int32_t _M0Lm9tag__1__2S140;
  int32_t _M0Lm6tag__3S141;
  int32_t _M0Lm6tag__2S142;
  int32_t _M0Lm9tag__2__1S143;
  int32_t _M0Lm6tag__4S144;
  int32_t _M0L6_2atmpS1669;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1712 = Moonbit_array_length(_M0L4reprS125);
  _M0L7_2abindS124
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1712, _M0L4reprS125
  };
  moonbit_incref(_M0L7_2abindS124.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS126 = _M0MPC16string10StringView4data(_M0L7_2abindS124);
  moonbit_incref(_M0L7_2abindS124.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS127
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS124);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1711 = _M0MPC16string10StringView6length(_M0L7_2abindS124);
  _M0L6_2aendS128 = _M0L8_2astartS127 + _M0L6_2atmpS1711;
  _M0Lm9_2acursorS129 = _M0L8_2astartS127;
  _M0Lm13accept__stateS130 = -1;
  _M0Lm10match__endS131 = -1;
  _M0Lm20match__tag__saver__0S132 = -1;
  _M0Lm20match__tag__saver__1S133 = -1;
  _M0Lm20match__tag__saver__2S134 = -1;
  _M0Lm20match__tag__saver__3S135 = -1;
  _M0Lm20match__tag__saver__4S136 = -1;
  _M0Lm6tag__0S137 = -1;
  _M0Lm6tag__1S138 = -1;
  _M0Lm9tag__1__1S139 = -1;
  _M0Lm9tag__1__2S140 = -1;
  _M0Lm6tag__3S141 = -1;
  _M0Lm6tag__2S142 = -1;
  _M0Lm9tag__2__1S143 = -1;
  _M0Lm6tag__4S144 = -1;
  _M0L6_2atmpS1669 = _M0Lm9_2acursorS129;
  if (_M0L6_2atmpS1669 < _M0L6_2aendS128) {
    int32_t _M0L6_2atmpS1671 = _M0Lm9_2acursorS129;
    int32_t _M0L6_2atmpS1670;
    moonbit_incref(_M0L7_2adataS126);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1670
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1671);
    if (_M0L6_2atmpS1670 == 64) {
      int32_t _M0L6_2atmpS1672 = _M0Lm9_2acursorS129;
      _M0Lm9_2acursorS129 = _M0L6_2atmpS1672 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1673;
        _M0Lm6tag__0S137 = _M0Lm9_2acursorS129;
        _M0L6_2atmpS1673 = _M0Lm9_2acursorS129;
        if (_M0L6_2atmpS1673 < _M0L6_2aendS128) {
          int32_t _M0L6_2atmpS1710 = _M0Lm9_2acursorS129;
          int32_t _M0L10next__charS152;
          int32_t _M0L6_2atmpS1674;
          moonbit_incref(_M0L7_2adataS126);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS152
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1710);
          _M0L6_2atmpS1674 = _M0Lm9_2acursorS129;
          _M0Lm9_2acursorS129 = _M0L6_2atmpS1674 + 1;
          if (_M0L10next__charS152 == 58) {
            int32_t _M0L6_2atmpS1675 = _M0Lm9_2acursorS129;
            if (_M0L6_2atmpS1675 < _M0L6_2aendS128) {
              int32_t _M0L6_2atmpS1676 = _M0Lm9_2acursorS129;
              int32_t _M0L12dispatch__15S153;
              _M0Lm9_2acursorS129 = _M0L6_2atmpS1676 + 1;
              _M0L12dispatch__15S153 = 0;
              loop__label__15_156:;
              while (1) {
                int32_t _M0L6_2atmpS1677;
                switch (_M0L12dispatch__15S153) {
                  case 3: {
                    int32_t _M0L6_2atmpS1680;
                    _M0Lm9tag__1__2S140 = _M0Lm9tag__1__1S139;
                    _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1680 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1680 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1685 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1681;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1685);
                      _M0L6_2atmpS1681 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1681 + 1;
                      if (_M0L10next__charS160 < 58) {
                        if (_M0L10next__charS160 < 48) {
                          goto join_159;
                        } else {
                          int32_t _M0L6_2atmpS1682;
                          _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                          _M0Lm9tag__2__1S143 = _M0Lm6tag__2S142;
                          _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                          _M0Lm6tag__3S141 = _M0Lm9_2acursorS129;
                          _M0L6_2atmpS1682 = _M0Lm9_2acursorS129;
                          if (_M0L6_2atmpS1682 < _M0L6_2aendS128) {
                            int32_t _M0L6_2atmpS1684 = _M0Lm9_2acursorS129;
                            int32_t _M0L10next__charS162;
                            int32_t _M0L6_2atmpS1683;
                            moonbit_incref(_M0L7_2adataS126);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS162
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1684);
                            _M0L6_2atmpS1683 = _M0Lm9_2acursorS129;
                            _M0Lm9_2acursorS129 = _M0L6_2atmpS1683 + 1;
                            if (_M0L10next__charS162 < 48) {
                              if (_M0L10next__charS162 == 45) {
                                goto join_154;
                              } else {
                                goto join_161;
                              }
                            } else if (_M0L10next__charS162 > 57) {
                              if (_M0L10next__charS162 < 59) {
                                _M0L12dispatch__15S153 = 3;
                                goto loop__label__15_156;
                              } else {
                                goto join_161;
                              }
                            } else {
                              _M0L12dispatch__15S153 = 6;
                              goto loop__label__15_156;
                            }
                            join_161:;
                            _M0L12dispatch__15S153 = 0;
                            goto loop__label__15_156;
                          } else {
                            goto join_145;
                          }
                        }
                      } else if (_M0L10next__charS160 > 58) {
                        goto join_159;
                      } else {
                        _M0L12dispatch__15S153 = 1;
                        goto loop__label__15_156;
                      }
                      join_159:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1686;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1686 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1686 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1688 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS164;
                      int32_t _M0L6_2atmpS1687;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS164
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1688);
                      _M0L6_2atmpS1687 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1687 + 1;
                      if (_M0L10next__charS164 < 58) {
                        if (_M0L10next__charS164 < 48) {
                          goto join_163;
                        } else {
                          _M0L12dispatch__15S153 = 2;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS164 > 58) {
                        goto join_163;
                      } else {
                        _M0L12dispatch__15S153 = 3;
                        goto loop__label__15_156;
                      }
                      join_163:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1689;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1689 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1689 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1691 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS165;
                      int32_t _M0L6_2atmpS1690;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS165
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1691);
                      _M0L6_2atmpS1690 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1690 + 1;
                      if (_M0L10next__charS165 == 58) {
                        _M0L12dispatch__15S153 = 1;
                        goto loop__label__15_156;
                      } else {
                        _M0L12dispatch__15S153 = 0;
                        goto loop__label__15_156;
                      }
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1692;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__4S144 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1692 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1692 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1700 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS167;
                      int32_t _M0L6_2atmpS1693;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS167
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1700);
                      _M0L6_2atmpS1693 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1693 + 1;
                      if (_M0L10next__charS167 < 58) {
                        if (_M0L10next__charS167 < 48) {
                          goto join_166;
                        } else {
                          _M0L12dispatch__15S153 = 4;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS167 > 58) {
                        goto join_166;
                      } else {
                        int32_t _M0L6_2atmpS1694;
                        _M0Lm9tag__1__2S140 = _M0Lm9tag__1__1S139;
                        _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                        _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                        _M0L6_2atmpS1694 = _M0Lm9_2acursorS129;
                        if (_M0L6_2atmpS1694 < _M0L6_2aendS128) {
                          int32_t _M0L6_2atmpS1699 = _M0Lm9_2acursorS129;
                          int32_t _M0L10next__charS169;
                          int32_t _M0L6_2atmpS1695;
                          moonbit_incref(_M0L7_2adataS126);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS169
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1699);
                          _M0L6_2atmpS1695 = _M0Lm9_2acursorS129;
                          _M0Lm9_2acursorS129 = _M0L6_2atmpS1695 + 1;
                          if (_M0L10next__charS169 < 58) {
                            if (_M0L10next__charS169 < 48) {
                              goto join_168;
                            } else {
                              int32_t _M0L6_2atmpS1696;
                              _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                              _M0Lm9tag__2__1S143 = _M0Lm6tag__2S142;
                              _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                              _M0L6_2atmpS1696 = _M0Lm9_2acursorS129;
                              if (_M0L6_2atmpS1696 < _M0L6_2aendS128) {
                                int32_t _M0L6_2atmpS1698 =
                                  _M0Lm9_2acursorS129;
                                int32_t _M0L10next__charS171;
                                int32_t _M0L6_2atmpS1697;
                                moonbit_incref(_M0L7_2adataS126);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS171
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1698);
                                _M0L6_2atmpS1697 = _M0Lm9_2acursorS129;
                                _M0Lm9_2acursorS129 = _M0L6_2atmpS1697 + 1;
                                if (_M0L10next__charS171 < 58) {
                                  if (_M0L10next__charS171 < 48) {
                                    goto join_170;
                                  } else {
                                    _M0L12dispatch__15S153 = 5;
                                    goto loop__label__15_156;
                                  }
                                } else if (_M0L10next__charS171 > 58) {
                                  goto join_170;
                                } else {
                                  _M0L12dispatch__15S153 = 3;
                                  goto loop__label__15_156;
                                }
                                join_170:;
                                _M0L12dispatch__15S153 = 0;
                                goto loop__label__15_156;
                              } else {
                                goto join_158;
                              }
                            }
                          } else if (_M0L10next__charS169 > 58) {
                            goto join_168;
                          } else {
                            _M0L12dispatch__15S153 = 1;
                            goto loop__label__15_156;
                          }
                          join_168:;
                          _M0L12dispatch__15S153 = 0;
                          goto loop__label__15_156;
                        } else {
                          goto join_145;
                        }
                      }
                      join_166:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1701;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1701 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1701 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1703 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS173;
                      int32_t _M0L6_2atmpS1702;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS173
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1703);
                      _M0L6_2atmpS1702 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1702 + 1;
                      if (_M0L10next__charS173 < 58) {
                        if (_M0L10next__charS173 < 48) {
                          goto join_172;
                        } else {
                          _M0L12dispatch__15S153 = 5;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS173 > 58) {
                        goto join_172;
                      } else {
                        _M0L12dispatch__15S153 = 3;
                        goto loop__label__15_156;
                      }
                      join_172:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_158;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1704;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__3S141 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1704 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1704 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1706 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS175;
                      int32_t _M0L6_2atmpS1705;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS175
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1706);
                      _M0L6_2atmpS1705 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1705 + 1;
                      if (_M0L10next__charS175 < 48) {
                        if (_M0L10next__charS175 == 45) {
                          goto join_154;
                        } else {
                          goto join_174;
                        }
                      } else if (_M0L10next__charS175 > 57) {
                        if (_M0L10next__charS175 < 59) {
                          _M0L12dispatch__15S153 = 3;
                          goto loop__label__15_156;
                        } else {
                          goto join_174;
                        }
                      } else {
                        _M0L12dispatch__15S153 = 6;
                        goto loop__label__15_156;
                      }
                      join_174:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1707;
                    _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1707 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1707 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1709 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS177;
                      int32_t _M0L6_2atmpS1708;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS177
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1709);
                      _M0L6_2atmpS1708 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1708 + 1;
                      if (_M0L10next__charS177 < 58) {
                        if (_M0L10next__charS177 < 48) {
                          goto join_176;
                        } else {
                          _M0L12dispatch__15S153 = 2;
                          goto loop__label__15_156;
                        }
                      } else if (_M0L10next__charS177 > 58) {
                        goto join_176;
                      } else {
                        _M0L12dispatch__15S153 = 1;
                        goto loop__label__15_156;
                      }
                      join_176:;
                      _M0L12dispatch__15S153 = 0;
                      goto loop__label__15_156;
                    } else {
                      goto join_145;
                    }
                    break;
                  }
                  default: {
                    goto join_145;
                    break;
                  }
                }
                join_158:;
                _M0Lm6tag__1S138 = _M0Lm9tag__1__2S140;
                _M0Lm6tag__2S142 = _M0Lm9tag__2__1S143;
                _M0Lm20match__tag__saver__0S132 = _M0Lm6tag__0S137;
                _M0Lm20match__tag__saver__1S133 = _M0Lm6tag__1S138;
                _M0Lm20match__tag__saver__2S134 = _M0Lm6tag__2S142;
                _M0Lm20match__tag__saver__3S135 = _M0Lm6tag__3S141;
                _M0Lm20match__tag__saver__4S136 = _M0Lm6tag__4S144;
                _M0Lm13accept__stateS130 = 0;
                _M0Lm10match__endS131 = _M0Lm9_2acursorS129;
                goto join_145;
                join_154:;
                _M0Lm9tag__1__1S139 = _M0Lm9tag__1__2S140;
                _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                _M0Lm6tag__2S142 = _M0Lm9tag__2__1S143;
                _M0L6_2atmpS1677 = _M0Lm9_2acursorS129;
                if (_M0L6_2atmpS1677 < _M0L6_2aendS128) {
                  int32_t _M0L6_2atmpS1679 = _M0Lm9_2acursorS129;
                  int32_t _M0L10next__charS157;
                  int32_t _M0L6_2atmpS1678;
                  moonbit_incref(_M0L7_2adataS126);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS157
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1679);
                  _M0L6_2atmpS1678 = _M0Lm9_2acursorS129;
                  _M0Lm9_2acursorS129 = _M0L6_2atmpS1678 + 1;
                  if (_M0L10next__charS157 < 58) {
                    if (_M0L10next__charS157 < 48) {
                      goto join_155;
                    } else {
                      _M0L12dispatch__15S153 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS157 > 58) {
                    goto join_155;
                  } else {
                    _M0L12dispatch__15S153 = 1;
                    continue;
                  }
                  join_155:;
                  _M0L12dispatch__15S153 = 0;
                  continue;
                } else {
                  goto join_145;
                }
                break;
              }
            } else {
              goto join_145;
            }
          } else {
            continue;
          }
        } else {
          goto join_145;
        }
        break;
      }
    } else {
      goto join_145;
    }
  } else {
    goto join_145;
  }
  join_145:;
  switch (_M0Lm13accept__stateS130) {
    case 0: {
      int32_t _M0L6_2atmpS1668 = _M0Lm20match__tag__saver__1S133;
      int32_t _M0L6_2atmpS1667 = _M0L6_2atmpS1668 + 1;
      int64_t _M0L6_2atmpS1664 = (int64_t)_M0L6_2atmpS1667;
      int32_t _M0L6_2atmpS1666 = _M0Lm20match__tag__saver__2S134;
      int64_t _M0L6_2atmpS1665 = (int64_t)_M0L6_2atmpS1666;
      struct _M0TPC16string10StringView _M0L11start__lineS146;
      int32_t _M0L6_2atmpS1663;
      int32_t _M0L6_2atmpS1662;
      int64_t _M0L6_2atmpS1659;
      int32_t _M0L6_2atmpS1661;
      int64_t _M0L6_2atmpS1660;
      struct _M0TPC16string10StringView _M0L13start__columnS147;
      int32_t _M0L6_2atmpS1658;
      int64_t _M0L6_2atmpS1655;
      int32_t _M0L6_2atmpS1657;
      int64_t _M0L6_2atmpS1656;
      struct _M0TPC16string10StringView _M0L3pkgS148;
      int32_t _M0L6_2atmpS1654;
      int32_t _M0L6_2atmpS1653;
      int64_t _M0L6_2atmpS1650;
      int32_t _M0L6_2atmpS1652;
      int64_t _M0L6_2atmpS1651;
      struct _M0TPC16string10StringView _M0L8filenameS149;
      int32_t _M0L6_2atmpS1649;
      int32_t _M0L6_2atmpS1648;
      int64_t _M0L6_2atmpS1645;
      int32_t _M0L6_2atmpS1647;
      int64_t _M0L6_2atmpS1646;
      struct _M0TPC16string10StringView _M0L9end__lineS150;
      int32_t _M0L6_2atmpS1644;
      int32_t _M0L6_2atmpS1643;
      int64_t _M0L6_2atmpS1640;
      int32_t _M0L6_2atmpS1642;
      int64_t _M0L6_2atmpS1641;
      struct _M0TPC16string10StringView _M0L11end__columnS151;
      struct _M0TPB13SourceLocRepr* _block_4062;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS146
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1664, _M0L6_2atmpS1665);
      _M0L6_2atmpS1663 = _M0Lm20match__tag__saver__2S134;
      _M0L6_2atmpS1662 = _M0L6_2atmpS1663 + 1;
      _M0L6_2atmpS1659 = (int64_t)_M0L6_2atmpS1662;
      _M0L6_2atmpS1661 = _M0Lm20match__tag__saver__3S135;
      _M0L6_2atmpS1660 = (int64_t)_M0L6_2atmpS1661;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS147
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1659, _M0L6_2atmpS1660);
      _M0L6_2atmpS1658 = _M0L8_2astartS127 + 1;
      _M0L6_2atmpS1655 = (int64_t)_M0L6_2atmpS1658;
      _M0L6_2atmpS1657 = _M0Lm20match__tag__saver__0S132;
      _M0L6_2atmpS1656 = (int64_t)_M0L6_2atmpS1657;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS148
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1655, _M0L6_2atmpS1656);
      _M0L6_2atmpS1654 = _M0Lm20match__tag__saver__0S132;
      _M0L6_2atmpS1653 = _M0L6_2atmpS1654 + 1;
      _M0L6_2atmpS1650 = (int64_t)_M0L6_2atmpS1653;
      _M0L6_2atmpS1652 = _M0Lm20match__tag__saver__1S133;
      _M0L6_2atmpS1651 = (int64_t)_M0L6_2atmpS1652;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS149
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1650, _M0L6_2atmpS1651);
      _M0L6_2atmpS1649 = _M0Lm20match__tag__saver__3S135;
      _M0L6_2atmpS1648 = _M0L6_2atmpS1649 + 1;
      _M0L6_2atmpS1645 = (int64_t)_M0L6_2atmpS1648;
      _M0L6_2atmpS1647 = _M0Lm20match__tag__saver__4S136;
      _M0L6_2atmpS1646 = (int64_t)_M0L6_2atmpS1647;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS150
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1645, _M0L6_2atmpS1646);
      _M0L6_2atmpS1644 = _M0Lm20match__tag__saver__4S136;
      _M0L6_2atmpS1643 = _M0L6_2atmpS1644 + 1;
      _M0L6_2atmpS1640 = (int64_t)_M0L6_2atmpS1643;
      _M0L6_2atmpS1642 = _M0Lm10match__endS131;
      _M0L6_2atmpS1641 = (int64_t)_M0L6_2atmpS1642;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS151
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1640, _M0L6_2atmpS1641);
      _block_4062
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4062)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4062->$0_0 = _M0L3pkgS148.$0;
      _block_4062->$0_1 = _M0L3pkgS148.$1;
      _block_4062->$0_2 = _M0L3pkgS148.$2;
      _block_4062->$1_0 = _M0L8filenameS149.$0;
      _block_4062->$1_1 = _M0L8filenameS149.$1;
      _block_4062->$1_2 = _M0L8filenameS149.$2;
      _block_4062->$2_0 = _M0L11start__lineS146.$0;
      _block_4062->$2_1 = _M0L11start__lineS146.$1;
      _block_4062->$2_2 = _M0L11start__lineS146.$2;
      _block_4062->$3_0 = _M0L13start__columnS147.$0;
      _block_4062->$3_1 = _M0L13start__columnS147.$1;
      _block_4062->$3_2 = _M0L13start__columnS147.$2;
      _block_4062->$4_0 = _M0L9end__lineS150.$0;
      _block_4062->$4_1 = _M0L9end__lineS150.$1;
      _block_4062->$4_2 = _M0L9end__lineS150.$2;
      _block_4062->$5_0 = _M0L11end__columnS151.$0;
      _block_4062->$5_1 = _M0L11end__columnS151.$1;
      _block_4062->$5_2 = _M0L11end__columnS151.$2;
      return _block_4062;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS126);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS122,
  int32_t _M0L5indexS123
) {
  int32_t _M0L3lenS121;
  int32_t _if__result_4063;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS121 = _M0L4selfS122->$1;
  if (_M0L5indexS123 >= 0) {
    _if__result_4063 = _M0L5indexS123 < _M0L3lenS121;
  } else {
    _if__result_4063 = 0;
  }
  if (_if__result_4063) {
    moonbit_string_t* _M0L6_2atmpS1639;
    moonbit_string_t _M0L6_2atmpS3738;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1639 = _M0MPC15array5Array6bufferGsE(_M0L4selfS122);
    if (
      _M0L5indexS123 < 0
      || _M0L5indexS123 >= Moonbit_array_length(_M0L6_2atmpS1639)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3738 = (moonbit_string_t)_M0L6_2atmpS1639[_M0L5indexS123];
    moonbit_incref(_M0L6_2atmpS3738);
    moonbit_decref(_M0L6_2atmpS1639);
    return _M0L6_2atmpS3738;
  } else {
    moonbit_decref(_M0L4selfS122);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS118
) {
  moonbit_string_t* _M0L8_2afieldS3739;
  int32_t _M0L6_2acntS3873;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3739 = _M0L4selfS118->$0;
  _M0L6_2acntS3873 = Moonbit_object_header(_M0L4selfS118)->rc;
  if (_M0L6_2acntS3873 > 1) {
    int32_t _M0L11_2anew__cntS3874 = _M0L6_2acntS3873 - 1;
    Moonbit_object_header(_M0L4selfS118)->rc = _M0L11_2anew__cntS3874;
    moonbit_incref(_M0L8_2afieldS3739);
  } else if (_M0L6_2acntS3873 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS118);
  }
  return _M0L8_2afieldS3739;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS119
) {
  struct _M0TUsiE** _M0L8_2afieldS3740;
  int32_t _M0L6_2acntS3875;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3740 = _M0L4selfS119->$0;
  _M0L6_2acntS3875 = Moonbit_object_header(_M0L4selfS119)->rc;
  if (_M0L6_2acntS3875 > 1) {
    int32_t _M0L11_2anew__cntS3876 = _M0L6_2acntS3875 - 1;
    Moonbit_object_header(_M0L4selfS119)->rc = _M0L11_2anew__cntS3876;
    moonbit_incref(_M0L8_2afieldS3740);
  } else if (_M0L6_2acntS3875 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS119);
  }
  return _M0L8_2afieldS3740;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS120
) {
  void** _M0L8_2afieldS3741;
  int32_t _M0L6_2acntS3877;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3741 = _M0L4selfS120->$0;
  _M0L6_2acntS3877 = Moonbit_object_header(_M0L4selfS120)->rc;
  if (_M0L6_2acntS3877 > 1) {
    int32_t _M0L11_2anew__cntS3878 = _M0L6_2acntS3877 - 1;
    Moonbit_object_header(_M0L4selfS120)->rc = _M0L11_2anew__cntS3878;
    moonbit_incref(_M0L8_2afieldS3741);
  } else if (_M0L6_2acntS3877 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS120);
  }
  return _M0L8_2afieldS3741;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS117) {
  struct _M0TPB13StringBuilder* _M0L3bufS116;
  struct _M0TPB6Logger _M0L6_2atmpS1638;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS116 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS116);
  _M0L6_2atmpS1638
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS116
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS117, _M0L6_2atmpS1638);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS116);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS113,
  int32_t _M0L5indexS114
) {
  int32_t _M0L2c1S112;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S112 = _M0L4selfS113[_M0L5indexS114];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S112)) {
    int32_t _M0L6_2atmpS1637 = _M0L5indexS114 + 1;
    int32_t _M0L6_2atmpS3742 = _M0L4selfS113[_M0L6_2atmpS1637];
    int32_t _M0L2c2S115;
    int32_t _M0L6_2atmpS1635;
    int32_t _M0L6_2atmpS1636;
    moonbit_decref(_M0L4selfS113);
    _M0L2c2S115 = _M0L6_2atmpS3742;
    _M0L6_2atmpS1635 = (int32_t)_M0L2c1S112;
    _M0L6_2atmpS1636 = (int32_t)_M0L2c2S115;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1635, _M0L6_2atmpS1636);
  } else {
    moonbit_decref(_M0L4selfS113);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S112);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS111) {
  int32_t _M0L6_2atmpS1634;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1634 = (int32_t)_M0L4selfS111;
  return _M0L6_2atmpS1634;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS109,
  int32_t _M0L8trailingS110
) {
  int32_t _M0L6_2atmpS1633;
  int32_t _M0L6_2atmpS1632;
  int32_t _M0L6_2atmpS1631;
  int32_t _M0L6_2atmpS1630;
  int32_t _M0L6_2atmpS1629;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1633 = _M0L7leadingS109 - 55296;
  _M0L6_2atmpS1632 = _M0L6_2atmpS1633 * 1024;
  _M0L6_2atmpS1631 = _M0L6_2atmpS1632 + _M0L8trailingS110;
  _M0L6_2atmpS1630 = _M0L6_2atmpS1631 - 56320;
  _M0L6_2atmpS1629 = _M0L6_2atmpS1630 + 65536;
  return _M0L6_2atmpS1629;
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
  int32_t _M0L3lenS1624;
  int32_t _M0L6_2atmpS1623;
  moonbit_bytes_t _M0L8_2afieldS3743;
  moonbit_bytes_t _M0L4dataS1627;
  int32_t _M0L3lenS1628;
  int32_t _M0L3incS105;
  int32_t _M0L3lenS1626;
  int32_t _M0L6_2atmpS1625;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1624 = _M0L4selfS104->$1;
  _M0L6_2atmpS1623 = _M0L3lenS1624 + 4;
  moonbit_incref(_M0L4selfS104);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS104, _M0L6_2atmpS1623);
  _M0L8_2afieldS3743 = _M0L4selfS104->$0;
  _M0L4dataS1627 = _M0L8_2afieldS3743;
  _M0L3lenS1628 = _M0L4selfS104->$1;
  moonbit_incref(_M0L4dataS1627);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS105
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1627, _M0L3lenS1628, _M0L2chS106);
  _M0L3lenS1626 = _M0L4selfS104->$1;
  _M0L6_2atmpS1625 = _M0L3lenS1626 + _M0L3incS105;
  _M0L4selfS104->$1 = _M0L6_2atmpS1625;
  moonbit_decref(_M0L4selfS104);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS99,
  int32_t _M0L8requiredS100
) {
  moonbit_bytes_t _M0L8_2afieldS3747;
  moonbit_bytes_t _M0L4dataS1622;
  int32_t _M0L6_2atmpS3746;
  int32_t _M0L12current__lenS98;
  int32_t _M0Lm13enough__spaceS101;
  int32_t _M0L6_2atmpS1620;
  int32_t _M0L6_2atmpS1621;
  moonbit_bytes_t _M0L9new__dataS103;
  moonbit_bytes_t _M0L8_2afieldS3745;
  moonbit_bytes_t _M0L4dataS1618;
  int32_t _M0L3lenS1619;
  moonbit_bytes_t _M0L6_2aoldS3744;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3747 = _M0L4selfS99->$0;
  _M0L4dataS1622 = _M0L8_2afieldS3747;
  _M0L6_2atmpS3746 = Moonbit_array_length(_M0L4dataS1622);
  _M0L12current__lenS98 = _M0L6_2atmpS3746;
  if (_M0L8requiredS100 <= _M0L12current__lenS98) {
    moonbit_decref(_M0L4selfS99);
    return 0;
  }
  _M0Lm13enough__spaceS101 = _M0L12current__lenS98;
  while (1) {
    int32_t _M0L6_2atmpS1616 = _M0Lm13enough__spaceS101;
    if (_M0L6_2atmpS1616 < _M0L8requiredS100) {
      int32_t _M0L6_2atmpS1617 = _M0Lm13enough__spaceS101;
      _M0Lm13enough__spaceS101 = _M0L6_2atmpS1617 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1620 = _M0Lm13enough__spaceS101;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1621 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS103
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1620, _M0L6_2atmpS1621);
  _M0L8_2afieldS3745 = _M0L4selfS99->$0;
  _M0L4dataS1618 = _M0L8_2afieldS3745;
  _M0L3lenS1619 = _M0L4selfS99->$1;
  moonbit_incref(_M0L4dataS1618);
  moonbit_incref(_M0L9new__dataS103);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS103, 0, _M0L4dataS1618, 0, _M0L3lenS1619);
  _M0L6_2aoldS3744 = _M0L4selfS99->$0;
  moonbit_decref(_M0L6_2aoldS3744);
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
    uint32_t _M0L6_2atmpS1599 = _M0L4codeS91 & 255u;
    int32_t _M0L6_2atmpS1598;
    int32_t _M0L6_2atmpS1600;
    uint32_t _M0L6_2atmpS1602;
    int32_t _M0L6_2atmpS1601;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1598 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1599);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1598;
    _M0L6_2atmpS1600 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1602 = _M0L4codeS91 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1601 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1602);
    if (
      _M0L6_2atmpS1600 < 0
      || _M0L6_2atmpS1600 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1600] = _M0L6_2atmpS1601;
    moonbit_decref(_M0L4selfS93);
    return 2;
  } else if (_M0L4codeS91 < 1114112u) {
    uint32_t _M0L2hiS95 = _M0L4codeS91 - 65536u;
    uint32_t _M0L6_2atmpS1615 = _M0L2hiS95 >> 10;
    uint32_t _M0L2loS96 = _M0L6_2atmpS1615 | 55296u;
    uint32_t _M0L6_2atmpS1614 = _M0L2hiS95 & 1023u;
    uint32_t _M0L2hiS97 = _M0L6_2atmpS1614 | 56320u;
    uint32_t _M0L6_2atmpS1604 = _M0L2loS96 & 255u;
    int32_t _M0L6_2atmpS1603;
    int32_t _M0L6_2atmpS1605;
    uint32_t _M0L6_2atmpS1607;
    int32_t _M0L6_2atmpS1606;
    int32_t _M0L6_2atmpS1608;
    uint32_t _M0L6_2atmpS1610;
    int32_t _M0L6_2atmpS1609;
    int32_t _M0L6_2atmpS1611;
    uint32_t _M0L6_2atmpS1613;
    int32_t _M0L6_2atmpS1612;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1603 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1604);
    if (
      _M0L6offsetS94 < 0
      || _M0L6offsetS94 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6offsetS94] = _M0L6_2atmpS1603;
    _M0L6_2atmpS1605 = _M0L6offsetS94 + 1;
    _M0L6_2atmpS1607 = _M0L2loS96 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1606 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1607);
    if (
      _M0L6_2atmpS1605 < 0
      || _M0L6_2atmpS1605 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1605] = _M0L6_2atmpS1606;
    _M0L6_2atmpS1608 = _M0L6offsetS94 + 2;
    _M0L6_2atmpS1610 = _M0L2hiS97 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1609 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1610);
    if (
      _M0L6_2atmpS1608 < 0
      || _M0L6_2atmpS1608 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1608] = _M0L6_2atmpS1609;
    _M0L6_2atmpS1611 = _M0L6offsetS94 + 3;
    _M0L6_2atmpS1613 = _M0L2hiS97 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1612 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1613);
    if (
      _M0L6_2atmpS1611 < 0
      || _M0L6_2atmpS1611 >= Moonbit_array_length(_M0L4selfS93)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS93[_M0L6_2atmpS1611] = _M0L6_2atmpS1612;
    moonbit_decref(_M0L4selfS93);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS93);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_91.data, (moonbit_string_t)moonbit_string_literal_92.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS90) {
  int32_t _M0L6_2atmpS1597;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1597 = *(int32_t*)&_M0L4selfS90;
  return _M0L6_2atmpS1597 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1596;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1596 = _M0L4selfS89;
  return *(uint32_t*)&_M0L6_2atmpS1596;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS88
) {
  moonbit_bytes_t _M0L8_2afieldS3749;
  moonbit_bytes_t _M0L4dataS1595;
  moonbit_bytes_t _M0L6_2atmpS1592;
  int32_t _M0L8_2afieldS3748;
  int32_t _M0L3lenS1594;
  int64_t _M0L6_2atmpS1593;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3749 = _M0L4selfS88->$0;
  _M0L4dataS1595 = _M0L8_2afieldS3749;
  moonbit_incref(_M0L4dataS1595);
  _M0L6_2atmpS1592 = _M0L4dataS1595;
  _M0L8_2afieldS3748 = _M0L4selfS88->$1;
  moonbit_decref(_M0L4selfS88);
  _M0L3lenS1594 = _M0L8_2afieldS3748;
  _M0L6_2atmpS1593 = (int64_t)_M0L3lenS1594;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1592, 0, _M0L6_2atmpS1593);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS83,
  int32_t _M0L6offsetS87,
  int64_t _M0L6lengthS85
) {
  int32_t _M0L3lenS82;
  int32_t _M0L6lengthS84;
  int32_t _if__result_4065;
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
      int32_t _M0L6_2atmpS1591 = _M0L6offsetS87 + _M0L6lengthS84;
      _if__result_4065 = _M0L6_2atmpS1591 <= _M0L3lenS82;
    } else {
      _if__result_4065 = 0;
    }
  } else {
    _if__result_4065 = 0;
  }
  if (_if__result_4065) {
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
  struct _M0TPB13StringBuilder* _block_4066;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS80 < 1) {
    _M0L7initialS79 = 1;
  } else {
    _M0L7initialS79 = _M0L10size__hintS80;
  }
  _M0L4dataS81 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS79, 0);
  _block_4066
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4066)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4066->$0 = _M0L4dataS81;
  _block_4066->$1 = 0;
  return _block_4066;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS78) {
  int32_t _M0L6_2atmpS1590;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1590 = (int32_t)_M0L4selfS78;
  return _M0L6_2atmpS1590;
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
  int32_t _if__result_4067;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS27 == _M0L3srcS28) {
    _if__result_4067 = _M0L11dst__offsetS29 < _M0L11src__offsetS30;
  } else {
    _if__result_4067 = 0;
  }
  if (_if__result_4067) {
    int32_t _M0L1iS31 = 0;
    while (1) {
      if (_M0L1iS31 < _M0L3lenS32) {
        int32_t _M0L6_2atmpS1554 = _M0L11dst__offsetS29 + _M0L1iS31;
        int32_t _M0L6_2atmpS1556 = _M0L11src__offsetS30 + _M0L1iS31;
        int32_t _M0L6_2atmpS1555;
        int32_t _M0L6_2atmpS1557;
        if (
          _M0L6_2atmpS1556 < 0
          || _M0L6_2atmpS1556 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1555 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1556];
        if (
          _M0L6_2atmpS1554 < 0
          || _M0L6_2atmpS1554 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1554] = _M0L6_2atmpS1555;
        _M0L6_2atmpS1557 = _M0L1iS31 + 1;
        _M0L1iS31 = _M0L6_2atmpS1557;
        continue;
      } else {
        moonbit_decref(_M0L3srcS28);
        moonbit_decref(_M0L3dstS27);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1562 = _M0L3lenS32 - 1;
    int32_t _M0L1iS34 = _M0L6_2atmpS1562;
    while (1) {
      if (_M0L1iS34 >= 0) {
        int32_t _M0L6_2atmpS1558 = _M0L11dst__offsetS29 + _M0L1iS34;
        int32_t _M0L6_2atmpS1560 = _M0L11src__offsetS30 + _M0L1iS34;
        int32_t _M0L6_2atmpS1559;
        int32_t _M0L6_2atmpS1561;
        if (
          _M0L6_2atmpS1560 < 0
          || _M0L6_2atmpS1560 >= Moonbit_array_length(_M0L3srcS28)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1559 = (int32_t)_M0L3srcS28[_M0L6_2atmpS1560];
        if (
          _M0L6_2atmpS1558 < 0
          || _M0L6_2atmpS1558 >= Moonbit_array_length(_M0L3dstS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS27[_M0L6_2atmpS1558] = _M0L6_2atmpS1559;
        _M0L6_2atmpS1561 = _M0L1iS34 - 1;
        _M0L1iS34 = _M0L6_2atmpS1561;
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
  int32_t _if__result_4070;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS36 == _M0L3srcS37) {
    _if__result_4070 = _M0L11dst__offsetS38 < _M0L11src__offsetS39;
  } else {
    _if__result_4070 = 0;
  }
  if (_if__result_4070) {
    int32_t _M0L1iS40 = 0;
    while (1) {
      if (_M0L1iS40 < _M0L3lenS41) {
        int32_t _M0L6_2atmpS1563 = _M0L11dst__offsetS38 + _M0L1iS40;
        int32_t _M0L6_2atmpS1565 = _M0L11src__offsetS39 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS3751;
        moonbit_string_t _M0L6_2atmpS1564;
        moonbit_string_t _M0L6_2aoldS3750;
        int32_t _M0L6_2atmpS1566;
        if (
          _M0L6_2atmpS1565 < 0
          || _M0L6_2atmpS1565 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3751 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1565];
        _M0L6_2atmpS1564 = _M0L6_2atmpS3751;
        if (
          _M0L6_2atmpS1563 < 0
          || _M0L6_2atmpS1563 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3750 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1563];
        moonbit_incref(_M0L6_2atmpS1564);
        moonbit_decref(_M0L6_2aoldS3750);
        _M0L3dstS36[_M0L6_2atmpS1563] = _M0L6_2atmpS1564;
        _M0L6_2atmpS1566 = _M0L1iS40 + 1;
        _M0L1iS40 = _M0L6_2atmpS1566;
        continue;
      } else {
        moonbit_decref(_M0L3srcS37);
        moonbit_decref(_M0L3dstS36);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1571 = _M0L3lenS41 - 1;
    int32_t _M0L1iS43 = _M0L6_2atmpS1571;
    while (1) {
      if (_M0L1iS43 >= 0) {
        int32_t _M0L6_2atmpS1567 = _M0L11dst__offsetS38 + _M0L1iS43;
        int32_t _M0L6_2atmpS1569 = _M0L11src__offsetS39 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS3753;
        moonbit_string_t _M0L6_2atmpS1568;
        moonbit_string_t _M0L6_2aoldS3752;
        int32_t _M0L6_2atmpS1570;
        if (
          _M0L6_2atmpS1569 < 0
          || _M0L6_2atmpS1569 >= Moonbit_array_length(_M0L3srcS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3753 = (moonbit_string_t)_M0L3srcS37[_M0L6_2atmpS1569];
        _M0L6_2atmpS1568 = _M0L6_2atmpS3753;
        if (
          _M0L6_2atmpS1567 < 0
          || _M0L6_2atmpS1567 >= Moonbit_array_length(_M0L3dstS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3752 = (moonbit_string_t)_M0L3dstS36[_M0L6_2atmpS1567];
        moonbit_incref(_M0L6_2atmpS1568);
        moonbit_decref(_M0L6_2aoldS3752);
        _M0L3dstS36[_M0L6_2atmpS1567] = _M0L6_2atmpS1568;
        _M0L6_2atmpS1570 = _M0L1iS43 - 1;
        _M0L1iS43 = _M0L6_2atmpS1570;
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
  int32_t _if__result_4073;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS45 == _M0L3srcS46) {
    _if__result_4073 = _M0L11dst__offsetS47 < _M0L11src__offsetS48;
  } else {
    _if__result_4073 = 0;
  }
  if (_if__result_4073) {
    int32_t _M0L1iS49 = 0;
    while (1) {
      if (_M0L1iS49 < _M0L3lenS50) {
        int32_t _M0L6_2atmpS1572 = _M0L11dst__offsetS47 + _M0L1iS49;
        int32_t _M0L6_2atmpS1574 = _M0L11src__offsetS48 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS3755;
        struct _M0TUsiE* _M0L6_2atmpS1573;
        struct _M0TUsiE* _M0L6_2aoldS3754;
        int32_t _M0L6_2atmpS1575;
        if (
          _M0L6_2atmpS1574 < 0
          || _M0L6_2atmpS1574 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3755 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1574];
        _M0L6_2atmpS1573 = _M0L6_2atmpS3755;
        if (
          _M0L6_2atmpS1572 < 0
          || _M0L6_2atmpS1572 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3754 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1572];
        if (_M0L6_2atmpS1573) {
          moonbit_incref(_M0L6_2atmpS1573);
        }
        if (_M0L6_2aoldS3754) {
          moonbit_decref(_M0L6_2aoldS3754);
        }
        _M0L3dstS45[_M0L6_2atmpS1572] = _M0L6_2atmpS1573;
        _M0L6_2atmpS1575 = _M0L1iS49 + 1;
        _M0L1iS49 = _M0L6_2atmpS1575;
        continue;
      } else {
        moonbit_decref(_M0L3srcS46);
        moonbit_decref(_M0L3dstS45);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1580 = _M0L3lenS50 - 1;
    int32_t _M0L1iS52 = _M0L6_2atmpS1580;
    while (1) {
      if (_M0L1iS52 >= 0) {
        int32_t _M0L6_2atmpS1576 = _M0L11dst__offsetS47 + _M0L1iS52;
        int32_t _M0L6_2atmpS1578 = _M0L11src__offsetS48 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS3757;
        struct _M0TUsiE* _M0L6_2atmpS1577;
        struct _M0TUsiE* _M0L6_2aoldS3756;
        int32_t _M0L6_2atmpS1579;
        if (
          _M0L6_2atmpS1578 < 0
          || _M0L6_2atmpS1578 >= Moonbit_array_length(_M0L3srcS46)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3757 = (struct _M0TUsiE*)_M0L3srcS46[_M0L6_2atmpS1578];
        _M0L6_2atmpS1577 = _M0L6_2atmpS3757;
        if (
          _M0L6_2atmpS1576 < 0
          || _M0L6_2atmpS1576 >= Moonbit_array_length(_M0L3dstS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3756 = (struct _M0TUsiE*)_M0L3dstS45[_M0L6_2atmpS1576];
        if (_M0L6_2atmpS1577) {
          moonbit_incref(_M0L6_2atmpS1577);
        }
        if (_M0L6_2aoldS3756) {
          moonbit_decref(_M0L6_2aoldS3756);
        }
        _M0L3dstS45[_M0L6_2atmpS1576] = _M0L6_2atmpS1577;
        _M0L6_2atmpS1579 = _M0L1iS52 - 1;
        _M0L1iS52 = _M0L6_2atmpS1579;
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
  int32_t _if__result_4076;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS54 == _M0L3srcS55) {
    _if__result_4076 = _M0L11dst__offsetS56 < _M0L11src__offsetS57;
  } else {
    _if__result_4076 = 0;
  }
  if (_if__result_4076) {
    int32_t _M0L1iS58 = 0;
    while (1) {
      if (_M0L1iS58 < _M0L3lenS59) {
        int32_t _M0L6_2atmpS1581 = _M0L11dst__offsetS56 + _M0L1iS58;
        int32_t _M0L6_2atmpS1583 = _M0L11src__offsetS57 + _M0L1iS58;
        void* _M0L6_2atmpS3759;
        void* _M0L6_2atmpS1582;
        void* _M0L6_2aoldS3758;
        int32_t _M0L6_2atmpS1584;
        if (
          _M0L6_2atmpS1583 < 0
          || _M0L6_2atmpS1583 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3759 = (void*)_M0L3srcS55[_M0L6_2atmpS1583];
        _M0L6_2atmpS1582 = _M0L6_2atmpS3759;
        if (
          _M0L6_2atmpS1581 < 0
          || _M0L6_2atmpS1581 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3758 = (void*)_M0L3dstS54[_M0L6_2atmpS1581];
        moonbit_incref(_M0L6_2atmpS1582);
        moonbit_decref(_M0L6_2aoldS3758);
        _M0L3dstS54[_M0L6_2atmpS1581] = _M0L6_2atmpS1582;
        _M0L6_2atmpS1584 = _M0L1iS58 + 1;
        _M0L1iS58 = _M0L6_2atmpS1584;
        continue;
      } else {
        moonbit_decref(_M0L3srcS55);
        moonbit_decref(_M0L3dstS54);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1589 = _M0L3lenS59 - 1;
    int32_t _M0L1iS61 = _M0L6_2atmpS1589;
    while (1) {
      if (_M0L1iS61 >= 0) {
        int32_t _M0L6_2atmpS1585 = _M0L11dst__offsetS56 + _M0L1iS61;
        int32_t _M0L6_2atmpS1587 = _M0L11src__offsetS57 + _M0L1iS61;
        void* _M0L6_2atmpS3761;
        void* _M0L6_2atmpS1586;
        void* _M0L6_2aoldS3760;
        int32_t _M0L6_2atmpS1588;
        if (
          _M0L6_2atmpS1587 < 0
          || _M0L6_2atmpS1587 >= Moonbit_array_length(_M0L3srcS55)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3761 = (void*)_M0L3srcS55[_M0L6_2atmpS1587];
        _M0L6_2atmpS1586 = _M0L6_2atmpS3761;
        if (
          _M0L6_2atmpS1585 < 0
          || _M0L6_2atmpS1585 >= Moonbit_array_length(_M0L3dstS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3760 = (void*)_M0L3dstS54[_M0L6_2atmpS1585];
        moonbit_incref(_M0L6_2atmpS1586);
        moonbit_decref(_M0L6_2aoldS3760);
        _M0L3dstS54[_M0L6_2atmpS1585] = _M0L6_2atmpS1586;
        _M0L6_2atmpS1588 = _M0L1iS61 - 1;
        _M0L1iS61 = _M0L6_2atmpS1588;
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
  moonbit_string_t _M0L6_2atmpS1538;
  moonbit_string_t _M0L6_2atmpS3764;
  moonbit_string_t _M0L6_2atmpS1536;
  moonbit_string_t _M0L6_2atmpS1537;
  moonbit_string_t _M0L6_2atmpS3763;
  moonbit_string_t _M0L6_2atmpS1535;
  moonbit_string_t _M0L6_2atmpS3762;
  moonbit_string_t _M0L6_2atmpS1534;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1538 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3764
  = moonbit_add_string(_M0L6_2atmpS1538, (moonbit_string_t)moonbit_string_literal_93.data);
  moonbit_decref(_M0L6_2atmpS1538);
  _M0L6_2atmpS1536 = _M0L6_2atmpS3764;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1537
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3763 = moonbit_add_string(_M0L6_2atmpS1536, _M0L6_2atmpS1537);
  moonbit_decref(_M0L6_2atmpS1536);
  moonbit_decref(_M0L6_2atmpS1537);
  _M0L6_2atmpS1535 = _M0L6_2atmpS3763;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3762
  = moonbit_add_string(_M0L6_2atmpS1535, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1535);
  _M0L6_2atmpS1534 = _M0L6_2atmpS3762;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1534);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1543;
  moonbit_string_t _M0L6_2atmpS3767;
  moonbit_string_t _M0L6_2atmpS1541;
  moonbit_string_t _M0L6_2atmpS1542;
  moonbit_string_t _M0L6_2atmpS3766;
  moonbit_string_t _M0L6_2atmpS1540;
  moonbit_string_t _M0L6_2atmpS3765;
  moonbit_string_t _M0L6_2atmpS1539;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1543 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3767
  = moonbit_add_string(_M0L6_2atmpS1543, (moonbit_string_t)moonbit_string_literal_93.data);
  moonbit_decref(_M0L6_2atmpS1543);
  _M0L6_2atmpS1541 = _M0L6_2atmpS3767;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1542
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3766 = moonbit_add_string(_M0L6_2atmpS1541, _M0L6_2atmpS1542);
  moonbit_decref(_M0L6_2atmpS1541);
  moonbit_decref(_M0L6_2atmpS1542);
  _M0L6_2atmpS1540 = _M0L6_2atmpS3766;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3765
  = moonbit_add_string(_M0L6_2atmpS1540, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1540);
  _M0L6_2atmpS1539 = _M0L6_2atmpS3765;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1539);
  return 0;
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1548;
  moonbit_string_t _M0L6_2atmpS3770;
  moonbit_string_t _M0L6_2atmpS1546;
  moonbit_string_t _M0L6_2atmpS1547;
  moonbit_string_t _M0L6_2atmpS3769;
  moonbit_string_t _M0L6_2atmpS1545;
  moonbit_string_t _M0L6_2atmpS3768;
  moonbit_string_t _M0L6_2atmpS1544;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1548 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3770
  = moonbit_add_string(_M0L6_2atmpS1548, (moonbit_string_t)moonbit_string_literal_93.data);
  moonbit_decref(_M0L6_2atmpS1548);
  _M0L6_2atmpS1546 = _M0L6_2atmpS3770;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1547
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3769 = moonbit_add_string(_M0L6_2atmpS1546, _M0L6_2atmpS1547);
  moonbit_decref(_M0L6_2atmpS1546);
  moonbit_decref(_M0L6_2atmpS1547);
  _M0L6_2atmpS1545 = _M0L6_2atmpS3769;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3768
  = moonbit_add_string(_M0L6_2atmpS1545, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1545);
  _M0L6_2atmpS1544 = _M0L6_2atmpS3768;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1544);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1553;
  moonbit_string_t _M0L6_2atmpS3773;
  moonbit_string_t _M0L6_2atmpS1551;
  moonbit_string_t _M0L6_2atmpS1552;
  moonbit_string_t _M0L6_2atmpS3772;
  moonbit_string_t _M0L6_2atmpS1550;
  moonbit_string_t _M0L6_2atmpS3771;
  moonbit_string_t _M0L6_2atmpS1549;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1553 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3773
  = moonbit_add_string(_M0L6_2atmpS1553, (moonbit_string_t)moonbit_string_literal_93.data);
  moonbit_decref(_M0L6_2atmpS1553);
  _M0L6_2atmpS1551 = _M0L6_2atmpS3773;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1552
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3772 = moonbit_add_string(_M0L6_2atmpS1551, _M0L6_2atmpS1552);
  moonbit_decref(_M0L6_2atmpS1551);
  moonbit_decref(_M0L6_2atmpS1552);
  _M0L6_2atmpS1550 = _M0L6_2atmpS3772;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3771
  = moonbit_add_string(_M0L6_2atmpS1550, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_decref(_M0L6_2atmpS1550);
  _M0L6_2atmpS1549 = _M0L6_2atmpS3771;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1549);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5valueS18
) {
  uint32_t _M0L3accS1533;
  uint32_t _M0L6_2atmpS1532;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1533 = _M0L4selfS17->$0;
  _M0L6_2atmpS1532 = _M0L3accS1533 + 4u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1532;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS17, _M0L5valueS18);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS15,
  uint32_t _M0L5inputS16
) {
  uint32_t _M0L3accS1530;
  uint32_t _M0L6_2atmpS1531;
  uint32_t _M0L6_2atmpS1529;
  uint32_t _M0L6_2atmpS1528;
  uint32_t _M0L6_2atmpS1527;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1530 = _M0L4selfS15->$0;
  _M0L6_2atmpS1531 = _M0L5inputS16 * 3266489917u;
  _M0L6_2atmpS1529 = _M0L3accS1530 + _M0L6_2atmpS1531;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1528 = _M0FPB4rotl(_M0L6_2atmpS1529, 17);
  _M0L6_2atmpS1527 = _M0L6_2atmpS1528 * 668265263u;
  _M0L4selfS15->$0 = _M0L6_2atmpS1527;
  moonbit_decref(_M0L4selfS15);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS13, int32_t _M0L1rS14) {
  uint32_t _M0L6_2atmpS1524;
  int32_t _M0L6_2atmpS1526;
  uint32_t _M0L6_2atmpS1525;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1524 = _M0L1xS13 << (_M0L1rS14 & 31);
  _M0L6_2atmpS1526 = 32 - _M0L1rS14;
  _M0L6_2atmpS1525 = _M0L1xS13 >> (_M0L6_2atmpS1526 & 31);
  return _M0L6_2atmpS1524 | _M0L6_2atmpS1525;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S9,
  struct _M0TPB6Logger _M0L10_2ax__4934S12
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS10;
  moonbit_string_t _M0L8_2afieldS3774;
  int32_t _M0L6_2acntS3879;
  moonbit_string_t _M0L15_2a_2aarg__4935S11;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS10
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S9;
  _M0L8_2afieldS3774 = _M0L10_2aFailureS10->$0;
  _M0L6_2acntS3879 = Moonbit_object_header(_M0L10_2aFailureS10)->rc;
  if (_M0L6_2acntS3879 > 1) {
    int32_t _M0L11_2anew__cntS3880 = _M0L6_2acntS3879 - 1;
    Moonbit_object_header(_M0L10_2aFailureS10)->rc = _M0L11_2anew__cntS3880;
    moonbit_incref(_M0L8_2afieldS3774);
  } else if (_M0L6_2acntS3879 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS10);
  }
  _M0L15_2a_2aarg__4935S11 = _M0L8_2afieldS3774;
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_94.data);
  if (_M0L10_2ax__4934S12.$1) {
    moonbit_incref(_M0L10_2ax__4934S12.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S12, _M0L15_2a_2aarg__4935S11);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S12.$0->$method_0(_M0L10_2ax__4934S12.$1, (moonbit_string_t)moonbit_string_literal_95.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS8) {
  void* _block_4079;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4079 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4079)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4079)->$0 = _M0L4selfS8;
  return _block_4079;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS7) {
  void* _block_4080;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4080 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4080)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4080)->$0 = _M0L5arrayS7;
  return _block_4080;
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

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t _M0L3msgS3) {
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1450) {
  switch (Moonbit_object_tag(_M0L4_2aeS1450)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1450);
      return (moonbit_string_t)moonbit_string_literal_96.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1450);
      return (moonbit_string_t)moonbit_string_literal_97.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1450);
      return (moonbit_string_t)moonbit_string_literal_98.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1450);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1450);
      return (moonbit_string_t)moonbit_string_literal_99.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1486,
  int32_t _M0L8_2aparamS1485
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1484 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1486;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1484, _M0L8_2aparamS1485);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1483,
  struct _M0TPC16string10StringView _M0L8_2aparamS1482
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1481 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1483;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1481, _M0L8_2aparamS1482);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1480,
  moonbit_string_t _M0L8_2aparamS1477,
  int32_t _M0L8_2aparamS1478,
  int32_t _M0L8_2aparamS1479
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1476 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1480;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1476, _M0L8_2aparamS1477, _M0L8_2aparamS1478, _M0L8_2aparamS1479);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1475,
  moonbit_string_t _M0L8_2aparamS1474
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1473 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1475;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1473, _M0L8_2aparamS1474);
  return 0;
}

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsE(
  void* _M0L11_2aobj__ptrS1472
) {
  struct _M0TPB5ArrayGsE* _M0L7_2aselfS1471 =
    (struct _M0TPB5ArrayGsE*)_M0L11_2aobj__ptrS1472;
  return _M0IPC15array5ArrayPB6ToJson8to__jsonGsE(_M0L7_2aselfS1471);
}

void* _M0IPB3MapPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGsRPC16string10StringViewE(
  void* _M0L11_2aobj__ptrS1470
) {
  struct _M0TPB3MapGsRPC16string10StringViewE* _M0L7_2aselfS1469 =
    (struct _M0TPB3MapGsRPC16string10StringViewE*)_M0L11_2aobj__ptrS1470;
  return _M0IPB3MapPB6ToJson8to__jsonGsRPC16string10StringViewE(_M0L7_2aselfS1469);
}

void moonbit_init() {
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1372 =
    (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1523 =
    _M0L7_2abindS1372;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1522 =
    (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){0,
                                                             0,
                                                             _M0L6_2atmpS1523};
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1521;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1494;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1373;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1520;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1519;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1518;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1495;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1374;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1517;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1516;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1515;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1496;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1375;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1514;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1513;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1512;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1497;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1376;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1511;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1510;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1509;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1498;
  moonbit_string_t* _M0L6_2atmpS1508;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1507;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1503;
  moonbit_string_t* _M0L6_2atmpS1506;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1505;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1504;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1377;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1502;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1501;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1500;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1499;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1371;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1493;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1492;
  #line 398 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1521
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1522);
  _M0L8_2atupleS1494
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1494)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1494->$0 = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L8_2atupleS1494->$1 = _M0L6_2atmpS1521;
  _M0L7_2abindS1373
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1520 = _M0L7_2abindS1373;
  _M0L6_2atmpS1519
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1520
  };
  #line 400 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1518
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1519);
  _M0L8_2atupleS1495
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1495)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1495->$0 = (moonbit_string_t)moonbit_string_literal_101.data;
  _M0L8_2atupleS1495->$1 = _M0L6_2atmpS1518;
  _M0L7_2abindS1374
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1517 = _M0L7_2abindS1374;
  _M0L6_2atmpS1516
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1517
  };
  #line 402 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1515
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1516);
  _M0L8_2atupleS1496
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1496)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1496->$0 = (moonbit_string_t)moonbit_string_literal_102.data;
  _M0L8_2atupleS1496->$1 = _M0L6_2atmpS1515;
  _M0L7_2abindS1375
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1514 = _M0L7_2abindS1375;
  _M0L6_2atmpS1513
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1514
  };
  #line 404 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1512
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1513);
  _M0L8_2atupleS1497
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1497)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1497->$0 = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L8_2atupleS1497->$1 = _M0L6_2atmpS1512;
  _M0L7_2abindS1376
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1511 = _M0L7_2abindS1376;
  _M0L6_2atmpS1510
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1511
  };
  #line 406 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1509
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1510);
  _M0L8_2atupleS1498
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1498)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1498->$0 = (moonbit_string_t)moonbit_string_literal_104.data;
  _M0L8_2atupleS1498->$1 = _M0L6_2atmpS1509;
  _M0L6_2atmpS1508 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1508[0] = (moonbit_string_t)moonbit_string_literal_105.data;
  moonbit_incref(_M0FP48clawteam8clawteam5tools4todo41____test__6d616e616765722e6d6274__0_2eclo);
  _M0L8_2atupleS1507
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1507)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1507->$0
  = _M0FP48clawteam8clawteam5tools4todo41____test__6d616e616765722e6d6274__0_2eclo;
  _M0L8_2atupleS1507->$1 = _M0L6_2atmpS1508;
  _M0L8_2atupleS1503
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1503)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1503->$0 = 0;
  _M0L8_2atupleS1503->$1 = _M0L8_2atupleS1507;
  _M0L6_2atmpS1506 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1506[0] = (moonbit_string_t)moonbit_string_literal_106.data;
  moonbit_incref(_M0FP48clawteam8clawteam5tools4todo41____test__6d616e616765722e6d6274__1_2eclo);
  _M0L8_2atupleS1505
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1505)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1505->$0
  = _M0FP48clawteam8clawteam5tools4todo41____test__6d616e616765722e6d6274__1_2eclo;
  _M0L8_2atupleS1505->$1 = _M0L6_2atmpS1506;
  _M0L8_2atupleS1504
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1504)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1504->$0 = 1;
  _M0L8_2atupleS1504->$1 = _M0L8_2atupleS1505;
  _M0L7_2abindS1377
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1377[0] = _M0L8_2atupleS1503;
  _M0L7_2abindS1377[1] = _M0L8_2atupleS1504;
  _M0L6_2atmpS1502 = _M0L7_2abindS1377;
  _M0L6_2atmpS1501
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 2, _M0L6_2atmpS1502
  };
  #line 408 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1500
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1501);
  _M0L8_2atupleS1499
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1499)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1499->$0 = (moonbit_string_t)moonbit_string_literal_107.data;
  _M0L8_2atupleS1499->$1 = _M0L6_2atmpS1500;
  _M0L7_2abindS1371
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(6);
  _M0L7_2abindS1371[0] = _M0L8_2atupleS1494;
  _M0L7_2abindS1371[1] = _M0L8_2atupleS1495;
  _M0L7_2abindS1371[2] = _M0L8_2atupleS1496;
  _M0L7_2abindS1371[3] = _M0L8_2atupleS1497;
  _M0L7_2abindS1371[4] = _M0L8_2atupleS1498;
  _M0L7_2abindS1371[5] = _M0L8_2atupleS1499;
  _M0L6_2atmpS1493 = _M0L7_2abindS1371;
  _M0L6_2atmpS1492
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 6, _M0L6_2atmpS1493
  };
  #line 397 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools4todo48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1492);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1491;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1444;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1445;
  int32_t _M0L7_2abindS1446;
  int32_t _M0L2__S1447;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1491
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1444
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1444)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1444->$0 = _M0L6_2atmpS1491;
  _M0L12async__testsS1444->$1 = 0;
  #line 449 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1445
  = _M0FP48clawteam8clawteam5tools4todo52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1446 = _M0L7_2abindS1445->$1;
  _M0L2__S1447 = 0;
  while (1) {
    if (_M0L2__S1447 < _M0L7_2abindS1446) {
      struct _M0TUsiE** _M0L8_2afieldS3778 = _M0L7_2abindS1445->$0;
      struct _M0TUsiE** _M0L3bufS1490 = _M0L8_2afieldS3778;
      struct _M0TUsiE* _M0L6_2atmpS3777 =
        (struct _M0TUsiE*)_M0L3bufS1490[_M0L2__S1447];
      struct _M0TUsiE* _M0L3argS1448 = _M0L6_2atmpS3777;
      moonbit_string_t _M0L8_2afieldS3776 = _M0L3argS1448->$0;
      moonbit_string_t _M0L6_2atmpS1487 = _M0L8_2afieldS3776;
      int32_t _M0L8_2afieldS3775 = _M0L3argS1448->$1;
      int32_t _M0L6_2atmpS1488 = _M0L8_2afieldS3775;
      int32_t _M0L6_2atmpS1489;
      moonbit_incref(_M0L6_2atmpS1487);
      moonbit_incref(_M0L12async__testsS1444);
      #line 450 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam5tools4todo44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1444, _M0L6_2atmpS1487, _M0L6_2atmpS1488);
      _M0L6_2atmpS1489 = _M0L2__S1447 + 1;
      _M0L2__S1447 = _M0L6_2atmpS1489;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1445);
    }
    break;
  }
  #line 452 "E:\\moonbit\\clawteam\\tools\\todo\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam5tools4todo28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools4todo34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1444);
  return 0;
}