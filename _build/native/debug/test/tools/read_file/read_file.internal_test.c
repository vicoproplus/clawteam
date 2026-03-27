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
struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPC13ref3RefGOOiE;

struct _M0DTPC16result6ResultGiRPC14json15JsonDecodeErrorE2Ok;

struct _M0TPB6Logger;

struct _M0TPB19MulShiftAll64Result;

struct _M0KTPB6ToJsonTP48clawteam8clawteam5tools10read__file13ReadFileInput;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools10read__file33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGiRPC14json15JsonDecodeErrorE3Err;

struct _M0DTPC14json8JsonPath3Key;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TURPC14json8JsonPathsE;

struct _M0TPB6Hasher;

struct _M0R38String_3a_3aiter_2eanon__u2105__l247__;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools10read__file13ReadFileInputRPC14json15JsonDecodeErrorE2Ok;

struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC14json8JsonPath5Index;

struct _M0TPB9ArrayViewGsE;

struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__;

struct _M0DTPC15error5Error110clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE3Err;

struct _M0TWEuQRPC15error5Error;

struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385;

struct _M0DTPC16result6ResultGuRPC14json15JsonDecodeErrorE2Ok;

struct _M0TPC13ref3RefGOsE;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC14json15JsonDecodeErrorE3Err;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE2Ok;

struct _M0TUsRPB4JsonE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPB4Json6Object;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__;

struct _M0DTPC16option6OptionGOiE4Some;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools10read__file13ReadFileInputRPC14json15JsonDecodeErrorE3Err;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools10read__file33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TPC13ref3RefGOOiE {
  void* $0;
  
};

struct _M0DTPC16result6ResultGiRPC14json15JsonDecodeErrorE2Ok {
  int32_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0KTPB6ToJsonTP48clawteam8clawteam5tools10read__file13ReadFileInput {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools10read__file33MoonBitTestDriverInternalSkipTestE3Err {
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

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__ {
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

struct _M0DTPC16result6ResultGiRPC14json15JsonDecodeErrorE3Err {
  void* $0;
  
};

struct _M0DTPC14json8JsonPath3Key {
  void* $0;
  moonbit_string_t $1;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TURPC14json8JsonPathsE {
  void* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2105__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools10read__file13ReadFileInputRPC14json15JsonDecodeErrorE2Ok {
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* $0;
  
};

struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC14json8JsonPath5Index {
  int32_t $1;
  void* $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput {
  int64_t $1;
  int64_t $2;
  moonbit_string_t $0;
  
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

struct _M0TPB9ArrayViewGUsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB4JsonE** $0;
  
};

struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError {
  struct _M0TURPC14json8JsonPathsE* $0;
  
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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0DTPC15error5Error110clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE3Err {
  void* $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGuRPC14json15JsonDecodeErrorE2Ok {
  int32_t $0;
  
};

struct _M0TPC13ref3RefGOsE {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGuRPC14json15JsonDecodeErrorE3Err {
  void* $0;
  
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

struct _M0DTPC16result6ResultGsRPC14json15JsonDecodeErrorE2Ok {
  moonbit_string_t $0;
  
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

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
};

struct _M0DTPC16option6OptionGOiE4Some {
  int64_t $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam5tools10read__file13ReadFileInputRPC14json15JsonDecodeErrorE3Err {
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

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5tools10read__file33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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
  union {
    struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* ok;
    void* err;
    
  } data;
  
};

struct moonbit_result_2 {
  int tag;
  union { moonbit_string_t ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools10read__file49____test__726561645f66696c652e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1394(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN14handle__resultS1385(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testC3153l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testC3149l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam5tools10read__file45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1318(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1313(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1300(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools10read__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools10read__file34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools10read__file39____test__726561645f66696c652e6d6274__0(
  
);

void* _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput*
);

struct moonbit_result_1 _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPC14json8FromJson10from__json(
  void*,
  void*
);

struct moonbit_result_2 _M0IPC16string6StringPC14json8FromJson10from__json(
  void*,
  void*
);

struct moonbit_result_0 _M0IPC13int3IntPC14json8FromJson10from__json(
  void*,
  void*
);

struct moonbit_result_1 _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(
  void*,
  void*
);

struct moonbit_result_1 _M0FPC14json18from__json_2einnerGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(
  void*,
  void*
);

int32_t _M0IPC14json8JsonPathPB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(
  void*,
  struct _M0TPB6Logger
);

void* _M0MPC14json8JsonPath8add__key(void*, moonbit_string_t);

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

struct moonbit_result_2 _M0FPC14json13decode__errorGsE(
  void*,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPC14json13decode__errorGiE(
  void*,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPC14json13decode__errorGuE(
  void*,
  moonbit_string_t
);

int32_t _M0IPC14json15JsonDecodeErrorPB4Show6output(
  void*,
  struct _M0TPB6Logger
);

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

int32_t _M0MPC16double6Double7to__int(double);

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

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2388l591(
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

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2124l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2105l247(struct _M0TWEOc*);

int32_t _M0MPC16string6String13contains__any(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView13contains__any(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView14contains__char(
  struct _M0TPC16string10StringView,
  int32_t
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

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView4iterC1978l198(struct _M0TWEOc*);

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

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView,
  int32_t
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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPC14json15JsonDecodeErrorE(
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

int32_t _M0MPB6Logger13write__objectGRPC14json8JsonPathE(
  struct _M0TPB6Logger,
  void*
);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0MPB6Logger13write__objectGiE(struct _M0TPB6Logger, int32_t);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

int64_t _M0FPC15abort5abortGOiE(moonbit_string_t);

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

void* _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_1 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    112, 97, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    74, 115, 111, 110, 68, 101, 99, 111, 100, 101, 69, 114, 114, 111, 
    114, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_67 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    77, 105, 115, 115, 105, 110, 103, 32, 102, 105, 101, 108, 100, 32, 
    112, 97, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 50, 54, 58, 57, 45, 
    52, 50, 54, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 54, 56, 58, 51, 49, 45, 54, 56, 58, 55, 
    51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 53, 56, 58, 49, 54, 45, 53, 56, 58, 50, 
    48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_66 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_53 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    101, 120, 97, 109, 112, 108, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_44 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[45]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 44), 
    69, 120, 112, 101, 99, 116, 101, 100, 32, 111, 98, 106, 101, 99, 
    116, 32, 116, 111, 32, 100, 101, 115, 101, 114, 105, 97, 108, 105, 
    122, 101, 32, 82, 101, 97, 100, 70, 105, 108, 101, 73, 110, 112, 
    117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    44, 32, 34, 109, 101, 115, 115, 97, 103, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[100]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 99), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 95, 
    102, 105, 108, 101, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 54, 53, 58, 51, 49, 45, 54, 53, 58, 53, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    114, 101, 97, 100, 95, 102, 105, 108, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    73, 110, 116, 58, 58, 102, 114, 111, 109, 95, 106, 115, 111, 110, 
    58, 32, 111, 118, 101, 114, 102, 108, 111, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_71 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    83, 116, 114, 105, 110, 103, 58, 58, 102, 114, 111, 109, 95, 106, 
    115, 111, 110, 58, 32, 101, 120, 112, 101, 99, 116, 101, 100, 32, 
    115, 116, 114, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    73, 110, 116, 58, 58, 102, 114, 111, 109, 95, 106, 115, 111, 110, 
    58, 32, 101, 120, 112, 101, 99, 116, 101, 100, 32, 110, 117, 109, 
    98, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 51, 50, 57, 58, 53, 45, 
    51, 50, 57, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 54, 56, 58, 51, 45, 54, 56, 58, 55, 52, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 53, 56, 58, 51, 48, 45, 54, 50, 58, 52, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 53, 56, 58, 51, 45, 54, 50, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 55, 48, 58, 51, 45, 55, 51, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    82, 101, 97, 100, 70, 105, 108, 101, 73, 110, 112, 117, 116, 58, 
    58, 102, 114, 111, 109, 95, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 55, 48, 58, 49, 54, 45, 55, 48, 58, 53, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 54, 56, 58, 49, 54, 45, 54, 56, 58, 50, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 55, 48, 58, 54, 54, 45, 55, 51, 58, 52, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 54, 53, 58, 51, 45, 54, 53, 58, 53, 55, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    101, 110, 100, 95, 108, 105, 110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    73, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_87 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_64 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 40, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 
    95, 102, 105, 108, 101, 58, 114, 101, 97, 100, 95, 102, 105, 108, 
    101, 46, 109, 98, 116, 58, 54, 53, 58, 49, 54, 45, 54, 53, 58, 50, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    126, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 95, 102, 105, 
    108, 101, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 
    34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[102]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 101), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 116, 111, 111, 108, 115, 47, 114, 101, 97, 100, 95, 
    102, 105, 108, 101, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    126, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    115, 99, 104, 101, 109, 97, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    126, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam5tools10read__file49____test__726561645f66696c652e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools10read__file49____test__726561645f66696c652e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1394$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1394
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam5tools10read__file45____test__726561645f66696c652e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam5tools10read__file49____test__726561645f66696c652e6d6274__0_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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

double _M0FPC16double8infinity;

double _M0FPC16double13neg__infinity;

moonbit_string_t _M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376 =
  (moonbit_string_t)moonbit_string_literal_0.data;

struct { int32_t rc; uint32_t meta; struct _M0TPB17FloatingDecimal64 data; 
} _M0FPB31ryu__to__string_2erecord_2f1050$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1050 =
  &_M0FPB31ryu__to__string_2erecord_2f1050$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam5tools10read__file48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools10read__file49____test__726561645f66696c652e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3184
) {
  return _M0FP48clawteam8clawteam5tools10read__file39____test__726561645f66696c652e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1415,
  moonbit_string_t _M0L8filenameS1390,
  int32_t _M0L5indexS1393
) {
  struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385* _closure_3609;
  struct _M0TWssbEu* _M0L14handle__resultS1385;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1394;
  void* _M0L11_2atry__errS1409;
  struct moonbit_result_0 _tmp_3611;
  int32_t _handle__error__result_3612;
  int32_t _M0L6_2atmpS3172;
  void* _M0L3errS1410;
  moonbit_string_t _M0L4nameS1412;
  struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1413;
  moonbit_string_t _M0L8_2afieldS3185;
  int32_t _M0L6_2acntS3496;
  moonbit_string_t _M0L7_2anameS1414;
  #line 526 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1390);
  _closure_3609
  = (struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385*)moonbit_malloc(sizeof(struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385));
  Moonbit_object_header(_closure_3609)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385, $1) >> 2, 1, 0);
  _closure_3609->code
  = &_M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN14handle__resultS1385;
  _closure_3609->$0 = _M0L5indexS1393;
  _closure_3609->$1 = _M0L8filenameS1390;
  _M0L14handle__resultS1385 = (struct _M0TWssbEu*)_closure_3609;
  _M0L17error__to__stringS1394
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1394$closure.data;
  moonbit_incref(_M0L12async__testsS1415);
  moonbit_incref(_M0L17error__to__stringS1394);
  moonbit_incref(_M0L8filenameS1390);
  moonbit_incref(_M0L14handle__resultS1385);
  #line 560 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _tmp_3611
  = _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__test(_M0L12async__testsS1415, _M0L8filenameS1390, _M0L5indexS1393, _M0L14handle__resultS1385, _M0L17error__to__stringS1394);
  if (_tmp_3611.tag) {
    int32_t const _M0L5_2aokS3181 = _tmp_3611.data.ok;
    _handle__error__result_3612 = _M0L5_2aokS3181;
  } else {
    void* const _M0L6_2aerrS3182 = _tmp_3611.data.err;
    moonbit_decref(_M0L12async__testsS1415);
    moonbit_decref(_M0L17error__to__stringS1394);
    moonbit_decref(_M0L8filenameS1390);
    _M0L11_2atry__errS1409 = _M0L6_2aerrS3182;
    goto join_1408;
  }
  if (_handle__error__result_3612) {
    moonbit_decref(_M0L12async__testsS1415);
    moonbit_decref(_M0L17error__to__stringS1394);
    moonbit_decref(_M0L8filenameS1390);
    _M0L6_2atmpS3172 = 1;
  } else {
    struct moonbit_result_0 _tmp_3613;
    int32_t _handle__error__result_3614;
    moonbit_incref(_M0L12async__testsS1415);
    moonbit_incref(_M0L17error__to__stringS1394);
    moonbit_incref(_M0L8filenameS1390);
    moonbit_incref(_M0L14handle__resultS1385);
    #line 563 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    _tmp_3613
    = _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1415, _M0L8filenameS1390, _M0L5indexS1393, _M0L14handle__resultS1385, _M0L17error__to__stringS1394);
    if (_tmp_3613.tag) {
      int32_t const _M0L5_2aokS3179 = _tmp_3613.data.ok;
      _handle__error__result_3614 = _M0L5_2aokS3179;
    } else {
      void* const _M0L6_2aerrS3180 = _tmp_3613.data.err;
      moonbit_decref(_M0L12async__testsS1415);
      moonbit_decref(_M0L17error__to__stringS1394);
      moonbit_decref(_M0L8filenameS1390);
      _M0L11_2atry__errS1409 = _M0L6_2aerrS3180;
      goto join_1408;
    }
    if (_handle__error__result_3614) {
      moonbit_decref(_M0L12async__testsS1415);
      moonbit_decref(_M0L17error__to__stringS1394);
      moonbit_decref(_M0L8filenameS1390);
      _M0L6_2atmpS3172 = 1;
    } else {
      struct moonbit_result_0 _tmp_3615;
      int32_t _handle__error__result_3616;
      moonbit_incref(_M0L12async__testsS1415);
      moonbit_incref(_M0L17error__to__stringS1394);
      moonbit_incref(_M0L8filenameS1390);
      moonbit_incref(_M0L14handle__resultS1385);
      #line 566 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _tmp_3615
      = _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1415, _M0L8filenameS1390, _M0L5indexS1393, _M0L14handle__resultS1385, _M0L17error__to__stringS1394);
      if (_tmp_3615.tag) {
        int32_t const _M0L5_2aokS3177 = _tmp_3615.data.ok;
        _handle__error__result_3616 = _M0L5_2aokS3177;
      } else {
        void* const _M0L6_2aerrS3178 = _tmp_3615.data.err;
        moonbit_decref(_M0L12async__testsS1415);
        moonbit_decref(_M0L17error__to__stringS1394);
        moonbit_decref(_M0L8filenameS1390);
        _M0L11_2atry__errS1409 = _M0L6_2aerrS3178;
        goto join_1408;
      }
      if (_handle__error__result_3616) {
        moonbit_decref(_M0L12async__testsS1415);
        moonbit_decref(_M0L17error__to__stringS1394);
        moonbit_decref(_M0L8filenameS1390);
        _M0L6_2atmpS3172 = 1;
      } else {
        struct moonbit_result_0 _tmp_3617;
        int32_t _handle__error__result_3618;
        moonbit_incref(_M0L12async__testsS1415);
        moonbit_incref(_M0L17error__to__stringS1394);
        moonbit_incref(_M0L8filenameS1390);
        moonbit_incref(_M0L14handle__resultS1385);
        #line 569 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        _tmp_3617
        = _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1415, _M0L8filenameS1390, _M0L5indexS1393, _M0L14handle__resultS1385, _M0L17error__to__stringS1394);
        if (_tmp_3617.tag) {
          int32_t const _M0L5_2aokS3175 = _tmp_3617.data.ok;
          _handle__error__result_3618 = _M0L5_2aokS3175;
        } else {
          void* const _M0L6_2aerrS3176 = _tmp_3617.data.err;
          moonbit_decref(_M0L12async__testsS1415);
          moonbit_decref(_M0L17error__to__stringS1394);
          moonbit_decref(_M0L8filenameS1390);
          _M0L11_2atry__errS1409 = _M0L6_2aerrS3176;
          goto join_1408;
        }
        if (_handle__error__result_3618) {
          moonbit_decref(_M0L12async__testsS1415);
          moonbit_decref(_M0L17error__to__stringS1394);
          moonbit_decref(_M0L8filenameS1390);
          _M0L6_2atmpS3172 = 1;
        } else {
          struct moonbit_result_0 _tmp_3619;
          moonbit_incref(_M0L14handle__resultS1385);
          #line 572 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
          _tmp_3619
          = _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1415, _M0L8filenameS1390, _M0L5indexS1393, _M0L14handle__resultS1385, _M0L17error__to__stringS1394);
          if (_tmp_3619.tag) {
            int32_t const _M0L5_2aokS3173 = _tmp_3619.data.ok;
            _M0L6_2atmpS3172 = _M0L5_2aokS3173;
          } else {
            void* const _M0L6_2aerrS3174 = _tmp_3619.data.err;
            _M0L11_2atry__errS1409 = _M0L6_2aerrS3174;
            goto join_1408;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3172) {
    void* _M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3183 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3183)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
    ((struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3183)->$0
    = (moonbit_string_t)moonbit_string_literal_1.data;
    _M0L11_2atry__errS1409
    = _M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3183;
    goto join_1408;
  } else {
    moonbit_decref(_M0L14handle__resultS1385);
  }
  goto joinlet_3610;
  join_1408:;
  _M0L3errS1410 = _M0L11_2atry__errS1409;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1413
  = (struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1410;
  _M0L8_2afieldS3185 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1413->$0;
  _M0L6_2acntS3496
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1413)->rc;
  if (_M0L6_2acntS3496 > 1) {
    int32_t _M0L11_2anew__cntS3497 = _M0L6_2acntS3496 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1413)->rc
    = _M0L11_2anew__cntS3497;
    moonbit_incref(_M0L8_2afieldS3185);
  } else if (_M0L6_2acntS3496 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1413);
  }
  _M0L7_2anameS1414 = _M0L8_2afieldS3185;
  _M0L4nameS1412 = _M0L7_2anameS1414;
  goto join_1411;
  goto joinlet_3620;
  join_1411:;
  #line 580 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN14handle__resultS1385(_M0L14handle__resultS1385, _M0L4nameS1412, (moonbit_string_t)moonbit_string_literal_2.data, 1);
  joinlet_3620:;
  joinlet_3610:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN17error__to__stringS1394(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3171,
  void* _M0L3errS1395
) {
  void* _M0L1eS1397;
  moonbit_string_t _M0L1eS1399;
  #line 549 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3171);
  switch (Moonbit_object_tag(_M0L3errS1395)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1400 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1395;
      moonbit_string_t _M0L8_2afieldS3186 = _M0L10_2aFailureS1400->$0;
      int32_t _M0L6_2acntS3498 =
        Moonbit_object_header(_M0L10_2aFailureS1400)->rc;
      moonbit_string_t _M0L4_2aeS1401;
      if (_M0L6_2acntS3498 > 1) {
        int32_t _M0L11_2anew__cntS3499 = _M0L6_2acntS3498 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1400)->rc
        = _M0L11_2anew__cntS3499;
        moonbit_incref(_M0L8_2afieldS3186);
      } else if (_M0L6_2acntS3498 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1400);
      }
      _M0L4_2aeS1401 = _M0L8_2afieldS3186;
      _M0L1eS1399 = _M0L4_2aeS1401;
      goto join_1398;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1402 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1395;
      moonbit_string_t _M0L8_2afieldS3187 = _M0L15_2aInspectErrorS1402->$0;
      int32_t _M0L6_2acntS3500 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1402)->rc;
      moonbit_string_t _M0L4_2aeS1403;
      if (_M0L6_2acntS3500 > 1) {
        int32_t _M0L11_2anew__cntS3501 = _M0L6_2acntS3500 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1402)->rc
        = _M0L11_2anew__cntS3501;
        moonbit_incref(_M0L8_2afieldS3187);
      } else if (_M0L6_2acntS3500 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1402);
      }
      _M0L4_2aeS1403 = _M0L8_2afieldS3187;
      _M0L1eS1399 = _M0L4_2aeS1403;
      goto join_1398;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1404 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1395;
      moonbit_string_t _M0L8_2afieldS3188 = _M0L16_2aSnapshotErrorS1404->$0;
      int32_t _M0L6_2acntS3502 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1404)->rc;
      moonbit_string_t _M0L4_2aeS1405;
      if (_M0L6_2acntS3502 > 1) {
        int32_t _M0L11_2anew__cntS3503 = _M0L6_2acntS3502 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1404)->rc
        = _M0L11_2anew__cntS3503;
        moonbit_incref(_M0L8_2afieldS3188);
      } else if (_M0L6_2acntS3502 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1404);
      }
      _M0L4_2aeS1405 = _M0L8_2afieldS3188;
      _M0L1eS1399 = _M0L4_2aeS1405;
      goto join_1398;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error110clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1406 =
        (struct _M0DTPC15error5Error110clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1395;
      moonbit_string_t _M0L8_2afieldS3189 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1406->$0;
      int32_t _M0L6_2acntS3504 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1406)->rc;
      moonbit_string_t _M0L4_2aeS1407;
      if (_M0L6_2acntS3504 > 1) {
        int32_t _M0L11_2anew__cntS3505 = _M0L6_2acntS3504 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1406)->rc
        = _M0L11_2anew__cntS3505;
        moonbit_incref(_M0L8_2afieldS3189);
      } else if (_M0L6_2acntS3504 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1406);
      }
      _M0L4_2aeS1407 = _M0L8_2afieldS3189;
      _M0L1eS1399 = _M0L4_2aeS1407;
      goto join_1398;
      break;
    }
    default: {
      _M0L1eS1397 = _M0L3errS1395;
      goto join_1396;
      break;
    }
  }
  join_1398:;
  return _M0L1eS1399;
  join_1396:;
  #line 555 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1397);
}

int32_t _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__executeN14handle__resultS1385(
  struct _M0TWssbEu* _M0L6_2aenvS3157,
  moonbit_string_t _M0L8testnameS1386,
  moonbit_string_t _M0L7messageS1387,
  int32_t _M0L7skippedS1388
) {
  struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385* _M0L14_2acasted__envS3158;
  moonbit_string_t _M0L8_2afieldS3199;
  moonbit_string_t _M0L8filenameS1390;
  int32_t _M0L8_2afieldS3198;
  int32_t _M0L6_2acntS3506;
  int32_t _M0L5indexS1393;
  int32_t _if__result_3623;
  moonbit_string_t _M0L10file__nameS1389;
  moonbit_string_t _M0L10test__nameS1391;
  moonbit_string_t _M0L7messageS1392;
  moonbit_string_t _M0L6_2atmpS3170;
  moonbit_string_t _M0L6_2atmpS3197;
  moonbit_string_t _M0L6_2atmpS3169;
  moonbit_string_t _M0L6_2atmpS3196;
  moonbit_string_t _M0L6_2atmpS3167;
  moonbit_string_t _M0L6_2atmpS3168;
  moonbit_string_t _M0L6_2atmpS3195;
  moonbit_string_t _M0L6_2atmpS3166;
  moonbit_string_t _M0L6_2atmpS3194;
  moonbit_string_t _M0L6_2atmpS3164;
  moonbit_string_t _M0L6_2atmpS3165;
  moonbit_string_t _M0L6_2atmpS3193;
  moonbit_string_t _M0L6_2atmpS3163;
  moonbit_string_t _M0L6_2atmpS3192;
  moonbit_string_t _M0L6_2atmpS3161;
  moonbit_string_t _M0L6_2atmpS3162;
  moonbit_string_t _M0L6_2atmpS3191;
  moonbit_string_t _M0L6_2atmpS3160;
  moonbit_string_t _M0L6_2atmpS3190;
  moonbit_string_t _M0L6_2atmpS3159;
  #line 533 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3158
  = (struct _M0R114_24clawteam_2fclawteam_2ftools_2fread__file_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1385*)_M0L6_2aenvS3157;
  _M0L8_2afieldS3199 = _M0L14_2acasted__envS3158->$1;
  _M0L8filenameS1390 = _M0L8_2afieldS3199;
  _M0L8_2afieldS3198 = _M0L14_2acasted__envS3158->$0;
  _M0L6_2acntS3506 = Moonbit_object_header(_M0L14_2acasted__envS3158)->rc;
  if (_M0L6_2acntS3506 > 1) {
    int32_t _M0L11_2anew__cntS3507 = _M0L6_2acntS3506 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3158)->rc
    = _M0L11_2anew__cntS3507;
    moonbit_incref(_M0L8filenameS1390);
  } else if (_M0L6_2acntS3506 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3158);
  }
  _M0L5indexS1393 = _M0L8_2afieldS3198;
  if (!_M0L7skippedS1388) {
    _if__result_3623 = 1;
  } else {
    _if__result_3623 = 0;
  }
  if (_if__result_3623) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1389 = _M0MPC16string6String6escape(_M0L8filenameS1390);
  #line 540 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1391 = _M0MPC16string6String6escape(_M0L8testnameS1386);
  #line 541 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1392 = _M0MPC16string6String6escape(_M0L7messageS1387);
  #line 542 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_3.data);
  #line 544 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3170
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1389);
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3197
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_4.data, _M0L6_2atmpS3170);
  moonbit_decref(_M0L6_2atmpS3170);
  _M0L6_2atmpS3169 = _M0L6_2atmpS3197;
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3196
  = moonbit_add_string(_M0L6_2atmpS3169, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3169);
  _M0L6_2atmpS3167 = _M0L6_2atmpS3196;
  #line 544 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3168
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1393);
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3195 = moonbit_add_string(_M0L6_2atmpS3167, _M0L6_2atmpS3168);
  moonbit_decref(_M0L6_2atmpS3167);
  moonbit_decref(_M0L6_2atmpS3168);
  _M0L6_2atmpS3166 = _M0L6_2atmpS3195;
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3194
  = moonbit_add_string(_M0L6_2atmpS3166, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3166);
  _M0L6_2atmpS3164 = _M0L6_2atmpS3194;
  #line 544 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3165
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1391);
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3193 = moonbit_add_string(_M0L6_2atmpS3164, _M0L6_2atmpS3165);
  moonbit_decref(_M0L6_2atmpS3164);
  moonbit_decref(_M0L6_2atmpS3165);
  _M0L6_2atmpS3163 = _M0L6_2atmpS3193;
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3192
  = moonbit_add_string(_M0L6_2atmpS3163, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3163);
  _M0L6_2atmpS3161 = _M0L6_2atmpS3192;
  #line 544 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3162
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1392);
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3191 = moonbit_add_string(_M0L6_2atmpS3161, _M0L6_2atmpS3162);
  moonbit_decref(_M0L6_2atmpS3161);
  moonbit_decref(_M0L6_2atmpS3162);
  _M0L6_2atmpS3160 = _M0L6_2atmpS3191;
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3190
  = moonbit_add_string(_M0L6_2atmpS3160, (moonbit_string_t)moonbit_string_literal_8.data);
  moonbit_decref(_M0L6_2atmpS3160);
  _M0L6_2atmpS3159 = _M0L6_2atmpS3190;
  #line 543 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3159);
  #line 546 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_9.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1384,
  moonbit_string_t _M0L8filenameS1381,
  int32_t _M0L5indexS1375,
  struct _M0TWssbEu* _M0L14handle__resultS1371,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1373
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1351;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1380;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1353;
  moonbit_string_t* _M0L5attrsS1354;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1374;
  moonbit_string_t _M0L4nameS1357;
  moonbit_string_t _M0L4nameS1355;
  int32_t _M0L6_2atmpS3156;
  struct _M0TWEOs* _M0L5_2aitS1359;
  struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__* _closure_3632;
  struct _M0TWEOc* _M0L6_2atmpS3147;
  struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__* _closure_3633;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3148;
  struct moonbit_result_0 _result_3634;
  #line 407 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1384);
  moonbit_incref(_M0FP48clawteam8clawteam5tools10read__file48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1380
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam5tools10read__file48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1381);
  if (_M0L7_2abindS1380 == 0) {
    struct moonbit_result_0 _result_3625;
    if (_M0L7_2abindS1380) {
      moonbit_decref(_M0L7_2abindS1380);
    }
    moonbit_decref(_M0L17error__to__stringS1373);
    moonbit_decref(_M0L14handle__resultS1371);
    _result_3625.tag = 1;
    _result_3625.data.ok = 0;
    return _result_3625;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1382 =
      _M0L7_2abindS1380;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1383 =
      _M0L7_2aSomeS1382;
    _M0L10index__mapS1351 = _M0L13_2aindex__mapS1383;
    goto join_1350;
  }
  join_1350:;
  #line 416 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1374
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1351, _M0L5indexS1375);
  if (_M0L7_2abindS1374 == 0) {
    struct moonbit_result_0 _result_3627;
    if (_M0L7_2abindS1374) {
      moonbit_decref(_M0L7_2abindS1374);
    }
    moonbit_decref(_M0L17error__to__stringS1373);
    moonbit_decref(_M0L14handle__resultS1371);
    _result_3627.tag = 1;
    _result_3627.data.ok = 0;
    return _result_3627;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1376 =
      _M0L7_2abindS1374;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1377 = _M0L7_2aSomeS1376;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3203 = _M0L4_2axS1377->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1378 = _M0L8_2afieldS3203;
    moonbit_string_t* _M0L8_2afieldS3202 = _M0L4_2axS1377->$1;
    int32_t _M0L6_2acntS3508 = Moonbit_object_header(_M0L4_2axS1377)->rc;
    moonbit_string_t* _M0L8_2aattrsS1379;
    if (_M0L6_2acntS3508 > 1) {
      int32_t _M0L11_2anew__cntS3509 = _M0L6_2acntS3508 - 1;
      Moonbit_object_header(_M0L4_2axS1377)->rc = _M0L11_2anew__cntS3509;
      moonbit_incref(_M0L8_2afieldS3202);
      moonbit_incref(_M0L4_2afS1378);
    } else if (_M0L6_2acntS3508 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1377);
    }
    _M0L8_2aattrsS1379 = _M0L8_2afieldS3202;
    _M0L1fS1353 = _M0L4_2afS1378;
    _M0L5attrsS1354 = _M0L8_2aattrsS1379;
    goto join_1352;
  }
  join_1352:;
  _M0L6_2atmpS3156 = Moonbit_array_length(_M0L5attrsS1354);
  if (_M0L6_2atmpS3156 >= 1) {
    moonbit_string_t _M0L6_2atmpS3201 = (moonbit_string_t)_M0L5attrsS1354[0];
    moonbit_string_t _M0L7_2anameS1358 = _M0L6_2atmpS3201;
    moonbit_incref(_M0L7_2anameS1358);
    _M0L4nameS1357 = _M0L7_2anameS1358;
    goto join_1356;
  } else {
    _M0L4nameS1355 = (moonbit_string_t)moonbit_string_literal_1.data;
  }
  goto joinlet_3628;
  join_1356:;
  _M0L4nameS1355 = _M0L4nameS1357;
  joinlet_3628:;
  #line 417 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1359 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1354);
  while (1) {
    moonbit_string_t _M0L4attrS1361;
    moonbit_string_t _M0L7_2abindS1368;
    int32_t _M0L6_2atmpS3140;
    int64_t _M0L6_2atmpS3139;
    moonbit_incref(_M0L5_2aitS1359);
    #line 419 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS1368 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1359);
    if (_M0L7_2abindS1368 == 0) {
      if (_M0L7_2abindS1368) {
        moonbit_decref(_M0L7_2abindS1368);
      }
      moonbit_decref(_M0L5_2aitS1359);
    } else {
      moonbit_string_t _M0L7_2aSomeS1369 = _M0L7_2abindS1368;
      moonbit_string_t _M0L7_2aattrS1370 = _M0L7_2aSomeS1369;
      _M0L4attrS1361 = _M0L7_2aattrS1370;
      goto join_1360;
    }
    goto joinlet_3630;
    join_1360:;
    _M0L6_2atmpS3140 = Moonbit_array_length(_M0L4attrS1361);
    _M0L6_2atmpS3139 = (int64_t)_M0L6_2atmpS3140;
    moonbit_incref(_M0L4attrS1361);
    #line 420 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1361, 5, 0, _M0L6_2atmpS3139)
    ) {
      int32_t _M0L6_2atmpS3146 = _M0L4attrS1361[0];
      int32_t _M0L4_2axS1362 = _M0L6_2atmpS3146;
      if (_M0L4_2axS1362 == 112) {
        int32_t _M0L6_2atmpS3145 = _M0L4attrS1361[1];
        int32_t _M0L4_2axS1363 = _M0L6_2atmpS3145;
        if (_M0L4_2axS1363 == 97) {
          int32_t _M0L6_2atmpS3144 = _M0L4attrS1361[2];
          int32_t _M0L4_2axS1364 = _M0L6_2atmpS3144;
          if (_M0L4_2axS1364 == 110) {
            int32_t _M0L6_2atmpS3143 = _M0L4attrS1361[3];
            int32_t _M0L4_2axS1365 = _M0L6_2atmpS3143;
            if (_M0L4_2axS1365 == 105) {
              int32_t _M0L6_2atmpS3200 = _M0L4attrS1361[4];
              int32_t _M0L6_2atmpS3142;
              int32_t _M0L4_2axS1366;
              moonbit_decref(_M0L4attrS1361);
              _M0L6_2atmpS3142 = _M0L6_2atmpS3200;
              _M0L4_2axS1366 = _M0L6_2atmpS3142;
              if (_M0L4_2axS1366 == 99) {
                void* _M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3141;
                struct moonbit_result_0 _result_3631;
                moonbit_decref(_M0L17error__to__stringS1373);
                moonbit_decref(_M0L14handle__resultS1371);
                moonbit_decref(_M0L5_2aitS1359);
                moonbit_decref(_M0L1fS1353);
                _M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3141
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3141)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
                ((struct _M0DTPC15error5Error112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3141)->$0
                = _M0L4nameS1355;
                _result_3631.tag = 0;
                _result_3631.data.err
                = _M0L112clawteam_2fclawteam_2ftools_2fread__file_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3141;
                return _result_3631;
              }
            } else {
              moonbit_decref(_M0L4attrS1361);
            }
          } else {
            moonbit_decref(_M0L4attrS1361);
          }
        } else {
          moonbit_decref(_M0L4attrS1361);
        }
      } else {
        moonbit_decref(_M0L4attrS1361);
      }
    } else {
      moonbit_decref(_M0L4attrS1361);
    }
    continue;
    joinlet_3630:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1371);
  moonbit_incref(_M0L4nameS1355);
  _closure_3632
  = (struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__*)moonbit_malloc(sizeof(struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__));
  Moonbit_object_header(_closure_3632)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__, $0) >> 2, 2, 0);
  _closure_3632->code
  = &_M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testC3153l427;
  _closure_3632->$0 = _M0L14handle__resultS1371;
  _closure_3632->$1 = _M0L4nameS1355;
  _M0L6_2atmpS3147 = (struct _M0TWEOc*)_closure_3632;
  _closure_3633
  = (struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__*)moonbit_malloc(sizeof(struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__));
  Moonbit_object_header(_closure_3633)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__, $0) >> 2, 3, 0);
  _closure_3633->code
  = &_M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testC3149l428;
  _closure_3633->$0 = _M0L17error__to__stringS1373;
  _closure_3633->$1 = _M0L14handle__resultS1371;
  _closure_3633->$2 = _M0L4nameS1355;
  _M0L6_2atmpS3148 = (struct _M0TWRPC15error5ErrorEu*)_closure_3633;
  #line 425 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools10read__file45moonbit__test__driver__internal__catch__error(_M0L1fS1353, _M0L6_2atmpS3147, _M0L6_2atmpS3148);
  _result_3634.tag = 1;
  _result_3634.data.ok = 1;
  return _result_3634;
}

int32_t _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testC3153l427(
  struct _M0TWEOc* _M0L6_2aenvS3154
) {
  struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__* _M0L14_2acasted__envS3155;
  moonbit_string_t _M0L8_2afieldS3205;
  moonbit_string_t _M0L4nameS1355;
  struct _M0TWssbEu* _M0L8_2afieldS3204;
  int32_t _M0L6_2acntS3510;
  struct _M0TWssbEu* _M0L14handle__resultS1371;
  #line 427 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3155
  = (struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3153__l427__*)_M0L6_2aenvS3154;
  _M0L8_2afieldS3205 = _M0L14_2acasted__envS3155->$1;
  _M0L4nameS1355 = _M0L8_2afieldS3205;
  _M0L8_2afieldS3204 = _M0L14_2acasted__envS3155->$0;
  _M0L6_2acntS3510 = Moonbit_object_header(_M0L14_2acasted__envS3155)->rc;
  if (_M0L6_2acntS3510 > 1) {
    int32_t _M0L11_2anew__cntS3511 = _M0L6_2acntS3510 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3155)->rc
    = _M0L11_2anew__cntS3511;
    moonbit_incref(_M0L4nameS1355);
    moonbit_incref(_M0L8_2afieldS3204);
  } else if (_M0L6_2acntS3510 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3155);
  }
  _M0L14handle__resultS1371 = _M0L8_2afieldS3204;
  #line 427 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1371->code(_M0L14handle__resultS1371, _M0L4nameS1355, (moonbit_string_t)moonbit_string_literal_1.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam5tools10read__file41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testC3149l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3150,
  void* _M0L3errS1372
) {
  struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__* _M0L14_2acasted__envS3151;
  moonbit_string_t _M0L8_2afieldS3208;
  moonbit_string_t _M0L4nameS1355;
  struct _M0TWssbEu* _M0L8_2afieldS3207;
  struct _M0TWssbEu* _M0L14handle__resultS1371;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3206;
  int32_t _M0L6_2acntS3512;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1373;
  moonbit_string_t _M0L6_2atmpS3152;
  #line 428 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3151
  = (struct _M0R197_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2ftools_2fread__file_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3149__l428__*)_M0L6_2aenvS3150;
  _M0L8_2afieldS3208 = _M0L14_2acasted__envS3151->$2;
  _M0L4nameS1355 = _M0L8_2afieldS3208;
  _M0L8_2afieldS3207 = _M0L14_2acasted__envS3151->$1;
  _M0L14handle__resultS1371 = _M0L8_2afieldS3207;
  _M0L8_2afieldS3206 = _M0L14_2acasted__envS3151->$0;
  _M0L6_2acntS3512 = Moonbit_object_header(_M0L14_2acasted__envS3151)->rc;
  if (_M0L6_2acntS3512 > 1) {
    int32_t _M0L11_2anew__cntS3513 = _M0L6_2acntS3512 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3151)->rc
    = _M0L11_2anew__cntS3513;
    moonbit_incref(_M0L4nameS1355);
    moonbit_incref(_M0L14handle__resultS1371);
    moonbit_incref(_M0L8_2afieldS3206);
  } else if (_M0L6_2acntS3512 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3151);
  }
  _M0L17error__to__stringS1373 = _M0L8_2afieldS3206;
  #line 428 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3152
  = _M0L17error__to__stringS1373->code(_M0L17error__to__stringS1373, _M0L3errS1372);
  #line 428 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1371->code(_M0L14handle__resultS1371, _M0L4nameS1355, _M0L6_2atmpS3152, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam5tools10read__file45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1345,
  struct _M0TWEOc* _M0L6on__okS1346,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1343
) {
  void* _M0L11_2atry__errS1341;
  struct moonbit_result_0 _tmp_3636;
  void* _M0L3errS1342;
  #line 375 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _tmp_3636 = _M0L1fS1345->code(_M0L1fS1345);
  if (_tmp_3636.tag) {
    int32_t const _M0L5_2aokS3137 = _tmp_3636.data.ok;
    moonbit_decref(_M0L7on__errS1343);
  } else {
    void* const _M0L6_2aerrS3138 = _tmp_3636.data.err;
    moonbit_decref(_M0L6on__okS1346);
    _M0L11_2atry__errS1341 = _M0L6_2aerrS3138;
    goto join_1340;
  }
  #line 382 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1346->code(_M0L6on__okS1346);
  goto joinlet_3635;
  join_1340:;
  _M0L3errS1342 = _M0L11_2atry__errS1341;
  #line 383 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1343->code(_M0L7on__errS1343, _M0L3errS1342);
  joinlet_3635:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1300;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1313;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1318;
  struct _M0TUsiE** _M0L6_2atmpS3136;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1325;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1326;
  moonbit_string_t _M0L6_2atmpS3135;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1327;
  int32_t _M0L7_2abindS1328;
  int32_t _M0L2__S1329;
  #line 193 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1300 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1313
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1318 = 0;
  _M0L6_2atmpS3136 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1325
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1325)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1325->$0 = _M0L6_2atmpS3136;
  _M0L16file__and__indexS1325->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1326
  = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1313(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1313);
  #line 284 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3135 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1326, 1);
  #line 283 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1327
  = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1318(_M0L51moonbit__test__driver__internal__split__mbt__stringS1318, _M0L6_2atmpS3135, 47);
  _M0L7_2abindS1328 = _M0L10test__argsS1327->$1;
  _M0L2__S1329 = 0;
  while (1) {
    if (_M0L2__S1329 < _M0L7_2abindS1328) {
      moonbit_string_t* _M0L8_2afieldS3210 = _M0L10test__argsS1327->$0;
      moonbit_string_t* _M0L3bufS3134 = _M0L8_2afieldS3210;
      moonbit_string_t _M0L6_2atmpS3209 =
        (moonbit_string_t)_M0L3bufS3134[_M0L2__S1329];
      moonbit_string_t _M0L3argS1330 = _M0L6_2atmpS3209;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1331;
      moonbit_string_t _M0L4fileS1332;
      moonbit_string_t _M0L5rangeS1333;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1334;
      moonbit_string_t _M0L6_2atmpS3132;
      int32_t _M0L5startS1335;
      moonbit_string_t _M0L6_2atmpS3131;
      int32_t _M0L3endS1336;
      int32_t _M0L1iS1337;
      int32_t _M0L6_2atmpS3133;
      moonbit_incref(_M0L3argS1330);
      #line 288 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1331
      = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1318(_M0L51moonbit__test__driver__internal__split__mbt__stringS1318, _M0L3argS1330, 58);
      moonbit_incref(_M0L16file__and__rangeS1331);
      #line 289 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1332
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1331, 0);
      #line 290 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1333
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1331, 1);
      #line 291 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1334
      = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1318(_M0L51moonbit__test__driver__internal__split__mbt__stringS1318, _M0L5rangeS1333, 45);
      moonbit_incref(_M0L15start__and__endS1334);
      #line 294 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3132
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1334, 0);
      #line 294 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1335
      = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1300(_M0L45moonbit__test__driver__internal__parse__int__S1300, _M0L6_2atmpS3132);
      #line 295 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3131
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1334, 1);
      #line 295 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1336
      = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1300(_M0L45moonbit__test__driver__internal__parse__int__S1300, _M0L6_2atmpS3131);
      _M0L1iS1337 = _M0L5startS1335;
      while (1) {
        if (_M0L1iS1337 < _M0L3endS1336) {
          struct _M0TUsiE* _M0L8_2atupleS3129;
          int32_t _M0L6_2atmpS3130;
          moonbit_incref(_M0L4fileS1332);
          _M0L8_2atupleS3129
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3129)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3129->$0 = _M0L4fileS1332;
          _M0L8_2atupleS3129->$1 = _M0L1iS1337;
          moonbit_incref(_M0L16file__and__indexS1325);
          #line 297 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1325, _M0L8_2atupleS3129);
          _M0L6_2atmpS3130 = _M0L1iS1337 + 1;
          _M0L1iS1337 = _M0L6_2atmpS3130;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1332);
        }
        break;
      }
      _M0L6_2atmpS3133 = _M0L2__S1329 + 1;
      _M0L2__S1329 = _M0L6_2atmpS3133;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1327);
    }
    break;
  }
  return _M0L16file__and__indexS1325;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1318(
  int32_t _M0L6_2aenvS3110,
  moonbit_string_t _M0L1sS1319,
  int32_t _M0L3sepS1320
) {
  moonbit_string_t* _M0L6_2atmpS3128;
  struct _M0TPB5ArrayGsE* _M0L3resS1321;
  struct _M0TPC13ref3RefGiE* _M0L1iS1322;
  struct _M0TPC13ref3RefGiE* _M0L5startS1323;
  int32_t _M0L3valS3123;
  int32_t _M0L6_2atmpS3124;
  #line 261 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3128 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1321
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1321)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1321->$0 = _M0L6_2atmpS3128;
  _M0L3resS1321->$1 = 0;
  _M0L1iS1322
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1322)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1322->$0 = 0;
  _M0L5startS1323
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1323)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1323->$0 = 0;
  while (1) {
    int32_t _M0L3valS3111 = _M0L1iS1322->$0;
    int32_t _M0L6_2atmpS3112 = Moonbit_array_length(_M0L1sS1319);
    if (_M0L3valS3111 < _M0L6_2atmpS3112) {
      int32_t _M0L3valS3115 = _M0L1iS1322->$0;
      int32_t _M0L6_2atmpS3114;
      int32_t _M0L6_2atmpS3113;
      int32_t _M0L3valS3122;
      int32_t _M0L6_2atmpS3121;
      if (
        _M0L3valS3115 < 0
        || _M0L3valS3115 >= Moonbit_array_length(_M0L1sS1319)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3114 = _M0L1sS1319[_M0L3valS3115];
      _M0L6_2atmpS3113 = _M0L6_2atmpS3114;
      if (_M0L6_2atmpS3113 == _M0L3sepS1320) {
        int32_t _M0L3valS3117 = _M0L5startS1323->$0;
        int32_t _M0L3valS3118 = _M0L1iS1322->$0;
        moonbit_string_t _M0L6_2atmpS3116;
        int32_t _M0L3valS3120;
        int32_t _M0L6_2atmpS3119;
        moonbit_incref(_M0L1sS1319);
        #line 270 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3116
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1319, _M0L3valS3117, _M0L3valS3118);
        moonbit_incref(_M0L3resS1321);
        #line 270 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1321, _M0L6_2atmpS3116);
        _M0L3valS3120 = _M0L1iS1322->$0;
        _M0L6_2atmpS3119 = _M0L3valS3120 + 1;
        _M0L5startS1323->$0 = _M0L6_2atmpS3119;
      }
      _M0L3valS3122 = _M0L1iS1322->$0;
      _M0L6_2atmpS3121 = _M0L3valS3122 + 1;
      _M0L1iS1322->$0 = _M0L6_2atmpS3121;
      continue;
    } else {
      moonbit_decref(_M0L1iS1322);
    }
    break;
  }
  _M0L3valS3123 = _M0L5startS1323->$0;
  _M0L6_2atmpS3124 = Moonbit_array_length(_M0L1sS1319);
  if (_M0L3valS3123 < _M0L6_2atmpS3124) {
    int32_t _M0L8_2afieldS3211 = _M0L5startS1323->$0;
    int32_t _M0L3valS3126;
    int32_t _M0L6_2atmpS3127;
    moonbit_string_t _M0L6_2atmpS3125;
    moonbit_decref(_M0L5startS1323);
    _M0L3valS3126 = _M0L8_2afieldS3211;
    _M0L6_2atmpS3127 = Moonbit_array_length(_M0L1sS1319);
    #line 276 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3125
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1319, _M0L3valS3126, _M0L6_2atmpS3127);
    moonbit_incref(_M0L3resS1321);
    #line 276 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1321, _M0L6_2atmpS3125);
  } else {
    moonbit_decref(_M0L5startS1323);
    moonbit_decref(_M0L1sS1319);
  }
  return _M0L3resS1321;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1313(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306
) {
  moonbit_bytes_t* _M0L3tmpS1314;
  int32_t _M0L6_2atmpS3109;
  struct _M0TPB5ArrayGsE* _M0L3resS1315;
  int32_t _M0L1iS1316;
  #line 250 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1314
  = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3109 = Moonbit_array_length(_M0L3tmpS1314);
  #line 254 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1315 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3109);
  _M0L1iS1316 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3105 = Moonbit_array_length(_M0L3tmpS1314);
    if (_M0L1iS1316 < _M0L6_2atmpS3105) {
      moonbit_bytes_t _M0L6_2atmpS3212;
      moonbit_bytes_t _M0L6_2atmpS3107;
      moonbit_string_t _M0L6_2atmpS3106;
      int32_t _M0L6_2atmpS3108;
      if (
        _M0L1iS1316 < 0 || _M0L1iS1316 >= Moonbit_array_length(_M0L3tmpS1314)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3212 = (moonbit_bytes_t)_M0L3tmpS1314[_M0L1iS1316];
      _M0L6_2atmpS3107 = _M0L6_2atmpS3212;
      moonbit_incref(_M0L6_2atmpS3107);
      #line 256 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3106
      = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306, _M0L6_2atmpS3107);
      moonbit_incref(_M0L3resS1315);
      #line 256 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1315, _M0L6_2atmpS3106);
      _M0L6_2atmpS3108 = _M0L1iS1316 + 1;
      _M0L1iS1316 = _M0L6_2atmpS3108;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1314);
    }
    break;
  }
  return _M0L3resS1315;
}

moonbit_string_t _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1306(
  int32_t _M0L6_2aenvS3019,
  moonbit_bytes_t _M0L5bytesS1307
) {
  struct _M0TPB13StringBuilder* _M0L3resS1308;
  int32_t _M0L3lenS1309;
  struct _M0TPC13ref3RefGiE* _M0L1iS1310;
  #line 206 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1308 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1309 = Moonbit_array_length(_M0L5bytesS1307);
  _M0L1iS1310
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1310)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1310->$0 = 0;
  while (1) {
    int32_t _M0L3valS3020 = _M0L1iS1310->$0;
    if (_M0L3valS3020 < _M0L3lenS1309) {
      int32_t _M0L3valS3104 = _M0L1iS1310->$0;
      int32_t _M0L6_2atmpS3103;
      int32_t _M0L6_2atmpS3102;
      struct _M0TPC13ref3RefGiE* _M0L1cS1311;
      int32_t _M0L3valS3021;
      if (
        _M0L3valS3104 < 0
        || _M0L3valS3104 >= Moonbit_array_length(_M0L5bytesS1307)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3103 = _M0L5bytesS1307[_M0L3valS3104];
      _M0L6_2atmpS3102 = (int32_t)_M0L6_2atmpS3103;
      _M0L1cS1311
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1311)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1311->$0 = _M0L6_2atmpS3102;
      _M0L3valS3021 = _M0L1cS1311->$0;
      if (_M0L3valS3021 < 128) {
        int32_t _M0L8_2afieldS3213 = _M0L1cS1311->$0;
        int32_t _M0L3valS3023;
        int32_t _M0L6_2atmpS3022;
        int32_t _M0L3valS3025;
        int32_t _M0L6_2atmpS3024;
        moonbit_decref(_M0L1cS1311);
        _M0L3valS3023 = _M0L8_2afieldS3213;
        _M0L6_2atmpS3022 = _M0L3valS3023;
        moonbit_incref(_M0L3resS1308);
        #line 215 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1308, _M0L6_2atmpS3022);
        _M0L3valS3025 = _M0L1iS1310->$0;
        _M0L6_2atmpS3024 = _M0L3valS3025 + 1;
        _M0L1iS1310->$0 = _M0L6_2atmpS3024;
      } else {
        int32_t _M0L3valS3026 = _M0L1cS1311->$0;
        if (_M0L3valS3026 < 224) {
          int32_t _M0L3valS3028 = _M0L1iS1310->$0;
          int32_t _M0L6_2atmpS3027 = _M0L3valS3028 + 1;
          int32_t _M0L3valS3037;
          int32_t _M0L6_2atmpS3036;
          int32_t _M0L6_2atmpS3030;
          int32_t _M0L3valS3035;
          int32_t _M0L6_2atmpS3034;
          int32_t _M0L6_2atmpS3033;
          int32_t _M0L6_2atmpS3032;
          int32_t _M0L6_2atmpS3031;
          int32_t _M0L6_2atmpS3029;
          int32_t _M0L8_2afieldS3214;
          int32_t _M0L3valS3039;
          int32_t _M0L6_2atmpS3038;
          int32_t _M0L3valS3041;
          int32_t _M0L6_2atmpS3040;
          if (_M0L6_2atmpS3027 >= _M0L3lenS1309) {
            moonbit_decref(_M0L1cS1311);
            moonbit_decref(_M0L1iS1310);
            moonbit_decref(_M0L5bytesS1307);
            break;
          }
          _M0L3valS3037 = _M0L1cS1311->$0;
          _M0L6_2atmpS3036 = _M0L3valS3037 & 31;
          _M0L6_2atmpS3030 = _M0L6_2atmpS3036 << 6;
          _M0L3valS3035 = _M0L1iS1310->$0;
          _M0L6_2atmpS3034 = _M0L3valS3035 + 1;
          if (
            _M0L6_2atmpS3034 < 0
            || _M0L6_2atmpS3034 >= Moonbit_array_length(_M0L5bytesS1307)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3033 = _M0L5bytesS1307[_M0L6_2atmpS3034];
          _M0L6_2atmpS3032 = (int32_t)_M0L6_2atmpS3033;
          _M0L6_2atmpS3031 = _M0L6_2atmpS3032 & 63;
          _M0L6_2atmpS3029 = _M0L6_2atmpS3030 | _M0L6_2atmpS3031;
          _M0L1cS1311->$0 = _M0L6_2atmpS3029;
          _M0L8_2afieldS3214 = _M0L1cS1311->$0;
          moonbit_decref(_M0L1cS1311);
          _M0L3valS3039 = _M0L8_2afieldS3214;
          _M0L6_2atmpS3038 = _M0L3valS3039;
          moonbit_incref(_M0L3resS1308);
          #line 222 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1308, _M0L6_2atmpS3038);
          _M0L3valS3041 = _M0L1iS1310->$0;
          _M0L6_2atmpS3040 = _M0L3valS3041 + 2;
          _M0L1iS1310->$0 = _M0L6_2atmpS3040;
        } else {
          int32_t _M0L3valS3042 = _M0L1cS1311->$0;
          if (_M0L3valS3042 < 240) {
            int32_t _M0L3valS3044 = _M0L1iS1310->$0;
            int32_t _M0L6_2atmpS3043 = _M0L3valS3044 + 2;
            int32_t _M0L3valS3060;
            int32_t _M0L6_2atmpS3059;
            int32_t _M0L6_2atmpS3052;
            int32_t _M0L3valS3058;
            int32_t _M0L6_2atmpS3057;
            int32_t _M0L6_2atmpS3056;
            int32_t _M0L6_2atmpS3055;
            int32_t _M0L6_2atmpS3054;
            int32_t _M0L6_2atmpS3053;
            int32_t _M0L6_2atmpS3046;
            int32_t _M0L3valS3051;
            int32_t _M0L6_2atmpS3050;
            int32_t _M0L6_2atmpS3049;
            int32_t _M0L6_2atmpS3048;
            int32_t _M0L6_2atmpS3047;
            int32_t _M0L6_2atmpS3045;
            int32_t _M0L8_2afieldS3215;
            int32_t _M0L3valS3062;
            int32_t _M0L6_2atmpS3061;
            int32_t _M0L3valS3064;
            int32_t _M0L6_2atmpS3063;
            if (_M0L6_2atmpS3043 >= _M0L3lenS1309) {
              moonbit_decref(_M0L1cS1311);
              moonbit_decref(_M0L1iS1310);
              moonbit_decref(_M0L5bytesS1307);
              break;
            }
            _M0L3valS3060 = _M0L1cS1311->$0;
            _M0L6_2atmpS3059 = _M0L3valS3060 & 15;
            _M0L6_2atmpS3052 = _M0L6_2atmpS3059 << 12;
            _M0L3valS3058 = _M0L1iS1310->$0;
            _M0L6_2atmpS3057 = _M0L3valS3058 + 1;
            if (
              _M0L6_2atmpS3057 < 0
              || _M0L6_2atmpS3057 >= Moonbit_array_length(_M0L5bytesS1307)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3056 = _M0L5bytesS1307[_M0L6_2atmpS3057];
            _M0L6_2atmpS3055 = (int32_t)_M0L6_2atmpS3056;
            _M0L6_2atmpS3054 = _M0L6_2atmpS3055 & 63;
            _M0L6_2atmpS3053 = _M0L6_2atmpS3054 << 6;
            _M0L6_2atmpS3046 = _M0L6_2atmpS3052 | _M0L6_2atmpS3053;
            _M0L3valS3051 = _M0L1iS1310->$0;
            _M0L6_2atmpS3050 = _M0L3valS3051 + 2;
            if (
              _M0L6_2atmpS3050 < 0
              || _M0L6_2atmpS3050 >= Moonbit_array_length(_M0L5bytesS1307)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3049 = _M0L5bytesS1307[_M0L6_2atmpS3050];
            _M0L6_2atmpS3048 = (int32_t)_M0L6_2atmpS3049;
            _M0L6_2atmpS3047 = _M0L6_2atmpS3048 & 63;
            _M0L6_2atmpS3045 = _M0L6_2atmpS3046 | _M0L6_2atmpS3047;
            _M0L1cS1311->$0 = _M0L6_2atmpS3045;
            _M0L8_2afieldS3215 = _M0L1cS1311->$0;
            moonbit_decref(_M0L1cS1311);
            _M0L3valS3062 = _M0L8_2afieldS3215;
            _M0L6_2atmpS3061 = _M0L3valS3062;
            moonbit_incref(_M0L3resS1308);
            #line 231 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1308, _M0L6_2atmpS3061);
            _M0L3valS3064 = _M0L1iS1310->$0;
            _M0L6_2atmpS3063 = _M0L3valS3064 + 3;
            _M0L1iS1310->$0 = _M0L6_2atmpS3063;
          } else {
            int32_t _M0L3valS3066 = _M0L1iS1310->$0;
            int32_t _M0L6_2atmpS3065 = _M0L3valS3066 + 3;
            int32_t _M0L3valS3089;
            int32_t _M0L6_2atmpS3088;
            int32_t _M0L6_2atmpS3081;
            int32_t _M0L3valS3087;
            int32_t _M0L6_2atmpS3086;
            int32_t _M0L6_2atmpS3085;
            int32_t _M0L6_2atmpS3084;
            int32_t _M0L6_2atmpS3083;
            int32_t _M0L6_2atmpS3082;
            int32_t _M0L6_2atmpS3074;
            int32_t _M0L3valS3080;
            int32_t _M0L6_2atmpS3079;
            int32_t _M0L6_2atmpS3078;
            int32_t _M0L6_2atmpS3077;
            int32_t _M0L6_2atmpS3076;
            int32_t _M0L6_2atmpS3075;
            int32_t _M0L6_2atmpS3068;
            int32_t _M0L3valS3073;
            int32_t _M0L6_2atmpS3072;
            int32_t _M0L6_2atmpS3071;
            int32_t _M0L6_2atmpS3070;
            int32_t _M0L6_2atmpS3069;
            int32_t _M0L6_2atmpS3067;
            int32_t _M0L3valS3091;
            int32_t _M0L6_2atmpS3090;
            int32_t _M0L3valS3095;
            int32_t _M0L6_2atmpS3094;
            int32_t _M0L6_2atmpS3093;
            int32_t _M0L6_2atmpS3092;
            int32_t _M0L8_2afieldS3216;
            int32_t _M0L3valS3099;
            int32_t _M0L6_2atmpS3098;
            int32_t _M0L6_2atmpS3097;
            int32_t _M0L6_2atmpS3096;
            int32_t _M0L3valS3101;
            int32_t _M0L6_2atmpS3100;
            if (_M0L6_2atmpS3065 >= _M0L3lenS1309) {
              moonbit_decref(_M0L1cS1311);
              moonbit_decref(_M0L1iS1310);
              moonbit_decref(_M0L5bytesS1307);
              break;
            }
            _M0L3valS3089 = _M0L1cS1311->$0;
            _M0L6_2atmpS3088 = _M0L3valS3089 & 7;
            _M0L6_2atmpS3081 = _M0L6_2atmpS3088 << 18;
            _M0L3valS3087 = _M0L1iS1310->$0;
            _M0L6_2atmpS3086 = _M0L3valS3087 + 1;
            if (
              _M0L6_2atmpS3086 < 0
              || _M0L6_2atmpS3086 >= Moonbit_array_length(_M0L5bytesS1307)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3085 = _M0L5bytesS1307[_M0L6_2atmpS3086];
            _M0L6_2atmpS3084 = (int32_t)_M0L6_2atmpS3085;
            _M0L6_2atmpS3083 = _M0L6_2atmpS3084 & 63;
            _M0L6_2atmpS3082 = _M0L6_2atmpS3083 << 12;
            _M0L6_2atmpS3074 = _M0L6_2atmpS3081 | _M0L6_2atmpS3082;
            _M0L3valS3080 = _M0L1iS1310->$0;
            _M0L6_2atmpS3079 = _M0L3valS3080 + 2;
            if (
              _M0L6_2atmpS3079 < 0
              || _M0L6_2atmpS3079 >= Moonbit_array_length(_M0L5bytesS1307)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3078 = _M0L5bytesS1307[_M0L6_2atmpS3079];
            _M0L6_2atmpS3077 = (int32_t)_M0L6_2atmpS3078;
            _M0L6_2atmpS3076 = _M0L6_2atmpS3077 & 63;
            _M0L6_2atmpS3075 = _M0L6_2atmpS3076 << 6;
            _M0L6_2atmpS3068 = _M0L6_2atmpS3074 | _M0L6_2atmpS3075;
            _M0L3valS3073 = _M0L1iS1310->$0;
            _M0L6_2atmpS3072 = _M0L3valS3073 + 3;
            if (
              _M0L6_2atmpS3072 < 0
              || _M0L6_2atmpS3072 >= Moonbit_array_length(_M0L5bytesS1307)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3071 = _M0L5bytesS1307[_M0L6_2atmpS3072];
            _M0L6_2atmpS3070 = (int32_t)_M0L6_2atmpS3071;
            _M0L6_2atmpS3069 = _M0L6_2atmpS3070 & 63;
            _M0L6_2atmpS3067 = _M0L6_2atmpS3068 | _M0L6_2atmpS3069;
            _M0L1cS1311->$0 = _M0L6_2atmpS3067;
            _M0L3valS3091 = _M0L1cS1311->$0;
            _M0L6_2atmpS3090 = _M0L3valS3091 - 65536;
            _M0L1cS1311->$0 = _M0L6_2atmpS3090;
            _M0L3valS3095 = _M0L1cS1311->$0;
            _M0L6_2atmpS3094 = _M0L3valS3095 >> 10;
            _M0L6_2atmpS3093 = _M0L6_2atmpS3094 + 55296;
            _M0L6_2atmpS3092 = _M0L6_2atmpS3093;
            moonbit_incref(_M0L3resS1308);
            #line 242 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1308, _M0L6_2atmpS3092);
            _M0L8_2afieldS3216 = _M0L1cS1311->$0;
            moonbit_decref(_M0L1cS1311);
            _M0L3valS3099 = _M0L8_2afieldS3216;
            _M0L6_2atmpS3098 = _M0L3valS3099 & 1023;
            _M0L6_2atmpS3097 = _M0L6_2atmpS3098 + 56320;
            _M0L6_2atmpS3096 = _M0L6_2atmpS3097;
            moonbit_incref(_M0L3resS1308);
            #line 243 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1308, _M0L6_2atmpS3096);
            _M0L3valS3101 = _M0L1iS1310->$0;
            _M0L6_2atmpS3100 = _M0L3valS3101 + 4;
            _M0L1iS1310->$0 = _M0L6_2atmpS3100;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1310);
      moonbit_decref(_M0L5bytesS1307);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1308);
}

int32_t _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1300(
  int32_t _M0L6_2aenvS3012,
  moonbit_string_t _M0L1sS1301
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1302;
  int32_t _M0L3lenS1303;
  int32_t _M0L1iS1304;
  int32_t _M0L8_2afieldS3217;
  #line 197 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1302
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1302)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1302->$0 = 0;
  _M0L3lenS1303 = Moonbit_array_length(_M0L1sS1301);
  _M0L1iS1304 = 0;
  while (1) {
    if (_M0L1iS1304 < _M0L3lenS1303) {
      int32_t _M0L3valS3017 = _M0L3resS1302->$0;
      int32_t _M0L6_2atmpS3014 = _M0L3valS3017 * 10;
      int32_t _M0L6_2atmpS3016;
      int32_t _M0L6_2atmpS3015;
      int32_t _M0L6_2atmpS3013;
      int32_t _M0L6_2atmpS3018;
      if (
        _M0L1iS1304 < 0 || _M0L1iS1304 >= Moonbit_array_length(_M0L1sS1301)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3016 = _M0L1sS1301[_M0L1iS1304];
      _M0L6_2atmpS3015 = _M0L6_2atmpS3016 - 48;
      _M0L6_2atmpS3013 = _M0L6_2atmpS3014 + _M0L6_2atmpS3015;
      _M0L3resS1302->$0 = _M0L6_2atmpS3013;
      _M0L6_2atmpS3018 = _M0L1iS1304 + 1;
      _M0L1iS1304 = _M0L6_2atmpS3018;
      continue;
    } else {
      moonbit_decref(_M0L1sS1301);
    }
    break;
  }
  _M0L8_2afieldS3217 = _M0L3resS1302->$0;
  moonbit_decref(_M0L3resS1302);
  return _M0L8_2afieldS3217;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1280,
  moonbit_string_t _M0L12_2adiscard__S1281,
  int32_t _M0L12_2adiscard__S1282,
  struct _M0TWssbEu* _M0L12_2adiscard__S1283,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1284
) {
  struct moonbit_result_0 _result_3643;
  #line 34 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1284);
  moonbit_decref(_M0L12_2adiscard__S1283);
  moonbit_decref(_M0L12_2adiscard__S1281);
  moonbit_decref(_M0L12_2adiscard__S1280);
  _result_3643.tag = 1;
  _result_3643.data.ok = 0;
  return _result_3643;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1285,
  moonbit_string_t _M0L12_2adiscard__S1286,
  int32_t _M0L12_2adiscard__S1287,
  struct _M0TWssbEu* _M0L12_2adiscard__S1288,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1289
) {
  struct moonbit_result_0 _result_3644;
  #line 34 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1289);
  moonbit_decref(_M0L12_2adiscard__S1288);
  moonbit_decref(_M0L12_2adiscard__S1286);
  moonbit_decref(_M0L12_2adiscard__S1285);
  _result_3644.tag = 1;
  _result_3644.data.ok = 0;
  return _result_3644;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1290,
  moonbit_string_t _M0L12_2adiscard__S1291,
  int32_t _M0L12_2adiscard__S1292,
  struct _M0TWssbEu* _M0L12_2adiscard__S1293,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1294
) {
  struct moonbit_result_0 _result_3645;
  #line 34 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1294);
  moonbit_decref(_M0L12_2adiscard__S1293);
  moonbit_decref(_M0L12_2adiscard__S1291);
  moonbit_decref(_M0L12_2adiscard__S1290);
  _result_3645.tag = 1;
  _result_3645.data.ok = 0;
  return _result_3645;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5tools10read__file21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5tools10read__file50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1295,
  moonbit_string_t _M0L12_2adiscard__S1296,
  int32_t _M0L12_2adiscard__S1297,
  struct _M0TWssbEu* _M0L12_2adiscard__S1298,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1299
) {
  struct moonbit_result_0 _result_3646;
  #line 34 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1299);
  moonbit_decref(_M0L12_2adiscard__S1298);
  moonbit_decref(_M0L12_2adiscard__S1296);
  moonbit_decref(_M0L12_2adiscard__S1295);
  _result_3646.tag = 1;
  _result_3646.data.ok = 0;
  return _result_3646;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam5tools10read__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools10read__file34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1279
) {
  #line 12 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1279);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5tools10read__file39____test__726561645f66696c652e6d6274__0(
  
) {
  void* _M0L6_2atmpS3011;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3004;
  moonbit_string_t _M0L6_2atmpS3010;
  void* _M0L6_2atmpS3009;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3005;
  moonbit_string_t _M0L6_2atmpS3008;
  void* _M0L6_2atmpS3007;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3006;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1265;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3003;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3002;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3001;
  void* _M0L5json1S1264;
  void* _M0L6_2atmpS2998;
  struct moonbit_result_1 _tmp_3647;
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L4dataS1266;
  struct _M0TPB6ToJson _M0L6_2atmpS2893;
  void* _M0L6_2atmpS2912;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2905;
  moonbit_string_t _M0L6_2atmpS2911;
  void* _M0L6_2atmpS2910;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2906;
  moonbit_string_t _M0L6_2atmpS2909;
  void* _M0L6_2atmpS2908;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2907;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1267;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2904;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2903;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2902;
  void* _M0L6_2atmpS2901;
  void* _M0L6_2atmpS2894;
  moonbit_string_t _M0L6_2atmpS2897;
  moonbit_string_t _M0L6_2atmpS2898;
  moonbit_string_t _M0L6_2atmpS2899;
  moonbit_string_t _M0L6_2atmpS2900;
  moonbit_string_t* _M0L6_2atmpS2896;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2895;
  struct moonbit_result_0 _tmp_3649;
  void* _M0L6_2atmpS2997;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2996;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1269;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2995;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2994;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2993;
  void* _M0L5json2S1268;
  void* _M0L6_2atmpS2990;
  struct moonbit_result_1 _tmp_3651;
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L5data2S1270;
  struct _M0TPB6ToJson _M0L6_2atmpS2915;
  void* _M0L6_2atmpS2928;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2927;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1271;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2926;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2925;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2924;
  void* _M0L6_2atmpS2923;
  void* _M0L6_2atmpS2916;
  moonbit_string_t _M0L6_2atmpS2919;
  moonbit_string_t _M0L6_2atmpS2920;
  moonbit_string_t _M0L6_2atmpS2921;
  moonbit_string_t _M0L6_2atmpS2922;
  moonbit_string_t* _M0L6_2atmpS2918;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2917;
  struct moonbit_result_0 _tmp_3653;
  void* _M0L6_2atmpS2989;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2985;
  moonbit_string_t _M0L6_2atmpS2988;
  void* _M0L6_2atmpS2987;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2986;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1273;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2984;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2983;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2982;
  void* _M0L5json3S1272;
  void* _M0L6_2atmpS2979;
  struct moonbit_result_1 _tmp_3655;
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L5data3S1274;
  struct _M0TPB6ToJson _M0L6_2atmpS2931;
  void* _M0L6_2atmpS2947;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2943;
  moonbit_string_t _M0L6_2atmpS2946;
  void* _M0L6_2atmpS2945;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2944;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1275;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2942;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2941;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2940;
  void* _M0L6_2atmpS2939;
  void* _M0L6_2atmpS2932;
  moonbit_string_t _M0L6_2atmpS2935;
  moonbit_string_t _M0L6_2atmpS2936;
  moonbit_string_t _M0L6_2atmpS2937;
  moonbit_string_t _M0L6_2atmpS2938;
  moonbit_string_t* _M0L6_2atmpS2934;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2933;
  struct moonbit_result_0 _tmp_3657;
  void* _M0L6_2atmpS2978;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2974;
  moonbit_string_t _M0L6_2atmpS2977;
  void* _M0L6_2atmpS2976;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2975;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1277;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2973;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2972;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2971;
  void* _M0L5json4S1276;
  void* _M0L6_2atmpS2968;
  struct moonbit_result_1 _tmp_3659;
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L6_2atmpS2967;
  struct _M0TPB6ToJson _M0L6_2atmpS2950;
  void* _M0L6_2atmpS2966;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2962;
  moonbit_string_t _M0L6_2atmpS2965;
  void* _M0L6_2atmpS2964;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS2963;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1278;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2961;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2960;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS2959;
  void* _M0L6_2atmpS2958;
  void* _M0L6_2atmpS2951;
  moonbit_string_t _M0L6_2atmpS2954;
  moonbit_string_t _M0L6_2atmpS2955;
  moonbit_string_t _M0L6_2atmpS2956;
  moonbit_string_t _M0L6_2atmpS2957;
  moonbit_string_t* _M0L6_2atmpS2953;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2952;
  #line 53 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  #line 54 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS3011
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3004
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3004)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3004->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS3004->$1 = _M0L6_2atmpS3011;
  _M0L6_2atmpS3010 = 0;
  #line 54 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS3009 = _M0MPC14json4Json6number(0x1.4p+2, _M0L6_2atmpS3010);
  _M0L8_2atupleS3005
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3005)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3005->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS3005->$1 = _M0L6_2atmpS3009;
  _M0L6_2atmpS3008 = 0;
  #line 54 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS3007 = _M0MPC14json4Json6number(0x1.4p+3, _M0L6_2atmpS3008);
  _M0L8_2atupleS3006
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3006)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3006->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS3006->$1 = _M0L6_2atmpS3007;
  _M0L7_2abindS1265 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1265[0] = _M0L8_2atupleS3004;
  _M0L7_2abindS1265[1] = _M0L8_2atupleS3005;
  _M0L7_2abindS1265[2] = _M0L8_2atupleS3006;
  _M0L6_2atmpS3003 = _M0L7_2abindS1265;
  _M0L6_2atmpS3002
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3003
  };
  #line 54 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS3001 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3002);
  #line 54 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L5json1S1264 = _M0MPC14json4Json6object(_M0L6_2atmpS3001);
  _M0L6_2atmpS2998 = 0;
  #line 57 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3647
  = _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(_M0L5json1S1264, _M0L6_2atmpS2998);
  if (_tmp_3647.tag) {
    struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* const _M0L5_2aokS2999 =
      _tmp_3647.data.ok;
    _M0L4dataS1266 = _M0L5_2aokS2999;
  } else {
    void* const _M0L6_2aerrS3000 = _tmp_3647.data.err;
    struct moonbit_result_0 _result_3648;
    _result_3648.tag = 0;
    _result_3648.data.err = _M0L6_2aerrS3000;
    return _result_3648;
  }
  _M0L6_2atmpS2893
  = (struct _M0TPB6ToJson){
    _M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L4dataS1266
  };
  #line 59 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2912
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2905
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2905)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2905->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2905->$1 = _M0L6_2atmpS2912;
  _M0L6_2atmpS2911 = 0;
  #line 60 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2910 = _M0MPC14json4Json6number(0x1.4p+2, _M0L6_2atmpS2911);
  _M0L8_2atupleS2906
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2906)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2906->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS2906->$1 = _M0L6_2atmpS2910;
  _M0L6_2atmpS2909 = 0;
  #line 61 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2908 = _M0MPC14json4Json6number(0x1.4p+3, _M0L6_2atmpS2909);
  _M0L8_2atupleS2907
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2907)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2907->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2907->$1 = _M0L6_2atmpS2908;
  _M0L7_2abindS1267 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1267[0] = _M0L8_2atupleS2905;
  _M0L7_2abindS1267[1] = _M0L8_2atupleS2906;
  _M0L7_2abindS1267[2] = _M0L8_2atupleS2907;
  _M0L6_2atmpS2904 = _M0L7_2abindS1267;
  _M0L6_2atmpS2903
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS2904
  };
  #line 58 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2902 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2903);
  #line 58 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2901 = _M0MPC14json4Json6object(_M0L6_2atmpS2902);
  _M0L6_2atmpS2894 = _M0L6_2atmpS2901;
  _M0L6_2atmpS2897 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS2898 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS2899 = 0;
  _M0L6_2atmpS2900 = 0;
  _M0L6_2atmpS2896 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2896[0] = _M0L6_2atmpS2897;
  _M0L6_2atmpS2896[1] = _M0L6_2atmpS2898;
  _M0L6_2atmpS2896[2] = _M0L6_2atmpS2899;
  _M0L6_2atmpS2896[3] = _M0L6_2atmpS2900;
  _M0L6_2atmpS2895
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2895)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2895->$0 = _M0L6_2atmpS2896;
  _M0L6_2atmpS2895->$1 = 4;
  #line 58 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3649
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2893, _M0L6_2atmpS2894, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS2895);
  if (_tmp_3649.tag) {
    int32_t const _M0L5_2aokS2913 = _tmp_3649.data.ok;
  } else {
    void* const _M0L6_2aerrS2914 = _tmp_3649.data.err;
    struct moonbit_result_0 _result_3650;
    _result_3650.tag = 0;
    _result_3650.data.err = _M0L6_2aerrS2914;
    return _result_3650;
  }
  #line 63 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2997
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2996
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2996)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2996->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2996->$1 = _M0L6_2atmpS2997;
  _M0L7_2abindS1269 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1269[0] = _M0L8_2atupleS2996;
  _M0L6_2atmpS2995 = _M0L7_2abindS1269;
  _M0L6_2atmpS2994
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS2995
  };
  #line 63 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2993 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2994);
  #line 63 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L5json2S1268 = _M0MPC14json4Json6object(_M0L6_2atmpS2993);
  _M0L6_2atmpS2990 = 0;
  #line 64 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3651
  = _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(_M0L5json2S1268, _M0L6_2atmpS2990);
  if (_tmp_3651.tag) {
    struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* const _M0L5_2aokS2991 =
      _tmp_3651.data.ok;
    _M0L5data2S1270 = _M0L5_2aokS2991;
  } else {
    void* const _M0L6_2aerrS2992 = _tmp_3651.data.err;
    struct moonbit_result_0 _result_3652;
    _result_3652.tag = 0;
    _result_3652.data.err = _M0L6_2aerrS2992;
    return _result_3652;
  }
  _M0L6_2atmpS2915
  = (struct _M0TPB6ToJson){
    _M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L5data2S1270
  };
  #line 65 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2928
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2927
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2927)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2927->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2927->$1 = _M0L6_2atmpS2928;
  _M0L7_2abindS1271 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1271[0] = _M0L8_2atupleS2927;
  _M0L6_2atmpS2926 = _M0L7_2abindS1271;
  _M0L6_2atmpS2925
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS2926
  };
  #line 65 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2924 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2925);
  #line 65 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2923 = _M0MPC14json4Json6object(_M0L6_2atmpS2924);
  _M0L6_2atmpS2916 = _M0L6_2atmpS2923;
  _M0L6_2atmpS2919 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS2920 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS2921 = 0;
  _M0L6_2atmpS2922 = 0;
  _M0L6_2atmpS2918 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2918[0] = _M0L6_2atmpS2919;
  _M0L6_2atmpS2918[1] = _M0L6_2atmpS2920;
  _M0L6_2atmpS2918[2] = _M0L6_2atmpS2921;
  _M0L6_2atmpS2918[3] = _M0L6_2atmpS2922;
  _M0L6_2atmpS2917
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2917)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2917->$0 = _M0L6_2atmpS2918;
  _M0L6_2atmpS2917->$1 = 4;
  #line 65 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3653
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2915, _M0L6_2atmpS2916, (moonbit_string_t)moonbit_string_literal_19.data, _M0L6_2atmpS2917);
  if (_tmp_3653.tag) {
    int32_t const _M0L5_2aokS2929 = _tmp_3653.data.ok;
  } else {
    void* const _M0L6_2aerrS2930 = _tmp_3653.data.err;
    struct moonbit_result_0 _result_3654;
    _result_3654.tag = 0;
    _result_3654.data.err = _M0L6_2aerrS2930;
    return _result_3654;
  }
  #line 66 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2989
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2985
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2985)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2985->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2985->$1 = _M0L6_2atmpS2989;
  _M0L6_2atmpS2988 = 0;
  #line 66 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2987 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2988);
  _M0L8_2atupleS2986
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2986)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2986->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS2986->$1 = _M0L6_2atmpS2987;
  _M0L7_2abindS1273 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1273[0] = _M0L8_2atupleS2985;
  _M0L7_2abindS1273[1] = _M0L8_2atupleS2986;
  _M0L6_2atmpS2984 = _M0L7_2abindS1273;
  _M0L6_2atmpS2983
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2984
  };
  #line 66 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2982 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2983);
  #line 66 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L5json3S1272 = _M0MPC14json4Json6object(_M0L6_2atmpS2982);
  _M0L6_2atmpS2979 = 0;
  #line 67 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3655
  = _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(_M0L5json3S1272, _M0L6_2atmpS2979);
  if (_tmp_3655.tag) {
    struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* const _M0L5_2aokS2980 =
      _tmp_3655.data.ok;
    _M0L5data3S1274 = _M0L5_2aokS2980;
  } else {
    void* const _M0L6_2aerrS2981 = _tmp_3655.data.err;
    struct moonbit_result_0 _result_3656;
    _result_3656.tag = 0;
    _result_3656.data.err = _M0L6_2aerrS2981;
    return _result_3656;
  }
  _M0L6_2atmpS2931
  = (struct _M0TPB6ToJson){
    _M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L5data3S1274
  };
  #line 68 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2947
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS2943
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2943)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2943->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2943->$1 = _M0L6_2atmpS2947;
  _M0L6_2atmpS2946 = 0;
  #line 68 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2945 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2946);
  _M0L8_2atupleS2944
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2944)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2944->$0 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L8_2atupleS2944->$1 = _M0L6_2atmpS2945;
  _M0L7_2abindS1275 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1275[0] = _M0L8_2atupleS2943;
  _M0L7_2abindS1275[1] = _M0L8_2atupleS2944;
  _M0L6_2atmpS2942 = _M0L7_2abindS1275;
  _M0L6_2atmpS2941
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2942
  };
  #line 68 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2940 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2941);
  #line 68 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2939 = _M0MPC14json4Json6object(_M0L6_2atmpS2940);
  _M0L6_2atmpS2932 = _M0L6_2atmpS2939;
  _M0L6_2atmpS2935 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS2936 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS2937 = 0;
  _M0L6_2atmpS2938 = 0;
  _M0L6_2atmpS2934 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2934[0] = _M0L6_2atmpS2935;
  _M0L6_2atmpS2934[1] = _M0L6_2atmpS2936;
  _M0L6_2atmpS2934[2] = _M0L6_2atmpS2937;
  _M0L6_2atmpS2934[3] = _M0L6_2atmpS2938;
  _M0L6_2atmpS2933
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2933)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2933->$0 = _M0L6_2atmpS2934;
  _M0L6_2atmpS2933->$1 = 4;
  #line 68 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3657
  = _M0FPC14json13json__inspect(_M0L6_2atmpS2931, _M0L6_2atmpS2932, (moonbit_string_t)moonbit_string_literal_22.data, _M0L6_2atmpS2933);
  if (_tmp_3657.tag) {
    int32_t const _M0L5_2aokS2948 = _tmp_3657.data.ok;
  } else {
    void* const _M0L6_2aerrS2949 = _tmp_3657.data.err;
    struct moonbit_result_0 _result_3658;
    _result_3658.tag = 0;
    _result_3658.data.err = _M0L6_2aerrS2949;
    return _result_3658;
  }
  #line 69 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2978
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS2974
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2974)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2974->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2974->$1 = _M0L6_2atmpS2978;
  _M0L6_2atmpS2977 = 0;
  #line 69 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2976 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2977);
  _M0L8_2atupleS2975
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2975)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2975->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2975->$1 = _M0L6_2atmpS2976;
  _M0L7_2abindS1277 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1277[0] = _M0L8_2atupleS2974;
  _M0L7_2abindS1277[1] = _M0L8_2atupleS2975;
  _M0L6_2atmpS2973 = _M0L7_2abindS1277;
  _M0L6_2atmpS2972
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2973
  };
  #line 69 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2971 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2972);
  #line 69 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L5json4S1276 = _M0MPC14json4Json6object(_M0L6_2atmpS2971);
  _M0L6_2atmpS2968 = 0;
  #line 70 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _tmp_3659
  = _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(_M0L5json4S1276, _M0L6_2atmpS2968);
  if (_tmp_3659.tag) {
    struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* const _M0L5_2aokS2969 =
      _tmp_3659.data.ok;
    _M0L6_2atmpS2967 = _M0L5_2aokS2969;
  } else {
    void* const _M0L6_2aerrS2970 = _tmp_3659.data.err;
    struct moonbit_result_0 _result_3660;
    _result_3660.tag = 0;
    _result_3660.data.err = _M0L6_2aerrS2970;
    return _result_3660;
  }
  _M0L6_2atmpS2950
  = (struct _M0TPB6ToJson){
    _M0FP0131clawteam_2fclawteam_2ftools_2fread__file_2fReadFileInput_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS2967
  };
  #line 71 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2966
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_23.data);
  _M0L8_2atupleS2962
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2962)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2962->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L8_2atupleS2962->$1 = _M0L6_2atmpS2966;
  _M0L6_2atmpS2965 = 0;
  #line 72 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2964 = _M0MPC14json4Json6number(0x1p+1, _M0L6_2atmpS2965);
  _M0L8_2atupleS2963
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS2963)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2963->$0 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L8_2atupleS2963->$1 = _M0L6_2atmpS2964;
  _M0L7_2abindS1278 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1278[0] = _M0L8_2atupleS2962;
  _M0L7_2abindS1278[1] = _M0L8_2atupleS2963;
  _M0L6_2atmpS2961 = _M0L7_2abindS1278;
  _M0L6_2atmpS2960
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS2961
  };
  #line 70 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2959 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2960);
  #line 70 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  _M0L6_2atmpS2958 = _M0MPC14json4Json6object(_M0L6_2atmpS2959);
  _M0L6_2atmpS2951 = _M0L6_2atmpS2958;
  _M0L6_2atmpS2954 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS2955 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS2956 = 0;
  _M0L6_2atmpS2957 = 0;
  _M0L6_2atmpS2953 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2953[0] = _M0L6_2atmpS2954;
  _M0L6_2atmpS2953[1] = _M0L6_2atmpS2955;
  _M0L6_2atmpS2953[2] = _M0L6_2atmpS2956;
  _M0L6_2atmpS2953[3] = _M0L6_2atmpS2957;
  _M0L6_2atmpS2952
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2952)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2952->$0 = _M0L6_2atmpS2953;
  _M0L6_2atmpS2952->$1 = 4;
  #line 70 "E:\\moonbit\\clawteam\\tools\\read_file\\read_file.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS2950, _M0L6_2atmpS2951, (moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS2952);
}

void* _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L9_2ax__149S1253
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1252;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS2892;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS2891;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1251;
  moonbit_string_t _M0L8_2afieldS3219;
  moonbit_string_t _M0L4pathS2888;
  void* _M0L6_2atmpS2887;
  int32_t _M0L8_24innerS1255;
  int64_t _M0L7_2abindS1256;
  void* _M0L6_2atmpS2889;
  int32_t _M0L8_24innerS1260;
  int64_t _M0L8_2afieldS3218;
  int64_t _M0L7_2abindS1261;
  void* _M0L6_2atmpS2890;
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L7_2abindS1252 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2892 = _M0L7_2abindS1252;
  _M0L6_2atmpS2891
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS2892
  };
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_24mapS1251 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS2891);
  _M0L8_2afieldS3219 = _M0L9_2ax__149S1253->$0;
  _M0L4pathS2888 = _M0L8_2afieldS3219;
  moonbit_incref(_M0L4pathS2888);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_2atmpS2887 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4pathS2888);
  moonbit_incref(_M0L6_24mapS1251);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1251, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS2887);
  _M0L7_2abindS1256 = _M0L9_2ax__149S1253->$1;
  if (_M0L7_2abindS1256 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1257 = _M0L7_2abindS1256;
    int32_t _M0L11_2a_24innerS1258 = (int32_t)_M0L7_2aSomeS1257;
    _M0L8_24innerS1255 = _M0L11_2a_24innerS1258;
    goto join_1254;
  }
  goto joinlet_3661;
  join_1254:;
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_2atmpS2889 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1255);
  moonbit_incref(_M0L6_24mapS1251);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1251, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS2889);
  joinlet_3661:;
  _M0L8_2afieldS3218 = _M0L9_2ax__149S1253->$2;
  moonbit_decref(_M0L9_2ax__149S1253);
  _M0L7_2abindS1261 = _M0L8_2afieldS3218;
  if (_M0L7_2abindS1261 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1262 = _M0L7_2abindS1261;
    int32_t _M0L11_2a_24innerS1263 = (int32_t)_M0L7_2aSomeS1262;
    _M0L8_24innerS1260 = _M0L11_2a_24innerS1263;
    goto join_1259;
  }
  goto joinlet_3662;
  join_1259:;
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_2atmpS2890 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1260);
  moonbit_incref(_M0L6_24mapS1251);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1251, (moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2890);
  joinlet_3662:;
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1251);
}

struct moonbit_result_1 _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPC14json8FromJson10from__json(
  void* _M0L9_2ax__154S1230,
  void* _M0L9_2ax__155S1216
) {
  void* _M0L4NoneS2886;
  struct _M0TPC13ref3RefGOOiE* _M0L23_2ade__start__line__158S1209;
  moonbit_string_t _M0L6_2atmpS2885;
  struct _M0TPC13ref3RefGOsE* _M0L16_2ade__path__157S1210;
  void* _M0L4NoneS2884;
  struct _M0TPC13ref3RefGOOiE* _M0L21_2ade__end__line__156S1211;
  struct _M0TPB3MapGsRPB4JsonE* _M0L5__mapS1213;
  void* _M0L3__vS1215;
  void* _M0L7_2abindS1217;
  void* _M0L6_2atmpS2864;
  struct moonbit_result_2 _tmp_3666;
  moonbit_string_t _M0L6_2atmpS2863;
  moonbit_string_t _M0L6_2atmpS2862;
  moonbit_string_t _M0L6_2aoldS3227;
  void* _M0L3__vS1221;
  void* _M0L7_2abindS1222;
  void* _M0L6_2atmpS2870;
  struct moonbit_result_0 _tmp_3669;
  int32_t _M0L6_2atmpS2869;
  int64_t _M0L6_2atmpS2868;
  void* _M0L4SomeS2867;
  void* _M0L6_2aoldS3226;
  void* _M0L3__vS1226;
  void* _M0L7_2abindS1227;
  void* _M0L6_2atmpS2876;
  struct moonbit_result_0 _tmp_3672;
  int32_t _M0L6_2atmpS2875;
  int64_t _M0L6_2atmpS2874;
  void* _M0L4SomeS2873;
  void* _M0L6_2aoldS3225;
  int64_t _M0L1vS1235;
  int64_t _M0L23_2ade__start__line__158S1233;
  void* _M0L8_2afieldS3224;
  int32_t _M0L6_2acntS3516;
  void* _M0L7_2abindS1236;
  moonbit_string_t _M0L1vS1241;
  moonbit_string_t _M0L16_2ade__path__157S1239;
  moonbit_string_t _M0L8_2afieldS3222;
  int32_t _M0L6_2acntS3518;
  moonbit_string_t _M0L7_2abindS1242;
  int64_t _M0L1vS1247;
  int64_t _M0L21_2ade__end__line__156S1245;
  void* _M0L8_2afieldS3221;
  int32_t _M0L6_2acntS3520;
  void* _M0L7_2abindS1248;
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L6_2atmpS2881;
  struct moonbit_result_1 _result_3678;
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L4NoneS2886
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L23_2ade__start__line__158S1209
  = (struct _M0TPC13ref3RefGOOiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOOiE));
  Moonbit_object_header(_M0L23_2ade__start__line__158S1209)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOOiE, $0) >> 2, 1, 0);
  _M0L23_2ade__start__line__158S1209->$0 = _M0L4NoneS2886;
  _M0L6_2atmpS2885 = 0;
  _M0L16_2ade__path__157S1210
  = (struct _M0TPC13ref3RefGOsE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOsE));
  Moonbit_object_header(_M0L16_2ade__path__157S1210)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOsE, $0) >> 2, 1, 0);
  _M0L16_2ade__path__157S1210->$0 = _M0L6_2atmpS2885;
  _M0L4NoneS2884
  = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  _M0L21_2ade__end__line__156S1211
  = (struct _M0TPC13ref3RefGOOiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGOOiE));
  Moonbit_object_header(_M0L21_2ade__end__line__156S1211)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGOOiE, $0) >> 2, 1, 0);
  _M0L21_2ade__end__line__156S1211->$0 = _M0L4NoneS2884;
  switch (Moonbit_object_tag(_M0L9_2ax__154S1230)) {
    case 6: {
      struct _M0DTPB4Json6Object* _M0L9_2aObjectS1231 =
        (struct _M0DTPB4Json6Object*)_M0L9_2ax__154S1230;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3228 =
        _M0L9_2aObjectS1231->$0;
      int32_t _M0L6_2acntS3514 =
        Moonbit_object_header(_M0L9_2aObjectS1231)->rc;
      struct _M0TPB3MapGsRPB4JsonE* _M0L8_2a__mapS1232;
      if (_M0L6_2acntS3514 > 1) {
        int32_t _M0L11_2anew__cntS3515 = _M0L6_2acntS3514 - 1;
        Moonbit_object_header(_M0L9_2aObjectS1231)->rc
        = _M0L11_2anew__cntS3515;
        moonbit_incref(_M0L8_2afieldS3228);
      } else if (_M0L6_2acntS3514 == 1) {
        #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
        moonbit_free(_M0L9_2aObjectS1231);
      }
      _M0L8_2a__mapS1232 = _M0L8_2afieldS3228;
      _M0L5__mapS1213 = _M0L8_2a__mapS1232;
      goto join_1212;
      break;
    }
    default: {
      struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2880;
      void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879;
      struct moonbit_result_1 _result_3664;
      moonbit_decref(_M0L9_2ax__154S1230);
      moonbit_decref(_M0L21_2ade__end__line__156S1211);
      moonbit_decref(_M0L16_2ade__path__157S1210);
      moonbit_decref(_M0L23_2ade__start__line__158S1209);
      _M0L8_2atupleS2880
      = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
      Moonbit_object_header(_M0L8_2atupleS2880)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
      _M0L8_2atupleS2880->$0 = _M0L9_2ax__155S1216;
      _M0L8_2atupleS2880->$1
      = (moonbit_string_t)moonbit_string_literal_27.data;
      _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
      Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
      ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879)->$0
      = _M0L8_2atupleS2880;
      _result_3664.tag = 0;
      _result_3664.data.err
      = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2879;
      return _result_3664;
      break;
    }
  }
  goto joinlet_3663;
  join_1212:;
  moonbit_incref(_M0L5__mapS1213);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L7_2abindS1217
  = _M0MPB3Map3getGsRPB4JsonE(_M0L5__mapS1213, (moonbit_string_t)moonbit_string_literal_11.data);
  if (_M0L7_2abindS1217 == 0) {
    if (_M0L7_2abindS1217) {
      moonbit_decref(_M0L7_2abindS1217);
    }
  } else {
    void* _M0L7_2aSomeS1218 = _M0L7_2abindS1217;
    void* _M0L6_2a__vS1219 = _M0L7_2aSomeS1218;
    _M0L3__vS1215 = _M0L6_2a__vS1219;
    goto join_1214;
  }
  goto joinlet_3665;
  join_1214:;
  moonbit_incref(_M0L9_2ax__155S1216);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_2atmpS2864
  = _M0MPC14json8JsonPath8add__key(_M0L9_2ax__155S1216, (moonbit_string_t)moonbit_string_literal_11.data);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _tmp_3666
  = _M0IPC16string6StringPC14json8FromJson10from__json(_M0L3__vS1215, _M0L6_2atmpS2864);
  if (_tmp_3666.tag) {
    moonbit_string_t const _M0L5_2aokS2865 = _tmp_3666.data.ok;
    _M0L6_2atmpS2863 = _M0L5_2aokS2865;
  } else {
    void* const _M0L6_2aerrS2866 = _tmp_3666.data.err;
    struct moonbit_result_1 _result_3667;
    moonbit_decref(_M0L9_2ax__155S1216);
    moonbit_decref(_M0L5__mapS1213);
    moonbit_decref(_M0L21_2ade__end__line__156S1211);
    moonbit_decref(_M0L16_2ade__path__157S1210);
    moonbit_decref(_M0L23_2ade__start__line__158S1209);
    _result_3667.tag = 0;
    _result_3667.data.err = _M0L6_2aerrS2866;
    return _result_3667;
  }
  _M0L6_2atmpS2862 = _M0L6_2atmpS2863;
  _M0L6_2aoldS3227 = _M0L16_2ade__path__157S1210->$0;
  if (_M0L6_2aoldS3227) {
    moonbit_decref(_M0L6_2aoldS3227);
  }
  _M0L16_2ade__path__157S1210->$0 = _M0L6_2atmpS2862;
  joinlet_3665:;
  moonbit_incref(_M0L5__mapS1213);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L7_2abindS1222
  = _M0MPB3Map3getGsRPB4JsonE(_M0L5__mapS1213, (moonbit_string_t)moonbit_string_literal_12.data);
  if (_M0L7_2abindS1222 == 0) {
    if (_M0L7_2abindS1222) {
      moonbit_decref(_M0L7_2abindS1222);
    }
  } else {
    void* _M0L7_2aSomeS1223 = _M0L7_2abindS1222;
    void* _M0L6_2a__vS1224 = _M0L7_2aSomeS1223;
    _M0L3__vS1221 = _M0L6_2a__vS1224;
    goto join_1220;
  }
  goto joinlet_3668;
  join_1220:;
  moonbit_incref(_M0L9_2ax__155S1216);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_2atmpS2870
  = _M0MPC14json8JsonPath8add__key(_M0L9_2ax__155S1216, (moonbit_string_t)moonbit_string_literal_12.data);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _tmp_3669
  = _M0IPC13int3IntPC14json8FromJson10from__json(_M0L3__vS1221, _M0L6_2atmpS2870);
  if (_tmp_3669.tag) {
    int32_t const _M0L5_2aokS2871 = _tmp_3669.data.ok;
    _M0L6_2atmpS2869 = _M0L5_2aokS2871;
  } else {
    void* const _M0L6_2aerrS2872 = _tmp_3669.data.err;
    struct moonbit_result_1 _result_3670;
    moonbit_decref(_M0L9_2ax__155S1216);
    moonbit_decref(_M0L5__mapS1213);
    moonbit_decref(_M0L21_2ade__end__line__156S1211);
    moonbit_decref(_M0L16_2ade__path__157S1210);
    moonbit_decref(_M0L23_2ade__start__line__158S1209);
    _result_3670.tag = 0;
    _result_3670.data.err = _M0L6_2aerrS2872;
    return _result_3670;
  }
  _M0L6_2atmpS2868 = (int64_t)_M0L6_2atmpS2869;
  _M0L4SomeS2867
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGOiE4Some));
  Moonbit_object_header(_M0L4SomeS2867)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16option6OptionGOiE4Some) >> 2, 0, 1);
  ((struct _M0DTPC16option6OptionGOiE4Some*)_M0L4SomeS2867)->$0
  = _M0L6_2atmpS2868;
  _M0L6_2aoldS3226 = _M0L23_2ade__start__line__158S1209->$0;
  moonbit_decref(_M0L6_2aoldS3226);
  _M0L23_2ade__start__line__158S1209->$0 = _M0L4SomeS2867;
  joinlet_3668:;
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L7_2abindS1227
  = _M0MPB3Map3getGsRPB4JsonE(_M0L5__mapS1213, (moonbit_string_t)moonbit_string_literal_13.data);
  if (_M0L7_2abindS1227 == 0) {
    if (_M0L7_2abindS1227) {
      moonbit_decref(_M0L7_2abindS1227);
    }
  } else {
    void* _M0L7_2aSomeS1228 = _M0L7_2abindS1227;
    void* _M0L6_2a__vS1229 = _M0L7_2aSomeS1228;
    _M0L3__vS1226 = _M0L6_2a__vS1229;
    goto join_1225;
  }
  goto joinlet_3671;
  join_1225:;
  moonbit_incref(_M0L9_2ax__155S1216);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _M0L6_2atmpS2876
  = _M0MPC14json8JsonPath8add__key(_M0L9_2ax__155S1216, (moonbit_string_t)moonbit_string_literal_13.data);
  #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
  _tmp_3672
  = _M0IPC13int3IntPC14json8FromJson10from__json(_M0L3__vS1226, _M0L6_2atmpS2876);
  if (_tmp_3672.tag) {
    int32_t const _M0L5_2aokS2877 = _tmp_3672.data.ok;
    _M0L6_2atmpS2875 = _M0L5_2aokS2877;
  } else {
    void* const _M0L6_2aerrS2878 = _tmp_3672.data.err;
    struct moonbit_result_1 _result_3673;
    moonbit_decref(_M0L9_2ax__155S1216);
    moonbit_decref(_M0L21_2ade__end__line__156S1211);
    moonbit_decref(_M0L16_2ade__path__157S1210);
    moonbit_decref(_M0L23_2ade__start__line__158S1209);
    _result_3673.tag = 0;
    _result_3673.data.err = _M0L6_2aerrS2878;
    return _result_3673;
  }
  _M0L6_2atmpS2874 = (int64_t)_M0L6_2atmpS2875;
  _M0L4SomeS2873
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGOiE4Some));
  Moonbit_object_header(_M0L4SomeS2873)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16option6OptionGOiE4Some) >> 2, 0, 1);
  ((struct _M0DTPC16option6OptionGOiE4Some*)_M0L4SomeS2873)->$0
  = _M0L6_2atmpS2874;
  _M0L6_2aoldS3225 = _M0L21_2ade__end__line__156S1211->$0;
  moonbit_decref(_M0L6_2aoldS3225);
  _M0L21_2ade__end__line__156S1211->$0 = _M0L4SomeS2873;
  joinlet_3671:;
  joinlet_3663:;
  _M0L8_2afieldS3224 = _M0L23_2ade__start__line__158S1209->$0;
  _M0L6_2acntS3516
  = Moonbit_object_header(_M0L23_2ade__start__line__158S1209)->rc;
  if (_M0L6_2acntS3516 > 1) {
    int32_t _M0L11_2anew__cntS3517 = _M0L6_2acntS3516 - 1;
    Moonbit_object_header(_M0L23_2ade__start__line__158S1209)->rc
    = _M0L11_2anew__cntS3517;
    moonbit_incref(_M0L8_2afieldS3224);
  } else if (_M0L6_2acntS3516 == 1) {
    #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
    moonbit_free(_M0L23_2ade__start__line__158S1209);
  }
  _M0L7_2abindS1236 = _M0L8_2afieldS3224;
  switch (Moonbit_object_tag(_M0L7_2abindS1236)) {
    case 1: {
      struct _M0DTPC16option6OptionGOiE4Some* _M0L7_2aSomeS1237 =
        (struct _M0DTPC16option6OptionGOiE4Some*)_M0L7_2abindS1236;
      int64_t _M0L8_2afieldS3223 = _M0L7_2aSomeS1237->$0;
      int64_t _M0L4_2avS1238;
      moonbit_decref(_M0L7_2aSomeS1237);
      _M0L4_2avS1238 = _M0L8_2afieldS3223;
      _M0L1vS1235 = _M0L4_2avS1238;
      goto join_1234;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS1236);
      _M0L23_2ade__start__line__158S1233 = 4294967296ll;
      break;
    }
  }
  goto joinlet_3674;
  join_1234:;
  _M0L23_2ade__start__line__158S1233 = _M0L1vS1235;
  joinlet_3674:;
  _M0L8_2afieldS3222 = _M0L16_2ade__path__157S1210->$0;
  _M0L6_2acntS3518 = Moonbit_object_header(_M0L16_2ade__path__157S1210)->rc;
  if (_M0L6_2acntS3518 > 1) {
    int32_t _M0L11_2anew__cntS3519 = _M0L6_2acntS3518 - 1;
    Moonbit_object_header(_M0L16_2ade__path__157S1210)->rc
    = _M0L11_2anew__cntS3519;
    if (_M0L8_2afieldS3222) {
      moonbit_incref(_M0L8_2afieldS3222);
    }
  } else if (_M0L6_2acntS3518 == 1) {
    #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
    moonbit_free(_M0L16_2ade__path__157S1210);
  }
  _M0L7_2abindS1242 = _M0L8_2afieldS3222;
  if (_M0L7_2abindS1242 == 0) {
    struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2883;
    void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2882;
    struct moonbit_result_1 _result_3676;
    if (_M0L7_2abindS1242) {
      moonbit_decref(_M0L7_2abindS1242);
    }
    moonbit_decref(_M0L21_2ade__end__line__156S1211);
    _M0L8_2atupleS2883
    = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
    Moonbit_object_header(_M0L8_2atupleS2883)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2883->$0 = _M0L9_2ax__155S1216;
    _M0L8_2atupleS2883->$1 = (moonbit_string_t)moonbit_string_literal_28.data;
    _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2882
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
    Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2882)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2882)->$0
    = _M0L8_2atupleS2883;
    _result_3676.tag = 0;
    _result_3676.data.err
    = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2882;
    return _result_3676;
  } else {
    moonbit_string_t _M0L7_2aSomeS1243;
    moonbit_string_t _M0L4_2avS1244;
    moonbit_decref(_M0L9_2ax__155S1216);
    _M0L7_2aSomeS1243 = _M0L7_2abindS1242;
    _M0L4_2avS1244 = _M0L7_2aSomeS1243;
    _M0L1vS1241 = _M0L4_2avS1244;
    goto join_1240;
  }
  goto joinlet_3675;
  join_1240:;
  _M0L16_2ade__path__157S1239 = _M0L1vS1241;
  joinlet_3675:;
  _M0L8_2afieldS3221 = _M0L21_2ade__end__line__156S1211->$0;
  _M0L6_2acntS3520
  = Moonbit_object_header(_M0L21_2ade__end__line__156S1211)->rc;
  if (_M0L6_2acntS3520 > 1) {
    int32_t _M0L11_2anew__cntS3521 = _M0L6_2acntS3520 - 1;
    Moonbit_object_header(_M0L21_2ade__end__line__156S1211)->rc
    = _M0L11_2anew__cntS3521;
    moonbit_incref(_M0L8_2afieldS3221);
  } else if (_M0L6_2acntS3520 == 1) {
    #line 2 "E:\\moonbit\\clawteam\\tools\\read_file\\schema.mbt"
    moonbit_free(_M0L21_2ade__end__line__156S1211);
  }
  _M0L7_2abindS1248 = _M0L8_2afieldS3221;
  switch (Moonbit_object_tag(_M0L7_2abindS1248)) {
    case 1: {
      struct _M0DTPC16option6OptionGOiE4Some* _M0L7_2aSomeS1249 =
        (struct _M0DTPC16option6OptionGOiE4Some*)_M0L7_2abindS1248;
      int64_t _M0L8_2afieldS3220 = _M0L7_2aSomeS1249->$0;
      int64_t _M0L4_2avS1250;
      moonbit_decref(_M0L7_2aSomeS1249);
      _M0L4_2avS1250 = _M0L8_2afieldS3220;
      _M0L1vS1247 = _M0L4_2avS1250;
      goto join_1246;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS1248);
      _M0L21_2ade__end__line__156S1245 = 4294967296ll;
      break;
    }
  }
  goto joinlet_3677;
  join_1246:;
  _M0L21_2ade__end__line__156S1245 = _M0L1vS1247;
  joinlet_3677:;
  _M0L6_2atmpS2881
  = (struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput));
  Moonbit_object_header(_M0L6_2atmpS2881)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput, $0) >> 2, 1, 0);
  _M0L6_2atmpS2881->$0 = _M0L16_2ade__path__157S1239;
  _M0L6_2atmpS2881->$1 = _M0L23_2ade__start__line__158S1233;
  _M0L6_2atmpS2881->$2 = _M0L21_2ade__end__line__156S1245;
  _result_3678.tag = 1;
  _result_3678.data.ok = _M0L6_2atmpS2881;
  return _result_3678;
}

struct moonbit_result_2 _M0IPC16string6StringPC14json8FromJson10from__json(
  void* _M0L4jsonS1205,
  void* _M0L4pathS1208
) {
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  switch (Moonbit_object_tag(_M0L4jsonS1205)) {
    case 4: {
      struct _M0DTPB4Json6String* _M0L9_2aStringS1206;
      moonbit_string_t _M0L8_2afieldS3229;
      int32_t _M0L6_2acntS3522;
      moonbit_string_t _M0L4_2aaS1207;
      struct moonbit_result_2 _result_3679;
      moonbit_decref(_M0L4pathS1208);
      _M0L9_2aStringS1206 = (struct _M0DTPB4Json6String*)_M0L4jsonS1205;
      _M0L8_2afieldS3229 = _M0L9_2aStringS1206->$0;
      _M0L6_2acntS3522 = Moonbit_object_header(_M0L9_2aStringS1206)->rc;
      if (_M0L6_2acntS3522 > 1) {
        int32_t _M0L11_2anew__cntS3523 = _M0L6_2acntS3522 - 1;
        Moonbit_object_header(_M0L9_2aStringS1206)->rc
        = _M0L11_2anew__cntS3523;
        moonbit_incref(_M0L8_2afieldS3229);
      } else if (_M0L6_2acntS3522 == 1) {
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
        moonbit_free(_M0L9_2aStringS1206);
      }
      _M0L4_2aaS1207 = _M0L8_2afieldS3229;
      _result_3679.tag = 1;
      _result_3679.data.ok = _M0L4_2aaS1207;
      return _result_3679;
      break;
    }
    default: {
      moonbit_decref(_M0L4jsonS1205);
      #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
      return _M0FPC14json13decode__errorGsE(_M0L4pathS1208, (moonbit_string_t)moonbit_string_literal_29.data);
      break;
    }
  }
}

struct moonbit_result_0 _M0IPC13int3IntPC14json8FromJson10from__json(
  void* _M0L4jsonS1202,
  void* _M0L4pathS1201
) {
  #line 51 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  switch (Moonbit_object_tag(_M0L4jsonS1202)) {
    case 3: {
      struct _M0DTPB4Json6Number* _M0L9_2aNumberS1203 =
        (struct _M0DTPB4Json6Number*)_M0L4jsonS1202;
      double _M0L8_2afieldS3230 = _M0L9_2aNumberS1203->$0;
      double _M0L4_2anS1204;
      moonbit_decref(_M0L9_2aNumberS1203);
      _M0L4_2anS1204 = _M0L8_2afieldS3230;
      if (_M0L4_2anS1204 != _M0FPC16double8infinity) {
        if (_M0L4_2anS1204 != _M0FPC16double13neg__infinity) {
          int32_t _if__result_3681;
          int32_t _M0L6_2atmpS2861;
          struct moonbit_result_0 _result_3684;
          if (_M0L4_2anS1204 > 0x1.fffffffcp+30) {
            _if__result_3681 = 1;
          } else {
            _if__result_3681 = _M0L4_2anS1204 < -0x1p+31;
          }
          if (_if__result_3681) {
            struct moonbit_result_0 _tmp_3682;
            #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
            _tmp_3682
            = _M0FPC14json13decode__errorGuE(_M0L4pathS1201, (moonbit_string_t)moonbit_string_literal_30.data);
            if (_tmp_3682.tag) {
              int32_t const _M0L5_2aokS2859 = _tmp_3682.data.ok;
            } else {
              void* const _M0L6_2aerrS2860 = _tmp_3682.data.err;
              struct moonbit_result_0 _result_3683;
              _result_3683.tag = 0;
              _result_3683.data.err = _M0L6_2aerrS2860;
              return _result_3683;
            }
          } else {
            moonbit_decref(_M0L4pathS1201);
          }
          #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
          _M0L6_2atmpS2861 = _M0MPC16double6Double7to__int(_M0L4_2anS1204);
          _result_3684.tag = 1;
          _result_3684.data.ok = _M0L6_2atmpS2861;
          return _result_3684;
        } else {
          goto join_1200;
        }
      } else {
        goto join_1200;
      }
      break;
    }
    default: {
      moonbit_decref(_M0L4jsonS1202);
      goto join_1200;
      break;
    }
  }
  join_1200:;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  return _M0FPC14json13decode__errorGiE(_M0L4pathS1201, (moonbit_string_t)moonbit_string_literal_31.data);
}

struct moonbit_result_1 _M0FPC14json10from__jsonGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(
  void* _M0L4jsonS1199,
  void* _M0L10path_2eoptS1197
) {
  void* _M0L4pathS1196;
  if (_M0L10path_2eoptS1197 == 0) {
    if (_M0L10path_2eoptS1197) {
      moonbit_decref(_M0L10path_2eoptS1197);
    }
    _M0L4pathS1196
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    void* _M0L7_2aSomeS1198 = _M0L10path_2eoptS1197;
    _M0L4pathS1196 = _M0L7_2aSomeS1198;
  }
  return _M0FPC14json18from__json_2einnerGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(_M0L4jsonS1199, _M0L4pathS1196);
}

struct moonbit_result_1 _M0FPC14json18from__json_2einnerGRP48clawteam8clawteam5tools10read__file13ReadFileInputE(
  void* _M0L4jsonS1194,
  void* _M0L4pathS1195
) {
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  return _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPC14json8FromJson10from__json(_M0L4jsonS1194, _M0L4pathS1195);
}

int32_t _M0IPC14json8JsonPathPB4Show6output(
  void* _M0L4selfS1192,
  struct _M0TPB6Logger _M0L6loggerS1193
) {
  #line 35 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(_M0L4selfS1192, _M0L6loggerS1193);
  return 0;
}

int32_t _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(
  void* _M0L4pathS1177,
  struct _M0TPB6Logger _M0L6loggerS1181
) {
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  switch (Moonbit_object_tag(_M0L4pathS1177)) {
    case 0: {
      if (_M0L6loggerS1181.$1) {
        moonbit_decref(_M0L6loggerS1181.$1);
      }
      break;
    }
    
    case 1: {
      struct _M0DTPC14json8JsonPath3Key* _M0L6_2aKeyS1178 =
        (struct _M0DTPC14json8JsonPath3Key*)_M0L4pathS1177;
      void* _M0L8_2afieldS3232 = _M0L6_2aKeyS1178->$0;
      void* _M0L9_2aparentS1179 = _M0L8_2afieldS3232;
      moonbit_string_t _M0L8_2afieldS3231 = _M0L6_2aKeyS1178->$1;
      int32_t _M0L6_2acntS3524 = Moonbit_object_header(_M0L6_2aKeyS1178)->rc;
      moonbit_string_t _M0L6_2akeyS1180;
      int32_t _M0L16_2areturn__valueS1183;
      int32_t _M0L6_2atmpS2857;
      struct _M0TPC16string10StringView _M0L6_2atmpS2856;
      int32_t _M0L6_2atmpS2855;
      struct _M0TWEOc* _M0L5_2aitS1184;
      if (_M0L6_2acntS3524 > 1) {
        int32_t _M0L11_2anew__cntS3525 = _M0L6_2acntS3524 - 1;
        Moonbit_object_header(_M0L6_2aKeyS1178)->rc = _M0L11_2anew__cntS3525;
        moonbit_incref(_M0L8_2afieldS3231);
        moonbit_incref(_M0L9_2aparentS1179);
      } else if (_M0L6_2acntS3524 == 1) {
        #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        moonbit_free(_M0L6_2aKeyS1178);
      }
      _M0L6_2akeyS1180 = _M0L8_2afieldS3231;
      if (_M0L6loggerS1181.$1) {
        moonbit_incref(_M0L6loggerS1181.$1);
      }
      #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(_M0L9_2aparentS1179, _M0L6loggerS1181);
      if (_M0L6loggerS1181.$1) {
        moonbit_incref(_M0L6loggerS1181.$1);
      }
      #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L6loggerS1181.$0->$method_3(_M0L6loggerS1181.$1, 47);
      _M0L6_2atmpS2857
      = Moonbit_array_length(_M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376);
      moonbit_incref(_M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376);
      _M0L6_2atmpS2856
      = (struct _M0TPC16string10StringView){
        0,
          _M0L6_2atmpS2857,
          _M0IPC14json8JsonPathPB4Show6outputN7_2abindS1376
      };
      moonbit_incref(_M0L6_2akeyS1180);
      #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L6_2atmpS2855
      = _M0MPC16string6String13contains__any(_M0L6_2akeyS1180, _M0L6_2atmpS2856);
      if (!_M0L6_2atmpS2855) {
        int32_t _M0L6_2atmpS2858;
        #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        _M0L6loggerS1181.$0->$method_0(_M0L6loggerS1181.$1, _M0L6_2akeyS1180);
        _M0L6_2atmpS2858 = 0;
        _M0L16_2areturn__valueS1183 = _M0L6_2atmpS2858;
        goto join_1182;
      }
      #line 42 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L5_2aitS1184 = _M0MPC16string6String4iter(_M0L6_2akeyS1180);
      while (1) {
        int32_t _M0L7_2abindS1185;
        moonbit_incref(_M0L5_2aitS1184);
        #line 42 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        _M0L7_2abindS1185 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1184);
        if (_M0L7_2abindS1185 == -1) {
          moonbit_decref(_M0L5_2aitS1184);
          if (_M0L6loggerS1181.$1) {
            moonbit_decref(_M0L6loggerS1181.$1);
          }
        } else {
          int32_t _M0L7_2aSomeS1186 = _M0L7_2abindS1185;
          int32_t _M0L5_2achS1187 = _M0L7_2aSomeS1186;
          if (_M0L5_2achS1187 == 126) {
            if (_M0L6loggerS1181.$1) {
              moonbit_incref(_M0L6loggerS1181.$1);
            }
            #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
            _M0L6loggerS1181.$0->$method_0(_M0L6loggerS1181.$1, (moonbit_string_t)moonbit_string_literal_32.data);
          } else if (_M0L5_2achS1187 == 47) {
            if (_M0L6loggerS1181.$1) {
              moonbit_incref(_M0L6loggerS1181.$1);
            }
            #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
            _M0L6loggerS1181.$0->$method_0(_M0L6loggerS1181.$1, (moonbit_string_t)moonbit_string_literal_33.data);
          } else {
            if (_M0L6loggerS1181.$1) {
              moonbit_incref(_M0L6loggerS1181.$1);
            }
            #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
            _M0L6loggerS1181.$0->$method_3(_M0L6loggerS1181.$1, _M0L5_2achS1187);
          }
          continue;
        }
        break;
      }
      goto joinlet_3685;
      join_1182:;
      joinlet_3685:;
      break;
    }
    default: {
      struct _M0DTPC14json8JsonPath5Index* _M0L8_2aIndexS1189 =
        (struct _M0DTPC14json8JsonPath5Index*)_M0L4pathS1177;
      void* _M0L8_2afieldS3234 = _M0L8_2aIndexS1189->$0;
      void* _M0L9_2aparentS1190 = _M0L8_2afieldS3234;
      int32_t _M0L8_2afieldS3233 = _M0L8_2aIndexS1189->$1;
      int32_t _M0L6_2acntS3526 =
        Moonbit_object_header(_M0L8_2aIndexS1189)->rc;
      int32_t _M0L8_2aindexS1191;
      if (_M0L6_2acntS3526 > 1) {
        int32_t _M0L11_2anew__cntS3527 = _M0L6_2acntS3526 - 1;
        Moonbit_object_header(_M0L8_2aIndexS1189)->rc
        = _M0L11_2anew__cntS3527;
        moonbit_incref(_M0L9_2aparentS1190);
      } else if (_M0L6_2acntS3526 == 1) {
        #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
        moonbit_free(_M0L8_2aIndexS1189);
      }
      _M0L8_2aindexS1191 = _M0L8_2afieldS3233;
      if (_M0L6loggerS1181.$1) {
        moonbit_incref(_M0L6loggerS1181.$1);
      }
      #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0IPC14json8JsonPathPB4Show6outputN11build__pathS178(_M0L9_2aparentS1190, _M0L6loggerS1181);
      if (_M0L6loggerS1181.$1) {
        moonbit_incref(_M0L6loggerS1181.$1);
      }
      #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0L6loggerS1181.$0->$method_3(_M0L6loggerS1181.$1, 47);
      #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
      _M0MPB6Logger13write__objectGiE(_M0L6loggerS1181, _M0L8_2aindexS1191);
      break;
    }
  }
  return 0;
}

void* _M0MPC14json8JsonPath8add__key(
  void* _M0L4selfS1175,
  moonbit_string_t _M0L3keyS1176
) {
  void* _block_3687;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json_path.mbt"
  _block_3687
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json8JsonPath3Key));
  Moonbit_object_header(_block_3687)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json8JsonPath3Key, $0) >> 2, 2, 1);
  ((struct _M0DTPC14json8JsonPath3Key*)_block_3687)->$0 = _M0L4selfS1175;
  ((struct _M0DTPC14json8JsonPath3Key*)_block_3687)->$1 = _M0L3keyS1176;
  return _block_3687;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1170,
  void* _M0L7contentS1172,
  moonbit_string_t _M0L3locS1166,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1168
) {
  moonbit_string_t _M0L3locS1165;
  moonbit_string_t _M0L9args__locS1167;
  void* _M0L6_2atmpS2853;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2854;
  moonbit_string_t _M0L6actualS1169;
  moonbit_string_t _M0L4wantS1171;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1165 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1166);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1167 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1168);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2853 = _M0L3objS1170.$0->$method_0(_M0L3objS1170.$1);
  _M0L6_2atmpS2854 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1169
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2853, 0, 0, _M0L6_2atmpS2854);
  if (_M0L7contentS1172 == 0) {
    void* _M0L6_2atmpS2850;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2851;
    if (_M0L7contentS1172) {
      moonbit_decref(_M0L7contentS1172);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2850
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_1.data);
    _M0L6_2atmpS2851 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1171
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2850, 0, 0, _M0L6_2atmpS2851);
  } else {
    void* _M0L7_2aSomeS1173 = _M0L7contentS1172;
    void* _M0L4_2axS1174 = _M0L7_2aSomeS1173;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2852 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1171
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1174, 0, 0, _M0L6_2atmpS2852);
  }
  moonbit_incref(_M0L4wantS1171);
  moonbit_incref(_M0L6actualS1169);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1169, _M0L4wantS1171)
  ) {
    moonbit_string_t _M0L6_2atmpS2848;
    moonbit_string_t _M0L6_2atmpS3242;
    moonbit_string_t _M0L6_2atmpS2847;
    moonbit_string_t _M0L6_2atmpS3241;
    moonbit_string_t _M0L6_2atmpS2845;
    moonbit_string_t _M0L6_2atmpS2846;
    moonbit_string_t _M0L6_2atmpS3240;
    moonbit_string_t _M0L6_2atmpS2844;
    moonbit_string_t _M0L6_2atmpS3239;
    moonbit_string_t _M0L6_2atmpS2841;
    moonbit_string_t _M0L6_2atmpS2843;
    moonbit_string_t _M0L6_2atmpS2842;
    moonbit_string_t _M0L6_2atmpS3238;
    moonbit_string_t _M0L6_2atmpS2840;
    moonbit_string_t _M0L6_2atmpS3237;
    moonbit_string_t _M0L6_2atmpS2837;
    moonbit_string_t _M0L6_2atmpS2839;
    moonbit_string_t _M0L6_2atmpS2838;
    moonbit_string_t _M0L6_2atmpS3236;
    moonbit_string_t _M0L6_2atmpS2836;
    moonbit_string_t _M0L6_2atmpS3235;
    moonbit_string_t _M0L6_2atmpS2835;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2834;
    struct moonbit_result_0 _result_3688;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2848
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1165);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3242
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_34.data, _M0L6_2atmpS2848);
    moonbit_decref(_M0L6_2atmpS2848);
    _M0L6_2atmpS2847 = _M0L6_2atmpS3242;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3241
    = moonbit_add_string(_M0L6_2atmpS2847, (moonbit_string_t)moonbit_string_literal_35.data);
    moonbit_decref(_M0L6_2atmpS2847);
    _M0L6_2atmpS2845 = _M0L6_2atmpS3241;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2846
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1167);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3240 = moonbit_add_string(_M0L6_2atmpS2845, _M0L6_2atmpS2846);
    moonbit_decref(_M0L6_2atmpS2845);
    moonbit_decref(_M0L6_2atmpS2846);
    _M0L6_2atmpS2844 = _M0L6_2atmpS3240;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3239
    = moonbit_add_string(_M0L6_2atmpS2844, (moonbit_string_t)moonbit_string_literal_36.data);
    moonbit_decref(_M0L6_2atmpS2844);
    _M0L6_2atmpS2841 = _M0L6_2atmpS3239;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2843 = _M0MPC16string6String6escape(_M0L4wantS1171);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2842
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2843);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3238 = moonbit_add_string(_M0L6_2atmpS2841, _M0L6_2atmpS2842);
    moonbit_decref(_M0L6_2atmpS2841);
    moonbit_decref(_M0L6_2atmpS2842);
    _M0L6_2atmpS2840 = _M0L6_2atmpS3238;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3237
    = moonbit_add_string(_M0L6_2atmpS2840, (moonbit_string_t)moonbit_string_literal_37.data);
    moonbit_decref(_M0L6_2atmpS2840);
    _M0L6_2atmpS2837 = _M0L6_2atmpS3237;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2839 = _M0MPC16string6String6escape(_M0L6actualS1169);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2838
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2839);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3236 = moonbit_add_string(_M0L6_2atmpS2837, _M0L6_2atmpS2838);
    moonbit_decref(_M0L6_2atmpS2837);
    moonbit_decref(_M0L6_2atmpS2838);
    _M0L6_2atmpS2836 = _M0L6_2atmpS3236;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3235
    = moonbit_add_string(_M0L6_2atmpS2836, (moonbit_string_t)moonbit_string_literal_38.data);
    moonbit_decref(_M0L6_2atmpS2836);
    _M0L6_2atmpS2835 = _M0L6_2atmpS3235;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2834
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2834)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2834)->$0
    = _M0L6_2atmpS2835;
    _result_3688.tag = 0;
    _result_3688.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2834;
    return _result_3688;
  } else {
    int32_t _M0L6_2atmpS2849;
    struct moonbit_result_0 _result_3689;
    moonbit_decref(_M0L4wantS1171);
    moonbit_decref(_M0L6actualS1169);
    moonbit_decref(_M0L9args__locS1167);
    moonbit_decref(_M0L3locS1165);
    _M0L6_2atmpS2849 = 0;
    _result_3689.tag = 1;
    _result_3689.data.ok = _M0L6_2atmpS2849;
    return _result_3689;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1164,
  int32_t _M0L13escape__slashS1136,
  int32_t _M0L6indentS1131,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1157
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1123;
  void** _M0L6_2atmpS2833;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1124;
  int32_t _M0Lm5depthS1125;
  void* _M0L6_2atmpS2832;
  void* _M0L8_2aparamS1126;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1123 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2833 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1124
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1124)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1124->$0 = _M0L6_2atmpS2833;
  _M0L5stackS1124->$1 = 0;
  _M0Lm5depthS1125 = 0;
  _M0L6_2atmpS2832 = _M0L4selfS1164;
  _M0L8_2aparamS1126 = _M0L6_2atmpS2832;
  _2aloop_1142:;
  while (1) {
    if (_M0L8_2aparamS1126 == 0) {
      int32_t _M0L3lenS2794;
      if (_M0L8_2aparamS1126) {
        moonbit_decref(_M0L8_2aparamS1126);
      }
      _M0L3lenS2794 = _M0L5stackS1124->$1;
      if (_M0L3lenS2794 == 0) {
        if (_M0L8replacerS1157) {
          moonbit_decref(_M0L8replacerS1157);
        }
        moonbit_decref(_M0L5stackS1124);
        break;
      } else {
        void** _M0L8_2afieldS3250 = _M0L5stackS1124->$0;
        void** _M0L3bufS2818 = _M0L8_2afieldS3250;
        int32_t _M0L3lenS2820 = _M0L5stackS1124->$1;
        int32_t _M0L6_2atmpS2819 = _M0L3lenS2820 - 1;
        void* _M0L6_2atmpS3249 = (void*)_M0L3bufS2818[_M0L6_2atmpS2819];
        void* _M0L4_2axS1143 = _M0L6_2atmpS3249;
        switch (Moonbit_object_tag(_M0L4_2axS1143)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1144 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1143;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3245 =
              _M0L8_2aArrayS1144->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1145 =
              _M0L8_2afieldS3245;
            int32_t _M0L4_2aiS1146 = _M0L8_2aArrayS1144->$1;
            int32_t _M0L3lenS2806 = _M0L6_2aarrS1145->$1;
            if (_M0L4_2aiS1146 < _M0L3lenS2806) {
              int32_t _if__result_3691;
              void** _M0L8_2afieldS3244;
              void** _M0L3bufS2812;
              void* _M0L6_2atmpS3243;
              void* _M0L7elementS1147;
              int32_t _M0L6_2atmpS2807;
              void* _M0L6_2atmpS2810;
              if (_M0L4_2aiS1146 < 0) {
                _if__result_3691 = 1;
              } else {
                int32_t _M0L3lenS2811 = _M0L6_2aarrS1145->$1;
                _if__result_3691 = _M0L4_2aiS1146 >= _M0L3lenS2811;
              }
              if (_if__result_3691) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3244 = _M0L6_2aarrS1145->$0;
              _M0L3bufS2812 = _M0L8_2afieldS3244;
              _M0L6_2atmpS3243 = (void*)_M0L3bufS2812[_M0L4_2aiS1146];
              _M0L7elementS1147 = _M0L6_2atmpS3243;
              _M0L6_2atmpS2807 = _M0L4_2aiS1146 + 1;
              _M0L8_2aArrayS1144->$1 = _M0L6_2atmpS2807;
              if (_M0L4_2aiS1146 > 0) {
                int32_t _M0L6_2atmpS2809;
                moonbit_string_t _M0L6_2atmpS2808;
                moonbit_incref(_M0L7elementS1147);
                moonbit_incref(_M0L3bufS1123);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 44);
                _M0L6_2atmpS2809 = _M0Lm5depthS1125;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2808
                = _M0FPC14json11indent__str(_M0L6_2atmpS2809, _M0L6indentS1131);
                moonbit_incref(_M0L3bufS1123);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2808);
              } else {
                moonbit_incref(_M0L7elementS1147);
              }
              _M0L6_2atmpS2810 = _M0L7elementS1147;
              _M0L8_2aparamS1126 = _M0L6_2atmpS2810;
              goto _2aloop_1142;
            } else {
              int32_t _M0L6_2atmpS2813 = _M0Lm5depthS1125;
              void* _M0L6_2atmpS2814;
              int32_t _M0L6_2atmpS2816;
              moonbit_string_t _M0L6_2atmpS2815;
              void* _M0L6_2atmpS2817;
              _M0Lm5depthS1125 = _M0L6_2atmpS2813 - 1;
              moonbit_incref(_M0L5stackS1124);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2814
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1124);
              if (_M0L6_2atmpS2814) {
                moonbit_decref(_M0L6_2atmpS2814);
              }
              _M0L6_2atmpS2816 = _M0Lm5depthS1125;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2815
              = _M0FPC14json11indent__str(_M0L6_2atmpS2816, _M0L6indentS1131);
              moonbit_incref(_M0L3bufS1123);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2815);
              moonbit_incref(_M0L3bufS1123);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 93);
              _M0L6_2atmpS2817 = 0;
              _M0L8_2aparamS1126 = _M0L6_2atmpS2817;
              goto _2aloop_1142;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1148 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1143;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3248 =
              _M0L9_2aObjectS1148->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1149 =
              _M0L8_2afieldS3248;
            int32_t _M0L8_2afirstS1150 = _M0L9_2aObjectS1148->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1151;
            moonbit_incref(_M0L11_2aiteratorS1149);
            moonbit_incref(_M0L9_2aObjectS1148);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1151
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1149);
            if (_M0L7_2abindS1151 == 0) {
              int32_t _M0L6_2atmpS2795;
              void* _M0L6_2atmpS2796;
              int32_t _M0L6_2atmpS2798;
              moonbit_string_t _M0L6_2atmpS2797;
              void* _M0L6_2atmpS2799;
              if (_M0L7_2abindS1151) {
                moonbit_decref(_M0L7_2abindS1151);
              }
              moonbit_decref(_M0L9_2aObjectS1148);
              _M0L6_2atmpS2795 = _M0Lm5depthS1125;
              _M0Lm5depthS1125 = _M0L6_2atmpS2795 - 1;
              moonbit_incref(_M0L5stackS1124);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2796
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1124);
              if (_M0L6_2atmpS2796) {
                moonbit_decref(_M0L6_2atmpS2796);
              }
              _M0L6_2atmpS2798 = _M0Lm5depthS1125;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2797
              = _M0FPC14json11indent__str(_M0L6_2atmpS2798, _M0L6indentS1131);
              moonbit_incref(_M0L3bufS1123);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2797);
              moonbit_incref(_M0L3bufS1123);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 125);
              _M0L6_2atmpS2799 = 0;
              _M0L8_2aparamS1126 = _M0L6_2atmpS2799;
              goto _2aloop_1142;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1152 = _M0L7_2abindS1151;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1153 = _M0L7_2aSomeS1152;
              moonbit_string_t _M0L8_2afieldS3247 = _M0L4_2axS1153->$0;
              moonbit_string_t _M0L4_2akS1154 = _M0L8_2afieldS3247;
              void* _M0L8_2afieldS3246 = _M0L4_2axS1153->$1;
              int32_t _M0L6_2acntS3528 =
                Moonbit_object_header(_M0L4_2axS1153)->rc;
              void* _M0L4_2avS1155;
              void* _M0Lm2v2S1156;
              moonbit_string_t _M0L6_2atmpS2803;
              void* _M0L6_2atmpS2805;
              void* _M0L6_2atmpS2804;
              if (_M0L6_2acntS3528 > 1) {
                int32_t _M0L11_2anew__cntS3529 = _M0L6_2acntS3528 - 1;
                Moonbit_object_header(_M0L4_2axS1153)->rc
                = _M0L11_2anew__cntS3529;
                moonbit_incref(_M0L8_2afieldS3246);
                moonbit_incref(_M0L4_2akS1154);
              } else if (_M0L6_2acntS3528 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1153);
              }
              _M0L4_2avS1155 = _M0L8_2afieldS3246;
              _M0Lm2v2S1156 = _M0L4_2avS1155;
              if (_M0L8replacerS1157 == 0) {
                moonbit_incref(_M0Lm2v2S1156);
                moonbit_decref(_M0L4_2avS1155);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1158 =
                  _M0L8replacerS1157;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1159 =
                  _M0L7_2aSomeS1158;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1160 =
                  _M0L11_2areplacerS1159;
                void* _M0L7_2abindS1161;
                moonbit_incref(_M0L7_2afuncS1160);
                moonbit_incref(_M0L4_2akS1154);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1161
                = _M0L7_2afuncS1160->code(_M0L7_2afuncS1160, _M0L4_2akS1154, _M0L4_2avS1155);
                if (_M0L7_2abindS1161 == 0) {
                  void* _M0L6_2atmpS2800;
                  if (_M0L7_2abindS1161) {
                    moonbit_decref(_M0L7_2abindS1161);
                  }
                  moonbit_decref(_M0L4_2akS1154);
                  moonbit_decref(_M0L9_2aObjectS1148);
                  _M0L6_2atmpS2800 = 0;
                  _M0L8_2aparamS1126 = _M0L6_2atmpS2800;
                  goto _2aloop_1142;
                } else {
                  void* _M0L7_2aSomeS1162 = _M0L7_2abindS1161;
                  void* _M0L4_2avS1163 = _M0L7_2aSomeS1162;
                  _M0Lm2v2S1156 = _M0L4_2avS1163;
                }
              }
              if (!_M0L8_2afirstS1150) {
                int32_t _M0L6_2atmpS2802;
                moonbit_string_t _M0L6_2atmpS2801;
                moonbit_incref(_M0L3bufS1123);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 44);
                _M0L6_2atmpS2802 = _M0Lm5depthS1125;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2801
                = _M0FPC14json11indent__str(_M0L6_2atmpS2802, _M0L6indentS1131);
                moonbit_incref(_M0L3bufS1123);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2801);
              }
              moonbit_incref(_M0L3bufS1123);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2803
              = _M0FPC14json6escape(_M0L4_2akS1154, _M0L13escape__slashS1136);
              moonbit_incref(_M0L3bufS1123);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2803);
              moonbit_incref(_M0L3bufS1123);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 34);
              moonbit_incref(_M0L3bufS1123);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 58);
              if (_M0L6indentS1131 > 0) {
                moonbit_incref(_M0L3bufS1123);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 32);
              }
              _M0L9_2aObjectS1148->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1148);
              _M0L6_2atmpS2805 = _M0Lm2v2S1156;
              _M0L6_2atmpS2804 = _M0L6_2atmpS2805;
              _M0L8_2aparamS1126 = _M0L6_2atmpS2804;
              goto _2aloop_1142;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1127 = _M0L8_2aparamS1126;
      void* _M0L8_2avalueS1128 = _M0L7_2aSomeS1127;
      void* _M0L6_2atmpS2831;
      switch (Moonbit_object_tag(_M0L8_2avalueS1128)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1129 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1128;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3251 =
            _M0L9_2aObjectS1129->$0;
          int32_t _M0L6_2acntS3530 =
            Moonbit_object_header(_M0L9_2aObjectS1129)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1130;
          if (_M0L6_2acntS3530 > 1) {
            int32_t _M0L11_2anew__cntS3531 = _M0L6_2acntS3530 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1129)->rc
            = _M0L11_2anew__cntS3531;
            moonbit_incref(_M0L8_2afieldS3251);
          } else if (_M0L6_2acntS3530 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1129);
          }
          _M0L10_2amembersS1130 = _M0L8_2afieldS3251;
          moonbit_incref(_M0L10_2amembersS1130);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1130)) {
            moonbit_decref(_M0L10_2amembersS1130);
            moonbit_incref(_M0L3bufS1123);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, (moonbit_string_t)moonbit_string_literal_39.data);
          } else {
            int32_t _M0L6_2atmpS2826 = _M0Lm5depthS1125;
            int32_t _M0L6_2atmpS2828;
            moonbit_string_t _M0L6_2atmpS2827;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2830;
            void* _M0L6ObjectS2829;
            _M0Lm5depthS1125 = _M0L6_2atmpS2826 + 1;
            moonbit_incref(_M0L3bufS1123);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 123);
            _M0L6_2atmpS2828 = _M0Lm5depthS1125;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2827
            = _M0FPC14json11indent__str(_M0L6_2atmpS2828, _M0L6indentS1131);
            moonbit_incref(_M0L3bufS1123);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2827);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2830
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1130);
            _M0L6ObjectS2829
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2829)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2829)->$0
            = _M0L6_2atmpS2830;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2829)->$1
            = 1;
            moonbit_incref(_M0L5stackS1124);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1124, _M0L6ObjectS2829);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1132 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1128;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3252 =
            _M0L8_2aArrayS1132->$0;
          int32_t _M0L6_2acntS3532 =
            Moonbit_object_header(_M0L8_2aArrayS1132)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1133;
          if (_M0L6_2acntS3532 > 1) {
            int32_t _M0L11_2anew__cntS3533 = _M0L6_2acntS3532 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1132)->rc
            = _M0L11_2anew__cntS3533;
            moonbit_incref(_M0L8_2afieldS3252);
          } else if (_M0L6_2acntS3532 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1132);
          }
          _M0L6_2aarrS1133 = _M0L8_2afieldS3252;
          moonbit_incref(_M0L6_2aarrS1133);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1133)) {
            moonbit_decref(_M0L6_2aarrS1133);
            moonbit_incref(_M0L3bufS1123);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, (moonbit_string_t)moonbit_string_literal_40.data);
          } else {
            int32_t _M0L6_2atmpS2822 = _M0Lm5depthS1125;
            int32_t _M0L6_2atmpS2824;
            moonbit_string_t _M0L6_2atmpS2823;
            void* _M0L5ArrayS2825;
            _M0Lm5depthS1125 = _M0L6_2atmpS2822 + 1;
            moonbit_incref(_M0L3bufS1123);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 91);
            _M0L6_2atmpS2824 = _M0Lm5depthS1125;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2823
            = _M0FPC14json11indent__str(_M0L6_2atmpS2824, _M0L6indentS1131);
            moonbit_incref(_M0L3bufS1123);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2823);
            _M0L5ArrayS2825
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2825)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2825)->$0
            = _M0L6_2aarrS1133;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2825)->$1
            = 0;
            moonbit_incref(_M0L5stackS1124);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1124, _M0L5ArrayS2825);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1134 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1128;
          moonbit_string_t _M0L8_2afieldS3253 = _M0L9_2aStringS1134->$0;
          int32_t _M0L6_2acntS3534 =
            Moonbit_object_header(_M0L9_2aStringS1134)->rc;
          moonbit_string_t _M0L4_2asS1135;
          moonbit_string_t _M0L6_2atmpS2821;
          if (_M0L6_2acntS3534 > 1) {
            int32_t _M0L11_2anew__cntS3535 = _M0L6_2acntS3534 - 1;
            Moonbit_object_header(_M0L9_2aStringS1134)->rc
            = _M0L11_2anew__cntS3535;
            moonbit_incref(_M0L8_2afieldS3253);
          } else if (_M0L6_2acntS3534 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1134);
          }
          _M0L4_2asS1135 = _M0L8_2afieldS3253;
          moonbit_incref(_M0L3bufS1123);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2821
          = _M0FPC14json6escape(_M0L4_2asS1135, _M0L13escape__slashS1136);
          moonbit_incref(_M0L3bufS1123);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L6_2atmpS2821);
          moonbit_incref(_M0L3bufS1123);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1123, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1137 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1128;
          double _M0L4_2anS1138 = _M0L9_2aNumberS1137->$0;
          moonbit_string_t _M0L8_2afieldS3254 = _M0L9_2aNumberS1137->$1;
          int32_t _M0L6_2acntS3536 =
            Moonbit_object_header(_M0L9_2aNumberS1137)->rc;
          moonbit_string_t _M0L7_2areprS1139;
          if (_M0L6_2acntS3536 > 1) {
            int32_t _M0L11_2anew__cntS3537 = _M0L6_2acntS3536 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1137)->rc
            = _M0L11_2anew__cntS3537;
            if (_M0L8_2afieldS3254) {
              moonbit_incref(_M0L8_2afieldS3254);
            }
          } else if (_M0L6_2acntS3536 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1137);
          }
          _M0L7_2areprS1139 = _M0L8_2afieldS3254;
          if (_M0L7_2areprS1139 == 0) {
            if (_M0L7_2areprS1139) {
              moonbit_decref(_M0L7_2areprS1139);
            }
            moonbit_incref(_M0L3bufS1123);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1123, _M0L4_2anS1138);
          } else {
            moonbit_string_t _M0L7_2aSomeS1140 = _M0L7_2areprS1139;
            moonbit_string_t _M0L4_2arS1141 = _M0L7_2aSomeS1140;
            moonbit_incref(_M0L3bufS1123);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, _M0L4_2arS1141);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1123);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, (moonbit_string_t)moonbit_string_literal_41.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1123);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, (moonbit_string_t)moonbit_string_literal_42.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1128);
          moonbit_incref(_M0L3bufS1123);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1123, (moonbit_string_t)moonbit_string_literal_43.data);
          break;
        }
      }
      _M0L6_2atmpS2831 = 0;
      _M0L8_2aparamS1126 = _M0L6_2atmpS2831;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1123);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1122,
  int32_t _M0L6indentS1120
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1120 == 0) {
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else {
    int32_t _M0L6spacesS1121 = _M0L6indentS1120 * _M0L5levelS1122;
    switch (_M0L6spacesS1121) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_44.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_45.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_46.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_47.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_48.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_49.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_50.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_51.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_52.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2793;
        moonbit_string_t _M0L6_2atmpS3255;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2793
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_53.data, _M0L6spacesS1121);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3255
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_44.data, _M0L6_2atmpS2793);
        moonbit_decref(_M0L6_2atmpS2793);
        return _M0L6_2atmpS3255;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1112,
  int32_t _M0L13escape__slashS1117
) {
  int32_t _M0L6_2atmpS2792;
  struct _M0TPB13StringBuilder* _M0L3bufS1111;
  struct _M0TWEOc* _M0L5_2aitS1113;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2792 = Moonbit_array_length(_M0L3strS1112);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1111 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2792);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1113 = _M0MPC16string6String4iter(_M0L3strS1112);
  while (1) {
    int32_t _M0L7_2abindS1114;
    moonbit_incref(_M0L5_2aitS1113);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1114 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1113);
    if (_M0L7_2abindS1114 == -1) {
      moonbit_decref(_M0L5_2aitS1113);
    } else {
      int32_t _M0L7_2aSomeS1115 = _M0L7_2abindS1114;
      int32_t _M0L4_2acS1116 = _M0L7_2aSomeS1115;
      if (_M0L4_2acS1116 == 34) {
        moonbit_incref(_M0L3bufS1111);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_54.data);
      } else if (_M0L4_2acS1116 == 92) {
        moonbit_incref(_M0L3bufS1111);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_55.data);
      } else if (_M0L4_2acS1116 == 47) {
        if (_M0L13escape__slashS1117) {
          moonbit_incref(_M0L3bufS1111);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_56.data);
        } else {
          moonbit_incref(_M0L3bufS1111);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1111, _M0L4_2acS1116);
        }
      } else if (_M0L4_2acS1116 == 10) {
        moonbit_incref(_M0L3bufS1111);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_57.data);
      } else if (_M0L4_2acS1116 == 13) {
        moonbit_incref(_M0L3bufS1111);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_58.data);
      } else if (_M0L4_2acS1116 == 8) {
        moonbit_incref(_M0L3bufS1111);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_59.data);
      } else if (_M0L4_2acS1116 == 9) {
        moonbit_incref(_M0L3bufS1111);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_60.data);
      } else {
        int32_t _M0L4codeS1118 = _M0L4_2acS1116;
        if (_M0L4codeS1118 == 12) {
          moonbit_incref(_M0L3bufS1111);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_61.data);
        } else if (_M0L4codeS1118 < 32) {
          int32_t _M0L6_2atmpS2791;
          moonbit_string_t _M0L6_2atmpS2790;
          moonbit_incref(_M0L3bufS1111);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, (moonbit_string_t)moonbit_string_literal_62.data);
          _M0L6_2atmpS2791 = _M0L4codeS1118 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2790 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2791);
          moonbit_incref(_M0L3bufS1111);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1111, _M0L6_2atmpS2790);
        } else {
          moonbit_incref(_M0L3bufS1111);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1111, _M0L4_2acS1116);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1111);
}

struct moonbit_result_2 _M0FPC14json13decode__errorGsE(
  void* _M0L4pathS1105,
  moonbit_string_t _M0L3msgS1106
) {
  struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2785;
  void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2784;
  struct moonbit_result_2 _result_3693;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L8_2atupleS2785
  = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
  Moonbit_object_header(_M0L8_2atupleS2785)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2785->$0 = _M0L4pathS1105;
  _M0L8_2atupleS2785->$1 = _M0L3msgS1106;
  _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2784
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
  Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2784)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2784)->$0
  = _M0L8_2atupleS2785;
  _result_3693.tag = 0;
  _result_3693.data.err
  = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2784;
  return _result_3693;
}

struct moonbit_result_0 _M0FPC14json13decode__errorGiE(
  void* _M0L4pathS1107,
  moonbit_string_t _M0L3msgS1108
) {
  struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2787;
  void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2786;
  struct moonbit_result_0 _result_3694;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L8_2atupleS2787
  = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
  Moonbit_object_header(_M0L8_2atupleS2787)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2787->$0 = _M0L4pathS1107;
  _M0L8_2atupleS2787->$1 = _M0L3msgS1108;
  _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2786
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
  Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2786)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2786)->$0
  = _M0L8_2atupleS2787;
  _result_3694.tag = 0;
  _result_3694.data.err
  = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2786;
  return _result_3694;
}

struct moonbit_result_0 _M0FPC14json13decode__errorGuE(
  void* _M0L4pathS1109,
  moonbit_string_t _M0L3msgS1110
) {
  struct _M0TURPC14json8JsonPathsE* _M0L8_2atupleS2789;
  void* _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2788;
  struct moonbit_result_0 _result_3695;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L8_2atupleS2789
  = (struct _M0TURPC14json8JsonPathsE*)moonbit_malloc(sizeof(struct _M0TURPC14json8JsonPathsE));
  Moonbit_object_header(_M0L8_2atupleS2789)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC14json8JsonPathsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2789->$0 = _M0L4pathS1109;
  _M0L8_2atupleS2789->$1 = _M0L3msgS1110;
  _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2788
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError));
  Moonbit_object_header(_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2788)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError, $0) >> 2, 1, 1);
  ((struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2788)->$0
  = _M0L8_2atupleS2789;
  _result_3695.tag = 0;
  _result_3695.data.err
  = _M0L61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeErrorS2788;
  return _result_3695;
}

int32_t _M0IPC14json15JsonDecodeErrorPB4Show6output(
  void* _M0L9_2ax__628S1099,
  struct _M0TPB6Logger _M0L9_2ax__629S1102
) {
  struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError* _M0L18_2aJsonDecodeErrorS1100;
  struct _M0TURPC14json8JsonPathsE* _M0L8_2afieldS3258;
  int32_t _M0L6_2acntS3538;
  struct _M0TURPC14json8JsonPathsE* _M0L14_2a_2aarg__630S1101;
  void* _M0L8_2afieldS3257;
  void* _M0L13_2a_2ax0__631S1103;
  moonbit_string_t _M0L8_2afieldS3256;
  int32_t _M0L6_2acntS3540;
  moonbit_string_t _M0L13_2a_2ax1__632S1104;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L18_2aJsonDecodeErrorS1100
  = (struct _M0DTPC15error5Error61moonbitlang_2fcore_2fjson_2eJsonDecodeError_2eJsonDecodeError*)_M0L9_2ax__628S1099;
  _M0L8_2afieldS3258 = _M0L18_2aJsonDecodeErrorS1100->$0;
  _M0L6_2acntS3538 = Moonbit_object_header(_M0L18_2aJsonDecodeErrorS1100)->rc;
  if (_M0L6_2acntS3538 > 1) {
    int32_t _M0L11_2anew__cntS3539 = _M0L6_2acntS3538 - 1;
    Moonbit_object_header(_M0L18_2aJsonDecodeErrorS1100)->rc
    = _M0L11_2anew__cntS3539;
    moonbit_incref(_M0L8_2afieldS3258);
  } else if (_M0L6_2acntS3538 == 1) {
    #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
    moonbit_free(_M0L18_2aJsonDecodeErrorS1100);
  }
  _M0L14_2a_2aarg__630S1101 = _M0L8_2afieldS3258;
  if (_M0L9_2ax__629S1102.$1) {
    moonbit_incref(_M0L9_2ax__629S1102.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1102.$0->$method_0(_M0L9_2ax__629S1102.$1, (moonbit_string_t)moonbit_string_literal_63.data);
  _M0L8_2afieldS3257 = _M0L14_2a_2aarg__630S1101->$0;
  _M0L13_2a_2ax0__631S1103 = _M0L8_2afieldS3257;
  _M0L8_2afieldS3256 = _M0L14_2a_2aarg__630S1101->$1;
  _M0L6_2acntS3540 = Moonbit_object_header(_M0L14_2a_2aarg__630S1101)->rc;
  if (_M0L6_2acntS3540 > 1) {
    int32_t _M0L11_2anew__cntS3541 = _M0L6_2acntS3540 - 1;
    Moonbit_object_header(_M0L14_2a_2aarg__630S1101)->rc
    = _M0L11_2anew__cntS3541;
    moonbit_incref(_M0L8_2afieldS3256);
    moonbit_incref(_M0L13_2a_2ax0__631S1103);
  } else if (_M0L6_2acntS3540 == 1) {
    #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
    moonbit_free(_M0L14_2a_2aarg__630S1101);
  }
  _M0L13_2a_2ax1__632S1104 = _M0L8_2afieldS3256;
  if (_M0L9_2ax__629S1102.$1) {
    moonbit_incref(_M0L9_2ax__629S1102.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1102.$0->$method_0(_M0L9_2ax__629S1102.$1, (moonbit_string_t)moonbit_string_literal_64.data);
  if (_M0L9_2ax__629S1102.$1) {
    moonbit_incref(_M0L9_2ax__629S1102.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0MPB6Logger13write__objectGRPC14json8JsonPathE(_M0L9_2ax__629S1102, _M0L13_2a_2ax0__631S1103);
  if (_M0L9_2ax__629S1102.$1) {
    moonbit_incref(_M0L9_2ax__629S1102.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1102.$0->$method_0(_M0L9_2ax__629S1102.$1, (moonbit_string_t)moonbit_string_literal_65.data);
  if (_M0L9_2ax__629S1102.$1) {
    moonbit_incref(_M0L9_2ax__629S1102.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L9_2ax__629S1102, _M0L13_2a_2ax1__632S1104);
  if (_M0L9_2ax__629S1102.$1) {
    moonbit_incref(_M0L9_2ax__629S1102.$1);
  }
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1102.$0->$method_0(_M0L9_2ax__629S1102.$1, (moonbit_string_t)moonbit_string_literal_66.data);
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\from_json.mbt"
  _M0L9_2ax__629S1102.$0->$method_0(_M0L9_2ax__629S1102.$1, (moonbit_string_t)moonbit_string_literal_66.data);
  return 0;
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1098
) {
  int32_t _M0L8_2afieldS3259;
  int32_t _M0L3lenS2783;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3259 = _M0L4selfS1098->$1;
  moonbit_decref(_M0L4selfS1098);
  _M0L3lenS2783 = _M0L8_2afieldS3259;
  return _M0L3lenS2783 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1095
) {
  int32_t _M0L3lenS1094;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1094 = _M0L4selfS1095->$1;
  if (_M0L3lenS1094 == 0) {
    moonbit_decref(_M0L4selfS1095);
    return 0;
  } else {
    int32_t _M0L5indexS1096 = _M0L3lenS1094 - 1;
    void** _M0L8_2afieldS3263 = _M0L4selfS1095->$0;
    void** _M0L3bufS2782 = _M0L8_2afieldS3263;
    void* _M0L6_2atmpS3262 = (void*)_M0L3bufS2782[_M0L5indexS1096];
    void* _M0L1vS1097 = _M0L6_2atmpS3262;
    void** _M0L8_2afieldS3261 = _M0L4selfS1095->$0;
    void** _M0L3bufS2781 = _M0L8_2afieldS3261;
    void* _M0L6_2aoldS3260;
    if (
      _M0L5indexS1096 < 0
      || _M0L5indexS1096 >= Moonbit_array_length(_M0L3bufS2781)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3260 = (void*)_M0L3bufS2781[_M0L5indexS1096];
    moonbit_incref(_M0L1vS1097);
    moonbit_decref(_M0L6_2aoldS3260);
    if (
      _M0L5indexS1096 < 0
      || _M0L5indexS1096 >= Moonbit_array_length(_M0L3bufS2781)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2781[_M0L5indexS1096]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1095->$1 = _M0L5indexS1096;
    moonbit_decref(_M0L4selfS1095);
    return _M0L1vS1097;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1092,
  struct _M0TPB6Logger _M0L6loggerS1093
) {
  moonbit_string_t _M0L6_2atmpS2780;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2779;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2780 = _M0L4selfS1092;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2779 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2780);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2779, _M0L6loggerS1093);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1069,
  struct _M0TPB6Logger _M0L6loggerS1091
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3272;
  struct _M0TPC16string10StringView _M0L3pkgS1068;
  moonbit_string_t _M0L7_2adataS1070;
  int32_t _M0L8_2astartS1071;
  int32_t _M0L6_2atmpS2778;
  int32_t _M0L6_2aendS1072;
  int32_t _M0Lm9_2acursorS1073;
  int32_t _M0Lm13accept__stateS1074;
  int32_t _M0Lm10match__endS1075;
  int32_t _M0Lm20match__tag__saver__0S1076;
  int32_t _M0Lm6tag__0S1077;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1078;
  struct _M0TPC16string10StringView _M0L8_2afieldS3271;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1087;
  void* _M0L8_2afieldS3270;
  int32_t _M0L6_2acntS3542;
  void* _M0L16_2apackage__nameS1088;
  struct _M0TPC16string10StringView _M0L8_2afieldS3268;
  struct _M0TPC16string10StringView _M0L8filenameS2755;
  struct _M0TPC16string10StringView _M0L8_2afieldS3267;
  struct _M0TPC16string10StringView _M0L11start__lineS2756;
  struct _M0TPC16string10StringView _M0L8_2afieldS3266;
  struct _M0TPC16string10StringView _M0L13start__columnS2757;
  struct _M0TPC16string10StringView _M0L8_2afieldS3265;
  struct _M0TPC16string10StringView _M0L9end__lineS2758;
  struct _M0TPC16string10StringView _M0L8_2afieldS3264;
  int32_t _M0L6_2acntS3546;
  struct _M0TPC16string10StringView _M0L11end__columnS2759;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3272
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1069->$0_1, _M0L4selfS1069->$0_2, _M0L4selfS1069->$0_0
  };
  _M0L3pkgS1068 = _M0L8_2afieldS3272;
  moonbit_incref(_M0L3pkgS1068.$0);
  moonbit_incref(_M0L3pkgS1068.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1070 = _M0MPC16string10StringView4data(_M0L3pkgS1068);
  moonbit_incref(_M0L3pkgS1068.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1071
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1068);
  moonbit_incref(_M0L3pkgS1068.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2778 = _M0MPC16string10StringView6length(_M0L3pkgS1068);
  _M0L6_2aendS1072 = _M0L8_2astartS1071 + _M0L6_2atmpS2778;
  _M0Lm9_2acursorS1073 = _M0L8_2astartS1071;
  _M0Lm13accept__stateS1074 = -1;
  _M0Lm10match__endS1075 = -1;
  _M0Lm20match__tag__saver__0S1076 = -1;
  _M0Lm6tag__0S1077 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2770 = _M0Lm9_2acursorS1073;
    if (_M0L6_2atmpS2770 < _M0L6_2aendS1072) {
      int32_t _M0L6_2atmpS2777 = _M0Lm9_2acursorS1073;
      int32_t _M0L10next__charS1082;
      int32_t _M0L6_2atmpS2771;
      moonbit_incref(_M0L7_2adataS1070);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1082
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1070, _M0L6_2atmpS2777);
      _M0L6_2atmpS2771 = _M0Lm9_2acursorS1073;
      _M0Lm9_2acursorS1073 = _M0L6_2atmpS2771 + 1;
      if (_M0L10next__charS1082 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2772;
          _M0Lm6tag__0S1077 = _M0Lm9_2acursorS1073;
          _M0L6_2atmpS2772 = _M0Lm9_2acursorS1073;
          if (_M0L6_2atmpS2772 < _M0L6_2aendS1072) {
            int32_t _M0L6_2atmpS2776 = _M0Lm9_2acursorS1073;
            int32_t _M0L10next__charS1083;
            int32_t _M0L6_2atmpS2773;
            moonbit_incref(_M0L7_2adataS1070);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1083
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1070, _M0L6_2atmpS2776);
            _M0L6_2atmpS2773 = _M0Lm9_2acursorS1073;
            _M0Lm9_2acursorS1073 = _M0L6_2atmpS2773 + 1;
            if (_M0L10next__charS1083 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2774 = _M0Lm9_2acursorS1073;
                if (_M0L6_2atmpS2774 < _M0L6_2aendS1072) {
                  int32_t _M0L6_2atmpS2775 = _M0Lm9_2acursorS1073;
                  _M0Lm9_2acursorS1073 = _M0L6_2atmpS2775 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1076 = _M0Lm6tag__0S1077;
                  _M0Lm13accept__stateS1074 = 0;
                  _M0Lm10match__endS1075 = _M0Lm9_2acursorS1073;
                  goto join_1079;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1079;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1079;
    }
    break;
  }
  goto joinlet_3696;
  join_1079:;
  switch (_M0Lm13accept__stateS1074) {
    case 0: {
      int32_t _M0L6_2atmpS2768;
      int32_t _M0L6_2atmpS2767;
      int64_t _M0L6_2atmpS2764;
      int32_t _M0L6_2atmpS2766;
      int64_t _M0L6_2atmpS2765;
      struct _M0TPC16string10StringView _M0L13package__nameS1080;
      int64_t _M0L6_2atmpS2761;
      int32_t _M0L6_2atmpS2763;
      int64_t _M0L6_2atmpS2762;
      struct _M0TPC16string10StringView _M0L12module__nameS1081;
      void* _M0L4SomeS2760;
      moonbit_decref(_M0L3pkgS1068.$0);
      _M0L6_2atmpS2768 = _M0Lm20match__tag__saver__0S1076;
      _M0L6_2atmpS2767 = _M0L6_2atmpS2768 + 1;
      _M0L6_2atmpS2764 = (int64_t)_M0L6_2atmpS2767;
      _M0L6_2atmpS2766 = _M0Lm10match__endS1075;
      _M0L6_2atmpS2765 = (int64_t)_M0L6_2atmpS2766;
      moonbit_incref(_M0L7_2adataS1070);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1080
      = _M0MPC16string6String4view(_M0L7_2adataS1070, _M0L6_2atmpS2764, _M0L6_2atmpS2765);
      _M0L6_2atmpS2761 = (int64_t)_M0L8_2astartS1071;
      _M0L6_2atmpS2763 = _M0Lm20match__tag__saver__0S1076;
      _M0L6_2atmpS2762 = (int64_t)_M0L6_2atmpS2763;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1081
      = _M0MPC16string6String4view(_M0L7_2adataS1070, _M0L6_2atmpS2761, _M0L6_2atmpS2762);
      _M0L4SomeS2760
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2760)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2760)->$0_0
      = _M0L13package__nameS1080.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2760)->$0_1
      = _M0L13package__nameS1080.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2760)->$0_2
      = _M0L13package__nameS1080.$2;
      _M0L7_2abindS1078
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1078)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1078->$0_0 = _M0L12module__nameS1081.$0;
      _M0L7_2abindS1078->$0_1 = _M0L12module__nameS1081.$1;
      _M0L7_2abindS1078->$0_2 = _M0L12module__nameS1081.$2;
      _M0L7_2abindS1078->$1 = _M0L4SomeS2760;
      break;
    }
    default: {
      void* _M0L4NoneS2769;
      moonbit_decref(_M0L7_2adataS1070);
      _M0L4NoneS2769
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1078
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1078)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1078->$0_0 = _M0L3pkgS1068.$0;
      _M0L7_2abindS1078->$0_1 = _M0L3pkgS1068.$1;
      _M0L7_2abindS1078->$0_2 = _M0L3pkgS1068.$2;
      _M0L7_2abindS1078->$1 = _M0L4NoneS2769;
      break;
    }
  }
  joinlet_3696:;
  _M0L8_2afieldS3271
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1078->$0_1, _M0L7_2abindS1078->$0_2, _M0L7_2abindS1078->$0_0
  };
  _M0L15_2amodule__nameS1087 = _M0L8_2afieldS3271;
  _M0L8_2afieldS3270 = _M0L7_2abindS1078->$1;
  _M0L6_2acntS3542 = Moonbit_object_header(_M0L7_2abindS1078)->rc;
  if (_M0L6_2acntS3542 > 1) {
    int32_t _M0L11_2anew__cntS3543 = _M0L6_2acntS3542 - 1;
    Moonbit_object_header(_M0L7_2abindS1078)->rc = _M0L11_2anew__cntS3543;
    moonbit_incref(_M0L8_2afieldS3270);
    moonbit_incref(_M0L15_2amodule__nameS1087.$0);
  } else if (_M0L6_2acntS3542 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1078);
  }
  _M0L16_2apackage__nameS1088 = _M0L8_2afieldS3270;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1088)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1089 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1088;
      struct _M0TPC16string10StringView _M0L8_2afieldS3269 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1089->$0_1,
                                              _M0L7_2aSomeS1089->$0_2,
                                              _M0L7_2aSomeS1089->$0_0};
      int32_t _M0L6_2acntS3544 = Moonbit_object_header(_M0L7_2aSomeS1089)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1090;
      if (_M0L6_2acntS3544 > 1) {
        int32_t _M0L11_2anew__cntS3545 = _M0L6_2acntS3544 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1089)->rc = _M0L11_2anew__cntS3545;
        moonbit_incref(_M0L8_2afieldS3269.$0);
      } else if (_M0L6_2acntS3544 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1089);
      }
      _M0L12_2apkg__nameS1090 = _M0L8_2afieldS3269;
      if (_M0L6loggerS1091.$1) {
        moonbit_incref(_M0L6loggerS1091.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L12_2apkg__nameS1090);
      if (_M0L6loggerS1091.$1) {
        moonbit_incref(_M0L6loggerS1091.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1091.$0->$method_3(_M0L6loggerS1091.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1088);
      break;
    }
  }
  _M0L8_2afieldS3268
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1069->$1_1, _M0L4selfS1069->$1_2, _M0L4selfS1069->$1_0
  };
  _M0L8filenameS2755 = _M0L8_2afieldS3268;
  moonbit_incref(_M0L8filenameS2755.$0);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L8filenameS2755);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_3(_M0L6loggerS1091.$1, 58);
  _M0L8_2afieldS3267
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1069->$2_1, _M0L4selfS1069->$2_2, _M0L4selfS1069->$2_0
  };
  _M0L11start__lineS2756 = _M0L8_2afieldS3267;
  moonbit_incref(_M0L11start__lineS2756.$0);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L11start__lineS2756);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_3(_M0L6loggerS1091.$1, 58);
  _M0L8_2afieldS3266
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1069->$3_1, _M0L4selfS1069->$3_2, _M0L4selfS1069->$3_0
  };
  _M0L13start__columnS2757 = _M0L8_2afieldS3266;
  moonbit_incref(_M0L13start__columnS2757.$0);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L13start__columnS2757);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_3(_M0L6loggerS1091.$1, 45);
  _M0L8_2afieldS3265
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1069->$4_1, _M0L4selfS1069->$4_2, _M0L4selfS1069->$4_0
  };
  _M0L9end__lineS2758 = _M0L8_2afieldS3265;
  moonbit_incref(_M0L9end__lineS2758.$0);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L9end__lineS2758);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_3(_M0L6loggerS1091.$1, 58);
  _M0L8_2afieldS3264
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1069->$5_1, _M0L4selfS1069->$5_2, _M0L4selfS1069->$5_0
  };
  _M0L6_2acntS3546 = Moonbit_object_header(_M0L4selfS1069)->rc;
  if (_M0L6_2acntS3546 > 1) {
    int32_t _M0L11_2anew__cntS3552 = _M0L6_2acntS3546 - 1;
    Moonbit_object_header(_M0L4selfS1069)->rc = _M0L11_2anew__cntS3552;
    moonbit_incref(_M0L8_2afieldS3264.$0);
  } else if (_M0L6_2acntS3546 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3551 =
      (struct _M0TPC16string10StringView){_M0L4selfS1069->$4_1,
                                            _M0L4selfS1069->$4_2,
                                            _M0L4selfS1069->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3550;
    struct _M0TPC16string10StringView _M0L8_2afieldS3549;
    struct _M0TPC16string10StringView _M0L8_2afieldS3548;
    struct _M0TPC16string10StringView _M0L8_2afieldS3547;
    moonbit_decref(_M0L8_2afieldS3551.$0);
    _M0L8_2afieldS3550
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1069->$3_1, _M0L4selfS1069->$3_2, _M0L4selfS1069->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3550.$0);
    _M0L8_2afieldS3549
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1069->$2_1, _M0L4selfS1069->$2_2, _M0L4selfS1069->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3549.$0);
    _M0L8_2afieldS3548
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1069->$1_1, _M0L4selfS1069->$1_2, _M0L4selfS1069->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3548.$0);
    _M0L8_2afieldS3547
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1069->$0_1, _M0L4selfS1069->$0_2, _M0L4selfS1069->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3547.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1069);
  }
  _M0L11end__columnS2759 = _M0L8_2afieldS3264;
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L11end__columnS2759);
  if (_M0L6loggerS1091.$1) {
    moonbit_incref(_M0L6loggerS1091.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_3(_M0L6loggerS1091.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1091.$0->$method_2(_M0L6loggerS1091.$1, _M0L15_2amodule__nameS1087);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1067) {
  moonbit_string_t _M0L6_2atmpS2754;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2754
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1067);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2754);
  moonbit_decref(_M0L6_2atmpS2754);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1066,
  struct _M0TPB6Logger _M0L6loggerS1065
) {
  moonbit_string_t _M0L6_2atmpS2753;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2753 = _M0MPC16double6Double10to__string(_M0L4selfS1066);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1065.$0->$method_0(_M0L6loggerS1065.$1, _M0L6_2atmpS2753);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1064) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1064);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1051) {
  uint64_t _M0L4bitsS1052;
  uint64_t _M0L6_2atmpS2752;
  uint64_t _M0L6_2atmpS2751;
  int32_t _M0L8ieeeSignS1053;
  uint64_t _M0L12ieeeMantissaS1054;
  uint64_t _M0L6_2atmpS2750;
  uint64_t _M0L6_2atmpS2749;
  int32_t _M0L12ieeeExponentS1055;
  int32_t _if__result_3700;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1056;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1057;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2748;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1051 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_67.data;
  }
  _M0L4bitsS1052 = *(int64_t*)&_M0L3valS1051;
  _M0L6_2atmpS2752 = _M0L4bitsS1052 >> 63;
  _M0L6_2atmpS2751 = _M0L6_2atmpS2752 & 1ull;
  _M0L8ieeeSignS1053 = _M0L6_2atmpS2751 != 0ull;
  _M0L12ieeeMantissaS1054 = _M0L4bitsS1052 & 4503599627370495ull;
  _M0L6_2atmpS2750 = _M0L4bitsS1052 >> 52;
  _M0L6_2atmpS2749 = _M0L6_2atmpS2750 & 2047ull;
  _M0L12ieeeExponentS1055 = (int32_t)_M0L6_2atmpS2749;
  if (_M0L12ieeeExponentS1055 == 2047) {
    _if__result_3700 = 1;
  } else if (_M0L12ieeeExponentS1055 == 0) {
    _if__result_3700 = _M0L12ieeeMantissaS1054 == 0ull;
  } else {
    _if__result_3700 = 0;
  }
  if (_if__result_3700) {
    int32_t _M0L6_2atmpS2737 = _M0L12ieeeExponentS1055 != 0;
    int32_t _M0L6_2atmpS2738 = _M0L12ieeeMantissaS1054 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1053, _M0L6_2atmpS2737, _M0L6_2atmpS2738);
  }
  _M0Lm1vS1056 = _M0FPB31ryu__to__string_2erecord_2f1050;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1057
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1054, _M0L12ieeeExponentS1055);
  if (_M0L5smallS1057 == 0) {
    uint32_t _M0L6_2atmpS2739;
    if (_M0L5smallS1057) {
      moonbit_decref(_M0L5smallS1057);
    }
    _M0L6_2atmpS2739 = *(uint32_t*)&_M0L12ieeeExponentS1055;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1056 = _M0FPB3d2d(_M0L12ieeeMantissaS1054, _M0L6_2atmpS2739);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1058 = _M0L5smallS1057;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1059 = _M0L7_2aSomeS1058;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1060 = _M0L4_2afS1059;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2747 = _M0Lm1xS1060;
      uint64_t _M0L8_2afieldS3275 = _M0L6_2atmpS2747->$0;
      uint64_t _M0L8mantissaS2746 = _M0L8_2afieldS3275;
      uint64_t _M0L1qS1061 = _M0L8mantissaS2746 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2745 = _M0Lm1xS1060;
      uint64_t _M0L8_2afieldS3274 = _M0L6_2atmpS2745->$0;
      uint64_t _M0L8mantissaS2743 = _M0L8_2afieldS3274;
      uint64_t _M0L6_2atmpS2744 = 10ull * _M0L1qS1061;
      uint64_t _M0L1rS1062 = _M0L8mantissaS2743 - _M0L6_2atmpS2744;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2742;
      int32_t _M0L8_2afieldS3273;
      int32_t _M0L8exponentS2741;
      int32_t _M0L6_2atmpS2740;
      if (_M0L1rS1062 != 0ull) {
        break;
      }
      _M0L6_2atmpS2742 = _M0Lm1xS1060;
      _M0L8_2afieldS3273 = _M0L6_2atmpS2742->$1;
      moonbit_decref(_M0L6_2atmpS2742);
      _M0L8exponentS2741 = _M0L8_2afieldS3273;
      _M0L6_2atmpS2740 = _M0L8exponentS2741 + 1;
      _M0Lm1xS1060
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1060)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1060->$0 = _M0L1qS1061;
      _M0Lm1xS1060->$1 = _M0L6_2atmpS2740;
      continue;
      break;
    }
    _M0Lm1vS1056 = _M0Lm1xS1060;
  }
  _M0L6_2atmpS2748 = _M0Lm1vS1056;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2748, _M0L8ieeeSignS1053);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1045,
  int32_t _M0L12ieeeExponentS1047
) {
  uint64_t _M0L2m2S1044;
  int32_t _M0L6_2atmpS2736;
  int32_t _M0L2e2S1046;
  int32_t _M0L6_2atmpS2735;
  uint64_t _M0L6_2atmpS2734;
  uint64_t _M0L4maskS1048;
  uint64_t _M0L8fractionS1049;
  int32_t _M0L6_2atmpS2733;
  uint64_t _M0L6_2atmpS2732;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2731;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1044 = 4503599627370496ull | _M0L12ieeeMantissaS1045;
  _M0L6_2atmpS2736 = _M0L12ieeeExponentS1047 - 1023;
  _M0L2e2S1046 = _M0L6_2atmpS2736 - 52;
  if (_M0L2e2S1046 > 0) {
    return 0;
  }
  if (_M0L2e2S1046 < -52) {
    return 0;
  }
  _M0L6_2atmpS2735 = -_M0L2e2S1046;
  _M0L6_2atmpS2734 = 1ull << (_M0L6_2atmpS2735 & 63);
  _M0L4maskS1048 = _M0L6_2atmpS2734 - 1ull;
  _M0L8fractionS1049 = _M0L2m2S1044 & _M0L4maskS1048;
  if (_M0L8fractionS1049 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2733 = -_M0L2e2S1046;
  _M0L6_2atmpS2732 = _M0L2m2S1044 >> (_M0L6_2atmpS2733 & 63);
  _M0L6_2atmpS2731
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2731)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2731->$0 = _M0L6_2atmpS2732;
  _M0L6_2atmpS2731->$1 = 0;
  return _M0L6_2atmpS2731;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1018,
  int32_t _M0L4signS1016
) {
  int32_t _M0L6_2atmpS2730;
  moonbit_bytes_t _M0L6resultS1014;
  int32_t _M0Lm5indexS1015;
  uint64_t _M0Lm6outputS1017;
  uint64_t _M0L6_2atmpS2729;
  int32_t _M0L7olengthS1019;
  int32_t _M0L8_2afieldS3276;
  int32_t _M0L8exponentS2728;
  int32_t _M0L6_2atmpS2727;
  int32_t _M0Lm3expS1020;
  int32_t _M0L6_2atmpS2726;
  int32_t _M0L6_2atmpS2724;
  int32_t _M0L18scientificNotationS1021;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2730 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1014
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2730);
  _M0Lm5indexS1015 = 0;
  if (_M0L4signS1016) {
    int32_t _M0L6_2atmpS2599 = _M0Lm5indexS1015;
    int32_t _M0L6_2atmpS2600;
    if (
      _M0L6_2atmpS2599 < 0
      || _M0L6_2atmpS2599 >= Moonbit_array_length(_M0L6resultS1014)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1014[_M0L6_2atmpS2599] = 45;
    _M0L6_2atmpS2600 = _M0Lm5indexS1015;
    _M0Lm5indexS1015 = _M0L6_2atmpS2600 + 1;
  }
  _M0Lm6outputS1017 = _M0L1vS1018->$0;
  _M0L6_2atmpS2729 = _M0Lm6outputS1017;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1019 = _M0FPB17decimal__length17(_M0L6_2atmpS2729);
  _M0L8_2afieldS3276 = _M0L1vS1018->$1;
  moonbit_decref(_M0L1vS1018);
  _M0L8exponentS2728 = _M0L8_2afieldS3276;
  _M0L6_2atmpS2727 = _M0L8exponentS2728 + _M0L7olengthS1019;
  _M0Lm3expS1020 = _M0L6_2atmpS2727 - 1;
  _M0L6_2atmpS2726 = _M0Lm3expS1020;
  if (_M0L6_2atmpS2726 >= -6) {
    int32_t _M0L6_2atmpS2725 = _M0Lm3expS1020;
    _M0L6_2atmpS2724 = _M0L6_2atmpS2725 < 21;
  } else {
    _M0L6_2atmpS2724 = 0;
  }
  _M0L18scientificNotationS1021 = !_M0L6_2atmpS2724;
  if (_M0L18scientificNotationS1021) {
    int32_t _M0L7_2abindS1022 = _M0L7olengthS1019 - 1;
    int32_t _M0L1iS1023 = 0;
    int32_t _M0L6_2atmpS2610;
    uint64_t _M0L6_2atmpS2615;
    int32_t _M0L6_2atmpS2614;
    int32_t _M0L6_2atmpS2613;
    int32_t _M0L6_2atmpS2612;
    int32_t _M0L6_2atmpS2611;
    int32_t _M0L6_2atmpS2619;
    int32_t _M0L6_2atmpS2620;
    int32_t _M0L6_2atmpS2621;
    int32_t _M0L6_2atmpS2622;
    int32_t _M0L6_2atmpS2623;
    int32_t _M0L6_2atmpS2629;
    int32_t _M0L6_2atmpS2662;
    while (1) {
      if (_M0L1iS1023 < _M0L7_2abindS1022) {
        uint64_t _M0L6_2atmpS2608 = _M0Lm6outputS1017;
        uint64_t _M0L1cS1024 = _M0L6_2atmpS2608 % 10ull;
        uint64_t _M0L6_2atmpS2601 = _M0Lm6outputS1017;
        int32_t _M0L6_2atmpS2607;
        int32_t _M0L6_2atmpS2606;
        int32_t _M0L6_2atmpS2602;
        int32_t _M0L6_2atmpS2605;
        int32_t _M0L6_2atmpS2604;
        int32_t _M0L6_2atmpS2603;
        int32_t _M0L6_2atmpS2609;
        _M0Lm6outputS1017 = _M0L6_2atmpS2601 / 10ull;
        _M0L6_2atmpS2607 = _M0Lm5indexS1015;
        _M0L6_2atmpS2606 = _M0L6_2atmpS2607 + _M0L7olengthS1019;
        _M0L6_2atmpS2602 = _M0L6_2atmpS2606 - _M0L1iS1023;
        _M0L6_2atmpS2605 = (int32_t)_M0L1cS1024;
        _M0L6_2atmpS2604 = 48 + _M0L6_2atmpS2605;
        _M0L6_2atmpS2603 = _M0L6_2atmpS2604 & 0xff;
        if (
          _M0L6_2atmpS2602 < 0
          || _M0L6_2atmpS2602 >= Moonbit_array_length(_M0L6resultS1014)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1014[_M0L6_2atmpS2602] = _M0L6_2atmpS2603;
        _M0L6_2atmpS2609 = _M0L1iS1023 + 1;
        _M0L1iS1023 = _M0L6_2atmpS2609;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2610 = _M0Lm5indexS1015;
    _M0L6_2atmpS2615 = _M0Lm6outputS1017;
    _M0L6_2atmpS2614 = (int32_t)_M0L6_2atmpS2615;
    _M0L6_2atmpS2613 = _M0L6_2atmpS2614 % 10;
    _M0L6_2atmpS2612 = 48 + _M0L6_2atmpS2613;
    _M0L6_2atmpS2611 = _M0L6_2atmpS2612 & 0xff;
    if (
      _M0L6_2atmpS2610 < 0
      || _M0L6_2atmpS2610 >= Moonbit_array_length(_M0L6resultS1014)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1014[_M0L6_2atmpS2610] = _M0L6_2atmpS2611;
    if (_M0L7olengthS1019 > 1) {
      int32_t _M0L6_2atmpS2617 = _M0Lm5indexS1015;
      int32_t _M0L6_2atmpS2616 = _M0L6_2atmpS2617 + 1;
      if (
        _M0L6_2atmpS2616 < 0
        || _M0L6_2atmpS2616 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2616] = 46;
    } else {
      int32_t _M0L6_2atmpS2618 = _M0Lm5indexS1015;
      _M0Lm5indexS1015 = _M0L6_2atmpS2618 - 1;
    }
    _M0L6_2atmpS2619 = _M0Lm5indexS1015;
    _M0L6_2atmpS2620 = _M0L7olengthS1019 + 1;
    _M0Lm5indexS1015 = _M0L6_2atmpS2619 + _M0L6_2atmpS2620;
    _M0L6_2atmpS2621 = _M0Lm5indexS1015;
    if (
      _M0L6_2atmpS2621 < 0
      || _M0L6_2atmpS2621 >= Moonbit_array_length(_M0L6resultS1014)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1014[_M0L6_2atmpS2621] = 101;
    _M0L6_2atmpS2622 = _M0Lm5indexS1015;
    _M0Lm5indexS1015 = _M0L6_2atmpS2622 + 1;
    _M0L6_2atmpS2623 = _M0Lm3expS1020;
    if (_M0L6_2atmpS2623 < 0) {
      int32_t _M0L6_2atmpS2624 = _M0Lm5indexS1015;
      int32_t _M0L6_2atmpS2625;
      int32_t _M0L6_2atmpS2626;
      if (
        _M0L6_2atmpS2624 < 0
        || _M0L6_2atmpS2624 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2624] = 45;
      _M0L6_2atmpS2625 = _M0Lm5indexS1015;
      _M0Lm5indexS1015 = _M0L6_2atmpS2625 + 1;
      _M0L6_2atmpS2626 = _M0Lm3expS1020;
      _M0Lm3expS1020 = -_M0L6_2atmpS2626;
    } else {
      int32_t _M0L6_2atmpS2627 = _M0Lm5indexS1015;
      int32_t _M0L6_2atmpS2628;
      if (
        _M0L6_2atmpS2627 < 0
        || _M0L6_2atmpS2627 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2627] = 43;
      _M0L6_2atmpS2628 = _M0Lm5indexS1015;
      _M0Lm5indexS1015 = _M0L6_2atmpS2628 + 1;
    }
    _M0L6_2atmpS2629 = _M0Lm3expS1020;
    if (_M0L6_2atmpS2629 >= 100) {
      int32_t _M0L6_2atmpS2645 = _M0Lm3expS1020;
      int32_t _M0L1aS1026 = _M0L6_2atmpS2645 / 100;
      int32_t _M0L6_2atmpS2644 = _M0Lm3expS1020;
      int32_t _M0L6_2atmpS2643 = _M0L6_2atmpS2644 / 10;
      int32_t _M0L1bS1027 = _M0L6_2atmpS2643 % 10;
      int32_t _M0L6_2atmpS2642 = _M0Lm3expS1020;
      int32_t _M0L1cS1028 = _M0L6_2atmpS2642 % 10;
      int32_t _M0L6_2atmpS2630 = _M0Lm5indexS1015;
      int32_t _M0L6_2atmpS2632 = 48 + _M0L1aS1026;
      int32_t _M0L6_2atmpS2631 = _M0L6_2atmpS2632 & 0xff;
      int32_t _M0L6_2atmpS2636;
      int32_t _M0L6_2atmpS2633;
      int32_t _M0L6_2atmpS2635;
      int32_t _M0L6_2atmpS2634;
      int32_t _M0L6_2atmpS2640;
      int32_t _M0L6_2atmpS2637;
      int32_t _M0L6_2atmpS2639;
      int32_t _M0L6_2atmpS2638;
      int32_t _M0L6_2atmpS2641;
      if (
        _M0L6_2atmpS2630 < 0
        || _M0L6_2atmpS2630 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2630] = _M0L6_2atmpS2631;
      _M0L6_2atmpS2636 = _M0Lm5indexS1015;
      _M0L6_2atmpS2633 = _M0L6_2atmpS2636 + 1;
      _M0L6_2atmpS2635 = 48 + _M0L1bS1027;
      _M0L6_2atmpS2634 = _M0L6_2atmpS2635 & 0xff;
      if (
        _M0L6_2atmpS2633 < 0
        || _M0L6_2atmpS2633 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2633] = _M0L6_2atmpS2634;
      _M0L6_2atmpS2640 = _M0Lm5indexS1015;
      _M0L6_2atmpS2637 = _M0L6_2atmpS2640 + 2;
      _M0L6_2atmpS2639 = 48 + _M0L1cS1028;
      _M0L6_2atmpS2638 = _M0L6_2atmpS2639 & 0xff;
      if (
        _M0L6_2atmpS2637 < 0
        || _M0L6_2atmpS2637 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2637] = _M0L6_2atmpS2638;
      _M0L6_2atmpS2641 = _M0Lm5indexS1015;
      _M0Lm5indexS1015 = _M0L6_2atmpS2641 + 3;
    } else {
      int32_t _M0L6_2atmpS2646 = _M0Lm3expS1020;
      if (_M0L6_2atmpS2646 >= 10) {
        int32_t _M0L6_2atmpS2656 = _M0Lm3expS1020;
        int32_t _M0L1aS1029 = _M0L6_2atmpS2656 / 10;
        int32_t _M0L6_2atmpS2655 = _M0Lm3expS1020;
        int32_t _M0L1bS1030 = _M0L6_2atmpS2655 % 10;
        int32_t _M0L6_2atmpS2647 = _M0Lm5indexS1015;
        int32_t _M0L6_2atmpS2649 = 48 + _M0L1aS1029;
        int32_t _M0L6_2atmpS2648 = _M0L6_2atmpS2649 & 0xff;
        int32_t _M0L6_2atmpS2653;
        int32_t _M0L6_2atmpS2650;
        int32_t _M0L6_2atmpS2652;
        int32_t _M0L6_2atmpS2651;
        int32_t _M0L6_2atmpS2654;
        if (
          _M0L6_2atmpS2647 < 0
          || _M0L6_2atmpS2647 >= Moonbit_array_length(_M0L6resultS1014)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1014[_M0L6_2atmpS2647] = _M0L6_2atmpS2648;
        _M0L6_2atmpS2653 = _M0Lm5indexS1015;
        _M0L6_2atmpS2650 = _M0L6_2atmpS2653 + 1;
        _M0L6_2atmpS2652 = 48 + _M0L1bS1030;
        _M0L6_2atmpS2651 = _M0L6_2atmpS2652 & 0xff;
        if (
          _M0L6_2atmpS2650 < 0
          || _M0L6_2atmpS2650 >= Moonbit_array_length(_M0L6resultS1014)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1014[_M0L6_2atmpS2650] = _M0L6_2atmpS2651;
        _M0L6_2atmpS2654 = _M0Lm5indexS1015;
        _M0Lm5indexS1015 = _M0L6_2atmpS2654 + 2;
      } else {
        int32_t _M0L6_2atmpS2657 = _M0Lm5indexS1015;
        int32_t _M0L6_2atmpS2660 = _M0Lm3expS1020;
        int32_t _M0L6_2atmpS2659 = 48 + _M0L6_2atmpS2660;
        int32_t _M0L6_2atmpS2658 = _M0L6_2atmpS2659 & 0xff;
        int32_t _M0L6_2atmpS2661;
        if (
          _M0L6_2atmpS2657 < 0
          || _M0L6_2atmpS2657 >= Moonbit_array_length(_M0L6resultS1014)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1014[_M0L6_2atmpS2657] = _M0L6_2atmpS2658;
        _M0L6_2atmpS2661 = _M0Lm5indexS1015;
        _M0Lm5indexS1015 = _M0L6_2atmpS2661 + 1;
      }
    }
    _M0L6_2atmpS2662 = _M0Lm5indexS1015;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1014, 0, _M0L6_2atmpS2662);
  } else {
    int32_t _M0L6_2atmpS2663 = _M0Lm3expS1020;
    int32_t _M0L6_2atmpS2723;
    if (_M0L6_2atmpS2663 < 0) {
      int32_t _M0L6_2atmpS2664 = _M0Lm5indexS1015;
      int32_t _M0L6_2atmpS2665;
      int32_t _M0L6_2atmpS2666;
      int32_t _M0L6_2atmpS2667;
      int32_t _M0L1iS1031;
      int32_t _M0L7currentS1033;
      int32_t _M0L1iS1034;
      if (
        _M0L6_2atmpS2664 < 0
        || _M0L6_2atmpS2664 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2664] = 48;
      _M0L6_2atmpS2665 = _M0Lm5indexS1015;
      _M0Lm5indexS1015 = _M0L6_2atmpS2665 + 1;
      _M0L6_2atmpS2666 = _M0Lm5indexS1015;
      if (
        _M0L6_2atmpS2666 < 0
        || _M0L6_2atmpS2666 >= Moonbit_array_length(_M0L6resultS1014)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1014[_M0L6_2atmpS2666] = 46;
      _M0L6_2atmpS2667 = _M0Lm5indexS1015;
      _M0Lm5indexS1015 = _M0L6_2atmpS2667 + 1;
      _M0L1iS1031 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2668 = _M0Lm3expS1020;
        if (_M0L1iS1031 > _M0L6_2atmpS2668) {
          int32_t _M0L6_2atmpS2669 = _M0Lm5indexS1015;
          int32_t _M0L6_2atmpS2670;
          int32_t _M0L6_2atmpS2671;
          if (
            _M0L6_2atmpS2669 < 0
            || _M0L6_2atmpS2669 >= Moonbit_array_length(_M0L6resultS1014)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1014[_M0L6_2atmpS2669] = 48;
          _M0L6_2atmpS2670 = _M0Lm5indexS1015;
          _M0Lm5indexS1015 = _M0L6_2atmpS2670 + 1;
          _M0L6_2atmpS2671 = _M0L1iS1031 - 1;
          _M0L1iS1031 = _M0L6_2atmpS2671;
          continue;
        }
        break;
      }
      _M0L7currentS1033 = _M0Lm5indexS1015;
      _M0L1iS1034 = 0;
      while (1) {
        if (_M0L1iS1034 < _M0L7olengthS1019) {
          int32_t _M0L6_2atmpS2679 = _M0L7currentS1033 + _M0L7olengthS1019;
          int32_t _M0L6_2atmpS2678 = _M0L6_2atmpS2679 - _M0L1iS1034;
          int32_t _M0L6_2atmpS2672 = _M0L6_2atmpS2678 - 1;
          uint64_t _M0L6_2atmpS2677 = _M0Lm6outputS1017;
          uint64_t _M0L6_2atmpS2676 = _M0L6_2atmpS2677 % 10ull;
          int32_t _M0L6_2atmpS2675 = (int32_t)_M0L6_2atmpS2676;
          int32_t _M0L6_2atmpS2674 = 48 + _M0L6_2atmpS2675;
          int32_t _M0L6_2atmpS2673 = _M0L6_2atmpS2674 & 0xff;
          uint64_t _M0L6_2atmpS2680;
          int32_t _M0L6_2atmpS2681;
          int32_t _M0L6_2atmpS2682;
          if (
            _M0L6_2atmpS2672 < 0
            || _M0L6_2atmpS2672 >= Moonbit_array_length(_M0L6resultS1014)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1014[_M0L6_2atmpS2672] = _M0L6_2atmpS2673;
          _M0L6_2atmpS2680 = _M0Lm6outputS1017;
          _M0Lm6outputS1017 = _M0L6_2atmpS2680 / 10ull;
          _M0L6_2atmpS2681 = _M0Lm5indexS1015;
          _M0Lm5indexS1015 = _M0L6_2atmpS2681 + 1;
          _M0L6_2atmpS2682 = _M0L1iS1034 + 1;
          _M0L1iS1034 = _M0L6_2atmpS2682;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2684 = _M0Lm3expS1020;
      int32_t _M0L6_2atmpS2683 = _M0L6_2atmpS2684 + 1;
      if (_M0L6_2atmpS2683 >= _M0L7olengthS1019) {
        int32_t _M0L1iS1036 = 0;
        int32_t _M0L6_2atmpS2696;
        int32_t _M0L6_2atmpS2700;
        int32_t _M0L7_2abindS1038;
        int32_t _M0L2__S1039;
        while (1) {
          if (_M0L1iS1036 < _M0L7olengthS1019) {
            int32_t _M0L6_2atmpS2693 = _M0Lm5indexS1015;
            int32_t _M0L6_2atmpS2692 = _M0L6_2atmpS2693 + _M0L7olengthS1019;
            int32_t _M0L6_2atmpS2691 = _M0L6_2atmpS2692 - _M0L1iS1036;
            int32_t _M0L6_2atmpS2685 = _M0L6_2atmpS2691 - 1;
            uint64_t _M0L6_2atmpS2690 = _M0Lm6outputS1017;
            uint64_t _M0L6_2atmpS2689 = _M0L6_2atmpS2690 % 10ull;
            int32_t _M0L6_2atmpS2688 = (int32_t)_M0L6_2atmpS2689;
            int32_t _M0L6_2atmpS2687 = 48 + _M0L6_2atmpS2688;
            int32_t _M0L6_2atmpS2686 = _M0L6_2atmpS2687 & 0xff;
            uint64_t _M0L6_2atmpS2694;
            int32_t _M0L6_2atmpS2695;
            if (
              _M0L6_2atmpS2685 < 0
              || _M0L6_2atmpS2685 >= Moonbit_array_length(_M0L6resultS1014)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1014[_M0L6_2atmpS2685] = _M0L6_2atmpS2686;
            _M0L6_2atmpS2694 = _M0Lm6outputS1017;
            _M0Lm6outputS1017 = _M0L6_2atmpS2694 / 10ull;
            _M0L6_2atmpS2695 = _M0L1iS1036 + 1;
            _M0L1iS1036 = _M0L6_2atmpS2695;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2696 = _M0Lm5indexS1015;
        _M0Lm5indexS1015 = _M0L6_2atmpS2696 + _M0L7olengthS1019;
        _M0L6_2atmpS2700 = _M0Lm3expS1020;
        _M0L7_2abindS1038 = _M0L6_2atmpS2700 + 1;
        _M0L2__S1039 = _M0L7olengthS1019;
        while (1) {
          if (_M0L2__S1039 < _M0L7_2abindS1038) {
            int32_t _M0L6_2atmpS2697 = _M0Lm5indexS1015;
            int32_t _M0L6_2atmpS2698;
            int32_t _M0L6_2atmpS2699;
            if (
              _M0L6_2atmpS2697 < 0
              || _M0L6_2atmpS2697 >= Moonbit_array_length(_M0L6resultS1014)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1014[_M0L6_2atmpS2697] = 48;
            _M0L6_2atmpS2698 = _M0Lm5indexS1015;
            _M0Lm5indexS1015 = _M0L6_2atmpS2698 + 1;
            _M0L6_2atmpS2699 = _M0L2__S1039 + 1;
            _M0L2__S1039 = _M0L6_2atmpS2699;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2722 = _M0Lm5indexS1015;
        int32_t _M0Lm7currentS1041 = _M0L6_2atmpS2722 + 1;
        int32_t _M0L1iS1042 = 0;
        int32_t _M0L6_2atmpS2720;
        int32_t _M0L6_2atmpS2721;
        while (1) {
          if (_M0L1iS1042 < _M0L7olengthS1019) {
            int32_t _M0L6_2atmpS2703 = _M0L7olengthS1019 - _M0L1iS1042;
            int32_t _M0L6_2atmpS2701 = _M0L6_2atmpS2703 - 1;
            int32_t _M0L6_2atmpS2702 = _M0Lm3expS1020;
            int32_t _M0L6_2atmpS2717;
            int32_t _M0L6_2atmpS2716;
            int32_t _M0L6_2atmpS2715;
            int32_t _M0L6_2atmpS2709;
            uint64_t _M0L6_2atmpS2714;
            uint64_t _M0L6_2atmpS2713;
            int32_t _M0L6_2atmpS2712;
            int32_t _M0L6_2atmpS2711;
            int32_t _M0L6_2atmpS2710;
            uint64_t _M0L6_2atmpS2718;
            int32_t _M0L6_2atmpS2719;
            if (_M0L6_2atmpS2701 == _M0L6_2atmpS2702) {
              int32_t _M0L6_2atmpS2707 = _M0Lm7currentS1041;
              int32_t _M0L6_2atmpS2706 = _M0L6_2atmpS2707 + _M0L7olengthS1019;
              int32_t _M0L6_2atmpS2705 = _M0L6_2atmpS2706 - _M0L1iS1042;
              int32_t _M0L6_2atmpS2704 = _M0L6_2atmpS2705 - 1;
              int32_t _M0L6_2atmpS2708;
              if (
                _M0L6_2atmpS2704 < 0
                || _M0L6_2atmpS2704 >= Moonbit_array_length(_M0L6resultS1014)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1014[_M0L6_2atmpS2704] = 46;
              _M0L6_2atmpS2708 = _M0Lm7currentS1041;
              _M0Lm7currentS1041 = _M0L6_2atmpS2708 - 1;
            }
            _M0L6_2atmpS2717 = _M0Lm7currentS1041;
            _M0L6_2atmpS2716 = _M0L6_2atmpS2717 + _M0L7olengthS1019;
            _M0L6_2atmpS2715 = _M0L6_2atmpS2716 - _M0L1iS1042;
            _M0L6_2atmpS2709 = _M0L6_2atmpS2715 - 1;
            _M0L6_2atmpS2714 = _M0Lm6outputS1017;
            _M0L6_2atmpS2713 = _M0L6_2atmpS2714 % 10ull;
            _M0L6_2atmpS2712 = (int32_t)_M0L6_2atmpS2713;
            _M0L6_2atmpS2711 = 48 + _M0L6_2atmpS2712;
            _M0L6_2atmpS2710 = _M0L6_2atmpS2711 & 0xff;
            if (
              _M0L6_2atmpS2709 < 0
              || _M0L6_2atmpS2709 >= Moonbit_array_length(_M0L6resultS1014)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1014[_M0L6_2atmpS2709] = _M0L6_2atmpS2710;
            _M0L6_2atmpS2718 = _M0Lm6outputS1017;
            _M0Lm6outputS1017 = _M0L6_2atmpS2718 / 10ull;
            _M0L6_2atmpS2719 = _M0L1iS1042 + 1;
            _M0L1iS1042 = _M0L6_2atmpS2719;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2720 = _M0Lm5indexS1015;
        _M0L6_2atmpS2721 = _M0L7olengthS1019 + 1;
        _M0Lm5indexS1015 = _M0L6_2atmpS2720 + _M0L6_2atmpS2721;
      }
    }
    _M0L6_2atmpS2723 = _M0Lm5indexS1015;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1014, 0, _M0L6_2atmpS2723);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS960,
  uint32_t _M0L12ieeeExponentS959
) {
  int32_t _M0Lm2e2S957;
  uint64_t _M0Lm2m2S958;
  uint64_t _M0L6_2atmpS2598;
  uint64_t _M0L6_2atmpS2597;
  int32_t _M0L4evenS961;
  uint64_t _M0L6_2atmpS2596;
  uint64_t _M0L2mvS962;
  int32_t _M0L7mmShiftS963;
  uint64_t _M0Lm2vrS964;
  uint64_t _M0Lm2vpS965;
  uint64_t _M0Lm2vmS966;
  int32_t _M0Lm3e10S967;
  int32_t _M0Lm17vmIsTrailingZerosS968;
  int32_t _M0Lm17vrIsTrailingZerosS969;
  int32_t _M0L6_2atmpS2498;
  int32_t _M0Lm7removedS988;
  int32_t _M0Lm16lastRemovedDigitS989;
  uint64_t _M0Lm6outputS990;
  int32_t _M0L6_2atmpS2594;
  int32_t _M0L6_2atmpS2595;
  int32_t _M0L3expS1013;
  uint64_t _M0L6_2atmpS2593;
  struct _M0TPB17FloatingDecimal64* _block_3713;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S957 = 0;
  _M0Lm2m2S958 = 0ull;
  if (_M0L12ieeeExponentS959 == 0u) {
    _M0Lm2e2S957 = -1076;
    _M0Lm2m2S958 = _M0L12ieeeMantissaS960;
  } else {
    int32_t _M0L6_2atmpS2497 = *(int32_t*)&_M0L12ieeeExponentS959;
    int32_t _M0L6_2atmpS2496 = _M0L6_2atmpS2497 - 1023;
    int32_t _M0L6_2atmpS2495 = _M0L6_2atmpS2496 - 52;
    _M0Lm2e2S957 = _M0L6_2atmpS2495 - 2;
    _M0Lm2m2S958 = 4503599627370496ull | _M0L12ieeeMantissaS960;
  }
  _M0L6_2atmpS2598 = _M0Lm2m2S958;
  _M0L6_2atmpS2597 = _M0L6_2atmpS2598 & 1ull;
  _M0L4evenS961 = _M0L6_2atmpS2597 == 0ull;
  _M0L6_2atmpS2596 = _M0Lm2m2S958;
  _M0L2mvS962 = 4ull * _M0L6_2atmpS2596;
  if (_M0L12ieeeMantissaS960 != 0ull) {
    _M0L7mmShiftS963 = 1;
  } else {
    _M0L7mmShiftS963 = _M0L12ieeeExponentS959 <= 1u;
  }
  _M0Lm2vrS964 = 0ull;
  _M0Lm2vpS965 = 0ull;
  _M0Lm2vmS966 = 0ull;
  _M0Lm3e10S967 = 0;
  _M0Lm17vmIsTrailingZerosS968 = 0;
  _M0Lm17vrIsTrailingZerosS969 = 0;
  _M0L6_2atmpS2498 = _M0Lm2e2S957;
  if (_M0L6_2atmpS2498 >= 0) {
    int32_t _M0L6_2atmpS2520 = _M0Lm2e2S957;
    int32_t _M0L6_2atmpS2516;
    int32_t _M0L6_2atmpS2519;
    int32_t _M0L6_2atmpS2518;
    int32_t _M0L6_2atmpS2517;
    int32_t _M0L1qS970;
    int32_t _M0L6_2atmpS2515;
    int32_t _M0L6_2atmpS2514;
    int32_t _M0L1kS971;
    int32_t _M0L6_2atmpS2513;
    int32_t _M0L6_2atmpS2512;
    int32_t _M0L6_2atmpS2511;
    int32_t _M0L1iS972;
    struct _M0TPB8Pow5Pair _M0L4pow5S973;
    uint64_t _M0L6_2atmpS2510;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS974;
    uint64_t _M0L8_2avrOutS975;
    uint64_t _M0L8_2avpOutS976;
    uint64_t _M0L8_2avmOutS977;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2516 = _M0FPB9log10Pow2(_M0L6_2atmpS2520);
    _M0L6_2atmpS2519 = _M0Lm2e2S957;
    _M0L6_2atmpS2518 = _M0L6_2atmpS2519 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2517 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2518);
    _M0L1qS970 = _M0L6_2atmpS2516 - _M0L6_2atmpS2517;
    _M0Lm3e10S967 = _M0L1qS970;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2515 = _M0FPB8pow5bits(_M0L1qS970);
    _M0L6_2atmpS2514 = 125 + _M0L6_2atmpS2515;
    _M0L1kS971 = _M0L6_2atmpS2514 - 1;
    _M0L6_2atmpS2513 = _M0Lm2e2S957;
    _M0L6_2atmpS2512 = -_M0L6_2atmpS2513;
    _M0L6_2atmpS2511 = _M0L6_2atmpS2512 + _M0L1qS970;
    _M0L1iS972 = _M0L6_2atmpS2511 + _M0L1kS971;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S973 = _M0FPB22double__computeInvPow5(_M0L1qS970);
    _M0L6_2atmpS2510 = _M0Lm2m2S958;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS974
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2510, _M0L4pow5S973, _M0L1iS972, _M0L7mmShiftS963);
    _M0L8_2avrOutS975 = _M0L7_2abindS974.$0;
    _M0L8_2avpOutS976 = _M0L7_2abindS974.$1;
    _M0L8_2avmOutS977 = _M0L7_2abindS974.$2;
    _M0Lm2vrS964 = _M0L8_2avrOutS975;
    _M0Lm2vpS965 = _M0L8_2avpOutS976;
    _M0Lm2vmS966 = _M0L8_2avmOutS977;
    if (_M0L1qS970 <= 21) {
      int32_t _M0L6_2atmpS2506 = (int32_t)_M0L2mvS962;
      uint64_t _M0L6_2atmpS2509 = _M0L2mvS962 / 5ull;
      int32_t _M0L6_2atmpS2508 = (int32_t)_M0L6_2atmpS2509;
      int32_t _M0L6_2atmpS2507 = 5 * _M0L6_2atmpS2508;
      int32_t _M0L6mvMod5S978 = _M0L6_2atmpS2506 - _M0L6_2atmpS2507;
      if (_M0L6mvMod5S978 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS969
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS962, _M0L1qS970);
      } else if (_M0L4evenS961) {
        uint64_t _M0L6_2atmpS2500 = _M0L2mvS962 - 1ull;
        uint64_t _M0L6_2atmpS2501;
        uint64_t _M0L6_2atmpS2499;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2501 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS963);
        _M0L6_2atmpS2499 = _M0L6_2atmpS2500 - _M0L6_2atmpS2501;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS968
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2499, _M0L1qS970);
      } else {
        uint64_t _M0L6_2atmpS2502 = _M0Lm2vpS965;
        uint64_t _M0L6_2atmpS2505 = _M0L2mvS962 + 2ull;
        int32_t _M0L6_2atmpS2504;
        uint64_t _M0L6_2atmpS2503;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2504
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2505, _M0L1qS970);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2503 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2504);
        _M0Lm2vpS965 = _M0L6_2atmpS2502 - _M0L6_2atmpS2503;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2534 = _M0Lm2e2S957;
    int32_t _M0L6_2atmpS2533 = -_M0L6_2atmpS2534;
    int32_t _M0L6_2atmpS2528;
    int32_t _M0L6_2atmpS2532;
    int32_t _M0L6_2atmpS2531;
    int32_t _M0L6_2atmpS2530;
    int32_t _M0L6_2atmpS2529;
    int32_t _M0L1qS979;
    int32_t _M0L6_2atmpS2521;
    int32_t _M0L6_2atmpS2527;
    int32_t _M0L6_2atmpS2526;
    int32_t _M0L1iS980;
    int32_t _M0L6_2atmpS2525;
    int32_t _M0L1kS981;
    int32_t _M0L1jS982;
    struct _M0TPB8Pow5Pair _M0L4pow5S983;
    uint64_t _M0L6_2atmpS2524;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS984;
    uint64_t _M0L8_2avrOutS985;
    uint64_t _M0L8_2avpOutS986;
    uint64_t _M0L8_2avmOutS987;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2528 = _M0FPB9log10Pow5(_M0L6_2atmpS2533);
    _M0L6_2atmpS2532 = _M0Lm2e2S957;
    _M0L6_2atmpS2531 = -_M0L6_2atmpS2532;
    _M0L6_2atmpS2530 = _M0L6_2atmpS2531 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2529 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2530);
    _M0L1qS979 = _M0L6_2atmpS2528 - _M0L6_2atmpS2529;
    _M0L6_2atmpS2521 = _M0Lm2e2S957;
    _M0Lm3e10S967 = _M0L1qS979 + _M0L6_2atmpS2521;
    _M0L6_2atmpS2527 = _M0Lm2e2S957;
    _M0L6_2atmpS2526 = -_M0L6_2atmpS2527;
    _M0L1iS980 = _M0L6_2atmpS2526 - _M0L1qS979;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2525 = _M0FPB8pow5bits(_M0L1iS980);
    _M0L1kS981 = _M0L6_2atmpS2525 - 125;
    _M0L1jS982 = _M0L1qS979 - _M0L1kS981;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S983 = _M0FPB19double__computePow5(_M0L1iS980);
    _M0L6_2atmpS2524 = _M0Lm2m2S958;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS984
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2524, _M0L4pow5S983, _M0L1jS982, _M0L7mmShiftS963);
    _M0L8_2avrOutS985 = _M0L7_2abindS984.$0;
    _M0L8_2avpOutS986 = _M0L7_2abindS984.$1;
    _M0L8_2avmOutS987 = _M0L7_2abindS984.$2;
    _M0Lm2vrS964 = _M0L8_2avrOutS985;
    _M0Lm2vpS965 = _M0L8_2avpOutS986;
    _M0Lm2vmS966 = _M0L8_2avmOutS987;
    if (_M0L1qS979 <= 1) {
      _M0Lm17vrIsTrailingZerosS969 = 1;
      if (_M0L4evenS961) {
        int32_t _M0L6_2atmpS2522;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2522 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS963);
        _M0Lm17vmIsTrailingZerosS968 = _M0L6_2atmpS2522 == 1;
      } else {
        uint64_t _M0L6_2atmpS2523 = _M0Lm2vpS965;
        _M0Lm2vpS965 = _M0L6_2atmpS2523 - 1ull;
      }
    } else if (_M0L1qS979 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS969
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS962, _M0L1qS979);
    }
  }
  _M0Lm7removedS988 = 0;
  _M0Lm16lastRemovedDigitS989 = 0;
  _M0Lm6outputS990 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS968 || _M0Lm17vrIsTrailingZerosS969) {
    int32_t _if__result_3710;
    uint64_t _M0L6_2atmpS2564;
    uint64_t _M0L6_2atmpS2570;
    uint64_t _M0L6_2atmpS2571;
    int32_t _if__result_3711;
    int32_t _M0L6_2atmpS2567;
    int64_t _M0L6_2atmpS2566;
    uint64_t _M0L6_2atmpS2565;
    while (1) {
      uint64_t _M0L6_2atmpS2547 = _M0Lm2vpS965;
      uint64_t _M0L7vpDiv10S991 = _M0L6_2atmpS2547 / 10ull;
      uint64_t _M0L6_2atmpS2546 = _M0Lm2vmS966;
      uint64_t _M0L7vmDiv10S992 = _M0L6_2atmpS2546 / 10ull;
      uint64_t _M0L6_2atmpS2545;
      int32_t _M0L6_2atmpS2542;
      int32_t _M0L6_2atmpS2544;
      int32_t _M0L6_2atmpS2543;
      int32_t _M0L7vmMod10S994;
      uint64_t _M0L6_2atmpS2541;
      uint64_t _M0L7vrDiv10S995;
      uint64_t _M0L6_2atmpS2540;
      int32_t _M0L6_2atmpS2537;
      int32_t _M0L6_2atmpS2539;
      int32_t _M0L6_2atmpS2538;
      int32_t _M0L7vrMod10S996;
      int32_t _M0L6_2atmpS2536;
      if (_M0L7vpDiv10S991 <= _M0L7vmDiv10S992) {
        break;
      }
      _M0L6_2atmpS2545 = _M0Lm2vmS966;
      _M0L6_2atmpS2542 = (int32_t)_M0L6_2atmpS2545;
      _M0L6_2atmpS2544 = (int32_t)_M0L7vmDiv10S992;
      _M0L6_2atmpS2543 = 10 * _M0L6_2atmpS2544;
      _M0L7vmMod10S994 = _M0L6_2atmpS2542 - _M0L6_2atmpS2543;
      _M0L6_2atmpS2541 = _M0Lm2vrS964;
      _M0L7vrDiv10S995 = _M0L6_2atmpS2541 / 10ull;
      _M0L6_2atmpS2540 = _M0Lm2vrS964;
      _M0L6_2atmpS2537 = (int32_t)_M0L6_2atmpS2540;
      _M0L6_2atmpS2539 = (int32_t)_M0L7vrDiv10S995;
      _M0L6_2atmpS2538 = 10 * _M0L6_2atmpS2539;
      _M0L7vrMod10S996 = _M0L6_2atmpS2537 - _M0L6_2atmpS2538;
      if (_M0Lm17vmIsTrailingZerosS968) {
        _M0Lm17vmIsTrailingZerosS968 = _M0L7vmMod10S994 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS968 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS969) {
        int32_t _M0L6_2atmpS2535 = _M0Lm16lastRemovedDigitS989;
        _M0Lm17vrIsTrailingZerosS969 = _M0L6_2atmpS2535 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS969 = 0;
      }
      _M0Lm16lastRemovedDigitS989 = _M0L7vrMod10S996;
      _M0Lm2vrS964 = _M0L7vrDiv10S995;
      _M0Lm2vpS965 = _M0L7vpDiv10S991;
      _M0Lm2vmS966 = _M0L7vmDiv10S992;
      _M0L6_2atmpS2536 = _M0Lm7removedS988;
      _M0Lm7removedS988 = _M0L6_2atmpS2536 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS968) {
      while (1) {
        uint64_t _M0L6_2atmpS2560 = _M0Lm2vmS966;
        uint64_t _M0L7vmDiv10S997 = _M0L6_2atmpS2560 / 10ull;
        uint64_t _M0L6_2atmpS2559 = _M0Lm2vmS966;
        int32_t _M0L6_2atmpS2556 = (int32_t)_M0L6_2atmpS2559;
        int32_t _M0L6_2atmpS2558 = (int32_t)_M0L7vmDiv10S997;
        int32_t _M0L6_2atmpS2557 = 10 * _M0L6_2atmpS2558;
        int32_t _M0L7vmMod10S998 = _M0L6_2atmpS2556 - _M0L6_2atmpS2557;
        uint64_t _M0L6_2atmpS2555;
        uint64_t _M0L7vpDiv10S1000;
        uint64_t _M0L6_2atmpS2554;
        uint64_t _M0L7vrDiv10S1001;
        uint64_t _M0L6_2atmpS2553;
        int32_t _M0L6_2atmpS2550;
        int32_t _M0L6_2atmpS2552;
        int32_t _M0L6_2atmpS2551;
        int32_t _M0L7vrMod10S1002;
        int32_t _M0L6_2atmpS2549;
        if (_M0L7vmMod10S998 != 0) {
          break;
        }
        _M0L6_2atmpS2555 = _M0Lm2vpS965;
        _M0L7vpDiv10S1000 = _M0L6_2atmpS2555 / 10ull;
        _M0L6_2atmpS2554 = _M0Lm2vrS964;
        _M0L7vrDiv10S1001 = _M0L6_2atmpS2554 / 10ull;
        _M0L6_2atmpS2553 = _M0Lm2vrS964;
        _M0L6_2atmpS2550 = (int32_t)_M0L6_2atmpS2553;
        _M0L6_2atmpS2552 = (int32_t)_M0L7vrDiv10S1001;
        _M0L6_2atmpS2551 = 10 * _M0L6_2atmpS2552;
        _M0L7vrMod10S1002 = _M0L6_2atmpS2550 - _M0L6_2atmpS2551;
        if (_M0Lm17vrIsTrailingZerosS969) {
          int32_t _M0L6_2atmpS2548 = _M0Lm16lastRemovedDigitS989;
          _M0Lm17vrIsTrailingZerosS969 = _M0L6_2atmpS2548 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS969 = 0;
        }
        _M0Lm16lastRemovedDigitS989 = _M0L7vrMod10S1002;
        _M0Lm2vrS964 = _M0L7vrDiv10S1001;
        _M0Lm2vpS965 = _M0L7vpDiv10S1000;
        _M0Lm2vmS966 = _M0L7vmDiv10S997;
        _M0L6_2atmpS2549 = _M0Lm7removedS988;
        _M0Lm7removedS988 = _M0L6_2atmpS2549 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS969) {
      int32_t _M0L6_2atmpS2563 = _M0Lm16lastRemovedDigitS989;
      if (_M0L6_2atmpS2563 == 5) {
        uint64_t _M0L6_2atmpS2562 = _M0Lm2vrS964;
        uint64_t _M0L6_2atmpS2561 = _M0L6_2atmpS2562 % 2ull;
        _if__result_3710 = _M0L6_2atmpS2561 == 0ull;
      } else {
        _if__result_3710 = 0;
      }
    } else {
      _if__result_3710 = 0;
    }
    if (_if__result_3710) {
      _M0Lm16lastRemovedDigitS989 = 4;
    }
    _M0L6_2atmpS2564 = _M0Lm2vrS964;
    _M0L6_2atmpS2570 = _M0Lm2vrS964;
    _M0L6_2atmpS2571 = _M0Lm2vmS966;
    if (_M0L6_2atmpS2570 == _M0L6_2atmpS2571) {
      if (!_M0L4evenS961) {
        _if__result_3711 = 1;
      } else {
        int32_t _M0L6_2atmpS2569 = _M0Lm17vmIsTrailingZerosS968;
        _if__result_3711 = !_M0L6_2atmpS2569;
      }
    } else {
      _if__result_3711 = 0;
    }
    if (_if__result_3711) {
      _M0L6_2atmpS2567 = 1;
    } else {
      int32_t _M0L6_2atmpS2568 = _M0Lm16lastRemovedDigitS989;
      _M0L6_2atmpS2567 = _M0L6_2atmpS2568 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2566 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2567);
    _M0L6_2atmpS2565 = *(uint64_t*)&_M0L6_2atmpS2566;
    _M0Lm6outputS990 = _M0L6_2atmpS2564 + _M0L6_2atmpS2565;
  } else {
    int32_t _M0Lm7roundUpS1003 = 0;
    uint64_t _M0L6_2atmpS2592 = _M0Lm2vpS965;
    uint64_t _M0L8vpDiv100S1004 = _M0L6_2atmpS2592 / 100ull;
    uint64_t _M0L6_2atmpS2591 = _M0Lm2vmS966;
    uint64_t _M0L8vmDiv100S1005 = _M0L6_2atmpS2591 / 100ull;
    uint64_t _M0L6_2atmpS2586;
    uint64_t _M0L6_2atmpS2589;
    uint64_t _M0L6_2atmpS2590;
    int32_t _M0L6_2atmpS2588;
    uint64_t _M0L6_2atmpS2587;
    if (_M0L8vpDiv100S1004 > _M0L8vmDiv100S1005) {
      uint64_t _M0L6_2atmpS2577 = _M0Lm2vrS964;
      uint64_t _M0L8vrDiv100S1006 = _M0L6_2atmpS2577 / 100ull;
      uint64_t _M0L6_2atmpS2576 = _M0Lm2vrS964;
      int32_t _M0L6_2atmpS2573 = (int32_t)_M0L6_2atmpS2576;
      int32_t _M0L6_2atmpS2575 = (int32_t)_M0L8vrDiv100S1006;
      int32_t _M0L6_2atmpS2574 = 100 * _M0L6_2atmpS2575;
      int32_t _M0L8vrMod100S1007 = _M0L6_2atmpS2573 - _M0L6_2atmpS2574;
      int32_t _M0L6_2atmpS2572;
      _M0Lm7roundUpS1003 = _M0L8vrMod100S1007 >= 50;
      _M0Lm2vrS964 = _M0L8vrDiv100S1006;
      _M0Lm2vpS965 = _M0L8vpDiv100S1004;
      _M0Lm2vmS966 = _M0L8vmDiv100S1005;
      _M0L6_2atmpS2572 = _M0Lm7removedS988;
      _M0Lm7removedS988 = _M0L6_2atmpS2572 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2585 = _M0Lm2vpS965;
      uint64_t _M0L7vpDiv10S1008 = _M0L6_2atmpS2585 / 10ull;
      uint64_t _M0L6_2atmpS2584 = _M0Lm2vmS966;
      uint64_t _M0L7vmDiv10S1009 = _M0L6_2atmpS2584 / 10ull;
      uint64_t _M0L6_2atmpS2583;
      uint64_t _M0L7vrDiv10S1011;
      uint64_t _M0L6_2atmpS2582;
      int32_t _M0L6_2atmpS2579;
      int32_t _M0L6_2atmpS2581;
      int32_t _M0L6_2atmpS2580;
      int32_t _M0L7vrMod10S1012;
      int32_t _M0L6_2atmpS2578;
      if (_M0L7vpDiv10S1008 <= _M0L7vmDiv10S1009) {
        break;
      }
      _M0L6_2atmpS2583 = _M0Lm2vrS964;
      _M0L7vrDiv10S1011 = _M0L6_2atmpS2583 / 10ull;
      _M0L6_2atmpS2582 = _M0Lm2vrS964;
      _M0L6_2atmpS2579 = (int32_t)_M0L6_2atmpS2582;
      _M0L6_2atmpS2581 = (int32_t)_M0L7vrDiv10S1011;
      _M0L6_2atmpS2580 = 10 * _M0L6_2atmpS2581;
      _M0L7vrMod10S1012 = _M0L6_2atmpS2579 - _M0L6_2atmpS2580;
      _M0Lm7roundUpS1003 = _M0L7vrMod10S1012 >= 5;
      _M0Lm2vrS964 = _M0L7vrDiv10S1011;
      _M0Lm2vpS965 = _M0L7vpDiv10S1008;
      _M0Lm2vmS966 = _M0L7vmDiv10S1009;
      _M0L6_2atmpS2578 = _M0Lm7removedS988;
      _M0Lm7removedS988 = _M0L6_2atmpS2578 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2586 = _M0Lm2vrS964;
    _M0L6_2atmpS2589 = _M0Lm2vrS964;
    _M0L6_2atmpS2590 = _M0Lm2vmS966;
    _M0L6_2atmpS2588
    = _M0L6_2atmpS2589 == _M0L6_2atmpS2590 || _M0Lm7roundUpS1003;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2587 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2588);
    _M0Lm6outputS990 = _M0L6_2atmpS2586 + _M0L6_2atmpS2587;
  }
  _M0L6_2atmpS2594 = _M0Lm3e10S967;
  _M0L6_2atmpS2595 = _M0Lm7removedS988;
  _M0L3expS1013 = _M0L6_2atmpS2594 + _M0L6_2atmpS2595;
  _M0L6_2atmpS2593 = _M0Lm6outputS990;
  _block_3713
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_3713)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_3713->$0 = _M0L6_2atmpS2593;
  _block_3713->$1 = _M0L3expS1013;
  return _block_3713;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS956) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS956) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS955) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS955) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS954) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS954) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS953) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS953 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS953 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS953 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS953 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS953 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS953 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS953 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS953 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS953 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS953 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS953 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS953 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS953 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS953 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS953 >= 100ull) {
    return 3;
  }
  if (_M0L1vS953 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS936) {
  int32_t _M0L6_2atmpS2494;
  int32_t _M0L6_2atmpS2493;
  int32_t _M0L4baseS935;
  int32_t _M0L5base2S937;
  int32_t _M0L6offsetS938;
  int32_t _M0L6_2atmpS2492;
  uint64_t _M0L4mul0S939;
  int32_t _M0L6_2atmpS2491;
  int32_t _M0L6_2atmpS2490;
  uint64_t _M0L4mul1S940;
  uint64_t _M0L1mS941;
  struct _M0TPB7Umul128 _M0L7_2abindS942;
  uint64_t _M0L7_2alow1S943;
  uint64_t _M0L8_2ahigh1S944;
  struct _M0TPB7Umul128 _M0L7_2abindS945;
  uint64_t _M0L7_2alow0S946;
  uint64_t _M0L8_2ahigh0S947;
  uint64_t _M0L3sumS948;
  uint64_t _M0Lm5high1S949;
  int32_t _M0L6_2atmpS2488;
  int32_t _M0L6_2atmpS2489;
  int32_t _M0L5deltaS950;
  uint64_t _M0L6_2atmpS2487;
  uint64_t _M0L6_2atmpS2479;
  int32_t _M0L6_2atmpS2486;
  uint32_t _M0L6_2atmpS2483;
  int32_t _M0L6_2atmpS2485;
  int32_t _M0L6_2atmpS2484;
  uint32_t _M0L6_2atmpS2482;
  uint32_t _M0L6_2atmpS2481;
  uint64_t _M0L6_2atmpS2480;
  uint64_t _M0L1aS951;
  uint64_t _M0L6_2atmpS2478;
  uint64_t _M0L1bS952;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2494 = _M0L1iS936 + 26;
  _M0L6_2atmpS2493 = _M0L6_2atmpS2494 - 1;
  _M0L4baseS935 = _M0L6_2atmpS2493 / 26;
  _M0L5base2S937 = _M0L4baseS935 * 26;
  _M0L6offsetS938 = _M0L5base2S937 - _M0L1iS936;
  _M0L6_2atmpS2492 = _M0L4baseS935 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S939
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2492);
  _M0L6_2atmpS2491 = _M0L4baseS935 * 2;
  _M0L6_2atmpS2490 = _M0L6_2atmpS2491 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S940
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2490);
  if (_M0L6offsetS938 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S939, _M0L4mul1S940};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS941
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS938);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS942 = _M0FPB7umul128(_M0L1mS941, _M0L4mul1S940);
  _M0L7_2alow1S943 = _M0L7_2abindS942.$0;
  _M0L8_2ahigh1S944 = _M0L7_2abindS942.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS945 = _M0FPB7umul128(_M0L1mS941, _M0L4mul0S939);
  _M0L7_2alow0S946 = _M0L7_2abindS945.$0;
  _M0L8_2ahigh0S947 = _M0L7_2abindS945.$1;
  _M0L3sumS948 = _M0L8_2ahigh0S947 + _M0L7_2alow1S943;
  _M0Lm5high1S949 = _M0L8_2ahigh1S944;
  if (_M0L3sumS948 < _M0L8_2ahigh0S947) {
    uint64_t _M0L6_2atmpS2477 = _M0Lm5high1S949;
    _M0Lm5high1S949 = _M0L6_2atmpS2477 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2488 = _M0FPB8pow5bits(_M0L5base2S937);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2489 = _M0FPB8pow5bits(_M0L1iS936);
  _M0L5deltaS950 = _M0L6_2atmpS2488 - _M0L6_2atmpS2489;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2487
  = _M0FPB13shiftright128(_M0L7_2alow0S946, _M0L3sumS948, _M0L5deltaS950);
  _M0L6_2atmpS2479 = _M0L6_2atmpS2487 + 1ull;
  _M0L6_2atmpS2486 = _M0L1iS936 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2483
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2486);
  _M0L6_2atmpS2485 = _M0L1iS936 % 16;
  _M0L6_2atmpS2484 = _M0L6_2atmpS2485 << 1;
  _M0L6_2atmpS2482 = _M0L6_2atmpS2483 >> (_M0L6_2atmpS2484 & 31);
  _M0L6_2atmpS2481 = _M0L6_2atmpS2482 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2480 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2481);
  _M0L1aS951 = _M0L6_2atmpS2479 + _M0L6_2atmpS2480;
  _M0L6_2atmpS2478 = _M0Lm5high1S949;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS952
  = _M0FPB13shiftright128(_M0L3sumS948, _M0L6_2atmpS2478, _M0L5deltaS950);
  return (struct _M0TPB8Pow5Pair){_M0L1aS951, _M0L1bS952};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS918) {
  int32_t _M0L4baseS917;
  int32_t _M0L5base2S919;
  int32_t _M0L6offsetS920;
  int32_t _M0L6_2atmpS2476;
  uint64_t _M0L4mul0S921;
  int32_t _M0L6_2atmpS2475;
  int32_t _M0L6_2atmpS2474;
  uint64_t _M0L4mul1S922;
  uint64_t _M0L1mS923;
  struct _M0TPB7Umul128 _M0L7_2abindS924;
  uint64_t _M0L7_2alow1S925;
  uint64_t _M0L8_2ahigh1S926;
  struct _M0TPB7Umul128 _M0L7_2abindS927;
  uint64_t _M0L7_2alow0S928;
  uint64_t _M0L8_2ahigh0S929;
  uint64_t _M0L3sumS930;
  uint64_t _M0Lm5high1S931;
  int32_t _M0L6_2atmpS2472;
  int32_t _M0L6_2atmpS2473;
  int32_t _M0L5deltaS932;
  uint64_t _M0L6_2atmpS2464;
  int32_t _M0L6_2atmpS2471;
  uint32_t _M0L6_2atmpS2468;
  int32_t _M0L6_2atmpS2470;
  int32_t _M0L6_2atmpS2469;
  uint32_t _M0L6_2atmpS2467;
  uint32_t _M0L6_2atmpS2466;
  uint64_t _M0L6_2atmpS2465;
  uint64_t _M0L1aS933;
  uint64_t _M0L6_2atmpS2463;
  uint64_t _M0L1bS934;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS917 = _M0L1iS918 / 26;
  _M0L5base2S919 = _M0L4baseS917 * 26;
  _M0L6offsetS920 = _M0L1iS918 - _M0L5base2S919;
  _M0L6_2atmpS2476 = _M0L4baseS917 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S921
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2476);
  _M0L6_2atmpS2475 = _M0L4baseS917 * 2;
  _M0L6_2atmpS2474 = _M0L6_2atmpS2475 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S922
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2474);
  if (_M0L6offsetS920 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S921, _M0L4mul1S922};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS923
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS920);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS924 = _M0FPB7umul128(_M0L1mS923, _M0L4mul1S922);
  _M0L7_2alow1S925 = _M0L7_2abindS924.$0;
  _M0L8_2ahigh1S926 = _M0L7_2abindS924.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS927 = _M0FPB7umul128(_M0L1mS923, _M0L4mul0S921);
  _M0L7_2alow0S928 = _M0L7_2abindS927.$0;
  _M0L8_2ahigh0S929 = _M0L7_2abindS927.$1;
  _M0L3sumS930 = _M0L8_2ahigh0S929 + _M0L7_2alow1S925;
  _M0Lm5high1S931 = _M0L8_2ahigh1S926;
  if (_M0L3sumS930 < _M0L8_2ahigh0S929) {
    uint64_t _M0L6_2atmpS2462 = _M0Lm5high1S931;
    _M0Lm5high1S931 = _M0L6_2atmpS2462 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2472 = _M0FPB8pow5bits(_M0L1iS918);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2473 = _M0FPB8pow5bits(_M0L5base2S919);
  _M0L5deltaS932 = _M0L6_2atmpS2472 - _M0L6_2atmpS2473;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2464
  = _M0FPB13shiftright128(_M0L7_2alow0S928, _M0L3sumS930, _M0L5deltaS932);
  _M0L6_2atmpS2471 = _M0L1iS918 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2468
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2471);
  _M0L6_2atmpS2470 = _M0L1iS918 % 16;
  _M0L6_2atmpS2469 = _M0L6_2atmpS2470 << 1;
  _M0L6_2atmpS2467 = _M0L6_2atmpS2468 >> (_M0L6_2atmpS2469 & 31);
  _M0L6_2atmpS2466 = _M0L6_2atmpS2467 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2465 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2466);
  _M0L1aS933 = _M0L6_2atmpS2464 + _M0L6_2atmpS2465;
  _M0L6_2atmpS2463 = _M0Lm5high1S931;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS934
  = _M0FPB13shiftright128(_M0L3sumS930, _M0L6_2atmpS2463, _M0L5deltaS932);
  return (struct _M0TPB8Pow5Pair){_M0L1aS933, _M0L1bS934};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS891,
  struct _M0TPB8Pow5Pair _M0L3mulS888,
  int32_t _M0L1jS904,
  int32_t _M0L7mmShiftS906
) {
  uint64_t _M0L7_2amul0S887;
  uint64_t _M0L7_2amul1S889;
  uint64_t _M0L1mS890;
  struct _M0TPB7Umul128 _M0L7_2abindS892;
  uint64_t _M0L5_2aloS893;
  uint64_t _M0L6_2atmpS894;
  struct _M0TPB7Umul128 _M0L7_2abindS895;
  uint64_t _M0L6_2alo2S896;
  uint64_t _M0L6_2ahi2S897;
  uint64_t _M0L3midS898;
  uint64_t _M0L6_2atmpS2461;
  uint64_t _M0L2hiS899;
  uint64_t _M0L3lo2S900;
  uint64_t _M0L6_2atmpS2459;
  uint64_t _M0L6_2atmpS2460;
  uint64_t _M0L4mid2S901;
  uint64_t _M0L6_2atmpS2458;
  uint64_t _M0L3hi2S902;
  int32_t _M0L6_2atmpS2457;
  int32_t _M0L6_2atmpS2456;
  uint64_t _M0L2vpS903;
  uint64_t _M0Lm2vmS905;
  int32_t _M0L6_2atmpS2455;
  int32_t _M0L6_2atmpS2454;
  uint64_t _M0L2vrS916;
  uint64_t _M0L6_2atmpS2453;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S887 = _M0L3mulS888.$0;
  _M0L7_2amul1S889 = _M0L3mulS888.$1;
  _M0L1mS890 = _M0L1mS891 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS892 = _M0FPB7umul128(_M0L1mS890, _M0L7_2amul0S887);
  _M0L5_2aloS893 = _M0L7_2abindS892.$0;
  _M0L6_2atmpS894 = _M0L7_2abindS892.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS895 = _M0FPB7umul128(_M0L1mS890, _M0L7_2amul1S889);
  _M0L6_2alo2S896 = _M0L7_2abindS895.$0;
  _M0L6_2ahi2S897 = _M0L7_2abindS895.$1;
  _M0L3midS898 = _M0L6_2atmpS894 + _M0L6_2alo2S896;
  if (_M0L3midS898 < _M0L6_2atmpS894) {
    _M0L6_2atmpS2461 = 1ull;
  } else {
    _M0L6_2atmpS2461 = 0ull;
  }
  _M0L2hiS899 = _M0L6_2ahi2S897 + _M0L6_2atmpS2461;
  _M0L3lo2S900 = _M0L5_2aloS893 + _M0L7_2amul0S887;
  _M0L6_2atmpS2459 = _M0L3midS898 + _M0L7_2amul1S889;
  if (_M0L3lo2S900 < _M0L5_2aloS893) {
    _M0L6_2atmpS2460 = 1ull;
  } else {
    _M0L6_2atmpS2460 = 0ull;
  }
  _M0L4mid2S901 = _M0L6_2atmpS2459 + _M0L6_2atmpS2460;
  if (_M0L4mid2S901 < _M0L3midS898) {
    _M0L6_2atmpS2458 = 1ull;
  } else {
    _M0L6_2atmpS2458 = 0ull;
  }
  _M0L3hi2S902 = _M0L2hiS899 + _M0L6_2atmpS2458;
  _M0L6_2atmpS2457 = _M0L1jS904 - 64;
  _M0L6_2atmpS2456 = _M0L6_2atmpS2457 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS903
  = _M0FPB13shiftright128(_M0L4mid2S901, _M0L3hi2S902, _M0L6_2atmpS2456);
  _M0Lm2vmS905 = 0ull;
  if (_M0L7mmShiftS906) {
    uint64_t _M0L3lo3S907 = _M0L5_2aloS893 - _M0L7_2amul0S887;
    uint64_t _M0L6_2atmpS2443 = _M0L3midS898 - _M0L7_2amul1S889;
    uint64_t _M0L6_2atmpS2444;
    uint64_t _M0L4mid3S908;
    uint64_t _M0L6_2atmpS2442;
    uint64_t _M0L3hi3S909;
    int32_t _M0L6_2atmpS2441;
    int32_t _M0L6_2atmpS2440;
    if (_M0L5_2aloS893 < _M0L3lo3S907) {
      _M0L6_2atmpS2444 = 1ull;
    } else {
      _M0L6_2atmpS2444 = 0ull;
    }
    _M0L4mid3S908 = _M0L6_2atmpS2443 - _M0L6_2atmpS2444;
    if (_M0L3midS898 < _M0L4mid3S908) {
      _M0L6_2atmpS2442 = 1ull;
    } else {
      _M0L6_2atmpS2442 = 0ull;
    }
    _M0L3hi3S909 = _M0L2hiS899 - _M0L6_2atmpS2442;
    _M0L6_2atmpS2441 = _M0L1jS904 - 64;
    _M0L6_2atmpS2440 = _M0L6_2atmpS2441 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS905
    = _M0FPB13shiftright128(_M0L4mid3S908, _M0L3hi3S909, _M0L6_2atmpS2440);
  } else {
    uint64_t _M0L3lo3S910 = _M0L5_2aloS893 + _M0L5_2aloS893;
    uint64_t _M0L6_2atmpS2451 = _M0L3midS898 + _M0L3midS898;
    uint64_t _M0L6_2atmpS2452;
    uint64_t _M0L4mid3S911;
    uint64_t _M0L6_2atmpS2449;
    uint64_t _M0L6_2atmpS2450;
    uint64_t _M0L3hi3S912;
    uint64_t _M0L3lo4S913;
    uint64_t _M0L6_2atmpS2447;
    uint64_t _M0L6_2atmpS2448;
    uint64_t _M0L4mid4S914;
    uint64_t _M0L6_2atmpS2446;
    uint64_t _M0L3hi4S915;
    int32_t _M0L6_2atmpS2445;
    if (_M0L3lo3S910 < _M0L5_2aloS893) {
      _M0L6_2atmpS2452 = 1ull;
    } else {
      _M0L6_2atmpS2452 = 0ull;
    }
    _M0L4mid3S911 = _M0L6_2atmpS2451 + _M0L6_2atmpS2452;
    _M0L6_2atmpS2449 = _M0L2hiS899 + _M0L2hiS899;
    if (_M0L4mid3S911 < _M0L3midS898) {
      _M0L6_2atmpS2450 = 1ull;
    } else {
      _M0L6_2atmpS2450 = 0ull;
    }
    _M0L3hi3S912 = _M0L6_2atmpS2449 + _M0L6_2atmpS2450;
    _M0L3lo4S913 = _M0L3lo3S910 - _M0L7_2amul0S887;
    _M0L6_2atmpS2447 = _M0L4mid3S911 - _M0L7_2amul1S889;
    if (_M0L3lo3S910 < _M0L3lo4S913) {
      _M0L6_2atmpS2448 = 1ull;
    } else {
      _M0L6_2atmpS2448 = 0ull;
    }
    _M0L4mid4S914 = _M0L6_2atmpS2447 - _M0L6_2atmpS2448;
    if (_M0L4mid3S911 < _M0L4mid4S914) {
      _M0L6_2atmpS2446 = 1ull;
    } else {
      _M0L6_2atmpS2446 = 0ull;
    }
    _M0L3hi4S915 = _M0L3hi3S912 - _M0L6_2atmpS2446;
    _M0L6_2atmpS2445 = _M0L1jS904 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS905
    = _M0FPB13shiftright128(_M0L4mid4S914, _M0L3hi4S915, _M0L6_2atmpS2445);
  }
  _M0L6_2atmpS2455 = _M0L1jS904 - 64;
  _M0L6_2atmpS2454 = _M0L6_2atmpS2455 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS916
  = _M0FPB13shiftright128(_M0L3midS898, _M0L2hiS899, _M0L6_2atmpS2454);
  _M0L6_2atmpS2453 = _M0Lm2vmS905;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS916,
                                                _M0L2vpS903,
                                                _M0L6_2atmpS2453};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS885,
  int32_t _M0L1pS886
) {
  uint64_t _M0L6_2atmpS2439;
  uint64_t _M0L6_2atmpS2438;
  uint64_t _M0L6_2atmpS2437;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2439 = 1ull << (_M0L1pS886 & 63);
  _M0L6_2atmpS2438 = _M0L6_2atmpS2439 - 1ull;
  _M0L6_2atmpS2437 = _M0L5valueS885 & _M0L6_2atmpS2438;
  return _M0L6_2atmpS2437 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS883,
  int32_t _M0L1pS884
) {
  int32_t _M0L6_2atmpS2436;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2436 = _M0FPB10pow5Factor(_M0L5valueS883);
  return _M0L6_2atmpS2436 >= _M0L1pS884;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS879) {
  uint64_t _M0L6_2atmpS2424;
  uint64_t _M0L6_2atmpS2425;
  uint64_t _M0L6_2atmpS2426;
  uint64_t _M0L6_2atmpS2427;
  int32_t _M0Lm5countS880;
  uint64_t _M0Lm5valueS881;
  uint64_t _M0L6_2atmpS2435;
  moonbit_string_t _M0L6_2atmpS2434;
  moonbit_string_t _M0L6_2atmpS3277;
  moonbit_string_t _M0L6_2atmpS2433;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2424 = _M0L5valueS879 % 5ull;
  if (_M0L6_2atmpS2424 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2425 = _M0L5valueS879 % 25ull;
  if (_M0L6_2atmpS2425 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2426 = _M0L5valueS879 % 125ull;
  if (_M0L6_2atmpS2426 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2427 = _M0L5valueS879 % 625ull;
  if (_M0L6_2atmpS2427 != 0ull) {
    return 3;
  }
  _M0Lm5countS880 = 4;
  _M0Lm5valueS881 = _M0L5valueS879 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2428 = _M0Lm5valueS881;
    if (_M0L6_2atmpS2428 > 0ull) {
      uint64_t _M0L6_2atmpS2430 = _M0Lm5valueS881;
      uint64_t _M0L6_2atmpS2429 = _M0L6_2atmpS2430 % 5ull;
      uint64_t _M0L6_2atmpS2431;
      int32_t _M0L6_2atmpS2432;
      if (_M0L6_2atmpS2429 != 0ull) {
        return _M0Lm5countS880;
      }
      _M0L6_2atmpS2431 = _M0Lm5valueS881;
      _M0Lm5valueS881 = _M0L6_2atmpS2431 / 5ull;
      _M0L6_2atmpS2432 = _M0Lm5countS880;
      _M0Lm5countS880 = _M0L6_2atmpS2432 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2435 = _M0Lm5valueS881;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2434
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2435);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3277
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS2434);
  moonbit_decref(_M0L6_2atmpS2434);
  _M0L6_2atmpS2433 = _M0L6_2atmpS3277;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2433, (moonbit_string_t)moonbit_string_literal_69.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS878,
  uint64_t _M0L2hiS876,
  int32_t _M0L4distS877
) {
  int32_t _M0L6_2atmpS2423;
  uint64_t _M0L6_2atmpS2421;
  uint64_t _M0L6_2atmpS2422;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2423 = 64 - _M0L4distS877;
  _M0L6_2atmpS2421 = _M0L2hiS876 << (_M0L6_2atmpS2423 & 63);
  _M0L6_2atmpS2422 = _M0L2loS878 >> (_M0L4distS877 & 63);
  return _M0L6_2atmpS2421 | _M0L6_2atmpS2422;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS866,
  uint64_t _M0L1bS869
) {
  uint64_t _M0L3aLoS865;
  uint64_t _M0L3aHiS867;
  uint64_t _M0L3bLoS868;
  uint64_t _M0L3bHiS870;
  uint64_t _M0L1xS871;
  uint64_t _M0L6_2atmpS2419;
  uint64_t _M0L6_2atmpS2420;
  uint64_t _M0L1yS872;
  uint64_t _M0L6_2atmpS2417;
  uint64_t _M0L6_2atmpS2418;
  uint64_t _M0L1zS873;
  uint64_t _M0L6_2atmpS2415;
  uint64_t _M0L6_2atmpS2416;
  uint64_t _M0L6_2atmpS2413;
  uint64_t _M0L6_2atmpS2414;
  uint64_t _M0L1wS874;
  uint64_t _M0L2loS875;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS865 = _M0L1aS866 & 4294967295ull;
  _M0L3aHiS867 = _M0L1aS866 >> 32;
  _M0L3bLoS868 = _M0L1bS869 & 4294967295ull;
  _M0L3bHiS870 = _M0L1bS869 >> 32;
  _M0L1xS871 = _M0L3aLoS865 * _M0L3bLoS868;
  _M0L6_2atmpS2419 = _M0L3aHiS867 * _M0L3bLoS868;
  _M0L6_2atmpS2420 = _M0L1xS871 >> 32;
  _M0L1yS872 = _M0L6_2atmpS2419 + _M0L6_2atmpS2420;
  _M0L6_2atmpS2417 = _M0L3aLoS865 * _M0L3bHiS870;
  _M0L6_2atmpS2418 = _M0L1yS872 & 4294967295ull;
  _M0L1zS873 = _M0L6_2atmpS2417 + _M0L6_2atmpS2418;
  _M0L6_2atmpS2415 = _M0L3aHiS867 * _M0L3bHiS870;
  _M0L6_2atmpS2416 = _M0L1yS872 >> 32;
  _M0L6_2atmpS2413 = _M0L6_2atmpS2415 + _M0L6_2atmpS2416;
  _M0L6_2atmpS2414 = _M0L1zS873 >> 32;
  _M0L1wS874 = _M0L6_2atmpS2413 + _M0L6_2atmpS2414;
  _M0L2loS875 = _M0L1aS866 * _M0L1bS869;
  return (struct _M0TPB7Umul128){_M0L2loS875, _M0L1wS874};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS860,
  int32_t _M0L4fromS864,
  int32_t _M0L2toS862
) {
  int32_t _M0L6_2atmpS2412;
  struct _M0TPB13StringBuilder* _M0L3bufS859;
  int32_t _M0L1iS861;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2412 = Moonbit_array_length(_M0L5bytesS860);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS859 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2412);
  _M0L1iS861 = _M0L4fromS864;
  while (1) {
    if (_M0L1iS861 < _M0L2toS862) {
      int32_t _M0L6_2atmpS2410;
      int32_t _M0L6_2atmpS2409;
      int32_t _M0L6_2atmpS2411;
      if (
        _M0L1iS861 < 0 || _M0L1iS861 >= Moonbit_array_length(_M0L5bytesS860)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2410 = (int32_t)_M0L5bytesS860[_M0L1iS861];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2409 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2410);
      moonbit_incref(_M0L3bufS859);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS859, _M0L6_2atmpS2409);
      _M0L6_2atmpS2411 = _M0L1iS861 + 1;
      _M0L1iS861 = _M0L6_2atmpS2411;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS860);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS859);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS858) {
  int32_t _M0L6_2atmpS2408;
  uint32_t _M0L6_2atmpS2407;
  uint32_t _M0L6_2atmpS2406;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2408 = _M0L1eS858 * 78913;
  _M0L6_2atmpS2407 = *(uint32_t*)&_M0L6_2atmpS2408;
  _M0L6_2atmpS2406 = _M0L6_2atmpS2407 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2406;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS857) {
  int32_t _M0L6_2atmpS2405;
  uint32_t _M0L6_2atmpS2404;
  uint32_t _M0L6_2atmpS2403;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2405 = _M0L1eS857 * 732923;
  _M0L6_2atmpS2404 = *(uint32_t*)&_M0L6_2atmpS2405;
  _M0L6_2atmpS2403 = _M0L6_2atmpS2404 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2403;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS855,
  int32_t _M0L8exponentS856,
  int32_t _M0L8mantissaS853
) {
  moonbit_string_t _M0L1sS854;
  moonbit_string_t _M0L6_2atmpS3278;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS853) {
    return (moonbit_string_t)moonbit_string_literal_70.data;
  }
  if (_M0L4signS855) {
    _M0L1sS854 = (moonbit_string_t)moonbit_string_literal_71.data;
  } else {
    _M0L1sS854 = (moonbit_string_t)moonbit_string_literal_1.data;
  }
  if (_M0L8exponentS856) {
    moonbit_string_t _M0L6_2atmpS3279;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3279
    = moonbit_add_string(_M0L1sS854, (moonbit_string_t)moonbit_string_literal_72.data);
    moonbit_decref(_M0L1sS854);
    return _M0L6_2atmpS3279;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3278
  = moonbit_add_string(_M0L1sS854, (moonbit_string_t)moonbit_string_literal_73.data);
  moonbit_decref(_M0L1sS854);
  return _M0L6_2atmpS3278;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS852) {
  int32_t _M0L6_2atmpS2402;
  uint32_t _M0L6_2atmpS2401;
  uint32_t _M0L6_2atmpS2400;
  int32_t _M0L6_2atmpS2399;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2402 = _M0L1eS852 * 1217359;
  _M0L6_2atmpS2401 = *(uint32_t*)&_M0L6_2atmpS2402;
  _M0L6_2atmpS2400 = _M0L6_2atmpS2401 >> 19;
  _M0L6_2atmpS2399 = *(int32_t*)&_M0L6_2atmpS2400;
  return _M0L6_2atmpS2399 + 1;
}

int32_t _M0MPC16double6Double7to__int(double _M0L4selfS851) {
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_to_int.mbt"
  if (_M0L4selfS851 != _M0L4selfS851) {
    return 0;
  } else if (_M0L4selfS851 >= 0x1.fffffffcp+30) {
    return 2147483647;
  } else if (_M0L4selfS851 <= -0x1p+31) {
    return (int32_t)0x80000000;
  } else {
    return (int32_t)_M0L4selfS851;
  }
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS850,
  struct _M0TPB6Hasher* _M0L6hasherS849
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS849, _M0L4selfS850);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS848,
  struct _M0TPB6Hasher* _M0L6hasherS847
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS847, _M0L4selfS848);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS845,
  moonbit_string_t _M0L5valueS843
) {
  int32_t _M0L7_2abindS842;
  int32_t _M0L1iS844;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS842 = Moonbit_array_length(_M0L5valueS843);
  _M0L1iS844 = 0;
  while (1) {
    if (_M0L1iS844 < _M0L7_2abindS842) {
      int32_t _M0L6_2atmpS2397 = _M0L5valueS843[_M0L1iS844];
      int32_t _M0L6_2atmpS2396 = (int32_t)_M0L6_2atmpS2397;
      uint32_t _M0L6_2atmpS2395 = *(uint32_t*)&_M0L6_2atmpS2396;
      int32_t _M0L6_2atmpS2398;
      moonbit_incref(_M0L4selfS845);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS845, _M0L6_2atmpS2395);
      _M0L6_2atmpS2398 = _M0L1iS844 + 1;
      _M0L1iS844 = _M0L6_2atmpS2398;
      continue;
    } else {
      moonbit_decref(_M0L4selfS845);
      moonbit_decref(_M0L5valueS843);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS840,
  int32_t _M0L3idxS841
) {
  int32_t _M0L6_2atmpS3280;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3280 = _M0L4selfS840[_M0L3idxS841];
  moonbit_decref(_M0L4selfS840);
  return _M0L6_2atmpS3280;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS839) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS839;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS838) {
  double _M0L6_2atmpS2393;
  moonbit_string_t _M0L6_2atmpS2394;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2393 = (double)_M0L4selfS838;
  _M0L6_2atmpS2394 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2393, _M0L6_2atmpS2394);
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS837) {
  void* _block_3717;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3717 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_3717)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_3717)->$0 = _M0L6objectS837;
  return _block_3717;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS836) {
  void* _block_3718;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3718 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3718)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3718)->$0 = _M0L6stringS836;
  return _block_3718;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS829
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3281;
  int32_t _M0L6_2acntS3553;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2392;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS828;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__* _closure_3719;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2387;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3281 = _M0L4selfS829->$5;
  _M0L6_2acntS3553 = Moonbit_object_header(_M0L4selfS829)->rc;
  if (_M0L6_2acntS3553 > 1) {
    int32_t _M0L11_2anew__cntS3555 = _M0L6_2acntS3553 - 1;
    Moonbit_object_header(_M0L4selfS829)->rc = _M0L11_2anew__cntS3555;
    if (_M0L8_2afieldS3281) {
      moonbit_incref(_M0L8_2afieldS3281);
    }
  } else if (_M0L6_2acntS3553 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3554 = _M0L4selfS829->$0;
    moonbit_decref(_M0L8_2afieldS3554);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS829);
  }
  _M0L4headS2392 = _M0L8_2afieldS3281;
  _M0L11curr__entryS828
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS828)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS828->$0 = _M0L4headS2392;
  _closure_3719
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__));
  Moonbit_object_header(_closure_3719)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__, $0) >> 2, 1, 0);
  _closure_3719->code = &_M0MPB3Map4iterGsRPB4JsonEC2388l591;
  _closure_3719->$0 = _M0L11curr__entryS828;
  _M0L6_2atmpS2387 = (struct _M0TWEOUsRPB4JsonE*)_closure_3719;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2387);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2388l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2389
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__* _M0L14_2acasted__envS2390;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3287;
  int32_t _M0L6_2acntS3556;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS828;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3286;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS830;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2390
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2388__l591__*)_M0L6_2aenvS2389;
  _M0L8_2afieldS3287 = _M0L14_2acasted__envS2390->$0;
  _M0L6_2acntS3556 = Moonbit_object_header(_M0L14_2acasted__envS2390)->rc;
  if (_M0L6_2acntS3556 > 1) {
    int32_t _M0L11_2anew__cntS3557 = _M0L6_2acntS3556 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2390)->rc
    = _M0L11_2anew__cntS3557;
    moonbit_incref(_M0L8_2afieldS3287);
  } else if (_M0L6_2acntS3556 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2390);
  }
  _M0L11curr__entryS828 = _M0L8_2afieldS3287;
  _M0L8_2afieldS3286 = _M0L11curr__entryS828->$0;
  _M0L7_2abindS830 = _M0L8_2afieldS3286;
  if (_M0L7_2abindS830 == 0) {
    moonbit_decref(_M0L11curr__entryS828);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS831 = _M0L7_2abindS830;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS832 = _M0L7_2aSomeS831;
    moonbit_string_t _M0L8_2afieldS3285 = _M0L4_2axS832->$4;
    moonbit_string_t _M0L6_2akeyS833 = _M0L8_2afieldS3285;
    void* _M0L8_2afieldS3284 = _M0L4_2axS832->$5;
    void* _M0L8_2avalueS834 = _M0L8_2afieldS3284;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3283 = _M0L4_2axS832->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS835 = _M0L8_2afieldS3283;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3282 =
      _M0L11curr__entryS828->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2391;
    if (_M0L7_2anextS835) {
      moonbit_incref(_M0L7_2anextS835);
    }
    moonbit_incref(_M0L8_2avalueS834);
    moonbit_incref(_M0L6_2akeyS833);
    if (_M0L6_2aoldS3282) {
      moonbit_decref(_M0L6_2aoldS3282);
    }
    _M0L11curr__entryS828->$0 = _M0L7_2anextS835;
    moonbit_decref(_M0L11curr__entryS828);
    _M0L8_2atupleS2391
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2391)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2391->$0 = _M0L6_2akeyS833;
    _M0L8_2atupleS2391->$1 = _M0L8_2avalueS834;
    return _M0L8_2atupleS2391;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS827
) {
  int32_t _M0L8_2afieldS3288;
  int32_t _M0L4sizeS2386;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3288 = _M0L4selfS827->$1;
  moonbit_decref(_M0L4selfS827);
  _M0L4sizeS2386 = _M0L8_2afieldS3288;
  return _M0L4sizeS2386 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS805,
  int32_t _M0L3keyS801
) {
  int32_t _M0L4hashS800;
  int32_t _M0L14capacity__maskS2357;
  int32_t _M0L6_2atmpS2356;
  int32_t _M0L1iS802;
  int32_t _M0L3idxS803;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS800 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS801);
  _M0L14capacity__maskS2357 = _M0L4selfS805->$3;
  _M0L6_2atmpS2356 = _M0L4hashS800 & _M0L14capacity__maskS2357;
  _M0L1iS802 = 0;
  _M0L3idxS803 = _M0L6_2atmpS2356;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3292 =
      _M0L4selfS805->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2355 =
      _M0L8_2afieldS3292;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3291;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS804;
    if (
      _M0L3idxS803 < 0
      || _M0L3idxS803 >= Moonbit_array_length(_M0L7entriesS2355)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3291
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2355[
        _M0L3idxS803
      ];
    _M0L7_2abindS804 = _M0L6_2atmpS3291;
    if (_M0L7_2abindS804 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2344;
      if (_M0L7_2abindS804) {
        moonbit_incref(_M0L7_2abindS804);
      }
      moonbit_decref(_M0L4selfS805);
      if (_M0L7_2abindS804) {
        moonbit_decref(_M0L7_2abindS804);
      }
      _M0L6_2atmpS2344 = 0;
      return _M0L6_2atmpS2344;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS806 =
        _M0L7_2abindS804;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS807 =
        _M0L7_2aSomeS806;
      int32_t _M0L4hashS2346 = _M0L8_2aentryS807->$3;
      int32_t _if__result_3721;
      int32_t _M0L8_2afieldS3289;
      int32_t _M0L3pslS2349;
      int32_t _M0L6_2atmpS2351;
      int32_t _M0L6_2atmpS2353;
      int32_t _M0L14capacity__maskS2354;
      int32_t _M0L6_2atmpS2352;
      if (_M0L4hashS2346 == _M0L4hashS800) {
        int32_t _M0L3keyS2345 = _M0L8_2aentryS807->$4;
        _if__result_3721 = _M0L3keyS2345 == _M0L3keyS801;
      } else {
        _if__result_3721 = 0;
      }
      if (_if__result_3721) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3290;
        int32_t _M0L6_2acntS3558;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2348;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2347;
        moonbit_incref(_M0L8_2aentryS807);
        moonbit_decref(_M0L4selfS805);
        _M0L8_2afieldS3290 = _M0L8_2aentryS807->$5;
        _M0L6_2acntS3558 = Moonbit_object_header(_M0L8_2aentryS807)->rc;
        if (_M0L6_2acntS3558 > 1) {
          int32_t _M0L11_2anew__cntS3560 = _M0L6_2acntS3558 - 1;
          Moonbit_object_header(_M0L8_2aentryS807)->rc
          = _M0L11_2anew__cntS3560;
          moonbit_incref(_M0L8_2afieldS3290);
        } else if (_M0L6_2acntS3558 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3559 =
            _M0L8_2aentryS807->$1;
          if (_M0L8_2afieldS3559) {
            moonbit_decref(_M0L8_2afieldS3559);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS807);
        }
        _M0L5valueS2348 = _M0L8_2afieldS3290;
        _M0L6_2atmpS2347 = _M0L5valueS2348;
        return _M0L6_2atmpS2347;
      } else {
        moonbit_incref(_M0L8_2aentryS807);
      }
      _M0L8_2afieldS3289 = _M0L8_2aentryS807->$2;
      moonbit_decref(_M0L8_2aentryS807);
      _M0L3pslS2349 = _M0L8_2afieldS3289;
      if (_M0L1iS802 > _M0L3pslS2349) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2350;
        moonbit_decref(_M0L4selfS805);
        _M0L6_2atmpS2350 = 0;
        return _M0L6_2atmpS2350;
      }
      _M0L6_2atmpS2351 = _M0L1iS802 + 1;
      _M0L6_2atmpS2353 = _M0L3idxS803 + 1;
      _M0L14capacity__maskS2354 = _M0L4selfS805->$3;
      _M0L6_2atmpS2352 = _M0L6_2atmpS2353 & _M0L14capacity__maskS2354;
      _M0L1iS802 = _M0L6_2atmpS2351;
      _M0L3idxS803 = _M0L6_2atmpS2352;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS814,
  moonbit_string_t _M0L3keyS810
) {
  int32_t _M0L4hashS809;
  int32_t _M0L14capacity__maskS2371;
  int32_t _M0L6_2atmpS2370;
  int32_t _M0L1iS811;
  int32_t _M0L3idxS812;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS810);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS809 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS810);
  _M0L14capacity__maskS2371 = _M0L4selfS814->$3;
  _M0L6_2atmpS2370 = _M0L4hashS809 & _M0L14capacity__maskS2371;
  _M0L1iS811 = 0;
  _M0L3idxS812 = _M0L6_2atmpS2370;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3298 =
      _M0L4selfS814->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2369 =
      _M0L8_2afieldS3298;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3297;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS813;
    if (
      _M0L3idxS812 < 0
      || _M0L3idxS812 >= Moonbit_array_length(_M0L7entriesS2369)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3297
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2369[
        _M0L3idxS812
      ];
    _M0L7_2abindS813 = _M0L6_2atmpS3297;
    if (_M0L7_2abindS813 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2358;
      if (_M0L7_2abindS813) {
        moonbit_incref(_M0L7_2abindS813);
      }
      moonbit_decref(_M0L4selfS814);
      if (_M0L7_2abindS813) {
        moonbit_decref(_M0L7_2abindS813);
      }
      moonbit_decref(_M0L3keyS810);
      _M0L6_2atmpS2358 = 0;
      return _M0L6_2atmpS2358;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS815 =
        _M0L7_2abindS813;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS816 =
        _M0L7_2aSomeS815;
      int32_t _M0L4hashS2360 = _M0L8_2aentryS816->$3;
      int32_t _if__result_3723;
      int32_t _M0L8_2afieldS3293;
      int32_t _M0L3pslS2363;
      int32_t _M0L6_2atmpS2365;
      int32_t _M0L6_2atmpS2367;
      int32_t _M0L14capacity__maskS2368;
      int32_t _M0L6_2atmpS2366;
      if (_M0L4hashS2360 == _M0L4hashS809) {
        moonbit_string_t _M0L8_2afieldS3296 = _M0L8_2aentryS816->$4;
        moonbit_string_t _M0L3keyS2359 = _M0L8_2afieldS3296;
        int32_t _M0L6_2atmpS3295;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3295
        = moonbit_val_array_equal(_M0L3keyS2359, _M0L3keyS810);
        _if__result_3723 = _M0L6_2atmpS3295;
      } else {
        _if__result_3723 = 0;
      }
      if (_if__result_3723) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3294;
        int32_t _M0L6_2acntS3561;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2362;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2361;
        moonbit_incref(_M0L8_2aentryS816);
        moonbit_decref(_M0L4selfS814);
        moonbit_decref(_M0L3keyS810);
        _M0L8_2afieldS3294 = _M0L8_2aentryS816->$5;
        _M0L6_2acntS3561 = Moonbit_object_header(_M0L8_2aentryS816)->rc;
        if (_M0L6_2acntS3561 > 1) {
          int32_t _M0L11_2anew__cntS3564 = _M0L6_2acntS3561 - 1;
          Moonbit_object_header(_M0L8_2aentryS816)->rc
          = _M0L11_2anew__cntS3564;
          moonbit_incref(_M0L8_2afieldS3294);
        } else if (_M0L6_2acntS3561 == 1) {
          moonbit_string_t _M0L8_2afieldS3563 = _M0L8_2aentryS816->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3562;
          moonbit_decref(_M0L8_2afieldS3563);
          _M0L8_2afieldS3562 = _M0L8_2aentryS816->$1;
          if (_M0L8_2afieldS3562) {
            moonbit_decref(_M0L8_2afieldS3562);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS816);
        }
        _M0L5valueS2362 = _M0L8_2afieldS3294;
        _M0L6_2atmpS2361 = _M0L5valueS2362;
        return _M0L6_2atmpS2361;
      } else {
        moonbit_incref(_M0L8_2aentryS816);
      }
      _M0L8_2afieldS3293 = _M0L8_2aentryS816->$2;
      moonbit_decref(_M0L8_2aentryS816);
      _M0L3pslS2363 = _M0L8_2afieldS3293;
      if (_M0L1iS811 > _M0L3pslS2363) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2364;
        moonbit_decref(_M0L4selfS814);
        moonbit_decref(_M0L3keyS810);
        _M0L6_2atmpS2364 = 0;
        return _M0L6_2atmpS2364;
      }
      _M0L6_2atmpS2365 = _M0L1iS811 + 1;
      _M0L6_2atmpS2367 = _M0L3idxS812 + 1;
      _M0L14capacity__maskS2368 = _M0L4selfS814->$3;
      _M0L6_2atmpS2366 = _M0L6_2atmpS2367 & _M0L14capacity__maskS2368;
      _M0L1iS811 = _M0L6_2atmpS2365;
      _M0L3idxS812 = _M0L6_2atmpS2366;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS823,
  moonbit_string_t _M0L3keyS819
) {
  int32_t _M0L4hashS818;
  int32_t _M0L14capacity__maskS2385;
  int32_t _M0L6_2atmpS2384;
  int32_t _M0L1iS820;
  int32_t _M0L3idxS821;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS819);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS818 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS819);
  _M0L14capacity__maskS2385 = _M0L4selfS823->$3;
  _M0L6_2atmpS2384 = _M0L4hashS818 & _M0L14capacity__maskS2385;
  _M0L1iS820 = 0;
  _M0L3idxS821 = _M0L6_2atmpS2384;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3304 = _M0L4selfS823->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2383 = _M0L8_2afieldS3304;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3303;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS822;
    if (
      _M0L3idxS821 < 0
      || _M0L3idxS821 >= Moonbit_array_length(_M0L7entriesS2383)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3303
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2383[_M0L3idxS821];
    _M0L7_2abindS822 = _M0L6_2atmpS3303;
    if (_M0L7_2abindS822 == 0) {
      void* _M0L6_2atmpS2372;
      if (_M0L7_2abindS822) {
        moonbit_incref(_M0L7_2abindS822);
      }
      moonbit_decref(_M0L4selfS823);
      if (_M0L7_2abindS822) {
        moonbit_decref(_M0L7_2abindS822);
      }
      moonbit_decref(_M0L3keyS819);
      _M0L6_2atmpS2372 = 0;
      return _M0L6_2atmpS2372;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS824 = _M0L7_2abindS822;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aentryS825 = _M0L7_2aSomeS824;
      int32_t _M0L4hashS2374 = _M0L8_2aentryS825->$3;
      int32_t _if__result_3725;
      int32_t _M0L8_2afieldS3299;
      int32_t _M0L3pslS2377;
      int32_t _M0L6_2atmpS2379;
      int32_t _M0L6_2atmpS2381;
      int32_t _M0L14capacity__maskS2382;
      int32_t _M0L6_2atmpS2380;
      if (_M0L4hashS2374 == _M0L4hashS818) {
        moonbit_string_t _M0L8_2afieldS3302 = _M0L8_2aentryS825->$4;
        moonbit_string_t _M0L3keyS2373 = _M0L8_2afieldS3302;
        int32_t _M0L6_2atmpS3301;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3301
        = moonbit_val_array_equal(_M0L3keyS2373, _M0L3keyS819);
        _if__result_3725 = _M0L6_2atmpS3301;
      } else {
        _if__result_3725 = 0;
      }
      if (_if__result_3725) {
        void* _M0L8_2afieldS3300;
        int32_t _M0L6_2acntS3565;
        void* _M0L5valueS2376;
        void* _M0L6_2atmpS2375;
        moonbit_incref(_M0L8_2aentryS825);
        moonbit_decref(_M0L4selfS823);
        moonbit_decref(_M0L3keyS819);
        _M0L8_2afieldS3300 = _M0L8_2aentryS825->$5;
        _M0L6_2acntS3565 = Moonbit_object_header(_M0L8_2aentryS825)->rc;
        if (_M0L6_2acntS3565 > 1) {
          int32_t _M0L11_2anew__cntS3568 = _M0L6_2acntS3565 - 1;
          Moonbit_object_header(_M0L8_2aentryS825)->rc
          = _M0L11_2anew__cntS3568;
          moonbit_incref(_M0L8_2afieldS3300);
        } else if (_M0L6_2acntS3565 == 1) {
          moonbit_string_t _M0L8_2afieldS3567 = _M0L8_2aentryS825->$4;
          struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3566;
          moonbit_decref(_M0L8_2afieldS3567);
          _M0L8_2afieldS3566 = _M0L8_2aentryS825->$1;
          if (_M0L8_2afieldS3566) {
            moonbit_decref(_M0L8_2afieldS3566);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS825);
        }
        _M0L5valueS2376 = _M0L8_2afieldS3300;
        _M0L6_2atmpS2375 = _M0L5valueS2376;
        return _M0L6_2atmpS2375;
      } else {
        moonbit_incref(_M0L8_2aentryS825);
      }
      _M0L8_2afieldS3299 = _M0L8_2aentryS825->$2;
      moonbit_decref(_M0L8_2aentryS825);
      _M0L3pslS2377 = _M0L8_2afieldS3299;
      if (_M0L1iS820 > _M0L3pslS2377) {
        void* _M0L6_2atmpS2378;
        moonbit_decref(_M0L4selfS823);
        moonbit_decref(_M0L3keyS819);
        _M0L6_2atmpS2378 = 0;
        return _M0L6_2atmpS2378;
      }
      _M0L6_2atmpS2379 = _M0L1iS820 + 1;
      _M0L6_2atmpS2381 = _M0L3idxS821 + 1;
      _M0L14capacity__maskS2382 = _M0L4selfS823->$3;
      _M0L6_2atmpS2380 = _M0L6_2atmpS2381 & _M0L14capacity__maskS2382;
      _M0L1iS820 = _M0L6_2atmpS2379;
      _M0L3idxS821 = _M0L6_2atmpS2380;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS777
) {
  int32_t _M0L6lengthS776;
  int32_t _M0Lm8capacityS778;
  int32_t _M0L6_2atmpS2309;
  int32_t _M0L6_2atmpS2308;
  int32_t _M0L6_2atmpS2319;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS779;
  int32_t _M0L3endS2317;
  int32_t _M0L5startS2318;
  int32_t _M0L7_2abindS780;
  int32_t _M0L2__S781;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS777.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS776
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS777);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS778 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS776);
  _M0L6_2atmpS2309 = _M0Lm8capacityS778;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2308 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2309);
  if (_M0L6lengthS776 > _M0L6_2atmpS2308) {
    int32_t _M0L6_2atmpS2310 = _M0Lm8capacityS778;
    _M0Lm8capacityS778 = _M0L6_2atmpS2310 * 2;
  }
  _M0L6_2atmpS2319 = _M0Lm8capacityS778;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS779
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2319);
  _M0L3endS2317 = _M0L3arrS777.$2;
  _M0L5startS2318 = _M0L3arrS777.$1;
  _M0L7_2abindS780 = _M0L3endS2317 - _M0L5startS2318;
  _M0L2__S781 = 0;
  while (1) {
    if (_M0L2__S781 < _M0L7_2abindS780) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3308 =
        _M0L3arrS777.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2314 =
        _M0L8_2afieldS3308;
      int32_t _M0L5startS2316 = _M0L3arrS777.$1;
      int32_t _M0L6_2atmpS2315 = _M0L5startS2316 + _M0L2__S781;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3307 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2314[
          _M0L6_2atmpS2315
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS782 =
        _M0L6_2atmpS3307;
      moonbit_string_t _M0L8_2afieldS3306 = _M0L1eS782->$0;
      moonbit_string_t _M0L6_2atmpS2311 = _M0L8_2afieldS3306;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3305 =
        _M0L1eS782->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2312 =
        _M0L8_2afieldS3305;
      int32_t _M0L6_2atmpS2313;
      moonbit_incref(_M0L6_2atmpS2312);
      moonbit_incref(_M0L6_2atmpS2311);
      moonbit_incref(_M0L1mS779);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS779, _M0L6_2atmpS2311, _M0L6_2atmpS2312);
      _M0L6_2atmpS2313 = _M0L2__S781 + 1;
      _M0L2__S781 = _M0L6_2atmpS2313;
      continue;
    } else {
      moonbit_decref(_M0L3arrS777.$0);
    }
    break;
  }
  return _M0L1mS779;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS785
) {
  int32_t _M0L6lengthS784;
  int32_t _M0Lm8capacityS786;
  int32_t _M0L6_2atmpS2321;
  int32_t _M0L6_2atmpS2320;
  int32_t _M0L6_2atmpS2331;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS787;
  int32_t _M0L3endS2329;
  int32_t _M0L5startS2330;
  int32_t _M0L7_2abindS788;
  int32_t _M0L2__S789;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS785.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS784
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS785);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS786 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS784);
  _M0L6_2atmpS2321 = _M0Lm8capacityS786;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2320 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2321);
  if (_M0L6lengthS784 > _M0L6_2atmpS2320) {
    int32_t _M0L6_2atmpS2322 = _M0Lm8capacityS786;
    _M0Lm8capacityS786 = _M0L6_2atmpS2322 * 2;
  }
  _M0L6_2atmpS2331 = _M0Lm8capacityS786;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS787
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2331);
  _M0L3endS2329 = _M0L3arrS785.$2;
  _M0L5startS2330 = _M0L3arrS785.$1;
  _M0L7_2abindS788 = _M0L3endS2329 - _M0L5startS2330;
  _M0L2__S789 = 0;
  while (1) {
    if (_M0L2__S789 < _M0L7_2abindS788) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3311 =
        _M0L3arrS785.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2326 =
        _M0L8_2afieldS3311;
      int32_t _M0L5startS2328 = _M0L3arrS785.$1;
      int32_t _M0L6_2atmpS2327 = _M0L5startS2328 + _M0L2__S789;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3310 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2326[
          _M0L6_2atmpS2327
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS790 = _M0L6_2atmpS3310;
      int32_t _M0L6_2atmpS2323 = _M0L1eS790->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3309 =
        _M0L1eS790->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2324 =
        _M0L8_2afieldS3309;
      int32_t _M0L6_2atmpS2325;
      moonbit_incref(_M0L6_2atmpS2324);
      moonbit_incref(_M0L1mS787);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS787, _M0L6_2atmpS2323, _M0L6_2atmpS2324);
      _M0L6_2atmpS2325 = _M0L2__S789 + 1;
      _M0L2__S789 = _M0L6_2atmpS2325;
      continue;
    } else {
      moonbit_decref(_M0L3arrS785.$0);
    }
    break;
  }
  return _M0L1mS787;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS793
) {
  int32_t _M0L6lengthS792;
  int32_t _M0Lm8capacityS794;
  int32_t _M0L6_2atmpS2333;
  int32_t _M0L6_2atmpS2332;
  int32_t _M0L6_2atmpS2343;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS795;
  int32_t _M0L3endS2341;
  int32_t _M0L5startS2342;
  int32_t _M0L7_2abindS796;
  int32_t _M0L2__S797;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS793.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS792 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS793);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS794 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS792);
  _M0L6_2atmpS2333 = _M0Lm8capacityS794;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2332 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2333);
  if (_M0L6lengthS792 > _M0L6_2atmpS2332) {
    int32_t _M0L6_2atmpS2334 = _M0Lm8capacityS794;
    _M0Lm8capacityS794 = _M0L6_2atmpS2334 * 2;
  }
  _M0L6_2atmpS2343 = _M0Lm8capacityS794;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS795 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2343);
  _M0L3endS2341 = _M0L3arrS793.$2;
  _M0L5startS2342 = _M0L3arrS793.$1;
  _M0L7_2abindS796 = _M0L3endS2341 - _M0L5startS2342;
  _M0L2__S797 = 0;
  while (1) {
    if (_M0L2__S797 < _M0L7_2abindS796) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS3315 = _M0L3arrS793.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2338 = _M0L8_2afieldS3315;
      int32_t _M0L5startS2340 = _M0L3arrS793.$1;
      int32_t _M0L6_2atmpS2339 = _M0L5startS2340 + _M0L2__S797;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS3314 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2338[_M0L6_2atmpS2339];
      struct _M0TUsRPB4JsonE* _M0L1eS798 = _M0L6_2atmpS3314;
      moonbit_string_t _M0L8_2afieldS3313 = _M0L1eS798->$0;
      moonbit_string_t _M0L6_2atmpS2335 = _M0L8_2afieldS3313;
      void* _M0L8_2afieldS3312 = _M0L1eS798->$1;
      void* _M0L6_2atmpS2336 = _M0L8_2afieldS3312;
      int32_t _M0L6_2atmpS2337;
      moonbit_incref(_M0L6_2atmpS2336);
      moonbit_incref(_M0L6_2atmpS2335);
      moonbit_incref(_M0L1mS795);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS795, _M0L6_2atmpS2335, _M0L6_2atmpS2336);
      _M0L6_2atmpS2337 = _M0L2__S797 + 1;
      _M0L2__S797 = _M0L6_2atmpS2337;
      continue;
    } else {
      moonbit_decref(_M0L3arrS793.$0);
    }
    break;
  }
  return _M0L1mS795;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS767,
  moonbit_string_t _M0L3keyS768,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS769
) {
  int32_t _M0L6_2atmpS2305;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS768);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2305 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS768);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS767, _M0L3keyS768, _M0L5valueS769, _M0L6_2atmpS2305);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS770,
  int32_t _M0L3keyS771,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS772
) {
  int32_t _M0L6_2atmpS2306;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2306 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS771);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS770, _M0L3keyS771, _M0L5valueS772, _M0L6_2atmpS2306);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS773,
  moonbit_string_t _M0L3keyS774,
  void* _M0L5valueS775
) {
  int32_t _M0L6_2atmpS2307;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS774);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2307 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS774);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS773, _M0L3keyS774, _M0L5valueS775, _M0L6_2atmpS2307);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS735
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3322;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS734;
  int32_t _M0L8capacityS2290;
  int32_t _M0L13new__capacityS736;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2285;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2284;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3321;
  int32_t _M0L6_2atmpS2286;
  int32_t _M0L8capacityS2288;
  int32_t _M0L6_2atmpS2287;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2289;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3320;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS737;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3322 = _M0L4selfS735->$5;
  _M0L9old__headS734 = _M0L8_2afieldS3322;
  _M0L8capacityS2290 = _M0L4selfS735->$2;
  _M0L13new__capacityS736 = _M0L8capacityS2290 << 1;
  _M0L6_2atmpS2285 = 0;
  _M0L6_2atmpS2284
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS736, _M0L6_2atmpS2285);
  _M0L6_2aoldS3321 = _M0L4selfS735->$0;
  if (_M0L9old__headS734) {
    moonbit_incref(_M0L9old__headS734);
  }
  moonbit_decref(_M0L6_2aoldS3321);
  _M0L4selfS735->$0 = _M0L6_2atmpS2284;
  _M0L4selfS735->$2 = _M0L13new__capacityS736;
  _M0L6_2atmpS2286 = _M0L13new__capacityS736 - 1;
  _M0L4selfS735->$3 = _M0L6_2atmpS2286;
  _M0L8capacityS2288 = _M0L4selfS735->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2287 = _M0FPB21calc__grow__threshold(_M0L8capacityS2288);
  _M0L4selfS735->$4 = _M0L6_2atmpS2287;
  _M0L4selfS735->$1 = 0;
  _M0L6_2atmpS2289 = 0;
  _M0L6_2aoldS3320 = _M0L4selfS735->$5;
  if (_M0L6_2aoldS3320) {
    moonbit_decref(_M0L6_2aoldS3320);
  }
  _M0L4selfS735->$5 = _M0L6_2atmpS2289;
  _M0L4selfS735->$6 = -1;
  _M0L8_2aparamS737 = _M0L9old__headS734;
  while (1) {
    if (_M0L8_2aparamS737 == 0) {
      if (_M0L8_2aparamS737) {
        moonbit_decref(_M0L8_2aparamS737);
      }
      moonbit_decref(_M0L4selfS735);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS738 =
        _M0L8_2aparamS737;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS739 =
        _M0L7_2aSomeS738;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3319 =
        _M0L4_2axS739->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS740 =
        _M0L8_2afieldS3319;
      moonbit_string_t _M0L8_2afieldS3318 = _M0L4_2axS739->$4;
      moonbit_string_t _M0L6_2akeyS741 = _M0L8_2afieldS3318;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3317 =
        _M0L4_2axS739->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS742 =
        _M0L8_2afieldS3317;
      int32_t _M0L8_2afieldS3316 = _M0L4_2axS739->$3;
      int32_t _M0L6_2acntS3569 = Moonbit_object_header(_M0L4_2axS739)->rc;
      int32_t _M0L7_2ahashS743;
      if (_M0L6_2acntS3569 > 1) {
        int32_t _M0L11_2anew__cntS3570 = _M0L6_2acntS3569 - 1;
        Moonbit_object_header(_M0L4_2axS739)->rc = _M0L11_2anew__cntS3570;
        moonbit_incref(_M0L8_2avalueS742);
        moonbit_incref(_M0L6_2akeyS741);
        if (_M0L7_2anextS740) {
          moonbit_incref(_M0L7_2anextS740);
        }
      } else if (_M0L6_2acntS3569 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS739);
      }
      _M0L7_2ahashS743 = _M0L8_2afieldS3316;
      moonbit_incref(_M0L4selfS735);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS735, _M0L6_2akeyS741, _M0L8_2avalueS742, _M0L7_2ahashS743);
      _M0L8_2aparamS737 = _M0L7_2anextS740;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS746
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3328;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS745;
  int32_t _M0L8capacityS2297;
  int32_t _M0L13new__capacityS747;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2292;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2291;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3327;
  int32_t _M0L6_2atmpS2293;
  int32_t _M0L8capacityS2295;
  int32_t _M0L6_2atmpS2294;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2296;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3326;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS748;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3328 = _M0L4selfS746->$5;
  _M0L9old__headS745 = _M0L8_2afieldS3328;
  _M0L8capacityS2297 = _M0L4selfS746->$2;
  _M0L13new__capacityS747 = _M0L8capacityS2297 << 1;
  _M0L6_2atmpS2292 = 0;
  _M0L6_2atmpS2291
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS747, _M0L6_2atmpS2292);
  _M0L6_2aoldS3327 = _M0L4selfS746->$0;
  if (_M0L9old__headS745) {
    moonbit_incref(_M0L9old__headS745);
  }
  moonbit_decref(_M0L6_2aoldS3327);
  _M0L4selfS746->$0 = _M0L6_2atmpS2291;
  _M0L4selfS746->$2 = _M0L13new__capacityS747;
  _M0L6_2atmpS2293 = _M0L13new__capacityS747 - 1;
  _M0L4selfS746->$3 = _M0L6_2atmpS2293;
  _M0L8capacityS2295 = _M0L4selfS746->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2294 = _M0FPB21calc__grow__threshold(_M0L8capacityS2295);
  _M0L4selfS746->$4 = _M0L6_2atmpS2294;
  _M0L4selfS746->$1 = 0;
  _M0L6_2atmpS2296 = 0;
  _M0L6_2aoldS3326 = _M0L4selfS746->$5;
  if (_M0L6_2aoldS3326) {
    moonbit_decref(_M0L6_2aoldS3326);
  }
  _M0L4selfS746->$5 = _M0L6_2atmpS2296;
  _M0L4selfS746->$6 = -1;
  _M0L8_2aparamS748 = _M0L9old__headS745;
  while (1) {
    if (_M0L8_2aparamS748 == 0) {
      if (_M0L8_2aparamS748) {
        moonbit_decref(_M0L8_2aparamS748);
      }
      moonbit_decref(_M0L4selfS746);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS749 =
        _M0L8_2aparamS748;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS750 =
        _M0L7_2aSomeS749;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3325 =
        _M0L4_2axS750->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS751 =
        _M0L8_2afieldS3325;
      int32_t _M0L6_2akeyS752 = _M0L4_2axS750->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3324 =
        _M0L4_2axS750->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS753 =
        _M0L8_2afieldS3324;
      int32_t _M0L8_2afieldS3323 = _M0L4_2axS750->$3;
      int32_t _M0L6_2acntS3571 = Moonbit_object_header(_M0L4_2axS750)->rc;
      int32_t _M0L7_2ahashS754;
      if (_M0L6_2acntS3571 > 1) {
        int32_t _M0L11_2anew__cntS3572 = _M0L6_2acntS3571 - 1;
        Moonbit_object_header(_M0L4_2axS750)->rc = _M0L11_2anew__cntS3572;
        moonbit_incref(_M0L8_2avalueS753);
        if (_M0L7_2anextS751) {
          moonbit_incref(_M0L7_2anextS751);
        }
      } else if (_M0L6_2acntS3571 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS750);
      }
      _M0L7_2ahashS754 = _M0L8_2afieldS3323;
      moonbit_incref(_M0L4selfS746);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS746, _M0L6_2akeyS752, _M0L8_2avalueS753, _M0L7_2ahashS754);
      _M0L8_2aparamS748 = _M0L7_2anextS751;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS757
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3335;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS756;
  int32_t _M0L8capacityS2304;
  int32_t _M0L13new__capacityS758;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2299;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2298;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS3334;
  int32_t _M0L6_2atmpS2300;
  int32_t _M0L8capacityS2302;
  int32_t _M0L6_2atmpS2301;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2303;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3333;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS759;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3335 = _M0L4selfS757->$5;
  _M0L9old__headS756 = _M0L8_2afieldS3335;
  _M0L8capacityS2304 = _M0L4selfS757->$2;
  _M0L13new__capacityS758 = _M0L8capacityS2304 << 1;
  _M0L6_2atmpS2299 = 0;
  _M0L6_2atmpS2298
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS758, _M0L6_2atmpS2299);
  _M0L6_2aoldS3334 = _M0L4selfS757->$0;
  if (_M0L9old__headS756) {
    moonbit_incref(_M0L9old__headS756);
  }
  moonbit_decref(_M0L6_2aoldS3334);
  _M0L4selfS757->$0 = _M0L6_2atmpS2298;
  _M0L4selfS757->$2 = _M0L13new__capacityS758;
  _M0L6_2atmpS2300 = _M0L13new__capacityS758 - 1;
  _M0L4selfS757->$3 = _M0L6_2atmpS2300;
  _M0L8capacityS2302 = _M0L4selfS757->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2301 = _M0FPB21calc__grow__threshold(_M0L8capacityS2302);
  _M0L4selfS757->$4 = _M0L6_2atmpS2301;
  _M0L4selfS757->$1 = 0;
  _M0L6_2atmpS2303 = 0;
  _M0L6_2aoldS3333 = _M0L4selfS757->$5;
  if (_M0L6_2aoldS3333) {
    moonbit_decref(_M0L6_2aoldS3333);
  }
  _M0L4selfS757->$5 = _M0L6_2atmpS2303;
  _M0L4selfS757->$6 = -1;
  _M0L8_2aparamS759 = _M0L9old__headS756;
  while (1) {
    if (_M0L8_2aparamS759 == 0) {
      if (_M0L8_2aparamS759) {
        moonbit_decref(_M0L8_2aparamS759);
      }
      moonbit_decref(_M0L4selfS757);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS760 = _M0L8_2aparamS759;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS761 = _M0L7_2aSomeS760;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3332 = _M0L4_2axS761->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS762 = _M0L8_2afieldS3332;
      moonbit_string_t _M0L8_2afieldS3331 = _M0L4_2axS761->$4;
      moonbit_string_t _M0L6_2akeyS763 = _M0L8_2afieldS3331;
      void* _M0L8_2afieldS3330 = _M0L4_2axS761->$5;
      void* _M0L8_2avalueS764 = _M0L8_2afieldS3330;
      int32_t _M0L8_2afieldS3329 = _M0L4_2axS761->$3;
      int32_t _M0L6_2acntS3573 = Moonbit_object_header(_M0L4_2axS761)->rc;
      int32_t _M0L7_2ahashS765;
      if (_M0L6_2acntS3573 > 1) {
        int32_t _M0L11_2anew__cntS3574 = _M0L6_2acntS3573 - 1;
        Moonbit_object_header(_M0L4_2axS761)->rc = _M0L11_2anew__cntS3574;
        moonbit_incref(_M0L8_2avalueS764);
        moonbit_incref(_M0L6_2akeyS763);
        if (_M0L7_2anextS762) {
          moonbit_incref(_M0L7_2anextS762);
        }
      } else if (_M0L6_2acntS3573 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS761);
      }
      _M0L7_2ahashS765 = _M0L8_2afieldS3329;
      moonbit_incref(_M0L4selfS757);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS757, _M0L6_2akeyS763, _M0L8_2avalueS764, _M0L7_2ahashS765);
      _M0L8_2aparamS759 = _M0L7_2anextS762;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS689,
  moonbit_string_t _M0L3keyS695,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS696,
  int32_t _M0L4hashS691
) {
  int32_t _M0L14capacity__maskS2247;
  int32_t _M0L6_2atmpS2246;
  int32_t _M0L3pslS686;
  int32_t _M0L3idxS687;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2247 = _M0L4selfS689->$3;
  _M0L6_2atmpS2246 = _M0L4hashS691 & _M0L14capacity__maskS2247;
  _M0L3pslS686 = 0;
  _M0L3idxS687 = _M0L6_2atmpS2246;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3340 =
      _M0L4selfS689->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2245 =
      _M0L8_2afieldS3340;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3339;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS688;
    if (
      _M0L3idxS687 < 0
      || _M0L3idxS687 >= Moonbit_array_length(_M0L7entriesS2245)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3339
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2245[
        _M0L3idxS687
      ];
    _M0L7_2abindS688 = _M0L6_2atmpS3339;
    if (_M0L7_2abindS688 == 0) {
      int32_t _M0L4sizeS2230 = _M0L4selfS689->$1;
      int32_t _M0L8grow__atS2231 = _M0L4selfS689->$4;
      int32_t _M0L7_2abindS692;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS693;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS694;
      if (_M0L4sizeS2230 >= _M0L8grow__atS2231) {
        int32_t _M0L14capacity__maskS2233;
        int32_t _M0L6_2atmpS2232;
        moonbit_incref(_M0L4selfS689);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689);
        _M0L14capacity__maskS2233 = _M0L4selfS689->$3;
        _M0L6_2atmpS2232 = _M0L4hashS691 & _M0L14capacity__maskS2233;
        _M0L3pslS686 = 0;
        _M0L3idxS687 = _M0L6_2atmpS2232;
        continue;
      }
      _M0L7_2abindS692 = _M0L4selfS689->$6;
      _M0L7_2abindS693 = 0;
      _M0L5entryS694
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS694)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS694->$0 = _M0L7_2abindS692;
      _M0L5entryS694->$1 = _M0L7_2abindS693;
      _M0L5entryS694->$2 = _M0L3pslS686;
      _M0L5entryS694->$3 = _M0L4hashS691;
      _M0L5entryS694->$4 = _M0L3keyS695;
      _M0L5entryS694->$5 = _M0L5valueS696;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689, _M0L3idxS687, _M0L5entryS694);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS697 =
        _M0L7_2abindS688;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS698 =
        _M0L7_2aSomeS697;
      int32_t _M0L4hashS2235 = _M0L14_2acurr__entryS698->$3;
      int32_t _if__result_3733;
      int32_t _M0L3pslS2236;
      int32_t _M0L6_2atmpS2241;
      int32_t _M0L6_2atmpS2243;
      int32_t _M0L14capacity__maskS2244;
      int32_t _M0L6_2atmpS2242;
      if (_M0L4hashS2235 == _M0L4hashS691) {
        moonbit_string_t _M0L8_2afieldS3338 = _M0L14_2acurr__entryS698->$4;
        moonbit_string_t _M0L3keyS2234 = _M0L8_2afieldS3338;
        int32_t _M0L6_2atmpS3337;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3337
        = moonbit_val_array_equal(_M0L3keyS2234, _M0L3keyS695);
        _if__result_3733 = _M0L6_2atmpS3337;
      } else {
        _if__result_3733 = 0;
      }
      if (_if__result_3733) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3336;
        moonbit_incref(_M0L14_2acurr__entryS698);
        moonbit_decref(_M0L3keyS695);
        moonbit_decref(_M0L4selfS689);
        _M0L6_2aoldS3336 = _M0L14_2acurr__entryS698->$5;
        moonbit_decref(_M0L6_2aoldS3336);
        _M0L14_2acurr__entryS698->$5 = _M0L5valueS696;
        moonbit_decref(_M0L14_2acurr__entryS698);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS698);
      }
      _M0L3pslS2236 = _M0L14_2acurr__entryS698->$2;
      if (_M0L3pslS686 > _M0L3pslS2236) {
        int32_t _M0L4sizeS2237 = _M0L4selfS689->$1;
        int32_t _M0L8grow__atS2238 = _M0L4selfS689->$4;
        int32_t _M0L7_2abindS699;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS700;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS701;
        if (_M0L4sizeS2237 >= _M0L8grow__atS2238) {
          int32_t _M0L14capacity__maskS2240;
          int32_t _M0L6_2atmpS2239;
          moonbit_decref(_M0L14_2acurr__entryS698);
          moonbit_incref(_M0L4selfS689);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689);
          _M0L14capacity__maskS2240 = _M0L4selfS689->$3;
          _M0L6_2atmpS2239 = _M0L4hashS691 & _M0L14capacity__maskS2240;
          _M0L3pslS686 = 0;
          _M0L3idxS687 = _M0L6_2atmpS2239;
          continue;
        }
        moonbit_incref(_M0L4selfS689);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689, _M0L3idxS687, _M0L14_2acurr__entryS698);
        _M0L7_2abindS699 = _M0L4selfS689->$6;
        _M0L7_2abindS700 = 0;
        _M0L5entryS701
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS701)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS701->$0 = _M0L7_2abindS699;
        _M0L5entryS701->$1 = _M0L7_2abindS700;
        _M0L5entryS701->$2 = _M0L3pslS686;
        _M0L5entryS701->$3 = _M0L4hashS691;
        _M0L5entryS701->$4 = _M0L3keyS695;
        _M0L5entryS701->$5 = _M0L5valueS696;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS689, _M0L3idxS687, _M0L5entryS701);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS698);
      }
      _M0L6_2atmpS2241 = _M0L3pslS686 + 1;
      _M0L6_2atmpS2243 = _M0L3idxS687 + 1;
      _M0L14capacity__maskS2244 = _M0L4selfS689->$3;
      _M0L6_2atmpS2242 = _M0L6_2atmpS2243 & _M0L14capacity__maskS2244;
      _M0L3pslS686 = _M0L6_2atmpS2241;
      _M0L3idxS687 = _M0L6_2atmpS2242;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS705,
  int32_t _M0L3keyS711,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS712,
  int32_t _M0L4hashS707
) {
  int32_t _M0L14capacity__maskS2265;
  int32_t _M0L6_2atmpS2264;
  int32_t _M0L3pslS702;
  int32_t _M0L3idxS703;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2265 = _M0L4selfS705->$3;
  _M0L6_2atmpS2264 = _M0L4hashS707 & _M0L14capacity__maskS2265;
  _M0L3pslS702 = 0;
  _M0L3idxS703 = _M0L6_2atmpS2264;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3343 =
      _M0L4selfS705->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2263 =
      _M0L8_2afieldS3343;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3342;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS704;
    if (
      _M0L3idxS703 < 0
      || _M0L3idxS703 >= Moonbit_array_length(_M0L7entriesS2263)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3342
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2263[
        _M0L3idxS703
      ];
    _M0L7_2abindS704 = _M0L6_2atmpS3342;
    if (_M0L7_2abindS704 == 0) {
      int32_t _M0L4sizeS2248 = _M0L4selfS705->$1;
      int32_t _M0L8grow__atS2249 = _M0L4selfS705->$4;
      int32_t _M0L7_2abindS708;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS709;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS710;
      if (_M0L4sizeS2248 >= _M0L8grow__atS2249) {
        int32_t _M0L14capacity__maskS2251;
        int32_t _M0L6_2atmpS2250;
        moonbit_incref(_M0L4selfS705);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705);
        _M0L14capacity__maskS2251 = _M0L4selfS705->$3;
        _M0L6_2atmpS2250 = _M0L4hashS707 & _M0L14capacity__maskS2251;
        _M0L3pslS702 = 0;
        _M0L3idxS703 = _M0L6_2atmpS2250;
        continue;
      }
      _M0L7_2abindS708 = _M0L4selfS705->$6;
      _M0L7_2abindS709 = 0;
      _M0L5entryS710
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS710)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS710->$0 = _M0L7_2abindS708;
      _M0L5entryS710->$1 = _M0L7_2abindS709;
      _M0L5entryS710->$2 = _M0L3pslS702;
      _M0L5entryS710->$3 = _M0L4hashS707;
      _M0L5entryS710->$4 = _M0L3keyS711;
      _M0L5entryS710->$5 = _M0L5valueS712;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705, _M0L3idxS703, _M0L5entryS710);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS713 =
        _M0L7_2abindS704;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS714 =
        _M0L7_2aSomeS713;
      int32_t _M0L4hashS2253 = _M0L14_2acurr__entryS714->$3;
      int32_t _if__result_3735;
      int32_t _M0L3pslS2254;
      int32_t _M0L6_2atmpS2259;
      int32_t _M0L6_2atmpS2261;
      int32_t _M0L14capacity__maskS2262;
      int32_t _M0L6_2atmpS2260;
      if (_M0L4hashS2253 == _M0L4hashS707) {
        int32_t _M0L3keyS2252 = _M0L14_2acurr__entryS714->$4;
        _if__result_3735 = _M0L3keyS2252 == _M0L3keyS711;
      } else {
        _if__result_3735 = 0;
      }
      if (_if__result_3735) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3341;
        moonbit_incref(_M0L14_2acurr__entryS714);
        moonbit_decref(_M0L4selfS705);
        _M0L6_2aoldS3341 = _M0L14_2acurr__entryS714->$5;
        moonbit_decref(_M0L6_2aoldS3341);
        _M0L14_2acurr__entryS714->$5 = _M0L5valueS712;
        moonbit_decref(_M0L14_2acurr__entryS714);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS714);
      }
      _M0L3pslS2254 = _M0L14_2acurr__entryS714->$2;
      if (_M0L3pslS702 > _M0L3pslS2254) {
        int32_t _M0L4sizeS2255 = _M0L4selfS705->$1;
        int32_t _M0L8grow__atS2256 = _M0L4selfS705->$4;
        int32_t _M0L7_2abindS715;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS716;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS717;
        if (_M0L4sizeS2255 >= _M0L8grow__atS2256) {
          int32_t _M0L14capacity__maskS2258;
          int32_t _M0L6_2atmpS2257;
          moonbit_decref(_M0L14_2acurr__entryS714);
          moonbit_incref(_M0L4selfS705);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705);
          _M0L14capacity__maskS2258 = _M0L4selfS705->$3;
          _M0L6_2atmpS2257 = _M0L4hashS707 & _M0L14capacity__maskS2258;
          _M0L3pslS702 = 0;
          _M0L3idxS703 = _M0L6_2atmpS2257;
          continue;
        }
        moonbit_incref(_M0L4selfS705);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705, _M0L3idxS703, _M0L14_2acurr__entryS714);
        _M0L7_2abindS715 = _M0L4selfS705->$6;
        _M0L7_2abindS716 = 0;
        _M0L5entryS717
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS717)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS717->$0 = _M0L7_2abindS715;
        _M0L5entryS717->$1 = _M0L7_2abindS716;
        _M0L5entryS717->$2 = _M0L3pslS702;
        _M0L5entryS717->$3 = _M0L4hashS707;
        _M0L5entryS717->$4 = _M0L3keyS711;
        _M0L5entryS717->$5 = _M0L5valueS712;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS705, _M0L3idxS703, _M0L5entryS717);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS714);
      }
      _M0L6_2atmpS2259 = _M0L3pslS702 + 1;
      _M0L6_2atmpS2261 = _M0L3idxS703 + 1;
      _M0L14capacity__maskS2262 = _M0L4selfS705->$3;
      _M0L6_2atmpS2260 = _M0L6_2atmpS2261 & _M0L14capacity__maskS2262;
      _M0L3pslS702 = _M0L6_2atmpS2259;
      _M0L3idxS703 = _M0L6_2atmpS2260;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS721,
  moonbit_string_t _M0L3keyS727,
  void* _M0L5valueS728,
  int32_t _M0L4hashS723
) {
  int32_t _M0L14capacity__maskS2283;
  int32_t _M0L6_2atmpS2282;
  int32_t _M0L3pslS718;
  int32_t _M0L3idxS719;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2283 = _M0L4selfS721->$3;
  _M0L6_2atmpS2282 = _M0L4hashS723 & _M0L14capacity__maskS2283;
  _M0L3pslS718 = 0;
  _M0L3idxS719 = _M0L6_2atmpS2282;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3348 = _M0L4selfS721->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2281 = _M0L8_2afieldS3348;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3347;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS720;
    if (
      _M0L3idxS719 < 0
      || _M0L3idxS719 >= Moonbit_array_length(_M0L7entriesS2281)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3347
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2281[_M0L3idxS719];
    _M0L7_2abindS720 = _M0L6_2atmpS3347;
    if (_M0L7_2abindS720 == 0) {
      int32_t _M0L4sizeS2266 = _M0L4selfS721->$1;
      int32_t _M0L8grow__atS2267 = _M0L4selfS721->$4;
      int32_t _M0L7_2abindS724;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS725;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS726;
      if (_M0L4sizeS2266 >= _M0L8grow__atS2267) {
        int32_t _M0L14capacity__maskS2269;
        int32_t _M0L6_2atmpS2268;
        moonbit_incref(_M0L4selfS721);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS721);
        _M0L14capacity__maskS2269 = _M0L4selfS721->$3;
        _M0L6_2atmpS2268 = _M0L4hashS723 & _M0L14capacity__maskS2269;
        _M0L3pslS718 = 0;
        _M0L3idxS719 = _M0L6_2atmpS2268;
        continue;
      }
      _M0L7_2abindS724 = _M0L4selfS721->$6;
      _M0L7_2abindS725 = 0;
      _M0L5entryS726
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS726)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS726->$0 = _M0L7_2abindS724;
      _M0L5entryS726->$1 = _M0L7_2abindS725;
      _M0L5entryS726->$2 = _M0L3pslS718;
      _M0L5entryS726->$3 = _M0L4hashS723;
      _M0L5entryS726->$4 = _M0L3keyS727;
      _M0L5entryS726->$5 = _M0L5valueS728;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS721, _M0L3idxS719, _M0L5entryS726);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS729 = _M0L7_2abindS720;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS730 =
        _M0L7_2aSomeS729;
      int32_t _M0L4hashS2271 = _M0L14_2acurr__entryS730->$3;
      int32_t _if__result_3737;
      int32_t _M0L3pslS2272;
      int32_t _M0L6_2atmpS2277;
      int32_t _M0L6_2atmpS2279;
      int32_t _M0L14capacity__maskS2280;
      int32_t _M0L6_2atmpS2278;
      if (_M0L4hashS2271 == _M0L4hashS723) {
        moonbit_string_t _M0L8_2afieldS3346 = _M0L14_2acurr__entryS730->$4;
        moonbit_string_t _M0L3keyS2270 = _M0L8_2afieldS3346;
        int32_t _M0L6_2atmpS3345;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3345
        = moonbit_val_array_equal(_M0L3keyS2270, _M0L3keyS727);
        _if__result_3737 = _M0L6_2atmpS3345;
      } else {
        _if__result_3737 = 0;
      }
      if (_if__result_3737) {
        void* _M0L6_2aoldS3344;
        moonbit_incref(_M0L14_2acurr__entryS730);
        moonbit_decref(_M0L3keyS727);
        moonbit_decref(_M0L4selfS721);
        _M0L6_2aoldS3344 = _M0L14_2acurr__entryS730->$5;
        moonbit_decref(_M0L6_2aoldS3344);
        _M0L14_2acurr__entryS730->$5 = _M0L5valueS728;
        moonbit_decref(_M0L14_2acurr__entryS730);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS730);
      }
      _M0L3pslS2272 = _M0L14_2acurr__entryS730->$2;
      if (_M0L3pslS718 > _M0L3pslS2272) {
        int32_t _M0L4sizeS2273 = _M0L4selfS721->$1;
        int32_t _M0L8grow__atS2274 = _M0L4selfS721->$4;
        int32_t _M0L7_2abindS731;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS732;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS733;
        if (_M0L4sizeS2273 >= _M0L8grow__atS2274) {
          int32_t _M0L14capacity__maskS2276;
          int32_t _M0L6_2atmpS2275;
          moonbit_decref(_M0L14_2acurr__entryS730);
          moonbit_incref(_M0L4selfS721);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS721);
          _M0L14capacity__maskS2276 = _M0L4selfS721->$3;
          _M0L6_2atmpS2275 = _M0L4hashS723 & _M0L14capacity__maskS2276;
          _M0L3pslS718 = 0;
          _M0L3idxS719 = _M0L6_2atmpS2275;
          continue;
        }
        moonbit_incref(_M0L4selfS721);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS721, _M0L3idxS719, _M0L14_2acurr__entryS730);
        _M0L7_2abindS731 = _M0L4selfS721->$6;
        _M0L7_2abindS732 = 0;
        _M0L5entryS733
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS733)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS733->$0 = _M0L7_2abindS731;
        _M0L5entryS733->$1 = _M0L7_2abindS732;
        _M0L5entryS733->$2 = _M0L3pslS718;
        _M0L5entryS733->$3 = _M0L4hashS723;
        _M0L5entryS733->$4 = _M0L3keyS727;
        _M0L5entryS733->$5 = _M0L5valueS728;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS721, _M0L3idxS719, _M0L5entryS733);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS730);
      }
      _M0L6_2atmpS2277 = _M0L3pslS718 + 1;
      _M0L6_2atmpS2279 = _M0L3idxS719 + 1;
      _M0L14capacity__maskS2280 = _M0L4selfS721->$3;
      _M0L6_2atmpS2278 = _M0L6_2atmpS2279 & _M0L14capacity__maskS2280;
      _M0L3pslS718 = _M0L6_2atmpS2277;
      _M0L3idxS719 = _M0L6_2atmpS2278;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS660,
  int32_t _M0L3idxS665,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS664
) {
  int32_t _M0L3pslS2197;
  int32_t _M0L6_2atmpS2193;
  int32_t _M0L6_2atmpS2195;
  int32_t _M0L14capacity__maskS2196;
  int32_t _M0L6_2atmpS2194;
  int32_t _M0L3pslS656;
  int32_t _M0L3idxS657;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS658;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2197 = _M0L5entryS664->$2;
  _M0L6_2atmpS2193 = _M0L3pslS2197 + 1;
  _M0L6_2atmpS2195 = _M0L3idxS665 + 1;
  _M0L14capacity__maskS2196 = _M0L4selfS660->$3;
  _M0L6_2atmpS2194 = _M0L6_2atmpS2195 & _M0L14capacity__maskS2196;
  _M0L3pslS656 = _M0L6_2atmpS2193;
  _M0L3idxS657 = _M0L6_2atmpS2194;
  _M0L5entryS658 = _M0L5entryS664;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3350 =
      _M0L4selfS660->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2192 =
      _M0L8_2afieldS3350;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3349;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS659;
    if (
      _M0L3idxS657 < 0
      || _M0L3idxS657 >= Moonbit_array_length(_M0L7entriesS2192)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3349
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2192[
        _M0L3idxS657
      ];
    _M0L7_2abindS659 = _M0L6_2atmpS3349;
    if (_M0L7_2abindS659 == 0) {
      _M0L5entryS658->$2 = _M0L3pslS656;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS660, _M0L5entryS658, _M0L3idxS657);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS662 =
        _M0L7_2abindS659;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS663 =
        _M0L7_2aSomeS662;
      int32_t _M0L3pslS2182 = _M0L14_2acurr__entryS663->$2;
      if (_M0L3pslS656 > _M0L3pslS2182) {
        int32_t _M0L3pslS2187;
        int32_t _M0L6_2atmpS2183;
        int32_t _M0L6_2atmpS2185;
        int32_t _M0L14capacity__maskS2186;
        int32_t _M0L6_2atmpS2184;
        _M0L5entryS658->$2 = _M0L3pslS656;
        moonbit_incref(_M0L14_2acurr__entryS663);
        moonbit_incref(_M0L4selfS660);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS660, _M0L5entryS658, _M0L3idxS657);
        _M0L3pslS2187 = _M0L14_2acurr__entryS663->$2;
        _M0L6_2atmpS2183 = _M0L3pslS2187 + 1;
        _M0L6_2atmpS2185 = _M0L3idxS657 + 1;
        _M0L14capacity__maskS2186 = _M0L4selfS660->$3;
        _M0L6_2atmpS2184 = _M0L6_2atmpS2185 & _M0L14capacity__maskS2186;
        _M0L3pslS656 = _M0L6_2atmpS2183;
        _M0L3idxS657 = _M0L6_2atmpS2184;
        _M0L5entryS658 = _M0L14_2acurr__entryS663;
        continue;
      } else {
        int32_t _M0L6_2atmpS2188 = _M0L3pslS656 + 1;
        int32_t _M0L6_2atmpS2190 = _M0L3idxS657 + 1;
        int32_t _M0L14capacity__maskS2191 = _M0L4selfS660->$3;
        int32_t _M0L6_2atmpS2189 =
          _M0L6_2atmpS2190 & _M0L14capacity__maskS2191;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3739 =
          _M0L5entryS658;
        _M0L3pslS656 = _M0L6_2atmpS2188;
        _M0L3idxS657 = _M0L6_2atmpS2189;
        _M0L5entryS658 = _tmp_3739;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS670,
  int32_t _M0L3idxS675,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS674
) {
  int32_t _M0L3pslS2213;
  int32_t _M0L6_2atmpS2209;
  int32_t _M0L6_2atmpS2211;
  int32_t _M0L14capacity__maskS2212;
  int32_t _M0L6_2atmpS2210;
  int32_t _M0L3pslS666;
  int32_t _M0L3idxS667;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS668;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2213 = _M0L5entryS674->$2;
  _M0L6_2atmpS2209 = _M0L3pslS2213 + 1;
  _M0L6_2atmpS2211 = _M0L3idxS675 + 1;
  _M0L14capacity__maskS2212 = _M0L4selfS670->$3;
  _M0L6_2atmpS2210 = _M0L6_2atmpS2211 & _M0L14capacity__maskS2212;
  _M0L3pslS666 = _M0L6_2atmpS2209;
  _M0L3idxS667 = _M0L6_2atmpS2210;
  _M0L5entryS668 = _M0L5entryS674;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3352 =
      _M0L4selfS670->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2208 =
      _M0L8_2afieldS3352;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3351;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS669;
    if (
      _M0L3idxS667 < 0
      || _M0L3idxS667 >= Moonbit_array_length(_M0L7entriesS2208)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3351
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2208[
        _M0L3idxS667
      ];
    _M0L7_2abindS669 = _M0L6_2atmpS3351;
    if (_M0L7_2abindS669 == 0) {
      _M0L5entryS668->$2 = _M0L3pslS666;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS670, _M0L5entryS668, _M0L3idxS667);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS672 =
        _M0L7_2abindS669;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS673 =
        _M0L7_2aSomeS672;
      int32_t _M0L3pslS2198 = _M0L14_2acurr__entryS673->$2;
      if (_M0L3pslS666 > _M0L3pslS2198) {
        int32_t _M0L3pslS2203;
        int32_t _M0L6_2atmpS2199;
        int32_t _M0L6_2atmpS2201;
        int32_t _M0L14capacity__maskS2202;
        int32_t _M0L6_2atmpS2200;
        _M0L5entryS668->$2 = _M0L3pslS666;
        moonbit_incref(_M0L14_2acurr__entryS673);
        moonbit_incref(_M0L4selfS670);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS670, _M0L5entryS668, _M0L3idxS667);
        _M0L3pslS2203 = _M0L14_2acurr__entryS673->$2;
        _M0L6_2atmpS2199 = _M0L3pslS2203 + 1;
        _M0L6_2atmpS2201 = _M0L3idxS667 + 1;
        _M0L14capacity__maskS2202 = _M0L4selfS670->$3;
        _M0L6_2atmpS2200 = _M0L6_2atmpS2201 & _M0L14capacity__maskS2202;
        _M0L3pslS666 = _M0L6_2atmpS2199;
        _M0L3idxS667 = _M0L6_2atmpS2200;
        _M0L5entryS668 = _M0L14_2acurr__entryS673;
        continue;
      } else {
        int32_t _M0L6_2atmpS2204 = _M0L3pslS666 + 1;
        int32_t _M0L6_2atmpS2206 = _M0L3idxS667 + 1;
        int32_t _M0L14capacity__maskS2207 = _M0L4selfS670->$3;
        int32_t _M0L6_2atmpS2205 =
          _M0L6_2atmpS2206 & _M0L14capacity__maskS2207;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3741 =
          _M0L5entryS668;
        _M0L3pslS666 = _M0L6_2atmpS2204;
        _M0L3idxS667 = _M0L6_2atmpS2205;
        _M0L5entryS668 = _tmp_3741;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS680,
  int32_t _M0L3idxS685,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS684
) {
  int32_t _M0L3pslS2229;
  int32_t _M0L6_2atmpS2225;
  int32_t _M0L6_2atmpS2227;
  int32_t _M0L14capacity__maskS2228;
  int32_t _M0L6_2atmpS2226;
  int32_t _M0L3pslS676;
  int32_t _M0L3idxS677;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS678;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2229 = _M0L5entryS684->$2;
  _M0L6_2atmpS2225 = _M0L3pslS2229 + 1;
  _M0L6_2atmpS2227 = _M0L3idxS685 + 1;
  _M0L14capacity__maskS2228 = _M0L4selfS680->$3;
  _M0L6_2atmpS2226 = _M0L6_2atmpS2227 & _M0L14capacity__maskS2228;
  _M0L3pslS676 = _M0L6_2atmpS2225;
  _M0L3idxS677 = _M0L6_2atmpS2226;
  _M0L5entryS678 = _M0L5entryS684;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3354 = _M0L4selfS680->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2224 = _M0L8_2afieldS3354;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3353;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS679;
    if (
      _M0L3idxS677 < 0
      || _M0L3idxS677 >= Moonbit_array_length(_M0L7entriesS2224)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3353
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2224[_M0L3idxS677];
    _M0L7_2abindS679 = _M0L6_2atmpS3353;
    if (_M0L7_2abindS679 == 0) {
      _M0L5entryS678->$2 = _M0L3pslS676;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS680, _M0L5entryS678, _M0L3idxS677);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS682 = _M0L7_2abindS679;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS683 =
        _M0L7_2aSomeS682;
      int32_t _M0L3pslS2214 = _M0L14_2acurr__entryS683->$2;
      if (_M0L3pslS676 > _M0L3pslS2214) {
        int32_t _M0L3pslS2219;
        int32_t _M0L6_2atmpS2215;
        int32_t _M0L6_2atmpS2217;
        int32_t _M0L14capacity__maskS2218;
        int32_t _M0L6_2atmpS2216;
        _M0L5entryS678->$2 = _M0L3pslS676;
        moonbit_incref(_M0L14_2acurr__entryS683);
        moonbit_incref(_M0L4selfS680);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS680, _M0L5entryS678, _M0L3idxS677);
        _M0L3pslS2219 = _M0L14_2acurr__entryS683->$2;
        _M0L6_2atmpS2215 = _M0L3pslS2219 + 1;
        _M0L6_2atmpS2217 = _M0L3idxS677 + 1;
        _M0L14capacity__maskS2218 = _M0L4selfS680->$3;
        _M0L6_2atmpS2216 = _M0L6_2atmpS2217 & _M0L14capacity__maskS2218;
        _M0L3pslS676 = _M0L6_2atmpS2215;
        _M0L3idxS677 = _M0L6_2atmpS2216;
        _M0L5entryS678 = _M0L14_2acurr__entryS683;
        continue;
      } else {
        int32_t _M0L6_2atmpS2220 = _M0L3pslS676 + 1;
        int32_t _M0L6_2atmpS2222 = _M0L3idxS677 + 1;
        int32_t _M0L14capacity__maskS2223 = _M0L4selfS680->$3;
        int32_t _M0L6_2atmpS2221 =
          _M0L6_2atmpS2222 & _M0L14capacity__maskS2223;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_3743 = _M0L5entryS678;
        _M0L3pslS676 = _M0L6_2atmpS2220;
        _M0L3idxS677 = _M0L6_2atmpS2221;
        _M0L5entryS678 = _tmp_3743;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS638,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS640,
  int32_t _M0L8new__idxS639
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3357;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2176;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2177;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3356;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3355;
  int32_t _M0L6_2acntS3575;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS641;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3357 = _M0L4selfS638->$0;
  _M0L7entriesS2176 = _M0L8_2afieldS3357;
  moonbit_incref(_M0L5entryS640);
  _M0L6_2atmpS2177 = _M0L5entryS640;
  if (
    _M0L8new__idxS639 < 0
    || _M0L8new__idxS639 >= Moonbit_array_length(_M0L7entriesS2176)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3356
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2176[
      _M0L8new__idxS639
    ];
  if (_M0L6_2aoldS3356) {
    moonbit_decref(_M0L6_2aoldS3356);
  }
  _M0L7entriesS2176[_M0L8new__idxS639] = _M0L6_2atmpS2177;
  _M0L8_2afieldS3355 = _M0L5entryS640->$1;
  _M0L6_2acntS3575 = Moonbit_object_header(_M0L5entryS640)->rc;
  if (_M0L6_2acntS3575 > 1) {
    int32_t _M0L11_2anew__cntS3578 = _M0L6_2acntS3575 - 1;
    Moonbit_object_header(_M0L5entryS640)->rc = _M0L11_2anew__cntS3578;
    if (_M0L8_2afieldS3355) {
      moonbit_incref(_M0L8_2afieldS3355);
    }
  } else if (_M0L6_2acntS3575 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3577 =
      _M0L5entryS640->$5;
    moonbit_string_t _M0L8_2afieldS3576;
    moonbit_decref(_M0L8_2afieldS3577);
    _M0L8_2afieldS3576 = _M0L5entryS640->$4;
    moonbit_decref(_M0L8_2afieldS3576);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS640);
  }
  _M0L7_2abindS641 = _M0L8_2afieldS3355;
  if (_M0L7_2abindS641 == 0) {
    if (_M0L7_2abindS641) {
      moonbit_decref(_M0L7_2abindS641);
    }
    _M0L4selfS638->$6 = _M0L8new__idxS639;
    moonbit_decref(_M0L4selfS638);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS642;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS643;
    moonbit_decref(_M0L4selfS638);
    _M0L7_2aSomeS642 = _M0L7_2abindS641;
    _M0L7_2anextS643 = _M0L7_2aSomeS642;
    _M0L7_2anextS643->$0 = _M0L8new__idxS639;
    moonbit_decref(_M0L7_2anextS643);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS644,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS646,
  int32_t _M0L8new__idxS645
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3360;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2178;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2179;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3359;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3358;
  int32_t _M0L6_2acntS3579;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS647;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3360 = _M0L4selfS644->$0;
  _M0L7entriesS2178 = _M0L8_2afieldS3360;
  moonbit_incref(_M0L5entryS646);
  _M0L6_2atmpS2179 = _M0L5entryS646;
  if (
    _M0L8new__idxS645 < 0
    || _M0L8new__idxS645 >= Moonbit_array_length(_M0L7entriesS2178)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3359
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2178[
      _M0L8new__idxS645
    ];
  if (_M0L6_2aoldS3359) {
    moonbit_decref(_M0L6_2aoldS3359);
  }
  _M0L7entriesS2178[_M0L8new__idxS645] = _M0L6_2atmpS2179;
  _M0L8_2afieldS3358 = _M0L5entryS646->$1;
  _M0L6_2acntS3579 = Moonbit_object_header(_M0L5entryS646)->rc;
  if (_M0L6_2acntS3579 > 1) {
    int32_t _M0L11_2anew__cntS3581 = _M0L6_2acntS3579 - 1;
    Moonbit_object_header(_M0L5entryS646)->rc = _M0L11_2anew__cntS3581;
    if (_M0L8_2afieldS3358) {
      moonbit_incref(_M0L8_2afieldS3358);
    }
  } else if (_M0L6_2acntS3579 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3580 =
      _M0L5entryS646->$5;
    moonbit_decref(_M0L8_2afieldS3580);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS646);
  }
  _M0L7_2abindS647 = _M0L8_2afieldS3358;
  if (_M0L7_2abindS647 == 0) {
    if (_M0L7_2abindS647) {
      moonbit_decref(_M0L7_2abindS647);
    }
    _M0L4selfS644->$6 = _M0L8new__idxS645;
    moonbit_decref(_M0L4selfS644);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS648;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS649;
    moonbit_decref(_M0L4selfS644);
    _M0L7_2aSomeS648 = _M0L7_2abindS647;
    _M0L7_2anextS649 = _M0L7_2aSomeS648;
    _M0L7_2anextS649->$0 = _M0L8new__idxS645;
    moonbit_decref(_M0L7_2anextS649);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS650,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS652,
  int32_t _M0L8new__idxS651
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3363;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2180;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2181;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3362;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3361;
  int32_t _M0L6_2acntS3582;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS653;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3363 = _M0L4selfS650->$0;
  _M0L7entriesS2180 = _M0L8_2afieldS3363;
  moonbit_incref(_M0L5entryS652);
  _M0L6_2atmpS2181 = _M0L5entryS652;
  if (
    _M0L8new__idxS651 < 0
    || _M0L8new__idxS651 >= Moonbit_array_length(_M0L7entriesS2180)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3362
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2180[_M0L8new__idxS651];
  if (_M0L6_2aoldS3362) {
    moonbit_decref(_M0L6_2aoldS3362);
  }
  _M0L7entriesS2180[_M0L8new__idxS651] = _M0L6_2atmpS2181;
  _M0L8_2afieldS3361 = _M0L5entryS652->$1;
  _M0L6_2acntS3582 = Moonbit_object_header(_M0L5entryS652)->rc;
  if (_M0L6_2acntS3582 > 1) {
    int32_t _M0L11_2anew__cntS3585 = _M0L6_2acntS3582 - 1;
    Moonbit_object_header(_M0L5entryS652)->rc = _M0L11_2anew__cntS3585;
    if (_M0L8_2afieldS3361) {
      moonbit_incref(_M0L8_2afieldS3361);
    }
  } else if (_M0L6_2acntS3582 == 1) {
    void* _M0L8_2afieldS3584 = _M0L5entryS652->$5;
    moonbit_string_t _M0L8_2afieldS3583;
    moonbit_decref(_M0L8_2afieldS3584);
    _M0L8_2afieldS3583 = _M0L5entryS652->$4;
    moonbit_decref(_M0L8_2afieldS3583);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS652);
  }
  _M0L7_2abindS653 = _M0L8_2afieldS3361;
  if (_M0L7_2abindS653 == 0) {
    if (_M0L7_2abindS653) {
      moonbit_decref(_M0L7_2abindS653);
    }
    _M0L4selfS650->$6 = _M0L8new__idxS651;
    moonbit_decref(_M0L4selfS650);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS654;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS655;
    moonbit_decref(_M0L4selfS650);
    _M0L7_2aSomeS654 = _M0L7_2abindS653;
    _M0L7_2anextS655 = _M0L7_2aSomeS654;
    _M0L7_2anextS655->$0 = _M0L8new__idxS651;
    moonbit_decref(_M0L7_2anextS655);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS627,
  int32_t _M0L3idxS629,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS628
) {
  int32_t _M0L7_2abindS626;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3365;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2154;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2155;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3364;
  int32_t _M0L4sizeS2157;
  int32_t _M0L6_2atmpS2156;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS626 = _M0L4selfS627->$6;
  switch (_M0L7_2abindS626) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2149;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3366;
      moonbit_incref(_M0L5entryS628);
      _M0L6_2atmpS2149 = _M0L5entryS628;
      _M0L6_2aoldS3366 = _M0L4selfS627->$5;
      if (_M0L6_2aoldS3366) {
        moonbit_decref(_M0L6_2aoldS3366);
      }
      _M0L4selfS627->$5 = _M0L6_2atmpS2149;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3369 =
        _M0L4selfS627->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2153 =
        _M0L8_2afieldS3369;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3368;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2152;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2150;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2151;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3367;
      if (
        _M0L7_2abindS626 < 0
        || _M0L7_2abindS626 >= Moonbit_array_length(_M0L7entriesS2153)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3368
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2153[
          _M0L7_2abindS626
        ];
      _M0L6_2atmpS2152 = _M0L6_2atmpS3368;
      if (_M0L6_2atmpS2152) {
        moonbit_incref(_M0L6_2atmpS2152);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2150
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2152);
      moonbit_incref(_M0L5entryS628);
      _M0L6_2atmpS2151 = _M0L5entryS628;
      _M0L6_2aoldS3367 = _M0L6_2atmpS2150->$1;
      if (_M0L6_2aoldS3367) {
        moonbit_decref(_M0L6_2aoldS3367);
      }
      _M0L6_2atmpS2150->$1 = _M0L6_2atmpS2151;
      moonbit_decref(_M0L6_2atmpS2150);
      break;
    }
  }
  _M0L4selfS627->$6 = _M0L3idxS629;
  _M0L8_2afieldS3365 = _M0L4selfS627->$0;
  _M0L7entriesS2154 = _M0L8_2afieldS3365;
  _M0L6_2atmpS2155 = _M0L5entryS628;
  if (
    _M0L3idxS629 < 0
    || _M0L3idxS629 >= Moonbit_array_length(_M0L7entriesS2154)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3364
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2154[
      _M0L3idxS629
    ];
  if (_M0L6_2aoldS3364) {
    moonbit_decref(_M0L6_2aoldS3364);
  }
  _M0L7entriesS2154[_M0L3idxS629] = _M0L6_2atmpS2155;
  _M0L4sizeS2157 = _M0L4selfS627->$1;
  _M0L6_2atmpS2156 = _M0L4sizeS2157 + 1;
  _M0L4selfS627->$1 = _M0L6_2atmpS2156;
  moonbit_decref(_M0L4selfS627);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS631,
  int32_t _M0L3idxS633,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS632
) {
  int32_t _M0L7_2abindS630;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3371;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2163;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2164;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3370;
  int32_t _M0L4sizeS2166;
  int32_t _M0L6_2atmpS2165;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS630 = _M0L4selfS631->$6;
  switch (_M0L7_2abindS630) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2158;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3372;
      moonbit_incref(_M0L5entryS632);
      _M0L6_2atmpS2158 = _M0L5entryS632;
      _M0L6_2aoldS3372 = _M0L4selfS631->$5;
      if (_M0L6_2aoldS3372) {
        moonbit_decref(_M0L6_2aoldS3372);
      }
      _M0L4selfS631->$5 = _M0L6_2atmpS2158;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3375 =
        _M0L4selfS631->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2162 =
        _M0L8_2afieldS3375;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3374;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2161;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2159;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2160;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3373;
      if (
        _M0L7_2abindS630 < 0
        || _M0L7_2abindS630 >= Moonbit_array_length(_M0L7entriesS2162)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3374
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2162[
          _M0L7_2abindS630
        ];
      _M0L6_2atmpS2161 = _M0L6_2atmpS3374;
      if (_M0L6_2atmpS2161) {
        moonbit_incref(_M0L6_2atmpS2161);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2159
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2161);
      moonbit_incref(_M0L5entryS632);
      _M0L6_2atmpS2160 = _M0L5entryS632;
      _M0L6_2aoldS3373 = _M0L6_2atmpS2159->$1;
      if (_M0L6_2aoldS3373) {
        moonbit_decref(_M0L6_2aoldS3373);
      }
      _M0L6_2atmpS2159->$1 = _M0L6_2atmpS2160;
      moonbit_decref(_M0L6_2atmpS2159);
      break;
    }
  }
  _M0L4selfS631->$6 = _M0L3idxS633;
  _M0L8_2afieldS3371 = _M0L4selfS631->$0;
  _M0L7entriesS2163 = _M0L8_2afieldS3371;
  _M0L6_2atmpS2164 = _M0L5entryS632;
  if (
    _M0L3idxS633 < 0
    || _M0L3idxS633 >= Moonbit_array_length(_M0L7entriesS2163)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3370
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2163[
      _M0L3idxS633
    ];
  if (_M0L6_2aoldS3370) {
    moonbit_decref(_M0L6_2aoldS3370);
  }
  _M0L7entriesS2163[_M0L3idxS633] = _M0L6_2atmpS2164;
  _M0L4sizeS2166 = _M0L4selfS631->$1;
  _M0L6_2atmpS2165 = _M0L4sizeS2166 + 1;
  _M0L4selfS631->$1 = _M0L6_2atmpS2165;
  moonbit_decref(_M0L4selfS631);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS635,
  int32_t _M0L3idxS637,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS636
) {
  int32_t _M0L7_2abindS634;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3377;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2172;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2173;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3376;
  int32_t _M0L4sizeS2175;
  int32_t _M0L6_2atmpS2174;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS634 = _M0L4selfS635->$6;
  switch (_M0L7_2abindS634) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2167;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3378;
      moonbit_incref(_M0L5entryS636);
      _M0L6_2atmpS2167 = _M0L5entryS636;
      _M0L6_2aoldS3378 = _M0L4selfS635->$5;
      if (_M0L6_2aoldS3378) {
        moonbit_decref(_M0L6_2aoldS3378);
      }
      _M0L4selfS635->$5 = _M0L6_2atmpS2167;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS3381 = _M0L4selfS635->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2171 = _M0L8_2afieldS3381;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS3380;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2170;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2168;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2169;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3379;
      if (
        _M0L7_2abindS634 < 0
        || _M0L7_2abindS634 >= Moonbit_array_length(_M0L7entriesS2171)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3380
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2171[_M0L7_2abindS634];
      _M0L6_2atmpS2170 = _M0L6_2atmpS3380;
      if (_M0L6_2atmpS2170) {
        moonbit_incref(_M0L6_2atmpS2170);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2168
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2170);
      moonbit_incref(_M0L5entryS636);
      _M0L6_2atmpS2169 = _M0L5entryS636;
      _M0L6_2aoldS3379 = _M0L6_2atmpS2168->$1;
      if (_M0L6_2aoldS3379) {
        moonbit_decref(_M0L6_2aoldS3379);
      }
      _M0L6_2atmpS2168->$1 = _M0L6_2atmpS2169;
      moonbit_decref(_M0L6_2atmpS2168);
      break;
    }
  }
  _M0L4selfS635->$6 = _M0L3idxS637;
  _M0L8_2afieldS3377 = _M0L4selfS635->$0;
  _M0L7entriesS2172 = _M0L8_2afieldS3377;
  _M0L6_2atmpS2173 = _M0L5entryS636;
  if (
    _M0L3idxS637 < 0
    || _M0L3idxS637 >= Moonbit_array_length(_M0L7entriesS2172)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3376
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2172[_M0L3idxS637];
  if (_M0L6_2aoldS3376) {
    moonbit_decref(_M0L6_2aoldS3376);
  }
  _M0L7entriesS2172[_M0L3idxS637] = _M0L6_2atmpS2173;
  _M0L4sizeS2175 = _M0L4selfS635->$1;
  _M0L6_2atmpS2174 = _M0L4sizeS2175 + 1;
  _M0L4selfS635->$1 = _M0L6_2atmpS2174;
  moonbit_decref(_M0L4selfS635);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS609
) {
  int32_t _M0L8capacityS608;
  int32_t _M0L7_2abindS610;
  int32_t _M0L7_2abindS611;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2146;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS612;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS613;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3744;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS608
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS609);
  _M0L7_2abindS610 = _M0L8capacityS608 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS611 = _M0FPB21calc__grow__threshold(_M0L8capacityS608);
  _M0L6_2atmpS2146 = 0;
  _M0L7_2abindS612
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS608, _M0L6_2atmpS2146);
  _M0L7_2abindS613 = 0;
  _block_3744
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3744)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3744->$0 = _M0L7_2abindS612;
  _block_3744->$1 = 0;
  _block_3744->$2 = _M0L8capacityS608;
  _block_3744->$3 = _M0L7_2abindS610;
  _block_3744->$4 = _M0L7_2abindS611;
  _block_3744->$5 = _M0L7_2abindS613;
  _block_3744->$6 = -1;
  return _block_3744;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS615
) {
  int32_t _M0L8capacityS614;
  int32_t _M0L7_2abindS616;
  int32_t _M0L7_2abindS617;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2147;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS618;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS619;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3745;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS614
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS615);
  _M0L7_2abindS616 = _M0L8capacityS614 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS617 = _M0FPB21calc__grow__threshold(_M0L8capacityS614);
  _M0L6_2atmpS2147 = 0;
  _M0L7_2abindS618
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS614, _M0L6_2atmpS2147);
  _M0L7_2abindS619 = 0;
  _block_3745
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3745)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3745->$0 = _M0L7_2abindS618;
  _block_3745->$1 = 0;
  _block_3745->$2 = _M0L8capacityS614;
  _block_3745->$3 = _M0L7_2abindS616;
  _block_3745->$4 = _M0L7_2abindS617;
  _block_3745->$5 = _M0L7_2abindS619;
  _block_3745->$6 = -1;
  return _block_3745;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS621
) {
  int32_t _M0L8capacityS620;
  int32_t _M0L7_2abindS622;
  int32_t _M0L7_2abindS623;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2148;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS624;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS625;
  struct _M0TPB3MapGsRPB4JsonE* _block_3746;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS620
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS621);
  _M0L7_2abindS622 = _M0L8capacityS620 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS623 = _M0FPB21calc__grow__threshold(_M0L8capacityS620);
  _M0L6_2atmpS2148 = 0;
  _M0L7_2abindS624
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS620, _M0L6_2atmpS2148);
  _M0L7_2abindS625 = 0;
  _block_3746
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_3746)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_3746->$0 = _M0L7_2abindS624;
  _block_3746->$1 = 0;
  _block_3746->$2 = _M0L8capacityS620;
  _block_3746->$3 = _M0L7_2abindS622;
  _block_3746->$4 = _M0L7_2abindS623;
  _block_3746->$5 = _M0L7_2abindS625;
  _block_3746->$6 = -1;
  return _block_3746;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS607) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS607 >= 0) {
    int32_t _M0L6_2atmpS2145;
    int32_t _M0L6_2atmpS2144;
    int32_t _M0L6_2atmpS2143;
    int32_t _M0L6_2atmpS2142;
    if (_M0L4selfS607 <= 1) {
      return 1;
    }
    if (_M0L4selfS607 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2145 = _M0L4selfS607 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2144 = moonbit_clz32(_M0L6_2atmpS2145);
    _M0L6_2atmpS2143 = _M0L6_2atmpS2144 - 1;
    _M0L6_2atmpS2142 = 2147483647 >> (_M0L6_2atmpS2143 & 31);
    return _M0L6_2atmpS2142 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS606) {
  int32_t _M0L6_2atmpS2141;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2141 = _M0L8capacityS606 * 13;
  return _M0L6_2atmpS2141 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS600
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS600 == 0) {
    if (_M0L4selfS600) {
      moonbit_decref(_M0L4selfS600);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS601 =
      _M0L4selfS600;
    return _M0L7_2aSomeS601;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS602
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS602 == 0) {
    if (_M0L4selfS602) {
      moonbit_decref(_M0L4selfS602);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS603 =
      _M0L4selfS602;
    return _M0L7_2aSomeS603;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS604
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS604 == 0) {
    if (_M0L4selfS604) {
      moonbit_decref(_M0L4selfS604);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS605 = _M0L4selfS604;
    return _M0L7_2aSomeS605;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS599
) {
  moonbit_string_t* _M0L6_2atmpS2140;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2140 = _M0L4selfS599;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2140);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS595,
  int32_t _M0L5indexS596
) {
  uint64_t* _M0L6_2atmpS2138;
  uint64_t _M0L6_2atmpS3382;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2138 = _M0L4selfS595;
  if (
    _M0L5indexS596 < 0
    || _M0L5indexS596 >= Moonbit_array_length(_M0L6_2atmpS2138)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3382 = (uint64_t)_M0L6_2atmpS2138[_M0L5indexS596];
  moonbit_decref(_M0L6_2atmpS2138);
  return _M0L6_2atmpS3382;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS597,
  int32_t _M0L5indexS598
) {
  uint32_t* _M0L6_2atmpS2139;
  uint32_t _M0L6_2atmpS3383;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2139 = _M0L4selfS597;
  if (
    _M0L5indexS598 < 0
    || _M0L5indexS598 >= Moonbit_array_length(_M0L6_2atmpS2139)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3383 = (uint32_t)_M0L6_2atmpS2139[_M0L5indexS598];
  moonbit_decref(_M0L6_2atmpS2139);
  return _M0L6_2atmpS3383;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS594
) {
  moonbit_string_t* _M0L6_2atmpS2136;
  int32_t _M0L6_2atmpS3384;
  int32_t _M0L6_2atmpS2137;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2135;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS594);
  _M0L6_2atmpS2136 = _M0L4selfS594;
  _M0L6_2atmpS3384 = Moonbit_array_length(_M0L4selfS594);
  moonbit_decref(_M0L4selfS594);
  _M0L6_2atmpS2137 = _M0L6_2atmpS3384;
  _M0L6_2atmpS2135
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2137, _M0L6_2atmpS2136
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2135);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS592
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS591;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__* _closure_3747;
  struct _M0TWEOs* _M0L6_2atmpS2123;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS591
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS591)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS591->$0 = 0;
  _closure_3747
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__));
  Moonbit_object_header(_closure_3747)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__, $0_0) >> 2, 2, 0);
  _closure_3747->code = &_M0MPC15array9ArrayView4iterGsEC2124l570;
  _closure_3747->$0_0 = _M0L4selfS592.$0;
  _closure_3747->$0_1 = _M0L4selfS592.$1;
  _closure_3747->$0_2 = _M0L4selfS592.$2;
  _closure_3747->$1 = _M0L1iS591;
  _M0L6_2atmpS2123 = (struct _M0TWEOs*)_closure_3747;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2123);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2124l570(
  struct _M0TWEOs* _M0L6_2aenvS2125
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__* _M0L14_2acasted__envS2126;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3389;
  struct _M0TPC13ref3RefGiE* _M0L1iS591;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3388;
  int32_t _M0L6_2acntS3586;
  struct _M0TPB9ArrayViewGsE _M0L4selfS592;
  int32_t _M0L3valS2127;
  int32_t _M0L6_2atmpS2128;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2126
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2124__l570__*)_M0L6_2aenvS2125;
  _M0L8_2afieldS3389 = _M0L14_2acasted__envS2126->$1;
  _M0L1iS591 = _M0L8_2afieldS3389;
  _M0L8_2afieldS3388
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2126->$0_1,
      _M0L14_2acasted__envS2126->$0_2,
      _M0L14_2acasted__envS2126->$0_0
  };
  _M0L6_2acntS3586 = Moonbit_object_header(_M0L14_2acasted__envS2126)->rc;
  if (_M0L6_2acntS3586 > 1) {
    int32_t _M0L11_2anew__cntS3587 = _M0L6_2acntS3586 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2126)->rc
    = _M0L11_2anew__cntS3587;
    moonbit_incref(_M0L1iS591);
    moonbit_incref(_M0L8_2afieldS3388.$0);
  } else if (_M0L6_2acntS3586 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2126);
  }
  _M0L4selfS592 = _M0L8_2afieldS3388;
  _M0L3valS2127 = _M0L1iS591->$0;
  moonbit_incref(_M0L4selfS592.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2128 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS592);
  if (_M0L3valS2127 < _M0L6_2atmpS2128) {
    moonbit_string_t* _M0L8_2afieldS3387 = _M0L4selfS592.$0;
    moonbit_string_t* _M0L3bufS2131 = _M0L8_2afieldS3387;
    int32_t _M0L8_2afieldS3386 = _M0L4selfS592.$1;
    int32_t _M0L5startS2133 = _M0L8_2afieldS3386;
    int32_t _M0L3valS2134 = _M0L1iS591->$0;
    int32_t _M0L6_2atmpS2132 = _M0L5startS2133 + _M0L3valS2134;
    moonbit_string_t _M0L6_2atmpS3385 =
      (moonbit_string_t)_M0L3bufS2131[_M0L6_2atmpS2132];
    moonbit_string_t _M0L4elemS593;
    int32_t _M0L3valS2130;
    int32_t _M0L6_2atmpS2129;
    moonbit_incref(_M0L6_2atmpS3385);
    moonbit_decref(_M0L3bufS2131);
    _M0L4elemS593 = _M0L6_2atmpS3385;
    _M0L3valS2130 = _M0L1iS591->$0;
    _M0L6_2atmpS2129 = _M0L3valS2130 + 1;
    _M0L1iS591->$0 = _M0L6_2atmpS2129;
    moonbit_decref(_M0L1iS591);
    return _M0L4elemS593;
  } else {
    moonbit_decref(_M0L4selfS592.$0);
    moonbit_decref(_M0L1iS591);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS590
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS590;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS589,
  struct _M0TPB6Logger _M0L6loggerS588
) {
  moonbit_string_t _M0L6_2atmpS2122;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2122
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS589, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS588.$0->$method_0(_M0L6loggerS588.$1, _M0L6_2atmpS2122);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS587,
  struct _M0TPB6Logger _M0L6loggerS586
) {
  moonbit_string_t _M0L6_2atmpS2121;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2121 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS587, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS586.$0->$method_0(_M0L6loggerS586.$1, _M0L6_2atmpS2121);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS581) {
  int32_t _M0L3lenS580;
  struct _M0TPC13ref3RefGiE* _M0L5indexS582;
  struct _M0R38String_3a_3aiter_2eanon__u2105__l247__* _closure_3748;
  struct _M0TWEOc* _M0L6_2atmpS2104;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS580 = Moonbit_array_length(_M0L4selfS581);
  _M0L5indexS582
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS582)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS582->$0 = 0;
  _closure_3748
  = (struct _M0R38String_3a_3aiter_2eanon__u2105__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2105__l247__));
  Moonbit_object_header(_closure_3748)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2105__l247__, $0) >> 2, 2, 0);
  _closure_3748->code = &_M0MPC16string6String4iterC2105l247;
  _closure_3748->$0 = _M0L5indexS582;
  _closure_3748->$1 = _M0L4selfS581;
  _closure_3748->$2 = _M0L3lenS580;
  _M0L6_2atmpS2104 = (struct _M0TWEOc*)_closure_3748;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2104);
}

int32_t _M0MPC16string6String4iterC2105l247(
  struct _M0TWEOc* _M0L6_2aenvS2106
) {
  struct _M0R38String_3a_3aiter_2eanon__u2105__l247__* _M0L14_2acasted__envS2107;
  int32_t _M0L3lenS580;
  moonbit_string_t _M0L8_2afieldS3392;
  moonbit_string_t _M0L4selfS581;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3391;
  int32_t _M0L6_2acntS3588;
  struct _M0TPC13ref3RefGiE* _M0L5indexS582;
  int32_t _M0L3valS2108;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2107
  = (struct _M0R38String_3a_3aiter_2eanon__u2105__l247__*)_M0L6_2aenvS2106;
  _M0L3lenS580 = _M0L14_2acasted__envS2107->$2;
  _M0L8_2afieldS3392 = _M0L14_2acasted__envS2107->$1;
  _M0L4selfS581 = _M0L8_2afieldS3392;
  _M0L8_2afieldS3391 = _M0L14_2acasted__envS2107->$0;
  _M0L6_2acntS3588 = Moonbit_object_header(_M0L14_2acasted__envS2107)->rc;
  if (_M0L6_2acntS3588 > 1) {
    int32_t _M0L11_2anew__cntS3589 = _M0L6_2acntS3588 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2107)->rc
    = _M0L11_2anew__cntS3589;
    moonbit_incref(_M0L4selfS581);
    moonbit_incref(_M0L8_2afieldS3391);
  } else if (_M0L6_2acntS3588 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2107);
  }
  _M0L5indexS582 = _M0L8_2afieldS3391;
  _M0L3valS2108 = _M0L5indexS582->$0;
  if (_M0L3valS2108 < _M0L3lenS580) {
    int32_t _M0L3valS2120 = _M0L5indexS582->$0;
    int32_t _M0L2c1S583 = _M0L4selfS581[_M0L3valS2120];
    int32_t _if__result_3749;
    int32_t _M0L3valS2118;
    int32_t _M0L6_2atmpS2117;
    int32_t _M0L6_2atmpS2119;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S583)) {
      int32_t _M0L3valS2110 = _M0L5indexS582->$0;
      int32_t _M0L6_2atmpS2109 = _M0L3valS2110 + 1;
      _if__result_3749 = _M0L6_2atmpS2109 < _M0L3lenS580;
    } else {
      _if__result_3749 = 0;
    }
    if (_if__result_3749) {
      int32_t _M0L3valS2116 = _M0L5indexS582->$0;
      int32_t _M0L6_2atmpS2115 = _M0L3valS2116 + 1;
      int32_t _M0L6_2atmpS3390 = _M0L4selfS581[_M0L6_2atmpS2115];
      int32_t _M0L2c2S584;
      moonbit_decref(_M0L4selfS581);
      _M0L2c2S584 = _M0L6_2atmpS3390;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S584)) {
        int32_t _M0L6_2atmpS2113 = (int32_t)_M0L2c1S583;
        int32_t _M0L6_2atmpS2114 = (int32_t)_M0L2c2S584;
        int32_t _M0L1cS585;
        int32_t _M0L3valS2112;
        int32_t _M0L6_2atmpS2111;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS585
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2113, _M0L6_2atmpS2114);
        _M0L3valS2112 = _M0L5indexS582->$0;
        _M0L6_2atmpS2111 = _M0L3valS2112 + 2;
        _M0L5indexS582->$0 = _M0L6_2atmpS2111;
        moonbit_decref(_M0L5indexS582);
        return _M0L1cS585;
      }
    } else {
      moonbit_decref(_M0L4selfS581);
    }
    _M0L3valS2118 = _M0L5indexS582->$0;
    _M0L6_2atmpS2117 = _M0L3valS2118 + 1;
    _M0L5indexS582->$0 = _M0L6_2atmpS2117;
    moonbit_decref(_M0L5indexS582);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2119 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S583);
    return _M0L6_2atmpS2119;
  } else {
    moonbit_decref(_M0L5indexS582);
    moonbit_decref(_M0L4selfS581);
    return -1;
  }
}

int32_t _M0MPC16string6String13contains__any(
  moonbit_string_t _M0L4selfS578,
  struct _M0TPC16string10StringView _M0L5charsS579
) {
  int32_t _M0L6_2atmpS2103;
  struct _M0TPC16string10StringView _M0L6_2atmpS2102;
  #line 559 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS2103 = Moonbit_array_length(_M0L4selfS578);
  _M0L6_2atmpS2102
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2103, _M0L4selfS578
  };
  #line 560 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView13contains__any(_M0L6_2atmpS2102, _M0L5charsS579);
}

int32_t _M0MPC16string10StringView13contains__any(
  struct _M0TPC16string10StringView _M0L4selfS572,
  struct _M0TPC16string10StringView _M0L5charsS570
) {
  moonbit_string_t _M0L8_2afieldS3397;
  moonbit_string_t _M0L3strS2087;
  int32_t _M0L5startS2088;
  int32_t _M0L3endS2090;
  int64_t _M0L6_2atmpS2089;
  #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L8_2afieldS3397 = _M0L5charsS570.$0;
  _M0L3strS2087 = _M0L8_2afieldS3397;
  _M0L5startS2088 = _M0L5charsS570.$1;
  _M0L3endS2090 = _M0L5charsS570.$2;
  _M0L6_2atmpS2089 = (int64_t)_M0L3endS2090;
  moonbit_incref(_M0L3strS2087);
  #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (
    _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2087, 0, _M0L5startS2088, _M0L6_2atmpS2089)
  ) {
    moonbit_decref(_M0L4selfS572.$0);
    moonbit_decref(_M0L5charsS570.$0);
    return 0;
  } else {
    moonbit_string_t _M0L8_2afieldS3396 = _M0L5charsS570.$0;
    moonbit_string_t _M0L3strS2091 = _M0L8_2afieldS3396;
    int32_t _M0L5startS2092 = _M0L5charsS570.$1;
    int32_t _M0L3endS2094 = _M0L5charsS570.$2;
    int64_t _M0L6_2atmpS2093 = (int64_t)_M0L3endS2094;
    moonbit_incref(_M0L3strS2091);
    #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    if (
      _M0MPC16string6String24char__length__eq_2einner(_M0L3strS2091, 1, _M0L5startS2092, _M0L6_2atmpS2093)
    ) {
      moonbit_string_t _M0L8_2afieldS3395 = _M0L5charsS570.$0;
      moonbit_string_t _M0L3strS2095 = _M0L8_2afieldS3395;
      moonbit_string_t _M0L8_2afieldS3394 = _M0L5charsS570.$0;
      moonbit_string_t _M0L3strS2098 = _M0L8_2afieldS3394;
      int32_t _M0L5startS2099 = _M0L5charsS570.$1;
      int32_t _M0L8_2afieldS3393 = _M0L5charsS570.$2;
      int32_t _M0L3endS2101;
      int64_t _M0L6_2atmpS2100;
      int64_t _M0L6_2atmpS2097;
      int32_t _M0L6_2atmpS2096;
      int32_t _M0L4_2acS571;
      moonbit_incref(_M0L3strS2095);
      _M0L3endS2101 = _M0L8_2afieldS3393;
      _M0L6_2atmpS2100 = (int64_t)_M0L3endS2101;
      #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L6_2atmpS2097
      = _M0MPC16string6String29offset__of__nth__char_2einner(_M0L3strS2098, 0, _M0L5startS2099, _M0L6_2atmpS2100);
      _M0L6_2atmpS2096 = (int32_t)_M0L6_2atmpS2097;
      #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L4_2acS571
      = _M0MPC16string6String16unsafe__char__at(_M0L3strS2095, _M0L6_2atmpS2096);
      #line 545 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      return _M0MPC16string10StringView14contains__char(_M0L4selfS572, _M0L4_2acS571);
    } else {
      struct _M0TWEOc* _M0L5_2aitS573;
      #line 542 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L5_2aitS573 = _M0MPC16string10StringView4iter(_M0L4selfS572);
      while (1) {
        int32_t _M0L7_2abindS574;
        moonbit_incref(_M0L5_2aitS573);
        #line 547 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0L7_2abindS574 = _M0MPB4Iter4nextGcE(_M0L5_2aitS573);
        if (_M0L7_2abindS574 == -1) {
          moonbit_decref(_M0L5_2aitS573);
          moonbit_decref(_M0L5charsS570.$0);
          return 0;
        } else {
          int32_t _M0L7_2aSomeS575 = _M0L7_2abindS574;
          int32_t _M0L4_2acS576 = _M0L7_2aSomeS575;
          moonbit_incref(_M0L5charsS570.$0);
          #line 548 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          if (
            _M0MPC16string10StringView14contains__char(_M0L5charsS570, _M0L4_2acS576)
          ) {
            moonbit_decref(_M0L5_2aitS573);
            moonbit_decref(_M0L5charsS570.$0);
            return 1;
          }
          continue;
        }
        break;
      }
    }
  }
}

int32_t _M0MPC16string10StringView14contains__char(
  struct _M0TPC16string10StringView _M0L4selfS560,
  int32_t _M0L1cS562
) {
  int32_t _M0L3lenS559;
  #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L4selfS560.$0);
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L3lenS559 = _M0MPC16string10StringView6length(_M0L4selfS560);
  if (_M0L3lenS559 > 0) {
    int32_t _M0L1cS561 = _M0L1cS562;
    if (_M0L1cS561 <= 65535) {
      int32_t _M0L1iS563 = 0;
      while (1) {
        if (_M0L1iS563 < _M0L3lenS559) {
          int32_t _M0L6_2atmpS2073;
          int32_t _M0L6_2atmpS2072;
          int32_t _M0L6_2atmpS2074;
          moonbit_incref(_M0L4selfS560.$0);
          #line 598 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2073
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS560, _M0L1iS563);
          _M0L6_2atmpS2072 = (int32_t)_M0L6_2atmpS2073;
          if (_M0L6_2atmpS2072 == _M0L1cS561) {
            moonbit_decref(_M0L4selfS560.$0);
            return 1;
          }
          _M0L6_2atmpS2074 = _M0L1iS563 + 1;
          _M0L1iS563 = _M0L6_2atmpS2074;
          continue;
        } else {
          moonbit_decref(_M0L4selfS560.$0);
        }
        break;
      }
    } else if (_M0L3lenS559 >= 2) {
      int32_t _M0L3adjS565 = _M0L1cS561 - 65536;
      int32_t _M0L6_2atmpS2086 = _M0L3adjS565 >> 10;
      int32_t _M0L4highS566 = 55296 + _M0L6_2atmpS2086;
      int32_t _M0L6_2atmpS2085 = _M0L3adjS565 & 1023;
      int32_t _M0L3lowS567 = 56320 + _M0L6_2atmpS2085;
      int32_t _M0Lm1iS568 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2075 = _M0Lm1iS568;
        int32_t _M0L6_2atmpS2076 = _M0L3lenS559 - 1;
        if (_M0L6_2atmpS2075 < _M0L6_2atmpS2076) {
          int32_t _M0L6_2atmpS2079 = _M0Lm1iS568;
          int32_t _M0L6_2atmpS2078;
          int32_t _M0L6_2atmpS2077;
          int32_t _M0L6_2atmpS2084;
          moonbit_incref(_M0L4selfS560.$0);
          #line 612 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS2078
          = _M0MPC16string10StringView11unsafe__get(_M0L4selfS560, _M0L6_2atmpS2079);
          _M0L6_2atmpS2077 = (int32_t)_M0L6_2atmpS2078;
          if (_M0L6_2atmpS2077 == _M0L4highS566) {
            int32_t _M0L6_2atmpS2080 = _M0Lm1iS568;
            int32_t _M0L6_2atmpS2083;
            int32_t _M0L6_2atmpS2082;
            int32_t _M0L6_2atmpS2081;
            _M0Lm1iS568 = _M0L6_2atmpS2080 + 1;
            _M0L6_2atmpS2083 = _M0Lm1iS568;
            moonbit_incref(_M0L4selfS560.$0);
            #line 614 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            _M0L6_2atmpS2082
            = _M0MPC16string10StringView11unsafe__get(_M0L4selfS560, _M0L6_2atmpS2083);
            _M0L6_2atmpS2081 = (int32_t)_M0L6_2atmpS2082;
            if (_M0L6_2atmpS2081 == _M0L3lowS567) {
              moonbit_decref(_M0L4selfS560.$0);
              return 1;
            }
          }
          _M0L6_2atmpS2084 = _M0Lm1iS568;
          _M0Lm1iS568 = _M0L6_2atmpS2084 + 1;
          continue;
        } else {
          moonbit_decref(_M0L4selfS560.$0);
        }
        break;
      }
    } else {
      moonbit_decref(_M0L4selfS560.$0);
      return 0;
    }
    return 0;
  } else {
    moonbit_decref(_M0L4selfS560.$0);
    return 0;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS550,
  moonbit_string_t _M0L5valueS552
) {
  int32_t _M0L3lenS2057;
  moonbit_string_t* _M0L6_2atmpS2059;
  int32_t _M0L6_2atmpS3400;
  int32_t _M0L6_2atmpS2058;
  int32_t _M0L6lengthS551;
  moonbit_string_t* _M0L8_2afieldS3399;
  moonbit_string_t* _M0L3bufS2060;
  moonbit_string_t _M0L6_2aoldS3398;
  int32_t _M0L6_2atmpS2061;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2057 = _M0L4selfS550->$1;
  moonbit_incref(_M0L4selfS550);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2059 = _M0MPC15array5Array6bufferGsE(_M0L4selfS550);
  _M0L6_2atmpS3400 = Moonbit_array_length(_M0L6_2atmpS2059);
  moonbit_decref(_M0L6_2atmpS2059);
  _M0L6_2atmpS2058 = _M0L6_2atmpS3400;
  if (_M0L3lenS2057 == _M0L6_2atmpS2058) {
    moonbit_incref(_M0L4selfS550);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS550);
  }
  _M0L6lengthS551 = _M0L4selfS550->$1;
  _M0L8_2afieldS3399 = _M0L4selfS550->$0;
  _M0L3bufS2060 = _M0L8_2afieldS3399;
  _M0L6_2aoldS3398 = (moonbit_string_t)_M0L3bufS2060[_M0L6lengthS551];
  moonbit_decref(_M0L6_2aoldS3398);
  _M0L3bufS2060[_M0L6lengthS551] = _M0L5valueS552;
  _M0L6_2atmpS2061 = _M0L6lengthS551 + 1;
  _M0L4selfS550->$1 = _M0L6_2atmpS2061;
  moonbit_decref(_M0L4selfS550);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS553,
  struct _M0TUsiE* _M0L5valueS555
) {
  int32_t _M0L3lenS2062;
  struct _M0TUsiE** _M0L6_2atmpS2064;
  int32_t _M0L6_2atmpS3403;
  int32_t _M0L6_2atmpS2063;
  int32_t _M0L6lengthS554;
  struct _M0TUsiE** _M0L8_2afieldS3402;
  struct _M0TUsiE** _M0L3bufS2065;
  struct _M0TUsiE* _M0L6_2aoldS3401;
  int32_t _M0L6_2atmpS2066;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2062 = _M0L4selfS553->$1;
  moonbit_incref(_M0L4selfS553);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2064 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS553);
  _M0L6_2atmpS3403 = Moonbit_array_length(_M0L6_2atmpS2064);
  moonbit_decref(_M0L6_2atmpS2064);
  _M0L6_2atmpS2063 = _M0L6_2atmpS3403;
  if (_M0L3lenS2062 == _M0L6_2atmpS2063) {
    moonbit_incref(_M0L4selfS553);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS553);
  }
  _M0L6lengthS554 = _M0L4selfS553->$1;
  _M0L8_2afieldS3402 = _M0L4selfS553->$0;
  _M0L3bufS2065 = _M0L8_2afieldS3402;
  _M0L6_2aoldS3401 = (struct _M0TUsiE*)_M0L3bufS2065[_M0L6lengthS554];
  if (_M0L6_2aoldS3401) {
    moonbit_decref(_M0L6_2aoldS3401);
  }
  _M0L3bufS2065[_M0L6lengthS554] = _M0L5valueS555;
  _M0L6_2atmpS2066 = _M0L6lengthS554 + 1;
  _M0L4selfS553->$1 = _M0L6_2atmpS2066;
  moonbit_decref(_M0L4selfS553);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS556,
  void* _M0L5valueS558
) {
  int32_t _M0L3lenS2067;
  void** _M0L6_2atmpS2069;
  int32_t _M0L6_2atmpS3406;
  int32_t _M0L6_2atmpS2068;
  int32_t _M0L6lengthS557;
  void** _M0L8_2afieldS3405;
  void** _M0L3bufS2070;
  void* _M0L6_2aoldS3404;
  int32_t _M0L6_2atmpS2071;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2067 = _M0L4selfS556->$1;
  moonbit_incref(_M0L4selfS556);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2069
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS556);
  _M0L6_2atmpS3406 = Moonbit_array_length(_M0L6_2atmpS2069);
  moonbit_decref(_M0L6_2atmpS2069);
  _M0L6_2atmpS2068 = _M0L6_2atmpS3406;
  if (_M0L3lenS2067 == _M0L6_2atmpS2068) {
    moonbit_incref(_M0L4selfS556);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS556);
  }
  _M0L6lengthS557 = _M0L4selfS556->$1;
  _M0L8_2afieldS3405 = _M0L4selfS556->$0;
  _M0L3bufS2070 = _M0L8_2afieldS3405;
  _M0L6_2aoldS3404 = (void*)_M0L3bufS2070[_M0L6lengthS557];
  moonbit_decref(_M0L6_2aoldS3404);
  _M0L3bufS2070[_M0L6lengthS557] = _M0L5valueS558;
  _M0L6_2atmpS2071 = _M0L6lengthS557 + 1;
  _M0L4selfS556->$1 = _M0L6_2atmpS2071;
  moonbit_decref(_M0L4selfS556);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS542) {
  int32_t _M0L8old__capS541;
  int32_t _M0L8new__capS543;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS541 = _M0L4selfS542->$1;
  if (_M0L8old__capS541 == 0) {
    _M0L8new__capS543 = 8;
  } else {
    _M0L8new__capS543 = _M0L8old__capS541 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS542, _M0L8new__capS543);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS545
) {
  int32_t _M0L8old__capS544;
  int32_t _M0L8new__capS546;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS544 = _M0L4selfS545->$1;
  if (_M0L8old__capS544 == 0) {
    _M0L8new__capS546 = 8;
  } else {
    _M0L8new__capS546 = _M0L8old__capS544 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS545, _M0L8new__capS546);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS548
) {
  int32_t _M0L8old__capS547;
  int32_t _M0L8new__capS549;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS547 = _M0L4selfS548->$1;
  if (_M0L8old__capS547 == 0) {
    _M0L8new__capS549 = 8;
  } else {
    _M0L8new__capS549 = _M0L8old__capS547 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS548, _M0L8new__capS549);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS526,
  int32_t _M0L13new__capacityS524
) {
  moonbit_string_t* _M0L8new__bufS523;
  moonbit_string_t* _M0L8_2afieldS3408;
  moonbit_string_t* _M0L8old__bufS525;
  int32_t _M0L8old__capS527;
  int32_t _M0L9copy__lenS528;
  moonbit_string_t* _M0L6_2aoldS3407;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS523
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS524, (moonbit_string_t)moonbit_string_literal_1.data);
  _M0L8_2afieldS3408 = _M0L4selfS526->$0;
  _M0L8old__bufS525 = _M0L8_2afieldS3408;
  _M0L8old__capS527 = Moonbit_array_length(_M0L8old__bufS525);
  if (_M0L8old__capS527 < _M0L13new__capacityS524) {
    _M0L9copy__lenS528 = _M0L8old__capS527;
  } else {
    _M0L9copy__lenS528 = _M0L13new__capacityS524;
  }
  moonbit_incref(_M0L8old__bufS525);
  moonbit_incref(_M0L8new__bufS523);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS523, 0, _M0L8old__bufS525, 0, _M0L9copy__lenS528);
  _M0L6_2aoldS3407 = _M0L4selfS526->$0;
  moonbit_decref(_M0L6_2aoldS3407);
  _M0L4selfS526->$0 = _M0L8new__bufS523;
  moonbit_decref(_M0L4selfS526);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS532,
  int32_t _M0L13new__capacityS530
) {
  struct _M0TUsiE** _M0L8new__bufS529;
  struct _M0TUsiE** _M0L8_2afieldS3410;
  struct _M0TUsiE** _M0L8old__bufS531;
  int32_t _M0L8old__capS533;
  int32_t _M0L9copy__lenS534;
  struct _M0TUsiE** _M0L6_2aoldS3409;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS529
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS530, 0);
  _M0L8_2afieldS3410 = _M0L4selfS532->$0;
  _M0L8old__bufS531 = _M0L8_2afieldS3410;
  _M0L8old__capS533 = Moonbit_array_length(_M0L8old__bufS531);
  if (_M0L8old__capS533 < _M0L13new__capacityS530) {
    _M0L9copy__lenS534 = _M0L8old__capS533;
  } else {
    _M0L9copy__lenS534 = _M0L13new__capacityS530;
  }
  moonbit_incref(_M0L8old__bufS531);
  moonbit_incref(_M0L8new__bufS529);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS529, 0, _M0L8old__bufS531, 0, _M0L9copy__lenS534);
  _M0L6_2aoldS3409 = _M0L4selfS532->$0;
  moonbit_decref(_M0L6_2aoldS3409);
  _M0L4selfS532->$0 = _M0L8new__bufS529;
  moonbit_decref(_M0L4selfS532);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS538,
  int32_t _M0L13new__capacityS536
) {
  void** _M0L8new__bufS535;
  void** _M0L8_2afieldS3412;
  void** _M0L8old__bufS537;
  int32_t _M0L8old__capS539;
  int32_t _M0L9copy__lenS540;
  void** _M0L6_2aoldS3411;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS535
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS536, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS3412 = _M0L4selfS538->$0;
  _M0L8old__bufS537 = _M0L8_2afieldS3412;
  _M0L8old__capS539 = Moonbit_array_length(_M0L8old__bufS537);
  if (_M0L8old__capS539 < _M0L13new__capacityS536) {
    _M0L9copy__lenS540 = _M0L8old__capS539;
  } else {
    _M0L9copy__lenS540 = _M0L13new__capacityS536;
  }
  moonbit_incref(_M0L8old__bufS537);
  moonbit_incref(_M0L8new__bufS535);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS535, 0, _M0L8old__bufS537, 0, _M0L9copy__lenS540);
  _M0L6_2aoldS3411 = _M0L4selfS538->$0;
  moonbit_decref(_M0L6_2aoldS3411);
  _M0L4selfS538->$0 = _M0L8new__bufS535;
  moonbit_decref(_M0L4selfS538);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS522
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS522 == 0) {
    moonbit_string_t* _M0L6_2atmpS2055 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3753 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3753)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3753->$0 = _M0L6_2atmpS2055;
    _block_3753->$1 = 0;
    return _block_3753;
  } else {
    moonbit_string_t* _M0L6_2atmpS2056 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS522, (moonbit_string_t)moonbit_string_literal_1.data);
    struct _M0TPB5ArrayGsE* _block_3754 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3754)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3754->$0 = _M0L6_2atmpS2056;
    _block_3754->$1 = 0;
    return _block_3754;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS516,
  int32_t _M0L1nS515
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS515 <= 0) {
    moonbit_decref(_M0L4selfS516);
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else if (_M0L1nS515 == 1) {
    return _M0L4selfS516;
  } else {
    int32_t _M0L3lenS517 = Moonbit_array_length(_M0L4selfS516);
    int32_t _M0L6_2atmpS2054 = _M0L3lenS517 * _M0L1nS515;
    struct _M0TPB13StringBuilder* _M0L3bufS518;
    moonbit_string_t _M0L3strS519;
    int32_t _M0L2__S520;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS518 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2054);
    _M0L3strS519 = _M0L4selfS516;
    _M0L2__S520 = 0;
    while (1) {
      if (_M0L2__S520 < _M0L1nS515) {
        int32_t _M0L6_2atmpS2053;
        moonbit_incref(_M0L3strS519);
        moonbit_incref(_M0L3bufS518);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS518, _M0L3strS519);
        _M0L6_2atmpS2053 = _M0L2__S520 + 1;
        _M0L2__S520 = _M0L6_2atmpS2053;
        continue;
      } else {
        moonbit_decref(_M0L3strS519);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS518);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS513,
  struct _M0TPC16string10StringView _M0L3strS514
) {
  int32_t _M0L3lenS2041;
  int32_t _M0L6_2atmpS2043;
  int32_t _M0L6_2atmpS2042;
  int32_t _M0L6_2atmpS2040;
  moonbit_bytes_t _M0L8_2afieldS3413;
  moonbit_bytes_t _M0L4dataS2044;
  int32_t _M0L3lenS2045;
  moonbit_string_t _M0L6_2atmpS2046;
  int32_t _M0L6_2atmpS2047;
  int32_t _M0L6_2atmpS2048;
  int32_t _M0L3lenS2050;
  int32_t _M0L6_2atmpS2052;
  int32_t _M0L6_2atmpS2051;
  int32_t _M0L6_2atmpS2049;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2041 = _M0L4selfS513->$1;
  moonbit_incref(_M0L3strS514.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2043 = _M0MPC16string10StringView6length(_M0L3strS514);
  _M0L6_2atmpS2042 = _M0L6_2atmpS2043 * 2;
  _M0L6_2atmpS2040 = _M0L3lenS2041 + _M0L6_2atmpS2042;
  moonbit_incref(_M0L4selfS513);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS513, _M0L6_2atmpS2040);
  _M0L8_2afieldS3413 = _M0L4selfS513->$0;
  _M0L4dataS2044 = _M0L8_2afieldS3413;
  _M0L3lenS2045 = _M0L4selfS513->$1;
  moonbit_incref(_M0L4dataS2044);
  moonbit_incref(_M0L3strS514.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2046 = _M0MPC16string10StringView4data(_M0L3strS514);
  moonbit_incref(_M0L3strS514.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2047 = _M0MPC16string10StringView13start__offset(_M0L3strS514);
  moonbit_incref(_M0L3strS514.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2048 = _M0MPC16string10StringView6length(_M0L3strS514);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2044, _M0L3lenS2045, _M0L6_2atmpS2046, _M0L6_2atmpS2047, _M0L6_2atmpS2048);
  _M0L3lenS2050 = _M0L4selfS513->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2052 = _M0MPC16string10StringView6length(_M0L3strS514);
  _M0L6_2atmpS2051 = _M0L6_2atmpS2052 * 2;
  _M0L6_2atmpS2049 = _M0L3lenS2050 + _M0L6_2atmpS2051;
  _M0L4selfS513->$1 = _M0L6_2atmpS2049;
  moonbit_decref(_M0L4selfS513);
  return 0;
}

int64_t _M0MPC16string6String29offset__of__nth__char_2einner(
  moonbit_string_t _M0L4selfS510,
  int32_t _M0L1iS511,
  int32_t _M0L13start__offsetS512,
  int64_t _M0L11end__offsetS508
) {
  int32_t _M0L11end__offsetS507;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS508 == 4294967296ll) {
    _M0L11end__offsetS507 = Moonbit_array_length(_M0L4selfS510);
  } else {
    int64_t _M0L7_2aSomeS509 = _M0L11end__offsetS508;
    _M0L11end__offsetS507 = (int32_t)_M0L7_2aSomeS509;
  }
  if (_M0L1iS511 >= 0) {
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String30offset__of__nth__char__forward(_M0L4selfS510, _M0L1iS511, _M0L13start__offsetS512, _M0L11end__offsetS507);
  } else {
    int32_t _M0L6_2atmpS2039 = -_M0L1iS511;
    #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0MPC16string6String31offset__of__nth__char__backward(_M0L4selfS510, _M0L6_2atmpS2039, _M0L13start__offsetS512, _M0L11end__offsetS507);
  }
}

int64_t _M0MPC16string6String30offset__of__nth__char__forward(
  moonbit_string_t _M0L4selfS505,
  int32_t _M0L1nS503,
  int32_t _M0L13start__offsetS499,
  int32_t _M0L11end__offsetS500
) {
  int32_t _if__result_3756;
  #line 322 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L13start__offsetS499 >= 0) {
    _if__result_3756 = _M0L13start__offsetS499 <= _M0L11end__offsetS500;
  } else {
    _if__result_3756 = 0;
  }
  if (_if__result_3756) {
    int32_t _M0Lm13utf16__offsetS501 = _M0L13start__offsetS499;
    int32_t _M0Lm11char__countS502 = 0;
    int32_t _M0L6_2atmpS2037;
    int32_t _if__result_3759;
    while (1) {
      int32_t _M0L6_2atmpS2031 = _M0Lm13utf16__offsetS501;
      int32_t _if__result_3758;
      if (_M0L6_2atmpS2031 < _M0L11end__offsetS500) {
        int32_t _M0L6_2atmpS2030 = _M0Lm11char__countS502;
        _if__result_3758 = _M0L6_2atmpS2030 < _M0L1nS503;
      } else {
        _if__result_3758 = 0;
      }
      if (_if__result_3758) {
        int32_t _M0L6_2atmpS2035 = _M0Lm13utf16__offsetS501;
        int32_t _M0L1cS504 = _M0L4selfS505[_M0L6_2atmpS2035];
        int32_t _M0L6_2atmpS2034;
        #line 336 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L1cS504)) {
          int32_t _M0L6_2atmpS2032 = _M0Lm13utf16__offsetS501;
          _M0Lm13utf16__offsetS501 = _M0L6_2atmpS2032 + 2;
        } else {
          int32_t _M0L6_2atmpS2033 = _M0Lm13utf16__offsetS501;
          _M0Lm13utf16__offsetS501 = _M0L6_2atmpS2033 + 1;
        }
        _M0L6_2atmpS2034 = _M0Lm11char__countS502;
        _M0Lm11char__countS502 = _M0L6_2atmpS2034 + 1;
        continue;
      } else {
        moonbit_decref(_M0L4selfS505);
      }
      break;
    }
    _M0L6_2atmpS2037 = _M0Lm11char__countS502;
    if (_M0L6_2atmpS2037 < _M0L1nS503) {
      _if__result_3759 = 1;
    } else {
      int32_t _M0L6_2atmpS2036 = _M0Lm13utf16__offsetS501;
      _if__result_3759 = _M0L6_2atmpS2036 >= _M0L11end__offsetS500;
    }
    if (_if__result_3759) {
      return 4294967296ll;
    } else {
      int32_t _M0L6_2atmpS2038 = _M0Lm13utf16__offsetS501;
      return (int64_t)_M0L6_2atmpS2038;
    }
  } else {
    moonbit_decref(_M0L4selfS505);
    #line 329 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGOiE((moonbit_string_t)moonbit_string_literal_74.data, (moonbit_string_t)moonbit_string_literal_75.data);
  }
}

int64_t _M0MPC16string6String31offset__of__nth__char__backward(
  moonbit_string_t _M0L4selfS497,
  int32_t _M0L1nS495,
  int32_t _M0L13start__offsetS494,
  int32_t _M0L11end__offsetS493
) {
  int32_t _M0Lm11char__countS491;
  int32_t _M0Lm13utf16__offsetS492;
  int32_t _M0L6_2atmpS2028;
  int32_t _if__result_3762;
  #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0Lm11char__countS491 = 0;
  _M0Lm13utf16__offsetS492 = _M0L11end__offsetS493;
  while (1) {
    int32_t _M0L6_2atmpS2021 = _M0Lm13utf16__offsetS492;
    int32_t _M0L6_2atmpS2020 = _M0L6_2atmpS2021 - 1;
    int32_t _if__result_3761;
    if (_M0L6_2atmpS2020 >= _M0L13start__offsetS494) {
      int32_t _M0L6_2atmpS2019 = _M0Lm11char__countS491;
      _if__result_3761 = _M0L6_2atmpS2019 < _M0L1nS495;
    } else {
      _if__result_3761 = 0;
    }
    if (_if__result_3761) {
      int32_t _M0L6_2atmpS2026 = _M0Lm13utf16__offsetS492;
      int32_t _M0L6_2atmpS2025 = _M0L6_2atmpS2026 - 1;
      int32_t _M0L1cS496 = _M0L4selfS497[_M0L6_2atmpS2025];
      int32_t _M0L6_2atmpS2024;
      #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L1cS496)) {
        int32_t _M0L6_2atmpS2022 = _M0Lm13utf16__offsetS492;
        _M0Lm13utf16__offsetS492 = _M0L6_2atmpS2022 - 2;
      } else {
        int32_t _M0L6_2atmpS2023 = _M0Lm13utf16__offsetS492;
        _M0Lm13utf16__offsetS492 = _M0L6_2atmpS2023 - 1;
      }
      _M0L6_2atmpS2024 = _M0Lm11char__countS491;
      _M0Lm11char__countS491 = _M0L6_2atmpS2024 + 1;
      continue;
    } else {
      moonbit_decref(_M0L4selfS497);
    }
    break;
  }
  _M0L6_2atmpS2028 = _M0Lm11char__countS491;
  if (_M0L6_2atmpS2028 < _M0L1nS495) {
    _if__result_3762 = 1;
  } else {
    int32_t _M0L6_2atmpS2027 = _M0Lm13utf16__offsetS492;
    _if__result_3762 = _M0L6_2atmpS2027 < _M0L13start__offsetS494;
  }
  if (_if__result_3762) {
    return 4294967296ll;
  } else {
    int32_t _M0L6_2atmpS2029 = _M0Lm13utf16__offsetS492;
    return (int64_t)_M0L6_2atmpS2029;
  }
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS483,
  int32_t _M0L3lenS486,
  int32_t _M0L13start__offsetS490,
  int64_t _M0L11end__offsetS481
) {
  int32_t _M0L11end__offsetS480;
  int32_t _M0L5indexS484;
  int32_t _M0L5countS485;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS481 == 4294967296ll) {
    _M0L11end__offsetS480 = Moonbit_array_length(_M0L4selfS483);
  } else {
    int64_t _M0L7_2aSomeS482 = _M0L11end__offsetS481;
    _M0L11end__offsetS480 = (int32_t)_M0L7_2aSomeS482;
  }
  _M0L5indexS484 = _M0L13start__offsetS490;
  _M0L5countS485 = 0;
  while (1) {
    int32_t _if__result_3764;
    if (_M0L5indexS484 < _M0L11end__offsetS480) {
      _if__result_3764 = _M0L5countS485 < _M0L3lenS486;
    } else {
      _if__result_3764 = 0;
    }
    if (_if__result_3764) {
      int32_t _M0L2c1S487 = _M0L4selfS483[_M0L5indexS484];
      int32_t _if__result_3765;
      int32_t _M0L6_2atmpS2017;
      int32_t _M0L6_2atmpS2018;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S487)) {
        int32_t _M0L6_2atmpS2013 = _M0L5indexS484 + 1;
        _if__result_3765 = _M0L6_2atmpS2013 < _M0L11end__offsetS480;
      } else {
        _if__result_3765 = 0;
      }
      if (_if__result_3765) {
        int32_t _M0L6_2atmpS2016 = _M0L5indexS484 + 1;
        int32_t _M0L2c2S488 = _M0L4selfS483[_M0L6_2atmpS2016];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S488)) {
          int32_t _M0L6_2atmpS2014 = _M0L5indexS484 + 2;
          int32_t _M0L6_2atmpS2015 = _M0L5countS485 + 1;
          _M0L5indexS484 = _M0L6_2atmpS2014;
          _M0L5countS485 = _M0L6_2atmpS2015;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_76.data, (moonbit_string_t)moonbit_string_literal_77.data);
        }
      }
      _M0L6_2atmpS2017 = _M0L5indexS484 + 1;
      _M0L6_2atmpS2018 = _M0L5countS485 + 1;
      _M0L5indexS484 = _M0L6_2atmpS2017;
      _M0L5countS485 = _M0L6_2atmpS2018;
      continue;
    } else {
      moonbit_decref(_M0L4selfS483);
      return _M0L5countS485 >= _M0L3lenS486;
    }
    break;
  }
}

int32_t _M0MPC16string6String24char__length__eq_2einner(
  moonbit_string_t _M0L4selfS472,
  int32_t _M0L3lenS475,
  int32_t _M0L13start__offsetS479,
  int64_t _M0L11end__offsetS470
) {
  int32_t _M0L11end__offsetS469;
  int32_t _M0L5indexS473;
  int32_t _M0L5countS474;
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS470 == 4294967296ll) {
    _M0L11end__offsetS469 = Moonbit_array_length(_M0L4selfS472);
  } else {
    int64_t _M0L7_2aSomeS471 = _M0L11end__offsetS470;
    _M0L11end__offsetS469 = (int32_t)_M0L7_2aSomeS471;
  }
  _M0L5indexS473 = _M0L13start__offsetS479;
  _M0L5countS474 = 0;
  while (1) {
    int32_t _if__result_3767;
    if (_M0L5indexS473 < _M0L11end__offsetS469) {
      _if__result_3767 = _M0L5countS474 < _M0L3lenS475;
    } else {
      _if__result_3767 = 0;
    }
    if (_if__result_3767) {
      int32_t _M0L2c1S476 = _M0L4selfS472[_M0L5indexS473];
      int32_t _if__result_3768;
      int32_t _M0L6_2atmpS2011;
      int32_t _M0L6_2atmpS2012;
      #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S476)) {
        int32_t _M0L6_2atmpS2007 = _M0L5indexS473 + 1;
        _if__result_3768 = _M0L6_2atmpS2007 < _M0L11end__offsetS469;
      } else {
        _if__result_3768 = 0;
      }
      if (_if__result_3768) {
        int32_t _M0L6_2atmpS2010 = _M0L5indexS473 + 1;
        int32_t _M0L2c2S477 = _M0L4selfS472[_M0L6_2atmpS2010];
        #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S477)) {
          int32_t _M0L6_2atmpS2008 = _M0L5indexS473 + 2;
          int32_t _M0L6_2atmpS2009 = _M0L5countS474 + 1;
          _M0L5indexS473 = _M0L6_2atmpS2008;
          _M0L5countS474 = _M0L6_2atmpS2009;
          continue;
        } else {
          #line 426 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_76.data, (moonbit_string_t)moonbit_string_literal_78.data);
        }
      }
      _M0L6_2atmpS2011 = _M0L5indexS473 + 1;
      _M0L6_2atmpS2012 = _M0L5countS474 + 1;
      _M0L5indexS473 = _M0L6_2atmpS2011;
      _M0L5countS474 = _M0L6_2atmpS2012;
      continue;
    } else {
      moonbit_decref(_M0L4selfS472);
      if (_M0L5countS474 == _M0L3lenS475) {
        return _M0L5indexS473 == _M0L11end__offsetS469;
      } else {
        return 0;
      }
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS465
) {
  int32_t _M0L3endS1999;
  int32_t _M0L8_2afieldS3414;
  int32_t _M0L5startS2000;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1999 = _M0L4selfS465.$2;
  _M0L8_2afieldS3414 = _M0L4selfS465.$1;
  moonbit_decref(_M0L4selfS465.$0);
  _M0L5startS2000 = _M0L8_2afieldS3414;
  return _M0L3endS1999 - _M0L5startS2000;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS466
) {
  int32_t _M0L3endS2001;
  int32_t _M0L8_2afieldS3415;
  int32_t _M0L5startS2002;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2001 = _M0L4selfS466.$2;
  _M0L8_2afieldS3415 = _M0L4selfS466.$1;
  moonbit_decref(_M0L4selfS466.$0);
  _M0L5startS2002 = _M0L8_2afieldS3415;
  return _M0L3endS2001 - _M0L5startS2002;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS467
) {
  int32_t _M0L3endS2003;
  int32_t _M0L8_2afieldS3416;
  int32_t _M0L5startS2004;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2003 = _M0L4selfS467.$2;
  _M0L8_2afieldS3416 = _M0L4selfS467.$1;
  moonbit_decref(_M0L4selfS467.$0);
  _M0L5startS2004 = _M0L8_2afieldS3416;
  return _M0L3endS2003 - _M0L5startS2004;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS468
) {
  int32_t _M0L3endS2005;
  int32_t _M0L8_2afieldS3417;
  int32_t _M0L5startS2006;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2005 = _M0L4selfS468.$2;
  _M0L8_2afieldS3417 = _M0L4selfS468.$1;
  moonbit_decref(_M0L4selfS468.$0);
  _M0L5startS2006 = _M0L8_2afieldS3417;
  return _M0L3endS2005 - _M0L5startS2006;
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
  int32_t _if__result_3769;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS456 == 4294967296ll) {
    _M0L11end__offsetS455 = Moonbit_array_length(_M0L4selfS458);
  } else {
    int64_t _M0L7_2aSomeS457 = _M0L11end__offsetS456;
    _M0L11end__offsetS455 = (int32_t)_M0L7_2aSomeS457;
  }
  if (_M0L13start__offsetS459 >= 0) {
    if (_M0L13start__offsetS459 <= _M0L11end__offsetS455) {
      int32_t _M0L6_2atmpS1998 = Moonbit_array_length(_M0L4selfS458);
      _if__result_3769 = _M0L11end__offsetS455 <= _M0L6_2atmpS1998;
    } else {
      _if__result_3769 = 0;
    }
  } else {
    _if__result_3769 = 0;
  }
  if (_if__result_3769) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS459,
                                                 _M0L11end__offsetS455,
                                                 _M0L4selfS458};
  } else {
    moonbit_decref(_M0L4selfS458);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_79.data, (moonbit_string_t)moonbit_string_literal_80.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS450
) {
  int32_t _M0L5startS449;
  int32_t _M0L3endS451;
  struct _M0TPC13ref3RefGiE* _M0L5indexS452;
  struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__* _closure_3770;
  struct _M0TWEOc* _M0L6_2atmpS1977;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS449 = _M0L4selfS450.$1;
  _M0L3endS451 = _M0L4selfS450.$2;
  _M0L5indexS452
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS452)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS452->$0 = _M0L5startS449;
  _closure_3770
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__));
  Moonbit_object_header(_closure_3770)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__, $0) >> 2, 2, 0);
  _closure_3770->code = &_M0MPC16string10StringView4iterC1978l198;
  _closure_3770->$0 = _M0L5indexS452;
  _closure_3770->$1 = _M0L3endS451;
  _closure_3770->$2_0 = _M0L4selfS450.$0;
  _closure_3770->$2_1 = _M0L4selfS450.$1;
  _closure_3770->$2_2 = _M0L4selfS450.$2;
  _M0L6_2atmpS1977 = (struct _M0TWEOc*)_closure_3770;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1977);
}

int32_t _M0MPC16string10StringView4iterC1978l198(
  struct _M0TWEOc* _M0L6_2aenvS1979
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__* _M0L14_2acasted__envS1980;
  struct _M0TPC16string10StringView _M0L8_2afieldS3423;
  struct _M0TPC16string10StringView _M0L4selfS450;
  int32_t _M0L3endS451;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3422;
  int32_t _M0L6_2acntS3590;
  struct _M0TPC13ref3RefGiE* _M0L5indexS452;
  int32_t _M0L3valS1981;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS1980
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1978__l198__*)_M0L6_2aenvS1979;
  _M0L8_2afieldS3423
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1980->$2_1,
      _M0L14_2acasted__envS1980->$2_2,
      _M0L14_2acasted__envS1980->$2_0
  };
  _M0L4selfS450 = _M0L8_2afieldS3423;
  _M0L3endS451 = _M0L14_2acasted__envS1980->$1;
  _M0L8_2afieldS3422 = _M0L14_2acasted__envS1980->$0;
  _M0L6_2acntS3590 = Moonbit_object_header(_M0L14_2acasted__envS1980)->rc;
  if (_M0L6_2acntS3590 > 1) {
    int32_t _M0L11_2anew__cntS3591 = _M0L6_2acntS3590 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1980)->rc
    = _M0L11_2anew__cntS3591;
    moonbit_incref(_M0L4selfS450.$0);
    moonbit_incref(_M0L8_2afieldS3422);
  } else if (_M0L6_2acntS3590 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS1980);
  }
  _M0L5indexS452 = _M0L8_2afieldS3422;
  _M0L3valS1981 = _M0L5indexS452->$0;
  if (_M0L3valS1981 < _M0L3endS451) {
    moonbit_string_t _M0L8_2afieldS3421 = _M0L4selfS450.$0;
    moonbit_string_t _M0L3strS1996 = _M0L8_2afieldS3421;
    int32_t _M0L3valS1997 = _M0L5indexS452->$0;
    int32_t _M0L6_2atmpS3420 = _M0L3strS1996[_M0L3valS1997];
    int32_t _M0L2c1S453 = _M0L6_2atmpS3420;
    int32_t _if__result_3771;
    int32_t _M0L3valS1994;
    int32_t _M0L6_2atmpS1993;
    int32_t _M0L6_2atmpS1995;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S453)) {
      int32_t _M0L3valS1984 = _M0L5indexS452->$0;
      int32_t _M0L6_2atmpS1982 = _M0L3valS1984 + 1;
      int32_t _M0L3endS1983 = _M0L4selfS450.$2;
      _if__result_3771 = _M0L6_2atmpS1982 < _M0L3endS1983;
    } else {
      _if__result_3771 = 0;
    }
    if (_if__result_3771) {
      moonbit_string_t _M0L8_2afieldS3419 = _M0L4selfS450.$0;
      moonbit_string_t _M0L3strS1990 = _M0L8_2afieldS3419;
      int32_t _M0L3valS1992 = _M0L5indexS452->$0;
      int32_t _M0L6_2atmpS1991 = _M0L3valS1992 + 1;
      int32_t _M0L6_2atmpS3418 = _M0L3strS1990[_M0L6_2atmpS1991];
      int32_t _M0L2c2S454;
      moonbit_decref(_M0L3strS1990);
      _M0L2c2S454 = _M0L6_2atmpS3418;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S454)) {
        int32_t _M0L3valS1986 = _M0L5indexS452->$0;
        int32_t _M0L6_2atmpS1985 = _M0L3valS1986 + 2;
        int32_t _M0L6_2atmpS1988;
        int32_t _M0L6_2atmpS1989;
        int32_t _M0L6_2atmpS1987;
        _M0L5indexS452->$0 = _M0L6_2atmpS1985;
        moonbit_decref(_M0L5indexS452);
        _M0L6_2atmpS1988 = (int32_t)_M0L2c1S453;
        _M0L6_2atmpS1989 = (int32_t)_M0L2c2S454;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS1987
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1988, _M0L6_2atmpS1989);
        return _M0L6_2atmpS1987;
      }
    } else {
      moonbit_decref(_M0L4selfS450.$0);
    }
    _M0L3valS1994 = _M0L5indexS452->$0;
    _M0L6_2atmpS1993 = _M0L3valS1994 + 1;
    _M0L5indexS452->$0 = _M0L6_2atmpS1993;
    moonbit_decref(_M0L5indexS452);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS1995 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S453);
    return _M0L6_2atmpS1995;
  } else {
    moonbit_decref(_M0L5indexS452);
    moonbit_decref(_M0L4selfS450.$0);
    return -1;
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS448
) {
  moonbit_string_t _M0L8_2afieldS3425;
  moonbit_string_t _M0L3strS1974;
  int32_t _M0L5startS1975;
  int32_t _M0L8_2afieldS3424;
  int32_t _M0L3endS1976;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3425 = _M0L4selfS448.$0;
  _M0L3strS1974 = _M0L8_2afieldS3425;
  _M0L5startS1975 = _M0L4selfS448.$1;
  _M0L8_2afieldS3424 = _M0L4selfS448.$2;
  _M0L3endS1976 = _M0L8_2afieldS3424;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1974, _M0L5startS1975, _M0L3endS1976);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS446,
  struct _M0TPB6Logger _M0L6loggerS447
) {
  moonbit_string_t _M0L8_2afieldS3427;
  moonbit_string_t _M0L3strS1971;
  int32_t _M0L5startS1972;
  int32_t _M0L8_2afieldS3426;
  int32_t _M0L3endS1973;
  moonbit_string_t _M0L6substrS445;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3427 = _M0L4selfS446.$0;
  _M0L3strS1971 = _M0L8_2afieldS3427;
  _M0L5startS1972 = _M0L4selfS446.$1;
  _M0L8_2afieldS3426 = _M0L4selfS446.$2;
  _M0L3endS1973 = _M0L8_2afieldS3426;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS445
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1971, _M0L5startS1972, _M0L3endS1973);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS445, _M0L6loggerS447);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS437,
  struct _M0TPB6Logger _M0L6loggerS435
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS436;
  int32_t _M0L3lenS438;
  int32_t _M0L1iS439;
  int32_t _M0L3segS440;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 34);
  moonbit_incref(_M0L4selfS437);
  if (_M0L6loggerS435.$1) {
    moonbit_incref(_M0L6loggerS435.$1);
  }
  _M0L6_2aenvS436
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS436)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS436->$0 = _M0L4selfS437;
  _M0L6_2aenvS436->$1_0 = _M0L6loggerS435.$0;
  _M0L6_2aenvS436->$1_1 = _M0L6loggerS435.$1;
  _M0L3lenS438 = Moonbit_array_length(_M0L4selfS437);
  _M0L1iS439 = 0;
  _M0L3segS440 = 0;
  _2afor_441:;
  while (1) {
    int32_t _M0L4codeS442;
    int32_t _M0L1cS444;
    int32_t _M0L6_2atmpS1955;
    int32_t _M0L6_2atmpS1956;
    int32_t _M0L6_2atmpS1957;
    int32_t _tmp_3775;
    int32_t _tmp_3776;
    if (_M0L1iS439 >= _M0L3lenS438) {
      moonbit_decref(_M0L4selfS437);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
      break;
    }
    _M0L4codeS442 = _M0L4selfS437[_M0L1iS439];
    switch (_M0L4codeS442) {
      case 34: {
        _M0L1cS444 = _M0L4codeS442;
        goto join_443;
        break;
      }
      
      case 92: {
        _M0L1cS444 = _M0L4codeS442;
        goto join_443;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1958;
        int32_t _M0L6_2atmpS1959;
        moonbit_incref(_M0L6_2aenvS436);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
        if (_M0L6loggerS435.$1) {
          moonbit_incref(_M0L6loggerS435.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS435.$0->$method_0(_M0L6loggerS435.$1, (moonbit_string_t)moonbit_string_literal_57.data);
        _M0L6_2atmpS1958 = _M0L1iS439 + 1;
        _M0L6_2atmpS1959 = _M0L1iS439 + 1;
        _M0L1iS439 = _M0L6_2atmpS1958;
        _M0L3segS440 = _M0L6_2atmpS1959;
        goto _2afor_441;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1960;
        int32_t _M0L6_2atmpS1961;
        moonbit_incref(_M0L6_2aenvS436);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
        if (_M0L6loggerS435.$1) {
          moonbit_incref(_M0L6loggerS435.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS435.$0->$method_0(_M0L6loggerS435.$1, (moonbit_string_t)moonbit_string_literal_58.data);
        _M0L6_2atmpS1960 = _M0L1iS439 + 1;
        _M0L6_2atmpS1961 = _M0L1iS439 + 1;
        _M0L1iS439 = _M0L6_2atmpS1960;
        _M0L3segS440 = _M0L6_2atmpS1961;
        goto _2afor_441;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1962;
        int32_t _M0L6_2atmpS1963;
        moonbit_incref(_M0L6_2aenvS436);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
        if (_M0L6loggerS435.$1) {
          moonbit_incref(_M0L6loggerS435.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS435.$0->$method_0(_M0L6loggerS435.$1, (moonbit_string_t)moonbit_string_literal_59.data);
        _M0L6_2atmpS1962 = _M0L1iS439 + 1;
        _M0L6_2atmpS1963 = _M0L1iS439 + 1;
        _M0L1iS439 = _M0L6_2atmpS1962;
        _M0L3segS440 = _M0L6_2atmpS1963;
        goto _2afor_441;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1964;
        int32_t _M0L6_2atmpS1965;
        moonbit_incref(_M0L6_2aenvS436);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
        if (_M0L6loggerS435.$1) {
          moonbit_incref(_M0L6loggerS435.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS435.$0->$method_0(_M0L6loggerS435.$1, (moonbit_string_t)moonbit_string_literal_60.data);
        _M0L6_2atmpS1964 = _M0L1iS439 + 1;
        _M0L6_2atmpS1965 = _M0L1iS439 + 1;
        _M0L1iS439 = _M0L6_2atmpS1964;
        _M0L3segS440 = _M0L6_2atmpS1965;
        goto _2afor_441;
        break;
      }
      default: {
        if (_M0L4codeS442 < 32) {
          int32_t _M0L6_2atmpS1967;
          moonbit_string_t _M0L6_2atmpS1966;
          int32_t _M0L6_2atmpS1968;
          int32_t _M0L6_2atmpS1969;
          moonbit_incref(_M0L6_2aenvS436);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
          if (_M0L6loggerS435.$1) {
            moonbit_incref(_M0L6loggerS435.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS435.$0->$method_0(_M0L6loggerS435.$1, (moonbit_string_t)moonbit_string_literal_81.data);
          _M0L6_2atmpS1967 = _M0L4codeS442 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1966 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1967);
          if (_M0L6loggerS435.$1) {
            moonbit_incref(_M0L6loggerS435.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS435.$0->$method_0(_M0L6loggerS435.$1, _M0L6_2atmpS1966);
          if (_M0L6loggerS435.$1) {
            moonbit_incref(_M0L6loggerS435.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 125);
          _M0L6_2atmpS1968 = _M0L1iS439 + 1;
          _M0L6_2atmpS1969 = _M0L1iS439 + 1;
          _M0L1iS439 = _M0L6_2atmpS1968;
          _M0L3segS440 = _M0L6_2atmpS1969;
          goto _2afor_441;
        } else {
          int32_t _M0L6_2atmpS1970 = _M0L1iS439 + 1;
          int32_t _tmp_3774 = _M0L3segS440;
          _M0L1iS439 = _M0L6_2atmpS1970;
          _M0L3segS440 = _tmp_3774;
          goto _2afor_441;
        }
        break;
      }
    }
    goto joinlet_3773;
    join_443:;
    moonbit_incref(_M0L6_2aenvS436);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS436, _M0L3segS440, _M0L1iS439);
    if (_M0L6loggerS435.$1) {
      moonbit_incref(_M0L6loggerS435.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1955 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS444);
    if (_M0L6loggerS435.$1) {
      moonbit_incref(_M0L6loggerS435.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, _M0L6_2atmpS1955);
    _M0L6_2atmpS1956 = _M0L1iS439 + 1;
    _M0L6_2atmpS1957 = _M0L1iS439 + 1;
    _M0L1iS439 = _M0L6_2atmpS1956;
    _M0L3segS440 = _M0L6_2atmpS1957;
    continue;
    joinlet_3773:;
    _tmp_3775 = _M0L1iS439;
    _tmp_3776 = _M0L3segS440;
    _M0L1iS439 = _tmp_3775;
    _M0L3segS440 = _tmp_3776;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS435.$0->$method_3(_M0L6loggerS435.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS431,
  int32_t _M0L3segS434,
  int32_t _M0L1iS433
) {
  struct _M0TPB6Logger _M0L8_2afieldS3429;
  struct _M0TPB6Logger _M0L6loggerS430;
  moonbit_string_t _M0L8_2afieldS3428;
  int32_t _M0L6_2acntS3592;
  moonbit_string_t _M0L4selfS432;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3429
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS431->$1_0, _M0L6_2aenvS431->$1_1
  };
  _M0L6loggerS430 = _M0L8_2afieldS3429;
  _M0L8_2afieldS3428 = _M0L6_2aenvS431->$0;
  _M0L6_2acntS3592 = Moonbit_object_header(_M0L6_2aenvS431)->rc;
  if (_M0L6_2acntS3592 > 1) {
    int32_t _M0L11_2anew__cntS3593 = _M0L6_2acntS3592 - 1;
    Moonbit_object_header(_M0L6_2aenvS431)->rc = _M0L11_2anew__cntS3593;
    if (_M0L6loggerS430.$1) {
      moonbit_incref(_M0L6loggerS430.$1);
    }
    moonbit_incref(_M0L8_2afieldS3428);
  } else if (_M0L6_2acntS3592 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS431);
  }
  _M0L4selfS432 = _M0L8_2afieldS3428;
  if (_M0L1iS433 > _M0L3segS434) {
    int32_t _M0L6_2atmpS1954 = _M0L1iS433 - _M0L3segS434;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS430.$0->$method_1(_M0L6loggerS430.$1, _M0L4selfS432, _M0L3segS434, _M0L6_2atmpS1954);
  } else {
    moonbit_decref(_M0L4selfS432);
    if (_M0L6loggerS430.$1) {
      moonbit_decref(_M0L6loggerS430.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS429) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS428;
  int32_t _M0L6_2atmpS1951;
  int32_t _M0L6_2atmpS1950;
  int32_t _M0L6_2atmpS1953;
  int32_t _M0L6_2atmpS1952;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1949;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS428 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1951 = _M0IPC14byte4BytePB3Div3div(_M0L1bS429, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1950
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1951);
  moonbit_incref(_M0L7_2aselfS428);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS428, _M0L6_2atmpS1950);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1953 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS429, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1952
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1953);
  moonbit_incref(_M0L7_2aselfS428);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS428, _M0L6_2atmpS1952);
  _M0L6_2atmpS1949 = _M0L7_2aselfS428;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1949);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS427) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS427 < 10) {
    int32_t _M0L6_2atmpS1946;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1946 = _M0IPC14byte4BytePB3Add3add(_M0L1iS427, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1946);
  } else {
    int32_t _M0L6_2atmpS1948;
    int32_t _M0L6_2atmpS1947;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1948 = _M0IPC14byte4BytePB3Add3add(_M0L1iS427, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1947 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1948, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1947);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS425,
  int32_t _M0L4thatS426
) {
  int32_t _M0L6_2atmpS1944;
  int32_t _M0L6_2atmpS1945;
  int32_t _M0L6_2atmpS1943;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1944 = (int32_t)_M0L4selfS425;
  _M0L6_2atmpS1945 = (int32_t)_M0L4thatS426;
  _M0L6_2atmpS1943 = _M0L6_2atmpS1944 - _M0L6_2atmpS1945;
  return _M0L6_2atmpS1943 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS423,
  int32_t _M0L4thatS424
) {
  int32_t _M0L6_2atmpS1941;
  int32_t _M0L6_2atmpS1942;
  int32_t _M0L6_2atmpS1940;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1941 = (int32_t)_M0L4selfS423;
  _M0L6_2atmpS1942 = (int32_t)_M0L4thatS424;
  _M0L6_2atmpS1940 = _M0L6_2atmpS1941 % _M0L6_2atmpS1942;
  return _M0L6_2atmpS1940 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS421,
  int32_t _M0L4thatS422
) {
  int32_t _M0L6_2atmpS1938;
  int32_t _M0L6_2atmpS1939;
  int32_t _M0L6_2atmpS1937;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1938 = (int32_t)_M0L4selfS421;
  _M0L6_2atmpS1939 = (int32_t)_M0L4thatS422;
  _M0L6_2atmpS1937 = _M0L6_2atmpS1938 / _M0L6_2atmpS1939;
  return _M0L6_2atmpS1937 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS419,
  int32_t _M0L4thatS420
) {
  int32_t _M0L6_2atmpS1935;
  int32_t _M0L6_2atmpS1936;
  int32_t _M0L6_2atmpS1934;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1935 = (int32_t)_M0L4selfS419;
  _M0L6_2atmpS1936 = (int32_t)_M0L4thatS420;
  _M0L6_2atmpS1934 = _M0L6_2atmpS1935 + _M0L6_2atmpS1936;
  return _M0L6_2atmpS1934 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS416,
  int32_t _M0L5startS414,
  int32_t _M0L3endS415
) {
  int32_t _if__result_3777;
  int32_t _M0L3lenS417;
  int32_t _M0L6_2atmpS1932;
  int32_t _M0L6_2atmpS1933;
  moonbit_bytes_t _M0L5bytesS418;
  moonbit_bytes_t _M0L6_2atmpS1931;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS414 == 0) {
    int32_t _M0L6_2atmpS1930 = Moonbit_array_length(_M0L3strS416);
    _if__result_3777 = _M0L3endS415 == _M0L6_2atmpS1930;
  } else {
    _if__result_3777 = 0;
  }
  if (_if__result_3777) {
    return _M0L3strS416;
  }
  _M0L3lenS417 = _M0L3endS415 - _M0L5startS414;
  _M0L6_2atmpS1932 = _M0L3lenS417 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1933 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS418
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1932, _M0L6_2atmpS1933);
  moonbit_incref(_M0L5bytesS418);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS418, 0, _M0L3strS416, _M0L5startS414, _M0L3lenS417);
  _M0L6_2atmpS1931 = _M0L5bytesS418;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1931, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS411) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS411;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS412
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS412;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS413) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS413;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS409,
  int32_t _M0L5indexS410
) {
  moonbit_string_t _M0L8_2afieldS3432;
  moonbit_string_t _M0L3strS1927;
  int32_t _M0L8_2afieldS3431;
  int32_t _M0L5startS1929;
  int32_t _M0L6_2atmpS1928;
  int32_t _M0L6_2atmpS3430;
  #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3432 = _M0L4selfS409.$0;
  _M0L3strS1927 = _M0L8_2afieldS3432;
  _M0L8_2afieldS3431 = _M0L4selfS409.$1;
  _M0L5startS1929 = _M0L8_2afieldS3431;
  _M0L6_2atmpS1928 = _M0L5startS1929 + _M0L5indexS410;
  _M0L6_2atmpS3430 = _M0L3strS1927[_M0L6_2atmpS1928];
  moonbit_decref(_M0L3strS1927);
  return _M0L6_2atmpS3430;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS401,
  int32_t _M0L5radixS400
) {
  int32_t _if__result_3778;
  uint16_t* _M0L6bufferS402;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS400 < 2) {
    _if__result_3778 = 1;
  } else {
    _if__result_3778 = _M0L5radixS400 > 36;
  }
  if (_if__result_3778) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_82.data, (moonbit_string_t)moonbit_string_literal_83.data);
  }
  if (_M0L4selfS401 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_67.data;
  }
  switch (_M0L5radixS400) {
    case 10: {
      int32_t _M0L3lenS403;
      uint16_t* _M0L6bufferS404;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS403 = _M0FPB12dec__count64(_M0L4selfS401);
      _M0L6bufferS404 = (uint16_t*)moonbit_make_string(_M0L3lenS403, 0);
      moonbit_incref(_M0L6bufferS404);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS404, _M0L4selfS401, 0, _M0L3lenS403);
      _M0L6bufferS402 = _M0L6bufferS404;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS405;
      uint16_t* _M0L6bufferS406;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS405 = _M0FPB12hex__count64(_M0L4selfS401);
      _M0L6bufferS406 = (uint16_t*)moonbit_make_string(_M0L3lenS405, 0);
      moonbit_incref(_M0L6bufferS406);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS406, _M0L4selfS401, 0, _M0L3lenS405);
      _M0L6bufferS402 = _M0L6bufferS406;
      break;
    }
    default: {
      int32_t _M0L3lenS407;
      uint16_t* _M0L6bufferS408;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS407 = _M0FPB14radix__count64(_M0L4selfS401, _M0L5radixS400);
      _M0L6bufferS408 = (uint16_t*)moonbit_make_string(_M0L3lenS407, 0);
      moonbit_incref(_M0L6bufferS408);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS408, _M0L4selfS401, 0, _M0L3lenS407, _M0L5radixS400);
      _M0L6bufferS402 = _M0L6bufferS408;
      break;
    }
  }
  return _M0L6bufferS402;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS390,
  uint64_t _M0L3numS378,
  int32_t _M0L12digit__startS381,
  int32_t _M0L10total__lenS380
) {
  uint64_t _M0Lm3numS377;
  int32_t _M0Lm6offsetS379;
  uint64_t _M0L6_2atmpS1926;
  int32_t _M0Lm9remainingS392;
  int32_t _M0L6_2atmpS1907;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS377 = _M0L3numS378;
  _M0Lm6offsetS379 = _M0L10total__lenS380 - _M0L12digit__startS381;
  while (1) {
    uint64_t _M0L6_2atmpS1870 = _M0Lm3numS377;
    if (_M0L6_2atmpS1870 >= 10000ull) {
      uint64_t _M0L6_2atmpS1893 = _M0Lm3numS377;
      uint64_t _M0L1tS382 = _M0L6_2atmpS1893 / 10000ull;
      uint64_t _M0L6_2atmpS1892 = _M0Lm3numS377;
      uint64_t _M0L6_2atmpS1891 = _M0L6_2atmpS1892 % 10000ull;
      int32_t _M0L1rS383 = (int32_t)_M0L6_2atmpS1891;
      int32_t _M0L2d1S384;
      int32_t _M0L2d2S385;
      int32_t _M0L6_2atmpS1871;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1889;
      int32_t _M0L6d1__hiS386;
      int32_t _M0L6_2atmpS1888;
      int32_t _M0L6_2atmpS1887;
      int32_t _M0L6d1__loS387;
      int32_t _M0L6_2atmpS1886;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6d2__hiS388;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6d2__loS389;
      int32_t _M0L6_2atmpS1873;
      int32_t _M0L6_2atmpS1872;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1875;
      int32_t _M0L6_2atmpS1874;
      int32_t _M0L6_2atmpS1879;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6_2atmpS1877;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L6_2atmpS1881;
      int32_t _M0L6_2atmpS1880;
      _M0Lm3numS377 = _M0L1tS382;
      _M0L2d1S384 = _M0L1rS383 / 100;
      _M0L2d2S385 = _M0L1rS383 % 100;
      _M0L6_2atmpS1871 = _M0Lm6offsetS379;
      _M0Lm6offsetS379 = _M0L6_2atmpS1871 - 4;
      _M0L6_2atmpS1890 = _M0L2d1S384 / 10;
      _M0L6_2atmpS1889 = 48 + _M0L6_2atmpS1890;
      _M0L6d1__hiS386 = (uint16_t)_M0L6_2atmpS1889;
      _M0L6_2atmpS1888 = _M0L2d1S384 % 10;
      _M0L6_2atmpS1887 = 48 + _M0L6_2atmpS1888;
      _M0L6d1__loS387 = (uint16_t)_M0L6_2atmpS1887;
      _M0L6_2atmpS1886 = _M0L2d2S385 / 10;
      _M0L6_2atmpS1885 = 48 + _M0L6_2atmpS1886;
      _M0L6d2__hiS388 = (uint16_t)_M0L6_2atmpS1885;
      _M0L6_2atmpS1884 = _M0L2d2S385 % 10;
      _M0L6_2atmpS1883 = 48 + _M0L6_2atmpS1884;
      _M0L6d2__loS389 = (uint16_t)_M0L6_2atmpS1883;
      _M0L6_2atmpS1873 = _M0Lm6offsetS379;
      _M0L6_2atmpS1872 = _M0L12digit__startS381 + _M0L6_2atmpS1873;
      _M0L6bufferS390[_M0L6_2atmpS1872] = _M0L6d1__hiS386;
      _M0L6_2atmpS1876 = _M0Lm6offsetS379;
      _M0L6_2atmpS1875 = _M0L12digit__startS381 + _M0L6_2atmpS1876;
      _M0L6_2atmpS1874 = _M0L6_2atmpS1875 + 1;
      _M0L6bufferS390[_M0L6_2atmpS1874] = _M0L6d1__loS387;
      _M0L6_2atmpS1879 = _M0Lm6offsetS379;
      _M0L6_2atmpS1878 = _M0L12digit__startS381 + _M0L6_2atmpS1879;
      _M0L6_2atmpS1877 = _M0L6_2atmpS1878 + 2;
      _M0L6bufferS390[_M0L6_2atmpS1877] = _M0L6d2__hiS388;
      _M0L6_2atmpS1882 = _M0Lm6offsetS379;
      _M0L6_2atmpS1881 = _M0L12digit__startS381 + _M0L6_2atmpS1882;
      _M0L6_2atmpS1880 = _M0L6_2atmpS1881 + 3;
      _M0L6bufferS390[_M0L6_2atmpS1880] = _M0L6d2__loS389;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1926 = _M0Lm3numS377;
  _M0Lm9remainingS392 = (int32_t)_M0L6_2atmpS1926;
  while (1) {
    int32_t _M0L6_2atmpS1894 = _M0Lm9remainingS392;
    if (_M0L6_2atmpS1894 >= 100) {
      int32_t _M0L6_2atmpS1906 = _M0Lm9remainingS392;
      int32_t _M0L1tS393 = _M0L6_2atmpS1906 / 100;
      int32_t _M0L6_2atmpS1905 = _M0Lm9remainingS392;
      int32_t _M0L1dS394 = _M0L6_2atmpS1905 % 100;
      int32_t _M0L6_2atmpS1895;
      int32_t _M0L6_2atmpS1904;
      int32_t _M0L6_2atmpS1903;
      int32_t _M0L5d__hiS395;
      int32_t _M0L6_2atmpS1902;
      int32_t _M0L6_2atmpS1901;
      int32_t _M0L5d__loS396;
      int32_t _M0L6_2atmpS1897;
      int32_t _M0L6_2atmpS1896;
      int32_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1899;
      int32_t _M0L6_2atmpS1898;
      _M0Lm9remainingS392 = _M0L1tS393;
      _M0L6_2atmpS1895 = _M0Lm6offsetS379;
      _M0Lm6offsetS379 = _M0L6_2atmpS1895 - 2;
      _M0L6_2atmpS1904 = _M0L1dS394 / 10;
      _M0L6_2atmpS1903 = 48 + _M0L6_2atmpS1904;
      _M0L5d__hiS395 = (uint16_t)_M0L6_2atmpS1903;
      _M0L6_2atmpS1902 = _M0L1dS394 % 10;
      _M0L6_2atmpS1901 = 48 + _M0L6_2atmpS1902;
      _M0L5d__loS396 = (uint16_t)_M0L6_2atmpS1901;
      _M0L6_2atmpS1897 = _M0Lm6offsetS379;
      _M0L6_2atmpS1896 = _M0L12digit__startS381 + _M0L6_2atmpS1897;
      _M0L6bufferS390[_M0L6_2atmpS1896] = _M0L5d__hiS395;
      _M0L6_2atmpS1900 = _M0Lm6offsetS379;
      _M0L6_2atmpS1899 = _M0L12digit__startS381 + _M0L6_2atmpS1900;
      _M0L6_2atmpS1898 = _M0L6_2atmpS1899 + 1;
      _M0L6bufferS390[_M0L6_2atmpS1898] = _M0L5d__loS396;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1907 = _M0Lm9remainingS392;
  if (_M0L6_2atmpS1907 >= 10) {
    int32_t _M0L6_2atmpS1908 = _M0Lm6offsetS379;
    int32_t _M0L6_2atmpS1919;
    int32_t _M0L6_2atmpS1918;
    int32_t _M0L6_2atmpS1917;
    int32_t _M0L5d__hiS398;
    int32_t _M0L6_2atmpS1916;
    int32_t _M0L6_2atmpS1915;
    int32_t _M0L6_2atmpS1914;
    int32_t _M0L5d__loS399;
    int32_t _M0L6_2atmpS1910;
    int32_t _M0L6_2atmpS1909;
    int32_t _M0L6_2atmpS1913;
    int32_t _M0L6_2atmpS1912;
    int32_t _M0L6_2atmpS1911;
    _M0Lm6offsetS379 = _M0L6_2atmpS1908 - 2;
    _M0L6_2atmpS1919 = _M0Lm9remainingS392;
    _M0L6_2atmpS1918 = _M0L6_2atmpS1919 / 10;
    _M0L6_2atmpS1917 = 48 + _M0L6_2atmpS1918;
    _M0L5d__hiS398 = (uint16_t)_M0L6_2atmpS1917;
    _M0L6_2atmpS1916 = _M0Lm9remainingS392;
    _M0L6_2atmpS1915 = _M0L6_2atmpS1916 % 10;
    _M0L6_2atmpS1914 = 48 + _M0L6_2atmpS1915;
    _M0L5d__loS399 = (uint16_t)_M0L6_2atmpS1914;
    _M0L6_2atmpS1910 = _M0Lm6offsetS379;
    _M0L6_2atmpS1909 = _M0L12digit__startS381 + _M0L6_2atmpS1910;
    _M0L6bufferS390[_M0L6_2atmpS1909] = _M0L5d__hiS398;
    _M0L6_2atmpS1913 = _M0Lm6offsetS379;
    _M0L6_2atmpS1912 = _M0L12digit__startS381 + _M0L6_2atmpS1913;
    _M0L6_2atmpS1911 = _M0L6_2atmpS1912 + 1;
    _M0L6bufferS390[_M0L6_2atmpS1911] = _M0L5d__loS399;
    moonbit_decref(_M0L6bufferS390);
  } else {
    int32_t _M0L6_2atmpS1920 = _M0Lm6offsetS379;
    int32_t _M0L6_2atmpS1925;
    int32_t _M0L6_2atmpS1921;
    int32_t _M0L6_2atmpS1924;
    int32_t _M0L6_2atmpS1923;
    int32_t _M0L6_2atmpS1922;
    _M0Lm6offsetS379 = _M0L6_2atmpS1920 - 1;
    _M0L6_2atmpS1925 = _M0Lm6offsetS379;
    _M0L6_2atmpS1921 = _M0L12digit__startS381 + _M0L6_2atmpS1925;
    _M0L6_2atmpS1924 = _M0Lm9remainingS392;
    _M0L6_2atmpS1923 = 48 + _M0L6_2atmpS1924;
    _M0L6_2atmpS1922 = (uint16_t)_M0L6_2atmpS1923;
    _M0L6bufferS390[_M0L6_2atmpS1921] = _M0L6_2atmpS1922;
    moonbit_decref(_M0L6bufferS390);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS372,
  uint64_t _M0L3numS366,
  int32_t _M0L12digit__startS364,
  int32_t _M0L10total__lenS363,
  int32_t _M0L5radixS368
) {
  int32_t _M0Lm6offsetS362;
  uint64_t _M0Lm1nS365;
  uint64_t _M0L4baseS367;
  int32_t _M0L6_2atmpS1852;
  int32_t _M0L6_2atmpS1851;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS362 = _M0L10total__lenS363 - _M0L12digit__startS364;
  _M0Lm1nS365 = _M0L3numS366;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS367 = _M0MPC13int3Int10to__uint64(_M0L5radixS368);
  _M0L6_2atmpS1852 = _M0L5radixS368 - 1;
  _M0L6_2atmpS1851 = _M0L5radixS368 & _M0L6_2atmpS1852;
  if (_M0L6_2atmpS1851 == 0) {
    int32_t _M0L5shiftS369;
    uint64_t _M0L4maskS370;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS369 = moonbit_ctz32(_M0L5radixS368);
    _M0L4maskS370 = _M0L4baseS367 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1853 = _M0Lm1nS365;
      if (_M0L6_2atmpS1853 > 0ull) {
        int32_t _M0L6_2atmpS1854 = _M0Lm6offsetS362;
        uint64_t _M0L6_2atmpS1860;
        uint64_t _M0L6_2atmpS1859;
        int32_t _M0L5digitS371;
        int32_t _M0L6_2atmpS1857;
        int32_t _M0L6_2atmpS1855;
        int32_t _M0L6_2atmpS1856;
        uint64_t _M0L6_2atmpS1858;
        _M0Lm6offsetS362 = _M0L6_2atmpS1854 - 1;
        _M0L6_2atmpS1860 = _M0Lm1nS365;
        _M0L6_2atmpS1859 = _M0L6_2atmpS1860 & _M0L4maskS370;
        _M0L5digitS371 = (int32_t)_M0L6_2atmpS1859;
        _M0L6_2atmpS1857 = _M0Lm6offsetS362;
        _M0L6_2atmpS1855 = _M0L12digit__startS364 + _M0L6_2atmpS1857;
        _M0L6_2atmpS1856
        = ((moonbit_string_t)moonbit_string_literal_84.data)[
          _M0L5digitS371
        ];
        _M0L6bufferS372[_M0L6_2atmpS1855] = _M0L6_2atmpS1856;
        _M0L6_2atmpS1858 = _M0Lm1nS365;
        _M0Lm1nS365 = _M0L6_2atmpS1858 >> (_M0L5shiftS369 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS372);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1861 = _M0Lm1nS365;
      if (_M0L6_2atmpS1861 > 0ull) {
        int32_t _M0L6_2atmpS1862 = _M0Lm6offsetS362;
        uint64_t _M0L6_2atmpS1869;
        uint64_t _M0L1qS374;
        uint64_t _M0L6_2atmpS1867;
        uint64_t _M0L6_2atmpS1868;
        uint64_t _M0L6_2atmpS1866;
        int32_t _M0L5digitS375;
        int32_t _M0L6_2atmpS1865;
        int32_t _M0L6_2atmpS1863;
        int32_t _M0L6_2atmpS1864;
        _M0Lm6offsetS362 = _M0L6_2atmpS1862 - 1;
        _M0L6_2atmpS1869 = _M0Lm1nS365;
        _M0L1qS374 = _M0L6_2atmpS1869 / _M0L4baseS367;
        _M0L6_2atmpS1867 = _M0Lm1nS365;
        _M0L6_2atmpS1868 = _M0L1qS374 * _M0L4baseS367;
        _M0L6_2atmpS1866 = _M0L6_2atmpS1867 - _M0L6_2atmpS1868;
        _M0L5digitS375 = (int32_t)_M0L6_2atmpS1866;
        _M0L6_2atmpS1865 = _M0Lm6offsetS362;
        _M0L6_2atmpS1863 = _M0L12digit__startS364 + _M0L6_2atmpS1865;
        _M0L6_2atmpS1864
        = ((moonbit_string_t)moonbit_string_literal_84.data)[
          _M0L5digitS375
        ];
        _M0L6bufferS372[_M0L6_2atmpS1863] = _M0L6_2atmpS1864;
        _M0Lm1nS365 = _M0L1qS374;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS372);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS359,
  uint64_t _M0L3numS355,
  int32_t _M0L12digit__startS353,
  int32_t _M0L10total__lenS352
) {
  int32_t _M0Lm6offsetS351;
  uint64_t _M0Lm1nS354;
  int32_t _M0L6_2atmpS1847;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS351 = _M0L10total__lenS352 - _M0L12digit__startS353;
  _M0Lm1nS354 = _M0L3numS355;
  while (1) {
    int32_t _M0L6_2atmpS1835 = _M0Lm6offsetS351;
    if (_M0L6_2atmpS1835 >= 2) {
      int32_t _M0L6_2atmpS1836 = _M0Lm6offsetS351;
      uint64_t _M0L6_2atmpS1846;
      uint64_t _M0L6_2atmpS1845;
      int32_t _M0L9byte__valS356;
      int32_t _M0L2hiS357;
      int32_t _M0L2loS358;
      int32_t _M0L6_2atmpS1839;
      int32_t _M0L6_2atmpS1837;
      int32_t _M0L6_2atmpS1838;
      int32_t _M0L6_2atmpS1843;
      int32_t _M0L6_2atmpS1842;
      int32_t _M0L6_2atmpS1840;
      int32_t _M0L6_2atmpS1841;
      uint64_t _M0L6_2atmpS1844;
      _M0Lm6offsetS351 = _M0L6_2atmpS1836 - 2;
      _M0L6_2atmpS1846 = _M0Lm1nS354;
      _M0L6_2atmpS1845 = _M0L6_2atmpS1846 & 255ull;
      _M0L9byte__valS356 = (int32_t)_M0L6_2atmpS1845;
      _M0L2hiS357 = _M0L9byte__valS356 / 16;
      _M0L2loS358 = _M0L9byte__valS356 % 16;
      _M0L6_2atmpS1839 = _M0Lm6offsetS351;
      _M0L6_2atmpS1837 = _M0L12digit__startS353 + _M0L6_2atmpS1839;
      _M0L6_2atmpS1838
      = ((moonbit_string_t)moonbit_string_literal_84.data)[
        _M0L2hiS357
      ];
      _M0L6bufferS359[_M0L6_2atmpS1837] = _M0L6_2atmpS1838;
      _M0L6_2atmpS1843 = _M0Lm6offsetS351;
      _M0L6_2atmpS1842 = _M0L12digit__startS353 + _M0L6_2atmpS1843;
      _M0L6_2atmpS1840 = _M0L6_2atmpS1842 + 1;
      _M0L6_2atmpS1841
      = ((moonbit_string_t)moonbit_string_literal_84.data)[
        _M0L2loS358
      ];
      _M0L6bufferS359[_M0L6_2atmpS1840] = _M0L6_2atmpS1841;
      _M0L6_2atmpS1844 = _M0Lm1nS354;
      _M0Lm1nS354 = _M0L6_2atmpS1844 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1847 = _M0Lm6offsetS351;
  if (_M0L6_2atmpS1847 == 1) {
    uint64_t _M0L6_2atmpS1850 = _M0Lm1nS354;
    uint64_t _M0L6_2atmpS1849 = _M0L6_2atmpS1850 & 15ull;
    int32_t _M0L6nibbleS361 = (int32_t)_M0L6_2atmpS1849;
    int32_t _M0L6_2atmpS1848 =
      ((moonbit_string_t)moonbit_string_literal_84.data)[_M0L6nibbleS361];
    _M0L6bufferS359[_M0L12digit__startS353] = _M0L6_2atmpS1848;
    moonbit_decref(_M0L6bufferS359);
  } else {
    moonbit_decref(_M0L6bufferS359);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS345,
  int32_t _M0L5radixS348
) {
  uint64_t _M0Lm3numS346;
  uint64_t _M0L4baseS347;
  int32_t _M0Lm5countS349;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS345 == 0ull) {
    return 1;
  }
  _M0Lm3numS346 = _M0L5valueS345;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS347 = _M0MPC13int3Int10to__uint64(_M0L5radixS348);
  _M0Lm5countS349 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1832 = _M0Lm3numS346;
    if (_M0L6_2atmpS1832 > 0ull) {
      int32_t _M0L6_2atmpS1833 = _M0Lm5countS349;
      uint64_t _M0L6_2atmpS1834;
      _M0Lm5countS349 = _M0L6_2atmpS1833 + 1;
      _M0L6_2atmpS1834 = _M0Lm3numS346;
      _M0Lm3numS346 = _M0L6_2atmpS1834 / _M0L4baseS347;
      continue;
    }
    break;
  }
  return _M0Lm5countS349;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS343) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS343 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS344;
    int32_t _M0L6_2atmpS1831;
    int32_t _M0L6_2atmpS1830;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS344 = moonbit_clz64(_M0L5valueS343);
    _M0L6_2atmpS1831 = 63 - _M0L14leading__zerosS344;
    _M0L6_2atmpS1830 = _M0L6_2atmpS1831 / 4;
    return _M0L6_2atmpS1830 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS342) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS342 >= 10000000000ull) {
    if (_M0L5valueS342 >= 100000000000000ull) {
      if (_M0L5valueS342 >= 10000000000000000ull) {
        if (_M0L5valueS342 >= 1000000000000000000ull) {
          if (_M0L5valueS342 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS342 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS342 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS342 >= 1000000000000ull) {
      if (_M0L5valueS342 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS342 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS342 >= 100000ull) {
    if (_M0L5valueS342 >= 10000000ull) {
      if (_M0L5valueS342 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS342 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS342 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS342 >= 1000ull) {
    if (_M0L5valueS342 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS342 >= 100ull) {
    return 3;
  } else if (_M0L5valueS342 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS326,
  int32_t _M0L5radixS325
) {
  int32_t _if__result_3785;
  int32_t _M0L12is__negativeS327;
  uint32_t _M0L3numS328;
  uint16_t* _M0L6bufferS329;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS325 < 2) {
    _if__result_3785 = 1;
  } else {
    _if__result_3785 = _M0L5radixS325 > 36;
  }
  if (_if__result_3785) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_82.data, (moonbit_string_t)moonbit_string_literal_85.data);
  }
  if (_M0L4selfS326 == 0) {
    return (moonbit_string_t)moonbit_string_literal_67.data;
  }
  _M0L12is__negativeS327 = _M0L4selfS326 < 0;
  if (_M0L12is__negativeS327) {
    int32_t _M0L6_2atmpS1829 = -_M0L4selfS326;
    _M0L3numS328 = *(uint32_t*)&_M0L6_2atmpS1829;
  } else {
    _M0L3numS328 = *(uint32_t*)&_M0L4selfS326;
  }
  switch (_M0L5radixS325) {
    case 10: {
      int32_t _M0L10digit__lenS330;
      int32_t _M0L6_2atmpS1826;
      int32_t _M0L10total__lenS331;
      uint16_t* _M0L6bufferS332;
      int32_t _M0L12digit__startS333;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS330 = _M0FPB12dec__count32(_M0L3numS328);
      if (_M0L12is__negativeS327) {
        _M0L6_2atmpS1826 = 1;
      } else {
        _M0L6_2atmpS1826 = 0;
      }
      _M0L10total__lenS331 = _M0L10digit__lenS330 + _M0L6_2atmpS1826;
      _M0L6bufferS332
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS331, 0);
      if (_M0L12is__negativeS327) {
        _M0L12digit__startS333 = 1;
      } else {
        _M0L12digit__startS333 = 0;
      }
      moonbit_incref(_M0L6bufferS332);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS332, _M0L3numS328, _M0L12digit__startS333, _M0L10total__lenS331);
      _M0L6bufferS329 = _M0L6bufferS332;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS334;
      int32_t _M0L6_2atmpS1827;
      int32_t _M0L10total__lenS335;
      uint16_t* _M0L6bufferS336;
      int32_t _M0L12digit__startS337;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS334 = _M0FPB12hex__count32(_M0L3numS328);
      if (_M0L12is__negativeS327) {
        _M0L6_2atmpS1827 = 1;
      } else {
        _M0L6_2atmpS1827 = 0;
      }
      _M0L10total__lenS335 = _M0L10digit__lenS334 + _M0L6_2atmpS1827;
      _M0L6bufferS336
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS335, 0);
      if (_M0L12is__negativeS327) {
        _M0L12digit__startS337 = 1;
      } else {
        _M0L12digit__startS337 = 0;
      }
      moonbit_incref(_M0L6bufferS336);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS336, _M0L3numS328, _M0L12digit__startS337, _M0L10total__lenS335);
      _M0L6bufferS329 = _M0L6bufferS336;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS338;
      int32_t _M0L6_2atmpS1828;
      int32_t _M0L10total__lenS339;
      uint16_t* _M0L6bufferS340;
      int32_t _M0L12digit__startS341;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS338
      = _M0FPB14radix__count32(_M0L3numS328, _M0L5radixS325);
      if (_M0L12is__negativeS327) {
        _M0L6_2atmpS1828 = 1;
      } else {
        _M0L6_2atmpS1828 = 0;
      }
      _M0L10total__lenS339 = _M0L10digit__lenS338 + _M0L6_2atmpS1828;
      _M0L6bufferS340
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS339, 0);
      if (_M0L12is__negativeS327) {
        _M0L12digit__startS341 = 1;
      } else {
        _M0L12digit__startS341 = 0;
      }
      moonbit_incref(_M0L6bufferS340);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS340, _M0L3numS328, _M0L12digit__startS341, _M0L10total__lenS339, _M0L5radixS325);
      _M0L6bufferS329 = _M0L6bufferS340;
      break;
    }
  }
  if (_M0L12is__negativeS327) {
    _M0L6bufferS329[0] = 45;
  }
  return _M0L6bufferS329;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS319,
  int32_t _M0L5radixS322
) {
  uint32_t _M0Lm3numS320;
  uint32_t _M0L4baseS321;
  int32_t _M0Lm5countS323;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS319 == 0u) {
    return 1;
  }
  _M0Lm3numS320 = _M0L5valueS319;
  _M0L4baseS321 = *(uint32_t*)&_M0L5radixS322;
  _M0Lm5countS323 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1823 = _M0Lm3numS320;
    if (_M0L6_2atmpS1823 > 0u) {
      int32_t _M0L6_2atmpS1824 = _M0Lm5countS323;
      uint32_t _M0L6_2atmpS1825;
      _M0Lm5countS323 = _M0L6_2atmpS1824 + 1;
      _M0L6_2atmpS1825 = _M0Lm3numS320;
      _M0Lm3numS320 = _M0L6_2atmpS1825 / _M0L4baseS321;
      continue;
    }
    break;
  }
  return _M0Lm5countS323;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS317) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS317 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS318;
    int32_t _M0L6_2atmpS1822;
    int32_t _M0L6_2atmpS1821;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS318 = moonbit_clz32(_M0L5valueS317);
    _M0L6_2atmpS1822 = 31 - _M0L14leading__zerosS318;
    _M0L6_2atmpS1821 = _M0L6_2atmpS1822 / 4;
    return _M0L6_2atmpS1821 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS316) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS316 >= 100000u) {
    if (_M0L5valueS316 >= 10000000u) {
      if (_M0L5valueS316 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS316 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS316 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS316 >= 1000u) {
    if (_M0L5valueS316 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS316 >= 100u) {
    return 3;
  } else if (_M0L5valueS316 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS306,
  uint32_t _M0L3numS294,
  int32_t _M0L12digit__startS297,
  int32_t _M0L10total__lenS296
) {
  uint32_t _M0Lm3numS293;
  int32_t _M0Lm6offsetS295;
  uint32_t _M0L6_2atmpS1820;
  int32_t _M0Lm9remainingS308;
  int32_t _M0L6_2atmpS1801;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS293 = _M0L3numS294;
  _M0Lm6offsetS295 = _M0L10total__lenS296 - _M0L12digit__startS297;
  while (1) {
    uint32_t _M0L6_2atmpS1764 = _M0Lm3numS293;
    if (_M0L6_2atmpS1764 >= 10000u) {
      uint32_t _M0L6_2atmpS1787 = _M0Lm3numS293;
      uint32_t _M0L1tS298 = _M0L6_2atmpS1787 / 10000u;
      uint32_t _M0L6_2atmpS1786 = _M0Lm3numS293;
      uint32_t _M0L6_2atmpS1785 = _M0L6_2atmpS1786 % 10000u;
      int32_t _M0L1rS299 = *(int32_t*)&_M0L6_2atmpS1785;
      int32_t _M0L2d1S300;
      int32_t _M0L2d2S301;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1784;
      int32_t _M0L6_2atmpS1783;
      int32_t _M0L6d1__hiS302;
      int32_t _M0L6_2atmpS1782;
      int32_t _M0L6_2atmpS1781;
      int32_t _M0L6d1__loS303;
      int32_t _M0L6_2atmpS1780;
      int32_t _M0L6_2atmpS1779;
      int32_t _M0L6d2__hiS304;
      int32_t _M0L6_2atmpS1778;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6d2__loS305;
      int32_t _M0L6_2atmpS1767;
      int32_t _M0L6_2atmpS1766;
      int32_t _M0L6_2atmpS1770;
      int32_t _M0L6_2atmpS1769;
      int32_t _M0L6_2atmpS1768;
      int32_t _M0L6_2atmpS1773;
      int32_t _M0L6_2atmpS1772;
      int32_t _M0L6_2atmpS1771;
      int32_t _M0L6_2atmpS1776;
      int32_t _M0L6_2atmpS1775;
      int32_t _M0L6_2atmpS1774;
      _M0Lm3numS293 = _M0L1tS298;
      _M0L2d1S300 = _M0L1rS299 / 100;
      _M0L2d2S301 = _M0L1rS299 % 100;
      _M0L6_2atmpS1765 = _M0Lm6offsetS295;
      _M0Lm6offsetS295 = _M0L6_2atmpS1765 - 4;
      _M0L6_2atmpS1784 = _M0L2d1S300 / 10;
      _M0L6_2atmpS1783 = 48 + _M0L6_2atmpS1784;
      _M0L6d1__hiS302 = (uint16_t)_M0L6_2atmpS1783;
      _M0L6_2atmpS1782 = _M0L2d1S300 % 10;
      _M0L6_2atmpS1781 = 48 + _M0L6_2atmpS1782;
      _M0L6d1__loS303 = (uint16_t)_M0L6_2atmpS1781;
      _M0L6_2atmpS1780 = _M0L2d2S301 / 10;
      _M0L6_2atmpS1779 = 48 + _M0L6_2atmpS1780;
      _M0L6d2__hiS304 = (uint16_t)_M0L6_2atmpS1779;
      _M0L6_2atmpS1778 = _M0L2d2S301 % 10;
      _M0L6_2atmpS1777 = 48 + _M0L6_2atmpS1778;
      _M0L6d2__loS305 = (uint16_t)_M0L6_2atmpS1777;
      _M0L6_2atmpS1767 = _M0Lm6offsetS295;
      _M0L6_2atmpS1766 = _M0L12digit__startS297 + _M0L6_2atmpS1767;
      _M0L6bufferS306[_M0L6_2atmpS1766] = _M0L6d1__hiS302;
      _M0L6_2atmpS1770 = _M0Lm6offsetS295;
      _M0L6_2atmpS1769 = _M0L12digit__startS297 + _M0L6_2atmpS1770;
      _M0L6_2atmpS1768 = _M0L6_2atmpS1769 + 1;
      _M0L6bufferS306[_M0L6_2atmpS1768] = _M0L6d1__loS303;
      _M0L6_2atmpS1773 = _M0Lm6offsetS295;
      _M0L6_2atmpS1772 = _M0L12digit__startS297 + _M0L6_2atmpS1773;
      _M0L6_2atmpS1771 = _M0L6_2atmpS1772 + 2;
      _M0L6bufferS306[_M0L6_2atmpS1771] = _M0L6d2__hiS304;
      _M0L6_2atmpS1776 = _M0Lm6offsetS295;
      _M0L6_2atmpS1775 = _M0L12digit__startS297 + _M0L6_2atmpS1776;
      _M0L6_2atmpS1774 = _M0L6_2atmpS1775 + 3;
      _M0L6bufferS306[_M0L6_2atmpS1774] = _M0L6d2__loS305;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1820 = _M0Lm3numS293;
  _M0Lm9remainingS308 = *(int32_t*)&_M0L6_2atmpS1820;
  while (1) {
    int32_t _M0L6_2atmpS1788 = _M0Lm9remainingS308;
    if (_M0L6_2atmpS1788 >= 100) {
      int32_t _M0L6_2atmpS1800 = _M0Lm9remainingS308;
      int32_t _M0L1tS309 = _M0L6_2atmpS1800 / 100;
      int32_t _M0L6_2atmpS1799 = _M0Lm9remainingS308;
      int32_t _M0L1dS310 = _M0L6_2atmpS1799 % 100;
      int32_t _M0L6_2atmpS1789;
      int32_t _M0L6_2atmpS1798;
      int32_t _M0L6_2atmpS1797;
      int32_t _M0L5d__hiS311;
      int32_t _M0L6_2atmpS1796;
      int32_t _M0L6_2atmpS1795;
      int32_t _M0L5d__loS312;
      int32_t _M0L6_2atmpS1791;
      int32_t _M0L6_2atmpS1790;
      int32_t _M0L6_2atmpS1794;
      int32_t _M0L6_2atmpS1793;
      int32_t _M0L6_2atmpS1792;
      _M0Lm9remainingS308 = _M0L1tS309;
      _M0L6_2atmpS1789 = _M0Lm6offsetS295;
      _M0Lm6offsetS295 = _M0L6_2atmpS1789 - 2;
      _M0L6_2atmpS1798 = _M0L1dS310 / 10;
      _M0L6_2atmpS1797 = 48 + _M0L6_2atmpS1798;
      _M0L5d__hiS311 = (uint16_t)_M0L6_2atmpS1797;
      _M0L6_2atmpS1796 = _M0L1dS310 % 10;
      _M0L6_2atmpS1795 = 48 + _M0L6_2atmpS1796;
      _M0L5d__loS312 = (uint16_t)_M0L6_2atmpS1795;
      _M0L6_2atmpS1791 = _M0Lm6offsetS295;
      _M0L6_2atmpS1790 = _M0L12digit__startS297 + _M0L6_2atmpS1791;
      _M0L6bufferS306[_M0L6_2atmpS1790] = _M0L5d__hiS311;
      _M0L6_2atmpS1794 = _M0Lm6offsetS295;
      _M0L6_2atmpS1793 = _M0L12digit__startS297 + _M0L6_2atmpS1794;
      _M0L6_2atmpS1792 = _M0L6_2atmpS1793 + 1;
      _M0L6bufferS306[_M0L6_2atmpS1792] = _M0L5d__loS312;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1801 = _M0Lm9remainingS308;
  if (_M0L6_2atmpS1801 >= 10) {
    int32_t _M0L6_2atmpS1802 = _M0Lm6offsetS295;
    int32_t _M0L6_2atmpS1813;
    int32_t _M0L6_2atmpS1812;
    int32_t _M0L6_2atmpS1811;
    int32_t _M0L5d__hiS314;
    int32_t _M0L6_2atmpS1810;
    int32_t _M0L6_2atmpS1809;
    int32_t _M0L6_2atmpS1808;
    int32_t _M0L5d__loS315;
    int32_t _M0L6_2atmpS1804;
    int32_t _M0L6_2atmpS1803;
    int32_t _M0L6_2atmpS1807;
    int32_t _M0L6_2atmpS1806;
    int32_t _M0L6_2atmpS1805;
    _M0Lm6offsetS295 = _M0L6_2atmpS1802 - 2;
    _M0L6_2atmpS1813 = _M0Lm9remainingS308;
    _M0L6_2atmpS1812 = _M0L6_2atmpS1813 / 10;
    _M0L6_2atmpS1811 = 48 + _M0L6_2atmpS1812;
    _M0L5d__hiS314 = (uint16_t)_M0L6_2atmpS1811;
    _M0L6_2atmpS1810 = _M0Lm9remainingS308;
    _M0L6_2atmpS1809 = _M0L6_2atmpS1810 % 10;
    _M0L6_2atmpS1808 = 48 + _M0L6_2atmpS1809;
    _M0L5d__loS315 = (uint16_t)_M0L6_2atmpS1808;
    _M0L6_2atmpS1804 = _M0Lm6offsetS295;
    _M0L6_2atmpS1803 = _M0L12digit__startS297 + _M0L6_2atmpS1804;
    _M0L6bufferS306[_M0L6_2atmpS1803] = _M0L5d__hiS314;
    _M0L6_2atmpS1807 = _M0Lm6offsetS295;
    _M0L6_2atmpS1806 = _M0L12digit__startS297 + _M0L6_2atmpS1807;
    _M0L6_2atmpS1805 = _M0L6_2atmpS1806 + 1;
    _M0L6bufferS306[_M0L6_2atmpS1805] = _M0L5d__loS315;
    moonbit_decref(_M0L6bufferS306);
  } else {
    int32_t _M0L6_2atmpS1814 = _M0Lm6offsetS295;
    int32_t _M0L6_2atmpS1819;
    int32_t _M0L6_2atmpS1815;
    int32_t _M0L6_2atmpS1818;
    int32_t _M0L6_2atmpS1817;
    int32_t _M0L6_2atmpS1816;
    _M0Lm6offsetS295 = _M0L6_2atmpS1814 - 1;
    _M0L6_2atmpS1819 = _M0Lm6offsetS295;
    _M0L6_2atmpS1815 = _M0L12digit__startS297 + _M0L6_2atmpS1819;
    _M0L6_2atmpS1818 = _M0Lm9remainingS308;
    _M0L6_2atmpS1817 = 48 + _M0L6_2atmpS1818;
    _M0L6_2atmpS1816 = (uint16_t)_M0L6_2atmpS1817;
    _M0L6bufferS306[_M0L6_2atmpS1815] = _M0L6_2atmpS1816;
    moonbit_decref(_M0L6bufferS306);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS288,
  uint32_t _M0L3numS282,
  int32_t _M0L12digit__startS280,
  int32_t _M0L10total__lenS279,
  int32_t _M0L5radixS284
) {
  int32_t _M0Lm6offsetS278;
  uint32_t _M0Lm1nS281;
  uint32_t _M0L4baseS283;
  int32_t _M0L6_2atmpS1746;
  int32_t _M0L6_2atmpS1745;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS278 = _M0L10total__lenS279 - _M0L12digit__startS280;
  _M0Lm1nS281 = _M0L3numS282;
  _M0L4baseS283 = *(uint32_t*)&_M0L5radixS284;
  _M0L6_2atmpS1746 = _M0L5radixS284 - 1;
  _M0L6_2atmpS1745 = _M0L5radixS284 & _M0L6_2atmpS1746;
  if (_M0L6_2atmpS1745 == 0) {
    int32_t _M0L5shiftS285;
    uint32_t _M0L4maskS286;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS285 = moonbit_ctz32(_M0L5radixS284);
    _M0L4maskS286 = _M0L4baseS283 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1747 = _M0Lm1nS281;
      if (_M0L6_2atmpS1747 > 0u) {
        int32_t _M0L6_2atmpS1748 = _M0Lm6offsetS278;
        uint32_t _M0L6_2atmpS1754;
        uint32_t _M0L6_2atmpS1753;
        int32_t _M0L5digitS287;
        int32_t _M0L6_2atmpS1751;
        int32_t _M0L6_2atmpS1749;
        int32_t _M0L6_2atmpS1750;
        uint32_t _M0L6_2atmpS1752;
        _M0Lm6offsetS278 = _M0L6_2atmpS1748 - 1;
        _M0L6_2atmpS1754 = _M0Lm1nS281;
        _M0L6_2atmpS1753 = _M0L6_2atmpS1754 & _M0L4maskS286;
        _M0L5digitS287 = *(int32_t*)&_M0L6_2atmpS1753;
        _M0L6_2atmpS1751 = _M0Lm6offsetS278;
        _M0L6_2atmpS1749 = _M0L12digit__startS280 + _M0L6_2atmpS1751;
        _M0L6_2atmpS1750
        = ((moonbit_string_t)moonbit_string_literal_84.data)[
          _M0L5digitS287
        ];
        _M0L6bufferS288[_M0L6_2atmpS1749] = _M0L6_2atmpS1750;
        _M0L6_2atmpS1752 = _M0Lm1nS281;
        _M0Lm1nS281 = _M0L6_2atmpS1752 >> (_M0L5shiftS285 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS288);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1755 = _M0Lm1nS281;
      if (_M0L6_2atmpS1755 > 0u) {
        int32_t _M0L6_2atmpS1756 = _M0Lm6offsetS278;
        uint32_t _M0L6_2atmpS1763;
        uint32_t _M0L1qS290;
        uint32_t _M0L6_2atmpS1761;
        uint32_t _M0L6_2atmpS1762;
        uint32_t _M0L6_2atmpS1760;
        int32_t _M0L5digitS291;
        int32_t _M0L6_2atmpS1759;
        int32_t _M0L6_2atmpS1757;
        int32_t _M0L6_2atmpS1758;
        _M0Lm6offsetS278 = _M0L6_2atmpS1756 - 1;
        _M0L6_2atmpS1763 = _M0Lm1nS281;
        _M0L1qS290 = _M0L6_2atmpS1763 / _M0L4baseS283;
        _M0L6_2atmpS1761 = _M0Lm1nS281;
        _M0L6_2atmpS1762 = _M0L1qS290 * _M0L4baseS283;
        _M0L6_2atmpS1760 = _M0L6_2atmpS1761 - _M0L6_2atmpS1762;
        _M0L5digitS291 = *(int32_t*)&_M0L6_2atmpS1760;
        _M0L6_2atmpS1759 = _M0Lm6offsetS278;
        _M0L6_2atmpS1757 = _M0L12digit__startS280 + _M0L6_2atmpS1759;
        _M0L6_2atmpS1758
        = ((moonbit_string_t)moonbit_string_literal_84.data)[
          _M0L5digitS291
        ];
        _M0L6bufferS288[_M0L6_2atmpS1757] = _M0L6_2atmpS1758;
        _M0Lm1nS281 = _M0L1qS290;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS288);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS275,
  uint32_t _M0L3numS271,
  int32_t _M0L12digit__startS269,
  int32_t _M0L10total__lenS268
) {
  int32_t _M0Lm6offsetS267;
  uint32_t _M0Lm1nS270;
  int32_t _M0L6_2atmpS1741;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS267 = _M0L10total__lenS268 - _M0L12digit__startS269;
  _M0Lm1nS270 = _M0L3numS271;
  while (1) {
    int32_t _M0L6_2atmpS1729 = _M0Lm6offsetS267;
    if (_M0L6_2atmpS1729 >= 2) {
      int32_t _M0L6_2atmpS1730 = _M0Lm6offsetS267;
      uint32_t _M0L6_2atmpS1740;
      uint32_t _M0L6_2atmpS1739;
      int32_t _M0L9byte__valS272;
      int32_t _M0L2hiS273;
      int32_t _M0L2loS274;
      int32_t _M0L6_2atmpS1733;
      int32_t _M0L6_2atmpS1731;
      int32_t _M0L6_2atmpS1732;
      int32_t _M0L6_2atmpS1737;
      int32_t _M0L6_2atmpS1736;
      int32_t _M0L6_2atmpS1734;
      int32_t _M0L6_2atmpS1735;
      uint32_t _M0L6_2atmpS1738;
      _M0Lm6offsetS267 = _M0L6_2atmpS1730 - 2;
      _M0L6_2atmpS1740 = _M0Lm1nS270;
      _M0L6_2atmpS1739 = _M0L6_2atmpS1740 & 255u;
      _M0L9byte__valS272 = *(int32_t*)&_M0L6_2atmpS1739;
      _M0L2hiS273 = _M0L9byte__valS272 / 16;
      _M0L2loS274 = _M0L9byte__valS272 % 16;
      _M0L6_2atmpS1733 = _M0Lm6offsetS267;
      _M0L6_2atmpS1731 = _M0L12digit__startS269 + _M0L6_2atmpS1733;
      _M0L6_2atmpS1732
      = ((moonbit_string_t)moonbit_string_literal_84.data)[
        _M0L2hiS273
      ];
      _M0L6bufferS275[_M0L6_2atmpS1731] = _M0L6_2atmpS1732;
      _M0L6_2atmpS1737 = _M0Lm6offsetS267;
      _M0L6_2atmpS1736 = _M0L12digit__startS269 + _M0L6_2atmpS1737;
      _M0L6_2atmpS1734 = _M0L6_2atmpS1736 + 1;
      _M0L6_2atmpS1735
      = ((moonbit_string_t)moonbit_string_literal_84.data)[
        _M0L2loS274
      ];
      _M0L6bufferS275[_M0L6_2atmpS1734] = _M0L6_2atmpS1735;
      _M0L6_2atmpS1738 = _M0Lm1nS270;
      _M0Lm1nS270 = _M0L6_2atmpS1738 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1741 = _M0Lm6offsetS267;
  if (_M0L6_2atmpS1741 == 1) {
    uint32_t _M0L6_2atmpS1744 = _M0Lm1nS270;
    uint32_t _M0L6_2atmpS1743 = _M0L6_2atmpS1744 & 15u;
    int32_t _M0L6nibbleS277 = *(int32_t*)&_M0L6_2atmpS1743;
    int32_t _M0L6_2atmpS1742 =
      ((moonbit_string_t)moonbit_string_literal_84.data)[_M0L6nibbleS277];
    _M0L6bufferS275[_M0L12digit__startS269] = _M0L6_2atmpS1742;
    moonbit_decref(_M0L6bufferS275);
  } else {
    moonbit_decref(_M0L6bufferS275);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS262) {
  struct _M0TWEOs* _M0L7_2afuncS261;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS261 = _M0L4selfS262;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS261->code(_M0L7_2afuncS261);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS264
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS263;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS263 = _M0L4selfS264;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS263->code(_M0L7_2afuncS263);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS266) {
  struct _M0TWEOc* _M0L7_2afuncS265;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS265 = _M0L4selfS266;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS265->code(_M0L7_2afuncS265);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS252
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS251;
  struct _M0TPB6Logger _M0L6_2atmpS1724;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS251 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS251);
  _M0L6_2atmpS1724
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS251
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS252, _M0L6_2atmpS1724);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS251);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS254
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS253;
  struct _M0TPB6Logger _M0L6_2atmpS1725;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS253 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS253);
  _M0L6_2atmpS1725
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS253
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS254, _M0L6_2atmpS1725);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS253);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS256
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS255;
  struct _M0TPB6Logger _M0L6_2atmpS1726;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS255 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS255);
  _M0L6_2atmpS1726
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS255
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS256, _M0L6_2atmpS1726);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS255);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPC14json15JsonDecodeErrorE(
  void* _M0L4selfS258
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS257;
  struct _M0TPB6Logger _M0L6_2atmpS1727;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS257 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS257);
  _M0L6_2atmpS1727
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS257
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14json15JsonDecodeErrorPB4Show6output(_M0L4selfS258, _M0L6_2atmpS1727);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS257);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS260
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS259;
  struct _M0TPB6Logger _M0L6_2atmpS1728;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS259 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS259);
  _M0L6_2atmpS1728
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS259
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS260, _M0L6_2atmpS1728);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS259);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS250
) {
  int32_t _M0L8_2afieldS3433;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3433 = _M0L4selfS250.$1;
  moonbit_decref(_M0L4selfS250.$0);
  return _M0L8_2afieldS3433;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS249
) {
  int32_t _M0L3endS1722;
  int32_t _M0L8_2afieldS3434;
  int32_t _M0L5startS1723;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1722 = _M0L4selfS249.$2;
  _M0L8_2afieldS3434 = _M0L4selfS249.$1;
  moonbit_decref(_M0L4selfS249.$0);
  _M0L5startS1723 = _M0L8_2afieldS3434;
  return _M0L3endS1722 - _M0L5startS1723;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS248
) {
  moonbit_string_t _M0L8_2afieldS3435;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3435 = _M0L4selfS248.$0;
  return _M0L8_2afieldS3435;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS244,
  moonbit_string_t _M0L5valueS245,
  int32_t _M0L5startS246,
  int32_t _M0L3lenS247
) {
  int32_t _M0L6_2atmpS1721;
  int64_t _M0L6_2atmpS1720;
  struct _M0TPC16string10StringView _M0L6_2atmpS1719;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1721 = _M0L5startS246 + _M0L3lenS247;
  _M0L6_2atmpS1720 = (int64_t)_M0L6_2atmpS1721;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1719
  = _M0MPC16string6String11sub_2einner(_M0L5valueS245, _M0L5startS246, _M0L6_2atmpS1720);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS244, _M0L6_2atmpS1719);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS237,
  int32_t _M0L5startS243,
  int64_t _M0L3endS239
) {
  int32_t _M0L3lenS236;
  int32_t _M0L3endS238;
  int32_t _M0L5startS242;
  int32_t _if__result_3792;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS236 = Moonbit_array_length(_M0L4selfS237);
  if (_M0L3endS239 == 4294967296ll) {
    _M0L3endS238 = _M0L3lenS236;
  } else {
    int64_t _M0L7_2aSomeS240 = _M0L3endS239;
    int32_t _M0L6_2aendS241 = (int32_t)_M0L7_2aSomeS240;
    if (_M0L6_2aendS241 < 0) {
      _M0L3endS238 = _M0L3lenS236 + _M0L6_2aendS241;
    } else {
      _M0L3endS238 = _M0L6_2aendS241;
    }
  }
  if (_M0L5startS243 < 0) {
    _M0L5startS242 = _M0L3lenS236 + _M0L5startS243;
  } else {
    _M0L5startS242 = _M0L5startS243;
  }
  if (_M0L5startS242 >= 0) {
    if (_M0L5startS242 <= _M0L3endS238) {
      _if__result_3792 = _M0L3endS238 <= _M0L3lenS236;
    } else {
      _if__result_3792 = 0;
    }
  } else {
    _if__result_3792 = 0;
  }
  if (_if__result_3792) {
    if (_M0L5startS242 < _M0L3lenS236) {
      int32_t _M0L6_2atmpS1716 = _M0L4selfS237[_M0L5startS242];
      int32_t _M0L6_2atmpS1715;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1715
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1716);
      if (!_M0L6_2atmpS1715) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS238 < _M0L3lenS236) {
      int32_t _M0L6_2atmpS1718 = _M0L4selfS237[_M0L3endS238];
      int32_t _M0L6_2atmpS1717;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1717
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1718);
      if (!_M0L6_2atmpS1717) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS242,
                                                 _M0L3endS238,
                                                 _M0L4selfS237};
  } else {
    moonbit_decref(_M0L4selfS237);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS233) {
  struct _M0TPB6Hasher* _M0L1hS232;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS232 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS232);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS232, _M0L4selfS233);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS232);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS235
) {
  struct _M0TPB6Hasher* _M0L1hS234;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS234 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS234);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS234, _M0L4selfS235);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS234);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS230) {
  int32_t _M0L4seedS229;
  if (_M0L10seed_2eoptS230 == 4294967296ll) {
    _M0L4seedS229 = 0;
  } else {
    int64_t _M0L7_2aSomeS231 = _M0L10seed_2eoptS230;
    _M0L4seedS229 = (int32_t)_M0L7_2aSomeS231;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS229);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS228) {
  uint32_t _M0L6_2atmpS1714;
  uint32_t _M0L6_2atmpS1713;
  struct _M0TPB6Hasher* _block_3793;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1714 = *(uint32_t*)&_M0L4seedS228;
  _M0L6_2atmpS1713 = _M0L6_2atmpS1714 + 374761393u;
  _block_3793
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3793)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3793->$0 = _M0L6_2atmpS1713;
  return _block_3793;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS227) {
  uint32_t _M0L6_2atmpS1712;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1712 = _M0MPB6Hasher9avalanche(_M0L4selfS227);
  return *(int32_t*)&_M0L6_2atmpS1712;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS226) {
  uint32_t _M0L8_2afieldS3436;
  uint32_t _M0Lm3accS225;
  uint32_t _M0L6_2atmpS1701;
  uint32_t _M0L6_2atmpS1703;
  uint32_t _M0L6_2atmpS1702;
  uint32_t _M0L6_2atmpS1704;
  uint32_t _M0L6_2atmpS1705;
  uint32_t _M0L6_2atmpS1707;
  uint32_t _M0L6_2atmpS1706;
  uint32_t _M0L6_2atmpS1708;
  uint32_t _M0L6_2atmpS1709;
  uint32_t _M0L6_2atmpS1711;
  uint32_t _M0L6_2atmpS1710;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3436 = _M0L4selfS226->$0;
  moonbit_decref(_M0L4selfS226);
  _M0Lm3accS225 = _M0L8_2afieldS3436;
  _M0L6_2atmpS1701 = _M0Lm3accS225;
  _M0L6_2atmpS1703 = _M0Lm3accS225;
  _M0L6_2atmpS1702 = _M0L6_2atmpS1703 >> 15;
  _M0Lm3accS225 = _M0L6_2atmpS1701 ^ _M0L6_2atmpS1702;
  _M0L6_2atmpS1704 = _M0Lm3accS225;
  _M0Lm3accS225 = _M0L6_2atmpS1704 * 2246822519u;
  _M0L6_2atmpS1705 = _M0Lm3accS225;
  _M0L6_2atmpS1707 = _M0Lm3accS225;
  _M0L6_2atmpS1706 = _M0L6_2atmpS1707 >> 13;
  _M0Lm3accS225 = _M0L6_2atmpS1705 ^ _M0L6_2atmpS1706;
  _M0L6_2atmpS1708 = _M0Lm3accS225;
  _M0Lm3accS225 = _M0L6_2atmpS1708 * 3266489917u;
  _M0L6_2atmpS1709 = _M0Lm3accS225;
  _M0L6_2atmpS1711 = _M0Lm3accS225;
  _M0L6_2atmpS1710 = _M0L6_2atmpS1711 >> 16;
  _M0Lm3accS225 = _M0L6_2atmpS1709 ^ _M0L6_2atmpS1710;
  return _M0Lm3accS225;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS223,
  moonbit_string_t _M0L1yS224
) {
  int32_t _M0L6_2atmpS3437;
  int32_t _M0L6_2atmpS1700;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3437 = moonbit_val_array_equal(_M0L1xS223, _M0L1yS224);
  moonbit_decref(_M0L1xS223);
  moonbit_decref(_M0L1yS224);
  _M0L6_2atmpS1700 = _M0L6_2atmpS3437;
  return !_M0L6_2atmpS1700;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS220,
  int32_t _M0L5valueS219
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS219, _M0L4selfS220);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS222,
  moonbit_string_t _M0L5valueS221
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS221, _M0L4selfS222);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS218) {
  int64_t _M0L6_2atmpS1699;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1699 = (int64_t)_M0L4selfS218;
  return *(uint64_t*)&_M0L6_2atmpS1699;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS216,
  moonbit_string_t _M0L4reprS217
) {
  void* _block_3794;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3794 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_3794)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_3794)->$0 = _M0L6numberS216;
  ((struct _M0DTPB4Json6Number*)_block_3794)->$1 = _M0L4reprS217;
  return _block_3794;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS214,
  int32_t _M0L5valueS215
) {
  uint32_t _M0L6_2atmpS1698;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1698 = *(uint32_t*)&_M0L5valueS215;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS214, _M0L6_2atmpS1698);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS207
) {
  struct _M0TPB13StringBuilder* _M0L3bufS205;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS206;
  int32_t _M0L7_2abindS208;
  int32_t _M0L1iS209;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS205 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS206 = _M0L4selfS207;
  moonbit_incref(_M0L3bufS205);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS205, 91);
  _M0L7_2abindS208 = _M0L7_2aselfS206->$1;
  _M0L1iS209 = 0;
  while (1) {
    if (_M0L1iS209 < _M0L7_2abindS208) {
      int32_t _if__result_3796;
      moonbit_string_t* _M0L8_2afieldS3439;
      moonbit_string_t* _M0L3bufS1696;
      moonbit_string_t _M0L6_2atmpS3438;
      moonbit_string_t _M0L4itemS210;
      int32_t _M0L6_2atmpS1697;
      if (_M0L1iS209 != 0) {
        moonbit_incref(_M0L3bufS205);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS205, (moonbit_string_t)moonbit_string_literal_65.data);
      }
      if (_M0L1iS209 < 0) {
        _if__result_3796 = 1;
      } else {
        int32_t _M0L3lenS1695 = _M0L7_2aselfS206->$1;
        _if__result_3796 = _M0L1iS209 >= _M0L3lenS1695;
      }
      if (_if__result_3796) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3439 = _M0L7_2aselfS206->$0;
      _M0L3bufS1696 = _M0L8_2afieldS3439;
      _M0L6_2atmpS3438 = (moonbit_string_t)_M0L3bufS1696[_M0L1iS209];
      _M0L4itemS210 = _M0L6_2atmpS3438;
      if (_M0L4itemS210 == 0) {
        moonbit_incref(_M0L3bufS205);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS205, (moonbit_string_t)moonbit_string_literal_43.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS211 = _M0L4itemS210;
        moonbit_string_t _M0L6_2alocS212 = _M0L7_2aSomeS211;
        moonbit_string_t _M0L6_2atmpS1694;
        moonbit_incref(_M0L6_2alocS212);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1694
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS212);
        moonbit_incref(_M0L3bufS205);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS205, _M0L6_2atmpS1694);
      }
      _M0L6_2atmpS1697 = _M0L1iS209 + 1;
      _M0L1iS209 = _M0L6_2atmpS1697;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS206);
    }
    break;
  }
  moonbit_incref(_M0L3bufS205);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS205, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS205);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS204
) {
  moonbit_string_t _M0L6_2atmpS1693;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1692;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1693 = _M0L4selfS204;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1692 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1693);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1692);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS203
) {
  struct _M0TPB13StringBuilder* _M0L2sbS202;
  struct _M0TPC16string10StringView _M0L8_2afieldS3452;
  struct _M0TPC16string10StringView _M0L3pkgS1677;
  moonbit_string_t _M0L6_2atmpS1676;
  moonbit_string_t _M0L6_2atmpS3451;
  moonbit_string_t _M0L6_2atmpS1675;
  moonbit_string_t _M0L6_2atmpS3450;
  moonbit_string_t _M0L6_2atmpS1674;
  struct _M0TPC16string10StringView _M0L8_2afieldS3449;
  struct _M0TPC16string10StringView _M0L8filenameS1678;
  struct _M0TPC16string10StringView _M0L8_2afieldS3448;
  struct _M0TPC16string10StringView _M0L11start__lineS1681;
  moonbit_string_t _M0L6_2atmpS1680;
  moonbit_string_t _M0L6_2atmpS3447;
  moonbit_string_t _M0L6_2atmpS1679;
  struct _M0TPC16string10StringView _M0L8_2afieldS3446;
  struct _M0TPC16string10StringView _M0L13start__columnS1684;
  moonbit_string_t _M0L6_2atmpS1683;
  moonbit_string_t _M0L6_2atmpS3445;
  moonbit_string_t _M0L6_2atmpS1682;
  struct _M0TPC16string10StringView _M0L8_2afieldS3444;
  struct _M0TPC16string10StringView _M0L9end__lineS1687;
  moonbit_string_t _M0L6_2atmpS1686;
  moonbit_string_t _M0L6_2atmpS3443;
  moonbit_string_t _M0L6_2atmpS1685;
  struct _M0TPC16string10StringView _M0L8_2afieldS3442;
  int32_t _M0L6_2acntS3594;
  struct _M0TPC16string10StringView _M0L11end__columnS1691;
  moonbit_string_t _M0L6_2atmpS1690;
  moonbit_string_t _M0L6_2atmpS3441;
  moonbit_string_t _M0L6_2atmpS1689;
  moonbit_string_t _M0L6_2atmpS3440;
  moonbit_string_t _M0L6_2atmpS1688;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS202 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3452
  = (struct _M0TPC16string10StringView){
    _M0L4selfS203->$0_1, _M0L4selfS203->$0_2, _M0L4selfS203->$0_0
  };
  _M0L3pkgS1677 = _M0L8_2afieldS3452;
  moonbit_incref(_M0L3pkgS1677.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1676
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1677);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3451
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_86.data, _M0L6_2atmpS1676);
  moonbit_decref(_M0L6_2atmpS1676);
  _M0L6_2atmpS1675 = _M0L6_2atmpS3451;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3450
  = moonbit_add_string(_M0L6_2atmpS1675, (moonbit_string_t)moonbit_string_literal_87.data);
  moonbit_decref(_M0L6_2atmpS1675);
  _M0L6_2atmpS1674 = _M0L6_2atmpS3450;
  moonbit_incref(_M0L2sbS202);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS202, _M0L6_2atmpS1674);
  moonbit_incref(_M0L2sbS202);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS202, (moonbit_string_t)moonbit_string_literal_88.data);
  _M0L8_2afieldS3449
  = (struct _M0TPC16string10StringView){
    _M0L4selfS203->$1_1, _M0L4selfS203->$1_2, _M0L4selfS203->$1_0
  };
  _M0L8filenameS1678 = _M0L8_2afieldS3449;
  moonbit_incref(_M0L8filenameS1678.$0);
  moonbit_incref(_M0L2sbS202);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS202, _M0L8filenameS1678);
  _M0L8_2afieldS3448
  = (struct _M0TPC16string10StringView){
    _M0L4selfS203->$2_1, _M0L4selfS203->$2_2, _M0L4selfS203->$2_0
  };
  _M0L11start__lineS1681 = _M0L8_2afieldS3448;
  moonbit_incref(_M0L11start__lineS1681.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1680
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1681);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3447
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_89.data, _M0L6_2atmpS1680);
  moonbit_decref(_M0L6_2atmpS1680);
  _M0L6_2atmpS1679 = _M0L6_2atmpS3447;
  moonbit_incref(_M0L2sbS202);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS202, _M0L6_2atmpS1679);
  _M0L8_2afieldS3446
  = (struct _M0TPC16string10StringView){
    _M0L4selfS203->$3_1, _M0L4selfS203->$3_2, _M0L4selfS203->$3_0
  };
  _M0L13start__columnS1684 = _M0L8_2afieldS3446;
  moonbit_incref(_M0L13start__columnS1684.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1683
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1684);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3445
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_90.data, _M0L6_2atmpS1683);
  moonbit_decref(_M0L6_2atmpS1683);
  _M0L6_2atmpS1682 = _M0L6_2atmpS3445;
  moonbit_incref(_M0L2sbS202);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS202, _M0L6_2atmpS1682);
  _M0L8_2afieldS3444
  = (struct _M0TPC16string10StringView){
    _M0L4selfS203->$4_1, _M0L4selfS203->$4_2, _M0L4selfS203->$4_0
  };
  _M0L9end__lineS1687 = _M0L8_2afieldS3444;
  moonbit_incref(_M0L9end__lineS1687.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1686
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1687);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3443
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_91.data, _M0L6_2atmpS1686);
  moonbit_decref(_M0L6_2atmpS1686);
  _M0L6_2atmpS1685 = _M0L6_2atmpS3443;
  moonbit_incref(_M0L2sbS202);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS202, _M0L6_2atmpS1685);
  _M0L8_2afieldS3442
  = (struct _M0TPC16string10StringView){
    _M0L4selfS203->$5_1, _M0L4selfS203->$5_2, _M0L4selfS203->$5_0
  };
  _M0L6_2acntS3594 = Moonbit_object_header(_M0L4selfS203)->rc;
  if (_M0L6_2acntS3594 > 1) {
    int32_t _M0L11_2anew__cntS3600 = _M0L6_2acntS3594 - 1;
    Moonbit_object_header(_M0L4selfS203)->rc = _M0L11_2anew__cntS3600;
    moonbit_incref(_M0L8_2afieldS3442.$0);
  } else if (_M0L6_2acntS3594 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3599 =
      (struct _M0TPC16string10StringView){_M0L4selfS203->$4_1,
                                            _M0L4selfS203->$4_2,
                                            _M0L4selfS203->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3598;
    struct _M0TPC16string10StringView _M0L8_2afieldS3597;
    struct _M0TPC16string10StringView _M0L8_2afieldS3596;
    struct _M0TPC16string10StringView _M0L8_2afieldS3595;
    moonbit_decref(_M0L8_2afieldS3599.$0);
    _M0L8_2afieldS3598
    = (struct _M0TPC16string10StringView){
      _M0L4selfS203->$3_1, _M0L4selfS203->$3_2, _M0L4selfS203->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3598.$0);
    _M0L8_2afieldS3597
    = (struct _M0TPC16string10StringView){
      _M0L4selfS203->$2_1, _M0L4selfS203->$2_2, _M0L4selfS203->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3597.$0);
    _M0L8_2afieldS3596
    = (struct _M0TPC16string10StringView){
      _M0L4selfS203->$1_1, _M0L4selfS203->$1_2, _M0L4selfS203->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3596.$0);
    _M0L8_2afieldS3595
    = (struct _M0TPC16string10StringView){
      _M0L4selfS203->$0_1, _M0L4selfS203->$0_2, _M0L4selfS203->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3595.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS203);
  }
  _M0L11end__columnS1691 = _M0L8_2afieldS3442;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1690
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1691);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3441
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_92.data, _M0L6_2atmpS1690);
  moonbit_decref(_M0L6_2atmpS1690);
  _M0L6_2atmpS1689 = _M0L6_2atmpS3441;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3440
  = moonbit_add_string(_M0L6_2atmpS1689, (moonbit_string_t)moonbit_string_literal_8.data);
  moonbit_decref(_M0L6_2atmpS1689);
  _M0L6_2atmpS1688 = _M0L6_2atmpS3440;
  moonbit_incref(_M0L2sbS202);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS202, _M0L6_2atmpS1688);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS202);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS200,
  moonbit_string_t _M0L3strS201
) {
  int32_t _M0L3lenS1664;
  int32_t _M0L6_2atmpS1666;
  int32_t _M0L6_2atmpS1665;
  int32_t _M0L6_2atmpS1663;
  moonbit_bytes_t _M0L8_2afieldS3454;
  moonbit_bytes_t _M0L4dataS1667;
  int32_t _M0L3lenS1668;
  int32_t _M0L6_2atmpS1669;
  int32_t _M0L3lenS1671;
  int32_t _M0L6_2atmpS3453;
  int32_t _M0L6_2atmpS1673;
  int32_t _M0L6_2atmpS1672;
  int32_t _M0L6_2atmpS1670;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1664 = _M0L4selfS200->$1;
  _M0L6_2atmpS1666 = Moonbit_array_length(_M0L3strS201);
  _M0L6_2atmpS1665 = _M0L6_2atmpS1666 * 2;
  _M0L6_2atmpS1663 = _M0L3lenS1664 + _M0L6_2atmpS1665;
  moonbit_incref(_M0L4selfS200);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS200, _M0L6_2atmpS1663);
  _M0L8_2afieldS3454 = _M0L4selfS200->$0;
  _M0L4dataS1667 = _M0L8_2afieldS3454;
  _M0L3lenS1668 = _M0L4selfS200->$1;
  _M0L6_2atmpS1669 = Moonbit_array_length(_M0L3strS201);
  moonbit_incref(_M0L4dataS1667);
  moonbit_incref(_M0L3strS201);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1667, _M0L3lenS1668, _M0L3strS201, 0, _M0L6_2atmpS1669);
  _M0L3lenS1671 = _M0L4selfS200->$1;
  _M0L6_2atmpS3453 = Moonbit_array_length(_M0L3strS201);
  moonbit_decref(_M0L3strS201);
  _M0L6_2atmpS1673 = _M0L6_2atmpS3453;
  _M0L6_2atmpS1672 = _M0L6_2atmpS1673 * 2;
  _M0L6_2atmpS1670 = _M0L3lenS1671 + _M0L6_2atmpS1672;
  _M0L4selfS200->$1 = _M0L6_2atmpS1670;
  moonbit_decref(_M0L4selfS200);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS192,
  int32_t _M0L13bytes__offsetS187,
  moonbit_string_t _M0L3strS194,
  int32_t _M0L11str__offsetS190,
  int32_t _M0L6lengthS188
) {
  int32_t _M0L6_2atmpS1662;
  int32_t _M0L6_2atmpS1661;
  int32_t _M0L2e1S186;
  int32_t _M0L6_2atmpS1660;
  int32_t _M0L2e2S189;
  int32_t _M0L4len1S191;
  int32_t _M0L4len2S193;
  int32_t _if__result_3797;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1662 = _M0L6lengthS188 * 2;
  _M0L6_2atmpS1661 = _M0L13bytes__offsetS187 + _M0L6_2atmpS1662;
  _M0L2e1S186 = _M0L6_2atmpS1661 - 1;
  _M0L6_2atmpS1660 = _M0L11str__offsetS190 + _M0L6lengthS188;
  _M0L2e2S189 = _M0L6_2atmpS1660 - 1;
  _M0L4len1S191 = Moonbit_array_length(_M0L4selfS192);
  _M0L4len2S193 = Moonbit_array_length(_M0L3strS194);
  if (_M0L6lengthS188 >= 0) {
    if (_M0L13bytes__offsetS187 >= 0) {
      if (_M0L2e1S186 < _M0L4len1S191) {
        if (_M0L11str__offsetS190 >= 0) {
          _if__result_3797 = _M0L2e2S189 < _M0L4len2S193;
        } else {
          _if__result_3797 = 0;
        }
      } else {
        _if__result_3797 = 0;
      }
    } else {
      _if__result_3797 = 0;
    }
  } else {
    _if__result_3797 = 0;
  }
  if (_if__result_3797) {
    int32_t _M0L16end__str__offsetS195 =
      _M0L11str__offsetS190 + _M0L6lengthS188;
    int32_t _M0L1iS196 = _M0L11str__offsetS190;
    int32_t _M0L1jS197 = _M0L13bytes__offsetS187;
    while (1) {
      if (_M0L1iS196 < _M0L16end__str__offsetS195) {
        int32_t _M0L6_2atmpS1657 = _M0L3strS194[_M0L1iS196];
        int32_t _M0L6_2atmpS1656 = (int32_t)_M0L6_2atmpS1657;
        uint32_t _M0L1cS198 = *(uint32_t*)&_M0L6_2atmpS1656;
        uint32_t _M0L6_2atmpS1652 = _M0L1cS198 & 255u;
        int32_t _M0L6_2atmpS1651;
        int32_t _M0L6_2atmpS1653;
        uint32_t _M0L6_2atmpS1655;
        int32_t _M0L6_2atmpS1654;
        int32_t _M0L6_2atmpS1658;
        int32_t _M0L6_2atmpS1659;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1651 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1652);
        if (
          _M0L1jS197 < 0 || _M0L1jS197 >= Moonbit_array_length(_M0L4selfS192)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS192[_M0L1jS197] = _M0L6_2atmpS1651;
        _M0L6_2atmpS1653 = _M0L1jS197 + 1;
        _M0L6_2atmpS1655 = _M0L1cS198 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1654 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1655);
        if (
          _M0L6_2atmpS1653 < 0
          || _M0L6_2atmpS1653 >= Moonbit_array_length(_M0L4selfS192)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS192[_M0L6_2atmpS1653] = _M0L6_2atmpS1654;
        _M0L6_2atmpS1658 = _M0L1iS196 + 1;
        _M0L6_2atmpS1659 = _M0L1jS197 + 2;
        _M0L1iS196 = _M0L6_2atmpS1658;
        _M0L1jS197 = _M0L6_2atmpS1659;
        continue;
      } else {
        moonbit_decref(_M0L3strS194);
        moonbit_decref(_M0L4selfS192);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS194);
    moonbit_decref(_M0L4selfS192);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS183,
  double _M0L3objS182
) {
  struct _M0TPB6Logger _M0L6_2atmpS1649;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1649
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS183
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS182, _M0L6_2atmpS1649);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS185,
  struct _M0TPC16string10StringView _M0L3objS184
) {
  struct _M0TPB6Logger _M0L6_2atmpS1650;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1650
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS185
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS184, _M0L6_2atmpS1650);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS128
) {
  int32_t _M0L6_2atmpS1648;
  struct _M0TPC16string10StringView _M0L7_2abindS127;
  moonbit_string_t _M0L7_2adataS129;
  int32_t _M0L8_2astartS130;
  int32_t _M0L6_2atmpS1647;
  int32_t _M0L6_2aendS131;
  int32_t _M0Lm9_2acursorS132;
  int32_t _M0Lm13accept__stateS133;
  int32_t _M0Lm10match__endS134;
  int32_t _M0Lm20match__tag__saver__0S135;
  int32_t _M0Lm20match__tag__saver__1S136;
  int32_t _M0Lm20match__tag__saver__2S137;
  int32_t _M0Lm20match__tag__saver__3S138;
  int32_t _M0Lm20match__tag__saver__4S139;
  int32_t _M0Lm6tag__0S140;
  int32_t _M0Lm6tag__1S141;
  int32_t _M0Lm9tag__1__1S142;
  int32_t _M0Lm9tag__1__2S143;
  int32_t _M0Lm6tag__3S144;
  int32_t _M0Lm6tag__2S145;
  int32_t _M0Lm9tag__2__1S146;
  int32_t _M0Lm6tag__4S147;
  int32_t _M0L6_2atmpS1605;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1648 = Moonbit_array_length(_M0L4reprS128);
  _M0L7_2abindS127
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1648, _M0L4reprS128
  };
  moonbit_incref(_M0L7_2abindS127.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS129 = _M0MPC16string10StringView4data(_M0L7_2abindS127);
  moonbit_incref(_M0L7_2abindS127.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS130
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS127);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1647 = _M0MPC16string10StringView6length(_M0L7_2abindS127);
  _M0L6_2aendS131 = _M0L8_2astartS130 + _M0L6_2atmpS1647;
  _M0Lm9_2acursorS132 = _M0L8_2astartS130;
  _M0Lm13accept__stateS133 = -1;
  _M0Lm10match__endS134 = -1;
  _M0Lm20match__tag__saver__0S135 = -1;
  _M0Lm20match__tag__saver__1S136 = -1;
  _M0Lm20match__tag__saver__2S137 = -1;
  _M0Lm20match__tag__saver__3S138 = -1;
  _M0Lm20match__tag__saver__4S139 = -1;
  _M0Lm6tag__0S140 = -1;
  _M0Lm6tag__1S141 = -1;
  _M0Lm9tag__1__1S142 = -1;
  _M0Lm9tag__1__2S143 = -1;
  _M0Lm6tag__3S144 = -1;
  _M0Lm6tag__2S145 = -1;
  _M0Lm9tag__2__1S146 = -1;
  _M0Lm6tag__4S147 = -1;
  _M0L6_2atmpS1605 = _M0Lm9_2acursorS132;
  if (_M0L6_2atmpS1605 < _M0L6_2aendS131) {
    int32_t _M0L6_2atmpS1607 = _M0Lm9_2acursorS132;
    int32_t _M0L6_2atmpS1606;
    moonbit_incref(_M0L7_2adataS129);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1606
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1607);
    if (_M0L6_2atmpS1606 == 64) {
      int32_t _M0L6_2atmpS1608 = _M0Lm9_2acursorS132;
      _M0Lm9_2acursorS132 = _M0L6_2atmpS1608 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1609;
        _M0Lm6tag__0S140 = _M0Lm9_2acursorS132;
        _M0L6_2atmpS1609 = _M0Lm9_2acursorS132;
        if (_M0L6_2atmpS1609 < _M0L6_2aendS131) {
          int32_t _M0L6_2atmpS1646 = _M0Lm9_2acursorS132;
          int32_t _M0L10next__charS155;
          int32_t _M0L6_2atmpS1610;
          moonbit_incref(_M0L7_2adataS129);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS155
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1646);
          _M0L6_2atmpS1610 = _M0Lm9_2acursorS132;
          _M0Lm9_2acursorS132 = _M0L6_2atmpS1610 + 1;
          if (_M0L10next__charS155 == 58) {
            int32_t _M0L6_2atmpS1611 = _M0Lm9_2acursorS132;
            if (_M0L6_2atmpS1611 < _M0L6_2aendS131) {
              int32_t _M0L6_2atmpS1612 = _M0Lm9_2acursorS132;
              int32_t _M0L12dispatch__15S156;
              _M0Lm9_2acursorS132 = _M0L6_2atmpS1612 + 1;
              _M0L12dispatch__15S156 = 0;
              loop__label__15_159:;
              while (1) {
                int32_t _M0L6_2atmpS1613;
                switch (_M0L12dispatch__15S156) {
                  case 3: {
                    int32_t _M0L6_2atmpS1616;
                    _M0Lm9tag__1__2S143 = _M0Lm9tag__1__1S142;
                    _M0Lm9tag__1__1S142 = _M0Lm6tag__1S141;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1616 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1616 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1621 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS163;
                      int32_t _M0L6_2atmpS1617;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS163
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1621);
                      _M0L6_2atmpS1617 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1617 + 1;
                      if (_M0L10next__charS163 < 58) {
                        if (_M0L10next__charS163 < 48) {
                          goto join_162;
                        } else {
                          int32_t _M0L6_2atmpS1618;
                          _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                          _M0Lm9tag__2__1S146 = _M0Lm6tag__2S145;
                          _M0Lm6tag__2S145 = _M0Lm9_2acursorS132;
                          _M0Lm6tag__3S144 = _M0Lm9_2acursorS132;
                          _M0L6_2atmpS1618 = _M0Lm9_2acursorS132;
                          if (_M0L6_2atmpS1618 < _M0L6_2aendS131) {
                            int32_t _M0L6_2atmpS1620 = _M0Lm9_2acursorS132;
                            int32_t _M0L10next__charS165;
                            int32_t _M0L6_2atmpS1619;
                            moonbit_incref(_M0L7_2adataS129);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS165
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1620);
                            _M0L6_2atmpS1619 = _M0Lm9_2acursorS132;
                            _M0Lm9_2acursorS132 = _M0L6_2atmpS1619 + 1;
                            if (_M0L10next__charS165 < 48) {
                              if (_M0L10next__charS165 == 45) {
                                goto join_157;
                              } else {
                                goto join_164;
                              }
                            } else if (_M0L10next__charS165 > 57) {
                              if (_M0L10next__charS165 < 59) {
                                _M0L12dispatch__15S156 = 3;
                                goto loop__label__15_159;
                              } else {
                                goto join_164;
                              }
                            } else {
                              _M0L12dispatch__15S156 = 6;
                              goto loop__label__15_159;
                            }
                            join_164:;
                            _M0L12dispatch__15S156 = 0;
                            goto loop__label__15_159;
                          } else {
                            goto join_148;
                          }
                        }
                      } else if (_M0L10next__charS163 > 58) {
                        goto join_162;
                      } else {
                        _M0L12dispatch__15S156 = 1;
                        goto loop__label__15_159;
                      }
                      join_162:;
                      _M0L12dispatch__15S156 = 0;
                      goto loop__label__15_159;
                    } else {
                      goto join_148;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1622;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0Lm6tag__2S145 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1622 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1622 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1624 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS167;
                      int32_t _M0L6_2atmpS1623;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS167
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1624);
                      _M0L6_2atmpS1623 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1623 + 1;
                      if (_M0L10next__charS167 < 58) {
                        if (_M0L10next__charS167 < 48) {
                          goto join_166;
                        } else {
                          _M0L12dispatch__15S156 = 2;
                          goto loop__label__15_159;
                        }
                      } else if (_M0L10next__charS167 > 58) {
                        goto join_166;
                      } else {
                        _M0L12dispatch__15S156 = 3;
                        goto loop__label__15_159;
                      }
                      join_166:;
                      _M0L12dispatch__15S156 = 0;
                      goto loop__label__15_159;
                    } else {
                      goto join_148;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1625;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1625 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1625 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1627 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1626;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1627);
                      _M0L6_2atmpS1626 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1626 + 1;
                      if (_M0L10next__charS168 == 58) {
                        _M0L12dispatch__15S156 = 1;
                        goto loop__label__15_159;
                      } else {
                        _M0L12dispatch__15S156 = 0;
                        goto loop__label__15_159;
                      }
                    } else {
                      goto join_148;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1628;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0Lm6tag__4S147 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1628 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1628 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1636 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1629;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1636);
                      _M0L6_2atmpS1629 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1629 + 1;
                      if (_M0L10next__charS170 < 58) {
                        if (_M0L10next__charS170 < 48) {
                          goto join_169;
                        } else {
                          _M0L12dispatch__15S156 = 4;
                          goto loop__label__15_159;
                        }
                      } else if (_M0L10next__charS170 > 58) {
                        goto join_169;
                      } else {
                        int32_t _M0L6_2atmpS1630;
                        _M0Lm9tag__1__2S143 = _M0Lm9tag__1__1S142;
                        _M0Lm9tag__1__1S142 = _M0Lm6tag__1S141;
                        _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                        _M0L6_2atmpS1630 = _M0Lm9_2acursorS132;
                        if (_M0L6_2atmpS1630 < _M0L6_2aendS131) {
                          int32_t _M0L6_2atmpS1635 = _M0Lm9_2acursorS132;
                          int32_t _M0L10next__charS172;
                          int32_t _M0L6_2atmpS1631;
                          moonbit_incref(_M0L7_2adataS129);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS172
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1635);
                          _M0L6_2atmpS1631 = _M0Lm9_2acursorS132;
                          _M0Lm9_2acursorS132 = _M0L6_2atmpS1631 + 1;
                          if (_M0L10next__charS172 < 58) {
                            if (_M0L10next__charS172 < 48) {
                              goto join_171;
                            } else {
                              int32_t _M0L6_2atmpS1632;
                              _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                              _M0Lm9tag__2__1S146 = _M0Lm6tag__2S145;
                              _M0Lm6tag__2S145 = _M0Lm9_2acursorS132;
                              _M0L6_2atmpS1632 = _M0Lm9_2acursorS132;
                              if (_M0L6_2atmpS1632 < _M0L6_2aendS131) {
                                int32_t _M0L6_2atmpS1634 =
                                  _M0Lm9_2acursorS132;
                                int32_t _M0L10next__charS174;
                                int32_t _M0L6_2atmpS1633;
                                moonbit_incref(_M0L7_2adataS129);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS174
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1634);
                                _M0L6_2atmpS1633 = _M0Lm9_2acursorS132;
                                _M0Lm9_2acursorS132 = _M0L6_2atmpS1633 + 1;
                                if (_M0L10next__charS174 < 58) {
                                  if (_M0L10next__charS174 < 48) {
                                    goto join_173;
                                  } else {
                                    _M0L12dispatch__15S156 = 5;
                                    goto loop__label__15_159;
                                  }
                                } else if (_M0L10next__charS174 > 58) {
                                  goto join_173;
                                } else {
                                  _M0L12dispatch__15S156 = 3;
                                  goto loop__label__15_159;
                                }
                                join_173:;
                                _M0L12dispatch__15S156 = 0;
                                goto loop__label__15_159;
                              } else {
                                goto join_161;
                              }
                            }
                          } else if (_M0L10next__charS172 > 58) {
                            goto join_171;
                          } else {
                            _M0L12dispatch__15S156 = 1;
                            goto loop__label__15_159;
                          }
                          join_171:;
                          _M0L12dispatch__15S156 = 0;
                          goto loop__label__15_159;
                        } else {
                          goto join_148;
                        }
                      }
                      join_169:;
                      _M0L12dispatch__15S156 = 0;
                      goto loop__label__15_159;
                    } else {
                      goto join_148;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1637;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0Lm6tag__2S145 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1637 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1637 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1639 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS176;
                      int32_t _M0L6_2atmpS1638;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS176
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1639);
                      _M0L6_2atmpS1638 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1638 + 1;
                      if (_M0L10next__charS176 < 58) {
                        if (_M0L10next__charS176 < 48) {
                          goto join_175;
                        } else {
                          _M0L12dispatch__15S156 = 5;
                          goto loop__label__15_159;
                        }
                      } else if (_M0L10next__charS176 > 58) {
                        goto join_175;
                      } else {
                        _M0L12dispatch__15S156 = 3;
                        goto loop__label__15_159;
                      }
                      join_175:;
                      _M0L12dispatch__15S156 = 0;
                      goto loop__label__15_159;
                    } else {
                      goto join_161;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1640;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0Lm6tag__2S145 = _M0Lm9_2acursorS132;
                    _M0Lm6tag__3S144 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1640 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1640 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1642 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS178;
                      int32_t _M0L6_2atmpS1641;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS178
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1642);
                      _M0L6_2atmpS1641 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1641 + 1;
                      if (_M0L10next__charS178 < 48) {
                        if (_M0L10next__charS178 == 45) {
                          goto join_157;
                        } else {
                          goto join_177;
                        }
                      } else if (_M0L10next__charS178 > 57) {
                        if (_M0L10next__charS178 < 59) {
                          _M0L12dispatch__15S156 = 3;
                          goto loop__label__15_159;
                        } else {
                          goto join_177;
                        }
                      } else {
                        _M0L12dispatch__15S156 = 6;
                        goto loop__label__15_159;
                      }
                      join_177:;
                      _M0L12dispatch__15S156 = 0;
                      goto loop__label__15_159;
                    } else {
                      goto join_148;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1643;
                    _M0Lm9tag__1__1S142 = _M0Lm6tag__1S141;
                    _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                    _M0L6_2atmpS1643 = _M0Lm9_2acursorS132;
                    if (_M0L6_2atmpS1643 < _M0L6_2aendS131) {
                      int32_t _M0L6_2atmpS1645 = _M0Lm9_2acursorS132;
                      int32_t _M0L10next__charS180;
                      int32_t _M0L6_2atmpS1644;
                      moonbit_incref(_M0L7_2adataS129);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS180
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1645);
                      _M0L6_2atmpS1644 = _M0Lm9_2acursorS132;
                      _M0Lm9_2acursorS132 = _M0L6_2atmpS1644 + 1;
                      if (_M0L10next__charS180 < 58) {
                        if (_M0L10next__charS180 < 48) {
                          goto join_179;
                        } else {
                          _M0L12dispatch__15S156 = 2;
                          goto loop__label__15_159;
                        }
                      } else if (_M0L10next__charS180 > 58) {
                        goto join_179;
                      } else {
                        _M0L12dispatch__15S156 = 1;
                        goto loop__label__15_159;
                      }
                      join_179:;
                      _M0L12dispatch__15S156 = 0;
                      goto loop__label__15_159;
                    } else {
                      goto join_148;
                    }
                    break;
                  }
                  default: {
                    goto join_148;
                    break;
                  }
                }
                join_161:;
                _M0Lm6tag__1S141 = _M0Lm9tag__1__2S143;
                _M0Lm6tag__2S145 = _M0Lm9tag__2__1S146;
                _M0Lm20match__tag__saver__0S135 = _M0Lm6tag__0S140;
                _M0Lm20match__tag__saver__1S136 = _M0Lm6tag__1S141;
                _M0Lm20match__tag__saver__2S137 = _M0Lm6tag__2S145;
                _M0Lm20match__tag__saver__3S138 = _M0Lm6tag__3S144;
                _M0Lm20match__tag__saver__4S139 = _M0Lm6tag__4S147;
                _M0Lm13accept__stateS133 = 0;
                _M0Lm10match__endS134 = _M0Lm9_2acursorS132;
                goto join_148;
                join_157:;
                _M0Lm9tag__1__1S142 = _M0Lm9tag__1__2S143;
                _M0Lm6tag__1S141 = _M0Lm9_2acursorS132;
                _M0Lm6tag__2S145 = _M0Lm9tag__2__1S146;
                _M0L6_2atmpS1613 = _M0Lm9_2acursorS132;
                if (_M0L6_2atmpS1613 < _M0L6_2aendS131) {
                  int32_t _M0L6_2atmpS1615 = _M0Lm9_2acursorS132;
                  int32_t _M0L10next__charS160;
                  int32_t _M0L6_2atmpS1614;
                  moonbit_incref(_M0L7_2adataS129);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS160
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS129, _M0L6_2atmpS1615);
                  _M0L6_2atmpS1614 = _M0Lm9_2acursorS132;
                  _M0Lm9_2acursorS132 = _M0L6_2atmpS1614 + 1;
                  if (_M0L10next__charS160 < 58) {
                    if (_M0L10next__charS160 < 48) {
                      goto join_158;
                    } else {
                      _M0L12dispatch__15S156 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS160 > 58) {
                    goto join_158;
                  } else {
                    _M0L12dispatch__15S156 = 1;
                    continue;
                  }
                  join_158:;
                  _M0L12dispatch__15S156 = 0;
                  continue;
                } else {
                  goto join_148;
                }
                break;
              }
            } else {
              goto join_148;
            }
          } else {
            continue;
          }
        } else {
          goto join_148;
        }
        break;
      }
    } else {
      goto join_148;
    }
  } else {
    goto join_148;
  }
  join_148:;
  switch (_M0Lm13accept__stateS133) {
    case 0: {
      int32_t _M0L6_2atmpS1604 = _M0Lm20match__tag__saver__1S136;
      int32_t _M0L6_2atmpS1603 = _M0L6_2atmpS1604 + 1;
      int64_t _M0L6_2atmpS1600 = (int64_t)_M0L6_2atmpS1603;
      int32_t _M0L6_2atmpS1602 = _M0Lm20match__tag__saver__2S137;
      int64_t _M0L6_2atmpS1601 = (int64_t)_M0L6_2atmpS1602;
      struct _M0TPC16string10StringView _M0L11start__lineS149;
      int32_t _M0L6_2atmpS1599;
      int32_t _M0L6_2atmpS1598;
      int64_t _M0L6_2atmpS1595;
      int32_t _M0L6_2atmpS1597;
      int64_t _M0L6_2atmpS1596;
      struct _M0TPC16string10StringView _M0L13start__columnS150;
      int32_t _M0L6_2atmpS1594;
      int64_t _M0L6_2atmpS1591;
      int32_t _M0L6_2atmpS1593;
      int64_t _M0L6_2atmpS1592;
      struct _M0TPC16string10StringView _M0L3pkgS151;
      int32_t _M0L6_2atmpS1590;
      int32_t _M0L6_2atmpS1589;
      int64_t _M0L6_2atmpS1586;
      int32_t _M0L6_2atmpS1588;
      int64_t _M0L6_2atmpS1587;
      struct _M0TPC16string10StringView _M0L8filenameS152;
      int32_t _M0L6_2atmpS1585;
      int32_t _M0L6_2atmpS1584;
      int64_t _M0L6_2atmpS1581;
      int32_t _M0L6_2atmpS1583;
      int64_t _M0L6_2atmpS1582;
      struct _M0TPC16string10StringView _M0L9end__lineS153;
      int32_t _M0L6_2atmpS1580;
      int32_t _M0L6_2atmpS1579;
      int64_t _M0L6_2atmpS1576;
      int32_t _M0L6_2atmpS1578;
      int64_t _M0L6_2atmpS1577;
      struct _M0TPC16string10StringView _M0L11end__columnS154;
      struct _M0TPB13SourceLocRepr* _block_3814;
      moonbit_incref(_M0L7_2adataS129);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS149
      = _M0MPC16string6String4view(_M0L7_2adataS129, _M0L6_2atmpS1600, _M0L6_2atmpS1601);
      _M0L6_2atmpS1599 = _M0Lm20match__tag__saver__2S137;
      _M0L6_2atmpS1598 = _M0L6_2atmpS1599 + 1;
      _M0L6_2atmpS1595 = (int64_t)_M0L6_2atmpS1598;
      _M0L6_2atmpS1597 = _M0Lm20match__tag__saver__3S138;
      _M0L6_2atmpS1596 = (int64_t)_M0L6_2atmpS1597;
      moonbit_incref(_M0L7_2adataS129);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS150
      = _M0MPC16string6String4view(_M0L7_2adataS129, _M0L6_2atmpS1595, _M0L6_2atmpS1596);
      _M0L6_2atmpS1594 = _M0L8_2astartS130 + 1;
      _M0L6_2atmpS1591 = (int64_t)_M0L6_2atmpS1594;
      _M0L6_2atmpS1593 = _M0Lm20match__tag__saver__0S135;
      _M0L6_2atmpS1592 = (int64_t)_M0L6_2atmpS1593;
      moonbit_incref(_M0L7_2adataS129);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS151
      = _M0MPC16string6String4view(_M0L7_2adataS129, _M0L6_2atmpS1591, _M0L6_2atmpS1592);
      _M0L6_2atmpS1590 = _M0Lm20match__tag__saver__0S135;
      _M0L6_2atmpS1589 = _M0L6_2atmpS1590 + 1;
      _M0L6_2atmpS1586 = (int64_t)_M0L6_2atmpS1589;
      _M0L6_2atmpS1588 = _M0Lm20match__tag__saver__1S136;
      _M0L6_2atmpS1587 = (int64_t)_M0L6_2atmpS1588;
      moonbit_incref(_M0L7_2adataS129);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS152
      = _M0MPC16string6String4view(_M0L7_2adataS129, _M0L6_2atmpS1586, _M0L6_2atmpS1587);
      _M0L6_2atmpS1585 = _M0Lm20match__tag__saver__3S138;
      _M0L6_2atmpS1584 = _M0L6_2atmpS1585 + 1;
      _M0L6_2atmpS1581 = (int64_t)_M0L6_2atmpS1584;
      _M0L6_2atmpS1583 = _M0Lm20match__tag__saver__4S139;
      _M0L6_2atmpS1582 = (int64_t)_M0L6_2atmpS1583;
      moonbit_incref(_M0L7_2adataS129);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS153
      = _M0MPC16string6String4view(_M0L7_2adataS129, _M0L6_2atmpS1581, _M0L6_2atmpS1582);
      _M0L6_2atmpS1580 = _M0Lm20match__tag__saver__4S139;
      _M0L6_2atmpS1579 = _M0L6_2atmpS1580 + 1;
      _M0L6_2atmpS1576 = (int64_t)_M0L6_2atmpS1579;
      _M0L6_2atmpS1578 = _M0Lm10match__endS134;
      _M0L6_2atmpS1577 = (int64_t)_M0L6_2atmpS1578;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS154
      = _M0MPC16string6String4view(_M0L7_2adataS129, _M0L6_2atmpS1576, _M0L6_2atmpS1577);
      _block_3814
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3814)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3814->$0_0 = _M0L3pkgS151.$0;
      _block_3814->$0_1 = _M0L3pkgS151.$1;
      _block_3814->$0_2 = _M0L3pkgS151.$2;
      _block_3814->$1_0 = _M0L8filenameS152.$0;
      _block_3814->$1_1 = _M0L8filenameS152.$1;
      _block_3814->$1_2 = _M0L8filenameS152.$2;
      _block_3814->$2_0 = _M0L11start__lineS149.$0;
      _block_3814->$2_1 = _M0L11start__lineS149.$1;
      _block_3814->$2_2 = _M0L11start__lineS149.$2;
      _block_3814->$3_0 = _M0L13start__columnS150.$0;
      _block_3814->$3_1 = _M0L13start__columnS150.$1;
      _block_3814->$3_2 = _M0L13start__columnS150.$2;
      _block_3814->$4_0 = _M0L9end__lineS153.$0;
      _block_3814->$4_1 = _M0L9end__lineS153.$1;
      _block_3814->$4_2 = _M0L9end__lineS153.$2;
      _block_3814->$5_0 = _M0L11end__columnS154.$0;
      _block_3814->$5_1 = _M0L11end__columnS154.$1;
      _block_3814->$5_2 = _M0L11end__columnS154.$2;
      return _block_3814;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS129);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS125,
  int32_t _M0L5indexS126
) {
  int32_t _M0L3lenS124;
  int32_t _if__result_3815;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS124 = _M0L4selfS125->$1;
  if (_M0L5indexS126 >= 0) {
    _if__result_3815 = _M0L5indexS126 < _M0L3lenS124;
  } else {
    _if__result_3815 = 0;
  }
  if (_if__result_3815) {
    moonbit_string_t* _M0L6_2atmpS1575;
    moonbit_string_t _M0L6_2atmpS3455;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1575 = _M0MPC15array5Array6bufferGsE(_M0L4selfS125);
    if (
      _M0L5indexS126 < 0
      || _M0L5indexS126 >= Moonbit_array_length(_M0L6_2atmpS1575)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3455 = (moonbit_string_t)_M0L6_2atmpS1575[_M0L5indexS126];
    moonbit_incref(_M0L6_2atmpS3455);
    moonbit_decref(_M0L6_2atmpS1575);
    return _M0L6_2atmpS3455;
  } else {
    moonbit_decref(_M0L4selfS125);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS121
) {
  moonbit_string_t* _M0L8_2afieldS3456;
  int32_t _M0L6_2acntS3601;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3456 = _M0L4selfS121->$0;
  _M0L6_2acntS3601 = Moonbit_object_header(_M0L4selfS121)->rc;
  if (_M0L6_2acntS3601 > 1) {
    int32_t _M0L11_2anew__cntS3602 = _M0L6_2acntS3601 - 1;
    Moonbit_object_header(_M0L4selfS121)->rc = _M0L11_2anew__cntS3602;
    moonbit_incref(_M0L8_2afieldS3456);
  } else if (_M0L6_2acntS3601 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS121);
  }
  return _M0L8_2afieldS3456;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS122
) {
  struct _M0TUsiE** _M0L8_2afieldS3457;
  int32_t _M0L6_2acntS3603;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3457 = _M0L4selfS122->$0;
  _M0L6_2acntS3603 = Moonbit_object_header(_M0L4selfS122)->rc;
  if (_M0L6_2acntS3603 > 1) {
    int32_t _M0L11_2anew__cntS3604 = _M0L6_2acntS3603 - 1;
    Moonbit_object_header(_M0L4selfS122)->rc = _M0L11_2anew__cntS3604;
    moonbit_incref(_M0L8_2afieldS3457);
  } else if (_M0L6_2acntS3603 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS122);
  }
  return _M0L8_2afieldS3457;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS123
) {
  void** _M0L8_2afieldS3458;
  int32_t _M0L6_2acntS3605;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3458 = _M0L4selfS123->$0;
  _M0L6_2acntS3605 = Moonbit_object_header(_M0L4selfS123)->rc;
  if (_M0L6_2acntS3605 > 1) {
    int32_t _M0L11_2anew__cntS3606 = _M0L6_2acntS3605 - 1;
    Moonbit_object_header(_M0L4selfS123)->rc = _M0L11_2anew__cntS3606;
    moonbit_incref(_M0L8_2afieldS3458);
  } else if (_M0L6_2acntS3605 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS123);
  }
  return _M0L8_2afieldS3458;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS120) {
  struct _M0TPB13StringBuilder* _M0L3bufS119;
  struct _M0TPB6Logger _M0L6_2atmpS1574;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS119 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS119);
  _M0L6_2atmpS1574
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS119
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS120, _M0L6_2atmpS1574);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS119);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS116,
  int32_t _M0L5indexS117
) {
  int32_t _M0L2c1S115;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S115 = _M0L4selfS116[_M0L5indexS117];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S115)) {
    int32_t _M0L6_2atmpS1573 = _M0L5indexS117 + 1;
    int32_t _M0L6_2atmpS3459 = _M0L4selfS116[_M0L6_2atmpS1573];
    int32_t _M0L2c2S118;
    int32_t _M0L6_2atmpS1571;
    int32_t _M0L6_2atmpS1572;
    moonbit_decref(_M0L4selfS116);
    _M0L2c2S118 = _M0L6_2atmpS3459;
    _M0L6_2atmpS1571 = (int32_t)_M0L2c1S115;
    _M0L6_2atmpS1572 = (int32_t)_M0L2c2S118;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1571, _M0L6_2atmpS1572);
  } else {
    moonbit_decref(_M0L4selfS116);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S115);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS114) {
  int32_t _M0L6_2atmpS1570;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1570 = (int32_t)_M0L4selfS114;
  return _M0L6_2atmpS1570;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS112,
  int32_t _M0L8trailingS113
) {
  int32_t _M0L6_2atmpS1569;
  int32_t _M0L6_2atmpS1568;
  int32_t _M0L6_2atmpS1567;
  int32_t _M0L6_2atmpS1566;
  int32_t _M0L6_2atmpS1565;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1569 = _M0L7leadingS112 - 55296;
  _M0L6_2atmpS1568 = _M0L6_2atmpS1569 * 1024;
  _M0L6_2atmpS1567 = _M0L6_2atmpS1568 + _M0L8trailingS113;
  _M0L6_2atmpS1566 = _M0L6_2atmpS1567 - 56320;
  _M0L6_2atmpS1565 = _M0L6_2atmpS1566 + 65536;
  return _M0L6_2atmpS1565;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS111) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS111 >= 56320) {
    return _M0L4selfS111 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS110) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS110 >= 55296) {
    return _M0L4selfS110 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS107,
  int32_t _M0L2chS109
) {
  int32_t _M0L3lenS1560;
  int32_t _M0L6_2atmpS1559;
  moonbit_bytes_t _M0L8_2afieldS3460;
  moonbit_bytes_t _M0L4dataS1563;
  int32_t _M0L3lenS1564;
  int32_t _M0L3incS108;
  int32_t _M0L3lenS1562;
  int32_t _M0L6_2atmpS1561;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1560 = _M0L4selfS107->$1;
  _M0L6_2atmpS1559 = _M0L3lenS1560 + 4;
  moonbit_incref(_M0L4selfS107);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS107, _M0L6_2atmpS1559);
  _M0L8_2afieldS3460 = _M0L4selfS107->$0;
  _M0L4dataS1563 = _M0L8_2afieldS3460;
  _M0L3lenS1564 = _M0L4selfS107->$1;
  moonbit_incref(_M0L4dataS1563);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS108
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1563, _M0L3lenS1564, _M0L2chS109);
  _M0L3lenS1562 = _M0L4selfS107->$1;
  _M0L6_2atmpS1561 = _M0L3lenS1562 + _M0L3incS108;
  _M0L4selfS107->$1 = _M0L6_2atmpS1561;
  moonbit_decref(_M0L4selfS107);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS102,
  int32_t _M0L8requiredS103
) {
  moonbit_bytes_t _M0L8_2afieldS3464;
  moonbit_bytes_t _M0L4dataS1558;
  int32_t _M0L6_2atmpS3463;
  int32_t _M0L12current__lenS101;
  int32_t _M0Lm13enough__spaceS104;
  int32_t _M0L6_2atmpS1556;
  int32_t _M0L6_2atmpS1557;
  moonbit_bytes_t _M0L9new__dataS106;
  moonbit_bytes_t _M0L8_2afieldS3462;
  moonbit_bytes_t _M0L4dataS1554;
  int32_t _M0L3lenS1555;
  moonbit_bytes_t _M0L6_2aoldS3461;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3464 = _M0L4selfS102->$0;
  _M0L4dataS1558 = _M0L8_2afieldS3464;
  _M0L6_2atmpS3463 = Moonbit_array_length(_M0L4dataS1558);
  _M0L12current__lenS101 = _M0L6_2atmpS3463;
  if (_M0L8requiredS103 <= _M0L12current__lenS101) {
    moonbit_decref(_M0L4selfS102);
    return 0;
  }
  _M0Lm13enough__spaceS104 = _M0L12current__lenS101;
  while (1) {
    int32_t _M0L6_2atmpS1552 = _M0Lm13enough__spaceS104;
    if (_M0L6_2atmpS1552 < _M0L8requiredS103) {
      int32_t _M0L6_2atmpS1553 = _M0Lm13enough__spaceS104;
      _M0Lm13enough__spaceS104 = _M0L6_2atmpS1553 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1556 = _M0Lm13enough__spaceS104;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1557 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS106
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1556, _M0L6_2atmpS1557);
  _M0L8_2afieldS3462 = _M0L4selfS102->$0;
  _M0L4dataS1554 = _M0L8_2afieldS3462;
  _M0L3lenS1555 = _M0L4selfS102->$1;
  moonbit_incref(_M0L4dataS1554);
  moonbit_incref(_M0L9new__dataS106);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS106, 0, _M0L4dataS1554, 0, _M0L3lenS1555);
  _M0L6_2aoldS3461 = _M0L4selfS102->$0;
  moonbit_decref(_M0L6_2aoldS3461);
  _M0L4selfS102->$0 = _M0L9new__dataS106;
  moonbit_decref(_M0L4selfS102);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS96,
  int32_t _M0L6offsetS97,
  int32_t _M0L5valueS95
) {
  uint32_t _M0L4codeS94;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS94 = _M0MPC14char4Char8to__uint(_M0L5valueS95);
  if (_M0L4codeS94 < 65536u) {
    uint32_t _M0L6_2atmpS1535 = _M0L4codeS94 & 255u;
    int32_t _M0L6_2atmpS1534;
    int32_t _M0L6_2atmpS1536;
    uint32_t _M0L6_2atmpS1538;
    int32_t _M0L6_2atmpS1537;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1534 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1535);
    if (
      _M0L6offsetS97 < 0
      || _M0L6offsetS97 >= Moonbit_array_length(_M0L4selfS96)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS96[_M0L6offsetS97] = _M0L6_2atmpS1534;
    _M0L6_2atmpS1536 = _M0L6offsetS97 + 1;
    _M0L6_2atmpS1538 = _M0L4codeS94 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1537 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1538);
    if (
      _M0L6_2atmpS1536 < 0
      || _M0L6_2atmpS1536 >= Moonbit_array_length(_M0L4selfS96)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS96[_M0L6_2atmpS1536] = _M0L6_2atmpS1537;
    moonbit_decref(_M0L4selfS96);
    return 2;
  } else if (_M0L4codeS94 < 1114112u) {
    uint32_t _M0L2hiS98 = _M0L4codeS94 - 65536u;
    uint32_t _M0L6_2atmpS1551 = _M0L2hiS98 >> 10;
    uint32_t _M0L2loS99 = _M0L6_2atmpS1551 | 55296u;
    uint32_t _M0L6_2atmpS1550 = _M0L2hiS98 & 1023u;
    uint32_t _M0L2hiS100 = _M0L6_2atmpS1550 | 56320u;
    uint32_t _M0L6_2atmpS1540 = _M0L2loS99 & 255u;
    int32_t _M0L6_2atmpS1539;
    int32_t _M0L6_2atmpS1541;
    uint32_t _M0L6_2atmpS1543;
    int32_t _M0L6_2atmpS1542;
    int32_t _M0L6_2atmpS1544;
    uint32_t _M0L6_2atmpS1546;
    int32_t _M0L6_2atmpS1545;
    int32_t _M0L6_2atmpS1547;
    uint32_t _M0L6_2atmpS1549;
    int32_t _M0L6_2atmpS1548;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1539 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1540);
    if (
      _M0L6offsetS97 < 0
      || _M0L6offsetS97 >= Moonbit_array_length(_M0L4selfS96)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS96[_M0L6offsetS97] = _M0L6_2atmpS1539;
    _M0L6_2atmpS1541 = _M0L6offsetS97 + 1;
    _M0L6_2atmpS1543 = _M0L2loS99 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1542 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1543);
    if (
      _M0L6_2atmpS1541 < 0
      || _M0L6_2atmpS1541 >= Moonbit_array_length(_M0L4selfS96)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS96[_M0L6_2atmpS1541] = _M0L6_2atmpS1542;
    _M0L6_2atmpS1544 = _M0L6offsetS97 + 2;
    _M0L6_2atmpS1546 = _M0L2hiS100 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1545 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1546);
    if (
      _M0L6_2atmpS1544 < 0
      || _M0L6_2atmpS1544 >= Moonbit_array_length(_M0L4selfS96)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS96[_M0L6_2atmpS1544] = _M0L6_2atmpS1545;
    _M0L6_2atmpS1547 = _M0L6offsetS97 + 3;
    _M0L6_2atmpS1549 = _M0L2hiS100 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1548 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1549);
    if (
      _M0L6_2atmpS1547 < 0
      || _M0L6_2atmpS1547 >= Moonbit_array_length(_M0L4selfS96)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS96[_M0L6_2atmpS1547] = _M0L6_2atmpS1548;
    moonbit_decref(_M0L4selfS96);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS96);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_93.data, (moonbit_string_t)moonbit_string_literal_94.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS93) {
  int32_t _M0L6_2atmpS1533;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1533 = *(int32_t*)&_M0L4selfS93;
  return _M0L6_2atmpS1533 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS92) {
  int32_t _M0L6_2atmpS1532;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1532 = _M0L4selfS92;
  return *(uint32_t*)&_M0L6_2atmpS1532;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS91
) {
  moonbit_bytes_t _M0L8_2afieldS3466;
  moonbit_bytes_t _M0L4dataS1531;
  moonbit_bytes_t _M0L6_2atmpS1528;
  int32_t _M0L8_2afieldS3465;
  int32_t _M0L3lenS1530;
  int64_t _M0L6_2atmpS1529;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3466 = _M0L4selfS91->$0;
  _M0L4dataS1531 = _M0L8_2afieldS3466;
  moonbit_incref(_M0L4dataS1531);
  _M0L6_2atmpS1528 = _M0L4dataS1531;
  _M0L8_2afieldS3465 = _M0L4selfS91->$1;
  moonbit_decref(_M0L4selfS91);
  _M0L3lenS1530 = _M0L8_2afieldS3465;
  _M0L6_2atmpS1529 = (int64_t)_M0L3lenS1530;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1528, 0, _M0L6_2atmpS1529);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS86,
  int32_t _M0L6offsetS90,
  int64_t _M0L6lengthS88
) {
  int32_t _M0L3lenS85;
  int32_t _M0L6lengthS87;
  int32_t _if__result_3817;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS85 = Moonbit_array_length(_M0L4selfS86);
  if (_M0L6lengthS88 == 4294967296ll) {
    _M0L6lengthS87 = _M0L3lenS85 - _M0L6offsetS90;
  } else {
    int64_t _M0L7_2aSomeS89 = _M0L6lengthS88;
    _M0L6lengthS87 = (int32_t)_M0L7_2aSomeS89;
  }
  if (_M0L6offsetS90 >= 0) {
    if (_M0L6lengthS87 >= 0) {
      int32_t _M0L6_2atmpS1527 = _M0L6offsetS90 + _M0L6lengthS87;
      _if__result_3817 = _M0L6_2atmpS1527 <= _M0L3lenS85;
    } else {
      _if__result_3817 = 0;
    }
  } else {
    _if__result_3817 = 0;
  }
  if (_if__result_3817) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS86, _M0L6offsetS90, _M0L6lengthS87);
  } else {
    moonbit_decref(_M0L4selfS86);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS83
) {
  int32_t _M0L7initialS82;
  moonbit_bytes_t _M0L4dataS84;
  struct _M0TPB13StringBuilder* _block_3818;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS83 < 1) {
    _M0L7initialS82 = 1;
  } else {
    _M0L7initialS82 = _M0L10size__hintS83;
  }
  _M0L4dataS84 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS82, 0);
  _block_3818
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3818)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3818->$0 = _M0L4dataS84;
  _block_3818->$1 = 0;
  return _block_3818;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS81) {
  int32_t _M0L6_2atmpS1526;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1526 = (int32_t)_M0L4selfS81;
  return _M0L6_2atmpS1526;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS66,
  int32_t _M0L11dst__offsetS67,
  moonbit_string_t* _M0L3srcS68,
  int32_t _M0L11src__offsetS69,
  int32_t _M0L3lenS70
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS66, _M0L11dst__offsetS67, _M0L3srcS68, _M0L11src__offsetS69, _M0L3lenS70);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS71,
  int32_t _M0L11dst__offsetS72,
  struct _M0TUsiE** _M0L3srcS73,
  int32_t _M0L11src__offsetS74,
  int32_t _M0L3lenS75
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS71, _M0L11dst__offsetS72, _M0L3srcS73, _M0L11src__offsetS74, _M0L3lenS75);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS76,
  int32_t _M0L11dst__offsetS77,
  void** _M0L3srcS78,
  int32_t _M0L11src__offsetS79,
  int32_t _M0L3lenS80
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS76, _M0L11dst__offsetS77, _M0L3srcS78, _M0L11src__offsetS79, _M0L3lenS80);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS30,
  int32_t _M0L11dst__offsetS32,
  moonbit_bytes_t _M0L3srcS31,
  int32_t _M0L11src__offsetS33,
  int32_t _M0L3lenS35
) {
  int32_t _if__result_3819;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS30 == _M0L3srcS31) {
    _if__result_3819 = _M0L11dst__offsetS32 < _M0L11src__offsetS33;
  } else {
    _if__result_3819 = 0;
  }
  if (_if__result_3819) {
    int32_t _M0L1iS34 = 0;
    while (1) {
      if (_M0L1iS34 < _M0L3lenS35) {
        int32_t _M0L6_2atmpS1490 = _M0L11dst__offsetS32 + _M0L1iS34;
        int32_t _M0L6_2atmpS1492 = _M0L11src__offsetS33 + _M0L1iS34;
        int32_t _M0L6_2atmpS1491;
        int32_t _M0L6_2atmpS1493;
        if (
          _M0L6_2atmpS1492 < 0
          || _M0L6_2atmpS1492 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1491 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1492];
        if (
          _M0L6_2atmpS1490 < 0
          || _M0L6_2atmpS1490 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1490] = _M0L6_2atmpS1491;
        _M0L6_2atmpS1493 = _M0L1iS34 + 1;
        _M0L1iS34 = _M0L6_2atmpS1493;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1498 = _M0L3lenS35 - 1;
    int32_t _M0L1iS37 = _M0L6_2atmpS1498;
    while (1) {
      if (_M0L1iS37 >= 0) {
        int32_t _M0L6_2atmpS1494 = _M0L11dst__offsetS32 + _M0L1iS37;
        int32_t _M0L6_2atmpS1496 = _M0L11src__offsetS33 + _M0L1iS37;
        int32_t _M0L6_2atmpS1495;
        int32_t _M0L6_2atmpS1497;
        if (
          _M0L6_2atmpS1496 < 0
          || _M0L6_2atmpS1496 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1495 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1496];
        if (
          _M0L6_2atmpS1494 < 0
          || _M0L6_2atmpS1494 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1494] = _M0L6_2atmpS1495;
        _M0L6_2atmpS1497 = _M0L1iS37 - 1;
        _M0L1iS37 = _M0L6_2atmpS1497;
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
  int32_t _if__result_3822;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS39 == _M0L3srcS40) {
    _if__result_3822 = _M0L11dst__offsetS41 < _M0L11src__offsetS42;
  } else {
    _if__result_3822 = 0;
  }
  if (_if__result_3822) {
    int32_t _M0L1iS43 = 0;
    while (1) {
      if (_M0L1iS43 < _M0L3lenS44) {
        int32_t _M0L6_2atmpS1499 = _M0L11dst__offsetS41 + _M0L1iS43;
        int32_t _M0L6_2atmpS1501 = _M0L11src__offsetS42 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS3468;
        moonbit_string_t _M0L6_2atmpS1500;
        moonbit_string_t _M0L6_2aoldS3467;
        int32_t _M0L6_2atmpS1502;
        if (
          _M0L6_2atmpS1501 < 0
          || _M0L6_2atmpS1501 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3468 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1501];
        _M0L6_2atmpS1500 = _M0L6_2atmpS3468;
        if (
          _M0L6_2atmpS1499 < 0
          || _M0L6_2atmpS1499 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3467 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1499];
        moonbit_incref(_M0L6_2atmpS1500);
        moonbit_decref(_M0L6_2aoldS3467);
        _M0L3dstS39[_M0L6_2atmpS1499] = _M0L6_2atmpS1500;
        _M0L6_2atmpS1502 = _M0L1iS43 + 1;
        _M0L1iS43 = _M0L6_2atmpS1502;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1507 = _M0L3lenS44 - 1;
    int32_t _M0L1iS46 = _M0L6_2atmpS1507;
    while (1) {
      if (_M0L1iS46 >= 0) {
        int32_t _M0L6_2atmpS1503 = _M0L11dst__offsetS41 + _M0L1iS46;
        int32_t _M0L6_2atmpS1505 = _M0L11src__offsetS42 + _M0L1iS46;
        moonbit_string_t _M0L6_2atmpS3470;
        moonbit_string_t _M0L6_2atmpS1504;
        moonbit_string_t _M0L6_2aoldS3469;
        int32_t _M0L6_2atmpS1506;
        if (
          _M0L6_2atmpS1505 < 0
          || _M0L6_2atmpS1505 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3470 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1505];
        _M0L6_2atmpS1504 = _M0L6_2atmpS3470;
        if (
          _M0L6_2atmpS1503 < 0
          || _M0L6_2atmpS1503 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3469 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1503];
        moonbit_incref(_M0L6_2atmpS1504);
        moonbit_decref(_M0L6_2aoldS3469);
        _M0L3dstS39[_M0L6_2atmpS1503] = _M0L6_2atmpS1504;
        _M0L6_2atmpS1506 = _M0L1iS46 - 1;
        _M0L1iS46 = _M0L6_2atmpS1506;
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
  int32_t _if__result_3825;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS48 == _M0L3srcS49) {
    _if__result_3825 = _M0L11dst__offsetS50 < _M0L11src__offsetS51;
  } else {
    _if__result_3825 = 0;
  }
  if (_if__result_3825) {
    int32_t _M0L1iS52 = 0;
    while (1) {
      if (_M0L1iS52 < _M0L3lenS53) {
        int32_t _M0L6_2atmpS1508 = _M0L11dst__offsetS50 + _M0L1iS52;
        int32_t _M0L6_2atmpS1510 = _M0L11src__offsetS51 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS3472;
        struct _M0TUsiE* _M0L6_2atmpS1509;
        struct _M0TUsiE* _M0L6_2aoldS3471;
        int32_t _M0L6_2atmpS1511;
        if (
          _M0L6_2atmpS1510 < 0
          || _M0L6_2atmpS1510 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3472 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1510];
        _M0L6_2atmpS1509 = _M0L6_2atmpS3472;
        if (
          _M0L6_2atmpS1508 < 0
          || _M0L6_2atmpS1508 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3471 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1508];
        if (_M0L6_2atmpS1509) {
          moonbit_incref(_M0L6_2atmpS1509);
        }
        if (_M0L6_2aoldS3471) {
          moonbit_decref(_M0L6_2aoldS3471);
        }
        _M0L3dstS48[_M0L6_2atmpS1508] = _M0L6_2atmpS1509;
        _M0L6_2atmpS1511 = _M0L1iS52 + 1;
        _M0L1iS52 = _M0L6_2atmpS1511;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1516 = _M0L3lenS53 - 1;
    int32_t _M0L1iS55 = _M0L6_2atmpS1516;
    while (1) {
      if (_M0L1iS55 >= 0) {
        int32_t _M0L6_2atmpS1512 = _M0L11dst__offsetS50 + _M0L1iS55;
        int32_t _M0L6_2atmpS1514 = _M0L11src__offsetS51 + _M0L1iS55;
        struct _M0TUsiE* _M0L6_2atmpS3474;
        struct _M0TUsiE* _M0L6_2atmpS1513;
        struct _M0TUsiE* _M0L6_2aoldS3473;
        int32_t _M0L6_2atmpS1515;
        if (
          _M0L6_2atmpS1514 < 0
          || _M0L6_2atmpS1514 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3474 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1514];
        _M0L6_2atmpS1513 = _M0L6_2atmpS3474;
        if (
          _M0L6_2atmpS1512 < 0
          || _M0L6_2atmpS1512 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3473 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1512];
        if (_M0L6_2atmpS1513) {
          moonbit_incref(_M0L6_2atmpS1513);
        }
        if (_M0L6_2aoldS3473) {
          moonbit_decref(_M0L6_2aoldS3473);
        }
        _M0L3dstS48[_M0L6_2atmpS1512] = _M0L6_2atmpS1513;
        _M0L6_2atmpS1515 = _M0L1iS55 - 1;
        _M0L1iS55 = _M0L6_2atmpS1515;
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
  int32_t _if__result_3828;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS57 == _M0L3srcS58) {
    _if__result_3828 = _M0L11dst__offsetS59 < _M0L11src__offsetS60;
  } else {
    _if__result_3828 = 0;
  }
  if (_if__result_3828) {
    int32_t _M0L1iS61 = 0;
    while (1) {
      if (_M0L1iS61 < _M0L3lenS62) {
        int32_t _M0L6_2atmpS1517 = _M0L11dst__offsetS59 + _M0L1iS61;
        int32_t _M0L6_2atmpS1519 = _M0L11src__offsetS60 + _M0L1iS61;
        void* _M0L6_2atmpS3476;
        void* _M0L6_2atmpS1518;
        void* _M0L6_2aoldS3475;
        int32_t _M0L6_2atmpS1520;
        if (
          _M0L6_2atmpS1519 < 0
          || _M0L6_2atmpS1519 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3476 = (void*)_M0L3srcS58[_M0L6_2atmpS1519];
        _M0L6_2atmpS1518 = _M0L6_2atmpS3476;
        if (
          _M0L6_2atmpS1517 < 0
          || _M0L6_2atmpS1517 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3475 = (void*)_M0L3dstS57[_M0L6_2atmpS1517];
        moonbit_incref(_M0L6_2atmpS1518);
        moonbit_decref(_M0L6_2aoldS3475);
        _M0L3dstS57[_M0L6_2atmpS1517] = _M0L6_2atmpS1518;
        _M0L6_2atmpS1520 = _M0L1iS61 + 1;
        _M0L1iS61 = _M0L6_2atmpS1520;
        continue;
      } else {
        moonbit_decref(_M0L3srcS58);
        moonbit_decref(_M0L3dstS57);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1525 = _M0L3lenS62 - 1;
    int32_t _M0L1iS64 = _M0L6_2atmpS1525;
    while (1) {
      if (_M0L1iS64 >= 0) {
        int32_t _M0L6_2atmpS1521 = _M0L11dst__offsetS59 + _M0L1iS64;
        int32_t _M0L6_2atmpS1523 = _M0L11src__offsetS60 + _M0L1iS64;
        void* _M0L6_2atmpS3478;
        void* _M0L6_2atmpS1522;
        void* _M0L6_2aoldS3477;
        int32_t _M0L6_2atmpS1524;
        if (
          _M0L6_2atmpS1523 < 0
          || _M0L6_2atmpS1523 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3478 = (void*)_M0L3srcS58[_M0L6_2atmpS1523];
        _M0L6_2atmpS1522 = _M0L6_2atmpS3478;
        if (
          _M0L6_2atmpS1521 < 0
          || _M0L6_2atmpS1521 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3477 = (void*)_M0L3dstS57[_M0L6_2atmpS1521];
        moonbit_incref(_M0L6_2atmpS1522);
        moonbit_decref(_M0L6_2aoldS3477);
        _M0L3dstS57[_M0L6_2atmpS1521] = _M0L6_2atmpS1522;
        _M0L6_2atmpS1524 = _M0L1iS64 - 1;
        _M0L1iS64 = _M0L6_2atmpS1524;
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

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1474;
  moonbit_string_t _M0L6_2atmpS3481;
  moonbit_string_t _M0L6_2atmpS1472;
  moonbit_string_t _M0L6_2atmpS1473;
  moonbit_string_t _M0L6_2atmpS3480;
  moonbit_string_t _M0L6_2atmpS1471;
  moonbit_string_t _M0L6_2atmpS3479;
  moonbit_string_t _M0L6_2atmpS1470;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1474 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3481
  = moonbit_add_string(_M0L6_2atmpS1474, (moonbit_string_t)moonbit_string_literal_95.data);
  moonbit_decref(_M0L6_2atmpS1474);
  _M0L6_2atmpS1472 = _M0L6_2atmpS3481;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1473
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3480 = moonbit_add_string(_M0L6_2atmpS1472, _M0L6_2atmpS1473);
  moonbit_decref(_M0L6_2atmpS1472);
  moonbit_decref(_M0L6_2atmpS1473);
  _M0L6_2atmpS1471 = _M0L6_2atmpS3480;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3479
  = moonbit_add_string(_M0L6_2atmpS1471, (moonbit_string_t)moonbit_string_literal_44.data);
  moonbit_decref(_M0L6_2atmpS1471);
  _M0L6_2atmpS1470 = _M0L6_2atmpS3479;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1470);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1479;
  moonbit_string_t _M0L6_2atmpS3484;
  moonbit_string_t _M0L6_2atmpS1477;
  moonbit_string_t _M0L6_2atmpS1478;
  moonbit_string_t _M0L6_2atmpS3483;
  moonbit_string_t _M0L6_2atmpS1476;
  moonbit_string_t _M0L6_2atmpS3482;
  moonbit_string_t _M0L6_2atmpS1475;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1479 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3484
  = moonbit_add_string(_M0L6_2atmpS1479, (moonbit_string_t)moonbit_string_literal_95.data);
  moonbit_decref(_M0L6_2atmpS1479);
  _M0L6_2atmpS1477 = _M0L6_2atmpS3484;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1478
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3483 = moonbit_add_string(_M0L6_2atmpS1477, _M0L6_2atmpS1478);
  moonbit_decref(_M0L6_2atmpS1477);
  moonbit_decref(_M0L6_2atmpS1478);
  _M0L6_2atmpS1476 = _M0L6_2atmpS3483;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3482
  = moonbit_add_string(_M0L6_2atmpS1476, (moonbit_string_t)moonbit_string_literal_44.data);
  moonbit_decref(_M0L6_2atmpS1476);
  _M0L6_2atmpS1475 = _M0L6_2atmpS3482;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1475);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS26,
  moonbit_string_t _M0L3locS27
) {
  moonbit_string_t _M0L6_2atmpS1484;
  moonbit_string_t _M0L6_2atmpS3487;
  moonbit_string_t _M0L6_2atmpS1482;
  moonbit_string_t _M0L6_2atmpS1483;
  moonbit_string_t _M0L6_2atmpS3486;
  moonbit_string_t _M0L6_2atmpS1481;
  moonbit_string_t _M0L6_2atmpS3485;
  moonbit_string_t _M0L6_2atmpS1480;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1484 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3487
  = moonbit_add_string(_M0L6_2atmpS1484, (moonbit_string_t)moonbit_string_literal_95.data);
  moonbit_decref(_M0L6_2atmpS1484);
  _M0L6_2atmpS1482 = _M0L6_2atmpS3487;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1483
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS27);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3486 = moonbit_add_string(_M0L6_2atmpS1482, _M0L6_2atmpS1483);
  moonbit_decref(_M0L6_2atmpS1482);
  moonbit_decref(_M0L6_2atmpS1483);
  _M0L6_2atmpS1481 = _M0L6_2atmpS3486;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3485
  = moonbit_add_string(_M0L6_2atmpS1481, (moonbit_string_t)moonbit_string_literal_44.data);
  moonbit_decref(_M0L6_2atmpS1481);
  _M0L6_2atmpS1480 = _M0L6_2atmpS3485;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1480);
}

int64_t _M0FPB5abortGOiE(
  moonbit_string_t _M0L6stringS28,
  moonbit_string_t _M0L3locS29
) {
  moonbit_string_t _M0L6_2atmpS1489;
  moonbit_string_t _M0L6_2atmpS3490;
  moonbit_string_t _M0L6_2atmpS1487;
  moonbit_string_t _M0L6_2atmpS1488;
  moonbit_string_t _M0L6_2atmpS3489;
  moonbit_string_t _M0L6_2atmpS1486;
  moonbit_string_t _M0L6_2atmpS3488;
  moonbit_string_t _M0L6_2atmpS1485;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1489 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS28);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3490
  = moonbit_add_string(_M0L6_2atmpS1489, (moonbit_string_t)moonbit_string_literal_95.data);
  moonbit_decref(_M0L6_2atmpS1489);
  _M0L6_2atmpS1487 = _M0L6_2atmpS3490;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1488
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS29);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3489 = moonbit_add_string(_M0L6_2atmpS1487, _M0L6_2atmpS1488);
  moonbit_decref(_M0L6_2atmpS1487);
  moonbit_decref(_M0L6_2atmpS1488);
  _M0L6_2atmpS1486 = _M0L6_2atmpS3489;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3488
  = moonbit_add_string(_M0L6_2atmpS1486, (moonbit_string_t)moonbit_string_literal_44.data);
  moonbit_decref(_M0L6_2atmpS1486);
  _M0L6_2atmpS1485 = _M0L6_2atmpS3488;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGOiE(_M0L6_2atmpS1485);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS20,
  uint32_t _M0L5valueS21
) {
  uint32_t _M0L3accS1469;
  uint32_t _M0L6_2atmpS1468;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1469 = _M0L4selfS20->$0;
  _M0L6_2atmpS1468 = _M0L3accS1469 + 4u;
  _M0L4selfS20->$0 = _M0L6_2atmpS1468;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS20, _M0L5valueS21);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS18,
  uint32_t _M0L5inputS19
) {
  uint32_t _M0L3accS1466;
  uint32_t _M0L6_2atmpS1467;
  uint32_t _M0L6_2atmpS1465;
  uint32_t _M0L6_2atmpS1464;
  uint32_t _M0L6_2atmpS1463;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1466 = _M0L4selfS18->$0;
  _M0L6_2atmpS1467 = _M0L5inputS19 * 3266489917u;
  _M0L6_2atmpS1465 = _M0L3accS1466 + _M0L6_2atmpS1467;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1464 = _M0FPB4rotl(_M0L6_2atmpS1465, 17);
  _M0L6_2atmpS1463 = _M0L6_2atmpS1464 * 668265263u;
  _M0L4selfS18->$0 = _M0L6_2atmpS1463;
  moonbit_decref(_M0L4selfS18);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS16, int32_t _M0L1rS17) {
  uint32_t _M0L6_2atmpS1460;
  int32_t _M0L6_2atmpS1462;
  uint32_t _M0L6_2atmpS1461;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1460 = _M0L1xS16 << (_M0L1rS17 & 31);
  _M0L6_2atmpS1462 = 32 - _M0L1rS17;
  _M0L6_2atmpS1461 = _M0L1xS16 >> (_M0L6_2atmpS1462 & 31);
  return _M0L6_2atmpS1460 | _M0L6_2atmpS1461;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S12,
  struct _M0TPB6Logger _M0L10_2ax__4934S15
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS13;
  moonbit_string_t _M0L8_2afieldS3491;
  int32_t _M0L6_2acntS3607;
  moonbit_string_t _M0L15_2a_2aarg__4935S14;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS13
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S12;
  _M0L8_2afieldS3491 = _M0L10_2aFailureS13->$0;
  _M0L6_2acntS3607 = Moonbit_object_header(_M0L10_2aFailureS13)->rc;
  if (_M0L6_2acntS3607 > 1) {
    int32_t _M0L11_2anew__cntS3608 = _M0L6_2acntS3607 - 1;
    Moonbit_object_header(_M0L10_2aFailureS13)->rc = _M0L11_2anew__cntS3608;
    moonbit_incref(_M0L8_2afieldS3491);
  } else if (_M0L6_2acntS3607 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS13);
  }
  _M0L15_2a_2aarg__4935S14 = _M0L8_2afieldS3491;
  if (_M0L10_2ax__4934S15.$1) {
    moonbit_incref(_M0L10_2ax__4934S15.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S15.$0->$method_0(_M0L10_2ax__4934S15.$1, (moonbit_string_t)moonbit_string_literal_96.data);
  if (_M0L10_2ax__4934S15.$1) {
    moonbit_incref(_M0L10_2ax__4934S15.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S15, _M0L15_2a_2aarg__4935S14);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S15.$0->$method_0(_M0L10_2ax__4934S15.$1, (moonbit_string_t)moonbit_string_literal_66.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS11) {
  void* _block_3831;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_3831 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_3831)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_3831)->$0 = _M0L4selfS11;
  return _block_3831;
}

int32_t _M0MPB6Logger13write__objectGRPC14json8JsonPathE(
  struct _M0TPB6Logger _M0L4selfS6,
  void* _M0L3objS5
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14json8JsonPathPB4Show6output(_M0L3objS5, _M0L4selfS6);
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

int32_t _M0MPB6Logger13write__objectGiE(
  struct _M0TPB6Logger _M0L4selfS10,
  int32_t _M0L3objS9
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L3objS9, _M0L4selfS10);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1422) {
  switch (Moonbit_object_tag(_M0L4_2aeS1422)) {
    case 4: {
      moonbit_decref(_M0L4_2aeS1422);
      return (moonbit_string_t)moonbit_string_literal_97.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1422);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1422);
      return (moonbit_string_t)moonbit_string_literal_98.data;
      break;
    }
    
    case 1: {
      return _M0IP016_24default__implPB4Show10to__stringGRPC14json15JsonDecodeErrorE(_M0L4_2aeS1422);
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1422);
      return (moonbit_string_t)moonbit_string_literal_99.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1422);
      return (moonbit_string_t)moonbit_string_literal_100.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1441,
  int32_t _M0L8_2aparamS1440
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1439 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1441;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1439, _M0L8_2aparamS1440);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1438,
  struct _M0TPC16string10StringView _M0L8_2aparamS1437
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1436 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1438;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1436, _M0L8_2aparamS1437);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1435,
  moonbit_string_t _M0L8_2aparamS1432,
  int32_t _M0L8_2aparamS1433,
  int32_t _M0L8_2aparamS1434
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1431 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1435;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1431, _M0L8_2aparamS1432, _M0L8_2aparamS1433, _M0L8_2aparamS1434);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1430,
  moonbit_string_t _M0L8_2aparamS1429
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1428 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1430;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1428, _M0L8_2aparamS1429);
  return 0;
}

void* _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1427
) {
  struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput* _M0L7_2aselfS1426 =
    (struct _M0TP48clawteam8clawteam5tools10read__file13ReadFileInput*)_M0L11_2aobj__ptrS1427;
  return _M0IP48clawteam8clawteam5tools10read__file13ReadFileInputPB6ToJson8to__json(_M0L7_2aselfS1426);
}

void moonbit_init() {
  int64_t _tmp_3832 = 9218868437227405312ll;
  int64_t _tmp_3833;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1348;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1459;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1458;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1457;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1449;
  moonbit_string_t* _M0L6_2atmpS1456;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1455;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1454;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1349;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1453;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1452;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1451;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1450;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1347;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1448;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1447;
  _M0FPC16double8infinity = *(double*)&_tmp_3832;
  _tmp_3833 = -4503599627370496ll;
  _M0FPC16double13neg__infinity = *(double*)&_tmp_3833;
  _M0L7_2abindS1348
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1459 = _M0L7_2abindS1348;
  _M0L6_2atmpS1458
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1459
  };
  #line 398 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1457
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1458);
  _M0L8_2atupleS1449
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1449)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1449->$0 = (moonbit_string_t)moonbit_string_literal_101.data;
  _M0L8_2atupleS1449->$1 = _M0L6_2atmpS1457;
  _M0L6_2atmpS1456 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1456[0] = (moonbit_string_t)moonbit_string_literal_102.data;
  moonbit_incref(_M0FP48clawteam8clawteam5tools10read__file45____test__726561645f66696c652e6d6274__0_2eclo);
  _M0L8_2atupleS1455
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1455)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1455->$0
  = _M0FP48clawteam8clawteam5tools10read__file45____test__726561645f66696c652e6d6274__0_2eclo;
  _M0L8_2atupleS1455->$1 = _M0L6_2atmpS1456;
  _M0L8_2atupleS1454
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1454)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1454->$0 = 0;
  _M0L8_2atupleS1454->$1 = _M0L8_2atupleS1455;
  _M0L7_2abindS1349
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1349[0] = _M0L8_2atupleS1454;
  _M0L6_2atmpS1453 = _M0L7_2abindS1349;
  _M0L6_2atmpS1452
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1453
  };
  #line 400 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1451
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1452);
  _M0L8_2atupleS1450
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1450)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1450->$0 = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L8_2atupleS1450->$1 = _M0L6_2atmpS1451;
  _M0L7_2abindS1347
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1347[0] = _M0L8_2atupleS1449;
  _M0L7_2abindS1347[1] = _M0L8_2atupleS1450;
  _M0L6_2atmpS1448 = _M0L7_2abindS1347;
  _M0L6_2atmpS1447
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1448
  };
  #line 397 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5tools10read__file48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1447);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1446;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1416;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1417;
  int32_t _M0L7_2abindS1418;
  int32_t _M0L2__S1419;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1446
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1416
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1416)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1416->$0 = _M0L6_2atmpS1446;
  _M0L12async__testsS1416->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1417
  = _M0FP48clawteam8clawteam5tools10read__file52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1418 = _M0L7_2abindS1417->$1;
  _M0L2__S1419 = 0;
  while (1) {
    if (_M0L2__S1419 < _M0L7_2abindS1418) {
      struct _M0TUsiE** _M0L8_2afieldS3495 = _M0L7_2abindS1417->$0;
      struct _M0TUsiE** _M0L3bufS1445 = _M0L8_2afieldS3495;
      struct _M0TUsiE* _M0L6_2atmpS3494 =
        (struct _M0TUsiE*)_M0L3bufS1445[_M0L2__S1419];
      struct _M0TUsiE* _M0L3argS1420 = _M0L6_2atmpS3494;
      moonbit_string_t _M0L8_2afieldS3493 = _M0L3argS1420->$0;
      moonbit_string_t _M0L6_2atmpS1442 = _M0L8_2afieldS3493;
      int32_t _M0L8_2afieldS3492 = _M0L3argS1420->$1;
      int32_t _M0L6_2atmpS1443 = _M0L8_2afieldS3492;
      int32_t _M0L6_2atmpS1444;
      moonbit_incref(_M0L6_2atmpS1442);
      moonbit_incref(_M0L12async__testsS1416);
      #line 441 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam5tools10read__file44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1416, _M0L6_2atmpS1442, _M0L6_2atmpS1443);
      _M0L6_2atmpS1444 = _M0L2__S1419 + 1;
      _M0L2__S1419 = _M0L6_2atmpS1444;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1417);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\tools\\read_file\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam5tools10read__file28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5tools10read__file34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1416);
  return 0;
}