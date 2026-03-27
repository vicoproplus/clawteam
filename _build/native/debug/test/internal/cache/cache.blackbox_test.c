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

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0R38String_3a_3aiter_2eanon__u2300__l247__;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21cache__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__;

struct _M0TPB6Logger;

struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB5ArrayGiE;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TUisE;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE;

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0TWEOs;

struct _M0TPC13ref3RefGORP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParamE;

struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam;

struct _M0TPB6Hasher;

struct _M0KTPB6ToJsonTPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant;

struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam;

struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597;

struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json;

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System;

struct _M0DTPB4Json6Object;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21cache__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam;

struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0R38String_3a_3aiter_2eanon__u2300__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21cache__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGiE {
  int32_t $1;
  int32_t* $0;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool {
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPB4Json6Number {
  double $0;
  moonbit_string_t $1;
  
};

struct _M0TUisE {
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE {
  int32_t $1;
  void** $0;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*,
    struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
  );
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0TPC13ref3RefGORP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParamE {
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0KTPB6ToJsonTPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
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

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE {
  int32_t $1;
  int32_t $2;
  void** $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE {
  int32_t $1;
  int32_t $2;
  void** $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
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

struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text {
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* $0;
  
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

struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall {
  moonbit_string_t $0;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* $1;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE {
  int32_t $1;
  void** $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*,
    void*
  );
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant {
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* $2;
  
};

struct _M0DTPC14json10WriteFrame5Array {
  int32_t $1;
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0BTPB6ToJson {
  void*(* $method_0)(void*);
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** $0;
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User {
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* $0;
  
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

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  
};

struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json*,
    void*
  );
  
};

struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System {
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* $0;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal21cache__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* $0;
  moonbit_string_t $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam {
  int64_t $1;
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*,
  void*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1606(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1597(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testC3923l433(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testC3919l434(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1530(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1525(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1512(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal21cache__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__6(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__5(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__4(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__0(
  
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0FP48clawteam8clawteam8internal5cache15cache__messages(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE
);

void* _M0FP48clawteam8clawteam8internal5cache14cache__message(void*);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(
  void*
);

void* _M0IP48clawteam8clawteam8internal6openai12CacheControlPB6ToJson8to__json(
  int32_t
);

void* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__json(
  void*
);

int32_t _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1279(
  int32_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TPB3MapGsRPB4JsonE*
);

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

void* _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction*
);

void* _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(
  moonbit_string_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai18assistant__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGsE(
  moonbit_string_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai13tool__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai13user__messageGsE(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai13user__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai15system__messageGsE(
  moonbit_string_t,
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai15system__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  moonbit_string_t
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IPC15array5ArrayP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__contentGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(
  void*
);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(
  moonbit_string_t
);

void* _M0FP48clawteam8clawteam8internal6openai19text__content__part(
  moonbit_string_t,
  int64_t
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

int32_t _M0MPC15array5Array9is__emptyGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

int32_t _M0MPC15array5Array6appendGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE
);

int32_t _M0MPC15array9ArrayView16blit__to_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  int32_t
);

int32_t _M0MPC15array5Array24unsafe__grow__to__lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  int32_t
);

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  int32_t,
  int64_t
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

void* _M0IPC15array9ArrayViewPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*
);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2584l591(
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

moonbit_string_t _M0MPC16option6Option10unwrap__orGsE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t
);

void* _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE,
  int32_t
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2319l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2300l247(struct _M0TWEOc*);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*,
  void*
);

int32_t _M0MPC15array5Array4pushGiE(struct _M0TPB5ArrayGiE*, int32_t);

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  void*
);

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  void*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*
);

int32_t _M0MPC15array5Array7reallocGiE(struct _M0TPB5ArrayGiE*);

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

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

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGiE(
  struct _M0TPB5ArrayGiE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
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

int32_t _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE
);

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE
);

int32_t _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE
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

void* _M0MPC15array5Array2atGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*,
  int32_t
);

int32_t _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

void** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*
);

int32_t* _M0MPC15array5Array6bufferGiE(struct _M0TPB5ArrayGiE*);

void** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  void**,
  int32_t,
  void**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGiE(
  int32_t*,
  int32_t,
  int32_t*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  void**,
  int32_t,
  void**,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamEE(
  void**,
  int32_t,
  void**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGiEE(
  int32_t*,
  int32_t,
  int32_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  void**,
  int32_t,
  void**,
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

void* _M0FPB5abortGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPB5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t,
  moonbit_string_t
);

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

void* _M0FPC15abort5abortGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  moonbit_string_t
);

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPC15abort5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    86, 105, 101, 119, 32, 105, 110, 100, 101, 120, 32, 111, 117, 116, 
    32, 111, 102, 32, 98, 111, 117, 110, 100, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 95, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    115, 105, 110, 103, 108, 101, 45, 116, 111, 111, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[77]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 76), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 55, 55, 58, 49, 54, 45, 49, 55, 55, 58, 
    50, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 50, 54, 51, 
    58, 53, 45, 50, 54, 51, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 52, 53, 58, 49, 54, 45, 52, 53, 58, 50, 50, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_67 =
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
} const moonbit_string_literal_120 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 51, 53, 
    58, 53, 45, 49, 51, 55, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 121, 112, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 97, 99, 104, 101, 
    34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    84, 104, 101, 32, 119, 101, 97, 116, 104, 101, 114, 32, 105, 115, 
    32, 115, 117, 110, 110, 121, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_77 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    87, 104, 97, 116, 39, 115, 32, 116, 104, 101, 32, 119, 101, 97, 116, 
    104, 101, 114, 32, 108, 105, 107, 101, 63, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 55, 55, 58, 51, 45, 50, 50, 54, 58, 53, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[73]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 72), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 48, 58, 51, 45, 51, 54, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 57, 52, 58, 51, 45, 49, 48, 53, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_93 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 48, 58, 51, 50, 45, 51, 54, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    101, 112, 104, 101, 109, 101, 114, 97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 101, 120, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 57, 52, 58, 51, 50, 45, 49, 48, 53, 58, 52, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 111, 111, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    115, 105, 110, 103, 108, 101, 45, 117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 48, 58, 49, 54, 45, 49, 48, 58, 50, 50, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    110, 111, 45, 115, 121, 115, 116, 101, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 54, 54, 58, 51, 50, 45, 56, 55, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 54, 54, 58, 49, 54, 45, 54, 54, 58, 50, 50, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 49, 52, 58, 51, 45, 49, 50, 54, 58, 53, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    72, 105, 44, 32, 104, 111, 119, 32, 99, 97, 110, 32, 73, 32, 104, 
    101, 108, 112, 32, 121, 111, 117, 63, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 95, 105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[74]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 73), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 52, 53, 58, 51, 50, 45, 53, 54, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    115, 105, 110, 103, 108, 101, 45, 115, 121, 115, 116, 101, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 115, 115, 105, 115, 116, 97, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    108, 97, 115, 116, 45, 116, 119, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    84, 111, 111, 108, 32, 114, 101, 115, 112, 111, 110, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[115]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 114), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 97, 99, 
    104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 
    107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 32, 110, 101, 101, 100, 32, 116, 111, 32, 99, 97, 108, 108, 32, 
    97, 32, 116, 111, 111, 108, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    115, 121, 115, 116, 101, 109, 45, 115, 105, 110, 103, 108, 101, 45, 
    117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    84, 104, 97, 116, 39, 115, 32, 102, 117, 110, 110, 121, 33, 32, 87, 
    104, 97, 116, 39, 115, 32, 116, 104, 101, 32, 119, 101, 97, 116, 
    104, 101, 114, 32, 108, 105, 107, 101, 44, 32, 98, 121, 32, 116, 
    104, 101, 32, 119, 97, 121, 63, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[77]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 76), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 51, 54, 58, 49, 54, 45, 49, 51, 54, 58, 
    50, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 51, 54, 58, 51, 50, 45, 49, 53, 55, 58, 
    52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    105, 110, 100, 101, 120, 32, 111, 117, 116, 32, 111, 102, 32, 98, 
    111, 117, 110, 100, 115, 58, 32, 116, 104, 101, 32, 108, 101, 110, 
    32, 105, 115, 32, 102, 114, 111, 109, 32, 48, 32, 116, 111, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 49, 52, 58, 51, 50, 45, 49, 50, 54, 58, 
    52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    72, 101, 108, 108, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_68 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    114, 111, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[76]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 75), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 55, 55, 58, 51, 50, 45, 50, 50, 54, 58, 
    52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    99, 97, 99, 104, 101, 95, 99, 111, 110, 116, 114, 111, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    32, 98, 117, 116, 32, 116, 104, 101, 32, 105, 110, 100, 101, 120, 
    32, 105, 115, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_119 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    115, 121, 115, 116, 101, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    89, 111, 117, 32, 97, 114, 101, 32, 97, 108, 115, 111, 32, 97, 32, 
    102, 117, 110, 110, 121, 32, 97, 115, 115, 105, 115, 116, 97, 110, 
    116, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[73]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 72), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 52, 53, 58, 51, 45, 53, 54, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    109, 117, 108, 116, 105, 45, 115, 121, 115, 116, 101, 109, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_101 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 41), 
    83, 117, 114, 101, 33, 32, 87, 104, 121, 32, 100, 105, 100, 32, 116, 
    104, 101, 32, 99, 104, 105, 99, 107, 101, 110, 32, 99, 114, 111, 
    115, 115, 32, 116, 104, 101, 32, 114, 111, 97, 100, 63, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[113]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 112), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 97, 99, 
    104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 
    114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 
    115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    67, 97, 110, 32, 121, 111, 117, 32, 116, 101, 108, 108, 32, 109, 
    101, 32, 97, 32, 106, 111, 107, 101, 63, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[73]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 72), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 54, 54, 58, 51, 45, 56, 55, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_110 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    89, 111, 117, 32, 97, 114, 101, 32, 97, 32, 104, 101, 108, 112, 102, 
    117, 108, 32, 97, 115, 115, 105, 115, 116, 97, 110, 116, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 114, 103, 117, 109, 101, 110, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 51, 54, 58, 51, 45, 49, 53, 55, 58, 53, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    99, 97, 99, 104, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[75]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 74), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 57, 52, 58, 49, 54, 45, 57, 52, 58, 50, 50, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    102, 117, 110, 99, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[77]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 76), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 99, 
    97, 99, 104, 101, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 
    101, 115, 116, 58, 99, 97, 99, 104, 101, 95, 116, 101, 115, 116, 
    46, 109, 98, 116, 58, 49, 49, 52, 58, 49, 54, 45, 49, 49, 52, 58, 
    50, 50, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__3_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1606$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1606
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__1_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__4_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__4_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__5_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__5_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__6_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__6_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson18to__json_2edyncall
  };

struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__6_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__6_2edyncall$closure.data;

struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__5_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__5_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__4_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__4_2edyncall$closure.data;

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall$closure.data;

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
} _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE}
  };

struct _M0BTPB6ToJson* _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1093$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1093 =
  &_M0FPB31ryu__to__string_2erecord_2f1093$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3963
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3962
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__3();
}

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0L6_2aenvS3961,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L4selfS1278
) {
  return _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(_M0L4selfS1278);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3960
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__4();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3959
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__5();
}

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json* _M0L6_2aenvS3958,
  void* _M0L4selfS1342
) {
  return _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(_M0L4selfS1342);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3957
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__6();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3956
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test51____test__63616368655f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3955
) {
  return _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__2();
}

void* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json* _M0L6_2aenvS3954,
  void* _M0L4selfS1322
) {
  return _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__json(_M0L4selfS1322);
}

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1627,
  moonbit_string_t _M0L8filenameS1602,
  int32_t _M0L5indexS1605
) {
  struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597* _closure_4542;
  struct _M0TWssbEu* _M0L14handle__resultS1597;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1606;
  void* _M0L11_2atry__errS1621;
  struct moonbit_result_0 _tmp_4544;
  int32_t _handle__error__result_4545;
  int32_t _M0L6_2atmpS3942;
  void* _M0L3errS1622;
  moonbit_string_t _M0L4nameS1624;
  struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1625;
  moonbit_string_t _M0L8_2afieldS3964;
  int32_t _M0L6_2acntS4366;
  moonbit_string_t _M0L7_2anameS1626;
  #line 532 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1602);
  _closure_4542
  = (struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597*)moonbit_malloc(sizeof(struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597));
  Moonbit_object_header(_closure_4542)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597, $1) >> 2, 1, 0);
  _closure_4542->code
  = &_M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1597;
  _closure_4542->$0 = _M0L5indexS1605;
  _closure_4542->$1 = _M0L8filenameS1602;
  _M0L14handle__resultS1597 = (struct _M0TWssbEu*)_closure_4542;
  _M0L17error__to__stringS1606
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1606$closure.data;
  moonbit_incref(_M0L12async__testsS1627);
  moonbit_incref(_M0L17error__to__stringS1606);
  moonbit_incref(_M0L8filenameS1602);
  moonbit_incref(_M0L14handle__resultS1597);
  #line 566 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4544
  = _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1627, _M0L8filenameS1602, _M0L5indexS1605, _M0L14handle__resultS1597, _M0L17error__to__stringS1606);
  if (_tmp_4544.tag) {
    int32_t const _M0L5_2aokS3951 = _tmp_4544.data.ok;
    _handle__error__result_4545 = _M0L5_2aokS3951;
  } else {
    void* const _M0L6_2aerrS3952 = _tmp_4544.data.err;
    moonbit_decref(_M0L12async__testsS1627);
    moonbit_decref(_M0L17error__to__stringS1606);
    moonbit_decref(_M0L8filenameS1602);
    _M0L11_2atry__errS1621 = _M0L6_2aerrS3952;
    goto join_1620;
  }
  if (_handle__error__result_4545) {
    moonbit_decref(_M0L12async__testsS1627);
    moonbit_decref(_M0L17error__to__stringS1606);
    moonbit_decref(_M0L8filenameS1602);
    _M0L6_2atmpS3942 = 1;
  } else {
    struct moonbit_result_0 _tmp_4546;
    int32_t _handle__error__result_4547;
    moonbit_incref(_M0L12async__testsS1627);
    moonbit_incref(_M0L17error__to__stringS1606);
    moonbit_incref(_M0L8filenameS1602);
    moonbit_incref(_M0L14handle__resultS1597);
    #line 569 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    _tmp_4546
    = _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1627, _M0L8filenameS1602, _M0L5indexS1605, _M0L14handle__resultS1597, _M0L17error__to__stringS1606);
    if (_tmp_4546.tag) {
      int32_t const _M0L5_2aokS3949 = _tmp_4546.data.ok;
      _handle__error__result_4547 = _M0L5_2aokS3949;
    } else {
      void* const _M0L6_2aerrS3950 = _tmp_4546.data.err;
      moonbit_decref(_M0L12async__testsS1627);
      moonbit_decref(_M0L17error__to__stringS1606);
      moonbit_decref(_M0L8filenameS1602);
      _M0L11_2atry__errS1621 = _M0L6_2aerrS3950;
      goto join_1620;
    }
    if (_handle__error__result_4547) {
      moonbit_decref(_M0L12async__testsS1627);
      moonbit_decref(_M0L17error__to__stringS1606);
      moonbit_decref(_M0L8filenameS1602);
      _M0L6_2atmpS3942 = 1;
    } else {
      struct moonbit_result_0 _tmp_4548;
      int32_t _handle__error__result_4549;
      moonbit_incref(_M0L12async__testsS1627);
      moonbit_incref(_M0L17error__to__stringS1606);
      moonbit_incref(_M0L8filenameS1602);
      moonbit_incref(_M0L14handle__resultS1597);
      #line 572 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _tmp_4548
      = _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1627, _M0L8filenameS1602, _M0L5indexS1605, _M0L14handle__resultS1597, _M0L17error__to__stringS1606);
      if (_tmp_4548.tag) {
        int32_t const _M0L5_2aokS3947 = _tmp_4548.data.ok;
        _handle__error__result_4549 = _M0L5_2aokS3947;
      } else {
        void* const _M0L6_2aerrS3948 = _tmp_4548.data.err;
        moonbit_decref(_M0L12async__testsS1627);
        moonbit_decref(_M0L17error__to__stringS1606);
        moonbit_decref(_M0L8filenameS1602);
        _M0L11_2atry__errS1621 = _M0L6_2aerrS3948;
        goto join_1620;
      }
      if (_handle__error__result_4549) {
        moonbit_decref(_M0L12async__testsS1627);
        moonbit_decref(_M0L17error__to__stringS1606);
        moonbit_decref(_M0L8filenameS1602);
        _M0L6_2atmpS3942 = 1;
      } else {
        struct moonbit_result_0 _tmp_4550;
        int32_t _handle__error__result_4551;
        moonbit_incref(_M0L12async__testsS1627);
        moonbit_incref(_M0L17error__to__stringS1606);
        moonbit_incref(_M0L8filenameS1602);
        moonbit_incref(_M0L14handle__resultS1597);
        #line 575 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        _tmp_4550
        = _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1627, _M0L8filenameS1602, _M0L5indexS1605, _M0L14handle__resultS1597, _M0L17error__to__stringS1606);
        if (_tmp_4550.tag) {
          int32_t const _M0L5_2aokS3945 = _tmp_4550.data.ok;
          _handle__error__result_4551 = _M0L5_2aokS3945;
        } else {
          void* const _M0L6_2aerrS3946 = _tmp_4550.data.err;
          moonbit_decref(_M0L12async__testsS1627);
          moonbit_decref(_M0L17error__to__stringS1606);
          moonbit_decref(_M0L8filenameS1602);
          _M0L11_2atry__errS1621 = _M0L6_2aerrS3946;
          goto join_1620;
        }
        if (_handle__error__result_4551) {
          moonbit_decref(_M0L12async__testsS1627);
          moonbit_decref(_M0L17error__to__stringS1606);
          moonbit_decref(_M0L8filenameS1602);
          _M0L6_2atmpS3942 = 1;
        } else {
          struct moonbit_result_0 _tmp_4552;
          moonbit_incref(_M0L14handle__resultS1597);
          #line 578 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
          _tmp_4552
          = _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1627, _M0L8filenameS1602, _M0L5indexS1605, _M0L14handle__resultS1597, _M0L17error__to__stringS1606);
          if (_tmp_4552.tag) {
            int32_t const _M0L5_2aokS3943 = _tmp_4552.data.ok;
            _M0L6_2atmpS3942 = _M0L5_2aokS3943;
          } else {
            void* const _M0L6_2aerrS3944 = _tmp_4552.data.err;
            _M0L11_2atry__errS1621 = _M0L6_2aerrS3944;
            goto join_1620;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3942) {
    void* _M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3953 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3953)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3953)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1621
    = _M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3953;
    goto join_1620;
  } else {
    moonbit_decref(_M0L14handle__resultS1597);
  }
  goto joinlet_4543;
  join_1620:;
  _M0L3errS1622 = _M0L11_2atry__errS1621;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1625
  = (struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1622;
  _M0L8_2afieldS3964 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1625->$0;
  _M0L6_2acntS4366
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1625)->rc;
  if (_M0L6_2acntS4366 > 1) {
    int32_t _M0L11_2anew__cntS4367 = _M0L6_2acntS4366 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1625)->rc
    = _M0L11_2anew__cntS4367;
    moonbit_incref(_M0L8_2afieldS3964);
  } else if (_M0L6_2acntS4366 == 1) {
    #line 585 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1625);
  }
  _M0L7_2anameS1626 = _M0L8_2afieldS3964;
  _M0L4nameS1624 = _M0L7_2anameS1626;
  goto join_1623;
  goto joinlet_4553;
  join_1623:;
  #line 586 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1597(_M0L14handle__resultS1597, _M0L4nameS1624, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4553:;
  joinlet_4543:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1606(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3941,
  void* _M0L3errS1607
) {
  void* _M0L1eS1609;
  moonbit_string_t _M0L1eS1611;
  #line 555 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3941);
  switch (Moonbit_object_tag(_M0L3errS1607)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1612 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1607;
      moonbit_string_t _M0L8_2afieldS3965 = _M0L10_2aFailureS1612->$0;
      int32_t _M0L6_2acntS4368 =
        Moonbit_object_header(_M0L10_2aFailureS1612)->rc;
      moonbit_string_t _M0L4_2aeS1613;
      if (_M0L6_2acntS4368 > 1) {
        int32_t _M0L11_2anew__cntS4369 = _M0L6_2acntS4368 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1612)->rc
        = _M0L11_2anew__cntS4369;
        moonbit_incref(_M0L8_2afieldS3965);
      } else if (_M0L6_2acntS4368 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1612);
      }
      _M0L4_2aeS1613 = _M0L8_2afieldS3965;
      _M0L1eS1611 = _M0L4_2aeS1613;
      goto join_1610;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1614 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1607;
      moonbit_string_t _M0L8_2afieldS3966 = _M0L15_2aInspectErrorS1614->$0;
      int32_t _M0L6_2acntS4370 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1614)->rc;
      moonbit_string_t _M0L4_2aeS1615;
      if (_M0L6_2acntS4370 > 1) {
        int32_t _M0L11_2anew__cntS4371 = _M0L6_2acntS4370 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1614)->rc
        = _M0L11_2anew__cntS4371;
        moonbit_incref(_M0L8_2afieldS3966);
      } else if (_M0L6_2acntS4370 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1614);
      }
      _M0L4_2aeS1615 = _M0L8_2afieldS3966;
      _M0L1eS1611 = _M0L4_2aeS1615;
      goto join_1610;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1616 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1607;
      moonbit_string_t _M0L8_2afieldS3967 = _M0L16_2aSnapshotErrorS1616->$0;
      int32_t _M0L6_2acntS4372 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1616)->rc;
      moonbit_string_t _M0L4_2aeS1617;
      if (_M0L6_2acntS4372 > 1) {
        int32_t _M0L11_2anew__cntS4373 = _M0L6_2acntS4372 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1616)->rc
        = _M0L11_2anew__cntS4373;
        moonbit_incref(_M0L8_2afieldS3967);
      } else if (_M0L6_2acntS4372 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1616);
      }
      _M0L4_2aeS1617 = _M0L8_2afieldS3967;
      _M0L1eS1611 = _M0L4_2aeS1617;
      goto join_1610;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1618 =
        (struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1607;
      moonbit_string_t _M0L8_2afieldS3968 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1618->$0;
      int32_t _M0L6_2acntS4374 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1618)->rc;
      moonbit_string_t _M0L4_2aeS1619;
      if (_M0L6_2acntS4374 > 1) {
        int32_t _M0L11_2anew__cntS4375 = _M0L6_2acntS4374 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1618)->rc
        = _M0L11_2anew__cntS4375;
        moonbit_incref(_M0L8_2afieldS3968);
      } else if (_M0L6_2acntS4374 == 1) {
        #line 556 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1618);
      }
      _M0L4_2aeS1619 = _M0L8_2afieldS3968;
      _M0L1eS1611 = _M0L4_2aeS1619;
      goto join_1610;
      break;
    }
    default: {
      _M0L1eS1609 = _M0L3errS1607;
      goto join_1608;
      break;
    }
  }
  join_1610:;
  return _M0L1eS1611;
  join_1608:;
  #line 561 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1609);
}

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1597(
  struct _M0TWssbEu* _M0L6_2aenvS3927,
  moonbit_string_t _M0L8testnameS1598,
  moonbit_string_t _M0L7messageS1599,
  int32_t _M0L7skippedS1600
) {
  struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597* _M0L14_2acasted__envS3928;
  moonbit_string_t _M0L8_2afieldS3978;
  moonbit_string_t _M0L8filenameS1602;
  int32_t _M0L8_2afieldS3977;
  int32_t _M0L6_2acntS4376;
  int32_t _M0L5indexS1605;
  int32_t _if__result_4556;
  moonbit_string_t _M0L10file__nameS1601;
  moonbit_string_t _M0L10test__nameS1603;
  moonbit_string_t _M0L7messageS1604;
  moonbit_string_t _M0L6_2atmpS3940;
  moonbit_string_t _M0L6_2atmpS3976;
  moonbit_string_t _M0L6_2atmpS3939;
  moonbit_string_t _M0L6_2atmpS3975;
  moonbit_string_t _M0L6_2atmpS3937;
  moonbit_string_t _M0L6_2atmpS3938;
  moonbit_string_t _M0L6_2atmpS3974;
  moonbit_string_t _M0L6_2atmpS3936;
  moonbit_string_t _M0L6_2atmpS3973;
  moonbit_string_t _M0L6_2atmpS3934;
  moonbit_string_t _M0L6_2atmpS3935;
  moonbit_string_t _M0L6_2atmpS3972;
  moonbit_string_t _M0L6_2atmpS3933;
  moonbit_string_t _M0L6_2atmpS3971;
  moonbit_string_t _M0L6_2atmpS3931;
  moonbit_string_t _M0L6_2atmpS3932;
  moonbit_string_t _M0L6_2atmpS3970;
  moonbit_string_t _M0L6_2atmpS3930;
  moonbit_string_t _M0L6_2atmpS3969;
  moonbit_string_t _M0L6_2atmpS3929;
  #line 539 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3928
  = (struct _M0R128_24clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1597*)_M0L6_2aenvS3927;
  _M0L8_2afieldS3978 = _M0L14_2acasted__envS3928->$1;
  _M0L8filenameS1602 = _M0L8_2afieldS3978;
  _M0L8_2afieldS3977 = _M0L14_2acasted__envS3928->$0;
  _M0L6_2acntS4376 = Moonbit_object_header(_M0L14_2acasted__envS3928)->rc;
  if (_M0L6_2acntS4376 > 1) {
    int32_t _M0L11_2anew__cntS4377 = _M0L6_2acntS4376 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3928)->rc
    = _M0L11_2anew__cntS4377;
    moonbit_incref(_M0L8filenameS1602);
  } else if (_M0L6_2acntS4376 == 1) {
    #line 539 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3928);
  }
  _M0L5indexS1605 = _M0L8_2afieldS3977;
  if (!_M0L7skippedS1600) {
    _if__result_4556 = 1;
  } else {
    _if__result_4556 = 0;
  }
  if (_if__result_4556) {
    
  }
  #line 545 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1601 = _M0MPC16string6String6escape(_M0L8filenameS1602);
  #line 546 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1603 = _M0MPC16string6String6escape(_M0L8testnameS1598);
  #line 547 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1604 = _M0MPC16string6String6escape(_M0L7messageS1599);
  #line 548 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 550 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3940
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1601);
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3976
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3940);
  moonbit_decref(_M0L6_2atmpS3940);
  _M0L6_2atmpS3939 = _M0L6_2atmpS3976;
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3975
  = moonbit_add_string(_M0L6_2atmpS3939, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3939);
  _M0L6_2atmpS3937 = _M0L6_2atmpS3975;
  #line 550 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3938
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1605);
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3974 = moonbit_add_string(_M0L6_2atmpS3937, _M0L6_2atmpS3938);
  moonbit_decref(_M0L6_2atmpS3937);
  moonbit_decref(_M0L6_2atmpS3938);
  _M0L6_2atmpS3936 = _M0L6_2atmpS3974;
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3973
  = moonbit_add_string(_M0L6_2atmpS3936, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3936);
  _M0L6_2atmpS3934 = _M0L6_2atmpS3973;
  #line 550 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3935
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1603);
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3972 = moonbit_add_string(_M0L6_2atmpS3934, _M0L6_2atmpS3935);
  moonbit_decref(_M0L6_2atmpS3934);
  moonbit_decref(_M0L6_2atmpS3935);
  _M0L6_2atmpS3933 = _M0L6_2atmpS3972;
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3971
  = moonbit_add_string(_M0L6_2atmpS3933, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3933);
  _M0L6_2atmpS3931 = _M0L6_2atmpS3971;
  #line 550 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3932
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1604);
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3970 = moonbit_add_string(_M0L6_2atmpS3931, _M0L6_2atmpS3932);
  moonbit_decref(_M0L6_2atmpS3931);
  moonbit_decref(_M0L6_2atmpS3932);
  _M0L6_2atmpS3930 = _M0L6_2atmpS3970;
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3969
  = moonbit_add_string(_M0L6_2atmpS3930, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3930);
  _M0L6_2atmpS3929 = _M0L6_2atmpS3969;
  #line 549 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3929);
  #line 552 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1596,
  moonbit_string_t _M0L8filenameS1593,
  int32_t _M0L5indexS1587,
  struct _M0TWssbEu* _M0L14handle__resultS1583,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1585
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1563;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1592;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1565;
  moonbit_string_t* _M0L5attrsS1566;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1586;
  moonbit_string_t _M0L4nameS1569;
  moonbit_string_t _M0L4nameS1567;
  int32_t _M0L6_2atmpS3926;
  struct _M0TWEOs* _M0L5_2aitS1571;
  struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__* _closure_4565;
  struct _M0TWEOc* _M0L6_2atmpS3917;
  struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__* _closure_4566;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3918;
  struct moonbit_result_0 _result_4567;
  #line 413 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1596);
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 420 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1592
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal21cache__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1593);
  if (_M0L7_2abindS1592 == 0) {
    struct moonbit_result_0 _result_4558;
    if (_M0L7_2abindS1592) {
      moonbit_decref(_M0L7_2abindS1592);
    }
    moonbit_decref(_M0L17error__to__stringS1585);
    moonbit_decref(_M0L14handle__resultS1583);
    _result_4558.tag = 1;
    _result_4558.data.ok = 0;
    return _result_4558;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1594 =
      _M0L7_2abindS1592;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1595 =
      _M0L7_2aSomeS1594;
    _M0L10index__mapS1563 = _M0L13_2aindex__mapS1595;
    goto join_1562;
  }
  join_1562:;
  #line 422 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1586
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1563, _M0L5indexS1587);
  if (_M0L7_2abindS1586 == 0) {
    struct moonbit_result_0 _result_4560;
    if (_M0L7_2abindS1586) {
      moonbit_decref(_M0L7_2abindS1586);
    }
    moonbit_decref(_M0L17error__to__stringS1585);
    moonbit_decref(_M0L14handle__resultS1583);
    _result_4560.tag = 1;
    _result_4560.data.ok = 0;
    return _result_4560;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1588 =
      _M0L7_2abindS1586;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1589 = _M0L7_2aSomeS1588;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3982 = _M0L4_2axS1589->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1590 = _M0L8_2afieldS3982;
    moonbit_string_t* _M0L8_2afieldS3981 = _M0L4_2axS1589->$1;
    int32_t _M0L6_2acntS4378 = Moonbit_object_header(_M0L4_2axS1589)->rc;
    moonbit_string_t* _M0L8_2aattrsS1591;
    if (_M0L6_2acntS4378 > 1) {
      int32_t _M0L11_2anew__cntS4379 = _M0L6_2acntS4378 - 1;
      Moonbit_object_header(_M0L4_2axS1589)->rc = _M0L11_2anew__cntS4379;
      moonbit_incref(_M0L8_2afieldS3981);
      moonbit_incref(_M0L4_2afS1590);
    } else if (_M0L6_2acntS4378 == 1) {
      #line 420 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1589);
    }
    _M0L8_2aattrsS1591 = _M0L8_2afieldS3981;
    _M0L1fS1565 = _M0L4_2afS1590;
    _M0L5attrsS1566 = _M0L8_2aattrsS1591;
    goto join_1564;
  }
  join_1564:;
  _M0L6_2atmpS3926 = Moonbit_array_length(_M0L5attrsS1566);
  if (_M0L6_2atmpS3926 >= 1) {
    moonbit_string_t _M0L6_2atmpS3980 = (moonbit_string_t)_M0L5attrsS1566[0];
    moonbit_string_t _M0L7_2anameS1570 = _M0L6_2atmpS3980;
    moonbit_incref(_M0L7_2anameS1570);
    _M0L4nameS1569 = _M0L7_2anameS1570;
    goto join_1568;
  } else {
    _M0L4nameS1567 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4561;
  join_1568:;
  _M0L4nameS1567 = _M0L4nameS1569;
  joinlet_4561:;
  #line 423 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1571 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1566);
  while (1) {
    moonbit_string_t _M0L4attrS1573;
    moonbit_string_t _M0L7_2abindS1580;
    int32_t _M0L6_2atmpS3910;
    int64_t _M0L6_2atmpS3909;
    moonbit_incref(_M0L5_2aitS1571);
    #line 425 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1580 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1571);
    if (_M0L7_2abindS1580 == 0) {
      if (_M0L7_2abindS1580) {
        moonbit_decref(_M0L7_2abindS1580);
      }
      moonbit_decref(_M0L5_2aitS1571);
    } else {
      moonbit_string_t _M0L7_2aSomeS1581 = _M0L7_2abindS1580;
      moonbit_string_t _M0L7_2aattrS1582 = _M0L7_2aSomeS1581;
      _M0L4attrS1573 = _M0L7_2aattrS1582;
      goto join_1572;
    }
    goto joinlet_4563;
    join_1572:;
    _M0L6_2atmpS3910 = Moonbit_array_length(_M0L4attrS1573);
    _M0L6_2atmpS3909 = (int64_t)_M0L6_2atmpS3910;
    moonbit_incref(_M0L4attrS1573);
    #line 426 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1573, 5, 0, _M0L6_2atmpS3909)
    ) {
      int32_t _M0L6_2atmpS3916 = _M0L4attrS1573[0];
      int32_t _M0L4_2axS1574 = _M0L6_2atmpS3916;
      if (_M0L4_2axS1574 == 112) {
        int32_t _M0L6_2atmpS3915 = _M0L4attrS1573[1];
        int32_t _M0L4_2axS1575 = _M0L6_2atmpS3915;
        if (_M0L4_2axS1575 == 97) {
          int32_t _M0L6_2atmpS3914 = _M0L4attrS1573[2];
          int32_t _M0L4_2axS1576 = _M0L6_2atmpS3914;
          if (_M0L4_2axS1576 == 110) {
            int32_t _M0L6_2atmpS3913 = _M0L4attrS1573[3];
            int32_t _M0L4_2axS1577 = _M0L6_2atmpS3913;
            if (_M0L4_2axS1577 == 105) {
              int32_t _M0L6_2atmpS3979 = _M0L4attrS1573[4];
              int32_t _M0L6_2atmpS3912;
              int32_t _M0L4_2axS1578;
              moonbit_decref(_M0L4attrS1573);
              _M0L6_2atmpS3912 = _M0L6_2atmpS3979;
              _M0L4_2axS1578 = _M0L6_2atmpS3912;
              if (_M0L4_2axS1578 == 99) {
                void* _M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3911;
                struct moonbit_result_0 _result_4564;
                moonbit_decref(_M0L17error__to__stringS1585);
                moonbit_decref(_M0L14handle__resultS1583);
                moonbit_decref(_M0L5_2aitS1571);
                moonbit_decref(_M0L1fS1565);
                _M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3911
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3911)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3911)->$0
                = _M0L4nameS1567;
                _result_4564.tag = 0;
                _result_4564.data.err
                = _M0L126clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3911;
                return _result_4564;
              }
            } else {
              moonbit_decref(_M0L4attrS1573);
            }
          } else {
            moonbit_decref(_M0L4attrS1573);
          }
        } else {
          moonbit_decref(_M0L4attrS1573);
        }
      } else {
        moonbit_decref(_M0L4attrS1573);
      }
    } else {
      moonbit_decref(_M0L4attrS1573);
    }
    continue;
    joinlet_4563:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1583);
  moonbit_incref(_M0L4nameS1567);
  _closure_4565
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__*)moonbit_malloc(sizeof(struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__));
  Moonbit_object_header(_closure_4565)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__, $0) >> 2, 2, 0);
  _closure_4565->code
  = &_M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testC3923l433;
  _closure_4565->$0 = _M0L14handle__resultS1583;
  _closure_4565->$1 = _M0L4nameS1567;
  _M0L6_2atmpS3917 = (struct _M0TWEOc*)_closure_4565;
  _closure_4566
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__*)moonbit_malloc(sizeof(struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__));
  Moonbit_object_header(_closure_4566)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__, $0) >> 2, 3, 0);
  _closure_4566->code
  = &_M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testC3919l434;
  _closure_4566->$0 = _M0L17error__to__stringS1585;
  _closure_4566->$1 = _M0L14handle__resultS1583;
  _closure_4566->$2 = _M0L4nameS1567;
  _M0L6_2atmpS3918 = (struct _M0TWRPC15error5ErrorEu*)_closure_4566;
  #line 431 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal21cache__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1565, _M0L6_2atmpS3917, _M0L6_2atmpS3918);
  _result_4567.tag = 1;
  _result_4567.data.ok = 1;
  return _result_4567;
}

int32_t _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testC3923l433(
  struct _M0TWEOc* _M0L6_2aenvS3924
) {
  struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__* _M0L14_2acasted__envS3925;
  moonbit_string_t _M0L8_2afieldS3984;
  moonbit_string_t _M0L4nameS1567;
  struct _M0TWssbEu* _M0L8_2afieldS3983;
  int32_t _M0L6_2acntS4380;
  struct _M0TWssbEu* _M0L14handle__resultS1583;
  #line 433 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3925
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3923__l433__*)_M0L6_2aenvS3924;
  _M0L8_2afieldS3984 = _M0L14_2acasted__envS3925->$1;
  _M0L4nameS1567 = _M0L8_2afieldS3984;
  _M0L8_2afieldS3983 = _M0L14_2acasted__envS3925->$0;
  _M0L6_2acntS4380 = Moonbit_object_header(_M0L14_2acasted__envS3925)->rc;
  if (_M0L6_2acntS4380 > 1) {
    int32_t _M0L11_2anew__cntS4381 = _M0L6_2acntS4380 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3925)->rc
    = _M0L11_2anew__cntS4381;
    moonbit_incref(_M0L4nameS1567);
    moonbit_incref(_M0L8_2afieldS3983);
  } else if (_M0L6_2acntS4380 == 1) {
    #line 433 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3925);
  }
  _M0L14handle__resultS1583 = _M0L8_2afieldS3983;
  #line 433 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1583->code(_M0L14handle__resultS1583, _M0L4nameS1567, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal21cache__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testC3919l434(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3920,
  void* _M0L3errS1584
) {
  struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__* _M0L14_2acasted__envS3921;
  moonbit_string_t _M0L8_2afieldS3987;
  moonbit_string_t _M0L4nameS1567;
  struct _M0TWssbEu* _M0L8_2afieldS3986;
  struct _M0TWssbEu* _M0L14handle__resultS1583;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3985;
  int32_t _M0L6_2acntS4382;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1585;
  moonbit_string_t _M0L6_2atmpS3922;
  #line 434 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3921
  = (struct _M0R225_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fcache__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3919__l434__*)_M0L6_2aenvS3920;
  _M0L8_2afieldS3987 = _M0L14_2acasted__envS3921->$2;
  _M0L4nameS1567 = _M0L8_2afieldS3987;
  _M0L8_2afieldS3986 = _M0L14_2acasted__envS3921->$1;
  _M0L14handle__resultS1583 = _M0L8_2afieldS3986;
  _M0L8_2afieldS3985 = _M0L14_2acasted__envS3921->$0;
  _M0L6_2acntS4382 = Moonbit_object_header(_M0L14_2acasted__envS3921)->rc;
  if (_M0L6_2acntS4382 > 1) {
    int32_t _M0L11_2anew__cntS4383 = _M0L6_2acntS4382 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3921)->rc
    = _M0L11_2anew__cntS4383;
    moonbit_incref(_M0L4nameS1567);
    moonbit_incref(_M0L14handle__resultS1583);
    moonbit_incref(_M0L8_2afieldS3985);
  } else if (_M0L6_2acntS4382 == 1) {
    #line 434 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3921);
  }
  _M0L17error__to__stringS1585 = _M0L8_2afieldS3985;
  #line 434 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3922
  = _M0L17error__to__stringS1585->code(_M0L17error__to__stringS1585, _M0L3errS1584);
  #line 434 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1583->code(_M0L14handle__resultS1583, _M0L4nameS1567, _M0L6_2atmpS3922, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1557,
  struct _M0TWEOc* _M0L6on__okS1558,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1555
) {
  void* _M0L11_2atry__errS1553;
  struct moonbit_result_0 _tmp_4569;
  void* _M0L3errS1554;
  #line 375 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4569 = _M0L1fS1557->code(_M0L1fS1557);
  if (_tmp_4569.tag) {
    int32_t const _M0L5_2aokS3907 = _tmp_4569.data.ok;
    moonbit_decref(_M0L7on__errS1555);
  } else {
    void* const _M0L6_2aerrS3908 = _tmp_4569.data.err;
    moonbit_decref(_M0L6on__okS1558);
    _M0L11_2atry__errS1553 = _M0L6_2aerrS3908;
    goto join_1552;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1558->code(_M0L6on__okS1558);
  goto joinlet_4568;
  join_1552:;
  _M0L3errS1554 = _M0L11_2atry__errS1553;
  #line 383 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1555->code(_M0L7on__errS1555, _M0L3errS1554);
  joinlet_4568:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1512;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1525;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1530;
  struct _M0TUsiE** _M0L6_2atmpS3906;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1537;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1538;
  moonbit_string_t _M0L6_2atmpS3905;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1539;
  int32_t _M0L7_2abindS1540;
  int32_t _M0L2__S1541;
  #line 193 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1512 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1525
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1530 = 0;
  _M0L6_2atmpS3906 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1537
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1537)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1537->$0 = _M0L6_2atmpS3906;
  _M0L16file__and__indexS1537->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1538
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1525(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1525);
  #line 284 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3905 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1538, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1539
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1530(_M0L51moonbit__test__driver__internal__split__mbt__stringS1530, _M0L6_2atmpS3905, 47);
  _M0L7_2abindS1540 = _M0L10test__argsS1539->$1;
  _M0L2__S1541 = 0;
  while (1) {
    if (_M0L2__S1541 < _M0L7_2abindS1540) {
      moonbit_string_t* _M0L8_2afieldS3989 = _M0L10test__argsS1539->$0;
      moonbit_string_t* _M0L3bufS3904 = _M0L8_2afieldS3989;
      moonbit_string_t _M0L6_2atmpS3988 =
        (moonbit_string_t)_M0L3bufS3904[_M0L2__S1541];
      moonbit_string_t _M0L3argS1542 = _M0L6_2atmpS3988;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1543;
      moonbit_string_t _M0L4fileS1544;
      moonbit_string_t _M0L5rangeS1545;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1546;
      moonbit_string_t _M0L6_2atmpS3902;
      int32_t _M0L5startS1547;
      moonbit_string_t _M0L6_2atmpS3901;
      int32_t _M0L3endS1548;
      int32_t _M0L1iS1549;
      int32_t _M0L6_2atmpS3903;
      moonbit_incref(_M0L3argS1542);
      #line 288 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1543
      = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1530(_M0L51moonbit__test__driver__internal__split__mbt__stringS1530, _M0L3argS1542, 58);
      moonbit_incref(_M0L16file__and__rangeS1543);
      #line 289 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1544
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1543, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1545
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1543, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1546
      = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1530(_M0L51moonbit__test__driver__internal__split__mbt__stringS1530, _M0L5rangeS1545, 45);
      moonbit_incref(_M0L15start__and__endS1546);
      #line 294 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3902
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1546, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1547
      = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1512(_M0L45moonbit__test__driver__internal__parse__int__S1512, _M0L6_2atmpS3902);
      #line 295 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3901
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1546, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1548
      = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1512(_M0L45moonbit__test__driver__internal__parse__int__S1512, _M0L6_2atmpS3901);
      _M0L1iS1549 = _M0L5startS1547;
      while (1) {
        if (_M0L1iS1549 < _M0L3endS1548) {
          struct _M0TUsiE* _M0L8_2atupleS3899;
          int32_t _M0L6_2atmpS3900;
          moonbit_incref(_M0L4fileS1544);
          _M0L8_2atupleS3899
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3899)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3899->$0 = _M0L4fileS1544;
          _M0L8_2atupleS3899->$1 = _M0L1iS1549;
          moonbit_incref(_M0L16file__and__indexS1537);
          #line 297 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1537, _M0L8_2atupleS3899);
          _M0L6_2atmpS3900 = _M0L1iS1549 + 1;
          _M0L1iS1549 = _M0L6_2atmpS3900;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1544);
        }
        break;
      }
      _M0L6_2atmpS3903 = _M0L2__S1541 + 1;
      _M0L2__S1541 = _M0L6_2atmpS3903;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1539);
    }
    break;
  }
  return _M0L16file__and__indexS1537;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1530(
  int32_t _M0L6_2aenvS3880,
  moonbit_string_t _M0L1sS1531,
  int32_t _M0L3sepS1532
) {
  moonbit_string_t* _M0L6_2atmpS3898;
  struct _M0TPB5ArrayGsE* _M0L3resS1533;
  struct _M0TPC13ref3RefGiE* _M0L1iS1534;
  struct _M0TPC13ref3RefGiE* _M0L5startS1535;
  int32_t _M0L3valS3893;
  int32_t _M0L6_2atmpS3894;
  #line 261 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3898 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1533
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1533)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1533->$0 = _M0L6_2atmpS3898;
  _M0L3resS1533->$1 = 0;
  _M0L1iS1534
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1534)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1534->$0 = 0;
  _M0L5startS1535
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1535)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1535->$0 = 0;
  while (1) {
    int32_t _M0L3valS3881 = _M0L1iS1534->$0;
    int32_t _M0L6_2atmpS3882 = Moonbit_array_length(_M0L1sS1531);
    if (_M0L3valS3881 < _M0L6_2atmpS3882) {
      int32_t _M0L3valS3885 = _M0L1iS1534->$0;
      int32_t _M0L6_2atmpS3884;
      int32_t _M0L6_2atmpS3883;
      int32_t _M0L3valS3892;
      int32_t _M0L6_2atmpS3891;
      if (
        _M0L3valS3885 < 0
        || _M0L3valS3885 >= Moonbit_array_length(_M0L1sS1531)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3884 = _M0L1sS1531[_M0L3valS3885];
      _M0L6_2atmpS3883 = _M0L6_2atmpS3884;
      if (_M0L6_2atmpS3883 == _M0L3sepS1532) {
        int32_t _M0L3valS3887 = _M0L5startS1535->$0;
        int32_t _M0L3valS3888 = _M0L1iS1534->$0;
        moonbit_string_t _M0L6_2atmpS3886;
        int32_t _M0L3valS3890;
        int32_t _M0L6_2atmpS3889;
        moonbit_incref(_M0L1sS1531);
        #line 270 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3886
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1531, _M0L3valS3887, _M0L3valS3888);
        moonbit_incref(_M0L3resS1533);
        #line 270 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1533, _M0L6_2atmpS3886);
        _M0L3valS3890 = _M0L1iS1534->$0;
        _M0L6_2atmpS3889 = _M0L3valS3890 + 1;
        _M0L5startS1535->$0 = _M0L6_2atmpS3889;
      }
      _M0L3valS3892 = _M0L1iS1534->$0;
      _M0L6_2atmpS3891 = _M0L3valS3892 + 1;
      _M0L1iS1534->$0 = _M0L6_2atmpS3891;
      continue;
    } else {
      moonbit_decref(_M0L1iS1534);
    }
    break;
  }
  _M0L3valS3893 = _M0L5startS1535->$0;
  _M0L6_2atmpS3894 = Moonbit_array_length(_M0L1sS1531);
  if (_M0L3valS3893 < _M0L6_2atmpS3894) {
    int32_t _M0L8_2afieldS3990 = _M0L5startS1535->$0;
    int32_t _M0L3valS3896;
    int32_t _M0L6_2atmpS3897;
    moonbit_string_t _M0L6_2atmpS3895;
    moonbit_decref(_M0L5startS1535);
    _M0L3valS3896 = _M0L8_2afieldS3990;
    _M0L6_2atmpS3897 = Moonbit_array_length(_M0L1sS1531);
    #line 276 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3895
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1531, _M0L3valS3896, _M0L6_2atmpS3897);
    moonbit_incref(_M0L3resS1533);
    #line 276 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1533, _M0L6_2atmpS3895);
  } else {
    moonbit_decref(_M0L5startS1535);
    moonbit_decref(_M0L1sS1531);
  }
  return _M0L3resS1533;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1525(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518
) {
  moonbit_bytes_t* _M0L3tmpS1526;
  int32_t _M0L6_2atmpS3879;
  struct _M0TPB5ArrayGsE* _M0L3resS1527;
  int32_t _M0L1iS1528;
  #line 250 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1526
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3879 = Moonbit_array_length(_M0L3tmpS1526);
  #line 254 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1527 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3879);
  _M0L1iS1528 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3875 = Moonbit_array_length(_M0L3tmpS1526);
    if (_M0L1iS1528 < _M0L6_2atmpS3875) {
      moonbit_bytes_t _M0L6_2atmpS3991;
      moonbit_bytes_t _M0L6_2atmpS3877;
      moonbit_string_t _M0L6_2atmpS3876;
      int32_t _M0L6_2atmpS3878;
      if (
        _M0L1iS1528 < 0 || _M0L1iS1528 >= Moonbit_array_length(_M0L3tmpS1526)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3991 = (moonbit_bytes_t)_M0L3tmpS1526[_M0L1iS1528];
      _M0L6_2atmpS3877 = _M0L6_2atmpS3991;
      moonbit_incref(_M0L6_2atmpS3877);
      #line 256 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3876
      = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518, _M0L6_2atmpS3877);
      moonbit_incref(_M0L3resS1527);
      #line 256 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1527, _M0L6_2atmpS3876);
      _M0L6_2atmpS3878 = _M0L1iS1528 + 1;
      _M0L1iS1528 = _M0L6_2atmpS3878;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1526);
    }
    break;
  }
  return _M0L3resS1527;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1518(
  int32_t _M0L6_2aenvS3789,
  moonbit_bytes_t _M0L5bytesS1519
) {
  struct _M0TPB13StringBuilder* _M0L3resS1520;
  int32_t _M0L3lenS1521;
  struct _M0TPC13ref3RefGiE* _M0L1iS1522;
  #line 206 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1520 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1521 = Moonbit_array_length(_M0L5bytesS1519);
  _M0L1iS1522
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1522)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1522->$0 = 0;
  while (1) {
    int32_t _M0L3valS3790 = _M0L1iS1522->$0;
    if (_M0L3valS3790 < _M0L3lenS1521) {
      int32_t _M0L3valS3874 = _M0L1iS1522->$0;
      int32_t _M0L6_2atmpS3873;
      int32_t _M0L6_2atmpS3872;
      struct _M0TPC13ref3RefGiE* _M0L1cS1523;
      int32_t _M0L3valS3791;
      if (
        _M0L3valS3874 < 0
        || _M0L3valS3874 >= Moonbit_array_length(_M0L5bytesS1519)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3873 = _M0L5bytesS1519[_M0L3valS3874];
      _M0L6_2atmpS3872 = (int32_t)_M0L6_2atmpS3873;
      _M0L1cS1523
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1523)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1523->$0 = _M0L6_2atmpS3872;
      _M0L3valS3791 = _M0L1cS1523->$0;
      if (_M0L3valS3791 < 128) {
        int32_t _M0L8_2afieldS3992 = _M0L1cS1523->$0;
        int32_t _M0L3valS3793;
        int32_t _M0L6_2atmpS3792;
        int32_t _M0L3valS3795;
        int32_t _M0L6_2atmpS3794;
        moonbit_decref(_M0L1cS1523);
        _M0L3valS3793 = _M0L8_2afieldS3992;
        _M0L6_2atmpS3792 = _M0L3valS3793;
        moonbit_incref(_M0L3resS1520);
        #line 215 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1520, _M0L6_2atmpS3792);
        _M0L3valS3795 = _M0L1iS1522->$0;
        _M0L6_2atmpS3794 = _M0L3valS3795 + 1;
        _M0L1iS1522->$0 = _M0L6_2atmpS3794;
      } else {
        int32_t _M0L3valS3796 = _M0L1cS1523->$0;
        if (_M0L3valS3796 < 224) {
          int32_t _M0L3valS3798 = _M0L1iS1522->$0;
          int32_t _M0L6_2atmpS3797 = _M0L3valS3798 + 1;
          int32_t _M0L3valS3807;
          int32_t _M0L6_2atmpS3806;
          int32_t _M0L6_2atmpS3800;
          int32_t _M0L3valS3805;
          int32_t _M0L6_2atmpS3804;
          int32_t _M0L6_2atmpS3803;
          int32_t _M0L6_2atmpS3802;
          int32_t _M0L6_2atmpS3801;
          int32_t _M0L6_2atmpS3799;
          int32_t _M0L8_2afieldS3993;
          int32_t _M0L3valS3809;
          int32_t _M0L6_2atmpS3808;
          int32_t _M0L3valS3811;
          int32_t _M0L6_2atmpS3810;
          if (_M0L6_2atmpS3797 >= _M0L3lenS1521) {
            moonbit_decref(_M0L1cS1523);
            moonbit_decref(_M0L1iS1522);
            moonbit_decref(_M0L5bytesS1519);
            break;
          }
          _M0L3valS3807 = _M0L1cS1523->$0;
          _M0L6_2atmpS3806 = _M0L3valS3807 & 31;
          _M0L6_2atmpS3800 = _M0L6_2atmpS3806 << 6;
          _M0L3valS3805 = _M0L1iS1522->$0;
          _M0L6_2atmpS3804 = _M0L3valS3805 + 1;
          if (
            _M0L6_2atmpS3804 < 0
            || _M0L6_2atmpS3804 >= Moonbit_array_length(_M0L5bytesS1519)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3803 = _M0L5bytesS1519[_M0L6_2atmpS3804];
          _M0L6_2atmpS3802 = (int32_t)_M0L6_2atmpS3803;
          _M0L6_2atmpS3801 = _M0L6_2atmpS3802 & 63;
          _M0L6_2atmpS3799 = _M0L6_2atmpS3800 | _M0L6_2atmpS3801;
          _M0L1cS1523->$0 = _M0L6_2atmpS3799;
          _M0L8_2afieldS3993 = _M0L1cS1523->$0;
          moonbit_decref(_M0L1cS1523);
          _M0L3valS3809 = _M0L8_2afieldS3993;
          _M0L6_2atmpS3808 = _M0L3valS3809;
          moonbit_incref(_M0L3resS1520);
          #line 222 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1520, _M0L6_2atmpS3808);
          _M0L3valS3811 = _M0L1iS1522->$0;
          _M0L6_2atmpS3810 = _M0L3valS3811 + 2;
          _M0L1iS1522->$0 = _M0L6_2atmpS3810;
        } else {
          int32_t _M0L3valS3812 = _M0L1cS1523->$0;
          if (_M0L3valS3812 < 240) {
            int32_t _M0L3valS3814 = _M0L1iS1522->$0;
            int32_t _M0L6_2atmpS3813 = _M0L3valS3814 + 2;
            int32_t _M0L3valS3830;
            int32_t _M0L6_2atmpS3829;
            int32_t _M0L6_2atmpS3822;
            int32_t _M0L3valS3828;
            int32_t _M0L6_2atmpS3827;
            int32_t _M0L6_2atmpS3826;
            int32_t _M0L6_2atmpS3825;
            int32_t _M0L6_2atmpS3824;
            int32_t _M0L6_2atmpS3823;
            int32_t _M0L6_2atmpS3816;
            int32_t _M0L3valS3821;
            int32_t _M0L6_2atmpS3820;
            int32_t _M0L6_2atmpS3819;
            int32_t _M0L6_2atmpS3818;
            int32_t _M0L6_2atmpS3817;
            int32_t _M0L6_2atmpS3815;
            int32_t _M0L8_2afieldS3994;
            int32_t _M0L3valS3832;
            int32_t _M0L6_2atmpS3831;
            int32_t _M0L3valS3834;
            int32_t _M0L6_2atmpS3833;
            if (_M0L6_2atmpS3813 >= _M0L3lenS1521) {
              moonbit_decref(_M0L1cS1523);
              moonbit_decref(_M0L1iS1522);
              moonbit_decref(_M0L5bytesS1519);
              break;
            }
            _M0L3valS3830 = _M0L1cS1523->$0;
            _M0L6_2atmpS3829 = _M0L3valS3830 & 15;
            _M0L6_2atmpS3822 = _M0L6_2atmpS3829 << 12;
            _M0L3valS3828 = _M0L1iS1522->$0;
            _M0L6_2atmpS3827 = _M0L3valS3828 + 1;
            if (
              _M0L6_2atmpS3827 < 0
              || _M0L6_2atmpS3827 >= Moonbit_array_length(_M0L5bytesS1519)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3826 = _M0L5bytesS1519[_M0L6_2atmpS3827];
            _M0L6_2atmpS3825 = (int32_t)_M0L6_2atmpS3826;
            _M0L6_2atmpS3824 = _M0L6_2atmpS3825 & 63;
            _M0L6_2atmpS3823 = _M0L6_2atmpS3824 << 6;
            _M0L6_2atmpS3816 = _M0L6_2atmpS3822 | _M0L6_2atmpS3823;
            _M0L3valS3821 = _M0L1iS1522->$0;
            _M0L6_2atmpS3820 = _M0L3valS3821 + 2;
            if (
              _M0L6_2atmpS3820 < 0
              || _M0L6_2atmpS3820 >= Moonbit_array_length(_M0L5bytesS1519)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3819 = _M0L5bytesS1519[_M0L6_2atmpS3820];
            _M0L6_2atmpS3818 = (int32_t)_M0L6_2atmpS3819;
            _M0L6_2atmpS3817 = _M0L6_2atmpS3818 & 63;
            _M0L6_2atmpS3815 = _M0L6_2atmpS3816 | _M0L6_2atmpS3817;
            _M0L1cS1523->$0 = _M0L6_2atmpS3815;
            _M0L8_2afieldS3994 = _M0L1cS1523->$0;
            moonbit_decref(_M0L1cS1523);
            _M0L3valS3832 = _M0L8_2afieldS3994;
            _M0L6_2atmpS3831 = _M0L3valS3832;
            moonbit_incref(_M0L3resS1520);
            #line 231 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1520, _M0L6_2atmpS3831);
            _M0L3valS3834 = _M0L1iS1522->$0;
            _M0L6_2atmpS3833 = _M0L3valS3834 + 3;
            _M0L1iS1522->$0 = _M0L6_2atmpS3833;
          } else {
            int32_t _M0L3valS3836 = _M0L1iS1522->$0;
            int32_t _M0L6_2atmpS3835 = _M0L3valS3836 + 3;
            int32_t _M0L3valS3859;
            int32_t _M0L6_2atmpS3858;
            int32_t _M0L6_2atmpS3851;
            int32_t _M0L3valS3857;
            int32_t _M0L6_2atmpS3856;
            int32_t _M0L6_2atmpS3855;
            int32_t _M0L6_2atmpS3854;
            int32_t _M0L6_2atmpS3853;
            int32_t _M0L6_2atmpS3852;
            int32_t _M0L6_2atmpS3844;
            int32_t _M0L3valS3850;
            int32_t _M0L6_2atmpS3849;
            int32_t _M0L6_2atmpS3848;
            int32_t _M0L6_2atmpS3847;
            int32_t _M0L6_2atmpS3846;
            int32_t _M0L6_2atmpS3845;
            int32_t _M0L6_2atmpS3838;
            int32_t _M0L3valS3843;
            int32_t _M0L6_2atmpS3842;
            int32_t _M0L6_2atmpS3841;
            int32_t _M0L6_2atmpS3840;
            int32_t _M0L6_2atmpS3839;
            int32_t _M0L6_2atmpS3837;
            int32_t _M0L3valS3861;
            int32_t _M0L6_2atmpS3860;
            int32_t _M0L3valS3865;
            int32_t _M0L6_2atmpS3864;
            int32_t _M0L6_2atmpS3863;
            int32_t _M0L6_2atmpS3862;
            int32_t _M0L8_2afieldS3995;
            int32_t _M0L3valS3869;
            int32_t _M0L6_2atmpS3868;
            int32_t _M0L6_2atmpS3867;
            int32_t _M0L6_2atmpS3866;
            int32_t _M0L3valS3871;
            int32_t _M0L6_2atmpS3870;
            if (_M0L6_2atmpS3835 >= _M0L3lenS1521) {
              moonbit_decref(_M0L1cS1523);
              moonbit_decref(_M0L1iS1522);
              moonbit_decref(_M0L5bytesS1519);
              break;
            }
            _M0L3valS3859 = _M0L1cS1523->$0;
            _M0L6_2atmpS3858 = _M0L3valS3859 & 7;
            _M0L6_2atmpS3851 = _M0L6_2atmpS3858 << 18;
            _M0L3valS3857 = _M0L1iS1522->$0;
            _M0L6_2atmpS3856 = _M0L3valS3857 + 1;
            if (
              _M0L6_2atmpS3856 < 0
              || _M0L6_2atmpS3856 >= Moonbit_array_length(_M0L5bytesS1519)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3855 = _M0L5bytesS1519[_M0L6_2atmpS3856];
            _M0L6_2atmpS3854 = (int32_t)_M0L6_2atmpS3855;
            _M0L6_2atmpS3853 = _M0L6_2atmpS3854 & 63;
            _M0L6_2atmpS3852 = _M0L6_2atmpS3853 << 12;
            _M0L6_2atmpS3844 = _M0L6_2atmpS3851 | _M0L6_2atmpS3852;
            _M0L3valS3850 = _M0L1iS1522->$0;
            _M0L6_2atmpS3849 = _M0L3valS3850 + 2;
            if (
              _M0L6_2atmpS3849 < 0
              || _M0L6_2atmpS3849 >= Moonbit_array_length(_M0L5bytesS1519)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3848 = _M0L5bytesS1519[_M0L6_2atmpS3849];
            _M0L6_2atmpS3847 = (int32_t)_M0L6_2atmpS3848;
            _M0L6_2atmpS3846 = _M0L6_2atmpS3847 & 63;
            _M0L6_2atmpS3845 = _M0L6_2atmpS3846 << 6;
            _M0L6_2atmpS3838 = _M0L6_2atmpS3844 | _M0L6_2atmpS3845;
            _M0L3valS3843 = _M0L1iS1522->$0;
            _M0L6_2atmpS3842 = _M0L3valS3843 + 3;
            if (
              _M0L6_2atmpS3842 < 0
              || _M0L6_2atmpS3842 >= Moonbit_array_length(_M0L5bytesS1519)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3841 = _M0L5bytesS1519[_M0L6_2atmpS3842];
            _M0L6_2atmpS3840 = (int32_t)_M0L6_2atmpS3841;
            _M0L6_2atmpS3839 = _M0L6_2atmpS3840 & 63;
            _M0L6_2atmpS3837 = _M0L6_2atmpS3838 | _M0L6_2atmpS3839;
            _M0L1cS1523->$0 = _M0L6_2atmpS3837;
            _M0L3valS3861 = _M0L1cS1523->$0;
            _M0L6_2atmpS3860 = _M0L3valS3861 - 65536;
            _M0L1cS1523->$0 = _M0L6_2atmpS3860;
            _M0L3valS3865 = _M0L1cS1523->$0;
            _M0L6_2atmpS3864 = _M0L3valS3865 >> 10;
            _M0L6_2atmpS3863 = _M0L6_2atmpS3864 + 55296;
            _M0L6_2atmpS3862 = _M0L6_2atmpS3863;
            moonbit_incref(_M0L3resS1520);
            #line 242 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1520, _M0L6_2atmpS3862);
            _M0L8_2afieldS3995 = _M0L1cS1523->$0;
            moonbit_decref(_M0L1cS1523);
            _M0L3valS3869 = _M0L8_2afieldS3995;
            _M0L6_2atmpS3868 = _M0L3valS3869 & 1023;
            _M0L6_2atmpS3867 = _M0L6_2atmpS3868 + 56320;
            _M0L6_2atmpS3866 = _M0L6_2atmpS3867;
            moonbit_incref(_M0L3resS1520);
            #line 243 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1520, _M0L6_2atmpS3866);
            _M0L3valS3871 = _M0L1iS1522->$0;
            _M0L6_2atmpS3870 = _M0L3valS3871 + 4;
            _M0L1iS1522->$0 = _M0L6_2atmpS3870;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1522);
      moonbit_decref(_M0L5bytesS1519);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1520);
}

int32_t _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1512(
  int32_t _M0L6_2aenvS3782,
  moonbit_string_t _M0L1sS1513
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1514;
  int32_t _M0L3lenS1515;
  int32_t _M0L1iS1516;
  int32_t _M0L8_2afieldS3996;
  #line 197 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1514
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1514)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1514->$0 = 0;
  _M0L3lenS1515 = Moonbit_array_length(_M0L1sS1513);
  _M0L1iS1516 = 0;
  while (1) {
    if (_M0L1iS1516 < _M0L3lenS1515) {
      int32_t _M0L3valS3787 = _M0L3resS1514->$0;
      int32_t _M0L6_2atmpS3784 = _M0L3valS3787 * 10;
      int32_t _M0L6_2atmpS3786;
      int32_t _M0L6_2atmpS3785;
      int32_t _M0L6_2atmpS3783;
      int32_t _M0L6_2atmpS3788;
      if (
        _M0L1iS1516 < 0 || _M0L1iS1516 >= Moonbit_array_length(_M0L1sS1513)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3786 = _M0L1sS1513[_M0L1iS1516];
      _M0L6_2atmpS3785 = _M0L6_2atmpS3786 - 48;
      _M0L6_2atmpS3783 = _M0L6_2atmpS3784 + _M0L6_2atmpS3785;
      _M0L3resS1514->$0 = _M0L6_2atmpS3783;
      _M0L6_2atmpS3788 = _M0L1iS1516 + 1;
      _M0L1iS1516 = _M0L6_2atmpS3788;
      continue;
    } else {
      moonbit_decref(_M0L1sS1513);
    }
    break;
  }
  _M0L8_2afieldS3996 = _M0L3resS1514->$0;
  moonbit_decref(_M0L3resS1514);
  return _M0L8_2afieldS3996;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1492,
  moonbit_string_t _M0L12_2adiscard__S1493,
  int32_t _M0L12_2adiscard__S1494,
  struct _M0TWssbEu* _M0L12_2adiscard__S1495,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1496
) {
  struct moonbit_result_0 _result_4576;
  #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1496);
  moonbit_decref(_M0L12_2adiscard__S1495);
  moonbit_decref(_M0L12_2adiscard__S1493);
  moonbit_decref(_M0L12_2adiscard__S1492);
  _result_4576.tag = 1;
  _result_4576.data.ok = 0;
  return _result_4576;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1497,
  moonbit_string_t _M0L12_2adiscard__S1498,
  int32_t _M0L12_2adiscard__S1499,
  struct _M0TWssbEu* _M0L12_2adiscard__S1500,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1501
) {
  struct moonbit_result_0 _result_4577;
  #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1501);
  moonbit_decref(_M0L12_2adiscard__S1500);
  moonbit_decref(_M0L12_2adiscard__S1498);
  moonbit_decref(_M0L12_2adiscard__S1497);
  _result_4577.tag = 1;
  _result_4577.data.ok = 0;
  return _result_4577;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1502,
  moonbit_string_t _M0L12_2adiscard__S1503,
  int32_t _M0L12_2adiscard__S1504,
  struct _M0TWssbEu* _M0L12_2adiscard__S1505,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1506
) {
  struct moonbit_result_0 _result_4578;
  #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1506);
  moonbit_decref(_M0L12_2adiscard__S1505);
  moonbit_decref(_M0L12_2adiscard__S1503);
  moonbit_decref(_M0L12_2adiscard__S1502);
  _result_4578.tag = 1;
  _result_4578.data.ok = 0;
  return _result_4578;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal21cache__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1507,
  moonbit_string_t _M0L12_2adiscard__S1508,
  int32_t _M0L12_2adiscard__S1509,
  struct _M0TWssbEu* _M0L12_2adiscard__S1510,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1511
) {
  struct moonbit_result_0 _result_4579;
  #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1511);
  moonbit_decref(_M0L12_2adiscard__S1510);
  moonbit_decref(_M0L12_2adiscard__S1508);
  moonbit_decref(_M0L12_2adiscard__S1507);
  _result_4579.tag = 1;
  _result_4579.data.ok = 0;
  return _result_4579;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal21cache__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1491
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1491);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__6(
  
) {
  moonbit_string_t _M0L6_2atmpS3781;
  void* _M0L6_2atmpS3761;
  moonbit_string_t _M0L6_2atmpS3780;
  void* _M0L6_2atmpS3762;
  moonbit_string_t _M0L6_2atmpS3777;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3778;
  moonbit_string_t _M0L6_2atmpS3779;
  void* _M0L6_2atmpS3763;
  moonbit_string_t _M0L6_2atmpS3776;
  void* _M0L6_2atmpS3764;
  moonbit_string_t _M0L6_2atmpS3773;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3774;
  moonbit_string_t _M0L6_2atmpS3775;
  void* _M0L6_2atmpS3765;
  moonbit_string_t _M0L6_2atmpS3772;
  void* _M0L6_2atmpS3766;
  moonbit_string_t _M0L6_2atmpS3769;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3770;
  moonbit_string_t _M0L6_2atmpS3771;
  void* _M0L6_2atmpS3767;
  void* _M0L6_2atmpS3768;
  void** _M0L6_2atmpS3760;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1475;
  void** _M0L8_2afieldS3998;
  void** _M0L3bufS3758;
  int32_t _M0L8_2afieldS3997;
  int32_t _M0L6_2acntS4384;
  int32_t _M0L3lenS3759;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3757;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1476;
  struct _M0TPB6ToJson _M0L6_2atmpS3617;
  void* _M0L6_2atmpS3756;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3736;
  void* _M0L6_2atmpS3755;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3745;
  void* _M0L6_2atmpS3754;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3746;
  void* _M0L6_2atmpS3753;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3752;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1479;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3751;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3750;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3749;
  void* _M0L6_2atmpS3748;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3747;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1478;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3744;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3743;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3742;
  void* _M0L6_2atmpS3741;
  void** _M0L6_2atmpS3740;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3739;
  void* _M0L6_2atmpS3738;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3737;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1477;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3735;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3734;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3733;
  void* _M0L6_2atmpS3628;
  void* _M0L6_2atmpS3732;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3729;
  void* _M0L6_2atmpS3731;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3730;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1480;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3728;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3727;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3726;
  void* _M0L6_2atmpS3629;
  void* _M0L6_2atmpS3725;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3718;
  void** _M0L6_2atmpS3724;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3723;
  void* _M0L6_2atmpS3722;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3719;
  void* _M0L6_2atmpS3721;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3720;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1481;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3717;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3716;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3715;
  void* _M0L6_2atmpS3630;
  void* _M0L6_2atmpS3714;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3711;
  void* _M0L6_2atmpS3713;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3712;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1482;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3710;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3709;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3708;
  void* _M0L6_2atmpS3631;
  void* _M0L6_2atmpS3707;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3700;
  void** _M0L6_2atmpS3706;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3705;
  void* _M0L6_2atmpS3704;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3701;
  void* _M0L6_2atmpS3703;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3702;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1483;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3699;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3698;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3697;
  void* _M0L6_2atmpS3632;
  void* _M0L6_2atmpS3696;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3676;
  void* _M0L6_2atmpS3695;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3685;
  void* _M0L6_2atmpS3694;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3686;
  void* _M0L6_2atmpS3693;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3692;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1486;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3691;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3690;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3689;
  void* _M0L6_2atmpS3688;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3687;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1485;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3684;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3683;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3682;
  void* _M0L6_2atmpS3681;
  void** _M0L6_2atmpS3680;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3679;
  void* _M0L6_2atmpS3678;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3677;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1484;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3675;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3674;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3673;
  void* _M0L6_2atmpS3633;
  void* _M0L6_2atmpS3672;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3665;
  void** _M0L6_2atmpS3671;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3670;
  void* _M0L6_2atmpS3669;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3666;
  void* _M0L6_2atmpS3668;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3667;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1487;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3664;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3663;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3662;
  void* _M0L6_2atmpS3634;
  void* _M0L6_2atmpS3661;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3639;
  void* _M0L6_2atmpS3660;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3650;
  void* _M0L6_2atmpS3659;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3651;
  void* _M0L6_2atmpS3658;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3657;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1490;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3656;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3655;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3654;
  void* _M0L6_2atmpS3653;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3652;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1489;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3649;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3648;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3647;
  void* _M0L6_2atmpS3646;
  void** _M0L6_2atmpS3645;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3644;
  void* _M0L6_2atmpS3643;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3640;
  void* _M0L6_2atmpS3642;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3641;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1488;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3638;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3637;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3636;
  void* _M0L6_2atmpS3635;
  void** _M0L6_2atmpS3627;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3626;
  void* _M0L6_2atmpS3625;
  void* _M0L6_2atmpS3618;
  moonbit_string_t _M0L6_2atmpS3621;
  moonbit_string_t _M0L6_2atmpS3622;
  moonbit_string_t _M0L6_2atmpS3623;
  moonbit_string_t _M0L6_2atmpS3624;
  moonbit_string_t* _M0L6_2atmpS3620;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3619;
  #line 161 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3781 = 0;
  #line 163 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3761
  = _M0FP48clawteam8clawteam8internal6openai15system__messageGsE((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS3781);
  _M0L6_2atmpS3780 = 0;
  #line 164 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3762
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_10.data, _M0L6_2atmpS3780);
  _M0L6_2atmpS3777 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3778 = 0;
  _M0L6_2atmpS3779 = 0;
  #line 165 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3763
  = _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(_M0L6_2atmpS3777, _M0L6_2atmpS3778, _M0L6_2atmpS3779);
  _M0L6_2atmpS3776 = 0;
  #line 166 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3764
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3776);
  _M0L6_2atmpS3773 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3774 = 0;
  _M0L6_2atmpS3775 = 0;
  #line 167 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3765
  = _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(_M0L6_2atmpS3773, _M0L6_2atmpS3774, _M0L6_2atmpS3775);
  _M0L6_2atmpS3772 = 0;
  #line 170 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3766
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS3772);
  _M0L6_2atmpS3769 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3770 = 0;
  _M0L6_2atmpS3771 = 0;
  #line 173 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3767
  = _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(_M0L6_2atmpS3769, _M0L6_2atmpS3770, _M0L6_2atmpS3771);
  #line 174 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3768
  = _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE((moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_17.data);
  _M0L6_2atmpS3760 = (void**)moonbit_make_ref_array_raw(8);
  _M0L6_2atmpS3760[0] = _M0L6_2atmpS3761;
  _M0L6_2atmpS3760[1] = _M0L6_2atmpS3762;
  _M0L6_2atmpS3760[2] = _M0L6_2atmpS3763;
  _M0L6_2atmpS3760[3] = _M0L6_2atmpS3764;
  _M0L6_2atmpS3760[4] = _M0L6_2atmpS3765;
  _M0L6_2atmpS3760[5] = _M0L6_2atmpS3766;
  _M0L6_2atmpS3760[6] = _M0L6_2atmpS3767;
  _M0L6_2atmpS3760[7] = _M0L6_2atmpS3768;
  _M0L8messagesS1475
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1475)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1475->$0 = _M0L6_2atmpS3760;
  _M0L8messagesS1475->$1 = 8;
  _M0L8_2afieldS3998 = _M0L8messagesS1475->$0;
  _M0L3bufS3758 = _M0L8_2afieldS3998;
  _M0L8_2afieldS3997 = _M0L8messagesS1475->$1;
  _M0L6_2acntS4384 = Moonbit_object_header(_M0L8messagesS1475)->rc;
  if (_M0L6_2acntS4384 > 1) {
    int32_t _M0L11_2anew__cntS4385 = _M0L6_2acntS4384 - 1;
    Moonbit_object_header(_M0L8messagesS1475)->rc = _M0L11_2anew__cntS4385;
    moonbit_incref(_M0L3bufS3758);
  } else if (_M0L6_2acntS4384 == 1) {
    #line 176 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1475);
  }
  _M0L3lenS3759 = _M0L8_2afieldS3997;
  _M0L6_2atmpS3757
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3759, _M0L3bufS3758
  };
  #line 176 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1476
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3757);
  _M0L6_2atmpS3617
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1476
  };
  #line 179 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3756
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_18.data);
  _M0L8_2atupleS3736
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3736)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3736->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3736->$1 = _M0L6_2atmpS3756;
  #line 182 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3755
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3745
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3745)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3745->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3745->$1 = _M0L6_2atmpS3755;
  #line 183 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3754
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L8_2atupleS3746
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3746)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3746->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3746->$1 = _M0L6_2atmpS3754;
  #line 184 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3753
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3752
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3752)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3752->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3752->$1 = _M0L6_2atmpS3753;
  _M0L7_2abindS1479 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1479[0] = _M0L8_2atupleS3752;
  _M0L6_2atmpS3751 = _M0L7_2abindS1479;
  _M0L6_2atmpS3750
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3751
  };
  #line 184 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3749 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3750);
  #line 184 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3748 = _M0MPC14json4Json6object(_M0L6_2atmpS3749);
  _M0L8_2atupleS3747
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3747)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3747->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3747->$1 = _M0L6_2atmpS3748;
  _M0L7_2abindS1478 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1478[0] = _M0L8_2atupleS3745;
  _M0L7_2abindS1478[1] = _M0L8_2atupleS3746;
  _M0L7_2abindS1478[2] = _M0L8_2atupleS3747;
  _M0L6_2atmpS3744 = _M0L7_2abindS1478;
  _M0L6_2atmpS3743
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3744
  };
  #line 181 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3742 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3743);
  #line 181 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3741 = _M0MPC14json4Json6object(_M0L6_2atmpS3742);
  _M0L6_2atmpS3740 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3740[0] = _M0L6_2atmpS3741;
  _M0L6_2atmpS3739
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3739)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3739->$0 = _M0L6_2atmpS3740;
  _M0L6_2atmpS3739->$1 = 1;
  #line 180 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3738 = _M0MPC14json4Json5array(_M0L6_2atmpS3739);
  _M0L8_2atupleS3737
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3737)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3737->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3737->$1 = _M0L6_2atmpS3738;
  _M0L7_2abindS1477 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1477[0] = _M0L8_2atupleS3736;
  _M0L7_2abindS1477[1] = _M0L8_2atupleS3737;
  _M0L6_2atmpS3735 = _M0L7_2abindS1477;
  _M0L6_2atmpS3734
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3735
  };
  #line 178 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3733 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3734);
  #line 178 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3628 = _M0MPC14json4Json6object(_M0L6_2atmpS3733);
  #line 188 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3732
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3729
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3729)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3729->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3729->$1 = _M0L6_2atmpS3732;
  #line 188 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3731
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3730
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3730)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3730->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3730->$1 = _M0L6_2atmpS3731;
  _M0L7_2abindS1480 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1480[0] = _M0L8_2atupleS3729;
  _M0L7_2abindS1480[1] = _M0L8_2atupleS3730;
  _M0L6_2atmpS3728 = _M0L7_2abindS1480;
  _M0L6_2atmpS3727
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3728
  };
  #line 188 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3726 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3727);
  #line 188 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3629 = _M0MPC14json4Json6object(_M0L6_2atmpS3726);
  #line 190 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3725
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS3718
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3718)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3718->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3718->$1 = _M0L6_2atmpS3725;
  _M0L6_2atmpS3724 = (void**)moonbit_empty_ref_array;
  _M0L6_2atmpS3723
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3723)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3723->$0 = _M0L6_2atmpS3724;
  _M0L6_2atmpS3723->$1 = 0;
  #line 191 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3722 = _M0MPC14json4Json5array(_M0L6_2atmpS3723);
  _M0L8_2atupleS3719
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3719)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3719->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3719->$1 = _M0L6_2atmpS3722;
  #line 192 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3721
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L8_2atupleS3720
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3720)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3720->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3720->$1 = _M0L6_2atmpS3721;
  _M0L7_2abindS1481 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1481[0] = _M0L8_2atupleS3718;
  _M0L7_2abindS1481[1] = _M0L8_2atupleS3719;
  _M0L7_2abindS1481[2] = _M0L8_2atupleS3720;
  _M0L6_2atmpS3717 = _M0L7_2abindS1481;
  _M0L6_2atmpS3716
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3717
  };
  #line 189 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3715 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3716);
  #line 189 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3630 = _M0MPC14json4Json6object(_M0L6_2atmpS3715);
  #line 194 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3714
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3711
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3711)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3711->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3711->$1 = _M0L6_2atmpS3714;
  #line 194 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3713
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS3712
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3712)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3712->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3712->$1 = _M0L6_2atmpS3713;
  _M0L7_2abindS1482 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1482[0] = _M0L8_2atupleS3711;
  _M0L7_2abindS1482[1] = _M0L8_2atupleS3712;
  _M0L6_2atmpS3710 = _M0L7_2abindS1482;
  _M0L6_2atmpS3709
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3710
  };
  #line 194 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3708 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3709);
  #line 194 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3631 = _M0MPC14json4Json6object(_M0L6_2atmpS3708);
  #line 196 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3707
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS3700
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3700)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3700->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3700->$1 = _M0L6_2atmpS3707;
  _M0L6_2atmpS3706 = (void**)moonbit_empty_ref_array;
  _M0L6_2atmpS3705
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3705)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3705->$0 = _M0L6_2atmpS3706;
  _M0L6_2atmpS3705->$1 = 0;
  #line 197 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3704 = _M0MPC14json4Json5array(_M0L6_2atmpS3705);
  _M0L8_2atupleS3701
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3701)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3701->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3701->$1 = _M0L6_2atmpS3704;
  #line 198 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3703
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_13.data);
  _M0L8_2atupleS3702
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3702)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3702->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3702->$1 = _M0L6_2atmpS3703;
  _M0L7_2abindS1483 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1483[0] = _M0L8_2atupleS3700;
  _M0L7_2abindS1483[1] = _M0L8_2atupleS3701;
  _M0L7_2abindS1483[2] = _M0L8_2atupleS3702;
  _M0L6_2atmpS3699 = _M0L7_2abindS1483;
  _M0L6_2atmpS3698
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3699
  };
  #line 195 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3697 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3698);
  #line 195 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3632 = _M0MPC14json4Json6object(_M0L6_2atmpS3697);
  #line 201 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3696
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3676
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3676)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3676->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3676->$1 = _M0L6_2atmpS3696;
  #line 204 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3695
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3685
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3685)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3685->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3685->$1 = _M0L6_2atmpS3695;
  #line 205 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3694
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS3686
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3686)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3686->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3686->$1 = _M0L6_2atmpS3694;
  #line 206 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3693
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3692
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3692)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3692->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3692->$1 = _M0L6_2atmpS3693;
  _M0L7_2abindS1486 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1486[0] = _M0L8_2atupleS3692;
  _M0L6_2atmpS3691 = _M0L7_2abindS1486;
  _M0L6_2atmpS3690
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3691
  };
  #line 206 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3689 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3690);
  #line 206 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3688 = _M0MPC14json4Json6object(_M0L6_2atmpS3689);
  _M0L8_2atupleS3687
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3687)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3687->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3687->$1 = _M0L6_2atmpS3688;
  _M0L7_2abindS1485 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1485[0] = _M0L8_2atupleS3685;
  _M0L7_2abindS1485[1] = _M0L8_2atupleS3686;
  _M0L7_2abindS1485[2] = _M0L8_2atupleS3687;
  _M0L6_2atmpS3684 = _M0L7_2abindS1485;
  _M0L6_2atmpS3683
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3684
  };
  #line 203 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3682 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3683);
  #line 203 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3681 = _M0MPC14json4Json6object(_M0L6_2atmpS3682);
  _M0L6_2atmpS3680 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3680[0] = _M0L6_2atmpS3681;
  _M0L6_2atmpS3679
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3679)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3679->$0 = _M0L6_2atmpS3680;
  _M0L6_2atmpS3679->$1 = 1;
  #line 202 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3678 = _M0MPC14json4Json5array(_M0L6_2atmpS3679);
  _M0L8_2atupleS3677
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3677)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3677->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3677->$1 = _M0L6_2atmpS3678;
  _M0L7_2abindS1484 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1484[0] = _M0L8_2atupleS3676;
  _M0L7_2abindS1484[1] = _M0L8_2atupleS3677;
  _M0L6_2atmpS3675 = _M0L7_2abindS1484;
  _M0L6_2atmpS3674
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3675
  };
  #line 200 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3673 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3674);
  #line 200 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3633 = _M0MPC14json4Json6object(_M0L6_2atmpS3673);
  #line 211 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3672
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS3665
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3665)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3665->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3665->$1 = _M0L6_2atmpS3672;
  _M0L6_2atmpS3671 = (void**)moonbit_empty_ref_array;
  _M0L6_2atmpS3670
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3670)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3670->$0 = _M0L6_2atmpS3671;
  _M0L6_2atmpS3670->$1 = 0;
  #line 212 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3669 = _M0MPC14json4Json5array(_M0L6_2atmpS3670);
  _M0L8_2atupleS3666
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3666)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3666->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3666->$1 = _M0L6_2atmpS3669;
  #line 213 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3668
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_15.data);
  _M0L8_2atupleS3667
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3667)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3667->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3667->$1 = _M0L6_2atmpS3668;
  _M0L7_2abindS1487 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1487[0] = _M0L8_2atupleS3665;
  _M0L7_2abindS1487[1] = _M0L8_2atupleS3666;
  _M0L7_2abindS1487[2] = _M0L8_2atupleS3667;
  _M0L6_2atmpS3664 = _M0L7_2abindS1487;
  _M0L6_2atmpS3663
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3664
  };
  #line 210 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3662 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3663);
  #line 210 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3634 = _M0MPC14json4Json6object(_M0L6_2atmpS3662);
  #line 216 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3661
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_28.data);
  _M0L8_2atupleS3639
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3639)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3639->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3639->$1 = _M0L6_2atmpS3661;
  #line 219 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3660
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3650
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3650)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3650->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3650->$1 = _M0L6_2atmpS3660;
  #line 220 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3659
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_16.data);
  _M0L8_2atupleS3651
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3651)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3651->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3651->$1 = _M0L6_2atmpS3659;
  #line 221 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3658
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3657
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3657)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3657->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3657->$1 = _M0L6_2atmpS3658;
  _M0L7_2abindS1490 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1490[0] = _M0L8_2atupleS3657;
  _M0L6_2atmpS3656 = _M0L7_2abindS1490;
  _M0L6_2atmpS3655
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3656
  };
  #line 221 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3654 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3655);
  #line 221 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3653 = _M0MPC14json4Json6object(_M0L6_2atmpS3654);
  _M0L8_2atupleS3652
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3652)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3652->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3652->$1 = _M0L6_2atmpS3653;
  _M0L7_2abindS1489 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1489[0] = _M0L8_2atupleS3650;
  _M0L7_2abindS1489[1] = _M0L8_2atupleS3651;
  _M0L7_2abindS1489[2] = _M0L8_2atupleS3652;
  _M0L6_2atmpS3649 = _M0L7_2abindS1489;
  _M0L6_2atmpS3648
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3649
  };
  #line 218 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3647 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3648);
  #line 218 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3646 = _M0MPC14json4Json6object(_M0L6_2atmpS3647);
  _M0L6_2atmpS3645 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3645[0] = _M0L6_2atmpS3646;
  _M0L6_2atmpS3644
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3644)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3644->$0 = _M0L6_2atmpS3645;
  _M0L6_2atmpS3644->$1 = 1;
  #line 217 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3643 = _M0MPC14json4Json5array(_M0L6_2atmpS3644);
  _M0L8_2atupleS3640
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3640)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3640->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3640->$1 = _M0L6_2atmpS3643;
  #line 224 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3642
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_17.data);
  _M0L8_2atupleS3641
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3641)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3641->$0 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L8_2atupleS3641->$1 = _M0L6_2atmpS3642;
  _M0L7_2abindS1488 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1488[0] = _M0L8_2atupleS3639;
  _M0L7_2abindS1488[1] = _M0L8_2atupleS3640;
  _M0L7_2abindS1488[2] = _M0L8_2atupleS3641;
  _M0L6_2atmpS3638 = _M0L7_2abindS1488;
  _M0L6_2atmpS3637
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3638
  };
  #line 215 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3636 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3637);
  #line 215 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3635 = _M0MPC14json4Json6object(_M0L6_2atmpS3636);
  _M0L6_2atmpS3627 = (void**)moonbit_make_ref_array_raw(8);
  _M0L6_2atmpS3627[0] = _M0L6_2atmpS3628;
  _M0L6_2atmpS3627[1] = _M0L6_2atmpS3629;
  _M0L6_2atmpS3627[2] = _M0L6_2atmpS3630;
  _M0L6_2atmpS3627[3] = _M0L6_2atmpS3631;
  _M0L6_2atmpS3627[4] = _M0L6_2atmpS3632;
  _M0L6_2atmpS3627[5] = _M0L6_2atmpS3633;
  _M0L6_2atmpS3627[6] = _M0L6_2atmpS3634;
  _M0L6_2atmpS3627[7] = _M0L6_2atmpS3635;
  _M0L6_2atmpS3626
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3626)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3626->$0 = _M0L6_2atmpS3627;
  _M0L6_2atmpS3626->$1 = 8;
  #line 177 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3625 = _M0MPC14json4Json5array(_M0L6_2atmpS3626);
  _M0L6_2atmpS3618 = _M0L6_2atmpS3625;
  _M0L6_2atmpS3621 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3622 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3623 = 0;
  _M0L6_2atmpS3624 = 0;
  _M0L6_2atmpS3620 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3620[0] = _M0L6_2atmpS3621;
  _M0L6_2atmpS3620[1] = _M0L6_2atmpS3622;
  _M0L6_2atmpS3620[2] = _M0L6_2atmpS3623;
  _M0L6_2atmpS3620[3] = _M0L6_2atmpS3624;
  _M0L6_2atmpS3619
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3619)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3619->$0 = _M0L6_2atmpS3620;
  _M0L6_2atmpS3619->$1 = 4;
  #line 177 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3617, _M0L6_2atmpS3618, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS3619);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__5(
  
) {
  moonbit_string_t _M0L6_2atmpS3616;
  void* _M0L6_2atmpS3613;
  moonbit_string_t _M0L6_2atmpS3615;
  void* _M0L6_2atmpS3614;
  void** _M0L6_2atmpS3612;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1467;
  void** _M0L8_2afieldS4000;
  void** _M0L3bufS3610;
  int32_t _M0L8_2afieldS3999;
  int32_t _M0L6_2acntS4386;
  int32_t _M0L3lenS3611;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3609;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1468;
  struct _M0TPB6ToJson _M0L6_2atmpS3548;
  void* _M0L6_2atmpS3608;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3588;
  void* _M0L6_2atmpS3607;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3597;
  void* _M0L6_2atmpS3606;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3598;
  void* _M0L6_2atmpS3605;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3604;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1471;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3603;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3602;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3601;
  void* _M0L6_2atmpS3600;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3599;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1470;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3596;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3595;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3594;
  void* _M0L6_2atmpS3593;
  void** _M0L6_2atmpS3592;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3591;
  void* _M0L6_2atmpS3590;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3589;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1469;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3587;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3586;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3585;
  void* _M0L6_2atmpS3559;
  void* _M0L6_2atmpS3584;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3564;
  void* _M0L6_2atmpS3583;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3573;
  void* _M0L6_2atmpS3582;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3574;
  void* _M0L6_2atmpS3581;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3580;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1474;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3579;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3578;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3577;
  void* _M0L6_2atmpS3576;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3575;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1473;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3572;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3571;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3570;
  void* _M0L6_2atmpS3569;
  void** _M0L6_2atmpS3568;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3567;
  void* _M0L6_2atmpS3566;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3565;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1472;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3563;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3562;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3561;
  void* _M0L6_2atmpS3560;
  void** _M0L6_2atmpS3558;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3557;
  void* _M0L6_2atmpS3556;
  void* _M0L6_2atmpS3549;
  moonbit_string_t _M0L6_2atmpS3552;
  moonbit_string_t _M0L6_2atmpS3553;
  moonbit_string_t _M0L6_2atmpS3554;
  moonbit_string_t _M0L6_2atmpS3555;
  moonbit_string_t* _M0L6_2atmpS3551;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3550;
  #line 130 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3616 = 0;
  #line 132 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3613
  = _M0FP48clawteam8clawteam8internal6openai15system__messageGsE((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS3616);
  _M0L6_2atmpS3615 = 0;
  #line 133 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3614
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_10.data, _M0L6_2atmpS3615);
  _M0L6_2atmpS3612 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3612[0] = _M0L6_2atmpS3613;
  _M0L6_2atmpS3612[1] = _M0L6_2atmpS3614;
  _M0L8messagesS1467
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1467)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1467->$0 = _M0L6_2atmpS3612;
  _M0L8messagesS1467->$1 = 2;
  _M0L8_2afieldS4000 = _M0L8messagesS1467->$0;
  _M0L3bufS3610 = _M0L8_2afieldS4000;
  _M0L8_2afieldS3999 = _M0L8messagesS1467->$1;
  _M0L6_2acntS4386 = Moonbit_object_header(_M0L8messagesS1467)->rc;
  if (_M0L6_2acntS4386 > 1) {
    int32_t _M0L11_2anew__cntS4387 = _M0L6_2acntS4386 - 1;
    Moonbit_object_header(_M0L8messagesS1467)->rc = _M0L11_2anew__cntS4387;
    moonbit_incref(_M0L3bufS3610);
  } else if (_M0L6_2acntS4386 == 1) {
    #line 135 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1467);
  }
  _M0L3lenS3611 = _M0L8_2afieldS3999;
  _M0L6_2atmpS3609
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3611, _M0L3bufS3610
  };
  #line 135 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1468
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3609);
  _M0L6_2atmpS3548
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1468
  };
  #line 138 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3608
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_18.data);
  _M0L8_2atupleS3588
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3588)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3588->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3588->$1 = _M0L6_2atmpS3608;
  #line 141 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3607
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3597
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3597)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3597->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3597->$1 = _M0L6_2atmpS3607;
  #line 142 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3606
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L8_2atupleS3598
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3598)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3598->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3598->$1 = _M0L6_2atmpS3606;
  #line 143 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3605
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3604
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3604)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3604->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3604->$1 = _M0L6_2atmpS3605;
  _M0L7_2abindS1471 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1471[0] = _M0L8_2atupleS3604;
  _M0L6_2atmpS3603 = _M0L7_2abindS1471;
  _M0L6_2atmpS3602
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3603
  };
  #line 143 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3601 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3602);
  #line 143 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3600 = _M0MPC14json4Json6object(_M0L6_2atmpS3601);
  _M0L8_2atupleS3599
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3599->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3599->$1 = _M0L6_2atmpS3600;
  _M0L7_2abindS1470 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1470[0] = _M0L8_2atupleS3597;
  _M0L7_2abindS1470[1] = _M0L8_2atupleS3598;
  _M0L7_2abindS1470[2] = _M0L8_2atupleS3599;
  _M0L6_2atmpS3596 = _M0L7_2abindS1470;
  _M0L6_2atmpS3595
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3596
  };
  #line 140 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3594 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3595);
  #line 140 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3593 = _M0MPC14json4Json6object(_M0L6_2atmpS3594);
  _M0L6_2atmpS3592 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3592[0] = _M0L6_2atmpS3593;
  _M0L6_2atmpS3591
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3591)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3591->$0 = _M0L6_2atmpS3592;
  _M0L6_2atmpS3591->$1 = 1;
  #line 139 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3590 = _M0MPC14json4Json5array(_M0L6_2atmpS3591);
  _M0L8_2atupleS3589
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3589)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3589->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3589->$1 = _M0L6_2atmpS3590;
  _M0L7_2abindS1469 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1469[0] = _M0L8_2atupleS3588;
  _M0L7_2abindS1469[1] = _M0L8_2atupleS3589;
  _M0L6_2atmpS3587 = _M0L7_2abindS1469;
  _M0L6_2atmpS3586
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3587
  };
  #line 137 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3585 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3586);
  #line 137 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3559 = _M0MPC14json4Json6object(_M0L6_2atmpS3585);
  #line 148 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3584
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3564
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3564)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3564->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3564->$1 = _M0L6_2atmpS3584;
  #line 151 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3583
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3573
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3573)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3573->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3573->$1 = _M0L6_2atmpS3583;
  #line 152 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3582
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3574
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3574)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3574->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3574->$1 = _M0L6_2atmpS3582;
  #line 153 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3581
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3580
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3580)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3580->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3580->$1 = _M0L6_2atmpS3581;
  _M0L7_2abindS1474 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1474[0] = _M0L8_2atupleS3580;
  _M0L6_2atmpS3579 = _M0L7_2abindS1474;
  _M0L6_2atmpS3578
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3579
  };
  #line 153 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3577 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3578);
  #line 153 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3576 = _M0MPC14json4Json6object(_M0L6_2atmpS3577);
  _M0L8_2atupleS3575
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3575)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3575->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3575->$1 = _M0L6_2atmpS3576;
  _M0L7_2abindS1473 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1473[0] = _M0L8_2atupleS3573;
  _M0L7_2abindS1473[1] = _M0L8_2atupleS3574;
  _M0L7_2abindS1473[2] = _M0L8_2atupleS3575;
  _M0L6_2atmpS3572 = _M0L7_2abindS1473;
  _M0L6_2atmpS3571
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3572
  };
  #line 150 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3570 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3571);
  #line 150 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3569 = _M0MPC14json4Json6object(_M0L6_2atmpS3570);
  _M0L6_2atmpS3568 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3568[0] = _M0L6_2atmpS3569;
  _M0L6_2atmpS3567
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3567)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3567->$0 = _M0L6_2atmpS3568;
  _M0L6_2atmpS3567->$1 = 1;
  #line 149 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3566 = _M0MPC14json4Json5array(_M0L6_2atmpS3567);
  _M0L8_2atupleS3565
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3565)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3565->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3565->$1 = _M0L6_2atmpS3566;
  _M0L7_2abindS1472 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1472[0] = _M0L8_2atupleS3564;
  _M0L7_2abindS1472[1] = _M0L8_2atupleS3565;
  _M0L6_2atmpS3563 = _M0L7_2abindS1472;
  _M0L6_2atmpS3562
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3563
  };
  #line 147 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3561 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3562);
  #line 147 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3560 = _M0MPC14json4Json6object(_M0L6_2atmpS3561);
  _M0L6_2atmpS3558 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3558[0] = _M0L6_2atmpS3559;
  _M0L6_2atmpS3558[1] = _M0L6_2atmpS3560;
  _M0L6_2atmpS3557
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3557)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3557->$0 = _M0L6_2atmpS3558;
  _M0L6_2atmpS3557->$1 = 2;
  #line 136 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3556 = _M0MPC14json4Json5array(_M0L6_2atmpS3557);
  _M0L6_2atmpS3549 = _M0L6_2atmpS3556;
  _M0L6_2atmpS3552 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3553 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3554 = 0;
  _M0L6_2atmpS3555 = 0;
  _M0L6_2atmpS3551 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3551[0] = _M0L6_2atmpS3552;
  _M0L6_2atmpS3551[1] = _M0L6_2atmpS3553;
  _M0L6_2atmpS3551[2] = _M0L6_2atmpS3554;
  _M0L6_2atmpS3551[3] = _M0L6_2atmpS3555;
  _M0L6_2atmpS3550
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3550)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3550->$0 = _M0L6_2atmpS3551;
  _M0L6_2atmpS3550->$1 = 4;
  #line 136 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3548, _M0L6_2atmpS3549, (moonbit_string_t)moonbit_string_literal_35.data, _M0L6_2atmpS3550);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__4(
  
) {
  void* _M0L6_2atmpS3547;
  void** _M0L6_2atmpS3546;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1462;
  void** _M0L8_2afieldS4002;
  void** _M0L3bufS3544;
  int32_t _M0L8_2afieldS4001;
  int32_t _M0L6_2acntS4388;
  int32_t _M0L3lenS3545;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3543;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1463;
  struct _M0TPB6ToJson _M0L6_2atmpS3505;
  void* _M0L6_2atmpS3542;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3520;
  void* _M0L6_2atmpS3541;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3531;
  void* _M0L6_2atmpS3540;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3532;
  void* _M0L6_2atmpS3539;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3538;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1466;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3537;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3536;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3535;
  void* _M0L6_2atmpS3534;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3533;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1465;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3530;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3529;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3528;
  void* _M0L6_2atmpS3527;
  void** _M0L6_2atmpS3526;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3525;
  void* _M0L6_2atmpS3524;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3521;
  void* _M0L6_2atmpS3523;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3522;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1464;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3519;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3518;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3517;
  void* _M0L6_2atmpS3516;
  void** _M0L6_2atmpS3515;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3514;
  void* _M0L6_2atmpS3513;
  void* _M0L6_2atmpS3506;
  moonbit_string_t _M0L6_2atmpS3509;
  moonbit_string_t _M0L6_2atmpS3510;
  moonbit_string_t _M0L6_2atmpS3511;
  moonbit_string_t _M0L6_2atmpS3512;
  moonbit_string_t* _M0L6_2atmpS3508;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3507;
  #line 109 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  #line 111 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3547
  = _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE((moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_17.data);
  _M0L6_2atmpS3546 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3546[0] = _M0L6_2atmpS3547;
  _M0L8messagesS1462
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1462)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1462->$0 = _M0L6_2atmpS3546;
  _M0L8messagesS1462->$1 = 1;
  _M0L8_2afieldS4002 = _M0L8messagesS1462->$0;
  _M0L3bufS3544 = _M0L8_2afieldS4002;
  _M0L8_2afieldS4001 = _M0L8messagesS1462->$1;
  _M0L6_2acntS4388 = Moonbit_object_header(_M0L8messagesS1462)->rc;
  if (_M0L6_2acntS4388 > 1) {
    int32_t _M0L11_2anew__cntS4389 = _M0L6_2acntS4388 - 1;
    Moonbit_object_header(_M0L8messagesS1462)->rc = _M0L11_2anew__cntS4389;
    moonbit_incref(_M0L3bufS3544);
  } else if (_M0L6_2acntS4388 == 1) {
    #line 113 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1462);
  }
  _M0L3lenS3545 = _M0L8_2afieldS4001;
  _M0L6_2atmpS3543
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3545, _M0L3bufS3544
  };
  #line 113 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1463
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3543);
  _M0L6_2atmpS3505
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1463
  };
  #line 116 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3542
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_28.data);
  _M0L8_2atupleS3520
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3520)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3520->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3520->$1 = _M0L6_2atmpS3542;
  #line 119 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3541
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3531
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3531)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3531->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3531->$1 = _M0L6_2atmpS3541;
  #line 120 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3540
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_16.data);
  _M0L8_2atupleS3532
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3532)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3532->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3532->$1 = _M0L6_2atmpS3540;
  #line 121 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3539
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3538
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3538)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3538->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3538->$1 = _M0L6_2atmpS3539;
  _M0L7_2abindS1466 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1466[0] = _M0L8_2atupleS3538;
  _M0L6_2atmpS3537 = _M0L7_2abindS1466;
  _M0L6_2atmpS3536
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3537
  };
  #line 121 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3535 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3536);
  #line 121 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3534 = _M0MPC14json4Json6object(_M0L6_2atmpS3535);
  _M0L8_2atupleS3533
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3533)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3533->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3533->$1 = _M0L6_2atmpS3534;
  _M0L7_2abindS1465 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1465[0] = _M0L8_2atupleS3531;
  _M0L7_2abindS1465[1] = _M0L8_2atupleS3532;
  _M0L7_2abindS1465[2] = _M0L8_2atupleS3533;
  _M0L6_2atmpS3530 = _M0L7_2abindS1465;
  _M0L6_2atmpS3529
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3530
  };
  #line 118 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3528 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3529);
  #line 118 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3527 = _M0MPC14json4Json6object(_M0L6_2atmpS3528);
  _M0L6_2atmpS3526 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3526[0] = _M0L6_2atmpS3527;
  _M0L6_2atmpS3525
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3525)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3525->$0 = _M0L6_2atmpS3526;
  _M0L6_2atmpS3525->$1 = 1;
  #line 117 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3524 = _M0MPC14json4Json5array(_M0L6_2atmpS3525);
  _M0L8_2atupleS3521
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3521)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3521->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3521->$1 = _M0L6_2atmpS3524;
  #line 124 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3523
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_17.data);
  _M0L8_2atupleS3522
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3522)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3522->$0 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L8_2atupleS3522->$1 = _M0L6_2atmpS3523;
  _M0L7_2abindS1464 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1464[0] = _M0L8_2atupleS3520;
  _M0L7_2abindS1464[1] = _M0L8_2atupleS3521;
  _M0L7_2abindS1464[2] = _M0L8_2atupleS3522;
  _M0L6_2atmpS3519 = _M0L7_2abindS1464;
  _M0L6_2atmpS3518
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3519
  };
  #line 115 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3517 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3518);
  #line 115 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3516 = _M0MPC14json4Json6object(_M0L6_2atmpS3517);
  _M0L6_2atmpS3515 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3515[0] = _M0L6_2atmpS3516;
  _M0L6_2atmpS3514
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3514)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3514->$0 = _M0L6_2atmpS3515;
  _M0L6_2atmpS3514->$1 = 1;
  #line 114 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3513 = _M0MPC14json4Json5array(_M0L6_2atmpS3514);
  _M0L6_2atmpS3506 = _M0L6_2atmpS3513;
  _M0L6_2atmpS3509 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3510 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3511 = 0;
  _M0L6_2atmpS3512 = 0;
  _M0L6_2atmpS3508 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3508[0] = _M0L6_2atmpS3509;
  _M0L6_2atmpS3508[1] = _M0L6_2atmpS3510;
  _M0L6_2atmpS3508[2] = _M0L6_2atmpS3511;
  _M0L6_2atmpS3508[3] = _M0L6_2atmpS3512;
  _M0L6_2atmpS3507
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3507)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3507->$0 = _M0L6_2atmpS3508;
  _M0L6_2atmpS3507->$1 = 4;
  #line 114 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3505, _M0L6_2atmpS3506, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS3507);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__3(
  
) {
  moonbit_string_t _M0L6_2atmpS3504;
  void* _M0L6_2atmpS3503;
  void** _M0L6_2atmpS3502;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1457;
  void** _M0L8_2afieldS4004;
  void** _M0L3bufS3500;
  int32_t _M0L8_2afieldS4003;
  int32_t _M0L6_2acntS4390;
  int32_t _M0L3lenS3501;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3499;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1458;
  struct _M0TPB6ToJson _M0L6_2atmpS3463;
  void* _M0L6_2atmpS3498;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3478;
  void* _M0L6_2atmpS3497;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3487;
  void* _M0L6_2atmpS3496;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3488;
  void* _M0L6_2atmpS3495;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3494;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1461;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3493;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3492;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3491;
  void* _M0L6_2atmpS3490;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3489;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1460;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3486;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3485;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3484;
  void* _M0L6_2atmpS3483;
  void** _M0L6_2atmpS3482;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3481;
  void* _M0L6_2atmpS3480;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3479;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1459;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3477;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3476;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3475;
  void* _M0L6_2atmpS3474;
  void** _M0L6_2atmpS3473;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3472;
  void* _M0L6_2atmpS3471;
  void* _M0L6_2atmpS3464;
  moonbit_string_t _M0L6_2atmpS3467;
  moonbit_string_t _M0L6_2atmpS3468;
  moonbit_string_t _M0L6_2atmpS3469;
  moonbit_string_t _M0L6_2atmpS3470;
  moonbit_string_t* _M0L6_2atmpS3466;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3465;
  #line 91 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3504 = 0;
  #line 92 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3503
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_10.data, _M0L6_2atmpS3504);
  _M0L6_2atmpS3502 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3502[0] = _M0L6_2atmpS3503;
  _M0L8messagesS1457
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1457)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1457->$0 = _M0L6_2atmpS3502;
  _M0L8messagesS1457->$1 = 1;
  _M0L8_2afieldS4004 = _M0L8messagesS1457->$0;
  _M0L3bufS3500 = _M0L8_2afieldS4004;
  _M0L8_2afieldS4003 = _M0L8messagesS1457->$1;
  _M0L6_2acntS4390 = Moonbit_object_header(_M0L8messagesS1457)->rc;
  if (_M0L6_2acntS4390 > 1) {
    int32_t _M0L11_2anew__cntS4391 = _M0L6_2acntS4390 - 1;
    Moonbit_object_header(_M0L8messagesS1457)->rc = _M0L11_2anew__cntS4391;
    moonbit_incref(_M0L3bufS3500);
  } else if (_M0L6_2acntS4390 == 1) {
    #line 93 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1457);
  }
  _M0L3lenS3501 = _M0L8_2afieldS4003;
  _M0L6_2atmpS3499
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3501, _M0L3bufS3500
  };
  #line 93 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1458
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3499);
  _M0L6_2atmpS3463
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1458
  };
  #line 96 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3498
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3478
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3478)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3478->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3478->$1 = _M0L6_2atmpS3498;
  #line 99 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3497
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3487
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3487)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3487->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3487->$1 = _M0L6_2atmpS3497;
  #line 100 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3496
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3488
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3488)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3488->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3488->$1 = _M0L6_2atmpS3496;
  #line 101 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3495
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3494
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3494)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3494->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3494->$1 = _M0L6_2atmpS3495;
  _M0L7_2abindS1461 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1461[0] = _M0L8_2atupleS3494;
  _M0L6_2atmpS3493 = _M0L7_2abindS1461;
  _M0L6_2atmpS3492
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3493
  };
  #line 101 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3491 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3492);
  #line 101 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3490 = _M0MPC14json4Json6object(_M0L6_2atmpS3491);
  _M0L8_2atupleS3489
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3489)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3489->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3489->$1 = _M0L6_2atmpS3490;
  _M0L7_2abindS1460 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1460[0] = _M0L8_2atupleS3487;
  _M0L7_2abindS1460[1] = _M0L8_2atupleS3488;
  _M0L7_2abindS1460[2] = _M0L8_2atupleS3489;
  _M0L6_2atmpS3486 = _M0L7_2abindS1460;
  _M0L6_2atmpS3485
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3486
  };
  #line 98 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3484 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3485);
  #line 98 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3483 = _M0MPC14json4Json6object(_M0L6_2atmpS3484);
  _M0L6_2atmpS3482 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3482[0] = _M0L6_2atmpS3483;
  _M0L6_2atmpS3481
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3481)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3481->$0 = _M0L6_2atmpS3482;
  _M0L6_2atmpS3481->$1 = 1;
  #line 97 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3480 = _M0MPC14json4Json5array(_M0L6_2atmpS3481);
  _M0L8_2atupleS3479
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3479)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3479->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3479->$1 = _M0L6_2atmpS3480;
  _M0L7_2abindS1459 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1459[0] = _M0L8_2atupleS3478;
  _M0L7_2abindS1459[1] = _M0L8_2atupleS3479;
  _M0L6_2atmpS3477 = _M0L7_2abindS1459;
  _M0L6_2atmpS3476
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3477
  };
  #line 95 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3475 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3476);
  #line 95 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3474 = _M0MPC14json4Json6object(_M0L6_2atmpS3475);
  _M0L6_2atmpS3473 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3473[0] = _M0L6_2atmpS3474;
  _M0L6_2atmpS3472
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3472)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3472->$0 = _M0L6_2atmpS3473;
  _M0L6_2atmpS3472->$1 = 1;
  #line 94 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3471 = _M0MPC14json4Json5array(_M0L6_2atmpS3472);
  _M0L6_2atmpS3464 = _M0L6_2atmpS3471;
  _M0L6_2atmpS3467 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3468 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3469 = 0;
  _M0L6_2atmpS3470 = 0;
  _M0L6_2atmpS3466 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3466[0] = _M0L6_2atmpS3467;
  _M0L6_2atmpS3466[1] = _M0L6_2atmpS3468;
  _M0L6_2atmpS3466[2] = _M0L6_2atmpS3469;
  _M0L6_2atmpS3466[3] = _M0L6_2atmpS3470;
  _M0L6_2atmpS3465
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3465)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3465->$0 = _M0L6_2atmpS3466;
  _M0L6_2atmpS3465->$1 = 4;
  #line 94 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3463, _M0L6_2atmpS3464, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS3465);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__2(
  
) {
  moonbit_string_t _M0L6_2atmpS3462;
  void* _M0L6_2atmpS3459;
  moonbit_string_t _M0L6_2atmpS3461;
  void* _M0L6_2atmpS3460;
  void** _M0L6_2atmpS3458;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1449;
  void** _M0L8_2afieldS4006;
  void** _M0L3bufS3456;
  int32_t _M0L8_2afieldS4005;
  int32_t _M0L6_2acntS4392;
  int32_t _M0L3lenS3457;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3455;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1450;
  struct _M0TPB6ToJson _M0L6_2atmpS3394;
  void* _M0L6_2atmpS3454;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3434;
  void* _M0L6_2atmpS3453;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3443;
  void* _M0L6_2atmpS3452;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3444;
  void* _M0L6_2atmpS3451;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3450;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1453;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3449;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3448;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3447;
  void* _M0L6_2atmpS3446;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3445;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1452;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3442;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3441;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3440;
  void* _M0L6_2atmpS3439;
  void** _M0L6_2atmpS3438;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3437;
  void* _M0L6_2atmpS3436;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3435;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1451;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3433;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3432;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3431;
  void* _M0L6_2atmpS3405;
  void* _M0L6_2atmpS3430;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3410;
  void* _M0L6_2atmpS3429;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3419;
  void* _M0L6_2atmpS3428;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3420;
  void* _M0L6_2atmpS3427;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3426;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1456;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3425;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3424;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3423;
  void* _M0L6_2atmpS3422;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3421;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1455;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3418;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3417;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3416;
  void* _M0L6_2atmpS3415;
  void** _M0L6_2atmpS3414;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3413;
  void* _M0L6_2atmpS3412;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3411;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1454;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3409;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3408;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3407;
  void* _M0L6_2atmpS3406;
  void** _M0L6_2atmpS3404;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3403;
  void* _M0L6_2atmpS3402;
  void* _M0L6_2atmpS3395;
  moonbit_string_t _M0L6_2atmpS3398;
  moonbit_string_t _M0L6_2atmpS3399;
  moonbit_string_t _M0L6_2atmpS3400;
  moonbit_string_t _M0L6_2atmpS3401;
  moonbit_string_t* _M0L6_2atmpS3397;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3396;
  #line 60 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3462 = 0;
  #line 62 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3459
  = _M0FP48clawteam8clawteam8internal6openai15system__messageGsE((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS3462);
  _M0L6_2atmpS3461 = 0;
  #line 63 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3460
  = _M0FP48clawteam8clawteam8internal6openai15system__messageGsE((moonbit_string_t)moonbit_string_literal_42.data, _M0L6_2atmpS3461);
  _M0L6_2atmpS3458 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3458[0] = _M0L6_2atmpS3459;
  _M0L6_2atmpS3458[1] = _M0L6_2atmpS3460;
  _M0L8messagesS1449
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1449)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1449->$0 = _M0L6_2atmpS3458;
  _M0L8messagesS1449->$1 = 2;
  _M0L8_2afieldS4006 = _M0L8messagesS1449->$0;
  _M0L3bufS3456 = _M0L8_2afieldS4006;
  _M0L8_2afieldS4005 = _M0L8messagesS1449->$1;
  _M0L6_2acntS4392 = Moonbit_object_header(_M0L8messagesS1449)->rc;
  if (_M0L6_2acntS4392 > 1) {
    int32_t _M0L11_2anew__cntS4393 = _M0L6_2acntS4392 - 1;
    Moonbit_object_header(_M0L8messagesS1449)->rc = _M0L11_2anew__cntS4393;
    moonbit_incref(_M0L3bufS3456);
  } else if (_M0L6_2acntS4392 == 1) {
    #line 65 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1449);
  }
  _M0L3lenS3457 = _M0L8_2afieldS4005;
  _M0L6_2atmpS3455
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3457, _M0L3bufS3456
  };
  #line 65 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1450
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3455);
  _M0L6_2atmpS3394
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1450
  };
  #line 68 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3454
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_18.data);
  _M0L8_2atupleS3434
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3434)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3434->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3434->$1 = _M0L6_2atmpS3454;
  #line 71 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3453
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3443
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3443)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3443->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3443->$1 = _M0L6_2atmpS3453;
  #line 72 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3452
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L8_2atupleS3444
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3444)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3444->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3444->$1 = _M0L6_2atmpS3452;
  #line 73 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3451
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3450
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3450)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3450->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3450->$1 = _M0L6_2atmpS3451;
  _M0L7_2abindS1453 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1453[0] = _M0L8_2atupleS3450;
  _M0L6_2atmpS3449 = _M0L7_2abindS1453;
  _M0L6_2atmpS3448
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3449
  };
  #line 73 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3447 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3448);
  #line 73 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3446 = _M0MPC14json4Json6object(_M0L6_2atmpS3447);
  _M0L8_2atupleS3445
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3445)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3445->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3445->$1 = _M0L6_2atmpS3446;
  _M0L7_2abindS1452 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1452[0] = _M0L8_2atupleS3443;
  _M0L7_2abindS1452[1] = _M0L8_2atupleS3444;
  _M0L7_2abindS1452[2] = _M0L8_2atupleS3445;
  _M0L6_2atmpS3442 = _M0L7_2abindS1452;
  _M0L6_2atmpS3441
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3442
  };
  #line 70 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3440 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3441);
  #line 70 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3439 = _M0MPC14json4Json6object(_M0L6_2atmpS3440);
  _M0L6_2atmpS3438 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3438[0] = _M0L6_2atmpS3439;
  _M0L6_2atmpS3437
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3437)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3437->$0 = _M0L6_2atmpS3438;
  _M0L6_2atmpS3437->$1 = 1;
  #line 69 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3436 = _M0MPC14json4Json5array(_M0L6_2atmpS3437);
  _M0L8_2atupleS3435
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3435)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3435->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3435->$1 = _M0L6_2atmpS3436;
  _M0L7_2abindS1451 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1451[0] = _M0L8_2atupleS3434;
  _M0L7_2abindS1451[1] = _M0L8_2atupleS3435;
  _M0L6_2atmpS3433 = _M0L7_2abindS1451;
  _M0L6_2atmpS3432
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3433
  };
  #line 67 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3431 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3432);
  #line 67 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3405 = _M0MPC14json4Json6object(_M0L6_2atmpS3431);
  #line 78 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3430
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_18.data);
  _M0L8_2atupleS3410
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3410)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3410->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3410->$1 = _M0L6_2atmpS3430;
  #line 81 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3429
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3419
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3419)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3419->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3419->$1 = _M0L6_2atmpS3429;
  #line 82 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3428
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_42.data);
  _M0L8_2atupleS3420
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3420)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3420->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3420->$1 = _M0L6_2atmpS3428;
  #line 83 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3427
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3426
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3426)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3426->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3426->$1 = _M0L6_2atmpS3427;
  _M0L7_2abindS1456 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1456[0] = _M0L8_2atupleS3426;
  _M0L6_2atmpS3425 = _M0L7_2abindS1456;
  _M0L6_2atmpS3424
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3425
  };
  #line 83 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3423 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3424);
  #line 83 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3422 = _M0MPC14json4Json6object(_M0L6_2atmpS3423);
  _M0L8_2atupleS3421
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3421)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3421->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3421->$1 = _M0L6_2atmpS3422;
  _M0L7_2abindS1455 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1455[0] = _M0L8_2atupleS3419;
  _M0L7_2abindS1455[1] = _M0L8_2atupleS3420;
  _M0L7_2abindS1455[2] = _M0L8_2atupleS3421;
  _M0L6_2atmpS3418 = _M0L7_2abindS1455;
  _M0L6_2atmpS3417
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3418
  };
  #line 80 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3416 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3417);
  #line 80 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3415 = _M0MPC14json4Json6object(_M0L6_2atmpS3416);
  _M0L6_2atmpS3414 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3414[0] = _M0L6_2atmpS3415;
  _M0L6_2atmpS3413
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3413)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3413->$0 = _M0L6_2atmpS3414;
  _M0L6_2atmpS3413->$1 = 1;
  #line 79 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3412 = _M0MPC14json4Json5array(_M0L6_2atmpS3413);
  _M0L8_2atupleS3411
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3411)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3411->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3411->$1 = _M0L6_2atmpS3412;
  _M0L7_2abindS1454 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1454[0] = _M0L8_2atupleS3410;
  _M0L7_2abindS1454[1] = _M0L8_2atupleS3411;
  _M0L6_2atmpS3409 = _M0L7_2abindS1454;
  _M0L6_2atmpS3408
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3409
  };
  #line 77 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3407 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3408);
  #line 77 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3406 = _M0MPC14json4Json6object(_M0L6_2atmpS3407);
  _M0L6_2atmpS3404 = (void**)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS3404[0] = _M0L6_2atmpS3405;
  _M0L6_2atmpS3404[1] = _M0L6_2atmpS3406;
  _M0L6_2atmpS3403
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3403)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3403->$0 = _M0L6_2atmpS3404;
  _M0L6_2atmpS3403->$1 = 2;
  #line 66 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3402 = _M0MPC14json4Json5array(_M0L6_2atmpS3403);
  _M0L6_2atmpS3395 = _M0L6_2atmpS3402;
  _M0L6_2atmpS3398 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3399 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3400 = 0;
  _M0L6_2atmpS3401 = 0;
  _M0L6_2atmpS3397 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3397[0] = _M0L6_2atmpS3398;
  _M0L6_2atmpS3397[1] = _M0L6_2atmpS3399;
  _M0L6_2atmpS3397[2] = _M0L6_2atmpS3400;
  _M0L6_2atmpS3397[3] = _M0L6_2atmpS3401;
  _M0L6_2atmpS3396
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3396)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3396->$0 = _M0L6_2atmpS3397;
  _M0L6_2atmpS3396->$1 = 4;
  #line 66 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3394, _M0L6_2atmpS3395, (moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS3396);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__1(
  
) {
  moonbit_string_t _M0L6_2atmpS3393;
  void* _M0L6_2atmpS3392;
  void** _M0L6_2atmpS3391;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1444;
  void** _M0L8_2afieldS4008;
  void** _M0L3bufS3389;
  int32_t _M0L8_2afieldS4007;
  int32_t _M0L6_2acntS4394;
  int32_t _M0L3lenS3390;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3388;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1445;
  struct _M0TPB6ToJson _M0L6_2atmpS3352;
  void* _M0L6_2atmpS3387;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3367;
  void* _M0L6_2atmpS3386;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3376;
  void* _M0L6_2atmpS3385;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3377;
  void* _M0L6_2atmpS3384;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3383;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1448;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3382;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3381;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3380;
  void* _M0L6_2atmpS3379;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3378;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1447;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3375;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3374;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3373;
  void* _M0L6_2atmpS3372;
  void** _M0L6_2atmpS3371;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3370;
  void* _M0L6_2atmpS3369;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3368;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1446;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3366;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3365;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3364;
  void* _M0L6_2atmpS3363;
  void** _M0L6_2atmpS3362;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3361;
  void* _M0L6_2atmpS3360;
  void* _M0L6_2atmpS3353;
  moonbit_string_t _M0L6_2atmpS3356;
  moonbit_string_t _M0L6_2atmpS3357;
  moonbit_string_t _M0L6_2atmpS3358;
  moonbit_string_t _M0L6_2atmpS3359;
  moonbit_string_t* _M0L6_2atmpS3355;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3354;
  #line 40 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3393 = 0;
  #line 42 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3392
  = _M0FP48clawteam8clawteam8internal6openai15system__messageGsE((moonbit_string_t)moonbit_string_literal_9.data, _M0L6_2atmpS3393);
  _M0L6_2atmpS3391 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3391[0] = _M0L6_2atmpS3392;
  _M0L8messagesS1444
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1444)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1444->$0 = _M0L6_2atmpS3391;
  _M0L8messagesS1444->$1 = 1;
  _M0L8_2afieldS4008 = _M0L8messagesS1444->$0;
  _M0L3bufS3389 = _M0L8_2afieldS4008;
  _M0L8_2afieldS4007 = _M0L8messagesS1444->$1;
  _M0L6_2acntS4394 = Moonbit_object_header(_M0L8messagesS1444)->rc;
  if (_M0L6_2acntS4394 > 1) {
    int32_t _M0L11_2anew__cntS4395 = _M0L6_2acntS4394 - 1;
    Moonbit_object_header(_M0L8messagesS1444)->rc = _M0L11_2anew__cntS4395;
    moonbit_incref(_M0L3bufS3389);
  } else if (_M0L6_2acntS4394 == 1) {
    #line 44 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1444);
  }
  _M0L3lenS3390 = _M0L8_2afieldS4007;
  _M0L6_2atmpS3388
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3390, _M0L3bufS3389
  };
  #line 44 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1445
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3388);
  _M0L6_2atmpS3352
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1445
  };
  #line 47 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3387
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_18.data);
  _M0L8_2atupleS3367
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3367)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3367->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3367->$1 = _M0L6_2atmpS3387;
  #line 50 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3386
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3376
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3376)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3376->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3376->$1 = _M0L6_2atmpS3386;
  #line 51 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3385
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_9.data);
  _M0L8_2atupleS3377
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3377)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3377->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3377->$1 = _M0L6_2atmpS3385;
  #line 52 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3384
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3383
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3383)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3383->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3383->$1 = _M0L6_2atmpS3384;
  _M0L7_2abindS1448 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1448[0] = _M0L8_2atupleS3383;
  _M0L6_2atmpS3382 = _M0L7_2abindS1448;
  _M0L6_2atmpS3381
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3382
  };
  #line 52 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3380 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3381);
  #line 52 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3379 = _M0MPC14json4Json6object(_M0L6_2atmpS3380);
  _M0L8_2atupleS3378
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3378)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3378->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3378->$1 = _M0L6_2atmpS3379;
  _M0L7_2abindS1447 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1447[0] = _M0L8_2atupleS3376;
  _M0L7_2abindS1447[1] = _M0L8_2atupleS3377;
  _M0L7_2abindS1447[2] = _M0L8_2atupleS3378;
  _M0L6_2atmpS3375 = _M0L7_2abindS1447;
  _M0L6_2atmpS3374
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3375
  };
  #line 49 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3373 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3374);
  #line 49 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3372 = _M0MPC14json4Json6object(_M0L6_2atmpS3373);
  _M0L6_2atmpS3371 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3371[0] = _M0L6_2atmpS3372;
  _M0L6_2atmpS3370
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3370)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3370->$0 = _M0L6_2atmpS3371;
  _M0L6_2atmpS3370->$1 = 1;
  #line 48 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3369 = _M0MPC14json4Json5array(_M0L6_2atmpS3370);
  _M0L8_2atupleS3368
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3368)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3368->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3368->$1 = _M0L6_2atmpS3369;
  _M0L7_2abindS1446 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1446[0] = _M0L8_2atupleS3367;
  _M0L7_2abindS1446[1] = _M0L8_2atupleS3368;
  _M0L6_2atmpS3366 = _M0L7_2abindS1446;
  _M0L6_2atmpS3365
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3366
  };
  #line 46 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3364 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3365);
  #line 46 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3363 = _M0MPC14json4Json6object(_M0L6_2atmpS3364);
  _M0L6_2atmpS3362 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3362[0] = _M0L6_2atmpS3363;
  _M0L6_2atmpS3361
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3361)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3361->$0 = _M0L6_2atmpS3362;
  _M0L6_2atmpS3361->$1 = 1;
  #line 45 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3360 = _M0MPC14json4Json5array(_M0L6_2atmpS3361);
  _M0L6_2atmpS3353 = _M0L6_2atmpS3360;
  _M0L6_2atmpS3356 = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3357 = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3358 = 0;
  _M0L6_2atmpS3359 = 0;
  _M0L6_2atmpS3355 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3355[0] = _M0L6_2atmpS3356;
  _M0L6_2atmpS3355[1] = _M0L6_2atmpS3357;
  _M0L6_2atmpS3355[2] = _M0L6_2atmpS3358;
  _M0L6_2atmpS3355[3] = _M0L6_2atmpS3359;
  _M0L6_2atmpS3354
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3354)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3354->$0 = _M0L6_2atmpS3355;
  _M0L6_2atmpS3354->$1 = 4;
  #line 45 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3352, _M0L6_2atmpS3353, (moonbit_string_t)moonbit_string_literal_48.data, _M0L6_2atmpS3354);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal21cache__blackbox__test41____test__63616368655f746573742e6d6274__0(
  
) {
  moonbit_string_t _M0L6_2atmpS3351;
  void* _M0L6_2atmpS3340;
  moonbit_string_t _M0L6_2atmpS3348;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3349;
  moonbit_string_t _M0L6_2atmpS3350;
  void* _M0L6_2atmpS3341;
  moonbit_string_t _M0L6_2atmpS3347;
  void* _M0L6_2atmpS3342;
  moonbit_string_t _M0L6_2atmpS3344;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3345;
  moonbit_string_t _M0L6_2atmpS3346;
  void* _M0L6_2atmpS3343;
  void** _M0L6_2atmpS3339;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L8messagesS1435;
  void** _M0L8_2afieldS4010;
  void** _M0L3bufS3337;
  int32_t _M0L8_2afieldS4009;
  int32_t _M0L6_2acntS4396;
  int32_t _M0L3lenS3338;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L6_2atmpS3336;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1436;
  struct _M0TPB6ToJson _M0L6_2atmpS3263;
  void* _M0L6_2atmpS3335;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3315;
  void* _M0L6_2atmpS3334;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3324;
  void* _M0L6_2atmpS3333;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3325;
  void* _M0L6_2atmpS3332;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3331;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1439;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3330;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3329;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3328;
  void* _M0L6_2atmpS3327;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3326;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1438;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3323;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3322;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3321;
  void* _M0L6_2atmpS3320;
  void** _M0L6_2atmpS3319;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3318;
  void* _M0L6_2atmpS3317;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3316;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1437;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3314;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3313;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3312;
  void* _M0L6_2atmpS3274;
  void* _M0L6_2atmpS3311;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3304;
  void** _M0L6_2atmpS3310;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3309;
  void* _M0L6_2atmpS3308;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3305;
  void* _M0L6_2atmpS3307;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3306;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1440;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3303;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3302;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3301;
  void* _M0L6_2atmpS3275;
  void* _M0L6_2atmpS3300;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3280;
  void* _M0L6_2atmpS3299;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3289;
  void* _M0L6_2atmpS3298;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3290;
  void* _M0L6_2atmpS3297;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3296;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1443;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3295;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3294;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3293;
  void* _M0L6_2atmpS3292;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3291;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1442;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3288;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3287;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3286;
  void* _M0L6_2atmpS3285;
  void** _M0L6_2atmpS3284;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3283;
  void* _M0L6_2atmpS3282;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3281;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1441;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3279;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3278;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3277;
  void* _M0L6_2atmpS3276;
  void** _M0L6_2atmpS3273;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3272;
  void* _M0L6_2atmpS3271;
  void* _M0L6_2atmpS3264;
  moonbit_string_t _M0L6_2atmpS3267;
  moonbit_string_t _M0L6_2atmpS3268;
  moonbit_string_t _M0L6_2atmpS3269;
  moonbit_string_t _M0L6_2atmpS3270;
  moonbit_string_t* _M0L6_2atmpS3266;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3265;
  #line 2 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3351 = 0;
  #line 4 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3340
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_10.data, _M0L6_2atmpS3351);
  _M0L6_2atmpS3348 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3349 = 0;
  _M0L6_2atmpS3350 = 0;
  #line 5 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3341
  = _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(_M0L6_2atmpS3348, _M0L6_2atmpS3349, _M0L6_2atmpS3350);
  _M0L6_2atmpS3347 = 0;
  #line 6 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3342
  = _M0FP48clawteam8clawteam8internal6openai13user__messageGsE((moonbit_string_t)moonbit_string_literal_49.data, _M0L6_2atmpS3347);
  _M0L6_2atmpS3344 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3345 = 0;
  _M0L6_2atmpS3346 = 0;
  #line 7 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3343
  = _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(_M0L6_2atmpS3344, _M0L6_2atmpS3345, _M0L6_2atmpS3346);
  _M0L6_2atmpS3339 = (void**)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3339[0] = _M0L6_2atmpS3340;
  _M0L6_2atmpS3339[1] = _M0L6_2atmpS3341;
  _M0L6_2atmpS3339[2] = _M0L6_2atmpS3342;
  _M0L6_2atmpS3339[3] = _M0L6_2atmpS3343;
  _M0L8messagesS1435
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L8messagesS1435)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L8messagesS1435->$0 = _M0L6_2atmpS3339;
  _M0L8messagesS1435->$1 = 4;
  _M0L8_2afieldS4010 = _M0L8messagesS1435->$0;
  _M0L3bufS3337 = _M0L8_2afieldS4010;
  _M0L8_2afieldS4009 = _M0L8messagesS1435->$1;
  _M0L6_2acntS4396 = Moonbit_object_header(_M0L8messagesS1435)->rc;
  if (_M0L6_2acntS4396 > 1) {
    int32_t _M0L11_2anew__cntS4397 = _M0L6_2acntS4396 - 1;
    Moonbit_object_header(_M0L8messagesS1435)->rc = _M0L11_2anew__cntS4397;
    moonbit_incref(_M0L3bufS3337);
  } else if (_M0L6_2acntS4396 == 1) {
    #line 9 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
    moonbit_free(_M0L8messagesS1435);
  }
  _M0L3lenS3338 = _M0L8_2afieldS4009;
  _M0L6_2atmpS3336
  = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
    0, _M0L3lenS3338, _M0L3bufS3337
  };
  #line 9 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6cachedS1436
  = _M0FP48clawteam8clawteam8internal5cache15cache__messages(_M0L6_2atmpS3336);
  _M0L6_2atmpS3263
  = (struct _M0TPB6ToJson){
    _M0FP0185moonbitlang_2fcore_2fbuiltin_2fArray_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageParam_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6cachedS1436
  };
  #line 12 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3335
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3315
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3315)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3315->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3315->$1 = _M0L6_2atmpS3335;
  #line 15 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3334
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3324
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3324)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3324->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3324->$1 = _M0L6_2atmpS3334;
  #line 16 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3333
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3325
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3325)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3325->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3325->$1 = _M0L6_2atmpS3333;
  #line 17 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3332
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3331
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3331)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3331->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3331->$1 = _M0L6_2atmpS3332;
  _M0L7_2abindS1439 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1439[0] = _M0L8_2atupleS3331;
  _M0L6_2atmpS3330 = _M0L7_2abindS1439;
  _M0L6_2atmpS3329
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3330
  };
  #line 17 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3328 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3329);
  #line 17 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3327 = _M0MPC14json4Json6object(_M0L6_2atmpS3328);
  _M0L8_2atupleS3326
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3326)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3326->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3326->$1 = _M0L6_2atmpS3327;
  _M0L7_2abindS1438 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1438[0] = _M0L8_2atupleS3324;
  _M0L7_2abindS1438[1] = _M0L8_2atupleS3325;
  _M0L7_2abindS1438[2] = _M0L8_2atupleS3326;
  _M0L6_2atmpS3323 = _M0L7_2abindS1438;
  _M0L6_2atmpS3322
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3323
  };
  #line 14 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3321 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3322);
  #line 14 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3320 = _M0MPC14json4Json6object(_M0L6_2atmpS3321);
  _M0L6_2atmpS3319 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3319[0] = _M0L6_2atmpS3320;
  _M0L6_2atmpS3318
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3318)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3318->$0 = _M0L6_2atmpS3319;
  _M0L6_2atmpS3318->$1 = 1;
  #line 13 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3317 = _M0MPC14json4Json5array(_M0L6_2atmpS3318);
  _M0L8_2atupleS3316
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3316)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3316->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3316->$1 = _M0L6_2atmpS3317;
  _M0L7_2abindS1437 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1437[0] = _M0L8_2atupleS3315;
  _M0L7_2abindS1437[1] = _M0L8_2atupleS3316;
  _M0L6_2atmpS3314 = _M0L7_2abindS1437;
  _M0L6_2atmpS3313
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3314
  };
  #line 11 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3312 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3313);
  #line 11 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3274 = _M0MPC14json4Json6object(_M0L6_2atmpS3312);
  #line 22 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3311
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS3304
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3304)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3304->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3304->$1 = _M0L6_2atmpS3311;
  _M0L6_2atmpS3310 = (void**)moonbit_empty_ref_array;
  _M0L6_2atmpS3309
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3309)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3309->$0 = _M0L6_2atmpS3310;
  _M0L6_2atmpS3309->$1 = 0;
  #line 23 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3308 = _M0MPC14json4Json5array(_M0L6_2atmpS3309);
  _M0L8_2atupleS3305
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3305)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3305->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3305->$1 = _M0L6_2atmpS3308;
  #line 24 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3307
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L8_2atupleS3306
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3306)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3306->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3306->$1 = _M0L6_2atmpS3307;
  _M0L7_2abindS1440 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1440[0] = _M0L8_2atupleS3304;
  _M0L7_2abindS1440[1] = _M0L8_2atupleS3305;
  _M0L7_2abindS1440[2] = _M0L8_2atupleS3306;
  _M0L6_2atmpS3303 = _M0L7_2abindS1440;
  _M0L6_2atmpS3302
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3303
  };
  #line 21 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3301 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3302);
  #line 21 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3275 = _M0MPC14json4Json6object(_M0L6_2atmpS3301);
  #line 27 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3300
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3280
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3280)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3280->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3280->$1 = _M0L6_2atmpS3300;
  #line 30 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3299
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3289
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3289)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3289->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3289->$1 = _M0L6_2atmpS3299;
  #line 31 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3298
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_49.data);
  _M0L8_2atupleS3290
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3290)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3290->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3290->$1 = _M0L6_2atmpS3298;
  #line 32 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3297
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3296
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3296)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3296->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3296->$1 = _M0L6_2atmpS3297;
  _M0L7_2abindS1443 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1443[0] = _M0L8_2atupleS3296;
  _M0L6_2atmpS3295 = _M0L7_2abindS1443;
  _M0L6_2atmpS3294
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3295
  };
  #line 32 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3293 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3294);
  #line 32 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3292 = _M0MPC14json4Json6object(_M0L6_2atmpS3293);
  _M0L8_2atupleS3291
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3291)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3291->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3291->$1 = _M0L6_2atmpS3292;
  _M0L7_2abindS1442 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1442[0] = _M0L8_2atupleS3289;
  _M0L7_2abindS1442[1] = _M0L8_2atupleS3290;
  _M0L7_2abindS1442[2] = _M0L8_2atupleS3291;
  _M0L6_2atmpS3288 = _M0L7_2abindS1442;
  _M0L6_2atmpS3287
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3288
  };
  #line 29 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3286 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3287);
  #line 29 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3285 = _M0MPC14json4Json6object(_M0L6_2atmpS3286);
  _M0L6_2atmpS3284 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3284[0] = _M0L6_2atmpS3285;
  _M0L6_2atmpS3283
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3283)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3283->$0 = _M0L6_2atmpS3284;
  _M0L6_2atmpS3283->$1 = 1;
  #line 28 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3282 = _M0MPC14json4Json5array(_M0L6_2atmpS3283);
  _M0L8_2atupleS3281
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3281)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3281->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3281->$1 = _M0L6_2atmpS3282;
  _M0L7_2abindS1441 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1441[0] = _M0L8_2atupleS3280;
  _M0L7_2abindS1441[1] = _M0L8_2atupleS3281;
  _M0L6_2atmpS3279 = _M0L7_2abindS1441;
  _M0L6_2atmpS3278
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3279
  };
  #line 26 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3277 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3278);
  #line 26 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3276 = _M0MPC14json4Json6object(_M0L6_2atmpS3277);
  _M0L6_2atmpS3273 = (void**)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS3273[0] = _M0L6_2atmpS3274;
  _M0L6_2atmpS3273[1] = _M0L6_2atmpS3275;
  _M0L6_2atmpS3273[2] = _M0L6_2atmpS3276;
  _M0L6_2atmpS3272
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3272)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3272->$0 = _M0L6_2atmpS3273;
  _M0L6_2atmpS3272->$1 = 3;
  #line 10 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  _M0L6_2atmpS3271 = _M0MPC14json4Json5array(_M0L6_2atmpS3272);
  _M0L6_2atmpS3264 = _M0L6_2atmpS3271;
  _M0L6_2atmpS3267 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3268 = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3269 = 0;
  _M0L6_2atmpS3270 = 0;
  _M0L6_2atmpS3266 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3266[0] = _M0L6_2atmpS3267;
  _M0L6_2atmpS3266[1] = _M0L6_2atmpS3268;
  _M0L6_2atmpS3266[2] = _M0L6_2atmpS3269;
  _M0L6_2atmpS3266[3] = _M0L6_2atmpS3270;
  _M0L6_2atmpS3265
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3265)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3265->$0 = _M0L6_2atmpS3266;
  _M0L6_2atmpS3265->$1 = 4;
  #line 10 "E:\\moonbit\\clawteam\\internal\\cache\\cache_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3263, _M0L6_2atmpS3264, (moonbit_string_t)moonbit_string_literal_53.data, _M0L6_2atmpS3265);
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0FP48clawteam8clawteam8internal5cache15cache__messages(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L8messagesS1405
) {
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6_2atmpS3262;
  struct _M0TPC13ref3RefGORP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParamE* _M0L12last__systemS1387;
  void** _M0L6_2atmpS3261;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L6cachedS1388;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L8messagesS1389;
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L8messagesS1390;
  int32_t* _M0L6_2atmpS3246;
  struct _M0TPB5ArrayGiE* _M0L19user__tool__indicesS1406;
  int32_t _M0L7_2abindS1407;
  int32_t _M0L7_2abindS1408;
  int32_t _M0L1iS1409;
  int32_t _M0L12second__lastS1415;
  int32_t _M0L4lastS1416;
  int32_t _M0L4lastS1424;
  int32_t _M0L3lenS3230;
  int32_t _M0L7_2abindS1425;
  int32_t _M0L1iS1426;
  void* _M0L6_2atmpS3229;
  void* _M0L6_2atmpS3228;
  int32_t _M0L7_2abindS1417;
  int32_t _M0L1iS1418;
  void* _M0L6_2atmpS3221;
  void* _M0L6_2atmpS3220;
  int32_t _M0L7_2abindS1420;
  int32_t _M0L1iS1421;
  void* _M0L6_2atmpS3225;
  void* _M0L6_2atmpS3224;
  #line 46 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3262 = 0;
  _M0L12last__systemS1387
  = (struct _M0TPC13ref3RefGORP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParamE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParamE));
  Moonbit_object_header(_M0L12last__systemS1387)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParamE, $0) >> 2, 1, 0);
  _M0L12last__systemS1387->$0 = _M0L6_2atmpS3262;
  _M0L6_2atmpS3261 = (void**)moonbit_empty_ref_array;
  _M0L6cachedS1388
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE));
  Moonbit_object_header(_M0L6cachedS1388)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE, $0) >> 2, 1, 0);
  _M0L6cachedS1388->$0 = _M0L6_2atmpS3261;
  _M0L6cachedS1388->$1 = 0;
  _M0L8messagesS1390 = _M0L8messagesS1405;
  while (1) {
    struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6systemS1392;
    struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2afieldS4027 =
      _M0L12last__systemS1387->$0;
    struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L7_2abindS1393 =
      _M0L8_2afieldS4027;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4026;
    int32_t _M0L6_2acntS4398;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3250;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3248;
    moonbit_string_t _M0L6_2atmpS3249;
    void* _M0L6_2atmpS3247;
    struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L8messagesS1399;
    struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6systemS1400;
    int32_t _M0L3endS3253;
    int32_t _M0L5startS3254;
    int32_t _M0L6_2atmpS3252;
    struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6_2atmpS3251;
    struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6_2aoldS4020;
    struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _tmp_4584;
    if (_M0L7_2abindS1393 == 0) {
      
    } else {
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L7_2aSomeS1394 =
        _M0L7_2abindS1393;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L9_2asystemS1395 =
        _M0L7_2aSomeS1394;
      moonbit_incref(_M0L9_2asystemS1395);
      _M0L6systemS1392 = _M0L9_2asystemS1395;
      goto join_1391;
    }
    goto joinlet_4581;
    join_1391:;
    _M0L8_2afieldS4026 = _M0L6systemS1392->$0;
    _M0L6_2acntS4398 = Moonbit_object_header(_M0L6systemS1392)->rc;
    if (_M0L6_2acntS4398 > 1) {
      int32_t _M0L11_2anew__cntS4400 = _M0L6_2acntS4398 - 1;
      Moonbit_object_header(_M0L6systemS1392)->rc = _M0L11_2anew__cntS4400;
      moonbit_incref(_M0L8_2afieldS4026);
    } else if (_M0L6_2acntS4398 == 1) {
      moonbit_string_t _M0L8_2afieldS4399 = _M0L6systemS1392->$1;
      if (_M0L8_2afieldS4399) {
        moonbit_decref(_M0L8_2afieldS4399);
      }
      #line 54 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      moonbit_free(_M0L6systemS1392);
    }
    _M0L7contentS3250 = _M0L8_2afieldS4026;
    #line 54 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    _M0L6_2atmpS3248
    = _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(_M0L7contentS3250);
    _M0L6_2atmpS3249 = 0;
    #line 54 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    _M0L6_2atmpS3247
    = _M0FP48clawteam8clawteam8internal6openai15system__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS3248, _M0L6_2atmpS3249);
    moonbit_incref(_M0L6cachedS1388);
    #line 53 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3247);
    joinlet_4581:;
    _M0L3endS3253 = _M0L8messagesS1390.$2;
    _M0L5startS3254 = _M0L8messagesS1390.$1;
    _M0L6_2atmpS3252 = _M0L3endS3253 - _M0L5startS3254;
    if (_M0L6_2atmpS3252 >= 1) {
      void** _M0L8_2afieldS4025 = _M0L8messagesS1390.$0;
      void** _M0L3bufS3259 = _M0L8_2afieldS4025;
      int32_t _M0L5startS3260 = _M0L8messagesS1390.$1;
      void* _M0L6_2atmpS4024 = (void*)_M0L3bufS3259[_M0L5startS3260];
      void* _M0L4_2axS1401 = _M0L6_2atmpS4024;
      switch (Moonbit_object_tag(_M0L4_2axS1401)) {
        case 0: {
          struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System* _M0L9_2aSystemS1402 =
            (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_M0L4_2axS1401;
          struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2afieldS4023 =
            _M0L9_2aSystemS1402->$0;
          struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L9_2asystemS1403 =
            _M0L8_2afieldS4023;
          void** _M0L8_2afieldS4022 = _M0L8messagesS1390.$0;
          void** _M0L3bufS3255 = _M0L8_2afieldS4022;
          int32_t _M0L5startS3258 = _M0L8messagesS1390.$1;
          int32_t _M0L6_2atmpS3256 = 1 + _M0L5startS3258;
          int32_t _M0L8_2afieldS4021 = _M0L8messagesS1390.$2;
          int32_t _M0L3endS3257;
          struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L4_2axS1404;
          moonbit_incref(_M0L9_2asystemS1403);
          _M0L3endS3257 = _M0L8_2afieldS4021;
          _M0L4_2axS1404
          = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE){
            _M0L6_2atmpS3256, _M0L3endS3257, _M0L3bufS3255
          };
          _M0L8messagesS1399 = _M0L4_2axS1404;
          _M0L6systemS1400 = _M0L9_2asystemS1403;
          goto join_1398;
          break;
        }
        default: {
          moonbit_decref(_M0L12last__systemS1387);
          goto join_1396;
          break;
        }
      }
    } else {
      moonbit_decref(_M0L12last__systemS1387);
      goto join_1396;
    }
    goto joinlet_4583;
    join_1398:;
    _M0L6_2atmpS3251 = _M0L6systemS1400;
    _M0L6_2aoldS4020 = _M0L12last__systemS1387->$0;
    if (_M0L6_2aoldS4020) {
      moonbit_decref(_M0L6_2aoldS4020);
    }
    _M0L12last__systemS1387->$0 = _M0L6_2atmpS3251;
    _M0L8messagesS1390 = _M0L8messagesS1399;
    continue;
    joinlet_4583:;
    goto joinlet_4582;
    join_1396:;
    _M0L8messagesS1389 = _M0L8messagesS1390;
    break;
    joinlet_4582:;
    _tmp_4584 = _M0L8messagesS1390;
    _M0L8messagesS1390 = _tmp_4584;
    continue;
    break;
  }
  _M0L6_2atmpS3246 = (int32_t*)moonbit_empty_int32_array;
  _M0L19user__tool__indicesS1406
  = (struct _M0TPB5ArrayGiE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGiE));
  Moonbit_object_header(_M0L19user__tool__indicesS1406)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGiE, $0) >> 2, 1, 0);
  _M0L19user__tool__indicesS1406->$0 = _M0L6_2atmpS3246;
  _M0L19user__tool__indicesS1406->$1 = 0;
  _M0L7_2abindS1407 = 0;
  moonbit_incref(_M0L8messagesS1389.$0);
  #line 67 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L7_2abindS1408
  = _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389);
  _M0L1iS1409 = _M0L7_2abindS1407;
  while (1) {
    if (_M0L1iS1409 < _M0L7_2abindS1408) {
      void* _M0L7_2abindS1413;
      int32_t _M0L6_2atmpS3217;
      moonbit_incref(_M0L8messagesS1389.$0);
      #line 68 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L7_2abindS1413
      = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L1iS1409);
      switch (Moonbit_object_tag(_M0L7_2abindS1413)) {
        case 1: {
          moonbit_decref(_M0L7_2abindS1413);
          goto join_1412;
          break;
        }
        
        case 3: {
          moonbit_decref(_M0L7_2abindS1413);
          goto join_1412;
          break;
        }
        default: {
          moonbit_decref(_M0L7_2abindS1413);
          goto join_1410;
          break;
        }
      }
      goto joinlet_4587;
      join_1412:;
      moonbit_incref(_M0L19user__tool__indicesS1406);
      #line 69 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0MPC15array5Array4pushGiE(_M0L19user__tool__indicesS1406, _M0L1iS1409);
      joinlet_4587:;
      goto join_1410;
      goto joinlet_4586;
      join_1410:;
      _M0L6_2atmpS3217 = _M0L1iS1409 + 1;
      _M0L1iS1409 = _M0L6_2atmpS3217;
      continue;
      joinlet_4586:;
    }
    break;
  }
  _M0L3lenS3230 = _M0L19user__tool__indicesS1406->$1;
  if (_M0L3lenS3230 == 0) {
    int32_t _M0L3endS3235;
    int32_t _M0L5startS3236;
    int32_t _M0L7_2abindS1428;
    int32_t _M0L2__S1429;
    moonbit_decref(_M0L19user__tool__indicesS1406);
    _M0L3endS3235 = _M0L8messagesS1389.$2;
    _M0L5startS3236 = _M0L8messagesS1389.$1;
    _M0L7_2abindS1428 = _M0L3endS3235 - _M0L5startS3236;
    _M0L2__S1429 = 0;
    while (1) {
      if (_M0L2__S1429 < _M0L7_2abindS1428) {
        void** _M0L8_2afieldS4012 = _M0L8messagesS1389.$0;
        void** _M0L3bufS3232 = _M0L8_2afieldS4012;
        int32_t _M0L5startS3234 = _M0L8messagesS1389.$1;
        int32_t _M0L6_2atmpS3233 = _M0L5startS3234 + _M0L2__S1429;
        void* _M0L6_2atmpS4011 = (void*)_M0L3bufS3232[_M0L6_2atmpS3233];
        void* _M0L7messageS1430 = _M0L6_2atmpS4011;
        int32_t _M0L6_2atmpS3231;
        moonbit_incref(_M0L7messageS1430);
        moonbit_incref(_M0L6cachedS1388);
        #line 76 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L7messageS1430);
        _M0L6_2atmpS3231 = _M0L2__S1429 + 1;
        _M0L2__S1429 = _M0L6_2atmpS3231;
        continue;
      } else {
        moonbit_decref(_M0L8messagesS1389.$0);
      }
      break;
    }
  } else {
    int32_t _M0L3lenS3237 = _M0L19user__tool__indicesS1406->$1;
    if (_M0L3lenS3237 == 1) {
      int32_t* _M0L8_2afieldS4014 = _M0L19user__tool__indicesS1406->$0;
      int32_t _M0L6_2acntS4401 =
        Moonbit_object_header(_M0L19user__tool__indicesS1406)->rc;
      int32_t* _M0L3bufS3238;
      int32_t _M0L6_2atmpS4013;
      int32_t _M0L7_2alastS1432;
      if (_M0L6_2acntS4401 > 1) {
        int32_t _M0L11_2anew__cntS4402 = _M0L6_2acntS4401 - 1;
        Moonbit_object_header(_M0L19user__tool__indicesS1406)->rc
        = _M0L11_2anew__cntS4402;
        moonbit_incref(_M0L8_2afieldS4014);
      } else if (_M0L6_2acntS4401 == 1) {
        #line 73 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L19user__tool__indicesS1406);
      }
      _M0L3bufS3238 = _M0L8_2afieldS4014;
      _M0L6_2atmpS4013 = (int32_t)_M0L3bufS3238[0];
      moonbit_decref(_M0L3bufS3238);
      _M0L7_2alastS1432 = _M0L6_2atmpS4013;
      _M0L4lastS1424 = _M0L7_2alastS1432;
      goto join_1423;
    } else {
      int32_t* _M0L8_2afieldS4019 = _M0L19user__tool__indicesS1406->$0;
      int32_t* _M0L3bufS3242 = _M0L8_2afieldS4019;
      int32_t _M0L3lenS3245 = _M0L19user__tool__indicesS1406->$1;
      int32_t _M0L6_2atmpS3244 = _M0L3lenS3245 - 1;
      int32_t _M0L6_2atmpS3243 = _M0L6_2atmpS3244 - 1;
      int32_t _M0L6_2atmpS4018 = (int32_t)_M0L3bufS3242[_M0L6_2atmpS3243];
      int32_t _M0L15_2asecond__lastS1433 = _M0L6_2atmpS4018;
      int32_t* _M0L8_2afieldS4017 = _M0L19user__tool__indicesS1406->$0;
      int32_t* _M0L3bufS3239 = _M0L8_2afieldS4017;
      int32_t _M0L8_2afieldS4016 = _M0L19user__tool__indicesS1406->$1;
      int32_t _M0L6_2acntS4403 =
        Moonbit_object_header(_M0L19user__tool__indicesS1406)->rc;
      int32_t _M0L3lenS3241;
      int32_t _M0L6_2atmpS3240;
      int32_t _M0L6_2atmpS4015;
      int32_t _M0L7_2alastS1434;
      if (_M0L6_2acntS4403 > 1) {
        int32_t _M0L11_2anew__cntS4404 = _M0L6_2acntS4403 - 1;
        Moonbit_object_header(_M0L19user__tool__indicesS1406)->rc
        = _M0L11_2anew__cntS4404;
        moonbit_incref(_M0L3bufS3239);
      } else if (_M0L6_2acntS4403 == 1) {
        #line 73 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L19user__tool__indicesS1406);
      }
      _M0L3lenS3241 = _M0L8_2afieldS4016;
      _M0L6_2atmpS3240 = _M0L3lenS3241 - 1;
      _M0L6_2atmpS4015 = (int32_t)_M0L3bufS3239[_M0L6_2atmpS3240];
      moonbit_decref(_M0L3bufS3239);
      _M0L7_2alastS1434 = _M0L6_2atmpS4015;
      _M0L12second__lastS1415 = _M0L15_2asecond__lastS1433;
      _M0L4lastS1416 = _M0L7_2alastS1434;
      goto join_1414;
    }
  }
  goto joinlet_4589;
  join_1423:;
  _M0L7_2abindS1425 = 0;
  _M0L1iS1426 = _M0L7_2abindS1425;
  while (1) {
    if (_M0L1iS1426 < _M0L4lastS1424) {
      void* _M0L6_2atmpS3226;
      int32_t _M0L6_2atmpS3227;
      moonbit_incref(_M0L8messagesS1389.$0);
      #line 80 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L6_2atmpS3226
      = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L1iS1426);
      moonbit_incref(_M0L6cachedS1388);
      #line 80 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3226);
      _M0L6_2atmpS3227 = _M0L1iS1426 + 1;
      _M0L1iS1426 = _M0L6_2atmpS3227;
      continue;
    }
    break;
  }
  #line 82 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3229
  = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L4lastS1424);
  #line 82 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3228
  = _M0FP48clawteam8clawteam8internal5cache14cache__message(_M0L6_2atmpS3229);
  moonbit_incref(_M0L6cachedS1388);
  #line 82 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3228);
  joinlet_4589:;
  goto joinlet_4588;
  join_1414:;
  _M0L7_2abindS1417 = 0;
  _M0L1iS1418 = _M0L7_2abindS1417;
  while (1) {
    if (_M0L1iS1418 < _M0L12second__lastS1415) {
      void* _M0L6_2atmpS3218;
      int32_t _M0L6_2atmpS3219;
      moonbit_incref(_M0L8messagesS1389.$0);
      #line 86 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L6_2atmpS3218
      = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L1iS1418);
      moonbit_incref(_M0L6cachedS1388);
      #line 86 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3218);
      _M0L6_2atmpS3219 = _M0L1iS1418 + 1;
      _M0L1iS1418 = _M0L6_2atmpS3219;
      continue;
    }
    break;
  }
  moonbit_incref(_M0L8messagesS1389.$0);
  #line 88 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3221
  = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L12second__lastS1415);
  #line 88 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3220
  = _M0FP48clawteam8clawteam8internal5cache14cache__message(_M0L6_2atmpS3221);
  moonbit_incref(_M0L6cachedS1388);
  #line 88 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3220);
  _M0L7_2abindS1420 = _M0L12second__lastS1415 + 1;
  _M0L1iS1421 = _M0L7_2abindS1420;
  while (1) {
    if (_M0L1iS1421 < _M0L4lastS1416) {
      void* _M0L6_2atmpS3222;
      int32_t _M0L6_2atmpS3223;
      moonbit_incref(_M0L8messagesS1389.$0);
      #line 90 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L6_2atmpS3222
      = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L1iS1421);
      moonbit_incref(_M0L6cachedS1388);
      #line 90 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3222);
      _M0L6_2atmpS3223 = _M0L1iS1421 + 1;
      _M0L1iS1421 = _M0L6_2atmpS3223;
      continue;
    }
    break;
  }
  #line 92 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3225
  = _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8messagesS1389, _M0L4lastS1416);
  #line 92 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3224
  = _M0FP48clawteam8clawteam8internal5cache14cache__message(_M0L6_2atmpS3225);
  moonbit_incref(_M0L6cachedS1388);
  #line 92 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6cachedS1388, _M0L6_2atmpS3224);
  joinlet_4588:;
  return _M0L6cachedS1388;
}

void* _M0FP48clawteam8clawteam8internal5cache14cache__message(
  void* _M0L7messageS1378
) {
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L4toolS1371;
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L9assistantS1373;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L4userS1375;
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6systemS1377;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4032;
  int32_t _M0L6_2acntS4415;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3216;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3214;
  moonbit_string_t _M0L6_2atmpS3215;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4031;
  int32_t _M0L6_2acntS4412;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3213;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3211;
  moonbit_string_t _M0L6_2atmpS3212;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4030;
  int32_t _M0L6_2acntS4408;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3210;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3209;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3206;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3207;
  moonbit_string_t _M0L6_2atmpS3208;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4029;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3205;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3203;
  moonbit_string_t _M0L8_2afieldS4028;
  int32_t _M0L6_2acntS4405;
  moonbit_string_t _M0L14tool__call__idS3204;
  #line 27 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  switch (Moonbit_object_tag(_M0L7messageS1378)) {
    case 0: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System* _M0L9_2aSystemS1379 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_M0L7messageS1378;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2afieldS4033 =
        _M0L9_2aSystemS1379->$0;
      int32_t _M0L6_2acntS4418 =
        Moonbit_object_header(_M0L9_2aSystemS1379)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L9_2asystemS1380;
      if (_M0L6_2acntS4418 > 1) {
        int32_t _M0L11_2anew__cntS4419 = _M0L6_2acntS4418 - 1;
        Moonbit_object_header(_M0L9_2aSystemS1379)->rc
        = _M0L11_2anew__cntS4419;
        moonbit_incref(_M0L8_2afieldS4033);
      } else if (_M0L6_2acntS4418 == 1) {
        #line 30 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L9_2aSystemS1379);
      }
      _M0L9_2asystemS1380 = _M0L8_2afieldS4033;
      _M0L6systemS1377 = _M0L9_2asystemS1380;
      goto join_1376;
      break;
    }
    
    case 1: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User* _M0L7_2aUserS1381 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User*)_M0L7messageS1378;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L8_2afieldS4034 =
        _M0L7_2aUserS1381->$0;
      int32_t _M0L6_2acntS4420 = Moonbit_object_header(_M0L7_2aUserS1381)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L7_2auserS1382;
      if (_M0L6_2acntS4420 > 1) {
        int32_t _M0L11_2anew__cntS4421 = _M0L6_2acntS4420 - 1;
        Moonbit_object_header(_M0L7_2aUserS1381)->rc = _M0L11_2anew__cntS4421;
        moonbit_incref(_M0L8_2afieldS4034);
      } else if (_M0L6_2acntS4420 == 1) {
        #line 30 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L7_2aUserS1381);
      }
      _M0L7_2auserS1382 = _M0L8_2afieldS4034;
      _M0L4userS1375 = _M0L7_2auserS1382;
      goto join_1374;
      break;
    }
    
    case 2: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant* _M0L12_2aAssistantS1383 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant*)_M0L7messageS1378;
      struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L8_2afieldS4035 =
        _M0L12_2aAssistantS1383->$0;
      int32_t _M0L6_2acntS4422 =
        Moonbit_object_header(_M0L12_2aAssistantS1383)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L12_2aassistantS1384;
      if (_M0L6_2acntS4422 > 1) {
        int32_t _M0L11_2anew__cntS4423 = _M0L6_2acntS4422 - 1;
        Moonbit_object_header(_M0L12_2aAssistantS1383)->rc
        = _M0L11_2anew__cntS4423;
        moonbit_incref(_M0L8_2afieldS4035);
      } else if (_M0L6_2acntS4422 == 1) {
        #line 30 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L12_2aAssistantS1383);
      }
      _M0L12_2aassistantS1384 = _M0L8_2afieldS4035;
      _M0L9assistantS1373 = _M0L12_2aassistantS1384;
      goto join_1372;
      break;
    }
    default: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool* _M0L7_2aToolS1385 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool*)_M0L7messageS1378;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L8_2afieldS4036 =
        _M0L7_2aToolS1385->$0;
      int32_t _M0L6_2acntS4424 = Moonbit_object_header(_M0L7_2aToolS1385)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L7_2atoolS1386;
      if (_M0L6_2acntS4424 > 1) {
        int32_t _M0L11_2anew__cntS4425 = _M0L6_2acntS4424 - 1;
        Moonbit_object_header(_M0L7_2aToolS1385)->rc = _M0L11_2anew__cntS4425;
        moonbit_incref(_M0L8_2afieldS4036);
      } else if (_M0L6_2acntS4424 == 1) {
        #line 30 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L7_2aToolS1385);
      }
      _M0L7_2atoolS1386 = _M0L8_2afieldS4036;
      _M0L4toolS1371 = _M0L7_2atoolS1386;
      goto join_1370;
      break;
    }
  }
  join_1376:;
  _M0L8_2afieldS4032 = _M0L6systemS1377->$0;
  _M0L6_2acntS4415 = Moonbit_object_header(_M0L6systemS1377)->rc;
  if (_M0L6_2acntS4415 > 1) {
    int32_t _M0L11_2anew__cntS4417 = _M0L6_2acntS4415 - 1;
    Moonbit_object_header(_M0L6systemS1377)->rc = _M0L11_2anew__cntS4417;
    moonbit_incref(_M0L8_2afieldS4032);
  } else if (_M0L6_2acntS4415 == 1) {
    moonbit_string_t _M0L8_2afieldS4416 = _M0L6systemS1377->$1;
    if (_M0L8_2afieldS4416) {
      moonbit_decref(_M0L8_2afieldS4416);
    }
    #line 32 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    moonbit_free(_M0L6systemS1377);
  }
  _M0L7contentS3216 = _M0L8_2afieldS4032;
  #line 32 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3214
  = _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(_M0L7contentS3216);
  _M0L6_2atmpS3215 = 0;
  #line 32 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  return _M0FP48clawteam8clawteam8internal6openai15system__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS3214, _M0L6_2atmpS3215);
  join_1374:;
  _M0L8_2afieldS4031 = _M0L4userS1375->$0;
  _M0L6_2acntS4412 = Moonbit_object_header(_M0L4userS1375)->rc;
  if (_M0L6_2acntS4412 > 1) {
    int32_t _M0L11_2anew__cntS4414 = _M0L6_2acntS4412 - 1;
    Moonbit_object_header(_M0L4userS1375)->rc = _M0L11_2anew__cntS4414;
    moonbit_incref(_M0L8_2afieldS4031);
  } else if (_M0L6_2acntS4412 == 1) {
    moonbit_string_t _M0L8_2afieldS4413 = _M0L4userS1375->$1;
    if (_M0L8_2afieldS4413) {
      moonbit_decref(_M0L8_2afieldS4413);
    }
    #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    moonbit_free(_M0L4userS1375);
  }
  _M0L7contentS3213 = _M0L8_2afieldS4031;
  #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3211
  = _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(_M0L7contentS3213);
  _M0L6_2atmpS3212 = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  return _M0FP48clawteam8clawteam8internal6openai13user__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS3211, _M0L6_2atmpS3212);
  join_1372:;
  _M0L8_2afieldS4030 = _M0L9assistantS1373->$0;
  _M0L6_2acntS4408 = Moonbit_object_header(_M0L9assistantS1373)->rc;
  if (_M0L6_2acntS4408 > 1) {
    int32_t _M0L11_2anew__cntS4411 = _M0L6_2acntS4408 - 1;
    Moonbit_object_header(_M0L9assistantS1373)->rc = _M0L11_2anew__cntS4411;
    moonbit_incref(_M0L8_2afieldS4030);
  } else if (_M0L6_2acntS4408 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L8_2afieldS4410 =
      _M0L9assistantS1373->$2;
    moonbit_string_t _M0L8_2afieldS4409;
    moonbit_decref(_M0L8_2afieldS4410);
    _M0L8_2afieldS4409 = _M0L9assistantS1373->$1;
    if (_M0L8_2afieldS4409) {
      moonbit_decref(_M0L8_2afieldS4409);
    }
    #line 36 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    moonbit_free(_M0L9assistantS1373);
  }
  _M0L7contentS3210 = _M0L8_2afieldS4030;
  #line 36 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3209
  = _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(_M0L7contentS3210);
  _M0L6_2atmpS3206 = _M0L6_2atmpS3209;
  _M0L6_2atmpS3207 = 0;
  _M0L6_2atmpS3208 = 0;
  #line 36 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  return _M0FP48clawteam8clawteam8internal6openai18assistant__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS3206, _M0L6_2atmpS3207, _M0L6_2atmpS3208);
  join_1370:;
  _M0L8_2afieldS4029 = _M0L4toolS1371->$0;
  _M0L7contentS3205 = _M0L8_2afieldS4029;
  moonbit_incref(_M0L7contentS3205);
  #line 39 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3203
  = _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(_M0L7contentS3205);
  _M0L8_2afieldS4028 = _M0L4toolS1371->$1;
  _M0L6_2acntS4405 = Moonbit_object_header(_M0L4toolS1371)->rc;
  if (_M0L6_2acntS4405 > 1) {
    int32_t _M0L11_2anew__cntS4407 = _M0L6_2acntS4405 - 1;
    Moonbit_object_header(_M0L4toolS1371)->rc = _M0L11_2anew__cntS4407;
    moonbit_incref(_M0L8_2afieldS4028);
  } else if (_M0L6_2acntS4405 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4406 =
      _M0L4toolS1371->$0;
    moonbit_decref(_M0L8_2afieldS4406);
    #line 40 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    moonbit_free(_M0L4toolS1371);
  }
  _M0L14tool__call__idS3204 = _M0L8_2afieldS4028;
  #line 38 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  return _M0FP48clawteam8clawteam8internal6openai13tool__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS3203, _M0L14tool__call__idS3204);
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0FP48clawteam8clawteam8internal5cache21cache__content__parts(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L5partsS1346
) {
  int32_t _M0L13parts__lengthS1345;
  int32_t _M0L5indexS1348;
  moonbit_string_t _M0L4textS1349;
  int32_t _M0L7_2abindS1359;
  struct _M0TUisE* _M0L7_2abindS1358;
  int32_t _M0L1iS1360;
  int32_t _M0L8_2aindexS1368;
  moonbit_string_t _M0L8_2afieldS4037;
  int32_t _M0L6_2acntS4430;
  moonbit_string_t _M0L7_2atextS1369;
  void** _M0L6_2atmpS3198;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6cachedS1350;
  int32_t _M0L7_2abindS1351;
  int32_t _M0L1iS1352;
  void* _M0L6_2atmpS3195;
  int32_t _M0L7_2abindS1354;
  int32_t _M0L7_2abindS1355;
  int32_t _M0L1iS1356;
  #line 2 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  moonbit_incref(_M0L5partsS1346);
  #line 6 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L13parts__lengthS1345
  = _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1346);
  _M0L7_2abindS1359 = 0;
  _M0L1iS1360 = _M0L7_2abindS1359;
  while (1) {
    if (_M0L1iS1360 < _M0L13parts__lengthS1345) {
      int32_t _M0L6_2atmpS3201 = _M0L13parts__lengthS1345 - 1;
      int32_t _M0L1iS1361 = _M0L6_2atmpS3201 - _M0L1iS1360;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L4textS1363;
      void* _M0L7_2abindS1365;
      struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text* _M0L7_2aTextS1366;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2afieldS4039;
      int32_t _M0L6_2acntS4428;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L7_2atextS1367;
      moonbit_string_t _M0L8_2afieldS4038;
      int32_t _M0L6_2acntS4426;
      moonbit_string_t _M0L4textS3200;
      struct _M0TUisE* _M0L8_2atupleS3199;
      int32_t _M0L6_2atmpS3202;
      moonbit_incref(_M0L5partsS1346);
      #line 9 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L7_2abindS1365
      = _M0MPC15array5Array2atGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1346, _M0L1iS1361);
      _M0L7_2aTextS1366
      = (struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_M0L7_2abindS1365;
      _M0L8_2afieldS4039 = _M0L7_2aTextS1366->$0;
      _M0L6_2acntS4428 = Moonbit_object_header(_M0L7_2aTextS1366)->rc;
      if (_M0L6_2acntS4428 > 1) {
        int32_t _M0L11_2anew__cntS4429 = _M0L6_2acntS4428 - 1;
        Moonbit_object_header(_M0L7_2aTextS1366)->rc = _M0L11_2anew__cntS4429;
        moonbit_incref(_M0L8_2afieldS4039);
      } else if (_M0L6_2acntS4428 == 1) {
        #line 9 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L7_2aTextS1366);
      }
      _M0L7_2atextS1367 = _M0L8_2afieldS4039;
      _M0L4textS1363 = _M0L7_2atextS1367;
      goto join_1362;
      goto joinlet_4600;
      join_1362:;
      _M0L8_2afieldS4038 = _M0L4textS1363->$0;
      _M0L6_2acntS4426 = Moonbit_object_header(_M0L4textS1363)->rc;
      if (_M0L6_2acntS4426 > 1) {
        int32_t _M0L11_2anew__cntS4427 = _M0L6_2acntS4426 - 1;
        Moonbit_object_header(_M0L4textS1363)->rc = _M0L11_2anew__cntS4427;
        moonbit_incref(_M0L8_2afieldS4038);
      } else if (_M0L6_2acntS4426 == 1) {
        #line 10 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
        moonbit_free(_M0L4textS1363);
      }
      _M0L4textS3200 = _M0L8_2afieldS4038;
      _M0L8_2atupleS3199
      = (struct _M0TUisE*)moonbit_malloc(sizeof(struct _M0TUisE));
      Moonbit_object_header(_M0L8_2atupleS3199)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TUisE, $1) >> 2, 1, 0);
      _M0L8_2atupleS3199->$0 = _M0L1iS1361;
      _M0L8_2atupleS3199->$1 = _M0L4textS3200;
      _M0L7_2abindS1358 = _M0L8_2atupleS3199;
      break;
      joinlet_4600:;
      _M0L6_2atmpS3202 = _M0L1iS1360 + 1;
      _M0L1iS1360 = _M0L6_2atmpS3202;
      continue;
    } else {
      return _M0L5partsS1346;
    }
    break;
  }
  _M0L8_2aindexS1368 = _M0L7_2abindS1358->$0;
  _M0L8_2afieldS4037 = _M0L7_2abindS1358->$1;
  _M0L6_2acntS4430 = Moonbit_object_header(_M0L7_2abindS1358)->rc;
  if (_M0L6_2acntS4430 > 1) {
    int32_t _M0L11_2anew__cntS4431 = _M0L6_2acntS4430 - 1;
    Moonbit_object_header(_M0L7_2abindS1358)->rc = _M0L11_2anew__cntS4431;
    moonbit_incref(_M0L8_2afieldS4037);
  } else if (_M0L6_2acntS4430 == 1) {
    #line 7 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
    moonbit_free(_M0L7_2abindS1358);
  }
  _M0L7_2atextS1369 = _M0L8_2afieldS4037;
  _M0L5indexS1348 = _M0L8_2aindexS1368;
  _M0L4textS1349 = _M0L7_2atextS1369;
  goto join_1347;
  join_1347:;
  _M0L6_2atmpS3198 = (void**)moonbit_empty_ref_array;
  _M0L6cachedS1350
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
  Moonbit_object_header(_M0L6cachedS1350)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
  _M0L6cachedS1350->$0 = _M0L6_2atmpS3198;
  _M0L6cachedS1350->$1 = 0;
  _M0L7_2abindS1351 = 0;
  _M0L1iS1352 = _M0L7_2abindS1351;
  while (1) {
    if (_M0L1iS1352 < _M0L5indexS1348) {
      void* _M0L6_2atmpS3193;
      int32_t _M0L6_2atmpS3194;
      moonbit_incref(_M0L5partsS1346);
      #line 17 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L6_2atmpS3193
      = _M0MPC15array5Array2atGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1346, _M0L1iS1352);
      moonbit_incref(_M0L6cachedS1350);
      #line 17 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L6cachedS1350, _M0L6_2atmpS3193);
      _M0L6_2atmpS3194 = _M0L1iS1352 + 1;
      _M0L1iS1352 = _M0L6_2atmpS3194;
      continue;
    }
    break;
  }
  #line 19 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L6_2atmpS3195
  = _M0FP48clawteam8clawteam8internal6openai19text__content__part(_M0L4textS1349, 0ll);
  moonbit_incref(_M0L6cachedS1350);
  #line 19 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L6cachedS1350, _M0L6_2atmpS3195);
  _M0L7_2abindS1354 = _M0L5indexS1348 + 1;
  moonbit_incref(_M0L5partsS1346);
  #line 20 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
  _M0L7_2abindS1355
  = _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1346);
  _M0L1iS1356 = _M0L7_2abindS1354;
  while (1) {
    if (_M0L1iS1356 < _M0L7_2abindS1355) {
      void* _M0L6_2atmpS3196;
      int32_t _M0L6_2atmpS3197;
      moonbit_incref(_M0L5partsS1346);
      #line 21 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0L6_2atmpS3196
      = _M0MPC15array5Array2atGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1346, _M0L1iS1356);
      moonbit_incref(_M0L6cachedS1350);
      #line 21 "E:\\moonbit\\clawteam\\internal\\cache\\cache.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L6cachedS1350, _M0L6_2atmpS3196);
      _M0L6_2atmpS3197 = _M0L1iS1356 + 1;
      _M0L1iS1356 = _M0L6_2atmpS3197;
      continue;
    } else {
      moonbit_decref(_M0L5partsS1346);
    }
    break;
  }
  return _M0L6cachedS1350;
}

void* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(
  void* _M0L4selfS1342
) {
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L5paramS1334;
  struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text* _M0L7_2aTextS1343;
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2afieldS4042;
  int32_t _M0L6_2acntS4432;
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2aparamS1344;
  void* _M0L6_2atmpS3192;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3188;
  moonbit_string_t _M0L8_2afieldS4041;
  moonbit_string_t _M0L4textS3191;
  void* _M0L6_2atmpS3190;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3189;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1336;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3187;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3186;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1335;
  int32_t _M0L14cache__controlS1338;
  int64_t _M0L8_2afieldS4040;
  int64_t _M0L7_2abindS1339;
  void* _M0L6_2atmpS3185;
  #line 16 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L7_2aTextS1343
  = (struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_M0L4selfS1342;
  _M0L8_2afieldS4042 = _M0L7_2aTextS1343->$0;
  _M0L6_2acntS4432 = Moonbit_object_header(_M0L7_2aTextS1343)->rc;
  if (_M0L6_2acntS4432 > 1) {
    int32_t _M0L11_2anew__cntS4433 = _M0L6_2acntS4432 - 1;
    Moonbit_object_header(_M0L7_2aTextS1343)->rc = _M0L11_2anew__cntS4433;
    moonbit_incref(_M0L8_2afieldS4042);
  } else if (_M0L6_2acntS4432 == 1) {
    #line 19 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
    moonbit_free(_M0L7_2aTextS1343);
  }
  _M0L8_2aparamS1344 = _M0L8_2afieldS4042;
  _M0L5paramS1334 = _M0L8_2aparamS1344;
  goto join_1333;
  join_1333:;
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L6_2atmpS3192
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3188
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3188)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3188->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3188->$1 = _M0L6_2atmpS3192;
  _M0L8_2afieldS4041 = _M0L5paramS1334->$0;
  _M0L4textS3191 = _M0L8_2afieldS4041;
  moonbit_incref(_M0L4textS3191);
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L6_2atmpS3190 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4textS3191);
  _M0L8_2atupleS3189
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3189)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3189->$0 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L8_2atupleS3189->$1 = _M0L6_2atmpS3190;
  _M0L7_2abindS1336 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1336[0] = _M0L8_2atupleS3188;
  _M0L7_2abindS1336[1] = _M0L8_2atupleS3189;
  _M0L6_2atmpS3187 = _M0L7_2abindS1336;
  _M0L6_2atmpS3186
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3187
  };
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L4jsonS1335 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3186);
  _M0L8_2afieldS4040 = _M0L5paramS1334->$1;
  moonbit_decref(_M0L5paramS1334);
  _M0L7_2abindS1339 = _M0L8_2afieldS4040;
  if (_M0L7_2abindS1339 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1340 = _M0L7_2abindS1339;
    int32_t _M0L17_2acache__controlS1341 = (int32_t)_M0L7_2aSomeS1340;
    _M0L14cache__controlS1338 = _M0L17_2acache__controlS1341;
    goto join_1337;
  }
  goto joinlet_4604;
  join_1337:;
  #line 23 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0L6_2atmpS3185
  = _M0IP48clawteam8clawteam8internal6openai12CacheControlPB6ToJson8to__json(_M0L14cache__controlS1338);
  moonbit_incref(_M0L4jsonS1335);
  #line 23 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1335, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS3185);
  joinlet_4604:;
  #line 25 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_content_part_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1335);
}

void* _M0IP48clawteam8clawteam8internal6openai12CacheControlPB6ToJson8to__json(
  int32_t _M0L4selfS1332
) {
  void* _M0L6_2atmpS3184;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3183;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1331;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3182;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3181;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3180;
  #line 8 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  _M0L6_2atmpS3184
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3183
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3183)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3183->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3183->$1 = _M0L6_2atmpS3184;
  _M0L7_2abindS1331 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1331[0] = _M0L8_2atupleS3183;
  _M0L6_2atmpS3182 = _M0L7_2abindS1331;
  _M0L6_2atmpS3181
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3182
  };
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  _M0L6_2atmpS3180 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3181);
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\cache_control.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS3180);
}

void* _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__json(
  void* _M0L4selfS1322
) {
  int32_t _M0L24content__parts__to__jsonS1279;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L5paramS1293;
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L5paramS1296;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L5paramS1305;
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L5paramS1314;
  void* _M0L6_2atmpS3179;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3178;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1316;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3177;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3176;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1315;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4052;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3174;
  moonbit_string_t _M0L4nameS1318;
  moonbit_string_t _M0L8_2afieldS4051;
  int32_t _M0L6_2acntS4444;
  moonbit_string_t _M0L7_2abindS1319;
  void* _M0L6_2atmpS3175;
  void* _M0L6_2atmpS3173;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3172;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1307;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3171;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3170;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1306;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4050;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3168;
  moonbit_string_t _M0L4nameS1309;
  moonbit_string_t _M0L8_2afieldS4049;
  int32_t _M0L6_2acntS4441;
  moonbit_string_t _M0L7_2abindS1310;
  void* _M0L6_2atmpS3169;
  void* _M0L6_2atmpS3167;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3166;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1298;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3165;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3164;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1297;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4048;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3158;
  moonbit_string_t _M0L4nameS1300;
  moonbit_string_t _M0L8_2afieldS4047;
  moonbit_string_t _M0L7_2abindS1301;
  void* _M0L6_2atmpS3159;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L8_2afieldS4046;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS3161;
  int32_t _M0L6_2atmpS3160;
  void* _M0L6_2atmpS3157;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3150;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4044;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS3156;
  void* _M0L6_2atmpS3155;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3151;
  moonbit_string_t _M0L8_2afieldS4043;
  int32_t _M0L6_2acntS4434;
  moonbit_string_t _M0L14tool__call__idS3154;
  void* _M0L6_2atmpS3153;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3152;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1294;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3149;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3148;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3147;
  #line 11 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L24content__parts__to__jsonS1279 = 0;
  switch (Moonbit_object_tag(_M0L4selfS1322)) {
    case 0: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System* _M0L9_2aSystemS1323 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_M0L4selfS1322;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2afieldS4053 =
        _M0L9_2aSystemS1323->$0;
      int32_t _M0L6_2acntS4447 =
        Moonbit_object_header(_M0L9_2aSystemS1323)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L8_2aparamS1324;
      if (_M0L6_2acntS4447 > 1) {
        int32_t _M0L11_2anew__cntS4448 = _M0L6_2acntS4447 - 1;
        Moonbit_object_header(_M0L9_2aSystemS1323)->rc
        = _M0L11_2anew__cntS4448;
        moonbit_incref(_M0L8_2afieldS4053);
      } else if (_M0L6_2acntS4447 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L9_2aSystemS1323);
      }
      _M0L8_2aparamS1324 = _M0L8_2afieldS4053;
      _M0L5paramS1314 = _M0L8_2aparamS1324;
      goto join_1313;
      break;
    }
    
    case 1: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User* _M0L7_2aUserS1325 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User*)_M0L4selfS1322;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L8_2afieldS4054 =
        _M0L7_2aUserS1325->$0;
      int32_t _M0L6_2acntS4449 = Moonbit_object_header(_M0L7_2aUserS1325)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L8_2aparamS1326;
      if (_M0L6_2acntS4449 > 1) {
        int32_t _M0L11_2anew__cntS4450 = _M0L6_2acntS4449 - 1;
        Moonbit_object_header(_M0L7_2aUserS1325)->rc = _M0L11_2anew__cntS4450;
        moonbit_incref(_M0L8_2afieldS4054);
      } else if (_M0L6_2acntS4449 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L7_2aUserS1325);
      }
      _M0L8_2aparamS1326 = _M0L8_2afieldS4054;
      _M0L5paramS1305 = _M0L8_2aparamS1326;
      goto join_1304;
      break;
    }
    
    case 2: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant* _M0L12_2aAssistantS1327 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant*)_M0L4selfS1322;
      struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L8_2afieldS4055 =
        _M0L12_2aAssistantS1327->$0;
      int32_t _M0L6_2acntS4451 =
        Moonbit_object_header(_M0L12_2aAssistantS1327)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L8_2aparamS1328;
      if (_M0L6_2acntS4451 > 1) {
        int32_t _M0L11_2anew__cntS4452 = _M0L6_2acntS4451 - 1;
        Moonbit_object_header(_M0L12_2aAssistantS1327)->rc
        = _M0L11_2anew__cntS4452;
        moonbit_incref(_M0L8_2afieldS4055);
      } else if (_M0L6_2acntS4451 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L12_2aAssistantS1327);
      }
      _M0L8_2aparamS1328 = _M0L8_2afieldS4055;
      _M0L5paramS1296 = _M0L8_2aparamS1328;
      goto join_1295;
      break;
    }
    default: {
      struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool* _M0L7_2aToolS1329 =
        (struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool*)_M0L4selfS1322;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L8_2afieldS4056 =
        _M0L7_2aToolS1329->$0;
      int32_t _M0L6_2acntS4453 = Moonbit_object_header(_M0L7_2aToolS1329)->rc;
      struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L8_2aparamS1330;
      if (_M0L6_2acntS4453 > 1) {
        int32_t _M0L11_2anew__cntS4454 = _M0L6_2acntS4453 - 1;
        Moonbit_object_header(_M0L7_2aToolS1329)->rc = _M0L11_2anew__cntS4454;
        moonbit_incref(_M0L8_2afieldS4056);
      } else if (_M0L6_2acntS4453 == 1) {
        #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        moonbit_free(_M0L7_2aToolS1329);
      }
      _M0L8_2aparamS1330 = _M0L8_2afieldS4056;
      _M0L5paramS1293 = _M0L8_2aparamS1330;
      goto join_1292;
      break;
    }
  }
  join_1313:;
  #line 28 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3179
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_18.data);
  _M0L8_2atupleS3178
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3178)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3178->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3178->$1 = _M0L6_2atmpS3179;
  _M0L7_2abindS1316 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1316[0] = _M0L8_2atupleS3178;
  _M0L6_2atmpS3177 = _M0L7_2abindS1316;
  _M0L6_2atmpS3176
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3177
  };
  #line 28 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L4jsonS1315 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3176);
  _M0L8_2afieldS4052 = _M0L5paramS1314->$0;
  _M0L7contentS3174 = _M0L8_2afieldS4052;
  moonbit_incref(_M0L7contentS3174);
  moonbit_incref(_M0L4jsonS1315);
  #line 29 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1279(_M0L24content__parts__to__jsonS1279, _M0L7contentS3174, _M0L4jsonS1315);
  _M0L8_2afieldS4051 = _M0L5paramS1314->$1;
  _M0L6_2acntS4444 = Moonbit_object_header(_M0L5paramS1314)->rc;
  if (_M0L6_2acntS4444 > 1) {
    int32_t _M0L11_2anew__cntS4446 = _M0L6_2acntS4444 - 1;
    Moonbit_object_header(_M0L5paramS1314)->rc = _M0L11_2anew__cntS4446;
    if (_M0L8_2afieldS4051) {
      moonbit_incref(_M0L8_2afieldS4051);
    }
  } else if (_M0L6_2acntS4444 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4445 =
      _M0L5paramS1314->$0;
    moonbit_decref(_M0L8_2afieldS4445);
    #line 30 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L5paramS1314);
  }
  _M0L7_2abindS1319 = _M0L8_2afieldS4051;
  if (_M0L7_2abindS1319 == 0) {
    if (_M0L7_2abindS1319) {
      moonbit_decref(_M0L7_2abindS1319);
    }
  } else {
    moonbit_string_t _M0L7_2aSomeS1320 = _M0L7_2abindS1319;
    moonbit_string_t _M0L7_2anameS1321 = _M0L7_2aSomeS1320;
    _M0L4nameS1318 = _M0L7_2anameS1321;
    goto join_1317;
  }
  goto joinlet_4609;
  join_1317:;
  #line 31 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3175 = _M0MPC14json4Json6string(_M0L4nameS1318);
  moonbit_incref(_M0L4jsonS1315);
  #line 31 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1315, (moonbit_string_t)moonbit_string_literal_54.data, _M0L6_2atmpS3175);
  joinlet_4609:;
  #line 33 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1315);
  join_1304:;
  #line 36 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3173
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3172
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3172)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3172->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3172->$1 = _M0L6_2atmpS3173;
  _M0L7_2abindS1307 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1307[0] = _M0L8_2atupleS3172;
  _M0L6_2atmpS3171 = _M0L7_2abindS1307;
  _M0L6_2atmpS3170
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3171
  };
  #line 36 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L4jsonS1306 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3170);
  _M0L8_2afieldS4050 = _M0L5paramS1305->$0;
  _M0L7contentS3168 = _M0L8_2afieldS4050;
  moonbit_incref(_M0L7contentS3168);
  moonbit_incref(_M0L4jsonS1306);
  #line 37 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1279(_M0L24content__parts__to__jsonS1279, _M0L7contentS3168, _M0L4jsonS1306);
  _M0L8_2afieldS4049 = _M0L5paramS1305->$1;
  _M0L6_2acntS4441 = Moonbit_object_header(_M0L5paramS1305)->rc;
  if (_M0L6_2acntS4441 > 1) {
    int32_t _M0L11_2anew__cntS4443 = _M0L6_2acntS4441 - 1;
    Moonbit_object_header(_M0L5paramS1305)->rc = _M0L11_2anew__cntS4443;
    if (_M0L8_2afieldS4049) {
      moonbit_incref(_M0L8_2afieldS4049);
    }
  } else if (_M0L6_2acntS4441 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4442 =
      _M0L5paramS1305->$0;
    moonbit_decref(_M0L8_2afieldS4442);
    #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L5paramS1305);
  }
  _M0L7_2abindS1310 = _M0L8_2afieldS4049;
  if (_M0L7_2abindS1310 == 0) {
    if (_M0L7_2abindS1310) {
      moonbit_decref(_M0L7_2abindS1310);
    }
  } else {
    moonbit_string_t _M0L7_2aSomeS1311 = _M0L7_2abindS1310;
    moonbit_string_t _M0L7_2anameS1312 = _M0L7_2aSomeS1311;
    _M0L4nameS1309 = _M0L7_2anameS1312;
    goto join_1308;
  }
  goto joinlet_4610;
  join_1308:;
  #line 39 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3169 = _M0MPC14json4Json6string(_M0L4nameS1309);
  moonbit_incref(_M0L4jsonS1306);
  #line 39 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1306, (moonbit_string_t)moonbit_string_literal_54.data, _M0L6_2atmpS3169);
  joinlet_4610:;
  #line 41 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1306);
  join_1295:;
  #line 44 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3167
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_26.data);
  _M0L8_2atupleS3166
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3166)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3166->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3166->$1 = _M0L6_2atmpS3167;
  _M0L7_2abindS1298 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1298[0] = _M0L8_2atupleS3166;
  _M0L6_2atmpS3165 = _M0L7_2abindS1298;
  _M0L6_2atmpS3164
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 1, _M0L6_2atmpS3165
  };
  #line 44 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L4jsonS1297 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3164);
  _M0L8_2afieldS4048 = _M0L5paramS1296->$0;
  _M0L7contentS3158 = _M0L8_2afieldS4048;
  moonbit_incref(_M0L7contentS3158);
  moonbit_incref(_M0L4jsonS1297);
  #line 45 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1279(_M0L24content__parts__to__jsonS1279, _M0L7contentS3158, _M0L4jsonS1297);
  _M0L8_2afieldS4047 = _M0L5paramS1296->$1;
  _M0L7_2abindS1301 = _M0L8_2afieldS4047;
  if (_M0L7_2abindS1301 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1302 = _M0L7_2abindS1301;
    moonbit_string_t _M0L7_2anameS1303 = _M0L7_2aSomeS1302;
    moonbit_incref(_M0L7_2anameS1303);
    _M0L4nameS1300 = _M0L7_2anameS1303;
    goto join_1299;
  }
  goto joinlet_4611;
  join_1299:;
  #line 47 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3159 = _M0MPC14json4Json6string(_M0L4nameS1300);
  moonbit_incref(_M0L4jsonS1297);
  #line 47 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1297, (moonbit_string_t)moonbit_string_literal_54.data, _M0L6_2atmpS3159);
  joinlet_4611:;
  _M0L8_2afieldS4046 = _M0L5paramS1296->$2;
  _M0L11tool__callsS3161 = _M0L8_2afieldS4046;
  moonbit_incref(_M0L11tool__callsS3161);
  #line 49 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3160
  = _M0MPC15array5Array9is__emptyGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS3161);
  if (!_M0L6_2atmpS3160) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L8_2afieldS4045 =
      _M0L5paramS1296->$2;
    int32_t _M0L6_2acntS4437 = Moonbit_object_header(_M0L5paramS1296)->rc;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS3163;
    void* _M0L6_2atmpS3162;
    if (_M0L6_2acntS4437 > 1) {
      int32_t _M0L11_2anew__cntS4440 = _M0L6_2acntS4437 - 1;
      Moonbit_object_header(_M0L5paramS1296)->rc = _M0L11_2anew__cntS4440;
      moonbit_incref(_M0L8_2afieldS4045);
    } else if (_M0L6_2acntS4437 == 1) {
      moonbit_string_t _M0L8_2afieldS4439 = _M0L5paramS1296->$1;
      struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4438;
      if (_M0L8_2afieldS4439) {
        moonbit_decref(_M0L8_2afieldS4439);
      }
      _M0L8_2afieldS4438 = _M0L5paramS1296->$0;
      moonbit_decref(_M0L8_2afieldS4438);
      #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
      moonbit_free(_M0L5paramS1296);
    }
    _M0L11tool__callsS3163 = _M0L8_2afieldS4045;
    #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    _M0L6_2atmpS3162
    = _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS3163);
    moonbit_incref(_M0L4jsonS1297);
    #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1297, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS3162);
  } else {
    moonbit_decref(_M0L5paramS1296);
  }
  #line 52 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1297);
  join_1292:;
  #line 56 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3157
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_28.data);
  _M0L8_2atupleS3150
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3150)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3150->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3150->$1 = _M0L6_2atmpS3157;
  _M0L8_2afieldS4044 = _M0L5paramS1293->$0;
  _M0L7contentS3156 = _M0L8_2afieldS4044;
  moonbit_incref(_M0L7contentS3156);
  #line 57 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3155
  = _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L7contentS3156);
  _M0L8_2atupleS3151
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3151)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3151->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3151->$1 = _M0L6_2atmpS3155;
  _M0L8_2afieldS4043 = _M0L5paramS1293->$1;
  _M0L6_2acntS4434 = Moonbit_object_header(_M0L5paramS1293)->rc;
  if (_M0L6_2acntS4434 > 1) {
    int32_t _M0L11_2anew__cntS4436 = _M0L6_2acntS4434 - 1;
    Moonbit_object_header(_M0L5paramS1293)->rc = _M0L11_2anew__cntS4436;
    moonbit_incref(_M0L8_2afieldS4043);
  } else if (_M0L6_2acntS4434 == 1) {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L8_2afieldS4435 =
      _M0L5paramS1293->$0;
    moonbit_decref(_M0L8_2afieldS4435);
    #line 58 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L5paramS1293);
  }
  _M0L14tool__call__idS3154 = _M0L8_2afieldS4043;
  #line 58 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3153
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L14tool__call__idS3154);
  _M0L8_2atupleS3152
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3152)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3152->$0 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L8_2atupleS3152->$1 = _M0L6_2atmpS3153;
  _M0L7_2abindS1294 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1294[0] = _M0L8_2atupleS3150;
  _M0L7_2abindS1294[1] = _M0L8_2atupleS3151;
  _M0L7_2abindS1294[2] = _M0L8_2atupleS3152;
  _M0L6_2atmpS3149 = _M0L7_2abindS1294;
  _M0L6_2atmpS3148
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3149
  };
  #line 55 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3147 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3148);
  #line 55 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS3147);
}

int32_t _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson8to__jsonN24content__parts__to__jsonS1279(
  int32_t _M0L6_2aenvS3136,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L14content__partsS1280,
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1281
) {
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L5partsS1283;
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L4textS1285;
  int32_t _M0L3lenS3140;
  moonbit_string_t _M0L8_2afieldS4057;
  int32_t _M0L6_2acntS4455;
  moonbit_string_t _M0L4textS3139;
  void* _M0L6_2atmpS3138;
  void* _M0L6_2atmpS3137;
  #line 14 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L3lenS3140 = _M0L14content__partsS1280->$1;
  if (_M0L3lenS3140 == 0) {
    moonbit_decref(_M0L4jsonS1281);
    moonbit_decref(_M0L14content__partsS1280);
  } else {
    int32_t _M0L3lenS3141 = _M0L14content__partsS1280->$1;
    if (_M0L3lenS3141 == 1) {
      void** _M0L8_2afieldS4060 = _M0L14content__partsS1280->$0;
      void** _M0L3bufS3144 = _M0L8_2afieldS4060;
      void* _M0L6_2atmpS4059 = (void*)_M0L3bufS3144[0];
      void* _M0L4_2axS1286 = _M0L6_2atmpS4059;
      struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text* _M0L7_2aTextS1287 =
        (struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_M0L4_2axS1286;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L8_2afieldS4058 =
        _M0L7_2aTextS1287->$0;
      struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L7_2atextS1288 =
        _M0L8_2afieldS4058;
      int64_t _M0L7_2abindS1289 = _M0L7_2atextS1288->$1;
      if (_M0L7_2abindS1289 == 4294967296ll) {
        moonbit_incref(_M0L7_2atextS1288);
        moonbit_decref(_M0L14content__partsS1280);
        _M0L4textS1285 = _M0L7_2atextS1288;
        goto join_1284;
      } else {
        int32_t _M0L3lenS3143 = _M0L14content__partsS1280->$1;
        int64_t _M0L6_2atmpS3142 = (int64_t)_M0L3lenS3143;
        struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4_2axS1290;
        #line 20 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
        _M0L4_2axS1290
        = _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L14content__partsS1280, 0, _M0L6_2atmpS3142);
        _M0L5partsS1283 = _M0L4_2axS1290;
        goto join_1282;
      }
    } else {
      int32_t _M0L3lenS3146 = _M0L14content__partsS1280->$1;
      int64_t _M0L6_2atmpS3145 = (int64_t)_M0L3lenS3146;
      struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4_2axS1291;
      #line 18 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
      _M0L4_2axS1291
      = _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L14content__partsS1280, 0, _M0L6_2atmpS3145);
      _M0L5partsS1283 = _M0L4_2axS1291;
      goto join_1282;
    }
  }
  goto joinlet_4613;
  join_1284:;
  _M0L8_2afieldS4057 = _M0L4textS1285->$0;
  _M0L6_2acntS4455 = Moonbit_object_header(_M0L4textS1285)->rc;
  if (_M0L6_2acntS4455 > 1) {
    int32_t _M0L11_2anew__cntS4456 = _M0L6_2acntS4455 - 1;
    Moonbit_object_header(_M0L4textS1285)->rc = _M0L11_2anew__cntS4456;
    moonbit_incref(_M0L8_2afieldS4057);
  } else if (_M0L6_2acntS4455 == 1) {
    #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
    moonbit_free(_M0L4textS1285);
  }
  _M0L4textS3139 = _M0L8_2afieldS4057;
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3138 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4textS3139);
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1281, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS3138);
  joinlet_4613:;
  goto joinlet_4612;
  join_1282:;
  #line 22 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0L6_2atmpS3137
  = _M0IPC15array9ArrayViewPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1283);
  #line 22 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_param.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1281, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS3137);
  joinlet_4612:;
  return 0;
}

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L4selfS1278
) {
  moonbit_string_t _M0L8_2afieldS4062;
  moonbit_string_t _M0L2idS3135;
  void* _M0L6_2atmpS3134;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3128;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L8_2afieldS4061;
  int32_t _M0L6_2acntS4457;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L8functionS3133;
  void* _M0L6_2atmpS3132;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3129;
  void* _M0L6_2atmpS3131;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3130;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1277;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3127;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3126;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3125;
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L8_2afieldS4062 = _M0L4selfS1278->$0;
  _M0L2idS3135 = _M0L8_2afieldS4062;
  moonbit_incref(_M0L2idS3135);
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3134 = _M0IPC16string6StringPB6ToJson8to__json(_M0L2idS3135);
  _M0L8_2atupleS3128
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3128)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3128->$0 = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L8_2atupleS3128->$1 = _M0L6_2atmpS3134;
  _M0L8_2afieldS4061 = _M0L4selfS1278->$1;
  _M0L6_2acntS4457 = Moonbit_object_header(_M0L4selfS1278)->rc;
  if (_M0L6_2acntS4457 > 1) {
    int32_t _M0L11_2anew__cntS4459 = _M0L6_2acntS4457 - 1;
    Moonbit_object_header(_M0L4selfS1278)->rc = _M0L11_2anew__cntS4459;
    moonbit_incref(_M0L8_2afieldS4061);
  } else if (_M0L6_2acntS4457 == 1) {
    moonbit_string_t _M0L8_2afieldS4458 = _M0L4selfS1278->$0;
    moonbit_decref(_M0L8_2afieldS4458);
    #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
    moonbit_free(_M0L4selfS1278);
  }
  _M0L8functionS3133 = _M0L8_2afieldS4061;
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3132
  = _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(_M0L8functionS3133);
  _M0L8_2atupleS3129
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3129)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3129->$0 = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L8_2atupleS3129->$1 = _M0L6_2atmpS3132;
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3131
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_56.data);
  _M0L8_2atupleS3130
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3130)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3130->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3130->$1 = _M0L6_2atmpS3131;
  _M0L7_2abindS1277 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1277[0] = _M0L8_2atupleS3128;
  _M0L7_2abindS1277[1] = _M0L8_2atupleS3129;
  _M0L7_2abindS1277[2] = _M0L8_2atupleS3130;
  _M0L6_2atmpS3127 = _M0L7_2abindS1277;
  _M0L6_2atmpS3126
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3127
  };
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3125 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3126);
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS3125);
}

void* _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L4selfS1276
) {
  moonbit_string_t _M0L8_2afieldS4064;
  moonbit_string_t _M0L4nameS3124;
  void* _M0L6_2atmpS3123;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3118;
  moonbit_string_t _M0L8_2afieldS4063;
  int32_t _M0L6_2acntS4460;
  moonbit_string_t _M0L9argumentsS3122;
  moonbit_string_t _M0L6_2atmpS3121;
  void* _M0L6_2atmpS3120;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3119;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1275;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3117;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3116;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3115;
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L8_2afieldS4064 = _M0L4selfS1276->$1;
  _M0L4nameS3124 = _M0L8_2afieldS4064;
  moonbit_incref(_M0L4nameS3124);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3123 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4nameS3124);
  _M0L8_2atupleS3118
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3118)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3118->$0 = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L8_2atupleS3118->$1 = _M0L6_2atmpS3123;
  _M0L8_2afieldS4063 = _M0L4selfS1276->$0;
  _M0L6_2acntS4460 = Moonbit_object_header(_M0L4selfS1276)->rc;
  if (_M0L6_2acntS4460 > 1) {
    int32_t _M0L11_2anew__cntS4462 = _M0L6_2acntS4460 - 1;
    Moonbit_object_header(_M0L4selfS1276)->rc = _M0L11_2anew__cntS4462;
    if (_M0L8_2afieldS4063) {
      moonbit_incref(_M0L8_2afieldS4063);
    }
  } else if (_M0L6_2acntS4460 == 1) {
    moonbit_string_t _M0L8_2afieldS4461 = _M0L4selfS1276->$1;
    moonbit_decref(_M0L8_2afieldS4461);
    #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
    moonbit_free(_M0L4selfS1276);
  }
  _M0L9argumentsS3122 = _M0L8_2afieldS4063;
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3121
  = _M0MPC16option6Option10unwrap__orGsE(_M0L9argumentsS3122, (moonbit_string_t)moonbit_string_literal_0.data);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3120
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS3121);
  _M0L8_2atupleS3119
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3119)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3119->$0 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L8_2atupleS3119->$1 = _M0L6_2atmpS3120;
  _M0L7_2abindS1275 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1275[0] = _M0L8_2atupleS3118;
  _M0L7_2abindS1275[1] = _M0L8_2atupleS3119;
  _M0L6_2atmpS3117 = _M0L7_2abindS1275;
  _M0L6_2atmpS3116
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3117
  };
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3115 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3116);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS3115);
}

void* _M0FP48clawteam8clawteam8internal6openai18assistant__messageGsE(
  moonbit_string_t _M0L7contentS1268,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L17tool__calls_2eoptS1266,
  moonbit_string_t _M0L4nameS1269
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1265;
  if (_M0L17tool__calls_2eoptS1266 == 0) {
    struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2atmpS3113;
    if (_M0L17tool__calls_2eoptS1266) {
      moonbit_decref(_M0L17tool__calls_2eoptS1266);
    }
    _M0L6_2atmpS3113
    = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**)moonbit_empty_ref_array;
    _M0L11tool__callsS1265
    = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE));
    Moonbit_object_header(_M0L11tool__callsS1265)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE, $0) >> 2, 1, 0);
    _M0L11tool__callsS1265->$0 = _M0L6_2atmpS3113;
    _M0L11tool__callsS1265->$1 = 0;
  } else {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L7_2aSomeS1267 =
      _M0L17tool__calls_2eoptS1266;
    _M0L11tool__callsS1265 = _M0L7_2aSomeS1267;
  }
  return _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGsE(_M0L7contentS1268, _M0L11tool__callsS1265, _M0L4nameS1269);
}

void* _M0FP48clawteam8clawteam8internal6openai18assistant__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1273,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L17tool__calls_2eoptS1271,
  moonbit_string_t _M0L4nameS1274
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1270;
  if (_M0L17tool__calls_2eoptS1271 == 0) {
    struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2atmpS3114;
    if (_M0L17tool__calls_2eoptS1271) {
      moonbit_decref(_M0L17tool__calls_2eoptS1271);
    }
    _M0L6_2atmpS3114
    = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**)moonbit_empty_ref_array;
    _M0L11tool__callsS1270
    = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE));
    Moonbit_object_header(_M0L11tool__callsS1270)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE, $0) >> 2, 1, 0);
    _M0L11tool__callsS1270->$0 = _M0L6_2atmpS3114;
    _M0L11tool__callsS1270->$1 = 0;
  } else {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L7_2aSomeS1272 =
      _M0L17tool__calls_2eoptS1271;
    _M0L11tool__callsS1270 = _M0L7_2aSomeS1272;
  }
  return _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L7contentS1273, _M0L11tool__callsS1270, _M0L4nameS1274);
}

void* _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGsE(
  moonbit_string_t _M0L7contentS1252,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1256,
  moonbit_string_t _M0L4nameS1255
) {
  moonbit_string_t _M0L7contentS1251;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1249;
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L6_2atmpS3109;
  void* _block_4615;
  #line 226 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  if (_M0L7contentS1252 == 0) {
    void** _M0L6_2atmpS3110;
    if (_M0L7contentS1252) {
      moonbit_decref(_M0L7contentS1252);
    }
    _M0L6_2atmpS3110 = (void**)moonbit_empty_ref_array;
    _M0L7contentS1249
    = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
    Moonbit_object_header(_M0L7contentS1249)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
    _M0L7contentS1249->$0 = _M0L6_2atmpS3110;
    _M0L7contentS1249->$1 = 0;
  } else {
    moonbit_string_t _M0L7_2aSomeS1253 = _M0L7contentS1252;
    moonbit_string_t _M0L10_2acontentS1254 = _M0L7_2aSomeS1253;
    _M0L7contentS1251 = _M0L10_2acontentS1254;
    goto join_1250;
  }
  goto joinlet_4614;
  join_1250:;
  #line 232 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L7contentS1249
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1251);
  joinlet_4614:;
  _M0L6_2atmpS3109
  = (struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3109)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam, $0) >> 2, 3, 0);
  _M0L6_2atmpS3109->$0 = _M0L7contentS1249;
  _M0L6_2atmpS3109->$1 = _M0L4nameS1255;
  _M0L6_2atmpS3109->$2 = _M0L11tool__callsS1256;
  _block_4615
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant));
  Moonbit_object_header(_block_4615)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant, $0) >> 2, 1, 2);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant*)_block_4615)->$0
  = _M0L6_2atmpS3109;
  return _block_4615;
}

void* _M0FP48clawteam8clawteam8internal6openai26assistant__message_2einnerGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1260,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1264,
  moonbit_string_t _M0L4nameS1263
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1259;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1257;
  struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam* _M0L6_2atmpS3111;
  void* _block_4617;
  #line 226 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  if (_M0L7contentS1260 == 0) {
    void** _M0L6_2atmpS3112;
    if (_M0L7contentS1260) {
      moonbit_decref(_M0L7contentS1260);
    }
    _M0L6_2atmpS3112 = (void**)moonbit_empty_ref_array;
    _M0L7contentS1257
    = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
    Moonbit_object_header(_M0L7contentS1257)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
    _M0L7contentS1257->$0 = _M0L6_2atmpS3112;
    _M0L7contentS1257->$1 = 0;
  } else {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7_2aSomeS1261 =
      _M0L7contentS1260;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L10_2acontentS1262 =
      _M0L7_2aSomeS1261;
    _M0L7contentS1259 = _M0L10_2acontentS1262;
    goto join_1258;
  }
  goto joinlet_4616;
  join_1258:;
  #line 232 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L7contentS1257
  = _M0IPC15array5ArrayP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__contentGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L7contentS1259);
  joinlet_4616:;
  _M0L6_2atmpS3111
  = (struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3111)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai35ChatCompletionAssistantMessageParam, $0) >> 2, 3, 0);
  _M0L6_2atmpS3111->$0 = _M0L7contentS1257;
  _M0L6_2atmpS3111->$1 = _M0L4nameS1263;
  _M0L6_2atmpS3111->$2 = _M0L11tool__callsS1264;
  _block_4617
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant));
  Moonbit_object_header(_block_4617)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant, $0) >> 2, 1, 2);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam9Assistant*)_block_4617)->$0
  = _M0L6_2atmpS3111;
  return _block_4617;
}

void* _M0FP48clawteam8clawteam8internal6openai13tool__messageGsE(
  moonbit_string_t _M0L7contentS1245,
  moonbit_string_t _M0L14tool__call__idS1246
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3106;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L6_2atmpS3105;
  void* _block_4618;
  #line 179 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 184 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3106
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1245);
  _M0L6_2atmpS3105
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3105)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS3105->$0 = _M0L6_2atmpS3106;
  _M0L6_2atmpS3105->$1 = _M0L14tool__call__idS1246;
  _block_4618
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool));
  Moonbit_object_header(_block_4618)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool, $0) >> 2, 1, 3);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool*)_block_4618)->$0
  = _M0L6_2atmpS3105;
  return _block_4618;
}

void* _M0FP48clawteam8clawteam8internal6openai13tool__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1247,
  moonbit_string_t _M0L14tool__call__idS1248
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3108;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam* _M0L6_2atmpS3107;
  void* _block_4619;
  #line 179 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 184 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3108
  = _M0IPC15array5ArrayP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__contentGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L7contentS1247);
  _M0L6_2atmpS3107
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3107)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionToolMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS3107->$0 = _M0L6_2atmpS3108;
  _M0L6_2atmpS3107->$1 = _M0L14tool__call__idS1248;
  _block_4619
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool));
  Moonbit_object_header(_block_4619)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool, $0) >> 2, 1, 3);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4Tool*)_block_4619)->$0
  = _M0L6_2atmpS3107;
  return _block_4619;
}

void* _M0FP48clawteam8clawteam8internal6openai13user__messageGsE(
  moonbit_string_t _M0L7contentS1241,
  moonbit_string_t _M0L4nameS1242
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3102;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L6_2atmpS3101;
  void* _block_4620;
  #line 160 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 164 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3102
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1241);
  _M0L6_2atmpS3101
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3101)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS3101->$0 = _M0L6_2atmpS3102;
  _M0L6_2atmpS3101->$1 = _M0L4nameS1242;
  _block_4620
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User));
  Moonbit_object_header(_block_4620)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User, $0) >> 2, 1, 1);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User*)_block_4620)->$0
  = _M0L6_2atmpS3101;
  return _block_4620;
}

void* _M0FP48clawteam8clawteam8internal6openai13user__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1243,
  moonbit_string_t _M0L4nameS1244
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3104;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam* _M0L6_2atmpS3103;
  void* _block_4621;
  #line 160 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 164 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3104
  = _M0IPC15array5ArrayP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__contentGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L7contentS1243);
  _M0L6_2atmpS3103
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3103)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionUserMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS3103->$0 = _M0L6_2atmpS3104;
  _M0L6_2atmpS3103->$1 = _M0L4nameS1244;
  _block_4621
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User));
  Moonbit_object_header(_block_4621)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User, $0) >> 2, 1, 1);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam4User*)_block_4621)->$0
  = _M0L6_2atmpS3103;
  return _block_4621;
}

void* _M0FP48clawteam8clawteam8internal6openai15system__messageGsE(
  moonbit_string_t _M0L7contentS1237,
  moonbit_string_t _M0L4nameS1238
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3098;
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6_2atmpS3097;
  void* _block_4622;
  #line 142 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3098
  = _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L7contentS1237);
  _M0L6_2atmpS3097
  = (struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3097)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS3097->$0 = _M0L6_2atmpS3098;
  _M0L6_2atmpS3097->$1 = _M0L4nameS1238;
  _block_4622
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System));
  Moonbit_object_header(_block_4622)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System, $0) >> 2, 1, 0);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_block_4622)->$0
  = _M0L6_2atmpS3097;
  return _block_4622;
}

void* _M0FP48clawteam8clawteam8internal6openai15system__messageGRPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7contentS1239,
  moonbit_string_t _M0L4nameS1240
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L6_2atmpS3100;
  struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam* _M0L6_2atmpS3099;
  void* _block_4623;
  #line 142 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3100
  = _M0IPC15array5ArrayP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__contentGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L7contentS1239);
  _M0L6_2atmpS3099
  = (struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam));
  Moonbit_object_header(_M0L6_2atmpS3099)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai32ChatCompletionSystemMessageParam, $0) >> 2, 2, 0);
  _M0L6_2atmpS3099->$0 = _M0L6_2atmpS3100;
  _M0L6_2atmpS3099->$1 = _M0L4nameS1240;
  _block_4623
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System));
  Moonbit_object_header(_block_4623)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System, $0) >> 2, 1, 0);
  ((struct _M0DTP48clawteam8clawteam8internal6openai26ChatCompletionMessageParam6System*)_block_4623)->$0
  = _M0L6_2atmpS3099;
  return _block_4623;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IPC15array5ArrayP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__contentGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS1232
) {
  void** _M0L6_2atmpS3096;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L5partsS1230;
  int32_t _M0L7_2abindS1231;
  int32_t _M0L2__S1233;
  #line 118 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3096 = (void**)moonbit_empty_ref_array;
  _M0L5partsS1230
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
  Moonbit_object_header(_M0L5partsS1230)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
  _M0L5partsS1230->$0 = _M0L6_2atmpS3096;
  _M0L5partsS1230->$1 = 0;
  _M0L7_2abindS1231 = _M0L4selfS1232->$1;
  _M0L2__S1233 = 0;
  while (1) {
    if (_M0L2__S1233 < _M0L7_2abindS1231) {
      void** _M0L8_2afieldS4068 = _M0L4selfS1232->$0;
      void** _M0L3bufS3095 = _M0L8_2afieldS4068;
      void* _M0L6_2atmpS4067 = (void*)_M0L3bufS3095[_M0L2__S1233];
      void* _M0L4partS1234 = _M0L6_2atmpS4067;
      struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L7_2abindS1235;
      void** _M0L8_2afieldS4066;
      void** _M0L3bufS3092;
      int32_t _M0L8_2afieldS4065;
      int32_t _M0L6_2acntS4463;
      int32_t _M0L3lenS3093;
      struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L6_2atmpS3091;
      int32_t _M0L6_2atmpS3094;
      moonbit_incref(_M0L4partS1234);
      #line 125 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
      _M0L7_2abindS1235
      = _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(_M0L4partS1234);
      _M0L8_2afieldS4066 = _M0L7_2abindS1235->$0;
      _M0L3bufS3092 = _M0L8_2afieldS4066;
      _M0L8_2afieldS4065 = _M0L7_2abindS1235->$1;
      _M0L6_2acntS4463 = Moonbit_object_header(_M0L7_2abindS1235)->rc;
      if (_M0L6_2acntS4463 > 1) {
        int32_t _M0L11_2anew__cntS4464 = _M0L6_2acntS4463 - 1;
        Moonbit_object_header(_M0L7_2abindS1235)->rc = _M0L11_2anew__cntS4464;
        moonbit_incref(_M0L3bufS3092);
      } else if (_M0L6_2acntS4463 == 1) {
        #line 125 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
        moonbit_free(_M0L7_2abindS1235);
      }
      _M0L3lenS3093 = _M0L8_2afieldS4065;
      _M0L6_2atmpS3091
      = (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE){
        0, _M0L3lenS3093, _M0L3bufS3092
      };
      moonbit_incref(_M0L5partsS1230);
      #line 125 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
      _M0MPC15array5Array6appendGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5partsS1230, _M0L6_2atmpS3091);
      _M0L6_2atmpS3094 = _M0L2__S1233 + 1;
      _M0L2__S1233 = _M0L6_2atmpS3094;
      continue;
    } else {
      moonbit_decref(_M0L4selfS1232);
    }
    break;
  }
  return _M0L5partsS1230;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(
  void* _M0L4selfS1229
) {
  void** _M0L6_2atmpS3090;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _block_4625;
  #line 111 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3090 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3090[0] = _M0L4selfS1229;
  _block_4625
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
  Moonbit_object_header(_block_4625)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
  _block_4625->$0 = _M0L6_2atmpS3090;
  _block_4625->$1 = 1;
  return _block_4625;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0IPC16string6StringP48clawteam8clawteam8internal6openai35ToChatCompletionMessageParamContent45to__chat__completion__message__param__content(
  moonbit_string_t _M0L4selfS1228
) {
  void* _M0L6_2atmpS3089;
  void** _M0L6_2atmpS3088;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _block_4626;
  #line 97 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  #line 100 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3089
  = _M0FP48clawteam8clawteam8internal6openai19text__content__part(_M0L4selfS1228, 4294967296ll);
  _M0L6_2atmpS3088 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3088[0] = _M0L6_2atmpS3089;
  _block_4626
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE));
  Moonbit_object_header(_block_4626)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE, $0) >> 2, 1, 0);
  _block_4626->$0 = _M0L6_2atmpS3088;
  _block_4626->$1 = 1;
  return _block_4626;
}

void* _M0FP48clawteam8clawteam8internal6openai19text__content__part(
  moonbit_string_t _M0L4textS1226,
  int64_t _M0L14cache__controlS1227
) {
  struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam* _M0L6_2atmpS3087;
  void* _block_4627;
  #line 82 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3087
  = (struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam));
  Moonbit_object_header(_M0L6_2atmpS3087)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai34ChatCompletionContentPartTextParam, $0) >> 2, 1, 0);
  _M0L6_2atmpS3087->$0 = _M0L4textS1226;
  _M0L6_2atmpS3087->$1 = _M0L14cache__controlS1227;
  _block_4627
  = (void*)moonbit_malloc(sizeof(struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text));
  Moonbit_object_header(_block_4627)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text, $0) >> 2, 1, 0);
  ((struct _M0DTP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParam4Text*)_block_4627)->$0
  = _M0L6_2atmpS3087;
  return _block_4627;
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1221,
  void* _M0L7contentS1223,
  moonbit_string_t _M0L3locS1217,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1219
) {
  moonbit_string_t _M0L3locS1216;
  moonbit_string_t _M0L9args__locS1218;
  void* _M0L6_2atmpS3085;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3086;
  moonbit_string_t _M0L6actualS1220;
  moonbit_string_t _M0L4wantS1222;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1216 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1217);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1218 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1219);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3085 = _M0L3objS1221.$0->$method_0(_M0L3objS1221.$1);
  _M0L6_2atmpS3086 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1220
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3085, 0, 0, _M0L6_2atmpS3086);
  if (_M0L7contentS1223 == 0) {
    void* _M0L6_2atmpS3082;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3083;
    if (_M0L7contentS1223) {
      moonbit_decref(_M0L7contentS1223);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3082
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS3083 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1222
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3082, 0, 0, _M0L6_2atmpS3083);
  } else {
    void* _M0L7_2aSomeS1224 = _M0L7contentS1223;
    void* _M0L4_2axS1225 = _M0L7_2aSomeS1224;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3084 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1222
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1225, 0, 0, _M0L6_2atmpS3084);
  }
  moonbit_incref(_M0L4wantS1222);
  moonbit_incref(_M0L6actualS1220);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1220, _M0L4wantS1222)
  ) {
    moonbit_string_t _M0L6_2atmpS3080;
    moonbit_string_t _M0L6_2atmpS4076;
    moonbit_string_t _M0L6_2atmpS3079;
    moonbit_string_t _M0L6_2atmpS4075;
    moonbit_string_t _M0L6_2atmpS3077;
    moonbit_string_t _M0L6_2atmpS3078;
    moonbit_string_t _M0L6_2atmpS4074;
    moonbit_string_t _M0L6_2atmpS3076;
    moonbit_string_t _M0L6_2atmpS4073;
    moonbit_string_t _M0L6_2atmpS3073;
    moonbit_string_t _M0L6_2atmpS3075;
    moonbit_string_t _M0L6_2atmpS3074;
    moonbit_string_t _M0L6_2atmpS4072;
    moonbit_string_t _M0L6_2atmpS3072;
    moonbit_string_t _M0L6_2atmpS4071;
    moonbit_string_t _M0L6_2atmpS3069;
    moonbit_string_t _M0L6_2atmpS3071;
    moonbit_string_t _M0L6_2atmpS3070;
    moonbit_string_t _M0L6_2atmpS4070;
    moonbit_string_t _M0L6_2atmpS3068;
    moonbit_string_t _M0L6_2atmpS4069;
    moonbit_string_t _M0L6_2atmpS3067;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3066;
    struct moonbit_result_0 _result_4628;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3080
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1216);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4076
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_58.data, _M0L6_2atmpS3080);
    moonbit_decref(_M0L6_2atmpS3080);
    _M0L6_2atmpS3079 = _M0L6_2atmpS4076;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4075
    = moonbit_add_string(_M0L6_2atmpS3079, (moonbit_string_t)moonbit_string_literal_59.data);
    moonbit_decref(_M0L6_2atmpS3079);
    _M0L6_2atmpS3077 = _M0L6_2atmpS4075;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3078
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1218);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4074 = moonbit_add_string(_M0L6_2atmpS3077, _M0L6_2atmpS3078);
    moonbit_decref(_M0L6_2atmpS3077);
    moonbit_decref(_M0L6_2atmpS3078);
    _M0L6_2atmpS3076 = _M0L6_2atmpS4074;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4073
    = moonbit_add_string(_M0L6_2atmpS3076, (moonbit_string_t)moonbit_string_literal_60.data);
    moonbit_decref(_M0L6_2atmpS3076);
    _M0L6_2atmpS3073 = _M0L6_2atmpS4073;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3075 = _M0MPC16string6String6escape(_M0L4wantS1222);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3074
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3075);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4072 = moonbit_add_string(_M0L6_2atmpS3073, _M0L6_2atmpS3074);
    moonbit_decref(_M0L6_2atmpS3073);
    moonbit_decref(_M0L6_2atmpS3074);
    _M0L6_2atmpS3072 = _M0L6_2atmpS4072;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4071
    = moonbit_add_string(_M0L6_2atmpS3072, (moonbit_string_t)moonbit_string_literal_61.data);
    moonbit_decref(_M0L6_2atmpS3072);
    _M0L6_2atmpS3069 = _M0L6_2atmpS4071;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3071 = _M0MPC16string6String6escape(_M0L6actualS1220);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3070
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3071);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4070 = moonbit_add_string(_M0L6_2atmpS3069, _M0L6_2atmpS3070);
    moonbit_decref(_M0L6_2atmpS3069);
    moonbit_decref(_M0L6_2atmpS3070);
    _M0L6_2atmpS3068 = _M0L6_2atmpS4070;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS4069
    = moonbit_add_string(_M0L6_2atmpS3068, (moonbit_string_t)moonbit_string_literal_62.data);
    moonbit_decref(_M0L6_2atmpS3068);
    _M0L6_2atmpS3067 = _M0L6_2atmpS4069;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3066
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3066)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3066)->$0
    = _M0L6_2atmpS3067;
    _result_4628.tag = 0;
    _result_4628.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3066;
    return _result_4628;
  } else {
    int32_t _M0L6_2atmpS3081;
    struct moonbit_result_0 _result_4629;
    moonbit_decref(_M0L4wantS1222);
    moonbit_decref(_M0L6actualS1220);
    moonbit_decref(_M0L9args__locS1218);
    moonbit_decref(_M0L3locS1216);
    _M0L6_2atmpS3081 = 0;
    _result_4629.tag = 1;
    _result_4629.data.ok = _M0L6_2atmpS3081;
    return _result_4629;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1215,
  int32_t _M0L13escape__slashS1187,
  int32_t _M0L6indentS1182,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1208
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1174;
  void** _M0L6_2atmpS3065;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1175;
  int32_t _M0Lm5depthS1176;
  void* _M0L6_2atmpS3064;
  void* _M0L8_2aparamS1177;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1174 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3065 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1175
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1175)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1175->$0 = _M0L6_2atmpS3065;
  _M0L5stackS1175->$1 = 0;
  _M0Lm5depthS1176 = 0;
  _M0L6_2atmpS3064 = _M0L4selfS1215;
  _M0L8_2aparamS1177 = _M0L6_2atmpS3064;
  _2aloop_1193:;
  while (1) {
    if (_M0L8_2aparamS1177 == 0) {
      int32_t _M0L3lenS3026;
      if (_M0L8_2aparamS1177) {
        moonbit_decref(_M0L8_2aparamS1177);
      }
      _M0L3lenS3026 = _M0L5stackS1175->$1;
      if (_M0L3lenS3026 == 0) {
        if (_M0L8replacerS1208) {
          moonbit_decref(_M0L8replacerS1208);
        }
        moonbit_decref(_M0L5stackS1175);
        break;
      } else {
        void** _M0L8_2afieldS4084 = _M0L5stackS1175->$0;
        void** _M0L3bufS3050 = _M0L8_2afieldS4084;
        int32_t _M0L3lenS3052 = _M0L5stackS1175->$1;
        int32_t _M0L6_2atmpS3051 = _M0L3lenS3052 - 1;
        void* _M0L6_2atmpS4083 = (void*)_M0L3bufS3050[_M0L6_2atmpS3051];
        void* _M0L4_2axS1194 = _M0L6_2atmpS4083;
        switch (Moonbit_object_tag(_M0L4_2axS1194)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1195 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1194;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS4079 =
              _M0L8_2aArrayS1195->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1196 =
              _M0L8_2afieldS4079;
            int32_t _M0L4_2aiS1197 = _M0L8_2aArrayS1195->$1;
            int32_t _M0L3lenS3038 = _M0L6_2aarrS1196->$1;
            if (_M0L4_2aiS1197 < _M0L3lenS3038) {
              int32_t _if__result_4631;
              void** _M0L8_2afieldS4078;
              void** _M0L3bufS3044;
              void* _M0L6_2atmpS4077;
              void* _M0L7elementS1198;
              int32_t _M0L6_2atmpS3039;
              void* _M0L6_2atmpS3042;
              if (_M0L4_2aiS1197 < 0) {
                _if__result_4631 = 1;
              } else {
                int32_t _M0L3lenS3043 = _M0L6_2aarrS1196->$1;
                _if__result_4631 = _M0L4_2aiS1197 >= _M0L3lenS3043;
              }
              if (_if__result_4631) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS4078 = _M0L6_2aarrS1196->$0;
              _M0L3bufS3044 = _M0L8_2afieldS4078;
              _M0L6_2atmpS4077 = (void*)_M0L3bufS3044[_M0L4_2aiS1197];
              _M0L7elementS1198 = _M0L6_2atmpS4077;
              _M0L6_2atmpS3039 = _M0L4_2aiS1197 + 1;
              _M0L8_2aArrayS1195->$1 = _M0L6_2atmpS3039;
              if (_M0L4_2aiS1197 > 0) {
                int32_t _M0L6_2atmpS3041;
                moonbit_string_t _M0L6_2atmpS3040;
                moonbit_incref(_M0L7elementS1198);
                moonbit_incref(_M0L3bufS1174);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 44);
                _M0L6_2atmpS3041 = _M0Lm5depthS1176;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3040
                = _M0FPC14json11indent__str(_M0L6_2atmpS3041, _M0L6indentS1182);
                moonbit_incref(_M0L3bufS1174);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3040);
              } else {
                moonbit_incref(_M0L7elementS1198);
              }
              _M0L6_2atmpS3042 = _M0L7elementS1198;
              _M0L8_2aparamS1177 = _M0L6_2atmpS3042;
              goto _2aloop_1193;
            } else {
              int32_t _M0L6_2atmpS3045 = _M0Lm5depthS1176;
              void* _M0L6_2atmpS3046;
              int32_t _M0L6_2atmpS3048;
              moonbit_string_t _M0L6_2atmpS3047;
              void* _M0L6_2atmpS3049;
              _M0Lm5depthS1176 = _M0L6_2atmpS3045 - 1;
              moonbit_incref(_M0L5stackS1175);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3046
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1175);
              if (_M0L6_2atmpS3046) {
                moonbit_decref(_M0L6_2atmpS3046);
              }
              _M0L6_2atmpS3048 = _M0Lm5depthS1176;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3047
              = _M0FPC14json11indent__str(_M0L6_2atmpS3048, _M0L6indentS1182);
              moonbit_incref(_M0L3bufS1174);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3047);
              moonbit_incref(_M0L3bufS1174);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 93);
              _M0L6_2atmpS3049 = 0;
              _M0L8_2aparamS1177 = _M0L6_2atmpS3049;
              goto _2aloop_1193;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1199 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1194;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS4082 =
              _M0L9_2aObjectS1199->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1200 =
              _M0L8_2afieldS4082;
            int32_t _M0L8_2afirstS1201 = _M0L9_2aObjectS1199->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1202;
            moonbit_incref(_M0L11_2aiteratorS1200);
            moonbit_incref(_M0L9_2aObjectS1199);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1202
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1200);
            if (_M0L7_2abindS1202 == 0) {
              int32_t _M0L6_2atmpS3027;
              void* _M0L6_2atmpS3028;
              int32_t _M0L6_2atmpS3030;
              moonbit_string_t _M0L6_2atmpS3029;
              void* _M0L6_2atmpS3031;
              if (_M0L7_2abindS1202) {
                moonbit_decref(_M0L7_2abindS1202);
              }
              moonbit_decref(_M0L9_2aObjectS1199);
              _M0L6_2atmpS3027 = _M0Lm5depthS1176;
              _M0Lm5depthS1176 = _M0L6_2atmpS3027 - 1;
              moonbit_incref(_M0L5stackS1175);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3028
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1175);
              if (_M0L6_2atmpS3028) {
                moonbit_decref(_M0L6_2atmpS3028);
              }
              _M0L6_2atmpS3030 = _M0Lm5depthS1176;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3029
              = _M0FPC14json11indent__str(_M0L6_2atmpS3030, _M0L6indentS1182);
              moonbit_incref(_M0L3bufS1174);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3029);
              moonbit_incref(_M0L3bufS1174);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 125);
              _M0L6_2atmpS3031 = 0;
              _M0L8_2aparamS1177 = _M0L6_2atmpS3031;
              goto _2aloop_1193;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1203 = _M0L7_2abindS1202;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1204 = _M0L7_2aSomeS1203;
              moonbit_string_t _M0L8_2afieldS4081 = _M0L4_2axS1204->$0;
              moonbit_string_t _M0L4_2akS1205 = _M0L8_2afieldS4081;
              void* _M0L8_2afieldS4080 = _M0L4_2axS1204->$1;
              int32_t _M0L6_2acntS4465 =
                Moonbit_object_header(_M0L4_2axS1204)->rc;
              void* _M0L4_2avS1206;
              void* _M0Lm2v2S1207;
              moonbit_string_t _M0L6_2atmpS3035;
              void* _M0L6_2atmpS3037;
              void* _M0L6_2atmpS3036;
              if (_M0L6_2acntS4465 > 1) {
                int32_t _M0L11_2anew__cntS4466 = _M0L6_2acntS4465 - 1;
                Moonbit_object_header(_M0L4_2axS1204)->rc
                = _M0L11_2anew__cntS4466;
                moonbit_incref(_M0L8_2afieldS4080);
                moonbit_incref(_M0L4_2akS1205);
              } else if (_M0L6_2acntS4465 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1204);
              }
              _M0L4_2avS1206 = _M0L8_2afieldS4080;
              _M0Lm2v2S1207 = _M0L4_2avS1206;
              if (_M0L8replacerS1208 == 0) {
                moonbit_incref(_M0Lm2v2S1207);
                moonbit_decref(_M0L4_2avS1206);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1209 =
                  _M0L8replacerS1208;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1210 =
                  _M0L7_2aSomeS1209;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1211 =
                  _M0L11_2areplacerS1210;
                void* _M0L7_2abindS1212;
                moonbit_incref(_M0L7_2afuncS1211);
                moonbit_incref(_M0L4_2akS1205);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1212
                = _M0L7_2afuncS1211->code(_M0L7_2afuncS1211, _M0L4_2akS1205, _M0L4_2avS1206);
                if (_M0L7_2abindS1212 == 0) {
                  void* _M0L6_2atmpS3032;
                  if (_M0L7_2abindS1212) {
                    moonbit_decref(_M0L7_2abindS1212);
                  }
                  moonbit_decref(_M0L4_2akS1205);
                  moonbit_decref(_M0L9_2aObjectS1199);
                  _M0L6_2atmpS3032 = 0;
                  _M0L8_2aparamS1177 = _M0L6_2atmpS3032;
                  goto _2aloop_1193;
                } else {
                  void* _M0L7_2aSomeS1213 = _M0L7_2abindS1212;
                  void* _M0L4_2avS1214 = _M0L7_2aSomeS1213;
                  _M0Lm2v2S1207 = _M0L4_2avS1214;
                }
              }
              if (!_M0L8_2afirstS1201) {
                int32_t _M0L6_2atmpS3034;
                moonbit_string_t _M0L6_2atmpS3033;
                moonbit_incref(_M0L3bufS1174);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 44);
                _M0L6_2atmpS3034 = _M0Lm5depthS1176;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3033
                = _M0FPC14json11indent__str(_M0L6_2atmpS3034, _M0L6indentS1182);
                moonbit_incref(_M0L3bufS1174);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3033);
              }
              moonbit_incref(_M0L3bufS1174);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3035
              = _M0FPC14json6escape(_M0L4_2akS1205, _M0L13escape__slashS1187);
              moonbit_incref(_M0L3bufS1174);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3035);
              moonbit_incref(_M0L3bufS1174);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 34);
              moonbit_incref(_M0L3bufS1174);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 58);
              if (_M0L6indentS1182 > 0) {
                moonbit_incref(_M0L3bufS1174);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 32);
              }
              _M0L9_2aObjectS1199->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1199);
              _M0L6_2atmpS3037 = _M0Lm2v2S1207;
              _M0L6_2atmpS3036 = _M0L6_2atmpS3037;
              _M0L8_2aparamS1177 = _M0L6_2atmpS3036;
              goto _2aloop_1193;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1178 = _M0L8_2aparamS1177;
      void* _M0L8_2avalueS1179 = _M0L7_2aSomeS1178;
      void* _M0L6_2atmpS3063;
      switch (Moonbit_object_tag(_M0L8_2avalueS1179)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1180 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1179;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS4085 =
            _M0L9_2aObjectS1180->$0;
          int32_t _M0L6_2acntS4467 =
            Moonbit_object_header(_M0L9_2aObjectS1180)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1181;
          if (_M0L6_2acntS4467 > 1) {
            int32_t _M0L11_2anew__cntS4468 = _M0L6_2acntS4467 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1180)->rc
            = _M0L11_2anew__cntS4468;
            moonbit_incref(_M0L8_2afieldS4085);
          } else if (_M0L6_2acntS4467 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1180);
          }
          _M0L10_2amembersS1181 = _M0L8_2afieldS4085;
          moonbit_incref(_M0L10_2amembersS1181);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1181)) {
            moonbit_decref(_M0L10_2amembersS1181);
            moonbit_incref(_M0L3bufS1174);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, (moonbit_string_t)moonbit_string_literal_63.data);
          } else {
            int32_t _M0L6_2atmpS3058 = _M0Lm5depthS1176;
            int32_t _M0L6_2atmpS3060;
            moonbit_string_t _M0L6_2atmpS3059;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3062;
            void* _M0L6ObjectS3061;
            _M0Lm5depthS1176 = _M0L6_2atmpS3058 + 1;
            moonbit_incref(_M0L3bufS1174);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 123);
            _M0L6_2atmpS3060 = _M0Lm5depthS1176;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3059
            = _M0FPC14json11indent__str(_M0L6_2atmpS3060, _M0L6indentS1182);
            moonbit_incref(_M0L3bufS1174);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3059);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3062
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1181);
            _M0L6ObjectS3061
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3061)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3061)->$0
            = _M0L6_2atmpS3062;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3061)->$1
            = 1;
            moonbit_incref(_M0L5stackS1175);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1175, _M0L6ObjectS3061);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1183 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1179;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS4086 =
            _M0L8_2aArrayS1183->$0;
          int32_t _M0L6_2acntS4469 =
            Moonbit_object_header(_M0L8_2aArrayS1183)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1184;
          if (_M0L6_2acntS4469 > 1) {
            int32_t _M0L11_2anew__cntS4470 = _M0L6_2acntS4469 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1183)->rc
            = _M0L11_2anew__cntS4470;
            moonbit_incref(_M0L8_2afieldS4086);
          } else if (_M0L6_2acntS4469 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1183);
          }
          _M0L6_2aarrS1184 = _M0L8_2afieldS4086;
          moonbit_incref(_M0L6_2aarrS1184);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1184)) {
            moonbit_decref(_M0L6_2aarrS1184);
            moonbit_incref(_M0L3bufS1174);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, (moonbit_string_t)moonbit_string_literal_64.data);
          } else {
            int32_t _M0L6_2atmpS3054 = _M0Lm5depthS1176;
            int32_t _M0L6_2atmpS3056;
            moonbit_string_t _M0L6_2atmpS3055;
            void* _M0L5ArrayS3057;
            _M0Lm5depthS1176 = _M0L6_2atmpS3054 + 1;
            moonbit_incref(_M0L3bufS1174);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 91);
            _M0L6_2atmpS3056 = _M0Lm5depthS1176;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3055
            = _M0FPC14json11indent__str(_M0L6_2atmpS3056, _M0L6indentS1182);
            moonbit_incref(_M0L3bufS1174);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3055);
            _M0L5ArrayS3057
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3057)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3057)->$0
            = _M0L6_2aarrS1184;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3057)->$1
            = 0;
            moonbit_incref(_M0L5stackS1175);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1175, _M0L5ArrayS3057);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1185 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1179;
          moonbit_string_t _M0L8_2afieldS4087 = _M0L9_2aStringS1185->$0;
          int32_t _M0L6_2acntS4471 =
            Moonbit_object_header(_M0L9_2aStringS1185)->rc;
          moonbit_string_t _M0L4_2asS1186;
          moonbit_string_t _M0L6_2atmpS3053;
          if (_M0L6_2acntS4471 > 1) {
            int32_t _M0L11_2anew__cntS4472 = _M0L6_2acntS4471 - 1;
            Moonbit_object_header(_M0L9_2aStringS1185)->rc
            = _M0L11_2anew__cntS4472;
            moonbit_incref(_M0L8_2afieldS4087);
          } else if (_M0L6_2acntS4471 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1185);
          }
          _M0L4_2asS1186 = _M0L8_2afieldS4087;
          moonbit_incref(_M0L3bufS1174);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3053
          = _M0FPC14json6escape(_M0L4_2asS1186, _M0L13escape__slashS1187);
          moonbit_incref(_M0L3bufS1174);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L6_2atmpS3053);
          moonbit_incref(_M0L3bufS1174);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1174, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1188 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1179;
          double _M0L4_2anS1189 = _M0L9_2aNumberS1188->$0;
          moonbit_string_t _M0L8_2afieldS4088 = _M0L9_2aNumberS1188->$1;
          int32_t _M0L6_2acntS4473 =
            Moonbit_object_header(_M0L9_2aNumberS1188)->rc;
          moonbit_string_t _M0L7_2areprS1190;
          if (_M0L6_2acntS4473 > 1) {
            int32_t _M0L11_2anew__cntS4474 = _M0L6_2acntS4473 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1188)->rc
            = _M0L11_2anew__cntS4474;
            if (_M0L8_2afieldS4088) {
              moonbit_incref(_M0L8_2afieldS4088);
            }
          } else if (_M0L6_2acntS4473 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1188);
          }
          _M0L7_2areprS1190 = _M0L8_2afieldS4088;
          if (_M0L7_2areprS1190 == 0) {
            if (_M0L7_2areprS1190) {
              moonbit_decref(_M0L7_2areprS1190);
            }
            moonbit_incref(_M0L3bufS1174);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1174, _M0L4_2anS1189);
          } else {
            moonbit_string_t _M0L7_2aSomeS1191 = _M0L7_2areprS1190;
            moonbit_string_t _M0L4_2arS1192 = _M0L7_2aSomeS1191;
            moonbit_incref(_M0L3bufS1174);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, _M0L4_2arS1192);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1174);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, (moonbit_string_t)moonbit_string_literal_65.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1174);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, (moonbit_string_t)moonbit_string_literal_66.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1179);
          moonbit_incref(_M0L3bufS1174);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1174, (moonbit_string_t)moonbit_string_literal_67.data);
          break;
        }
      }
      _M0L6_2atmpS3063 = 0;
      _M0L8_2aparamS1177 = _M0L6_2atmpS3063;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1174);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1173,
  int32_t _M0L6indentS1171
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1171 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1172 = _M0L6indentS1171 * _M0L5levelS1173;
    switch (_M0L6spacesS1172) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_68.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_69.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_70.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_71.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_72.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_73.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_74.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_75.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_76.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS3025;
        moonbit_string_t _M0L6_2atmpS4089;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3025
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_77.data, _M0L6spacesS1172);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS4089
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS3025);
        moonbit_decref(_M0L6_2atmpS3025);
        return _M0L6_2atmpS4089;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1163,
  int32_t _M0L13escape__slashS1168
) {
  int32_t _M0L6_2atmpS3024;
  struct _M0TPB13StringBuilder* _M0L3bufS1162;
  struct _M0TWEOc* _M0L5_2aitS1164;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3024 = Moonbit_array_length(_M0L3strS1163);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1162 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3024);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1164 = _M0MPC16string6String4iter(_M0L3strS1163);
  while (1) {
    int32_t _M0L7_2abindS1165;
    moonbit_incref(_M0L5_2aitS1164);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1165 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1164);
    if (_M0L7_2abindS1165 == -1) {
      moonbit_decref(_M0L5_2aitS1164);
    } else {
      int32_t _M0L7_2aSomeS1166 = _M0L7_2abindS1165;
      int32_t _M0L4_2acS1167 = _M0L7_2aSomeS1166;
      if (_M0L4_2acS1167 == 34) {
        moonbit_incref(_M0L3bufS1162);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_78.data);
      } else if (_M0L4_2acS1167 == 92) {
        moonbit_incref(_M0L3bufS1162);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_79.data);
      } else if (_M0L4_2acS1167 == 47) {
        if (_M0L13escape__slashS1168) {
          moonbit_incref(_M0L3bufS1162);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_80.data);
        } else {
          moonbit_incref(_M0L3bufS1162);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1162, _M0L4_2acS1167);
        }
      } else if (_M0L4_2acS1167 == 10) {
        moonbit_incref(_M0L3bufS1162);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_81.data);
      } else if (_M0L4_2acS1167 == 13) {
        moonbit_incref(_M0L3bufS1162);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_82.data);
      } else if (_M0L4_2acS1167 == 8) {
        moonbit_incref(_M0L3bufS1162);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_83.data);
      } else if (_M0L4_2acS1167 == 9) {
        moonbit_incref(_M0L3bufS1162);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_84.data);
      } else {
        int32_t _M0L4codeS1169 = _M0L4_2acS1167;
        if (_M0L4codeS1169 == 12) {
          moonbit_incref(_M0L3bufS1162);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_85.data);
        } else if (_M0L4codeS1169 < 32) {
          int32_t _M0L6_2atmpS3023;
          moonbit_string_t _M0L6_2atmpS3022;
          moonbit_incref(_M0L3bufS1162);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, (moonbit_string_t)moonbit_string_literal_86.data);
          _M0L6_2atmpS3023 = _M0L4codeS1169 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3022 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3023);
          moonbit_incref(_M0L3bufS1162);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1162, _M0L6_2atmpS3022);
        } else {
          moonbit_incref(_M0L3bufS1162);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1162, _M0L4_2acS1167);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1162);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1160
) {
  int32_t _M0L8_2afieldS4090;
  int32_t _M0L3lenS3020;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS4090 = _M0L4selfS1160->$1;
  moonbit_decref(_M0L4selfS1160);
  _M0L3lenS3020 = _M0L8_2afieldS4090;
  return _M0L3lenS3020 == 0;
}

int32_t _M0MPC15array5Array9is__emptyGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS1161
) {
  int32_t _M0L8_2afieldS4091;
  int32_t _M0L3lenS3021;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS4091 = _M0L4selfS1161->$1;
  moonbit_decref(_M0L4selfS1161);
  _M0L3lenS3021 = _M0L8_2afieldS4091;
  return _M0L3lenS3021 == 0;
}

int32_t _M0MPC15array5Array6appendGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS1159,
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L5otherS1158
) {
  int32_t _M0L3lenS3019;
  #line 415 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS3019 = _M0L4selfS1159->$1;
  #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0MPC15array9ArrayView16blit__to_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L5otherS1158, _M0L4selfS1159, _M0L3lenS3019);
  return 0;
}

int32_t _M0MPC15array9ArrayView16blit__to_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4selfS1155,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L3dstS1157,
  int32_t _M0L11dst__offsetS1156
) {
  int32_t _M0L3endS3017;
  int32_t _M0L5startS3018;
  int32_t _M0L3lenS1154;
  int32_t _if__result_4633;
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array_block.mbt"
  _M0L3endS3017 = _M0L4selfS1155.$2;
  _M0L5startS3018 = _M0L4selfS1155.$1;
  _M0L3lenS1154 = _M0L3endS3017 - _M0L5startS3018;
  if (_M0L11dst__offsetS1156 >= 0) {
    int32_t _M0L3lenS3010 = _M0L3dstS1157->$1;
    _if__result_4633 = _M0L11dst__offsetS1156 <= _M0L3lenS3010;
  } else {
    _if__result_4633 = 0;
  }
  if (_if__result_4633) {
    int32_t _M0L6_2atmpS3011 = _M0L11dst__offsetS1156 + _M0L3lenS1154;
    int32_t _M0L3lenS3012 = _M0L3dstS1157->$1;
    void** _M0L6_2atmpS3014;
    void** _M0L8_2afieldS4093;
    void** _M0L3bufS3015;
    int32_t _M0L8_2afieldS4092;
    int32_t _M0L5startS3016;
    if (_M0L6_2atmpS3011 > _M0L3lenS3012) {
      int32_t _M0L6_2atmpS3013 = _M0L11dst__offsetS1156 + _M0L3lenS1154;
      moonbit_incref(_M0L3dstS1157);
      #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array_block.mbt"
      _M0MPC15array5Array24unsafe__grow__to__lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L3dstS1157, _M0L6_2atmpS3013);
    }
    #line 202 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array_block.mbt"
    _M0L6_2atmpS3014
    = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L3dstS1157);
    _M0L8_2afieldS4093 = _M0L4selfS1155.$0;
    _M0L3bufS3015 = _M0L8_2afieldS4093;
    _M0L8_2afieldS4092 = _M0L4selfS1155.$1;
    _M0L5startS3016 = _M0L8_2afieldS4092;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array_block.mbt"
    _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L6_2atmpS3014, _M0L11dst__offsetS1156, _M0L3bufS3015, _M0L5startS3016, _M0L3lenS1154);
  } else {
    moonbit_decref(_M0L3dstS1157);
    moonbit_decref(_M0L4selfS1155.$0);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array_block.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC15array5Array24unsafe__grow__to__lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS1152,
  int32_t _M0L8new__lenS1151
) {
  int32_t _M0L3lenS3007;
  #line 417 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS3007 = _M0L4selfS1152->$1;
  if (_M0L8new__lenS1151 >= _M0L3lenS3007) {
    void** _M0L8new__bufS1153 =
      (void**)moonbit_make_ref_array(_M0L8new__lenS1151, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
    void** _M0L8_2afieldS4095 = _M0L4selfS1152->$0;
    void** _M0L3bufS3008 = _M0L8_2afieldS4095;
    int32_t _M0L3lenS3009 = _M0L4selfS1152->$1;
    void** _M0L6_2aoldS4094;
    moonbit_incref(_M0L3bufS3008);
    moonbit_incref(_M0L8new__bufS1153);
    #line 420 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L8new__bufS1153, 0, _M0L3bufS3008, 0, _M0L3lenS3009);
    _M0L4selfS1152->$1 = _M0L8new__lenS1151;
    _M0L6_2aoldS4094 = _M0L4selfS1152->$0;
    moonbit_decref(_M0L6_2aoldS4094);
    _M0L4selfS1152->$0 = _M0L8new__bufS1153;
    moonbit_decref(_M0L4selfS1152);
  } else {
    moonbit_decref(_M0L4selfS1152);
    #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_panic();
  }
  return 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1148
) {
  int32_t _M0L3lenS1147;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1147 = _M0L4selfS1148->$1;
  if (_M0L3lenS1147 == 0) {
    moonbit_decref(_M0L4selfS1148);
    return 0;
  } else {
    int32_t _M0L5indexS1149 = _M0L3lenS1147 - 1;
    void** _M0L8_2afieldS4099 = _M0L4selfS1148->$0;
    void** _M0L3bufS3006 = _M0L8_2afieldS4099;
    void* _M0L6_2atmpS4098 = (void*)_M0L3bufS3006[_M0L5indexS1149];
    void* _M0L1vS1150 = _M0L6_2atmpS4098;
    void** _M0L8_2afieldS4097 = _M0L4selfS1148->$0;
    void** _M0L3bufS3005 = _M0L8_2afieldS4097;
    void* _M0L6_2aoldS4096;
    if (
      _M0L5indexS1149 < 0
      || _M0L5indexS1149 >= Moonbit_array_length(_M0L3bufS3005)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4096 = (void*)_M0L3bufS3005[_M0L5indexS1149];
    moonbit_incref(_M0L1vS1150);
    moonbit_decref(_M0L6_2aoldS4096);
    if (
      _M0L5indexS1149 < 0
      || _M0L5indexS1149 >= Moonbit_array_length(_M0L3bufS3005)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3005[_M0L5indexS1149]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1148->$1 = _M0L5indexS1149;
    moonbit_decref(_M0L4selfS1148);
    return _M0L1vS1150;
  }
}

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0MPC15array5Array12view_2einnerGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS1138,
  int32_t _M0L5startS1144,
  int64_t _M0L3endS1140
) {
  int32_t _M0L3lenS1137;
  int32_t _M0L3endS1139;
  int32_t _M0L5startS1143;
  int32_t _if__result_4634;
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3lenS1137 = _M0L4selfS1138->$1;
  if (_M0L3endS1140 == 4294967296ll) {
    _M0L3endS1139 = _M0L3lenS1137;
  } else {
    int64_t _M0L7_2aSomeS1141 = _M0L3endS1140;
    int32_t _M0L6_2aendS1142 = (int32_t)_M0L7_2aSomeS1141;
    if (_M0L6_2aendS1142 < 0) {
      _M0L3endS1139 = _M0L3lenS1137 + _M0L6_2aendS1142;
    } else {
      _M0L3endS1139 = _M0L6_2aendS1142;
    }
  }
  if (_M0L5startS1144 < 0) {
    _M0L5startS1143 = _M0L3lenS1137 + _M0L5startS1144;
  } else {
    _M0L5startS1143 = _M0L5startS1144;
  }
  if (_M0L5startS1143 >= 0) {
    if (_M0L5startS1143 <= _M0L3endS1139) {
      _if__result_4634 = _M0L3endS1139 <= _M0L3lenS1137;
    } else {
      _if__result_4634 = 0;
    }
  } else {
    _if__result_4634 = 0;
  }
  if (_if__result_4634) {
    void** _M0L7_2abindS1145;
    int32_t _M0L7_2abindS1146;
    int32_t _M0L6_2atmpS3004;
    #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L7_2abindS1145
    = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS1138);
    _M0L7_2abindS1146 = _M0L3endS1139 - _M0L5startS1143;
    _M0L6_2atmpS3004 = _M0L5startS1143 + _M0L7_2abindS1146;
    return (struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE){_M0L5startS1143,
                                                                    _M0L6_2atmpS3004,
                                                                    _M0L7_2abindS1145};
  } else {
    moonbit_decref(_M0L4selfS1138);
    #line 263 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE((moonbit_string_t)moonbit_string_literal_87.data, (moonbit_string_t)moonbit_string_literal_88.data);
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1135,
  struct _M0TPB6Logger _M0L6loggerS1136
) {
  moonbit_string_t _M0L6_2atmpS3003;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS3002;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3003 = _M0L4selfS1135;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3002 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS3003);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS3002, _M0L6loggerS1136);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1112,
  struct _M0TPB6Logger _M0L6loggerS1134
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS4108;
  struct _M0TPC16string10StringView _M0L3pkgS1111;
  moonbit_string_t _M0L7_2adataS1113;
  int32_t _M0L8_2astartS1114;
  int32_t _M0L6_2atmpS3001;
  int32_t _M0L6_2aendS1115;
  int32_t _M0Lm9_2acursorS1116;
  int32_t _M0Lm13accept__stateS1117;
  int32_t _M0Lm10match__endS1118;
  int32_t _M0Lm20match__tag__saver__0S1119;
  int32_t _M0Lm6tag__0S1120;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1121;
  struct _M0TPC16string10StringView _M0L8_2afieldS4107;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1130;
  void* _M0L8_2afieldS4106;
  int32_t _M0L6_2acntS4475;
  void* _M0L16_2apackage__nameS1131;
  struct _M0TPC16string10StringView _M0L8_2afieldS4104;
  struct _M0TPC16string10StringView _M0L8filenameS2978;
  struct _M0TPC16string10StringView _M0L8_2afieldS4103;
  struct _M0TPC16string10StringView _M0L11start__lineS2979;
  struct _M0TPC16string10StringView _M0L8_2afieldS4102;
  struct _M0TPC16string10StringView _M0L13start__columnS2980;
  struct _M0TPC16string10StringView _M0L8_2afieldS4101;
  struct _M0TPC16string10StringView _M0L9end__lineS2981;
  struct _M0TPC16string10StringView _M0L8_2afieldS4100;
  int32_t _M0L6_2acntS4479;
  struct _M0TPC16string10StringView _M0L11end__columnS2982;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS4108
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1112->$0_1, _M0L4selfS1112->$0_2, _M0L4selfS1112->$0_0
  };
  _M0L3pkgS1111 = _M0L8_2afieldS4108;
  moonbit_incref(_M0L3pkgS1111.$0);
  moonbit_incref(_M0L3pkgS1111.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1113 = _M0MPC16string10StringView4data(_M0L3pkgS1111);
  moonbit_incref(_M0L3pkgS1111.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1114
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1111);
  moonbit_incref(_M0L3pkgS1111.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3001 = _M0MPC16string10StringView6length(_M0L3pkgS1111);
  _M0L6_2aendS1115 = _M0L8_2astartS1114 + _M0L6_2atmpS3001;
  _M0Lm9_2acursorS1116 = _M0L8_2astartS1114;
  _M0Lm13accept__stateS1117 = -1;
  _M0Lm10match__endS1118 = -1;
  _M0Lm20match__tag__saver__0S1119 = -1;
  _M0Lm6tag__0S1120 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2993 = _M0Lm9_2acursorS1116;
    if (_M0L6_2atmpS2993 < _M0L6_2aendS1115) {
      int32_t _M0L6_2atmpS3000 = _M0Lm9_2acursorS1116;
      int32_t _M0L10next__charS1125;
      int32_t _M0L6_2atmpS2994;
      moonbit_incref(_M0L7_2adataS1113);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1125
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1113, _M0L6_2atmpS3000);
      _M0L6_2atmpS2994 = _M0Lm9_2acursorS1116;
      _M0Lm9_2acursorS1116 = _M0L6_2atmpS2994 + 1;
      if (_M0L10next__charS1125 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2995;
          _M0Lm6tag__0S1120 = _M0Lm9_2acursorS1116;
          _M0L6_2atmpS2995 = _M0Lm9_2acursorS1116;
          if (_M0L6_2atmpS2995 < _M0L6_2aendS1115) {
            int32_t _M0L6_2atmpS2999 = _M0Lm9_2acursorS1116;
            int32_t _M0L10next__charS1126;
            int32_t _M0L6_2atmpS2996;
            moonbit_incref(_M0L7_2adataS1113);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1126
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1113, _M0L6_2atmpS2999);
            _M0L6_2atmpS2996 = _M0Lm9_2acursorS1116;
            _M0Lm9_2acursorS1116 = _M0L6_2atmpS2996 + 1;
            if (_M0L10next__charS1126 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2997 = _M0Lm9_2acursorS1116;
                if (_M0L6_2atmpS2997 < _M0L6_2aendS1115) {
                  int32_t _M0L6_2atmpS2998 = _M0Lm9_2acursorS1116;
                  _M0Lm9_2acursorS1116 = _M0L6_2atmpS2998 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1119 = _M0Lm6tag__0S1120;
                  _M0Lm13accept__stateS1117 = 0;
                  _M0Lm10match__endS1118 = _M0Lm9_2acursorS1116;
                  goto join_1122;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1122;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1122;
    }
    break;
  }
  goto joinlet_4635;
  join_1122:;
  switch (_M0Lm13accept__stateS1117) {
    case 0: {
      int32_t _M0L6_2atmpS2991;
      int32_t _M0L6_2atmpS2990;
      int64_t _M0L6_2atmpS2987;
      int32_t _M0L6_2atmpS2989;
      int64_t _M0L6_2atmpS2988;
      struct _M0TPC16string10StringView _M0L13package__nameS1123;
      int64_t _M0L6_2atmpS2984;
      int32_t _M0L6_2atmpS2986;
      int64_t _M0L6_2atmpS2985;
      struct _M0TPC16string10StringView _M0L12module__nameS1124;
      void* _M0L4SomeS2983;
      moonbit_decref(_M0L3pkgS1111.$0);
      _M0L6_2atmpS2991 = _M0Lm20match__tag__saver__0S1119;
      _M0L6_2atmpS2990 = _M0L6_2atmpS2991 + 1;
      _M0L6_2atmpS2987 = (int64_t)_M0L6_2atmpS2990;
      _M0L6_2atmpS2989 = _M0Lm10match__endS1118;
      _M0L6_2atmpS2988 = (int64_t)_M0L6_2atmpS2989;
      moonbit_incref(_M0L7_2adataS1113);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1123
      = _M0MPC16string6String4view(_M0L7_2adataS1113, _M0L6_2atmpS2987, _M0L6_2atmpS2988);
      _M0L6_2atmpS2984 = (int64_t)_M0L8_2astartS1114;
      _M0L6_2atmpS2986 = _M0Lm20match__tag__saver__0S1119;
      _M0L6_2atmpS2985 = (int64_t)_M0L6_2atmpS2986;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1124
      = _M0MPC16string6String4view(_M0L7_2adataS1113, _M0L6_2atmpS2984, _M0L6_2atmpS2985);
      _M0L4SomeS2983
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2983)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2983)->$0_0
      = _M0L13package__nameS1123.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2983)->$0_1
      = _M0L13package__nameS1123.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2983)->$0_2
      = _M0L13package__nameS1123.$2;
      _M0L7_2abindS1121
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1121)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1121->$0_0 = _M0L12module__nameS1124.$0;
      _M0L7_2abindS1121->$0_1 = _M0L12module__nameS1124.$1;
      _M0L7_2abindS1121->$0_2 = _M0L12module__nameS1124.$2;
      _M0L7_2abindS1121->$1 = _M0L4SomeS2983;
      break;
    }
    default: {
      void* _M0L4NoneS2992;
      moonbit_decref(_M0L7_2adataS1113);
      _M0L4NoneS2992
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1121
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1121)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1121->$0_0 = _M0L3pkgS1111.$0;
      _M0L7_2abindS1121->$0_1 = _M0L3pkgS1111.$1;
      _M0L7_2abindS1121->$0_2 = _M0L3pkgS1111.$2;
      _M0L7_2abindS1121->$1 = _M0L4NoneS2992;
      break;
    }
  }
  joinlet_4635:;
  _M0L8_2afieldS4107
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1121->$0_1, _M0L7_2abindS1121->$0_2, _M0L7_2abindS1121->$0_0
  };
  _M0L15_2amodule__nameS1130 = _M0L8_2afieldS4107;
  _M0L8_2afieldS4106 = _M0L7_2abindS1121->$1;
  _M0L6_2acntS4475 = Moonbit_object_header(_M0L7_2abindS1121)->rc;
  if (_M0L6_2acntS4475 > 1) {
    int32_t _M0L11_2anew__cntS4476 = _M0L6_2acntS4475 - 1;
    Moonbit_object_header(_M0L7_2abindS1121)->rc = _M0L11_2anew__cntS4476;
    moonbit_incref(_M0L8_2afieldS4106);
    moonbit_incref(_M0L15_2amodule__nameS1130.$0);
  } else if (_M0L6_2acntS4475 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1121);
  }
  _M0L16_2apackage__nameS1131 = _M0L8_2afieldS4106;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1131)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1132 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1131;
      struct _M0TPC16string10StringView _M0L8_2afieldS4105 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1132->$0_1,
                                              _M0L7_2aSomeS1132->$0_2,
                                              _M0L7_2aSomeS1132->$0_0};
      int32_t _M0L6_2acntS4477 = Moonbit_object_header(_M0L7_2aSomeS1132)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1133;
      if (_M0L6_2acntS4477 > 1) {
        int32_t _M0L11_2anew__cntS4478 = _M0L6_2acntS4477 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1132)->rc = _M0L11_2anew__cntS4478;
        moonbit_incref(_M0L8_2afieldS4105.$0);
      } else if (_M0L6_2acntS4477 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1132);
      }
      _M0L12_2apkg__nameS1133 = _M0L8_2afieldS4105;
      if (_M0L6loggerS1134.$1) {
        moonbit_incref(_M0L6loggerS1134.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L12_2apkg__nameS1133);
      if (_M0L6loggerS1134.$1) {
        moonbit_incref(_M0L6loggerS1134.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1134.$0->$method_3(_M0L6loggerS1134.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1131);
      break;
    }
  }
  _M0L8_2afieldS4104
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1112->$1_1, _M0L4selfS1112->$1_2, _M0L4selfS1112->$1_0
  };
  _M0L8filenameS2978 = _M0L8_2afieldS4104;
  moonbit_incref(_M0L8filenameS2978.$0);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L8filenameS2978);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_3(_M0L6loggerS1134.$1, 58);
  _M0L8_2afieldS4103
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1112->$2_1, _M0L4selfS1112->$2_2, _M0L4selfS1112->$2_0
  };
  _M0L11start__lineS2979 = _M0L8_2afieldS4103;
  moonbit_incref(_M0L11start__lineS2979.$0);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L11start__lineS2979);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_3(_M0L6loggerS1134.$1, 58);
  _M0L8_2afieldS4102
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1112->$3_1, _M0L4selfS1112->$3_2, _M0L4selfS1112->$3_0
  };
  _M0L13start__columnS2980 = _M0L8_2afieldS4102;
  moonbit_incref(_M0L13start__columnS2980.$0);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L13start__columnS2980);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_3(_M0L6loggerS1134.$1, 45);
  _M0L8_2afieldS4101
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1112->$4_1, _M0L4selfS1112->$4_2, _M0L4selfS1112->$4_0
  };
  _M0L9end__lineS2981 = _M0L8_2afieldS4101;
  moonbit_incref(_M0L9end__lineS2981.$0);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L9end__lineS2981);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_3(_M0L6loggerS1134.$1, 58);
  _M0L8_2afieldS4100
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1112->$5_1, _M0L4selfS1112->$5_2, _M0L4selfS1112->$5_0
  };
  _M0L6_2acntS4479 = Moonbit_object_header(_M0L4selfS1112)->rc;
  if (_M0L6_2acntS4479 > 1) {
    int32_t _M0L11_2anew__cntS4485 = _M0L6_2acntS4479 - 1;
    Moonbit_object_header(_M0L4selfS1112)->rc = _M0L11_2anew__cntS4485;
    moonbit_incref(_M0L8_2afieldS4100.$0);
  } else if (_M0L6_2acntS4479 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4484 =
      (struct _M0TPC16string10StringView){_M0L4selfS1112->$4_1,
                                            _M0L4selfS1112->$4_2,
                                            _M0L4selfS1112->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4483;
    struct _M0TPC16string10StringView _M0L8_2afieldS4482;
    struct _M0TPC16string10StringView _M0L8_2afieldS4481;
    struct _M0TPC16string10StringView _M0L8_2afieldS4480;
    moonbit_decref(_M0L8_2afieldS4484.$0);
    _M0L8_2afieldS4483
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1112->$3_1, _M0L4selfS1112->$3_2, _M0L4selfS1112->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4483.$0);
    _M0L8_2afieldS4482
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1112->$2_1, _M0L4selfS1112->$2_2, _M0L4selfS1112->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4482.$0);
    _M0L8_2afieldS4481
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1112->$1_1, _M0L4selfS1112->$1_2, _M0L4selfS1112->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4481.$0);
    _M0L8_2afieldS4480
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1112->$0_1, _M0L4selfS1112->$0_2, _M0L4selfS1112->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4480.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1112);
  }
  _M0L11end__columnS2982 = _M0L8_2afieldS4100;
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L11end__columnS2982);
  if (_M0L6loggerS1134.$1) {
    moonbit_incref(_M0L6loggerS1134.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_3(_M0L6loggerS1134.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1134.$0->$method_2(_M0L6loggerS1134.$1, _M0L15_2amodule__nameS1130);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1110) {
  moonbit_string_t _M0L6_2atmpS2977;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2977
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1110);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2977);
  moonbit_decref(_M0L6_2atmpS2977);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1109,
  struct _M0TPB6Logger _M0L6loggerS1108
) {
  moonbit_string_t _M0L6_2atmpS2976;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2976 = _M0MPC16double6Double10to__string(_M0L4selfS1109);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1108.$0->$method_0(_M0L6loggerS1108.$1, _M0L6_2atmpS2976);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1107) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1107);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1094) {
  uint64_t _M0L4bitsS1095;
  uint64_t _M0L6_2atmpS2975;
  uint64_t _M0L6_2atmpS2974;
  int32_t _M0L8ieeeSignS1096;
  uint64_t _M0L12ieeeMantissaS1097;
  uint64_t _M0L6_2atmpS2973;
  uint64_t _M0L6_2atmpS2972;
  int32_t _M0L12ieeeExponentS1098;
  int32_t _if__result_4639;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1099;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1100;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2971;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1094 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_89.data;
  }
  _M0L4bitsS1095 = *(int64_t*)&_M0L3valS1094;
  _M0L6_2atmpS2975 = _M0L4bitsS1095 >> 63;
  _M0L6_2atmpS2974 = _M0L6_2atmpS2975 & 1ull;
  _M0L8ieeeSignS1096 = _M0L6_2atmpS2974 != 0ull;
  _M0L12ieeeMantissaS1097 = _M0L4bitsS1095 & 4503599627370495ull;
  _M0L6_2atmpS2973 = _M0L4bitsS1095 >> 52;
  _M0L6_2atmpS2972 = _M0L6_2atmpS2973 & 2047ull;
  _M0L12ieeeExponentS1098 = (int32_t)_M0L6_2atmpS2972;
  if (_M0L12ieeeExponentS1098 == 2047) {
    _if__result_4639 = 1;
  } else if (_M0L12ieeeExponentS1098 == 0) {
    _if__result_4639 = _M0L12ieeeMantissaS1097 == 0ull;
  } else {
    _if__result_4639 = 0;
  }
  if (_if__result_4639) {
    int32_t _M0L6_2atmpS2960 = _M0L12ieeeExponentS1098 != 0;
    int32_t _M0L6_2atmpS2961 = _M0L12ieeeMantissaS1097 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1096, _M0L6_2atmpS2960, _M0L6_2atmpS2961);
  }
  _M0Lm1vS1099 = _M0FPB31ryu__to__string_2erecord_2f1093;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1100
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1097, _M0L12ieeeExponentS1098);
  if (_M0L5smallS1100 == 0) {
    uint32_t _M0L6_2atmpS2962;
    if (_M0L5smallS1100) {
      moonbit_decref(_M0L5smallS1100);
    }
    _M0L6_2atmpS2962 = *(uint32_t*)&_M0L12ieeeExponentS1098;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1099 = _M0FPB3d2d(_M0L12ieeeMantissaS1097, _M0L6_2atmpS2962);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1101 = _M0L5smallS1100;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1102 = _M0L7_2aSomeS1101;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1103 = _M0L4_2afS1102;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2970 = _M0Lm1xS1103;
      uint64_t _M0L8_2afieldS4111 = _M0L6_2atmpS2970->$0;
      uint64_t _M0L8mantissaS2969 = _M0L8_2afieldS4111;
      uint64_t _M0L1qS1104 = _M0L8mantissaS2969 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2968 = _M0Lm1xS1103;
      uint64_t _M0L8_2afieldS4110 = _M0L6_2atmpS2968->$0;
      uint64_t _M0L8mantissaS2966 = _M0L8_2afieldS4110;
      uint64_t _M0L6_2atmpS2967 = 10ull * _M0L1qS1104;
      uint64_t _M0L1rS1105 = _M0L8mantissaS2966 - _M0L6_2atmpS2967;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2965;
      int32_t _M0L8_2afieldS4109;
      int32_t _M0L8exponentS2964;
      int32_t _M0L6_2atmpS2963;
      if (_M0L1rS1105 != 0ull) {
        break;
      }
      _M0L6_2atmpS2965 = _M0Lm1xS1103;
      _M0L8_2afieldS4109 = _M0L6_2atmpS2965->$1;
      moonbit_decref(_M0L6_2atmpS2965);
      _M0L8exponentS2964 = _M0L8_2afieldS4109;
      _M0L6_2atmpS2963 = _M0L8exponentS2964 + 1;
      _M0Lm1xS1103
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1103)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1103->$0 = _M0L1qS1104;
      _M0Lm1xS1103->$1 = _M0L6_2atmpS2963;
      continue;
      break;
    }
    _M0Lm1vS1099 = _M0Lm1xS1103;
  }
  _M0L6_2atmpS2971 = _M0Lm1vS1099;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2971, _M0L8ieeeSignS1096);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1088,
  int32_t _M0L12ieeeExponentS1090
) {
  uint64_t _M0L2m2S1087;
  int32_t _M0L6_2atmpS2959;
  int32_t _M0L2e2S1089;
  int32_t _M0L6_2atmpS2958;
  uint64_t _M0L6_2atmpS2957;
  uint64_t _M0L4maskS1091;
  uint64_t _M0L8fractionS1092;
  int32_t _M0L6_2atmpS2956;
  uint64_t _M0L6_2atmpS2955;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2954;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1087 = 4503599627370496ull | _M0L12ieeeMantissaS1088;
  _M0L6_2atmpS2959 = _M0L12ieeeExponentS1090 - 1023;
  _M0L2e2S1089 = _M0L6_2atmpS2959 - 52;
  if (_M0L2e2S1089 > 0) {
    return 0;
  }
  if (_M0L2e2S1089 < -52) {
    return 0;
  }
  _M0L6_2atmpS2958 = -_M0L2e2S1089;
  _M0L6_2atmpS2957 = 1ull << (_M0L6_2atmpS2958 & 63);
  _M0L4maskS1091 = _M0L6_2atmpS2957 - 1ull;
  _M0L8fractionS1092 = _M0L2m2S1087 & _M0L4maskS1091;
  if (_M0L8fractionS1092 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2956 = -_M0L2e2S1089;
  _M0L6_2atmpS2955 = _M0L2m2S1087 >> (_M0L6_2atmpS2956 & 63);
  _M0L6_2atmpS2954
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2954)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2954->$0 = _M0L6_2atmpS2955;
  _M0L6_2atmpS2954->$1 = 0;
  return _M0L6_2atmpS2954;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1061,
  int32_t _M0L4signS1059
) {
  int32_t _M0L6_2atmpS2953;
  moonbit_bytes_t _M0L6resultS1057;
  int32_t _M0Lm5indexS1058;
  uint64_t _M0Lm6outputS1060;
  uint64_t _M0L6_2atmpS2952;
  int32_t _M0L7olengthS1062;
  int32_t _M0L8_2afieldS4112;
  int32_t _M0L8exponentS2951;
  int32_t _M0L6_2atmpS2950;
  int32_t _M0Lm3expS1063;
  int32_t _M0L6_2atmpS2949;
  int32_t _M0L6_2atmpS2947;
  int32_t _M0L18scientificNotationS1064;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2953 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1057
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2953);
  _M0Lm5indexS1058 = 0;
  if (_M0L4signS1059) {
    int32_t _M0L6_2atmpS2822 = _M0Lm5indexS1058;
    int32_t _M0L6_2atmpS2823;
    if (
      _M0L6_2atmpS2822 < 0
      || _M0L6_2atmpS2822 >= Moonbit_array_length(_M0L6resultS1057)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1057[_M0L6_2atmpS2822] = 45;
    _M0L6_2atmpS2823 = _M0Lm5indexS1058;
    _M0Lm5indexS1058 = _M0L6_2atmpS2823 + 1;
  }
  _M0Lm6outputS1060 = _M0L1vS1061->$0;
  _M0L6_2atmpS2952 = _M0Lm6outputS1060;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1062 = _M0FPB17decimal__length17(_M0L6_2atmpS2952);
  _M0L8_2afieldS4112 = _M0L1vS1061->$1;
  moonbit_decref(_M0L1vS1061);
  _M0L8exponentS2951 = _M0L8_2afieldS4112;
  _M0L6_2atmpS2950 = _M0L8exponentS2951 + _M0L7olengthS1062;
  _M0Lm3expS1063 = _M0L6_2atmpS2950 - 1;
  _M0L6_2atmpS2949 = _M0Lm3expS1063;
  if (_M0L6_2atmpS2949 >= -6) {
    int32_t _M0L6_2atmpS2948 = _M0Lm3expS1063;
    _M0L6_2atmpS2947 = _M0L6_2atmpS2948 < 21;
  } else {
    _M0L6_2atmpS2947 = 0;
  }
  _M0L18scientificNotationS1064 = !_M0L6_2atmpS2947;
  if (_M0L18scientificNotationS1064) {
    int32_t _M0L7_2abindS1065 = _M0L7olengthS1062 - 1;
    int32_t _M0L1iS1066 = 0;
    int32_t _M0L6_2atmpS2833;
    uint64_t _M0L6_2atmpS2838;
    int32_t _M0L6_2atmpS2837;
    int32_t _M0L6_2atmpS2836;
    int32_t _M0L6_2atmpS2835;
    int32_t _M0L6_2atmpS2834;
    int32_t _M0L6_2atmpS2842;
    int32_t _M0L6_2atmpS2843;
    int32_t _M0L6_2atmpS2844;
    int32_t _M0L6_2atmpS2845;
    int32_t _M0L6_2atmpS2846;
    int32_t _M0L6_2atmpS2852;
    int32_t _M0L6_2atmpS2885;
    while (1) {
      if (_M0L1iS1066 < _M0L7_2abindS1065) {
        uint64_t _M0L6_2atmpS2831 = _M0Lm6outputS1060;
        uint64_t _M0L1cS1067 = _M0L6_2atmpS2831 % 10ull;
        uint64_t _M0L6_2atmpS2824 = _M0Lm6outputS1060;
        int32_t _M0L6_2atmpS2830;
        int32_t _M0L6_2atmpS2829;
        int32_t _M0L6_2atmpS2825;
        int32_t _M0L6_2atmpS2828;
        int32_t _M0L6_2atmpS2827;
        int32_t _M0L6_2atmpS2826;
        int32_t _M0L6_2atmpS2832;
        _M0Lm6outputS1060 = _M0L6_2atmpS2824 / 10ull;
        _M0L6_2atmpS2830 = _M0Lm5indexS1058;
        _M0L6_2atmpS2829 = _M0L6_2atmpS2830 + _M0L7olengthS1062;
        _M0L6_2atmpS2825 = _M0L6_2atmpS2829 - _M0L1iS1066;
        _M0L6_2atmpS2828 = (int32_t)_M0L1cS1067;
        _M0L6_2atmpS2827 = 48 + _M0L6_2atmpS2828;
        _M0L6_2atmpS2826 = _M0L6_2atmpS2827 & 0xff;
        if (
          _M0L6_2atmpS2825 < 0
          || _M0L6_2atmpS2825 >= Moonbit_array_length(_M0L6resultS1057)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1057[_M0L6_2atmpS2825] = _M0L6_2atmpS2826;
        _M0L6_2atmpS2832 = _M0L1iS1066 + 1;
        _M0L1iS1066 = _M0L6_2atmpS2832;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2833 = _M0Lm5indexS1058;
    _M0L6_2atmpS2838 = _M0Lm6outputS1060;
    _M0L6_2atmpS2837 = (int32_t)_M0L6_2atmpS2838;
    _M0L6_2atmpS2836 = _M0L6_2atmpS2837 % 10;
    _M0L6_2atmpS2835 = 48 + _M0L6_2atmpS2836;
    _M0L6_2atmpS2834 = _M0L6_2atmpS2835 & 0xff;
    if (
      _M0L6_2atmpS2833 < 0
      || _M0L6_2atmpS2833 >= Moonbit_array_length(_M0L6resultS1057)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1057[_M0L6_2atmpS2833] = _M0L6_2atmpS2834;
    if (_M0L7olengthS1062 > 1) {
      int32_t _M0L6_2atmpS2840 = _M0Lm5indexS1058;
      int32_t _M0L6_2atmpS2839 = _M0L6_2atmpS2840 + 1;
      if (
        _M0L6_2atmpS2839 < 0
        || _M0L6_2atmpS2839 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2839] = 46;
    } else {
      int32_t _M0L6_2atmpS2841 = _M0Lm5indexS1058;
      _M0Lm5indexS1058 = _M0L6_2atmpS2841 - 1;
    }
    _M0L6_2atmpS2842 = _M0Lm5indexS1058;
    _M0L6_2atmpS2843 = _M0L7olengthS1062 + 1;
    _M0Lm5indexS1058 = _M0L6_2atmpS2842 + _M0L6_2atmpS2843;
    _M0L6_2atmpS2844 = _M0Lm5indexS1058;
    if (
      _M0L6_2atmpS2844 < 0
      || _M0L6_2atmpS2844 >= Moonbit_array_length(_M0L6resultS1057)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1057[_M0L6_2atmpS2844] = 101;
    _M0L6_2atmpS2845 = _M0Lm5indexS1058;
    _M0Lm5indexS1058 = _M0L6_2atmpS2845 + 1;
    _M0L6_2atmpS2846 = _M0Lm3expS1063;
    if (_M0L6_2atmpS2846 < 0) {
      int32_t _M0L6_2atmpS2847 = _M0Lm5indexS1058;
      int32_t _M0L6_2atmpS2848;
      int32_t _M0L6_2atmpS2849;
      if (
        _M0L6_2atmpS2847 < 0
        || _M0L6_2atmpS2847 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2847] = 45;
      _M0L6_2atmpS2848 = _M0Lm5indexS1058;
      _M0Lm5indexS1058 = _M0L6_2atmpS2848 + 1;
      _M0L6_2atmpS2849 = _M0Lm3expS1063;
      _M0Lm3expS1063 = -_M0L6_2atmpS2849;
    } else {
      int32_t _M0L6_2atmpS2850 = _M0Lm5indexS1058;
      int32_t _M0L6_2atmpS2851;
      if (
        _M0L6_2atmpS2850 < 0
        || _M0L6_2atmpS2850 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2850] = 43;
      _M0L6_2atmpS2851 = _M0Lm5indexS1058;
      _M0Lm5indexS1058 = _M0L6_2atmpS2851 + 1;
    }
    _M0L6_2atmpS2852 = _M0Lm3expS1063;
    if (_M0L6_2atmpS2852 >= 100) {
      int32_t _M0L6_2atmpS2868 = _M0Lm3expS1063;
      int32_t _M0L1aS1069 = _M0L6_2atmpS2868 / 100;
      int32_t _M0L6_2atmpS2867 = _M0Lm3expS1063;
      int32_t _M0L6_2atmpS2866 = _M0L6_2atmpS2867 / 10;
      int32_t _M0L1bS1070 = _M0L6_2atmpS2866 % 10;
      int32_t _M0L6_2atmpS2865 = _M0Lm3expS1063;
      int32_t _M0L1cS1071 = _M0L6_2atmpS2865 % 10;
      int32_t _M0L6_2atmpS2853 = _M0Lm5indexS1058;
      int32_t _M0L6_2atmpS2855 = 48 + _M0L1aS1069;
      int32_t _M0L6_2atmpS2854 = _M0L6_2atmpS2855 & 0xff;
      int32_t _M0L6_2atmpS2859;
      int32_t _M0L6_2atmpS2856;
      int32_t _M0L6_2atmpS2858;
      int32_t _M0L6_2atmpS2857;
      int32_t _M0L6_2atmpS2863;
      int32_t _M0L6_2atmpS2860;
      int32_t _M0L6_2atmpS2862;
      int32_t _M0L6_2atmpS2861;
      int32_t _M0L6_2atmpS2864;
      if (
        _M0L6_2atmpS2853 < 0
        || _M0L6_2atmpS2853 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2853] = _M0L6_2atmpS2854;
      _M0L6_2atmpS2859 = _M0Lm5indexS1058;
      _M0L6_2atmpS2856 = _M0L6_2atmpS2859 + 1;
      _M0L6_2atmpS2858 = 48 + _M0L1bS1070;
      _M0L6_2atmpS2857 = _M0L6_2atmpS2858 & 0xff;
      if (
        _M0L6_2atmpS2856 < 0
        || _M0L6_2atmpS2856 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2856] = _M0L6_2atmpS2857;
      _M0L6_2atmpS2863 = _M0Lm5indexS1058;
      _M0L6_2atmpS2860 = _M0L6_2atmpS2863 + 2;
      _M0L6_2atmpS2862 = 48 + _M0L1cS1071;
      _M0L6_2atmpS2861 = _M0L6_2atmpS2862 & 0xff;
      if (
        _M0L6_2atmpS2860 < 0
        || _M0L6_2atmpS2860 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2860] = _M0L6_2atmpS2861;
      _M0L6_2atmpS2864 = _M0Lm5indexS1058;
      _M0Lm5indexS1058 = _M0L6_2atmpS2864 + 3;
    } else {
      int32_t _M0L6_2atmpS2869 = _M0Lm3expS1063;
      if (_M0L6_2atmpS2869 >= 10) {
        int32_t _M0L6_2atmpS2879 = _M0Lm3expS1063;
        int32_t _M0L1aS1072 = _M0L6_2atmpS2879 / 10;
        int32_t _M0L6_2atmpS2878 = _M0Lm3expS1063;
        int32_t _M0L1bS1073 = _M0L6_2atmpS2878 % 10;
        int32_t _M0L6_2atmpS2870 = _M0Lm5indexS1058;
        int32_t _M0L6_2atmpS2872 = 48 + _M0L1aS1072;
        int32_t _M0L6_2atmpS2871 = _M0L6_2atmpS2872 & 0xff;
        int32_t _M0L6_2atmpS2876;
        int32_t _M0L6_2atmpS2873;
        int32_t _M0L6_2atmpS2875;
        int32_t _M0L6_2atmpS2874;
        int32_t _M0L6_2atmpS2877;
        if (
          _M0L6_2atmpS2870 < 0
          || _M0L6_2atmpS2870 >= Moonbit_array_length(_M0L6resultS1057)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1057[_M0L6_2atmpS2870] = _M0L6_2atmpS2871;
        _M0L6_2atmpS2876 = _M0Lm5indexS1058;
        _M0L6_2atmpS2873 = _M0L6_2atmpS2876 + 1;
        _M0L6_2atmpS2875 = 48 + _M0L1bS1073;
        _M0L6_2atmpS2874 = _M0L6_2atmpS2875 & 0xff;
        if (
          _M0L6_2atmpS2873 < 0
          || _M0L6_2atmpS2873 >= Moonbit_array_length(_M0L6resultS1057)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1057[_M0L6_2atmpS2873] = _M0L6_2atmpS2874;
        _M0L6_2atmpS2877 = _M0Lm5indexS1058;
        _M0Lm5indexS1058 = _M0L6_2atmpS2877 + 2;
      } else {
        int32_t _M0L6_2atmpS2880 = _M0Lm5indexS1058;
        int32_t _M0L6_2atmpS2883 = _M0Lm3expS1063;
        int32_t _M0L6_2atmpS2882 = 48 + _M0L6_2atmpS2883;
        int32_t _M0L6_2atmpS2881 = _M0L6_2atmpS2882 & 0xff;
        int32_t _M0L6_2atmpS2884;
        if (
          _M0L6_2atmpS2880 < 0
          || _M0L6_2atmpS2880 >= Moonbit_array_length(_M0L6resultS1057)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1057[_M0L6_2atmpS2880] = _M0L6_2atmpS2881;
        _M0L6_2atmpS2884 = _M0Lm5indexS1058;
        _M0Lm5indexS1058 = _M0L6_2atmpS2884 + 1;
      }
    }
    _M0L6_2atmpS2885 = _M0Lm5indexS1058;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1057, 0, _M0L6_2atmpS2885);
  } else {
    int32_t _M0L6_2atmpS2886 = _M0Lm3expS1063;
    int32_t _M0L6_2atmpS2946;
    if (_M0L6_2atmpS2886 < 0) {
      int32_t _M0L6_2atmpS2887 = _M0Lm5indexS1058;
      int32_t _M0L6_2atmpS2888;
      int32_t _M0L6_2atmpS2889;
      int32_t _M0L6_2atmpS2890;
      int32_t _M0L1iS1074;
      int32_t _M0L7currentS1076;
      int32_t _M0L1iS1077;
      if (
        _M0L6_2atmpS2887 < 0
        || _M0L6_2atmpS2887 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2887] = 48;
      _M0L6_2atmpS2888 = _M0Lm5indexS1058;
      _M0Lm5indexS1058 = _M0L6_2atmpS2888 + 1;
      _M0L6_2atmpS2889 = _M0Lm5indexS1058;
      if (
        _M0L6_2atmpS2889 < 0
        || _M0L6_2atmpS2889 >= Moonbit_array_length(_M0L6resultS1057)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1057[_M0L6_2atmpS2889] = 46;
      _M0L6_2atmpS2890 = _M0Lm5indexS1058;
      _M0Lm5indexS1058 = _M0L6_2atmpS2890 + 1;
      _M0L1iS1074 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2891 = _M0Lm3expS1063;
        if (_M0L1iS1074 > _M0L6_2atmpS2891) {
          int32_t _M0L6_2atmpS2892 = _M0Lm5indexS1058;
          int32_t _M0L6_2atmpS2893;
          int32_t _M0L6_2atmpS2894;
          if (
            _M0L6_2atmpS2892 < 0
            || _M0L6_2atmpS2892 >= Moonbit_array_length(_M0L6resultS1057)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1057[_M0L6_2atmpS2892] = 48;
          _M0L6_2atmpS2893 = _M0Lm5indexS1058;
          _M0Lm5indexS1058 = _M0L6_2atmpS2893 + 1;
          _M0L6_2atmpS2894 = _M0L1iS1074 - 1;
          _M0L1iS1074 = _M0L6_2atmpS2894;
          continue;
        }
        break;
      }
      _M0L7currentS1076 = _M0Lm5indexS1058;
      _M0L1iS1077 = 0;
      while (1) {
        if (_M0L1iS1077 < _M0L7olengthS1062) {
          int32_t _M0L6_2atmpS2902 = _M0L7currentS1076 + _M0L7olengthS1062;
          int32_t _M0L6_2atmpS2901 = _M0L6_2atmpS2902 - _M0L1iS1077;
          int32_t _M0L6_2atmpS2895 = _M0L6_2atmpS2901 - 1;
          uint64_t _M0L6_2atmpS2900 = _M0Lm6outputS1060;
          uint64_t _M0L6_2atmpS2899 = _M0L6_2atmpS2900 % 10ull;
          int32_t _M0L6_2atmpS2898 = (int32_t)_M0L6_2atmpS2899;
          int32_t _M0L6_2atmpS2897 = 48 + _M0L6_2atmpS2898;
          int32_t _M0L6_2atmpS2896 = _M0L6_2atmpS2897 & 0xff;
          uint64_t _M0L6_2atmpS2903;
          int32_t _M0L6_2atmpS2904;
          int32_t _M0L6_2atmpS2905;
          if (
            _M0L6_2atmpS2895 < 0
            || _M0L6_2atmpS2895 >= Moonbit_array_length(_M0L6resultS1057)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1057[_M0L6_2atmpS2895] = _M0L6_2atmpS2896;
          _M0L6_2atmpS2903 = _M0Lm6outputS1060;
          _M0Lm6outputS1060 = _M0L6_2atmpS2903 / 10ull;
          _M0L6_2atmpS2904 = _M0Lm5indexS1058;
          _M0Lm5indexS1058 = _M0L6_2atmpS2904 + 1;
          _M0L6_2atmpS2905 = _M0L1iS1077 + 1;
          _M0L1iS1077 = _M0L6_2atmpS2905;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2907 = _M0Lm3expS1063;
      int32_t _M0L6_2atmpS2906 = _M0L6_2atmpS2907 + 1;
      if (_M0L6_2atmpS2906 >= _M0L7olengthS1062) {
        int32_t _M0L1iS1079 = 0;
        int32_t _M0L6_2atmpS2919;
        int32_t _M0L6_2atmpS2923;
        int32_t _M0L7_2abindS1081;
        int32_t _M0L2__S1082;
        while (1) {
          if (_M0L1iS1079 < _M0L7olengthS1062) {
            int32_t _M0L6_2atmpS2916 = _M0Lm5indexS1058;
            int32_t _M0L6_2atmpS2915 = _M0L6_2atmpS2916 + _M0L7olengthS1062;
            int32_t _M0L6_2atmpS2914 = _M0L6_2atmpS2915 - _M0L1iS1079;
            int32_t _M0L6_2atmpS2908 = _M0L6_2atmpS2914 - 1;
            uint64_t _M0L6_2atmpS2913 = _M0Lm6outputS1060;
            uint64_t _M0L6_2atmpS2912 = _M0L6_2atmpS2913 % 10ull;
            int32_t _M0L6_2atmpS2911 = (int32_t)_M0L6_2atmpS2912;
            int32_t _M0L6_2atmpS2910 = 48 + _M0L6_2atmpS2911;
            int32_t _M0L6_2atmpS2909 = _M0L6_2atmpS2910 & 0xff;
            uint64_t _M0L6_2atmpS2917;
            int32_t _M0L6_2atmpS2918;
            if (
              _M0L6_2atmpS2908 < 0
              || _M0L6_2atmpS2908 >= Moonbit_array_length(_M0L6resultS1057)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1057[_M0L6_2atmpS2908] = _M0L6_2atmpS2909;
            _M0L6_2atmpS2917 = _M0Lm6outputS1060;
            _M0Lm6outputS1060 = _M0L6_2atmpS2917 / 10ull;
            _M0L6_2atmpS2918 = _M0L1iS1079 + 1;
            _M0L1iS1079 = _M0L6_2atmpS2918;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2919 = _M0Lm5indexS1058;
        _M0Lm5indexS1058 = _M0L6_2atmpS2919 + _M0L7olengthS1062;
        _M0L6_2atmpS2923 = _M0Lm3expS1063;
        _M0L7_2abindS1081 = _M0L6_2atmpS2923 + 1;
        _M0L2__S1082 = _M0L7olengthS1062;
        while (1) {
          if (_M0L2__S1082 < _M0L7_2abindS1081) {
            int32_t _M0L6_2atmpS2920 = _M0Lm5indexS1058;
            int32_t _M0L6_2atmpS2921;
            int32_t _M0L6_2atmpS2922;
            if (
              _M0L6_2atmpS2920 < 0
              || _M0L6_2atmpS2920 >= Moonbit_array_length(_M0L6resultS1057)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1057[_M0L6_2atmpS2920] = 48;
            _M0L6_2atmpS2921 = _M0Lm5indexS1058;
            _M0Lm5indexS1058 = _M0L6_2atmpS2921 + 1;
            _M0L6_2atmpS2922 = _M0L2__S1082 + 1;
            _M0L2__S1082 = _M0L6_2atmpS2922;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2945 = _M0Lm5indexS1058;
        int32_t _M0Lm7currentS1084 = _M0L6_2atmpS2945 + 1;
        int32_t _M0L1iS1085 = 0;
        int32_t _M0L6_2atmpS2943;
        int32_t _M0L6_2atmpS2944;
        while (1) {
          if (_M0L1iS1085 < _M0L7olengthS1062) {
            int32_t _M0L6_2atmpS2926 = _M0L7olengthS1062 - _M0L1iS1085;
            int32_t _M0L6_2atmpS2924 = _M0L6_2atmpS2926 - 1;
            int32_t _M0L6_2atmpS2925 = _M0Lm3expS1063;
            int32_t _M0L6_2atmpS2940;
            int32_t _M0L6_2atmpS2939;
            int32_t _M0L6_2atmpS2938;
            int32_t _M0L6_2atmpS2932;
            uint64_t _M0L6_2atmpS2937;
            uint64_t _M0L6_2atmpS2936;
            int32_t _M0L6_2atmpS2935;
            int32_t _M0L6_2atmpS2934;
            int32_t _M0L6_2atmpS2933;
            uint64_t _M0L6_2atmpS2941;
            int32_t _M0L6_2atmpS2942;
            if (_M0L6_2atmpS2924 == _M0L6_2atmpS2925) {
              int32_t _M0L6_2atmpS2930 = _M0Lm7currentS1084;
              int32_t _M0L6_2atmpS2929 = _M0L6_2atmpS2930 + _M0L7olengthS1062;
              int32_t _M0L6_2atmpS2928 = _M0L6_2atmpS2929 - _M0L1iS1085;
              int32_t _M0L6_2atmpS2927 = _M0L6_2atmpS2928 - 1;
              int32_t _M0L6_2atmpS2931;
              if (
                _M0L6_2atmpS2927 < 0
                || _M0L6_2atmpS2927 >= Moonbit_array_length(_M0L6resultS1057)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1057[_M0L6_2atmpS2927] = 46;
              _M0L6_2atmpS2931 = _M0Lm7currentS1084;
              _M0Lm7currentS1084 = _M0L6_2atmpS2931 - 1;
            }
            _M0L6_2atmpS2940 = _M0Lm7currentS1084;
            _M0L6_2atmpS2939 = _M0L6_2atmpS2940 + _M0L7olengthS1062;
            _M0L6_2atmpS2938 = _M0L6_2atmpS2939 - _M0L1iS1085;
            _M0L6_2atmpS2932 = _M0L6_2atmpS2938 - 1;
            _M0L6_2atmpS2937 = _M0Lm6outputS1060;
            _M0L6_2atmpS2936 = _M0L6_2atmpS2937 % 10ull;
            _M0L6_2atmpS2935 = (int32_t)_M0L6_2atmpS2936;
            _M0L6_2atmpS2934 = 48 + _M0L6_2atmpS2935;
            _M0L6_2atmpS2933 = _M0L6_2atmpS2934 & 0xff;
            if (
              _M0L6_2atmpS2932 < 0
              || _M0L6_2atmpS2932 >= Moonbit_array_length(_M0L6resultS1057)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1057[_M0L6_2atmpS2932] = _M0L6_2atmpS2933;
            _M0L6_2atmpS2941 = _M0Lm6outputS1060;
            _M0Lm6outputS1060 = _M0L6_2atmpS2941 / 10ull;
            _M0L6_2atmpS2942 = _M0L1iS1085 + 1;
            _M0L1iS1085 = _M0L6_2atmpS2942;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2943 = _M0Lm5indexS1058;
        _M0L6_2atmpS2944 = _M0L7olengthS1062 + 1;
        _M0Lm5indexS1058 = _M0L6_2atmpS2943 + _M0L6_2atmpS2944;
      }
    }
    _M0L6_2atmpS2946 = _M0Lm5indexS1058;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1057, 0, _M0L6_2atmpS2946);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1003,
  uint32_t _M0L12ieeeExponentS1002
) {
  int32_t _M0Lm2e2S1000;
  uint64_t _M0Lm2m2S1001;
  uint64_t _M0L6_2atmpS2821;
  uint64_t _M0L6_2atmpS2820;
  int32_t _M0L4evenS1004;
  uint64_t _M0L6_2atmpS2819;
  uint64_t _M0L2mvS1005;
  int32_t _M0L7mmShiftS1006;
  uint64_t _M0Lm2vrS1007;
  uint64_t _M0Lm2vpS1008;
  uint64_t _M0Lm2vmS1009;
  int32_t _M0Lm3e10S1010;
  int32_t _M0Lm17vmIsTrailingZerosS1011;
  int32_t _M0Lm17vrIsTrailingZerosS1012;
  int32_t _M0L6_2atmpS2721;
  int32_t _M0Lm7removedS1031;
  int32_t _M0Lm16lastRemovedDigitS1032;
  uint64_t _M0Lm6outputS1033;
  int32_t _M0L6_2atmpS2817;
  int32_t _M0L6_2atmpS2818;
  int32_t _M0L3expS1056;
  uint64_t _M0L6_2atmpS2816;
  struct _M0TPB17FloatingDecimal64* _block_4652;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1000 = 0;
  _M0Lm2m2S1001 = 0ull;
  if (_M0L12ieeeExponentS1002 == 0u) {
    _M0Lm2e2S1000 = -1076;
    _M0Lm2m2S1001 = _M0L12ieeeMantissaS1003;
  } else {
    int32_t _M0L6_2atmpS2720 = *(int32_t*)&_M0L12ieeeExponentS1002;
    int32_t _M0L6_2atmpS2719 = _M0L6_2atmpS2720 - 1023;
    int32_t _M0L6_2atmpS2718 = _M0L6_2atmpS2719 - 52;
    _M0Lm2e2S1000 = _M0L6_2atmpS2718 - 2;
    _M0Lm2m2S1001 = 4503599627370496ull | _M0L12ieeeMantissaS1003;
  }
  _M0L6_2atmpS2821 = _M0Lm2m2S1001;
  _M0L6_2atmpS2820 = _M0L6_2atmpS2821 & 1ull;
  _M0L4evenS1004 = _M0L6_2atmpS2820 == 0ull;
  _M0L6_2atmpS2819 = _M0Lm2m2S1001;
  _M0L2mvS1005 = 4ull * _M0L6_2atmpS2819;
  if (_M0L12ieeeMantissaS1003 != 0ull) {
    _M0L7mmShiftS1006 = 1;
  } else {
    _M0L7mmShiftS1006 = _M0L12ieeeExponentS1002 <= 1u;
  }
  _M0Lm2vrS1007 = 0ull;
  _M0Lm2vpS1008 = 0ull;
  _M0Lm2vmS1009 = 0ull;
  _M0Lm3e10S1010 = 0;
  _M0Lm17vmIsTrailingZerosS1011 = 0;
  _M0Lm17vrIsTrailingZerosS1012 = 0;
  _M0L6_2atmpS2721 = _M0Lm2e2S1000;
  if (_M0L6_2atmpS2721 >= 0) {
    int32_t _M0L6_2atmpS2743 = _M0Lm2e2S1000;
    int32_t _M0L6_2atmpS2739;
    int32_t _M0L6_2atmpS2742;
    int32_t _M0L6_2atmpS2741;
    int32_t _M0L6_2atmpS2740;
    int32_t _M0L1qS1013;
    int32_t _M0L6_2atmpS2738;
    int32_t _M0L6_2atmpS2737;
    int32_t _M0L1kS1014;
    int32_t _M0L6_2atmpS2736;
    int32_t _M0L6_2atmpS2735;
    int32_t _M0L6_2atmpS2734;
    int32_t _M0L1iS1015;
    struct _M0TPB8Pow5Pair _M0L4pow5S1016;
    uint64_t _M0L6_2atmpS2733;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1017;
    uint64_t _M0L8_2avrOutS1018;
    uint64_t _M0L8_2avpOutS1019;
    uint64_t _M0L8_2avmOutS1020;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2739 = _M0FPB9log10Pow2(_M0L6_2atmpS2743);
    _M0L6_2atmpS2742 = _M0Lm2e2S1000;
    _M0L6_2atmpS2741 = _M0L6_2atmpS2742 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2740 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2741);
    _M0L1qS1013 = _M0L6_2atmpS2739 - _M0L6_2atmpS2740;
    _M0Lm3e10S1010 = _M0L1qS1013;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2738 = _M0FPB8pow5bits(_M0L1qS1013);
    _M0L6_2atmpS2737 = 125 + _M0L6_2atmpS2738;
    _M0L1kS1014 = _M0L6_2atmpS2737 - 1;
    _M0L6_2atmpS2736 = _M0Lm2e2S1000;
    _M0L6_2atmpS2735 = -_M0L6_2atmpS2736;
    _M0L6_2atmpS2734 = _M0L6_2atmpS2735 + _M0L1qS1013;
    _M0L1iS1015 = _M0L6_2atmpS2734 + _M0L1kS1014;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1016 = _M0FPB22double__computeInvPow5(_M0L1qS1013);
    _M0L6_2atmpS2733 = _M0Lm2m2S1001;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1017
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2733, _M0L4pow5S1016, _M0L1iS1015, _M0L7mmShiftS1006);
    _M0L8_2avrOutS1018 = _M0L7_2abindS1017.$0;
    _M0L8_2avpOutS1019 = _M0L7_2abindS1017.$1;
    _M0L8_2avmOutS1020 = _M0L7_2abindS1017.$2;
    _M0Lm2vrS1007 = _M0L8_2avrOutS1018;
    _M0Lm2vpS1008 = _M0L8_2avpOutS1019;
    _M0Lm2vmS1009 = _M0L8_2avmOutS1020;
    if (_M0L1qS1013 <= 21) {
      int32_t _M0L6_2atmpS2729 = (int32_t)_M0L2mvS1005;
      uint64_t _M0L6_2atmpS2732 = _M0L2mvS1005 / 5ull;
      int32_t _M0L6_2atmpS2731 = (int32_t)_M0L6_2atmpS2732;
      int32_t _M0L6_2atmpS2730 = 5 * _M0L6_2atmpS2731;
      int32_t _M0L6mvMod5S1021 = _M0L6_2atmpS2729 - _M0L6_2atmpS2730;
      if (_M0L6mvMod5S1021 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1012
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1005, _M0L1qS1013);
      } else if (_M0L4evenS1004) {
        uint64_t _M0L6_2atmpS2723 = _M0L2mvS1005 - 1ull;
        uint64_t _M0L6_2atmpS2724;
        uint64_t _M0L6_2atmpS2722;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2724 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1006);
        _M0L6_2atmpS2722 = _M0L6_2atmpS2723 - _M0L6_2atmpS2724;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1011
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2722, _M0L1qS1013);
      } else {
        uint64_t _M0L6_2atmpS2725 = _M0Lm2vpS1008;
        uint64_t _M0L6_2atmpS2728 = _M0L2mvS1005 + 2ull;
        int32_t _M0L6_2atmpS2727;
        uint64_t _M0L6_2atmpS2726;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2727
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2728, _M0L1qS1013);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2726 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2727);
        _M0Lm2vpS1008 = _M0L6_2atmpS2725 - _M0L6_2atmpS2726;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2757 = _M0Lm2e2S1000;
    int32_t _M0L6_2atmpS2756 = -_M0L6_2atmpS2757;
    int32_t _M0L6_2atmpS2751;
    int32_t _M0L6_2atmpS2755;
    int32_t _M0L6_2atmpS2754;
    int32_t _M0L6_2atmpS2753;
    int32_t _M0L6_2atmpS2752;
    int32_t _M0L1qS1022;
    int32_t _M0L6_2atmpS2744;
    int32_t _M0L6_2atmpS2750;
    int32_t _M0L6_2atmpS2749;
    int32_t _M0L1iS1023;
    int32_t _M0L6_2atmpS2748;
    int32_t _M0L1kS1024;
    int32_t _M0L1jS1025;
    struct _M0TPB8Pow5Pair _M0L4pow5S1026;
    uint64_t _M0L6_2atmpS2747;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1027;
    uint64_t _M0L8_2avrOutS1028;
    uint64_t _M0L8_2avpOutS1029;
    uint64_t _M0L8_2avmOutS1030;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2751 = _M0FPB9log10Pow5(_M0L6_2atmpS2756);
    _M0L6_2atmpS2755 = _M0Lm2e2S1000;
    _M0L6_2atmpS2754 = -_M0L6_2atmpS2755;
    _M0L6_2atmpS2753 = _M0L6_2atmpS2754 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2752 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2753);
    _M0L1qS1022 = _M0L6_2atmpS2751 - _M0L6_2atmpS2752;
    _M0L6_2atmpS2744 = _M0Lm2e2S1000;
    _M0Lm3e10S1010 = _M0L1qS1022 + _M0L6_2atmpS2744;
    _M0L6_2atmpS2750 = _M0Lm2e2S1000;
    _M0L6_2atmpS2749 = -_M0L6_2atmpS2750;
    _M0L1iS1023 = _M0L6_2atmpS2749 - _M0L1qS1022;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2748 = _M0FPB8pow5bits(_M0L1iS1023);
    _M0L1kS1024 = _M0L6_2atmpS2748 - 125;
    _M0L1jS1025 = _M0L1qS1022 - _M0L1kS1024;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1026 = _M0FPB19double__computePow5(_M0L1iS1023);
    _M0L6_2atmpS2747 = _M0Lm2m2S1001;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1027
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2747, _M0L4pow5S1026, _M0L1jS1025, _M0L7mmShiftS1006);
    _M0L8_2avrOutS1028 = _M0L7_2abindS1027.$0;
    _M0L8_2avpOutS1029 = _M0L7_2abindS1027.$1;
    _M0L8_2avmOutS1030 = _M0L7_2abindS1027.$2;
    _M0Lm2vrS1007 = _M0L8_2avrOutS1028;
    _M0Lm2vpS1008 = _M0L8_2avpOutS1029;
    _M0Lm2vmS1009 = _M0L8_2avmOutS1030;
    if (_M0L1qS1022 <= 1) {
      _M0Lm17vrIsTrailingZerosS1012 = 1;
      if (_M0L4evenS1004) {
        int32_t _M0L6_2atmpS2745;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2745 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1006);
        _M0Lm17vmIsTrailingZerosS1011 = _M0L6_2atmpS2745 == 1;
      } else {
        uint64_t _M0L6_2atmpS2746 = _M0Lm2vpS1008;
        _M0Lm2vpS1008 = _M0L6_2atmpS2746 - 1ull;
      }
    } else if (_M0L1qS1022 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1012
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1005, _M0L1qS1022);
    }
  }
  _M0Lm7removedS1031 = 0;
  _M0Lm16lastRemovedDigitS1032 = 0;
  _M0Lm6outputS1033 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1011 || _M0Lm17vrIsTrailingZerosS1012) {
    int32_t _if__result_4649;
    uint64_t _M0L6_2atmpS2787;
    uint64_t _M0L6_2atmpS2793;
    uint64_t _M0L6_2atmpS2794;
    int32_t _if__result_4650;
    int32_t _M0L6_2atmpS2790;
    int64_t _M0L6_2atmpS2789;
    uint64_t _M0L6_2atmpS2788;
    while (1) {
      uint64_t _M0L6_2atmpS2770 = _M0Lm2vpS1008;
      uint64_t _M0L7vpDiv10S1034 = _M0L6_2atmpS2770 / 10ull;
      uint64_t _M0L6_2atmpS2769 = _M0Lm2vmS1009;
      uint64_t _M0L7vmDiv10S1035 = _M0L6_2atmpS2769 / 10ull;
      uint64_t _M0L6_2atmpS2768;
      int32_t _M0L6_2atmpS2765;
      int32_t _M0L6_2atmpS2767;
      int32_t _M0L6_2atmpS2766;
      int32_t _M0L7vmMod10S1037;
      uint64_t _M0L6_2atmpS2764;
      uint64_t _M0L7vrDiv10S1038;
      uint64_t _M0L6_2atmpS2763;
      int32_t _M0L6_2atmpS2760;
      int32_t _M0L6_2atmpS2762;
      int32_t _M0L6_2atmpS2761;
      int32_t _M0L7vrMod10S1039;
      int32_t _M0L6_2atmpS2759;
      if (_M0L7vpDiv10S1034 <= _M0L7vmDiv10S1035) {
        break;
      }
      _M0L6_2atmpS2768 = _M0Lm2vmS1009;
      _M0L6_2atmpS2765 = (int32_t)_M0L6_2atmpS2768;
      _M0L6_2atmpS2767 = (int32_t)_M0L7vmDiv10S1035;
      _M0L6_2atmpS2766 = 10 * _M0L6_2atmpS2767;
      _M0L7vmMod10S1037 = _M0L6_2atmpS2765 - _M0L6_2atmpS2766;
      _M0L6_2atmpS2764 = _M0Lm2vrS1007;
      _M0L7vrDiv10S1038 = _M0L6_2atmpS2764 / 10ull;
      _M0L6_2atmpS2763 = _M0Lm2vrS1007;
      _M0L6_2atmpS2760 = (int32_t)_M0L6_2atmpS2763;
      _M0L6_2atmpS2762 = (int32_t)_M0L7vrDiv10S1038;
      _M0L6_2atmpS2761 = 10 * _M0L6_2atmpS2762;
      _M0L7vrMod10S1039 = _M0L6_2atmpS2760 - _M0L6_2atmpS2761;
      if (_M0Lm17vmIsTrailingZerosS1011) {
        _M0Lm17vmIsTrailingZerosS1011 = _M0L7vmMod10S1037 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1011 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1012) {
        int32_t _M0L6_2atmpS2758 = _M0Lm16lastRemovedDigitS1032;
        _M0Lm17vrIsTrailingZerosS1012 = _M0L6_2atmpS2758 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1012 = 0;
      }
      _M0Lm16lastRemovedDigitS1032 = _M0L7vrMod10S1039;
      _M0Lm2vrS1007 = _M0L7vrDiv10S1038;
      _M0Lm2vpS1008 = _M0L7vpDiv10S1034;
      _M0Lm2vmS1009 = _M0L7vmDiv10S1035;
      _M0L6_2atmpS2759 = _M0Lm7removedS1031;
      _M0Lm7removedS1031 = _M0L6_2atmpS2759 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1011) {
      while (1) {
        uint64_t _M0L6_2atmpS2783 = _M0Lm2vmS1009;
        uint64_t _M0L7vmDiv10S1040 = _M0L6_2atmpS2783 / 10ull;
        uint64_t _M0L6_2atmpS2782 = _M0Lm2vmS1009;
        int32_t _M0L6_2atmpS2779 = (int32_t)_M0L6_2atmpS2782;
        int32_t _M0L6_2atmpS2781 = (int32_t)_M0L7vmDiv10S1040;
        int32_t _M0L6_2atmpS2780 = 10 * _M0L6_2atmpS2781;
        int32_t _M0L7vmMod10S1041 = _M0L6_2atmpS2779 - _M0L6_2atmpS2780;
        uint64_t _M0L6_2atmpS2778;
        uint64_t _M0L7vpDiv10S1043;
        uint64_t _M0L6_2atmpS2777;
        uint64_t _M0L7vrDiv10S1044;
        uint64_t _M0L6_2atmpS2776;
        int32_t _M0L6_2atmpS2773;
        int32_t _M0L6_2atmpS2775;
        int32_t _M0L6_2atmpS2774;
        int32_t _M0L7vrMod10S1045;
        int32_t _M0L6_2atmpS2772;
        if (_M0L7vmMod10S1041 != 0) {
          break;
        }
        _M0L6_2atmpS2778 = _M0Lm2vpS1008;
        _M0L7vpDiv10S1043 = _M0L6_2atmpS2778 / 10ull;
        _M0L6_2atmpS2777 = _M0Lm2vrS1007;
        _M0L7vrDiv10S1044 = _M0L6_2atmpS2777 / 10ull;
        _M0L6_2atmpS2776 = _M0Lm2vrS1007;
        _M0L6_2atmpS2773 = (int32_t)_M0L6_2atmpS2776;
        _M0L6_2atmpS2775 = (int32_t)_M0L7vrDiv10S1044;
        _M0L6_2atmpS2774 = 10 * _M0L6_2atmpS2775;
        _M0L7vrMod10S1045 = _M0L6_2atmpS2773 - _M0L6_2atmpS2774;
        if (_M0Lm17vrIsTrailingZerosS1012) {
          int32_t _M0L6_2atmpS2771 = _M0Lm16lastRemovedDigitS1032;
          _M0Lm17vrIsTrailingZerosS1012 = _M0L6_2atmpS2771 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1012 = 0;
        }
        _M0Lm16lastRemovedDigitS1032 = _M0L7vrMod10S1045;
        _M0Lm2vrS1007 = _M0L7vrDiv10S1044;
        _M0Lm2vpS1008 = _M0L7vpDiv10S1043;
        _M0Lm2vmS1009 = _M0L7vmDiv10S1040;
        _M0L6_2atmpS2772 = _M0Lm7removedS1031;
        _M0Lm7removedS1031 = _M0L6_2atmpS2772 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1012) {
      int32_t _M0L6_2atmpS2786 = _M0Lm16lastRemovedDigitS1032;
      if (_M0L6_2atmpS2786 == 5) {
        uint64_t _M0L6_2atmpS2785 = _M0Lm2vrS1007;
        uint64_t _M0L6_2atmpS2784 = _M0L6_2atmpS2785 % 2ull;
        _if__result_4649 = _M0L6_2atmpS2784 == 0ull;
      } else {
        _if__result_4649 = 0;
      }
    } else {
      _if__result_4649 = 0;
    }
    if (_if__result_4649) {
      _M0Lm16lastRemovedDigitS1032 = 4;
    }
    _M0L6_2atmpS2787 = _M0Lm2vrS1007;
    _M0L6_2atmpS2793 = _M0Lm2vrS1007;
    _M0L6_2atmpS2794 = _M0Lm2vmS1009;
    if (_M0L6_2atmpS2793 == _M0L6_2atmpS2794) {
      if (!_M0L4evenS1004) {
        _if__result_4650 = 1;
      } else {
        int32_t _M0L6_2atmpS2792 = _M0Lm17vmIsTrailingZerosS1011;
        _if__result_4650 = !_M0L6_2atmpS2792;
      }
    } else {
      _if__result_4650 = 0;
    }
    if (_if__result_4650) {
      _M0L6_2atmpS2790 = 1;
    } else {
      int32_t _M0L6_2atmpS2791 = _M0Lm16lastRemovedDigitS1032;
      _M0L6_2atmpS2790 = _M0L6_2atmpS2791 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2789 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2790);
    _M0L6_2atmpS2788 = *(uint64_t*)&_M0L6_2atmpS2789;
    _M0Lm6outputS1033 = _M0L6_2atmpS2787 + _M0L6_2atmpS2788;
  } else {
    int32_t _M0Lm7roundUpS1046 = 0;
    uint64_t _M0L6_2atmpS2815 = _M0Lm2vpS1008;
    uint64_t _M0L8vpDiv100S1047 = _M0L6_2atmpS2815 / 100ull;
    uint64_t _M0L6_2atmpS2814 = _M0Lm2vmS1009;
    uint64_t _M0L8vmDiv100S1048 = _M0L6_2atmpS2814 / 100ull;
    uint64_t _M0L6_2atmpS2809;
    uint64_t _M0L6_2atmpS2812;
    uint64_t _M0L6_2atmpS2813;
    int32_t _M0L6_2atmpS2811;
    uint64_t _M0L6_2atmpS2810;
    if (_M0L8vpDiv100S1047 > _M0L8vmDiv100S1048) {
      uint64_t _M0L6_2atmpS2800 = _M0Lm2vrS1007;
      uint64_t _M0L8vrDiv100S1049 = _M0L6_2atmpS2800 / 100ull;
      uint64_t _M0L6_2atmpS2799 = _M0Lm2vrS1007;
      int32_t _M0L6_2atmpS2796 = (int32_t)_M0L6_2atmpS2799;
      int32_t _M0L6_2atmpS2798 = (int32_t)_M0L8vrDiv100S1049;
      int32_t _M0L6_2atmpS2797 = 100 * _M0L6_2atmpS2798;
      int32_t _M0L8vrMod100S1050 = _M0L6_2atmpS2796 - _M0L6_2atmpS2797;
      int32_t _M0L6_2atmpS2795;
      _M0Lm7roundUpS1046 = _M0L8vrMod100S1050 >= 50;
      _M0Lm2vrS1007 = _M0L8vrDiv100S1049;
      _M0Lm2vpS1008 = _M0L8vpDiv100S1047;
      _M0Lm2vmS1009 = _M0L8vmDiv100S1048;
      _M0L6_2atmpS2795 = _M0Lm7removedS1031;
      _M0Lm7removedS1031 = _M0L6_2atmpS2795 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2808 = _M0Lm2vpS1008;
      uint64_t _M0L7vpDiv10S1051 = _M0L6_2atmpS2808 / 10ull;
      uint64_t _M0L6_2atmpS2807 = _M0Lm2vmS1009;
      uint64_t _M0L7vmDiv10S1052 = _M0L6_2atmpS2807 / 10ull;
      uint64_t _M0L6_2atmpS2806;
      uint64_t _M0L7vrDiv10S1054;
      uint64_t _M0L6_2atmpS2805;
      int32_t _M0L6_2atmpS2802;
      int32_t _M0L6_2atmpS2804;
      int32_t _M0L6_2atmpS2803;
      int32_t _M0L7vrMod10S1055;
      int32_t _M0L6_2atmpS2801;
      if (_M0L7vpDiv10S1051 <= _M0L7vmDiv10S1052) {
        break;
      }
      _M0L6_2atmpS2806 = _M0Lm2vrS1007;
      _M0L7vrDiv10S1054 = _M0L6_2atmpS2806 / 10ull;
      _M0L6_2atmpS2805 = _M0Lm2vrS1007;
      _M0L6_2atmpS2802 = (int32_t)_M0L6_2atmpS2805;
      _M0L6_2atmpS2804 = (int32_t)_M0L7vrDiv10S1054;
      _M0L6_2atmpS2803 = 10 * _M0L6_2atmpS2804;
      _M0L7vrMod10S1055 = _M0L6_2atmpS2802 - _M0L6_2atmpS2803;
      _M0Lm7roundUpS1046 = _M0L7vrMod10S1055 >= 5;
      _M0Lm2vrS1007 = _M0L7vrDiv10S1054;
      _M0Lm2vpS1008 = _M0L7vpDiv10S1051;
      _M0Lm2vmS1009 = _M0L7vmDiv10S1052;
      _M0L6_2atmpS2801 = _M0Lm7removedS1031;
      _M0Lm7removedS1031 = _M0L6_2atmpS2801 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2809 = _M0Lm2vrS1007;
    _M0L6_2atmpS2812 = _M0Lm2vrS1007;
    _M0L6_2atmpS2813 = _M0Lm2vmS1009;
    _M0L6_2atmpS2811
    = _M0L6_2atmpS2812 == _M0L6_2atmpS2813 || _M0Lm7roundUpS1046;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2810 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2811);
    _M0Lm6outputS1033 = _M0L6_2atmpS2809 + _M0L6_2atmpS2810;
  }
  _M0L6_2atmpS2817 = _M0Lm3e10S1010;
  _M0L6_2atmpS2818 = _M0Lm7removedS1031;
  _M0L3expS1056 = _M0L6_2atmpS2817 + _M0L6_2atmpS2818;
  _M0L6_2atmpS2816 = _M0Lm6outputS1033;
  _block_4652
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4652)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4652->$0 = _M0L6_2atmpS2816;
  _block_4652->$1 = _M0L3expS1056;
  return _block_4652;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS999) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS999) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS998) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS998) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS997) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS997) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS996) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS996 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS996 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS996 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS996 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS996 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS996 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS996 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS996 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS996 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS996 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS996 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS996 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS996 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS996 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS996 >= 100ull) {
    return 3;
  }
  if (_M0L1vS996 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS979) {
  int32_t _M0L6_2atmpS2717;
  int32_t _M0L6_2atmpS2716;
  int32_t _M0L4baseS978;
  int32_t _M0L5base2S980;
  int32_t _M0L6offsetS981;
  int32_t _M0L6_2atmpS2715;
  uint64_t _M0L4mul0S982;
  int32_t _M0L6_2atmpS2714;
  int32_t _M0L6_2atmpS2713;
  uint64_t _M0L4mul1S983;
  uint64_t _M0L1mS984;
  struct _M0TPB7Umul128 _M0L7_2abindS985;
  uint64_t _M0L7_2alow1S986;
  uint64_t _M0L8_2ahigh1S987;
  struct _M0TPB7Umul128 _M0L7_2abindS988;
  uint64_t _M0L7_2alow0S989;
  uint64_t _M0L8_2ahigh0S990;
  uint64_t _M0L3sumS991;
  uint64_t _M0Lm5high1S992;
  int32_t _M0L6_2atmpS2711;
  int32_t _M0L6_2atmpS2712;
  int32_t _M0L5deltaS993;
  uint64_t _M0L6_2atmpS2710;
  uint64_t _M0L6_2atmpS2702;
  int32_t _M0L6_2atmpS2709;
  uint32_t _M0L6_2atmpS2706;
  int32_t _M0L6_2atmpS2708;
  int32_t _M0L6_2atmpS2707;
  uint32_t _M0L6_2atmpS2705;
  uint32_t _M0L6_2atmpS2704;
  uint64_t _M0L6_2atmpS2703;
  uint64_t _M0L1aS994;
  uint64_t _M0L6_2atmpS2701;
  uint64_t _M0L1bS995;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2717 = _M0L1iS979 + 26;
  _M0L6_2atmpS2716 = _M0L6_2atmpS2717 - 1;
  _M0L4baseS978 = _M0L6_2atmpS2716 / 26;
  _M0L5base2S980 = _M0L4baseS978 * 26;
  _M0L6offsetS981 = _M0L5base2S980 - _M0L1iS979;
  _M0L6_2atmpS2715 = _M0L4baseS978 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S982
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2715);
  _M0L6_2atmpS2714 = _M0L4baseS978 * 2;
  _M0L6_2atmpS2713 = _M0L6_2atmpS2714 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S983
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2713);
  if (_M0L6offsetS981 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S982, _M0L4mul1S983};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS984
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS981);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS985 = _M0FPB7umul128(_M0L1mS984, _M0L4mul1S983);
  _M0L7_2alow1S986 = _M0L7_2abindS985.$0;
  _M0L8_2ahigh1S987 = _M0L7_2abindS985.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS988 = _M0FPB7umul128(_M0L1mS984, _M0L4mul0S982);
  _M0L7_2alow0S989 = _M0L7_2abindS988.$0;
  _M0L8_2ahigh0S990 = _M0L7_2abindS988.$1;
  _M0L3sumS991 = _M0L8_2ahigh0S990 + _M0L7_2alow1S986;
  _M0Lm5high1S992 = _M0L8_2ahigh1S987;
  if (_M0L3sumS991 < _M0L8_2ahigh0S990) {
    uint64_t _M0L6_2atmpS2700 = _M0Lm5high1S992;
    _M0Lm5high1S992 = _M0L6_2atmpS2700 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2711 = _M0FPB8pow5bits(_M0L5base2S980);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2712 = _M0FPB8pow5bits(_M0L1iS979);
  _M0L5deltaS993 = _M0L6_2atmpS2711 - _M0L6_2atmpS2712;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2710
  = _M0FPB13shiftright128(_M0L7_2alow0S989, _M0L3sumS991, _M0L5deltaS993);
  _M0L6_2atmpS2702 = _M0L6_2atmpS2710 + 1ull;
  _M0L6_2atmpS2709 = _M0L1iS979 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2706
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2709);
  _M0L6_2atmpS2708 = _M0L1iS979 % 16;
  _M0L6_2atmpS2707 = _M0L6_2atmpS2708 << 1;
  _M0L6_2atmpS2705 = _M0L6_2atmpS2706 >> (_M0L6_2atmpS2707 & 31);
  _M0L6_2atmpS2704 = _M0L6_2atmpS2705 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2703 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2704);
  _M0L1aS994 = _M0L6_2atmpS2702 + _M0L6_2atmpS2703;
  _M0L6_2atmpS2701 = _M0Lm5high1S992;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS995
  = _M0FPB13shiftright128(_M0L3sumS991, _M0L6_2atmpS2701, _M0L5deltaS993);
  return (struct _M0TPB8Pow5Pair){_M0L1aS994, _M0L1bS995};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS961) {
  int32_t _M0L4baseS960;
  int32_t _M0L5base2S962;
  int32_t _M0L6offsetS963;
  int32_t _M0L6_2atmpS2699;
  uint64_t _M0L4mul0S964;
  int32_t _M0L6_2atmpS2698;
  int32_t _M0L6_2atmpS2697;
  uint64_t _M0L4mul1S965;
  uint64_t _M0L1mS966;
  struct _M0TPB7Umul128 _M0L7_2abindS967;
  uint64_t _M0L7_2alow1S968;
  uint64_t _M0L8_2ahigh1S969;
  struct _M0TPB7Umul128 _M0L7_2abindS970;
  uint64_t _M0L7_2alow0S971;
  uint64_t _M0L8_2ahigh0S972;
  uint64_t _M0L3sumS973;
  uint64_t _M0Lm5high1S974;
  int32_t _M0L6_2atmpS2695;
  int32_t _M0L6_2atmpS2696;
  int32_t _M0L5deltaS975;
  uint64_t _M0L6_2atmpS2687;
  int32_t _M0L6_2atmpS2694;
  uint32_t _M0L6_2atmpS2691;
  int32_t _M0L6_2atmpS2693;
  int32_t _M0L6_2atmpS2692;
  uint32_t _M0L6_2atmpS2690;
  uint32_t _M0L6_2atmpS2689;
  uint64_t _M0L6_2atmpS2688;
  uint64_t _M0L1aS976;
  uint64_t _M0L6_2atmpS2686;
  uint64_t _M0L1bS977;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS960 = _M0L1iS961 / 26;
  _M0L5base2S962 = _M0L4baseS960 * 26;
  _M0L6offsetS963 = _M0L1iS961 - _M0L5base2S962;
  _M0L6_2atmpS2699 = _M0L4baseS960 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S964
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2699);
  _M0L6_2atmpS2698 = _M0L4baseS960 * 2;
  _M0L6_2atmpS2697 = _M0L6_2atmpS2698 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S965
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2697);
  if (_M0L6offsetS963 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S964, _M0L4mul1S965};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS966
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS963);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS967 = _M0FPB7umul128(_M0L1mS966, _M0L4mul1S965);
  _M0L7_2alow1S968 = _M0L7_2abindS967.$0;
  _M0L8_2ahigh1S969 = _M0L7_2abindS967.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS970 = _M0FPB7umul128(_M0L1mS966, _M0L4mul0S964);
  _M0L7_2alow0S971 = _M0L7_2abindS970.$0;
  _M0L8_2ahigh0S972 = _M0L7_2abindS970.$1;
  _M0L3sumS973 = _M0L8_2ahigh0S972 + _M0L7_2alow1S968;
  _M0Lm5high1S974 = _M0L8_2ahigh1S969;
  if (_M0L3sumS973 < _M0L8_2ahigh0S972) {
    uint64_t _M0L6_2atmpS2685 = _M0Lm5high1S974;
    _M0Lm5high1S974 = _M0L6_2atmpS2685 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2695 = _M0FPB8pow5bits(_M0L1iS961);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2696 = _M0FPB8pow5bits(_M0L5base2S962);
  _M0L5deltaS975 = _M0L6_2atmpS2695 - _M0L6_2atmpS2696;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2687
  = _M0FPB13shiftright128(_M0L7_2alow0S971, _M0L3sumS973, _M0L5deltaS975);
  _M0L6_2atmpS2694 = _M0L1iS961 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2691
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2694);
  _M0L6_2atmpS2693 = _M0L1iS961 % 16;
  _M0L6_2atmpS2692 = _M0L6_2atmpS2693 << 1;
  _M0L6_2atmpS2690 = _M0L6_2atmpS2691 >> (_M0L6_2atmpS2692 & 31);
  _M0L6_2atmpS2689 = _M0L6_2atmpS2690 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2688 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2689);
  _M0L1aS976 = _M0L6_2atmpS2687 + _M0L6_2atmpS2688;
  _M0L6_2atmpS2686 = _M0Lm5high1S974;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS977
  = _M0FPB13shiftright128(_M0L3sumS973, _M0L6_2atmpS2686, _M0L5deltaS975);
  return (struct _M0TPB8Pow5Pair){_M0L1aS976, _M0L1bS977};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS934,
  struct _M0TPB8Pow5Pair _M0L3mulS931,
  int32_t _M0L1jS947,
  int32_t _M0L7mmShiftS949
) {
  uint64_t _M0L7_2amul0S930;
  uint64_t _M0L7_2amul1S932;
  uint64_t _M0L1mS933;
  struct _M0TPB7Umul128 _M0L7_2abindS935;
  uint64_t _M0L5_2aloS936;
  uint64_t _M0L6_2atmpS937;
  struct _M0TPB7Umul128 _M0L7_2abindS938;
  uint64_t _M0L6_2alo2S939;
  uint64_t _M0L6_2ahi2S940;
  uint64_t _M0L3midS941;
  uint64_t _M0L6_2atmpS2684;
  uint64_t _M0L2hiS942;
  uint64_t _M0L3lo2S943;
  uint64_t _M0L6_2atmpS2682;
  uint64_t _M0L6_2atmpS2683;
  uint64_t _M0L4mid2S944;
  uint64_t _M0L6_2atmpS2681;
  uint64_t _M0L3hi2S945;
  int32_t _M0L6_2atmpS2680;
  int32_t _M0L6_2atmpS2679;
  uint64_t _M0L2vpS946;
  uint64_t _M0Lm2vmS948;
  int32_t _M0L6_2atmpS2678;
  int32_t _M0L6_2atmpS2677;
  uint64_t _M0L2vrS959;
  uint64_t _M0L6_2atmpS2676;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S930 = _M0L3mulS931.$0;
  _M0L7_2amul1S932 = _M0L3mulS931.$1;
  _M0L1mS933 = _M0L1mS934 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS935 = _M0FPB7umul128(_M0L1mS933, _M0L7_2amul0S930);
  _M0L5_2aloS936 = _M0L7_2abindS935.$0;
  _M0L6_2atmpS937 = _M0L7_2abindS935.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS938 = _M0FPB7umul128(_M0L1mS933, _M0L7_2amul1S932);
  _M0L6_2alo2S939 = _M0L7_2abindS938.$0;
  _M0L6_2ahi2S940 = _M0L7_2abindS938.$1;
  _M0L3midS941 = _M0L6_2atmpS937 + _M0L6_2alo2S939;
  if (_M0L3midS941 < _M0L6_2atmpS937) {
    _M0L6_2atmpS2684 = 1ull;
  } else {
    _M0L6_2atmpS2684 = 0ull;
  }
  _M0L2hiS942 = _M0L6_2ahi2S940 + _M0L6_2atmpS2684;
  _M0L3lo2S943 = _M0L5_2aloS936 + _M0L7_2amul0S930;
  _M0L6_2atmpS2682 = _M0L3midS941 + _M0L7_2amul1S932;
  if (_M0L3lo2S943 < _M0L5_2aloS936) {
    _M0L6_2atmpS2683 = 1ull;
  } else {
    _M0L6_2atmpS2683 = 0ull;
  }
  _M0L4mid2S944 = _M0L6_2atmpS2682 + _M0L6_2atmpS2683;
  if (_M0L4mid2S944 < _M0L3midS941) {
    _M0L6_2atmpS2681 = 1ull;
  } else {
    _M0L6_2atmpS2681 = 0ull;
  }
  _M0L3hi2S945 = _M0L2hiS942 + _M0L6_2atmpS2681;
  _M0L6_2atmpS2680 = _M0L1jS947 - 64;
  _M0L6_2atmpS2679 = _M0L6_2atmpS2680 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS946
  = _M0FPB13shiftright128(_M0L4mid2S944, _M0L3hi2S945, _M0L6_2atmpS2679);
  _M0Lm2vmS948 = 0ull;
  if (_M0L7mmShiftS949) {
    uint64_t _M0L3lo3S950 = _M0L5_2aloS936 - _M0L7_2amul0S930;
    uint64_t _M0L6_2atmpS2666 = _M0L3midS941 - _M0L7_2amul1S932;
    uint64_t _M0L6_2atmpS2667;
    uint64_t _M0L4mid3S951;
    uint64_t _M0L6_2atmpS2665;
    uint64_t _M0L3hi3S952;
    int32_t _M0L6_2atmpS2664;
    int32_t _M0L6_2atmpS2663;
    if (_M0L5_2aloS936 < _M0L3lo3S950) {
      _M0L6_2atmpS2667 = 1ull;
    } else {
      _M0L6_2atmpS2667 = 0ull;
    }
    _M0L4mid3S951 = _M0L6_2atmpS2666 - _M0L6_2atmpS2667;
    if (_M0L3midS941 < _M0L4mid3S951) {
      _M0L6_2atmpS2665 = 1ull;
    } else {
      _M0L6_2atmpS2665 = 0ull;
    }
    _M0L3hi3S952 = _M0L2hiS942 - _M0L6_2atmpS2665;
    _M0L6_2atmpS2664 = _M0L1jS947 - 64;
    _M0L6_2atmpS2663 = _M0L6_2atmpS2664 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS948
    = _M0FPB13shiftright128(_M0L4mid3S951, _M0L3hi3S952, _M0L6_2atmpS2663);
  } else {
    uint64_t _M0L3lo3S953 = _M0L5_2aloS936 + _M0L5_2aloS936;
    uint64_t _M0L6_2atmpS2674 = _M0L3midS941 + _M0L3midS941;
    uint64_t _M0L6_2atmpS2675;
    uint64_t _M0L4mid3S954;
    uint64_t _M0L6_2atmpS2672;
    uint64_t _M0L6_2atmpS2673;
    uint64_t _M0L3hi3S955;
    uint64_t _M0L3lo4S956;
    uint64_t _M0L6_2atmpS2670;
    uint64_t _M0L6_2atmpS2671;
    uint64_t _M0L4mid4S957;
    uint64_t _M0L6_2atmpS2669;
    uint64_t _M0L3hi4S958;
    int32_t _M0L6_2atmpS2668;
    if (_M0L3lo3S953 < _M0L5_2aloS936) {
      _M0L6_2atmpS2675 = 1ull;
    } else {
      _M0L6_2atmpS2675 = 0ull;
    }
    _M0L4mid3S954 = _M0L6_2atmpS2674 + _M0L6_2atmpS2675;
    _M0L6_2atmpS2672 = _M0L2hiS942 + _M0L2hiS942;
    if (_M0L4mid3S954 < _M0L3midS941) {
      _M0L6_2atmpS2673 = 1ull;
    } else {
      _M0L6_2atmpS2673 = 0ull;
    }
    _M0L3hi3S955 = _M0L6_2atmpS2672 + _M0L6_2atmpS2673;
    _M0L3lo4S956 = _M0L3lo3S953 - _M0L7_2amul0S930;
    _M0L6_2atmpS2670 = _M0L4mid3S954 - _M0L7_2amul1S932;
    if (_M0L3lo3S953 < _M0L3lo4S956) {
      _M0L6_2atmpS2671 = 1ull;
    } else {
      _M0L6_2atmpS2671 = 0ull;
    }
    _M0L4mid4S957 = _M0L6_2atmpS2670 - _M0L6_2atmpS2671;
    if (_M0L4mid3S954 < _M0L4mid4S957) {
      _M0L6_2atmpS2669 = 1ull;
    } else {
      _M0L6_2atmpS2669 = 0ull;
    }
    _M0L3hi4S958 = _M0L3hi3S955 - _M0L6_2atmpS2669;
    _M0L6_2atmpS2668 = _M0L1jS947 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS948
    = _M0FPB13shiftright128(_M0L4mid4S957, _M0L3hi4S958, _M0L6_2atmpS2668);
  }
  _M0L6_2atmpS2678 = _M0L1jS947 - 64;
  _M0L6_2atmpS2677 = _M0L6_2atmpS2678 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS959
  = _M0FPB13shiftright128(_M0L3midS941, _M0L2hiS942, _M0L6_2atmpS2677);
  _M0L6_2atmpS2676 = _M0Lm2vmS948;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS959,
                                                _M0L2vpS946,
                                                _M0L6_2atmpS2676};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS928,
  int32_t _M0L1pS929
) {
  uint64_t _M0L6_2atmpS2662;
  uint64_t _M0L6_2atmpS2661;
  uint64_t _M0L6_2atmpS2660;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2662 = 1ull << (_M0L1pS929 & 63);
  _M0L6_2atmpS2661 = _M0L6_2atmpS2662 - 1ull;
  _M0L6_2atmpS2660 = _M0L5valueS928 & _M0L6_2atmpS2661;
  return _M0L6_2atmpS2660 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS926,
  int32_t _M0L1pS927
) {
  int32_t _M0L6_2atmpS2659;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2659 = _M0FPB10pow5Factor(_M0L5valueS926);
  return _M0L6_2atmpS2659 >= _M0L1pS927;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS922) {
  uint64_t _M0L6_2atmpS2647;
  uint64_t _M0L6_2atmpS2648;
  uint64_t _M0L6_2atmpS2649;
  uint64_t _M0L6_2atmpS2650;
  int32_t _M0Lm5countS923;
  uint64_t _M0Lm5valueS924;
  uint64_t _M0L6_2atmpS2658;
  moonbit_string_t _M0L6_2atmpS2657;
  moonbit_string_t _M0L6_2atmpS4113;
  moonbit_string_t _M0L6_2atmpS2656;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2647 = _M0L5valueS922 % 5ull;
  if (_M0L6_2atmpS2647 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2648 = _M0L5valueS922 % 25ull;
  if (_M0L6_2atmpS2648 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2649 = _M0L5valueS922 % 125ull;
  if (_M0L6_2atmpS2649 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2650 = _M0L5valueS922 % 625ull;
  if (_M0L6_2atmpS2650 != 0ull) {
    return 3;
  }
  _M0Lm5countS923 = 4;
  _M0Lm5valueS924 = _M0L5valueS922 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2651 = _M0Lm5valueS924;
    if (_M0L6_2atmpS2651 > 0ull) {
      uint64_t _M0L6_2atmpS2653 = _M0Lm5valueS924;
      uint64_t _M0L6_2atmpS2652 = _M0L6_2atmpS2653 % 5ull;
      uint64_t _M0L6_2atmpS2654;
      int32_t _M0L6_2atmpS2655;
      if (_M0L6_2atmpS2652 != 0ull) {
        return _M0Lm5countS923;
      }
      _M0L6_2atmpS2654 = _M0Lm5valueS924;
      _M0Lm5valueS924 = _M0L6_2atmpS2654 / 5ull;
      _M0L6_2atmpS2655 = _M0Lm5countS923;
      _M0Lm5countS923 = _M0L6_2atmpS2655 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2658 = _M0Lm5valueS924;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2657
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2658);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS4113
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_90.data, _M0L6_2atmpS2657);
  moonbit_decref(_M0L6_2atmpS2657);
  _M0L6_2atmpS2656 = _M0L6_2atmpS4113;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2656, (moonbit_string_t)moonbit_string_literal_91.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS921,
  uint64_t _M0L2hiS919,
  int32_t _M0L4distS920
) {
  int32_t _M0L6_2atmpS2646;
  uint64_t _M0L6_2atmpS2644;
  uint64_t _M0L6_2atmpS2645;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2646 = 64 - _M0L4distS920;
  _M0L6_2atmpS2644 = _M0L2hiS919 << (_M0L6_2atmpS2646 & 63);
  _M0L6_2atmpS2645 = _M0L2loS921 >> (_M0L4distS920 & 63);
  return _M0L6_2atmpS2644 | _M0L6_2atmpS2645;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS909,
  uint64_t _M0L1bS912
) {
  uint64_t _M0L3aLoS908;
  uint64_t _M0L3aHiS910;
  uint64_t _M0L3bLoS911;
  uint64_t _M0L3bHiS913;
  uint64_t _M0L1xS914;
  uint64_t _M0L6_2atmpS2642;
  uint64_t _M0L6_2atmpS2643;
  uint64_t _M0L1yS915;
  uint64_t _M0L6_2atmpS2640;
  uint64_t _M0L6_2atmpS2641;
  uint64_t _M0L1zS916;
  uint64_t _M0L6_2atmpS2638;
  uint64_t _M0L6_2atmpS2639;
  uint64_t _M0L6_2atmpS2636;
  uint64_t _M0L6_2atmpS2637;
  uint64_t _M0L1wS917;
  uint64_t _M0L2loS918;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS908 = _M0L1aS909 & 4294967295ull;
  _M0L3aHiS910 = _M0L1aS909 >> 32;
  _M0L3bLoS911 = _M0L1bS912 & 4294967295ull;
  _M0L3bHiS913 = _M0L1bS912 >> 32;
  _M0L1xS914 = _M0L3aLoS908 * _M0L3bLoS911;
  _M0L6_2atmpS2642 = _M0L3aHiS910 * _M0L3bLoS911;
  _M0L6_2atmpS2643 = _M0L1xS914 >> 32;
  _M0L1yS915 = _M0L6_2atmpS2642 + _M0L6_2atmpS2643;
  _M0L6_2atmpS2640 = _M0L3aLoS908 * _M0L3bHiS913;
  _M0L6_2atmpS2641 = _M0L1yS915 & 4294967295ull;
  _M0L1zS916 = _M0L6_2atmpS2640 + _M0L6_2atmpS2641;
  _M0L6_2atmpS2638 = _M0L3aHiS910 * _M0L3bHiS913;
  _M0L6_2atmpS2639 = _M0L1yS915 >> 32;
  _M0L6_2atmpS2636 = _M0L6_2atmpS2638 + _M0L6_2atmpS2639;
  _M0L6_2atmpS2637 = _M0L1zS916 >> 32;
  _M0L1wS917 = _M0L6_2atmpS2636 + _M0L6_2atmpS2637;
  _M0L2loS918 = _M0L1aS909 * _M0L1bS912;
  return (struct _M0TPB7Umul128){_M0L2loS918, _M0L1wS917};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS903,
  int32_t _M0L4fromS907,
  int32_t _M0L2toS905
) {
  int32_t _M0L6_2atmpS2635;
  struct _M0TPB13StringBuilder* _M0L3bufS902;
  int32_t _M0L1iS904;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2635 = Moonbit_array_length(_M0L5bytesS903);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS902 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2635);
  _M0L1iS904 = _M0L4fromS907;
  while (1) {
    if (_M0L1iS904 < _M0L2toS905) {
      int32_t _M0L6_2atmpS2633;
      int32_t _M0L6_2atmpS2632;
      int32_t _M0L6_2atmpS2634;
      if (
        _M0L1iS904 < 0 || _M0L1iS904 >= Moonbit_array_length(_M0L5bytesS903)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2633 = (int32_t)_M0L5bytesS903[_M0L1iS904];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2632 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2633);
      moonbit_incref(_M0L3bufS902);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS902, _M0L6_2atmpS2632);
      _M0L6_2atmpS2634 = _M0L1iS904 + 1;
      _M0L1iS904 = _M0L6_2atmpS2634;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS903);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS902);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS901) {
  int32_t _M0L6_2atmpS2631;
  uint32_t _M0L6_2atmpS2630;
  uint32_t _M0L6_2atmpS2629;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2631 = _M0L1eS901 * 78913;
  _M0L6_2atmpS2630 = *(uint32_t*)&_M0L6_2atmpS2631;
  _M0L6_2atmpS2629 = _M0L6_2atmpS2630 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2629;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS900) {
  int32_t _M0L6_2atmpS2628;
  uint32_t _M0L6_2atmpS2627;
  uint32_t _M0L6_2atmpS2626;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2628 = _M0L1eS900 * 732923;
  _M0L6_2atmpS2627 = *(uint32_t*)&_M0L6_2atmpS2628;
  _M0L6_2atmpS2626 = _M0L6_2atmpS2627 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2626;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS898,
  int32_t _M0L8exponentS899,
  int32_t _M0L8mantissaS896
) {
  moonbit_string_t _M0L1sS897;
  moonbit_string_t _M0L6_2atmpS4114;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS896) {
    return (moonbit_string_t)moonbit_string_literal_92.data;
  }
  if (_M0L4signS898) {
    _M0L1sS897 = (moonbit_string_t)moonbit_string_literal_93.data;
  } else {
    _M0L1sS897 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS899) {
    moonbit_string_t _M0L6_2atmpS4115;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS4115
    = moonbit_add_string(_M0L1sS897, (moonbit_string_t)moonbit_string_literal_94.data);
    moonbit_decref(_M0L1sS897);
    return _M0L6_2atmpS4115;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS4114
  = moonbit_add_string(_M0L1sS897, (moonbit_string_t)moonbit_string_literal_95.data);
  moonbit_decref(_M0L1sS897);
  return _M0L6_2atmpS4114;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS895) {
  int32_t _M0L6_2atmpS2625;
  uint32_t _M0L6_2atmpS2624;
  uint32_t _M0L6_2atmpS2623;
  int32_t _M0L6_2atmpS2622;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2625 = _M0L1eS895 * 1217359;
  _M0L6_2atmpS2624 = *(uint32_t*)&_M0L6_2atmpS2625;
  _M0L6_2atmpS2623 = _M0L6_2atmpS2624 >> 19;
  _M0L6_2atmpS2622 = *(int32_t*)&_M0L6_2atmpS2623;
  return _M0L6_2atmpS2622 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS894,
  struct _M0TPB6Hasher* _M0L6hasherS893
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS893, _M0L4selfS894);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS892,
  struct _M0TPB6Hasher* _M0L6hasherS891
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS891, _M0L4selfS892);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS889,
  moonbit_string_t _M0L5valueS887
) {
  int32_t _M0L7_2abindS886;
  int32_t _M0L1iS888;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS886 = Moonbit_array_length(_M0L5valueS887);
  _M0L1iS888 = 0;
  while (1) {
    if (_M0L1iS888 < _M0L7_2abindS886) {
      int32_t _M0L6_2atmpS2620 = _M0L5valueS887[_M0L1iS888];
      int32_t _M0L6_2atmpS2619 = (int32_t)_M0L6_2atmpS2620;
      uint32_t _M0L6_2atmpS2618 = *(uint32_t*)&_M0L6_2atmpS2619;
      int32_t _M0L6_2atmpS2621;
      moonbit_incref(_M0L4selfS889);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS889, _M0L6_2atmpS2618);
      _M0L6_2atmpS2621 = _M0L1iS888 + 1;
      _M0L1iS888 = _M0L6_2atmpS2621;
      continue;
    } else {
      moonbit_decref(_M0L4selfS889);
      moonbit_decref(_M0L5valueS887);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS884,
  int32_t _M0L3idxS885
) {
  int32_t _M0L6_2atmpS4116;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4116 = _M0L4selfS884[_M0L3idxS885];
  moonbit_decref(_M0L4selfS884);
  return _M0L6_2atmpS4116;
}

void* _M0IPC15array9ArrayViewPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4selfS878
) {
  int32_t _M0L3lenS877;
  int32_t _M0L6_2atmpS2617;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3resS879;
  int32_t _M0L3endS2615;
  int32_t _M0L5startS2616;
  int32_t _M0L7_2abindS880;
  int32_t _M0L1iS881;
  void* _block_4657;
  #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0L4selfS878.$0);
  #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L3lenS877
  = _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS878);
  if (_M0L3lenS877 == 0) {
    void** _M0L6_2atmpS2608;
    struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2607;
    moonbit_decref(_M0L4selfS878.$0);
    _M0L6_2atmpS2608 = (void**)moonbit_empty_ref_array;
    _M0L6_2atmpS2607
    = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
    Moonbit_object_header(_M0L6_2atmpS2607)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
    _M0L6_2atmpS2607->$0 = _M0L6_2atmpS2608;
    _M0L6_2atmpS2607->$1 = 0;
    #line 268 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json5array(_M0L6_2atmpS2607);
  }
  moonbit_incref(_M0L4selfS878.$0);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2617
  = _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS878);
  #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L3resS879
  = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L6_2atmpS2617);
  _M0L3endS2615 = _M0L4selfS878.$2;
  _M0L5startS2616 = _M0L4selfS878.$1;
  _M0L7_2abindS880 = _M0L3endS2615 - _M0L5startS2616;
  _M0L1iS881 = 0;
  while (1) {
    if (_M0L1iS881 < _M0L7_2abindS880) {
      void** _M0L8_2afieldS4120 = _M0L4selfS878.$0;
      void** _M0L3bufS2612 = _M0L8_2afieldS4120;
      int32_t _M0L5startS2614 = _M0L4selfS878.$1;
      int32_t _M0L6_2atmpS2613 = _M0L5startS2614 + _M0L1iS881;
      void* _M0L6_2atmpS4119 = (void*)_M0L3bufS2612[_M0L6_2atmpS2613];
      void* _M0L1xS882 = _M0L6_2atmpS4119;
      void** _M0L8_2afieldS4118 = _M0L3resS879->$0;
      void** _M0L3bufS2609 = _M0L8_2afieldS4118;
      void* _M0L6_2atmpS2610;
      void* _M0L6_2aoldS4117;
      int32_t _M0L6_2atmpS2611;
      moonbit_incref(_M0L3bufS2609);
      moonbit_incref(_M0L1xS882);
      #line 272 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      _M0L6_2atmpS2610
      = _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson8to__json(_M0L1xS882);
      _M0L6_2aoldS4117 = (void*)_M0L3bufS2609[_M0L1iS881];
      moonbit_decref(_M0L6_2aoldS4117);
      _M0L3bufS2609[_M0L1iS881] = _M0L6_2atmpS2610;
      moonbit_decref(_M0L3bufS2609);
      _M0L6_2atmpS2611 = _M0L1iS881 + 1;
      _M0L1iS881 = _M0L6_2atmpS2611;
      continue;
    } else {
      moonbit_decref(_M0L4selfS878.$0);
    }
    break;
  }
  _block_4657 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4657)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4657)->$0 = _M0L3resS879;
  return _block_4657;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L4selfS874
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2604;
  void* _block_4658;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2604
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamRPB4JsonE(_M0L4selfS874, _M0IP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamPB6ToJson14to__json_2eclo);
  _block_4658 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4658)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4658)->$0 = _M0L6_2atmpS2604;
  return _block_4658;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS875
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2605;
  void* _block_4659;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2605
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamRPB4JsonE(_M0L4selfS875, _M0IP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamPB6ToJson14to__json_2eclo);
  _block_4659 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4659)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4659)->$0 = _M0L6_2atmpS2605;
  return _block_4659;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS876
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2606;
  void* _block_4660;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2606
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(_M0L4selfS876, _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo);
  _block_4660 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4660)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4660)->$0 = _M0L6_2atmpS2606;
  return _block_4660;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L4selfS854,
  struct _M0TWRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamERPB4Json* _M0L1fS858
) {
  int32_t _M0L3lenS2593;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS853;
  int32_t _M0L7_2abindS855;
  int32_t _M0L1iS856;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2593 = _M0L4selfS854->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS853 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2593);
  _M0L7_2abindS855 = _M0L4selfS854->$1;
  _M0L1iS856 = 0;
  while (1) {
    if (_M0L1iS856 < _M0L7_2abindS855) {
      void** _M0L8_2afieldS4124 = _M0L4selfS854->$0;
      void** _M0L3bufS2592 = _M0L8_2afieldS4124;
      void* _M0L6_2atmpS4123 = (void*)_M0L3bufS2592[_M0L1iS856];
      void* _M0L1vS857 = _M0L6_2atmpS4123;
      void** _M0L8_2afieldS4122 = _M0L3arrS853->$0;
      void** _M0L3bufS2589 = _M0L8_2afieldS4122;
      void* _M0L6_2atmpS2590;
      void* _M0L6_2aoldS4121;
      int32_t _M0L6_2atmpS2591;
      moonbit_incref(_M0L3bufS2589);
      moonbit_incref(_M0L1fS858);
      moonbit_incref(_M0L1vS857);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2590 = _M0L1fS858->code(_M0L1fS858, _M0L1vS857);
      _M0L6_2aoldS4121 = (void*)_M0L3bufS2589[_M0L1iS856];
      moonbit_decref(_M0L6_2aoldS4121);
      _M0L3bufS2589[_M0L1iS856] = _M0L6_2atmpS2590;
      moonbit_decref(_M0L3bufS2589);
      _M0L6_2atmpS2591 = _M0L1iS856 + 1;
      _M0L1iS856 = _M0L6_2atmpS2591;
      continue;
    } else {
      moonbit_decref(_M0L1fS858);
      moonbit_decref(_M0L4selfS854);
    }
    break;
  }
  return _M0L3arrS853;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS861,
  struct _M0TWRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamERPB4Json* _M0L1fS865
) {
  int32_t _M0L3lenS2598;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS860;
  int32_t _M0L7_2abindS862;
  int32_t _M0L1iS863;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2598 = _M0L4selfS861->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS860 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2598);
  _M0L7_2abindS862 = _M0L4selfS861->$1;
  _M0L1iS863 = 0;
  while (1) {
    if (_M0L1iS863 < _M0L7_2abindS862) {
      void** _M0L8_2afieldS4128 = _M0L4selfS861->$0;
      void** _M0L3bufS2597 = _M0L8_2afieldS4128;
      void* _M0L6_2atmpS4127 = (void*)_M0L3bufS2597[_M0L1iS863];
      void* _M0L1vS864 = _M0L6_2atmpS4127;
      void** _M0L8_2afieldS4126 = _M0L3arrS860->$0;
      void** _M0L3bufS2594 = _M0L8_2afieldS4126;
      void* _M0L6_2atmpS2595;
      void* _M0L6_2aoldS4125;
      int32_t _M0L6_2atmpS2596;
      moonbit_incref(_M0L3bufS2594);
      moonbit_incref(_M0L1fS865);
      moonbit_incref(_M0L1vS864);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2595 = _M0L1fS865->code(_M0L1fS865, _M0L1vS864);
      _M0L6_2aoldS4125 = (void*)_M0L3bufS2594[_M0L1iS863];
      moonbit_decref(_M0L6_2aoldS4125);
      _M0L3bufS2594[_M0L1iS863] = _M0L6_2atmpS2595;
      moonbit_decref(_M0L3bufS2594);
      _M0L6_2atmpS2596 = _M0L1iS863 + 1;
      _M0L1iS863 = _M0L6_2atmpS2596;
      continue;
    } else {
      moonbit_decref(_M0L1fS865);
      moonbit_decref(_M0L4selfS861);
    }
    break;
  }
  return _M0L3arrS860;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS868,
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0L1fS872
) {
  int32_t _M0L3lenS2603;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS867;
  int32_t _M0L7_2abindS869;
  int32_t _M0L1iS870;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2603 = _M0L4selfS868->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS867 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2603);
  _M0L7_2abindS869 = _M0L4selfS868->$1;
  _M0L1iS870 = 0;
  while (1) {
    if (_M0L1iS870 < _M0L7_2abindS869) {
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS4132 =
        _M0L4selfS868->$0;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3bufS2602 =
        _M0L8_2afieldS4132;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS4131 =
        (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3bufS2602[
          _M0L1iS870
        ];
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L1vS871 =
        _M0L6_2atmpS4131;
      void** _M0L8_2afieldS4130 = _M0L3arrS867->$0;
      void** _M0L3bufS2599 = _M0L8_2afieldS4130;
      void* _M0L6_2atmpS2600;
      void* _M0L6_2aoldS4129;
      int32_t _M0L6_2atmpS2601;
      moonbit_incref(_M0L3bufS2599);
      moonbit_incref(_M0L1fS872);
      moonbit_incref(_M0L1vS871);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2600 = _M0L1fS872->code(_M0L1fS872, _M0L1vS871);
      _M0L6_2aoldS4129 = (void*)_M0L3bufS2599[_M0L1iS870];
      moonbit_decref(_M0L6_2aoldS4129);
      _M0L3bufS2599[_M0L1iS870] = _M0L6_2atmpS2600;
      moonbit_decref(_M0L3bufS2599);
      _M0L6_2atmpS2601 = _M0L1iS870 + 1;
      _M0L1iS870 = _M0L6_2atmpS2601;
      continue;
    } else {
      moonbit_decref(_M0L1fS872);
      moonbit_decref(_M0L4selfS868);
    }
    break;
  }
  return _M0L3arrS867;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS852) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS852;
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS851) {
  void* _block_4664;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4664 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_4664)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_4664)->$0 = _M0L6objectS851;
  return _block_4664;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS850) {
  void* _block_4665;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4665 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4665)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4665)->$0 = _M0L6stringS850;
  return _block_4665;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS843
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4133;
  int32_t _M0L6_2acntS4486;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2588;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS842;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__* _closure_4666;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2583;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4133 = _M0L4selfS843->$5;
  _M0L6_2acntS4486 = Moonbit_object_header(_M0L4selfS843)->rc;
  if (_M0L6_2acntS4486 > 1) {
    int32_t _M0L11_2anew__cntS4488 = _M0L6_2acntS4486 - 1;
    Moonbit_object_header(_M0L4selfS843)->rc = _M0L11_2anew__cntS4488;
    if (_M0L8_2afieldS4133) {
      moonbit_incref(_M0L8_2afieldS4133);
    }
  } else if (_M0L6_2acntS4486 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4487 = _M0L4selfS843->$0;
    moonbit_decref(_M0L8_2afieldS4487);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS843);
  }
  _M0L4headS2588 = _M0L8_2afieldS4133;
  _M0L11curr__entryS842
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS842)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS842->$0 = _M0L4headS2588;
  _closure_4666
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__));
  Moonbit_object_header(_closure_4666)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__, $0) >> 2, 1, 0);
  _closure_4666->code = &_M0MPB3Map4iterGsRPB4JsonEC2584l591;
  _closure_4666->$0 = _M0L11curr__entryS842;
  _M0L6_2atmpS2583 = (struct _M0TWEOUsRPB4JsonE*)_closure_4666;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2583);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2584l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2585
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__* _M0L14_2acasted__envS2586;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS4139;
  int32_t _M0L6_2acntS4489;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS842;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4138;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS844;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2586
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2584__l591__*)_M0L6_2aenvS2585;
  _M0L8_2afieldS4139 = _M0L14_2acasted__envS2586->$0;
  _M0L6_2acntS4489 = Moonbit_object_header(_M0L14_2acasted__envS2586)->rc;
  if (_M0L6_2acntS4489 > 1) {
    int32_t _M0L11_2anew__cntS4490 = _M0L6_2acntS4489 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2586)->rc
    = _M0L11_2anew__cntS4490;
    moonbit_incref(_M0L8_2afieldS4139);
  } else if (_M0L6_2acntS4489 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2586);
  }
  _M0L11curr__entryS842 = _M0L8_2afieldS4139;
  _M0L8_2afieldS4138 = _M0L11curr__entryS842->$0;
  _M0L7_2abindS844 = _M0L8_2afieldS4138;
  if (_M0L7_2abindS844 == 0) {
    moonbit_decref(_M0L11curr__entryS842);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS845 = _M0L7_2abindS844;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS846 = _M0L7_2aSomeS845;
    moonbit_string_t _M0L8_2afieldS4137 = _M0L4_2axS846->$4;
    moonbit_string_t _M0L6_2akeyS847 = _M0L8_2afieldS4137;
    void* _M0L8_2afieldS4136 = _M0L4_2axS846->$5;
    void* _M0L8_2avalueS848 = _M0L8_2afieldS4136;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4135 = _M0L4_2axS846->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS849 = _M0L8_2afieldS4135;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4134 =
      _M0L11curr__entryS842->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2587;
    if (_M0L7_2anextS849) {
      moonbit_incref(_M0L7_2anextS849);
    }
    moonbit_incref(_M0L8_2avalueS848);
    moonbit_incref(_M0L6_2akeyS847);
    if (_M0L6_2aoldS4134) {
      moonbit_decref(_M0L6_2aoldS4134);
    }
    _M0L11curr__entryS842->$0 = _M0L7_2anextS849;
    moonbit_decref(_M0L11curr__entryS842);
    _M0L8_2atupleS2587
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2587)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2587->$0 = _M0L6_2akeyS847;
    _M0L8_2atupleS2587->$1 = _M0L8_2avalueS848;
    return _M0L8_2atupleS2587;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS841
) {
  int32_t _M0L8_2afieldS4140;
  int32_t _M0L4sizeS2582;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4140 = _M0L4selfS841->$1;
  moonbit_decref(_M0L4selfS841);
  _M0L4sizeS2582 = _M0L8_2afieldS4140;
  return _M0L4sizeS2582 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS828,
  int32_t _M0L3keyS824
) {
  int32_t _M0L4hashS823;
  int32_t _M0L14capacity__maskS2567;
  int32_t _M0L6_2atmpS2566;
  int32_t _M0L1iS825;
  int32_t _M0L3idxS826;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS823 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS824);
  _M0L14capacity__maskS2567 = _M0L4selfS828->$3;
  _M0L6_2atmpS2566 = _M0L4hashS823 & _M0L14capacity__maskS2567;
  _M0L1iS825 = 0;
  _M0L3idxS826 = _M0L6_2atmpS2566;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4144 =
      _M0L4selfS828->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2565 =
      _M0L8_2afieldS4144;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4143;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS827;
    if (
      _M0L3idxS826 < 0
      || _M0L3idxS826 >= Moonbit_array_length(_M0L7entriesS2565)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4143
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2565[
        _M0L3idxS826
      ];
    _M0L7_2abindS827 = _M0L6_2atmpS4143;
    if (_M0L7_2abindS827 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2554;
      if (_M0L7_2abindS827) {
        moonbit_incref(_M0L7_2abindS827);
      }
      moonbit_decref(_M0L4selfS828);
      if (_M0L7_2abindS827) {
        moonbit_decref(_M0L7_2abindS827);
      }
      _M0L6_2atmpS2554 = 0;
      return _M0L6_2atmpS2554;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS829 =
        _M0L7_2abindS827;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS830 =
        _M0L7_2aSomeS829;
      int32_t _M0L4hashS2556 = _M0L8_2aentryS830->$3;
      int32_t _if__result_4668;
      int32_t _M0L8_2afieldS4141;
      int32_t _M0L3pslS2559;
      int32_t _M0L6_2atmpS2561;
      int32_t _M0L6_2atmpS2563;
      int32_t _M0L14capacity__maskS2564;
      int32_t _M0L6_2atmpS2562;
      if (_M0L4hashS2556 == _M0L4hashS823) {
        int32_t _M0L3keyS2555 = _M0L8_2aentryS830->$4;
        _if__result_4668 = _M0L3keyS2555 == _M0L3keyS824;
      } else {
        _if__result_4668 = 0;
      }
      if (_if__result_4668) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4142;
        int32_t _M0L6_2acntS4491;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2558;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2557;
        moonbit_incref(_M0L8_2aentryS830);
        moonbit_decref(_M0L4selfS828);
        _M0L8_2afieldS4142 = _M0L8_2aentryS830->$5;
        _M0L6_2acntS4491 = Moonbit_object_header(_M0L8_2aentryS830)->rc;
        if (_M0L6_2acntS4491 > 1) {
          int32_t _M0L11_2anew__cntS4493 = _M0L6_2acntS4491 - 1;
          Moonbit_object_header(_M0L8_2aentryS830)->rc
          = _M0L11_2anew__cntS4493;
          moonbit_incref(_M0L8_2afieldS4142);
        } else if (_M0L6_2acntS4491 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4492 =
            _M0L8_2aentryS830->$1;
          if (_M0L8_2afieldS4492) {
            moonbit_decref(_M0L8_2afieldS4492);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS830);
        }
        _M0L5valueS2558 = _M0L8_2afieldS4142;
        _M0L6_2atmpS2557 = _M0L5valueS2558;
        return _M0L6_2atmpS2557;
      } else {
        moonbit_incref(_M0L8_2aentryS830);
      }
      _M0L8_2afieldS4141 = _M0L8_2aentryS830->$2;
      moonbit_decref(_M0L8_2aentryS830);
      _M0L3pslS2559 = _M0L8_2afieldS4141;
      if (_M0L1iS825 > _M0L3pslS2559) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2560;
        moonbit_decref(_M0L4selfS828);
        _M0L6_2atmpS2560 = 0;
        return _M0L6_2atmpS2560;
      }
      _M0L6_2atmpS2561 = _M0L1iS825 + 1;
      _M0L6_2atmpS2563 = _M0L3idxS826 + 1;
      _M0L14capacity__maskS2564 = _M0L4selfS828->$3;
      _M0L6_2atmpS2562 = _M0L6_2atmpS2563 & _M0L14capacity__maskS2564;
      _M0L1iS825 = _M0L6_2atmpS2561;
      _M0L3idxS826 = _M0L6_2atmpS2562;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS837,
  moonbit_string_t _M0L3keyS833
) {
  int32_t _M0L4hashS832;
  int32_t _M0L14capacity__maskS2581;
  int32_t _M0L6_2atmpS2580;
  int32_t _M0L1iS834;
  int32_t _M0L3idxS835;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS833);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS832 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS833);
  _M0L14capacity__maskS2581 = _M0L4selfS837->$3;
  _M0L6_2atmpS2580 = _M0L4hashS832 & _M0L14capacity__maskS2581;
  _M0L1iS834 = 0;
  _M0L3idxS835 = _M0L6_2atmpS2580;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4150 =
      _M0L4selfS837->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2579 =
      _M0L8_2afieldS4150;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4149;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS836;
    if (
      _M0L3idxS835 < 0
      || _M0L3idxS835 >= Moonbit_array_length(_M0L7entriesS2579)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4149
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2579[
        _M0L3idxS835
      ];
    _M0L7_2abindS836 = _M0L6_2atmpS4149;
    if (_M0L7_2abindS836 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2568;
      if (_M0L7_2abindS836) {
        moonbit_incref(_M0L7_2abindS836);
      }
      moonbit_decref(_M0L4selfS837);
      if (_M0L7_2abindS836) {
        moonbit_decref(_M0L7_2abindS836);
      }
      moonbit_decref(_M0L3keyS833);
      _M0L6_2atmpS2568 = 0;
      return _M0L6_2atmpS2568;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS838 =
        _M0L7_2abindS836;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS839 =
        _M0L7_2aSomeS838;
      int32_t _M0L4hashS2570 = _M0L8_2aentryS839->$3;
      int32_t _if__result_4670;
      int32_t _M0L8_2afieldS4145;
      int32_t _M0L3pslS2573;
      int32_t _M0L6_2atmpS2575;
      int32_t _M0L6_2atmpS2577;
      int32_t _M0L14capacity__maskS2578;
      int32_t _M0L6_2atmpS2576;
      if (_M0L4hashS2570 == _M0L4hashS832) {
        moonbit_string_t _M0L8_2afieldS4148 = _M0L8_2aentryS839->$4;
        moonbit_string_t _M0L3keyS2569 = _M0L8_2afieldS4148;
        int32_t _M0L6_2atmpS4147;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4147
        = moonbit_val_array_equal(_M0L3keyS2569, _M0L3keyS833);
        _if__result_4670 = _M0L6_2atmpS4147;
      } else {
        _if__result_4670 = 0;
      }
      if (_if__result_4670) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4146;
        int32_t _M0L6_2acntS4494;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2572;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2571;
        moonbit_incref(_M0L8_2aentryS839);
        moonbit_decref(_M0L4selfS837);
        moonbit_decref(_M0L3keyS833);
        _M0L8_2afieldS4146 = _M0L8_2aentryS839->$5;
        _M0L6_2acntS4494 = Moonbit_object_header(_M0L8_2aentryS839)->rc;
        if (_M0L6_2acntS4494 > 1) {
          int32_t _M0L11_2anew__cntS4497 = _M0L6_2acntS4494 - 1;
          Moonbit_object_header(_M0L8_2aentryS839)->rc
          = _M0L11_2anew__cntS4497;
          moonbit_incref(_M0L8_2afieldS4146);
        } else if (_M0L6_2acntS4494 == 1) {
          moonbit_string_t _M0L8_2afieldS4496 = _M0L8_2aentryS839->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4495;
          moonbit_decref(_M0L8_2afieldS4496);
          _M0L8_2afieldS4495 = _M0L8_2aentryS839->$1;
          if (_M0L8_2afieldS4495) {
            moonbit_decref(_M0L8_2afieldS4495);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS839);
        }
        _M0L5valueS2572 = _M0L8_2afieldS4146;
        _M0L6_2atmpS2571 = _M0L5valueS2572;
        return _M0L6_2atmpS2571;
      } else {
        moonbit_incref(_M0L8_2aentryS839);
      }
      _M0L8_2afieldS4145 = _M0L8_2aentryS839->$2;
      moonbit_decref(_M0L8_2aentryS839);
      _M0L3pslS2573 = _M0L8_2afieldS4145;
      if (_M0L1iS834 > _M0L3pslS2573) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2574;
        moonbit_decref(_M0L4selfS837);
        moonbit_decref(_M0L3keyS833);
        _M0L6_2atmpS2574 = 0;
        return _M0L6_2atmpS2574;
      }
      _M0L6_2atmpS2575 = _M0L1iS834 + 1;
      _M0L6_2atmpS2577 = _M0L3idxS835 + 1;
      _M0L14capacity__maskS2578 = _M0L4selfS837->$3;
      _M0L6_2atmpS2576 = _M0L6_2atmpS2577 & _M0L14capacity__maskS2578;
      _M0L1iS834 = _M0L6_2atmpS2575;
      _M0L3idxS835 = _M0L6_2atmpS2576;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS800
) {
  int32_t _M0L6lengthS799;
  int32_t _M0Lm8capacityS801;
  int32_t _M0L6_2atmpS2519;
  int32_t _M0L6_2atmpS2518;
  int32_t _M0L6_2atmpS2529;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS802;
  int32_t _M0L3endS2527;
  int32_t _M0L5startS2528;
  int32_t _M0L7_2abindS803;
  int32_t _M0L2__S804;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS800.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS799
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS800);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS801 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS799);
  _M0L6_2atmpS2519 = _M0Lm8capacityS801;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2518 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2519);
  if (_M0L6lengthS799 > _M0L6_2atmpS2518) {
    int32_t _M0L6_2atmpS2520 = _M0Lm8capacityS801;
    _M0Lm8capacityS801 = _M0L6_2atmpS2520 * 2;
  }
  _M0L6_2atmpS2529 = _M0Lm8capacityS801;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS802
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2529);
  _M0L3endS2527 = _M0L3arrS800.$2;
  _M0L5startS2528 = _M0L3arrS800.$1;
  _M0L7_2abindS803 = _M0L3endS2527 - _M0L5startS2528;
  _M0L2__S804 = 0;
  while (1) {
    if (_M0L2__S804 < _M0L7_2abindS803) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4154 =
        _M0L3arrS800.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2524 =
        _M0L8_2afieldS4154;
      int32_t _M0L5startS2526 = _M0L3arrS800.$1;
      int32_t _M0L6_2atmpS2525 = _M0L5startS2526 + _M0L2__S804;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4153 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2524[
          _M0L6_2atmpS2525
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS805 =
        _M0L6_2atmpS4153;
      moonbit_string_t _M0L8_2afieldS4152 = _M0L1eS805->$0;
      moonbit_string_t _M0L6_2atmpS2521 = _M0L8_2afieldS4152;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4151 =
        _M0L1eS805->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2522 =
        _M0L8_2afieldS4151;
      int32_t _M0L6_2atmpS2523;
      moonbit_incref(_M0L6_2atmpS2522);
      moonbit_incref(_M0L6_2atmpS2521);
      moonbit_incref(_M0L1mS802);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS802, _M0L6_2atmpS2521, _M0L6_2atmpS2522);
      _M0L6_2atmpS2523 = _M0L2__S804 + 1;
      _M0L2__S804 = _M0L6_2atmpS2523;
      continue;
    } else {
      moonbit_decref(_M0L3arrS800.$0);
    }
    break;
  }
  return _M0L1mS802;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS808
) {
  int32_t _M0L6lengthS807;
  int32_t _M0Lm8capacityS809;
  int32_t _M0L6_2atmpS2531;
  int32_t _M0L6_2atmpS2530;
  int32_t _M0L6_2atmpS2541;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS810;
  int32_t _M0L3endS2539;
  int32_t _M0L5startS2540;
  int32_t _M0L7_2abindS811;
  int32_t _M0L2__S812;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS808.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS807
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS808);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS809 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS807);
  _M0L6_2atmpS2531 = _M0Lm8capacityS809;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2530 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2531);
  if (_M0L6lengthS807 > _M0L6_2atmpS2530) {
    int32_t _M0L6_2atmpS2532 = _M0Lm8capacityS809;
    _M0Lm8capacityS809 = _M0L6_2atmpS2532 * 2;
  }
  _M0L6_2atmpS2541 = _M0Lm8capacityS809;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS810
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2541);
  _M0L3endS2539 = _M0L3arrS808.$2;
  _M0L5startS2540 = _M0L3arrS808.$1;
  _M0L7_2abindS811 = _M0L3endS2539 - _M0L5startS2540;
  _M0L2__S812 = 0;
  while (1) {
    if (_M0L2__S812 < _M0L7_2abindS811) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4157 =
        _M0L3arrS808.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2536 =
        _M0L8_2afieldS4157;
      int32_t _M0L5startS2538 = _M0L3arrS808.$1;
      int32_t _M0L6_2atmpS2537 = _M0L5startS2538 + _M0L2__S812;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4156 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2536[
          _M0L6_2atmpS2537
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS813 = _M0L6_2atmpS4156;
      int32_t _M0L6_2atmpS2533 = _M0L1eS813->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4155 =
        _M0L1eS813->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2534 =
        _M0L8_2afieldS4155;
      int32_t _M0L6_2atmpS2535;
      moonbit_incref(_M0L6_2atmpS2534);
      moonbit_incref(_M0L1mS810);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS810, _M0L6_2atmpS2533, _M0L6_2atmpS2534);
      _M0L6_2atmpS2535 = _M0L2__S812 + 1;
      _M0L2__S812 = _M0L6_2atmpS2535;
      continue;
    } else {
      moonbit_decref(_M0L3arrS808.$0);
    }
    break;
  }
  return _M0L1mS810;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS816
) {
  int32_t _M0L6lengthS815;
  int32_t _M0Lm8capacityS817;
  int32_t _M0L6_2atmpS2543;
  int32_t _M0L6_2atmpS2542;
  int32_t _M0L6_2atmpS2553;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS818;
  int32_t _M0L3endS2551;
  int32_t _M0L5startS2552;
  int32_t _M0L7_2abindS819;
  int32_t _M0L2__S820;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS816.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS815 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS816);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS817 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS815);
  _M0L6_2atmpS2543 = _M0Lm8capacityS817;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2542 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2543);
  if (_M0L6lengthS815 > _M0L6_2atmpS2542) {
    int32_t _M0L6_2atmpS2544 = _M0Lm8capacityS817;
    _M0Lm8capacityS817 = _M0L6_2atmpS2544 * 2;
  }
  _M0L6_2atmpS2553 = _M0Lm8capacityS817;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS818 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2553);
  _M0L3endS2551 = _M0L3arrS816.$2;
  _M0L5startS2552 = _M0L3arrS816.$1;
  _M0L7_2abindS819 = _M0L3endS2551 - _M0L5startS2552;
  _M0L2__S820 = 0;
  while (1) {
    if (_M0L2__S820 < _M0L7_2abindS819) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS4161 = _M0L3arrS816.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2548 = _M0L8_2afieldS4161;
      int32_t _M0L5startS2550 = _M0L3arrS816.$1;
      int32_t _M0L6_2atmpS2549 = _M0L5startS2550 + _M0L2__S820;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS4160 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2548[_M0L6_2atmpS2549];
      struct _M0TUsRPB4JsonE* _M0L1eS821 = _M0L6_2atmpS4160;
      moonbit_string_t _M0L8_2afieldS4159 = _M0L1eS821->$0;
      moonbit_string_t _M0L6_2atmpS2545 = _M0L8_2afieldS4159;
      void* _M0L8_2afieldS4158 = _M0L1eS821->$1;
      void* _M0L6_2atmpS2546 = _M0L8_2afieldS4158;
      int32_t _M0L6_2atmpS2547;
      moonbit_incref(_M0L6_2atmpS2546);
      moonbit_incref(_M0L6_2atmpS2545);
      moonbit_incref(_M0L1mS818);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS818, _M0L6_2atmpS2545, _M0L6_2atmpS2546);
      _M0L6_2atmpS2547 = _M0L2__S820 + 1;
      _M0L2__S820 = _M0L6_2atmpS2547;
      continue;
    } else {
      moonbit_decref(_M0L3arrS816.$0);
    }
    break;
  }
  return _M0L1mS818;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS790,
  moonbit_string_t _M0L3keyS791,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS792
) {
  int32_t _M0L6_2atmpS2515;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS791);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2515 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS791);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS790, _M0L3keyS791, _M0L5valueS792, _M0L6_2atmpS2515);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS793,
  int32_t _M0L3keyS794,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS795
) {
  int32_t _M0L6_2atmpS2516;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2516 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS794);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS793, _M0L3keyS794, _M0L5valueS795, _M0L6_2atmpS2516);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS796,
  moonbit_string_t _M0L3keyS797,
  void* _M0L5valueS798
) {
  int32_t _M0L6_2atmpS2517;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS797);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2517 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS797);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS796, _M0L3keyS797, _M0L5valueS798, _M0L6_2atmpS2517);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS758
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4168;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS757;
  int32_t _M0L8capacityS2500;
  int32_t _M0L13new__capacityS759;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2495;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2494;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS4167;
  int32_t _M0L6_2atmpS2496;
  int32_t _M0L8capacityS2498;
  int32_t _M0L6_2atmpS2497;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2499;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4166;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS760;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4168 = _M0L4selfS758->$5;
  _M0L9old__headS757 = _M0L8_2afieldS4168;
  _M0L8capacityS2500 = _M0L4selfS758->$2;
  _M0L13new__capacityS759 = _M0L8capacityS2500 << 1;
  _M0L6_2atmpS2495 = 0;
  _M0L6_2atmpS2494
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS759, _M0L6_2atmpS2495);
  _M0L6_2aoldS4167 = _M0L4selfS758->$0;
  if (_M0L9old__headS757) {
    moonbit_incref(_M0L9old__headS757);
  }
  moonbit_decref(_M0L6_2aoldS4167);
  _M0L4selfS758->$0 = _M0L6_2atmpS2494;
  _M0L4selfS758->$2 = _M0L13new__capacityS759;
  _M0L6_2atmpS2496 = _M0L13new__capacityS759 - 1;
  _M0L4selfS758->$3 = _M0L6_2atmpS2496;
  _M0L8capacityS2498 = _M0L4selfS758->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2497 = _M0FPB21calc__grow__threshold(_M0L8capacityS2498);
  _M0L4selfS758->$4 = _M0L6_2atmpS2497;
  _M0L4selfS758->$1 = 0;
  _M0L6_2atmpS2499 = 0;
  _M0L6_2aoldS4166 = _M0L4selfS758->$5;
  if (_M0L6_2aoldS4166) {
    moonbit_decref(_M0L6_2aoldS4166);
  }
  _M0L4selfS758->$5 = _M0L6_2atmpS2499;
  _M0L4selfS758->$6 = -1;
  _M0L8_2aparamS760 = _M0L9old__headS757;
  while (1) {
    if (_M0L8_2aparamS760 == 0) {
      if (_M0L8_2aparamS760) {
        moonbit_decref(_M0L8_2aparamS760);
      }
      moonbit_decref(_M0L4selfS758);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS761 =
        _M0L8_2aparamS760;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS762 =
        _M0L7_2aSomeS761;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4165 =
        _M0L4_2axS762->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS763 =
        _M0L8_2afieldS4165;
      moonbit_string_t _M0L8_2afieldS4164 = _M0L4_2axS762->$4;
      moonbit_string_t _M0L6_2akeyS764 = _M0L8_2afieldS4164;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4163 =
        _M0L4_2axS762->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS765 =
        _M0L8_2afieldS4163;
      int32_t _M0L8_2afieldS4162 = _M0L4_2axS762->$3;
      int32_t _M0L6_2acntS4498 = Moonbit_object_header(_M0L4_2axS762)->rc;
      int32_t _M0L7_2ahashS766;
      if (_M0L6_2acntS4498 > 1) {
        int32_t _M0L11_2anew__cntS4499 = _M0L6_2acntS4498 - 1;
        Moonbit_object_header(_M0L4_2axS762)->rc = _M0L11_2anew__cntS4499;
        moonbit_incref(_M0L8_2avalueS765);
        moonbit_incref(_M0L6_2akeyS764);
        if (_M0L7_2anextS763) {
          moonbit_incref(_M0L7_2anextS763);
        }
      } else if (_M0L6_2acntS4498 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS762);
      }
      _M0L7_2ahashS766 = _M0L8_2afieldS4162;
      moonbit_incref(_M0L4selfS758);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS758, _M0L6_2akeyS764, _M0L8_2avalueS765, _M0L7_2ahashS766);
      _M0L8_2aparamS760 = _M0L7_2anextS763;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS769
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4174;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS768;
  int32_t _M0L8capacityS2507;
  int32_t _M0L13new__capacityS770;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2502;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2501;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4173;
  int32_t _M0L6_2atmpS2503;
  int32_t _M0L8capacityS2505;
  int32_t _M0L6_2atmpS2504;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2506;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4172;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS771;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4174 = _M0L4selfS769->$5;
  _M0L9old__headS768 = _M0L8_2afieldS4174;
  _M0L8capacityS2507 = _M0L4selfS769->$2;
  _M0L13new__capacityS770 = _M0L8capacityS2507 << 1;
  _M0L6_2atmpS2502 = 0;
  _M0L6_2atmpS2501
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS770, _M0L6_2atmpS2502);
  _M0L6_2aoldS4173 = _M0L4selfS769->$0;
  if (_M0L9old__headS768) {
    moonbit_incref(_M0L9old__headS768);
  }
  moonbit_decref(_M0L6_2aoldS4173);
  _M0L4selfS769->$0 = _M0L6_2atmpS2501;
  _M0L4selfS769->$2 = _M0L13new__capacityS770;
  _M0L6_2atmpS2503 = _M0L13new__capacityS770 - 1;
  _M0L4selfS769->$3 = _M0L6_2atmpS2503;
  _M0L8capacityS2505 = _M0L4selfS769->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2504 = _M0FPB21calc__grow__threshold(_M0L8capacityS2505);
  _M0L4selfS769->$4 = _M0L6_2atmpS2504;
  _M0L4selfS769->$1 = 0;
  _M0L6_2atmpS2506 = 0;
  _M0L6_2aoldS4172 = _M0L4selfS769->$5;
  if (_M0L6_2aoldS4172) {
    moonbit_decref(_M0L6_2aoldS4172);
  }
  _M0L4selfS769->$5 = _M0L6_2atmpS2506;
  _M0L4selfS769->$6 = -1;
  _M0L8_2aparamS771 = _M0L9old__headS768;
  while (1) {
    if (_M0L8_2aparamS771 == 0) {
      if (_M0L8_2aparamS771) {
        moonbit_decref(_M0L8_2aparamS771);
      }
      moonbit_decref(_M0L4selfS769);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS772 =
        _M0L8_2aparamS771;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS773 =
        _M0L7_2aSomeS772;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4171 =
        _M0L4_2axS773->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS774 =
        _M0L8_2afieldS4171;
      int32_t _M0L6_2akeyS775 = _M0L4_2axS773->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4170 =
        _M0L4_2axS773->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS776 =
        _M0L8_2afieldS4170;
      int32_t _M0L8_2afieldS4169 = _M0L4_2axS773->$3;
      int32_t _M0L6_2acntS4500 = Moonbit_object_header(_M0L4_2axS773)->rc;
      int32_t _M0L7_2ahashS777;
      if (_M0L6_2acntS4500 > 1) {
        int32_t _M0L11_2anew__cntS4501 = _M0L6_2acntS4500 - 1;
        Moonbit_object_header(_M0L4_2axS773)->rc = _M0L11_2anew__cntS4501;
        moonbit_incref(_M0L8_2avalueS776);
        if (_M0L7_2anextS774) {
          moonbit_incref(_M0L7_2anextS774);
        }
      } else if (_M0L6_2acntS4500 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS773);
      }
      _M0L7_2ahashS777 = _M0L8_2afieldS4169;
      moonbit_incref(_M0L4selfS769);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS769, _M0L6_2akeyS775, _M0L8_2avalueS776, _M0L7_2ahashS777);
      _M0L8_2aparamS771 = _M0L7_2anextS774;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS780
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4181;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS779;
  int32_t _M0L8capacityS2514;
  int32_t _M0L13new__capacityS781;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2509;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2508;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS4180;
  int32_t _M0L6_2atmpS2510;
  int32_t _M0L8capacityS2512;
  int32_t _M0L6_2atmpS2511;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2513;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4179;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS782;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4181 = _M0L4selfS780->$5;
  _M0L9old__headS779 = _M0L8_2afieldS4181;
  _M0L8capacityS2514 = _M0L4selfS780->$2;
  _M0L13new__capacityS781 = _M0L8capacityS2514 << 1;
  _M0L6_2atmpS2509 = 0;
  _M0L6_2atmpS2508
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS781, _M0L6_2atmpS2509);
  _M0L6_2aoldS4180 = _M0L4selfS780->$0;
  if (_M0L9old__headS779) {
    moonbit_incref(_M0L9old__headS779);
  }
  moonbit_decref(_M0L6_2aoldS4180);
  _M0L4selfS780->$0 = _M0L6_2atmpS2508;
  _M0L4selfS780->$2 = _M0L13new__capacityS781;
  _M0L6_2atmpS2510 = _M0L13new__capacityS781 - 1;
  _M0L4selfS780->$3 = _M0L6_2atmpS2510;
  _M0L8capacityS2512 = _M0L4selfS780->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2511 = _M0FPB21calc__grow__threshold(_M0L8capacityS2512);
  _M0L4selfS780->$4 = _M0L6_2atmpS2511;
  _M0L4selfS780->$1 = 0;
  _M0L6_2atmpS2513 = 0;
  _M0L6_2aoldS4179 = _M0L4selfS780->$5;
  if (_M0L6_2aoldS4179) {
    moonbit_decref(_M0L6_2aoldS4179);
  }
  _M0L4selfS780->$5 = _M0L6_2atmpS2513;
  _M0L4selfS780->$6 = -1;
  _M0L8_2aparamS782 = _M0L9old__headS779;
  while (1) {
    if (_M0L8_2aparamS782 == 0) {
      if (_M0L8_2aparamS782) {
        moonbit_decref(_M0L8_2aparamS782);
      }
      moonbit_decref(_M0L4selfS780);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS783 = _M0L8_2aparamS782;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS784 = _M0L7_2aSomeS783;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4178 = _M0L4_2axS784->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS785 = _M0L8_2afieldS4178;
      moonbit_string_t _M0L8_2afieldS4177 = _M0L4_2axS784->$4;
      moonbit_string_t _M0L6_2akeyS786 = _M0L8_2afieldS4177;
      void* _M0L8_2afieldS4176 = _M0L4_2axS784->$5;
      void* _M0L8_2avalueS787 = _M0L8_2afieldS4176;
      int32_t _M0L8_2afieldS4175 = _M0L4_2axS784->$3;
      int32_t _M0L6_2acntS4502 = Moonbit_object_header(_M0L4_2axS784)->rc;
      int32_t _M0L7_2ahashS788;
      if (_M0L6_2acntS4502 > 1) {
        int32_t _M0L11_2anew__cntS4503 = _M0L6_2acntS4502 - 1;
        Moonbit_object_header(_M0L4_2axS784)->rc = _M0L11_2anew__cntS4503;
        moonbit_incref(_M0L8_2avalueS787);
        moonbit_incref(_M0L6_2akeyS786);
        if (_M0L7_2anextS785) {
          moonbit_incref(_M0L7_2anextS785);
        }
      } else if (_M0L6_2acntS4502 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS784);
      }
      _M0L7_2ahashS788 = _M0L8_2afieldS4175;
      moonbit_incref(_M0L4selfS780);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS780, _M0L6_2akeyS786, _M0L8_2avalueS787, _M0L7_2ahashS788);
      _M0L8_2aparamS782 = _M0L7_2anextS785;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS712,
  moonbit_string_t _M0L3keyS718,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS719,
  int32_t _M0L4hashS714
) {
  int32_t _M0L14capacity__maskS2457;
  int32_t _M0L6_2atmpS2456;
  int32_t _M0L3pslS709;
  int32_t _M0L3idxS710;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2457 = _M0L4selfS712->$3;
  _M0L6_2atmpS2456 = _M0L4hashS714 & _M0L14capacity__maskS2457;
  _M0L3pslS709 = 0;
  _M0L3idxS710 = _M0L6_2atmpS2456;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4186 =
      _M0L4selfS712->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2455 =
      _M0L8_2afieldS4186;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4185;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS711;
    if (
      _M0L3idxS710 < 0
      || _M0L3idxS710 >= Moonbit_array_length(_M0L7entriesS2455)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4185
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2455[
        _M0L3idxS710
      ];
    _M0L7_2abindS711 = _M0L6_2atmpS4185;
    if (_M0L7_2abindS711 == 0) {
      int32_t _M0L4sizeS2440 = _M0L4selfS712->$1;
      int32_t _M0L8grow__atS2441 = _M0L4selfS712->$4;
      int32_t _M0L7_2abindS715;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS716;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS717;
      if (_M0L4sizeS2440 >= _M0L8grow__atS2441) {
        int32_t _M0L14capacity__maskS2443;
        int32_t _M0L6_2atmpS2442;
        moonbit_incref(_M0L4selfS712);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS712);
        _M0L14capacity__maskS2443 = _M0L4selfS712->$3;
        _M0L6_2atmpS2442 = _M0L4hashS714 & _M0L14capacity__maskS2443;
        _M0L3pslS709 = 0;
        _M0L3idxS710 = _M0L6_2atmpS2442;
        continue;
      }
      _M0L7_2abindS715 = _M0L4selfS712->$6;
      _M0L7_2abindS716 = 0;
      _M0L5entryS717
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS717)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS717->$0 = _M0L7_2abindS715;
      _M0L5entryS717->$1 = _M0L7_2abindS716;
      _M0L5entryS717->$2 = _M0L3pslS709;
      _M0L5entryS717->$3 = _M0L4hashS714;
      _M0L5entryS717->$4 = _M0L3keyS718;
      _M0L5entryS717->$5 = _M0L5valueS719;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS712, _M0L3idxS710, _M0L5entryS717);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS720 =
        _M0L7_2abindS711;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS721 =
        _M0L7_2aSomeS720;
      int32_t _M0L4hashS2445 = _M0L14_2acurr__entryS721->$3;
      int32_t _if__result_4678;
      int32_t _M0L3pslS2446;
      int32_t _M0L6_2atmpS2451;
      int32_t _M0L6_2atmpS2453;
      int32_t _M0L14capacity__maskS2454;
      int32_t _M0L6_2atmpS2452;
      if (_M0L4hashS2445 == _M0L4hashS714) {
        moonbit_string_t _M0L8_2afieldS4184 = _M0L14_2acurr__entryS721->$4;
        moonbit_string_t _M0L3keyS2444 = _M0L8_2afieldS4184;
        int32_t _M0L6_2atmpS4183;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4183
        = moonbit_val_array_equal(_M0L3keyS2444, _M0L3keyS718);
        _if__result_4678 = _M0L6_2atmpS4183;
      } else {
        _if__result_4678 = 0;
      }
      if (_if__result_4678) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4182;
        moonbit_incref(_M0L14_2acurr__entryS721);
        moonbit_decref(_M0L3keyS718);
        moonbit_decref(_M0L4selfS712);
        _M0L6_2aoldS4182 = _M0L14_2acurr__entryS721->$5;
        moonbit_decref(_M0L6_2aoldS4182);
        _M0L14_2acurr__entryS721->$5 = _M0L5valueS719;
        moonbit_decref(_M0L14_2acurr__entryS721);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS721);
      }
      _M0L3pslS2446 = _M0L14_2acurr__entryS721->$2;
      if (_M0L3pslS709 > _M0L3pslS2446) {
        int32_t _M0L4sizeS2447 = _M0L4selfS712->$1;
        int32_t _M0L8grow__atS2448 = _M0L4selfS712->$4;
        int32_t _M0L7_2abindS722;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS723;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS724;
        if (_M0L4sizeS2447 >= _M0L8grow__atS2448) {
          int32_t _M0L14capacity__maskS2450;
          int32_t _M0L6_2atmpS2449;
          moonbit_decref(_M0L14_2acurr__entryS721);
          moonbit_incref(_M0L4selfS712);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS712);
          _M0L14capacity__maskS2450 = _M0L4selfS712->$3;
          _M0L6_2atmpS2449 = _M0L4hashS714 & _M0L14capacity__maskS2450;
          _M0L3pslS709 = 0;
          _M0L3idxS710 = _M0L6_2atmpS2449;
          continue;
        }
        moonbit_incref(_M0L4selfS712);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS712, _M0L3idxS710, _M0L14_2acurr__entryS721);
        _M0L7_2abindS722 = _M0L4selfS712->$6;
        _M0L7_2abindS723 = 0;
        _M0L5entryS724
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS724)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS724->$0 = _M0L7_2abindS722;
        _M0L5entryS724->$1 = _M0L7_2abindS723;
        _M0L5entryS724->$2 = _M0L3pslS709;
        _M0L5entryS724->$3 = _M0L4hashS714;
        _M0L5entryS724->$4 = _M0L3keyS718;
        _M0L5entryS724->$5 = _M0L5valueS719;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS712, _M0L3idxS710, _M0L5entryS724);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS721);
      }
      _M0L6_2atmpS2451 = _M0L3pslS709 + 1;
      _M0L6_2atmpS2453 = _M0L3idxS710 + 1;
      _M0L14capacity__maskS2454 = _M0L4selfS712->$3;
      _M0L6_2atmpS2452 = _M0L6_2atmpS2453 & _M0L14capacity__maskS2454;
      _M0L3pslS709 = _M0L6_2atmpS2451;
      _M0L3idxS710 = _M0L6_2atmpS2452;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS728,
  int32_t _M0L3keyS734,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS735,
  int32_t _M0L4hashS730
) {
  int32_t _M0L14capacity__maskS2475;
  int32_t _M0L6_2atmpS2474;
  int32_t _M0L3pslS725;
  int32_t _M0L3idxS726;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2475 = _M0L4selfS728->$3;
  _M0L6_2atmpS2474 = _M0L4hashS730 & _M0L14capacity__maskS2475;
  _M0L3pslS725 = 0;
  _M0L3idxS726 = _M0L6_2atmpS2474;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4189 =
      _M0L4selfS728->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2473 =
      _M0L8_2afieldS4189;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4188;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS727;
    if (
      _M0L3idxS726 < 0
      || _M0L3idxS726 >= Moonbit_array_length(_M0L7entriesS2473)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4188
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2473[
        _M0L3idxS726
      ];
    _M0L7_2abindS727 = _M0L6_2atmpS4188;
    if (_M0L7_2abindS727 == 0) {
      int32_t _M0L4sizeS2458 = _M0L4selfS728->$1;
      int32_t _M0L8grow__atS2459 = _M0L4selfS728->$4;
      int32_t _M0L7_2abindS731;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS732;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS733;
      if (_M0L4sizeS2458 >= _M0L8grow__atS2459) {
        int32_t _M0L14capacity__maskS2461;
        int32_t _M0L6_2atmpS2460;
        moonbit_incref(_M0L4selfS728);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS728);
        _M0L14capacity__maskS2461 = _M0L4selfS728->$3;
        _M0L6_2atmpS2460 = _M0L4hashS730 & _M0L14capacity__maskS2461;
        _M0L3pslS725 = 0;
        _M0L3idxS726 = _M0L6_2atmpS2460;
        continue;
      }
      _M0L7_2abindS731 = _M0L4selfS728->$6;
      _M0L7_2abindS732 = 0;
      _M0L5entryS733
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS733)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS733->$0 = _M0L7_2abindS731;
      _M0L5entryS733->$1 = _M0L7_2abindS732;
      _M0L5entryS733->$2 = _M0L3pslS725;
      _M0L5entryS733->$3 = _M0L4hashS730;
      _M0L5entryS733->$4 = _M0L3keyS734;
      _M0L5entryS733->$5 = _M0L5valueS735;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS728, _M0L3idxS726, _M0L5entryS733);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS736 =
        _M0L7_2abindS727;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS737 =
        _M0L7_2aSomeS736;
      int32_t _M0L4hashS2463 = _M0L14_2acurr__entryS737->$3;
      int32_t _if__result_4680;
      int32_t _M0L3pslS2464;
      int32_t _M0L6_2atmpS2469;
      int32_t _M0L6_2atmpS2471;
      int32_t _M0L14capacity__maskS2472;
      int32_t _M0L6_2atmpS2470;
      if (_M0L4hashS2463 == _M0L4hashS730) {
        int32_t _M0L3keyS2462 = _M0L14_2acurr__entryS737->$4;
        _if__result_4680 = _M0L3keyS2462 == _M0L3keyS734;
      } else {
        _if__result_4680 = 0;
      }
      if (_if__result_4680) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4187;
        moonbit_incref(_M0L14_2acurr__entryS737);
        moonbit_decref(_M0L4selfS728);
        _M0L6_2aoldS4187 = _M0L14_2acurr__entryS737->$5;
        moonbit_decref(_M0L6_2aoldS4187);
        _M0L14_2acurr__entryS737->$5 = _M0L5valueS735;
        moonbit_decref(_M0L14_2acurr__entryS737);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS737);
      }
      _M0L3pslS2464 = _M0L14_2acurr__entryS737->$2;
      if (_M0L3pslS725 > _M0L3pslS2464) {
        int32_t _M0L4sizeS2465 = _M0L4selfS728->$1;
        int32_t _M0L8grow__atS2466 = _M0L4selfS728->$4;
        int32_t _M0L7_2abindS738;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS739;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS740;
        if (_M0L4sizeS2465 >= _M0L8grow__atS2466) {
          int32_t _M0L14capacity__maskS2468;
          int32_t _M0L6_2atmpS2467;
          moonbit_decref(_M0L14_2acurr__entryS737);
          moonbit_incref(_M0L4selfS728);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS728);
          _M0L14capacity__maskS2468 = _M0L4selfS728->$3;
          _M0L6_2atmpS2467 = _M0L4hashS730 & _M0L14capacity__maskS2468;
          _M0L3pslS725 = 0;
          _M0L3idxS726 = _M0L6_2atmpS2467;
          continue;
        }
        moonbit_incref(_M0L4selfS728);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS728, _M0L3idxS726, _M0L14_2acurr__entryS737);
        _M0L7_2abindS738 = _M0L4selfS728->$6;
        _M0L7_2abindS739 = 0;
        _M0L5entryS740
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS740)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS740->$0 = _M0L7_2abindS738;
        _M0L5entryS740->$1 = _M0L7_2abindS739;
        _M0L5entryS740->$2 = _M0L3pslS725;
        _M0L5entryS740->$3 = _M0L4hashS730;
        _M0L5entryS740->$4 = _M0L3keyS734;
        _M0L5entryS740->$5 = _M0L5valueS735;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS728, _M0L3idxS726, _M0L5entryS740);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS737);
      }
      _M0L6_2atmpS2469 = _M0L3pslS725 + 1;
      _M0L6_2atmpS2471 = _M0L3idxS726 + 1;
      _M0L14capacity__maskS2472 = _M0L4selfS728->$3;
      _M0L6_2atmpS2470 = _M0L6_2atmpS2471 & _M0L14capacity__maskS2472;
      _M0L3pslS725 = _M0L6_2atmpS2469;
      _M0L3idxS726 = _M0L6_2atmpS2470;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS744,
  moonbit_string_t _M0L3keyS750,
  void* _M0L5valueS751,
  int32_t _M0L4hashS746
) {
  int32_t _M0L14capacity__maskS2493;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0L3pslS741;
  int32_t _M0L3idxS742;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2493 = _M0L4selfS744->$3;
  _M0L6_2atmpS2492 = _M0L4hashS746 & _M0L14capacity__maskS2493;
  _M0L3pslS741 = 0;
  _M0L3idxS742 = _M0L6_2atmpS2492;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4194 = _M0L4selfS744->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2491 = _M0L8_2afieldS4194;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4193;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS743;
    if (
      _M0L3idxS742 < 0
      || _M0L3idxS742 >= Moonbit_array_length(_M0L7entriesS2491)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4193
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2491[_M0L3idxS742];
    _M0L7_2abindS743 = _M0L6_2atmpS4193;
    if (_M0L7_2abindS743 == 0) {
      int32_t _M0L4sizeS2476 = _M0L4selfS744->$1;
      int32_t _M0L8grow__atS2477 = _M0L4selfS744->$4;
      int32_t _M0L7_2abindS747;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS748;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS749;
      if (_M0L4sizeS2476 >= _M0L8grow__atS2477) {
        int32_t _M0L14capacity__maskS2479;
        int32_t _M0L6_2atmpS2478;
        moonbit_incref(_M0L4selfS744);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS744);
        _M0L14capacity__maskS2479 = _M0L4selfS744->$3;
        _M0L6_2atmpS2478 = _M0L4hashS746 & _M0L14capacity__maskS2479;
        _M0L3pslS741 = 0;
        _M0L3idxS742 = _M0L6_2atmpS2478;
        continue;
      }
      _M0L7_2abindS747 = _M0L4selfS744->$6;
      _M0L7_2abindS748 = 0;
      _M0L5entryS749
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS749)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS749->$0 = _M0L7_2abindS747;
      _M0L5entryS749->$1 = _M0L7_2abindS748;
      _M0L5entryS749->$2 = _M0L3pslS741;
      _M0L5entryS749->$3 = _M0L4hashS746;
      _M0L5entryS749->$4 = _M0L3keyS750;
      _M0L5entryS749->$5 = _M0L5valueS751;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS744, _M0L3idxS742, _M0L5entryS749);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS752 = _M0L7_2abindS743;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS753 =
        _M0L7_2aSomeS752;
      int32_t _M0L4hashS2481 = _M0L14_2acurr__entryS753->$3;
      int32_t _if__result_4682;
      int32_t _M0L3pslS2482;
      int32_t _M0L6_2atmpS2487;
      int32_t _M0L6_2atmpS2489;
      int32_t _M0L14capacity__maskS2490;
      int32_t _M0L6_2atmpS2488;
      if (_M0L4hashS2481 == _M0L4hashS746) {
        moonbit_string_t _M0L8_2afieldS4192 = _M0L14_2acurr__entryS753->$4;
        moonbit_string_t _M0L3keyS2480 = _M0L8_2afieldS4192;
        int32_t _M0L6_2atmpS4191;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4191
        = moonbit_val_array_equal(_M0L3keyS2480, _M0L3keyS750);
        _if__result_4682 = _M0L6_2atmpS4191;
      } else {
        _if__result_4682 = 0;
      }
      if (_if__result_4682) {
        void* _M0L6_2aoldS4190;
        moonbit_incref(_M0L14_2acurr__entryS753);
        moonbit_decref(_M0L3keyS750);
        moonbit_decref(_M0L4selfS744);
        _M0L6_2aoldS4190 = _M0L14_2acurr__entryS753->$5;
        moonbit_decref(_M0L6_2aoldS4190);
        _M0L14_2acurr__entryS753->$5 = _M0L5valueS751;
        moonbit_decref(_M0L14_2acurr__entryS753);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS753);
      }
      _M0L3pslS2482 = _M0L14_2acurr__entryS753->$2;
      if (_M0L3pslS741 > _M0L3pslS2482) {
        int32_t _M0L4sizeS2483 = _M0L4selfS744->$1;
        int32_t _M0L8grow__atS2484 = _M0L4selfS744->$4;
        int32_t _M0L7_2abindS754;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS755;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS756;
        if (_M0L4sizeS2483 >= _M0L8grow__atS2484) {
          int32_t _M0L14capacity__maskS2486;
          int32_t _M0L6_2atmpS2485;
          moonbit_decref(_M0L14_2acurr__entryS753);
          moonbit_incref(_M0L4selfS744);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS744);
          _M0L14capacity__maskS2486 = _M0L4selfS744->$3;
          _M0L6_2atmpS2485 = _M0L4hashS746 & _M0L14capacity__maskS2486;
          _M0L3pslS741 = 0;
          _M0L3idxS742 = _M0L6_2atmpS2485;
          continue;
        }
        moonbit_incref(_M0L4selfS744);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS744, _M0L3idxS742, _M0L14_2acurr__entryS753);
        _M0L7_2abindS754 = _M0L4selfS744->$6;
        _M0L7_2abindS755 = 0;
        _M0L5entryS756
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS756)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS756->$0 = _M0L7_2abindS754;
        _M0L5entryS756->$1 = _M0L7_2abindS755;
        _M0L5entryS756->$2 = _M0L3pslS741;
        _M0L5entryS756->$3 = _M0L4hashS746;
        _M0L5entryS756->$4 = _M0L3keyS750;
        _M0L5entryS756->$5 = _M0L5valueS751;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS744, _M0L3idxS742, _M0L5entryS756);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS753);
      }
      _M0L6_2atmpS2487 = _M0L3pslS741 + 1;
      _M0L6_2atmpS2489 = _M0L3idxS742 + 1;
      _M0L14capacity__maskS2490 = _M0L4selfS744->$3;
      _M0L6_2atmpS2488 = _M0L6_2atmpS2489 & _M0L14capacity__maskS2490;
      _M0L3pslS741 = _M0L6_2atmpS2487;
      _M0L3idxS742 = _M0L6_2atmpS2488;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS683,
  int32_t _M0L3idxS688,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS687
) {
  int32_t _M0L3pslS2407;
  int32_t _M0L6_2atmpS2403;
  int32_t _M0L6_2atmpS2405;
  int32_t _M0L14capacity__maskS2406;
  int32_t _M0L6_2atmpS2404;
  int32_t _M0L3pslS679;
  int32_t _M0L3idxS680;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS681;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2407 = _M0L5entryS687->$2;
  _M0L6_2atmpS2403 = _M0L3pslS2407 + 1;
  _M0L6_2atmpS2405 = _M0L3idxS688 + 1;
  _M0L14capacity__maskS2406 = _M0L4selfS683->$3;
  _M0L6_2atmpS2404 = _M0L6_2atmpS2405 & _M0L14capacity__maskS2406;
  _M0L3pslS679 = _M0L6_2atmpS2403;
  _M0L3idxS680 = _M0L6_2atmpS2404;
  _M0L5entryS681 = _M0L5entryS687;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4196 =
      _M0L4selfS683->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2402 =
      _M0L8_2afieldS4196;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4195;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS682;
    if (
      _M0L3idxS680 < 0
      || _M0L3idxS680 >= Moonbit_array_length(_M0L7entriesS2402)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4195
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2402[
        _M0L3idxS680
      ];
    _M0L7_2abindS682 = _M0L6_2atmpS4195;
    if (_M0L7_2abindS682 == 0) {
      _M0L5entryS681->$2 = _M0L3pslS679;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS683, _M0L5entryS681, _M0L3idxS680);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS685 =
        _M0L7_2abindS682;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS686 =
        _M0L7_2aSomeS685;
      int32_t _M0L3pslS2392 = _M0L14_2acurr__entryS686->$2;
      if (_M0L3pslS679 > _M0L3pslS2392) {
        int32_t _M0L3pslS2397;
        int32_t _M0L6_2atmpS2393;
        int32_t _M0L6_2atmpS2395;
        int32_t _M0L14capacity__maskS2396;
        int32_t _M0L6_2atmpS2394;
        _M0L5entryS681->$2 = _M0L3pslS679;
        moonbit_incref(_M0L14_2acurr__entryS686);
        moonbit_incref(_M0L4selfS683);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS683, _M0L5entryS681, _M0L3idxS680);
        _M0L3pslS2397 = _M0L14_2acurr__entryS686->$2;
        _M0L6_2atmpS2393 = _M0L3pslS2397 + 1;
        _M0L6_2atmpS2395 = _M0L3idxS680 + 1;
        _M0L14capacity__maskS2396 = _M0L4selfS683->$3;
        _M0L6_2atmpS2394 = _M0L6_2atmpS2395 & _M0L14capacity__maskS2396;
        _M0L3pslS679 = _M0L6_2atmpS2393;
        _M0L3idxS680 = _M0L6_2atmpS2394;
        _M0L5entryS681 = _M0L14_2acurr__entryS686;
        continue;
      } else {
        int32_t _M0L6_2atmpS2398 = _M0L3pslS679 + 1;
        int32_t _M0L6_2atmpS2400 = _M0L3idxS680 + 1;
        int32_t _M0L14capacity__maskS2401 = _M0L4selfS683->$3;
        int32_t _M0L6_2atmpS2399 =
          _M0L6_2atmpS2400 & _M0L14capacity__maskS2401;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4684 =
          _M0L5entryS681;
        _M0L3pslS679 = _M0L6_2atmpS2398;
        _M0L3idxS680 = _M0L6_2atmpS2399;
        _M0L5entryS681 = _tmp_4684;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS693,
  int32_t _M0L3idxS698,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS697
) {
  int32_t _M0L3pslS2423;
  int32_t _M0L6_2atmpS2419;
  int32_t _M0L6_2atmpS2421;
  int32_t _M0L14capacity__maskS2422;
  int32_t _M0L6_2atmpS2420;
  int32_t _M0L3pslS689;
  int32_t _M0L3idxS690;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS691;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2423 = _M0L5entryS697->$2;
  _M0L6_2atmpS2419 = _M0L3pslS2423 + 1;
  _M0L6_2atmpS2421 = _M0L3idxS698 + 1;
  _M0L14capacity__maskS2422 = _M0L4selfS693->$3;
  _M0L6_2atmpS2420 = _M0L6_2atmpS2421 & _M0L14capacity__maskS2422;
  _M0L3pslS689 = _M0L6_2atmpS2419;
  _M0L3idxS690 = _M0L6_2atmpS2420;
  _M0L5entryS691 = _M0L5entryS697;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4198 =
      _M0L4selfS693->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2418 =
      _M0L8_2afieldS4198;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4197;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS692;
    if (
      _M0L3idxS690 < 0
      || _M0L3idxS690 >= Moonbit_array_length(_M0L7entriesS2418)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4197
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2418[
        _M0L3idxS690
      ];
    _M0L7_2abindS692 = _M0L6_2atmpS4197;
    if (_M0L7_2abindS692 == 0) {
      _M0L5entryS691->$2 = _M0L3pslS689;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS693, _M0L5entryS691, _M0L3idxS690);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS695 =
        _M0L7_2abindS692;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS696 =
        _M0L7_2aSomeS695;
      int32_t _M0L3pslS2408 = _M0L14_2acurr__entryS696->$2;
      if (_M0L3pslS689 > _M0L3pslS2408) {
        int32_t _M0L3pslS2413;
        int32_t _M0L6_2atmpS2409;
        int32_t _M0L6_2atmpS2411;
        int32_t _M0L14capacity__maskS2412;
        int32_t _M0L6_2atmpS2410;
        _M0L5entryS691->$2 = _M0L3pslS689;
        moonbit_incref(_M0L14_2acurr__entryS696);
        moonbit_incref(_M0L4selfS693);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS693, _M0L5entryS691, _M0L3idxS690);
        _M0L3pslS2413 = _M0L14_2acurr__entryS696->$2;
        _M0L6_2atmpS2409 = _M0L3pslS2413 + 1;
        _M0L6_2atmpS2411 = _M0L3idxS690 + 1;
        _M0L14capacity__maskS2412 = _M0L4selfS693->$3;
        _M0L6_2atmpS2410 = _M0L6_2atmpS2411 & _M0L14capacity__maskS2412;
        _M0L3pslS689 = _M0L6_2atmpS2409;
        _M0L3idxS690 = _M0L6_2atmpS2410;
        _M0L5entryS691 = _M0L14_2acurr__entryS696;
        continue;
      } else {
        int32_t _M0L6_2atmpS2414 = _M0L3pslS689 + 1;
        int32_t _M0L6_2atmpS2416 = _M0L3idxS690 + 1;
        int32_t _M0L14capacity__maskS2417 = _M0L4selfS693->$3;
        int32_t _M0L6_2atmpS2415 =
          _M0L6_2atmpS2416 & _M0L14capacity__maskS2417;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4686 =
          _M0L5entryS691;
        _M0L3pslS689 = _M0L6_2atmpS2414;
        _M0L3idxS690 = _M0L6_2atmpS2415;
        _M0L5entryS691 = _tmp_4686;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS703,
  int32_t _M0L3idxS708,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS707
) {
  int32_t _M0L3pslS2439;
  int32_t _M0L6_2atmpS2435;
  int32_t _M0L6_2atmpS2437;
  int32_t _M0L14capacity__maskS2438;
  int32_t _M0L6_2atmpS2436;
  int32_t _M0L3pslS699;
  int32_t _M0L3idxS700;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS701;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2439 = _M0L5entryS707->$2;
  _M0L6_2atmpS2435 = _M0L3pslS2439 + 1;
  _M0L6_2atmpS2437 = _M0L3idxS708 + 1;
  _M0L14capacity__maskS2438 = _M0L4selfS703->$3;
  _M0L6_2atmpS2436 = _M0L6_2atmpS2437 & _M0L14capacity__maskS2438;
  _M0L3pslS699 = _M0L6_2atmpS2435;
  _M0L3idxS700 = _M0L6_2atmpS2436;
  _M0L5entryS701 = _M0L5entryS707;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4200 = _M0L4selfS703->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2434 = _M0L8_2afieldS4200;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4199;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS702;
    if (
      _M0L3idxS700 < 0
      || _M0L3idxS700 >= Moonbit_array_length(_M0L7entriesS2434)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4199
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2434[_M0L3idxS700];
    _M0L7_2abindS702 = _M0L6_2atmpS4199;
    if (_M0L7_2abindS702 == 0) {
      _M0L5entryS701->$2 = _M0L3pslS699;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS703, _M0L5entryS701, _M0L3idxS700);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS705 = _M0L7_2abindS702;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS706 =
        _M0L7_2aSomeS705;
      int32_t _M0L3pslS2424 = _M0L14_2acurr__entryS706->$2;
      if (_M0L3pslS699 > _M0L3pslS2424) {
        int32_t _M0L3pslS2429;
        int32_t _M0L6_2atmpS2425;
        int32_t _M0L6_2atmpS2427;
        int32_t _M0L14capacity__maskS2428;
        int32_t _M0L6_2atmpS2426;
        _M0L5entryS701->$2 = _M0L3pslS699;
        moonbit_incref(_M0L14_2acurr__entryS706);
        moonbit_incref(_M0L4selfS703);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS703, _M0L5entryS701, _M0L3idxS700);
        _M0L3pslS2429 = _M0L14_2acurr__entryS706->$2;
        _M0L6_2atmpS2425 = _M0L3pslS2429 + 1;
        _M0L6_2atmpS2427 = _M0L3idxS700 + 1;
        _M0L14capacity__maskS2428 = _M0L4selfS703->$3;
        _M0L6_2atmpS2426 = _M0L6_2atmpS2427 & _M0L14capacity__maskS2428;
        _M0L3pslS699 = _M0L6_2atmpS2425;
        _M0L3idxS700 = _M0L6_2atmpS2426;
        _M0L5entryS701 = _M0L14_2acurr__entryS706;
        continue;
      } else {
        int32_t _M0L6_2atmpS2430 = _M0L3pslS699 + 1;
        int32_t _M0L6_2atmpS2432 = _M0L3idxS700 + 1;
        int32_t _M0L14capacity__maskS2433 = _M0L4selfS703->$3;
        int32_t _M0L6_2atmpS2431 =
          _M0L6_2atmpS2432 & _M0L14capacity__maskS2433;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_4688 = _M0L5entryS701;
        _M0L3pslS699 = _M0L6_2atmpS2430;
        _M0L3idxS700 = _M0L6_2atmpS2431;
        _M0L5entryS701 = _tmp_4688;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS661,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS663,
  int32_t _M0L8new__idxS662
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4203;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2386;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2387;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4202;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4201;
  int32_t _M0L6_2acntS4504;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS664;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4203 = _M0L4selfS661->$0;
  _M0L7entriesS2386 = _M0L8_2afieldS4203;
  moonbit_incref(_M0L5entryS663);
  _M0L6_2atmpS2387 = _M0L5entryS663;
  if (
    _M0L8new__idxS662 < 0
    || _M0L8new__idxS662 >= Moonbit_array_length(_M0L7entriesS2386)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4202
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2386[
      _M0L8new__idxS662
    ];
  if (_M0L6_2aoldS4202) {
    moonbit_decref(_M0L6_2aoldS4202);
  }
  _M0L7entriesS2386[_M0L8new__idxS662] = _M0L6_2atmpS2387;
  _M0L8_2afieldS4201 = _M0L5entryS663->$1;
  _M0L6_2acntS4504 = Moonbit_object_header(_M0L5entryS663)->rc;
  if (_M0L6_2acntS4504 > 1) {
    int32_t _M0L11_2anew__cntS4507 = _M0L6_2acntS4504 - 1;
    Moonbit_object_header(_M0L5entryS663)->rc = _M0L11_2anew__cntS4507;
    if (_M0L8_2afieldS4201) {
      moonbit_incref(_M0L8_2afieldS4201);
    }
  } else if (_M0L6_2acntS4504 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4506 =
      _M0L5entryS663->$5;
    moonbit_string_t _M0L8_2afieldS4505;
    moonbit_decref(_M0L8_2afieldS4506);
    _M0L8_2afieldS4505 = _M0L5entryS663->$4;
    moonbit_decref(_M0L8_2afieldS4505);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS663);
  }
  _M0L7_2abindS664 = _M0L8_2afieldS4201;
  if (_M0L7_2abindS664 == 0) {
    if (_M0L7_2abindS664) {
      moonbit_decref(_M0L7_2abindS664);
    }
    _M0L4selfS661->$6 = _M0L8new__idxS662;
    moonbit_decref(_M0L4selfS661);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS665;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS666;
    moonbit_decref(_M0L4selfS661);
    _M0L7_2aSomeS665 = _M0L7_2abindS664;
    _M0L7_2anextS666 = _M0L7_2aSomeS665;
    _M0L7_2anextS666->$0 = _M0L8new__idxS662;
    moonbit_decref(_M0L7_2anextS666);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS667,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS669,
  int32_t _M0L8new__idxS668
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4206;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2388;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2389;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4205;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4204;
  int32_t _M0L6_2acntS4508;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS670;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4206 = _M0L4selfS667->$0;
  _M0L7entriesS2388 = _M0L8_2afieldS4206;
  moonbit_incref(_M0L5entryS669);
  _M0L6_2atmpS2389 = _M0L5entryS669;
  if (
    _M0L8new__idxS668 < 0
    || _M0L8new__idxS668 >= Moonbit_array_length(_M0L7entriesS2388)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4205
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2388[
      _M0L8new__idxS668
    ];
  if (_M0L6_2aoldS4205) {
    moonbit_decref(_M0L6_2aoldS4205);
  }
  _M0L7entriesS2388[_M0L8new__idxS668] = _M0L6_2atmpS2389;
  _M0L8_2afieldS4204 = _M0L5entryS669->$1;
  _M0L6_2acntS4508 = Moonbit_object_header(_M0L5entryS669)->rc;
  if (_M0L6_2acntS4508 > 1) {
    int32_t _M0L11_2anew__cntS4510 = _M0L6_2acntS4508 - 1;
    Moonbit_object_header(_M0L5entryS669)->rc = _M0L11_2anew__cntS4510;
    if (_M0L8_2afieldS4204) {
      moonbit_incref(_M0L8_2afieldS4204);
    }
  } else if (_M0L6_2acntS4508 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4509 =
      _M0L5entryS669->$5;
    moonbit_decref(_M0L8_2afieldS4509);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS669);
  }
  _M0L7_2abindS670 = _M0L8_2afieldS4204;
  if (_M0L7_2abindS670 == 0) {
    if (_M0L7_2abindS670) {
      moonbit_decref(_M0L7_2abindS670);
    }
    _M0L4selfS667->$6 = _M0L8new__idxS668;
    moonbit_decref(_M0L4selfS667);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS671;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS672;
    moonbit_decref(_M0L4selfS667);
    _M0L7_2aSomeS671 = _M0L7_2abindS670;
    _M0L7_2anextS672 = _M0L7_2aSomeS671;
    _M0L7_2anextS672->$0 = _M0L8new__idxS668;
    moonbit_decref(_M0L7_2anextS672);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS673,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS675,
  int32_t _M0L8new__idxS674
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4209;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2390;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2391;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4208;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4207;
  int32_t _M0L6_2acntS4511;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS676;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4209 = _M0L4selfS673->$0;
  _M0L7entriesS2390 = _M0L8_2afieldS4209;
  moonbit_incref(_M0L5entryS675);
  _M0L6_2atmpS2391 = _M0L5entryS675;
  if (
    _M0L8new__idxS674 < 0
    || _M0L8new__idxS674 >= Moonbit_array_length(_M0L7entriesS2390)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4208
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2390[_M0L8new__idxS674];
  if (_M0L6_2aoldS4208) {
    moonbit_decref(_M0L6_2aoldS4208);
  }
  _M0L7entriesS2390[_M0L8new__idxS674] = _M0L6_2atmpS2391;
  _M0L8_2afieldS4207 = _M0L5entryS675->$1;
  _M0L6_2acntS4511 = Moonbit_object_header(_M0L5entryS675)->rc;
  if (_M0L6_2acntS4511 > 1) {
    int32_t _M0L11_2anew__cntS4514 = _M0L6_2acntS4511 - 1;
    Moonbit_object_header(_M0L5entryS675)->rc = _M0L11_2anew__cntS4514;
    if (_M0L8_2afieldS4207) {
      moonbit_incref(_M0L8_2afieldS4207);
    }
  } else if (_M0L6_2acntS4511 == 1) {
    void* _M0L8_2afieldS4513 = _M0L5entryS675->$5;
    moonbit_string_t _M0L8_2afieldS4512;
    moonbit_decref(_M0L8_2afieldS4513);
    _M0L8_2afieldS4512 = _M0L5entryS675->$4;
    moonbit_decref(_M0L8_2afieldS4512);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS675);
  }
  _M0L7_2abindS676 = _M0L8_2afieldS4207;
  if (_M0L7_2abindS676 == 0) {
    if (_M0L7_2abindS676) {
      moonbit_decref(_M0L7_2abindS676);
    }
    _M0L4selfS673->$6 = _M0L8new__idxS674;
    moonbit_decref(_M0L4selfS673);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS677;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS678;
    moonbit_decref(_M0L4selfS673);
    _M0L7_2aSomeS677 = _M0L7_2abindS676;
    _M0L7_2anextS678 = _M0L7_2aSomeS677;
    _M0L7_2anextS678->$0 = _M0L8new__idxS674;
    moonbit_decref(_M0L7_2anextS678);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS650,
  int32_t _M0L3idxS652,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS651
) {
  int32_t _M0L7_2abindS649;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4211;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2364;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2365;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4210;
  int32_t _M0L4sizeS2367;
  int32_t _M0L6_2atmpS2366;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS649 = _M0L4selfS650->$6;
  switch (_M0L7_2abindS649) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2359;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4212;
      moonbit_incref(_M0L5entryS651);
      _M0L6_2atmpS2359 = _M0L5entryS651;
      _M0L6_2aoldS4212 = _M0L4selfS650->$5;
      if (_M0L6_2aoldS4212) {
        moonbit_decref(_M0L6_2aoldS4212);
      }
      _M0L4selfS650->$5 = _M0L6_2atmpS2359;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4215 =
        _M0L4selfS650->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2363 =
        _M0L8_2afieldS4215;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4214;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2362;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2360;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2361;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4213;
      if (
        _M0L7_2abindS649 < 0
        || _M0L7_2abindS649 >= Moonbit_array_length(_M0L7entriesS2363)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4214
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2363[
          _M0L7_2abindS649
        ];
      _M0L6_2atmpS2362 = _M0L6_2atmpS4214;
      if (_M0L6_2atmpS2362) {
        moonbit_incref(_M0L6_2atmpS2362);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2360
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2362);
      moonbit_incref(_M0L5entryS651);
      _M0L6_2atmpS2361 = _M0L5entryS651;
      _M0L6_2aoldS4213 = _M0L6_2atmpS2360->$1;
      if (_M0L6_2aoldS4213) {
        moonbit_decref(_M0L6_2aoldS4213);
      }
      _M0L6_2atmpS2360->$1 = _M0L6_2atmpS2361;
      moonbit_decref(_M0L6_2atmpS2360);
      break;
    }
  }
  _M0L4selfS650->$6 = _M0L3idxS652;
  _M0L8_2afieldS4211 = _M0L4selfS650->$0;
  _M0L7entriesS2364 = _M0L8_2afieldS4211;
  _M0L6_2atmpS2365 = _M0L5entryS651;
  if (
    _M0L3idxS652 < 0
    || _M0L3idxS652 >= Moonbit_array_length(_M0L7entriesS2364)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4210
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2364[
      _M0L3idxS652
    ];
  if (_M0L6_2aoldS4210) {
    moonbit_decref(_M0L6_2aoldS4210);
  }
  _M0L7entriesS2364[_M0L3idxS652] = _M0L6_2atmpS2365;
  _M0L4sizeS2367 = _M0L4selfS650->$1;
  _M0L6_2atmpS2366 = _M0L4sizeS2367 + 1;
  _M0L4selfS650->$1 = _M0L6_2atmpS2366;
  moonbit_decref(_M0L4selfS650);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS654,
  int32_t _M0L3idxS656,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS655
) {
  int32_t _M0L7_2abindS653;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4217;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2373;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2374;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4216;
  int32_t _M0L4sizeS2376;
  int32_t _M0L6_2atmpS2375;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS653 = _M0L4selfS654->$6;
  switch (_M0L7_2abindS653) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2368;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4218;
      moonbit_incref(_M0L5entryS655);
      _M0L6_2atmpS2368 = _M0L5entryS655;
      _M0L6_2aoldS4218 = _M0L4selfS654->$5;
      if (_M0L6_2aoldS4218) {
        moonbit_decref(_M0L6_2aoldS4218);
      }
      _M0L4selfS654->$5 = _M0L6_2atmpS2368;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4221 =
        _M0L4selfS654->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2372 =
        _M0L8_2afieldS4221;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4220;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2371;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2369;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2370;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4219;
      if (
        _M0L7_2abindS653 < 0
        || _M0L7_2abindS653 >= Moonbit_array_length(_M0L7entriesS2372)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4220
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2372[
          _M0L7_2abindS653
        ];
      _M0L6_2atmpS2371 = _M0L6_2atmpS4220;
      if (_M0L6_2atmpS2371) {
        moonbit_incref(_M0L6_2atmpS2371);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2369
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2371);
      moonbit_incref(_M0L5entryS655);
      _M0L6_2atmpS2370 = _M0L5entryS655;
      _M0L6_2aoldS4219 = _M0L6_2atmpS2369->$1;
      if (_M0L6_2aoldS4219) {
        moonbit_decref(_M0L6_2aoldS4219);
      }
      _M0L6_2atmpS2369->$1 = _M0L6_2atmpS2370;
      moonbit_decref(_M0L6_2atmpS2369);
      break;
    }
  }
  _M0L4selfS654->$6 = _M0L3idxS656;
  _M0L8_2afieldS4217 = _M0L4selfS654->$0;
  _M0L7entriesS2373 = _M0L8_2afieldS4217;
  _M0L6_2atmpS2374 = _M0L5entryS655;
  if (
    _M0L3idxS656 < 0
    || _M0L3idxS656 >= Moonbit_array_length(_M0L7entriesS2373)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4216
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2373[
      _M0L3idxS656
    ];
  if (_M0L6_2aoldS4216) {
    moonbit_decref(_M0L6_2aoldS4216);
  }
  _M0L7entriesS2373[_M0L3idxS656] = _M0L6_2atmpS2374;
  _M0L4sizeS2376 = _M0L4selfS654->$1;
  _M0L6_2atmpS2375 = _M0L4sizeS2376 + 1;
  _M0L4selfS654->$1 = _M0L6_2atmpS2375;
  moonbit_decref(_M0L4selfS654);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS658,
  int32_t _M0L3idxS660,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS659
) {
  int32_t _M0L7_2abindS657;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4223;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2382;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2383;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4222;
  int32_t _M0L4sizeS2385;
  int32_t _M0L6_2atmpS2384;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS657 = _M0L4selfS658->$6;
  switch (_M0L7_2abindS657) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2377;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4224;
      moonbit_incref(_M0L5entryS659);
      _M0L6_2atmpS2377 = _M0L5entryS659;
      _M0L6_2aoldS4224 = _M0L4selfS658->$5;
      if (_M0L6_2aoldS4224) {
        moonbit_decref(_M0L6_2aoldS4224);
      }
      _M0L4selfS658->$5 = _M0L6_2atmpS2377;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4227 = _M0L4selfS658->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2381 = _M0L8_2afieldS4227;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4226;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2380;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2378;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2379;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4225;
      if (
        _M0L7_2abindS657 < 0
        || _M0L7_2abindS657 >= Moonbit_array_length(_M0L7entriesS2381)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4226
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2381[_M0L7_2abindS657];
      _M0L6_2atmpS2380 = _M0L6_2atmpS4226;
      if (_M0L6_2atmpS2380) {
        moonbit_incref(_M0L6_2atmpS2380);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2378
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2380);
      moonbit_incref(_M0L5entryS659);
      _M0L6_2atmpS2379 = _M0L5entryS659;
      _M0L6_2aoldS4225 = _M0L6_2atmpS2378->$1;
      if (_M0L6_2aoldS4225) {
        moonbit_decref(_M0L6_2aoldS4225);
      }
      _M0L6_2atmpS2378->$1 = _M0L6_2atmpS2379;
      moonbit_decref(_M0L6_2atmpS2378);
      break;
    }
  }
  _M0L4selfS658->$6 = _M0L3idxS660;
  _M0L8_2afieldS4223 = _M0L4selfS658->$0;
  _M0L7entriesS2382 = _M0L8_2afieldS4223;
  _M0L6_2atmpS2383 = _M0L5entryS659;
  if (
    _M0L3idxS660 < 0
    || _M0L3idxS660 >= Moonbit_array_length(_M0L7entriesS2382)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4222
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2382[_M0L3idxS660];
  if (_M0L6_2aoldS4222) {
    moonbit_decref(_M0L6_2aoldS4222);
  }
  _M0L7entriesS2382[_M0L3idxS660] = _M0L6_2atmpS2383;
  _M0L4sizeS2385 = _M0L4selfS658->$1;
  _M0L6_2atmpS2384 = _M0L4sizeS2385 + 1;
  _M0L4selfS658->$1 = _M0L6_2atmpS2384;
  moonbit_decref(_M0L4selfS658);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS632
) {
  int32_t _M0L8capacityS631;
  int32_t _M0L7_2abindS633;
  int32_t _M0L7_2abindS634;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2356;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS635;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS636;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4689;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS631
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS632);
  _M0L7_2abindS633 = _M0L8capacityS631 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS634 = _M0FPB21calc__grow__threshold(_M0L8capacityS631);
  _M0L6_2atmpS2356 = 0;
  _M0L7_2abindS635
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS631, _M0L6_2atmpS2356);
  _M0L7_2abindS636 = 0;
  _block_4689
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4689)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4689->$0 = _M0L7_2abindS635;
  _block_4689->$1 = 0;
  _block_4689->$2 = _M0L8capacityS631;
  _block_4689->$3 = _M0L7_2abindS633;
  _block_4689->$4 = _M0L7_2abindS634;
  _block_4689->$5 = _M0L7_2abindS636;
  _block_4689->$6 = -1;
  return _block_4689;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS638
) {
  int32_t _M0L8capacityS637;
  int32_t _M0L7_2abindS639;
  int32_t _M0L7_2abindS640;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2357;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS641;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS642;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4690;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS637
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS638);
  _M0L7_2abindS639 = _M0L8capacityS637 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS640 = _M0FPB21calc__grow__threshold(_M0L8capacityS637);
  _M0L6_2atmpS2357 = 0;
  _M0L7_2abindS641
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS637, _M0L6_2atmpS2357);
  _M0L7_2abindS642 = 0;
  _block_4690
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4690)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4690->$0 = _M0L7_2abindS641;
  _block_4690->$1 = 0;
  _block_4690->$2 = _M0L8capacityS637;
  _block_4690->$3 = _M0L7_2abindS639;
  _block_4690->$4 = _M0L7_2abindS640;
  _block_4690->$5 = _M0L7_2abindS642;
  _block_4690->$6 = -1;
  return _block_4690;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS644
) {
  int32_t _M0L8capacityS643;
  int32_t _M0L7_2abindS645;
  int32_t _M0L7_2abindS646;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2358;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS647;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS648;
  struct _M0TPB3MapGsRPB4JsonE* _block_4691;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS643
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS644);
  _M0L7_2abindS645 = _M0L8capacityS643 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS646 = _M0FPB21calc__grow__threshold(_M0L8capacityS643);
  _M0L6_2atmpS2358 = 0;
  _M0L7_2abindS647
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS643, _M0L6_2atmpS2358);
  _M0L7_2abindS648 = 0;
  _block_4691
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_4691)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_4691->$0 = _M0L7_2abindS647;
  _block_4691->$1 = 0;
  _block_4691->$2 = _M0L8capacityS643;
  _block_4691->$3 = _M0L7_2abindS645;
  _block_4691->$4 = _M0L7_2abindS646;
  _block_4691->$5 = _M0L7_2abindS648;
  _block_4691->$6 = -1;
  return _block_4691;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS630) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS630 >= 0) {
    int32_t _M0L6_2atmpS2355;
    int32_t _M0L6_2atmpS2354;
    int32_t _M0L6_2atmpS2353;
    int32_t _M0L6_2atmpS2352;
    if (_M0L4selfS630 <= 1) {
      return 1;
    }
    if (_M0L4selfS630 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2355 = _M0L4selfS630 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2354 = moonbit_clz32(_M0L6_2atmpS2355);
    _M0L6_2atmpS2353 = _M0L6_2atmpS2354 - 1;
    _M0L6_2atmpS2352 = 2147483647 >> (_M0L6_2atmpS2353 & 31);
    return _M0L6_2atmpS2352 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS629) {
  int32_t _M0L6_2atmpS2351;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2351 = _M0L8capacityS629 * 13;
  return _M0L6_2atmpS2351 / 16;
}

moonbit_string_t _M0MPC16option6Option10unwrap__orGsE(
  moonbit_string_t _M0L4selfS626,
  moonbit_string_t _M0L7defaultS627
) {
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS626 == 0) {
    if (_M0L4selfS626) {
      moonbit_decref(_M0L4selfS626);
    }
    return _M0L7defaultS627;
  } else {
    moonbit_string_t _M0L7_2aSomeS628;
    moonbit_decref(_M0L7defaultS627);
    _M0L7_2aSomeS628 = _M0L4selfS626;
    return _M0L7_2aSomeS628;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS620
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS620 == 0) {
    if (_M0L4selfS620) {
      moonbit_decref(_M0L4selfS620);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS621 =
      _M0L4selfS620;
    return _M0L7_2aSomeS621;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS622
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS622 == 0) {
    if (_M0L4selfS622) {
      moonbit_decref(_M0L4selfS622);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS623 =
      _M0L4selfS622;
    return _M0L7_2aSomeS623;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS624
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS624 == 0) {
    if (_M0L4selfS624) {
      moonbit_decref(_M0L4selfS624);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS625 = _M0L4selfS624;
    return _M0L7_2aSomeS625;
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS619
) {
  void** _M0L6_2atmpS2350;
  struct _M0TPB5ArrayGRPB4JsonE* _block_4692;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2350
  = (void**)moonbit_make_ref_array(_M0L3lenS619, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_4692
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_4692)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_4692->$0 = _M0L6_2atmpS2350;
  _block_4692->$1 = _M0L3lenS619;
  return _block_4692;
}

void* _M0MPC15array9ArrayView2atGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L4selfS618,
  int32_t _M0L5indexS617
) {
  int32_t _if__result_4693;
  #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  if (_M0L5indexS617 >= 0) {
    int32_t _M0L3endS2337 = _M0L4selfS618.$2;
    int32_t _M0L5startS2338 = _M0L4selfS618.$1;
    int32_t _M0L6_2atmpS2336 = _M0L3endS2337 - _M0L5startS2338;
    _if__result_4693 = _M0L5indexS617 < _M0L6_2atmpS2336;
  } else {
    _if__result_4693 = 0;
  }
  if (_if__result_4693) {
    void** _M0L8_2afieldS4230 = _M0L4selfS618.$0;
    void** _M0L3bufS2339 = _M0L8_2afieldS4230;
    int32_t _M0L8_2afieldS4229 = _M0L4selfS618.$1;
    int32_t _M0L5startS2341 = _M0L8_2afieldS4229;
    int32_t _M0L6_2atmpS2340 = _M0L5startS2341 + _M0L5indexS617;
    void* _M0L6_2atmpS4228;
    if (
      _M0L6_2atmpS2340 < 0
      || _M0L6_2atmpS2340 >= Moonbit_array_length(_M0L3bufS2339)
    ) {
      #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4228 = (void*)_M0L3bufS2339[_M0L6_2atmpS2340];
    moonbit_incref(_M0L6_2atmpS4228);
    moonbit_decref(_M0L3bufS2339);
    return _M0L6_2atmpS4228;
  } else {
    int32_t _M0L3endS2348 = _M0L4selfS618.$2;
    int32_t _M0L8_2afieldS4234 = _M0L4selfS618.$1;
    int32_t _M0L5startS2349;
    int32_t _M0L6_2atmpS2347;
    moonbit_string_t _M0L6_2atmpS2346;
    moonbit_string_t _M0L6_2atmpS4233;
    moonbit_string_t _M0L6_2atmpS2345;
    moonbit_string_t _M0L6_2atmpS4232;
    moonbit_string_t _M0L6_2atmpS2343;
    moonbit_string_t _M0L6_2atmpS2344;
    moonbit_string_t _M0L6_2atmpS4231;
    moonbit_string_t _M0L6_2atmpS2342;
    moonbit_decref(_M0L4selfS618.$0);
    _M0L5startS2349 = _M0L8_2afieldS4234;
    _M0L6_2atmpS2347 = _M0L3endS2348 - _M0L5startS2349;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2346
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2347);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4233
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_96.data, _M0L6_2atmpS2346);
    moonbit_decref(_M0L6_2atmpS2346);
    _M0L6_2atmpS2345 = _M0L6_2atmpS4233;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4232
    = moonbit_add_string(_M0L6_2atmpS2345, (moonbit_string_t)moonbit_string_literal_97.data);
    moonbit_decref(_M0L6_2atmpS2345);
    _M0L6_2atmpS2343 = _M0L6_2atmpS4232;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2344
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS617);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4231 = moonbit_add_string(_M0L6_2atmpS2343, _M0L6_2atmpS2344);
    moonbit_decref(_M0L6_2atmpS2343);
    moonbit_decref(_M0L6_2atmpS2344);
    _M0L6_2atmpS2342 = _M0L6_2atmpS4231;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6_2atmpS2342, (moonbit_string_t)moonbit_string_literal_98.data);
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS616
) {
  moonbit_string_t* _M0L6_2atmpS2335;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2335 = _M0L4selfS616;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2335);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS612,
  int32_t _M0L5indexS613
) {
  uint64_t* _M0L6_2atmpS2333;
  uint64_t _M0L6_2atmpS4235;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2333 = _M0L4selfS612;
  if (
    _M0L5indexS613 < 0
    || _M0L5indexS613 >= Moonbit_array_length(_M0L6_2atmpS2333)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4235 = (uint64_t)_M0L6_2atmpS2333[_M0L5indexS613];
  moonbit_decref(_M0L6_2atmpS2333);
  return _M0L6_2atmpS4235;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS614,
  int32_t _M0L5indexS615
) {
  uint32_t* _M0L6_2atmpS2334;
  uint32_t _M0L6_2atmpS4236;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2334 = _M0L4selfS614;
  if (
    _M0L5indexS615 < 0
    || _M0L5indexS615 >= Moonbit_array_length(_M0L6_2atmpS2334)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4236 = (uint32_t)_M0L6_2atmpS2334[_M0L5indexS615];
  moonbit_decref(_M0L6_2atmpS2334);
  return _M0L6_2atmpS4236;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS611
) {
  moonbit_string_t* _M0L6_2atmpS2331;
  int32_t _M0L6_2atmpS4237;
  int32_t _M0L6_2atmpS2332;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2330;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS611);
  _M0L6_2atmpS2331 = _M0L4selfS611;
  _M0L6_2atmpS4237 = Moonbit_array_length(_M0L4selfS611);
  moonbit_decref(_M0L4selfS611);
  _M0L6_2atmpS2332 = _M0L6_2atmpS4237;
  _M0L6_2atmpS2330
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2332, _M0L6_2atmpS2331
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2330);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS609
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS608;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__* _closure_4694;
  struct _M0TWEOs* _M0L6_2atmpS2318;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS608
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS608)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS608->$0 = 0;
  _closure_4694
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__));
  Moonbit_object_header(_closure_4694)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__, $0_0) >> 2, 2, 0);
  _closure_4694->code = &_M0MPC15array9ArrayView4iterGsEC2319l570;
  _closure_4694->$0_0 = _M0L4selfS609.$0;
  _closure_4694->$0_1 = _M0L4selfS609.$1;
  _closure_4694->$0_2 = _M0L4selfS609.$2;
  _closure_4694->$1 = _M0L1iS608;
  _M0L6_2atmpS2318 = (struct _M0TWEOs*)_closure_4694;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2318);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2319l570(
  struct _M0TWEOs* _M0L6_2aenvS2320
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__* _M0L14_2acasted__envS2321;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4242;
  struct _M0TPC13ref3RefGiE* _M0L1iS608;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4241;
  int32_t _M0L6_2acntS4515;
  struct _M0TPB9ArrayViewGsE _M0L4selfS609;
  int32_t _M0L3valS2322;
  int32_t _M0L6_2atmpS2323;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2321
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2319__l570__*)_M0L6_2aenvS2320;
  _M0L8_2afieldS4242 = _M0L14_2acasted__envS2321->$1;
  _M0L1iS608 = _M0L8_2afieldS4242;
  _M0L8_2afieldS4241
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2321->$0_1,
      _M0L14_2acasted__envS2321->$0_2,
      _M0L14_2acasted__envS2321->$0_0
  };
  _M0L6_2acntS4515 = Moonbit_object_header(_M0L14_2acasted__envS2321)->rc;
  if (_M0L6_2acntS4515 > 1) {
    int32_t _M0L11_2anew__cntS4516 = _M0L6_2acntS4515 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2321)->rc
    = _M0L11_2anew__cntS4516;
    moonbit_incref(_M0L1iS608);
    moonbit_incref(_M0L8_2afieldS4241.$0);
  } else if (_M0L6_2acntS4515 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2321);
  }
  _M0L4selfS609 = _M0L8_2afieldS4241;
  _M0L3valS2322 = _M0L1iS608->$0;
  moonbit_incref(_M0L4selfS609.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2323 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS609);
  if (_M0L3valS2322 < _M0L6_2atmpS2323) {
    moonbit_string_t* _M0L8_2afieldS4240 = _M0L4selfS609.$0;
    moonbit_string_t* _M0L3bufS2326 = _M0L8_2afieldS4240;
    int32_t _M0L8_2afieldS4239 = _M0L4selfS609.$1;
    int32_t _M0L5startS2328 = _M0L8_2afieldS4239;
    int32_t _M0L3valS2329 = _M0L1iS608->$0;
    int32_t _M0L6_2atmpS2327 = _M0L5startS2328 + _M0L3valS2329;
    moonbit_string_t _M0L6_2atmpS4238 =
      (moonbit_string_t)_M0L3bufS2326[_M0L6_2atmpS2327];
    moonbit_string_t _M0L4elemS610;
    int32_t _M0L3valS2325;
    int32_t _M0L6_2atmpS2324;
    moonbit_incref(_M0L6_2atmpS4238);
    moonbit_decref(_M0L3bufS2326);
    _M0L4elemS610 = _M0L6_2atmpS4238;
    _M0L3valS2325 = _M0L1iS608->$0;
    _M0L6_2atmpS2324 = _M0L3valS2325 + 1;
    _M0L1iS608->$0 = _M0L6_2atmpS2324;
    moonbit_decref(_M0L1iS608);
    return _M0L4elemS610;
  } else {
    moonbit_decref(_M0L4selfS609.$0);
    moonbit_decref(_M0L1iS608);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS607
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS607;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS606,
  struct _M0TPB6Logger _M0L6loggerS605
) {
  moonbit_string_t _M0L6_2atmpS2317;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2317
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS606, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS605.$0->$method_0(_M0L6loggerS605.$1, _M0L6_2atmpS2317);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS604,
  struct _M0TPB6Logger _M0L6loggerS603
) {
  moonbit_string_t _M0L6_2atmpS2316;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2316 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS604, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS603.$0->$method_0(_M0L6loggerS603.$1, _M0L6_2atmpS2316);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS598) {
  int32_t _M0L3lenS597;
  struct _M0TPC13ref3RefGiE* _M0L5indexS599;
  struct _M0R38String_3a_3aiter_2eanon__u2300__l247__* _closure_4695;
  struct _M0TWEOc* _M0L6_2atmpS2299;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS597 = Moonbit_array_length(_M0L4selfS598);
  _M0L5indexS599
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS599)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS599->$0 = 0;
  _closure_4695
  = (struct _M0R38String_3a_3aiter_2eanon__u2300__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2300__l247__));
  Moonbit_object_header(_closure_4695)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2300__l247__, $0) >> 2, 2, 0);
  _closure_4695->code = &_M0MPC16string6String4iterC2300l247;
  _closure_4695->$0 = _M0L5indexS599;
  _closure_4695->$1 = _M0L4selfS598;
  _closure_4695->$2 = _M0L3lenS597;
  _M0L6_2atmpS2299 = (struct _M0TWEOc*)_closure_4695;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2299);
}

int32_t _M0MPC16string6String4iterC2300l247(
  struct _M0TWEOc* _M0L6_2aenvS2301
) {
  struct _M0R38String_3a_3aiter_2eanon__u2300__l247__* _M0L14_2acasted__envS2302;
  int32_t _M0L3lenS597;
  moonbit_string_t _M0L8_2afieldS4245;
  moonbit_string_t _M0L4selfS598;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4244;
  int32_t _M0L6_2acntS4517;
  struct _M0TPC13ref3RefGiE* _M0L5indexS599;
  int32_t _M0L3valS2303;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2302
  = (struct _M0R38String_3a_3aiter_2eanon__u2300__l247__*)_M0L6_2aenvS2301;
  _M0L3lenS597 = _M0L14_2acasted__envS2302->$2;
  _M0L8_2afieldS4245 = _M0L14_2acasted__envS2302->$1;
  _M0L4selfS598 = _M0L8_2afieldS4245;
  _M0L8_2afieldS4244 = _M0L14_2acasted__envS2302->$0;
  _M0L6_2acntS4517 = Moonbit_object_header(_M0L14_2acasted__envS2302)->rc;
  if (_M0L6_2acntS4517 > 1) {
    int32_t _M0L11_2anew__cntS4518 = _M0L6_2acntS4517 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2302)->rc
    = _M0L11_2anew__cntS4518;
    moonbit_incref(_M0L4selfS598);
    moonbit_incref(_M0L8_2afieldS4244);
  } else if (_M0L6_2acntS4517 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2302);
  }
  _M0L5indexS599 = _M0L8_2afieldS4244;
  _M0L3valS2303 = _M0L5indexS599->$0;
  if (_M0L3valS2303 < _M0L3lenS597) {
    int32_t _M0L3valS2315 = _M0L5indexS599->$0;
    int32_t _M0L2c1S600 = _M0L4selfS598[_M0L3valS2315];
    int32_t _if__result_4696;
    int32_t _M0L3valS2313;
    int32_t _M0L6_2atmpS2312;
    int32_t _M0L6_2atmpS2314;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S600)) {
      int32_t _M0L3valS2305 = _M0L5indexS599->$0;
      int32_t _M0L6_2atmpS2304 = _M0L3valS2305 + 1;
      _if__result_4696 = _M0L6_2atmpS2304 < _M0L3lenS597;
    } else {
      _if__result_4696 = 0;
    }
    if (_if__result_4696) {
      int32_t _M0L3valS2311 = _M0L5indexS599->$0;
      int32_t _M0L6_2atmpS2310 = _M0L3valS2311 + 1;
      int32_t _M0L6_2atmpS4243 = _M0L4selfS598[_M0L6_2atmpS2310];
      int32_t _M0L2c2S601;
      moonbit_decref(_M0L4selfS598);
      _M0L2c2S601 = _M0L6_2atmpS4243;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S601)) {
        int32_t _M0L6_2atmpS2308 = (int32_t)_M0L2c1S600;
        int32_t _M0L6_2atmpS2309 = (int32_t)_M0L2c2S601;
        int32_t _M0L1cS602;
        int32_t _M0L3valS2307;
        int32_t _M0L6_2atmpS2306;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS602
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2308, _M0L6_2atmpS2309);
        _M0L3valS2307 = _M0L5indexS599->$0;
        _M0L6_2atmpS2306 = _M0L3valS2307 + 2;
        _M0L5indexS599->$0 = _M0L6_2atmpS2306;
        moonbit_decref(_M0L5indexS599);
        return _M0L1cS602;
      }
    } else {
      moonbit_decref(_M0L4selfS598);
    }
    _M0L3valS2313 = _M0L5indexS599->$0;
    _M0L6_2atmpS2312 = _M0L3valS2313 + 1;
    _M0L5indexS599->$0 = _M0L6_2atmpS2312;
    moonbit_decref(_M0L5indexS599);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2314 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S600);
    return _M0L6_2atmpS2314;
  } else {
    moonbit_decref(_M0L5indexS599);
    moonbit_decref(_M0L4selfS598);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS579,
  moonbit_string_t _M0L5valueS581
) {
  int32_t _M0L3lenS2269;
  moonbit_string_t* _M0L6_2atmpS2271;
  int32_t _M0L6_2atmpS4248;
  int32_t _M0L6_2atmpS2270;
  int32_t _M0L6lengthS580;
  moonbit_string_t* _M0L8_2afieldS4247;
  moonbit_string_t* _M0L3bufS2272;
  moonbit_string_t _M0L6_2aoldS4246;
  int32_t _M0L6_2atmpS2273;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2269 = _M0L4selfS579->$1;
  moonbit_incref(_M0L4selfS579);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2271 = _M0MPC15array5Array6bufferGsE(_M0L4selfS579);
  _M0L6_2atmpS4248 = Moonbit_array_length(_M0L6_2atmpS2271);
  moonbit_decref(_M0L6_2atmpS2271);
  _M0L6_2atmpS2270 = _M0L6_2atmpS4248;
  if (_M0L3lenS2269 == _M0L6_2atmpS2270) {
    moonbit_incref(_M0L4selfS579);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS579);
  }
  _M0L6lengthS580 = _M0L4selfS579->$1;
  _M0L8_2afieldS4247 = _M0L4selfS579->$0;
  _M0L3bufS2272 = _M0L8_2afieldS4247;
  _M0L6_2aoldS4246 = (moonbit_string_t)_M0L3bufS2272[_M0L6lengthS580];
  moonbit_decref(_M0L6_2aoldS4246);
  _M0L3bufS2272[_M0L6lengthS580] = _M0L5valueS581;
  _M0L6_2atmpS2273 = _M0L6lengthS580 + 1;
  _M0L4selfS579->$1 = _M0L6_2atmpS2273;
  moonbit_decref(_M0L4selfS579);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS582,
  struct _M0TUsiE* _M0L5valueS584
) {
  int32_t _M0L3lenS2274;
  struct _M0TUsiE** _M0L6_2atmpS2276;
  int32_t _M0L6_2atmpS4251;
  int32_t _M0L6_2atmpS2275;
  int32_t _M0L6lengthS583;
  struct _M0TUsiE** _M0L8_2afieldS4250;
  struct _M0TUsiE** _M0L3bufS2277;
  struct _M0TUsiE* _M0L6_2aoldS4249;
  int32_t _M0L6_2atmpS2278;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2274 = _M0L4selfS582->$1;
  moonbit_incref(_M0L4selfS582);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2276 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS582);
  _M0L6_2atmpS4251 = Moonbit_array_length(_M0L6_2atmpS2276);
  moonbit_decref(_M0L6_2atmpS2276);
  _M0L6_2atmpS2275 = _M0L6_2atmpS4251;
  if (_M0L3lenS2274 == _M0L6_2atmpS2275) {
    moonbit_incref(_M0L4selfS582);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS582);
  }
  _M0L6lengthS583 = _M0L4selfS582->$1;
  _M0L8_2afieldS4250 = _M0L4selfS582->$0;
  _M0L3bufS2277 = _M0L8_2afieldS4250;
  _M0L6_2aoldS4249 = (struct _M0TUsiE*)_M0L3bufS2277[_M0L6lengthS583];
  if (_M0L6_2aoldS4249) {
    moonbit_decref(_M0L6_2aoldS4249);
  }
  _M0L3bufS2277[_M0L6lengthS583] = _M0L5valueS584;
  _M0L6_2atmpS2278 = _M0L6lengthS583 + 1;
  _M0L4selfS582->$1 = _M0L6_2atmpS2278;
  moonbit_decref(_M0L4selfS582);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L4selfS585,
  void* _M0L5valueS587
) {
  int32_t _M0L3lenS2279;
  void** _M0L6_2atmpS2281;
  int32_t _M0L6_2atmpS4254;
  int32_t _M0L6_2atmpS2280;
  int32_t _M0L6lengthS586;
  void** _M0L8_2afieldS4253;
  void** _M0L3bufS2282;
  void* _M0L6_2aoldS4252;
  int32_t _M0L6_2atmpS2283;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2279 = _M0L4selfS585->$1;
  moonbit_incref(_M0L4selfS585);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2281
  = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L4selfS585);
  _M0L6_2atmpS4254 = Moonbit_array_length(_M0L6_2atmpS2281);
  moonbit_decref(_M0L6_2atmpS2281);
  _M0L6_2atmpS2280 = _M0L6_2atmpS4254;
  if (_M0L3lenS2279 == _M0L6_2atmpS2280) {
    moonbit_incref(_M0L4selfS585);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L4selfS585);
  }
  _M0L6lengthS586 = _M0L4selfS585->$1;
  _M0L8_2afieldS4253 = _M0L4selfS585->$0;
  _M0L3bufS2282 = _M0L8_2afieldS4253;
  _M0L6_2aoldS4252 = (void*)_M0L3bufS2282[_M0L6lengthS586];
  moonbit_decref(_M0L6_2aoldS4252);
  _M0L3bufS2282[_M0L6lengthS586] = _M0L5valueS587;
  _M0L6_2atmpS2283 = _M0L6lengthS586 + 1;
  _M0L4selfS585->$1 = _M0L6_2atmpS2283;
  moonbit_decref(_M0L4selfS585);
  return 0;
}

int32_t _M0MPC15array5Array4pushGiE(
  struct _M0TPB5ArrayGiE* _M0L4selfS588,
  int32_t _M0L5valueS590
) {
  int32_t _M0L3lenS2284;
  int32_t* _M0L6_2atmpS2286;
  int32_t _M0L6_2atmpS4256;
  int32_t _M0L6_2atmpS2285;
  int32_t _M0L6lengthS589;
  int32_t* _M0L8_2afieldS4255;
  int32_t* _M0L3bufS2287;
  int32_t _M0L6_2atmpS2288;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2284 = _M0L4selfS588->$1;
  moonbit_incref(_M0L4selfS588);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2286 = _M0MPC15array5Array6bufferGiE(_M0L4selfS588);
  _M0L6_2atmpS4256 = Moonbit_array_length(_M0L6_2atmpS2286);
  moonbit_decref(_M0L6_2atmpS2286);
  _M0L6_2atmpS2285 = _M0L6_2atmpS4256;
  if (_M0L3lenS2284 == _M0L6_2atmpS2285) {
    moonbit_incref(_M0L4selfS588);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGiE(_M0L4selfS588);
  }
  _M0L6lengthS589 = _M0L4selfS588->$1;
  _M0L8_2afieldS4255 = _M0L4selfS588->$0;
  _M0L3bufS2287 = _M0L8_2afieldS4255;
  _M0L3bufS2287[_M0L6lengthS589] = _M0L5valueS590;
  _M0L6_2atmpS2288 = _M0L6lengthS589 + 1;
  _M0L4selfS588->$1 = _M0L6_2atmpS2288;
  moonbit_decref(_M0L4selfS588);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS591,
  void* _M0L5valueS593
) {
  int32_t _M0L3lenS2289;
  void** _M0L6_2atmpS2291;
  int32_t _M0L6_2atmpS4259;
  int32_t _M0L6_2atmpS2290;
  int32_t _M0L6lengthS592;
  void** _M0L8_2afieldS4258;
  void** _M0L3bufS2292;
  void* _M0L6_2aoldS4257;
  int32_t _M0L6_2atmpS2293;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2289 = _M0L4selfS591->$1;
  moonbit_incref(_M0L4selfS591);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2291
  = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS591);
  _M0L6_2atmpS4259 = Moonbit_array_length(_M0L6_2atmpS2291);
  moonbit_decref(_M0L6_2atmpS2291);
  _M0L6_2atmpS2290 = _M0L6_2atmpS4259;
  if (_M0L3lenS2289 == _M0L6_2atmpS2290) {
    moonbit_incref(_M0L4selfS591);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS591);
  }
  _M0L6lengthS592 = _M0L4selfS591->$1;
  _M0L8_2afieldS4258 = _M0L4selfS591->$0;
  _M0L3bufS2292 = _M0L8_2afieldS4258;
  _M0L6_2aoldS4257 = (void*)_M0L3bufS2292[_M0L6lengthS592];
  moonbit_decref(_M0L6_2aoldS4257);
  _M0L3bufS2292[_M0L6lengthS592] = _M0L5valueS593;
  _M0L6_2atmpS2293 = _M0L6lengthS592 + 1;
  _M0L4selfS591->$1 = _M0L6_2atmpS2293;
  moonbit_decref(_M0L4selfS591);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS594,
  void* _M0L5valueS596
) {
  int32_t _M0L3lenS2294;
  void** _M0L6_2atmpS2296;
  int32_t _M0L6_2atmpS4262;
  int32_t _M0L6_2atmpS2295;
  int32_t _M0L6lengthS595;
  void** _M0L8_2afieldS4261;
  void** _M0L3bufS2297;
  void* _M0L6_2aoldS4260;
  int32_t _M0L6_2atmpS2298;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2294 = _M0L4selfS594->$1;
  moonbit_incref(_M0L4selfS594);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2296
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS594);
  _M0L6_2atmpS4262 = Moonbit_array_length(_M0L6_2atmpS2296);
  moonbit_decref(_M0L6_2atmpS2296);
  _M0L6_2atmpS2295 = _M0L6_2atmpS4262;
  if (_M0L3lenS2294 == _M0L6_2atmpS2295) {
    moonbit_incref(_M0L4selfS594);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS594);
  }
  _M0L6lengthS595 = _M0L4selfS594->$1;
  _M0L8_2afieldS4261 = _M0L4selfS594->$0;
  _M0L3bufS2297 = _M0L8_2afieldS4261;
  _M0L6_2aoldS4260 = (void*)_M0L3bufS2297[_M0L6lengthS595];
  moonbit_decref(_M0L6_2aoldS4260);
  _M0L3bufS2297[_M0L6lengthS595] = _M0L5valueS596;
  _M0L6_2atmpS2298 = _M0L6lengthS595 + 1;
  _M0L4selfS594->$1 = _M0L6_2atmpS2298;
  moonbit_decref(_M0L4selfS594);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS562) {
  int32_t _M0L8old__capS561;
  int32_t _M0L8new__capS563;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS561 = _M0L4selfS562->$1;
  if (_M0L8old__capS561 == 0) {
    _M0L8new__capS563 = 8;
  } else {
    _M0L8new__capS563 = _M0L8old__capS561 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS562, _M0L8new__capS563);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS565
) {
  int32_t _M0L8old__capS564;
  int32_t _M0L8new__capS566;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS564 = _M0L4selfS565->$1;
  if (_M0L8old__capS564 == 0) {
    _M0L8new__capS566 = 8;
  } else {
    _M0L8new__capS566 = _M0L8old__capS564 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS565, _M0L8new__capS566);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L4selfS568
) {
  int32_t _M0L8old__capS567;
  int32_t _M0L8new__capS569;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS567 = _M0L4selfS568->$1;
  if (_M0L8old__capS567 == 0) {
    _M0L8new__capS569 = 8;
  } else {
    _M0L8new__capS569 = _M0L8old__capS567 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L4selfS568, _M0L8new__capS569);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGiE(struct _M0TPB5ArrayGiE* _M0L4selfS571) {
  int32_t _M0L8old__capS570;
  int32_t _M0L8new__capS572;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS570 = _M0L4selfS571->$1;
  if (_M0L8old__capS570 == 0) {
    _M0L8new__capS572 = 8;
  } else {
    _M0L8new__capS572 = _M0L8old__capS570 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGiE(_M0L4selfS571, _M0L8new__capS572);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS574
) {
  int32_t _M0L8old__capS573;
  int32_t _M0L8new__capS575;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS573 = _M0L4selfS574->$1;
  if (_M0L8old__capS573 == 0) {
    _M0L8new__capS575 = 8;
  } else {
    _M0L8new__capS575 = _M0L8old__capS573 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS574, _M0L8new__capS575);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS577
) {
  int32_t _M0L8old__capS576;
  int32_t _M0L8new__capS578;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS576 = _M0L4selfS577->$1;
  if (_M0L8old__capS576 == 0) {
    _M0L8new__capS578 = 8;
  } else {
    _M0L8new__capS578 = _M0L8old__capS576 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS577, _M0L8new__capS578);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS528,
  int32_t _M0L13new__capacityS526
) {
  moonbit_string_t* _M0L8new__bufS525;
  moonbit_string_t* _M0L8_2afieldS4264;
  moonbit_string_t* _M0L8old__bufS527;
  int32_t _M0L8old__capS529;
  int32_t _M0L9copy__lenS530;
  moonbit_string_t* _M0L6_2aoldS4263;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS525
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS526, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4264 = _M0L4selfS528->$0;
  _M0L8old__bufS527 = _M0L8_2afieldS4264;
  _M0L8old__capS529 = Moonbit_array_length(_M0L8old__bufS527);
  if (_M0L8old__capS529 < _M0L13new__capacityS526) {
    _M0L9copy__lenS530 = _M0L8old__capS529;
  } else {
    _M0L9copy__lenS530 = _M0L13new__capacityS526;
  }
  moonbit_incref(_M0L8old__bufS527);
  moonbit_incref(_M0L8new__bufS525);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS525, 0, _M0L8old__bufS527, 0, _M0L9copy__lenS530);
  _M0L6_2aoldS4263 = _M0L4selfS528->$0;
  moonbit_decref(_M0L6_2aoldS4263);
  _M0L4selfS528->$0 = _M0L8new__bufS525;
  moonbit_decref(_M0L4selfS528);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS534,
  int32_t _M0L13new__capacityS532
) {
  struct _M0TUsiE** _M0L8new__bufS531;
  struct _M0TUsiE** _M0L8_2afieldS4266;
  struct _M0TUsiE** _M0L8old__bufS533;
  int32_t _M0L8old__capS535;
  int32_t _M0L9copy__lenS536;
  struct _M0TUsiE** _M0L6_2aoldS4265;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS531
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS532, 0);
  _M0L8_2afieldS4266 = _M0L4selfS534->$0;
  _M0L8old__bufS533 = _M0L8_2afieldS4266;
  _M0L8old__capS535 = Moonbit_array_length(_M0L8old__bufS533);
  if (_M0L8old__capS535 < _M0L13new__capacityS532) {
    _M0L9copy__lenS536 = _M0L8old__capS535;
  } else {
    _M0L9copy__lenS536 = _M0L13new__capacityS532;
  }
  moonbit_incref(_M0L8old__bufS533);
  moonbit_incref(_M0L8new__bufS531);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS531, 0, _M0L8old__bufS533, 0, _M0L9copy__lenS536);
  _M0L6_2aoldS4265 = _M0L4selfS534->$0;
  moonbit_decref(_M0L6_2aoldS4265);
  _M0L4selfS534->$0 = _M0L8new__bufS531;
  moonbit_decref(_M0L4selfS534);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L4selfS540,
  int32_t _M0L13new__capacityS538
) {
  void** _M0L8new__bufS537;
  void** _M0L8_2afieldS4268;
  void** _M0L8old__bufS539;
  int32_t _M0L8old__capS541;
  int32_t _M0L9copy__lenS542;
  void** _M0L6_2aoldS4267;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS537
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS538, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4268 = _M0L4selfS540->$0;
  _M0L8old__bufS539 = _M0L8_2afieldS4268;
  _M0L8old__capS541 = Moonbit_array_length(_M0L8old__bufS539);
  if (_M0L8old__capS541 < _M0L13new__capacityS538) {
    _M0L9copy__lenS542 = _M0L8old__capS541;
  } else {
    _M0L9copy__lenS542 = _M0L13new__capacityS538;
  }
  moonbit_incref(_M0L8old__bufS539);
  moonbit_incref(_M0L8new__bufS537);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L8new__bufS537, 0, _M0L8old__bufS539, 0, _M0L9copy__lenS542);
  _M0L6_2aoldS4267 = _M0L4selfS540->$0;
  moonbit_decref(_M0L6_2aoldS4267);
  _M0L4selfS540->$0 = _M0L8new__bufS537;
  moonbit_decref(_M0L4selfS540);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGiE(
  struct _M0TPB5ArrayGiE* _M0L4selfS546,
  int32_t _M0L13new__capacityS544
) {
  int32_t* _M0L8new__bufS543;
  int32_t* _M0L8_2afieldS4270;
  int32_t* _M0L8old__bufS545;
  int32_t _M0L8old__capS547;
  int32_t _M0L9copy__lenS548;
  int32_t* _M0L6_2aoldS4269;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS543
  = (int32_t*)moonbit_make_int32_array_raw(_M0L13new__capacityS544);
  _M0L8_2afieldS4270 = _M0L4selfS546->$0;
  _M0L8old__bufS545 = _M0L8_2afieldS4270;
  _M0L8old__capS547 = Moonbit_array_length(_M0L8old__bufS545);
  if (_M0L8old__capS547 < _M0L13new__capacityS544) {
    _M0L9copy__lenS548 = _M0L8old__capS547;
  } else {
    _M0L9copy__lenS548 = _M0L13new__capacityS544;
  }
  moonbit_incref(_M0L8old__bufS545);
  moonbit_incref(_M0L8new__bufS543);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGiE(_M0L8new__bufS543, 0, _M0L8old__bufS545, 0, _M0L9copy__lenS548);
  _M0L6_2aoldS4269 = _M0L4selfS546->$0;
  moonbit_decref(_M0L6_2aoldS4269);
  _M0L4selfS546->$0 = _M0L8new__bufS543;
  moonbit_decref(_M0L4selfS546);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS552,
  int32_t _M0L13new__capacityS550
) {
  void** _M0L8new__bufS549;
  void** _M0L8_2afieldS4272;
  void** _M0L8old__bufS551;
  int32_t _M0L8old__capS553;
  int32_t _M0L9copy__lenS554;
  void** _M0L6_2aoldS4271;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS549
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS550, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4272 = _M0L4selfS552->$0;
  _M0L8old__bufS551 = _M0L8_2afieldS4272;
  _M0L8old__capS553 = Moonbit_array_length(_M0L8old__bufS551);
  if (_M0L8old__capS553 < _M0L13new__capacityS550) {
    _M0L9copy__lenS554 = _M0L8old__capS553;
  } else {
    _M0L9copy__lenS554 = _M0L13new__capacityS550;
  }
  moonbit_incref(_M0L8old__bufS551);
  moonbit_incref(_M0L8new__bufS549);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L8new__bufS549, 0, _M0L8old__bufS551, 0, _M0L9copy__lenS554);
  _M0L6_2aoldS4271 = _M0L4selfS552->$0;
  moonbit_decref(_M0L6_2aoldS4271);
  _M0L4selfS552->$0 = _M0L8new__bufS549;
  moonbit_decref(_M0L4selfS552);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS558,
  int32_t _M0L13new__capacityS556
) {
  void** _M0L8new__bufS555;
  void** _M0L8_2afieldS4274;
  void** _M0L8old__bufS557;
  int32_t _M0L8old__capS559;
  int32_t _M0L9copy__lenS560;
  void** _M0L6_2aoldS4273;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS555
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS556, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4274 = _M0L4selfS558->$0;
  _M0L8old__bufS557 = _M0L8_2afieldS4274;
  _M0L8old__capS559 = Moonbit_array_length(_M0L8old__bufS557);
  if (_M0L8old__capS559 < _M0L13new__capacityS556) {
    _M0L9copy__lenS560 = _M0L8old__capS559;
  } else {
    _M0L9copy__lenS560 = _M0L13new__capacityS556;
  }
  moonbit_incref(_M0L8old__bufS557);
  moonbit_incref(_M0L8new__bufS555);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS555, 0, _M0L8old__bufS557, 0, _M0L9copy__lenS560);
  _M0L6_2aoldS4273 = _M0L4selfS558->$0;
  moonbit_decref(_M0L6_2aoldS4273);
  _M0L4selfS558->$0 = _M0L8new__bufS555;
  moonbit_decref(_M0L4selfS558);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS524
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS524 == 0) {
    moonbit_string_t* _M0L6_2atmpS2267 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4697 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4697)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4697->$0 = _M0L6_2atmpS2267;
    _block_4697->$1 = 0;
    return _block_4697;
  } else {
    moonbit_string_t* _M0L6_2atmpS2268 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS524, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4698 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4698)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4698->$0 = _M0L6_2atmpS2268;
    _block_4698->$1 = 0;
    return _block_4698;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS518,
  int32_t _M0L1nS517
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS517 <= 0) {
    moonbit_decref(_M0L4selfS518);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS517 == 1) {
    return _M0L4selfS518;
  } else {
    int32_t _M0L3lenS519 = Moonbit_array_length(_M0L4selfS518);
    int32_t _M0L6_2atmpS2266 = _M0L3lenS519 * _M0L1nS517;
    struct _M0TPB13StringBuilder* _M0L3bufS520;
    moonbit_string_t _M0L3strS521;
    int32_t _M0L2__S522;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS520 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2266);
    _M0L3strS521 = _M0L4selfS518;
    _M0L2__S522 = 0;
    while (1) {
      if (_M0L2__S522 < _M0L1nS517) {
        int32_t _M0L6_2atmpS2265;
        moonbit_incref(_M0L3strS521);
        moonbit_incref(_M0L3bufS520);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS520, _M0L3strS521);
        _M0L6_2atmpS2265 = _M0L2__S522 + 1;
        _M0L2__S522 = _M0L6_2atmpS2265;
        continue;
      } else {
        moonbit_decref(_M0L3strS521);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS520);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS515,
  struct _M0TPC16string10StringView _M0L3strS516
) {
  int32_t _M0L3lenS2253;
  int32_t _M0L6_2atmpS2255;
  int32_t _M0L6_2atmpS2254;
  int32_t _M0L6_2atmpS2252;
  moonbit_bytes_t _M0L8_2afieldS4275;
  moonbit_bytes_t _M0L4dataS2256;
  int32_t _M0L3lenS2257;
  moonbit_string_t _M0L6_2atmpS2258;
  int32_t _M0L6_2atmpS2259;
  int32_t _M0L6_2atmpS2260;
  int32_t _M0L3lenS2262;
  int32_t _M0L6_2atmpS2264;
  int32_t _M0L6_2atmpS2263;
  int32_t _M0L6_2atmpS2261;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2253 = _M0L4selfS515->$1;
  moonbit_incref(_M0L3strS516.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2255 = _M0MPC16string10StringView6length(_M0L3strS516);
  _M0L6_2atmpS2254 = _M0L6_2atmpS2255 * 2;
  _M0L6_2atmpS2252 = _M0L3lenS2253 + _M0L6_2atmpS2254;
  moonbit_incref(_M0L4selfS515);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS515, _M0L6_2atmpS2252);
  _M0L8_2afieldS4275 = _M0L4selfS515->$0;
  _M0L4dataS2256 = _M0L8_2afieldS4275;
  _M0L3lenS2257 = _M0L4selfS515->$1;
  moonbit_incref(_M0L4dataS2256);
  moonbit_incref(_M0L3strS516.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2258 = _M0MPC16string10StringView4data(_M0L3strS516);
  moonbit_incref(_M0L3strS516.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2259 = _M0MPC16string10StringView13start__offset(_M0L3strS516);
  moonbit_incref(_M0L3strS516.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2260 = _M0MPC16string10StringView6length(_M0L3strS516);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2256, _M0L3lenS2257, _M0L6_2atmpS2258, _M0L6_2atmpS2259, _M0L6_2atmpS2260);
  _M0L3lenS2262 = _M0L4selfS515->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2264 = _M0MPC16string10StringView6length(_M0L3strS516);
  _M0L6_2atmpS2263 = _M0L6_2atmpS2264 * 2;
  _M0L6_2atmpS2261 = _M0L3lenS2262 + _M0L6_2atmpS2263;
  _M0L4selfS515->$1 = _M0L6_2atmpS2261;
  moonbit_decref(_M0L4selfS515);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS507,
  int32_t _M0L3lenS510,
  int32_t _M0L13start__offsetS514,
  int64_t _M0L11end__offsetS505
) {
  int32_t _M0L11end__offsetS504;
  int32_t _M0L5indexS508;
  int32_t _M0L5countS509;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS505 == 4294967296ll) {
    _M0L11end__offsetS504 = Moonbit_array_length(_M0L4selfS507);
  } else {
    int64_t _M0L7_2aSomeS506 = _M0L11end__offsetS505;
    _M0L11end__offsetS504 = (int32_t)_M0L7_2aSomeS506;
  }
  _M0L5indexS508 = _M0L13start__offsetS514;
  _M0L5countS509 = 0;
  while (1) {
    int32_t _if__result_4701;
    if (_M0L5indexS508 < _M0L11end__offsetS504) {
      _if__result_4701 = _M0L5countS509 < _M0L3lenS510;
    } else {
      _if__result_4701 = 0;
    }
    if (_if__result_4701) {
      int32_t _M0L2c1S511 = _M0L4selfS507[_M0L5indexS508];
      int32_t _if__result_4702;
      int32_t _M0L6_2atmpS2250;
      int32_t _M0L6_2atmpS2251;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S511)) {
        int32_t _M0L6_2atmpS2246 = _M0L5indexS508 + 1;
        _if__result_4702 = _M0L6_2atmpS2246 < _M0L11end__offsetS504;
      } else {
        _if__result_4702 = 0;
      }
      if (_if__result_4702) {
        int32_t _M0L6_2atmpS2249 = _M0L5indexS508 + 1;
        int32_t _M0L2c2S512 = _M0L4selfS507[_M0L6_2atmpS2249];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S512)) {
          int32_t _M0L6_2atmpS2247 = _M0L5indexS508 + 2;
          int32_t _M0L6_2atmpS2248 = _M0L5countS509 + 1;
          _M0L5indexS508 = _M0L6_2atmpS2247;
          _M0L5countS509 = _M0L6_2atmpS2248;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_99.data, (moonbit_string_t)moonbit_string_literal_100.data);
        }
      }
      _M0L6_2atmpS2250 = _M0L5indexS508 + 1;
      _M0L6_2atmpS2251 = _M0L5countS509 + 1;
      _M0L5indexS508 = _M0L6_2atmpS2250;
      _M0L5countS509 = _M0L6_2atmpS2251;
      continue;
    } else {
      moonbit_decref(_M0L4selfS507);
      return _M0L5countS509 >= _M0L3lenS510;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS498
) {
  int32_t _M0L3endS2234;
  int32_t _M0L8_2afieldS4276;
  int32_t _M0L5startS2235;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2234 = _M0L4selfS498.$2;
  _M0L8_2afieldS4276 = _M0L4selfS498.$1;
  moonbit_decref(_M0L4selfS498.$0);
  _M0L5startS2235 = _M0L8_2afieldS4276;
  return _M0L3endS2234 - _M0L5startS2235;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS499
) {
  int32_t _M0L3endS2236;
  int32_t _M0L8_2afieldS4277;
  int32_t _M0L5startS2237;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2236 = _M0L4selfS499.$2;
  _M0L8_2afieldS4277 = _M0L4selfS499.$1;
  moonbit_decref(_M0L4selfS499.$0);
  _M0L5startS2237 = _M0L8_2afieldS4277;
  return _M0L3endS2236 - _M0L5startS2237;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS500
) {
  int32_t _M0L3endS2238;
  int32_t _M0L8_2afieldS4278;
  int32_t _M0L5startS2239;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2238 = _M0L4selfS500.$2;
  _M0L8_2afieldS4278 = _M0L4selfS500.$1;
  moonbit_decref(_M0L4selfS500.$0);
  _M0L5startS2239 = _M0L8_2afieldS4278;
  return _M0L3endS2238 - _M0L5startS2239;
}

int32_t _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE _M0L4selfS501
) {
  int32_t _M0L3endS2240;
  int32_t _M0L8_2afieldS4279;
  int32_t _M0L5startS2241;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2240 = _M0L4selfS501.$2;
  _M0L8_2afieldS4279 = _M0L4selfS501.$1;
  moonbit_decref(_M0L4selfS501.$0);
  _M0L5startS2241 = _M0L8_2afieldS4279;
  return _M0L3endS2240 - _M0L5startS2241;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS502
) {
  int32_t _M0L3endS2242;
  int32_t _M0L8_2afieldS4280;
  int32_t _M0L5startS2243;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2242 = _M0L4selfS502.$2;
  _M0L8_2afieldS4280 = _M0L4selfS502.$1;
  moonbit_decref(_M0L4selfS502.$0);
  _M0L5startS2243 = _M0L8_2afieldS4280;
  return _M0L3endS2242 - _M0L5startS2243;
}

int32_t _M0MPC15array9ArrayView6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0L4selfS503
) {
  int32_t _M0L3endS2244;
  int32_t _M0L8_2afieldS4281;
  int32_t _M0L5startS2245;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2244 = _M0L4selfS503.$2;
  _M0L8_2afieldS4281 = _M0L4selfS503.$1;
  moonbit_decref(_M0L4selfS503.$0);
  _M0L5startS2245 = _M0L8_2afieldS4281;
  return _M0L3endS2244 - _M0L5startS2245;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS496,
  int64_t _M0L19start__offset_2eoptS494,
  int64_t _M0L11end__offsetS497
) {
  int32_t _M0L13start__offsetS493;
  if (_M0L19start__offset_2eoptS494 == 4294967296ll) {
    _M0L13start__offsetS493 = 0;
  } else {
    int64_t _M0L7_2aSomeS495 = _M0L19start__offset_2eoptS494;
    _M0L13start__offsetS493 = (int32_t)_M0L7_2aSomeS495;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS496, _M0L13start__offsetS493, _M0L11end__offsetS497);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS491,
  int32_t _M0L13start__offsetS492,
  int64_t _M0L11end__offsetS489
) {
  int32_t _M0L11end__offsetS488;
  int32_t _if__result_4703;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS489 == 4294967296ll) {
    _M0L11end__offsetS488 = Moonbit_array_length(_M0L4selfS491);
  } else {
    int64_t _M0L7_2aSomeS490 = _M0L11end__offsetS489;
    _M0L11end__offsetS488 = (int32_t)_M0L7_2aSomeS490;
  }
  if (_M0L13start__offsetS492 >= 0) {
    if (_M0L13start__offsetS492 <= _M0L11end__offsetS488) {
      int32_t _M0L6_2atmpS2233 = Moonbit_array_length(_M0L4selfS491);
      _if__result_4703 = _M0L11end__offsetS488 <= _M0L6_2atmpS2233;
    } else {
      _if__result_4703 = 0;
    }
  } else {
    _if__result_4703 = 0;
  }
  if (_if__result_4703) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS492,
                                                 _M0L11end__offsetS488,
                                                 _M0L4selfS491};
  } else {
    moonbit_decref(_M0L4selfS491);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_101.data, (moonbit_string_t)moonbit_string_literal_102.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS487
) {
  moonbit_string_t _M0L8_2afieldS4283;
  moonbit_string_t _M0L3strS2230;
  int32_t _M0L5startS2231;
  int32_t _M0L8_2afieldS4282;
  int32_t _M0L3endS2232;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4283 = _M0L4selfS487.$0;
  _M0L3strS2230 = _M0L8_2afieldS4283;
  _M0L5startS2231 = _M0L4selfS487.$1;
  _M0L8_2afieldS4282 = _M0L4selfS487.$2;
  _M0L3endS2232 = _M0L8_2afieldS4282;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2230, _M0L5startS2231, _M0L3endS2232);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS485,
  struct _M0TPB6Logger _M0L6loggerS486
) {
  moonbit_string_t _M0L8_2afieldS4285;
  moonbit_string_t _M0L3strS2227;
  int32_t _M0L5startS2228;
  int32_t _M0L8_2afieldS4284;
  int32_t _M0L3endS2229;
  moonbit_string_t _M0L6substrS484;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4285 = _M0L4selfS485.$0;
  _M0L3strS2227 = _M0L8_2afieldS4285;
  _M0L5startS2228 = _M0L4selfS485.$1;
  _M0L8_2afieldS4284 = _M0L4selfS485.$2;
  _M0L3endS2229 = _M0L8_2afieldS4284;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS484
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2227, _M0L5startS2228, _M0L3endS2229);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS484, _M0L6loggerS486);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS476,
  struct _M0TPB6Logger _M0L6loggerS474
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS475;
  int32_t _M0L3lenS477;
  int32_t _M0L1iS478;
  int32_t _M0L3segS479;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS474.$1) {
    moonbit_incref(_M0L6loggerS474.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 34);
  moonbit_incref(_M0L4selfS476);
  if (_M0L6loggerS474.$1) {
    moonbit_incref(_M0L6loggerS474.$1);
  }
  _M0L6_2aenvS475
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS475)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS475->$0 = _M0L4selfS476;
  _M0L6_2aenvS475->$1_0 = _M0L6loggerS474.$0;
  _M0L6_2aenvS475->$1_1 = _M0L6loggerS474.$1;
  _M0L3lenS477 = Moonbit_array_length(_M0L4selfS476);
  _M0L1iS478 = 0;
  _M0L3segS479 = 0;
  _2afor_480:;
  while (1) {
    int32_t _M0L4codeS481;
    int32_t _M0L1cS483;
    int32_t _M0L6_2atmpS2211;
    int32_t _M0L6_2atmpS2212;
    int32_t _M0L6_2atmpS2213;
    int32_t _tmp_4707;
    int32_t _tmp_4708;
    if (_M0L1iS478 >= _M0L3lenS477) {
      moonbit_decref(_M0L4selfS476);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
      break;
    }
    _M0L4codeS481 = _M0L4selfS476[_M0L1iS478];
    switch (_M0L4codeS481) {
      case 34: {
        _M0L1cS483 = _M0L4codeS481;
        goto join_482;
        break;
      }
      
      case 92: {
        _M0L1cS483 = _M0L4codeS481;
        goto join_482;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2214;
        int32_t _M0L6_2atmpS2215;
        moonbit_incref(_M0L6_2aenvS475);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_81.data);
        _M0L6_2atmpS2214 = _M0L1iS478 + 1;
        _M0L6_2atmpS2215 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS2214;
        _M0L3segS479 = _M0L6_2atmpS2215;
        goto _2afor_480;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2216;
        int32_t _M0L6_2atmpS2217;
        moonbit_incref(_M0L6_2aenvS475);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_82.data);
        _M0L6_2atmpS2216 = _M0L1iS478 + 1;
        _M0L6_2atmpS2217 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS2216;
        _M0L3segS479 = _M0L6_2atmpS2217;
        goto _2afor_480;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2218;
        int32_t _M0L6_2atmpS2219;
        moonbit_incref(_M0L6_2aenvS475);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_83.data);
        _M0L6_2atmpS2218 = _M0L1iS478 + 1;
        _M0L6_2atmpS2219 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS2218;
        _M0L3segS479 = _M0L6_2atmpS2219;
        goto _2afor_480;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2220;
        int32_t _M0L6_2atmpS2221;
        moonbit_incref(_M0L6_2aenvS475);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
        if (_M0L6loggerS474.$1) {
          moonbit_incref(_M0L6loggerS474.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_84.data);
        _M0L6_2atmpS2220 = _M0L1iS478 + 1;
        _M0L6_2atmpS2221 = _M0L1iS478 + 1;
        _M0L1iS478 = _M0L6_2atmpS2220;
        _M0L3segS479 = _M0L6_2atmpS2221;
        goto _2afor_480;
        break;
      }
      default: {
        if (_M0L4codeS481 < 32) {
          int32_t _M0L6_2atmpS2223;
          moonbit_string_t _M0L6_2atmpS2222;
          int32_t _M0L6_2atmpS2224;
          int32_t _M0L6_2atmpS2225;
          moonbit_incref(_M0L6_2aenvS475);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
          if (_M0L6loggerS474.$1) {
            moonbit_incref(_M0L6loggerS474.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, (moonbit_string_t)moonbit_string_literal_103.data);
          _M0L6_2atmpS2223 = _M0L4codeS481 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2222 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2223);
          if (_M0L6loggerS474.$1) {
            moonbit_incref(_M0L6loggerS474.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS474.$0->$method_0(_M0L6loggerS474.$1, _M0L6_2atmpS2222);
          if (_M0L6loggerS474.$1) {
            moonbit_incref(_M0L6loggerS474.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 125);
          _M0L6_2atmpS2224 = _M0L1iS478 + 1;
          _M0L6_2atmpS2225 = _M0L1iS478 + 1;
          _M0L1iS478 = _M0L6_2atmpS2224;
          _M0L3segS479 = _M0L6_2atmpS2225;
          goto _2afor_480;
        } else {
          int32_t _M0L6_2atmpS2226 = _M0L1iS478 + 1;
          int32_t _tmp_4706 = _M0L3segS479;
          _M0L1iS478 = _M0L6_2atmpS2226;
          _M0L3segS479 = _tmp_4706;
          goto _2afor_480;
        }
        break;
      }
    }
    goto joinlet_4705;
    join_482:;
    moonbit_incref(_M0L6_2aenvS475);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS475, _M0L3segS479, _M0L1iS478);
    if (_M0L6loggerS474.$1) {
      moonbit_incref(_M0L6loggerS474.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2211 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS483);
    if (_M0L6loggerS474.$1) {
      moonbit_incref(_M0L6loggerS474.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, _M0L6_2atmpS2211);
    _M0L6_2atmpS2212 = _M0L1iS478 + 1;
    _M0L6_2atmpS2213 = _M0L1iS478 + 1;
    _M0L1iS478 = _M0L6_2atmpS2212;
    _M0L3segS479 = _M0L6_2atmpS2213;
    continue;
    joinlet_4705:;
    _tmp_4707 = _M0L1iS478;
    _tmp_4708 = _M0L3segS479;
    _M0L1iS478 = _tmp_4707;
    _M0L3segS479 = _tmp_4708;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS474.$0->$method_3(_M0L6loggerS474.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS470,
  int32_t _M0L3segS473,
  int32_t _M0L1iS472
) {
  struct _M0TPB6Logger _M0L8_2afieldS4287;
  struct _M0TPB6Logger _M0L6loggerS469;
  moonbit_string_t _M0L8_2afieldS4286;
  int32_t _M0L6_2acntS4519;
  moonbit_string_t _M0L4selfS471;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4287
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS470->$1_0, _M0L6_2aenvS470->$1_1
  };
  _M0L6loggerS469 = _M0L8_2afieldS4287;
  _M0L8_2afieldS4286 = _M0L6_2aenvS470->$0;
  _M0L6_2acntS4519 = Moonbit_object_header(_M0L6_2aenvS470)->rc;
  if (_M0L6_2acntS4519 > 1) {
    int32_t _M0L11_2anew__cntS4520 = _M0L6_2acntS4519 - 1;
    Moonbit_object_header(_M0L6_2aenvS470)->rc = _M0L11_2anew__cntS4520;
    if (_M0L6loggerS469.$1) {
      moonbit_incref(_M0L6loggerS469.$1);
    }
    moonbit_incref(_M0L8_2afieldS4286);
  } else if (_M0L6_2acntS4519 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS470);
  }
  _M0L4selfS471 = _M0L8_2afieldS4286;
  if (_M0L1iS472 > _M0L3segS473) {
    int32_t _M0L6_2atmpS2210 = _M0L1iS472 - _M0L3segS473;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS469.$0->$method_1(_M0L6loggerS469.$1, _M0L4selfS471, _M0L3segS473, _M0L6_2atmpS2210);
  } else {
    moonbit_decref(_M0L4selfS471);
    if (_M0L6loggerS469.$1) {
      moonbit_decref(_M0L6loggerS469.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS468) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS467;
  int32_t _M0L6_2atmpS2207;
  int32_t _M0L6_2atmpS2206;
  int32_t _M0L6_2atmpS2209;
  int32_t _M0L6_2atmpS2208;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2205;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS467 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2207 = _M0IPC14byte4BytePB3Div3div(_M0L1bS468, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2206
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2207);
  moonbit_incref(_M0L7_2aselfS467);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS467, _M0L6_2atmpS2206);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2209 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS468, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2208
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2209);
  moonbit_incref(_M0L7_2aselfS467);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS467, _M0L6_2atmpS2208);
  _M0L6_2atmpS2205 = _M0L7_2aselfS467;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2205);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS466) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS466 < 10) {
    int32_t _M0L6_2atmpS2202;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2202 = _M0IPC14byte4BytePB3Add3add(_M0L1iS466, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2202);
  } else {
    int32_t _M0L6_2atmpS2204;
    int32_t _M0L6_2atmpS2203;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2204 = _M0IPC14byte4BytePB3Add3add(_M0L1iS466, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2203 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2204, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2203);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS464,
  int32_t _M0L4thatS465
) {
  int32_t _M0L6_2atmpS2200;
  int32_t _M0L6_2atmpS2201;
  int32_t _M0L6_2atmpS2199;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2200 = (int32_t)_M0L4selfS464;
  _M0L6_2atmpS2201 = (int32_t)_M0L4thatS465;
  _M0L6_2atmpS2199 = _M0L6_2atmpS2200 - _M0L6_2atmpS2201;
  return _M0L6_2atmpS2199 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS462,
  int32_t _M0L4thatS463
) {
  int32_t _M0L6_2atmpS2197;
  int32_t _M0L6_2atmpS2198;
  int32_t _M0L6_2atmpS2196;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2197 = (int32_t)_M0L4selfS462;
  _M0L6_2atmpS2198 = (int32_t)_M0L4thatS463;
  _M0L6_2atmpS2196 = _M0L6_2atmpS2197 % _M0L6_2atmpS2198;
  return _M0L6_2atmpS2196 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS460,
  int32_t _M0L4thatS461
) {
  int32_t _M0L6_2atmpS2194;
  int32_t _M0L6_2atmpS2195;
  int32_t _M0L6_2atmpS2193;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2194 = (int32_t)_M0L4selfS460;
  _M0L6_2atmpS2195 = (int32_t)_M0L4thatS461;
  _M0L6_2atmpS2193 = _M0L6_2atmpS2194 / _M0L6_2atmpS2195;
  return _M0L6_2atmpS2193 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS458,
  int32_t _M0L4thatS459
) {
  int32_t _M0L6_2atmpS2191;
  int32_t _M0L6_2atmpS2192;
  int32_t _M0L6_2atmpS2190;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2191 = (int32_t)_M0L4selfS458;
  _M0L6_2atmpS2192 = (int32_t)_M0L4thatS459;
  _M0L6_2atmpS2190 = _M0L6_2atmpS2191 + _M0L6_2atmpS2192;
  return _M0L6_2atmpS2190 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS455,
  int32_t _M0L5startS453,
  int32_t _M0L3endS454
) {
  int32_t _if__result_4709;
  int32_t _M0L3lenS456;
  int32_t _M0L6_2atmpS2188;
  int32_t _M0L6_2atmpS2189;
  moonbit_bytes_t _M0L5bytesS457;
  moonbit_bytes_t _M0L6_2atmpS2187;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS453 == 0) {
    int32_t _M0L6_2atmpS2186 = Moonbit_array_length(_M0L3strS455);
    _if__result_4709 = _M0L3endS454 == _M0L6_2atmpS2186;
  } else {
    _if__result_4709 = 0;
  }
  if (_if__result_4709) {
    return _M0L3strS455;
  }
  _M0L3lenS456 = _M0L3endS454 - _M0L5startS453;
  _M0L6_2atmpS2188 = _M0L3lenS456 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2189 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS457
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2188, _M0L6_2atmpS2189);
  moonbit_incref(_M0L5bytesS457);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS457, 0, _M0L3strS455, _M0L5startS453, _M0L3lenS456);
  _M0L6_2atmpS2187 = _M0L5bytesS457;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2187, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS450) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS450;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS451
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS451;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS452) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS452;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS442,
  int32_t _M0L5radixS441
) {
  int32_t _if__result_4710;
  uint16_t* _M0L6bufferS443;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS441 < 2) {
    _if__result_4710 = 1;
  } else {
    _if__result_4710 = _M0L5radixS441 > 36;
  }
  if (_if__result_4710) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_105.data);
  }
  if (_M0L4selfS442 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_89.data;
  }
  switch (_M0L5radixS441) {
    case 10: {
      int32_t _M0L3lenS444;
      uint16_t* _M0L6bufferS445;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS444 = _M0FPB12dec__count64(_M0L4selfS442);
      _M0L6bufferS445 = (uint16_t*)moonbit_make_string(_M0L3lenS444, 0);
      moonbit_incref(_M0L6bufferS445);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS445, _M0L4selfS442, 0, _M0L3lenS444);
      _M0L6bufferS443 = _M0L6bufferS445;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS446;
      uint16_t* _M0L6bufferS447;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS446 = _M0FPB12hex__count64(_M0L4selfS442);
      _M0L6bufferS447 = (uint16_t*)moonbit_make_string(_M0L3lenS446, 0);
      moonbit_incref(_M0L6bufferS447);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS447, _M0L4selfS442, 0, _M0L3lenS446);
      _M0L6bufferS443 = _M0L6bufferS447;
      break;
    }
    default: {
      int32_t _M0L3lenS448;
      uint16_t* _M0L6bufferS449;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS448 = _M0FPB14radix__count64(_M0L4selfS442, _M0L5radixS441);
      _M0L6bufferS449 = (uint16_t*)moonbit_make_string(_M0L3lenS448, 0);
      moonbit_incref(_M0L6bufferS449);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS449, _M0L4selfS442, 0, _M0L3lenS448, _M0L5radixS441);
      _M0L6bufferS443 = _M0L6bufferS449;
      break;
    }
  }
  return _M0L6bufferS443;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS431,
  uint64_t _M0L3numS419,
  int32_t _M0L12digit__startS422,
  int32_t _M0L10total__lenS421
) {
  uint64_t _M0Lm3numS418;
  int32_t _M0Lm6offsetS420;
  uint64_t _M0L6_2atmpS2185;
  int32_t _M0Lm9remainingS433;
  int32_t _M0L6_2atmpS2166;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS418 = _M0L3numS419;
  _M0Lm6offsetS420 = _M0L10total__lenS421 - _M0L12digit__startS422;
  while (1) {
    uint64_t _M0L6_2atmpS2129 = _M0Lm3numS418;
    if (_M0L6_2atmpS2129 >= 10000ull) {
      uint64_t _M0L6_2atmpS2152 = _M0Lm3numS418;
      uint64_t _M0L1tS423 = _M0L6_2atmpS2152 / 10000ull;
      uint64_t _M0L6_2atmpS2151 = _M0Lm3numS418;
      uint64_t _M0L6_2atmpS2150 = _M0L6_2atmpS2151 % 10000ull;
      int32_t _M0L1rS424 = (int32_t)_M0L6_2atmpS2150;
      int32_t _M0L2d1S425;
      int32_t _M0L2d2S426;
      int32_t _M0L6_2atmpS2130;
      int32_t _M0L6_2atmpS2149;
      int32_t _M0L6_2atmpS2148;
      int32_t _M0L6d1__hiS427;
      int32_t _M0L6_2atmpS2147;
      int32_t _M0L6_2atmpS2146;
      int32_t _M0L6d1__loS428;
      int32_t _M0L6_2atmpS2145;
      int32_t _M0L6_2atmpS2144;
      int32_t _M0L6d2__hiS429;
      int32_t _M0L6_2atmpS2143;
      int32_t _M0L6_2atmpS2142;
      int32_t _M0L6d2__loS430;
      int32_t _M0L6_2atmpS2132;
      int32_t _M0L6_2atmpS2131;
      int32_t _M0L6_2atmpS2135;
      int32_t _M0L6_2atmpS2134;
      int32_t _M0L6_2atmpS2133;
      int32_t _M0L6_2atmpS2138;
      int32_t _M0L6_2atmpS2137;
      int32_t _M0L6_2atmpS2136;
      int32_t _M0L6_2atmpS2141;
      int32_t _M0L6_2atmpS2140;
      int32_t _M0L6_2atmpS2139;
      _M0Lm3numS418 = _M0L1tS423;
      _M0L2d1S425 = _M0L1rS424 / 100;
      _M0L2d2S426 = _M0L1rS424 % 100;
      _M0L6_2atmpS2130 = _M0Lm6offsetS420;
      _M0Lm6offsetS420 = _M0L6_2atmpS2130 - 4;
      _M0L6_2atmpS2149 = _M0L2d1S425 / 10;
      _M0L6_2atmpS2148 = 48 + _M0L6_2atmpS2149;
      _M0L6d1__hiS427 = (uint16_t)_M0L6_2atmpS2148;
      _M0L6_2atmpS2147 = _M0L2d1S425 % 10;
      _M0L6_2atmpS2146 = 48 + _M0L6_2atmpS2147;
      _M0L6d1__loS428 = (uint16_t)_M0L6_2atmpS2146;
      _M0L6_2atmpS2145 = _M0L2d2S426 / 10;
      _M0L6_2atmpS2144 = 48 + _M0L6_2atmpS2145;
      _M0L6d2__hiS429 = (uint16_t)_M0L6_2atmpS2144;
      _M0L6_2atmpS2143 = _M0L2d2S426 % 10;
      _M0L6_2atmpS2142 = 48 + _M0L6_2atmpS2143;
      _M0L6d2__loS430 = (uint16_t)_M0L6_2atmpS2142;
      _M0L6_2atmpS2132 = _M0Lm6offsetS420;
      _M0L6_2atmpS2131 = _M0L12digit__startS422 + _M0L6_2atmpS2132;
      _M0L6bufferS431[_M0L6_2atmpS2131] = _M0L6d1__hiS427;
      _M0L6_2atmpS2135 = _M0Lm6offsetS420;
      _M0L6_2atmpS2134 = _M0L12digit__startS422 + _M0L6_2atmpS2135;
      _M0L6_2atmpS2133 = _M0L6_2atmpS2134 + 1;
      _M0L6bufferS431[_M0L6_2atmpS2133] = _M0L6d1__loS428;
      _M0L6_2atmpS2138 = _M0Lm6offsetS420;
      _M0L6_2atmpS2137 = _M0L12digit__startS422 + _M0L6_2atmpS2138;
      _M0L6_2atmpS2136 = _M0L6_2atmpS2137 + 2;
      _M0L6bufferS431[_M0L6_2atmpS2136] = _M0L6d2__hiS429;
      _M0L6_2atmpS2141 = _M0Lm6offsetS420;
      _M0L6_2atmpS2140 = _M0L12digit__startS422 + _M0L6_2atmpS2141;
      _M0L6_2atmpS2139 = _M0L6_2atmpS2140 + 3;
      _M0L6bufferS431[_M0L6_2atmpS2139] = _M0L6d2__loS430;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2185 = _M0Lm3numS418;
  _M0Lm9remainingS433 = (int32_t)_M0L6_2atmpS2185;
  while (1) {
    int32_t _M0L6_2atmpS2153 = _M0Lm9remainingS433;
    if (_M0L6_2atmpS2153 >= 100) {
      int32_t _M0L6_2atmpS2165 = _M0Lm9remainingS433;
      int32_t _M0L1tS434 = _M0L6_2atmpS2165 / 100;
      int32_t _M0L6_2atmpS2164 = _M0Lm9remainingS433;
      int32_t _M0L1dS435 = _M0L6_2atmpS2164 % 100;
      int32_t _M0L6_2atmpS2154;
      int32_t _M0L6_2atmpS2163;
      int32_t _M0L6_2atmpS2162;
      int32_t _M0L5d__hiS436;
      int32_t _M0L6_2atmpS2161;
      int32_t _M0L6_2atmpS2160;
      int32_t _M0L5d__loS437;
      int32_t _M0L6_2atmpS2156;
      int32_t _M0L6_2atmpS2155;
      int32_t _M0L6_2atmpS2159;
      int32_t _M0L6_2atmpS2158;
      int32_t _M0L6_2atmpS2157;
      _M0Lm9remainingS433 = _M0L1tS434;
      _M0L6_2atmpS2154 = _M0Lm6offsetS420;
      _M0Lm6offsetS420 = _M0L6_2atmpS2154 - 2;
      _M0L6_2atmpS2163 = _M0L1dS435 / 10;
      _M0L6_2atmpS2162 = 48 + _M0L6_2atmpS2163;
      _M0L5d__hiS436 = (uint16_t)_M0L6_2atmpS2162;
      _M0L6_2atmpS2161 = _M0L1dS435 % 10;
      _M0L6_2atmpS2160 = 48 + _M0L6_2atmpS2161;
      _M0L5d__loS437 = (uint16_t)_M0L6_2atmpS2160;
      _M0L6_2atmpS2156 = _M0Lm6offsetS420;
      _M0L6_2atmpS2155 = _M0L12digit__startS422 + _M0L6_2atmpS2156;
      _M0L6bufferS431[_M0L6_2atmpS2155] = _M0L5d__hiS436;
      _M0L6_2atmpS2159 = _M0Lm6offsetS420;
      _M0L6_2atmpS2158 = _M0L12digit__startS422 + _M0L6_2atmpS2159;
      _M0L6_2atmpS2157 = _M0L6_2atmpS2158 + 1;
      _M0L6bufferS431[_M0L6_2atmpS2157] = _M0L5d__loS437;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2166 = _M0Lm9remainingS433;
  if (_M0L6_2atmpS2166 >= 10) {
    int32_t _M0L6_2atmpS2167 = _M0Lm6offsetS420;
    int32_t _M0L6_2atmpS2178;
    int32_t _M0L6_2atmpS2177;
    int32_t _M0L6_2atmpS2176;
    int32_t _M0L5d__hiS439;
    int32_t _M0L6_2atmpS2175;
    int32_t _M0L6_2atmpS2174;
    int32_t _M0L6_2atmpS2173;
    int32_t _M0L5d__loS440;
    int32_t _M0L6_2atmpS2169;
    int32_t _M0L6_2atmpS2168;
    int32_t _M0L6_2atmpS2172;
    int32_t _M0L6_2atmpS2171;
    int32_t _M0L6_2atmpS2170;
    _M0Lm6offsetS420 = _M0L6_2atmpS2167 - 2;
    _M0L6_2atmpS2178 = _M0Lm9remainingS433;
    _M0L6_2atmpS2177 = _M0L6_2atmpS2178 / 10;
    _M0L6_2atmpS2176 = 48 + _M0L6_2atmpS2177;
    _M0L5d__hiS439 = (uint16_t)_M0L6_2atmpS2176;
    _M0L6_2atmpS2175 = _M0Lm9remainingS433;
    _M0L6_2atmpS2174 = _M0L6_2atmpS2175 % 10;
    _M0L6_2atmpS2173 = 48 + _M0L6_2atmpS2174;
    _M0L5d__loS440 = (uint16_t)_M0L6_2atmpS2173;
    _M0L6_2atmpS2169 = _M0Lm6offsetS420;
    _M0L6_2atmpS2168 = _M0L12digit__startS422 + _M0L6_2atmpS2169;
    _M0L6bufferS431[_M0L6_2atmpS2168] = _M0L5d__hiS439;
    _M0L6_2atmpS2172 = _M0Lm6offsetS420;
    _M0L6_2atmpS2171 = _M0L12digit__startS422 + _M0L6_2atmpS2172;
    _M0L6_2atmpS2170 = _M0L6_2atmpS2171 + 1;
    _M0L6bufferS431[_M0L6_2atmpS2170] = _M0L5d__loS440;
    moonbit_decref(_M0L6bufferS431);
  } else {
    int32_t _M0L6_2atmpS2179 = _M0Lm6offsetS420;
    int32_t _M0L6_2atmpS2184;
    int32_t _M0L6_2atmpS2180;
    int32_t _M0L6_2atmpS2183;
    int32_t _M0L6_2atmpS2182;
    int32_t _M0L6_2atmpS2181;
    _M0Lm6offsetS420 = _M0L6_2atmpS2179 - 1;
    _M0L6_2atmpS2184 = _M0Lm6offsetS420;
    _M0L6_2atmpS2180 = _M0L12digit__startS422 + _M0L6_2atmpS2184;
    _M0L6_2atmpS2183 = _M0Lm9remainingS433;
    _M0L6_2atmpS2182 = 48 + _M0L6_2atmpS2183;
    _M0L6_2atmpS2181 = (uint16_t)_M0L6_2atmpS2182;
    _M0L6bufferS431[_M0L6_2atmpS2180] = _M0L6_2atmpS2181;
    moonbit_decref(_M0L6bufferS431);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS413,
  uint64_t _M0L3numS407,
  int32_t _M0L12digit__startS405,
  int32_t _M0L10total__lenS404,
  int32_t _M0L5radixS409
) {
  int32_t _M0Lm6offsetS403;
  uint64_t _M0Lm1nS406;
  uint64_t _M0L4baseS408;
  int32_t _M0L6_2atmpS2111;
  int32_t _M0L6_2atmpS2110;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS403 = _M0L10total__lenS404 - _M0L12digit__startS405;
  _M0Lm1nS406 = _M0L3numS407;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS408 = _M0MPC13int3Int10to__uint64(_M0L5radixS409);
  _M0L6_2atmpS2111 = _M0L5radixS409 - 1;
  _M0L6_2atmpS2110 = _M0L5radixS409 & _M0L6_2atmpS2111;
  if (_M0L6_2atmpS2110 == 0) {
    int32_t _M0L5shiftS410;
    uint64_t _M0L4maskS411;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS410 = moonbit_ctz32(_M0L5radixS409);
    _M0L4maskS411 = _M0L4baseS408 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS2112 = _M0Lm1nS406;
      if (_M0L6_2atmpS2112 > 0ull) {
        int32_t _M0L6_2atmpS2113 = _M0Lm6offsetS403;
        uint64_t _M0L6_2atmpS2119;
        uint64_t _M0L6_2atmpS2118;
        int32_t _M0L5digitS412;
        int32_t _M0L6_2atmpS2116;
        int32_t _M0L6_2atmpS2114;
        int32_t _M0L6_2atmpS2115;
        uint64_t _M0L6_2atmpS2117;
        _M0Lm6offsetS403 = _M0L6_2atmpS2113 - 1;
        _M0L6_2atmpS2119 = _M0Lm1nS406;
        _M0L6_2atmpS2118 = _M0L6_2atmpS2119 & _M0L4maskS411;
        _M0L5digitS412 = (int32_t)_M0L6_2atmpS2118;
        _M0L6_2atmpS2116 = _M0Lm6offsetS403;
        _M0L6_2atmpS2114 = _M0L12digit__startS405 + _M0L6_2atmpS2116;
        _M0L6_2atmpS2115
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS412
        ];
        _M0L6bufferS413[_M0L6_2atmpS2114] = _M0L6_2atmpS2115;
        _M0L6_2atmpS2117 = _M0Lm1nS406;
        _M0Lm1nS406 = _M0L6_2atmpS2117 >> (_M0L5shiftS410 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS413);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS2120 = _M0Lm1nS406;
      if (_M0L6_2atmpS2120 > 0ull) {
        int32_t _M0L6_2atmpS2121 = _M0Lm6offsetS403;
        uint64_t _M0L6_2atmpS2128;
        uint64_t _M0L1qS415;
        uint64_t _M0L6_2atmpS2126;
        uint64_t _M0L6_2atmpS2127;
        uint64_t _M0L6_2atmpS2125;
        int32_t _M0L5digitS416;
        int32_t _M0L6_2atmpS2124;
        int32_t _M0L6_2atmpS2122;
        int32_t _M0L6_2atmpS2123;
        _M0Lm6offsetS403 = _M0L6_2atmpS2121 - 1;
        _M0L6_2atmpS2128 = _M0Lm1nS406;
        _M0L1qS415 = _M0L6_2atmpS2128 / _M0L4baseS408;
        _M0L6_2atmpS2126 = _M0Lm1nS406;
        _M0L6_2atmpS2127 = _M0L1qS415 * _M0L4baseS408;
        _M0L6_2atmpS2125 = _M0L6_2atmpS2126 - _M0L6_2atmpS2127;
        _M0L5digitS416 = (int32_t)_M0L6_2atmpS2125;
        _M0L6_2atmpS2124 = _M0Lm6offsetS403;
        _M0L6_2atmpS2122 = _M0L12digit__startS405 + _M0L6_2atmpS2124;
        _M0L6_2atmpS2123
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS416
        ];
        _M0L6bufferS413[_M0L6_2atmpS2122] = _M0L6_2atmpS2123;
        _M0Lm1nS406 = _M0L1qS415;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS413);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS400,
  uint64_t _M0L3numS396,
  int32_t _M0L12digit__startS394,
  int32_t _M0L10total__lenS393
) {
  int32_t _M0Lm6offsetS392;
  uint64_t _M0Lm1nS395;
  int32_t _M0L6_2atmpS2106;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS392 = _M0L10total__lenS393 - _M0L12digit__startS394;
  _M0Lm1nS395 = _M0L3numS396;
  while (1) {
    int32_t _M0L6_2atmpS2094 = _M0Lm6offsetS392;
    if (_M0L6_2atmpS2094 >= 2) {
      int32_t _M0L6_2atmpS2095 = _M0Lm6offsetS392;
      uint64_t _M0L6_2atmpS2105;
      uint64_t _M0L6_2atmpS2104;
      int32_t _M0L9byte__valS397;
      int32_t _M0L2hiS398;
      int32_t _M0L2loS399;
      int32_t _M0L6_2atmpS2098;
      int32_t _M0L6_2atmpS2096;
      int32_t _M0L6_2atmpS2097;
      int32_t _M0L6_2atmpS2102;
      int32_t _M0L6_2atmpS2101;
      int32_t _M0L6_2atmpS2099;
      int32_t _M0L6_2atmpS2100;
      uint64_t _M0L6_2atmpS2103;
      _M0Lm6offsetS392 = _M0L6_2atmpS2095 - 2;
      _M0L6_2atmpS2105 = _M0Lm1nS395;
      _M0L6_2atmpS2104 = _M0L6_2atmpS2105 & 255ull;
      _M0L9byte__valS397 = (int32_t)_M0L6_2atmpS2104;
      _M0L2hiS398 = _M0L9byte__valS397 / 16;
      _M0L2loS399 = _M0L9byte__valS397 % 16;
      _M0L6_2atmpS2098 = _M0Lm6offsetS392;
      _M0L6_2atmpS2096 = _M0L12digit__startS394 + _M0L6_2atmpS2098;
      _M0L6_2atmpS2097
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2hiS398
      ];
      _M0L6bufferS400[_M0L6_2atmpS2096] = _M0L6_2atmpS2097;
      _M0L6_2atmpS2102 = _M0Lm6offsetS392;
      _M0L6_2atmpS2101 = _M0L12digit__startS394 + _M0L6_2atmpS2102;
      _M0L6_2atmpS2099 = _M0L6_2atmpS2101 + 1;
      _M0L6_2atmpS2100
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2loS399
      ];
      _M0L6bufferS400[_M0L6_2atmpS2099] = _M0L6_2atmpS2100;
      _M0L6_2atmpS2103 = _M0Lm1nS395;
      _M0Lm1nS395 = _M0L6_2atmpS2103 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2106 = _M0Lm6offsetS392;
  if (_M0L6_2atmpS2106 == 1) {
    uint64_t _M0L6_2atmpS2109 = _M0Lm1nS395;
    uint64_t _M0L6_2atmpS2108 = _M0L6_2atmpS2109 & 15ull;
    int32_t _M0L6nibbleS402 = (int32_t)_M0L6_2atmpS2108;
    int32_t _M0L6_2atmpS2107 =
      ((moonbit_string_t)moonbit_string_literal_106.data)[_M0L6nibbleS402];
    _M0L6bufferS400[_M0L12digit__startS394] = _M0L6_2atmpS2107;
    moonbit_decref(_M0L6bufferS400);
  } else {
    moonbit_decref(_M0L6bufferS400);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS386,
  int32_t _M0L5radixS389
) {
  uint64_t _M0Lm3numS387;
  uint64_t _M0L4baseS388;
  int32_t _M0Lm5countS390;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS386 == 0ull) {
    return 1;
  }
  _M0Lm3numS387 = _M0L5valueS386;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS388 = _M0MPC13int3Int10to__uint64(_M0L5radixS389);
  _M0Lm5countS390 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS2091 = _M0Lm3numS387;
    if (_M0L6_2atmpS2091 > 0ull) {
      int32_t _M0L6_2atmpS2092 = _M0Lm5countS390;
      uint64_t _M0L6_2atmpS2093;
      _M0Lm5countS390 = _M0L6_2atmpS2092 + 1;
      _M0L6_2atmpS2093 = _M0Lm3numS387;
      _M0Lm3numS387 = _M0L6_2atmpS2093 / _M0L4baseS388;
      continue;
    }
    break;
  }
  return _M0Lm5countS390;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS384) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS384 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS385;
    int32_t _M0L6_2atmpS2090;
    int32_t _M0L6_2atmpS2089;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS385 = moonbit_clz64(_M0L5valueS384);
    _M0L6_2atmpS2090 = 63 - _M0L14leading__zerosS385;
    _M0L6_2atmpS2089 = _M0L6_2atmpS2090 / 4;
    return _M0L6_2atmpS2089 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS383) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS383 >= 10000000000ull) {
    if (_M0L5valueS383 >= 100000000000000ull) {
      if (_M0L5valueS383 >= 10000000000000000ull) {
        if (_M0L5valueS383 >= 1000000000000000000ull) {
          if (_M0L5valueS383 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS383 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS383 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS383 >= 1000000000000ull) {
      if (_M0L5valueS383 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS383 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS383 >= 100000ull) {
    if (_M0L5valueS383 >= 10000000ull) {
      if (_M0L5valueS383 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS383 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS383 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS383 >= 1000ull) {
    if (_M0L5valueS383 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS383 >= 100ull) {
    return 3;
  } else if (_M0L5valueS383 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS367,
  int32_t _M0L5radixS366
) {
  int32_t _if__result_4717;
  int32_t _M0L12is__negativeS368;
  uint32_t _M0L3numS369;
  uint16_t* _M0L6bufferS370;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS366 < 2) {
    _if__result_4717 = 1;
  } else {
    _if__result_4717 = _M0L5radixS366 > 36;
  }
  if (_if__result_4717) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_107.data);
  }
  if (_M0L4selfS367 == 0) {
    return (moonbit_string_t)moonbit_string_literal_89.data;
  }
  _M0L12is__negativeS368 = _M0L4selfS367 < 0;
  if (_M0L12is__negativeS368) {
    int32_t _M0L6_2atmpS2088 = -_M0L4selfS367;
    _M0L3numS369 = *(uint32_t*)&_M0L6_2atmpS2088;
  } else {
    _M0L3numS369 = *(uint32_t*)&_M0L4selfS367;
  }
  switch (_M0L5radixS366) {
    case 10: {
      int32_t _M0L10digit__lenS371;
      int32_t _M0L6_2atmpS2085;
      int32_t _M0L10total__lenS372;
      uint16_t* _M0L6bufferS373;
      int32_t _M0L12digit__startS374;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS371 = _M0FPB12dec__count32(_M0L3numS369);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS2085 = 1;
      } else {
        _M0L6_2atmpS2085 = 0;
      }
      _M0L10total__lenS372 = _M0L10digit__lenS371 + _M0L6_2atmpS2085;
      _M0L6bufferS373
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS372, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS374 = 1;
      } else {
        _M0L12digit__startS374 = 0;
      }
      moonbit_incref(_M0L6bufferS373);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS373, _M0L3numS369, _M0L12digit__startS374, _M0L10total__lenS372);
      _M0L6bufferS370 = _M0L6bufferS373;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS375;
      int32_t _M0L6_2atmpS2086;
      int32_t _M0L10total__lenS376;
      uint16_t* _M0L6bufferS377;
      int32_t _M0L12digit__startS378;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS375 = _M0FPB12hex__count32(_M0L3numS369);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS2086 = 1;
      } else {
        _M0L6_2atmpS2086 = 0;
      }
      _M0L10total__lenS376 = _M0L10digit__lenS375 + _M0L6_2atmpS2086;
      _M0L6bufferS377
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS376, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS378 = 1;
      } else {
        _M0L12digit__startS378 = 0;
      }
      moonbit_incref(_M0L6bufferS377);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS377, _M0L3numS369, _M0L12digit__startS378, _M0L10total__lenS376);
      _M0L6bufferS370 = _M0L6bufferS377;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS379;
      int32_t _M0L6_2atmpS2087;
      int32_t _M0L10total__lenS380;
      uint16_t* _M0L6bufferS381;
      int32_t _M0L12digit__startS382;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS379
      = _M0FPB14radix__count32(_M0L3numS369, _M0L5radixS366);
      if (_M0L12is__negativeS368) {
        _M0L6_2atmpS2087 = 1;
      } else {
        _M0L6_2atmpS2087 = 0;
      }
      _M0L10total__lenS380 = _M0L10digit__lenS379 + _M0L6_2atmpS2087;
      _M0L6bufferS381
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS380, 0);
      if (_M0L12is__negativeS368) {
        _M0L12digit__startS382 = 1;
      } else {
        _M0L12digit__startS382 = 0;
      }
      moonbit_incref(_M0L6bufferS381);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS381, _M0L3numS369, _M0L12digit__startS382, _M0L10total__lenS380, _M0L5radixS366);
      _M0L6bufferS370 = _M0L6bufferS381;
      break;
    }
  }
  if (_M0L12is__negativeS368) {
    _M0L6bufferS370[0] = 45;
  }
  return _M0L6bufferS370;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS360,
  int32_t _M0L5radixS363
) {
  uint32_t _M0Lm3numS361;
  uint32_t _M0L4baseS362;
  int32_t _M0Lm5countS364;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS360 == 0u) {
    return 1;
  }
  _M0Lm3numS361 = _M0L5valueS360;
  _M0L4baseS362 = *(uint32_t*)&_M0L5radixS363;
  _M0Lm5countS364 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS2082 = _M0Lm3numS361;
    if (_M0L6_2atmpS2082 > 0u) {
      int32_t _M0L6_2atmpS2083 = _M0Lm5countS364;
      uint32_t _M0L6_2atmpS2084;
      _M0Lm5countS364 = _M0L6_2atmpS2083 + 1;
      _M0L6_2atmpS2084 = _M0Lm3numS361;
      _M0Lm3numS361 = _M0L6_2atmpS2084 / _M0L4baseS362;
      continue;
    }
    break;
  }
  return _M0Lm5countS364;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS358) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS358 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS359;
    int32_t _M0L6_2atmpS2081;
    int32_t _M0L6_2atmpS2080;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS359 = moonbit_clz32(_M0L5valueS358);
    _M0L6_2atmpS2081 = 31 - _M0L14leading__zerosS359;
    _M0L6_2atmpS2080 = _M0L6_2atmpS2081 / 4;
    return _M0L6_2atmpS2080 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS357) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS357 >= 100000u) {
    if (_M0L5valueS357 >= 10000000u) {
      if (_M0L5valueS357 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS357 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS357 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS357 >= 1000u) {
    if (_M0L5valueS357 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS357 >= 100u) {
    return 3;
  } else if (_M0L5valueS357 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS347,
  uint32_t _M0L3numS335,
  int32_t _M0L12digit__startS338,
  int32_t _M0L10total__lenS337
) {
  uint32_t _M0Lm3numS334;
  int32_t _M0Lm6offsetS336;
  uint32_t _M0L6_2atmpS2079;
  int32_t _M0Lm9remainingS349;
  int32_t _M0L6_2atmpS2060;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS334 = _M0L3numS335;
  _M0Lm6offsetS336 = _M0L10total__lenS337 - _M0L12digit__startS338;
  while (1) {
    uint32_t _M0L6_2atmpS2023 = _M0Lm3numS334;
    if (_M0L6_2atmpS2023 >= 10000u) {
      uint32_t _M0L6_2atmpS2046 = _M0Lm3numS334;
      uint32_t _M0L1tS339 = _M0L6_2atmpS2046 / 10000u;
      uint32_t _M0L6_2atmpS2045 = _M0Lm3numS334;
      uint32_t _M0L6_2atmpS2044 = _M0L6_2atmpS2045 % 10000u;
      int32_t _M0L1rS340 = *(int32_t*)&_M0L6_2atmpS2044;
      int32_t _M0L2d1S341;
      int32_t _M0L2d2S342;
      int32_t _M0L6_2atmpS2024;
      int32_t _M0L6_2atmpS2043;
      int32_t _M0L6_2atmpS2042;
      int32_t _M0L6d1__hiS343;
      int32_t _M0L6_2atmpS2041;
      int32_t _M0L6_2atmpS2040;
      int32_t _M0L6d1__loS344;
      int32_t _M0L6_2atmpS2039;
      int32_t _M0L6_2atmpS2038;
      int32_t _M0L6d2__hiS345;
      int32_t _M0L6_2atmpS2037;
      int32_t _M0L6_2atmpS2036;
      int32_t _M0L6d2__loS346;
      int32_t _M0L6_2atmpS2026;
      int32_t _M0L6_2atmpS2025;
      int32_t _M0L6_2atmpS2029;
      int32_t _M0L6_2atmpS2028;
      int32_t _M0L6_2atmpS2027;
      int32_t _M0L6_2atmpS2032;
      int32_t _M0L6_2atmpS2031;
      int32_t _M0L6_2atmpS2030;
      int32_t _M0L6_2atmpS2035;
      int32_t _M0L6_2atmpS2034;
      int32_t _M0L6_2atmpS2033;
      _M0Lm3numS334 = _M0L1tS339;
      _M0L2d1S341 = _M0L1rS340 / 100;
      _M0L2d2S342 = _M0L1rS340 % 100;
      _M0L6_2atmpS2024 = _M0Lm6offsetS336;
      _M0Lm6offsetS336 = _M0L6_2atmpS2024 - 4;
      _M0L6_2atmpS2043 = _M0L2d1S341 / 10;
      _M0L6_2atmpS2042 = 48 + _M0L6_2atmpS2043;
      _M0L6d1__hiS343 = (uint16_t)_M0L6_2atmpS2042;
      _M0L6_2atmpS2041 = _M0L2d1S341 % 10;
      _M0L6_2atmpS2040 = 48 + _M0L6_2atmpS2041;
      _M0L6d1__loS344 = (uint16_t)_M0L6_2atmpS2040;
      _M0L6_2atmpS2039 = _M0L2d2S342 / 10;
      _M0L6_2atmpS2038 = 48 + _M0L6_2atmpS2039;
      _M0L6d2__hiS345 = (uint16_t)_M0L6_2atmpS2038;
      _M0L6_2atmpS2037 = _M0L2d2S342 % 10;
      _M0L6_2atmpS2036 = 48 + _M0L6_2atmpS2037;
      _M0L6d2__loS346 = (uint16_t)_M0L6_2atmpS2036;
      _M0L6_2atmpS2026 = _M0Lm6offsetS336;
      _M0L6_2atmpS2025 = _M0L12digit__startS338 + _M0L6_2atmpS2026;
      _M0L6bufferS347[_M0L6_2atmpS2025] = _M0L6d1__hiS343;
      _M0L6_2atmpS2029 = _M0Lm6offsetS336;
      _M0L6_2atmpS2028 = _M0L12digit__startS338 + _M0L6_2atmpS2029;
      _M0L6_2atmpS2027 = _M0L6_2atmpS2028 + 1;
      _M0L6bufferS347[_M0L6_2atmpS2027] = _M0L6d1__loS344;
      _M0L6_2atmpS2032 = _M0Lm6offsetS336;
      _M0L6_2atmpS2031 = _M0L12digit__startS338 + _M0L6_2atmpS2032;
      _M0L6_2atmpS2030 = _M0L6_2atmpS2031 + 2;
      _M0L6bufferS347[_M0L6_2atmpS2030] = _M0L6d2__hiS345;
      _M0L6_2atmpS2035 = _M0Lm6offsetS336;
      _M0L6_2atmpS2034 = _M0L12digit__startS338 + _M0L6_2atmpS2035;
      _M0L6_2atmpS2033 = _M0L6_2atmpS2034 + 3;
      _M0L6bufferS347[_M0L6_2atmpS2033] = _M0L6d2__loS346;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2079 = _M0Lm3numS334;
  _M0Lm9remainingS349 = *(int32_t*)&_M0L6_2atmpS2079;
  while (1) {
    int32_t _M0L6_2atmpS2047 = _M0Lm9remainingS349;
    if (_M0L6_2atmpS2047 >= 100) {
      int32_t _M0L6_2atmpS2059 = _M0Lm9remainingS349;
      int32_t _M0L1tS350 = _M0L6_2atmpS2059 / 100;
      int32_t _M0L6_2atmpS2058 = _M0Lm9remainingS349;
      int32_t _M0L1dS351 = _M0L6_2atmpS2058 % 100;
      int32_t _M0L6_2atmpS2048;
      int32_t _M0L6_2atmpS2057;
      int32_t _M0L6_2atmpS2056;
      int32_t _M0L5d__hiS352;
      int32_t _M0L6_2atmpS2055;
      int32_t _M0L6_2atmpS2054;
      int32_t _M0L5d__loS353;
      int32_t _M0L6_2atmpS2050;
      int32_t _M0L6_2atmpS2049;
      int32_t _M0L6_2atmpS2053;
      int32_t _M0L6_2atmpS2052;
      int32_t _M0L6_2atmpS2051;
      _M0Lm9remainingS349 = _M0L1tS350;
      _M0L6_2atmpS2048 = _M0Lm6offsetS336;
      _M0Lm6offsetS336 = _M0L6_2atmpS2048 - 2;
      _M0L6_2atmpS2057 = _M0L1dS351 / 10;
      _M0L6_2atmpS2056 = 48 + _M0L6_2atmpS2057;
      _M0L5d__hiS352 = (uint16_t)_M0L6_2atmpS2056;
      _M0L6_2atmpS2055 = _M0L1dS351 % 10;
      _M0L6_2atmpS2054 = 48 + _M0L6_2atmpS2055;
      _M0L5d__loS353 = (uint16_t)_M0L6_2atmpS2054;
      _M0L6_2atmpS2050 = _M0Lm6offsetS336;
      _M0L6_2atmpS2049 = _M0L12digit__startS338 + _M0L6_2atmpS2050;
      _M0L6bufferS347[_M0L6_2atmpS2049] = _M0L5d__hiS352;
      _M0L6_2atmpS2053 = _M0Lm6offsetS336;
      _M0L6_2atmpS2052 = _M0L12digit__startS338 + _M0L6_2atmpS2053;
      _M0L6_2atmpS2051 = _M0L6_2atmpS2052 + 1;
      _M0L6bufferS347[_M0L6_2atmpS2051] = _M0L5d__loS353;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2060 = _M0Lm9remainingS349;
  if (_M0L6_2atmpS2060 >= 10) {
    int32_t _M0L6_2atmpS2061 = _M0Lm6offsetS336;
    int32_t _M0L6_2atmpS2072;
    int32_t _M0L6_2atmpS2071;
    int32_t _M0L6_2atmpS2070;
    int32_t _M0L5d__hiS355;
    int32_t _M0L6_2atmpS2069;
    int32_t _M0L6_2atmpS2068;
    int32_t _M0L6_2atmpS2067;
    int32_t _M0L5d__loS356;
    int32_t _M0L6_2atmpS2063;
    int32_t _M0L6_2atmpS2062;
    int32_t _M0L6_2atmpS2066;
    int32_t _M0L6_2atmpS2065;
    int32_t _M0L6_2atmpS2064;
    _M0Lm6offsetS336 = _M0L6_2atmpS2061 - 2;
    _M0L6_2atmpS2072 = _M0Lm9remainingS349;
    _M0L6_2atmpS2071 = _M0L6_2atmpS2072 / 10;
    _M0L6_2atmpS2070 = 48 + _M0L6_2atmpS2071;
    _M0L5d__hiS355 = (uint16_t)_M0L6_2atmpS2070;
    _M0L6_2atmpS2069 = _M0Lm9remainingS349;
    _M0L6_2atmpS2068 = _M0L6_2atmpS2069 % 10;
    _M0L6_2atmpS2067 = 48 + _M0L6_2atmpS2068;
    _M0L5d__loS356 = (uint16_t)_M0L6_2atmpS2067;
    _M0L6_2atmpS2063 = _M0Lm6offsetS336;
    _M0L6_2atmpS2062 = _M0L12digit__startS338 + _M0L6_2atmpS2063;
    _M0L6bufferS347[_M0L6_2atmpS2062] = _M0L5d__hiS355;
    _M0L6_2atmpS2066 = _M0Lm6offsetS336;
    _M0L6_2atmpS2065 = _M0L12digit__startS338 + _M0L6_2atmpS2066;
    _M0L6_2atmpS2064 = _M0L6_2atmpS2065 + 1;
    _M0L6bufferS347[_M0L6_2atmpS2064] = _M0L5d__loS356;
    moonbit_decref(_M0L6bufferS347);
  } else {
    int32_t _M0L6_2atmpS2073 = _M0Lm6offsetS336;
    int32_t _M0L6_2atmpS2078;
    int32_t _M0L6_2atmpS2074;
    int32_t _M0L6_2atmpS2077;
    int32_t _M0L6_2atmpS2076;
    int32_t _M0L6_2atmpS2075;
    _M0Lm6offsetS336 = _M0L6_2atmpS2073 - 1;
    _M0L6_2atmpS2078 = _M0Lm6offsetS336;
    _M0L6_2atmpS2074 = _M0L12digit__startS338 + _M0L6_2atmpS2078;
    _M0L6_2atmpS2077 = _M0Lm9remainingS349;
    _M0L6_2atmpS2076 = 48 + _M0L6_2atmpS2077;
    _M0L6_2atmpS2075 = (uint16_t)_M0L6_2atmpS2076;
    _M0L6bufferS347[_M0L6_2atmpS2074] = _M0L6_2atmpS2075;
    moonbit_decref(_M0L6bufferS347);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS329,
  uint32_t _M0L3numS323,
  int32_t _M0L12digit__startS321,
  int32_t _M0L10total__lenS320,
  int32_t _M0L5radixS325
) {
  int32_t _M0Lm6offsetS319;
  uint32_t _M0Lm1nS322;
  uint32_t _M0L4baseS324;
  int32_t _M0L6_2atmpS2005;
  int32_t _M0L6_2atmpS2004;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS319 = _M0L10total__lenS320 - _M0L12digit__startS321;
  _M0Lm1nS322 = _M0L3numS323;
  _M0L4baseS324 = *(uint32_t*)&_M0L5radixS325;
  _M0L6_2atmpS2005 = _M0L5radixS325 - 1;
  _M0L6_2atmpS2004 = _M0L5radixS325 & _M0L6_2atmpS2005;
  if (_M0L6_2atmpS2004 == 0) {
    int32_t _M0L5shiftS326;
    uint32_t _M0L4maskS327;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS326 = moonbit_ctz32(_M0L5radixS325);
    _M0L4maskS327 = _M0L4baseS324 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS2006 = _M0Lm1nS322;
      if (_M0L6_2atmpS2006 > 0u) {
        int32_t _M0L6_2atmpS2007 = _M0Lm6offsetS319;
        uint32_t _M0L6_2atmpS2013;
        uint32_t _M0L6_2atmpS2012;
        int32_t _M0L5digitS328;
        int32_t _M0L6_2atmpS2010;
        int32_t _M0L6_2atmpS2008;
        int32_t _M0L6_2atmpS2009;
        uint32_t _M0L6_2atmpS2011;
        _M0Lm6offsetS319 = _M0L6_2atmpS2007 - 1;
        _M0L6_2atmpS2013 = _M0Lm1nS322;
        _M0L6_2atmpS2012 = _M0L6_2atmpS2013 & _M0L4maskS327;
        _M0L5digitS328 = *(int32_t*)&_M0L6_2atmpS2012;
        _M0L6_2atmpS2010 = _M0Lm6offsetS319;
        _M0L6_2atmpS2008 = _M0L12digit__startS321 + _M0L6_2atmpS2010;
        _M0L6_2atmpS2009
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS328
        ];
        _M0L6bufferS329[_M0L6_2atmpS2008] = _M0L6_2atmpS2009;
        _M0L6_2atmpS2011 = _M0Lm1nS322;
        _M0Lm1nS322 = _M0L6_2atmpS2011 >> (_M0L5shiftS326 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS329);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS2014 = _M0Lm1nS322;
      if (_M0L6_2atmpS2014 > 0u) {
        int32_t _M0L6_2atmpS2015 = _M0Lm6offsetS319;
        uint32_t _M0L6_2atmpS2022;
        uint32_t _M0L1qS331;
        uint32_t _M0L6_2atmpS2020;
        uint32_t _M0L6_2atmpS2021;
        uint32_t _M0L6_2atmpS2019;
        int32_t _M0L5digitS332;
        int32_t _M0L6_2atmpS2018;
        int32_t _M0L6_2atmpS2016;
        int32_t _M0L6_2atmpS2017;
        _M0Lm6offsetS319 = _M0L6_2atmpS2015 - 1;
        _M0L6_2atmpS2022 = _M0Lm1nS322;
        _M0L1qS331 = _M0L6_2atmpS2022 / _M0L4baseS324;
        _M0L6_2atmpS2020 = _M0Lm1nS322;
        _M0L6_2atmpS2021 = _M0L1qS331 * _M0L4baseS324;
        _M0L6_2atmpS2019 = _M0L6_2atmpS2020 - _M0L6_2atmpS2021;
        _M0L5digitS332 = *(int32_t*)&_M0L6_2atmpS2019;
        _M0L6_2atmpS2018 = _M0Lm6offsetS319;
        _M0L6_2atmpS2016 = _M0L12digit__startS321 + _M0L6_2atmpS2018;
        _M0L6_2atmpS2017
        = ((moonbit_string_t)moonbit_string_literal_106.data)[
          _M0L5digitS332
        ];
        _M0L6bufferS329[_M0L6_2atmpS2016] = _M0L6_2atmpS2017;
        _M0Lm1nS322 = _M0L1qS331;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS329);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS316,
  uint32_t _M0L3numS312,
  int32_t _M0L12digit__startS310,
  int32_t _M0L10total__lenS309
) {
  int32_t _M0Lm6offsetS308;
  uint32_t _M0Lm1nS311;
  int32_t _M0L6_2atmpS2000;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS308 = _M0L10total__lenS309 - _M0L12digit__startS310;
  _M0Lm1nS311 = _M0L3numS312;
  while (1) {
    int32_t _M0L6_2atmpS1988 = _M0Lm6offsetS308;
    if (_M0L6_2atmpS1988 >= 2) {
      int32_t _M0L6_2atmpS1989 = _M0Lm6offsetS308;
      uint32_t _M0L6_2atmpS1999;
      uint32_t _M0L6_2atmpS1998;
      int32_t _M0L9byte__valS313;
      int32_t _M0L2hiS314;
      int32_t _M0L2loS315;
      int32_t _M0L6_2atmpS1992;
      int32_t _M0L6_2atmpS1990;
      int32_t _M0L6_2atmpS1991;
      int32_t _M0L6_2atmpS1996;
      int32_t _M0L6_2atmpS1995;
      int32_t _M0L6_2atmpS1993;
      int32_t _M0L6_2atmpS1994;
      uint32_t _M0L6_2atmpS1997;
      _M0Lm6offsetS308 = _M0L6_2atmpS1989 - 2;
      _M0L6_2atmpS1999 = _M0Lm1nS311;
      _M0L6_2atmpS1998 = _M0L6_2atmpS1999 & 255u;
      _M0L9byte__valS313 = *(int32_t*)&_M0L6_2atmpS1998;
      _M0L2hiS314 = _M0L9byte__valS313 / 16;
      _M0L2loS315 = _M0L9byte__valS313 % 16;
      _M0L6_2atmpS1992 = _M0Lm6offsetS308;
      _M0L6_2atmpS1990 = _M0L12digit__startS310 + _M0L6_2atmpS1992;
      _M0L6_2atmpS1991
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2hiS314
      ];
      _M0L6bufferS316[_M0L6_2atmpS1990] = _M0L6_2atmpS1991;
      _M0L6_2atmpS1996 = _M0Lm6offsetS308;
      _M0L6_2atmpS1995 = _M0L12digit__startS310 + _M0L6_2atmpS1996;
      _M0L6_2atmpS1993 = _M0L6_2atmpS1995 + 1;
      _M0L6_2atmpS1994
      = ((moonbit_string_t)moonbit_string_literal_106.data)[
        _M0L2loS315
      ];
      _M0L6bufferS316[_M0L6_2atmpS1993] = _M0L6_2atmpS1994;
      _M0L6_2atmpS1997 = _M0Lm1nS311;
      _M0Lm1nS311 = _M0L6_2atmpS1997 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2000 = _M0Lm6offsetS308;
  if (_M0L6_2atmpS2000 == 1) {
    uint32_t _M0L6_2atmpS2003 = _M0Lm1nS311;
    uint32_t _M0L6_2atmpS2002 = _M0L6_2atmpS2003 & 15u;
    int32_t _M0L6nibbleS318 = *(int32_t*)&_M0L6_2atmpS2002;
    int32_t _M0L6_2atmpS2001 =
      ((moonbit_string_t)moonbit_string_literal_106.data)[_M0L6nibbleS318];
    _M0L6bufferS316[_M0L12digit__startS310] = _M0L6_2atmpS2001;
    moonbit_decref(_M0L6bufferS316);
  } else {
    moonbit_decref(_M0L6bufferS316);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS303) {
  struct _M0TWEOs* _M0L7_2afuncS302;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS302 = _M0L4selfS303;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS302->code(_M0L7_2afuncS302);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS305
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS304;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS304 = _M0L4selfS305;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS304->code(_M0L7_2afuncS304);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS307) {
  struct _M0TWEOc* _M0L7_2afuncS306;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS306 = _M0L4selfS307;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS306->code(_M0L7_2afuncS306);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS295
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS294;
  struct _M0TPB6Logger _M0L6_2atmpS1984;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS294 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS294);
  _M0L6_2atmpS1984
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS294
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS295, _M0L6_2atmpS1984);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS294);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS297
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS296;
  struct _M0TPB6Logger _M0L6_2atmpS1985;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS296 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS296);
  _M0L6_2atmpS1985
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS296
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS297, _M0L6_2atmpS1985);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS296);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS299
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS298;
  struct _M0TPB6Logger _M0L6_2atmpS1986;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS298 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS298);
  _M0L6_2atmpS1986
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS298
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS299, _M0L6_2atmpS1986);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS298);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS301
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS300;
  struct _M0TPB6Logger _M0L6_2atmpS1987;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS300 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS300);
  _M0L6_2atmpS1987
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS300
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS301, _M0L6_2atmpS1987);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS300);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS293
) {
  int32_t _M0L8_2afieldS4288;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4288 = _M0L4selfS293.$1;
  moonbit_decref(_M0L4selfS293.$0);
  return _M0L8_2afieldS4288;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS292
) {
  int32_t _M0L3endS1982;
  int32_t _M0L8_2afieldS4289;
  int32_t _M0L5startS1983;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1982 = _M0L4selfS292.$2;
  _M0L8_2afieldS4289 = _M0L4selfS292.$1;
  moonbit_decref(_M0L4selfS292.$0);
  _M0L5startS1983 = _M0L8_2afieldS4289;
  return _M0L3endS1982 - _M0L5startS1983;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS291
) {
  moonbit_string_t _M0L8_2afieldS4290;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4290 = _M0L4selfS291.$0;
  return _M0L8_2afieldS4290;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS287,
  moonbit_string_t _M0L5valueS288,
  int32_t _M0L5startS289,
  int32_t _M0L3lenS290
) {
  int32_t _M0L6_2atmpS1981;
  int64_t _M0L6_2atmpS1980;
  struct _M0TPC16string10StringView _M0L6_2atmpS1979;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1981 = _M0L5startS289 + _M0L3lenS290;
  _M0L6_2atmpS1980 = (int64_t)_M0L6_2atmpS1981;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1979
  = _M0MPC16string6String11sub_2einner(_M0L5valueS288, _M0L5startS289, _M0L6_2atmpS1980);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS287, _M0L6_2atmpS1979);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS280,
  int32_t _M0L5startS286,
  int64_t _M0L3endS282
) {
  int32_t _M0L3lenS279;
  int32_t _M0L3endS281;
  int32_t _M0L5startS285;
  int32_t _if__result_4724;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS279 = Moonbit_array_length(_M0L4selfS280);
  if (_M0L3endS282 == 4294967296ll) {
    _M0L3endS281 = _M0L3lenS279;
  } else {
    int64_t _M0L7_2aSomeS283 = _M0L3endS282;
    int32_t _M0L6_2aendS284 = (int32_t)_M0L7_2aSomeS283;
    if (_M0L6_2aendS284 < 0) {
      _M0L3endS281 = _M0L3lenS279 + _M0L6_2aendS284;
    } else {
      _M0L3endS281 = _M0L6_2aendS284;
    }
  }
  if (_M0L5startS286 < 0) {
    _M0L5startS285 = _M0L3lenS279 + _M0L5startS286;
  } else {
    _M0L5startS285 = _M0L5startS286;
  }
  if (_M0L5startS285 >= 0) {
    if (_M0L5startS285 <= _M0L3endS281) {
      _if__result_4724 = _M0L3endS281 <= _M0L3lenS279;
    } else {
      _if__result_4724 = 0;
    }
  } else {
    _if__result_4724 = 0;
  }
  if (_if__result_4724) {
    if (_M0L5startS285 < _M0L3lenS279) {
      int32_t _M0L6_2atmpS1976 = _M0L4selfS280[_M0L5startS285];
      int32_t _M0L6_2atmpS1975;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1975
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1976);
      if (!_M0L6_2atmpS1975) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS281 < _M0L3lenS279) {
      int32_t _M0L6_2atmpS1978 = _M0L4selfS280[_M0L3endS281];
      int32_t _M0L6_2atmpS1977;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1977
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1978);
      if (!_M0L6_2atmpS1977) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS285,
                                                 _M0L3endS281,
                                                 _M0L4selfS280};
  } else {
    moonbit_decref(_M0L4selfS280);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS276) {
  struct _M0TPB6Hasher* _M0L1hS275;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS275 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS275);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS275, _M0L4selfS276);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS275);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS278
) {
  struct _M0TPB6Hasher* _M0L1hS277;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS277 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS277);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS277, _M0L4selfS278);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS277);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS273) {
  int32_t _M0L4seedS272;
  if (_M0L10seed_2eoptS273 == 4294967296ll) {
    _M0L4seedS272 = 0;
  } else {
    int64_t _M0L7_2aSomeS274 = _M0L10seed_2eoptS273;
    _M0L4seedS272 = (int32_t)_M0L7_2aSomeS274;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS272);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS271) {
  uint32_t _M0L6_2atmpS1974;
  uint32_t _M0L6_2atmpS1973;
  struct _M0TPB6Hasher* _block_4725;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1974 = *(uint32_t*)&_M0L4seedS271;
  _M0L6_2atmpS1973 = _M0L6_2atmpS1974 + 374761393u;
  _block_4725
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4725)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4725->$0 = _M0L6_2atmpS1973;
  return _block_4725;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS270) {
  uint32_t _M0L6_2atmpS1972;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1972 = _M0MPB6Hasher9avalanche(_M0L4selfS270);
  return *(int32_t*)&_M0L6_2atmpS1972;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS269) {
  uint32_t _M0L8_2afieldS4291;
  uint32_t _M0Lm3accS268;
  uint32_t _M0L6_2atmpS1961;
  uint32_t _M0L6_2atmpS1963;
  uint32_t _M0L6_2atmpS1962;
  uint32_t _M0L6_2atmpS1964;
  uint32_t _M0L6_2atmpS1965;
  uint32_t _M0L6_2atmpS1967;
  uint32_t _M0L6_2atmpS1966;
  uint32_t _M0L6_2atmpS1968;
  uint32_t _M0L6_2atmpS1969;
  uint32_t _M0L6_2atmpS1971;
  uint32_t _M0L6_2atmpS1970;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4291 = _M0L4selfS269->$0;
  moonbit_decref(_M0L4selfS269);
  _M0Lm3accS268 = _M0L8_2afieldS4291;
  _M0L6_2atmpS1961 = _M0Lm3accS268;
  _M0L6_2atmpS1963 = _M0Lm3accS268;
  _M0L6_2atmpS1962 = _M0L6_2atmpS1963 >> 15;
  _M0Lm3accS268 = _M0L6_2atmpS1961 ^ _M0L6_2atmpS1962;
  _M0L6_2atmpS1964 = _M0Lm3accS268;
  _M0Lm3accS268 = _M0L6_2atmpS1964 * 2246822519u;
  _M0L6_2atmpS1965 = _M0Lm3accS268;
  _M0L6_2atmpS1967 = _M0Lm3accS268;
  _M0L6_2atmpS1966 = _M0L6_2atmpS1967 >> 13;
  _M0Lm3accS268 = _M0L6_2atmpS1965 ^ _M0L6_2atmpS1966;
  _M0L6_2atmpS1968 = _M0Lm3accS268;
  _M0Lm3accS268 = _M0L6_2atmpS1968 * 3266489917u;
  _M0L6_2atmpS1969 = _M0Lm3accS268;
  _M0L6_2atmpS1971 = _M0Lm3accS268;
  _M0L6_2atmpS1970 = _M0L6_2atmpS1971 >> 16;
  _M0Lm3accS268 = _M0L6_2atmpS1969 ^ _M0L6_2atmpS1970;
  return _M0Lm3accS268;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS266,
  moonbit_string_t _M0L1yS267
) {
  int32_t _M0L6_2atmpS4292;
  int32_t _M0L6_2atmpS1960;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4292 = moonbit_val_array_equal(_M0L1xS266, _M0L1yS267);
  moonbit_decref(_M0L1xS266);
  moonbit_decref(_M0L1yS267);
  _M0L6_2atmpS1960 = _M0L6_2atmpS4292;
  return !_M0L6_2atmpS1960;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS263,
  int32_t _M0L5valueS262
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS262, _M0L4selfS263);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS265,
  moonbit_string_t _M0L5valueS264
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS264, _M0L4selfS265);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS261) {
  int64_t _M0L6_2atmpS1959;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1959 = (int64_t)_M0L4selfS261;
  return *(uint64_t*)&_M0L6_2atmpS1959;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS259,
  int32_t _M0L5valueS260
) {
  uint32_t _M0L6_2atmpS1958;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1958 = *(uint32_t*)&_M0L5valueS260;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS259, _M0L6_2atmpS1958);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS252
) {
  struct _M0TPB13StringBuilder* _M0L3bufS250;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS251;
  int32_t _M0L7_2abindS253;
  int32_t _M0L1iS254;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS250 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS251 = _M0L4selfS252;
  moonbit_incref(_M0L3bufS250);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS250, 91);
  _M0L7_2abindS253 = _M0L7_2aselfS251->$1;
  _M0L1iS254 = 0;
  while (1) {
    if (_M0L1iS254 < _M0L7_2abindS253) {
      int32_t _if__result_4727;
      moonbit_string_t* _M0L8_2afieldS4294;
      moonbit_string_t* _M0L3bufS1956;
      moonbit_string_t _M0L6_2atmpS4293;
      moonbit_string_t _M0L4itemS255;
      int32_t _M0L6_2atmpS1957;
      if (_M0L1iS254 != 0) {
        moonbit_incref(_M0L3bufS250);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, (moonbit_string_t)moonbit_string_literal_108.data);
      }
      if (_M0L1iS254 < 0) {
        _if__result_4727 = 1;
      } else {
        int32_t _M0L3lenS1955 = _M0L7_2aselfS251->$1;
        _if__result_4727 = _M0L1iS254 >= _M0L3lenS1955;
      }
      if (_if__result_4727) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4294 = _M0L7_2aselfS251->$0;
      _M0L3bufS1956 = _M0L8_2afieldS4294;
      _M0L6_2atmpS4293 = (moonbit_string_t)_M0L3bufS1956[_M0L1iS254];
      _M0L4itemS255 = _M0L6_2atmpS4293;
      if (_M0L4itemS255 == 0) {
        moonbit_incref(_M0L3bufS250);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, (moonbit_string_t)moonbit_string_literal_67.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS256 = _M0L4itemS255;
        moonbit_string_t _M0L6_2alocS257 = _M0L7_2aSomeS256;
        moonbit_string_t _M0L6_2atmpS1954;
        moonbit_incref(_M0L6_2alocS257);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1954
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS257);
        moonbit_incref(_M0L3bufS250);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS250, _M0L6_2atmpS1954);
      }
      _M0L6_2atmpS1957 = _M0L1iS254 + 1;
      _M0L1iS254 = _M0L6_2atmpS1957;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS251);
    }
    break;
  }
  moonbit_incref(_M0L3bufS250);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS250, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS250);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS249
) {
  moonbit_string_t _M0L6_2atmpS1953;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1952;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1953 = _M0L4selfS249;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1952 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1953);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1952);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L2sbS247;
  struct _M0TPC16string10StringView _M0L8_2afieldS4307;
  struct _M0TPC16string10StringView _M0L3pkgS1937;
  moonbit_string_t _M0L6_2atmpS1936;
  moonbit_string_t _M0L6_2atmpS4306;
  moonbit_string_t _M0L6_2atmpS1935;
  moonbit_string_t _M0L6_2atmpS4305;
  moonbit_string_t _M0L6_2atmpS1934;
  struct _M0TPC16string10StringView _M0L8_2afieldS4304;
  struct _M0TPC16string10StringView _M0L8filenameS1938;
  struct _M0TPC16string10StringView _M0L8_2afieldS4303;
  struct _M0TPC16string10StringView _M0L11start__lineS1941;
  moonbit_string_t _M0L6_2atmpS1940;
  moonbit_string_t _M0L6_2atmpS4302;
  moonbit_string_t _M0L6_2atmpS1939;
  struct _M0TPC16string10StringView _M0L8_2afieldS4301;
  struct _M0TPC16string10StringView _M0L13start__columnS1944;
  moonbit_string_t _M0L6_2atmpS1943;
  moonbit_string_t _M0L6_2atmpS4300;
  moonbit_string_t _M0L6_2atmpS1942;
  struct _M0TPC16string10StringView _M0L8_2afieldS4299;
  struct _M0TPC16string10StringView _M0L9end__lineS1947;
  moonbit_string_t _M0L6_2atmpS1946;
  moonbit_string_t _M0L6_2atmpS4298;
  moonbit_string_t _M0L6_2atmpS1945;
  struct _M0TPC16string10StringView _M0L8_2afieldS4297;
  int32_t _M0L6_2acntS4521;
  struct _M0TPC16string10StringView _M0L11end__columnS1951;
  moonbit_string_t _M0L6_2atmpS1950;
  moonbit_string_t _M0L6_2atmpS4296;
  moonbit_string_t _M0L6_2atmpS1949;
  moonbit_string_t _M0L6_2atmpS4295;
  moonbit_string_t _M0L6_2atmpS1948;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS247 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4307
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$0_1, _M0L4selfS248->$0_2, _M0L4selfS248->$0_0
  };
  _M0L3pkgS1937 = _M0L8_2afieldS4307;
  moonbit_incref(_M0L3pkgS1937.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1936
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1937);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4306
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_109.data, _M0L6_2atmpS1936);
  moonbit_decref(_M0L6_2atmpS1936);
  _M0L6_2atmpS1935 = _M0L6_2atmpS4306;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4305
  = moonbit_add_string(_M0L6_2atmpS1935, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS1935);
  _M0L6_2atmpS1934 = _M0L6_2atmpS4305;
  moonbit_incref(_M0L2sbS247);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1934);
  moonbit_incref(_M0L2sbS247);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, (moonbit_string_t)moonbit_string_literal_111.data);
  _M0L8_2afieldS4304
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$1_1, _M0L4selfS248->$1_2, _M0L4selfS248->$1_0
  };
  _M0L8filenameS1938 = _M0L8_2afieldS4304;
  moonbit_incref(_M0L8filenameS1938.$0);
  moonbit_incref(_M0L2sbS247);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS247, _M0L8filenameS1938);
  _M0L8_2afieldS4303
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$2_1, _M0L4selfS248->$2_2, _M0L4selfS248->$2_0
  };
  _M0L11start__lineS1941 = _M0L8_2afieldS4303;
  moonbit_incref(_M0L11start__lineS1941.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1940
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1941);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4302
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_112.data, _M0L6_2atmpS1940);
  moonbit_decref(_M0L6_2atmpS1940);
  _M0L6_2atmpS1939 = _M0L6_2atmpS4302;
  moonbit_incref(_M0L2sbS247);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1939);
  _M0L8_2afieldS4301
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$3_1, _M0L4selfS248->$3_2, _M0L4selfS248->$3_0
  };
  _M0L13start__columnS1944 = _M0L8_2afieldS4301;
  moonbit_incref(_M0L13start__columnS1944.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1943
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1944);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4300
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_113.data, _M0L6_2atmpS1943);
  moonbit_decref(_M0L6_2atmpS1943);
  _M0L6_2atmpS1942 = _M0L6_2atmpS4300;
  moonbit_incref(_M0L2sbS247);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1942);
  _M0L8_2afieldS4299
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$4_1, _M0L4selfS248->$4_2, _M0L4selfS248->$4_0
  };
  _M0L9end__lineS1947 = _M0L8_2afieldS4299;
  moonbit_incref(_M0L9end__lineS1947.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1946
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1947);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4298
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_114.data, _M0L6_2atmpS1946);
  moonbit_decref(_M0L6_2atmpS1946);
  _M0L6_2atmpS1945 = _M0L6_2atmpS4298;
  moonbit_incref(_M0L2sbS247);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1945);
  _M0L8_2afieldS4297
  = (struct _M0TPC16string10StringView){
    _M0L4selfS248->$5_1, _M0L4selfS248->$5_2, _M0L4selfS248->$5_0
  };
  _M0L6_2acntS4521 = Moonbit_object_header(_M0L4selfS248)->rc;
  if (_M0L6_2acntS4521 > 1) {
    int32_t _M0L11_2anew__cntS4527 = _M0L6_2acntS4521 - 1;
    Moonbit_object_header(_M0L4selfS248)->rc = _M0L11_2anew__cntS4527;
    moonbit_incref(_M0L8_2afieldS4297.$0);
  } else if (_M0L6_2acntS4521 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4526 =
      (struct _M0TPC16string10StringView){_M0L4selfS248->$4_1,
                                            _M0L4selfS248->$4_2,
                                            _M0L4selfS248->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4525;
    struct _M0TPC16string10StringView _M0L8_2afieldS4524;
    struct _M0TPC16string10StringView _M0L8_2afieldS4523;
    struct _M0TPC16string10StringView _M0L8_2afieldS4522;
    moonbit_decref(_M0L8_2afieldS4526.$0);
    _M0L8_2afieldS4525
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$3_1, _M0L4selfS248->$3_2, _M0L4selfS248->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4525.$0);
    _M0L8_2afieldS4524
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$2_1, _M0L4selfS248->$2_2, _M0L4selfS248->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4524.$0);
    _M0L8_2afieldS4523
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$1_1, _M0L4selfS248->$1_2, _M0L4selfS248->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4523.$0);
    _M0L8_2afieldS4522
    = (struct _M0TPC16string10StringView){
      _M0L4selfS248->$0_1, _M0L4selfS248->$0_2, _M0L4selfS248->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4522.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS248);
  }
  _M0L11end__columnS1951 = _M0L8_2afieldS4297;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1950
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1951);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4296
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_115.data, _M0L6_2atmpS1950);
  moonbit_decref(_M0L6_2atmpS1950);
  _M0L6_2atmpS1949 = _M0L6_2atmpS4296;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4295
  = moonbit_add_string(_M0L6_2atmpS1949, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1949);
  _M0L6_2atmpS1948 = _M0L6_2atmpS4295;
  moonbit_incref(_M0L2sbS247);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS247, _M0L6_2atmpS1948);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS247);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS245,
  moonbit_string_t _M0L3strS246
) {
  int32_t _M0L3lenS1924;
  int32_t _M0L6_2atmpS1926;
  int32_t _M0L6_2atmpS1925;
  int32_t _M0L6_2atmpS1923;
  moonbit_bytes_t _M0L8_2afieldS4309;
  moonbit_bytes_t _M0L4dataS1927;
  int32_t _M0L3lenS1928;
  int32_t _M0L6_2atmpS1929;
  int32_t _M0L3lenS1931;
  int32_t _M0L6_2atmpS4308;
  int32_t _M0L6_2atmpS1933;
  int32_t _M0L6_2atmpS1932;
  int32_t _M0L6_2atmpS1930;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1924 = _M0L4selfS245->$1;
  _M0L6_2atmpS1926 = Moonbit_array_length(_M0L3strS246);
  _M0L6_2atmpS1925 = _M0L6_2atmpS1926 * 2;
  _M0L6_2atmpS1923 = _M0L3lenS1924 + _M0L6_2atmpS1925;
  moonbit_incref(_M0L4selfS245);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS245, _M0L6_2atmpS1923);
  _M0L8_2afieldS4309 = _M0L4selfS245->$0;
  _M0L4dataS1927 = _M0L8_2afieldS4309;
  _M0L3lenS1928 = _M0L4selfS245->$1;
  _M0L6_2atmpS1929 = Moonbit_array_length(_M0L3strS246);
  moonbit_incref(_M0L4dataS1927);
  moonbit_incref(_M0L3strS246);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1927, _M0L3lenS1928, _M0L3strS246, 0, _M0L6_2atmpS1929);
  _M0L3lenS1931 = _M0L4selfS245->$1;
  _M0L6_2atmpS4308 = Moonbit_array_length(_M0L3strS246);
  moonbit_decref(_M0L3strS246);
  _M0L6_2atmpS1933 = _M0L6_2atmpS4308;
  _M0L6_2atmpS1932 = _M0L6_2atmpS1933 * 2;
  _M0L6_2atmpS1930 = _M0L3lenS1931 + _M0L6_2atmpS1932;
  _M0L4selfS245->$1 = _M0L6_2atmpS1930;
  moonbit_decref(_M0L4selfS245);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS237,
  int32_t _M0L13bytes__offsetS232,
  moonbit_string_t _M0L3strS239,
  int32_t _M0L11str__offsetS235,
  int32_t _M0L6lengthS233
) {
  int32_t _M0L6_2atmpS1922;
  int32_t _M0L6_2atmpS1921;
  int32_t _M0L2e1S231;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L2e2S234;
  int32_t _M0L4len1S236;
  int32_t _M0L4len2S238;
  int32_t _if__result_4728;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1922 = _M0L6lengthS233 * 2;
  _M0L6_2atmpS1921 = _M0L13bytes__offsetS232 + _M0L6_2atmpS1922;
  _M0L2e1S231 = _M0L6_2atmpS1921 - 1;
  _M0L6_2atmpS1920 = _M0L11str__offsetS235 + _M0L6lengthS233;
  _M0L2e2S234 = _M0L6_2atmpS1920 - 1;
  _M0L4len1S236 = Moonbit_array_length(_M0L4selfS237);
  _M0L4len2S238 = Moonbit_array_length(_M0L3strS239);
  if (_M0L6lengthS233 >= 0) {
    if (_M0L13bytes__offsetS232 >= 0) {
      if (_M0L2e1S231 < _M0L4len1S236) {
        if (_M0L11str__offsetS235 >= 0) {
          _if__result_4728 = _M0L2e2S234 < _M0L4len2S238;
        } else {
          _if__result_4728 = 0;
        }
      } else {
        _if__result_4728 = 0;
      }
    } else {
      _if__result_4728 = 0;
    }
  } else {
    _if__result_4728 = 0;
  }
  if (_if__result_4728) {
    int32_t _M0L16end__str__offsetS240 =
      _M0L11str__offsetS235 + _M0L6lengthS233;
    int32_t _M0L1iS241 = _M0L11str__offsetS235;
    int32_t _M0L1jS242 = _M0L13bytes__offsetS232;
    while (1) {
      if (_M0L1iS241 < _M0L16end__str__offsetS240) {
        int32_t _M0L6_2atmpS1917 = _M0L3strS239[_M0L1iS241];
        int32_t _M0L6_2atmpS1916 = (int32_t)_M0L6_2atmpS1917;
        uint32_t _M0L1cS243 = *(uint32_t*)&_M0L6_2atmpS1916;
        uint32_t _M0L6_2atmpS1912 = _M0L1cS243 & 255u;
        int32_t _M0L6_2atmpS1911;
        int32_t _M0L6_2atmpS1913;
        uint32_t _M0L6_2atmpS1915;
        int32_t _M0L6_2atmpS1914;
        int32_t _M0L6_2atmpS1918;
        int32_t _M0L6_2atmpS1919;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1911 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1912);
        if (
          _M0L1jS242 < 0 || _M0L1jS242 >= Moonbit_array_length(_M0L4selfS237)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS237[_M0L1jS242] = _M0L6_2atmpS1911;
        _M0L6_2atmpS1913 = _M0L1jS242 + 1;
        _M0L6_2atmpS1915 = _M0L1cS243 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1914 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1915);
        if (
          _M0L6_2atmpS1913 < 0
          || _M0L6_2atmpS1913 >= Moonbit_array_length(_M0L4selfS237)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS237[_M0L6_2atmpS1913] = _M0L6_2atmpS1914;
        _M0L6_2atmpS1918 = _M0L1iS241 + 1;
        _M0L6_2atmpS1919 = _M0L1jS242 + 2;
        _M0L1iS241 = _M0L6_2atmpS1918;
        _M0L1jS242 = _M0L6_2atmpS1919;
        continue;
      } else {
        moonbit_decref(_M0L3strS239);
        moonbit_decref(_M0L4selfS237);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS239);
    moonbit_decref(_M0L4selfS237);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS228,
  double _M0L3objS227
) {
  struct _M0TPB6Logger _M0L6_2atmpS1909;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1909
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS228
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS227, _M0L6_2atmpS1909);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS230,
  struct _M0TPC16string10StringView _M0L3objS229
) {
  struct _M0TPB6Logger _M0L6_2atmpS1910;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1910
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS230
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS229, _M0L6_2atmpS1910);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS173
) {
  int32_t _M0L6_2atmpS1908;
  struct _M0TPC16string10StringView _M0L7_2abindS172;
  moonbit_string_t _M0L7_2adataS174;
  int32_t _M0L8_2astartS175;
  int32_t _M0L6_2atmpS1907;
  int32_t _M0L6_2aendS176;
  int32_t _M0Lm9_2acursorS177;
  int32_t _M0Lm13accept__stateS178;
  int32_t _M0Lm10match__endS179;
  int32_t _M0Lm20match__tag__saver__0S180;
  int32_t _M0Lm20match__tag__saver__1S181;
  int32_t _M0Lm20match__tag__saver__2S182;
  int32_t _M0Lm20match__tag__saver__3S183;
  int32_t _M0Lm20match__tag__saver__4S184;
  int32_t _M0Lm6tag__0S185;
  int32_t _M0Lm6tag__1S186;
  int32_t _M0Lm9tag__1__1S187;
  int32_t _M0Lm9tag__1__2S188;
  int32_t _M0Lm6tag__3S189;
  int32_t _M0Lm6tag__2S190;
  int32_t _M0Lm9tag__2__1S191;
  int32_t _M0Lm6tag__4S192;
  int32_t _M0L6_2atmpS1865;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1908 = Moonbit_array_length(_M0L4reprS173);
  _M0L7_2abindS172
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1908, _M0L4reprS173
  };
  moonbit_incref(_M0L7_2abindS172.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS174 = _M0MPC16string10StringView4data(_M0L7_2abindS172);
  moonbit_incref(_M0L7_2abindS172.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS175
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS172);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1907 = _M0MPC16string10StringView6length(_M0L7_2abindS172);
  _M0L6_2aendS176 = _M0L8_2astartS175 + _M0L6_2atmpS1907;
  _M0Lm9_2acursorS177 = _M0L8_2astartS175;
  _M0Lm13accept__stateS178 = -1;
  _M0Lm10match__endS179 = -1;
  _M0Lm20match__tag__saver__0S180 = -1;
  _M0Lm20match__tag__saver__1S181 = -1;
  _M0Lm20match__tag__saver__2S182 = -1;
  _M0Lm20match__tag__saver__3S183 = -1;
  _M0Lm20match__tag__saver__4S184 = -1;
  _M0Lm6tag__0S185 = -1;
  _M0Lm6tag__1S186 = -1;
  _M0Lm9tag__1__1S187 = -1;
  _M0Lm9tag__1__2S188 = -1;
  _M0Lm6tag__3S189 = -1;
  _M0Lm6tag__2S190 = -1;
  _M0Lm9tag__2__1S191 = -1;
  _M0Lm6tag__4S192 = -1;
  _M0L6_2atmpS1865 = _M0Lm9_2acursorS177;
  if (_M0L6_2atmpS1865 < _M0L6_2aendS176) {
    int32_t _M0L6_2atmpS1867 = _M0Lm9_2acursorS177;
    int32_t _M0L6_2atmpS1866;
    moonbit_incref(_M0L7_2adataS174);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1866
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1867);
    if (_M0L6_2atmpS1866 == 64) {
      int32_t _M0L6_2atmpS1868 = _M0Lm9_2acursorS177;
      _M0Lm9_2acursorS177 = _M0L6_2atmpS1868 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1869;
        _M0Lm6tag__0S185 = _M0Lm9_2acursorS177;
        _M0L6_2atmpS1869 = _M0Lm9_2acursorS177;
        if (_M0L6_2atmpS1869 < _M0L6_2aendS176) {
          int32_t _M0L6_2atmpS1906 = _M0Lm9_2acursorS177;
          int32_t _M0L10next__charS200;
          int32_t _M0L6_2atmpS1870;
          moonbit_incref(_M0L7_2adataS174);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS200
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1906);
          _M0L6_2atmpS1870 = _M0Lm9_2acursorS177;
          _M0Lm9_2acursorS177 = _M0L6_2atmpS1870 + 1;
          if (_M0L10next__charS200 == 58) {
            int32_t _M0L6_2atmpS1871 = _M0Lm9_2acursorS177;
            if (_M0L6_2atmpS1871 < _M0L6_2aendS176) {
              int32_t _M0L6_2atmpS1872 = _M0Lm9_2acursorS177;
              int32_t _M0L12dispatch__15S201;
              _M0Lm9_2acursorS177 = _M0L6_2atmpS1872 + 1;
              _M0L12dispatch__15S201 = 0;
              loop__label__15_204:;
              while (1) {
                int32_t _M0L6_2atmpS1873;
                switch (_M0L12dispatch__15S201) {
                  case 3: {
                    int32_t _M0L6_2atmpS1876;
                    _M0Lm9tag__1__2S188 = _M0Lm9tag__1__1S187;
                    _M0Lm9tag__1__1S187 = _M0Lm6tag__1S186;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1876 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1876 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1881 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS208;
                      int32_t _M0L6_2atmpS1877;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS208
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1881);
                      _M0L6_2atmpS1877 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1877 + 1;
                      if (_M0L10next__charS208 < 58) {
                        if (_M0L10next__charS208 < 48) {
                          goto join_207;
                        } else {
                          int32_t _M0L6_2atmpS1878;
                          _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                          _M0Lm9tag__2__1S191 = _M0Lm6tag__2S190;
                          _M0Lm6tag__2S190 = _M0Lm9_2acursorS177;
                          _M0Lm6tag__3S189 = _M0Lm9_2acursorS177;
                          _M0L6_2atmpS1878 = _M0Lm9_2acursorS177;
                          if (_M0L6_2atmpS1878 < _M0L6_2aendS176) {
                            int32_t _M0L6_2atmpS1880 = _M0Lm9_2acursorS177;
                            int32_t _M0L10next__charS210;
                            int32_t _M0L6_2atmpS1879;
                            moonbit_incref(_M0L7_2adataS174);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS210
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1880);
                            _M0L6_2atmpS1879 = _M0Lm9_2acursorS177;
                            _M0Lm9_2acursorS177 = _M0L6_2atmpS1879 + 1;
                            if (_M0L10next__charS210 < 48) {
                              if (_M0L10next__charS210 == 45) {
                                goto join_202;
                              } else {
                                goto join_209;
                              }
                            } else if (_M0L10next__charS210 > 57) {
                              if (_M0L10next__charS210 < 59) {
                                _M0L12dispatch__15S201 = 3;
                                goto loop__label__15_204;
                              } else {
                                goto join_209;
                              }
                            } else {
                              _M0L12dispatch__15S201 = 6;
                              goto loop__label__15_204;
                            }
                            join_209:;
                            _M0L12dispatch__15S201 = 0;
                            goto loop__label__15_204;
                          } else {
                            goto join_193;
                          }
                        }
                      } else if (_M0L10next__charS208 > 58) {
                        goto join_207;
                      } else {
                        _M0L12dispatch__15S201 = 1;
                        goto loop__label__15_204;
                      }
                      join_207:;
                      _M0L12dispatch__15S201 = 0;
                      goto loop__label__15_204;
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1882;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0Lm6tag__2S190 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1882 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1882 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1884 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS212;
                      int32_t _M0L6_2atmpS1883;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS212
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1884);
                      _M0L6_2atmpS1883 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1883 + 1;
                      if (_M0L10next__charS212 < 58) {
                        if (_M0L10next__charS212 < 48) {
                          goto join_211;
                        } else {
                          _M0L12dispatch__15S201 = 2;
                          goto loop__label__15_204;
                        }
                      } else if (_M0L10next__charS212 > 58) {
                        goto join_211;
                      } else {
                        _M0L12dispatch__15S201 = 3;
                        goto loop__label__15_204;
                      }
                      join_211:;
                      _M0L12dispatch__15S201 = 0;
                      goto loop__label__15_204;
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1885;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1885 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1885 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1887 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS213;
                      int32_t _M0L6_2atmpS1886;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS213
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1887);
                      _M0L6_2atmpS1886 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1886 + 1;
                      if (_M0L10next__charS213 == 58) {
                        _M0L12dispatch__15S201 = 1;
                        goto loop__label__15_204;
                      } else {
                        _M0L12dispatch__15S201 = 0;
                        goto loop__label__15_204;
                      }
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1888;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0Lm6tag__4S192 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1888 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1888 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1896 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS215;
                      int32_t _M0L6_2atmpS1889;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS215
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1896);
                      _M0L6_2atmpS1889 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1889 + 1;
                      if (_M0L10next__charS215 < 58) {
                        if (_M0L10next__charS215 < 48) {
                          goto join_214;
                        } else {
                          _M0L12dispatch__15S201 = 4;
                          goto loop__label__15_204;
                        }
                      } else if (_M0L10next__charS215 > 58) {
                        goto join_214;
                      } else {
                        int32_t _M0L6_2atmpS1890;
                        _M0Lm9tag__1__2S188 = _M0Lm9tag__1__1S187;
                        _M0Lm9tag__1__1S187 = _M0Lm6tag__1S186;
                        _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                        _M0L6_2atmpS1890 = _M0Lm9_2acursorS177;
                        if (_M0L6_2atmpS1890 < _M0L6_2aendS176) {
                          int32_t _M0L6_2atmpS1895 = _M0Lm9_2acursorS177;
                          int32_t _M0L10next__charS217;
                          int32_t _M0L6_2atmpS1891;
                          moonbit_incref(_M0L7_2adataS174);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS217
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1895);
                          _M0L6_2atmpS1891 = _M0Lm9_2acursorS177;
                          _M0Lm9_2acursorS177 = _M0L6_2atmpS1891 + 1;
                          if (_M0L10next__charS217 < 58) {
                            if (_M0L10next__charS217 < 48) {
                              goto join_216;
                            } else {
                              int32_t _M0L6_2atmpS1892;
                              _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                              _M0Lm9tag__2__1S191 = _M0Lm6tag__2S190;
                              _M0Lm6tag__2S190 = _M0Lm9_2acursorS177;
                              _M0L6_2atmpS1892 = _M0Lm9_2acursorS177;
                              if (_M0L6_2atmpS1892 < _M0L6_2aendS176) {
                                int32_t _M0L6_2atmpS1894 =
                                  _M0Lm9_2acursorS177;
                                int32_t _M0L10next__charS219;
                                int32_t _M0L6_2atmpS1893;
                                moonbit_incref(_M0L7_2adataS174);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS219
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1894);
                                _M0L6_2atmpS1893 = _M0Lm9_2acursorS177;
                                _M0Lm9_2acursorS177 = _M0L6_2atmpS1893 + 1;
                                if (_M0L10next__charS219 < 58) {
                                  if (_M0L10next__charS219 < 48) {
                                    goto join_218;
                                  } else {
                                    _M0L12dispatch__15S201 = 5;
                                    goto loop__label__15_204;
                                  }
                                } else if (_M0L10next__charS219 > 58) {
                                  goto join_218;
                                } else {
                                  _M0L12dispatch__15S201 = 3;
                                  goto loop__label__15_204;
                                }
                                join_218:;
                                _M0L12dispatch__15S201 = 0;
                                goto loop__label__15_204;
                              } else {
                                goto join_206;
                              }
                            }
                          } else if (_M0L10next__charS217 > 58) {
                            goto join_216;
                          } else {
                            _M0L12dispatch__15S201 = 1;
                            goto loop__label__15_204;
                          }
                          join_216:;
                          _M0L12dispatch__15S201 = 0;
                          goto loop__label__15_204;
                        } else {
                          goto join_193;
                        }
                      }
                      join_214:;
                      _M0L12dispatch__15S201 = 0;
                      goto loop__label__15_204;
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1897;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0Lm6tag__2S190 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1897 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1897 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1899 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS221;
                      int32_t _M0L6_2atmpS1898;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS221
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1899);
                      _M0L6_2atmpS1898 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1898 + 1;
                      if (_M0L10next__charS221 < 58) {
                        if (_M0L10next__charS221 < 48) {
                          goto join_220;
                        } else {
                          _M0L12dispatch__15S201 = 5;
                          goto loop__label__15_204;
                        }
                      } else if (_M0L10next__charS221 > 58) {
                        goto join_220;
                      } else {
                        _M0L12dispatch__15S201 = 3;
                        goto loop__label__15_204;
                      }
                      join_220:;
                      _M0L12dispatch__15S201 = 0;
                      goto loop__label__15_204;
                    } else {
                      goto join_206;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1900;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0Lm6tag__2S190 = _M0Lm9_2acursorS177;
                    _M0Lm6tag__3S189 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1900 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1900 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1902 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS223;
                      int32_t _M0L6_2atmpS1901;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS223
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1902);
                      _M0L6_2atmpS1901 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1901 + 1;
                      if (_M0L10next__charS223 < 48) {
                        if (_M0L10next__charS223 == 45) {
                          goto join_202;
                        } else {
                          goto join_222;
                        }
                      } else if (_M0L10next__charS223 > 57) {
                        if (_M0L10next__charS223 < 59) {
                          _M0L12dispatch__15S201 = 3;
                          goto loop__label__15_204;
                        } else {
                          goto join_222;
                        }
                      } else {
                        _M0L12dispatch__15S201 = 6;
                        goto loop__label__15_204;
                      }
                      join_222:;
                      _M0L12dispatch__15S201 = 0;
                      goto loop__label__15_204;
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1903;
                    _M0Lm9tag__1__1S187 = _M0Lm6tag__1S186;
                    _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                    _M0L6_2atmpS1903 = _M0Lm9_2acursorS177;
                    if (_M0L6_2atmpS1903 < _M0L6_2aendS176) {
                      int32_t _M0L6_2atmpS1905 = _M0Lm9_2acursorS177;
                      int32_t _M0L10next__charS225;
                      int32_t _M0L6_2atmpS1904;
                      moonbit_incref(_M0L7_2adataS174);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS225
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1905);
                      _M0L6_2atmpS1904 = _M0Lm9_2acursorS177;
                      _M0Lm9_2acursorS177 = _M0L6_2atmpS1904 + 1;
                      if (_M0L10next__charS225 < 58) {
                        if (_M0L10next__charS225 < 48) {
                          goto join_224;
                        } else {
                          _M0L12dispatch__15S201 = 2;
                          goto loop__label__15_204;
                        }
                      } else if (_M0L10next__charS225 > 58) {
                        goto join_224;
                      } else {
                        _M0L12dispatch__15S201 = 1;
                        goto loop__label__15_204;
                      }
                      join_224:;
                      _M0L12dispatch__15S201 = 0;
                      goto loop__label__15_204;
                    } else {
                      goto join_193;
                    }
                    break;
                  }
                  default: {
                    goto join_193;
                    break;
                  }
                }
                join_206:;
                _M0Lm6tag__1S186 = _M0Lm9tag__1__2S188;
                _M0Lm6tag__2S190 = _M0Lm9tag__2__1S191;
                _M0Lm20match__tag__saver__0S180 = _M0Lm6tag__0S185;
                _M0Lm20match__tag__saver__1S181 = _M0Lm6tag__1S186;
                _M0Lm20match__tag__saver__2S182 = _M0Lm6tag__2S190;
                _M0Lm20match__tag__saver__3S183 = _M0Lm6tag__3S189;
                _M0Lm20match__tag__saver__4S184 = _M0Lm6tag__4S192;
                _M0Lm13accept__stateS178 = 0;
                _M0Lm10match__endS179 = _M0Lm9_2acursorS177;
                goto join_193;
                join_202:;
                _M0Lm9tag__1__1S187 = _M0Lm9tag__1__2S188;
                _M0Lm6tag__1S186 = _M0Lm9_2acursorS177;
                _M0Lm6tag__2S190 = _M0Lm9tag__2__1S191;
                _M0L6_2atmpS1873 = _M0Lm9_2acursorS177;
                if (_M0L6_2atmpS1873 < _M0L6_2aendS176) {
                  int32_t _M0L6_2atmpS1875 = _M0Lm9_2acursorS177;
                  int32_t _M0L10next__charS205;
                  int32_t _M0L6_2atmpS1874;
                  moonbit_incref(_M0L7_2adataS174);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS205
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS174, _M0L6_2atmpS1875);
                  _M0L6_2atmpS1874 = _M0Lm9_2acursorS177;
                  _M0Lm9_2acursorS177 = _M0L6_2atmpS1874 + 1;
                  if (_M0L10next__charS205 < 58) {
                    if (_M0L10next__charS205 < 48) {
                      goto join_203;
                    } else {
                      _M0L12dispatch__15S201 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS205 > 58) {
                    goto join_203;
                  } else {
                    _M0L12dispatch__15S201 = 1;
                    continue;
                  }
                  join_203:;
                  _M0L12dispatch__15S201 = 0;
                  continue;
                } else {
                  goto join_193;
                }
                break;
              }
            } else {
              goto join_193;
            }
          } else {
            continue;
          }
        } else {
          goto join_193;
        }
        break;
      }
    } else {
      goto join_193;
    }
  } else {
    goto join_193;
  }
  join_193:;
  switch (_M0Lm13accept__stateS178) {
    case 0: {
      int32_t _M0L6_2atmpS1864 = _M0Lm20match__tag__saver__1S181;
      int32_t _M0L6_2atmpS1863 = _M0L6_2atmpS1864 + 1;
      int64_t _M0L6_2atmpS1860 = (int64_t)_M0L6_2atmpS1863;
      int32_t _M0L6_2atmpS1862 = _M0Lm20match__tag__saver__2S182;
      int64_t _M0L6_2atmpS1861 = (int64_t)_M0L6_2atmpS1862;
      struct _M0TPC16string10StringView _M0L11start__lineS194;
      int32_t _M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1858;
      int64_t _M0L6_2atmpS1855;
      int32_t _M0L6_2atmpS1857;
      int64_t _M0L6_2atmpS1856;
      struct _M0TPC16string10StringView _M0L13start__columnS195;
      int32_t _M0L6_2atmpS1854;
      int64_t _M0L6_2atmpS1851;
      int32_t _M0L6_2atmpS1853;
      int64_t _M0L6_2atmpS1852;
      struct _M0TPC16string10StringView _M0L3pkgS196;
      int32_t _M0L6_2atmpS1850;
      int32_t _M0L6_2atmpS1849;
      int64_t _M0L6_2atmpS1846;
      int32_t _M0L6_2atmpS1848;
      int64_t _M0L6_2atmpS1847;
      struct _M0TPC16string10StringView _M0L8filenameS197;
      int32_t _M0L6_2atmpS1845;
      int32_t _M0L6_2atmpS1844;
      int64_t _M0L6_2atmpS1841;
      int32_t _M0L6_2atmpS1843;
      int64_t _M0L6_2atmpS1842;
      struct _M0TPC16string10StringView _M0L9end__lineS198;
      int32_t _M0L6_2atmpS1840;
      int32_t _M0L6_2atmpS1839;
      int64_t _M0L6_2atmpS1836;
      int32_t _M0L6_2atmpS1838;
      int64_t _M0L6_2atmpS1837;
      struct _M0TPC16string10StringView _M0L11end__columnS199;
      struct _M0TPB13SourceLocRepr* _block_4745;
      moonbit_incref(_M0L7_2adataS174);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS194
      = _M0MPC16string6String4view(_M0L7_2adataS174, _M0L6_2atmpS1860, _M0L6_2atmpS1861);
      _M0L6_2atmpS1859 = _M0Lm20match__tag__saver__2S182;
      _M0L6_2atmpS1858 = _M0L6_2atmpS1859 + 1;
      _M0L6_2atmpS1855 = (int64_t)_M0L6_2atmpS1858;
      _M0L6_2atmpS1857 = _M0Lm20match__tag__saver__3S183;
      _M0L6_2atmpS1856 = (int64_t)_M0L6_2atmpS1857;
      moonbit_incref(_M0L7_2adataS174);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS195
      = _M0MPC16string6String4view(_M0L7_2adataS174, _M0L6_2atmpS1855, _M0L6_2atmpS1856);
      _M0L6_2atmpS1854 = _M0L8_2astartS175 + 1;
      _M0L6_2atmpS1851 = (int64_t)_M0L6_2atmpS1854;
      _M0L6_2atmpS1853 = _M0Lm20match__tag__saver__0S180;
      _M0L6_2atmpS1852 = (int64_t)_M0L6_2atmpS1853;
      moonbit_incref(_M0L7_2adataS174);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS196
      = _M0MPC16string6String4view(_M0L7_2adataS174, _M0L6_2atmpS1851, _M0L6_2atmpS1852);
      _M0L6_2atmpS1850 = _M0Lm20match__tag__saver__0S180;
      _M0L6_2atmpS1849 = _M0L6_2atmpS1850 + 1;
      _M0L6_2atmpS1846 = (int64_t)_M0L6_2atmpS1849;
      _M0L6_2atmpS1848 = _M0Lm20match__tag__saver__1S181;
      _M0L6_2atmpS1847 = (int64_t)_M0L6_2atmpS1848;
      moonbit_incref(_M0L7_2adataS174);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS197
      = _M0MPC16string6String4view(_M0L7_2adataS174, _M0L6_2atmpS1846, _M0L6_2atmpS1847);
      _M0L6_2atmpS1845 = _M0Lm20match__tag__saver__3S183;
      _M0L6_2atmpS1844 = _M0L6_2atmpS1845 + 1;
      _M0L6_2atmpS1841 = (int64_t)_M0L6_2atmpS1844;
      _M0L6_2atmpS1843 = _M0Lm20match__tag__saver__4S184;
      _M0L6_2atmpS1842 = (int64_t)_M0L6_2atmpS1843;
      moonbit_incref(_M0L7_2adataS174);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS198
      = _M0MPC16string6String4view(_M0L7_2adataS174, _M0L6_2atmpS1841, _M0L6_2atmpS1842);
      _M0L6_2atmpS1840 = _M0Lm20match__tag__saver__4S184;
      _M0L6_2atmpS1839 = _M0L6_2atmpS1840 + 1;
      _M0L6_2atmpS1836 = (int64_t)_M0L6_2atmpS1839;
      _M0L6_2atmpS1838 = _M0Lm10match__endS179;
      _M0L6_2atmpS1837 = (int64_t)_M0L6_2atmpS1838;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS199
      = _M0MPC16string6String4view(_M0L7_2adataS174, _M0L6_2atmpS1836, _M0L6_2atmpS1837);
      _block_4745
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4745)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4745->$0_0 = _M0L3pkgS196.$0;
      _block_4745->$0_1 = _M0L3pkgS196.$1;
      _block_4745->$0_2 = _M0L3pkgS196.$2;
      _block_4745->$1_0 = _M0L8filenameS197.$0;
      _block_4745->$1_1 = _M0L8filenameS197.$1;
      _block_4745->$1_2 = _M0L8filenameS197.$2;
      _block_4745->$2_0 = _M0L11start__lineS194.$0;
      _block_4745->$2_1 = _M0L11start__lineS194.$1;
      _block_4745->$2_2 = _M0L11start__lineS194.$2;
      _block_4745->$3_0 = _M0L13start__columnS195.$0;
      _block_4745->$3_1 = _M0L13start__columnS195.$1;
      _block_4745->$3_2 = _M0L13start__columnS195.$2;
      _block_4745->$4_0 = _M0L9end__lineS198.$0;
      _block_4745->$4_1 = _M0L9end__lineS198.$1;
      _block_4745->$4_2 = _M0L9end__lineS198.$2;
      _block_4745->$5_0 = _M0L11end__columnS199.$0;
      _block_4745->$5_1 = _M0L11end__columnS199.$1;
      _block_4745->$5_2 = _M0L11end__columnS199.$2;
      return _block_4745;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS174);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS167,
  int32_t _M0L5indexS168
) {
  int32_t _M0L3lenS166;
  int32_t _if__result_4746;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS166 = _M0L4selfS167->$1;
  if (_M0L5indexS168 >= 0) {
    _if__result_4746 = _M0L5indexS168 < _M0L3lenS166;
  } else {
    _if__result_4746 = 0;
  }
  if (_if__result_4746) {
    moonbit_string_t* _M0L6_2atmpS1834;
    moonbit_string_t _M0L6_2atmpS4310;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1834 = _M0MPC15array5Array6bufferGsE(_M0L4selfS167);
    if (
      _M0L5indexS168 < 0
      || _M0L5indexS168 >= Moonbit_array_length(_M0L6_2atmpS1834)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4310 = (moonbit_string_t)_M0L6_2atmpS1834[_M0L5indexS168];
    moonbit_incref(_M0L6_2atmpS4310);
    moonbit_decref(_M0L6_2atmpS1834);
    return _M0L6_2atmpS4310;
  } else {
    moonbit_decref(_M0L4selfS167);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

void* _M0MPC15array5Array2atGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS170,
  int32_t _M0L5indexS171
) {
  int32_t _M0L3lenS169;
  int32_t _if__result_4747;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS169 = _M0L4selfS170->$1;
  if (_M0L5indexS171 >= 0) {
    _if__result_4747 = _M0L5indexS171 < _M0L3lenS169;
  } else {
    _if__result_4747 = 0;
  }
  if (_if__result_4747) {
    void** _M0L6_2atmpS1835;
    void* _M0L6_2atmpS4311;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1835
    = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(_M0L4selfS170);
    if (
      _M0L5indexS171 < 0
      || _M0L5indexS171 >= Moonbit_array_length(_M0L6_2atmpS1835)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4311 = (void*)_M0L6_2atmpS1835[_M0L5indexS171];
    moonbit_incref(_M0L6_2atmpS4311);
    moonbit_decref(_M0L6_2atmpS1835);
    return _M0L6_2atmpS4311;
  } else {
    moonbit_decref(_M0L4selfS170);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array6lengthGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS165
) {
  int32_t _M0L8_2afieldS4312;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4312 = _M0L4selfS165->$1;
  moonbit_decref(_M0L4selfS165);
  return _M0L8_2afieldS4312;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS159
) {
  moonbit_string_t* _M0L8_2afieldS4313;
  int32_t _M0L6_2acntS4528;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4313 = _M0L4selfS159->$0;
  _M0L6_2acntS4528 = Moonbit_object_header(_M0L4selfS159)->rc;
  if (_M0L6_2acntS4528 > 1) {
    int32_t _M0L11_2anew__cntS4529 = _M0L6_2acntS4528 - 1;
    Moonbit_object_header(_M0L4selfS159)->rc = _M0L11_2anew__cntS4529;
    moonbit_incref(_M0L8_2afieldS4313);
  } else if (_M0L6_2acntS4528 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS159);
  }
  return _M0L8_2afieldS4313;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS160
) {
  struct _M0TUsiE** _M0L8_2afieldS4314;
  int32_t _M0L6_2acntS4530;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4314 = _M0L4selfS160->$0;
  _M0L6_2acntS4530 = Moonbit_object_header(_M0L4selfS160)->rc;
  if (_M0L6_2acntS4530 > 1) {
    int32_t _M0L11_2anew__cntS4531 = _M0L6_2acntS4530 - 1;
    Moonbit_object_header(_M0L4selfS160)->rc = _M0L11_2anew__cntS4531;
    moonbit_incref(_M0L8_2afieldS4314);
  } else if (_M0L6_2acntS4530 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS160);
  }
  return _M0L8_2afieldS4314;
}

void** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L4selfS161
) {
  void** _M0L8_2afieldS4315;
  int32_t _M0L6_2acntS4532;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4315 = _M0L4selfS161->$0;
  _M0L6_2acntS4532 = Moonbit_object_header(_M0L4selfS161)->rc;
  if (_M0L6_2acntS4532 > 1) {
    int32_t _M0L11_2anew__cntS4533 = _M0L6_2acntS4532 - 1;
    Moonbit_object_header(_M0L4selfS161)->rc = _M0L11_2anew__cntS4533;
    moonbit_incref(_M0L8_2afieldS4315);
  } else if (_M0L6_2acntS4532 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS161);
  }
  return _M0L8_2afieldS4315;
}

int32_t* _M0MPC15array5Array6bufferGiE(struct _M0TPB5ArrayGiE* _M0L4selfS162) {
  int32_t* _M0L8_2afieldS4316;
  int32_t _M0L6_2acntS4534;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4316 = _M0L4selfS162->$0;
  _M0L6_2acntS4534 = Moonbit_object_header(_M0L4selfS162)->rc;
  if (_M0L6_2acntS4534 > 1) {
    int32_t _M0L11_2anew__cntS4535 = _M0L6_2acntS4534 - 1;
    Moonbit_object_header(_M0L4selfS162)->rc = _M0L11_2anew__cntS4535;
    moonbit_incref(_M0L8_2afieldS4316);
  } else if (_M0L6_2acntS4534 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS162);
  }
  return _M0L8_2afieldS4316;
}

void** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE* _M0L4selfS163
) {
  void** _M0L8_2afieldS4317;
  int32_t _M0L6_2acntS4536;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4317 = _M0L4selfS163->$0;
  _M0L6_2acntS4536 = Moonbit_object_header(_M0L4selfS163)->rc;
  if (_M0L6_2acntS4536 > 1) {
    int32_t _M0L11_2anew__cntS4537 = _M0L6_2acntS4536 - 1;
    Moonbit_object_header(_M0L4selfS163)->rc = _M0L11_2anew__cntS4537;
    moonbit_incref(_M0L8_2afieldS4317);
  } else if (_M0L6_2acntS4536 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS163);
  }
  return _M0L8_2afieldS4317;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS164
) {
  void** _M0L8_2afieldS4318;
  int32_t _M0L6_2acntS4538;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4318 = _M0L4selfS164->$0;
  _M0L6_2acntS4538 = Moonbit_object_header(_M0L4selfS164)->rc;
  if (_M0L6_2acntS4538 > 1) {
    int32_t _M0L11_2anew__cntS4539 = _M0L6_2acntS4538 - 1;
    Moonbit_object_header(_M0L4selfS164)->rc = _M0L11_2anew__cntS4539;
    moonbit_incref(_M0L8_2afieldS4318);
  } else if (_M0L6_2acntS4538 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS164);
  }
  return _M0L8_2afieldS4318;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS158) {
  struct _M0TPB13StringBuilder* _M0L3bufS157;
  struct _M0TPB6Logger _M0L6_2atmpS1833;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS157 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS157);
  _M0L6_2atmpS1833
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS157
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS158, _M0L6_2atmpS1833);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS157);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS156) {
  int32_t _M0L6_2atmpS1832;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1832 = (int32_t)_M0L4selfS156;
  return _M0L6_2atmpS1832;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS154,
  int32_t _M0L8trailingS155
) {
  int32_t _M0L6_2atmpS1831;
  int32_t _M0L6_2atmpS1830;
  int32_t _M0L6_2atmpS1829;
  int32_t _M0L6_2atmpS1828;
  int32_t _M0L6_2atmpS1827;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1831 = _M0L7leadingS154 - 55296;
  _M0L6_2atmpS1830 = _M0L6_2atmpS1831 * 1024;
  _M0L6_2atmpS1829 = _M0L6_2atmpS1830 + _M0L8trailingS155;
  _M0L6_2atmpS1828 = _M0L6_2atmpS1829 - 56320;
  _M0L6_2atmpS1827 = _M0L6_2atmpS1828 + 65536;
  return _M0L6_2atmpS1827;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS153) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS153 >= 56320) {
    return _M0L4selfS153 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS152) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS152 >= 55296) {
    return _M0L4selfS152 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS149,
  int32_t _M0L2chS151
) {
  int32_t _M0L3lenS1822;
  int32_t _M0L6_2atmpS1821;
  moonbit_bytes_t _M0L8_2afieldS4319;
  moonbit_bytes_t _M0L4dataS1825;
  int32_t _M0L3lenS1826;
  int32_t _M0L3incS150;
  int32_t _M0L3lenS1824;
  int32_t _M0L6_2atmpS1823;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1822 = _M0L4selfS149->$1;
  _M0L6_2atmpS1821 = _M0L3lenS1822 + 4;
  moonbit_incref(_M0L4selfS149);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS149, _M0L6_2atmpS1821);
  _M0L8_2afieldS4319 = _M0L4selfS149->$0;
  _M0L4dataS1825 = _M0L8_2afieldS4319;
  _M0L3lenS1826 = _M0L4selfS149->$1;
  moonbit_incref(_M0L4dataS1825);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS150
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1825, _M0L3lenS1826, _M0L2chS151);
  _M0L3lenS1824 = _M0L4selfS149->$1;
  _M0L6_2atmpS1823 = _M0L3lenS1824 + _M0L3incS150;
  _M0L4selfS149->$1 = _M0L6_2atmpS1823;
  moonbit_decref(_M0L4selfS149);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS144,
  int32_t _M0L8requiredS145
) {
  moonbit_bytes_t _M0L8_2afieldS4323;
  moonbit_bytes_t _M0L4dataS1820;
  int32_t _M0L6_2atmpS4322;
  int32_t _M0L12current__lenS143;
  int32_t _M0Lm13enough__spaceS146;
  int32_t _M0L6_2atmpS1818;
  int32_t _M0L6_2atmpS1819;
  moonbit_bytes_t _M0L9new__dataS148;
  moonbit_bytes_t _M0L8_2afieldS4321;
  moonbit_bytes_t _M0L4dataS1816;
  int32_t _M0L3lenS1817;
  moonbit_bytes_t _M0L6_2aoldS4320;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4323 = _M0L4selfS144->$0;
  _M0L4dataS1820 = _M0L8_2afieldS4323;
  _M0L6_2atmpS4322 = Moonbit_array_length(_M0L4dataS1820);
  _M0L12current__lenS143 = _M0L6_2atmpS4322;
  if (_M0L8requiredS145 <= _M0L12current__lenS143) {
    moonbit_decref(_M0L4selfS144);
    return 0;
  }
  _M0Lm13enough__spaceS146 = _M0L12current__lenS143;
  while (1) {
    int32_t _M0L6_2atmpS1814 = _M0Lm13enough__spaceS146;
    if (_M0L6_2atmpS1814 < _M0L8requiredS145) {
      int32_t _M0L6_2atmpS1815 = _M0Lm13enough__spaceS146;
      _M0Lm13enough__spaceS146 = _M0L6_2atmpS1815 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1818 = _M0Lm13enough__spaceS146;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1819 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS148
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1818, _M0L6_2atmpS1819);
  _M0L8_2afieldS4321 = _M0L4selfS144->$0;
  _M0L4dataS1816 = _M0L8_2afieldS4321;
  _M0L3lenS1817 = _M0L4selfS144->$1;
  moonbit_incref(_M0L4dataS1816);
  moonbit_incref(_M0L9new__dataS148);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS148, 0, _M0L4dataS1816, 0, _M0L3lenS1817);
  _M0L6_2aoldS4320 = _M0L4selfS144->$0;
  moonbit_decref(_M0L6_2aoldS4320);
  _M0L4selfS144->$0 = _M0L9new__dataS148;
  moonbit_decref(_M0L4selfS144);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS138,
  int32_t _M0L6offsetS139,
  int32_t _M0L5valueS137
) {
  uint32_t _M0L4codeS136;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS136 = _M0MPC14char4Char8to__uint(_M0L5valueS137);
  if (_M0L4codeS136 < 65536u) {
    uint32_t _M0L6_2atmpS1797 = _M0L4codeS136 & 255u;
    int32_t _M0L6_2atmpS1796;
    int32_t _M0L6_2atmpS1798;
    uint32_t _M0L6_2atmpS1800;
    int32_t _M0L6_2atmpS1799;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1796 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1797);
    if (
      _M0L6offsetS139 < 0
      || _M0L6offsetS139 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6offsetS139] = _M0L6_2atmpS1796;
    _M0L6_2atmpS1798 = _M0L6offsetS139 + 1;
    _M0L6_2atmpS1800 = _M0L4codeS136 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1799 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1800);
    if (
      _M0L6_2atmpS1798 < 0
      || _M0L6_2atmpS1798 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1798] = _M0L6_2atmpS1799;
    moonbit_decref(_M0L4selfS138);
    return 2;
  } else if (_M0L4codeS136 < 1114112u) {
    uint32_t _M0L2hiS140 = _M0L4codeS136 - 65536u;
    uint32_t _M0L6_2atmpS1813 = _M0L2hiS140 >> 10;
    uint32_t _M0L2loS141 = _M0L6_2atmpS1813 | 55296u;
    uint32_t _M0L6_2atmpS1812 = _M0L2hiS140 & 1023u;
    uint32_t _M0L2hiS142 = _M0L6_2atmpS1812 | 56320u;
    uint32_t _M0L6_2atmpS1802 = _M0L2loS141 & 255u;
    int32_t _M0L6_2atmpS1801;
    int32_t _M0L6_2atmpS1803;
    uint32_t _M0L6_2atmpS1805;
    int32_t _M0L6_2atmpS1804;
    int32_t _M0L6_2atmpS1806;
    uint32_t _M0L6_2atmpS1808;
    int32_t _M0L6_2atmpS1807;
    int32_t _M0L6_2atmpS1809;
    uint32_t _M0L6_2atmpS1811;
    int32_t _M0L6_2atmpS1810;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1801 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1802);
    if (
      _M0L6offsetS139 < 0
      || _M0L6offsetS139 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6offsetS139] = _M0L6_2atmpS1801;
    _M0L6_2atmpS1803 = _M0L6offsetS139 + 1;
    _M0L6_2atmpS1805 = _M0L2loS141 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1804 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1805);
    if (
      _M0L6_2atmpS1803 < 0
      || _M0L6_2atmpS1803 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1803] = _M0L6_2atmpS1804;
    _M0L6_2atmpS1806 = _M0L6offsetS139 + 2;
    _M0L6_2atmpS1808 = _M0L2hiS142 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1807 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1808);
    if (
      _M0L6_2atmpS1806 < 0
      || _M0L6_2atmpS1806 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1806] = _M0L6_2atmpS1807;
    _M0L6_2atmpS1809 = _M0L6offsetS139 + 3;
    _M0L6_2atmpS1811 = _M0L2hiS142 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1810 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1811);
    if (
      _M0L6_2atmpS1809 < 0
      || _M0L6_2atmpS1809 >= Moonbit_array_length(_M0L4selfS138)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS138[_M0L6_2atmpS1809] = _M0L6_2atmpS1810;
    moonbit_decref(_M0L4selfS138);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS138);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_116.data, (moonbit_string_t)moonbit_string_literal_117.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS135) {
  int32_t _M0L6_2atmpS1795;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1795 = *(int32_t*)&_M0L4selfS135;
  return _M0L6_2atmpS1795 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS134) {
  int32_t _M0L6_2atmpS1794;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1794 = _M0L4selfS134;
  return *(uint32_t*)&_M0L6_2atmpS1794;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS133
) {
  moonbit_bytes_t _M0L8_2afieldS4325;
  moonbit_bytes_t _M0L4dataS1793;
  moonbit_bytes_t _M0L6_2atmpS1790;
  int32_t _M0L8_2afieldS4324;
  int32_t _M0L3lenS1792;
  int64_t _M0L6_2atmpS1791;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4325 = _M0L4selfS133->$0;
  _M0L4dataS1793 = _M0L8_2afieldS4325;
  moonbit_incref(_M0L4dataS1793);
  _M0L6_2atmpS1790 = _M0L4dataS1793;
  _M0L8_2afieldS4324 = _M0L4selfS133->$1;
  moonbit_decref(_M0L4selfS133);
  _M0L3lenS1792 = _M0L8_2afieldS4324;
  _M0L6_2atmpS1791 = (int64_t)_M0L3lenS1792;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1790, 0, _M0L6_2atmpS1791);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS128,
  int32_t _M0L6offsetS132,
  int64_t _M0L6lengthS130
) {
  int32_t _M0L3lenS127;
  int32_t _M0L6lengthS129;
  int32_t _if__result_4749;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS127 = Moonbit_array_length(_M0L4selfS128);
  if (_M0L6lengthS130 == 4294967296ll) {
    _M0L6lengthS129 = _M0L3lenS127 - _M0L6offsetS132;
  } else {
    int64_t _M0L7_2aSomeS131 = _M0L6lengthS130;
    _M0L6lengthS129 = (int32_t)_M0L7_2aSomeS131;
  }
  if (_M0L6offsetS132 >= 0) {
    if (_M0L6lengthS129 >= 0) {
      int32_t _M0L6_2atmpS1789 = _M0L6offsetS132 + _M0L6lengthS129;
      _if__result_4749 = _M0L6_2atmpS1789 <= _M0L3lenS127;
    } else {
      _if__result_4749 = 0;
    }
  } else {
    _if__result_4749 = 0;
  }
  if (_if__result_4749) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS128, _M0L6offsetS132, _M0L6lengthS129);
  } else {
    moonbit_decref(_M0L4selfS128);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS125
) {
  int32_t _M0L7initialS124;
  moonbit_bytes_t _M0L4dataS126;
  struct _M0TPB13StringBuilder* _block_4750;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS125 < 1) {
    _M0L7initialS124 = 1;
  } else {
    _M0L7initialS124 = _M0L10size__hintS125;
  }
  _M0L4dataS126 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS124, 0);
  _block_4750
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4750)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4750->$0 = _M0L4dataS126;
  _block_4750->$1 = 0;
  return _block_4750;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS123) {
  int32_t _M0L6_2atmpS1788;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1788 = (int32_t)_M0L4selfS123;
  return _M0L6_2atmpS1788;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS93,
  int32_t _M0L11dst__offsetS94,
  moonbit_string_t* _M0L3srcS95,
  int32_t _M0L11src__offsetS96,
  int32_t _M0L3lenS97
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS93, _M0L11dst__offsetS94, _M0L3srcS95, _M0L11src__offsetS96, _M0L3lenS97);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS98,
  int32_t _M0L11dst__offsetS99,
  struct _M0TUsiE** _M0L3srcS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L3lenS102
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS98, _M0L11dst__offsetS99, _M0L3srcS100, _M0L11src__offsetS101, _M0L3lenS102);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  void** _M0L3dstS103,
  int32_t _M0L11dst__offsetS104,
  void** _M0L3srcS105,
  int32_t _M0L11src__offsetS106,
  int32_t _M0L3lenS107
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamEE(_M0L3dstS103, _M0L11dst__offsetS104, _M0L3srcS105, _M0L11src__offsetS106, _M0L3lenS107);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGiE(
  int32_t* _M0L3dstS108,
  int32_t _M0L11dst__offsetS109,
  int32_t* _M0L3srcS110,
  int32_t _M0L11src__offsetS111,
  int32_t _M0L3lenS112
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGiEE(_M0L3dstS108, _M0L11dst__offsetS109, _M0L3srcS110, _M0L11src__offsetS111, _M0L3lenS112);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE(
  void** _M0L3dstS113,
  int32_t _M0L11dst__offsetS114,
  void** _M0L3srcS115,
  int32_t _M0L11src__offsetS116,
  int32_t _M0L3lenS117
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L3dstS113, _M0L11dst__offsetS114, _M0L3srcS115, _M0L11src__offsetS116, _M0L3lenS117);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS118,
  int32_t _M0L11dst__offsetS119,
  void** _M0L3srcS120,
  int32_t _M0L11src__offsetS121,
  int32_t _M0L3lenS122
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS118, _M0L11dst__offsetS119, _M0L3srcS120, _M0L11src__offsetS121, _M0L3lenS122);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS30,
  int32_t _M0L11dst__offsetS32,
  moonbit_bytes_t _M0L3srcS31,
  int32_t _M0L11src__offsetS33,
  int32_t _M0L3lenS35
) {
  int32_t _if__result_4751;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS30 == _M0L3srcS31) {
    _if__result_4751 = _M0L11dst__offsetS32 < _M0L11src__offsetS33;
  } else {
    _if__result_4751 = 0;
  }
  if (_if__result_4751) {
    int32_t _M0L1iS34 = 0;
    while (1) {
      if (_M0L1iS34 < _M0L3lenS35) {
        int32_t _M0L6_2atmpS1725 = _M0L11dst__offsetS32 + _M0L1iS34;
        int32_t _M0L6_2atmpS1727 = _M0L11src__offsetS33 + _M0L1iS34;
        int32_t _M0L6_2atmpS1726;
        int32_t _M0L6_2atmpS1728;
        if (
          _M0L6_2atmpS1727 < 0
          || _M0L6_2atmpS1727 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1726 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1727];
        if (
          _M0L6_2atmpS1725 < 0
          || _M0L6_2atmpS1725 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1725] = _M0L6_2atmpS1726;
        _M0L6_2atmpS1728 = _M0L1iS34 + 1;
        _M0L1iS34 = _M0L6_2atmpS1728;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1733 = _M0L3lenS35 - 1;
    int32_t _M0L1iS37 = _M0L6_2atmpS1733;
    while (1) {
      if (_M0L1iS37 >= 0) {
        int32_t _M0L6_2atmpS1729 = _M0L11dst__offsetS32 + _M0L1iS37;
        int32_t _M0L6_2atmpS1731 = _M0L11src__offsetS33 + _M0L1iS37;
        int32_t _M0L6_2atmpS1730;
        int32_t _M0L6_2atmpS1732;
        if (
          _M0L6_2atmpS1731 < 0
          || _M0L6_2atmpS1731 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1730 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1731];
        if (
          _M0L6_2atmpS1729 < 0
          || _M0L6_2atmpS1729 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1729] = _M0L6_2atmpS1730;
        _M0L6_2atmpS1732 = _M0L1iS37 - 1;
        _M0L1iS37 = _M0L6_2atmpS1732;
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
  int32_t _if__result_4754;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS39 == _M0L3srcS40) {
    _if__result_4754 = _M0L11dst__offsetS41 < _M0L11src__offsetS42;
  } else {
    _if__result_4754 = 0;
  }
  if (_if__result_4754) {
    int32_t _M0L1iS43 = 0;
    while (1) {
      if (_M0L1iS43 < _M0L3lenS44) {
        int32_t _M0L6_2atmpS1734 = _M0L11dst__offsetS41 + _M0L1iS43;
        int32_t _M0L6_2atmpS1736 = _M0L11src__offsetS42 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS4327;
        moonbit_string_t _M0L6_2atmpS1735;
        moonbit_string_t _M0L6_2aoldS4326;
        int32_t _M0L6_2atmpS1737;
        if (
          _M0L6_2atmpS1736 < 0
          || _M0L6_2atmpS1736 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4327 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1736];
        _M0L6_2atmpS1735 = _M0L6_2atmpS4327;
        if (
          _M0L6_2atmpS1734 < 0
          || _M0L6_2atmpS1734 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4326 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1734];
        moonbit_incref(_M0L6_2atmpS1735);
        moonbit_decref(_M0L6_2aoldS4326);
        _M0L3dstS39[_M0L6_2atmpS1734] = _M0L6_2atmpS1735;
        _M0L6_2atmpS1737 = _M0L1iS43 + 1;
        _M0L1iS43 = _M0L6_2atmpS1737;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1742 = _M0L3lenS44 - 1;
    int32_t _M0L1iS46 = _M0L6_2atmpS1742;
    while (1) {
      if (_M0L1iS46 >= 0) {
        int32_t _M0L6_2atmpS1738 = _M0L11dst__offsetS41 + _M0L1iS46;
        int32_t _M0L6_2atmpS1740 = _M0L11src__offsetS42 + _M0L1iS46;
        moonbit_string_t _M0L6_2atmpS4329;
        moonbit_string_t _M0L6_2atmpS1739;
        moonbit_string_t _M0L6_2aoldS4328;
        int32_t _M0L6_2atmpS1741;
        if (
          _M0L6_2atmpS1740 < 0
          || _M0L6_2atmpS1740 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4329 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1740];
        _M0L6_2atmpS1739 = _M0L6_2atmpS4329;
        if (
          _M0L6_2atmpS1738 < 0
          || _M0L6_2atmpS1738 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4328 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1738];
        moonbit_incref(_M0L6_2atmpS1739);
        moonbit_decref(_M0L6_2aoldS4328);
        _M0L3dstS39[_M0L6_2atmpS1738] = _M0L6_2atmpS1739;
        _M0L6_2atmpS1741 = _M0L1iS46 - 1;
        _M0L1iS46 = _M0L6_2atmpS1741;
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
  int32_t _if__result_4757;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS48 == _M0L3srcS49) {
    _if__result_4757 = _M0L11dst__offsetS50 < _M0L11src__offsetS51;
  } else {
    _if__result_4757 = 0;
  }
  if (_if__result_4757) {
    int32_t _M0L1iS52 = 0;
    while (1) {
      if (_M0L1iS52 < _M0L3lenS53) {
        int32_t _M0L6_2atmpS1743 = _M0L11dst__offsetS50 + _M0L1iS52;
        int32_t _M0L6_2atmpS1745 = _M0L11src__offsetS51 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS4331;
        struct _M0TUsiE* _M0L6_2atmpS1744;
        struct _M0TUsiE* _M0L6_2aoldS4330;
        int32_t _M0L6_2atmpS1746;
        if (
          _M0L6_2atmpS1745 < 0
          || _M0L6_2atmpS1745 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4331 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1745];
        _M0L6_2atmpS1744 = _M0L6_2atmpS4331;
        if (
          _M0L6_2atmpS1743 < 0
          || _M0L6_2atmpS1743 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4330 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1743];
        if (_M0L6_2atmpS1744) {
          moonbit_incref(_M0L6_2atmpS1744);
        }
        if (_M0L6_2aoldS4330) {
          moonbit_decref(_M0L6_2aoldS4330);
        }
        _M0L3dstS48[_M0L6_2atmpS1743] = _M0L6_2atmpS1744;
        _M0L6_2atmpS1746 = _M0L1iS52 + 1;
        _M0L1iS52 = _M0L6_2atmpS1746;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1751 = _M0L3lenS53 - 1;
    int32_t _M0L1iS55 = _M0L6_2atmpS1751;
    while (1) {
      if (_M0L1iS55 >= 0) {
        int32_t _M0L6_2atmpS1747 = _M0L11dst__offsetS50 + _M0L1iS55;
        int32_t _M0L6_2atmpS1749 = _M0L11src__offsetS51 + _M0L1iS55;
        struct _M0TUsiE* _M0L6_2atmpS4333;
        struct _M0TUsiE* _M0L6_2atmpS1748;
        struct _M0TUsiE* _M0L6_2aoldS4332;
        int32_t _M0L6_2atmpS1750;
        if (
          _M0L6_2atmpS1749 < 0
          || _M0L6_2atmpS1749 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4333 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1749];
        _M0L6_2atmpS1748 = _M0L6_2atmpS4333;
        if (
          _M0L6_2atmpS1747 < 0
          || _M0L6_2atmpS1747 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4332 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1747];
        if (_M0L6_2atmpS1748) {
          moonbit_incref(_M0L6_2atmpS1748);
        }
        if (_M0L6_2aoldS4332) {
          moonbit_decref(_M0L6_2aoldS4332);
        }
        _M0L3dstS48[_M0L6_2atmpS1747] = _M0L6_2atmpS1748;
        _M0L6_2atmpS1750 = _M0L1iS55 - 1;
        _M0L1iS55 = _M0L6_2atmpS1750;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamEE(
  void** _M0L3dstS57,
  int32_t _M0L11dst__offsetS59,
  void** _M0L3srcS58,
  int32_t _M0L11src__offsetS60,
  int32_t _M0L3lenS62
) {
  int32_t _if__result_4760;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS57 == _M0L3srcS58) {
    _if__result_4760 = _M0L11dst__offsetS59 < _M0L11src__offsetS60;
  } else {
    _if__result_4760 = 0;
  }
  if (_if__result_4760) {
    int32_t _M0L1iS61 = 0;
    while (1) {
      if (_M0L1iS61 < _M0L3lenS62) {
        int32_t _M0L6_2atmpS1752 = _M0L11dst__offsetS59 + _M0L1iS61;
        int32_t _M0L6_2atmpS1754 = _M0L11src__offsetS60 + _M0L1iS61;
        void* _M0L6_2atmpS4335;
        void* _M0L6_2atmpS1753;
        void* _M0L6_2aoldS4334;
        int32_t _M0L6_2atmpS1755;
        if (
          _M0L6_2atmpS1754 < 0
          || _M0L6_2atmpS1754 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4335 = (void*)_M0L3srcS58[_M0L6_2atmpS1754];
        _M0L6_2atmpS1753 = _M0L6_2atmpS4335;
        if (
          _M0L6_2atmpS1752 < 0
          || _M0L6_2atmpS1752 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4334 = (void*)_M0L3dstS57[_M0L6_2atmpS1752];
        moonbit_incref(_M0L6_2atmpS1753);
        moonbit_decref(_M0L6_2aoldS4334);
        _M0L3dstS57[_M0L6_2atmpS1752] = _M0L6_2atmpS1753;
        _M0L6_2atmpS1755 = _M0L1iS61 + 1;
        _M0L1iS61 = _M0L6_2atmpS1755;
        continue;
      } else {
        moonbit_decref(_M0L3srcS58);
        moonbit_decref(_M0L3dstS57);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1760 = _M0L3lenS62 - 1;
    int32_t _M0L1iS64 = _M0L6_2atmpS1760;
    while (1) {
      if (_M0L1iS64 >= 0) {
        int32_t _M0L6_2atmpS1756 = _M0L11dst__offsetS59 + _M0L1iS64;
        int32_t _M0L6_2atmpS1758 = _M0L11src__offsetS60 + _M0L1iS64;
        void* _M0L6_2atmpS4337;
        void* _M0L6_2atmpS1757;
        void* _M0L6_2aoldS4336;
        int32_t _M0L6_2atmpS1759;
        if (
          _M0L6_2atmpS1758 < 0
          || _M0L6_2atmpS1758 >= Moonbit_array_length(_M0L3srcS58)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4337 = (void*)_M0L3srcS58[_M0L6_2atmpS1758];
        _M0L6_2atmpS1757 = _M0L6_2atmpS4337;
        if (
          _M0L6_2atmpS1756 < 0
          || _M0L6_2atmpS1756 >= Moonbit_array_length(_M0L3dstS57)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4336 = (void*)_M0L3dstS57[_M0L6_2atmpS1756];
        moonbit_incref(_M0L6_2atmpS1757);
        moonbit_decref(_M0L6_2aoldS4336);
        _M0L3dstS57[_M0L6_2atmpS1756] = _M0L6_2atmpS1757;
        _M0L6_2atmpS1759 = _M0L1iS64 - 1;
        _M0L1iS64 = _M0L6_2atmpS1759;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGiEE(
  int32_t* _M0L3dstS66,
  int32_t _M0L11dst__offsetS68,
  int32_t* _M0L3srcS67,
  int32_t _M0L11src__offsetS69,
  int32_t _M0L3lenS71
) {
  int32_t _if__result_4763;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS66 == _M0L3srcS67) {
    _if__result_4763 = _M0L11dst__offsetS68 < _M0L11src__offsetS69;
  } else {
    _if__result_4763 = 0;
  }
  if (_if__result_4763) {
    int32_t _M0L1iS70 = 0;
    while (1) {
      if (_M0L1iS70 < _M0L3lenS71) {
        int32_t _M0L6_2atmpS1761 = _M0L11dst__offsetS68 + _M0L1iS70;
        int32_t _M0L6_2atmpS1763 = _M0L11src__offsetS69 + _M0L1iS70;
        int32_t _M0L6_2atmpS1762;
        int32_t _M0L6_2atmpS1764;
        if (
          _M0L6_2atmpS1763 < 0
          || _M0L6_2atmpS1763 >= Moonbit_array_length(_M0L3srcS67)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1762 = (int32_t)_M0L3srcS67[_M0L6_2atmpS1763];
        if (
          _M0L6_2atmpS1761 < 0
          || _M0L6_2atmpS1761 >= Moonbit_array_length(_M0L3dstS66)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS66[_M0L6_2atmpS1761] = _M0L6_2atmpS1762;
        _M0L6_2atmpS1764 = _M0L1iS70 + 1;
        _M0L1iS70 = _M0L6_2atmpS1764;
        continue;
      } else {
        moonbit_decref(_M0L3srcS67);
        moonbit_decref(_M0L3dstS66);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1769 = _M0L3lenS71 - 1;
    int32_t _M0L1iS73 = _M0L6_2atmpS1769;
    while (1) {
      if (_M0L1iS73 >= 0) {
        int32_t _M0L6_2atmpS1765 = _M0L11dst__offsetS68 + _M0L1iS73;
        int32_t _M0L6_2atmpS1767 = _M0L11src__offsetS69 + _M0L1iS73;
        int32_t _M0L6_2atmpS1766;
        int32_t _M0L6_2atmpS1768;
        if (
          _M0L6_2atmpS1767 < 0
          || _M0L6_2atmpS1767 >= Moonbit_array_length(_M0L3srcS67)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1766 = (int32_t)_M0L3srcS67[_M0L6_2atmpS1767];
        if (
          _M0L6_2atmpS1765 < 0
          || _M0L6_2atmpS1765 >= Moonbit_array_length(_M0L3dstS66)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS66[_M0L6_2atmpS1765] = _M0L6_2atmpS1766;
        _M0L6_2atmpS1768 = _M0L1iS73 - 1;
        _M0L1iS73 = _M0L6_2atmpS1768;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  void** _M0L3dstS75,
  int32_t _M0L11dst__offsetS77,
  void** _M0L3srcS76,
  int32_t _M0L11src__offsetS78,
  int32_t _M0L3lenS80
) {
  int32_t _if__result_4766;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS75 == _M0L3srcS76) {
    _if__result_4766 = _M0L11dst__offsetS77 < _M0L11src__offsetS78;
  } else {
    _if__result_4766 = 0;
  }
  if (_if__result_4766) {
    int32_t _M0L1iS79 = 0;
    while (1) {
      if (_M0L1iS79 < _M0L3lenS80) {
        int32_t _M0L6_2atmpS1770 = _M0L11dst__offsetS77 + _M0L1iS79;
        int32_t _M0L6_2atmpS1772 = _M0L11src__offsetS78 + _M0L1iS79;
        void* _M0L6_2atmpS4339;
        void* _M0L6_2atmpS1771;
        void* _M0L6_2aoldS4338;
        int32_t _M0L6_2atmpS1773;
        if (
          _M0L6_2atmpS1772 < 0
          || _M0L6_2atmpS1772 >= Moonbit_array_length(_M0L3srcS76)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4339 = (void*)_M0L3srcS76[_M0L6_2atmpS1772];
        _M0L6_2atmpS1771 = _M0L6_2atmpS4339;
        if (
          _M0L6_2atmpS1770 < 0
          || _M0L6_2atmpS1770 >= Moonbit_array_length(_M0L3dstS75)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4338 = (void*)_M0L3dstS75[_M0L6_2atmpS1770];
        moonbit_incref(_M0L6_2atmpS1771);
        moonbit_decref(_M0L6_2aoldS4338);
        _M0L3dstS75[_M0L6_2atmpS1770] = _M0L6_2atmpS1771;
        _M0L6_2atmpS1773 = _M0L1iS79 + 1;
        _M0L1iS79 = _M0L6_2atmpS1773;
        continue;
      } else {
        moonbit_decref(_M0L3srcS76);
        moonbit_decref(_M0L3dstS75);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1778 = _M0L3lenS80 - 1;
    int32_t _M0L1iS82 = _M0L6_2atmpS1778;
    while (1) {
      if (_M0L1iS82 >= 0) {
        int32_t _M0L6_2atmpS1774 = _M0L11dst__offsetS77 + _M0L1iS82;
        int32_t _M0L6_2atmpS1776 = _M0L11src__offsetS78 + _M0L1iS82;
        void* _M0L6_2atmpS4341;
        void* _M0L6_2atmpS1775;
        void* _M0L6_2aoldS4340;
        int32_t _M0L6_2atmpS1777;
        if (
          _M0L6_2atmpS1776 < 0
          || _M0L6_2atmpS1776 >= Moonbit_array_length(_M0L3srcS76)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4341 = (void*)_M0L3srcS76[_M0L6_2atmpS1776];
        _M0L6_2atmpS1775 = _M0L6_2atmpS4341;
        if (
          _M0L6_2atmpS1774 < 0
          || _M0L6_2atmpS1774 >= Moonbit_array_length(_M0L3dstS75)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4340 = (void*)_M0L3dstS75[_M0L6_2atmpS1774];
        moonbit_incref(_M0L6_2atmpS1775);
        moonbit_decref(_M0L6_2aoldS4340);
        _M0L3dstS75[_M0L6_2atmpS1774] = _M0L6_2atmpS1775;
        _M0L6_2atmpS1777 = _M0L1iS82 - 1;
        _M0L1iS82 = _M0L6_2atmpS1777;
        continue;
      } else {
        moonbit_decref(_M0L3srcS76);
        moonbit_decref(_M0L3dstS75);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS84,
  int32_t _M0L11dst__offsetS86,
  void** _M0L3srcS85,
  int32_t _M0L11src__offsetS87,
  int32_t _M0L3lenS89
) {
  int32_t _if__result_4769;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS84 == _M0L3srcS85) {
    _if__result_4769 = _M0L11dst__offsetS86 < _M0L11src__offsetS87;
  } else {
    _if__result_4769 = 0;
  }
  if (_if__result_4769) {
    int32_t _M0L1iS88 = 0;
    while (1) {
      if (_M0L1iS88 < _M0L3lenS89) {
        int32_t _M0L6_2atmpS1779 = _M0L11dst__offsetS86 + _M0L1iS88;
        int32_t _M0L6_2atmpS1781 = _M0L11src__offsetS87 + _M0L1iS88;
        void* _M0L6_2atmpS4343;
        void* _M0L6_2atmpS1780;
        void* _M0L6_2aoldS4342;
        int32_t _M0L6_2atmpS1782;
        if (
          _M0L6_2atmpS1781 < 0
          || _M0L6_2atmpS1781 >= Moonbit_array_length(_M0L3srcS85)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4343 = (void*)_M0L3srcS85[_M0L6_2atmpS1781];
        _M0L6_2atmpS1780 = _M0L6_2atmpS4343;
        if (
          _M0L6_2atmpS1779 < 0
          || _M0L6_2atmpS1779 >= Moonbit_array_length(_M0L3dstS84)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4342 = (void*)_M0L3dstS84[_M0L6_2atmpS1779];
        moonbit_incref(_M0L6_2atmpS1780);
        moonbit_decref(_M0L6_2aoldS4342);
        _M0L3dstS84[_M0L6_2atmpS1779] = _M0L6_2atmpS1780;
        _M0L6_2atmpS1782 = _M0L1iS88 + 1;
        _M0L1iS88 = _M0L6_2atmpS1782;
        continue;
      } else {
        moonbit_decref(_M0L3srcS85);
        moonbit_decref(_M0L3dstS84);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1787 = _M0L3lenS89 - 1;
    int32_t _M0L1iS91 = _M0L6_2atmpS1787;
    while (1) {
      if (_M0L1iS91 >= 0) {
        int32_t _M0L6_2atmpS1783 = _M0L11dst__offsetS86 + _M0L1iS91;
        int32_t _M0L6_2atmpS1785 = _M0L11src__offsetS87 + _M0L1iS91;
        void* _M0L6_2atmpS4345;
        void* _M0L6_2atmpS1784;
        void* _M0L6_2aoldS4344;
        int32_t _M0L6_2atmpS1786;
        if (
          _M0L6_2atmpS1785 < 0
          || _M0L6_2atmpS1785 >= Moonbit_array_length(_M0L3srcS85)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4345 = (void*)_M0L3srcS85[_M0L6_2atmpS1785];
        _M0L6_2atmpS1784 = _M0L6_2atmpS4345;
        if (
          _M0L6_2atmpS1783 < 0
          || _M0L6_2atmpS1783 >= Moonbit_array_length(_M0L3dstS84)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4344 = (void*)_M0L3dstS84[_M0L6_2atmpS1783];
        moonbit_incref(_M0L6_2atmpS1784);
        moonbit_decref(_M0L6_2aoldS4344);
        _M0L3dstS84[_M0L6_2atmpS1783] = _M0L6_2atmpS1784;
        _M0L6_2atmpS1786 = _M0L1iS91 - 1;
        _M0L1iS91 = _M0L6_2atmpS1786;
        continue;
      } else {
        moonbit_decref(_M0L3srcS85);
        moonbit_decref(_M0L3dstS84);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1704;
  moonbit_string_t _M0L6_2atmpS4348;
  moonbit_string_t _M0L6_2atmpS1702;
  moonbit_string_t _M0L6_2atmpS1703;
  moonbit_string_t _M0L6_2atmpS4347;
  moonbit_string_t _M0L6_2atmpS1701;
  moonbit_string_t _M0L6_2atmpS4346;
  moonbit_string_t _M0L6_2atmpS1700;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1704 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4348
  = moonbit_add_string(_M0L6_2atmpS1704, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1704);
  _M0L6_2atmpS1702 = _M0L6_2atmpS4348;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1703
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4347 = moonbit_add_string(_M0L6_2atmpS1702, _M0L6_2atmpS1703);
  moonbit_decref(_M0L6_2atmpS1702);
  moonbit_decref(_M0L6_2atmpS1703);
  _M0L6_2atmpS1701 = _M0L6_2atmpS4347;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4346
  = moonbit_add_string(_M0L6_2atmpS1701, (moonbit_string_t)moonbit_string_literal_68.data);
  moonbit_decref(_M0L6_2atmpS1701);
  _M0L6_2atmpS1700 = _M0L6_2atmpS4346;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1700);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1709;
  moonbit_string_t _M0L6_2atmpS4351;
  moonbit_string_t _M0L6_2atmpS1707;
  moonbit_string_t _M0L6_2atmpS1708;
  moonbit_string_t _M0L6_2atmpS4350;
  moonbit_string_t _M0L6_2atmpS1706;
  moonbit_string_t _M0L6_2atmpS4349;
  moonbit_string_t _M0L6_2atmpS1705;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1709 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4351
  = moonbit_add_string(_M0L6_2atmpS1709, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1709);
  _M0L6_2atmpS1707 = _M0L6_2atmpS4351;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1708
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4350 = moonbit_add_string(_M0L6_2atmpS1707, _M0L6_2atmpS1708);
  moonbit_decref(_M0L6_2atmpS1707);
  moonbit_decref(_M0L6_2atmpS1708);
  _M0L6_2atmpS1706 = _M0L6_2atmpS4350;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4349
  = moonbit_add_string(_M0L6_2atmpS1706, (moonbit_string_t)moonbit_string_literal_68.data);
  moonbit_decref(_M0L6_2atmpS1706);
  _M0L6_2atmpS1705 = _M0L6_2atmpS4349;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1705);
  return 0;
}

void* _M0FPB5abortGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1714;
  moonbit_string_t _M0L6_2atmpS4354;
  moonbit_string_t _M0L6_2atmpS1712;
  moonbit_string_t _M0L6_2atmpS1713;
  moonbit_string_t _M0L6_2atmpS4353;
  moonbit_string_t _M0L6_2atmpS1711;
  moonbit_string_t _M0L6_2atmpS4352;
  moonbit_string_t _M0L6_2atmpS1710;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1714 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4354
  = moonbit_add_string(_M0L6_2atmpS1714, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1714);
  _M0L6_2atmpS1712 = _M0L6_2atmpS4354;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1713
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4353 = moonbit_add_string(_M0L6_2atmpS1712, _M0L6_2atmpS1713);
  moonbit_decref(_M0L6_2atmpS1712);
  moonbit_decref(_M0L6_2atmpS1713);
  _M0L6_2atmpS1711 = _M0L6_2atmpS4353;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4352
  = moonbit_add_string(_M0L6_2atmpS1711, (moonbit_string_t)moonbit_string_literal_68.data);
  moonbit_decref(_M0L6_2atmpS1711);
  _M0L6_2atmpS1710 = _M0L6_2atmpS4352;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L6_2atmpS1710);
}

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPB5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t _M0L6stringS26,
  moonbit_string_t _M0L3locS27
) {
  moonbit_string_t _M0L6_2atmpS1719;
  moonbit_string_t _M0L6_2atmpS4357;
  moonbit_string_t _M0L6_2atmpS1717;
  moonbit_string_t _M0L6_2atmpS1718;
  moonbit_string_t _M0L6_2atmpS4356;
  moonbit_string_t _M0L6_2atmpS1716;
  moonbit_string_t _M0L6_2atmpS4355;
  moonbit_string_t _M0L6_2atmpS1715;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1719 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4357
  = moonbit_add_string(_M0L6_2atmpS1719, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1719);
  _M0L6_2atmpS1717 = _M0L6_2atmpS4357;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1718
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS27);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4356 = moonbit_add_string(_M0L6_2atmpS1717, _M0L6_2atmpS1718);
  moonbit_decref(_M0L6_2atmpS1717);
  moonbit_decref(_M0L6_2atmpS1718);
  _M0L6_2atmpS1716 = _M0L6_2atmpS4356;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4355
  = moonbit_add_string(_M0L6_2atmpS1716, (moonbit_string_t)moonbit_string_literal_68.data);
  moonbit_decref(_M0L6_2atmpS1716);
  _M0L6_2atmpS1715 = _M0L6_2atmpS4355;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(_M0L6_2atmpS1715);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS28,
  moonbit_string_t _M0L3locS29
) {
  moonbit_string_t _M0L6_2atmpS1724;
  moonbit_string_t _M0L6_2atmpS4360;
  moonbit_string_t _M0L6_2atmpS1722;
  moonbit_string_t _M0L6_2atmpS1723;
  moonbit_string_t _M0L6_2atmpS4359;
  moonbit_string_t _M0L6_2atmpS1721;
  moonbit_string_t _M0L6_2atmpS4358;
  moonbit_string_t _M0L6_2atmpS1720;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1724 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS28);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4360
  = moonbit_add_string(_M0L6_2atmpS1724, (moonbit_string_t)moonbit_string_literal_118.data);
  moonbit_decref(_M0L6_2atmpS1724);
  _M0L6_2atmpS1722 = _M0L6_2atmpS4360;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1723
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS29);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4359 = moonbit_add_string(_M0L6_2atmpS1722, _M0L6_2atmpS1723);
  moonbit_decref(_M0L6_2atmpS1722);
  moonbit_decref(_M0L6_2atmpS1723);
  _M0L6_2atmpS1721 = _M0L6_2atmpS4359;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4358
  = moonbit_add_string(_M0L6_2atmpS1721, (moonbit_string_t)moonbit_string_literal_68.data);
  moonbit_decref(_M0L6_2atmpS1721);
  _M0L6_2atmpS1720 = _M0L6_2atmpS4358;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1720);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS18,
  uint32_t _M0L5valueS19
) {
  uint32_t _M0L3accS1699;
  uint32_t _M0L6_2atmpS1698;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1699 = _M0L4selfS18->$0;
  _M0L6_2atmpS1698 = _M0L3accS1699 + 4u;
  _M0L4selfS18->$0 = _M0L6_2atmpS1698;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS18, _M0L5valueS19);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5inputS17
) {
  uint32_t _M0L3accS1696;
  uint32_t _M0L6_2atmpS1697;
  uint32_t _M0L6_2atmpS1695;
  uint32_t _M0L6_2atmpS1694;
  uint32_t _M0L6_2atmpS1693;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1696 = _M0L4selfS16->$0;
  _M0L6_2atmpS1697 = _M0L5inputS17 * 3266489917u;
  _M0L6_2atmpS1695 = _M0L3accS1696 + _M0L6_2atmpS1697;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1694 = _M0FPB4rotl(_M0L6_2atmpS1695, 17);
  _M0L6_2atmpS1693 = _M0L6_2atmpS1694 * 668265263u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1693;
  moonbit_decref(_M0L4selfS16);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS14, int32_t _M0L1rS15) {
  uint32_t _M0L6_2atmpS1690;
  int32_t _M0L6_2atmpS1692;
  uint32_t _M0L6_2atmpS1691;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1690 = _M0L1xS14 << (_M0L1rS15 & 31);
  _M0L6_2atmpS1692 = 32 - _M0L1rS15;
  _M0L6_2atmpS1691 = _M0L1xS14 >> (_M0L6_2atmpS1692 & 31);
  return _M0L6_2atmpS1690 | _M0L6_2atmpS1691;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S10,
  struct _M0TPB6Logger _M0L10_2ax__4934S13
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS11;
  moonbit_string_t _M0L8_2afieldS4361;
  int32_t _M0L6_2acntS4540;
  moonbit_string_t _M0L15_2a_2aarg__4935S12;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS11
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S10;
  _M0L8_2afieldS4361 = _M0L10_2aFailureS11->$0;
  _M0L6_2acntS4540 = Moonbit_object_header(_M0L10_2aFailureS11)->rc;
  if (_M0L6_2acntS4540 > 1) {
    int32_t _M0L11_2anew__cntS4541 = _M0L6_2acntS4540 - 1;
    Moonbit_object_header(_M0L10_2aFailureS11)->rc = _M0L11_2anew__cntS4541;
    moonbit_incref(_M0L8_2afieldS4361);
  } else if (_M0L6_2acntS4540 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS11);
  }
  _M0L15_2a_2aarg__4935S12 = _M0L8_2afieldS4361;
  if (_M0L10_2ax__4934S13.$1) {
    moonbit_incref(_M0L10_2ax__4934S13.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S13.$0->$method_0(_M0L10_2ax__4934S13.$1, (moonbit_string_t)moonbit_string_literal_119.data);
  if (_M0L10_2ax__4934S13.$1) {
    moonbit_incref(_M0L10_2ax__4934S13.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S13, _M0L15_2a_2aarg__4935S12);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S13.$0->$method_0(_M0L10_2ax__4934S13.$1, (moonbit_string_t)moonbit_string_literal_120.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS9) {
  void* _block_4772;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4772 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4772)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4772)->$0 = _M0L4selfS9;
  return _block_4772;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS8) {
  void* _block_4773;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4773 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4773)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4773)->$0 = _M0L5arrayS8;
  return _block_4773;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS7,
  moonbit_string_t _M0L3objS6
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS6, _M0L4selfS7);
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

void* _M0FPC15abort5abortGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamE _M0FPC15abort5abortGRPB9ArrayViewGRP48clawteam8clawteam8internal6openai30ChatCompletionContentPartParamEE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS5
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS5);
  moonbit_decref(_M0L3msgS5);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1634) {
  switch (Moonbit_object_tag(_M0L4_2aeS1634)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1634);
      return (moonbit_string_t)moonbit_string_literal_121.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1634);
      return (moonbit_string_t)moonbit_string_literal_122.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1634);
      return (moonbit_string_t)moonbit_string_literal_123.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1634);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1634);
      return (moonbit_string_t)moonbit_string_literal_124.data;
      break;
    }
  }
}

void* _M0IPC15array5ArrayPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJsonGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(
  void* _M0L11_2aobj__ptrS1653
) {
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE* _M0L7_2aselfS1652 =
    (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE*)_M0L11_2aobj__ptrS1653;
  return _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai26ChatCompletionMessageParamE(_M0L7_2aselfS1652);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1651,
  int32_t _M0L8_2aparamS1650
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1649 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1651;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1649, _M0L8_2aparamS1650);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1648,
  struct _M0TPC16string10StringView _M0L8_2aparamS1647
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1646 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1648;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1646, _M0L8_2aparamS1647);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1645,
  moonbit_string_t _M0L8_2aparamS1642,
  int32_t _M0L8_2aparamS1643,
  int32_t _M0L8_2aparamS1644
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1641 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1645;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1641, _M0L8_2aparamS1642, _M0L8_2aparamS1643, _M0L8_2aparamS1644);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1640,
  moonbit_string_t _M0L8_2aparamS1639
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1638 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1640;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1638, _M0L8_2aparamS1639);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1689 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1688;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1669;
  moonbit_string_t* _M0L6_2atmpS1687;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1686;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1670;
  moonbit_string_t* _M0L6_2atmpS1685;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1684;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1671;
  moonbit_string_t* _M0L6_2atmpS1683;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1682;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1672;
  moonbit_string_t* _M0L6_2atmpS1681;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1680;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1673;
  moonbit_string_t* _M0L6_2atmpS1679;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1678;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1674;
  moonbit_string_t* _M0L6_2atmpS1677;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1676;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1675;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1560;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1668;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1667;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1666;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1661;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1561;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1665;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1664;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1663;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1662;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1559;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1660;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1659;
  _M0L6_2atmpS1689[0] = (moonbit_string_t)moonbit_string_literal_125.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1688
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1688)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1688->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1688->$1 = _M0L6_2atmpS1689;
  _M0L8_2atupleS1669
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1669)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1669->$0 = 0;
  _M0L8_2atupleS1669->$1 = _M0L8_2atupleS1688;
  _M0L6_2atmpS1687 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1687[0] = (moonbit_string_t)moonbit_string_literal_126.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1686
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1686)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1686->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1686->$1 = _M0L6_2atmpS1687;
  _M0L8_2atupleS1670
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1670)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1670->$0 = 1;
  _M0L8_2atupleS1670->$1 = _M0L8_2atupleS1686;
  _M0L6_2atmpS1685 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1685[0] = (moonbit_string_t)moonbit_string_literal_127.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1684
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1684)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1684->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1684->$1 = _M0L6_2atmpS1685;
  _M0L8_2atupleS1671
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1671)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1671->$0 = 2;
  _M0L8_2atupleS1671->$1 = _M0L8_2atupleS1684;
  _M0L6_2atmpS1683 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1683[0] = (moonbit_string_t)moonbit_string_literal_128.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1682
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1682)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1682->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1682->$1 = _M0L6_2atmpS1683;
  _M0L8_2atupleS1672
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1672)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1672->$0 = 3;
  _M0L8_2atupleS1672->$1 = _M0L8_2atupleS1682;
  _M0L6_2atmpS1681 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1681[0] = (moonbit_string_t)moonbit_string_literal_129.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__4_2eclo);
  _M0L8_2atupleS1680
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1680)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1680->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__4_2eclo;
  _M0L8_2atupleS1680->$1 = _M0L6_2atmpS1681;
  _M0L8_2atupleS1673
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1673)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1673->$0 = 4;
  _M0L8_2atupleS1673->$1 = _M0L8_2atupleS1680;
  _M0L6_2atmpS1679 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1679[0] = (moonbit_string_t)moonbit_string_literal_130.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__5_2eclo);
  _M0L8_2atupleS1678
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1678)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1678->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__5_2eclo;
  _M0L8_2atupleS1678->$1 = _M0L6_2atmpS1679;
  _M0L8_2atupleS1674
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1674)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1674->$0 = 5;
  _M0L8_2atupleS1674->$1 = _M0L8_2atupleS1678;
  _M0L6_2atmpS1677 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1677[0] = (moonbit_string_t)moonbit_string_literal_131.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__6_2eclo);
  _M0L8_2atupleS1676
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1676)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1676->$0
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test47____test__63616368655f746573742e6d6274__6_2eclo;
  _M0L8_2atupleS1676->$1 = _M0L6_2atmpS1677;
  _M0L8_2atupleS1675
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1675)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1675->$0 = 6;
  _M0L8_2atupleS1675->$1 = _M0L8_2atupleS1676;
  _M0L7_2abindS1560
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(7);
  _M0L7_2abindS1560[0] = _M0L8_2atupleS1669;
  _M0L7_2abindS1560[1] = _M0L8_2atupleS1670;
  _M0L7_2abindS1560[2] = _M0L8_2atupleS1671;
  _M0L7_2abindS1560[3] = _M0L8_2atupleS1672;
  _M0L7_2abindS1560[4] = _M0L8_2atupleS1673;
  _M0L7_2abindS1560[5] = _M0L8_2atupleS1674;
  _M0L7_2abindS1560[6] = _M0L8_2atupleS1675;
  _M0L6_2atmpS1668 = _M0L7_2abindS1560;
  _M0L6_2atmpS1667
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 7, _M0L6_2atmpS1668
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1666
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1667);
  _M0L8_2atupleS1661
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1661)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1661->$0 = (moonbit_string_t)moonbit_string_literal_132.data;
  _M0L8_2atupleS1661->$1 = _M0L6_2atmpS1666;
  _M0L7_2abindS1561
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1665 = _M0L7_2abindS1561;
  _M0L6_2atmpS1664
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1665
  };
  #line 407 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1663
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1664);
  _M0L8_2atupleS1662
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1662)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1662->$0 = (moonbit_string_t)moonbit_string_literal_133.data;
  _M0L8_2atupleS1662->$1 = _M0L6_2atmpS1663;
  _M0L7_2abindS1559
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1559[0] = _M0L8_2atupleS1661;
  _M0L7_2abindS1559[1] = _M0L8_2atupleS1662;
  _M0L6_2atmpS1660 = _M0L7_2abindS1559;
  _M0L6_2atmpS1659
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1660
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal21cache__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1659);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1658;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1628;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1629;
  int32_t _M0L7_2abindS1630;
  int32_t _M0L2__S1631;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1658
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1628
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1628)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1628->$0 = _M0L6_2atmpS1658;
  _M0L12async__testsS1628->$1 = 0;
  #line 446 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1629
  = _M0FP48clawteam8clawteam8internal21cache__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1630 = _M0L7_2abindS1629->$1;
  _M0L2__S1631 = 0;
  while (1) {
    if (_M0L2__S1631 < _M0L7_2abindS1630) {
      struct _M0TUsiE** _M0L8_2afieldS4365 = _M0L7_2abindS1629->$0;
      struct _M0TUsiE** _M0L3bufS1657 = _M0L8_2afieldS4365;
      struct _M0TUsiE* _M0L6_2atmpS4364 =
        (struct _M0TUsiE*)_M0L3bufS1657[_M0L2__S1631];
      struct _M0TUsiE* _M0L3argS1632 = _M0L6_2atmpS4364;
      moonbit_string_t _M0L8_2afieldS4363 = _M0L3argS1632->$0;
      moonbit_string_t _M0L6_2atmpS1654 = _M0L8_2afieldS4363;
      int32_t _M0L8_2afieldS4362 = _M0L3argS1632->$1;
      int32_t _M0L6_2atmpS1655 = _M0L8_2afieldS4362;
      int32_t _M0L6_2atmpS1656;
      moonbit_incref(_M0L6_2atmpS1654);
      moonbit_incref(_M0L12async__testsS1628);
      #line 447 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal21cache__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1628, _M0L6_2atmpS1654, _M0L6_2atmpS1655);
      _M0L6_2atmpS1656 = _M0L2__S1631 + 1;
      _M0L2__S1631 = _M0L6_2atmpS1656;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1629);
    }
    break;
  }
  #line 449 "E:\\moonbit\\clawteam\\internal\\cache\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal21cache__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal21cache__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1628);
  return 0;
}